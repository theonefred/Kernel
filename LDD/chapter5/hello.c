#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/moduleparam.h>

#include <linux/sched.h>    //current,...
#include <linux/types.h>    //dev_t,...
#include <linux/fs.h>   //register_chrdev_region() / alloc_chrdev_region(),...
#include <linux/cdev.h> //struct cdev,...
#include <asm/uaccess.h>    //copy_to_user() / copy_from_user(),...
//#include <linux/kdev_t.h> //for MAJOR,MINOR, included by <linux/fs.h>
//#include <asm/semaphore.h>    //struct semaphore, included by <linux/fs.h>
#include <linux/slab.h> //kmalloc / kfree,...
#include <linux/completion.h> //complete(), wait_for_completion(),...
#include <linux/rwsem.h> //rw_semaphore,...
#include <asm/atomic.h>

static int number=10;
module_param(number, int, S_IRUGO);
MODULE_PARM_DESC(number, "an integer parameter example");
static char* name="default name";
module_param(name, charp, S_IRUGO);
MODULE_PARM_DESC(name, "an charp parameter example");

static int num=0;
static int arr[]={0,1,2,3,4,5};
module_param_array(arr, int, num, S_IRUGO);

dev_t mydev;
int hello_major=0;
int count=1;
int reg_ok=1;

#define HELLO_MAX_BUF_SIZE 128

struct hello_dev {
    int num;
    //char* buf;
    char buf[HELLO_MAX_BUF_SIZE];
    struct rw_semaphore rwsem;
    struct cdev cdev;
    atomic_t v;
};

struct hello_dev my_devices;


int my_open(struct inode *inode, struct file *filp)
{
    struct hello_dev *dev; /* device information */
    dev = container_of(inode->i_cdev, struct hello_dev, cdev);
    
    printk(KERN_DEBUG "into my_open()\n");
    filp->private_data = dev; /* for other methods */
    dev->num=0x12345678;
    printk(KERN_DEBUG "&(*dev)=0x%p, &my_devices=0x%p\n, my_devices.num=0x%x\n", &(*dev), &my_devices, my_devices.num);
    
    //the root cause of the crash of using completion for two user space processes with their own open
    //if process A reads and is suspended due to some mutex, so buf is allocated
    //when B writes, buf is allocated again, the previous buf alllocated by A is lost
    //then when A is resumed, it has its reading done, and releases the buf
    //while when B is finished, it also releases the buf
    //it is not caused by completion at all
    //solution:
    //1. not allocate buf dynamically
    //2. add reference counter for dynamically allocated buf
    //once it is >0, no more malloc; not free it until no user (ref num==0) in my_release()
    //or 3. use one user space exe file, which opens the device once, and then fork anther process to write
    
    //dev->buf=(char*)kmalloc(HELLO_MAX_BUF_SIZE, GFP_KERNEL);
    //if (dev->buf) {
    //    memset(dev->buf, 0, HELLO_MAX_BUF_SIZE);
    //    printk(KERN_DEBUG "buf is memset to 0\n");
    //}
    //else
    //    printk(KERN_ERR "kmalloc failed\n");

    return 0;          /* success */
}

int my_release(struct inode *inode, struct file *filp)
{
    printk(KERN_DEBUG "into my_release()\n");
    //kfree(my_devices.buf);
    return 0;
}

DECLARE_COMPLETION(comp);

void use_atomic(void)
{
    int i=5, j=3, k=0;
    printk(KERN_DEBUG "atomic var v=%d\n", atomic_read(&my_devices.v));
    atomic_add(i, &my_devices.v);
    printk(KERN_DEBUG "after atomic_add %d, v=%d\n", i, atomic_read(&my_devices.v));
    atomic_sub(j, &my_devices.v);
    printk(KERN_DEBUG "after atomic_sub %d, v=%d\n", j, atomic_read(&my_devices.v));
    atomic_inc(&my_devices.v);
    printk(KERN_DEBUG "after atomic_inc, v=%d\n", atomic_read(&my_devices.v));
    atomic_dec(&my_devices.v);
    printk(KERN_DEBUG "after atomic_dec, v=%d\n", atomic_read(&my_devices.v));
    
    printk(KERN_DEBUG "after atomic_add_return %d, v=%d\n", i, atomic_add_return(i, &my_devices.v));
    printk(KERN_DEBUG "after atomic_sub_return %d, v=%d\n", j, atomic_sub_return(j, &my_devices.v));
    printk(KERN_DEBUG "after atomic_inc_return, v=%d\n", atomic_inc_return(&my_devices.v));
    printk(KERN_DEBUG "after atomic_dec_return, v=%d\n", atomic_dec_return(&my_devices.v));
    
    k=atomic_sub_and_test(1, &my_devices.v);
    if (!k)
        printk(KERN_DEBUG "atomic_sub_and_test()=%d\n", k);
}

