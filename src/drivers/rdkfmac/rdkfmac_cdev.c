#include "rdkfmac.h"

struct rdkfmac_device_data g_char_device;
static DECLARE_WAIT_QUEUE_HEAD(rdkfmac_rq);
static wlan_emu_msg_data_t  *pop_from_char_device(void);
static unsigned int get_list_entries_count_in_char_device(void);

const char *rdkfmac_cfg80211_ops_type_to_string(wlan_emu_cfg80211_ops_type_t type)
{
#define CFG80211_TO_S(x) case x: return #x;
    switch (type) {
        CFG80211_TO_S(wlan_emu_cfg80211_ops_type_none)
        CFG80211_TO_S(wlan_emu_cfg80211_ops_type_add_intf)
        CFG80211_TO_S(wlan_emu_cfg80211_ops_type_del_intf)
        CFG80211_TO_S(wlan_emu_cfg80211_ops_type_change_intf)
        CFG80211_TO_S(wlan_emu_cfg80211_ops_type_start_ap)
        CFG80211_TO_S(wlan_emu_cfg80211_ops_type_change_beacon)
        CFG80211_TO_S(wlan_emu_cfg80211_ops_type_stop_ap)
        default:
            break;
    }

    return "wlan_emu_cfg80211_ops_type_unknown";
}

const char *rdkfmac_mac80211_ops_type_to_string(wlan_emu_mac80211_ops_type_t type)
{
#define MAC80211_TO_S(x) case x: return #x;
    switch (type) {
        MAC80211_TO_S(wlan_emu_mac80211_ops_type_none)
        MAC80211_TO_S(wlan_emu_mac80211_ops_type_tx)
        MAC80211_TO_S(wlan_emu_mac80211_ops_type_start)
        MAC80211_TO_S(wlan_emu_mac80211_ops_type_stop)
        MAC80211_TO_S(wlan_emu_mac80211_ops_type_add_intf)
        MAC80211_TO_S(wlan_emu_mac80211_ops_type_change_intf)
        MAC80211_TO_S(wlan_emu_mac80211_ops_type_remove_intf)
        MAC80211_TO_S(wlan_emu_mac80211_ops_type_config)
        MAC80211_TO_S(wlan_emu_mac80211_ops_type_bss_info_changed)
        MAC80211_TO_S(wlan_emu_mac80211_ops_type_start_ap)
        MAC80211_TO_S(wlan_emu_mac80211_ops_type_stop_ap)
        default:
            break;
    }

    return "wlan_emu_mac80211_ops_type_unknown";
}

const char *rdkfmac_emu80211_ops_type_to_string(wlan_emu_emu80211_ops_type_t type)
{
#define EMU80211_TO_S(x) case x: return #x;
    switch (type) {
        EMU80211_TO_S(wlan_emu_emu80211_ops_type_none)
        EMU80211_TO_S(wlan_emu_emu80211_ops_type_tctrl)
        EMU80211_TO_S(wlan_emu_emu80211_ops_type_close)
        default:
            break;
    }

    return "wlan_emu_emu80211_ops_type_unknown";
}


static unsigned int rdkfmac_poll(struct file *filp, struct poll_table_struct *wait)
{
    __poll_t mask = 0;

    poll_wait(filp, &rdkfmac_rq, wait);

    if (get_list_entries_count_in_char_device() != 0) {
           mask |= (POLLIN | POLLRDNORM);
    }

      return mask;
}

static ssize_t rdkfmac_write(struct file *file, const char __user *user_buffer,
                    size_t size, loff_t * offset)
{
    wlan_emu_msg_data_t    spec;
    ssize_t sz;


    if (copy_from_user(&spec, user_buffer, sizeof(wlan_emu_msg_data_t))) {
        printk("%s:%d: potential copy error\n", __func__, __LINE__);
        return 0;
    }

    sz = sizeof(wlan_emu_msg_data_t);

    if (spec.type != wlan_emu_msg_type_emu80211) {
        printk("%s:%d: received invalid control data\n", __func__, __LINE__);
        return sz;
    }

    switch (spec.u.emu80211.ops) {
    case wlan_emu_emu80211_ops_type_close:
    case wlan_emu_emu80211_ops_type_tctrl:
        push_to_char_device(&spec);
        break;
    default:
        break;
    }

    return sz;
}

