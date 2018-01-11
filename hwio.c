
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/capability.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include "hwio.h"
#include "mem_arch.h"


static int open_hwio(struct inode *inode, struct file *file)
{
    mmio_range_t *range;
    
    if(!capable(CAP_SYS_RAWIO))
        return -EPERM;
    
    file->private_data = kmalloc(sizeof(mmio_range_t), GFP_KERNEL);
    
    range = file->private_data;
    range->phys = 0;
    range->length = 0;
    
    return 0;
}

static const struct vm_operations_struct mmap_mmio_ops = {
#ifndef CONFIG_HAVE_IOREMAP_PROT
    .access = generic_access_phys
#endif //CONFIG_HAVE_IOREMAP_PROT
};

static int mmap_hwio(struct file* file, struct vm_area_struct *vma)
{
    mmio_range_t *range = (mmio_range_t*)file->private_data;
    size_t size = vma->vm_end - vma->vm_start;
    phys_addr_t offset = (phys_addr_t)vma->vm_pgoff << PAGE_SHIFT;

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

static long ioctl_hwio(struct file *file, unsigned int cmd, unsigned long arg)
{
    mmio_range_t *range = (mmio_range_t*)file->private_data;

    // if the range is already set it cannot be changed anymore
    if(range->phys || range->length)
        return -EACCES;

    switch (cmd)
    {
        case MMIO_SET_RANGE:
            if(copy_from_user(range, (mmio_range_t*)arg, sizeof(mmio_range_t))){
                return -EACCES;
            }
            break;
        default:
            return -EINVAL;
    }

    // extend size to full pages since we can only mmap pages
    range->length = range->length + (range->length % PAGE_SIZE);

    return 0;
}

static int close_hwio(struct inode *inode, struct file *file)
{
    kfree(file->private_data);
    return 0;
}

static const struct file_operations hwio_fops = {
    .open = open_hwio,
    .mmap = mmap_hwio,
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
MODULE_AUTHOR("Johannes Kliemann <jk@jkliemann.de");
MODULE_DESCRIPTION("Provide hardware io resources in user space");

