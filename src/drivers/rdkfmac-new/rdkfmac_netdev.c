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

struct sta_info {
    struct rhlist_head hash_node;
    u8 addr[ETH_ALEN];
};

const unsigned char rdkfmac_mac_addr[ETH_ALEN] = {0xab, 0xbc, 0xcd, 0xde, 0xef};

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

static const struct rhashtable_params sta_rht_params = {
    .nelem_hint = 3, /* start small */
    .automatic_shrinking = true,
    .head_offset = offsetof(struct sta_info, hash_node),
    .key_offset = offsetof(struct sta_info, addr),
    .key_len = ETH_ALEN,
    .max_size = CONFIG_MAC80211_STA_HASH_MAX_SIZE,
};

struct rdkfmac_ieee80211_driver
/*{
    struct ieee80211_hw hw;

    const struct ieee80211_ops *ops;

    u8 ext_capa[8];
};*/
{
    /* embed the driver visible part.
     * don't cast (use the static inlines below), but we keep
     * it first anyway so they become a no-op */
    struct ieee80211_hw hw;

    struct fq fq;
    struct codel_vars *cvars;
    struct codel_params cparams;

    /* protects active_txqs and txqi->schedule_order */
    spinlock_t active_txq_lock[IEEE80211_NUM_ACS];
    struct list_head active_txqs[IEEE80211_NUM_ACS];
    u16 schedule_round[IEEE80211_NUM_ACS];

    u16 airtime_flags;

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
    int probe_req_reg;
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
    bool offchannel_ps_enabled;
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

    /* TDLS channel switch */
    struct work_struct tdls_chsw_work;
    struct sk_buff_head skb_queue_tdls_chsw;
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

static void sta_info_cleanup(struct timer_list *t)
{

}

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

static int rdkfmac_stop_ap(struct wiphy *wiphy, struct net_device *dev, unsigned int link_id)
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

struct rdkfmac_vif_priv {
    u32 magic;
    u8 bssid[ETH_ALEN];
    bool assoc;
    bool bcn_en;
    u16 aid;
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
                    IEEE80211_HE_MAC_CAP3_MAX_AMPDU_LEN_EXP_EXT_2,
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
                    IEEE80211_HE_MAC_CAP3_MAX_AMPDU_LEN_EXP_EXT_2,
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

struct mac80211_rdkfmac_data {
    struct list_head list;
    struct rhash_head rht;
    struct ieee80211_hw *hw;
    struct device *dev;
    struct ieee80211_supported_band bands[NUM_NL80211_BANDS];
    struct ieee80211_channel channels_2ghz[ARRAY_SIZE(rdkfmac_channels_2ghz)];
    struct ieee80211_channel channels_5ghz[ARRAY_SIZE(rdkfmac_channels_5ghz)];
    struct ieee80211_rate rates[ARRAY_SIZE(rdkfmac_rates)];
    struct ieee80211_iface_combination if_combination;
    struct ieee80211_iface_limit if_limits[3];
    int n_if_limits;

    u32 ciphers[ARRAY_SIZE(rdkfmac_ciphers)];

    struct mac_address addresses[2];
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
    } survey_data[ARRAY_SIZE(rdkfmac_channels_2ghz) + ARRAY_SIZE(rdkfmac_channels_5ghz)];

    struct ieee80211_channel *channel;
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

    uintptr_t pending_cookie;
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
};

#define RDKFMAC_VIF_MAGIC 0x69537748

static inline void rdkfmac_check_magic(struct ieee80211_vif *vif)
{
    struct rdkfmac_vif_priv *vp = (void *)vif->drv_priv;

    WARN(vp->magic != RDKFMAC_VIF_MAGIC,
         "Invalid VIF (%p) magic %#x, %pM, %d/%d\n",
         vif, vp->magic, vif->addr, vif->type, vif->p2p);
}

static inline void rdkfmac_set_magic(struct ieee80211_vif *vif)
{
    struct rdkfmac_vif_priv *vp = (void *)vif->drv_priv;
    vp->magic = RDKFMAC_VIF_MAGIC;
}

static inline void rdkfmac_clear_magic(struct ieee80211_vif *vif)
{
    struct rdkfmac_vif_priv *vp = (void *)vif->drv_priv;
    vp->magic = 0;
}

struct rdkfmac_sta_priv {
    u32 magic;
};

#define RDKFMAC_STA_MAGIC 0x6d537749

static inline void rdkfmac_check_sta_magic(struct ieee80211_sta *sta)
{
    struct rdkfmac_sta_priv *sp = (void *)sta->drv_priv;
    WARN_ON(sp->magic != RDKFMAC_STA_MAGIC);
}

static inline void rdkfmac_set_sta_magic(struct ieee80211_sta *sta)
{
    struct rdkfmac_sta_priv *sp = (void *)sta->drv_priv;
    sp->magic = RDKFMAC_STA_MAGIC;
}

static inline void rdkfmac_clear_sta_magic(struct ieee80211_sta *sta)
{
    struct rdkfmac_sta_priv *sp = (void *)sta->drv_priv;
    sp->magic = 0;
}

static void mac80211_rdkfmacm_tx(struct ieee80211_hw *hw,
                  struct ieee80211_tx_control *control,
                  struct sk_buff *skb)
{
}

static int mac80211_rdkfmac_start(struct ieee80211_hw *hw)
{
    struct mac80211_rdkfmac_data *data = hw->priv;
    printk("%s\n", __func__);
    data->started = true;
    return 0;
}

static void mac80211_rdkfmac_stop(struct ieee80211_hw *hw)
{
    struct mac80211_rdkfmac_data *data = hw->priv;
    data->started = false;
    hrtimer_cancel(&data->beacon_timer);
    printk("%s\n", __func__);
}

static int mac80211_rdkfmac_add_interface(struct ieee80211_hw *hw,
                    struct ieee80211_vif *vif)
{
    printk("%s (type=%d mac_addr=%pM)\n", __func__, ieee80211_vif_type_p2p(vif), vif->addr);

    rdkfmac_set_magic(vif);

    vif->cab_queue = 0;
    vif->hw_queue[IEEE80211_AC_VO] = 0;
    vif->hw_queue[IEEE80211_AC_VI] = 1;
    vif->hw_queue[IEEE80211_AC_BE] = 2;
    vif->hw_queue[IEEE80211_AC_BK] = 3;

    return 0;
}

static int mac80211_rdkfmac_change_interface(struct ieee80211_hw *hw,
                       struct ieee80211_vif *vif,
                       enum nl80211_iftype newtype,
                       bool newp2p)
{
    newtype = ieee80211_iftype_p2p(newtype, newp2p);

    printk("%s (old type=%d, new type=%d, mac_addr=%pM)\n", __func__,
        ieee80211_vif_type_p2p(vif), newtype, vif->addr);

    rdkfmac_check_magic(vif);

    vif->cab_queue = 0;

    return 0;
}

static void mac80211_rdkfmac_remove_interface(
    struct ieee80211_hw *hw, struct ieee80211_vif *vif)
{
    printk("%s (type=%d mac_addr=%pM)\n", __func__, ieee80211_vif_type_p2p(vif), vif->addr);

    rdkfmac_check_magic(vif);
    rdkfmac_clear_magic(vif);
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

static inline u64 mac80211_rdkfmac_get_tsf_raw(void)
{
    return ktime_to_us(ktime_get_real());
}

static __le64 __mac80211_rdkfmac_get_tsf(struct mac80211_rdkfmac_data *data)
{
    u64 now = mac80211_rdkfmac_get_tsf_raw();
    return cpu_to_le64(now + data->tsf_offset);
}

static u64 mac80211_rdkfmac_get_tsf(struct ieee80211_hw *hw,
                  struct ieee80211_vif *vif)
{
    struct mac80211_rdkfmac_data *data = hw->priv;
    return le64_to_cpu(__mac80211_rdkfmac_get_tsf(data));
}

static void mac80211_rdkfmac_set_tsf(struct ieee80211_hw *hw,
        struct ieee80211_vif *vif, u64 tsf)
{
    struct mac80211_rdkfmac_data *data = hw->priv;
    u64 now = mac80211_rdkfmac_get_tsf(hw, vif);
    u32 bcn_int = data->beacon_int;
    u64 delta = abs(tsf - now);

    if (tsf > now) {
        data->tsf_offset += delta;
        data->bcn_delta = do_div(delta, bcn_int);
    } else {
        data->tsf_offset -= delta;
        data->bcn_delta = -(s64)do_div(delta, bcn_int);
    }
}

static const char * const rdkfmac_chanwidths[] = {
    [NL80211_CHAN_WIDTH_20_NOHT] = "noht",
    [NL80211_CHAN_WIDTH_20] = "ht20",
    [NL80211_CHAN_WIDTH_40] = "ht40",
    [NL80211_CHAN_WIDTH_80] = "vht80",
    [NL80211_CHAN_WIDTH_80P80] = "vht80p80",
    [NL80211_CHAN_WIDTH_160] = "vht160",
};

static int mac80211_rdkfmac_config(struct ieee80211_hw *hw, u32 changed)
{
    struct mac80211_rdkfmac_data *data = hw->priv;
    struct ieee80211_conf *conf = &hw->conf;
    static const char *smps_modes[IEEE80211_SMPS_NUM_MODES] = {
        [IEEE80211_SMPS_AUTOMATIC] = "auto",
        [IEEE80211_SMPS_OFF] = "off",
        [IEEE80211_SMPS_STATIC] = "static",
        [IEEE80211_SMPS_DYNAMIC] = "dynamic",
    };
    int idx;

    if (conf->chandef.chan)
        printk("%s (freq=%d(%d - %d)/%s idle=%d ps=%d smps=%s)\n", __func__,
            conf->chandef.chan->center_freq,
            conf->chandef.center_freq1,
            conf->chandef.center_freq2,
            rdkfmac_chanwidths[conf->chandef.width],
            !!(conf->flags & IEEE80211_CONF_IDLE),
            !!(conf->flags & IEEE80211_CONF_PS),
            smps_modes[conf->smps_mode]);
    else
        printk("%s (freq=0 idle=%d ps=%d smps=%s)\n", __func__,
            !!(conf->flags & IEEE80211_CONF_IDLE),
            !!(conf->flags & IEEE80211_CONF_PS),
            smps_modes[conf->smps_mode]);

    data->idle = !!(conf->flags & IEEE80211_CONF_IDLE);

    WARN_ON(conf->chandef.chan && data->use_chanctx);

    mutex_lock(&data->mutex);
    if (data->scanning && conf->chandef.chan) {
        for (idx = 0; idx < ARRAY_SIZE(data->survey_data); idx++) {
            if (data->survey_data[idx].channel == data->channel) {
                data->survey_data[idx].start =
                    data->survey_data[idx].next_start;
                data->survey_data[idx].end = jiffies;
                break;
            }
        }

        data->channel = conf->chandef.chan;

        for (idx = 0; idx < ARRAY_SIZE(data->survey_data); idx++) {
            if (data->survey_data[idx].channel &&
                data->survey_data[idx].channel != data->channel)
                continue;
            data->survey_data[idx].channel = data->channel;
            data->survey_data[idx].next_start = jiffies;
            break;
        }
    } else {
        data->channel = conf->chandef.chan;
    }
    mutex_unlock(&data->mutex);

    if (!data->started || !data->beacon_int)
        hrtimer_cancel(&data->beacon_timer);
    else if (!hrtimer_is_queued(&data->beacon_timer)) {
        u64 tsf = mac80211_rdkfmac_get_tsf(hw, NULL);
        u32 bcn_int = data->beacon_int;
        u64 until_tbtt = bcn_int - do_div(tsf, bcn_int);

        hrtimer_start(&data->beacon_timer,
                  ns_to_ktime(until_tbtt * NSEC_PER_USEC),
                  HRTIMER_MODE_REL_SOFT);
    }

    return 0;
}

static void mac80211_rdkfmac_configure_filter(struct ieee80211_hw *hw,
                        unsigned int changed_flags,
                        unsigned int *total_flags,u64 multicast)
{
    struct mac80211_rdkfmac_data *data = hw->priv;

    printk("%s\n", __func__);

    data->rx_filter = 0;
    if (*total_flags & FIF_ALLMULTI)
        data->rx_filter |= FIF_ALLMULTI;

    *total_flags = data->rx_filter;
}

static void mac80211_rdkfmac_bcn_en_iter(void *data, u8 *mac,
                       struct ieee80211_vif *vif)
{
    unsigned int *count = data;
    struct rdkfmac_vif_priv *vp = (void *)vif->drv_priv;

    if (vp->bcn_en)
        (*count)++;
}

static void mac80211_rdkfmac_bss_info_changed(struct ieee80211_hw *hw,
                        struct ieee80211_vif *vif,
                        struct ieee80211_bss_conf *info,
                        u32 changed)
{
    struct rdkfmac_vif_priv *vp = (void *)vif->drv_priv;
    struct mac80211_rdkfmac_data *data = hw->priv;

    rdkfmac_check_magic(vif);

    printk("%s(changed=0x%x vif->addr=%pM)\n", __func__, changed, vif->addr);

    if (changed & BSS_CHANGED_BSSID) {
        printk("%s: BSSID changed: %pM\n", __func__, info->bssid);
        memcpy(vp->bssid, info->bssid, ETH_ALEN);
    }

    if (changed & BSS_CHANGED_ASSOC) {
        printk("ASSOC: assoc=%d aid=%d\n", info->assoc, info->aid);
        vp->assoc = info->assoc;
        vp->aid = info->aid;
    }

    if (changed & BSS_CHANGED_BEACON_ENABLED) {
        printk("BCN EN: %d (BI=%u)\n", info->enable_beacon, info->beacon_int);
        vp->bcn_en = info->enable_beacon;
        if (data->started &&
            !hrtimer_is_queued(&data->beacon_timer) &&
            info->enable_beacon) {
            u64 tsf, until_tbtt;
            u32 bcn_int;
            data->beacon_int = info->beacon_int * 1024;
            tsf = mac80211_rdkfmac_get_tsf(hw, vif);
            bcn_int = data->beacon_int;
            until_tbtt = bcn_int - do_div(tsf, bcn_int);

            hrtimer_start(&data->beacon_timer,
                      ns_to_ktime(until_tbtt * NSEC_PER_USEC),
                      HRTIMER_MODE_REL_SOFT);
        } else if (!info->enable_beacon) {
            unsigned int count = 0;
            ieee80211_iterate_active_interfaces_atomic(
                data->hw, IEEE80211_IFACE_ITER_NORMAL,
                mac80211_rdkfmac_bcn_en_iter, &count);
            printk("beaconing vifs remaining: %u", count);
            if (count == 0) {
                hrtimer_cancel(&data->beacon_timer);
                data->beacon_int = 0;
            }
        }
    }

    if (changed & BSS_CHANGED_ERP_CTS_PROT) {
        printk("ERP_CTS_PROT: %d\n", info->use_cts_prot);
    }

    if (changed & BSS_CHANGED_ERP_PREAMBLE) {
        printk("ERP_PREAMBLE: %d\n", info->use_short_preamble);
    }

    if (changed & BSS_CHANGED_ERP_SLOT) {
        printk("ERP_SLOT: %d\n", info->use_short_slot);
    }

    if (changed & BSS_CHANGED_HT) {
        printk("HT: op_mode=0x%x\n", info->ht_operation_mode);
    }

    if (changed & BSS_CHANGED_BASIC_RATES) {
        printk("BASIC_RATES: 0x%llx\n", (unsigned long long) info->basic_rates);
    }

    if (changed & BSS_CHANGED_TXPOWER)
        printk("TX Power: %d dBm\n", info->txpower);
}

static int mac80211_rdkfmac_sta_add(struct ieee80211_hw *hw,
                  struct ieee80211_vif *vif,
                  struct ieee80211_sta *sta)
{
    rdkfmac_check_magic(vif);
    rdkfmac_set_sta_magic(sta);

    return 0;
}

static int mac80211_rdkfmac_sta_remove(struct ieee80211_hw *hw,
                     struct ieee80211_vif *vif,
                     struct ieee80211_sta *sta)
{
    rdkfmac_check_magic(vif);
    rdkfmac_clear_sta_magic(sta);

    return 0;
}

static void mac80211_rdkfmac_sta_notify(struct ieee80211_hw *hw,
                      struct ieee80211_vif *vif,
                      enum sta_notify_cmd cmd,
                      struct ieee80211_sta *sta)
{
    rdkfmac_check_magic(vif);
}

static int mac80211_rdkfmac_set_tim(struct ieee80211_hw *hw,
                  struct ieee80211_sta *sta,
                  bool set)
{
    rdkfmac_check_sta_magic(sta);
    return 0;
}

static int mac80211_rdkfmac_conf_tx(
    struct ieee80211_hw *hw,
    struct ieee80211_vif *vif, u16 queue,
    const struct ieee80211_tx_queue_params *params)
{
    printk("%s (queue=%d txop=%d cw_min=%d cw_max=%d aifs=%d)\n",
        __func__, queue,
        params->txop, params->cw_min,
        params->cw_max, params->aifs);
    return 0;
}

static int mac80211_rdkfmac_get_survey(struct ieee80211_hw *hw, int idx,
                     struct survey_info *survey)
{
    struct mac80211_rdkfmac_data *rdkfmac = hw->priv;

    if (idx < 0 || idx >= ARRAY_SIZE(rdkfmac->survey_data))
        return -ENOENT;

    mutex_lock(&rdkfmac->mutex);
    survey->channel = rdkfmac->survey_data[idx].channel;
    if (!survey->channel) {
        mutex_unlock(&rdkfmac->mutex);
        return -ENOENT;
    }

    //XXX hardcode
    survey->filled = SURVEY_INFO_NOISE_DBM |
             SURVEY_INFO_TIME |
             SURVEY_INFO_TIME_BUSY;
    survey->noise = -92;
    survey->time =
        jiffies_to_msecs(rdkfmac->survey_data[idx].end - rdkfmac->survey_data[idx].start);
    survey->time_busy = survey->time/7;
    mutex_unlock(&rdkfmac->mutex);

    return 0;
}

static int mac80211_rdkfmac_ampdu_action(struct ieee80211_hw *hw,
                       struct ieee80211_vif *vif,
                       struct ieee80211_ampdu_params *params)
{
    struct ieee80211_sta *sta = params->sta;
    enum ieee80211_ampdu_mlme_action action = params->action;
    u16 tid = params->tid;

    switch (action) {
    case IEEE80211_AMPDU_TX_START:
        ieee80211_start_tx_ba_cb_irqsafe(vif, sta->addr, tid);
        break;
    case IEEE80211_AMPDU_TX_STOP_CONT:
    case IEEE80211_AMPDU_TX_STOP_FLUSH:
    case IEEE80211_AMPDU_TX_STOP_FLUSH_CONT:
        ieee80211_stop_tx_ba_cb_irqsafe(vif, sta->addr, tid);
        break;
    case IEEE80211_AMPDU_TX_OPERATIONAL:
        break;
    case IEEE80211_AMPDU_RX_START:
    case IEEE80211_AMPDU_RX_STOP:
        break;
    default:
        return -EOPNOTSUPP;
    }

    return 0;
}

static void mac80211_rdkfmac_flush(struct ieee80211_hw *hw,
                 struct ieee80211_vif *vif,
                 u32 queues, bool drop)
{
}

static const char mac80211_rdkfmac_gstrings_stats[][ETH_GSTRING_LEN] = {
    "tx_pkts_nic",
    "tx_bytes_nic",
    "rx_pkts_nic",
    "rx_bytes_nic",
    "d_tx_dropped",
    "d_tx_failed",
    "d_ps_mode",
    "d_group",
};

#define MAC80211_RDKFMAC_SSTATS_LEN ARRAY_SIZE(mac80211_rdkfmac_gstrings_stats)

static void mac80211_rdkfmac_get_et_strings(struct ieee80211_hw *hw,
                      struct ieee80211_vif *vif,
                      u32 sset, u8 *data)
{
    if (sset == ETH_SS_STATS) {
        memcpy(data, *mac80211_rdkfmac_gstrings_stats,
               sizeof(mac80211_rdkfmac_gstrings_stats));
    }
}

static int mac80211_rdkfmac_get_et_sset_count(struct ieee80211_hw *hw,
                        struct ieee80211_vif *vif, int sset)
{
    if (sset == ETH_SS_STATS)
        return MAC80211_RDKFMAC_SSTATS_LEN;
    return 0;
}

static void mac80211_rdkfmac_get_et_stats(struct ieee80211_hw *hw,
                    struct ieee80211_vif *vif,
                    struct ethtool_stats *stats, u64 *data)
{
    struct mac80211_rdkfmac_data *ar = hw->priv;
    int i = 0;

    data[i++] = ar->tx_pkts;
    data[i++] = ar->tx_bytes;
    data[i++] = ar->rx_pkts;
    data[i++] = ar->rx_bytes;
    data[i++] = ar->tx_dropped;
    data[i++] = ar->tx_failed;
    data[i++] = ar->ps;
    data[i++] = ar->group;

    WARN_ON(i != MAC80211_RDKFMAC_SSTATS_LEN);
}

static void mac80211_rdkfmac_sw_scan(struct ieee80211_hw *hw,
                   struct ieee80211_vif *vif,
                   const u8 *mac_addr)
{
}

static void mac80211_rdkfmac_sw_scan_complete(struct ieee80211_hw *hw,
                        struct ieee80211_vif *vif)
{
}

static int test_key(struct ieee80211_hw *hw, enum set_key_cmd cmd,
               struct ieee80211_vif *vif, struct ieee80211_sta *sta,
               struct ieee80211_key_conf *key)
{
    return 0;
}

static const struct ieee80211_ops mac80211_rdkfmac_ops = {
    .tx = mac80211_rdkfmacm_tx,
    .start = mac80211_rdkfmac_start,
    .stop = mac80211_rdkfmac_stop,
    .add_interface = mac80211_rdkfmac_add_interface,
    .change_interface = mac80211_rdkfmac_change_interface,
    .remove_interface = mac80211_rdkfmac_remove_interface,
    .config = mac80211_rdkfmac_config,
    .configure_filter = mac80211_rdkfmac_configure_filter,
    .bss_info_changed = mac80211_rdkfmac_bss_info_changed,
    .sta_add = mac80211_rdkfmac_sta_add,
    .sta_remove = mac80211_rdkfmac_sta_remove,
    .sta_notify = mac80211_rdkfmac_sta_notify,
    .set_tim = mac80211_rdkfmac_set_tim,
    .conf_tx = mac80211_rdkfmac_conf_tx,
    .get_survey = mac80211_rdkfmac_get_survey,
    .ampdu_action = mac80211_rdkfmac_ampdu_action,
    .flush = mac80211_rdkfmac_flush,
    .get_tsf = mac80211_rdkfmac_get_tsf,
    .set_tsf = mac80211_rdkfmac_set_tsf,
    .get_et_sset_count = mac80211_rdkfmac_get_et_sset_count,
    .get_et_stats = mac80211_rdkfmac_get_et_stats,
    .get_et_strings = mac80211_rdkfmac_get_et_strings,
    .sw_scan_start = mac80211_rdkfmac_sw_scan,
    .sw_scan_complete = mac80211_rdkfmac_sw_scan_complete,
    .set_key = test_key,
};

static inline int rdkfmac_net_get_netgroup(struct net *net)
{
    struct rdkfmac_net *rdkfmac_net = net_generic(net, rdkfmac_net_id);

    return rdkfmac_net->netgroup;
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

static int mac80211_rdkfmac_new_radio(void)
{
    int err;
    u8 addr[ETH_ALEN];
    struct mac80211_rdkfmac_data *data;
    struct ieee80211_hw *hw;
    enum nl80211_band band;
    const struct ieee80211_ops *ops = &mac80211_rdkfmac_ops;
    struct net *net;
    int idx, i;
    int n_limits = 0;


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

    //wiphy->bss_priv_size = sizeof(struct ieee80211_bss);

    local = wiphy_priv(wiphy);


    if (rhltable_init(&local->sta_hash, &sta_rht_params)) {
            wiphy_free(wiphy);
            goto failed;
    }
    spin_lock_init(&local->tim_lock);
    mutex_init(&local->sta_mtx);
    INIT_LIST_HEAD(&local->sta_list);

    timer_setup(&local->sta_cleanup, sta_info_cleanup, 0);


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

    //data->if_limits[n_limits].types = BIT(NL80211_IFTYPE_STATION) | BIT(NL80211_IFTYPE_AP);
    data->if_limits[n_limits].types = BIT(NL80211_IFTYPE_AP);
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

    //hw->wiphy->interface_modes = BIT(NL80211_IFTYPE_STATION) | BIT(NL80211_IFTYPE_AP);
    hw->wiphy->interface_modes = BIT(NL80211_IFTYPE_AP);

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
    .ndo_start_xmit         = hwsim_mon_xmit,
    .ndo_set_mac_address    = eth_mac_addr,
    .ndo_validate_addr      = eth_validate_addr,
};

static void hwsim_mon_setup(struct net_device *dev)
{
    dev->netdev_ops = &hwsim_netdev_ops;
    dev->needs_free_netdev = true;
    ether_setup(dev);
    dev->priv_flags |= IFF_NO_QUEUE;
    dev->type = ARPHRD_IEEE80211_RADIOTAP;
    eth_zero_addr(dev->dev_addr);
    memcpy(dev->dev_addr, rdkfmac_mac_addr, ETH_ALEN);
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
