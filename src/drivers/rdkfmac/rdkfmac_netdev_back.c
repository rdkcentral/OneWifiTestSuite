#include "rdkfmac.h"
#include "rdkfmac_cmd.h"
#include "rdkfmac_cfg80211.h"
#include <net/mac80211.h>
#include <linux/ieee80211.h>
#include <linux/rtnetlink.h>
#include <net/genetlink.h>
#include <net/netns/generic.h>
#include <linux/etherdevice.h>
#include <linux/list.h>
#include <net/fq.h>
#include <crypto/arc4.h>
#include <linux/leds.h>
#include <linux/average.h>

#define NUM_DEFAULT_KEYS 4
#define NUM_DEFAULT_MGMT_KEYS 2
#define NUM_DEFAULT_BEACON_KEYS 2
#define INVALID_PTK_KEYIDX 2

#define IEEE80211_FRAGMENT_MAX 4

DECLARE_EWMA(signal, 10, 8)
DECLARE_EWMA(avg_signal, 10, 8)

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

static const struct rhashtable_params sta_rht_params = {
    .nelem_hint = 3, /* start small */
    .automatic_shrinking = true,
    .head_offset = offsetof(struct sta_info, hash_node),
    .key_offset = offsetof(struct sta_info, addr),
    .key_len = ETH_ALEN,
    .max_size = CONFIG_MAC80211_STA_HASH_MAX_SIZE,
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

enum mac80211_scan_state {
    SCAN_DECISION,
    SCAN_SET_CHANNEL,
    SCAN_SEND_PROBE,
    SCAN_SUSPEND,
    SCAN_RESUME,
    SCAN_ABORT,
};

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

struct rdkfmac_ieee80211_driver;

#ifdef CONFIG_MAC80211_LEDS
struct tpt_led_trigger {
    char name[32];
    const struct ieee80211_tpt_blink *blink_table;
    unsigned int blink_table_len;
    struct timer_list timer;
    struct rdkfmac_ieee80211_driver *local;
    unsigned long prev_traffic;
    unsigned long tx_bytes, rx_bytes;
    unsigned int active, want;
    bool running;
};
#endif

const unsigned char rdkfmac_mac_addr[ETH_ALEN] = {0xab, 0xbc, 0xcd, 0xde, 0xef};

struct rdkfmac_ieee80211_driver {
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
//#error 1
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
//#error 2
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


static const struct ieee80211_txrx_stypes
rdkfmac_cfg80211_default_mgmt_stypes[NUM_NL80211_IFTYPES] = {
    [NL80211_IFTYPE_STATION] = {
        .tx = 0xffff,
        .rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
        BIT(IEEE80211_STYPE_PROBE_REQ >> 4)
    },
    [NL80211_IFTYPE_AP] = {
        .tx = 0xffff,
        .rx = BIT(IEEE80211_STYPE_ASSOC_REQ >> 4) |
        BIT(IEEE80211_STYPE_REASSOC_REQ >> 4) |
        BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
        BIT(IEEE80211_STYPE_DISASSOC >> 4) |
        BIT(IEEE80211_STYPE_AUTH >> 4) |
        BIT(IEEE80211_STYPE_DEAUTH >> 4) |
        BIT(IEEE80211_STYPE_ACTION >> 4)
    },
};

static void *rdkfmac_wiphy_privid = &rdkfmac_wiphy_privid;

static struct wireless_dev *rdkfmac_add_virtual_intf(struct wiphy *wiphy,
                          const char *name,
                          unsigned char name_assign_t,
                          enum nl80211_iftype type,
                          struct vif_params *params)
{
    int ret = 0;

    return ERR_PTR(ret);
}

static int
rdkfmac_change_virtual_intf(struct wiphy *wiphy,
             struct net_device *dev,
             enum nl80211_iftype type,
             struct vif_params *params)
{
    dev->ieee80211_ptr->iftype = type;
    return 0;
}

int rdkfmac_del_virtual_intf(struct wiphy *wiphy, struct wireless_dev *wdev)
{
    return 0;
}

static int rdkfmac_start_ap(struct wiphy *wiphy, struct net_device *dev,
             struct cfg80211_ap_settings *settings)
{
    return 0;
}

static int rdkfmac_change_beacon(struct wiphy *wiphy, struct net_device *dev,
                  struct cfg80211_beacon_data *info)
{

    return 0;
}

static int rdkfmac_stop_ap(struct wiphy *wiphy, struct net_device *dev,
        unsigned int id)
{
    return 0;
}

static int rdkfmac_set_wiphy_params(struct wiphy *wiphy, u32 changed)
{
    return 0;
}

static void
rdkfmac_update_mgmt_frame_registrations(struct wiphy *wiphy,
                     struct wireless_dev *wdev,
                     struct mgmt_frame_regs *upd)
{

}

static int
rdkfmac_mgmt_tx(struct wiphy *wiphy, struct wireless_dev *wdev,
         struct cfg80211_mgmt_tx_params *params, u64 *cookie)
{
    return 0;
}

static int
rdkfmac_change_station(struct wiphy *wiphy, struct net_device *dev,
            const u8 *mac, struct station_parameters *params)
{
    return 0;
}

static int
rdkfmac_del_station(struct wiphy *wiphy, struct net_device *dev,
         struct station_del_parameters *params)
{
    return 0;
}

static int
rdkfmac_get_station(struct wiphy *wiphy, struct net_device *dev,
         const u8 *mac, struct station_info *sinfo)
{
    return 0;
}

static int
rdkfmac_dump_station(struct wiphy *wiphy, struct net_device *dev,
          int idx, u8 *mac, struct station_info *sinfo)
{
    return 0;
}

static int rdkfmac_add_key(struct wiphy *wiphy, struct net_device *dev,
            u8 key_index, bool pairwise,
            const u8 *mac_addr, struct key_params *params)
{
    return 0;
}

static int rdkfmac_del_key(struct wiphy *wiphy, struct net_device *dev,
            u8 key_index, bool pairwise,
            const u8 *mac_addr)
{
    return 0;
}

static int rdkfmac_set_default_key(struct wiphy *wiphy, struct net_device *dev,
                u8 key_index, bool unicast,
                bool multicast)
{
    return 0;
}

static int
rdkfmac_set_default_mgmt_key(struct wiphy *wiphy, struct net_device *dev,
              u8 key_index)
{
    return 0;
}

static int
rdkfmac_scan(struct wiphy *wiphy, struct cfg80211_scan_request *request)
{
    return 0;
}

static int
rdkfmac_connect(struct wiphy *wiphy, struct net_device *dev,
         struct cfg80211_connect_params *sme)
{
    return 0;
}

static int
rdkfmac_external_auth(struct wiphy *wiphy, struct net_device *dev,
           struct cfg80211_external_auth_params *auth)
{
    return 0;
}

static int
rdkfmac_disconnect(struct wiphy *wiphy, struct net_device *dev,
        u16 reason_code)
{
    return 0;
}

static int
rdkfmac_dump_survey(struct wiphy *wiphy, struct net_device *dev,
         int idx, struct survey_info *survey)
{
    return 0;
}

static int
rdkfmac_get_channel(struct wiphy *wiphy, struct wireless_dev *wdev,
         unsigned int link_id, struct cfg80211_chan_def *chandef)
{
    return 0;
}

static int rdkfmac_channel_switch(struct wiphy *wiphy, struct net_device *dev,
                   struct cfg80211_csa_settings *params)
{
    return 0;
}

static int rdkfmac_start_radar_detection(struct wiphy *wiphy,
                      struct net_device *ndev,
                      struct cfg80211_chan_def *chandef,
                      u32 cac_time_ms)
{
    return 0;
}

static int rdkfmac_set_mac_acl(struct wiphy *wiphy,
                struct net_device *dev,
                const struct cfg80211_acl_data *params)
{
    return 0;
}

static int rdkfmac_set_power_mgmt(struct wiphy *wiphy, struct net_device *dev,
                   bool enabled, int timeout)
{
    return 0;
}

static int rdkfmac_get_tx_power(struct wiphy *wiphy, struct wireless_dev *wdev,
                 int *dbm)
{
    return 0;
}

static int rdkfmac_set_tx_power(struct wiphy *wiphy, struct wireless_dev *wdev,
                 enum nl80211_tx_power_setting type, int mbm)
{
    return 0;
}

static int rdkfmac_update_owe_info(struct wiphy *wiphy, struct net_device *dev,
                struct cfg80211_update_owe_info *owe_info)
{
    return 0;
}



static struct cfg80211_ops rdkfmac_cfg80211_ops = {
    .add_virtual_intf   = rdkfmac_add_virtual_intf,
    .change_virtual_intf    = rdkfmac_change_virtual_intf,
    .del_virtual_intf   = rdkfmac_del_virtual_intf,
    .start_ap       = rdkfmac_start_ap,
    .change_beacon      = rdkfmac_change_beacon,
    .stop_ap        = rdkfmac_stop_ap,
    .set_wiphy_params   = rdkfmac_set_wiphy_params,
    //.update_mgmt_frame_registrations =
     //   rdkfmac_update_mgmt_frame_registrations,
    .mgmt_tx        = rdkfmac_mgmt_tx,
    .change_station     = rdkfmac_change_station,
    .del_station        = rdkfmac_del_station,
    .get_station        = rdkfmac_get_station,
    .dump_station       = rdkfmac_dump_station,
    .add_key        = rdkfmac_add_key,
    .del_key        = rdkfmac_del_key,
    .set_default_key    = rdkfmac_set_default_key,
    .set_default_mgmt_key   = rdkfmac_set_default_mgmt_key,
    .scan           = rdkfmac_scan,
    .connect        = rdkfmac_connect,
    .external_auth      = rdkfmac_external_auth,
    .disconnect     = rdkfmac_disconnect,
    .dump_survey        = rdkfmac_dump_survey,
   // .get_channel        = rdkfmac_get_channel,
    .channel_switch     = rdkfmac_channel_switch,
    .start_radar_detection  = rdkfmac_start_radar_detection,
    .set_mac_acl        = rdkfmac_set_mac_acl,
    .set_power_mgmt     = rdkfmac_set_power_mgmt,
    .get_tx_power       = rdkfmac_get_tx_power,
    .set_tx_power       = rdkfmac_set_tx_power,
    .update_owe_info    = rdkfmac_update_owe_info,
#if 0
    .suspend        = rdkfmac_suspend,
    .resume         = rdkfmac_resume,
    .set_wakeup     = rdkfmac_set_wakeup,
#endif
};













static spinlock_t radio_lock;
static LIST_HEAD(rdkfmac_radios);

static unsigned int rdkfmac_net_id;

static struct genl_family rdkfmac_genl_family;

static struct class *rdkfmac_class;

static DEFINE_IDA(rdkfmac_netgroup_ida);

struct rdkfmac_net {
    int netgroup;
    u32 wmediumd;
};

static const struct ieee80211_sband_iftype_data he_capa_2ghz[] = {
    {
        .types_mask = BIT(NL80211_IFTYPE_STATION) |
                  BIT(NL80211_IFTYPE_AP),
        .he_cap = {
            .has_he = true,
            .he_cap_elem = {
                .mac_cap_info[0] =
                    IEEE80211_HE_MAC_CAP0_HTC_HE,
                .mac_cap_info[1] =
                    IEEE80211_HE_MAC_CAP1_TF_MAC_PAD_DUR_16US |
                    IEEE80211_HE_MAC_CAP1_MULTI_TID_AGG_RX_QOS_8,
                .mac_cap_info[2] =
                    IEEE80211_HE_MAC_CAP2_BSR |
                    IEEE80211_HE_MAC_CAP2_MU_CASCADING |
                    IEEE80211_HE_MAC_CAP2_ACK_EN,
                .mac_cap_info[3] =
                    IEEE80211_HE_MAC_CAP3_OMI_CONTROL |
                    IEEE80211_HE_MAC_CAP3_MAX_AMPDU_LEN_EXP_EXT_3,
                .mac_cap_info[4] = IEEE80211_HE_MAC_CAP4_AMSDU_IN_AMPDU,
                .phy_cap_info[1] =
                    IEEE80211_HE_PHY_CAP1_PREAMBLE_PUNC_RX_MASK |
                    IEEE80211_HE_PHY_CAP1_DEVICE_CLASS_A |
                    IEEE80211_HE_PHY_CAP1_LDPC_CODING_IN_PAYLOAD |
                    IEEE80211_HE_PHY_CAP1_MIDAMBLE_RX_TX_MAX_NSTS,
                .phy_cap_info[2] =
                    IEEE80211_HE_PHY_CAP2_NDP_4x_LTF_AND_3_2US |
                    IEEE80211_HE_PHY_CAP2_STBC_TX_UNDER_80MHZ |
                    IEEE80211_HE_PHY_CAP2_STBC_RX_UNDER_80MHZ |
                    IEEE80211_HE_PHY_CAP2_UL_MU_FULL_MU_MIMO |
                    IEEE80211_HE_PHY_CAP2_UL_MU_PARTIAL_MU_MIMO,
            },
            .he_mcs_nss_supp = {
                .rx_mcs_80 = cpu_to_le16(0xfffa),
                .tx_mcs_80 = cpu_to_le16(0xfffa),
                .rx_mcs_160 = cpu_to_le16(0xffff),
                .tx_mcs_160 = cpu_to_le16(0xffff),
                .rx_mcs_80p80 = cpu_to_le16(0xffff),
                .tx_mcs_80p80 = cpu_to_le16(0xffff),
            },
        },
    },
};

static const struct ieee80211_sband_iftype_data he_capa_5ghz[] = {
    {
        .types_mask = BIT(NL80211_IFTYPE_STATION) |
                  BIT(NL80211_IFTYPE_AP),
        .he_cap = {
            .has_he = true,
            .he_cap_elem = {
                .mac_cap_info[0] =
                    IEEE80211_HE_MAC_CAP0_HTC_HE,
                .mac_cap_info[1] =
                    IEEE80211_HE_MAC_CAP1_TF_MAC_PAD_DUR_16US |
                    IEEE80211_HE_MAC_CAP1_MULTI_TID_AGG_RX_QOS_8,
                .mac_cap_info[2] =
                    IEEE80211_HE_MAC_CAP2_BSR |
                    IEEE80211_HE_MAC_CAP2_MU_CASCADING |
                    IEEE80211_HE_MAC_CAP2_ACK_EN,
                .mac_cap_info[3] =
                    IEEE80211_HE_MAC_CAP3_OMI_CONTROL |
                    IEEE80211_HE_MAC_CAP3_MAX_AMPDU_LEN_EXP_EXT_3,
                .mac_cap_info[4] = IEEE80211_HE_MAC_CAP4_AMSDU_IN_AMPDU,
                .phy_cap_info[0] =
                    IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_80MHZ_IN_5G |
                    IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_160MHZ_IN_5G |
                    IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_80PLUS80_MHZ_IN_5G,
                .phy_cap_info[1] =
                    IEEE80211_HE_PHY_CAP1_PREAMBLE_PUNC_RX_MASK |
                    IEEE80211_HE_PHY_CAP1_DEVICE_CLASS_A |
                    IEEE80211_HE_PHY_CAP1_LDPC_CODING_IN_PAYLOAD |
                    IEEE80211_HE_PHY_CAP1_MIDAMBLE_RX_TX_MAX_NSTS,
                .phy_cap_info[2] =
                    IEEE80211_HE_PHY_CAP2_NDP_4x_LTF_AND_3_2US |
                    IEEE80211_HE_PHY_CAP2_STBC_TX_UNDER_80MHZ |
                    IEEE80211_HE_PHY_CAP2_STBC_RX_UNDER_80MHZ |
                    IEEE80211_HE_PHY_CAP2_UL_MU_FULL_MU_MIMO |
                    IEEE80211_HE_PHY_CAP2_UL_MU_PARTIAL_MU_MIMO,

            },
            .he_mcs_nss_supp = {
                .rx_mcs_80 = cpu_to_le16(0xfffa),
                .tx_mcs_80 = cpu_to_le16(0xfffa),
                .rx_mcs_160 = cpu_to_le16(0xfffa),
                .tx_mcs_160 = cpu_to_le16(0xfffa),
                .rx_mcs_80p80 = cpu_to_le16(0xfffa),
                .tx_mcs_80p80 = cpu_to_le16(0xfffa),
            },
        },
    },
};

static void hw_roc_start(struct work_struct *work)
{
    struct mac80211_rdkfmac_data *hwsim =
        container_of(work, struct mac80211_rdkfmac_data, roc_start.work);

    mutex_lock(&hwsim->mutex);

    printk("hwsim ROC begins\n");
    hwsim->tmp_chan = hwsim->roc_chan;

    ieee80211_ready_on_channel(hwsim->hw);

    ieee80211_queue_delayed_work(hwsim->hw, &hwsim->roc_done,
                     msecs_to_jiffies(hwsim->roc_duration));

    mutex_unlock(&hwsim->mutex);
}

static void hw_roc_done(struct work_struct *work)
{
    struct mac80211_rdkfmac_data *hwsim =
        container_of(work, struct mac80211_rdkfmac_data, roc_done.work);

    mutex_lock(&hwsim->mutex);
    ieee80211_remain_on_channel_expired(hwsim->hw);
    hwsim->tmp_chan = NULL;
    mutex_unlock(&hwsim->mutex);

    printk("hwsim ROC expired\n");
}

enum {
    HWSIM_ATTR_UNSPEC,
    HWSIM_ATTR_ADDR_RECEIVER,
    HWSIM_ATTR_ADDR_TRANSMITTER,
    HWSIM_ATTR_FRAME,
    HWSIM_ATTR_FLAGS,
    HWSIM_ATTR_RX_RATE,
    HWSIM_ATTR_SIGNAL,
    HWSIM_ATTR_TX_INFO,
    HWSIM_ATTR_COOKIE,
    HWSIM_ATTR_CHANNELS,
    HWSIM_ATTR_RADIO_ID,
    HWSIM_ATTR_REG_HINT_ALPHA2,
    HWSIM_ATTR_REG_CUSTOM_REG,
    HWSIM_ATTR_REG_STRICT_REG,
    HWSIM_ATTR_SUPPORT_P2P_DEVICE,
    HWSIM_ATTR_USE_CHANCTX,
    HWSIM_ATTR_DESTROY_RADIO_ON_CLOSE,
    HWSIM_ATTR_RADIO_NAME,
    HWSIM_ATTR_NO_VIF,
    HWSIM_ATTR_FREQ,
    HWSIM_ATTR_PAD,
    HWSIM_ATTR_TX_INFO_FLAGS,
    HWSIM_ATTR_PERM_ADDR,
    HWSIM_ATTR_IFTYPE_SUPPORT,
    HWSIM_ATTR_CIPHER_SUPPORT,
    __HWSIM_ATTR_MAX,
};
#define HWSIM_ATTR_MAX (__HWSIM_ATTR_MAX - 1)

enum {
    HWSIM_CMD_UNSPEC,
    HWSIM_CMD_REGISTER,
    HWSIM_CMD_FRAME,
    HWSIM_CMD_TX_INFO_FRAME,
    HWSIM_CMD_NEW_RADIO,
    HWSIM_CMD_DEL_RADIO,
    HWSIM_CMD_GET_RADIO,
    HWSIM_CMD_ADD_MAC_ADDR,
    HWSIM_CMD_DEL_MAC_ADDR,
    __HWSIM_CMD_MAX,
};
#define HWSIM_CMD_MAX (_HWSIM_CMD_MAX - 1)

static inline int rdkfmac_net_get_netgroup(struct net *net)
{
    struct rdkfmac_net *rdkfmac_net = net_generic(net, rdkfmac_net_id);

    return rdkfmac_net->netgroup;
}

static int hwsim_unicast_netgroup(struct mac80211_rdkfmac_data *data,
                  struct sk_buff *skb, int portid)
{
    struct net *net;
    bool found = false;
    int res = -ENOENT;

    rcu_read_lock();
    for_each_net_rcu(net) {
        if (data->netgroup == rdkfmac_net_get_netgroup(net)) {
            res = genlmsg_unicast(net, skb, portid);
            found = true;
            break;
        }
    }
    rcu_read_unlock();

    if (!found)
        nlmsg_free(skb);

    return res;
}

static void mac80211_hwsim_config_mac_nl(struct ieee80211_hw *hw,
                     const u8 *addr, bool add)
{
    struct mac80211_rdkfmac_data *data = hw->priv;
    u32 _portid = READ_ONCE(data->wmediumd);
    struct sk_buff *skb;
    void *msg_head;

    if (!_portid)
        return;

    skb = genlmsg_new(GENLMSG_DEFAULT_SIZE, GFP_ATOMIC);
    if (!skb)
        return;

    msg_head = genlmsg_put(skb, 0, 0, &rdkfmac_genl_family, 0,
                   add ? HWSIM_CMD_ADD_MAC_ADDR :
                     HWSIM_CMD_DEL_MAC_ADDR);
    if (!msg_head) {
        printk("mac80211_hwsim: problem with msg_head\n");
        goto nla_put_failure;
    }

    if (nla_put(skb, HWSIM_ATTR_ADDR_TRANSMITTER,
            ETH_ALEN, data->addresses[1].addr))
        goto nla_put_failure;

    if (nla_put(skb, HWSIM_ATTR_ADDR_RECEIVER, ETH_ALEN, addr))
        goto nla_put_failure;

    genlmsg_end(skb, msg_head);

    hwsim_unicast_netgroup(data, skb, _portid);
    return;
nla_put_failure:
    nlmsg_free(skb);
}

static void mac80211_hwsim_tx_frame(struct ieee80211_hw *hw,
                    struct sk_buff *skb,
                    struct ieee80211_channel *chan)
{
}

static void hw_scan_work(struct work_struct *work)
{
    struct mac80211_rdkfmac_data *hwsim =
        container_of(work, struct mac80211_rdkfmac_data, hw_scan.work);
    struct cfg80211_scan_request *req = hwsim->hw_scan_request;
    int dwell, i;

    mutex_lock(&hwsim->mutex);
    if (hwsim->scan_chan_idx >= req->n_channels) {
        struct cfg80211_scan_info info = {
            .aborted = false,
        };

        printk("hw scan complete\n");
        ieee80211_scan_completed(hwsim->hw, &info);
        hwsim->hw_scan_request = NULL;
        hwsim->hw_scan_vif = NULL;
        hwsim->tmp_chan = NULL;
        mutex_unlock(&hwsim->mutex);
        mac80211_hwsim_config_mac_nl(hwsim->hw, hwsim->scan_addr,
                         false);
        return;
    }

    printk("hw scan %d MHz\n",
          req->channels[hwsim->scan_chan_idx]->center_freq);

    hwsim->tmp_chan = req->channels[hwsim->scan_chan_idx];
    if (hwsim->tmp_chan->flags & (IEEE80211_CHAN_NO_IR |
                      IEEE80211_CHAN_RADAR) ||
        !req->n_ssids) {
        dwell = 120;
    } else {
        dwell = 30;
        /* send probes */
        for (i = 0; i < req->n_ssids; i++) {
            struct sk_buff *probe;
            struct ieee80211_mgmt *mgmt;

            probe = ieee80211_probereq_get(hwsim->hw,
                               hwsim->scan_addr,
                               req->ssids[i].ssid,
                               req->ssids[i].ssid_len,
                               req->ie_len);
            if (!probe)
                continue;

            mgmt = (struct ieee80211_mgmt *) probe->data;
            memcpy(mgmt->da, req->bssid, ETH_ALEN);
            memcpy(mgmt->bssid, req->bssid, ETH_ALEN);

            if (req->ie_len)
                skb_put_data(probe, req->ie, req->ie_len);

            rcu_read_lock();
            if (!ieee80211_tx_prepare_skb(hwsim->hw,
                              hwsim->hw_scan_vif,
                              probe,
                              hwsim->tmp_chan->band,
                              NULL)) {
                rcu_read_unlock();
                kfree_skb(probe);
                continue;
            }

            local_bh_disable();
            mac80211_hwsim_tx_frame(hwsim->hw, probe,
                        hwsim->tmp_chan);
            rcu_read_unlock();
            local_bh_enable();
        }
    }
    ieee80211_queue_delayed_work(hwsim->hw, &hwsim->hw_scan,
                     msecs_to_jiffies(dwell));
    hwsim->survey_data[hwsim->scan_chan_idx].channel = hwsim->tmp_chan;
    hwsim->survey_data[hwsim->scan_chan_idx].start = jiffies;
    hwsim->survey_data[hwsim->scan_chan_idx].end =
        jiffies + msecs_to_jiffies(dwell);
    hwsim->scan_chan_idx++;
    mutex_unlock(&hwsim->mutex);
}


static inline int rdkfmac_net_set_netgroup(struct net *net)
{
    struct rdkfmac_net *rdkfmac_net = net_generic(net, rdkfmac_net_id);

    rdkfmac_net->netgroup = ida_simple_get(&rdkfmac_netgroup_ida, 0, 0, GFP_KERNEL);

    return rdkfmac_net->netgroup >= 0 ? 0 : -ENOMEM;
}

static __net_init int rdkfmac_init_net(struct net *net)
{
    return rdkfmac_net_set_netgroup(net);
}

static inline u32 rdkfmac_net_get_wmediumd(struct net *net)
{
    struct rdkfmac_net *rdkfmac_net= net_generic(net, rdkfmac_net_id);

    return rdkfmac_net->wmediumd;
}

static inline void rdkfmac_net_set_wmediumd(struct net *net, u32 portid)
{
    struct rdkfmac_net *rdkfmac_net = net_generic(net, rdkfmac_net_id);

    rdkfmac_net->wmediumd = portid;
}

static void __net_exit rdkfmac_exit_net(struct net *net)
{
    struct mac80211_rdkfmac_data *data, *tmp;

    list_for_each_entry_safe(data, tmp, &rdkfmac_radios, list) {
        list_del(&data->list);
        ieee80211_unregister_hw(data->hw);
        device_release_driver(data->dev);
        device_unregister(data->dev);
        ieee80211_free_hw(data->hw);
    }

    ida_simple_remove(&rdkfmac_netgroup_ida, rdkfmac_net_get_netgroup(net));
}

static struct pernet_operations rdkfmac_net_ops = {
    .init = rdkfmac_init_net,
    .exit = rdkfmac_exit_net,
    .id   = &rdkfmac_net_id,
    .size = sizeof(struct rdkfmac_net),
};

static struct platform_driver mac80211_rdkfmac_driver = {
    .driver = {
        .name = "mac80211_rdkfmac",
    },
};

static void rdkfmac_register_wmediumd(struct net *net, u32 portid)
{
    struct mac80211_rdkfmac_data *data;

    rdkfmac_net_set_wmediumd(net, portid);

    spin_lock_bh(&radio_lock);
    list_for_each_entry(data, &rdkfmac_radios, list) {
        if (data->netgroup == rdkfmac_net_get_netgroup(net))
            data->wmediumd = portid;
    }
    spin_unlock_bh(&radio_lock);
}

static int mac80211_rdkfmac_netlink_notify(struct notifier_block *nb,
                     unsigned long state,
                     void *_notify)
{
    struct netlink_notify *notify = _notify;
    struct mac80211_rdkfmac_data *data, *tmp;
    LIST_HEAD(list);

    if (state != NETLINK_URELEASE)
        return NOTIFY_DONE;

    list_for_each_entry_safe(data, tmp, &list, list) {
        list_del(&data->list);
        ieee80211_unregister_hw(data->hw);
        device_release_driver(data->dev);
        device_unregister(data->dev);
        ieee80211_free_hw(data->hw);
    }

    if (notify->portid == rdkfmac_net_get_wmediumd(notify->net)) {
        printk("mac80211_rdkfmac: wmediumd released netlink"
            " socket, switching to perfect channel medium\n");
        rdkfmac_register_wmediumd(notify->net, 0);
    }
    return NOTIFY_DONE;

}

static struct notifier_block rdkfmac_netlink_notifier = {
    .notifier_call = mac80211_rdkfmac_netlink_notify,
};

static int __init rdkfmac_init_netlink(void)
{
    int rc;

    printk("mac80211_rdkfmac: initializing netlink\n");

    rc = genl_register_family(&rdkfmac_genl_family);
    if (rc)
        goto failure;

    rc = netlink_register_notifier(&rdkfmac_netlink_notifier);
    if (rc) {
        genl_unregister_family(&rdkfmac_genl_family);
        goto failure;
    }

    return 0;

failure:
    pr_debug("mac80211_rdkfmac: error occurred in %s\n", __func__);
    return -EINVAL;
}

static void rdkfmac_exit_netlink(void)
{
    netlink_unregister_notifier(&rdkfmac_netlink_notifier);
    genl_unregister_family(&rdkfmac_genl_family);
}

static void mac80211_rdkfmac_free(void)
{
    struct mac80211_rdkfmac_data *data;

    while ((data = list_first_entry_or_null(&rdkfmac_radios,
                        struct mac80211_rdkfmac_data,
                        list))) {
        list_del(&data->list);
        ieee80211_unregister_hw(data->hw);
        device_release_driver(data->dev);
        device_unregister(data->dev);
        ieee80211_free_hw(data->hw);
    }
    class_destroy(rdkfmac_class);
}

static void mac80211_rdkfmac_he_capab(struct ieee80211_supported_band *sband)
{
    u16 n_iftype_data;

    if (sband->band == NL80211_BAND_2GHZ) {
        n_iftype_data = ARRAY_SIZE(he_capa_2ghz);
        sband->iftype_data =
            (struct ieee80211_sband_iftype_data *)he_capa_2ghz;
    } else if (sband->band == NL80211_BAND_5GHZ) {
        n_iftype_data = ARRAY_SIZE(he_capa_5ghz);
        sband->iftype_data =
            (struct ieee80211_sband_iftype_data *)he_capa_5ghz;
    } else {
        return;
    }

    sband->n_iftype_data = n_iftype_data;
}

static void mac80211_rdkfmac_beacon_tx(void *arg, u8 *mac,
                     struct ieee80211_vif *vif)
{
}

static enum hrtimer_restart
mac80211_rdkfmac_beacon(struct hrtimer *timer)
{
    struct mac80211_rdkfmac_data *data =
        container_of(timer, struct mac80211_rdkfmac_data, beacon_timer);
    struct ieee80211_hw *hw = data->hw;
    u64 bcn_int = data->beacon_int;

    if (!data->started)
        return HRTIMER_NORESTART;

    ieee80211_iterate_active_interfaces_atomic(
        hw, IEEE80211_IFACE_ITER_NORMAL,
        mac80211_rdkfmac_beacon_tx, data);

    if (data->bcn_delta) {
        bcn_int -= data->bcn_delta;
        data->bcn_delta = 0;
    }
    hrtimer_forward(&data->beacon_timer, hrtimer_get_expires(timer),
            ns_to_ktime(bcn_int * NSEC_PER_USEC));
    return HRTIMER_RESTART;
}

static int mac80211_rdkfmac_new_radio(void)
{
    int err;
    u8 addr[ETH_ALEN];
    struct mac80211_rdkfmac_data *data;
    struct ieee80211_hw *hw;
    enum nl80211_band band;
    const struct ieee80211_ops *ops;
    struct net *net;
    int idx, i;
    int n_limits = 0;

    ops = rdkfmac_get_ieee80211_ops();

//--------------------------------------------------------------

    struct rdkfmac_ieee80211_driver *local;
    struct wiphy *wiphy;
    int priv_size;

    priv_size = ALIGN(sizeof(*local), NETDEV_ALIGN) + sizeof(*data);

    wiphy = wiphy_new(&rdkfmac_cfg80211_ops, priv_size);

    if (!wiphy)
        return NULL;

    wiphy->mgmt_stypes = rdkfmac_cfg80211_default_mgmt_stypes;

    wiphy->privid = rdkfmac_wiphy_privid;

    wiphy->flags |= WIPHY_FLAG_NETNS_OK |
            WIPHY_FLAG_4ADDR_AP |
            WIPHY_FLAG_4ADDR_STATION |
            WIPHY_FLAG_REPORTS_OBSS |
            WIPHY_FLAG_OFFCHAN_TX;

    if (ops->remain_on_channel)
        wiphy->flags |= WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL;

    wiphy->features |= NL80211_FEATURE_SK_TX_STATUS |
               NL80211_FEATURE_SAE |
               NL80211_FEATURE_HT_IBSS |
               NL80211_FEATURE_VIF_TXPOWER |
               NL80211_FEATURE_MAC_ON_CREATE |
               NL80211_FEATURE_USERSPACE_MPM |
               NL80211_FEATURE_FULL_AP_CLIENT_STATE;
    wiphy_ext_feature_set(wiphy, NL80211_EXT_FEATURE_FILS_STA);
    wiphy_ext_feature_set(wiphy,
                  NL80211_EXT_FEATURE_CONTROL_PORT_OVER_NL80211);

    if (!ops->hw_scan) {
        wiphy->features |= NL80211_FEATURE_LOW_PRIORITY_SCAN |
                   NL80211_FEATURE_AP_SCAN;
        /*
         * if the driver behaves correctly using the probe request
         * (template) from mac80211, then both of these should be
         * supported even with hw scan - but let drivers opt in.
         */
        wiphy_ext_feature_set(wiphy,
                      NL80211_EXT_FEATURE_SCAN_RANDOM_SN);
        wiphy_ext_feature_set(wiphy,
                      NL80211_EXT_FEATURE_SCAN_MIN_PREQ_CONTENT);
    }

    if (!ops->set_key)
        wiphy->flags |= WIPHY_FLAG_IBSS_RSN;

    if (ops->wake_tx_queue)
        wiphy_ext_feature_set(wiphy, NL80211_EXT_FEATURE_TXQS);

    wiphy_ext_feature_set(wiphy, NL80211_EXT_FEATURE_RRM);

    local = wiphy_priv(wiphy);


    rhltable_init(&local->sta_hash, &sta_rht_params);
    spin_lock_init(&local->tim_lock);
    mutex_init(&local->sta_mtx);
    INIT_LIST_HEAD(&local->sta_list);


    local->hw.wiphy = wiphy;

    local->hw.priv = (char *)local + ALIGN(sizeof(*local), NETDEV_ALIGN);

    local->ops = ops;

    local->hw.tx_sk_pacing_shift = 7;

    /* set up some defaults */
    local->hw.queues = 1;
    local->hw.max_rates = 1;
    local->hw.max_report_rates = 0;
    local->hw.max_rx_aggregation_subframes = IEEE80211_MAX_AMPDU_BUF_HT;
    local->hw.max_tx_aggregation_subframes = IEEE80211_MAX_AMPDU_BUF_HT;
    local->hw.offchannel_tx_hw_queue = IEEE80211_INVAL_HW_QUEUE;
    local->hw.conf.long_frame_max_tx_count = wiphy->retry_long;
    local->hw.conf.short_frame_max_tx_count = wiphy->retry_short;
    local->hw.radiotap_mcs_details = IEEE80211_RADIOTAP_MCS_HAVE_MCS |
                     IEEE80211_RADIOTAP_MCS_HAVE_GI |
                     IEEE80211_RADIOTAP_MCS_HAVE_BW;
    local->hw.radiotap_vht_details = IEEE80211_RADIOTAP_VHT_KNOWN_GI |
                     IEEE80211_RADIOTAP_VHT_KNOWN_BANDWIDTH;
    local->hw.uapsd_queues = 0;
    local->hw.uapsd_max_sp_len = 0;
    local->hw.max_mtu = IEEE80211_MAX_DATA_LEN;
    //wiphy->ht_capa_mod_mask = &mac80211_ht_capa_mod_mask;
    //wiphy->vht_capa_mod_mask = &mac80211_vht_capa_mod_mask;

    local->ext_capa[7] = WLAN_EXT_CAPA8_OPMODE_NOTIF;

    wiphy->extended_capabilities = local->ext_capa;
    wiphy->extended_capabilities_mask = local->ext_capa;
    wiphy->extended_capabilities_len =
        ARRAY_SIZE(local->ext_capa);

    INIT_LIST_HEAD(&local->interfaces);
    INIT_LIST_HEAD(&local->mon_list);

    __hw_addr_init(&local->mc_list);

    mutex_init(&local->iflist_mtx);
    mutex_init(&local->mtx);

    mutex_init(&local->key_mtx);
    spin_lock_init(&local->filter_lock);
    spin_lock_init(&local->rx_path_lock);
    spin_lock_init(&local->queue_stop_reason_lock);

    INIT_LIST_HEAD(&local->chanctx_list);
    mutex_init(&local->chanctx_mtx);
    INIT_LIST_HEAD(&local->roc_list);

//----------------------------------------







    idx = 0;

    hw = &local->hw;//ieee80211_alloc_hw_nm(sizeof(*data), ops, "rdkfmac");
    if (!hw) {
        pr_debug("mac80211_rdkfmac: ieee80211_alloc_hw failed\n");
        err = -ENOMEM;
        goto failed;
    }

    net = &init_net;
    wiphy_net_set(hw->wiphy, net);

    data = hw->priv;
    data->hw = hw;

    data->dev = device_create(rdkfmac_class, NULL, 0, hw, "rdkfmac%d", idx);
    if (IS_ERR(data->dev)) {
        printk("mac80211_rdkfmac: device_create failed (%ld)\n", PTR_ERR(data->dev));
        err = -ENOMEM;
        goto failed_drvdata;
    }
    data->dev->driver = &mac80211_rdkfmac_driver.driver;
    err = device_bind_driver(data->dev);
    if (err != 0) {
        pr_debug("mac80211_rdkfmac: device_bind_driver failed (%d)\n", err);
        goto failed_bind;
    }

    SET_IEEE80211_DEV(hw, data->dev);
    eth_zero_addr(addr);
    memcpy(addr, rdkfmac_mac_addr, ETH_ALEN);
    addr[0] = 0x02;
    addr[3] = idx >> 8;
    addr[4] = idx;
    memcpy(data->addresses[0].addr, addr, ETH_ALEN);
    memcpy(data->addresses[1].addr, addr, ETH_ALEN);
    data->addresses[1].addr[0] |= 0x40;
    hw->wiphy->n_addresses = 2;
    hw->wiphy->addresses = data->addresses;

    data->channels = 1;
    data->use_chanctx = 0;
    data->idx = 0;


    data->if_limits[n_limits].max = 8;

    data->if_limits[n_limits].types = BIT(NL80211_IFTYPE_STATION) | BIT(NL80211_IFTYPE_AP);
    n_limits++;


    data->if_combination.num_different_channels = 1;
    data->if_combination.radar_detect_widths =
        BIT(NL80211_CHAN_WIDTH_20_NOHT) |
        BIT(NL80211_CHAN_WIDTH_20) |
        BIT(NL80211_CHAN_WIDTH_40) |
        BIT(NL80211_CHAN_WIDTH_80) |
        BIT(NL80211_CHAN_WIDTH_160);

    if (!n_limits) {
        err = -EINVAL;
        goto failed_hw;
    }

    data->if_combination.max_interfaces = 0;
    for (i = 0; i < n_limits; i++)
        data->if_combination.max_interfaces +=
            data->if_limits[i].max;

    data->if_combination.n_limits = n_limits;
    data->if_combination.limits = data->if_limits;

    if (data->if_combination.max_interfaces > 1) {
        hw->wiphy->iface_combinations = &data->if_combination;
        hw->wiphy->n_iface_combinations = 1;
    }

    hw->queues = 5;
    hw->offchannel_tx_hw_queue = 4;

    data->rx_rssi = -50;

    INIT_DELAYED_WORK(&data->roc_start, hw_roc_start);
    INIT_DELAYED_WORK(&data->roc_done, hw_roc_done);
    INIT_DELAYED_WORK(&data->hw_scan, hw_scan_work);

    hw->queues = 5;
    hw->offchannel_tx_hw_queue = 4;

    ieee80211_hw_set(hw, SUPPORT_FAST_XMIT);
    ieee80211_hw_set(hw, CHANCTX_STA_CSA);
    ieee80211_hw_set(hw, SUPPORTS_HT_CCK_RATES);
    ieee80211_hw_set(hw, QUEUE_CONTROL);
    ieee80211_hw_set(hw, WANT_MONITOR_VIF);
    ieee80211_hw_set(hw, AMPDU_AGGREGATION);
    ieee80211_hw_set(hw, MFP_CAPABLE);
    ieee80211_hw_set(hw, SIGNAL_DBM);
    ieee80211_hw_set(hw, SUPPORTS_PS);
    ieee80211_hw_set(hw, TDLS_WIDER_BW);
    ieee80211_hw_set(hw, SUPPORTS_MULTI_BSSID);

    hw->wiphy->flags |= WIPHY_FLAG_SUPPORTS_TDLS |
                WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL |
                WIPHY_FLAG_AP_UAPSD |
                WIPHY_FLAG_HAS_CHANNEL_SWITCH;
    hw->wiphy->features |= NL80211_FEATURE_ACTIVE_MONITOR |
                   NL80211_FEATURE_AP_MODE_CHAN_WIDTH_CHANGE |
                   NL80211_FEATURE_STATIC_SMPS |
                   NL80211_FEATURE_DYNAMIC_SMPS |
                   NL80211_FEATURE_SCAN_RANDOM_MAC_ADDR;
    wiphy_ext_feature_set(hw->wiphy, NL80211_EXT_FEATURE_VHT_IBSS);

    hw->wiphy->interface_modes = BIT(NL80211_IFTYPE_STATION) | BIT(NL80211_IFTYPE_AP);

    hw->vif_data_size = sizeof(struct rdkfmac_vif_priv);
    hw->sta_data_size = sizeof(struct rdkfmac_sta_priv);

    memcpy(data->channels_2ghz, rdkfmac_channels_2ghz, sizeof(rdkfmac_channels_2ghz));
    memcpy(data->channels_5ghz, rdkfmac_channels_5ghz, sizeof(rdkfmac_channels_5ghz));
    memcpy(data->rates, rdkfmac_rates, sizeof(rdkfmac_rates));

    for (band = NL80211_BAND_2GHZ; band < NUM_NL80211_BANDS; band++) {
        struct ieee80211_supported_band *sband = &data->bands[band];

        sband->band = band;

        switch (band) {
        case NL80211_BAND_2GHZ:
            sband->channels = data->channels_2ghz;
            sband->n_channels = ARRAY_SIZE(rdkfmac_channels_2ghz);
            sband->bitrates = data->rates;
            sband->n_bitrates = ARRAY_SIZE(rdkfmac_rates);
            break;
        case NL80211_BAND_5GHZ:
            sband->channels = data->channels_5ghz;
            sband->n_channels = ARRAY_SIZE(rdkfmac_channels_5ghz);
            sband->bitrates = data->rates + 4;
            sband->n_bitrates = ARRAY_SIZE(rdkfmac_rates) - 4;

            sband->vht_cap.vht_supported = true;
            sband->vht_cap.cap =
                IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_11454 |
                IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ |
                IEEE80211_VHT_CAP_RXLDPC |
                IEEE80211_VHT_CAP_SHORT_GI_80 |
                IEEE80211_VHT_CAP_SHORT_GI_160 |
                IEEE80211_VHT_CAP_TXSTBC |
                IEEE80211_VHT_CAP_RXSTBC_4 |
                IEEE80211_VHT_CAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK;
            sband->vht_cap.vht_mcs.rx_mcs_map =
                cpu_to_le16(IEEE80211_VHT_MCS_SUPPORT_0_9 << 0 |
                        IEEE80211_VHT_MCS_SUPPORT_0_9 << 2 |
                        IEEE80211_VHT_MCS_SUPPORT_0_9 << 4 |
                        IEEE80211_VHT_MCS_SUPPORT_0_9 << 6 |
                        IEEE80211_VHT_MCS_SUPPORT_0_9 << 8 |
                        IEEE80211_VHT_MCS_SUPPORT_0_9 << 10 |
                        IEEE80211_VHT_MCS_SUPPORT_0_9 << 12 |
                        IEEE80211_VHT_MCS_SUPPORT_0_9 << 14);
            sband->vht_cap.vht_mcs.tx_mcs_map =
                sband->vht_cap.vht_mcs.rx_mcs_map;
            break;
        default:
            continue;
        }

        sband->ht_cap.ht_supported = true;
        sband->ht_cap.cap = IEEE80211_HT_CAP_SUP_WIDTH_20_40 |
                    IEEE80211_HT_CAP_GRN_FLD |
                    IEEE80211_HT_CAP_SGI_20 |
                    IEEE80211_HT_CAP_SGI_40 |
                    IEEE80211_HT_CAP_DSSSCCK40;
        sband->ht_cap.ampdu_factor = 0x3;
        sband->ht_cap.ampdu_density = 0x6;
        memset(&sband->ht_cap.mcs, 0,
               sizeof(sband->ht_cap.mcs));
        sband->ht_cap.mcs.rx_mask[0] = 0xff;
        sband->ht_cap.mcs.rx_mask[1] = 0xff;
        sband->ht_cap.mcs.tx_params = IEEE80211_HT_MCS_TX_DEFINED;

        mac80211_rdkfmac_he_capab(sband);

        hw->wiphy->bands[band] = sband;
    }

    data->group = 1;
    mutex_init(&data->mutex);

    data->netgroup = rdkfmac_net_get_netgroup(net);
    data->wmediumd = rdkfmac_net_get_wmediumd(net);

    /* Enable frame retransmissions for lossy channels */
    hw->max_rates = 4;
    hw->max_rate_tries = 11;

    wiphy_ext_feature_set(hw->wiphy, NL80211_EXT_FEATURE_CQM_RSSI_LIST);

    hrtimer_init(&data->beacon_timer, CLOCK_MONOTONIC,
             HRTIMER_MODE_ABS_SOFT);
    data->beacon_timer.function = mac80211_rdkfmac_beacon;

    err = ieee80211_register_hw(hw);
    if (err < 0) {
        pr_debug("mac80211_rdkfmac: ieee80211_register_hw failed (%d)\n", err);
        goto failed_hw;
    }

    printk("hwaddr %pM registered\n", hw->wiphy->perm_addr);

    spin_lock_bh(&radio_lock);
    list_add_tail(&data->list, &rdkfmac_radios);
    spin_unlock_bh(&radio_lock);

    return 0;

failed_hw:
    device_release_driver(data->dev);
failed_bind:
    device_unregister(data->dev);
failed_drvdata:
    ieee80211_free_hw(hw);
failed:
    return err;
}

static netdev_tx_t hwsim_mon_xmit(struct sk_buff *skb,
                    struct net_device *dev)
{
    /* TODO: allow packet injection */
    dev_kfree_skb(skb);
    return NETDEV_TX_OK;
}

static const struct net_device_ops hwsim_netdev_ops = {
    .ndo_start_xmit     = hwsim_mon_xmit,
    .ndo_set_mac_address    = eth_mac_addr,
    .ndo_validate_addr  = eth_validate_addr,
};

static void hwsim_mon_setup(struct net_device *dev)
{
    u8 addr[ETH_ALEN];

    dev->netdev_ops = &hwsim_netdev_ops;
    dev->needs_free_netdev = true;
    ether_setup(dev);
    dev->priv_flags |= IFF_NO_QUEUE;
    dev->type = ARPHRD_IEEE80211_RADIOTAP;
    eth_zero_addr(addr);
    addr[0] = 0x12;
    eth_hw_addr_set(dev, addr);
}

static int __init rdkfmac_init_module(void)
{
    int err;

    spin_lock_init(&radio_lock);

    err = register_pernet_device(&rdkfmac_net_ops);
    if (err)
        return -EINVAL;

    err = platform_driver_register(&mac80211_rdkfmac_driver);
    if (err)
        goto out_unregister_pernet;

    err = rdkfmac_init_netlink();
    if (err)
        goto out_unregister_driver;

    rdkfmac_class = class_create(THIS_MODULE, "mac80211_rdkfmac");
    if (IS_ERR(rdkfmac_class)) {
        err = PTR_ERR(rdkfmac_class);
        goto out_exit_netlink;
    }

    err = mac80211_rdkfmac_new_radio();
    if (err < 0)
        goto out_free_radios;

    init_rdkfmac_cdev();

    return 0;

out_free_radios:
    mac80211_rdkfmac_free();
out_exit_netlink:
    rdkfmac_exit_netlink();
out_unregister_driver:
    platform_driver_unregister(&mac80211_rdkfmac_driver);
out_unregister_pernet:
    unregister_pernet_device(&rdkfmac_net_ops);
    return err;
}

static void __exit rdkfmac_cleanup_module(void)
{
    printk("%s:%d Unloading module: %s success\n", __func__, __LINE__, NETDEV_DRV_NAME);

    rdkfmac_exit_netlink();
    mac80211_rdkfmac_free();
    platform_driver_unregister(&mac80211_rdkfmac_driver);
    unregister_pernet_device(&rdkfmac_net_ops);
}

module_init(rdkfmac_init_module);
module_exit(rdkfmac_cleanup_module);
MODULE_LICENSE("GPL");
MODULE_ALIAS_RTNL_LINK(NETDEV_DRV_NAME);
