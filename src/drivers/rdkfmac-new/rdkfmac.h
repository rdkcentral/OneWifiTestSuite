#ifndef RDKFMAC_H
#define RDKFMAC_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/ip.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/etherdevice.h>
#include <net/sock.h>
#include <net/lib80211.h>
#include <net/cfg80211.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <linux/ctype.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/rtnetlink.h>
#include <linux/net_tstamp.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <net/rtnetlink.h>
#include <linux/u64_stats_sync.h>
#include <linux/cdev.h>

#define NETDEV_DRV_NAME "rdkfmac"

#define RDKFMAC_MAJOR       42
#define RDKFMAC_MAX_MINORS  1
#define BUF_LEN 256
#define RDKFMAC_DEVICE_NAME "rdkfmac_dev"
#define RDKFMAC_DEVICE_DRIVER_NAME  "rdkfmac_device_driver"
#define RDKFMAC_CLASS_NAME "rdkfmac_class"
#define RDKFMAC_WDOG_TIMEOUT    5
#define RDKFMAC_PRIMARY_VIF_IDX 0
#define RDKFMAC_MAX_MAC         3
#define RDKFMAC_MAX_INTF        8
#define RDKFMAC_MAX_VSIE_LEN       255

enum rdkfmac_hw_capab {
    RDKFMAC_HW_CAPAB_REG_UPDATE = 0,
    RDKFMAC_HW_CAPAB_STA_INACT_TIMEOUT,
    RDKFMAC_HW_CAPAB_DFS_OFFLOAD,
    RDKFMAC_HW_CAPAB_SCAN_RANDOM_MAC_ADDR,
    RDKFMAC_HW_CAPAB_PWR_MGMT,
    RDKFMAC_HW_CAPAB_OBSS_SCAN,
    RDKFMAC_HW_CAPAB_SCAN_DWELL,
    RDKFMAC_HW_CAPAB_SAE,
    RDKFMAC_HW_CAPAB_HW_BRIDGE,
    RDKFMAC_HW_CAPAB_NUM
};

typedef struct rdkfmac_hw_info {
    u32 ql_proto_ver;
    u8 num_mac;
    u8 mac_bitmap;
    u32 fw_ver;
    u8 total_tx_chain;
    u8 total_rx_chain;
    char fw_version[ETHTOOL_FWVERS_LEN];
    u32 hw_version;
    u8 hw_capab[RDKFMAC_HW_CAPAB_NUM / BITS_PER_BYTE + 1];
} rdkfmac_hw_info_t;

typedef struct rdkfmac_device_data {
    struct cdev cdev;
    char buffer[BUF_LEN];
    size_t size;
    struct class*  rdkfmac_class;
    struct device* rdkfmac_dev;
} rdkfmac_device_data_t;

struct rdkfmac_wmac;
struct rdkfmac_bus;

typedef struct rdkfmac_vif {
    struct wireless_dev wdev;
    u8 bssid[ETH_ALEN];
    u8 mac_addr[ETH_ALEN];
    u8 vifid;
    struct net_device *netdev;
    struct rdkfmac_wmac *mac;
    struct work_struct reset_work;
    struct work_struct high_pri_tx_work;
    struct sk_buff_head high_pri_tx_queue;
    unsigned long cons_tx_timeout_cnt;
    int generation;
} rdkfmac_vif_t;

typedef struct {
    u8 bands_cap;
    u8 num_tx_chain;
    u8 num_rx_chain;
    u16 max_ap_assoc_sta;
    u32 frag_thr;
    u32 rts_thr;
    u8 lretry_limit;
    u8 sretry_limit;
    u8 coverage_class;
    u8 radar_detect_widths;
    u8 max_scan_ssids;
    u16 max_acl_mac_addrs;
    struct ieee80211_ht_cap ht_cap_mod_mask;
    struct ieee80211_vht_cap vht_cap_mod_mask;
    struct ieee80211_iface_combination *if_comb;
    size_t n_if_comb;
    u8 *extended_capabilities;
    u8 *extended_capabilities_mask;
    u8 extended_capabilities_len;
    struct wiphy_wowlan_support *wowlan;
} rdkfmac_mac_info_t;

