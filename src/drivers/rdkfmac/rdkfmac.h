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
#include <linux/ethtool.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/rtnetlink.h>
#include <linux/net_tstamp.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/list.h>
#include <net/rtnetlink.h>
#include <linux/u64_stats_sync.h>
#include <linux/cdev.h>
#include <net/mac80211.h>
#include <linux/types.h>
#include <linux/if_ether.h>
#include <linux/average.h>
#include <linux/bitfield.h>
#include <linux/rhashtable.h>
#include <net/fq.h>

#include "key.h"
#include "debug.h"
#include "wlan_emu_msg_data.h"

#define NETDEV_DRV_NAME "rdkfmac"

extern const struct cfg80211_ops rdkfmac_config_ops;

extern const void *const rdkfmac_wiphy_privid; /* for wiphy privid */

struct ieee80211_local;

#define RDKFMAC_MAJOR       42
#define BUF_LEN 256
#define RDKFMAC_DEVICE_NAME "rdkfmac_dev"
#define RDKFMAC_DEVICE_DRIVER_NAME  "rdkfmac_device_driver"
#define RDKFMAC_CLASS_NAME "rdkfmac_class"
#define RDKFMAC_WDOG_TIMEOUT    5
#define RDKFMAC_PRIMARY_VIF_IDX 0
#define RDKFMAC_MAX_MAC         3
#define RDKFMAC_MAX_INTF        8
#define RDKFMAC_MAX_VSIE_LEN    255

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

typedef struct {
    wlan_emu_msg_data_t *spec;
    struct list_head    list_entry;
} wlan_emu_msg_data_entry_t;

typedef struct rdkfmac_device_data {
    struct cdev cdev;
    struct class*  class;
    struct device* dev;
    struct list_head list_head;
    struct list_head *list_tail;
    dev_t   tdev;
    signed int num_inst;
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

#define CHAN2G(_freq)  { \
    .band = NL80211_BAND_2GHZ, \
    .center_freq = (_freq), \
    .hw_value = (_freq), \
    .max_power = 20, \
}

#define CHAN5G(_freq) { \
    .band = NL80211_BAND_5GHZ, \
    .center_freq = (_freq), \
    .hw_value = (_freq), \
    .max_power = 20, \
}

#define CHAN6G(_freq) { \
    .band = NL80211_BAND_6GHZ, \
    .center_freq = (_freq), \
    .hw_value = (_freq), \
}

#define NUM_S1G_CHANS_US 51

static const struct ieee80211_channel rdkfmac_channels_2ghz[] = {
    CHAN2G(2412), /* Channel 1 */
    CHAN2G(2417), /* Channel 2 */
    CHAN2G(2422), /* Channel 3 */
    CHAN2G(2427), /* Channel 4 */
    CHAN2G(2432), /* Channel 5 */
    CHAN2G(2437), /* Channel 6 */
    CHAN2G(2442), /* Channel 7 */
    CHAN2G(2447), /* Channel 8 */
    CHAN2G(2452), /* Channel 9 */
    CHAN2G(2457), /* Channel 10 */
    CHAN2G(2462), /* Channel 11 */
    CHAN2G(2467), /* Channel 12 */
    CHAN2G(2472), /* Channel 13 */
    CHAN2G(2484), /* Channel 14 */
};

static const struct ieee80211_channel rdkfmac_channels_5ghz[] = {
    CHAN5G(5180), /* Channel 36 */
    CHAN5G(5200), /* Channel 40 */
    CHAN5G(5220), /* Channel 44 */
    CHAN5G(5240), /* Channel 48 */

    CHAN5G(5260), /* Channel 52 */
    CHAN5G(5280), /* Channel 56 */
    CHAN5G(5300), /* Channel 60 */
    CHAN5G(5320), /* Channel 64 */

    CHAN5G(5500), /* Channel 100 */
    CHAN5G(5520), /* Channel 104 */
    CHAN5G(5540), /* Channel 108 */
    CHAN5G(5560), /* Channel 112 */
    CHAN5G(5580), /* Channel 116 */
    CHAN5G(5600), /* Channel 120 */
    CHAN5G(5620), /* Channel 124 */
    CHAN5G(5640), /* Channel 128 */
    CHAN5G(5660), /* Channel 132 */
    CHAN5G(5680), /* Channel 136 */
    CHAN5G(5700), /* Channel 140 */

    CHAN5G(5745), /* Channel 149 */
    CHAN5G(5765), /* Channel 153 */
    CHAN5G(5785), /* Channel 157 */
    CHAN5G(5805), /* Channel 161 */
    CHAN5G(5825), /* Channel 165 */
    CHAN5G(5845), /* Channel 169 */
};

static const struct ieee80211_channel rdkfmac_channels_6ghz[] = {
    CHAN6G(5955), /* Channel 1 */
    CHAN6G(5975), /* Channel 5 */
    CHAN6G(5995), /* Channel 9 */
    CHAN6G(6015), /* Channel 13 */
    CHAN6G(6035), /* Channel 17 */
    CHAN6G(6055), /* Channel 21 */
    CHAN6G(6075), /* Channel 25 */
    CHAN6G(6095), /* Channel 29 */
    CHAN6G(6115), /* Channel 33 */
    CHAN6G(6135), /* Channel 37 */
    CHAN6G(6155), /* Channel 41 */
    CHAN6G(6175), /* Channel 45 */
    CHAN6G(6195), /* Channel 49 */
    CHAN6G(6215), /* Channel 53 */
    CHAN6G(6235), /* Channel 57 */
    CHAN6G(6255), /* Channel 61 */
    CHAN6G(6275), /* Channel 65 */
    CHAN6G(6295), /* Channel 69 */
    CHAN6G(6315), /* Channel 73 */
    CHAN6G(6335), /* Channel 77 */
    CHAN6G(6355), /* Channel 81 */
    CHAN6G(6375), /* Channel 85 */
    CHAN6G(6395), /* Channel 89 */
    CHAN6G(6415), /* Channel 93 */
    CHAN6G(6435), /* Channel 97 */
    CHAN6G(6455), /* Channel 181 */
    CHAN6G(6475), /* Channel 105 */
    CHAN6G(6495), /* Channel 109 */
    CHAN6G(6515), /* Channel 113 */
    CHAN6G(6535), /* Channel 117 */
    CHAN6G(6555), /* Channel 121 */
    CHAN6G(6575), /* Channel 125 */
    CHAN6G(6595), /* Channel 129 */
    CHAN6G(6615), /* Channel 133 */
    CHAN6G(6635), /* Channel 137 */
    CHAN6G(6655), /* Channel 141 */
    CHAN6G(6675), /* Channel 145 */
    CHAN6G(6695), /* Channel 149 */
    CHAN6G(6715), /* Channel 153 */
    CHAN6G(6735), /* Channel 157 */
    CHAN6G(6755), /* Channel 161 */
    CHAN6G(6775), /* Channel 165 */
    CHAN6G(6795), /* Channel 169 */
    CHAN6G(6815), /* Channel 173 */
    CHAN6G(6835), /* Channel 177 */
    CHAN6G(6855), /* Channel 181 */
    CHAN6G(6875), /* Channel 185 */
    CHAN6G(6895), /* Channel 189 */
    CHAN6G(6915), /* Channel 193 */
    CHAN6G(6935), /* Channel 197 */
    CHAN6G(6955), /* Channel 201 */
    CHAN6G(6975), /* Channel 205 */
    CHAN6G(6995), /* Channel 209 */
    CHAN6G(7015), /* Channel 213 */
    CHAN6G(7035), /* Channel 217 */
    CHAN6G(7055), /* Channel 221 */
    CHAN6G(7075), /* Channel 225 */
    CHAN6G(7095), /* Channel 229 */
    CHAN6G(7115), /* Channel 233 */
};

static struct ieee80211_channel rdkfmac_channels_s1g[NUM_S1G_CHANS_US];

static const struct ieee80211_rate rdkfmac_rates[] = {
    { .bitrate = 10 },
    { .bitrate = 20, .flags = IEEE80211_RATE_SHORT_PREAMBLE },
    { .bitrate = 55, .flags = IEEE80211_RATE_SHORT_PREAMBLE },
    { .bitrate = 110, .flags = IEEE80211_RATE_SHORT_PREAMBLE },
    { .bitrate = 60 },
    { .bitrate = 90 },
    { .bitrate = 120 },
    { .bitrate = 180 },
    { .bitrate = 240 },
    { .bitrate = 360 },
    { .bitrate = 480 },
    { .bitrate = 540 }
};

static const u32 rdkfmac_ciphers[] = {
    WLAN_CIPHER_SUITE_TKIP,
    WLAN_CIPHER_SUITE_CCMP,
    WLAN_CIPHER_SUITE_CCMP_256,
    WLAN_CIPHER_SUITE_GCMP,
    WLAN_CIPHER_SUITE_GCMP_256,
    WLAN_CIPHER_SUITE_AES_CMAC,
};

struct rdkfmac_sta_priv {
    u32 magic;
};

struct rdkfmac_vif_priv {
    u32 magic;
    u8 bssid[ETH_ALEN];
    bool assoc;
    bool bcn_en;
    u16 aid;
};

struct mac80211_rdkfmac_data {
    struct list_head list;
    struct rhash_head rht;
    struct ieee80211_hw *hw;
    struct device *dev;
    struct ieee80211_supported_band bands[NUM_NL80211_BANDS];
    struct ieee80211_channel channels_2ghz[ARRAY_SIZE(rdkfmac_channels_2ghz)];
    struct ieee80211_channel channels_5ghz[ARRAY_SIZE(rdkfmac_channels_5ghz)];
    struct ieee80211_channel channels_6ghz[ARRAY_SIZE(rdkfmac_channels_6ghz)];
    struct ieee80211_channel channels_s1g[ARRAY_SIZE(rdkfmac_channels_s1g)];
    struct ieee80211_rate rates[ARRAY_SIZE(rdkfmac_rates)];
    struct ieee80211_iface_combination if_combination;
    struct ieee80211_iface_limit if_limits[3];
    int n_if_limits;

    u32 ciphers[ARRAY_SIZE(rdkfmac_ciphers)];

    struct mac_address addresses[2];
    struct ieee80211_chanctx_conf *chanctx;
    int channels, idx;
    bool use_chanctx;
    bool destroy_on_close;
    u32 portid;
    char alpha2[2];
    const struct ieee80211_regdomain *regd;

    struct ieee80211_channel *tmp_chan;
    struct ieee80211_channel *roc_chan;
    u32 roc_duration;
    struct delayed_work roc_start;
    struct delayed_work roc_done;
    struct delayed_work hw_scan;
    struct cfg80211_scan_request *hw_scan_request;
    struct ieee80211_vif *hw_scan_vif;
    int scan_chan_idx;
    u8 scan_addr[ETH_ALEN];
    struct {
        struct ieee80211_channel *channel;
        unsigned long next_start, start, end;
    } survey_data[ARRAY_SIZE(rdkfmac_channels_2ghz) +
              ARRAY_SIZE(rdkfmac_channels_5ghz) +
              ARRAY_SIZE(rdkfmac_channels_6ghz)];

    struct ieee80211_channel *channel;
    enum nl80211_chan_width bw;
    u64 beacon_int;
    unsigned int rx_filter;
    bool started, idle, scanning;
    struct mutex mutex;
    struct hrtimer beacon_timer;
    enum ps_mode {
        PS_DISABLED, PS_ENABLED, PS_AUTO_POLL, PS_MANUAL_POLL
    } ps;
    bool ps_poll_pending;
    struct dentry *debugfs;

    atomic_t pending_cookie;
    struct sk_buff_head pending;
    u64 group;

    int netgroup;
    u32 wmediumd;

    s64 tsf_offset;
    s64 bcn_delta;
    u64 abs_bcn_ts;

    u64 tx_pkts;
    u64 rx_pkts;
    u64 tx_bytes;
    u64 rx_bytes;
    u64 tx_dropped;
    u64 tx_failed;

    int rx_rssi;
};


int init_rdkfmac_cdev(void);
void cleanup_rdkfmac_cdev(void);
void rdkfmac_bus_pseudo_init(rdkfmac_bus_t *bus);
rdkfmac_bus_t *rdkfmac_get_bus(void);
struct wiphy *rdkfmac_wiphy_allocate(rdkfmac_bus_t *bus, struct platform_device *pdev);
struct ieee80211_ops *rdkfmac_get_ieee80211_ops(void);
struct rdkfmac_device_data *get_char_device_data(void);
void push_to_char_device(wlan_emu_msg_data_t *data);

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

//------------------------sta_info.h
enum ieee80211_sta_info_flags {
    WLAN_STA_AUTH,
    WLAN_STA_ASSOC,
    WLAN_STA_PS_STA,
    WLAN_STA_AUTHORIZED,
    WLAN_STA_SHORT_PREAMBLE,
    WLAN_STA_WDS,
    WLAN_STA_CLEAR_PS_FILT,
    WLAN_STA_MFP,
    WLAN_STA_BLOCK_BA,
    WLAN_STA_PS_DRIVER,
    WLAN_STA_PSPOLL,
    WLAN_STA_TDLS_PEER,
    WLAN_STA_TDLS_PEER_AUTH,
    WLAN_STA_TDLS_INITIATOR,
    WLAN_STA_TDLS_CHAN_SWITCH,
    WLAN_STA_TDLS_OFF_CHANNEL,
    WLAN_STA_TDLS_WIDER_BW,
    WLAN_STA_UAPSD,
    WLAN_STA_SP,
    WLAN_STA_4ADDR_EVENT,
    WLAN_STA_INSERTED,
    WLAN_STA_RATE_CONTROL,
    WLAN_STA_TOFFSET_KNOWN,
    WLAN_STA_MPSP_OWNER,
    WLAN_STA_MPSP_RECIPIENT,
    WLAN_STA_PS_DELIVER,
    WLAN_STA_USES_ENCRYPTION,
    WLAN_STA_DECAP_OFFLOAD,

    NUM_WLAN_STA_FLAGS,
};

#define ADDBA_RESP_INTERVAL HZ
#define HT_AGG_MAX_RETRIES      15
#define HT_AGG_BURST_RETRIES        3
#define HT_AGG_RETRIES_PERIOD       (15 * HZ)

#define HT_AGG_STATE_DRV_READY      0
#define HT_AGG_STATE_RESPONSE_RECEIVED  1
#define HT_AGG_STATE_OPERATIONAL    2
#define HT_AGG_STATE_STOPPING       3
#define HT_AGG_STATE_WANT_START     4
#define HT_AGG_STATE_WANT_STOP      5
#define HT_AGG_STATE_START_CB       6
#define HT_AGG_STATE_STOP_CB        7
#define HT_AGG_STATE_SENT_ADDBA     8

DECLARE_EWMA(avg_signal, 10, 8)
enum ieee80211_agg_stop_reason {
    AGG_STOP_DECLINED,
    AGG_STOP_LOCAL_REQUEST,
    AGG_STOP_PEER_REQUEST,
    AGG_STOP_DESTROY_STA,
};

/* Debugfs flags to enable/disable use of RX/TX airtime in scheduler */
#define AIRTIME_USE_TX      BIT(0)
#define AIRTIME_USE_RX      BIT(1)


struct airtime_info {
    u64 rx_airtime;
    u64 tx_airtime;
    u64 v_t;
    u64 last_scheduled;
    struct list_head list;
    atomic_t aql_tx_pending; /* Estimated airtime for frames pending */
    u32 aql_limit_low;
    u32 aql_limit_high;
    u32 weight_reciprocal;
    u16 weight;
};

void ieee80211_sta_update_pending_airtime(struct ieee80211_local *local,
                      struct sta_info *sta, u8 ac,
                      u16 tx_airtime, bool tx_completed);
void ieee80211_register_airtime(struct ieee80211_txq *txq,
                u32 tx_airtime, u32 rx_airtime);

struct sta_info;

struct tid_ampdu_tx {
    struct rcu_head rcu_head;
    struct timer_list session_timer;
    struct timer_list addba_resp_timer;
    struct sk_buff_head pending;
    struct sta_info *sta;
    unsigned long state;
    unsigned long last_tx;
    u16 timeout;
    u8 dialog_token;
    u8 stop_initiator;
    bool tx_stop;
    u16 buf_size;
    u16 ssn;

    u16 failed_bar_ssn;
    bool bar_pending;
    bool amsdu;
    u8 tid;
};

struct tid_ampdu_rx {
    struct rcu_head rcu_head;
    spinlock_t reorder_lock;
    u64 reorder_buf_filtered;
    struct sk_buff_head *reorder_buf;
    unsigned long *reorder_time;
    struct sta_info *sta;
    struct timer_list session_timer;
    struct timer_list reorder_timer;
    unsigned long last_rx;
    u16 head_seq_num;
    u16 stored_mpdu_num;
    u16 ssn;
    u16 buf_size;
    u16 timeout;
    u8 tid;
    u8 auto_seq:1,
       removed:1,
       started:1;
};

