#include "rdkfmac.h"

struct rdkfmac_device_data g_char_device;
static DECLARE_WAIT_QUEUE_HEAD(rdkfmac_rq);
//static wlan_emu_spec_data_t  *pop_from_char_device(void);

static unsigned int rdkfmac_poll(struct file *filp, struct poll_table_struct *wait)
{
    __poll_t mask = 0;
    struct list_head *head;

    head = &g_char_device.list_head;

    poll_wait(filp, &rdkfmac_rq, wait);

    if (list_empty(head) != 0) {
           mask |= (POLLIN | POLLRDNORM);
    }

      return mask;
}

static ssize_t rdkfmac_write(struct file *file, const char __user *user_buffer,
                    size_t size, loff_t * offset)
{

    return 0;
}

static ssize_t rdkfmac_read(struct file *file, char __user *user_buffer,
                   size_t size, loff_t *offset)
{
//  unsigned long sz;
//  wlan_emu_spec_data_t *spec;

//  spec = pop_from_char_device();
//  if (spec == NULL) {
//      return 0;
//  }
//  if ((sz = copy_to_user(user_buffer, spec, sizeof(wlan_emu_spec_data_t))) != sizeof(wlan_emu_spec_data_t)) {
        return 0;
//  }

//  kfree(spec);

//  return sizeof(wlan_emu_spec_data_t);;
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
    int ret_val;

    printk(KERN_INFO "%s:%d\n", __func__, __LINE__);
    ret_val = register_chrdev_region(MKDEV(RDKFMAC_MAJOR, 0), 1, RDKFMAC_DEVICE_DRIVER_NAME);
    if (ret_val != 0) {
           printk(KERN_INFO "%s:%d: register_chrdev_region():failed with error code:%d\n", __func__, __LINE__, ret_val);
        return ret_val;
    }

    cdev_init(&g_char_device.cdev, &rdkfmac_fops);
    cdev_add(&g_char_device.cdev, MKDEV(RDKFMAC_MAJOR, 0), 1);
    g_char_device.class = class_create(THIS_MODULE, RDKFMAC_CLASS_NAME);
    if (IS_ERR(g_char_device.class)){
        printk(KERN_ALERT "cdrv : register device class failed\n");
        return PTR_ERR(g_char_device.class);
    }

    INIT_LIST_HEAD(&g_char_device.list_head);
    g_char_device.list_tail = &g_char_device.list_head;
    printk(KERN_INFO "%s:%d: registered successfully\n", __func__, __LINE__);
    g_char_device.tdev = MKDEV(RDKFMAC_MAJOR, 0);
    g_char_device.dev = device_create(g_char_device.class, NULL,
                g_char_device.tdev, NULL, RDKFMAC_DEVICE_NAME);

    return 0;
}

void cleanup_rdkfmac_cdev(void)
{
    device_destroy(g_char_device.class, g_char_device.tdev);
    class_destroy(g_char_device.class);
    cdev_del(&g_char_device.cdev);
    unregister_chrdev_region(MKDEV(RDKFMAC_MAJOR, 0), 1);

    printk(KERN_INFO "%s:%d: unregistered successfully\n", __func__, __LINE__);
}
#if 0
void push_to_char_device(wlan_emu_spec_data_t *data)
{
    wlan_emu_spec_data_entry_t   *entry;
    wlan_emu_spec_data_t    *spec;

    entry = kmalloc(sizeof(wlan_emu_spec_data_entry_t), GFP_KERNEL);
    spec = kmalloc(sizeof(wlan_emu_spec_data_t), GFP_KERNEL);
    entry->spec = spec;

    memcpy(spec, data, sizeof(wlan_emu_spec_data_t));

    printk("%s:%d: pushing data to queue, current size:%d\n", __func__, __LINE__,
        list_empty(&g_char_device.list_head));
    list_add(&entry->list_entry, g_char_device.list_tail);
    g_char_device.list_tail = &entry->list_entry;

    wake_up_interruptible(&rdkfmac_rq);
}

wlan_emu_spec_data_t  *pop_from_char_device(void)
{
    wlan_emu_spec_data_t *spec = NULL;
    wlan_emu_spec_data_entry_t *entry = NULL;

    if (g_char_device.list_tail == &g_char_device.list_head) {
        printk("%s:%d list is empty\n", __func__, __LINE__);
        return NULL;
    }

    entry = list_entry(g_char_device.list_tail, wlan_emu_spec_data_entry_t, list_entry);

    g_char_device.list_tail  = g_char_device.list_tail->prev;
    list_del(&entry->list_entry);

    spec = entry->spec;
    kfree(entry);

    return spec;
}
#endif
struct rdkfmac_device_data *get_char_device_data(void)
{
    return &g_char_device;
}