ssize_t my_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    int ret=0;
    int i=0;
    printk(KERN_DEBUG "into my_read(), go to sleep for writing\n");
    wait_for_completion(&comp);

    down_read(&my_devices.rwsem);
    for (i=32;i<127;++i) //printable ASCII chars
        my_devices.buf[i-32]=i;
    ret=copy_to_user(buf, my_devices.buf, HELLO_MAX_BUF_SIZE);
    if (ret)
        printk(KERN_ERR "copy_to_user failed\n");
    else
        retval=127-32;
    up_read(&my_devices.rwsem);
    
    use_atomic();
    return retval;
}

ssize_t my_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    int ret=0;
    printk(KERN_DEBUG "into my_write(), wake the reader \n");
    complete(&comp);
    
    down_write(&my_devices.rwsem);
    ret=copy_from_user(&my_devices.num, buf, sizeof(my_devices.num));
    printk(KERN_DEBUG "my_devices.num=0x%x after the writing\n", my_devices.num);
    if (ret)
        printk(KERN_ERR "copy_from_user failed\n");
    else
        retval=0;
    up_write(&my_devices.rwsem);

    return retval;
}

static struct file_operations my_ops = {
    .owner   = THIS_MODULE,
    .open    = my_open,
    .read    = my_read,
    .write  = my_write,
    .release = my_release
};

static void hello(void)
{
    int i=0;
    
    printk(KERN_DEBUG "into hello()\n");

    printk(KERN_ALERT "Hello, world\n");
    printk(KERN_DEBUG "UTS_RELEASE=%s\n",UTS_RELEASE);
    printk(KERN_DEBUG "LINUX_VERSION_CODE=%u\n",LINUX_VERSION_CODE);
    printk(KERN_DEBUG "number=%d\n",number);
    printk(KERN_DEBUG "name=%s\n",name);
    printk(KERN_DEBUG "num=%d\n",num);
    
    for (;i<num;++i) {
        //printk(KERN_DEBUG "arr[%d]=%d,", i, arr[i]); //it will be printed as "arr[0]=0,<7>arr[1]=1,..."
        printk(KERN_DEBUG "arr[%d]=%d\n", i, arr[i]);
    }
    
    printk(KERN_DEBUG "Process [%s], pid=%i\n", current->comm, current->pid);
}

//static int register_mydev(void)
static void register_mydev(void)
{
    reg_ok=alloc_chrdev_region(&mydev, 0, count, "hello");
    if (reg_ok)
        printk(KERN_ERR "alloc_chrdev_region failed\n");
    else {
        printk(KERN_DEBUG "alloc_chrdev_region OK\n");
        hello_major=MAJOR(mydev);
    }
}

static void hello_setup_cdev(struct hello_dev* dev)
{
    int err=0;
    cdev_init(&dev->cdev, &my_ops);
    dev->cdev.owner = THIS_MODULE;
    //dev->cdev.ops=&my_ops;    //not needed
    err = cdev_add (&dev->cdev, mydev, 1);  //***** use mydev here
    /* Fail gracefully if need be */
    if (err)
        printk(KERN_ERR "Error %d adding hello cdev\n", err);
    else
        printk(KERN_DEBUG "%d adding hello cdev OK\n", err);

    init_rwsem(&my_devices.rwsem);
    atomic_set(&my_devices.v, 0);
}

static int __init hello_init(void)
{
    hello();
    register_mydev();
    
    if (!reg_ok)
        hello_setup_cdev(&my_devices);
    
    return 0;
}

static void hello_clean_cdev(void)
{
    printk(KERN_DEBUG "into hello_clean_cdev()\n");
    cdev_del(&my_devices.cdev);
}

static void __exit hello_exit(void)
{
    printk(KERN_ALERT "Goodbye, world\n");
    if (!reg_ok) {
        hello_clean_cdev();
        unregister_chrdev_region(mydev, count);
    }
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fred");
MODULE_DESCRIPTION("My 2nd driver demo");