struct sta_ampdu_mlme {
    struct mutex mtx;
    /* rx */
    struct tid_ampdu_rx __rcu *tid_rx[IEEE80211_NUM_TIDS];
    u8 tid_rx_token[IEEE80211_NUM_TIDS];
    unsigned long tid_rx_timer_expired[BITS_TO_LONGS(IEEE80211_NUM_TIDS)];
    unsigned long tid_rx_stop_requested[BITS_TO_LONGS(IEEE80211_NUM_TIDS)];
    unsigned long tid_rx_manage_offl[BITS_TO_LONGS(2 * IEEE80211_NUM_TIDS)];
    unsigned long agg_session_valid[BITS_TO_LONGS(IEEE80211_NUM_TIDS)];
    unsigned long unexpected_agg[BITS_TO_LONGS(IEEE80211_NUM_TIDS)];
    /* tx */
    struct work_struct work;
    struct tid_ampdu_tx __rcu *tid_tx[IEEE80211_NUM_TIDS];
    struct tid_ampdu_tx *tid_start_tx[IEEE80211_NUM_TIDS];
    unsigned long last_addba_req_time[IEEE80211_NUM_TIDS];
    u8 addba_req_num[IEEE80211_NUM_TIDS];
    u8 dialog_token_allocator;
};


/* Value to indicate no TID reservation */
#define IEEE80211_TID_UNRESERVED    0xff

#define IEEE80211_FAST_XMIT_MAX_IV  18

struct ieee80211_fast_tx {
    struct ieee80211_key *key;
    u8 hdr_len;
    u8 sa_offs, da_offs, pn_offs;
    u8 band;
    u8 hdr[30 + 2 + IEEE80211_FAST_XMIT_MAX_IV +
           sizeof(rfc1042_header)] __aligned(2);

    struct rcu_head rcu_head;
};

struct ieee80211_fast_rx {
    struct net_device *dev;
    enum nl80211_iftype vif_type;
    u8 vif_addr[ETH_ALEN] __aligned(2);
    u8 rfc1042_hdr[6] __aligned(2);
    __be16 control_port_protocol;
    __le16 expected_ds_bits;
    u8 icv_len;
    u8 key:1,
       internal_forward:1,
       uses_rss:1;
    u8 da_offs, sa_offs;

    struct rcu_head rcu_head;
};

/* we use only values in the range 0-100, so pick a large precision */
DECLARE_EWMA(mesh_fail_avg, 20, 8)
DECLARE_EWMA(mesh_tx_rate_avg, 8, 16)

struct mesh_sta {
    struct timer_list plink_timer;
    struct sta_info *plink_sta;

    s64 t_offset;
    s64 t_offset_setpoint;

    spinlock_t plink_lock;
    u16 llid;
    u16 plid;
    u16 aid;
    u16 reason;
    u8 plink_retries;

    bool processed_beacon;
    bool connected_to_gate;
    bool connected_to_as;

    enum nl80211_plink_state plink_state;
    u32 plink_timeout;

    /* mesh power save */
    enum nl80211_mesh_power_mode local_pm;
    enum nl80211_mesh_power_mode peer_pm;
    enum nl80211_mesh_power_mode nonpeer_pm;

    /* moving percentage of failed MSDUs */
    struct ewma_mesh_fail_avg fail_avg;
    /* moving average of tx bitrate */
    struct ewma_mesh_tx_rate_avg tx_rate_avg;
};

DECLARE_EWMA(signal, 10, 8)

struct ieee80211_sta_rx_stats {
    unsigned long packets;
    unsigned long last_rx;
    unsigned long num_duplicates;
    unsigned long fragments;
    unsigned long dropped;
    int last_signal;
    u8 chains;
    s8 chain_signal_last[IEEE80211_MAX_CHAINS];
    u32 last_rate;
    struct u64_stats_sync syncp;
    u64 bytes;
    u64 msdu[IEEE80211_NUM_TIDS + 1];
};

#define IEEE80211_FRAGMENT_MAX 4

struct ieee80211_fragment_entry {
    struct sk_buff_head skb_list;
    unsigned long first_frag_time;
    u16 seq;
    u16 extra_len;
    u16 last_frag;
    u8 rx_queue;
    u8 check_sequential_pn:1, /* needed for CCMP/GCMP */
       is_protected:1;
    u8 last_pn[6]; /* PN of the last fragment if CCMP was used */
    unsigned int key_color;
};

struct ieee80211_fragment_cache {
    struct ieee80211_fragment_entry entries[IEEE80211_FRAGMENT_MAX];
    unsigned int next;
};

#define STA_SLOW_THRESHOLD 6000 /* 6 Mbps */

struct link_sta_info {
    u8 addr[ETH_ALEN];
    u8 link_id;

    /* TODO rhash head/node for finding link_sta based on addr */

    struct sta_info *sta;
    struct ieee80211_key __rcu *gtk[NUM_DEFAULT_KEYS +
                    NUM_DEFAULT_MGMT_KEYS +
                    NUM_DEFAULT_BEACON_KEYS];
    struct ieee80211_sta_rx_stats __percpu *pcpu_rx_stats;

    /* Updated from RX path only, no locking requirements */
    struct ieee80211_sta_rx_stats rx_stats;
    struct {
        struct ewma_signal signal;
        struct ewma_signal chain_signal[IEEE80211_MAX_CHAINS];
    } rx_stats_avg;

    /* Updated from TX status path only, no locking requirements */
    struct {
        unsigned long filtered;
        unsigned long retry_failed, retry_count;
        unsigned int lost_packets;
        unsigned long last_pkt_time;
        u64 msdu_retries[IEEE80211_NUM_TIDS + 1];
        u64 msdu_failed[IEEE80211_NUM_TIDS + 1];
        unsigned long last_ack;
        s8 last_ack_signal;
        bool ack_signal_filled;
        struct ewma_avg_signal avg_ack_signal;
    } status_stats;

    /* Updated from TX path only, no locking requirements */
    struct {
        u64 packets[IEEE80211_NUM_ACS];
        u64 bytes[IEEE80211_NUM_ACS];
        struct ieee80211_tx_rate last_rate;
        struct rate_info last_rate_info;
        u64 msdu[IEEE80211_NUM_TIDS + 1];
    } tx_stats;

    enum ieee80211_sta_rx_bandwidth cur_max_bandwidth;
};

struct sta_info {
    /* General information, mostly static */
    struct list_head list, free_list;
    struct rcu_head rcu_head;
    struct rhlist_head hash_node;
    u8 addr[ETH_ALEN];
    struct ieee80211_local *local;
    struct ieee80211_sub_if_data *sdata;
    struct ieee80211_key __rcu *ptk[NUM_DEFAULT_KEYS];
    u8 ptk_idx;
    struct rate_control_ref *rate_ctrl;
    void *rate_ctrl_priv;
    spinlock_t rate_ctrl_lock;
    spinlock_t lock;

    struct ieee80211_fast_tx __rcu *fast_tx;
    struct ieee80211_fast_rx __rcu *fast_rx;

#ifdef CONFIG_MAC80211_MESH
    struct mesh_sta *mesh;
#endif

    struct work_struct drv_deliver_wk;

    u16 listen_interval;

    bool dead;
    bool removed;

    bool uploaded;

    enum ieee80211_sta_state sta_state;

    /* use the accessors defined below */
    unsigned long _flags;

    /* STA powersave lock and frame queues */
    spinlock_t ps_lock;
    struct sk_buff_head ps_tx_buf[IEEE80211_NUM_ACS];
    struct sk_buff_head tx_filtered[IEEE80211_NUM_ACS];
    unsigned long driver_buffered_tids;
    unsigned long txq_buffered_tids;

    u64 assoc_at;
    long last_connected;

    /* Plus 1 for non-QoS frames */
    __le16 last_seq_ctrl[IEEE80211_NUM_TIDS + 1];

    u16 tid_seq[IEEE80211_QOS_CTL_TID_MASK + 1];

    struct airtime_info airtime[IEEE80211_NUM_ACS];

    /*
     * Aggregation information, locked with lock.
     */
    struct sta_ampdu_mlme ampdu_mlme;

#ifdef CONFIG_MAC80211_DEBUGFS
    struct dentry *debugfs_dir;
#endif

    enum ieee80211_smps_mode known_smps_mode;
    const struct ieee80211_cipher_scheme *cipher_scheme;

    struct codel_params cparams;

    u8 reserved_tid;

    struct cfg80211_chan_def tdls_chandef;

    struct ieee80211_fragment_cache frags;

    bool multi_link_sta;
    struct link_sta_info deflink;
    struct link_sta_info *link[MAX_STA_LINKS];

    /* keep last! */
    struct ieee80211_sta sta;
};

static inline enum nl80211_plink_state sta_plink_state(struct sta_info *sta)
{
#ifdef CONFIG_MAC80211_MESH
    return sta->mesh->plink_state;
#endif
    return NL80211_PLINK_LISTEN;
}

static inline void set_sta_flag(struct sta_info *sta,
                enum ieee80211_sta_info_flags flag)
{
    WARN_ON(flag == WLAN_STA_AUTH ||
        flag == WLAN_STA_ASSOC ||
        flag == WLAN_STA_AUTHORIZED);
    set_bit(flag, &sta->_flags);
}

static inline void clear_sta_flag(struct sta_info *sta,
                  enum ieee80211_sta_info_flags flag)
{
    WARN_ON(flag == WLAN_STA_AUTH ||
        flag == WLAN_STA_ASSOC ||
        flag == WLAN_STA_AUTHORIZED);
    clear_bit(flag, &sta->_flags);
}

static inline int test_sta_flag(struct sta_info *sta,
                enum ieee80211_sta_info_flags flag)
{
    return test_bit(flag, &sta->_flags);
}

static inline int test_and_clear_sta_flag(struct sta_info *sta,
                      enum ieee80211_sta_info_flags flag)
{
    WARN_ON(flag == WLAN_STA_AUTH ||
        flag == WLAN_STA_ASSOC ||
        flag == WLAN_STA_AUTHORIZED);
    return test_and_clear_bit(flag, &sta->_flags);
}

static inline int test_and_set_sta_flag(struct sta_info *sta,
                    enum ieee80211_sta_info_flags flag)
{
    WARN_ON(flag == WLAN_STA_AUTH ||
        flag == WLAN_STA_ASSOC ||
        flag == WLAN_STA_AUTHORIZED);
    return test_and_set_bit(flag, &sta->_flags);
}

int sta_info_move_state(struct sta_info *sta,
            enum ieee80211_sta_state new_state);

static inline void sta_info_pre_move_state(struct sta_info *sta,
                       enum ieee80211_sta_state new_state)
{
    int ret;

    WARN_ON_ONCE(test_sta_flag(sta, WLAN_STA_INSERTED));

    ret = sta_info_move_state(sta, new_state);
    WARN_ON_ONCE(ret);
}


void ieee80211_assign_tid_tx(struct sta_info *sta, int tid,
                 struct tid_ampdu_tx *tid_tx);

static inline struct tid_ampdu_tx *
rcu_dereference_protected_tid_tx(struct sta_info *sta, int tid)
{
    return rcu_dereference_protected(sta->ampdu_mlme.tid_tx[tid],
                     lockdep_is_held(&sta->lock) ||
                     lockdep_is_held(&sta->ampdu_mlme.mtx));
}

/* Maximum number of frames to buffer per power saving station per AC */
#define STA_MAX_TX_BUFFER   64

/* Minimum buffered frame expiry time. If STA uses listen interval that is
 * smaller than this value, the minimum value here is used instead. */
#define STA_TX_BUFFER_EXPIRE (10 * HZ)

/* How often station data is cleaned up (e.g., expiration of buffered frames)
 */
#define STA_INFO_CLEANUP_INTERVAL (10 * HZ)

struct rhlist_head *sta_info_hash_lookup(struct ieee80211_local *local,
                     const u8 *addr);

/*
 * Get a STA info, must be under RCU read lock.
 */
struct sta_info *sta_info_get(struct ieee80211_sub_if_data *sdata,
                  const u8 *addr);

struct sta_info *sta_info_get_bss(struct ieee80211_sub_if_data *sdata,
                  const u8 *addr);

/* user must hold sta_mtx or be in RCU critical section */
struct sta_info *sta_info_get_by_addrs(struct ieee80211_local *local,
                       const u8 *sta_addr, const u8 *vif_addr);

#define for_each_sta_info(local, _addr, _sta, _tmp)         \
    rhl_for_each_entry_rcu(_sta, _tmp,              \
                   sta_info_hash_lookup(local, _addr), hash_node)

/*
 * Get STA info by index, BROKEN!
 */
struct sta_info *sta_info_get_by_idx(struct ieee80211_sub_if_data *sdata,
                     int idx);
/*
 * Create a new STA info, caller owns returned structure
 * until sta_info_insert().
 */
struct sta_info *sta_info_alloc(struct ieee80211_sub_if_data *sdata,
                const u8 *addr, gfp_t gfp);

void sta_info_free(struct ieee80211_local *local, struct sta_info *sta);

int sta_info_insert(struct sta_info *sta);
int sta_info_insert_rcu(struct sta_info *sta) __acquires(RCU);

int __must_check __sta_info_destroy(struct sta_info *sta);
int sta_info_destroy_addr(struct ieee80211_sub_if_data *sdata,
              const u8 *addr);
int sta_info_destroy_addr_bss(struct ieee80211_sub_if_data *sdata,
                  const u8 *addr);

void sta_info_recalc_tim(struct sta_info *sta);

int sta_info_init(struct ieee80211_local *local);
void sta_info_stop(struct ieee80211_local *local);

/**
 * __sta_info_flush - flush matching STA entries from the STA table
 *
 * Returns the number of removed STA entries.
 *
 * @sdata: sdata to remove all stations from
 * @vlans: if the given interface is an AP interface, also flush VLANs
 */
int __sta_info_flush(struct ieee80211_sub_if_data *sdata, bool vlans);

/**
 * sta_info_flush - flush matching STA entries from the STA table
 *
 * Returns the number of removed STA entries.
 *
 * @sdata: sdata to remove all stations from
 */
static inline int sta_info_flush(struct ieee80211_sub_if_data *sdata)
{
    return __sta_info_flush(sdata, false);
}

void sta_set_rate_info_tx(struct sta_info *sta,
              const struct ieee80211_tx_rate *rate,
              struct rate_info *rinfo);
void sta_set_sinfo(struct sta_info *sta, struct station_info *sinfo,
           bool tidstats);

u32 sta_get_expected_throughput(struct sta_info *sta);

void ieee80211_sta_expire(struct ieee80211_sub_if_data *sdata,
              unsigned long exp_time);
u8 sta_info_tx_streams(struct sta_info *sta);

void ieee80211_sta_ps_deliver_wakeup(struct sta_info *sta);
void ieee80211_sta_ps_deliver_poll_response(struct sta_info *sta);
void ieee80211_sta_ps_deliver_uapsd(struct sta_info *sta);

unsigned long ieee80211_sta_last_active(struct sta_info *sta);

enum sta_stats_type {
    STA_STATS_RATE_TYPE_INVALID = 0,
    STA_STATS_RATE_TYPE_LEGACY,
    STA_STATS_RATE_TYPE_HT,
    STA_STATS_RATE_TYPE_VHT,
    STA_STATS_RATE_TYPE_HE,
    STA_STATS_RATE_TYPE_S1G,
};

#define STA_STATS_FIELD_HT_MCS      GENMASK( 7,  0)
#define STA_STATS_FIELD_LEGACY_IDX  GENMASK( 3,  0)
#define STA_STATS_FIELD_LEGACY_BAND GENMASK( 7,  4)
#define STA_STATS_FIELD_VHT_MCS     GENMASK( 3,  0)
#define STA_STATS_FIELD_VHT_NSS     GENMASK( 7,  4)
#define STA_STATS_FIELD_HE_MCS      GENMASK( 3,  0)
#define STA_STATS_FIELD_HE_NSS      GENMASK( 7,  4)
#define STA_STATS_FIELD_BW          GENMASK(11,  8)
#define STA_STATS_FIELD_SGI         GENMASK(12, 12)
#define STA_STATS_FIELD_TYPE        GENMASK(15, 13)
#define STA_STATS_FIELD_HE_RU       GENMASK(18, 16)
#define STA_STATS_FIELD_HE_GI       GENMASK(20, 19)
#define STA_STATS_FIELD_HE_DCM      GENMASK(21, 21)

#define STA_STATS_FIELD(_n, _v)     FIELD_PREP(STA_STATS_FIELD_ ## _n, _v)
#define STA_STATS_GET(_n, _v)       FIELD_GET(STA_STATS_FIELD_ ## _n, _v)

#define STA_STATS_RATE_INVALID      0