typedef struct rdkfmac_wmac {
    u8 macid;
    u8 wiphy_registered;
    u8 macaddr[ETH_ALEN];
    struct rdkfmac_bus *bus;
    rdkfmac_mac_info_t macinfo;
    struct rdkfmac_vif iflist[RDKFMAC_MAX_INTF];
    struct cfg80211_scan_request *scan_req;
    struct mutex mac_lock;  /* lock during wmac speicific ops */
    struct delayed_work scan_timeout;
    struct ieee80211_regdomain *rd;
    struct platform_device *pdev;
} rdkfmac_wmac_t;

typedef enum {
    RDKFMAC_FW_STATE_DETACHED,
    RDKFMAC_FW_STATE_BOOT_DONE,
    RDKFMAC_FW_STATE_ACTIVE,
    RDKFMAC_FW_STATE_RUNNING,
    RDKFMAC_FW_STATE_DEAD,
} rdkfmac_fw_state_t;

struct rdkfmac_frame_meta_info {
    u8 magic_s;
    u8 ifidx;
    u8 macid;
    u8 magic_e;
} __packed;

struct rdkfmac_bus;

typedef struct rdkfmac_bus_ops {
    /* mgmt methods */
    int (*preinit)(struct rdkfmac_bus *);
    void (*stop)(struct rdkfmac_bus *);

    /* control path methods */
    int (*control_tx)(struct rdkfmac_bus *, struct sk_buff *);

    /* data xfer methods */
    int (*data_tx)(struct rdkfmac_bus *bus, struct sk_buff *skb,
               unsigned int macid, unsigned int vifid);
    void (*data_tx_timeout)(struct rdkfmac_bus *, struct net_device *);
    void (*data_tx_use_meta_set)(struct rdkfmac_bus *bus, bool use_meta);
    void (*data_rx_start)(struct rdkfmac_bus *);
    void (*data_rx_stop)(struct rdkfmac_bus *);
} rdkfmac_bus_ops_t;

typedef struct rdkfmac_cmd_ctl_node {
    struct completion cmd_resp_completion;
    struct sk_buff *resp_skb;
    u16 seq_num;
    bool waiting_for_resp;
    spinlock_t resp_lock; /* lock for resp_skb & waiting_for_resp changes */
} rdkfmac_cmd_ctl_node_t;

typedef struct rdkfmac_qlink_transport {
    rdkfmac_cmd_ctl_node_t curr_cmd;
    struct sk_buff_head event_queue;
    size_t event_queue_max_len;
} rdkfmac_qlink_transport_t;

typedef struct rdkfmac_bus {
    struct device *dev;
    rdkfmac_fw_state_t fw_state;
    u32 chip;
    u32 chiprev;
    struct rdkfmac_bus_ops *bus_ops;
    rdkfmac_wmac_t *mac[RDKFMAC_MAX_MAC];
    rdkfmac_cmd_ctl_node_t trans;
    struct rdkfmac_hw_info hw_info;
    struct napi_struct mux_napi;
    struct net_device mux_dev;
    struct workqueue_struct *workqueue;
    struct workqueue_struct *hprio_workqueue;
    struct work_struct fw_work;
    struct work_struct event_work;
    struct mutex bus_lock; /* lock during command/event processing */
    struct dentry *dbg_dir;
    struct notifier_block netdev_nb;
    u8 hw_id[ETH_ALEN];
    /* bus private data */
    char bus_priv[] __aligned(sizeof(void *));
} rdkfmac_bus_t;

int init_rdkfmac_cdev(void);
void cleanup_rdkfmac_cdev(void);
void rdkfmac_bus_pseudo_init(rdkfmac_bus_t *bus);
rdkfmac_bus_t *rdkfmac_get_bus(void);
struct wiphy *rdkfmac_wiphy_allocate(rdkfmac_bus_t *bus, struct platform_device *pdev);

static inline rdkfmac_vif_t *rdkfmac_netdev_get_priv(struct net_device *dev)
{
    return *((void **)netdev_priv(dev));
}

static inline bool rdkfmac_dfs_offload_get(void)
{
    return false;
}

static inline bool rdkfmac_hwcap_is_set(rdkfmac_hw_info_t *hw_info, unsigned int bit)
{
    return false;
}

#endif // RDKFMAC_H
