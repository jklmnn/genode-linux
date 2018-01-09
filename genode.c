
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

static int __init genode_init(void)
{
    printk(KERN_INFO "genode module registered\n");
    return 0;
}

static void __exit genode_exit(void)
{
    printk(KERN_INFO "genode module unregistered\n");
}

module_init(genode_init);
module_exit(genode_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Johannes Kliemann <jk@jkliemann.de");
MODULE_DESCRIPTION("Provide kernel resources in user space for Genode");