static inline u32 sta_stats_encode_rate(struct ieee80211_rx_status *s)
{
    u32 r;

    r = STA_STATS_FIELD(BW, s->bw);

    if (s->enc_flags & RX_ENC_FLAG_SHORT_GI)
        r |= STA_STATS_FIELD(SGI, 1);

    switch (s->encoding) {
    case RX_ENC_VHT:
        r |= STA_STATS_FIELD(TYPE, STA_STATS_RATE_TYPE_VHT);
        r |= STA_STATS_FIELD(VHT_NSS, s->nss);
        r |= STA_STATS_FIELD(VHT_MCS, s->rate_idx);
        break;
    case RX_ENC_HT:
        r |= STA_STATS_FIELD(TYPE, STA_STATS_RATE_TYPE_HT);
        r |= STA_STATS_FIELD(HT_MCS, s->rate_idx);
        break;
    case RX_ENC_LEGACY:
        r |= STA_STATS_FIELD(TYPE, STA_STATS_RATE_TYPE_LEGACY);
        r |= STA_STATS_FIELD(LEGACY_BAND, s->band);
        r |= STA_STATS_FIELD(LEGACY_IDX, s->rate_idx);
        break;
    case RX_ENC_HE:
        r |= STA_STATS_FIELD(TYPE, STA_STATS_RATE_TYPE_HE);
        r |= STA_STATS_FIELD(HE_NSS, s->nss);
        r |= STA_STATS_FIELD(HE_MCS, s->rate_idx);
        r |= STA_STATS_FIELD(HE_GI, s->he_gi);
        r |= STA_STATS_FIELD(HE_RU, s->he_ru);
        r |= STA_STATS_FIELD(HE_DCM, s->he_dcm);
        break;
    default:
        WARN_ON(1);
        return STA_STATS_RATE_INVALID;
    }

    return r;
}

//----------------ieee80211_i.h--

#define AP_MAX_BC_BUFFER 128

#define TOTAL_MAX_TX_BUFFER 512

/* Required encryption head and tailroom */
#define IEEE80211_ENCRYPT_HEADROOM 8
#define IEEE80211_ENCRYPT_TAILROOM 18

/* power level hasn't been configured (or set to automatic) */
#define IEEE80211_UNSET_POWER_LEVEL INT_MIN

#define IEEE80211_DEFAULT_UAPSD_QUEUES 0

#define IEEE80211_DEFAULT_MAX_SP_LEN        \
    IEEE80211_WMM_IE_STA_QOSINFO_SP_ALL

extern const u8 ieee80211_ac_to_qos_mask[IEEE80211_NUM_ACS];

#define IEEE80211_DEAUTH_FRAME_LEN  (24 /* hdr */ + 2 /* reason */)

#define IEEE80211_MAX_NAN_INSTANCE_ID 255

struct ieee80211_bss {
    u32 device_ts_beacon, device_ts_presp;

    bool wmm_used;
    bool uapsd_supported;

#define IEEE80211_MAX_SUPP_RATES 32
    u8 supp_rates[IEEE80211_MAX_SUPP_RATES];
    size_t supp_rates_len;
    struct ieee80211_rate *beacon_rate;

    u32 vht_cap_info;

    /*
     * During association, we save an ERP value from a probe response so
     * that we can feed ERP info to the driver when handling the
     * association completes. these fields probably won't be up-to-date
     * otherwise, you probably don't want to use them.
     */
    bool has_erp_value;
    u8 erp_value;

    /* Keep track of the corruption of the last beacon/probe response. */
    u8 corrupt_data;

    /* Keep track of what bits of information we have valid info for. */
    u8 valid_data;
};

enum ieee80211_bss_corrupt_data_flags {
    IEEE80211_BSS_CORRUPT_BEACON        = BIT(0),
    IEEE80211_BSS_CORRUPT_PROBE_RESP    = BIT(1)
};

enum ieee80211_bss_valid_data_flags {
    IEEE80211_BSS_VALID_WMM         = BIT(1),
    IEEE80211_BSS_VALID_RATES       = BIT(2),
    IEEE80211_BSS_VALID_ERP         = BIT(3)
};

typedef unsigned __bitwise ieee80211_tx_result;
#define TX_CONTINUE ((__force ieee80211_tx_result) 0u)
#define TX_DROP     ((__force ieee80211_tx_result) 1u)
#define TX_QUEUED   ((__force ieee80211_tx_result) 2u)

#define IEEE80211_TX_UNICAST        BIT(1)
#define IEEE80211_TX_PS_BUFFERED    BIT(2)

struct ieee80211_tx_data {
    struct sk_buff *skb;
    struct sk_buff_head skbs;
    struct ieee80211_local *local;
    struct ieee80211_sub_if_data *sdata;
    struct sta_info *sta;
    struct ieee80211_key *key;
    struct ieee80211_tx_rate rate;

    unsigned int flags;
};


typedef unsigned __bitwise ieee80211_rx_result;
#define RX_CONTINUE     ((__force ieee80211_rx_result) 0u)
#define RX_DROP_UNUSABLE    ((__force ieee80211_rx_result) 1u)
#define RX_DROP_MONITOR     ((__force ieee80211_rx_result) 2u)
#define RX_QUEUED       ((__force ieee80211_rx_result) 3u)

enum ieee80211_packet_rx_flags {
    IEEE80211_RX_AMSDU          = BIT(3),
    IEEE80211_RX_MALFORMED_ACTION_FRM   = BIT(4),
    IEEE80211_RX_DEFERRED_RELEASE       = BIT(5),
};

enum ieee80211_rx_flags {
    IEEE80211_RX_CMNTR      = BIT(0),
    IEEE80211_RX_BEACON_REPORTED    = BIT(1),
};

struct ieee80211_rx_data {
    struct list_head *list;
    struct sk_buff *skb;
    struct ieee80211_local *local;
    struct ieee80211_sub_if_data *sdata;
    struct sta_info *sta;
    struct ieee80211_key *key;

    unsigned int flags;

    /*
     * Index into sequence numbers array, 0..16
     * since the last (16) is used for non-QoS,
     * will be 16 on non-QoS frames.
     */
    int seqno_idx;

    /*
     * Index into the security IV/PN arrays, 0..16
     * since the last (16) is used for CCMP-encrypted
     * management frames, will be set to 16 on mgmt
     * frames and 0 on non-QoS frames.
     */
    int security_idx;

    union {
        struct {
            u32 iv32;
            u16 iv16;
        } tkip;
        struct {
            u8 pn[IEEE80211_CCMP_PN_LEN];
        } ccm_gcm;
    };
};

struct ieee80211_csa_settings {
    const u16 *counter_offsets_beacon;
    const u16 *counter_offsets_presp;

    int n_counter_offsets_beacon;
    int n_counter_offsets_presp;

    u8 count;
};

struct ieee80211_color_change_settings {
    u16 counter_offset_beacon;
    u16 counter_offset_presp;
    u8 count;
};

struct beacon_data {
    u8 *head, *tail;
    int head_len, tail_len;
    struct ieee80211_meshconf_ie *meshconf;
    u16 cntdwn_counter_offsets[IEEE80211_MAX_CNTDWN_COUNTERS_NUM];
    u8 cntdwn_current_counter;
    struct cfg80211_mbssid_elems *mbssid_ies;
    struct rcu_head rcu_head;
};

struct probe_resp {
    struct rcu_head rcu_head;
    int len;
    u16 cntdwn_counter_offsets[IEEE80211_MAX_CNTDWN_COUNTERS_NUM];
    u8 data[];
};

struct fils_discovery_data {
    struct rcu_head rcu_head;
    int len;
    u8 data[];
};

struct unsol_bcast_probe_resp_data {
    struct rcu_head rcu_head;
    int len;
    u8 data[];
};

struct ps_data {
    /* yes, this looks ugly, but guarantees that we can later use
     * bitmap_empty :)
     * NB: don't touch this bitmap, use sta_info_{set,clear}_tim_bit */
    u8 tim[sizeof(unsigned long) * BITS_TO_LONGS(IEEE80211_MAX_AID + 1)]
            __aligned(__alignof__(unsigned long));
    struct sk_buff_head bc_buf;
    atomic_t num_sta_ps; /* number of stations in PS mode */
    int dtim_count;
    bool dtim_bc_mc;
};

struct ieee80211_if_ap {
    struct beacon_data __rcu *beacon;
    struct probe_resp __rcu *probe_resp;
    struct fils_discovery_data __rcu *fils_discovery;
    struct unsol_bcast_probe_resp_data __rcu *unsol_bcast_probe_resp;

    /* to be used after channel switch. */
    struct cfg80211_beacon_data *next_beacon;
    struct list_head vlans; /* write-protected with RTNL and local->mtx */

    struct ps_data ps;
    atomic_t num_mcast_sta; /* number of stations receiving multicast */

    bool multicast_to_unicast;
};

struct ieee80211_if_vlan {
    struct list_head list; /* write-protected with RTNL and local->mtx */

    /* used for all tx if the VLAN is configured to 4-addr mode */
    struct sta_info __rcu *sta;
    atomic_t num_mcast_sta; /* number of stations receiving multicast */
};

struct mesh_stats {
    __u32 fwded_mcast;      /* Mesh forwarded multicast frames */
    __u32 fwded_unicast;        /* Mesh forwarded unicast frames */
    __u32 fwded_frames;     /* Mesh total forwarded frames */
    __u32 dropped_frames_ttl;   /* Not transmitted since mesh_ttl == 0*/
    __u32 dropped_frames_no_route;  /* Not transmitted, no route found */
    __u32 dropped_frames_congestion;/* Not forwarded due to congestion */
};

#define PREQ_Q_F_START      0x1
#define PREQ_Q_F_REFRESH    0x2
struct mesh_preq_queue {
    struct list_head list;
    u8 dst[ETH_ALEN];
    u8 flags;
};

struct ieee80211_roc_work {
    struct list_head list;

    struct ieee80211_sub_if_data *sdata;

    struct ieee80211_channel *chan;

    bool started, abort, hw_begun, notified;
    bool on_channel;

    unsigned long start_time;

    u32 duration, req_duration;
    struct sk_buff *frame;
    u64 cookie, mgmt_tx_cookie;
    enum ieee80211_roc_type type;
};

/* flags used in struct ieee80211_if_managed.flags */
enum ieee80211_sta_flags {
    IEEE80211_STA_CONNECTION_POLL   = BIT(1),
    IEEE80211_STA_CONTROL_PORT      = BIT(2),
    IEEE80211_STA_DISABLE_HT        = BIT(4),
    IEEE80211_STA_MFP_ENABLED       = BIT(6),
    IEEE80211_STA_UAPSD_ENABLED     = BIT(7),
    IEEE80211_STA_NULLFUNC_ACKED    = BIT(8),
    IEEE80211_STA_RESET_SIGNAL_AVE  = BIT(9),
    IEEE80211_STA_DISABLE_40MHZ     = BIT(10),
    IEEE80211_STA_DISABLE_VHT       = BIT(11),
    IEEE80211_STA_DISABLE_80P80MHZ  = BIT(12),
    IEEE80211_STA_DISABLE_160MHZ    = BIT(13),
    IEEE80211_STA_DISABLE_WMM       = BIT(14),
    IEEE80211_STA_ENABLE_RRM        = BIT(15),
    IEEE80211_STA_DISABLE_HE        = BIT(16),
    IEEE80211_STA_DISABLE_EHT       = BIT(17),
    IEEE80211_STA_DISABLE_320MHZ    = BIT(18),
};

struct ieee80211_mgd_auth_data {
    struct cfg80211_bss *bss;
    unsigned long timeout;
    int tries;
    u16 algorithm, expected_transaction;

    u8 key[WLAN_KEY_LEN_WEP104];
    u8 key_len, key_idx;
    bool done, waiting;
    bool peer_confirmed;
    bool timeout_started;

    u16 sae_trans, sae_status;
    size_t data_len;
    u8 data[];
};

struct ieee80211_mgd_assoc_data {
    struct cfg80211_bss *bss;
    const u8 *supp_rates;

    unsigned long timeout;
    int tries;

    u16 capability;
    u8 prev_bssid[ETH_ALEN];
    u8 ssid[IEEE80211_MAX_SSID_LEN];
    u8 ssid_len;
    u8 supp_rates_len;
    bool wmm, uapsd;
    bool need_beacon;
    bool synced;
    bool timeout_started;

    u8 ap_ht_param;

    struct ieee80211_vht_cap ap_vht_cap;

    u8 fils_nonces[2 * FILS_NONCE_LEN];
    u8 fils_kek[FILS_MAX_KEK_LEN];
    size_t fils_kek_len;

    size_t ie_len;
    u8 ie[];
};

struct ieee80211_sta_tx_tspec {
    /* timestamp of the first packet in the time slice */
    unsigned long time_slice_start;

    u32 admitted_time; /* in usecs, unlike over the air */
    u8 tsid;
    s8 up; /* signed to be able to invalidate with -1 during teardown */

    /* consumed TX time in microseconds in the time slice */
    u32 consumed_tx_time;
    enum {
        TX_TSPEC_ACTION_NONE = 0,
        TX_TSPEC_ACTION_DOWNGRADE,
        TX_TSPEC_ACTION_STOP_DOWNGRADE,
    } action;
    bool downgraded;
};

DECLARE_EWMA(beacon_signal, 4, 4)

struct ieee80211_if_managed {
    struct timer_list timer;
    struct timer_list conn_mon_timer;
    struct timer_list bcn_mon_timer;
    struct timer_list chswitch_timer;
    struct work_struct monitor_work;
    struct work_struct chswitch_work;
    struct work_struct beacon_connection_loss_work;
    struct work_struct csa_connection_drop_work;

    unsigned long beacon_timeout;
    unsigned long probe_timeout;
    int probe_send_count;
    bool nullfunc_failed;
    u8 connection_loss:1,
       driver_disconnect:1,
       reconnect:1,
       associated:1;

    struct cfg80211_bss *assoc_bss;
    struct ieee80211_mgd_auth_data *auth_data;
    struct ieee80211_mgd_assoc_data *assoc_data;

    u8 bssid[ETH_ALEN] __aligned(2);

    bool powersave; /* powersave requested for this iface */
    bool broken_ap; /* AP is broken -- turn off powersave */
    bool have_beacon;
    u8 dtim_period;
    enum ieee80211_smps_mode req_smps, /* requested smps mode */
                 driver_smps_mode; /* smps mode request */

    struct work_struct request_smps_work;

    unsigned int flags;

    bool csa_waiting_bcn;
    bool csa_ignored_same_chan;

    bool beacon_crc_valid;
    u32 beacon_crc;

    bool status_acked;
    bool status_received;
    __le16 status_fc;

    enum {
        IEEE80211_MFP_DISABLED,
        IEEE80211_MFP_OPTIONAL,
        IEEE80211_MFP_REQUIRED
    } mfp; /* management frame protection */

    /*
     * Bitmask of enabled u-apsd queues,
     * IEEE80211_WMM_IE_STA_QOSINFO_AC_BE & co. Needs a new association
     * to take effect.
     */
    unsigned int uapsd_queues;

    /*
     * Maximum number of buffered frames AP can deliver during a
     * service period, IEEE80211_WMM_IE_STA_QOSINFO_SP_ALL or similar.
     * Needs a new association to take effect.
     */
    unsigned int uapsd_max_sp_len;

    int wmm_last_param_set;
    int mu_edca_last_param_set;

    u8 use_4addr;

    s16 p2p_noa_index;

    struct ewma_beacon_signal ave_beacon_signal;

    /*
     * Number of Beacon frames used in ave_beacon_signal. This can be used
     * to avoid generating less reliable cqm events that would be based
     * only on couple of received frames.
     */
    unsigned int count_beacon_signal;

    /* Number of times beacon loss was invoked. */
    unsigned int beacon_loss_count;

    /*
     * Last Beacon frame signal strength average (ave_beacon_signal / 16)
     * that triggered a cqm event. 0 indicates that no event has been
     * generated for the current association.
     */
    int last_cqm_event_signal;

    /*
     * State variables for keeping track of RSSI of the AP currently
     * connected to and informing driver when RSSI has gone
     * below/above a certain threshold.
     */
    int rssi_min_thold, rssi_max_thold;
    int last_ave_beacon_signal;

    struct ieee80211_ht_cap ht_capa; /* configured ht-cap over-rides */
    struct ieee80211_ht_cap ht_capa_mask; /* Valid parts of ht_capa */
    struct ieee80211_vht_cap vht_capa; /* configured VHT overrides */
    struct ieee80211_vht_cap vht_capa_mask; /* Valid parts of vht_capa */
    struct ieee80211_s1g_cap s1g_capa; /* configured S1G overrides */
    struct ieee80211_s1g_cap s1g_capa_mask; /* valid s1g_capa bits */

    /* TDLS support */
    u8 tdls_peer[ETH_ALEN] __aligned(2);
    struct delayed_work tdls_peer_del_work;
    struct sk_buff *orig_teardown_skb; /* The original teardown skb */
    struct sk_buff *teardown_skb; /* A copy to send through the AP */
    spinlock_t teardown_lock; /* To lock changing teardown_skb */
    bool tdls_chan_switch_prohibited;
    bool tdls_wider_bw_prohibited;

