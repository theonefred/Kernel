#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/moduleparam.h>

#include <linux/sched.h>    //current, jiffies.h, ...
#include <linux/types.h>    //dev_t,...
#include <linux/fs.h>   //register_chrdev_region() / alloc_chrdev_region(),...
#include <linux/cdev.h> //struct cdev,...
#include <asm/uaccess.h>    //copy_to_user() / copy_from_user(),...
//#include <linux/kdev_t.h> //for MAJOR,MINOR, included by <linux/fs.h>
//#include <asm/semaphore.h>    //struct semaphore, included by <linux/fs.h>
#include <linux/slab.h> //kmalloc / kfree,...

#include <linux/param.h> //HZ
#include <linux/time.h> //timespec, do_gettimeofday(),...
#include <asm/msr.h> //rdtsc(),...
#include <linux/timex.h> //get_cycles()


static int delay=HZ;
module_param(delay, int, S_IRUGO);
MODULE_PARM_DESC(delay, "delay in second(s)");

dev_t mydev;
int jit_major=0;
int count=1;
int reg_ok=1;

#define JIT_MAX_BUF_SIZE 128

struct jit_dev {
    int num;
    char* buf;
    struct semaphore sem;
    struct cdev cdev;
};

struct jit_dev my_devices;


int my_open(struct inode *inode, struct file *filp)
{
    struct jit_dev *dev; /* device information */
    dev = container_of(inode->i_cdev, struct jit_dev, cdev);
    
    printk(KERN_DEBUG "into my_open()\n");
    filp->private_data = dev; /* for other methods */
    dev->num=0x12345678;
    printk(KERN_DEBUG "&(*dev)=0x%p, &my_devices=0x%p\n, my_devices.num=0x%x\n", &(*dev), &my_devices, my_devices.num);
    
    dev->buf=(char*)kmalloc(JIT_MAX_BUF_SIZE, GFP_KERNEL);
    if (dev->buf) {
        memset(dev->buf, 0, JIT_MAX_BUF_SIZE);
        printk(KERN_DEBUG "buf is memset to 0\n");
    }
    else
        printk(KERN_ERR "kmalloc failed\n");

    return 0;          /* success */
}

int my_release(struct inode *inode, struct file *filp)
{
    printk(KERN_DEBUG "into my_release()\n");
    kfree(my_devices.buf);
    return 0;
}

int jit_currentime(char *buf)
{
    struct timeval tv1;
    struct timespec tv2;
    unsigned long j1;
    u64 j2;
    int len=0;

    j1 = jiffies;
    j2 = get_jiffies_64();
    do_gettimeofday(&tv1);
    tv2 = current_kernel_time();

    len += sprintf(buf,"0x%08lx 0x%016Lx %10i.%06i\n"
        "%40i.%09i\n",
        j1, j2,
        (int) tv1.tv_sec, (int) tv1.tv_usec,
        (int) tv2.tv_sec, (int) tv2.tv_nsec);
    printk(KERN_DEBUG "buf=%s\n", buf);
    
    return len;
}

ssize_t my_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    int ret=0;
    //int i=0;
    int len=0;
    printk(KERN_DEBUG "into my_read()\n");
    
    if (down_interruptible(&my_devices.sem))
        return -ERESTARTSYS;

    len=jit_currentime(my_devices.buf);
    len=(len<=JIT_MAX_BUF_SIZE)?len:JIT_MAX_BUF_SIZE;
    printk(KERN_DEBUG "len=%d, my_devices.buf=%s\n", len, my_devices.buf);
    ret=copy_to_user(buf, my_devices.buf, len);
    if (ret)
        printk(KERN_ERR "copy_to_user failed\n");
    else
        retval=0;
    
    up(&my_devices.sem);
    return retval;
}

static struct file_operations my_ops = {
    .owner   = THIS_MODULE,
    .open    = my_open,
    .read    = my_read,
    .write  = NULL,
    .release = my_release
};


//static int register_mydev(void)
static void register_mydev(void)
{
    reg_ok=alloc_chrdev_region(&mydev, 0, count, "jit");
    if (reg_ok)
        printk(KERN_ERR "alloc_chrdev_region failed\n");
    else {
        printk(KERN_DEBUG "alloc_chrdev_region OK\n");
        jit_major=MAJOR(mydev);
    }
}

static void jit_setup_cdev(struct jit_dev* dev)
{
    int err=0;
    cdev_init(&dev->cdev, &my_ops);
    dev->cdev.owner = THIS_MODULE;
    //dev->cdev.ops=&my_ops;    //not needed
    err = cdev_add (&dev->cdev, mydev, 1);  //***** use mydev here
    /* Fail gracefully if need be */
    if (err)
        printk(KERN_ERR "Error %d adding jit cdev\n", err);
    else
        printk(KERN_DEBUG "%d adding jit cdev OK\n", err);

    sema_init(&my_devices.sem, 1);
}

static int __init jit_init(void)
{
    register_mydev();
    
    if (!reg_ok)
        jit_setup_cdev(&my_devices);
    
    return 0;
}

static void jit_clean_cdev(void)
{
    printk(KERN_DEBUG "into jit_clean_cdev()\n");
    cdev_del(&my_devices.cdev);
}

static void __exit jit_exit(void)
{
    printk(KERN_ALERT "Goodbye, world\n");
    if (!reg_ok) {
        jit_clean_cdev();
        unregister_chrdev_region(mydev, count);
    }
}

module_init(jit_init);
module_exit(jit_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fred");
MODULE_DESCRIPTION("My driver demo for chapter7");
