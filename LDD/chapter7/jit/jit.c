#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/moduleparam.h>

#include <linux/sched.h>    //current, jiffies.h, schedule(), ...
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
//#include <asm/processor.h> //cpu_relax(), included by <linux/sched.h>
//#include <linux/wait.h> //schedule(), included by <linux/sched.h>
#include <linux/delay.h> //#include <asm/delay.h>
//#include <linux/timer.h> //timer_list, init_timer(), ..., included by <linux/sched.h>
#include <linux/interrupt.h> //tasklet_struct


enum jit_ways {
    JIT_CURTIME=0,
    JIT_BUSY,
    JIT_SCHED,
    JIT_QUEUE,
    JIT_SCHEDTO,
    JIT_LATENCY_NS=5,
    JIT_LATENCY_US,
    JIT_LATENCY_MS,
    JIT_LATENCY_SLEEP,
    JIT_TIMER,
    JIT_TASKLET=10,
    JIT_TASKLET_HIGH
};

static int delay=HZ;
//module_param(delay, int, S_IRUGO);
//MODULE_PARM_DESC(delay, "delay in second(s)");
static int tdelay = 10;
//module_param(tdelay, int, 0);
static int jit_way=JIT_CURTIME;
module_param(jit_way, int, S_IRUGO);
MODULE_PARM_DESC(jit_way, "JIT_CURTIME=0, JIT_BUSY=1, JIT_SCHED=2, JIT_QUEUE=3, JIT_SCHEDTO=4, ...");

dev_t mydev;
int jit_major=0;
int count=1;
int reg_ok=1;

#define JIT_MAX_BUF_SIZE 1000

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

int jit_fn(char *buf, int way)
{
    int len=0;
    unsigned long j0, j1; /* jiffies */
    wait_queue_head_t wait;
    unsigned long ini=0, end=0;
    u64 tsc1=0, tsc2=0;

    init_waitqueue_head (&wait);
    j0 = jiffies;
    j1 = j0 + delay;

    switch(way) {
        case JIT_BUSY:
            while (time_before(jiffies, j1))
                cpu_relax();
            break;
            
        case JIT_SCHED:
            while (time_before(jiffies, j1)) {
                schedule();
            }
            break;
            
        case JIT_QUEUE:
            wait_event_interruptible_timeout(wait, 0, delay);
            break;
            
        case JIT_SCHEDTO:
            set_current_state(TASK_INTERRUPTIBLE);
            schedule_timeout (delay);
            break;
            
        case JIT_LATENCY_NS:
            rdtscl(ini);
            rdtscll(tsc1);
            ndelay(10); 
            rdtscl(end); //end-ini=4xxx clock/3392MHz =1.x us
            tsc2=(u64)get_cycles();
            break;
            
        case JIT_LATENCY_US:
            rdtscl(ini);
            rdtscll(tsc1);
            udelay(10);
            rdtscl(end);
            tsc2=(u64)get_cycles();
            break;
            
        case JIT_LATENCY_MS:
            rdtscl(ini);
            rdtscll(tsc1);
            mdelay(10);
            rdtscl(end);
            tsc2=(u64)get_cycles();
            break;
            
        case JIT_LATENCY_SLEEP:
            rdtscl(ini);
            rdtscll(tsc1);
            msleep(10);
            rdtscl(end);
            tsc2=(u64)get_cycles();
            break;
    }
    j1 = jiffies; /* actual value after we delayed */

    if (way >= JIT_LATENCY_NS) {
        printk(KERN_DEBUG "ini=%lu, end=%lu, end-ini=%lu \n", ini, end, end-ini);
        printk(KERN_DEBUG "tsc1=%llu, tsc2=%llu \n", tsc1, tsc2);
        len = sprintf(buf, "ini=%lu, end=%lu, end-ini=%lu, tsc1=%llu, tsc2=%llu, delta=%llu \n", ini, end, end-ini, tsc1, tsc2, tsc2-tsc1);
    }
    else {
        printk(KERN_DEBUG "j0=%lu, j1=%lu\n", j0, j1);
        len = sprintf(buf, "%9lu %9lu\n", j0, j1);
    }
    return len;
}

struct jit_data {
    struct timer_list timer;
    struct tasklet_struct tlet;
    int hi; /* tasklet or tasklet_hi */
    wait_queue_head_t wait;
    unsigned long prevjiffies;
    char *buf;
    int loops;
    int len;
};
#define JIT_ASYNC_LOOPS 5