    /* WMM-AC TSPEC support */
    struct ieee80211_sta_tx_tspec tx_tspec[IEEE80211_NUM_ACS];
    /* Use a separate work struct so that we can do something here
     * while the sdata->work is flushing the queues, for example.
     * otherwise, in scenarios where we hardly get any traffic out
     * on the BE queue, but there's a lot of VO traffic, we might
     * get stuck in a downgraded situation and flush takes forever.
     */
    struct delayed_work tx_tspec_wk;

    /* Information elements from the last transmitted (Re)Association
     * Request frame.
     */
    u8 *assoc_req_ies;
    size_t assoc_req_ies_len;
};

struct ieee80211_if_ibss {
    struct timer_list timer;
    struct work_struct csa_connection_drop_work;

    unsigned long last_scan_completed;

    u32 basic_rates;

    bool fixed_bssid;
    bool fixed_channel;
    bool privacy;

    bool control_port;
    bool userspace_handles_dfs;

    u8 bssid[ETH_ALEN] __aligned(2);
    u8 ssid[IEEE80211_MAX_SSID_LEN];
    u8 ssid_len, ie_len;
    u8 *ie;
    struct cfg80211_chan_def chandef;

    unsigned long ibss_join_req;
    /* probe response/beacon for IBSS */
    struct beacon_data __rcu *presp;

    struct ieee80211_ht_cap ht_capa; /* configured ht-cap over-rides */
    struct ieee80211_ht_cap ht_capa_mask; /* Valid parts of ht_capa */

    spinlock_t incomplete_lock;
    struct list_head incomplete_stations;

    enum {
        IEEE80211_IBSS_MLME_SEARCH,
        IEEE80211_IBSS_MLME_JOINED,
    } state;
};

/**
 * struct ieee80211_if_ocb - OCB mode state
 *
 * @housekeeping_timer: timer for periodic invocation of a housekeeping task
 * @wrkq_flags: OCB deferred task action
 * @incomplete_lock: delayed STA insertion lock
 * @incomplete_stations: list of STAs waiting for delayed insertion
 * @joined: indication if the interface is connected to an OCB network
 */
struct ieee80211_if_ocb {
    struct timer_list housekeeping_timer;
    unsigned long wrkq_flags;

    spinlock_t incomplete_lock;
    struct list_head incomplete_stations;

    bool joined;
};

/**
 * struct ieee80211_mesh_sync_ops - Extensible synchronization framework interface
 *
 * these declarations define the interface, which enables
 * vendor-specific mesh synchronization
 *
 */
struct ieee802_11_elems;
struct ieee80211_mesh_sync_ops {
    void (*rx_bcn_presp)(struct ieee80211_sub_if_data *sdata, u16 stype,
                 struct ieee80211_mgmt *mgmt, unsigned int len,
                 const struct ieee80211_meshconf_ie *mesh_cfg,
                 struct ieee80211_rx_status *rx_status);

    /* should be called with beacon_data under RCU read lock */
    void (*adjust_tsf)(struct ieee80211_sub_if_data *sdata,
               struct beacon_data *beacon);
    /* add other framework functions here */
};

struct mesh_csa_settings {
    struct rcu_head rcu_head;
    struct cfg80211_csa_settings settings;
};

/**
 * struct mesh_table
 *
 * @known_gates: list of known mesh gates and their mpaths by the station. The
 * gate's mpath may or may not be resolved and active.
 * @gates_lock: protects updates to known_gates
 * @rhead: the rhashtable containing struct mesh_paths, keyed by dest addr
 * @walk_head: linked list containing all mesh_path objects
 * @walk_lock: lock protecting walk_head
 * @entries: number of entries in the table
 */
struct mesh_table {
    struct hlist_head known_gates;
    spinlock_t gates_lock;
    struct rhashtable rhead;
    struct hlist_head walk_head;
    spinlock_t walk_lock;
    atomic_t entries;       /* Up to MAX_MESH_NEIGHBOURS */
};

struct ieee80211_if_mesh {
    struct timer_list housekeeping_timer;
    struct timer_list mesh_path_timer;
    struct timer_list mesh_path_root_timer;

    unsigned long wrkq_flags;
    unsigned long mbss_changed;

    bool userspace_handles_dfs;

    u8 mesh_id[IEEE80211_MAX_MESH_ID_LEN];
    size_t mesh_id_len;
    /* Active Path Selection Protocol Identifier */
    u8 mesh_pp_id;
    /* Active Path Selection Metric Identifier */
    u8 mesh_pm_id;
    /* Congestion Control Mode Identifier */
    u8 mesh_cc_id;
    /* Synchronization Protocol Identifier */
    u8 mesh_sp_id;
    /* Authentication Protocol Identifier */
    u8 mesh_auth_id;
    /* Local mesh Sequence Number */
    u32 sn;
    /* Last used PREQ ID */
    u32 preq_id;
    atomic_t mpaths;
    /* Timestamp of last SN update */
    unsigned long last_sn_update;
    /* Time when it's ok to send next PERR */
    unsigned long next_perr;
    /* Timestamp of last PREQ sent */
    unsigned long last_preq;
    struct mesh_rmc *rmc;
    spinlock_t mesh_preq_queue_lock;
    struct mesh_preq_queue preq_queue;
    int preq_queue_len;
    struct mesh_stats mshstats;
    struct mesh_config mshcfg;
    atomic_t estab_plinks;
    u32 mesh_seqnum;
    bool accepting_plinks;
    int num_gates;
    struct beacon_data __rcu *beacon;
    const u8 *ie;
    u8 ie_len;
    enum {
        IEEE80211_MESH_SEC_NONE = 0x0,
        IEEE80211_MESH_SEC_AUTHED = 0x1,
        IEEE80211_MESH_SEC_SECURED = 0x2,
    } security;
    bool user_mpm;
    /* Extensible Synchronization Framework */
    const struct ieee80211_mesh_sync_ops *sync_ops;
    s64 sync_offset_clockdrift_max;
    spinlock_t sync_offset_lock;
    /* mesh power save */
    enum nl80211_mesh_power_mode nonpeer_pm;
    int ps_peers_light_sleep;
    int ps_peers_deep_sleep;
    struct ps_data ps;
    /* Channel Switching Support */
    struct mesh_csa_settings __rcu *csa;
    enum {
        IEEE80211_MESH_CSA_ROLE_NONE,
        IEEE80211_MESH_CSA_ROLE_INIT,
        IEEE80211_MESH_CSA_ROLE_REPEATER,
    } csa_role;
    u8 chsw_ttl;
    u16 pre_value;

    /* offset from skb->data while building IE */
    int meshconf_offset;

    struct mesh_table mesh_paths;
    struct mesh_table mpp_paths; /* Store paths for MPP&MAP */
    int mesh_paths_generation;
    int mpp_paths_generation;
};

#ifdef CONFIG_MAC80211_MESH
#define IEEE80211_IFSTA_MESH_CTR_INC(msh, name) \
    do { (msh)->mshstats.name++; } while (0)
#else
#define IEEE80211_IFSTA_MESH_CTR_INC(msh, name) \
    do { } while (0)
#endif

/**
 * enum ieee80211_sub_if_data_flags - virtual interface flags
 *
 * @IEEE80211_SDATA_ALLMULTI: interface wants all multicast packets
 * @IEEE80211_SDATA_OPERATING_GMODE: operating in G-only mode
 * @IEEE80211_SDATA_DONT_BRIDGE_PACKETS: bridge packets between
 *  associated stations and deliver multicast frames both
 *  back to wireless media and to the local net stack.
 * @IEEE80211_SDATA_DISCONNECT_RESUME: Disconnect after resume.
 * @IEEE80211_SDATA_IN_DRIVER: indicates interface was added to driver
 * @IEEE80211_SDATA_DISCONNECT_HW_RESTART: Disconnect after hardware restart
 *  recovery
 */
enum ieee80211_sub_if_data_flags {
    IEEE80211_SDATA_ALLMULTI                = BIT(0),
    IEEE80211_SDATA_OPERATING_GMODE         = BIT(2),
    IEEE80211_SDATA_DONT_BRIDGE_PACKETS     = BIT(3),
    IEEE80211_SDATA_DISCONNECT_RESUME       = BIT(4),
    IEEE80211_SDATA_IN_DRIVER               = BIT(5),
    IEEE80211_SDATA_DISCONNECT_HW_RESTART   = BIT(6),
};

/**
 * enum ieee80211_sdata_state_bits - virtual interface state bits
 * @SDATA_STATE_RUNNING: virtual interface is up & running; this
 *  mirrors netif_running() but is separate for interface type
 *  change handling while the interface is up
 * @SDATA_STATE_OFFCHANNEL: This interface is currently in offchannel
 *  mode, so queues are stopped
 * @SDATA_STATE_OFFCHANNEL_BEACON_STOPPED: Beaconing was stopped due
 *  to offchannel, reset when offchannel returns
 */
enum ieee80211_sdata_state_bits {
    SDATA_STATE_RUNNING,
    SDATA_STATE_OFFCHANNEL,
    SDATA_STATE_OFFCHANNEL_BEACON_STOPPED,
};

/**
 * enum ieee80211_chanctx_mode - channel context configuration mode
 *
 * @IEEE80211_CHANCTX_SHARED: channel context may be used by
 *  multiple interfaces
 * @IEEE80211_CHANCTX_EXCLUSIVE: channel context can be used
 *  only by a single interface. This can be used for example for
 *  non-fixed channel IBSS.
 */
enum ieee80211_chanctx_mode {
    IEEE80211_CHANCTX_SHARED,
    IEEE80211_CHANCTX_EXCLUSIVE
};

/**
 * enum ieee80211_chanctx_replace_state - channel context replacement state
 *
 * This is used for channel context in-place reservations that require channel
 * context switch/swap.
 *
 * @IEEE80211_CHANCTX_REPLACE_NONE: no replacement is taking place
 * @IEEE80211_CHANCTX_WILL_BE_REPLACED: this channel context will be replaced
 *  by a (not yet registered) channel context pointed by %replace_ctx.
 * @IEEE80211_CHANCTX_REPLACES_OTHER: this (not yet registered) channel context
 *  replaces an existing channel context pointed to by %replace_ctx.
 */
enum ieee80211_chanctx_replace_state {
    IEEE80211_CHANCTX_REPLACE_NONE,
    IEEE80211_CHANCTX_WILL_BE_REPLACED,
    IEEE80211_CHANCTX_REPLACES_OTHER,
};

struct ieee80211_chanctx {
    struct list_head list;
    struct rcu_head rcu_head;

    struct list_head assigned_vifs;
    struct list_head reserved_vifs;

    enum ieee80211_chanctx_replace_state replace_state;
    struct ieee80211_chanctx *replace_ctx;

    enum ieee80211_chanctx_mode mode;
    bool driver_present;

    struct ieee80211_chanctx_conf conf;
};

struct mac80211_qos_map {
    struct cfg80211_qos_map qos_map;
    struct rcu_head rcu_head;
};

enum txq_info_flags {
    IEEE80211_TXQ_STOP,
    IEEE80211_TXQ_AMPDU,
    IEEE80211_TXQ_NO_AMSDU,
    IEEE80211_TXQ_STOP_NETIF_TX,
};

/**
 * struct txq_info - per tid queue
 *
 * @tin: contains packets split into multiple flows
 * @def_flow: used as a fallback flow when a packet destined to @tin hashes to
 *  a fq_flow which is already owned by a different tin
 * @def_cvars: codel vars for @def_flow
 * @schedule_order: used with ieee80211_local->active_txqs
 * @frags: used to keep fragments created after dequeue
 */
struct txq_info {
    struct fq_tin tin;
    struct codel_vars def_cvars;
    struct codel_stats cstats;
    struct rb_node schedule_order;

    struct sk_buff_head frags;
    unsigned long flags;

    /* keep last! */
    struct ieee80211_txq txq;
};

struct ieee80211_if_mntr {
    u32 flags;
    u8 mu_follow_addr[ETH_ALEN] __aligned(2);

    struct list_head list;
};

/**
 * struct ieee80211_if_nan - NAN state
 *
 * @conf: current NAN configuration
 * @func_ids: a bitmap of available instance_id's
 */
struct ieee80211_if_nan {
    struct cfg80211_nan_conf conf;

    /* protects function_inst_ids */
    spinlock_t func_lock;
    struct idr function_inst_ids;
};

struct ieee80211_sub_if_data {
    struct list_head list;

    struct wireless_dev wdev;

    /* keys */
    struct list_head key_list;

    /* count for keys needing tailroom space allocation */
    int crypto_tx_tailroom_needed_cnt;
    int crypto_tx_tailroom_pending_dec;
    struct delayed_work dec_tailroom_needed_wk;

    struct net_device *dev;
    struct ieee80211_local *local;

    unsigned int flags;

    unsigned long state;

    char name[IFNAMSIZ];

    struct ieee80211_fragment_cache frags;

    /* TID bitmap for NoAck policy */
    u16 noack_map;

    /* bit field of ACM bits (BIT(802.1D tag)) */
    u8 wmm_acm;

    struct ieee80211_key __rcu *keys[NUM_DEFAULT_KEYS +
                     NUM_DEFAULT_MGMT_KEYS +
                     NUM_DEFAULT_BEACON_KEYS];
    struct ieee80211_key __rcu *default_unicast_key;
    struct ieee80211_key __rcu *default_multicast_key;
    struct ieee80211_key __rcu *default_mgmt_key;
    struct ieee80211_key __rcu *default_beacon_key;

    u16 sequence_number;
    __be16 control_port_protocol;
    bool control_port_no_encrypt;
    bool control_port_no_preauth;
    bool control_port_over_nl80211;
    int encrypt_headroom;

    atomic_t num_tx_queued;
    struct ieee80211_tx_queue_params tx_conf[IEEE80211_NUM_ACS];
    struct mac80211_qos_map __rcu *qos_map;

    struct airtime_info airtime[IEEE80211_NUM_ACS];

    struct work_struct csa_finalize_work;
    bool csa_block_tx; /* write-protected by sdata_lock and local->mtx */
    struct cfg80211_chan_def csa_chandef;

    struct work_struct color_change_finalize_work;

    struct list_head assigned_chanctx_list; /* protected by chanctx_mtx */
    struct list_head reserved_chanctx_list; /* protected by chanctx_mtx */

    /* context reservation -- protected with chanctx_mtx */
    struct ieee80211_chanctx *reserved_chanctx;
    struct cfg80211_chan_def reserved_chandef;
    bool reserved_radar_required;
    bool reserved_ready;

    /* used to reconfigure hardware SM PS */
    struct work_struct recalc_smps;

    struct work_struct work;
    struct sk_buff_head skb_queue;
    struct sk_buff_head status_queue;

    u8 needed_rx_chains;
    enum ieee80211_smps_mode smps_mode;

    int user_power_level; /* in dBm */
    int ap_power_level; /* in dBm */

    bool radar_required;
    struct delayed_work dfs_cac_timer_work;

    /*
     * AP this belongs to: self in AP mode and
     * corresponding AP in VLAN mode, NULL for
     * all others (might be needed later in IBSS)
     */
    struct ieee80211_if_ap *bss;

    /* bitmap of allowed (non-MCS) rate indexes for rate control */
    u32 rc_rateidx_mask[NUM_NL80211_BANDS];

    bool rc_has_mcs_mask[NUM_NL80211_BANDS];
    u8  rc_rateidx_mcs_mask[NUM_NL80211_BANDS][IEEE80211_HT_MCS_MASK_LEN];

    bool rc_has_vht_mcs_mask[NUM_NL80211_BANDS];
    u16 rc_rateidx_vht_mcs_mask[NUM_NL80211_BANDS][NL80211_VHT_NSS_MAX];

    /* Beacon frame (non-MCS) rate (as a bitmap) */
    u32 beacon_rateidx_mask[NUM_NL80211_BANDS];
    bool beacon_rate_set;

    union {
        struct ieee80211_if_ap ap;
        struct ieee80211_if_vlan vlan;
        struct ieee80211_if_managed mgd;
        struct ieee80211_if_ibss ibss;
        struct ieee80211_if_mesh mesh;
        struct ieee80211_if_ocb ocb;
        struct ieee80211_if_mntr mntr;
        struct ieee80211_if_nan nan;
    } u;

#ifdef CONFIG_MAC80211_DEBUGFS
    struct {
        struct dentry *subdir_stations;
        struct dentry *default_unicast_key;
        struct dentry *default_multicast_key;
        struct dentry *default_mgmt_key;
        struct dentry *default_beacon_key;
    } debugfs;
#endif

    /* must be last, dynamically sized area in this! */
    struct ieee80211_vif vif;
};

static inline
struct ieee80211_sub_if_data *vif_to_sdata(struct ieee80211_vif *p)
{
    return container_of(p, struct ieee80211_sub_if_data, vif);
}

