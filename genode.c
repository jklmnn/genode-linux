
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/capability.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>

static int open_mmio(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "%s\n", __func__);
    return capable(CAP_SYS_RAWIO) ? 0 : -EPERM;
}

static int mmap_mmio(struct file* file, struct vm_area_struct *vma)
{
    printk(KERN_INFO "%s\n", __func__);
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

static const struct file_operations mmio_fops = {
    .open = open_mmio,
    .mmap = mmap_mmio,
    .read = read_mmio,
    .write = write_mmio
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
