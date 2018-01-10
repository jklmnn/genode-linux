
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/capability.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include "genode.h"
#include "mem_arch.h"


static int open_mmio(struct inode *inode, struct file *file)
{
    mmio_range_t *range;
    printk(KERN_INFO "%s\n", __func__);
    
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

static int mmap_mmio(struct file* file, struct vm_area_struct *vma)
{
    mmio_range_t *range = (mmio_range_t*)file->private_data;
    size_t size = vma->vm_end - vma->vm_start;
    phys_addr_t offset = (phys_addr_t)vma->vm_pgoff << PAGE_SHIFT;
    
    if(size > range->length)
        return -EPERM;

    if(range->phys > offset)
        return -EPERM;

    if(range->phys + range->length < offset + size)
        return -EPERM;

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

    printk(KERN_INFO "%s -> %lx x %lu\n", __func__, range->phys, range->length);
    return 0;
}

static ssize_t read_mmio(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    printk(KERN_INFO "%s\n", __func__);
    return -EPERM;
}

static ssize_t write_mmio(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    printk(KERN_INFO "%s\n", __func__);
    return -EPERM;
}

static long ioctl_mmio(struct file *file, unsigned int cmd, unsigned long arg)
{
    mmio_range_t *range = (mmio_range_t*)file->private_data;
    printk(KERN_INFO "%s", __func__);

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

    return 0;
}

static int close_mmio(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "%s\n", __func__);
    kfree(file->private_data);
    return 0;
}

static const struct file_operations mmio_fops = {
    .open = open_mmio,
    .mmap = mmap_mmio,
    .read = read_mmio,
    .write = write_mmio,
    .unlocked_ioctl = ioctl_mmio,
    .release = close_mmio
};

static struct miscdevice genode_mmio = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "genode-mmio",
    .fops = &mmio_fops
};

static int __init genode_init(void)
{
    misc_register(&genode_mmio);
    printk(KERN_INFO "genode module registered\n");
    return 0;
}

static void __exit genode_exit(void)
{
    misc_deregister(&genode_mmio);
    printk(KERN_INFO "genode module unregistered\n");
}


module_init(genode_init);
module_exit(genode_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Johannes Kliemann <jk@jkliemann.de");
MODULE_DESCRIPTION("Provide kernel resources in user space for Genode");