static inline void sdata_lock(struct ieee80211_sub_if_data *sdata)
    __acquires(&sdata->wdev.mtx)
{
    mutex_lock(&sdata->wdev.mtx);
    __acquire(&sdata->wdev.mtx);
}

static inline void sdata_unlock(struct ieee80211_sub_if_data *sdata)
    __releases(&sdata->wdev.mtx)
{
    mutex_unlock(&sdata->wdev.mtx);
    __release(&sdata->wdev.mtx);
}

#define sdata_dereference(p, sdata) \
    rcu_dereference_protected(p, lockdep_is_held(&sdata->wdev.mtx))

static inline void
sdata_assert_lock(struct ieee80211_sub_if_data *sdata)
{
    lockdep_assert_held(&sdata->wdev.mtx);
}

static inline int
ieee80211_chandef_get_shift(struct cfg80211_chan_def *chandef)
{
    switch (chandef->width) {
    case NL80211_CHAN_WIDTH_5:
        return 2;
    case NL80211_CHAN_WIDTH_10:
        return 1;
    default:
        return 0;
    }
}

static inline int
ieee80211_vif_get_shift(struct ieee80211_vif *vif)
{
    struct ieee80211_chanctx_conf *chanctx_conf;
    int shift = 0;

    rcu_read_lock();
    chanctx_conf = rcu_dereference(vif->bss_conf.chanctx_conf);
    if (chanctx_conf)
        shift = ieee80211_chandef_get_shift(&chanctx_conf->def);
    rcu_read_unlock();

    return shift;
}

static inline int
ieee80211_get_mbssid_beacon_len(struct cfg80211_mbssid_elems *elems)
{
    int i, len = 0;

    if (!elems)
        return 0;

    for (i = 0; i < elems->cnt; i++)
        len += elems->elem[i].len;

    return len;
}

enum {
    IEEE80211_RX_MSG    = 1,
    IEEE80211_TX_STATUS_MSG = 2,
};

enum queue_stop_reason {
    IEEE80211_QUEUE_STOP_REASON_DRIVER,
    IEEE80211_QUEUE_STOP_REASON_PS,
    IEEE80211_QUEUE_STOP_REASON_CSA,
    IEEE80211_QUEUE_STOP_REASON_AGGREGATION,
    IEEE80211_QUEUE_STOP_REASON_SUSPEND,
    IEEE80211_QUEUE_STOP_REASON_SKB_ADD,
    IEEE80211_QUEUE_STOP_REASON_OFFCHANNEL,
    IEEE80211_QUEUE_STOP_REASON_FLUSH,
    IEEE80211_QUEUE_STOP_REASON_TDLS_TEARDOWN,
    IEEE80211_QUEUE_STOP_REASON_RESERVE_TID,
    IEEE80211_QUEUE_STOP_REASON_IFTYPE_CHANGE,

    IEEE80211_QUEUE_STOP_REASONS,
};

#ifdef CONFIG_MAC80211_LEDS
struct tpt_led_trigger {
    char name[32];
    const struct ieee80211_tpt_blink *blink_table;
    unsigned int blink_table_len;
    struct timer_list timer;
    struct ieee80211_local *local;
    unsigned long prev_traffic;
    unsigned long tx_bytes, rx_bytes;
    unsigned int active, want;
    bool running;
};
#endif

/**
 * mac80211 scan flags - currently active scan mode
 *
 * @SCAN_SW_SCANNING: We're currently in the process of scanning but may as
 *  well be on the operating channel
 * @SCAN_HW_SCANNING: The hardware is scanning for us, we have no way to
 *  determine if we are on the operating channel or not
 * @SCAN_ONCHANNEL_SCANNING:  Do a software scan on only the current operating
 *  channel. This should not interrupt normal traffic.
 * @SCAN_COMPLETED: Set for our scan work function when the driver reported
 *  that the scan completed.
 * @SCAN_ABORTED: Set for our scan work function when the driver reported
 *  a scan complete for an aborted scan.
 * @SCAN_HW_CANCELLED: Set for our scan work function when the scan is being
 *  cancelled.
 * @SCAN_BEACON_WAIT: Set whenever we're passive scanning because of radar/no-IR
 *  and could send a probe request after receiving a beacon.
 * @SCAN_BEACON_DONE: Beacon received, we can now send a probe request
 */
enum {
    SCAN_SW_SCANNING,
    SCAN_HW_SCANNING,
    SCAN_ONCHANNEL_SCANNING,
    SCAN_COMPLETED,
    SCAN_ABORTED,
    SCAN_HW_CANCELLED,
    SCAN_BEACON_WAIT,
    SCAN_BEACON_DONE,
};

/**
 * enum mac80211_scan_state - scan state machine states
 *
 * @SCAN_DECISION: Main entry point to the scan state machine, this state
 *  determines if we should keep on scanning or switch back to the
 *  operating channel
 * @SCAN_SET_CHANNEL: Set the next channel to be scanned
 * @SCAN_SEND_PROBE: Send probe requests and wait for probe responses
 * @SCAN_SUSPEND: Suspend the scan and go back to operating channel to
 *  send out data
 * @SCAN_RESUME: Resume the scan and scan the next channel
 * @SCAN_ABORT: Abort the scan and go back to operating channel
 */
enum mac80211_scan_state {
    SCAN_DECISION,
    SCAN_SET_CHANNEL,
    SCAN_SEND_PROBE,
    SCAN_SUSPEND,
    SCAN_RESUME,
    SCAN_ABORT,
};

/**
 * struct airtime_sched_info - state used for airtime scheduling and AQL
 *
 * @lock: spinlock that protects all the fields in this struct
 * @active_txqs: rbtree of currently backlogged queues, sorted by virtual time
 * @schedule_pos: the current position maintained while a driver walks the tree
 *                with ieee80211_next_txq()
 * @active_list: list of struct airtime_info structs that were active within
 *               the last AIRTIME_ACTIVE_DURATION (100 ms), used to compute
 *               weight_sum
 * @last_weight_update: used for rate limiting walking active_list
 * @last_schedule_time: tracks the last time a transmission was scheduled; used
 *                      for catching up v_t if no stations are eligible for
 *                      transmission.
 * @v_t: global virtual time; queues with v_t < this are eligible for
 *       transmission
 * @weight_sum: total sum of all active stations used for dividing airtime
 * @weight_sum_reciprocal: reciprocal of weight_sum (to avoid divisions in fast
 *                         path - see comment above
 *                         IEEE80211_RECIPROCAL_DIVISOR_64)
 * @aql_txq_limit_low: AQL limit when total outstanding airtime
 *                     is < IEEE80211_AQL_THRESHOLD
 * @aql_txq_limit_high: AQL limit when total outstanding airtime
 *                      is > IEEE80211_AQL_THRESHOLD
 */
struct airtime_sched_info {
    spinlock_t lock;
    struct rb_root_cached active_txqs;
    struct rb_node *schedule_pos;
    struct list_head active_list;
    u64 last_weight_update;
    u64 last_schedule_activity;
    u64 v_t;
    u64 weight_sum;
    u64 weight_sum_reciprocal;
    u32 aql_txq_limit_low;
    u32 aql_txq_limit_high;
};
DECLARE_STATIC_KEY_FALSE(aql_disable);

struct ieee80211_local {
    /* embed the driver visible part.
     * don't cast (use the static inlines below), but we keep
     * it first anyway so they become a no-op */
    struct ieee80211_hw hw;

    struct fq fq;
    struct codel_vars *cvars;
    struct codel_params cparams;

    /* protects active_txqs and txqi->schedule_order */
    struct airtime_sched_info airtime[IEEE80211_NUM_ACS];
    u16 airtime_flags;
    u32 aql_threshold;
    atomic_t aql_total_pending_airtime;

    const struct ieee80211_ops *ops;

    /*
     * private workqueue to mac80211. mac80211 makes this accessible
     * via ieee80211_queue_work()
     */
    struct workqueue_struct *workqueue;

    unsigned long queue_stop_reasons[IEEE80211_MAX_QUEUES];
    int q_stop_reasons[IEEE80211_MAX_QUEUES][IEEE80211_QUEUE_STOP_REASONS];
    /* also used to protect ampdu_ac_queue and amdpu_ac_stop_refcnt */
    spinlock_t queue_stop_reason_lock;

    int open_count;
    int monitors, cooked_mntrs;
    /* number of interfaces with corresponding FIF_ flags */
    int fif_fcsfail, fif_plcpfail, fif_control, fif_other_bss, fif_pspoll,
        fif_probe_req;
    bool probe_req_reg;
    bool rx_mcast_action_reg;
    unsigned int filter_flags; /* FIF_* */

    bool wiphy_ciphers_allocated;

    bool use_chanctx;

    /* protects the aggregated multicast list and filter calls */
    spinlock_t filter_lock;

    /* used for uploading changed mc list */
    struct work_struct reconfig_filter;

    /* aggregated multicast list */
    struct netdev_hw_addr_list mc_list;

    bool tim_in_locked_section; /* see ieee80211_beacon_get() */

    /*
     * suspended is true if we finished all the suspend _and_ we have
     * not yet come up from resume. This is to be used by mac80211
     * to ensure driver sanity during suspend and mac80211's own
     * sanity. It can eventually be used for WoW as well.
     */
    bool suspended;

    /* suspending is true during the whole suspend process */
    bool suspending;

    /*
     * Resuming is true while suspended, but when we're reprogramming the
     * hardware -- at that time it's allowed to use ieee80211_queue_work()
     * again even though some other parts of the stack are still suspended
     * and we still drop received frames to avoid waking the stack.
     */
    bool resuming;

    /*
     * quiescing is true during the suspend process _only_ to
     * ease timer cancelling etc.
     */
    bool quiescing;

    /* device is started */
    bool started;

    /* device is during a HW reconfig */
    bool in_reconfig;

    /* wowlan is enabled -- don't reconfig on resume */
    bool wowlan;

    struct work_struct radar_detected_work;

    /* number of RX chains the hardware has */
    u8 rx_chains;

    /* bitmap of which sbands were copied */
    u8 sband_allocated;

    int tx_headroom; /* required headroom for hardware/radiotap */

    /* Tasklet and skb queue to process calls from IRQ mode. All frames
     * added to skb_queue will be processed, but frames in
     * skb_queue_unreliable may be dropped if the total length of these
     * queues increases over the limit. */
#define IEEE80211_IRQSAFE_QUEUE_LIMIT 128
    struct tasklet_struct tasklet;
    struct sk_buff_head skb_queue;
    struct sk_buff_head skb_queue_unreliable;

    spinlock_t rx_path_lock;

    /* Station data */
    /*
     * The mutex only protects the list, hash table and
     * counter, reads are done with RCU.
     */
    struct mutex sta_mtx;
    spinlock_t tim_lock;
    unsigned long num_sta;
    struct list_head sta_list;
    struct rhltable sta_hash;
    struct timer_list sta_cleanup;
    int sta_generation;

    struct sk_buff_head pending[IEEE80211_MAX_QUEUES];
    struct tasklet_struct tx_pending_tasklet;
    struct tasklet_struct wake_txqs_tasklet;

    atomic_t agg_queue_stop[IEEE80211_MAX_QUEUES];

    /* number of interfaces with allmulti RX */
    atomic_t iff_allmultis;

    struct rate_control_ref *rate_ctrl;

    struct arc4_ctx wep_tx_ctx;
    struct arc4_ctx wep_rx_ctx;
    u32 wep_iv;

    /* see iface.c */
    struct list_head interfaces;
    struct list_head mon_list; /* only that are IFF_UP && !cooked */
    struct mutex iflist_mtx;

    /*
     * Key mutex, protects sdata's key_list and sta_info's
     * key pointers and ptk_idx (write access, they're RCU.)
     */
    struct mutex key_mtx;

    /* mutex for scan and work locking */
    struct mutex mtx;

    /* Scanning and BSS list */
    unsigned long scanning;
    struct cfg80211_ssid scan_ssid;
    struct cfg80211_scan_request *int_scan_req;
    struct cfg80211_scan_request __rcu *scan_req;
    struct ieee80211_scan_request *hw_scan_req;
    struct cfg80211_chan_def scan_chandef;
    enum nl80211_band hw_scan_band;
    int scan_channel_idx;
    int scan_ies_len;
    int hw_scan_ies_bufsize;
    struct cfg80211_scan_info scan_info;

    struct work_struct sched_scan_stopped_work;
    struct ieee80211_sub_if_data __rcu *sched_scan_sdata;
    struct cfg80211_sched_scan_request __rcu *sched_scan_req;
    u8 scan_addr[ETH_ALEN];

    unsigned long leave_oper_channel_time;
    enum mac80211_scan_state next_scan_state;
    struct delayed_work scan_work;
    struct ieee80211_sub_if_data __rcu *scan_sdata;
    /* For backward compatibility only -- do not use */
    struct cfg80211_chan_def _oper_chandef;

    /* Temporary remain-on-channel for off-channel operations */
    struct ieee80211_channel *tmp_channel;

    /* channel contexts */
    struct list_head chanctx_list;
    struct mutex chanctx_mtx;

#ifdef CONFIG_MAC80211_LEDS
    struct led_trigger tx_led, rx_led, assoc_led, radio_led;
    struct led_trigger tpt_led;
    atomic_t tx_led_active, rx_led_active, assoc_led_active;
    atomic_t radio_led_active, tpt_led_active;
    struct tpt_led_trigger *tpt_led_trigger;
#endif

#ifdef CONFIG_MAC80211_DEBUG_COUNTERS
    /* SNMP counters */
    /* dot11CountersTable */
    u32 dot11TransmittedFragmentCount;
    u32 dot11MulticastTransmittedFrameCount;
    u32 dot11FailedCount;
    u32 dot11RetryCount;
    u32 dot11MultipleRetryCount;
    u32 dot11FrameDuplicateCount;
    u32 dot11ReceivedFragmentCount;
    u32 dot11MulticastReceivedFrameCount;
    u32 dot11TransmittedFrameCount;

    /* TX/RX handler statistics */
    unsigned int tx_handlers_drop;
    unsigned int tx_handlers_queued;
    unsigned int tx_handlers_drop_wep;
    unsigned int tx_handlers_drop_not_assoc;
    unsigned int tx_handlers_drop_unauth_port;
    unsigned int rx_handlers_drop;
    unsigned int rx_handlers_queued;
    unsigned int rx_handlers_drop_nullfunc;
    unsigned int rx_handlers_drop_defrag;
    unsigned int tx_expand_skb_head;
    unsigned int tx_expand_skb_head_cloned;
    unsigned int rx_expand_skb_head_defrag;
    unsigned int rx_handlers_fragments;
    unsigned int tx_status_drop;
#define I802_DEBUG_INC(c) (c)++
#else /* CONFIG_MAC80211_DEBUG_COUNTERS */
#define I802_DEBUG_INC(c) do { } while (0)
#endif /* CONFIG_MAC80211_DEBUG_COUNTERS */


    int total_ps_buffered; /* total number of all buffered unicast and
                * multicast packets for power saving stations
                */

    bool pspolling;
    /*
     * PS can only be enabled when we have exactly one managed
     * interface (and monitors) in PS, this then points there.
     */
    struct ieee80211_sub_if_data *ps_sdata;
    struct work_struct dynamic_ps_enable_work;
    struct work_struct dynamic_ps_disable_work;
    struct timer_list dynamic_ps_timer;
    struct notifier_block ifa_notifier;
    struct notifier_block ifa6_notifier;

    /*
     * The dynamic ps timeout configured from user space via WEXT -
     * this will override whatever chosen by mac80211 internally.
     */
    int dynamic_ps_forced_timeout;

    int user_power_level; /* in dBm, for all interfaces */

    enum ieee80211_smps_mode smps_mode;

    struct work_struct restart_work;

#ifdef CONFIG_MAC80211_DEBUGFS
    struct local_debugfsdentries {
        struct dentry *rcdir;
        struct dentry *keys;
    } debugfs;
    bool force_tx_status;
#endif

    /*
     * Remain-on-channel support
     */
    struct delayed_work roc_work;
    struct list_head roc_list;
    struct work_struct hw_roc_start, hw_roc_done;
    unsigned long hw_roc_start_time;
    u64 roc_cookie_counter;

    struct idr ack_status_frames;
    spinlock_t ack_status_lock;

    struct ieee80211_sub_if_data __rcu *p2p_sdata;

    /* virtual monitor interface */
    struct ieee80211_sub_if_data __rcu *monitor_sdata;
    struct cfg80211_chan_def monitor_chandef;

    /* extended capabilities provided by mac80211 */
    u8 ext_capa[8];
};

static inline struct ieee80211_sub_if_data *
IEEE80211_DEV_TO_SUB_IF(const struct net_device *dev)
{
    return netdev_priv(dev);
}

static inline struct ieee80211_sub_if_data *
IEEE80211_WDEV_TO_SUB_IF(struct wireless_dev *wdev)
{
    return container_of(wdev, struct ieee80211_sub_if_data, wdev);
}