void jit_timer_fn(unsigned long arg)
{
    struct jit_data *data = (struct jit_data *)arg;
    unsigned long j = jiffies;
    data->len += sprintf(data->buf+data->len, "%9lu  %3lu     %u    %6u   %u   %s\n",
        j, j - data->prevjiffies, in_interrupt() ? 1 : 0,
        current->pid, smp_processor_id(), current->comm);

    if ((--data->loops) && (data->len < JIT_MAX_BUF_SIZE-200)) {
        data->timer.expires += tdelay;
        data->prevjiffies = j;
        add_timer(&data->timer);
    } else {
        wake_up_interruptible(&data->wait);
    }
}

int jit_timer(char *buf)
{
    struct jit_data jitdata;
    struct jit_data *data=&jitdata;
    char *buf2 = buf;
    unsigned long j = jiffies;

    //data = kmalloc(sizeof(*data), GFP_KERNEL);
    //if (!data)
    //    return -ENOMEM;

    init_timer(&data->timer);
    init_waitqueue_head (&data->wait);

    data->len = sprintf(buf2, "   time   delta  inirq    pid   cpu command\n"
        "%9lu  %3lu     %u    %6u   %u   %s\n",
        j, 0L, in_interrupt() ? 1 : 0,
        current->pid, smp_processor_id(), current->comm);

    data->prevjiffies = j;
    data->buf = buf2;
    data->loops = JIT_ASYNC_LOOPS;

    data->timer.data = (unsigned long)data;
    data->timer.function = jit_timer_fn;
    data->timer.expires = j + tdelay;
    add_timer(&data->timer);

    wait_event_interruptible(data->wait, !data->loops);
    if (signal_pending(current))
        return -ERESTARTSYS;
    //buf2 = data->buf;
    //kfree(data);
    //return buf2 - buf;
    return data->len;
}

void jit_tasklet_fn(unsigned long arg)
{
    struct jit_data *data = (struct jit_data *)arg;
    unsigned long j = jiffies;
    data->len += sprintf(data->buf+data->len, "%9lu  %3lu     %u    %6u   %u   %s\n",
        j, j - data->prevjiffies, in_interrupt() ? 1 : 0,
        current->pid, smp_processor_id(), current->comm);

    if ((--data->loops) && (data->len < JIT_MAX_BUF_SIZE-200)) {
        data->prevjiffies = j;
        if (data->hi)
            tasklet_hi_schedule(&data->tlet);
        else
            tasklet_schedule(&data->tlet);
    } else {
        wake_up_interruptible(&data->wait);
    }
}

int jit_tasklet(char *buf, long hi)
{
    struct jit_data jitdata;
    struct jit_data *data=&jitdata;
    char *buf2 = buf;
    unsigned long j = jiffies;

    //data = kmalloc(sizeof(*data), GFP_KERNEL);
    //if (!data)
    //    return -ENOMEM;

    init_waitqueue_head (&data->wait);

    data->len = sprintf(buf2, "   time   delta  inirq    pid   cpu command\n"
        "%9lu  %3lu     %u    %6u   %u   %s\n",
        j, 0L, in_interrupt() ? 1 : 0,
        current->pid, smp_processor_id(), current->comm);

    data->prevjiffies = j;
    data->buf = buf2;
    data->loops = JIT_ASYNC_LOOPS;

    tasklet_init(&data->tlet, jit_tasklet_fn, (unsigned long)data);
    data->hi = hi;
    if (hi)
        tasklet_hi_schedule(&data->tlet);
    else
        tasklet_schedule(&data->tlet);

    wait_event_interruptible(data->wait, !data->loops);
    if (signal_pending(current))
        return -ERESTARTSYS;
    //buf2 = data->buf;
    //kfree(data);
    //return buf2 - buf;
    return data->len;
}

ssize_t my_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    int ret=0;
    int len=0;
    printk(KERN_DEBUG "into my_read()\n");
    
    if (down_interruptible(&my_devices.sem))
        return -ERESTARTSYS;

    if (jit_way==JIT_CURTIME)
        len=jit_currentime(my_devices.buf);
    else if (jit_way<JIT_LATENCY_SLEEP)
        len=jit_fn(my_devices.buf, jit_way);
    else if (jit_way==JIT_TIMER)
        len=jit_timer(my_devices.buf);
    else if (jit_way==JIT_TASKLET)
        len=jit_tasklet(my_devices.buf,0);
    else if (jit_way==JIT_TASKLET_HIGH)
        len=jit_tasklet(my_devices.buf,1);

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
    printk(KERN_ALERT "Hello, world\n");
    printk(KERN_DEBUG "delay=%d\n",delay);
    printk(KERN_DEBUG "jit_way=%d\n",jit_way);
    
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