static ssize_t rdkfmac_read(struct file *file, char __user *user_buffer,
                   size_t size, loff_t *offset)
{
    wlan_emu_msg_data_t *spec;

    spec = pop_from_char_device();
    if (spec == NULL) {
        printk("%s:%d: nothing to pop\n", __func__, __LINE__);
        return 0;
    }
    if (copy_to_user(user_buffer, spec, sizeof(wlan_emu_msg_data_t))) {
        printk("%s:%d: potential copy error\n", __func__, __LINE__);
        return 0;
    }

    kfree(spec);

    return sizeof(wlan_emu_msg_data_t);
}

static int rdkfmac_open(struct inode *inode, struct file *file)
{

    g_char_device.num_inst++;
       printk(KERN_INFO "%s:%d Opened Instances: %d\n", __func__, __LINE__, g_char_device.num_inst);

    return 0;
}

static int rdkfmac_release(struct inode *inode, struct file *file)
{
    if (g_char_device.num_inst > 0) {
        g_char_device.num_inst--;
    }

       printk(KERN_INFO "%s:%d Opened Instances: %d\n", __func__, __LINE__, g_char_device.num_inst);
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

    memset(&g_char_device, 0, sizeof(rdkfmac_device_data_t));

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

unsigned int get_list_entries_count_in_char_device(void)
{
    unsigned count = 0;
    struct list_head *ptr = &g_char_device.list_head;

    for (ptr = &g_char_device.list_head; ptr != g_char_device.list_tail; ptr = ptr->next) {
        count++;
    }

    return count;
}

void push_to_char_device(wlan_emu_msg_data_t *data)
{
    wlan_emu_msg_data_entry_t   *entry;
    wlan_emu_msg_data_t    *spec;
    char    str_spec_type[32];
    char    str_ops[128];

    // do not push to list if nobody is listening
    if (g_char_device.num_inst == 0) {
        return;
    }

    entry = kmalloc(sizeof(wlan_emu_msg_data_entry_t), GFP_KERNEL);
    spec = kmalloc(sizeof(wlan_emu_msg_data_t), GFP_KERNEL);
    entry->spec = spec;

    memcpy(spec, data, sizeof(wlan_emu_msg_data_t));

    switch (spec->type) {
    case wlan_emu_msg_type_cfg80211:
        strcpy(str_spec_type, "cfg80211");
        strcpy(str_ops, rdkfmac_cfg80211_ops_type_to_string(spec->u.cfg80211.ops));
        break;
    case wlan_emu_msg_type_mac80211:
        strcpy(str_spec_type, "mac80211");
        strcpy(str_ops, rdkfmac_mac80211_ops_type_to_string(spec->u.mac80211.ops));
        break;
    case wlan_emu_msg_type_emu80211:
        strcpy(str_spec_type, "emu80211");
        strcpy(str_ops, rdkfmac_emu80211_ops_type_to_string(spec->u.emu80211.ops));
        break;
    default:
        break;
    }

    printk("%s:%d: pushing data to queue, type: %s ops: %s current size: %d\n", __func__, __LINE__,
        str_spec_type, str_ops, get_list_entries_count_in_char_device());
    list_add(&entry->list_entry, g_char_device.list_tail);
    g_char_device.list_tail = &entry->list_entry;

    wake_up_interruptible(&rdkfmac_rq);
}

wlan_emu_msg_data_t  *pop_from_char_device(void)
{
    wlan_emu_msg_data_t *spec = NULL;
    wlan_emu_msg_data_entry_t *entry = NULL;

    if (g_char_device.list_tail == &g_char_device.list_head) {
        printk("%s:%d list is empty\n", __func__, __LINE__);
        return NULL;
    }

    entry = list_entry(g_char_device.list_tail, wlan_emu_msg_data_entry_t, list_entry);

    g_char_device.list_tail  = g_char_device.list_tail->prev;
    list_del(&entry->list_entry);

    spec = entry->spec;
    kfree(entry);

    return spec;
}

struct rdkfmac_device_data *get_char_device_data(void)
{
    return &g_char_device;
}