static inline struct ieee80211_supported_band *
ieee80211_get_sband(struct ieee80211_sub_if_data *sdata)
{
    struct ieee80211_local *local = sdata->local;
    struct ieee80211_chanctx_conf *chanctx_conf;
    enum nl80211_band band;

    rcu_read_lock();
    chanctx_conf = rcu_dereference(sdata->vif.bss_conf.chanctx_conf);

    if (!chanctx_conf) {
        rcu_read_unlock();
        return NULL;
    }

    band = chanctx_conf->def.chan->band;
    rcu_read_unlock();

    return local->hw.wiphy->bands[band];
}

/* this struct holds the value parsing from channel switch IE  */
struct ieee80211_csa_ie {
    struct cfg80211_chan_def chandef;
    u8 mode;
    u8 count;
    u8 ttl;
    u16 pre_value;
    u16 reason_code;
    u32 max_switch_time;
};

/* Parsed Information Elements */
struct ieee802_11_elems {
    const u8 *ie_start;
    size_t total_len;
    u32 crc;

    /* pointers to IEs */
    const struct ieee80211_tdls_lnkie *lnk_id;
    const struct ieee80211_ch_switch_timing *ch_sw_timing;
    const u8 *ext_capab;
    const u8 *ssid;
    const u8 *supp_rates;
    const u8 *ds_params;
    const struct ieee80211_tim_ie *tim;
    const u8 *rsn;
    const u8 *rsnx;
    const u8 *erp_info;
    const u8 *ext_supp_rates;
    const u8 *wmm_info;
    const u8 *wmm_param;
    const struct ieee80211_ht_cap *ht_cap_elem;
    const struct ieee80211_ht_operation *ht_operation;
    const struct ieee80211_vht_cap *vht_cap_elem;
    const struct ieee80211_vht_operation *vht_operation;
    const struct ieee80211_meshconf_ie *mesh_config;
    const u8 *he_cap;
    const struct ieee80211_he_operation *he_operation;
    const struct ieee80211_he_spr *he_spr;
    const struct ieee80211_mu_edca_param_set *mu_edca_param_set;
    const struct ieee80211_he_6ghz_capa *he_6ghz_capa;
    const struct ieee80211_tx_pwr_env *tx_pwr_env[IEEE80211_TPE_MAX_IE_COUNT];
    const u8 *uora_element;
    const u8 *mesh_id;
    const u8 *peering;
    const __le16 *awake_window;
    const u8 *preq;
    const u8 *prep;
    const u8 *perr;
    const struct ieee80211_rann_ie *rann;
    const struct ieee80211_channel_sw_ie *ch_switch_ie;
    const struct ieee80211_ext_chansw_ie *ext_chansw_ie;
    const struct ieee80211_wide_bw_chansw_ie *wide_bw_chansw_ie;
    const u8 *max_channel_switch_time;
    const u8 *country_elem;
    const u8 *pwr_constr_elem;
    const u8 *cisco_dtpc_elem;
    const struct ieee80211_timeout_interval_ie *timeout_int;
    const u8 *opmode_notif;
    const struct ieee80211_sec_chan_offs_ie *sec_chan_offs;
    struct ieee80211_mesh_chansw_params_ie *mesh_chansw_params_ie;
    const struct ieee80211_bss_max_idle_period_ie *max_idle_period_ie;
    const struct ieee80211_multiple_bssid_configuration *mbssid_config_ie;
    const struct ieee80211_bssid_index *bssid_index;
    u8 max_bssid_indicator;
    u8 dtim_count;
    u8 dtim_period;
    const struct ieee80211_addba_ext_ie *addba_ext_ie;
    const struct ieee80211_s1g_cap *s1g_capab;
    const struct ieee80211_s1g_oper_ie *s1g_oper;
    const struct ieee80211_s1g_bcn_compat_ie *s1g_bcn_compat;
    const struct ieee80211_aid_response_ie *aid_resp;
    const struct ieee80211_eht_cap_elem *eht_cap;
    const struct ieee80211_eht_operation *eht_operation;

    /* length of them, respectively */
    u8 ext_capab_len;
    u8 ssid_len;
    u8 supp_rates_len;
    u8 tim_len;
    u8 rsn_len;
    u8 rsnx_len;
    u8 ext_supp_rates_len;
    u8 wmm_info_len;
    u8 wmm_param_len;
    u8 he_cap_len;
    u8 mesh_id_len;
    u8 peering_len;
    u8 preq_len;
    u8 prep_len;
    u8 perr_len;
    u8 country_elem_len;
    u8 bssid_index_len;
    u8 tx_pwr_env_len[IEEE80211_TPE_MAX_IE_COUNT];
    u8 tx_pwr_env_num;
    u8 eht_cap_len;

    /* whether a parse error occurred while retrieving these elements */
    bool parse_error;

    /*
     * scratch buffer that can be used for various element parsing related
     * tasks, e.g., element de-fragmentation etc.
     */
    size_t scratch_len;
    u8 *scratch_pos;
    u8 scratch[];
};

static inline struct ieee80211_local *hw_to_local(
    struct ieee80211_hw *hw)
{
    return container_of(hw, struct ieee80211_local, hw);
}

static inline struct txq_info *to_txq_info(struct ieee80211_txq *txq)
{
    return container_of(txq, struct txq_info, txq);
}

static inline bool txq_has_queue(struct ieee80211_txq *txq)
{
    struct txq_info *txqi = to_txq_info(txq);

    return !(skb_queue_empty(&txqi->frags) && !txqi->tin.backlog_packets);
}

static inline struct airtime_info *to_airtime_info(struct ieee80211_txq *txq)
{
    struct ieee80211_sub_if_data *sdata;
    struct sta_info *sta;

    if (txq->sta) {
        sta = container_of(txq->sta, struct sta_info, sta);
        return &sta->airtime[txq->ac];
    }

    sdata = vif_to_sdata(txq->vif);
    return &sdata->airtime[txq->ac];
}

/* To avoid divisions in the fast path, we keep pre-computed reciprocals for
 * airtime weight calculations. There are two different weights to keep track
 * of: The per-station weight and the sum of weights per phy.
 *
 * For the per-station weights (kept in airtime_info below), we use 32-bit
 * reciprocals with a devisor of 2^19. This lets us keep the multiplications and
 * divisions for the station weights as 32-bit operations at the cost of a bit
 * of rounding error for high weights; but the choice of divisor keeps rounding
 * errors <10% for weights <2^15, assuming no more than 8ms of airtime is
 * reported at a time.
 *
 * For the per-phy sum of weights the values can get higher, so we use 64-bit
 * operations for those with a 32-bit divisor, which should avoid any
 * significant rounding errors.
 */
#define IEEE80211_RECIPROCAL_DIVISOR_64 0x100000000ULL
#define IEEE80211_RECIPROCAL_SHIFT_64 32
#define IEEE80211_RECIPROCAL_DIVISOR_32 0x80000U
#define IEEE80211_RECIPROCAL_SHIFT_32 19

static inline void airtime_weight_set(struct airtime_info *air_info, u16 weight)
{
    if (air_info->weight == weight)
        return;

    air_info->weight = weight;
    if (weight) {
        air_info->weight_reciprocal =
            IEEE80211_RECIPROCAL_DIVISOR_32 / weight;
    } else {
        air_info->weight_reciprocal = 0;
    }
}

static inline void airtime_weight_sum_set(struct airtime_sched_info *air_sched,
                      int weight_sum)
{
    if (air_sched->weight_sum == weight_sum)
        return;

    air_sched->weight_sum = weight_sum;
    if (air_sched->weight_sum) {
        air_sched->weight_sum_reciprocal = IEEE80211_RECIPROCAL_DIVISOR_64;
        do_div(air_sched->weight_sum_reciprocal, air_sched->weight_sum);
    } else {
        air_sched->weight_sum_reciprocal = 0;
    }
}

/* A problem when trying to enforce airtime fairness is that we want to divide
 * the airtime between the currently *active* stations. However, basing this on
 * the instantaneous queue state of stations doesn't work, as queues tend to
 * oscillate very quickly between empty and occupied, leading to the scheduler
 * thinking only a single station is active when deciding whether to allow
 * transmission (and thus not throttling correctly).
 *
 * To fix this we use a timer-based notion of activity: a station is considered
 * active if it has been scheduled within the last 100 ms; we keep a separate
 * list of all the stations considered active in this manner, and lazily update
 * the total weight of active stations from this list (filtering the stations in
 * the list by their 'last active' time).
 *
 * We add one additional safeguard to guard against stations that manage to get
 * scheduled every 100 ms but don't transmit a lot of data, and thus don't use
 * up any airtime. Such stations would be able to get priority for an extended
 * period of time if they do start transmitting at full capacity again, and so
 * we add an explicit maximum for how far behind a station is allowed to fall in
 * the virtual airtime domain. This limit is set to a relatively high value of
 * 20 ms because the main mechanism for catching up idle stations is the active
 * state as described above; i.e., the hard limit should only be hit in
 * pathological cases.
 */
#define AIRTIME_ACTIVE_DURATION (100 * NSEC_PER_MSEC)
#define AIRTIME_MAX_BEHIND 20000 /* 20 ms */

static inline bool airtime_is_active(struct airtime_info *air_info, u64 now)
{
    return air_info->last_scheduled >= now - AIRTIME_ACTIVE_DURATION;
}

static inline void airtime_set_active(struct airtime_sched_info *air_sched,
                      struct airtime_info *air_info, u64 now)
{
    air_info->last_scheduled = now;
    air_sched->last_schedule_activity = now;
    list_move_tail(&air_info->list, &air_sched->active_list);
}

static inline bool airtime_catchup_v_t(struct airtime_sched_info *air_sched,
                       u64 v_t, u64 now)
{
    air_sched->v_t = v_t;
    return true;
}

static inline void init_airtime_info(struct airtime_info *air_info,
                     struct airtime_sched_info *air_sched)
{
    atomic_set(&air_info->aql_tx_pending, 0);
    air_info->aql_limit_low = air_sched->aql_txq_limit_low;
    air_info->aql_limit_high = air_sched->aql_txq_limit_high;
    airtime_weight_set(air_info, IEEE80211_DEFAULT_AIRTIME_WEIGHT);
    INIT_LIST_HEAD(&air_info->list);
}

static inline int ieee80211_bssid_match(const u8 *raddr, const u8 *addr)
{
    return ether_addr_equal(raddr, addr) ||
           is_broadcast_ether_addr(raddr);
}

static inline bool
ieee80211_have_rx_timestamp(struct ieee80211_rx_status *status)
{
    WARN_ON_ONCE(status->flag & RX_FLAG_MACTIME_START &&
             status->flag & RX_FLAG_MACTIME_END);
    return !!(status->flag & (RX_FLAG_MACTIME_START | RX_FLAG_MACTIME_END |
                  RX_FLAG_MACTIME_PLCP_START));
}

void ieee80211_vif_inc_num_mcast(struct ieee80211_sub_if_data *sdata);
void ieee80211_vif_dec_num_mcast(struct ieee80211_sub_if_data *sdata);

/* This function returns the number of multicast stations connected to this
 * interface. It returns -1 if that number is not tracked, that is for netdevs
 * not in AP or AP_VLAN mode or when using 4addr.
 */
static inline int
ieee80211_vif_get_num_mcast_if(struct ieee80211_sub_if_data *sdata)
{
    if (sdata->vif.type == NL80211_IFTYPE_AP)
        return atomic_read(&sdata->u.ap.num_mcast_sta);
    if (sdata->vif.type == NL80211_IFTYPE_AP_VLAN && !sdata->u.vlan.sta)
        return atomic_read(&sdata->u.vlan.num_mcast_sta);
    return -1;
}

u64 ieee80211_calculate_rx_timestamp(struct ieee80211_local *local,
                     struct ieee80211_rx_status *status,
                     unsigned int mpdu_len,
                     unsigned int mpdu_offset);
int ieee80211_hw_config(struct ieee80211_local *local, u32 changed);
void ieee80211_tx_set_protected(struct ieee80211_tx_data *tx);
void ieee80211_bss_info_change_notify(struct ieee80211_sub_if_data *sdata,
                      u32 changed);
void ieee80211_configure_filter(struct ieee80211_local *local);
u32 ieee80211_reset_erp_info(struct ieee80211_sub_if_data *sdata);

u64 ieee80211_mgmt_tx_cookie(struct ieee80211_local *local);
int ieee80211_attach_ack_skb(struct ieee80211_local *local, struct sk_buff *skb,
                 u64 *cookie, gfp_t gfp);

void ieee80211_check_fast_rx(struct sta_info *sta);
void __ieee80211_check_fast_rx_iface(struct ieee80211_sub_if_data *sdata);
void ieee80211_check_fast_rx_iface(struct ieee80211_sub_if_data *sdata);
void ieee80211_clear_fast_rx(struct sta_info *sta);

/* STA code */
void ieee80211_sta_setup_sdata(struct ieee80211_sub_if_data *sdata);
int ieee80211_mgd_auth(struct ieee80211_sub_if_data *sdata,
               struct cfg80211_auth_request *req);
int ieee80211_mgd_assoc(struct ieee80211_sub_if_data *sdata,
            struct cfg80211_assoc_request *req);
int ieee80211_mgd_deauth(struct ieee80211_sub_if_data *sdata,
             struct cfg80211_deauth_request *req);
int ieee80211_mgd_disassoc(struct ieee80211_sub_if_data *sdata,
               struct cfg80211_disassoc_request *req);
void ieee80211_send_pspoll(struct ieee80211_local *local,
               struct ieee80211_sub_if_data *sdata);
void ieee80211_recalc_ps(struct ieee80211_local *local);
void ieee80211_recalc_ps_vif(struct ieee80211_sub_if_data *sdata);
int ieee80211_set_arp_filter(struct ieee80211_sub_if_data *sdata);
void ieee80211_sta_work(struct ieee80211_sub_if_data *sdata);
void ieee80211_sta_rx_queued_mgmt(struct ieee80211_sub_if_data *sdata,
                  struct sk_buff *skb);
void ieee80211_sta_rx_queued_ext(struct ieee80211_sub_if_data *sdata,
                 struct sk_buff *skb);
void ieee80211_sta_reset_beacon_monitor(struct ieee80211_sub_if_data *sdata);
void ieee80211_sta_reset_conn_monitor(struct ieee80211_sub_if_data *sdata);
void ieee80211_mgd_stop(struct ieee80211_sub_if_data *sdata);
void ieee80211_mgd_conn_tx_status(struct ieee80211_sub_if_data *sdata,
                  __le16 fc, bool acked);
void ieee80211_mgd_quiesce(struct ieee80211_sub_if_data *sdata);
void ieee80211_sta_restart(struct ieee80211_sub_if_data *sdata);
void ieee80211_sta_handle_tspec_ac_params(struct ieee80211_sub_if_data *sdata);
void ieee80211_sta_connection_lost(struct ieee80211_sub_if_data *sdata,
                   u8 reason, bool tx);

/* IBSS code */
void ieee80211_ibss_notify_scan_completed(struct ieee80211_local *local);
void ieee80211_ibss_setup_sdata(struct ieee80211_sub_if_data *sdata);
void ieee80211_ibss_rx_no_sta(struct ieee80211_sub_if_data *sdata,
                  const u8 *bssid, const u8 *addr, u32 supp_rates);
int ieee80211_ibss_join(struct ieee80211_sub_if_data *sdata,
            struct cfg80211_ibss_params *params);
int ieee80211_ibss_leave(struct ieee80211_sub_if_data *sdata);
void ieee80211_ibss_work(struct ieee80211_sub_if_data *sdata);
void ieee80211_ibss_rx_queued_mgmt(struct ieee80211_sub_if_data *sdata,
                   struct sk_buff *skb);
int ieee80211_ibss_csa_beacon(struct ieee80211_sub_if_data *sdata,
                  struct cfg80211_csa_settings *csa_settings);
int ieee80211_ibss_finish_csa(struct ieee80211_sub_if_data *sdata);
void ieee80211_ibss_stop(struct ieee80211_sub_if_data *sdata);

/* OCB code */
void ieee80211_ocb_work(struct ieee80211_sub_if_data *sdata);
void ieee80211_ocb_rx_no_sta(struct ieee80211_sub_if_data *sdata,
                 const u8 *bssid, const u8 *addr, u32 supp_rates);
void ieee80211_ocb_setup_sdata(struct ieee80211_sub_if_data *sdata);
int ieee80211_ocb_join(struct ieee80211_sub_if_data *sdata,
               struct ocb_setup *setup);
int ieee80211_ocb_leave(struct ieee80211_sub_if_data *sdata);

/* mesh code */
void ieee80211_mesh_work(struct ieee80211_sub_if_data *sdata);
void ieee80211_mesh_rx_queued_mgmt(struct ieee80211_sub_if_data *sdata,
                   struct sk_buff *skb);
int ieee80211_mesh_csa_beacon(struct ieee80211_sub_if_data *sdata,
                  struct cfg80211_csa_settings *csa_settings);
