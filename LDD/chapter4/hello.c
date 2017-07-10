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
#include <linux/proc_fs.h> //create_proc_read_entry / remove_proc_entry  ,...

static int number=10;
module_param(number, int, S_IRUGO);
MODULE_PARM_DESC(number, "an integer parameter example");
static char* name="default name";
module_param(name, charp, S_IRUGO);
MODULE_PARM_DESC(name, "an charp parameter example");

static int num=0;
static int arr[]={0,1,2,3,4,5};
module_param_array(arr, int, num, S_IRUGO);


static int hello_read_proc(char *buf, char **start, off_t offset, int len, int *eof, void *data)
{
    len=0;
    len = sprintf(buf, "UTS_RELEASE=%s\n"
        "LINUX_VERSION_CODE=%u\n"
        "number=%d\n"
        "name=%s\n"
        "num=%d\n"
        "Process [%s], pid=%i\n",
        UTS_RELEASE, LINUX_VERSION_CODE,
        number, name, num,
        current->comm, current->pid);
    //*start = buf; //with this line, it will print the above lines again and again
    //*start =(char*)8; //also print the above lines again and again
    //*start = 0; //it equals to do nothing with *start
    //*eof=0;
    printk(KERN_ALERT "*start=%p, *eof=%d, buf=%p\n",*start, *eof, buf); //something like: *start=00000000, *eof=0, buf=d9f53000
    return len;
}


static int __init hello_init(void)
{
    printk(KERN_ALERT "Hello, world\n");
    create_proc_read_entry("driver/hello", 0, NULL, hello_read_proc, NULL);
    return 0;
}

static void __exit hello_exit(void)
{
    remove_proc_entry("hello", NULL);
    printk(KERN_ALERT "Goodbye, world\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fred");
MODULE_DESCRIPTION("My Chapter4 driver demo");
