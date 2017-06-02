#include <linux/init.h>
#include <linux/module.h>

#include <linux/version.h>
#include <linux/moduleparam.h>

#include <linux/sched.h>

static int number=10;
module_param(number, int, S_IRUGO);
static char* name="default name";
module_param(name, charp, S_IRUGO);

static int num=0;
static int arr[]={0,1,2,3,4,5};
module_param_array(arr, int, num, S_IRUGO);

static int __init hello_init(void)
{
    int i=0;
    
    printk(KERN_ALERT "Hello, world\n");

    printk(KERN_DEBUG "UTS_RELEASE=%s\n",UTS_RELEASE);
    printk(KERN_DEBUG "LINUX_VERSION_CODE=%u\n",LINUX_VERSION_CODE);
    
    printk(KERN_DEBUG "number=%d\n",number);
    printk(KERN_DEBUG "name=%s\n",name);
    
    printk(KERN_DEBUG "num=%d\n",num);
    for (;i<num;++i){
        //printk(KERN_DEBUG "arr[%d]=%d,", i, arr[i]); //it will be printed as "arr[0]=0,<7>arr[1]=1,..."
        printk(KERN_DEBUG "arr[%d]=%d\n", i, arr[i]);
    }
    
    printk(KERN_DEBUG "Process [%s], pid=%i\n", current->comm, current->pid);
    
    return 0;
}

static void __exit hello_exit(void)
{
    printk(KERN_ALERT "Goodbye, world\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
//MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Fred");
MODULE_DESCRIPTION("my first driver demo");