int ieee80211_mesh_finish_csa(struct ieee80211_sub_if_data *sdata);

/* scan/BSS handling */
void ieee80211_scan_work(struct work_struct *work);
int ieee80211_request_ibss_scan(struct ieee80211_sub_if_data *sdata,
                const u8 *ssid, u8 ssid_len,
                struct ieee80211_channel **channels,
                unsigned int n_channels,
                enum nl80211_bss_scan_width scan_width);
int ieee80211_request_scan(struct ieee80211_sub_if_data *sdata,
               struct cfg80211_scan_request *req);
void ieee80211_scan_cancel(struct ieee80211_local *local);
void ieee80211_run_deferred_scan(struct ieee80211_local *local);
void ieee80211_scan_rx(struct ieee80211_local *local, struct sk_buff *skb);

void ieee80211_mlme_notify_scan_completed(struct ieee80211_local *local);
struct ieee80211_bss *
ieee80211_bss_info_update(struct ieee80211_local *local,
              struct ieee80211_rx_status *rx_status,
              struct ieee80211_mgmt *mgmt,
              size_t len,
              struct ieee80211_channel *channel);
void ieee80211_rx_bss_put(struct ieee80211_local *local,
              struct ieee80211_bss *bss);

/* scheduled scan handling */
int
__ieee80211_request_sched_scan_start(struct ieee80211_sub_if_data *sdata,
                     struct cfg80211_sched_scan_request *req);
int ieee80211_request_sched_scan_start(struct ieee80211_sub_if_data *sdata,
                       struct cfg80211_sched_scan_request *req);
int ieee80211_request_sched_scan_stop(struct ieee80211_local *local);
void ieee80211_sched_scan_end(struct ieee80211_local *local);
void ieee80211_sched_scan_stopped_work(struct work_struct *work);

/* off-channel/mgmt-tx */
void ieee80211_offchannel_stop_vifs(struct ieee80211_local *local);
void ieee80211_offchannel_return(struct ieee80211_local *local);
void ieee80211_roc_setup(struct ieee80211_local *local);
void ieee80211_start_next_roc(struct ieee80211_local *local);
void ieee80211_roc_purge(struct ieee80211_local *local,
             struct ieee80211_sub_if_data *sdata);
int ieee80211_remain_on_channel(struct wiphy *wiphy, struct wireless_dev *wdev,
                struct ieee80211_channel *chan,
                unsigned int duration, u64 *cookie);
int ieee80211_cancel_remain_on_channel(struct wiphy *wiphy,
                       struct wireless_dev *wdev, u64 cookie);
int ieee80211_mgmt_tx(struct wiphy *wiphy, struct wireless_dev *wdev,
              struct cfg80211_mgmt_tx_params *params, u64 *cookie);
int ieee80211_mgmt_tx_cancel_wait(struct wiphy *wiphy,
                  struct wireless_dev *wdev, u64 cookie);

/* channel switch handling */
void ieee80211_csa_finalize_work(struct work_struct *work);
int ieee80211_channel_switch(struct wiphy *wiphy, struct net_device *dev,
                 struct cfg80211_csa_settings *params);

/* color change handling */
void ieee80211_color_change_finalize_work(struct work_struct *work);

/* interface handling */
#define MAC80211_SUPPORTED_FEATURES_TX  (NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM | \
                     NETIF_F_HW_CSUM | NETIF_F_SG | \
                     NETIF_F_HIGHDMA | NETIF_F_GSO_SOFTWARE)
#define MAC80211_SUPPORTED_FEATURES_RX  (NETIF_F_RXCSUM)
#define MAC80211_SUPPORTED_FEATURES (MAC80211_SUPPORTED_FEATURES_TX | \
                     MAC80211_SUPPORTED_FEATURES_RX)

int ieee80211_iface_init(void);
void ieee80211_iface_exit(void);
int ieee80211_if_add(struct ieee80211_local *local, const char *name,
             unsigned char name_assign_type,
             struct wireless_dev **new_wdev, enum nl80211_iftype type,
             struct vif_params *params);
int ieee80211_if_change_type(struct ieee80211_sub_if_data *sdata,
                 enum nl80211_iftype type);
void ieee80211_if_remove(struct ieee80211_sub_if_data *sdata);
void ieee80211_remove_interfaces(struct ieee80211_local *local);
u32 ieee80211_idle_off(struct ieee80211_local *local);
void ieee80211_recalc_idle(struct ieee80211_local *local);
void ieee80211_adjust_monitor_flags(struct ieee80211_sub_if_data *sdata,
                    const int offset);
int ieee80211_do_open(struct wireless_dev *wdev, bool coming_up);
void ieee80211_sdata_stop(struct ieee80211_sub_if_data *sdata);
int ieee80211_add_virtual_monitor(struct ieee80211_local *local);
void ieee80211_del_virtual_monitor(struct ieee80211_local *local);

bool __ieee80211_recalc_txpower(struct ieee80211_sub_if_data *sdata);
void ieee80211_recalc_txpower(struct ieee80211_sub_if_data *sdata,
                  bool update_bss);
void ieee80211_recalc_offload(struct ieee80211_local *local);

static inline bool ieee80211_sdata_running(struct ieee80211_sub_if_data *sdata)
{
    return test_bit(SDATA_STATE_RUNNING, &sdata->state);
}

/* tx handling */
void ieee80211_clear_tx_pending(struct ieee80211_local *local);
void ieee80211_tx_pending(struct tasklet_struct *t);
netdev_tx_t ieee80211_monitor_start_xmit(struct sk_buff *skb,
                     struct net_device *dev);
netdev_tx_t ieee80211_subif_start_xmit(struct sk_buff *skb,
                       struct net_device *dev);
netdev_tx_t ieee80211_subif_start_xmit_8023(struct sk_buff *skb,
                        struct net_device *dev);
void __ieee80211_subif_start_xmit(struct sk_buff *skb,
                  struct net_device *dev,
                  u32 info_flags,
                  u32 ctrl_flags,
                  u64 *cookie);
void ieee80211_purge_tx_queue(struct ieee80211_hw *hw,
                  struct sk_buff_head *skbs);
struct sk_buff *
ieee80211_build_data_template(struct ieee80211_sub_if_data *sdata,
                  struct sk_buff *skb, u32 info_flags);
void ieee80211_tx_monitor(struct ieee80211_local *local, struct sk_buff *skb,
              struct ieee80211_supported_band *sband,
              int retry_count, int shift, bool send_to_cooked,
              struct ieee80211_tx_status *status);

void ieee80211_check_fast_xmit(struct sta_info *sta);
void ieee80211_check_fast_xmit_all(struct ieee80211_local *local);
void ieee80211_check_fast_xmit_iface(struct ieee80211_sub_if_data *sdata);
void ieee80211_clear_fast_xmit(struct sta_info *sta);
int ieee80211_tx_control_port(struct wiphy *wiphy, struct net_device *dev,
                  const u8 *buf, size_t len,
                  const u8 *dest, __be16 proto, bool unencrypted,
                  u64 *cookie);
int ieee80211_probe_mesh_link(struct wiphy *wiphy, struct net_device *dev,
                  const u8 *buf, size_t len);
void ieee80211_resort_txq(struct ieee80211_hw *hw,
              struct ieee80211_txq *txq);
void ieee80211_unschedule_txq(struct ieee80211_hw *hw,
                  struct ieee80211_txq *txq,
                  bool purge);
void ieee80211_update_airtime_weight(struct ieee80211_local *local,
                     struct airtime_sched_info *air_sched,
                     u64 now, bool force);

/* HT */
void ieee80211_apply_htcap_overrides(struct ieee80211_sub_if_data *sdata,
                     struct ieee80211_sta_ht_cap *ht_cap);
bool ieee80211_ht_cap_ie_to_sta_ht_cap(struct ieee80211_sub_if_data *sdata,
                       struct ieee80211_supported_band *sband,
                       const struct ieee80211_ht_cap *ht_cap_ie,
                       struct sta_info *sta);
void ieee80211_send_delba(struct ieee80211_sub_if_data *sdata,
              const u8 *da, u16 tid,
              u16 initiator, u16 reason_code);
int ieee80211_send_smps_action(struct ieee80211_sub_if_data *sdata,
                   enum ieee80211_smps_mode smps, const u8 *da,
                   const u8 *bssid);
void ieee80211_request_smps_ap_work(struct work_struct *work);
void ieee80211_request_smps_mgd_work(struct work_struct *work);
bool ieee80211_smps_is_restrictive(enum ieee80211_smps_mode smps_mode_old,
                   enum ieee80211_smps_mode smps_mode_new);

void ___ieee80211_stop_rx_ba_session(struct sta_info *sta, u16 tid,
                     u16 initiator, u16 reason, bool stop);
void __ieee80211_stop_rx_ba_session(struct sta_info *sta, u16 tid,
                    u16 initiator, u16 reason, bool stop);
void ___ieee80211_start_rx_ba_session(struct sta_info *sta,
                      u8 dialog_token, u16 timeout,
                      u16 start_seq_num, u16 ba_policy, u16 tid,
                      u16 buf_size, bool tx, bool auto_seq,
                      const struct ieee80211_addba_ext_ie *addbaext);
void ieee80211_sta_tear_down_BA_sessions(struct sta_info *sta,
                     enum ieee80211_agg_stop_reason reason);
void ieee80211_process_delba(struct ieee80211_sub_if_data *sdata,
                 struct sta_info *sta,
                 struct ieee80211_mgmt *mgmt, size_t len);
void ieee80211_process_addba_resp(struct ieee80211_local *local,
                  struct sta_info *sta,
                  struct ieee80211_mgmt *mgmt,
                  size_t len);
void ieee80211_process_addba_request(struct ieee80211_local *local,
                     struct sta_info *sta,
                     struct ieee80211_mgmt *mgmt,
                     size_t len);

int __ieee80211_stop_tx_ba_session(struct sta_info *sta, u16 tid,
                   enum ieee80211_agg_stop_reason reason);
int ___ieee80211_stop_tx_ba_session(struct sta_info *sta, u16 tid,
                    enum ieee80211_agg_stop_reason reason);
void ieee80211_start_tx_ba_cb(struct sta_info *sta, int tid,
                  struct tid_ampdu_tx *tid_tx);
void ieee80211_stop_tx_ba_cb(struct sta_info *sta, int tid,
                 struct tid_ampdu_tx *tid_tx);
void ieee80211_ba_session_work(struct work_struct *work);
void ieee80211_tx_ba_session_handle_start(struct sta_info *sta, int tid);
void ieee80211_release_reorder_timeout(struct sta_info *sta, int tid);

u8 ieee80211_mcs_to_chains(const struct ieee80211_mcs_info *mcs);
enum nl80211_smps_mode
ieee80211_smps_mode_to_smps_mode(enum ieee80211_smps_mode smps);

/* VHT */
void
ieee80211_vht_cap_ie_to_sta_vht_cap(struct ieee80211_sub_if_data *sdata,
                    struct ieee80211_supported_band *sband,
                    const struct ieee80211_vht_cap *vht_cap_ie,
                    struct sta_info *sta);
enum ieee80211_sta_rx_bandwidth ieee80211_sta_cap_rx_bw(struct sta_info *sta);
enum ieee80211_sta_rx_bandwidth ieee80211_sta_cur_vht_bw(struct sta_info *sta);
void ieee80211_sta_set_rx_nss(struct sta_info *sta);
enum ieee80211_sta_rx_bandwidth
ieee80211_chan_width_to_rx_bw(enum nl80211_chan_width width);
enum nl80211_chan_width ieee80211_sta_cap_chan_bw(struct sta_info *sta);
void ieee80211_process_mu_groups(struct ieee80211_sub_if_data *sdata,
                 struct ieee80211_mgmt *mgmt);
u32 __ieee80211_vht_handle_opmode(struct ieee80211_sub_if_data *sdata,
                                  struct sta_info *sta, u8 opmode,
                  enum nl80211_band band);
void ieee80211_vht_handle_opmode(struct ieee80211_sub_if_data *sdata,
                 struct sta_info *sta, u8 opmode,
                 enum nl80211_band band);
void ieee80211_apply_vhtcap_overrides(struct ieee80211_sub_if_data *sdata,
                      struct ieee80211_sta_vht_cap *vht_cap);
void ieee80211_get_vht_mask_from_cap(__le16 vht_cap,
                     u16 vht_mask[NL80211_VHT_NSS_MAX]);
enum nl80211_chan_width
ieee80211_sta_rx_bw_to_chan_width(struct sta_info *sta);

/* HE */
void
ieee80211_he_cap_ie_to_sta_he_cap(struct ieee80211_sub_if_data *sdata,
                  struct ieee80211_supported_band *sband,
                  const u8 *he_cap_ie, u8 he_cap_len,
                  const struct ieee80211_he_6ghz_capa *he_6ghz_capa,
                  struct sta_info *sta);
void
ieee80211_he_spr_ie_to_bss_conf(struct ieee80211_vif *vif,
                const struct ieee80211_he_spr *he_spr_ie_elem);

void
ieee80211_he_op_ie_to_bss_conf(struct ieee80211_vif *vif,
            const struct ieee80211_he_operation *he_op_ie_elem);

/* S1G */
void ieee80211_s1g_sta_rate_init(struct sta_info *sta);
bool ieee80211_s1g_is_twt_setup(struct sk_buff *skb);
void ieee80211_s1g_rx_twt_action(struct ieee80211_sub_if_data *sdata,
                 struct sk_buff *skb);
void ieee80211_s1g_status_twt_action(struct ieee80211_sub_if_data *sdata,
                     struct sk_buff *skb);

/* Spectrum management */
void ieee80211_process_measurement_req(struct ieee80211_sub_if_data *sdata,
                       struct ieee80211_mgmt *mgmt,
                       size_t len);
/**
 * ieee80211_parse_ch_switch_ie - parses channel switch IEs
 * @sdata: the sdata of the interface which has received the frame
 * @elems: parsed 802.11 elements received with the frame
 * @current_band: indicates the current band
 * @vht_cap_info: VHT capabilities of the transmitter
 * @sta_flags: contains information about own capabilities and restrictions
 *  to decide which channel switch announcements can be accepted. Only the
 *  following subset of &enum ieee80211_sta_flags are evaluated:
 *  %IEEE80211_STA_DISABLE_HT, %IEEE80211_STA_DISABLE_VHT,
 *  %IEEE80211_STA_DISABLE_40MHZ, %IEEE80211_STA_DISABLE_80P80MHZ,
 *  %IEEE80211_STA_DISABLE_160MHZ.
 * @bssid: the currently connected bssid (for reporting)
 * @csa_ie: parsed 802.11 csa elements on count, mode, chandef and mesh ttl.
    All of them will be filled with if success only.
 * Return: 0 on success, <0 on error and >0 if there is nothing to parse.
 */
int ieee80211_parse_ch_switch_ie(struct ieee80211_sub_if_data *sdata,
                 struct ieee802_11_elems *elems,
                 enum nl80211_band current_band,
                 u32 vht_cap_info,
                 u32 sta_flags, u8 *bssid,
                 struct ieee80211_csa_ie *csa_ie);

/* Suspend/resume and hw reconfiguration */
int ieee80211_reconfig(struct ieee80211_local *local);
void ieee80211_stop_device(struct ieee80211_local *local);

int __ieee80211_suspend(struct ieee80211_hw *hw,
            struct cfg80211_wowlan *wowlan);

static inline int __ieee80211_resume(struct ieee80211_hw *hw)
{
    struct ieee80211_local *local = hw_to_local(hw);

    WARN(test_bit(SCAN_HW_SCANNING, &local->scanning) &&
         !test_bit(SCAN_COMPLETED, &local->scanning),
        "%s: resume with hardware scan still in progress\n",
        wiphy_name(hw->wiphy));

    return ieee80211_reconfig(hw_to_local(hw));
}

int ieee80211_frame_duration(enum nl80211_band band, size_t len,
                 int rate, int erp, int short_preamble,
                 int shift);
void ieee80211_regulatory_limit_wmm_params(struct ieee80211_sub_if_data *sdata,
                       struct ieee80211_tx_queue_params *qparam,
                       int ac);
void ieee80211_set_wmm_default(struct ieee80211_sub_if_data *sdata,
                   bool bss_notify, bool enable_qos);
void ieee80211_xmit(struct ieee80211_sub_if_data *sdata,
            struct sta_info *sta, struct sk_buff *skb);

void __ieee80211_tx_skb_tid_band(struct ieee80211_sub_if_data *sdata,
                 struct sk_buff *skb, int tid,
                 enum nl80211_band band);

/* sta_out needs to be checked for ERR_PTR() before using */
int ieee80211_lookup_ra_sta(struct ieee80211_sub_if_data *sdata,
                struct sk_buff *skb,
                struct sta_info **sta_out);

