
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/capability.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/string.h>

#include "hwio.h"
#include "mem_arch.h"

typedef struct {
    int irq;
    wait_queue_head_t wait;
    int status;
} irq_t;

typedef struct {
    int type;
    union {
        mmio_range_t mmio;
        irq_t irq;
    };
} hwio_data_t;

static int open_hwio(struct inode *inode, struct file *file)
{
    hwio_data_t *range;
    
    if(!capable(CAP_SYS_RAWIO))
        return -EPERM;
    
    file->private_data = kmalloc(sizeof(hwio_data_t), GFP_KERNEL);
    
    range = file->private_data;
    memset(range, 0, sizeof(hwio_data_t));
    
    return 0;
}

static const struct vm_operations_struct mmap_mmio_ops = {
#ifndef CONFIG_HAVE_IOREMAP_PROT
    .access = generic_access_phys
#endif //CONFIG_HAVE_IOREMAP_PROT
};

static int mmap_hwio(struct file* file, struct vm_area_struct *vma)
{
    hwio_data_t *hwio = (hwio_data_t*)file->private_data;
    mmio_range_t *range = &(hwio->mmio);
    size_t size = vma->vm_end - vma->vm_start;
    phys_addr_t offset;
    
    if (hwio->type != T_MMIO){
        return -EPERM;
    }

    vma->vm_pgoff = range->phys >> PAGE_SHIFT;
    offset = (phys_addr_t)(vma->vm_pgoff) << PAGE_SHIFT;

    printk("%s vma->vm_pgoff: %lx\n", __func__, vma->vm_pgoff);
    printk("%s offset: %llx\n", __func__, offset);
    printk("%s range: %lx x %zu\n", __func__, range->phys, range->length);

    // check boundaries set by ioctl
    if(size > range->length)
        return -EPERM;

    if(range->phys > offset)
        return -EPERM;

    if(range->phys + range->length < offset + size)
        return -EPERM;


    // check boundaries set by kernel and architecture
    if(offset + (phys_addr_t)size - 1 < offset)
        return -EINVAL;

    if(!valid_mmap_phys_addr_range(vma->vm_pgoff, size))
        return -EINVAL;

    if(!private_mapping_ok(vma))
        return -ENOSYS;

    if(!range_is_allowed(vma->vm_pgoff, size))
        return -EPERM;

    if(!phys_mem_access_prot_allowed(file, vma->vm_pgoff, size, &vma->vm_page_prot))
        return -EINVAL;


    vma->vm_ops = &mmap_mmio_ops;

    if(remap_pfn_range(
                vma,
                vma->vm_start,
                vma->vm_pgoff,
                size,
                vma->vm_page_prot
                ))
        return -EAGAIN;

    return 0;
}

static irqreturn_t irq_dispatcher(int irq, void *dev_id)
{
    hwio_data_t *hwio = (hwio_data_t*)dev_id;

    if (hwio->type == T_IRQ){
        hwio->irq.status = 1;
        wake_up_all(&(hwio->irq.wait));
    }

    return IRQ_RETVAL(1);
}

static long ioctl_hwio(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret;
    hwio_data_t *hwio = (hwio_data_t*)file->private_data;
    mmio_range_t *range = &(hwio->mmio);
    irq_t *irq = &(hwio->irq);

    // if the range is already set it cannot be changed anymore
    if(hwio->type != T_UNCONFIGURED)
        return -EACCES;

    switch (cmd)
    {
        case MMIO_SET_RANGE:
            if(copy_from_user(range, (mmio_range_t*)arg, sizeof(mmio_range_t))){
                return -EACCES;
            }
            hwio->type = T_MMIO;
            break;
        case IRQ_SET:
            if(copy_from_user(&(irq->irq), (int*)arg, sizeof(irq_t))){
                return -EACCES;
            }
            ret = request_irq(irq->irq, irq_dispatcher, IRQF_SHARED | IRQF_NO_SUSPEND, __func__, (void*)hwio);
            if (ret < 0){
                printk(KERN_ALERT "%s: requesting irq %d failed with %d\n", __func__, irq->irq, ret);
                return ret;
            }
            init_waitqueue_head(&(irq->wait));
            hwio->type = T_IRQ;
            break;
        default:
            return -EINVAL;
    }

    // extend size to full pages since we can only mmap pages
    if(hwio->type == T_MMIO){
        range->length = range->length + (range->length % PAGE_SIZE);
    }

    return 0;
}

ssize_t read_hwio (struct file *file, char __user * __attribute__((unused))str, size_t __attribute__((unused))size, loff_t *__attribute__((unused))offset)
{
    hwio_data_t *hwio = (hwio_data_t*)(file->private_data);

    if (hwio->type != T_IRQ){
        return -EACCES;
    }

    wait_event_interruptible(hwio->irq.wait, hwio->irq.status);
    hwio->irq.status = 0;

    return 0;
}

static int close_hwio(struct inode *inode, struct file *file)
{
    hwio_data_t *hwio = (hwio_data_t*)(file->private_data);
    if (hwio->type == T_IRQ)
    {
        free_irq(hwio->irq.irq, hwio);
    }

    kfree(file->private_data);
    return 0;
}

static const struct file_operations hwio_fops = {
    .open = open_hwio,
    .mmap = mmap_hwio,
    .read = read_hwio,
    .unlocked_ioctl = ioctl_hwio,
    .release = close_hwio
};

static struct miscdevice hwio_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "hwio",
    .fops = &hwio_fops
};

static int __init hwio_init(void)
{
    misc_register(&hwio_dev);
    printk(KERN_INFO "hwio module registered\n");
    return 0;
}

static void __exit hwio_exit(void)
{
    misc_deregister(&hwio_dev);
    printk(KERN_INFO "hwio module unregistered\n");
}


module_init(hwio_init);
module_exit(hwio_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Johannes Kliemann <jk@jkliemann.de>");
MODULE_DESCRIPTION("Provide hardware io resources in user space");

