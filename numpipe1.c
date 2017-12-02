/*
Raymond Rolston
December 1 2017
numpipe1 charachter device
controls reads and writes to avoid race conditions and failed pipes
*/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/semaphore.h>
//#include <linux/rwsem.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

#define DEV_NAME "numpipe1"
#define CLASS_NAME "classnumpipe1"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raymond Rolston");
MODULE_DESCRIPTION("Charachter Device that can be used as a pipe");
MODULE_VERSION("0.3");

static int N = 1;
module_param(N, int, 0644);
MODULE_PARM_DESC(N, " Maximum N numbers in FIFO queue ");
static int dev_number;
static char tstr[32767] = {0};
//static char* tstr_ptr;
static int string_size = 32767;
//static struct semaphore pipe_lock;
//static struct rw_semaphore pipe_lock;
static struct semaphore pipe_cap;
static struct semaphore read_lock;
static struct semaphore write_lock;
static struct class* numpipe1Class = NULL;
static struct device* numpipe1Device = NULL;

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = dev_read,
    .write = dev_write,
    .open = dev_open,
    .release = dev_release
};

static int __init numpipe1_init(void){
//    init_rwsem(&pipe_lock);
//    sema_init(&pipe_lock, 1);
    sema_init(&pipe_cap, N);
    sema_init(&read_lock, 1);
    sema_init(&write_lock, 1);
    dev_number = register_chrdev(0, DEV_NAME, &fops);
    if(dev_number<0){
        printk(KERN_ALERT "Registering charachter device failed with %d\n", dev_number);
        return dev_number;
    }
    printk(KERN_INFO "Charachter device %s with number %d registered.\n", DEV_NAME, dev_number);

    numpipe1Class = class_create(THIS_MODULE, CLASS_NAME);
    if(IS_ERR(numpipe1Class)){
        unregister_chrdev(dev_number, DEV_NAME);
        printk(KERN_ALERT "Registering charachter device CLASS failed with %d\n", dev_number);
        return PTR_ERR(numpipe1Class);
    }
    printk(KERN_INFO "Charachter device %s CLASS  with number %d registered.\n", DEV_NAME, dev_number);

    numpipe1Device = device_create(numpipe1Class, NULL, MKDEV(dev_number, 0), NULL, DEV_NAME);
    if(IS_ERR(numpipe1Device)){
        class_destroy(numpipe1Class);
        unregister_chrdev(dev_number, DEV_NAME);
        printk(KERN_ALERT "Registering charachter device DRIVER failed with %d\n", dev_number);
        return PTR_ERR(numpipe1Device);
    }
    printk(KERN_INFO "Charachter device %s DRIVER with number %d registered.\n", DEV_NAME, dev_number);

    return 0;
}

static void __exit numpipe1_exit(void){
    device_destroy(numpipe1Class, MKDEV(dev_number,0));
    class_unregister(numpipe1Class);
    class_destroy(numpipe1Class);
    unregister_chrdev(dev_number, DEV_NAME);
    printk(KERN_INFO "%s Exit: module unloaded", DEV_NAME);
}

static int dev_open(struct inode *inodep, struct file *filep){
    printk(KERN_INFO "process opening device%d: securing module %s\n", dev_number, DEV_NAME);
    try_module_get(THIS_MODULE);
    return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
    down_interruptible(&read_lock);
    //critical region semaphore aquired
    printk(KERN_INFO "process reading device%d: %s", dev_number, DEV_NAME);
    //tstr_ptr = tstr;
    if((copy_to_user(buffer, tstr, sizeof(int)))==0){
        printk(KERN_INFO "numpipe1 device sent the number:\n %s \n", buffer);
        up(&pipe_cap);
        up(&read_lock);
        return 0;
    }else{
        printk(KERN_INFO "failed to read numpipe1 device message");
        up(&read_lock);
        return -EFAULT;
    }
}

static ssize_t dev_write( struct file *filep, const char *buffer, size_t len, loff_t *offset){
    //if(down_interruptible(&write_lock)){//semaphore not aquired recieving signal
    down_interruptible(&write_lock);//semaphore not aquired recieving signal
    printk(KERN_INFO "Writing to pipe\n");
        //critical region semaphore aquired
        //if(sscanf(buffer, "%s", num) >= 0){
            //printk(KERN_INFO "%s", &num);
            //if(down_interruptible(&pipe_cap)){//pipe space is being used
    down_interruptible(&pipe_cap);//pipe space is being used
    if((copy_from_user(tstr, buffer, sizeof(int))) == 0){
        printk(KERN_INFO "Wrote to numpipe1: %s", tstr);
            //}
    }else{
        printk(KERN_INFO "failed write");
        //}
    }
    up(&write_lock);//semaphore released
    return 0;
}

static int dev_release(struct inode *inodep, struct file *filep){
    printk(KERN_INFO "process closing device%d: releasing module %s", dev_number, DEV_NAME);
    module_put(THIS_MODULE);
    return 0;
}

module_init(numpipe1_init);
module_exit(numpipe1_exit);