static inline void
ieee80211_tx_skb_tid_band(struct ieee80211_sub_if_data *sdata,
              struct sk_buff *skb, int tid,
              enum nl80211_band band)
{
    rcu_read_lock();
    __ieee80211_tx_skb_tid_band(sdata, skb, tid, band);
    rcu_read_unlock();
}

static inline void ieee80211_tx_skb_tid(struct ieee80211_sub_if_data *sdata,
                    struct sk_buff *skb, int tid)
{
    struct ieee80211_chanctx_conf *chanctx_conf;

    rcu_read_lock();
    chanctx_conf = rcu_dereference(sdata->vif.bss_conf.chanctx_conf);
    if (WARN_ON(!chanctx_conf)) {
        rcu_read_unlock();
        kfree_skb(skb);
        return;
    }

    __ieee80211_tx_skb_tid_band(sdata, skb, tid,
                    chanctx_conf->def.chan->band);
    rcu_read_unlock();
}

static inline void ieee80211_tx_skb(struct ieee80211_sub_if_data *sdata,
                    struct sk_buff *skb)
{
    /* Send all internal mgmt frames on VO. Accordingly set TID to 7. */
    ieee80211_tx_skb_tid(sdata, skb, 7);
}

struct ieee802_11_elems *ieee802_11_parse_elems_crc(const u8 *start, size_t len,
                            bool action,
                            u64 filter, u32 crc,
                            const u8 *transmitter_bssid,
                            const u8 *bss_bssid);
static inline struct ieee802_11_elems *
ieee802_11_parse_elems(const u8 *start, size_t len, bool action,
               const u8 *transmitter_bssid,
               const u8 *bss_bssid)
{
    return ieee802_11_parse_elems_crc(start, len, action, 0, 0,
                      transmitter_bssid, bss_bssid);
}


extern const int ieee802_1d_to_ac[8];

static inline int ieee80211_ac_from_tid(int tid)
{
    return ieee802_1d_to_ac[tid & 7];
}

void ieee80211_dynamic_ps_enable_work(struct work_struct *work);
void ieee80211_dynamic_ps_disable_work(struct work_struct *work);
void ieee80211_dynamic_ps_timer(struct timer_list *t);
void ieee80211_send_nullfunc(struct ieee80211_local *local,
                 struct ieee80211_sub_if_data *sdata,
                 bool powersave);
void ieee80211_send_4addr_nullfunc(struct ieee80211_local *local,
                   struct ieee80211_sub_if_data *sdata);
void ieee80211_sta_tx_notify(struct ieee80211_sub_if_data *sdata,
                 struct ieee80211_hdr *hdr, bool ack, u16 tx_time);

void ieee80211_wake_queues_by_reason(struct ieee80211_hw *hw,
                     unsigned long queues,
                     enum queue_stop_reason reason,
                     bool refcounted);
void ieee80211_stop_vif_queues(struct ieee80211_local *local,
                   struct ieee80211_sub_if_data *sdata,
                   enum queue_stop_reason reason);
void ieee80211_wake_vif_queues(struct ieee80211_local *local,
                   struct ieee80211_sub_if_data *sdata,
                   enum queue_stop_reason reason);
void ieee80211_stop_queues_by_reason(struct ieee80211_hw *hw,
                     unsigned long queues,
                     enum queue_stop_reason reason,
                     bool refcounted);
void ieee80211_wake_queue_by_reason(struct ieee80211_hw *hw, int queue,
                    enum queue_stop_reason reason,
                    bool refcounted);
void ieee80211_stop_queue_by_reason(struct ieee80211_hw *hw, int queue,
                    enum queue_stop_reason reason,
                    bool refcounted);
void ieee80211_propagate_queue_wake(struct ieee80211_local *local, int queue);
void ieee80211_add_pending_skb(struct ieee80211_local *local,
                   struct sk_buff *skb);
void ieee80211_add_pending_skbs(struct ieee80211_local *local,
                struct sk_buff_head *skbs);
void ieee80211_flush_queues(struct ieee80211_local *local,
                struct ieee80211_sub_if_data *sdata, bool drop);
void __ieee80211_flush_queues(struct ieee80211_local *local,
                  struct ieee80211_sub_if_data *sdata,
                  unsigned int queues, bool drop);

static inline bool ieee80211_can_run_worker(struct ieee80211_local *local)
{
    /*
     * It's unsafe to try to do any work during reconfigure flow.
     * When the flow ends the work will be requeued.
     */
    if (local->in_reconfig)
        return false;

    /*
     * If quiescing is set, we are racing with __ieee80211_suspend.
     * __ieee80211_suspend flushes the workers after setting quiescing,
     * and we check quiescing / suspended before enqueing new workers.
     * We should abort the worker to avoid the races below.
     */
    if (local->quiescing)
        return false;

    /*
     * We might already be suspended if the following scenario occurs:
     * __ieee80211_suspend      Control path
     *
     *              if (local->quiescing)
     *                  return;
     * local->quiescing = true;
     * flush_workqueue();
     *              queue_work(...);
     * local->suspended = true;
     * local->quiescing = false;
     *              worker starts running...
     */
    if (local->suspended)
        return false;

    return true;
}

int ieee80211_txq_setup_flows(struct ieee80211_local *local);
void ieee80211_txq_set_params(struct ieee80211_local *local);
void ieee80211_txq_teardown_flows(struct ieee80211_local *local);
void ieee80211_txq_init(struct ieee80211_sub_if_data *sdata,
            struct sta_info *sta,
            struct txq_info *txq, int tid);
void ieee80211_txq_purge(struct ieee80211_local *local,
             struct txq_info *txqi);
void ieee80211_txq_remove_vlan(struct ieee80211_local *local,
                   struct ieee80211_sub_if_data *sdata);
void ieee80211_fill_txq_stats(struct cfg80211_txq_stats *txqstats,
                  struct txq_info *txqi);
void ieee80211_wake_txqs(struct tasklet_struct *t);
void ieee80211_send_auth(struct ieee80211_sub_if_data *sdata,
             u16 transaction, u16 auth_alg, u16 status,
             const u8 *extra, size_t extra_len, const u8 *bssid,
             const u8 *da, const u8 *key, u8 key_len, u8 key_idx,
             u32 tx_flags);
void ieee80211_send_deauth_disassoc(struct ieee80211_sub_if_data *sdata,
                    const u8 *da, const u8 *bssid,
                    u16 stype, u16 reason,
                    bool send_frame, u8 *frame_buf);

enum {
    IEEE80211_PROBE_FLAG_DIRECTED       = BIT(0),
    IEEE80211_PROBE_FLAG_MIN_CONTENT    = BIT(1),
    IEEE80211_PROBE_FLAG_RANDOM_SN      = BIT(2),
};

int ieee80211_build_preq_ies(struct ieee80211_sub_if_data *sdata, u8 *buffer,
                 size_t buffer_len,
                 struct ieee80211_scan_ies *ie_desc,
                 const u8 *ie, size_t ie_len,
                 u8 bands_used, u32 *rate_masks,
                 struct cfg80211_chan_def *chandef,
                 u32 flags);
struct sk_buff *ieee80211_build_probe_req(struct ieee80211_sub_if_data *sdata,
                      const u8 *src, const u8 *dst,
                      u32 ratemask,
                      struct ieee80211_channel *chan,
                      const u8 *ssid, size_t ssid_len,
                      const u8 *ie, size_t ie_len,
                      u32 flags);
u32 ieee80211_sta_get_rates(struct ieee80211_sub_if_data *sdata,
                struct ieee802_11_elems *elems,
                enum nl80211_band band, u32 *basic_rates);
int __ieee80211_request_smps_mgd(struct ieee80211_sub_if_data *sdata,
                 enum ieee80211_smps_mode smps_mode);
void ieee80211_recalc_smps(struct ieee80211_sub_if_data *sdata);
void ieee80211_recalc_min_chandef(struct ieee80211_sub_if_data *sdata);

size_t ieee80211_ie_split_vendor(const u8 *ies, size_t ielen, size_t offset);
u8 *ieee80211_ie_build_ht_cap(u8 *pos, struct ieee80211_sta_ht_cap *ht_cap,
                  u16 cap);
u8 *ieee80211_ie_build_ht_oper(u8 *pos, struct ieee80211_sta_ht_cap *ht_cap,
                   const struct cfg80211_chan_def *chandef,
                   u16 prot_mode, bool rifs_mode);
void ieee80211_ie_build_wide_bw_cs(u8 *pos,
                   const struct cfg80211_chan_def *chandef);
u8 *ieee80211_ie_build_vht_cap(u8 *pos, struct ieee80211_sta_vht_cap *vht_cap,
                   u32 cap);
u8 *ieee80211_ie_build_vht_oper(u8 *pos, struct ieee80211_sta_vht_cap *vht_cap,
                const struct cfg80211_chan_def *chandef);
u8 ieee80211_ie_len_he_cap(struct ieee80211_sub_if_data *sdata, u8 iftype);
u8 *ieee80211_ie_build_he_cap(u32 disable_flags, u8 *pos,
                  const struct ieee80211_sta_he_cap *he_cap,
                  u8 *end);
void ieee80211_ie_build_he_6ghz_cap(struct ieee80211_sub_if_data *sdata,
                    struct sk_buff *skb);
u8 *ieee80211_ie_build_he_oper(u8 *pos, struct cfg80211_chan_def *chandef);
int ieee80211_parse_bitrates(struct cfg80211_chan_def *chandef,
                 const struct ieee80211_supported_band *sband,
                 const u8 *srates, int srates_len, u32 *rates);
int ieee80211_add_srates_ie(struct ieee80211_sub_if_data *sdata,
                struct sk_buff *skb, bool need_basic,
                enum nl80211_band band);
int ieee80211_add_ext_srates_ie(struct ieee80211_sub_if_data *sdata,
                struct sk_buff *skb, bool need_basic,
                enum nl80211_band band);
u8 *ieee80211_add_wmm_info_ie(u8 *buf, u8 qosinfo);
void ieee80211_add_s1g_capab_ie(struct ieee80211_sub_if_data *sdata,
                struct ieee80211_sta_s1g_cap *caps,
                struct sk_buff *skb);
void ieee80211_add_aid_request_ie(struct ieee80211_sub_if_data *sdata,
                  struct sk_buff *skb);

/* channel management */
bool ieee80211_chandef_ht_oper(const struct ieee80211_ht_operation *ht_oper,
                   struct cfg80211_chan_def *chandef);
bool ieee80211_chandef_vht_oper(struct ieee80211_hw *hw, u32 vht_cap_info,
                const struct ieee80211_vht_operation *oper,
                const struct ieee80211_ht_operation *htop,
                struct cfg80211_chan_def *chandef);
bool ieee80211_chandef_he_6ghz_oper(struct ieee80211_sub_if_data *sdata,
                    const struct ieee80211_he_operation *he_oper,
                    const struct ieee80211_eht_operation *eht_oper,
                    struct cfg80211_chan_def *chandef);
bool ieee80211_chandef_s1g_oper(const struct ieee80211_s1g_oper_ie *oper,
                struct cfg80211_chan_def *chandef);
u32 ieee80211_chandef_downgrade(struct cfg80211_chan_def *c);

int __must_check
ieee80211_vif_use_channel(struct ieee80211_sub_if_data *sdata,
              const struct cfg80211_chan_def *chandef,
              enum ieee80211_chanctx_mode mode);
int __must_check
ieee80211_vif_reserve_chanctx(struct ieee80211_sub_if_data *sdata,
                  const struct cfg80211_chan_def *chandef,
                  enum ieee80211_chanctx_mode mode,
                  bool radar_required);
int __must_check
ieee80211_vif_use_reserved_context(struct ieee80211_sub_if_data *sdata);
int ieee80211_vif_unreserve_chanctx(struct ieee80211_sub_if_data *sdata);

int __must_check
ieee80211_vif_change_bandwidth(struct ieee80211_sub_if_data *sdata,
                   const struct cfg80211_chan_def *chandef,
                   u32 *changed);
void ieee80211_vif_release_channel(struct ieee80211_sub_if_data *sdata);
void ieee80211_vif_vlan_copy_chanctx(struct ieee80211_sub_if_data *sdata);
void ieee80211_vif_copy_chanctx_to_vlans(struct ieee80211_sub_if_data *sdata,
                     bool clear);
int ieee80211_chanctx_refcount(struct ieee80211_local *local,
                   struct ieee80211_chanctx *ctx);

void ieee80211_recalc_smps_chanctx(struct ieee80211_local *local,
                   struct ieee80211_chanctx *chanctx);
void ieee80211_recalc_chanctx_min_def(struct ieee80211_local *local,
                      struct ieee80211_chanctx *ctx);
bool ieee80211_is_radar_required(struct ieee80211_local *local);

void ieee80211_dfs_cac_timer(unsigned long data);
void ieee80211_dfs_cac_timer_work(struct work_struct *work);
void ieee80211_dfs_cac_cancel(struct ieee80211_local *local);
void ieee80211_dfs_radar_detected_work(struct work_struct *work);
int ieee80211_send_action_csa(struct ieee80211_sub_if_data *sdata,
                  struct cfg80211_csa_settings *csa_settings);

bool ieee80211_cs_valid(const struct ieee80211_cipher_scheme *cs);
bool ieee80211_cs_list_valid(const struct ieee80211_cipher_scheme *cs, int n);
const struct ieee80211_cipher_scheme *
ieee80211_cs_get(struct ieee80211_local *local, u32 cipher,
         enum nl80211_iftype iftype);
int ieee80211_cs_headroom(struct ieee80211_local *local,
              struct cfg80211_crypto_settings *crypto,
              enum nl80211_iftype iftype);
void ieee80211_recalc_dtim(struct ieee80211_local *local,
               struct ieee80211_sub_if_data *sdata);
int ieee80211_check_combinations(struct ieee80211_sub_if_data *sdata,
                 const struct cfg80211_chan_def *chandef,
                 enum ieee80211_chanctx_mode chanmode,
                 u8 radar_detect);
int ieee80211_max_num_channels(struct ieee80211_local *local);
void ieee80211_recalc_chanctx_chantype(struct ieee80211_local *local,
                       struct ieee80211_chanctx *ctx);

/* TDLS */
int ieee80211_tdls_mgmt(struct wiphy *wiphy, struct net_device *dev,
            const u8 *peer, u8 action_code, u8 dialog_token,
            u16 status_code, u32 peer_capability,
            bool initiator, const u8 *extra_ies,
            size_t extra_ies_len);
int ieee80211_tdls_oper(struct wiphy *wiphy, struct net_device *dev,
            const u8 *peer, enum nl80211_tdls_operation oper);
void ieee80211_tdls_peer_del_work(struct work_struct *wk);
int ieee80211_tdls_channel_switch(struct wiphy *wiphy, struct net_device *dev,
                  const u8 *addr, u8 oper_class,
                  struct cfg80211_chan_def *chandef);
void ieee80211_tdls_cancel_channel_switch(struct wiphy *wiphy,
                      struct net_device *dev,
                      const u8 *addr);
void ieee80211_teardown_tdls_peers(struct ieee80211_sub_if_data *sdata);
void ieee80211_tdls_handle_disconnect(struct ieee80211_sub_if_data *sdata,
                      const u8 *peer, u16 reason);
void
ieee80211_process_tdls_channel_switch(struct ieee80211_sub_if_data *sdata,
                      struct sk_buff *skb);


const char *ieee80211_get_reason_code_string(u16 reason_code);
u16 ieee80211_encode_usf(int val);
u8 *ieee80211_get_bssid(struct ieee80211_hdr *hdr, size_t len,
            enum nl80211_iftype type);

extern const struct ethtool_ops ieee80211_ethtool_ops;

u32 ieee80211_calc_expected_tx_airtime(struct ieee80211_hw *hw,
                       struct ieee80211_vif *vif,
                       struct ieee80211_sta *pubsta,
                       int len, bool ampdu);
#ifdef CONFIG_MAC80211_NOINLINE
#define debug_noinline noinline
#else
#define debug_noinline
#endif

void ieee80211_init_frag_cache(struct ieee80211_fragment_cache *cache);
void ieee80211_destroy_frag_cache(struct ieee80211_fragment_cache *cache);

u8 ieee80211_ie_len_eht_cap(struct ieee80211_sub_if_data *sdata, u8 iftype);
u8 *ieee80211_ie_build_eht_cap(u8 *pos,
                   const struct ieee80211_sta_he_cap *he_cap,
                   const struct ieee80211_sta_eht_cap *eht_cap,
                   u8 *end);

void
ieee80211_eht_cap_ie_to_sta_eht_cap(struct ieee80211_sub_if_data *sdata,
                    struct ieee80211_supported_band *sband,
                    const u8 *he_cap_ie, u8 he_cap_len,
                    const struct ieee80211_eht_cap_elem *eht_cap_ie_elem,
                    u8 eht_cap_len, struct sta_info *sta);


//-------------------------------


struct ieee80211_hw *rdkfmac_alloc_hw(size_t priv_data_len,
                       const struct ieee80211_ops *ops);

#endif // RDKFMAC_H
