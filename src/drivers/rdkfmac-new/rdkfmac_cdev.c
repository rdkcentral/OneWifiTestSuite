#include "rdkfmac.h"

struct rdkfmac_device_data char_device[RDKFMAC_MAX_MINORS];
static DECLARE_WAIT_QUEUE_HEAD(rdkfmac_rq);

static unsigned int rdkfmac_poll(struct file *filp, struct poll_table_struct *wait)
{
    __poll_t mask = 0;

    printk(KERN_INFO "%s:%d: Before wait\n", __func__, __LINE__);
    poll_wait(filp, &rdkfmac_rq, wait);
    printk(KERN_INFO "%s:%d: After wait\n", __func__, __LINE__);

    //mask |= (POLLIN | POLLRDNORM);

    return mask;
}

static ssize_t rdkfmac_write(struct file *file, const char __user *user_buffer,
                    size_t size, loff_t * offset)
{
    struct rdkfmac_device_data *rdkfmac_data = &char_device[0];
    ssize_t len = min(rdkfmac_data->size - *offset, size);

    printk(KERN_INFO "%s:%d: writing:bytes=%d\n", __func__, __LINE__, size);

    return len;
}

static ssize_t rdkfmac_read(struct file *file, char __user *user_buffer,
                   size_t size, loff_t *offset)
{
    struct rdkfmac_device_data *rdkfmac_data = &char_device[0];
    ssize_t len = min(rdkfmac_data->size - *offset, size);

    printk(KERN_INFO "%s:%d: reading:bytes=%d\n", __func__, __LINE__, size);
    return len;
}

static int rdkfmac_open(struct inode *inode, struct file *file){
   printk(KERN_INFO "%s:%d\n", __func__, __LINE__);
   return 0;
}

static int rdkfmac_release(struct inode *inode, struct file *file){
   printk(KERN_INFO "%s:%d\n", __func__, __LINE__);
   return 0;
}

const struct file_operations rdkfmac_fops = {
    .owner = THIS_MODULE,
    .open = rdkfmac_open,
    .read = rdkfmac_read,
    .write = rdkfmac_write,
    .release = rdkfmac_release,
    .poll = rdkfmac_poll
};

int init_rdkfmac_cdev(void)
{
    int count, ret_val;

    printk(KERN_INFO "%s:%d\n", __func__, __LINE__);
    ret_val = register_chrdev_region(MKDEV(RDKFMAC_MAJOR, 0), RDKFMAC_MAX_MINORS, RDKFMAC_DEVICE_DRIVER_NAME);
    if (ret_val != 0) {
        printk(KERN_INFO "%s:%d: register_chrdev_region():failed with error code:%d\n", __func__, __LINE__, ret_val);
        return ret_val;
    }

    for(count = 0; count < RDKFMAC_MAX_MINORS; count++) {
        cdev_init(&char_device[count].cdev, &rdkfmac_fops);
        cdev_add(&char_device[count].cdev, MKDEV(RDKFMAC_MAJOR, count), 1);
        char_device[count].rdkfmac_class = class_create(THIS_MODULE, RDKFMAC_CLASS_NAME);
        if (IS_ERR(char_device[count].rdkfmac_class)){
             printk(KERN_ALERT "cdrv : register device class failed\n");
             return PTR_ERR(char_device[count].rdkfmac_class);
        }
        char_device[count].size = BUF_LEN;
        printk(KERN_INFO "%s:%d: registered successfully\n", __func__, __LINE__);
        char_device[count].rdkfmac_dev = device_create(char_device[count].rdkfmac_class, NULL, MKDEV(RDKFMAC_MAJOR, count), NULL, RDKFMAC_DEVICE_NAME);

    }

    return 0;
}

void cleanup_rdkfmac_cdev(void)
{
    int count;

    for(count = 0; count < RDKFMAC_MAX_MINORS; count++) {
        device_destroy(char_device[count].rdkfmac_class, &char_device[count].rdkfmac_dev);
        class_destroy(char_device[count].rdkfmac_class);
        cdev_del(&char_device[count].cdev);
    }
    unregister_chrdev_region(MKDEV(RDKFMAC_MAJOR, 0), RDKFMAC_MAX_MINORS);

    printk(KERN_INFO "%s:%d: unregistered successfully\n", __func__, __LINE__);
}
