#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <net/dst.h>
#include <net/xfrm.h>
#include <net/mac80211.h>
#include <net/ieee80211_radiotap.h>
#include <linux/if_arp.h>
#include <linux/rtnetlink.h>
#include <linux/selinux_netlink.h>
#include <linux/etherdevice.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/ktime.h>
#include <net/genetlink.h>
#include <net/net_namespace.h>
#include <net/netns/generic.h>
#include <linux/rhashtable.h>
#include <linux/nospec.h>
#include <net/fq.h>
#include <net/cfg80211.h>
#include <crypto/arc4.h>
#include <linux/leds.h>
#include <linux/average.h>
#include "rdkfmac.h"
#include "rdkfmac_cmd.h"
#include "rdkfmac_cfg80211.h"
#include "wlan_emu_msg_data.h"

#define WARN_QUEUE 100
#define MAX_QUEUE 200

enum hwsim_tx_control_flags {
    HWSIM_TX_CTL_REQ_TX_STATUS  = BIT(0),
    HWSIM_TX_CTL_NO_ACK         = BIT(1),
    HWSIM_TX_STAT_ACK           = BIT(2),
};

enum hwsim_cmd {
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

#define HWSIM_CMD_CREATE_RADIO   HWSIM_CMD_NEW_RADIO
#define HWSIM_CMD_DESTROY_RADIO  HWSIM_CMD_DEL_RADIO

enum hwsim_attr {
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

struct hwsim_tx_rate {
    s8 idx;
    u8 count;
} __packed;

enum hwsim_tx_rate_flags {
    MAC80211_HWSIM_TX_RC_USE_RTS_CTS        = BIT(0),
    MAC80211_HWSIM_TX_RC_USE_CTS_PROTECT    = BIT(1),
    MAC80211_HWSIM_TX_RC_USE_SHORT_PREAMBLE = BIT(2),

    /* rate index is an HT/VHT MCS instead of an index */
    MAC80211_HWSIM_TX_RC_MCS            = BIT(3),
    MAC80211_HWSIM_TX_RC_GREEN_FIELD    = BIT(4),
    MAC80211_HWSIM_TX_RC_40_MHZ_WIDTH   = BIT(5),
    MAC80211_HWSIM_TX_RC_DUP_DATA       = BIT(6),
    MAC80211_HWSIM_TX_RC_SHORT_GI       = BIT(7),
    MAC80211_HWSIM_TX_RC_VHT_MCS        = BIT(8),
    MAC80211_HWSIM_TX_RC_80_MHZ_WIDTH   = BIT(9),
    MAC80211_HWSIM_TX_RC_160_MHZ_WIDTH  = BIT(10),
};

struct hwsim_tx_rate_flag {
    s8 idx;
    u16 flags;
} __packed;

static const struct ieee80211_regdomain hwsim_world_regdom_custom_01 = {
    .n_reg_rules = 5,
    .alpha2 =  "99",
    .reg_rules = {
        REG_RULE(2412-10, 2462+10, 40, 0, 20, 0),
        REG_RULE(2484-10, 2484+10, 40, 0, 20, 0),
        REG_RULE(5150-10, 5240+10, 40, 0, 30, 0),
        REG_RULE(5745-10, 5825+10, 40, 0, 30, 0),
        REG_RULE(5855-10, 5925+10, 40, 0, 33, 0),
    }
};

static const struct ieee80211_regdomain hwsim_world_regdom_custom_02 = {
    .n_reg_rules = 3,
    .alpha2 =  "99",
    .reg_rules = {
        REG_RULE(2412-10, 2462+10, 40, 0, 20, 0),
        REG_RULE(5725-10, 5850+10, 40, 0, 30,
             NL80211_RRF_NO_IR),
        REG_RULE(5855-10, 5925+10, 40, 0, 33, 0),
    }
};

static const struct ieee80211_regdomain hwsim_world_regdom_custom_03 = {
    .n_reg_rules = 6,
    .alpha2 =  "99",
    .reg_rules = {
        REG_RULE(2412 - 10, 2462 + 10, 40, 0, 20, 0),
        REG_RULE(2484 - 10, 2484 + 10, 40, 0, 20, 0),
        REG_RULE(5150 - 10, 5240 + 10, 40, 0, 30, 0),
        REG_RULE(5745 - 10, 5825 + 10, 40, 0, 30, 0),
        REG_RULE(5855 - 10, 5925 + 10, 40, 0, 33, 0),
        REG_RULE(5955 - 10, 7125 + 10, 320, 0, 33, 0),
    }
};

static const struct ieee80211_regdomain *hwsim_world_regdom_custom[] = {
    &hwsim_world_regdom_custom_01,
    &hwsim_world_regdom_custom_02,
    &hwsim_world_regdom_custom_03,
};

struct hwsim_vif_priv {
    u32 magic;
    u8 bssid[ETH_ALEN];
    bool assoc;
    bool bcn_en;
    u16 aid;
};

#define HWSIM_VIF_MAGIC 0x69537748

static inline void hwsim_check_magic(struct ieee80211_vif *vif)
{
    struct hwsim_vif_priv *vp = (void *)vif->drv_priv;
    WARN(vp->magic != HWSIM_VIF_MAGIC,
         "Invalid VIF (%p) magic %#x, %pM, %d/%d\n",
         vif, vp->magic, vif->addr, vif->type, vif->p2p);
}

static inline void hwsim_set_magic(struct ieee80211_vif *vif)
{
    struct hwsim_vif_priv *vp = (void *)vif->drv_priv;
    vp->magic = HWSIM_VIF_MAGIC;
}

static inline void hwsim_clear_magic(struct ieee80211_vif *vif)
{
    struct hwsim_vif_priv *vp = (void *)vif->drv_priv;
    vp->magic = 0;
}

struct hwsim_sta_priv {
    u32 magic;
};

#define HWSIM_STA_MAGIC 0x6d537749

static inline void hwsim_check_sta_magic(struct ieee80211_sta *sta)
{
    struct hwsim_sta_priv *sp = (void *)sta->drv_priv;
    WARN_ON(sp->magic != HWSIM_STA_MAGIC);
}

static inline void hwsim_set_sta_magic(struct ieee80211_sta *sta)
{
    struct hwsim_sta_priv *sp = (void *)sta->drv_priv;
    sp->magic = HWSIM_STA_MAGIC;
}

static inline void hwsim_clear_sta_magic(struct ieee80211_sta *sta)
{
    struct hwsim_sta_priv *sp = (void *)sta->drv_priv;
    sp->magic = 0;
}

struct hwsim_chanctx_priv {
    u32 magic;
};

#define HWSIM_CHANCTX_MAGIC 0x6d53774a

static inline void hwsim_check_chanctx_magic(struct ieee80211_chanctx_conf *c)
{
    struct hwsim_chanctx_priv *cp = (void *)c->drv_priv;
    WARN_ON(cp->magic != HWSIM_CHANCTX_MAGIC);
}

static inline void hwsim_set_chanctx_magic(struct ieee80211_chanctx_conf *c)
{
    struct hwsim_chanctx_priv *cp = (void *)c->drv_priv;
    cp->magic = HWSIM_CHANCTX_MAGIC;
}

static inline void hwsim_clear_chanctx_magic(struct ieee80211_chanctx_conf *c)
{
    struct hwsim_chanctx_priv *cp = (void *)c->drv_priv;
    cp->magic = 0;
}

static unsigned int hwsim_net_id;

static DEFINE_IDA(hwsim_netgroup_ida);

struct hwsim_net {
    int netgroup;
    u32 wmediumd;
};

static inline int hwsim_net_get_netgroup(struct net *net)
{
    struct hwsim_net *hwsim_net = net_generic(net, hwsim_net_id);

    return hwsim_net->netgroup;
}

static inline int hwsim_net_set_netgroup(struct net *net)
{
    struct hwsim_net *hwsim_net = net_generic(net, hwsim_net_id);

    hwsim_net->netgroup = ida_simple_get(&hwsim_netgroup_ida,
                         0, 0, GFP_KERNEL);
    return hwsim_net->netgroup >= 0 ? 0 : -ENOMEM;
}

static inline u32 hwsim_net_get_wmediumd(struct net *net)
{
    struct hwsim_net *hwsim_net = net_generic(net, hwsim_net_id);

    return hwsim_net->wmediumd;
}

static inline void hwsim_net_set_wmediumd(struct net *net, u32 portid)
{
    struct hwsim_net *hwsim_net = net_generic(net, hwsim_net_id);

    hwsim_net->wmediumd = portid;
}

static struct class *hwsim_class;

static struct net_device *hwsim_mon; /* global monitor netdev */

static struct net_device *nl_msg_mon; /* netlink monitor netdev */

static const struct ieee80211_sta_s1g_cap hwsim_s1g_cap = {
    .s1g = true,
    .cap = { S1G_CAP0_SGI_1MHZ | S1G_CAP0_SGI_2MHZ,
         0,
         0,
         S1G_CAP3_MAX_MPDU_LEN,
         0,
         S1G_CAP5_AMPDU,
         0,
         S1G_CAP7_DUP_1MHZ,
         S1G_CAP8_TWT_RESPOND | S1G_CAP8_TWT_REQUEST,
         0},
    .nss_mcs = { 0xfc | 1, /* MCS 7 for 1 SS */
    /* RX Highest Supported Long GI Data Rate 0:7 */
             0,
    /* RX Highest Supported Long GI Data Rate 0:7 */
    /* TX S1G MCS Map 0:6 */
             0xfa,
    /* TX S1G MCS Map :7 */
    /* TX Highest Supported Long GI Data Rate 0:6 */
             0x80,
    /* TX Highest Supported Long GI Data Rate 7:8 */
    /* Rx Single spatial stream and S1G-MCS Map for 1MHz */
    /* Tx Single spatial stream and S1G-MCS Map for 1MHz */
             0 },
};

static void hwsim_init_s1g_channels(struct ieee80211_channel *chans)
{
    int ch, freq;

    for (ch = 0; ch < NUM_S1G_CHANS_US; ch++) {
        freq = 902000 + (ch + 1) * 500;
        chans[ch].band = NL80211_BAND_S1GHZ;
        chans[ch].center_freq = KHZ_TO_MHZ(freq);
        chans[ch].freq_offset = freq % 1000;
        chans[ch].hw_value = ch + 1;
    }
}

static const struct ieee80211_rate hwsim_rates[] = {
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

#define DEFAULT_RX_RSSI -50

static const u32 hwsim_ciphers[] = {
    WLAN_CIPHER_SUITE_WEP40,
    WLAN_CIPHER_SUITE_WEP104,
    WLAN_CIPHER_SUITE_TKIP,
    WLAN_CIPHER_SUITE_CCMP,
    WLAN_CIPHER_SUITE_CCMP_256,
    WLAN_CIPHER_SUITE_GCMP,
    WLAN_CIPHER_SUITE_GCMP_256,
    WLAN_CIPHER_SUITE_AES_CMAC,
    WLAN_CIPHER_SUITE_BIP_CMAC_256,
    WLAN_CIPHER_SUITE_BIP_GMAC_128,
    WLAN_CIPHER_SUITE_BIP_GMAC_256,
};

#define OUI_QCA 0x001374
#define QCA_NL80211_SUBCMD_TEST 1
enum qca_nl80211_vendor_subcmds {
    QCA_WLAN_VENDOR_ATTR_TEST = 8,
    QCA_WLAN_VENDOR_ATTR_MAX = QCA_WLAN_VENDOR_ATTR_TEST
};

static const struct nla_policy
hwsim_vendor_test_policy[QCA_WLAN_VENDOR_ATTR_MAX + 1] = {
    [QCA_WLAN_VENDOR_ATTR_MAX] = { .type = NLA_U32 },
};

static int mac80211_hwsim_vendor_cmd_test(struct wiphy *wiphy,
                      struct wireless_dev *wdev,
                      const void *data, int data_len)
{
    struct sk_buff *skb;
    struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_MAX + 1];
    int err;
    u32 val;

    err = nla_parse_deprecated(tb, QCA_WLAN_VENDOR_ATTR_MAX, data,
                   data_len, hwsim_vendor_test_policy, NULL);
    if (err)
        return err;
    if (!tb[QCA_WLAN_VENDOR_ATTR_TEST])
        return -EINVAL;
    val = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_TEST]);
    wiphy_dbg(wiphy, "%s: test=%u\n", __func__, val);

    /* Send a vendor event as a test. Note that this would not normally be
     * done within a command handler, but rather, based on some other
     * trigger. For simplicity, this command is used to trigger the event
     * here.
     *
     * event_idx = 0 (index in mac80211_hwsim_vendor_commands)
     */
    skb = cfg80211_vendor_event_alloc(wiphy, wdev, 100, 0, GFP_KERNEL);
    if (skb) {
        /* skb_put() or nla_put() will fill up data within
         * NL80211_ATTR_VENDOR_DATA.
         */

        /* Add vendor data */
        nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TEST, val + 1);

        /* Send the event - this will call nla_nest_end() */
        cfg80211_vendor_event(skb, GFP_KERNEL);
    }

    /* Send a response to the command */
    skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 10);
    if (!skb)
        return -ENOMEM;

    /* skb_put() or nla_put() will fill up data within
     * NL80211_ATTR_VENDOR_DATA
     */
    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TEST, val + 2);

    return cfg80211_vendor_cmd_reply(skb);
}

static struct wiphy_vendor_command mac80211_hwsim_vendor_commands[] = {
    {
        .info = { .vendor_id = OUI_QCA,
              .subcmd = QCA_NL80211_SUBCMD_TEST },
        .flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
        .doit = mac80211_hwsim_vendor_cmd_test,
        .policy = hwsim_vendor_test_policy,
        .maxattr = QCA_WLAN_VENDOR_ATTR_MAX,
    }
};

/* Advertise support vendor specific events */
static const struct nl80211_vendor_cmd_info mac80211_hwsim_vendor_events[] = {
    { .vendor_id = OUI_QCA, .subcmd = 1 },
};

static DEFINE_SPINLOCK(hwsim_radio_lock);
static LIST_HEAD(hwsim_radios);
static struct rhashtable hwsim_radios_rht;
static int hwsim_radio_idx;
static int hwsim_radios_generation = 1;

static struct platform_driver mac80211_hwsim_driver = {
    .driver = {
        .name = "mac80211_hwsim",
    },
};

static const struct rhashtable_params hwsim_rht_params = {
    .nelem_hint = 2,
    .automatic_shrinking = true,
    .key_len = ETH_ALEN,
    .key_offset = offsetof(struct mac80211_rdkfmac_data, addresses[1]),
    .head_offset = offsetof(struct mac80211_rdkfmac_data, rht),
};

struct hwsim_radiotap_hdr {
    struct ieee80211_radiotap_header hdr;
    __le64 rt_tsft;
    u8 rt_flags;
    u8 rt_rate;
    __le16 rt_channel;
    __le16 rt_chbitmask;
} __packed;

struct hwsim_radiotap_ack_hdr {
    struct ieee80211_radiotap_header hdr;
    u8 rt_flags;
    u8 pad;
    __le16 rt_channel;
    __le16 rt_chbitmask;
} __packed;

/* MAC80211_HWSIM netlink family */
static struct genl_family hwsim_genl_family;

enum hwsim_multicast_groups {
    HWSIM_MCGRP_CONFIG,
};

static const struct genl_multicast_group hwsim_mcgrps[] = {
    [HWSIM_MCGRP_CONFIG] = { .name = "config", },
};

/* MAC80211_HWSIM netlink policy */

static const struct nla_policy hwsim_genl_policy[HWSIM_ATTR_MAX + 1] = {
    [HWSIM_ATTR_ADDR_RECEIVER] = NLA_POLICY_ETH_ADDR_COMPAT,
    [HWSIM_ATTR_ADDR_TRANSMITTER] = NLA_POLICY_ETH_ADDR_COMPAT,
    [HWSIM_ATTR_FRAME] = { .type = NLA_BINARY,
                   .len = IEEE80211_MAX_DATA_LEN },
    [HWSIM_ATTR_FLAGS] = { .type = NLA_U32 },
    [HWSIM_ATTR_RX_RATE] = { .type = NLA_U32 },
    [HWSIM_ATTR_SIGNAL] = { .type = NLA_U32 },
    [HWSIM_ATTR_TX_INFO] = { .type = NLA_BINARY,
                 .len = IEEE80211_TX_MAX_RATES *
                    sizeof(struct hwsim_tx_rate)},
    [HWSIM_ATTR_COOKIE] = { .type = NLA_U64 },
    [HWSIM_ATTR_CHANNELS] = { .type = NLA_U32 },
    [HWSIM_ATTR_RADIO_ID] = { .type = NLA_U32 },
    [HWSIM_ATTR_REG_HINT_ALPHA2] = { .type = NLA_STRING, .len = 2 },
    [HWSIM_ATTR_REG_CUSTOM_REG] = { .type = NLA_U32 },
    [HWSIM_ATTR_REG_STRICT_REG] = { .type = NLA_FLAG },
    [HWSIM_ATTR_SUPPORT_P2P_DEVICE] = { .type = NLA_FLAG },
    [HWSIM_ATTR_USE_CHANCTX] = { .type = NLA_FLAG },
    [HWSIM_ATTR_DESTROY_RADIO_ON_CLOSE] = { .type = NLA_FLAG },
    [HWSIM_ATTR_RADIO_NAME] = { .type = NLA_STRING },
    [HWSIM_ATTR_NO_VIF] = { .type = NLA_FLAG },
    [HWSIM_ATTR_FREQ] = { .type = NLA_U32 },
    [HWSIM_ATTR_TX_INFO_FLAGS] = { .type = NLA_BINARY },
    [HWSIM_ATTR_PERM_ADDR] = NLA_POLICY_ETH_ADDR_COMPAT,
    [HWSIM_ATTR_IFTYPE_SUPPORT] = { .type = NLA_U32 },
    [HWSIM_ATTR_CIPHER_SUPPORT] = { .type = NLA_BINARY },
};

static int hwsim_get_chanwidth(enum nl80211_chan_width bw)
{
    switch (bw) {
    case NL80211_CHAN_WIDTH_20_NOHT:
    case NL80211_CHAN_WIDTH_20:
        return 20;
    case NL80211_CHAN_WIDTH_40:
        return 40;
    case NL80211_CHAN_WIDTH_80:
        return 80;
    case NL80211_CHAN_WIDTH_80P80:
    case NL80211_CHAN_WIDTH_160:
        return 160;
    case NL80211_CHAN_WIDTH_320:
        return 320;
    case NL80211_CHAN_WIDTH_5:
        return 5;
    case NL80211_CHAN_WIDTH_10:
        return 10;
    case NL80211_CHAN_WIDTH_1:
        return 1;
    case NL80211_CHAN_WIDTH_2:
        return 2;
    case NL80211_CHAN_WIDTH_4:
        return 4;
    case NL80211_CHAN_WIDTH_8:
        return 8;
    case NL80211_CHAN_WIDTH_16:
        return 16;
    }

    return INT_MAX;
}

static void mac80211_hwsim_tx_frame(struct ieee80211_hw *hw,
                    struct sk_buff *skb,
                    struct ieee80211_channel *chan);

/* sysfs attributes */
static void hwsim_send_ps_poll(void *dat, u8 *mac, struct ieee80211_vif *vif)
{
    struct mac80211_rdkfmac_data *data = dat;
    struct hwsim_vif_priv *vp = (void *)vif->drv_priv;
    struct sk_buff *skb;
    struct ieee80211_pspoll *pspoll;

    if (!vp->assoc)
        return;

    wiphy_dbg(data->hw->wiphy,
          "%s: send PS-Poll to %pM for aid %d\n",
          __func__, vp->bssid, vp->aid);

    skb = dev_alloc_skb(sizeof(*pspoll));
    if (!skb)
        return;
    pspoll = skb_put(skb, sizeof(*pspoll));
    pspoll->frame_control = cpu_to_le16(IEEE80211_FTYPE_CTL |
                        IEEE80211_STYPE_PSPOLL |
                        IEEE80211_FCTL_PM);
    pspoll->aid = cpu_to_le16(0xc000 | vp->aid);
    memcpy(pspoll->bssid, vp->bssid, ETH_ALEN);
    memcpy(pspoll->ta, mac, ETH_ALEN);

    rcu_read_lock();
    mac80211_hwsim_tx_frame(data->hw, skb,
                rcu_dereference(vif->bss_conf.chanctx_conf)->def.chan);
    rcu_read_unlock();
}

static void hwsim_send_nullfunc(struct mac80211_rdkfmac_data *data, u8 *mac,
                struct ieee80211_vif *vif, int ps)
{
    struct hwsim_vif_priv *vp = (void *)vif->drv_priv;
    struct sk_buff *skb;
    struct ieee80211_hdr *hdr;

    if (!vp->assoc)
        return;

    wiphy_dbg(data->hw->wiphy,
          "%s: send data::nullfunc to %pM ps=%d\n",
          __func__, vp->bssid, ps);

    skb = dev_alloc_skb(sizeof(*hdr));
    if (!skb)
        return;
    hdr = skb_put(skb, sizeof(*hdr) - ETH_ALEN);
    hdr->frame_control = cpu_to_le16(IEEE80211_FTYPE_DATA |
                     IEEE80211_STYPE_NULLFUNC |
                     IEEE80211_FCTL_TODS |
                     (ps ? IEEE80211_FCTL_PM : 0));
    hdr->duration_id = cpu_to_le16(0);
    memcpy(hdr->addr1, vp->bssid, ETH_ALEN);
    memcpy(hdr->addr2, mac, ETH_ALEN);
    memcpy(hdr->addr3, vp->bssid, ETH_ALEN);

    rcu_read_lock();
    mac80211_hwsim_tx_frame(data->hw, skb,
                rcu_dereference(vif->bss_conf.chanctx_conf)->def.chan);
    rcu_read_unlock();
}


static void hwsim_send_nullfunc_ps(void *dat, u8 *mac,
                   struct ieee80211_vif *vif)
{
    struct mac80211_rdkfmac_data *data = dat;
    hwsim_send_nullfunc(data, mac, vif, 1);
}

static void hwsim_send_nullfunc_no_ps(void *dat, u8 *mac,
                      struct ieee80211_vif *vif)
{
    struct mac80211_rdkfmac_data *data = dat;
    hwsim_send_nullfunc(data, mac, vif, 0);
}

static int hwsim_fops_ps_read(void *dat, u64 *val)
{
    struct mac80211_rdkfmac_data *data = dat;
    *val = data->ps;
    return 0;
}

static int hwsim_fops_ps_write(void *dat, u64 val)
{
    struct mac80211_rdkfmac_data *data = dat;
    enum ps_mode old_ps;

    if (val != PS_DISABLED && val != PS_ENABLED && val != PS_AUTO_POLL &&
        val != PS_MANUAL_POLL)
        return -EINVAL;

    if (val == PS_MANUAL_POLL) {
        if (data->ps != PS_ENABLED)
            return -EINVAL;
        local_bh_disable();
        ieee80211_iterate_active_interfaces_atomic(
            data->hw, IEEE80211_IFACE_ITER_NORMAL,
            hwsim_send_ps_poll, data);
        local_bh_enable();
        return 0;
    }
    old_ps = data->ps;
    data->ps = val;

    local_bh_disable();
    if (old_ps == PS_DISABLED && val != PS_DISABLED) {
        ieee80211_iterate_active_interfaces_atomic(
            data->hw, IEEE80211_IFACE_ITER_NORMAL,
            hwsim_send_nullfunc_ps, data);
    } else if (old_ps != PS_DISABLED && val == PS_DISABLED) {
        ieee80211_iterate_active_interfaces_atomic(
            data->hw, IEEE80211_IFACE_ITER_NORMAL,
            hwsim_send_nullfunc_no_ps, data);
    }
    local_bh_enable();

    return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(hwsim_fops_ps, hwsim_fops_ps_read, hwsim_fops_ps_write,
             "%llu\n");

static int hwsim_write_simulate_radar(void *dat, u64 val)
{
    struct mac80211_rdkfmac_data *data = dat;

    ieee80211_radar_detected(data->hw);

    return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(hwsim_simulate_radar, NULL,
             hwsim_write_simulate_radar, "%llu\n");

static int hwsim_fops_group_read(void *dat, u64 *val)
{
    struct mac80211_rdkfmac_data *data = dat;
    *val = data->group;
    return 0;
}

static int hwsim_fops_group_write(void *dat, u64 val)
{
    struct mac80211_rdkfmac_data *data = dat;
    data->group = val;
    return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(hwsim_fops_group,
             hwsim_fops_group_read, hwsim_fops_group_write,
             "%llx\n");

static int hwsim_fops_rx_rssi_read(void *dat, u64 *val)
{
    struct mac80211_rdkfmac_data *data = dat;
    *val = data->rx_rssi;
    return 0;
}

static int hwsim_fops_rx_rssi_write(void *dat, u64 val)
{
    struct mac80211_rdkfmac_data *data = dat;
    int rssi = (int)val;

    if (rssi >= 0 || rssi < -100)
        return -EINVAL;

    data->rx_rssi = rssi;
    return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(hwsim_fops_rx_rssi,
             hwsim_fops_rx_rssi_read, hwsim_fops_rx_rssi_write,
             "%lld\n");

static netdev_tx_t hwsim_mon_xmit(struct sk_buff *skb,
                    struct net_device *dev)
{
    printk("mon_xmit\n");
    /* TODO: allow packet injection */
    dev_kfree_skb(skb);
    return NETDEV_TX_OK;
}

static inline u64 mac80211_hwsim_get_tsf_raw(void)
{
    return ktime_to_us(ktime_get_real());
}

static __le64 __mac80211_hwsim_get_tsf(struct mac80211_rdkfmac_data *data)
{
    u64 now = mac80211_hwsim_get_tsf_raw();
    return cpu_to_le64(now + data->tsf_offset);
}

static u64 mac80211_hwsim_get_tsf(struct ieee80211_hw *hw,
                  struct ieee80211_vif *vif)
{
    struct mac80211_rdkfmac_data *data = hw->priv;
    return le64_to_cpu(__mac80211_hwsim_get_tsf(data));
}

static void mac80211_hwsim_set_tsf(struct ieee80211_hw *hw,
        struct ieee80211_vif *vif, u64 tsf)
{
    struct mac80211_rdkfmac_data *data = hw->priv;
    u64 now = mac80211_hwsim_get_tsf(hw, vif);
    u32 bcn_int = data->beacon_int;
    u64 delta = abs(tsf - now);

    /* adjust after beaconing with new timestamp at old TBTT */
    if (tsf > now) {
        data->tsf_offset += delta;
        data->bcn_delta = do_div(delta, bcn_int);
    } else {
        data->tsf_offset -= delta;
        data->bcn_delta = -(s64)do_div(delta, bcn_int);
    }
}

static void mac80211_hwsim_monitor_rx(struct ieee80211_hw *hw,
                      struct sk_buff *tx_skb,
                      struct ieee80211_channel *chan)
{
    struct mac80211_rdkfmac_data *data = hw->priv;
    struct sk_buff *skb;
    struct hwsim_radiotap_hdr *hdr;
    u16 flags, bitrate;
    struct ieee80211_tx_info *info = IEEE80211_SKB_CB(tx_skb);
    struct ieee80211_rate *txrate = ieee80211_get_tx_rate(hw, info);

    if (!txrate)
        bitrate = 0;
    else
        bitrate = txrate->bitrate;

    if (!netif_running(hwsim_mon))
        return;

    skb = skb_copy_expand(tx_skb, sizeof(*hdr), 0, GFP_ATOMIC);
    if (skb == NULL)
        return;

    hdr = skb_push(skb, sizeof(*hdr));
    hdr->hdr.it_version = PKTHDR_RADIOTAP_VERSION;
    hdr->hdr.it_pad = 0;
    hdr->hdr.it_len = cpu_to_le16(sizeof(*hdr));
    hdr->hdr.it_present = cpu_to_le32((1 << IEEE80211_RADIOTAP_FLAGS) |
                      (1 << IEEE80211_RADIOTAP_RATE) |
                      (1 << IEEE80211_RADIOTAP_TSFT) |
                      (1 << IEEE80211_RADIOTAP_CHANNEL));
    hdr->rt_tsft = __mac80211_hwsim_get_tsf(data);
    hdr->rt_flags = 0;
    hdr->rt_rate = bitrate / 5;
    hdr->rt_channel = cpu_to_le16(chan->center_freq);
    flags = IEEE80211_CHAN_2GHZ;
    if (txrate && txrate->flags & IEEE80211_RATE_ERP_G)
        flags |= IEEE80211_CHAN_OFDM;
    else
        flags |= IEEE80211_CHAN_CCK;
    hdr->rt_chbitmask = cpu_to_le16(flags);

    skb->dev = hwsim_mon;
    skb_reset_mac_header(skb);
    skb->ip_summed = CHECKSUM_UNNECESSARY;
    skb->pkt_type = PACKET_OTHERHOST;
    skb->protocol = htons(ETH_P_802_2);
    memset(skb->cb, 0, sizeof(skb->cb));
    netif_rx(skb);
}


static void mac80211_hwsim_monitor_ack(struct ieee80211_channel *chan,
                       const u8 *addr)
{
    struct sk_buff *skb;
    struct hwsim_radiotap_ack_hdr *hdr;
    u16 flags;
    struct ieee80211_hdr *hdr11;

    if (!netif_running(hwsim_mon))
        return;

    skb = dev_alloc_skb(100);
    if (skb == NULL)
        return;

    hdr = skb_put(skb, sizeof(*hdr));
    hdr->hdr.it_version = PKTHDR_RADIOTAP_VERSION;
    hdr->hdr.it_pad = 0;
    hdr->hdr.it_len = cpu_to_le16(sizeof(*hdr));
    hdr->hdr.it_present = cpu_to_le32((1 << IEEE80211_RADIOTAP_FLAGS) |
                      (1 << IEEE80211_RADIOTAP_CHANNEL));
    hdr->rt_flags = 0;
    hdr->pad = 0;
    hdr->rt_channel = cpu_to_le16(chan->center_freq);
    flags = IEEE80211_CHAN_2GHZ;
    hdr->rt_chbitmask = cpu_to_le16(flags);

    hdr11 = skb_put(skb, 10);
    hdr11->frame_control = cpu_to_le16(IEEE80211_FTYPE_CTL |
                       IEEE80211_STYPE_ACK);
    hdr11->duration_id = cpu_to_le16(0);
    memcpy(hdr11->addr1, addr, ETH_ALEN);

    skb->dev = hwsim_mon;
    skb_reset_mac_header(skb);
    skb->ip_summed = CHECKSUM_UNNECESSARY;
    skb->pkt_type = PACKET_OTHERHOST;
    skb->protocol = htons(ETH_P_802_2);
    memset(skb->cb, 0, sizeof(skb->cb));
    netif_rx(skb);
}

struct mac80211_hwsim_addr_match_data {
    u8 addr[ETH_ALEN];
    bool ret;
};

static void mac80211_hwsim_addr_iter(void *data, u8 *mac,
                     struct ieee80211_vif *vif)
{
    struct mac80211_hwsim_addr_match_data *md = data;

    if (memcmp(mac, md->addr, ETH_ALEN) == 0)
        md->ret = true;
}

static bool mac80211_hwsim_addr_match(struct mac80211_rdkfmac_data *data,
                      const u8 *addr)
{
    struct mac80211_hwsim_addr_match_data md = {
        .ret = false,
    };

    if (data->scanning && memcmp(addr, data->scan_addr, ETH_ALEN) == 0)
        return true;

    memcpy(md.addr, addr, ETH_ALEN);

    ieee80211_iterate_active_interfaces_atomic(data->hw,
                           IEEE80211_IFACE_ITER_NORMAL,
                           mac80211_hwsim_addr_iter,
                           &md);

    return md.ret;
}

static bool hwsim_ps_rx_ok(struct mac80211_rdkfmac_data *data,
               struct sk_buff *skb)
{
    switch (data->ps) {
    case PS_DISABLED:
        return true;
    case PS_ENABLED:
        return false;
    case PS_AUTO_POLL:
        /* TODO: accept (some) Beacons by default and other frames only
         * if pending PS-Poll has been sent */
        return true;
    case PS_MANUAL_POLL:
        /* Allow unicast frames to own address if there is a pending
         * PS-Poll */
        if (data->ps_poll_pending &&
            mac80211_hwsim_addr_match(data, skb->data + 4)) {
            data->ps_poll_pending = false;
            return true;
        }
        return false;
    }

    return true;
}

static int hwsim_unicast_netgroup(struct mac80211_rdkfmac_data *data,
                  struct sk_buff *skb, int portid)
{
    struct net *net;
    bool found = false;
    int res = -ENOENT;

    rcu_read_lock();
    for_each_net_rcu(net) {
        if (data->netgroup == hwsim_net_get_netgroup(net)) {
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

    msg_head = genlmsg_put(skb, 0, 0, &hwsim_genl_family, 0,
                   add ? HWSIM_CMD_ADD_MAC_ADDR :
                     HWSIM_CMD_DEL_MAC_ADDR);
    if (!msg_head) {
        pr_debug("mac80211_hwsim: problem with msg_head\n");
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

static inline u16 trans_tx_rate_flags_ieee2hwsim(struct ieee80211_tx_rate *rate)
{
    u16 result = 0;

    if (rate->flags & IEEE80211_TX_RC_USE_RTS_CTS)
        result |= MAC80211_HWSIM_TX_RC_USE_RTS_CTS;
    if (rate->flags & IEEE80211_TX_RC_USE_CTS_PROTECT)
        result |= MAC80211_HWSIM_TX_RC_USE_CTS_PROTECT;
    if (rate->flags & IEEE80211_TX_RC_USE_SHORT_PREAMBLE)
        result |= MAC80211_HWSIM_TX_RC_USE_SHORT_PREAMBLE;
    if (rate->flags & IEEE80211_TX_RC_MCS)
        result |= MAC80211_HWSIM_TX_RC_MCS;
    if (rate->flags & IEEE80211_TX_RC_GREEN_FIELD)
        result |= MAC80211_HWSIM_TX_RC_GREEN_FIELD;
    if (rate->flags & IEEE80211_TX_RC_40_MHZ_WIDTH)
        result |= MAC80211_HWSIM_TX_RC_40_MHZ_WIDTH;
    if (rate->flags & IEEE80211_TX_RC_DUP_DATA)
        result |= MAC80211_HWSIM_TX_RC_DUP_DATA;
    if (rate->flags & IEEE80211_TX_RC_SHORT_GI)
        result |= MAC80211_HWSIM_TX_RC_SHORT_GI;
    if (rate->flags & IEEE80211_TX_RC_VHT_MCS)
        result |= MAC80211_HWSIM_TX_RC_VHT_MCS;
    if (rate->flags & IEEE80211_TX_RC_80_MHZ_WIDTH)
        result |= MAC80211_HWSIM_TX_RC_80_MHZ_WIDTH;
    if (rate->flags & IEEE80211_TX_RC_160_MHZ_WIDTH)
        result |= MAC80211_HWSIM_TX_RC_160_MHZ_WIDTH;

    return result;
}

static void mac80211_hwsim_tx_frame_nl(struct ieee80211_hw *hw,
                       struct sk_buff *my_skb,
                       int dst_portid,
                       struct ieee80211_channel *channel)
{
    struct sk_buff *skb;
    struct mac80211_rdkfmac_data *data = hw->priv;
    struct ieee80211_hdr *hdr = (struct ieee80211_hdr *) my_skb->data;
    struct ieee80211_tx_info *info = IEEE80211_SKB_CB(my_skb);
    void *msg_head;
    unsigned int hwsim_flags = 0;
    int i;
    struct hwsim_tx_rate tx_attempts[IEEE80211_TX_MAX_RATES];
    struct hwsim_tx_rate_flag tx_attempts_flags[IEEE80211_TX_MAX_RATES];
    uintptr_t cookie;

    if (data->ps != PS_DISABLED)
        hdr->frame_control |= cpu_to_le16(IEEE80211_FCTL_PM);
    /* If the queue contains MAX_QUEUE skb's drop some */
    if (skb_queue_len(&data->pending) >= MAX_QUEUE) {
        /* Dropping until WARN_QUEUE level */
        while (skb_queue_len(&data->pending) >= WARN_QUEUE) {
            ieee80211_free_txskb(hw, skb_dequeue(&data->pending));
            data->tx_dropped++;
        }
    }

    skb = genlmsg_new(GENLMSG_DEFAULT_SIZE, GFP_ATOMIC);
    if (skb == NULL)
        goto nla_put_failure;

    msg_head = genlmsg_put(skb, 0, 0, &hwsim_genl_family, 0,
                   HWSIM_CMD_FRAME);
    if (msg_head == NULL) {
        pr_debug("mac80211_hwsim: problem with msg_head\n");
        goto nla_put_failure;
    }

    if (nla_put(skb, HWSIM_ATTR_ADDR_TRANSMITTER,
            ETH_ALEN, data->addresses[1].addr))
        goto nla_put_failure;

    /* We get the skb->data */
    if (nla_put(skb, HWSIM_ATTR_FRAME, my_skb->len, my_skb->data))
        goto nla_put_failure;

    /* We get the flags for this transmission, and we translate them to
       wmediumd flags  */

    if (info->flags & IEEE80211_TX_CTL_REQ_TX_STATUS)
        hwsim_flags |= HWSIM_TX_CTL_REQ_TX_STATUS;

    if (info->flags & IEEE80211_TX_CTL_NO_ACK)
        hwsim_flags |= HWSIM_TX_CTL_NO_ACK;

    if (nla_put_u32(skb, HWSIM_ATTR_FLAGS, hwsim_flags))
        goto nla_put_failure;

    if (nla_put_u32(skb, HWSIM_ATTR_FREQ, channel->center_freq))
        goto nla_put_failure;

    /* We get the tx control (rate and retries) info*/

    for (i = 0; i < IEEE80211_TX_MAX_RATES; i++) {
        tx_attempts[i].idx = info->status.rates[i].idx;
        tx_attempts_flags[i].idx = info->status.rates[i].idx;
        tx_attempts[i].count = info->status.rates[i].count;
        tx_attempts_flags[i].flags =
                trans_tx_rate_flags_ieee2hwsim(
                        &info->status.rates[i]);
    }

    if (nla_put(skb, HWSIM_ATTR_TX_INFO,
            sizeof(struct hwsim_tx_rate)*IEEE80211_TX_MAX_RATES,
            tx_attempts))
        goto nla_put_failure;

    if (nla_put(skb, HWSIM_ATTR_TX_INFO_FLAGS,
            sizeof(struct hwsim_tx_rate_flag) * IEEE80211_TX_MAX_RATES,
            tx_attempts_flags))
        goto nla_put_failure;

    /* We create a cookie to identify this skb */
    cookie = atomic_inc_return(&data->pending_cookie);
    info->rate_driver_data[0] = (void *)cookie;
    if (nla_put_u64_64bit(skb, HWSIM_ATTR_COOKIE, cookie, HWSIM_ATTR_PAD))
        goto nla_put_failure;

    genlmsg_end(skb, msg_head);

    if (hwsim_unicast_netgroup(data, skb, dst_portid))
        goto err_free_txskb;

    /* Enqueue the packet */
    skb_queue_tail(&data->pending, my_skb);
    data->tx_pkts++;
    data->tx_bytes += my_skb->len;
    return;

nla_put_failure:
    nlmsg_free(skb);
err_free_txskb:
    pr_debug("mac80211_hwsim: error occurred in %s\n", __func__);
    ieee80211_free_txskb(hw, my_skb);
    data->tx_failed++;
}

static bool hwsim_chans_compat(struct ieee80211_channel *c1,
                   struct ieee80211_channel *c2)
{
    if (!c1 || !c2)
        return false;

    return c1->center_freq == c2->center_freq;
}

struct tx_iter_data {
    struct ieee80211_channel *channel;
    bool receive;
};

static void mac80211_hwsim_tx_iter(void *_data, u8 *addr,
                   struct ieee80211_vif *vif)
{
    struct tx_iter_data *data = _data;

    if (!vif->bss_conf.chanctx_conf)
        return;

    if (!hwsim_chans_compat(data->channel,
                rcu_dereference(vif->bss_conf.chanctx_conf)->def.chan))
        return;

    data->receive = true;
}

static void mac80211_hwsim_add_vendor_rtap(struct sk_buff *skb)
{
    /*
     * To enable this code, #define the HWSIM_RADIOTAP_OUI,
     * e.g. like this:
     * #define HWSIM_RADIOTAP_OUI "\x02\x00\x00"
     * (but you should use a valid OUI, not that)
     *
     * If anyone wants to 'donate' a radiotap OUI/subns code
     * please send a patch removing this #ifdef and changing
     * the values accordingly.
     */
#ifdef HWSIM_RADIOTAP_OUI
    struct ieee80211_vendor_radiotap *rtap;

    /*
     * Note that this code requires the headroom in the SKB
     * that was allocated earlier.
     */
    rtap = skb_push(skb, sizeof(*rtap) + 8 + 4);
    rtap->oui[0] = HWSIM_RADIOTAP_OUI[0];
    rtap->oui[1] = HWSIM_RADIOTAP_OUI[1];
    rtap->oui[2] = HWSIM_RADIOTAP_OUI[2];
    rtap->subns = 127;

    /*
     * Radiotap vendor namespaces can (and should) also be
     * split into fields by using the standard radiotap
     * presence bitmap mechanism. Use just BIT(0) here for
     * the presence bitmap.
     */
    rtap->present = BIT(0);
    /* We have 8 bytes of (dummy) data */
    rtap->len = 8;
    /* For testing, also require it to be aligned */
    rtap->align = 8;
    /* And also test that padding works, 4 bytes */
    rtap->pad = 4;
    /* push the data */
    memcpy(rtap->data, "ABCDEFGH", 8);
    /* make sure to clear padding, mac80211 doesn't */
    memset(rtap->data + 8, 0, 4);

    IEEE80211_SKB_RXCB(skb)->flag |= RX_FLAG_RADIOTAP_VENDOR_DATA;
#endif
}

static bool mac80211_hwsim_tx_frame_no_nl(struct ieee80211_hw *hw,
                      struct sk_buff *skb,
                      struct ieee80211_channel *chan)
{
    struct mac80211_rdkfmac_data *data = hw->priv, *data2;
    bool ack = false;
    struct ieee80211_hdr *hdr = (struct ieee80211_hdr *) skb->data;
    struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
    struct ieee80211_rx_status rx_status;
    u64 now;

    memset(&rx_status, 0, sizeof(rx_status));
    rx_status.flag |= RX_FLAG_MACTIME_START;
    rx_status.freq = chan->center_freq;
    rx_status.freq_offset = chan->freq_offset ? 1 : 0;
    rx_status.band = chan->band;
    if (info->control.rates[0].flags & IEEE80211_TX_RC_VHT_MCS) {
        rx_status.rate_idx =
            ieee80211_rate_get_vht_mcs(&info->control.rates[0]);
        rx_status.nss =
            ieee80211_rate_get_vht_nss(&info->control.rates[0]);
        rx_status.encoding = RX_ENC_VHT;
    } else {
        rx_status.rate_idx = info->control.rates[0].idx;
        if (info->control.rates[0].flags & IEEE80211_TX_RC_MCS)
            rx_status.encoding = RX_ENC_HT;
    }
    if (info->control.rates[0].flags & IEEE80211_TX_RC_40_MHZ_WIDTH)
        rx_status.bw = RATE_INFO_BW_40;
    else if (info->control.rates[0].flags & IEEE80211_TX_RC_80_MHZ_WIDTH)
        rx_status.bw = RATE_INFO_BW_80;
    else if (info->control.rates[0].flags & IEEE80211_TX_RC_160_MHZ_WIDTH)
        rx_status.bw = RATE_INFO_BW_160;
    else
        rx_status.bw = RATE_INFO_BW_20;
    if (info->control.rates[0].flags & IEEE80211_TX_RC_SHORT_GI)
        rx_status.enc_flags |= RX_ENC_FLAG_SHORT_GI;
    /* TODO: simulate optional packet loss */
    rx_status.signal = data->rx_rssi;
    if (info->control.vif)
        rx_status.signal += info->control.vif->bss_conf.txpower;

    if (data->ps != PS_DISABLED)
        hdr->frame_control |= cpu_to_le16(IEEE80211_FCTL_PM);

    /* release the skb's source info */
    skb_orphan(skb);
    skb_dst_drop(skb);
    skb->mark = 0;
    skb_ext_reset(skb);
    nf_reset_ct(skb);

    /*
     * Get absolute mactime here so all HWs RX at the "same time", and
     * absolute TX time for beacon mactime so the timestamp matches.
     * Giving beacons a different mactime than non-beacons looks messy, but
     * it helps the Toffset be exact and a ~10us mactime discrepancy
     * probably doesn't really matter.
     */
    if (ieee80211_is_beacon(hdr->frame_control) ||
        ieee80211_is_probe_resp(hdr->frame_control)) {
        rx_status.boottime_ns = ktime_get_boottime_ns();
        now = data->abs_bcn_ts;
    } else {
        now = mac80211_hwsim_get_tsf_raw();
    }

    spin_lock(&hwsim_radio_lock);
    list_for_each_entry(data2, &hwsim_radios, list) {
        struct sk_buff *nskb;
        struct tx_iter_data tx_iter_data = {
            .receive = false,
            .channel = chan,
        };

        if (data == data2)
            continue;

        if (!data2->started || (data2->idle && !data2->tmp_chan) ||
            !hwsim_ps_rx_ok(data2, skb))
            continue;

        if (!(data->group & data2->group))
            continue;

        if (data->netgroup != data2->netgroup)
            continue;

        if (!hwsim_chans_compat(chan, data2->tmp_chan) &&
            !hwsim_chans_compat(chan, data2->channel)) {
            ieee80211_iterate_active_interfaces_atomic(
                data2->hw, IEEE80211_IFACE_ITER_NORMAL,
                mac80211_hwsim_tx_iter, &tx_iter_data);
            if (!tx_iter_data.receive)
                continue;
        }

        /*
         * reserve some space for our vendor and the normal
         * radiotap header, since we're copying anyway
         */
        if (skb->len < PAGE_SIZE) {
            struct page *page = alloc_page(GFP_ATOMIC);

            if (!page)
                continue;

            nskb = dev_alloc_skb(128);
            if (!nskb) {
                __free_page(page);
                continue;
            }

            memcpy(page_address(page), skb->data, skb->len);
            skb_add_rx_frag(nskb, 0, page, 0, skb->len, skb->len);
        } else {
            nskb = skb_copy(skb, GFP_ATOMIC);
            if (!nskb)
                continue;
        }

        if (mac80211_hwsim_addr_match(data2, hdr->addr1))
            ack = true;

        rx_status.mactime = now + data2->tsf_offset;

        memcpy(IEEE80211_SKB_RXCB(nskb), &rx_status, sizeof(rx_status));

        mac80211_hwsim_add_vendor_rtap(nskb);

        data2->rx_pkts++;
        data2->rx_bytes += nskb->len;
        ieee80211_rx_irqsafe(data2->hw, nskb);
    }
    spin_unlock(&hwsim_radio_lock);

    return ack;
}

static void mac80211_hwsim_tx(struct ieee80211_hw *hw,
                  struct ieee80211_tx_control *control,
                  struct sk_buff *skb)
{
    struct mac80211_rdkfmac_data *data = hw->priv;
    struct ieee80211_tx_info *txi = IEEE80211_SKB_CB(skb);
    struct ieee80211_hdr *hdr = (void *)skb->data;
    struct ieee80211_chanctx_conf *chanctx_conf;
    struct ieee80211_channel *channel;
    bool ack;
    enum nl80211_chan_width confbw = NL80211_CHAN_WIDTH_20_NOHT;
    u32 _portid, i;

    if (WARN_ON(skb->len < 10)) {
        /* Should not happen; just a sanity check for addr1 use */
        ieee80211_free_txskb(hw, skb);
        return;
    }

    if (!data->use_chanctx) {
        channel = data->channel;
        confbw = data->bw;
    } else if (txi->hw_queue == 4) {
        channel = data->tmp_chan;
    } else {
        struct ieee80211_bss_conf *bss_conf;

        bss_conf = &txi->control.vif->bss_conf;

        chanctx_conf = rcu_dereference(bss_conf->chanctx_conf);
        if (chanctx_conf) {
            channel = chanctx_conf->def.chan;
            confbw = chanctx_conf->def.width;
        } else {
            channel = NULL;
        }
    }

    if (WARN(!channel, "TX w/o channel - queue = %d\n", txi->hw_queue)) {
        ieee80211_free_txskb(hw, skb);
        return;
    }

    if (data->idle && !data->tmp_chan) {
        wiphy_dbg(hw->wiphy, "Trying to TX when idle - reject\n");
        ieee80211_free_txskb(hw, skb);
        return;
    }

    if (txi->control.vif)
        hwsim_check_magic(txi->control.vif);
    if (control->sta)
        hwsim_check_sta_magic(control->sta);

    if (ieee80211_hw_check(hw, SUPPORTS_RC_TABLE))
        ieee80211_get_tx_rates(txi->control.vif, control->sta, skb,
                       txi->control.rates,
                       ARRAY_SIZE(txi->control.rates));

    for (i = 0; i < ARRAY_SIZE(txi->control.rates); i++) {
        u16 rflags = txi->control.rates[i].flags;
        /* initialize to data->bw for 5/10 MHz handling */
        enum nl80211_chan_width bw = data->bw;

        if (txi->control.rates[i].idx == -1)
            break;

        if (rflags & IEEE80211_TX_RC_40_MHZ_WIDTH)
            bw = NL80211_CHAN_WIDTH_40;
        else if (rflags & IEEE80211_TX_RC_80_MHZ_WIDTH)
            bw = NL80211_CHAN_WIDTH_80;
        else if (rflags & IEEE80211_TX_RC_160_MHZ_WIDTH)
            bw = NL80211_CHAN_WIDTH_160;

        if (WARN_ON(hwsim_get_chanwidth(bw) > hwsim_get_chanwidth(confbw)))
            return;
    }

    if (skb->len >= 24 + 8 &&
        ieee80211_is_probe_resp(hdr->frame_control)) {
        /* fake header transmission time */
        struct ieee80211_mgmt *mgmt;
        struct ieee80211_rate *txrate;
        /* TODO: get MCS */
        int bitrate = 100;
        u64 ts;

        mgmt = (struct ieee80211_mgmt *)skb->data;
        txrate = ieee80211_get_tx_rate(hw, txi);
        if (txrate)
            bitrate = txrate->bitrate;
        ts = mac80211_hwsim_get_tsf_raw();
        mgmt->u.probe_resp.timestamp =
            cpu_to_le64(ts + data->tsf_offset +
                    24 * 8 * 10 / bitrate);
    }

    mac80211_hwsim_monitor_rx(hw, skb, channel);

    /* wmediumd mode check */
    _portid = READ_ONCE(data->wmediumd);

    if (_portid)
        return mac80211_hwsim_tx_frame_nl(hw, skb, _portid, channel);

    /* NO wmediumd detected, perfect medium simulation */
    data->tx_pkts++;
    data->tx_bytes += skb->len;
    ack = mac80211_hwsim_tx_frame_no_nl(hw, skb, channel);

    if (ack && skb->len >= 16)
        mac80211_hwsim_monitor_ack(channel, hdr->addr2);

    ieee80211_tx_info_clear_status(txi);

    /* frame was transmitted at most favorable rate at first attempt */
    txi->control.rates[0].count = 1;
    txi->control.rates[1].idx = -1;

    if (!(txi->flags & IEEE80211_TX_CTL_NO_ACK) && ack)
        txi->flags |= IEEE80211_TX_STAT_ACK;
    ieee80211_tx_status_irqsafe(hw, skb);
}

void *prep_mac80211_spec(wlan_emu_msg_data_t *spec, wlan_emu_mac80211_ops_type_t ops)
{
    wlan_emu_msg_mac80211_t *mac80211;

    memset(spec, 0, sizeof(wlan_emu_msg_data_t));
    spec->type = wlan_emu_msg_type_mac80211;
    mac80211 = &spec->u.mac80211;
    mac80211->ops = ops;

    return (void *)(&mac80211->ops + sizeof(wlan_emu_cfg80211_ops_type_t));
}

static int mac80211_hwsim_start(struct ieee80211_hw *hw)
{
    struct mac80211_rdkfmac_data *data = hw->priv;
    wlan_emu_msg_data_t spec;
    wlan_emu_mac80211_start_t   *start;

    wiphy_dbg(hw->wiphy, "%s\n", __func__);
    data->started = true;

    start = (wlan_emu_mac80211_start_t *)prep_mac80211_spec(&spec, wlan_emu_mac80211_ops_type_start);
    push_to_char_device(&spec);

    return 0;
}


static void mac80211_hwsim_stop(struct ieee80211_hw *hw)
{
    struct mac80211_rdkfmac_data *data = hw->priv;
    wlan_emu_msg_data_t spec;
    wlan_emu_mac80211_stop_t    *stop;

    data->started = false;
    hrtimer_cancel(&data->beacon_timer);

    while (!skb_queue_empty(&data->pending))
        ieee80211_free_txskb(hw, skb_dequeue(&data->pending));

    wiphy_dbg(hw->wiphy, "%s\n", __func__);

    stop = (wlan_emu_mac80211_stop_t *)prep_mac80211_spec(&spec, wlan_emu_mac80211_ops_type_stop);
    push_to_char_device(&spec);

}


static int mac80211_hwsim_add_interface(struct ieee80211_hw *hw,
                    struct ieee80211_vif *vif)
{
    wlan_emu_msg_data_t spec;
    wlan_emu_mac80211_add_intf_t    *add_intf;

    wiphy_dbg(hw->wiphy, "%s (type=%d mac_addr=%pM)\n",
          __func__, ieee80211_vif_type_p2p(vif),
          vif->addr);
    hwsim_set_magic(vif);

    if (vif->type != NL80211_IFTYPE_MONITOR)
        mac80211_hwsim_config_mac_nl(hw, vif->addr, true);

    vif->cab_queue = 0;
    vif->hw_queue[IEEE80211_AC_VO] = 0;
    vif->hw_queue[IEEE80211_AC_VI] = 1;
    vif->hw_queue[IEEE80211_AC_BE] = 2;
    vif->hw_queue[IEEE80211_AC_BK] = 3;

    add_intf = (wlan_emu_mac80211_add_intf_t *)prep_mac80211_spec(&spec, wlan_emu_mac80211_ops_type_add_intf);
    push_to_char_device(&spec);

    return 0;
}


static int mac80211_hwsim_change_interface(struct ieee80211_hw *hw,
                       struct ieee80211_vif *vif,
                       enum nl80211_iftype newtype,
                       bool newp2p)
{
        wlan_emu_msg_data_t spec;
    wlan_emu_mac80211_change_intf_t *change_intf;
    newtype = ieee80211_iftype_p2p(newtype, newp2p);

    wiphy_dbg(hw->wiphy,
          "%s (old type=%d, new type=%d, mac_addr=%pM)\n",
          __func__, ieee80211_vif_type_p2p(vif),
            newtype, vif->addr);
    hwsim_check_magic(vif);

    /*
     * interface may change from non-AP to AP in
     * which case this needs to be set up again
     */
    vif->cab_queue = 0;

    change_intf = (wlan_emu_mac80211_change_intf_t *)prep_mac80211_spec(&spec, wlan_emu_mac80211_ops_type_change_intf);
    push_to_char_device(&spec);

    return 0;
}

static void mac80211_hwsim_remove_interface(
    struct ieee80211_hw *hw, struct ieee80211_vif *vif)
{
        wlan_emu_msg_data_t spec;
    wlan_emu_mac80211_remove_intf_t *remove_intf;

    wiphy_dbg(hw->wiphy, "%s (type=%d mac_addr=%pM)\n",
          __func__, ieee80211_vif_type_p2p(vif),
          vif->addr);
    hwsim_check_magic(vif);
    hwsim_clear_magic(vif);
    if (vif->type != NL80211_IFTYPE_MONITOR)
        mac80211_hwsim_config_mac_nl(hw, vif->addr, false);

    remove_intf = (wlan_emu_mac80211_remove_intf_t *)prep_mac80211_spec(&spec, wlan_emu_mac80211_ops_type_remove_intf);
    push_to_char_device(&spec);
}

static void mac80211_hwsim_tx_frame(struct ieee80211_hw *hw,
                    struct sk_buff *skb,
                    struct ieee80211_channel *chan)
{
    struct mac80211_rdkfmac_data *data = hw->priv;
    u32 _pid = READ_ONCE(data->wmediumd);

    if (ieee80211_hw_check(hw, SUPPORTS_RC_TABLE)) {
        struct ieee80211_tx_info *txi = IEEE80211_SKB_CB(skb);
        ieee80211_get_tx_rates(txi->control.vif, NULL, skb,
                       txi->control.rates,
                       ARRAY_SIZE(txi->control.rates));
    }

    mac80211_hwsim_monitor_rx(hw, skb, chan);

    if (_pid)
        return mac80211_hwsim_tx_frame_nl(hw, skb, _pid, chan);

    data->tx_pkts++;
    data->tx_bytes += skb->len;
    mac80211_hwsim_tx_frame_no_nl(hw, skb, chan);
    dev_kfree_skb(skb);
}

static void mac80211_hwsim_beacon_tx(void *arg, u8 *mac,
                     struct ieee80211_vif *vif)
{
    struct mac80211_rdkfmac_data *data = arg;
    struct ieee80211_hw *hw = data->hw;
    struct ieee80211_tx_info *info;
    struct ieee80211_rate *txrate;
    struct ieee80211_mgmt *mgmt;
    struct sk_buff *skb;
    /* TODO: get MCS */
    int bitrate = 100;

    hwsim_check_magic(vif);

    if (vif->type != NL80211_IFTYPE_AP &&
        vif->type != NL80211_IFTYPE_MESH_POINT &&
        vif->type != NL80211_IFTYPE_ADHOC &&
        vif->type != NL80211_IFTYPE_OCB)
        return;

    skb = ieee80211_beacon_get(hw, vif);
    if (skb == NULL)
        return;
    info = IEEE80211_SKB_CB(skb);
    if (ieee80211_hw_check(hw, SUPPORTS_RC_TABLE))
        ieee80211_get_tx_rates(vif, NULL, skb,
                       info->control.rates,
                       ARRAY_SIZE(info->control.rates));

    txrate = ieee80211_get_tx_rate(hw, info);
    if (txrate)
        bitrate = txrate->bitrate;

    mgmt = (struct ieee80211_mgmt *) skb->data;
    /* fake header transmission time */
    data->abs_bcn_ts = mac80211_hwsim_get_tsf_raw();
    if (ieee80211_is_s1g_beacon(mgmt->frame_control)) {
        struct ieee80211_ext *ext = (void *) mgmt;

        ext->u.s1g_beacon.timestamp = cpu_to_le32(data->abs_bcn_ts +
                              data->tsf_offset +
                              10 * 8 * 10 /
                              bitrate);
    } else {
        mgmt->u.beacon.timestamp = cpu_to_le64(data->abs_bcn_ts +
                               data->tsf_offset +
                               24 * 8 * 10 /
                               bitrate);
    }

    mac80211_hwsim_tx_frame(hw, skb,
                rcu_dereference(vif->bss_conf.chanctx_conf)->def.chan);

    while ((skb = ieee80211_get_buffered_bc(hw, vif)) != NULL) {
        mac80211_hwsim_tx_frame(hw, skb,
                rcu_dereference(vif->bss_conf.chanctx_conf)->def.chan);
    }

    if (vif->bss_conf.csa_active && ieee80211_beacon_cntdwn_is_complete(vif))
        ieee80211_csa_finish(vif);
}

static enum hrtimer_restart
mac80211_hwsim_beacon(struct hrtimer *timer)
{
    struct mac80211_rdkfmac_data *data =
        container_of(timer, struct mac80211_rdkfmac_data , beacon_timer);
    struct ieee80211_hw *hw = data->hw;
    u64 bcn_int = data->beacon_int;

    if (!data->started)
        return HRTIMER_NORESTART;

    ieee80211_iterate_active_interfaces_atomic(
        hw, IEEE80211_IFACE_ITER_NORMAL,
        mac80211_hwsim_beacon_tx, data);

    /* beacon at new TBTT + beacon interval */
    if (data->bcn_delta) {
        bcn_int -= data->bcn_delta;
        data->bcn_delta = 0;
    }
    hrtimer_forward_now(&data->beacon_timer,
                ns_to_ktime(bcn_int * NSEC_PER_USEC));
    return HRTIMER_RESTART;
}

static const char * const hwsim_chanwidths[] = {
    [NL80211_CHAN_WIDTH_5] = "ht5",
    [NL80211_CHAN_WIDTH_10] = "ht10",
    [NL80211_CHAN_WIDTH_20_NOHT] = "noht",
    [NL80211_CHAN_WIDTH_20] = "ht20",
    [NL80211_CHAN_WIDTH_40] = "ht40",
    [NL80211_CHAN_WIDTH_80] = "vht80",
    [NL80211_CHAN_WIDTH_80P80] = "vht80p80",
    [NL80211_CHAN_WIDTH_160] = "vht160",
    [NL80211_CHAN_WIDTH_1] = "1MHz",
    [NL80211_CHAN_WIDTH_2] = "2MHz",
    [NL80211_CHAN_WIDTH_4] = "4MHz",
    [NL80211_CHAN_WIDTH_8] = "8MHz",
    [NL80211_CHAN_WIDTH_16] = "16MHz",
};

static int mac80211_hwsim_config(struct ieee80211_hw *hw, u32 changed)
{
    wlan_emu_msg_data_t spec;
    wlan_emu_mac80211_config_t  *config;
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
        wiphy_dbg(hw->wiphy,
              "%s (freq=%d(%d - %d)/%s idle=%d ps=%d smps=%s)\n",
              __func__,
              conf->chandef.chan->center_freq,
              conf->chandef.center_freq1,
              conf->chandef.center_freq2,
              hwsim_chanwidths[conf->chandef.width],
              !!(conf->flags & IEEE80211_CONF_IDLE),
              !!(conf->flags & IEEE80211_CONF_PS),
              smps_modes[conf->smps_mode]);
    else
        wiphy_dbg(hw->wiphy,
              "%s (freq=0 idle=%d ps=%d smps=%s)\n",
              __func__,
              !!(conf->flags & IEEE80211_CONF_IDLE),
              !!(conf->flags & IEEE80211_CONF_PS),
              smps_modes[conf->smps_mode]);

    config = (wlan_emu_mac80211_config_t *)prep_mac80211_spec(&spec, wlan_emu_mac80211_ops_type_config);
    push_to_char_device(&spec);

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
        data->bw = conf->chandef.width;

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
        data->bw = conf->chandef.width;
    }
    mutex_unlock(&data->mutex);

    if (!data->started || !data->beacon_int)
        hrtimer_cancel(&data->beacon_timer);
    else if (!hrtimer_is_queued(&data->beacon_timer)) {
        u64 tsf = mac80211_hwsim_get_tsf(hw, NULL);
        u32 bcn_int = data->beacon_int;
        u64 until_tbtt = bcn_int - do_div(tsf, bcn_int);

        hrtimer_start(&data->beacon_timer,
                  ns_to_ktime(until_tbtt * NSEC_PER_USEC),
                  HRTIMER_MODE_REL_SOFT);
    }

    return 0;
}


static void mac80211_hwsim_configure_filter(struct ieee80211_hw *hw,
                        unsigned int changed_flags,
                        unsigned int *total_flags,u64 multicast)
{
    struct mac80211_rdkfmac_data *data = hw->priv;
    wlan_emu_msg_data_t spec;
    wlan_emu_mac80211_configure_filter_t    *configure_filter;

    wiphy_dbg(hw->wiphy, "%s\n", __func__);

    data->rx_filter = 0;
    if (*total_flags & FIF_ALLMULTI)
        data->rx_filter |= FIF_ALLMULTI;
    if (*total_flags & FIF_MCAST_ACTION)
        data->rx_filter |= FIF_MCAST_ACTION;

    *total_flags = data->rx_filter;

    configure_filter = (wlan_emu_mac80211_configure_filter_t *)prep_mac80211_spec(&spec, wlan_emu_mac80211_ops_type_configure_filter);
    push_to_char_device(&spec);
}

static void mac80211_hwsim_bcn_en_iter(void *data, u8 *mac,
                       struct ieee80211_vif *vif)
{
    unsigned int *count = data;
    struct hwsim_vif_priv *vp = (void *)vif->drv_priv;

    if (vp->bcn_en)
        (*count)++;
}

static void mac80211_hwsim_bss_info_changed(struct ieee80211_hw *hw,
                        struct ieee80211_vif *vif,
                        struct ieee80211_bss_conf *info,
                        u32 changed)
{
    struct hwsim_vif_priv *vp = (void *)vif->drv_priv;
    struct mac80211_rdkfmac_data *data = hw->priv;
    wlan_emu_msg_data_t spec;
    wlan_emu_mac80211_bss_info_changed_t    *bss_info_changed;

    hwsim_check_magic(vif);

    wiphy_dbg(hw->wiphy, "%s(changed=0x%x vif->addr=%pM)\n",
          __func__, changed, vif->addr);

    if (changed & BSS_CHANGED_BSSID) {
        wiphy_dbg(hw->wiphy, "%s: BSSID changed: %pM\n",
              __func__, info->bssid);
        memcpy(vp->bssid, info->bssid, ETH_ALEN);
    }

    if (changed & BSS_CHANGED_ASSOC) {
        wiphy_dbg(hw->wiphy, "  ASSOC: assoc=%d aid=%d\n",
              info->assoc, info->aid);
        vp->assoc = info->assoc;
        vp->aid = info->aid;
    }

    if (changed & BSS_CHANGED_BEACON_ENABLED) {
        wiphy_dbg(hw->wiphy, "  BCN EN: %d (BI=%u)\n",
              info->enable_beacon, info->beacon_int);
        vp->bcn_en = info->enable_beacon;
        if (data->started &&
            !hrtimer_is_queued(&data->beacon_timer) &&
            info->enable_beacon) {
            u64 tsf, until_tbtt;
            u32 bcn_int;
            data->beacon_int = info->beacon_int * 1024;
            tsf = mac80211_hwsim_get_tsf(hw, vif);
            bcn_int = data->beacon_int;
            until_tbtt = bcn_int - do_div(tsf, bcn_int);

            hrtimer_start(&data->beacon_timer,
                      ns_to_ktime(until_tbtt * NSEC_PER_USEC),
                      HRTIMER_MODE_REL_SOFT);
        } else if (!info->enable_beacon) {
            unsigned int count = 0;
            ieee80211_iterate_active_interfaces_atomic(
                data->hw, IEEE80211_IFACE_ITER_NORMAL,
                mac80211_hwsim_bcn_en_iter, &count);
            wiphy_dbg(hw->wiphy, "  beaconing vifs remaining: %u",
                  count);
            if (count == 0) {
                hrtimer_cancel(&data->beacon_timer);
                data->beacon_int = 0;
            }
        }
    }

    if (changed & BSS_CHANGED_ERP_CTS_PROT) {
        wiphy_dbg(hw->wiphy, "  ERP_CTS_PROT: %d\n",
              info->use_cts_prot);
    }

    if (changed & BSS_CHANGED_ERP_PREAMBLE) {
        wiphy_dbg(hw->wiphy, "  ERP_PREAMBLE: %d\n",
              info->use_short_preamble);
    }

    if (changed & BSS_CHANGED_ERP_SLOT) {
        wiphy_dbg(hw->wiphy, "  ERP_SLOT: %d\n", info->use_short_slot);
    }

    if (changed & BSS_CHANGED_HT) {
        wiphy_dbg(hw->wiphy, "  HT: op_mode=0x%x\n",
              info->ht_operation_mode);
    }

    if (changed & BSS_CHANGED_BASIC_RATES) {
        wiphy_dbg(hw->wiphy, "  BASIC_RATES: 0x%llx\n",
              (unsigned long long) info->basic_rates);
    }

    if (changed & BSS_CHANGED_TXPOWER)
        wiphy_dbg(hw->wiphy, "  TX Power: %d dBm\n", info->txpower);

    bss_info_changed = (wlan_emu_mac80211_bss_info_changed_t *)prep_mac80211_spec(&spec, wlan_emu_mac80211_ops_type_bss_info_changed);
    push_to_char_device(&spec);
}

static void
mac80211_hwsim_sta_rc_update(struct ieee80211_hw *hw,
                 struct ieee80211_vif *vif,
                 struct ieee80211_sta *sta,
                 u32 changed)
{
    struct mac80211_rdkfmac_data *data = hw->priv;
    u32 bw = U32_MAX;
    enum nl80211_chan_width confbw = NL80211_CHAN_WIDTH_20_NOHT;

    switch (sta->deflink.bandwidth) {
#define C(_bw) case IEEE80211_STA_RX_BW_##_bw: bw = _bw; break
    C(20);
    C(40);
    C(80);
    C(160);
    C(320);
#undef C
    }

    if (!data->use_chanctx) {
        confbw = data->bw;
    } else {
        struct ieee80211_chanctx_conf *chanctx_conf;

        rcu_read_lock();
        chanctx_conf = rcu_dereference(vif->bss_conf.chanctx_conf);

        if (!WARN_ON(!chanctx_conf))
            confbw = chanctx_conf->def.width;
        rcu_read_unlock();
    }

    WARN(bw > hwsim_get_chanwidth(confbw),
         "intf %pM: bad STA %pM bandwidth %d MHz (%d) > channel config %d MHz (%d)\n",
         vif->addr, sta->addr, bw, sta->deflink.bandwidth,
         hwsim_get_chanwidth(data->bw), data->bw);
}

static int mac80211_hwsim_sta_add(struct ieee80211_hw *hw,
                  struct ieee80211_vif *vif,
                  struct ieee80211_sta *sta)
{
    hwsim_check_magic(vif);
    hwsim_set_sta_magic(sta);
    mac80211_hwsim_sta_rc_update(hw, vif, sta, 0);

    return 0;
}

static int mac80211_hwsim_sta_remove(struct ieee80211_hw *hw,
                     struct ieee80211_vif *vif,
                     struct ieee80211_sta *sta)
{
    hwsim_check_magic(vif);
    hwsim_clear_sta_magic(sta);

    return 0;
}

static void mac80211_hwsim_sta_notify(struct ieee80211_hw *hw,
                      struct ieee80211_vif *vif,
                      enum sta_notify_cmd cmd,
                      struct ieee80211_sta *sta)
{
    hwsim_check_magic(vif);

    switch (cmd) {
    case STA_NOTIFY_SLEEP:
    case STA_NOTIFY_AWAKE:
        /* TODO: make good use of these flags */
        break;
    default:
        WARN(1, "Invalid sta notify: %d\n", cmd);
        break;
    }
}

static int mac80211_hwsim_set_tim(struct ieee80211_hw *hw,
                  struct ieee80211_sta *sta,
                  bool set)
{
    hwsim_check_sta_magic(sta);
    return 0;
}

static int mac80211_hwsim_conf_tx(
    struct ieee80211_hw *hw,
    struct ieee80211_vif *vif, u16 queue,
    const struct ieee80211_tx_queue_params *params)
{
    wiphy_dbg(hw->wiphy,
          "%s (queue=%d txop=%d cw_min=%d cw_max=%d aifs=%d)\n",
          __func__, queue,
          params->txop, params->cw_min,
          params->cw_max, params->aifs);
    return 0;
}

static int mac80211_hwsim_get_survey(struct ieee80211_hw *hw, int idx,
                     struct survey_info *survey)
{
    struct mac80211_rdkfmac_data *hwsim = hw->priv;

    if (idx < 0 || idx >= ARRAY_SIZE(hwsim->survey_data))
        return -ENOENT;

    mutex_lock(&hwsim->mutex);
    survey->channel = hwsim->survey_data[idx].channel;
    if (!survey->channel) {
        mutex_unlock(&hwsim->mutex);
        return -ENOENT;
    }

    /*
     * Magically conjured dummy values --- this is only ok for simulated hardware.
     *
     * A real driver which cannot determine real values noise MUST NOT
     * report any, especially not a magically conjured ones :-)
     */
    survey->filled = SURVEY_INFO_NOISE_DBM |
             SURVEY_INFO_TIME |
             SURVEY_INFO_TIME_BUSY;
    survey->noise = -92;
    survey->time =
        jiffies_to_msecs(hwsim->survey_data[idx].end -
                 hwsim->survey_data[idx].start);
    /* report 12.5% of channel time is used */
    survey->time_busy = survey->time/8;
    mutex_unlock(&hwsim->mutex);

    return 0;
}

#ifdef CONFIG_NL80211_TESTMODE
/*
 * This section contains example code for using netlink
 * attributes with the testmode command in nl80211.
 */

/* These enums need to be kept in sync with userspace */
enum hwsim_testmode_attr {
    __HWSIM_TM_ATTR_INVALID = 0,
    HWSIM_TM_ATTR_CMD   = 1,
    HWSIM_TM_ATTR_PS    = 2,

    /* keep last */
    __HWSIM_TM_ATTR_AFTER_LAST,
    HWSIM_TM_ATTR_MAX   = __HWSIM_TM_ATTR_AFTER_LAST - 1
};

enum hwsim_testmode_cmd {
    HWSIM_TM_CMD_SET_PS     = 0,
    HWSIM_TM_CMD_GET_PS     = 1,
    HWSIM_TM_CMD_STOP_QUEUES    = 2,
    HWSIM_TM_CMD_WAKE_QUEUES    = 3,
};

static const struct nla_policy hwsim_testmode_policy[HWSIM_TM_ATTR_MAX + 1] = {
    [HWSIM_TM_ATTR_CMD] = { .type = NLA_U32 },
    [HWSIM_TM_ATTR_PS] = { .type = NLA_U32 },
};

static int mac80211_hwsim_testmode_cmd(struct ieee80211_hw *hw,
                       struct ieee80211_vif *vif,
                       void *data, int len)
{
    struct mac80211_rdkfmac_data *hwsim = hw->priv;
    struct nlattr *tb[HWSIM_TM_ATTR_MAX + 1];
    struct sk_buff *skb;
    int err, ps;

    err = nla_parse_deprecated(tb, HWSIM_TM_ATTR_MAX, data, len,
                   hwsim_testmode_policy, NULL);
    if (err)
        return err;

    if (!tb[HWSIM_TM_ATTR_CMD])
        return -EINVAL;

    switch (nla_get_u32(tb[HWSIM_TM_ATTR_CMD])) {
    case HWSIM_TM_CMD_SET_PS:
        if (!tb[HWSIM_TM_ATTR_PS])
            return -EINVAL;
        ps = nla_get_u32(tb[HWSIM_TM_ATTR_PS]);
        return hwsim_fops_ps_write(hwsim, ps);
    case HWSIM_TM_CMD_GET_PS:
        skb = cfg80211_testmode_alloc_reply_skb(hw->wiphy,
                        nla_total_size(sizeof(u32)));
        if (!skb)
            return -ENOMEM;
        if (nla_put_u32(skb, HWSIM_TM_ATTR_PS, hwsim->ps))
            goto nla_put_failure;
        return cfg80211_testmode_reply(skb);
    case HWSIM_TM_CMD_STOP_QUEUES:
        ieee80211_stop_queues(hw);
        return 0;
    case HWSIM_TM_CMD_WAKE_QUEUES:
        ieee80211_wake_queues(hw);
        return 0;
    default:
        return -EOPNOTSUPP;
    }

 nla_put_failure:
    kfree_skb(skb);
    return -ENOBUFS;
}
#endif

static int mac80211_hwsim_ampdu_action(struct ieee80211_hw *hw,
                       struct ieee80211_vif *vif,
                       struct ieee80211_ampdu_params *params)
{
    struct ieee80211_sta *sta = params->sta;
    enum ieee80211_ampdu_mlme_action action = params->action;
    u16 tid = params->tid;

    switch (action) {
    case IEEE80211_AMPDU_TX_START:
        return IEEE80211_AMPDU_TX_START_IMMEDIATE;
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

static void mac80211_hwsim_flush(struct ieee80211_hw *hw,
                 struct ieee80211_vif *vif,
                 u32 queues, bool drop)
{
    /* Not implemented, queues only on kernel side */
}

static void hw_scan_work(struct work_struct *work)
{
    struct mac80211_rdkfmac_data *hwsim =
        container_of(work, struct mac80211_rdkfmac_data , hw_scan.work);
    struct cfg80211_scan_request *req = hwsim->hw_scan_request;
    int dwell, i;

    mutex_lock(&hwsim->mutex);
    if (hwsim->scan_chan_idx >= req->n_channels) {
        struct cfg80211_scan_info info = {
            .aborted = false,
        };

        wiphy_dbg(hwsim->hw->wiphy, "hw scan complete\n");
        ieee80211_scan_completed(hwsim->hw, &info);
        hwsim->hw_scan_request = NULL;
        hwsim->hw_scan_vif = NULL;
        hwsim->tmp_chan = NULL;
        mutex_unlock(&hwsim->mutex);
        mac80211_hwsim_config_mac_nl(hwsim->hw, hwsim->scan_addr,
                         false);
        return;
    }

    wiphy_dbg(hwsim->hw->wiphy, "hw scan %d MHz\n",
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

static int mac80211_hwsim_hw_scan(struct ieee80211_hw *hw,
                  struct ieee80211_vif *vif,
                  struct ieee80211_scan_request *hw_req)
{
    struct mac80211_rdkfmac_data *hwsim = hw->priv;
    struct cfg80211_scan_request *req = &hw_req->req;

    mutex_lock(&hwsim->mutex);
    if (WARN_ON(hwsim->tmp_chan || hwsim->hw_scan_request)) {
        mutex_unlock(&hwsim->mutex);
        return -EBUSY;
    }
    hwsim->hw_scan_request = req;
    hwsim->hw_scan_vif = vif;
    hwsim->scan_chan_idx = 0;
    if (req->flags & NL80211_SCAN_FLAG_RANDOM_ADDR)
        get_random_mask_addr(hwsim->scan_addr,
                     hw_req->req.mac_addr,
                     hw_req->req.mac_addr_mask);
    else
        memcpy(hwsim->scan_addr, vif->addr, ETH_ALEN);
    memset(hwsim->survey_data, 0, sizeof(hwsim->survey_data));
    mutex_unlock(&hwsim->mutex);

    mac80211_hwsim_config_mac_nl(hw, hwsim->scan_addr, true);
    wiphy_dbg(hw->wiphy, "hwsim hw_scan request\n");

    ieee80211_queue_delayed_work(hwsim->hw, &hwsim->hw_scan, 0);

    return 0;
}

static void mac80211_hwsim_cancel_hw_scan(struct ieee80211_hw *hw,
                      struct ieee80211_vif *vif)
{
    struct mac80211_rdkfmac_data *hwsim = hw->priv;
    struct cfg80211_scan_info info = {
        .aborted = true,
    };

    wiphy_dbg(hw->wiphy, "hwsim cancel_hw_scan\n");

    cancel_delayed_work_sync(&hwsim->hw_scan);

    mutex_lock(&hwsim->mutex);
    ieee80211_scan_completed(hwsim->hw, &info);
    hwsim->tmp_chan = NULL;
    hwsim->hw_scan_request = NULL;
    hwsim->hw_scan_vif = NULL;
    mutex_unlock(&hwsim->mutex);
}

static void mac80211_hwsim_sw_scan(struct ieee80211_hw *hw,
                   struct ieee80211_vif *vif,
                   const u8 *mac_addr)
{
    struct mac80211_rdkfmac_data *hwsim = hw->priv;

    mutex_lock(&hwsim->mutex);

    if (hwsim->scanning) {
        pr_debug("two hwsim sw_scans detected!\n");
        goto out;
    }

    pr_debug("hwsim sw_scan request, prepping stuff\n");

    memcpy(hwsim->scan_addr, mac_addr, ETH_ALEN);
    mac80211_hwsim_config_mac_nl(hw, hwsim->scan_addr, true);
    hwsim->scanning = true;
    memset(hwsim->survey_data, 0, sizeof(hwsim->survey_data));

out:
    mutex_unlock(&hwsim->mutex);
}

static void mac80211_hwsim_sw_scan_complete(struct ieee80211_hw *hw,
                        struct ieee80211_vif *vif)
{
    struct mac80211_rdkfmac_data *hwsim = hw->priv;

    mutex_lock(&hwsim->mutex);

    pr_debug("hwsim sw_scan_complete\n");
    hwsim->scanning = false;
    mac80211_hwsim_config_mac_nl(hw, hwsim->scan_addr, false);
    eth_zero_addr(hwsim->scan_addr);

    mutex_unlock(&hwsim->mutex);
}

static void hw_roc_start(struct work_struct *work)
{
    struct mac80211_rdkfmac_data *hwsim =
        container_of(work, struct mac80211_rdkfmac_data , roc_start.work);

    mutex_lock(&hwsim->mutex);

    wiphy_dbg(hwsim->hw->wiphy, "hwsim ROC begins\n");
    hwsim->tmp_chan = hwsim->roc_chan;
    ieee80211_ready_on_channel(hwsim->hw);

    ieee80211_queue_delayed_work(hwsim->hw, &hwsim->roc_done,
                     msecs_to_jiffies(hwsim->roc_duration));

    mutex_unlock(&hwsim->mutex);
}

static void hw_roc_done(struct work_struct *work)
{
    struct mac80211_rdkfmac_data *hwsim =
        container_of(work, struct mac80211_rdkfmac_data , roc_done.work);

    mutex_lock(&hwsim->mutex);
    ieee80211_remain_on_channel_expired(hwsim->hw);
    hwsim->tmp_chan = NULL;
    mutex_unlock(&hwsim->mutex);

    wiphy_dbg(hwsim->hw->wiphy, "hwsim ROC expired\n");
}

static int mac80211_hwsim_roc(struct ieee80211_hw *hw,
                  struct ieee80211_vif *vif,
                  struct ieee80211_channel *chan,
                  int duration,
                  enum ieee80211_roc_type type)
{
    struct mac80211_rdkfmac_data *hwsim = hw->priv;

    mutex_lock(&hwsim->mutex);
    if (WARN_ON(hwsim->tmp_chan || hwsim->hw_scan_request)) {
        mutex_unlock(&hwsim->mutex);
        return -EBUSY;
    }

    hwsim->roc_chan = chan;
    hwsim->roc_duration = duration;
    mutex_unlock(&hwsim->mutex);

    wiphy_dbg(hw->wiphy, "hwsim ROC (%d MHz, %d ms)\n",
          chan->center_freq, duration);
    ieee80211_queue_delayed_work(hw, &hwsim->roc_start, HZ/50);

    return 0;
}

static int mac80211_hwsim_croc(struct ieee80211_hw *hw,
                   struct ieee80211_vif *vif)
{
    struct mac80211_rdkfmac_data *hwsim = hw->priv;

    cancel_delayed_work_sync(&hwsim->roc_start);
    cancel_delayed_work_sync(&hwsim->roc_done);

    mutex_lock(&hwsim->mutex);
    hwsim->tmp_chan = NULL;
    mutex_unlock(&hwsim->mutex);

    wiphy_dbg(hw->wiphy, "hwsim ROC canceled\n");

    return 0;
}

static int mac80211_hwsim_add_chanctx(struct ieee80211_hw *hw,
                      struct ieee80211_chanctx_conf *ctx)
{
    struct mac80211_rdkfmac_data *hwsim = hw->priv;

    mutex_lock(&hwsim->mutex);
    hwsim->chanctx = ctx;
    mutex_unlock(&hwsim->mutex);
    hwsim_set_chanctx_magic(ctx);
    wiphy_dbg(hw->wiphy,
          "add channel context control: %d MHz/width: %d/cfreqs:%d/%d MHz\n",
          ctx->def.chan->center_freq, ctx->def.width,
          ctx->def.center_freq1, ctx->def.center_freq2);
    return 0;
}

static void mac80211_hwsim_remove_chanctx(struct ieee80211_hw *hw,
                      struct ieee80211_chanctx_conf *ctx)
{
    struct mac80211_rdkfmac_data *hwsim = hw->priv;

    mutex_lock(&hwsim->mutex);
    hwsim->chanctx = NULL;
    mutex_unlock(&hwsim->mutex);
    wiphy_dbg(hw->wiphy,
          "remove channel context control: %d MHz/width: %d/cfreqs:%d/%d MHz\n",
          ctx->def.chan->center_freq, ctx->def.width,
          ctx->def.center_freq1, ctx->def.center_freq2);
    hwsim_check_chanctx_magic(ctx);
    hwsim_clear_chanctx_magic(ctx);
}

static void mac80211_hwsim_change_chanctx(struct ieee80211_hw *hw,
                      struct ieee80211_chanctx_conf *ctx,
                      u32 changed)
{
    struct mac80211_rdkfmac_data *hwsim = hw->priv;

    mutex_lock(&hwsim->mutex);
    hwsim->chanctx = ctx;
    mutex_unlock(&hwsim->mutex);
    hwsim_check_chanctx_magic(ctx);
    wiphy_dbg(hw->wiphy,
          "change channel context control: %d MHz/width: %d/cfreqs:%d/%d MHz\n",
          ctx->def.chan->center_freq, ctx->def.width,
          ctx->def.center_freq1, ctx->def.center_freq2);
}

static int mac80211_hwsim_assign_vif_chanctx(struct ieee80211_hw *hw,
                         struct ieee80211_vif *vif,
                         struct ieee80211_chanctx_conf *ctx)
{
    hwsim_check_magic(vif);
    hwsim_check_chanctx_magic(ctx);

    return 0;
}

static void mac80211_hwsim_unassign_vif_chanctx(struct ieee80211_hw *hw,
                        struct ieee80211_vif *vif,
                        struct ieee80211_chanctx_conf *ctx)
{
    hwsim_check_magic(vif);
    hwsim_check_chanctx_magic(ctx);
}

static const char mac80211_hwsim_gstrings_stats[][ETH_GSTRING_LEN] = {
    "tx_pkts_nic",
    "tx_bytes_nic",
    "rx_pkts_nic",
    "rx_bytes_nic",
    "d_tx_dropped",
    "d_tx_failed",
    "d_ps_mode",
    "d_group",
};

#define MAC80211_HWSIM_SSTATS_LEN ARRAY_SIZE(mac80211_hwsim_gstrings_stats)

static void mac80211_hwsim_get_et_strings(struct ieee80211_hw *hw,
                      struct ieee80211_vif *vif,
                      u32 sset, u8 *data)
{
    if (sset == ETH_SS_STATS)
        memcpy(data, *mac80211_hwsim_gstrings_stats,
               sizeof(mac80211_hwsim_gstrings_stats));
}

static int mac80211_hwsim_get_et_sset_count(struct ieee80211_hw *hw,
                        struct ieee80211_vif *vif, int sset)
{
    if (sset == ETH_SS_STATS)
        return MAC80211_HWSIM_SSTATS_LEN;
    return 0;
}

static void mac80211_hwsim_get_et_stats(struct ieee80211_hw *hw,
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

    WARN_ON(i != MAC80211_HWSIM_SSTATS_LEN);
}

static int mac80211_hwsim_tx_last_beacon(struct ieee80211_hw *hw)
{
    return 1;
}

#define HWSIM_COMMON_OPS                    \
    .tx = mac80211_hwsim_tx,                \
    .start = mac80211_hwsim_start,              \
    .stop = mac80211_hwsim_stop,                \
    .add_interface = mac80211_hwsim_add_interface,      \
    .change_interface = mac80211_hwsim_change_interface,    \
    .remove_interface = mac80211_hwsim_remove_interface,    \
    .config = mac80211_hwsim_config,            \
    .configure_filter = mac80211_hwsim_configure_filter,    \
    .bss_info_changed = mac80211_hwsim_bss_info_changed,    \
    .tx_last_beacon = mac80211_hwsim_tx_last_beacon,    \
    .sta_add = mac80211_hwsim_sta_add,          \
    .sta_remove = mac80211_hwsim_sta_remove,        \
    .sta_notify = mac80211_hwsim_sta_notify,        \
    .sta_rc_update = mac80211_hwsim_sta_rc_update,      \
    .set_tim = mac80211_hwsim_set_tim,          \
    .conf_tx = mac80211_hwsim_conf_tx,          \
    .get_survey = mac80211_hwsim_get_survey,        \
    CFG80211_TESTMODE_CMD(mac80211_hwsim_testmode_cmd)  \
    .ampdu_action = mac80211_hwsim_ampdu_action,        \
    .flush = mac80211_hwsim_flush,              \
    .get_tsf = mac80211_hwsim_get_tsf,          \
    .set_tsf = mac80211_hwsim_set_tsf,          \
    .get_et_sset_count = mac80211_hwsim_get_et_sset_count,  \
    .get_et_stats = mac80211_hwsim_get_et_stats,        \
    .get_et_strings = mac80211_hwsim_get_et_strings,

static const struct ieee80211_ops mac80211_hwsim_ops = {
    HWSIM_COMMON_OPS
    .sw_scan_start = mac80211_hwsim_sw_scan,
    .sw_scan_complete = mac80211_hwsim_sw_scan_complete,
};

static const struct ieee80211_ops mac80211_hwsim_mchan_ops = {
    HWSIM_COMMON_OPS
    .hw_scan = mac80211_hwsim_hw_scan,
    .cancel_hw_scan = mac80211_hwsim_cancel_hw_scan,
    .sw_scan_start = NULL,
    .sw_scan_complete = NULL,
    .remain_on_channel = mac80211_hwsim_roc,
    .cancel_remain_on_channel = mac80211_hwsim_croc,
    .add_chanctx = mac80211_hwsim_add_chanctx,
    .remove_chanctx = mac80211_hwsim_remove_chanctx,
    .change_chanctx = mac80211_hwsim_change_chanctx,
    .assign_vif_chanctx = mac80211_hwsim_assign_vif_chanctx,
    .unassign_vif_chanctx = mac80211_hwsim_unassign_vif_chanctx,
};

struct hwsim_new_radio_params {
    unsigned int channels;
    const char *reg_alpha2;
    const struct ieee80211_regdomain *regd;
    bool reg_strict;
    bool p2p_device;
    bool use_chanctx;
    bool destroy_on_close;
    const char *hwname;
    bool no_vif;
    const u8 *perm_addr;
    u32 iftypes;
    u32 *ciphers;
    u8 n_ciphers;
};

static void hwsim_mcast_config_msg(struct sk_buff *mcast_skb,
                   struct genl_info *info)
{
    if (info)
        genl_notify(&hwsim_genl_family, mcast_skb, info,
                HWSIM_MCGRP_CONFIG, GFP_KERNEL);
    else
        genlmsg_multicast(&hwsim_genl_family, mcast_skb, 0,
                  HWSIM_MCGRP_CONFIG, GFP_KERNEL);
}

static int append_radio_msg(struct sk_buff *skb, int id,
                struct hwsim_new_radio_params *param)
{
    int ret;

    ret = nla_put_u32(skb, HWSIM_ATTR_RADIO_ID, id);
    if (ret < 0)
        return ret;

    if (param->channels) {
        ret = nla_put_u32(skb, HWSIM_ATTR_CHANNELS, param->channels);
        if (ret < 0)
            return ret;
    }

    if (param->reg_alpha2) {
        ret = nla_put(skb, HWSIM_ATTR_REG_HINT_ALPHA2, 2,
                  param->reg_alpha2);
        if (ret < 0)
            return ret;
    }

    if (param->regd) {
        int i;

        for (i = 0; i < ARRAY_SIZE(hwsim_world_regdom_custom); i++) {
            if (hwsim_world_regdom_custom[i] != param->regd)
                continue;

            ret = nla_put_u32(skb, HWSIM_ATTR_REG_CUSTOM_REG, i);
            if (ret < 0)
                return ret;
            break;
        }
    }

    if (param->reg_strict) {
        ret = nla_put_flag(skb, HWSIM_ATTR_REG_STRICT_REG);
        if (ret < 0)
            return ret;
    }

    if (param->p2p_device) {
        ret = nla_put_flag(skb, HWSIM_ATTR_SUPPORT_P2P_DEVICE);
        if (ret < 0)
            return ret;
    }

    if (param->use_chanctx) {
        ret = nla_put_flag(skb, HWSIM_ATTR_USE_CHANCTX);
        if (ret < 0)
            return ret;
    }

    if (param->hwname) {
        ret = nla_put(skb, HWSIM_ATTR_RADIO_NAME,
                  strlen(param->hwname), param->hwname);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static void hwsim_mcast_new_radio(int id, struct genl_info *info,
                  struct hwsim_new_radio_params *param)
{
    struct sk_buff *mcast_skb;
    void *data;

    mcast_skb = genlmsg_new(GENLMSG_DEFAULT_SIZE, GFP_KERNEL);
    if (!mcast_skb)
        return;

    data = genlmsg_put(mcast_skb, 0, 0, &hwsim_genl_family, 0,
               HWSIM_CMD_NEW_RADIO);
    if (!data)
        goto out_err;

    if (append_radio_msg(mcast_skb, id, param) < 0)
        goto out_err;

    genlmsg_end(mcast_skb, data);

    hwsim_mcast_config_msg(mcast_skb, info);
    return;

out_err:
    nlmsg_free(mcast_skb);
}

static const struct ieee80211_sband_iftype_data sband_capa_2ghz[] = {
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

                /* Leave all the other PHY capability bytes
                 * unset, as DCM, beam forming, RU and PPE
                 * threshold information are not supported
                 */
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
        .eht_cap = {
            .has_eht = true,
            .eht_cap_elem = {
                .mac_cap_info[0] =
                    IEEE80211_EHT_MAC_CAP0_NSEP_PRIO_ACCESS |
                    IEEE80211_EHT_MAC_CAP0_OM_CONTROL |
                    IEEE80211_EHT_MAC_CAP0_TRIG_TXOP_SHARING_MODE1,
                .phy_cap_info[0] =
                    IEEE80211_EHT_PHY_CAP0_242_TONE_RU_GT20MHZ |
                    IEEE80211_EHT_PHY_CAP0_NDP_4_EHT_LFT_32_GI |
                    IEEE80211_EHT_PHY_CAP0_PARTIAL_BW_UL_MU_MIMO |
                    IEEE80211_EHT_PHY_CAP0_SU_BEAMFORMER |
                    IEEE80211_EHT_PHY_CAP0_SU_BEAMFORMEE,
                .phy_cap_info[3] =
                    IEEE80211_EHT_PHY_CAP3_NG_16_SU_FEEDBACK |
                    IEEE80211_EHT_PHY_CAP3_NG_16_MU_FEEDBACK |
                    IEEE80211_EHT_PHY_CAP3_CODEBOOK_4_2_SU_FDBK |
                    IEEE80211_EHT_PHY_CAP3_CODEBOOK_7_5_MU_FDBK |
                    IEEE80211_EHT_PHY_CAP3_TRIG_SU_BF_FDBK |
                    IEEE80211_EHT_PHY_CAP3_TRIG_MU_BF_PART_BW_FDBK |
                    IEEE80211_EHT_PHY_CAP3_TRIG_CQI_FDBK,
                .phy_cap_info[4] =
                    IEEE80211_EHT_PHY_CAP4_PART_BW_DL_MU_MIMO |
                    IEEE80211_EHT_PHY_CAP4_PSR_SR_SUPP |
                    IEEE80211_EHT_PHY_CAP4_POWER_BOOST_FACT_SUPP |
                    IEEE80211_EHT_PHY_CAP4_EHT_MU_PPDU_4_EHT_LTF_08_GI |
                    IEEE80211_EHT_PHY_CAP4_MAX_NC_MASK,
                .phy_cap_info[5] =
                    IEEE80211_EHT_PHY_CAP5_NON_TRIG_CQI_FEEDBACK |
                    IEEE80211_EHT_PHY_CAP5_TX_LESS_242_TONE_RU_SUPP |
                    IEEE80211_EHT_PHY_CAP5_RX_LESS_242_TONE_RU_SUPP |
                    IEEE80211_EHT_PHY_CAP5_PPE_THRESHOLD_PRESENT |
                    IEEE80211_EHT_PHY_CAP5_COMMON_NOMINAL_PKT_PAD_MASK |
                    IEEE80211_EHT_PHY_CAP5_MAX_NUM_SUPP_EHT_LTF_MASK,
                .phy_cap_info[6] =
                    IEEE80211_EHT_PHY_CAP6_MAX_NUM_SUPP_EHT_LTF_MASK |
                    IEEE80211_EHT_PHY_CAP6_MCS15_SUPP_MASK,
                .phy_cap_info[7] =
                    IEEE80211_EHT_PHY_CAP7_20MHZ_STA_RX_NDP_WIDER_BW,
            },

            /* For all MCS and bandwidth, set 8 NSS for both Tx and
             * Rx
             */
            .eht_mcs_nss_supp = {
                /*
                 * Since B0, B1, B2 and B3 are not set in
                 * the supported channel width set field in the
                 * HE PHY capabilities information field the
                 * device is a 20MHz only device on 2.4GHz band.
                 */
                .only_20mhz = {
                    .rx_tx_mcs7_max_nss = 0x88,
                    .rx_tx_mcs9_max_nss = 0x88,
                    .rx_tx_mcs11_max_nss = 0x88,
                    .rx_tx_mcs13_max_nss = 0x88,
                },
            },
            /* PPE threshold information is not supported */
        },
    },
#ifdef CONFIG_MAC80211_MESH
    {
        .types_mask = BIT(NL80211_IFTYPE_MESH_POINT),
        .he_cap = {
            .has_he = true,
            .he_cap_elem = {
                .mac_cap_info[0] =
                    IEEE80211_HE_MAC_CAP0_HTC_HE,
                .mac_cap_info[1] =
                    IEEE80211_HE_MAC_CAP1_MULTI_TID_AGG_RX_QOS_8,
                .mac_cap_info[2] =
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
                .phy_cap_info[2] = 0,

                /* Leave all the other PHY capability bytes
                 * unset, as DCM, beam forming, RU and PPE
                 * threshold information are not supported
                 */
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
#endif
};

static const struct ieee80211_sband_iftype_data sband_capa_5ghz[] = {
    {
        /* TODO: should we support other types, e.g., P2P?*/
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

                /* Leave all the other PHY capability bytes
                 * unset, as DCM, beam forming, RU and PPE
                 * threshold information are not supported
                 */
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
        .eht_cap = {
            .has_eht = true,
            .eht_cap_elem = {
                .mac_cap_info[0] =
                    IEEE80211_EHT_MAC_CAP0_NSEP_PRIO_ACCESS |
                    IEEE80211_EHT_MAC_CAP0_OM_CONTROL |
                    IEEE80211_EHT_MAC_CAP0_TRIG_TXOP_SHARING_MODE1,
                .phy_cap_info[0] =
                    IEEE80211_EHT_PHY_CAP0_242_TONE_RU_GT20MHZ |
                    IEEE80211_EHT_PHY_CAP0_NDP_4_EHT_LFT_32_GI |
                    IEEE80211_EHT_PHY_CAP0_PARTIAL_BW_UL_MU_MIMO |
                    IEEE80211_EHT_PHY_CAP0_SU_BEAMFORMER |
                    IEEE80211_EHT_PHY_CAP0_SU_BEAMFORMEE |
                    IEEE80211_EHT_PHY_CAP0_BEAMFORMEE_SS_80MHZ_MASK,
                .phy_cap_info[1] =
                    IEEE80211_EHT_PHY_CAP1_BEAMFORMEE_SS_80MHZ_MASK |
                    IEEE80211_EHT_PHY_CAP1_BEAMFORMEE_SS_160MHZ_MASK,
                .phy_cap_info[2] =
                    IEEE80211_EHT_PHY_CAP2_SOUNDING_DIM_80MHZ_MASK |
                    IEEE80211_EHT_PHY_CAP2_SOUNDING_DIM_160MHZ_MASK,
                .phy_cap_info[3] =
                    IEEE80211_EHT_PHY_CAP3_NG_16_SU_FEEDBACK |
                    IEEE80211_EHT_PHY_CAP3_NG_16_MU_FEEDBACK |
                    IEEE80211_EHT_PHY_CAP3_CODEBOOK_4_2_SU_FDBK |
                    IEEE80211_EHT_PHY_CAP3_CODEBOOK_7_5_MU_FDBK |
                    IEEE80211_EHT_PHY_CAP3_TRIG_SU_BF_FDBK |
                    IEEE80211_EHT_PHY_CAP3_TRIG_MU_BF_PART_BW_FDBK |
                    IEEE80211_EHT_PHY_CAP3_TRIG_CQI_FDBK,
                .phy_cap_info[4] =
                    IEEE80211_EHT_PHY_CAP4_PART_BW_DL_MU_MIMO |
                    IEEE80211_EHT_PHY_CAP4_PSR_SR_SUPP |
                    IEEE80211_EHT_PHY_CAP4_POWER_BOOST_FACT_SUPP |
                    IEEE80211_EHT_PHY_CAP4_EHT_MU_PPDU_4_EHT_LTF_08_GI |
                    IEEE80211_EHT_PHY_CAP4_MAX_NC_MASK,
                .phy_cap_info[5] =
                    IEEE80211_EHT_PHY_CAP5_NON_TRIG_CQI_FEEDBACK |
                    IEEE80211_EHT_PHY_CAP5_TX_LESS_242_TONE_RU_SUPP |
                    IEEE80211_EHT_PHY_CAP5_RX_LESS_242_TONE_RU_SUPP |
                    IEEE80211_EHT_PHY_CAP5_PPE_THRESHOLD_PRESENT |
                    IEEE80211_EHT_PHY_CAP5_COMMON_NOMINAL_PKT_PAD_MASK |
                    IEEE80211_EHT_PHY_CAP5_MAX_NUM_SUPP_EHT_LTF_MASK,
                .phy_cap_info[6] =
                    IEEE80211_EHT_PHY_CAP6_MAX_NUM_SUPP_EHT_LTF_MASK |
                    IEEE80211_EHT_PHY_CAP6_MCS15_SUPP_MASK,
                .phy_cap_info[7] =
                    IEEE80211_EHT_PHY_CAP7_20MHZ_STA_RX_NDP_WIDER_BW |
                    IEEE80211_EHT_PHY_CAP7_NON_OFDMA_UL_MU_MIMO_80MHZ |
                    IEEE80211_EHT_PHY_CAP7_NON_OFDMA_UL_MU_MIMO_160MHZ |
                    IEEE80211_EHT_PHY_CAP7_MU_BEAMFORMER_80MHZ |
                    IEEE80211_EHT_PHY_CAP7_MU_BEAMFORMER_160MHZ,
            },

            /* For all MCS and bandwidth, set 8 NSS for both Tx and
             * Rx
             */
            .eht_mcs_nss_supp = {
                /*
                 * As B1 and B2 are set in the supported
                 * channel width set field in the HE PHY
                 * capabilities information field include all
                 * the following MCS/NSS.
                 */
                .bw._80 = {
                    .rx_tx_mcs9_max_nss = 0x88,
                    .rx_tx_mcs11_max_nss = 0x88,
                    .rx_tx_mcs13_max_nss = 0x88,
                },
                .bw._160 = {
                    .rx_tx_mcs9_max_nss = 0x88,
                    .rx_tx_mcs11_max_nss = 0x88,
                    .rx_tx_mcs13_max_nss = 0x88,
                },
            },
            /* PPE threshold information is not supported */
        },
    },
#ifdef CONFIG_MAC80211_MESH
    {
        /* TODO: should we support other types, e.g., IBSS?*/
        .types_mask = BIT(NL80211_IFTYPE_MESH_POINT),
        .he_cap = {
            .has_he = true,
            .he_cap_elem = {
                .mac_cap_info[0] =
                    IEEE80211_HE_MAC_CAP0_HTC_HE,
                .mac_cap_info[1] =
                    IEEE80211_HE_MAC_CAP1_MULTI_TID_AGG_RX_QOS_8,
                .mac_cap_info[2] =
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
                .phy_cap_info[2] = 0,

                /* Leave all the other PHY capability bytes
                 * unset, as DCM, beam forming, RU and PPE
                 * threshold information are not supported
                 */
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
#endif
};

static const struct ieee80211_sband_iftype_data sband_capa_6ghz[] = {
    {
        /* TODO: should we support other types, e.g., P2P?*/
        .types_mask = BIT(NL80211_IFTYPE_STATION) |
                  BIT(NL80211_IFTYPE_AP),
        .he_6ghz_capa = {
            .capa = cpu_to_le16(IEEE80211_HE_6GHZ_CAP_MIN_MPDU_START |
                        IEEE80211_HE_6GHZ_CAP_MAX_AMPDU_LEN_EXP |
                        IEEE80211_HE_6GHZ_CAP_MAX_MPDU_LEN |
                        IEEE80211_HE_6GHZ_CAP_SM_PS |
                        IEEE80211_HE_6GHZ_CAP_RD_RESPONDER |
                        IEEE80211_HE_6GHZ_CAP_TX_ANTPAT_CONS |
                        IEEE80211_HE_6GHZ_CAP_RX_ANTPAT_CONS),
        },
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

                /* Leave all the other PHY capability bytes
                 * unset, as DCM, beam forming, RU and PPE
                 * threshold information are not supported
                 */
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
        .eht_cap = {
            .has_eht = true,
            .eht_cap_elem = {
                .mac_cap_info[0] =
                    IEEE80211_EHT_MAC_CAP0_NSEP_PRIO_ACCESS |
                    IEEE80211_EHT_MAC_CAP0_OM_CONTROL |
                    IEEE80211_EHT_MAC_CAP0_TRIG_TXOP_SHARING_MODE1,
                .phy_cap_info[0] =
                    IEEE80211_EHT_PHY_CAP0_320MHZ_IN_6GHZ |
                    IEEE80211_EHT_PHY_CAP0_242_TONE_RU_GT20MHZ |
                    IEEE80211_EHT_PHY_CAP0_NDP_4_EHT_LFT_32_GI |
                    IEEE80211_EHT_PHY_CAP0_PARTIAL_BW_UL_MU_MIMO |
                    IEEE80211_EHT_PHY_CAP0_SU_BEAMFORMER |
                    IEEE80211_EHT_PHY_CAP0_SU_BEAMFORMEE |
                    IEEE80211_EHT_PHY_CAP0_BEAMFORMEE_SS_80MHZ_MASK,
                .phy_cap_info[1] =
                    IEEE80211_EHT_PHY_CAP1_BEAMFORMEE_SS_80MHZ_MASK |
                    IEEE80211_EHT_PHY_CAP1_BEAMFORMEE_SS_160MHZ_MASK |
                    IEEE80211_EHT_PHY_CAP1_BEAMFORMEE_SS_320MHZ_MASK,
                .phy_cap_info[2] =
                    IEEE80211_EHT_PHY_CAP2_SOUNDING_DIM_80MHZ_MASK |
                    IEEE80211_EHT_PHY_CAP2_SOUNDING_DIM_160MHZ_MASK |
                    IEEE80211_EHT_PHY_CAP2_SOUNDING_DIM_320MHZ_MASK,
                .phy_cap_info[3] =
                    IEEE80211_EHT_PHY_CAP3_NG_16_SU_FEEDBACK |
                    IEEE80211_EHT_PHY_CAP3_NG_16_MU_FEEDBACK |
                    IEEE80211_EHT_PHY_CAP3_CODEBOOK_4_2_SU_FDBK |
                    IEEE80211_EHT_PHY_CAP3_CODEBOOK_7_5_MU_FDBK |
                    IEEE80211_EHT_PHY_CAP3_TRIG_SU_BF_FDBK |
                    IEEE80211_EHT_PHY_CAP3_TRIG_MU_BF_PART_BW_FDBK |
                    IEEE80211_EHT_PHY_CAP3_TRIG_CQI_FDBK,
                .phy_cap_info[4] =
                    IEEE80211_EHT_PHY_CAP4_PART_BW_DL_MU_MIMO |
                    IEEE80211_EHT_PHY_CAP4_PSR_SR_SUPP |
                    IEEE80211_EHT_PHY_CAP4_POWER_BOOST_FACT_SUPP |
                    IEEE80211_EHT_PHY_CAP4_EHT_MU_PPDU_4_EHT_LTF_08_GI |
                    IEEE80211_EHT_PHY_CAP4_MAX_NC_MASK,
                .phy_cap_info[5] =
                    IEEE80211_EHT_PHY_CAP5_NON_TRIG_CQI_FEEDBACK |
                    IEEE80211_EHT_PHY_CAP5_TX_LESS_242_TONE_RU_SUPP |
                    IEEE80211_EHT_PHY_CAP5_RX_LESS_242_TONE_RU_SUPP |
                    IEEE80211_EHT_PHY_CAP5_PPE_THRESHOLD_PRESENT |
                    IEEE80211_EHT_PHY_CAP5_COMMON_NOMINAL_PKT_PAD_MASK |
                    IEEE80211_EHT_PHY_CAP5_MAX_NUM_SUPP_EHT_LTF_MASK,
                .phy_cap_info[6] =
                    IEEE80211_EHT_PHY_CAP6_MAX_NUM_SUPP_EHT_LTF_MASK |
                    IEEE80211_EHT_PHY_CAP6_MCS15_SUPP_MASK |
                    IEEE80211_EHT_PHY_CAP6_EHT_DUP_6GHZ_SUPP,
                .phy_cap_info[7] =
                    IEEE80211_EHT_PHY_CAP7_20MHZ_STA_RX_NDP_WIDER_BW |
                    IEEE80211_EHT_PHY_CAP7_NON_OFDMA_UL_MU_MIMO_80MHZ |
                    IEEE80211_EHT_PHY_CAP7_NON_OFDMA_UL_MU_MIMO_160MHZ |
                    IEEE80211_EHT_PHY_CAP7_NON_OFDMA_UL_MU_MIMO_320MHZ |
                    IEEE80211_EHT_PHY_CAP7_MU_BEAMFORMER_80MHZ |
                    IEEE80211_EHT_PHY_CAP7_MU_BEAMFORMER_160MHZ |
                    IEEE80211_EHT_PHY_CAP7_MU_BEAMFORMER_320MHZ,
            },

            /* For all MCS and bandwidth, set 8 NSS for both Tx and
             * Rx
             */
            .eht_mcs_nss_supp = {
                /*
                 * As B1 and B2 are set in the supported
                 * channel width set field in the HE PHY
                 * capabilities information field and 320MHz in
                 * 6GHz is supported include all the following
                 * MCS/NSS.
                 */
                .bw._80 = {
                    .rx_tx_mcs9_max_nss = 0x88,
                    .rx_tx_mcs11_max_nss = 0x88,
                    .rx_tx_mcs13_max_nss = 0x88,
                },
                .bw._160 = {
                    .rx_tx_mcs9_max_nss = 0x88,
                    .rx_tx_mcs11_max_nss = 0x88,
                    .rx_tx_mcs13_max_nss = 0x88,
                },
                .bw._320 = {
                    .rx_tx_mcs9_max_nss = 0x88,
                    .rx_tx_mcs11_max_nss = 0x88,
                    .rx_tx_mcs13_max_nss = 0x88,
                },
            },
            /* PPE threshold information is not supported */
        },
    },
#ifdef CONFIG_MAC80211_MESH
    {
        /* TODO: should we support other types, e.g., IBSS?*/
        .types_mask = BIT(NL80211_IFTYPE_MESH_POINT),
        .he_6ghz_capa = {
            .capa = cpu_to_le16(IEEE80211_HE_6GHZ_CAP_MIN_MPDU_START |
                        IEEE80211_HE_6GHZ_CAP_MAX_AMPDU_LEN_EXP |
                        IEEE80211_HE_6GHZ_CAP_MAX_MPDU_LEN |
                        IEEE80211_HE_6GHZ_CAP_SM_PS |
                        IEEE80211_HE_6GHZ_CAP_RD_RESPONDER |
                        IEEE80211_HE_6GHZ_CAP_TX_ANTPAT_CONS |
                        IEEE80211_HE_6GHZ_CAP_RX_ANTPAT_CONS),
        },
        .he_cap = {
            .has_he = true,
            .he_cap_elem = {
                .mac_cap_info[0] =
                    IEEE80211_HE_MAC_CAP0_HTC_HE,
                .mac_cap_info[1] =
                    IEEE80211_HE_MAC_CAP1_MULTI_TID_AGG_RX_QOS_8,
                .mac_cap_info[2] =
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
                .phy_cap_info[2] = 0,

                /* Leave all the other PHY capability bytes
                 * unset, as DCM, beam forming, RU and PPE
                 * threshold information are not supported
                 */
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
#endif
};

static void mac80211_hwsim_sband_capab(struct ieee80211_supported_band *sband)
{
    u16 n_iftype_data;

    if (sband->band == NL80211_BAND_2GHZ) {
        n_iftype_data = ARRAY_SIZE(sband_capa_2ghz);
        sband->iftype_data =
            (struct ieee80211_sband_iftype_data *)sband_capa_2ghz;
    } else if (sband->band == NL80211_BAND_5GHZ) {
        n_iftype_data = ARRAY_SIZE(sband_capa_5ghz);
        sband->iftype_data =
            (struct ieee80211_sband_iftype_data *)sband_capa_5ghz;
    } else if (sband->band == NL80211_BAND_6GHZ) {
        n_iftype_data = ARRAY_SIZE(sband_capa_6ghz);
        sband->iftype_data =
            (struct ieee80211_sband_iftype_data *)sband_capa_6ghz;
    } else {
        return;
    }

    sband->n_iftype_data = n_iftype_data;
}

#ifdef CONFIG_MAC80211_MESH
#define HWSIM_MESH_BIT BIT(NL80211_IFTYPE_MESH_POINT)
#else
#define HWSIM_MESH_BIT 0
#endif

#define HWSIM_IFTYPE_SUPPORT_MASK \
    (BIT(NL80211_IFTYPE_STATION) | \
     BIT(NL80211_IFTYPE_AP) | \
     BIT(NL80211_IFTYPE_P2P_CLIENT) | \
     BIT(NL80211_IFTYPE_P2P_GO) | \
     BIT(NL80211_IFTYPE_ADHOC) | \
     BIT(NL80211_IFTYPE_MESH_POINT) | \
     BIT(NL80211_IFTYPE_OCB))

static int mac80211_hwsim_new_radio(struct genl_info *info,
                    struct hwsim_new_radio_params *param)
{
    int err;
    u8 addr[ETH_ALEN];
    struct mac80211_rdkfmac_data *data;
    struct ieee80211_hw *hw;
    enum nl80211_band band;
    const struct ieee80211_ops *ops = &mac80211_hwsim_ops;
    struct net *net;
    int idx, i;
    int n_limits = 0;

    spin_lock_bh(&hwsim_radio_lock);
    idx = hwsim_radio_idx++;
    spin_unlock_bh(&hwsim_radio_lock);

    if (param->use_chanctx)
        ops = &mac80211_hwsim_mchan_ops;

    hw = rdkfmac_alloc_hw(sizeof(*data), ops);
    if (!hw) {
        pr_debug("mac80211_hwsim: ieee80211_alloc_hw failed\n");
        err = -ENOMEM;
        goto failed;
    }

    param->hwname = wiphy_name(hw->wiphy);

    if (info)
        net = genl_info_net(info);
    else
        net = &init_net;
    wiphy_net_set(hw->wiphy, net);

    data = hw->priv;
    data->hw = hw;

    data->dev = device_create(hwsim_class, NULL, 0, hw, "hwsim%d", idx);
    if (IS_ERR(data->dev)) {
        printk(KERN_DEBUG
               "mac80211_hwsim: device_create failed (%ld)\n",
               PTR_ERR(data->dev));
        err = -ENOMEM;
        goto failed_drvdata;
    }
    data->dev->driver = &mac80211_hwsim_driver.driver;
    err = device_bind_driver(data->dev);
    if (err != 0) {
        pr_debug("mac80211_hwsim: device_bind_driver failed (%d)\n",
               err);
        goto failed_bind;
    }

    skb_queue_head_init(&data->pending);

    SET_IEEE80211_DEV(hw, data->dev);
    eth_zero_addr(addr);
    addr[0] = 0x02;
    addr[3] = idx >> 8;
    addr[4] = idx;
    memcpy(data->addresses[0].addr, addr, ETH_ALEN);
    memcpy(data->addresses[1].addr, addr, ETH_ALEN);
    data->addresses[1].addr[0] |= 0x40;
    hw->wiphy->n_addresses = 2;
    hw->wiphy->addresses = data->addresses;


    data->channels = param->channels;
    data->use_chanctx = param->use_chanctx;
    data->idx = idx;
    data->destroy_on_close = param->destroy_on_close;
    if (info)
        data->portid = info->snd_portid;

    data->if_limits[n_limits].max = 2048;
    data->if_limits[n_limits].types = BIT(NL80211_IFTYPE_STATION) | BIT(NL80211_IFTYPE_AP);
    n_limits++;

    if (data->use_chanctx) {
        hw->wiphy->max_scan_ssids = 255;
        hw->wiphy->max_scan_ie_len = IEEE80211_MAX_DATA_LEN;
        hw->wiphy->max_remain_on_channel_duration = 1000;
        data->if_combination.radar_detect_widths = 0;
        data->if_combination.num_different_channels = data->channels;
        data->chanctx = NULL;
    } else {
        data->if_combination.num_different_channels = 1;
        data->if_combination.radar_detect_widths =
                    BIT(NL80211_CHAN_WIDTH_5) |
                    BIT(NL80211_CHAN_WIDTH_10) |
                    BIT(NL80211_CHAN_WIDTH_20_NOHT) |
                    BIT(NL80211_CHAN_WIDTH_20) |
                    BIT(NL80211_CHAN_WIDTH_40) |
                    BIT(NL80211_CHAN_WIDTH_80) |
                    BIT(NL80211_CHAN_WIDTH_160);
    }

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

    /*
     * If we actually were asked to support combinations,
     * advertise them - if there's only a single thing like
     * only IBSS then don't advertise it as combinations.
     */
    if (data->if_combination.max_interfaces > 1) {
        hw->wiphy->iface_combinations = &data->if_combination;
        hw->wiphy->n_iface_combinations = 1;
    }

    if (param->ciphers) {
        memcpy(data->ciphers, param->ciphers,
               param->n_ciphers * sizeof(u32));
        hw->wiphy->cipher_suites = data->ciphers;
        hw->wiphy->n_cipher_suites = param->n_ciphers;
    }

    data->rx_rssi = DEFAULT_RX_RSSI;

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
    ieee80211_hw_set(hw, REPORTS_TX_ACK_STATUS);
    ieee80211_hw_set(hw, HOST_BROADCAST_PS_BUFFERING);
    ieee80211_hw_set(hw, PS_NULLFUNC_STACK);
    ieee80211_hw_set(hw, TDLS_WIDER_BW);
    ieee80211_hw_set(hw, SUPPORTS_MULTI_BSSID);

    hw->wiphy->flags &= ~WIPHY_FLAG_PS_ON_BY_DEFAULT;
    hw->wiphy->flags |= WIPHY_FLAG_SUPPORTS_TDLS |
                WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL |
                WIPHY_FLAG_AP_UAPSD |
                WIPHY_FLAG_SUPPORTS_5_10_MHZ |
                WIPHY_FLAG_HAS_CHANNEL_SWITCH;
    hw->wiphy->features |= NL80211_FEATURE_ACTIVE_MONITOR |
                   NL80211_FEATURE_AP_MODE_CHAN_WIDTH_CHANGE |
                   NL80211_FEATURE_STATIC_SMPS |
                   NL80211_FEATURE_DYNAMIC_SMPS |
                   NL80211_FEATURE_SCAN_RANDOM_MAC_ADDR;
    wiphy_ext_feature_set(hw->wiphy, NL80211_EXT_FEATURE_VHT_IBSS);
    wiphy_ext_feature_set(hw->wiphy, NL80211_EXT_FEATURE_BEACON_PROTECTION);
    wiphy_ext_feature_set(hw->wiphy,
                  NL80211_EXT_FEATURE_MULTICAST_REGISTRATIONS);
    wiphy_ext_feature_set(hw->wiphy,
                  NL80211_EXT_FEATURE_BEACON_RATE_LEGACY);

    hw->wiphy->interface_modes = BIT(NL80211_IFTYPE_STATION) | BIT(NL80211_IFTYPE_AP);

    /* ask mac80211 to reserve space for magic */
    hw->vif_data_size = sizeof(struct hwsim_vif_priv);
    hw->sta_data_size = sizeof(struct hwsim_sta_priv);
    hw->chanctx_data_size = sizeof(struct hwsim_chanctx_priv);

    memcpy(data->channels_2ghz, rdkfmac_channels_2ghz,
        sizeof(rdkfmac_channels_2ghz));
    memcpy(data->channels_5ghz, rdkfmac_channels_5ghz,
        sizeof(rdkfmac_channels_5ghz));
    memcpy(data->channels_6ghz, rdkfmac_channels_6ghz,
        sizeof(rdkfmac_channels_6ghz));
    memcpy(data->channels_s1g, rdkfmac_channels_s1g,
           sizeof(rdkfmac_channels_s1g));
    memcpy(data->rates, hwsim_rates, sizeof(hwsim_rates));

    for (band = NL80211_BAND_2GHZ; band < NUM_NL80211_BANDS; band++) {
        struct ieee80211_supported_band *sband = &data->bands[band];

        sband->band = band;

        switch (band) {
        case NL80211_BAND_2GHZ:
            sband->channels = data->channels_2ghz;
            sband->n_channels = ARRAY_SIZE(rdkfmac_channels_2ghz);
            sband->bitrates = data->rates;
            sband->n_bitrates = ARRAY_SIZE(hwsim_rates);
            break;
        case NL80211_BAND_5GHZ:
            sband->channels = data->channels_5ghz;
            sband->n_channels = ARRAY_SIZE(rdkfmac_channels_5ghz);
            sband->bitrates = data->rates + 4;
            sband->n_bitrates = ARRAY_SIZE(hwsim_rates) - 4;

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
        case NL80211_BAND_6GHZ:
            sband->channels = data->channels_6ghz;
            sband->n_channels = ARRAY_SIZE(rdkfmac_channels_6ghz);
            sband->bitrates = data->rates + 4;
            sband->n_bitrates = ARRAY_SIZE(hwsim_rates) - 4;
            break;
        case NL80211_BAND_S1GHZ:
            memcpy(&sband->s1g_cap, &hwsim_s1g_cap,
                   sizeof(sband->s1g_cap));
            sband->channels = data->channels_s1g;
            sband->n_channels = ARRAY_SIZE(rdkfmac_channels_s1g);
            break;
        default:
            continue;
        }

        if (band != NL80211_BAND_6GHZ){
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
        }

        mac80211_hwsim_sband_capab(sband);

        hw->wiphy->bands[band] = sband;
    }

    data->group = 1;
    mutex_init(&data->mutex);

    data->netgroup = hwsim_net_get_netgroup(net);
    data->wmediumd = hwsim_net_get_wmediumd(net);

    /* Enable frame retransmissions for lossy channels */
    hw->max_rates = 4;
    hw->max_rate_tries = 11;

    hw->wiphy->vendor_commands = mac80211_hwsim_vendor_commands;
    hw->wiphy->n_vendor_commands =
        ARRAY_SIZE(mac80211_hwsim_vendor_commands);
    hw->wiphy->vendor_events = mac80211_hwsim_vendor_events;
    hw->wiphy->n_vendor_events = ARRAY_SIZE(mac80211_hwsim_vendor_events);

    if (param->reg_strict)
        hw->wiphy->regulatory_flags |= REGULATORY_STRICT_REG;
    if (param->regd) {
        data->regd = param->regd;
        hw->wiphy->regulatory_flags |= REGULATORY_CUSTOM_REG;
        wiphy_apply_custom_regulatory(hw->wiphy, param->regd);
        /* give the regulatory workqueue a chance to run */
        schedule_timeout_interruptible(1);
    }

    if (param->no_vif)
        ieee80211_hw_set(hw, NO_AUTO_VIF);

    wiphy_ext_feature_set(hw->wiphy, NL80211_EXT_FEATURE_CQM_RSSI_LIST);

    hrtimer_init(&data->beacon_timer, CLOCK_MONOTONIC,
             HRTIMER_MODE_ABS_SOFT);
    data->beacon_timer.function = mac80211_hwsim_beacon;

    err = ieee80211_register_hw(hw);
    if (err < 0) {
        pr_debug("mac80211_hwsim: ieee80211_register_hw failed (%d)\n",
               err);
        goto failed_hw;
    }

    wiphy_dbg(hw->wiphy, "hwaddr %pM registered\n", hw->wiphy->perm_addr);

    if (param->reg_alpha2) {
        data->alpha2[0] = param->reg_alpha2[0];
        data->alpha2[1] = param->reg_alpha2[1];
        regulatory_hint(hw->wiphy, param->reg_alpha2);
    }

    data->debugfs = debugfs_create_dir("hwsim", hw->wiphy->debugfsdir);
    debugfs_create_file("ps", 0666, data->debugfs, data, &hwsim_fops_ps);
    debugfs_create_file("group", 0666, data->debugfs, data,
                &hwsim_fops_group);
    debugfs_create_file("rx_rssi", 0666, data->debugfs, data,
                &hwsim_fops_rx_rssi);
    if (!data->use_chanctx)
        debugfs_create_file("dfs_simulate_radar", 0222,
                    data->debugfs,
                    data, &hwsim_simulate_radar);

    spin_lock_bh(&hwsim_radio_lock);
    err = rhashtable_insert_fast(&hwsim_radios_rht, &data->rht,
                     hwsim_rht_params);
    if (err < 0) {
        if (info) {
            GENL_SET_ERR_MSG(info, "perm addr already present");
            NL_SET_BAD_ATTR(info->extack,
                    info->attrs[HWSIM_ATTR_PERM_ADDR]);
        }
        spin_unlock_bh(&hwsim_radio_lock);
        goto failed_final_insert;
    }

    list_add_tail(&data->list, &hwsim_radios);
    hwsim_radios_generation++;
    spin_unlock_bh(&hwsim_radio_lock);

    hwsim_mcast_new_radio(idx, info, param);

    return idx;

failed_final_insert:
    debugfs_remove_recursive(data->debugfs);
    ieee80211_unregister_hw(data->hw);
failed_hw:
    device_release_driver(data->dev);
failed_bind:
    device_unregister(data->dev);
failed_drvdata:
    ieee80211_free_hw(hw);
failed:
    return err;
}

static void hwsim_mcast_del_radio(int id, const char *hwname,
                  struct genl_info *info)
{
    struct sk_buff *skb;
    void *data;
    int ret;

    skb = genlmsg_new(GENLMSG_DEFAULT_SIZE, GFP_KERNEL);
    if (!skb)
        return;

    data = genlmsg_put(skb, 0, 0, &hwsim_genl_family, 0,
               HWSIM_CMD_DEL_RADIO);
    if (!data)
        goto error;

    ret = nla_put_u32(skb, HWSIM_ATTR_RADIO_ID, id);
    if (ret < 0)
        goto error;

    ret = nla_put(skb, HWSIM_ATTR_RADIO_NAME, strlen(hwname),
              hwname);
    if (ret < 0)
        goto error;

    genlmsg_end(skb, data);

    hwsim_mcast_config_msg(skb, info);

    return;

error:
    nlmsg_free(skb);
}

static void mac80211_hwsim_del_radio(struct mac80211_rdkfmac_data *data,
                     const char *hwname,
                     struct genl_info *info)
{
    hwsim_mcast_del_radio(data->idx, hwname, info);
    debugfs_remove_recursive(data->debugfs);
    ieee80211_unregister_hw(data->hw);
    device_release_driver(data->dev);
    device_unregister(data->dev);
    ieee80211_free_hw(data->hw);
}

static int mac80211_hwsim_get_radio(struct sk_buff *skb,
                    struct mac80211_rdkfmac_data *data,
                    u32 portid, u32 seq,
                    struct netlink_callback *cb, int flags)
{
    void *hdr;
    struct hwsim_new_radio_params param = { };
    int res = -EMSGSIZE;

    hdr = genlmsg_put(skb, portid, seq, &hwsim_genl_family, flags,
              HWSIM_CMD_GET_RADIO);
    if (!hdr)
        return -EMSGSIZE;

    if (cb)
        genl_dump_check_consistent(cb, hdr);

    if (data->alpha2[0] && data->alpha2[1])
        param.reg_alpha2 = data->alpha2;

    param.reg_strict = !!(data->hw->wiphy->regulatory_flags &
                    REGULATORY_STRICT_REG);
    param.p2p_device = !!(data->hw->wiphy->interface_modes &
                    BIT(NL80211_IFTYPE_P2P_DEVICE));
    param.use_chanctx = data->use_chanctx;
    param.regd = data->regd;
    param.channels = data->channels;
    param.hwname = wiphy_name(data->hw->wiphy);

    res = append_radio_msg(skb, data->idx, &param);
    if (res < 0)
        goto out_err;

    genlmsg_end(skb, hdr);
    return 0;

out_err:
    genlmsg_cancel(skb, hdr);
    return res;
}

static void mac80211_hwsim_free(void)
{
    struct mac80211_rdkfmac_data *data;

    spin_lock_bh(&hwsim_radio_lock);
    while ((data = list_first_entry_or_null(&hwsim_radios,
                        struct mac80211_rdkfmac_data ,
                        list))) {
        list_del(&data->list);
        spin_unlock_bh(&hwsim_radio_lock);
        mac80211_hwsim_del_radio(data, wiphy_name(data->hw->wiphy),
                     NULL);
        spin_lock_bh(&hwsim_radio_lock);
    }
    spin_unlock_bh(&hwsim_radio_lock);
    class_destroy(hwsim_class);
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




static int validate_beacon_head(const struct nlattr *attr,
                struct netlink_ext_ack *extack)
{
    const u8 *data = nla_data(attr);
    unsigned int len = nla_len(attr);
    const struct element *elem;
    const struct ieee80211_mgmt *mgmt = (void *)data;
    unsigned int fixedlen, hdrlen;
    bool s1g_bcn;

    if (len < offsetofend(typeof(*mgmt), frame_control))
        goto err;

    s1g_bcn = ieee80211_is_s1g_beacon(mgmt->frame_control);
    if (s1g_bcn) {
        fixedlen = offsetof(struct ieee80211_ext,
                    u.s1g_beacon.variable);
        hdrlen = offsetof(struct ieee80211_ext, u.s1g_beacon);
    } else {
        fixedlen = offsetof(struct ieee80211_mgmt,
                    u.beacon.variable);
        hdrlen = offsetof(struct ieee80211_mgmt, u.beacon);
    }

    if (len < fixedlen)
        goto err;

    if (ieee80211_hdrlen(mgmt->frame_control) != hdrlen)
        goto err;

    data += fixedlen;
    len -= fixedlen;

    for_each_element(elem, data, len) {
        /* nothing */
    }

    if (for_each_element_completed(elem, data, len))
        return 0;

err:
    NL_SET_ERR_MSG_ATTR(extack, attr, "malformed beacon head");
    return -EINVAL;
}

static int validate_ie_attr(const struct nlattr *attr,
                struct netlink_ext_ack *extack)
{
    const u8 *data = nla_data(attr);
    unsigned int len = nla_len(attr);
    const struct element *elem;

    for_each_element(elem, data, len) {
        /* nothing */
    }

    if (for_each_element_completed(elem, data, len))
        return 0;

    NL_SET_ERR_MSG_ATTR(extack, attr, "malformed information elements");
    return -EINVAL;
}

static int validate_he_capa(const struct nlattr *attr,
                struct netlink_ext_ack *extack)
{
    if (!ieee80211_he_capa_size_ok(nla_data(attr), nla_len(attr)))
        return -EINVAL;

    return 0;
}

static const struct nla_policy test_policy[NUM_NL80211_ATTR];

static const struct nla_policy
nl80211_ftm_responder_policy[NL80211_FTM_RESP_ATTR_MAX + 1] = {
    [NL80211_FTM_RESP_ATTR_ENABLED] = { .type = NLA_FLAG, },
    [NL80211_FTM_RESP_ATTR_LCI] = { .type = NLA_BINARY,
                    .len = U8_MAX },
    [NL80211_FTM_RESP_ATTR_CIVICLOC] = { .type = NLA_BINARY,
                         .len = U8_MAX },
};

static const struct nla_policy
nl80211_pmsr_ftm_req_attr_policy[NL80211_PMSR_FTM_REQ_ATTR_MAX + 1] = {
    [NL80211_PMSR_FTM_REQ_ATTR_ASAP] = { .type = NLA_FLAG },
    [NL80211_PMSR_FTM_REQ_ATTR_PREAMBLE] = { .type = NLA_U32 },
    [NL80211_PMSR_FTM_REQ_ATTR_NUM_BURSTS_EXP] =
        NLA_POLICY_MAX(NLA_U8, 15),
    [NL80211_PMSR_FTM_REQ_ATTR_BURST_PERIOD] = { .type = NLA_U16 },
    [NL80211_PMSR_FTM_REQ_ATTR_BURST_DURATION] =
        NLA_POLICY_MAX(NLA_U8, 15),
    [NL80211_PMSR_FTM_REQ_ATTR_FTMS_PER_BURST] =
        NLA_POLICY_MAX(NLA_U8, 31),
    [NL80211_PMSR_FTM_REQ_ATTR_NUM_FTMR_RETRIES] = { .type = NLA_U8 },
    [NL80211_PMSR_FTM_REQ_ATTR_REQUEST_LCI] = { .type = NLA_FLAG },
    [NL80211_PMSR_FTM_REQ_ATTR_REQUEST_CIVICLOC] = { .type = NLA_FLAG },
    [NL80211_PMSR_FTM_REQ_ATTR_TRIGGER_BASED] = { .type = NLA_FLAG },
    [NL80211_PMSR_FTM_REQ_ATTR_NON_TRIGGER_BASED] = { .type = NLA_FLAG },
    [NL80211_PMSR_FTM_REQ_ATTR_LMR_FEEDBACK] = { .type = NLA_FLAG },
};

static const struct nla_policy
nl80211_pmsr_req_data_policy[NL80211_PMSR_TYPE_MAX + 1] = {
    [NL80211_PMSR_TYPE_FTM] =
        NLA_POLICY_NESTED(nl80211_pmsr_ftm_req_attr_policy),
};

static const struct nla_policy
nl80211_pmsr_req_attr_policy[NL80211_PMSR_REQ_ATTR_MAX + 1] = {
    [NL80211_PMSR_REQ_ATTR_DATA] =
        NLA_POLICY_NESTED(nl80211_pmsr_req_data_policy),
    [NL80211_PMSR_REQ_ATTR_GET_AP_TSF] = { .type = NLA_FLAG },
};

static const struct nla_policy
nl80211_pmsr_peer_attr_policy[NL80211_PMSR_PEER_ATTR_MAX + 1] = {
    [NL80211_PMSR_PEER_ATTR_ADDR] = NLA_POLICY_ETH_ADDR,
    [NL80211_PMSR_PEER_ATTR_CHAN] = NLA_POLICY_NESTED(test_policy),
    [NL80211_PMSR_PEER_ATTR_REQ] =
        NLA_POLICY_NESTED(nl80211_pmsr_req_attr_policy),
    [NL80211_PMSR_PEER_ATTR_RESP] = { .type = NLA_REJECT },
};

static const struct nla_policy
nl80211_pmsr_attr_policy[NL80211_PMSR_ATTR_MAX + 1] = {
    [NL80211_PMSR_ATTR_MAX_PEERS] = { .type = NLA_REJECT },
    [NL80211_PMSR_ATTR_REPORT_AP_TSF] = { .type = NLA_REJECT },
    [NL80211_PMSR_ATTR_RANDOMIZE_MAC_ADDR] = { .type = NLA_REJECT },
    [NL80211_PMSR_ATTR_TYPE_CAPA] = { .type = NLA_REJECT },
    [NL80211_PMSR_ATTR_PEERS] =
        NLA_POLICY_NESTED_ARRAY(nl80211_pmsr_peer_attr_policy),
};

static const struct nla_policy
he_obss_pd_policy[NL80211_HE_OBSS_PD_ATTR_MAX + 1] = {
    [NL80211_HE_OBSS_PD_ATTR_MIN_OFFSET] =
        NLA_POLICY_RANGE(NLA_U8, 1, 20),
    [NL80211_HE_OBSS_PD_ATTR_MAX_OFFSET] =
        NLA_POLICY_RANGE(NLA_U8, 1, 20),
    [NL80211_HE_OBSS_PD_ATTR_NON_SRG_MAX_OFFSET] =
        NLA_POLICY_RANGE(NLA_U8, 1, 20),
    [NL80211_HE_OBSS_PD_ATTR_BSS_COLOR_BITMAP] =
        NLA_POLICY_EXACT_LEN(8),
    [NL80211_HE_OBSS_PD_ATTR_PARTIAL_BSSID_BITMAP] =
        NLA_POLICY_EXACT_LEN(8),
    [NL80211_HE_OBSS_PD_ATTR_SR_CTRL] = { .type = NLA_U8 },
};

static const struct nla_policy
he_bss_color_policy[NL80211_HE_BSS_COLOR_ATTR_MAX + 1] = {
    [NL80211_HE_BSS_COLOR_ATTR_COLOR] = NLA_POLICY_RANGE(NLA_U8, 1, 63),
    [NL80211_HE_BSS_COLOR_ATTR_DISABLED] = { .type = NLA_FLAG },
    [NL80211_HE_BSS_COLOR_ATTR_PARTIAL] = { .type = NLA_FLAG },
};

static const struct nla_policy nl80211_txattr_policy[NL80211_TXRATE_MAX + 1] = {
    [NL80211_TXRATE_LEGACY] = { .type = NLA_BINARY,
                    .len = NL80211_MAX_SUPP_RATES },
    [NL80211_TXRATE_HT] = { .type = NLA_BINARY,
                .len = NL80211_MAX_SUPP_HT_RATES },
    [NL80211_TXRATE_VHT] = NLA_POLICY_EXACT_LEN_WARN(sizeof(struct nl80211_txrate_vht)),
    [NL80211_TXRATE_GI] = { .type = NLA_U8 },
    [NL80211_TXRATE_HE] = NLA_POLICY_EXACT_LEN(sizeof(struct nl80211_txrate_he)),
    [NL80211_TXRATE_HE_GI] =  NLA_POLICY_RANGE(NLA_U8,
                           NL80211_RATE_INFO_HE_GI_0_8,
                           NL80211_RATE_INFO_HE_GI_3_2),
    [NL80211_TXRATE_HE_LTF] = NLA_POLICY_RANGE(NLA_U8,
                           NL80211_RATE_INFO_HE_1XLTF,
                           NL80211_RATE_INFO_HE_4XLTF),
};

static const struct nla_policy
nl80211_tid_config_attr_policy[NL80211_TID_CONFIG_ATTR_MAX + 1] = {
    [NL80211_TID_CONFIG_ATTR_VIF_SUPP] = { .type = NLA_U64 },
    [NL80211_TID_CONFIG_ATTR_PEER_SUPP] = { .type = NLA_U64 },
    [NL80211_TID_CONFIG_ATTR_OVERRIDE] = { .type = NLA_FLAG },
    [NL80211_TID_CONFIG_ATTR_TIDS] = NLA_POLICY_RANGE(NLA_U16, 1, 0xff),
    [NL80211_TID_CONFIG_ATTR_NOACK] =
            NLA_POLICY_MAX(NLA_U8, NL80211_TID_CONFIG_DISABLE),
    [NL80211_TID_CONFIG_ATTR_RETRY_SHORT] = NLA_POLICY_MIN(NLA_U8, 1),
    [NL80211_TID_CONFIG_ATTR_RETRY_LONG] = NLA_POLICY_MIN(NLA_U8, 1),
    [NL80211_TID_CONFIG_ATTR_AMPDU_CTRL] =
            NLA_POLICY_MAX(NLA_U8, NL80211_TID_CONFIG_DISABLE),
    [NL80211_TID_CONFIG_ATTR_RTSCTS_CTRL] =
            NLA_POLICY_MAX(NLA_U8, NL80211_TID_CONFIG_DISABLE),
    [NL80211_TID_CONFIG_ATTR_AMSDU_CTRL] =
            NLA_POLICY_MAX(NLA_U8, NL80211_TID_CONFIG_DISABLE),
    [NL80211_TID_CONFIG_ATTR_TX_RATE_TYPE] =
            NLA_POLICY_MAX(NLA_U8, NL80211_TX_RATE_FIXED),
    [NL80211_TID_CONFIG_ATTR_TX_RATE] =
            NLA_POLICY_NESTED(nl80211_txattr_policy),
};

static const struct nla_policy
nl80211_fils_discovery_policy[NL80211_FILS_DISCOVERY_ATTR_MAX + 1] = {
    [NL80211_FILS_DISCOVERY_ATTR_INT_MIN] = NLA_POLICY_MAX(NLA_U32, 10000),
    [NL80211_FILS_DISCOVERY_ATTR_INT_MAX] = NLA_POLICY_MAX(NLA_U32, 10000),
    [NL80211_FILS_DISCOVERY_ATTR_TMPL] =
            NLA_POLICY_RANGE(NLA_BINARY,
                     NL80211_FILS_DISCOVERY_TMPL_MIN_LEN,
                     IEEE80211_MAX_DATA_LEN),
};

static const struct nla_policy
nl80211_unsol_bcast_probe_resp_policy[NL80211_UNSOL_BCAST_PROBE_RESP_ATTR_MAX + 1] = {
    [NL80211_UNSOL_BCAST_PROBE_RESP_ATTR_INT] = NLA_POLICY_MAX(NLA_U32, 20),
    [NL80211_UNSOL_BCAST_PROBE_RESP_ATTR_TMPL] = { .type = NLA_BINARY,
                               .len = IEEE80211_MAX_DATA_LEN }
};

static const struct nla_policy
sar_specs_policy[NL80211_SAR_ATTR_SPECS_MAX + 1] = {
    [NL80211_SAR_ATTR_SPECS_POWER] = { .type = NLA_S32 },
    [NL80211_SAR_ATTR_SPECS_RANGE_INDEX] = {.type = NLA_U32 },
};

static const struct nla_policy
sar_policy[NL80211_SAR_ATTR_MAX + 1] = {
    [NL80211_SAR_ATTR_TYPE] = NLA_POLICY_MAX(NLA_U32, NUM_NL80211_SAR_TYPE),
    [NL80211_SAR_ATTR_SPECS] = NLA_POLICY_NESTED_ARRAY(sar_specs_policy),
};

static const struct nla_policy
nl80211_mbssid_config_policy[NL80211_MBSSID_CONFIG_ATTR_MAX + 1] = {
    [NL80211_MBSSID_CONFIG_ATTR_MAX_INTERFACES] = NLA_POLICY_MIN(NLA_U8, 2),
    [NL80211_MBSSID_CONFIG_ATTR_MAX_EMA_PROFILE_PERIODICITY] =
                        NLA_POLICY_MIN(NLA_U8, 1),
    [NL80211_MBSSID_CONFIG_ATTR_INDEX] = { .type = NLA_U8 },
    [NL80211_MBSSID_CONFIG_ATTR_TX_IFINDEX] = { .type = NLA_U32 },
    [NL80211_MBSSID_CONFIG_ATTR_EMA] = { .type = NLA_FLAG },
};

static const struct nla_policy test_policy[NUM_NL80211_ATTR] = {
    [0] = { .strict_start_type = NL80211_ATTR_HE_OBSS_PD },
    [NL80211_ATTR_WIPHY] = { .type = NLA_U32 },
    [NL80211_ATTR_WIPHY_NAME] = { .type = NLA_NUL_STRING,
                      .len = 20-1 },
    [NL80211_ATTR_WIPHY_TXQ_PARAMS] = { .type = NLA_NESTED },

    [NL80211_ATTR_WIPHY_FREQ] = { .type = NLA_U32 },
    [NL80211_ATTR_WIPHY_CHANNEL_TYPE] = { .type = NLA_U32 },
    [NL80211_ATTR_WIPHY_EDMG_CHANNELS] = NLA_POLICY_RANGE(NLA_U8,
                        NL80211_EDMG_CHANNELS_MIN,
                        NL80211_EDMG_CHANNELS_MAX),
    [NL80211_ATTR_WIPHY_EDMG_BW_CONFIG] = NLA_POLICY_RANGE(NLA_U8,
                        NL80211_EDMG_BW_CONFIG_MIN,
                        NL80211_EDMG_BW_CONFIG_MAX),

    [NL80211_ATTR_CHANNEL_WIDTH] = { .type = NLA_U32 },
    [NL80211_ATTR_CENTER_FREQ1] = { .type = NLA_U32 },
    [NL80211_ATTR_CENTER_FREQ1_OFFSET] = NLA_POLICY_RANGE(NLA_U32, 0, 999),
    [NL80211_ATTR_CENTER_FREQ2] = { .type = NLA_U32 },

    [NL80211_ATTR_WIPHY_RETRY_SHORT] = NLA_POLICY_MIN(NLA_U8, 1),
    [NL80211_ATTR_WIPHY_RETRY_LONG] = NLA_POLICY_MIN(NLA_U8, 1),
    [NL80211_ATTR_WIPHY_FRAG_THRESHOLD] = { .type = NLA_U32 },
    [NL80211_ATTR_WIPHY_RTS_THRESHOLD] = { .type = NLA_U32 },
    [NL80211_ATTR_WIPHY_COVERAGE_CLASS] = { .type = NLA_U8 },
    [NL80211_ATTR_WIPHY_DYN_ACK] = { .type = NLA_FLAG },

    [NL80211_ATTR_IFTYPE] = NLA_POLICY_MAX(NLA_U32, NL80211_IFTYPE_MAX),
    [NL80211_ATTR_IFINDEX] = { .type = NLA_U32 },
    [NL80211_ATTR_IFNAME] = { .type = NLA_NUL_STRING, .len = IFNAMSIZ-1 },

    [NL80211_ATTR_MAC] = NLA_POLICY_EXACT_LEN_WARN(ETH_ALEN),
    [NL80211_ATTR_PREV_BSSID] = NLA_POLICY_EXACT_LEN_WARN(ETH_ALEN),

    [NL80211_ATTR_KEY] = { .type = NLA_NESTED, },
    [NL80211_ATTR_KEY_DATA] = { .type = NLA_BINARY,
                    .len = WLAN_MAX_KEY_LEN },
    [NL80211_ATTR_KEY_IDX] = NLA_POLICY_MAX(NLA_U8, 7),
    [NL80211_ATTR_KEY_CIPHER] = { .type = NLA_U32 },
    [NL80211_ATTR_KEY_DEFAULT] = { .type = NLA_FLAG },
    [NL80211_ATTR_KEY_SEQ] = { .type = NLA_BINARY, .len = 16 },
    [NL80211_ATTR_KEY_TYPE] =
        NLA_POLICY_MAX(NLA_U32, NUM_NL80211_KEYTYPES),

    [NL80211_ATTR_BEACON_INTERVAL] = { .type = NLA_U32 },
    [NL80211_ATTR_DTIM_PERIOD] = { .type = NLA_U32 },
    [NL80211_ATTR_BEACON_HEAD] =
        NLA_POLICY_VALIDATE_FN(NLA_BINARY, validate_beacon_head,
                       IEEE80211_MAX_DATA_LEN),
    [NL80211_ATTR_BEACON_TAIL] =
        NLA_POLICY_VALIDATE_FN(NLA_BINARY, validate_ie_attr,
                       IEEE80211_MAX_DATA_LEN),
    [NL80211_ATTR_STA_AID] =
        NLA_POLICY_RANGE(NLA_U16, 1, IEEE80211_MAX_AID),
    [NL80211_ATTR_STA_FLAGS] = { .type = NLA_NESTED },
    [NL80211_ATTR_STA_LISTEN_INTERVAL] = { .type = NLA_U16 },
    [NL80211_ATTR_STA_SUPPORTED_RATES] = { .type = NLA_BINARY,
                           .len = NL80211_MAX_SUPP_RATES },
    [NL80211_ATTR_STA_PLINK_ACTION] =
        NLA_POLICY_MAX(NLA_U8, NUM_NL80211_PLINK_ACTIONS - 1),
    [NL80211_ATTR_STA_TX_POWER_SETTING] =
        NLA_POLICY_RANGE(NLA_U8,
                 NL80211_TX_POWER_AUTOMATIC,
                 NL80211_TX_POWER_FIXED),
    [NL80211_ATTR_STA_TX_POWER] = { .type = NLA_S16 },
    [NL80211_ATTR_STA_VLAN] = { .type = NLA_U32 },
    [NL80211_ATTR_MNTR_FLAGS] = { /* NLA_NESTED can't be empty */ },
    [NL80211_ATTR_MESH_ID] = { .type = NLA_BINARY,
                   .len = IEEE80211_MAX_MESH_ID_LEN },
    [NL80211_ATTR_MPATH_NEXT_HOP] = NLA_POLICY_ETH_ADDR_COMPAT,

    /* allow 3 for NUL-termination, we used to declare this NLA_STRING */
    [NL80211_ATTR_REG_ALPHA2] = NLA_POLICY_RANGE(NLA_BINARY, 2, 3),
    [NL80211_ATTR_REG_RULES] = { .type = NLA_NESTED },

    [NL80211_ATTR_BSS_CTS_PROT] = { .type = NLA_U8 },
    [NL80211_ATTR_BSS_SHORT_PREAMBLE] = { .type = NLA_U8 },
    [NL80211_ATTR_BSS_SHORT_SLOT_TIME] = { .type = NLA_U8 },
    [NL80211_ATTR_BSS_BASIC_RATES] = { .type = NLA_BINARY,
                       .len = NL80211_MAX_SUPP_RATES },
    [NL80211_ATTR_BSS_HT_OPMODE] = { .type = NLA_U16 },

    [NL80211_ATTR_MESH_CONFIG] = { .type = NLA_NESTED },
    [NL80211_ATTR_SUPPORT_MESH_AUTH] = { .type = NLA_FLAG },

    [NL80211_ATTR_HT_CAPABILITY] = NLA_POLICY_EXACT_LEN_WARN(NL80211_HT_CAPABILITY_LEN),

    [NL80211_ATTR_MGMT_SUBTYPE] = { .type = NLA_U8 },
    [NL80211_ATTR_IE] = NLA_POLICY_VALIDATE_FN(NLA_BINARY,
                           validate_ie_attr,
                           IEEE80211_MAX_DATA_LEN),
    [NL80211_ATTR_SCAN_FREQUENCIES] = { .type = NLA_NESTED },
    [NL80211_ATTR_SCAN_SSIDS] = { .type = NLA_NESTED },

    [NL80211_ATTR_SSID] = { .type = NLA_BINARY,
                .len = IEEE80211_MAX_SSID_LEN },
    [NL80211_ATTR_AUTH_TYPE] = { .type = NLA_U32 },
    [NL80211_ATTR_REASON_CODE] = { .type = NLA_U16 },
    [NL80211_ATTR_FREQ_FIXED] = { .type = NLA_FLAG },
    [NL80211_ATTR_TIMED_OUT] = { .type = NLA_FLAG },
    [NL80211_ATTR_USE_MFP] = NLA_POLICY_RANGE(NLA_U32,
                          NL80211_MFP_NO,
                          NL80211_MFP_OPTIONAL),
    [NL80211_ATTR_STA_FLAGS2] = {
        .len = sizeof(struct nl80211_sta_flag_update),
    },
    [NL80211_ATTR_CONTROL_PORT] = { .type = NLA_FLAG },
    [NL80211_ATTR_CONTROL_PORT_ETHERTYPE] = { .type = NLA_U16 },
    [NL80211_ATTR_CONTROL_PORT_NO_ENCRYPT] = { .type = NLA_FLAG },
    [NL80211_ATTR_CONTROL_PORT_OVER_NL80211] = { .type = NLA_FLAG },
    [NL80211_ATTR_PRIVACY] = { .type = NLA_FLAG },
    [NL80211_ATTR_STATUS_CODE] = { .type = NLA_U16 },
    [NL80211_ATTR_CIPHER_SUITE_GROUP] = { .type = NLA_U32 },
    [NL80211_ATTR_WPA_VERSIONS] = { .type = NLA_U32 },
    [NL80211_ATTR_PID] = { .type = NLA_U32 },
    [NL80211_ATTR_4ADDR] = { .type = NLA_U8 },
    [NL80211_ATTR_PMKID] = NLA_POLICY_EXACT_LEN_WARN(WLAN_PMKID_LEN),
    [NL80211_ATTR_DURATION] = { .type = NLA_U32 },
    [NL80211_ATTR_COOKIE] = { .type = NLA_U64 },
    [NL80211_ATTR_TX_RATES] = { .type = NLA_NESTED },
    [NL80211_ATTR_FRAME] = { .type = NLA_BINARY,
                 .len = IEEE80211_MAX_DATA_LEN },
    [NL80211_ATTR_FRAME_MATCH] = { .type = NLA_BINARY, },
    [NL80211_ATTR_PS_STATE] = NLA_POLICY_RANGE(NLA_U32,
                           NL80211_PS_DISABLED,
                           NL80211_PS_ENABLED),
    [NL80211_ATTR_CQM] = { .type = NLA_NESTED, },
    [NL80211_ATTR_LOCAL_STATE_CHANGE] = { .type = NLA_FLAG },
    [NL80211_ATTR_AP_ISOLATE] = { .type = NLA_U8 },
    [NL80211_ATTR_WIPHY_TX_POWER_SETTING] = { .type = NLA_U32 },
    [NL80211_ATTR_WIPHY_TX_POWER_LEVEL] = { .type = NLA_U32 },
    [NL80211_ATTR_FRAME_TYPE] = { .type = NLA_U16 },
    [NL80211_ATTR_WIPHY_ANTENNA_TX] = { .type = NLA_U32 },
    [NL80211_ATTR_WIPHY_ANTENNA_RX] = { .type = NLA_U32 },
    [NL80211_ATTR_MCAST_RATE] = { .type = NLA_U32 },
    [NL80211_ATTR_OFFCHANNEL_TX_OK] = { .type = NLA_FLAG },
    [NL80211_ATTR_KEY_DEFAULT_TYPES] = { .type = NLA_NESTED },
    [NL80211_ATTR_WOWLAN_TRIGGERS] = { .type = NLA_NESTED },
    [NL80211_ATTR_STA_PLINK_STATE] =
        NLA_POLICY_MAX(NLA_U8, NUM_NL80211_PLINK_STATES - 1),
    [NL80211_ATTR_MEASUREMENT_DURATION] = { .type = NLA_U16 },
    [NL80211_ATTR_MEASUREMENT_DURATION_MANDATORY] = { .type = NLA_FLAG },
    [NL80211_ATTR_MESH_PEER_AID] =
        NLA_POLICY_RANGE(NLA_U16, 1, IEEE80211_MAX_AID),
    [NL80211_ATTR_SCHED_SCAN_INTERVAL] = { .type = NLA_U32 },
    [NL80211_ATTR_REKEY_DATA] = { .type = NLA_NESTED },
    [NL80211_ATTR_SCAN_SUPP_RATES] = { .type = NLA_NESTED },
    [NL80211_ATTR_HIDDEN_SSID] =
        NLA_POLICY_RANGE(NLA_U32,
                 NL80211_HIDDEN_SSID_NOT_IN_USE,
                 NL80211_HIDDEN_SSID_ZERO_CONTENTS),
    [NL80211_ATTR_IE_PROBE_RESP] =
        NLA_POLICY_VALIDATE_FN(NLA_BINARY, validate_ie_attr,
                       IEEE80211_MAX_DATA_LEN),
    [NL80211_ATTR_IE_ASSOC_RESP] =
        NLA_POLICY_VALIDATE_FN(NLA_BINARY, validate_ie_attr,
                       IEEE80211_MAX_DATA_LEN),
    [NL80211_ATTR_ROAM_SUPPORT] = { .type = NLA_FLAG },
    [NL80211_ATTR_SCHED_SCAN_MATCH] = { .type = NLA_NESTED },
    [NL80211_ATTR_TX_NO_CCK_RATE] = { .type = NLA_FLAG },
    [NL80211_ATTR_TDLS_ACTION] = { .type = NLA_U8 },
    [NL80211_ATTR_TDLS_DIALOG_TOKEN] = { .type = NLA_U8 },
    [NL80211_ATTR_TDLS_OPERATION] = { .type = NLA_U8 },
    [NL80211_ATTR_TDLS_SUPPORT] = { .type = NLA_FLAG },
    [NL80211_ATTR_TDLS_EXTERNAL_SETUP] = { .type = NLA_FLAG },
    [NL80211_ATTR_TDLS_INITIATOR] = { .type = NLA_FLAG },
    [NL80211_ATTR_DONT_WAIT_FOR_ACK] = { .type = NLA_FLAG },
    [NL80211_ATTR_PROBE_RESP] = { .type = NLA_BINARY,
                      .len = IEEE80211_MAX_DATA_LEN },
    [NL80211_ATTR_DFS_REGION] = { .type = NLA_U8 },
    [NL80211_ATTR_DISABLE_HT] = { .type = NLA_FLAG },
    [NL80211_ATTR_HT_CAPABILITY_MASK] = {
        .len = NL80211_HT_CAPABILITY_LEN
    },
    [NL80211_ATTR_NOACK_MAP] = { .type = NLA_U16 },
    [NL80211_ATTR_INACTIVITY_TIMEOUT] = { .type = NLA_U16 },
    [NL80211_ATTR_BG_SCAN_PERIOD] = { .type = NLA_U16 },
    [NL80211_ATTR_WDEV] = { .type = NLA_U64 },
    [NL80211_ATTR_USER_REG_HINT_TYPE] = { .type = NLA_U32 },

    /* need to include at least Auth Transaction and Status Code */
    [NL80211_ATTR_AUTH_DATA] = NLA_POLICY_MIN_LEN(4),

    [NL80211_ATTR_VHT_CAPABILITY] = NLA_POLICY_EXACT_LEN_WARN(NL80211_VHT_CAPABILITY_LEN),
    [NL80211_ATTR_SCAN_FLAGS] = { .type = NLA_U32 },
    [NL80211_ATTR_P2P_CTWINDOW] = NLA_POLICY_MAX(NLA_U8, 127),
    [NL80211_ATTR_P2P_OPPPS] = NLA_POLICY_MAX(NLA_U8, 1),
    [NL80211_ATTR_LOCAL_MESH_POWER_MODE] =
        NLA_POLICY_RANGE(NLA_U32,
                 NL80211_MESH_POWER_UNKNOWN + 1,
                 NL80211_MESH_POWER_MAX),
    [NL80211_ATTR_ACL_POLICY] = {. type = NLA_U32 },
    [NL80211_ATTR_MAC_ADDRS] = { .type = NLA_NESTED },
    [NL80211_ATTR_STA_CAPABILITY] = { .type = NLA_U16 },
    [NL80211_ATTR_STA_EXT_CAPABILITY] = { .type = NLA_BINARY, },
    [NL80211_ATTR_SPLIT_WIPHY_DUMP] = { .type = NLA_FLAG, },
    [NL80211_ATTR_DISABLE_VHT] = { .type = NLA_FLAG },
    [NL80211_ATTR_VHT_CAPABILITY_MASK] = {
        .len = NL80211_VHT_CAPABILITY_LEN,
    },
    [NL80211_ATTR_MDID] = { .type = NLA_U16 },
    [NL80211_ATTR_IE_RIC] = { .type = NLA_BINARY,
                  .len = IEEE80211_MAX_DATA_LEN },
    [NL80211_ATTR_CRIT_PROT_ID] = { .type = NLA_U16 },
    [NL80211_ATTR_MAX_CRIT_PROT_DURATION] =
        NLA_POLICY_MAX(NLA_U16, NL80211_CRIT_PROTO_MAX_DURATION),
    [NL80211_ATTR_PEER_AID] =
        NLA_POLICY_RANGE(NLA_U16, 1, IEEE80211_MAX_AID),
    [NL80211_ATTR_CH_SWITCH_COUNT] = { .type = NLA_U32 },
    [NL80211_ATTR_CH_SWITCH_BLOCK_TX] = { .type = NLA_FLAG },
    [NL80211_ATTR_CSA_IES] = { .type = NLA_NESTED },
    [NL80211_ATTR_CNTDWN_OFFS_BEACON] = { .type = NLA_BINARY },
    [NL80211_ATTR_CNTDWN_OFFS_PRESP] = { .type = NLA_BINARY },
    [NL80211_ATTR_STA_SUPPORTED_CHANNELS] = NLA_POLICY_MIN_LEN(2),
    /*
     * The value of the Length field of the Supported Operating
     * Classes element is between 2 and 253.
     */
    [NL80211_ATTR_STA_SUPPORTED_OPER_CLASSES] =
        NLA_POLICY_RANGE(NLA_BINARY, 2, 253),
    [NL80211_ATTR_HANDLE_DFS] = { .type = NLA_FLAG },
    [NL80211_ATTR_OPMODE_NOTIF] = { .type = NLA_U8 },
    [NL80211_ATTR_VENDOR_ID] = { .type = NLA_U32 },
    [NL80211_ATTR_VENDOR_SUBCMD] = { .type = NLA_U32 },
    [NL80211_ATTR_VENDOR_DATA] = { .type = NLA_BINARY },
    [NL80211_ATTR_QOS_MAP] = NLA_POLICY_RANGE(NLA_BINARY,
                          IEEE80211_QOS_MAP_LEN_MIN,
                          IEEE80211_QOS_MAP_LEN_MAX),
    [NL80211_ATTR_MAC_HINT] = NLA_POLICY_EXACT_LEN_WARN(ETH_ALEN),
    [NL80211_ATTR_WIPHY_FREQ_HINT] = { .type = NLA_U32 },
    [NL80211_ATTR_TDLS_PEER_CAPABILITY] = { .type = NLA_U32 },
    [NL80211_ATTR_SOCKET_OWNER] = { .type = NLA_FLAG },
    [NL80211_ATTR_CSA_C_OFFSETS_TX] = { .type = NLA_BINARY },
    [NL80211_ATTR_USE_RRM] = { .type = NLA_FLAG },
    [NL80211_ATTR_TSID] = NLA_POLICY_MAX(NLA_U8, IEEE80211_NUM_TIDS - 1),
    [NL80211_ATTR_USER_PRIO] =
        NLA_POLICY_MAX(NLA_U8, IEEE80211_NUM_UPS - 1),
    [NL80211_ATTR_ADMITTED_TIME] = { .type = NLA_U16 },
    [NL80211_ATTR_SMPS_MODE] = { .type = NLA_U8 },
    [NL80211_ATTR_OPER_CLASS] = { .type = NLA_U8 },
    [NL80211_ATTR_MAC_MASK] = NLA_POLICY_EXACT_LEN_WARN(ETH_ALEN),
    [NL80211_ATTR_WIPHY_SELF_MANAGED_REG] = { .type = NLA_FLAG },
    [NL80211_ATTR_NETNS_FD] = { .type = NLA_U32 },
    [NL80211_ATTR_SCHED_SCAN_DELAY] = { .type = NLA_U32 },
    [NL80211_ATTR_REG_INDOOR] = { .type = NLA_FLAG },
    [NL80211_ATTR_PBSS] = { .type = NLA_FLAG },
    [NL80211_ATTR_BSS_SELECT] = { .type = NLA_NESTED },
    [NL80211_ATTR_STA_SUPPORT_P2P_PS] =
        NLA_POLICY_MAX(NLA_U8, NUM_NL80211_P2P_PS_STATUS - 1),
    [NL80211_ATTR_MU_MIMO_GROUP_DATA] = {
        .len = VHT_MUMIMO_GROUPS_DATA_LEN
    },
    [NL80211_ATTR_MU_MIMO_FOLLOW_MAC_ADDR] = NLA_POLICY_EXACT_LEN_WARN(ETH_ALEN),
    [NL80211_ATTR_NAN_MASTER_PREF] = NLA_POLICY_MIN(NLA_U8, 1),
    [NL80211_ATTR_BANDS] = { .type = NLA_U32 },
    [NL80211_ATTR_NAN_FUNC] = { .type = NLA_NESTED },
    [NL80211_ATTR_FILS_KEK] = { .type = NLA_BINARY,
                    .len = FILS_MAX_KEK_LEN },
    [NL80211_ATTR_FILS_NONCES] = NLA_POLICY_EXACT_LEN_WARN(2 * FILS_NONCE_LEN),
    [NL80211_ATTR_MULTICAST_TO_UNICAST_ENABLED] = { .type = NLA_FLAG, },
    [NL80211_ATTR_BSSID] = NLA_POLICY_EXACT_LEN_WARN(ETH_ALEN),
    [NL80211_ATTR_SCHED_SCAN_RELATIVE_RSSI] = { .type = NLA_S8 },
    [NL80211_ATTR_SCHED_SCAN_RSSI_ADJUST] = {
        .len = sizeof(struct nl80211_bss_select_rssi_adjust)
    },
    [NL80211_ATTR_TIMEOUT_REASON] = { .type = NLA_U32 },
    [NL80211_ATTR_FILS_ERP_USERNAME] = { .type = NLA_BINARY,
                         .len = FILS_ERP_MAX_USERNAME_LEN },
    [NL80211_ATTR_FILS_ERP_REALM] = { .type = NLA_BINARY,
                      .len = FILS_ERP_MAX_REALM_LEN },
    [NL80211_ATTR_FILS_ERP_NEXT_SEQ_NUM] = { .type = NLA_U16 },
    [NL80211_ATTR_FILS_ERP_RRK] = { .type = NLA_BINARY,
                    .len = FILS_ERP_MAX_RRK_LEN },
    [NL80211_ATTR_FILS_CACHE_ID] = NLA_POLICY_EXACT_LEN_WARN(2),
    [NL80211_ATTR_PMK] = { .type = NLA_BINARY, .len = PMK_MAX_LEN },
    [NL80211_ATTR_PMKR0_NAME] = NLA_POLICY_EXACT_LEN(WLAN_PMK_NAME_LEN),
    [NL80211_ATTR_SCHED_SCAN_MULTI] = { .type = NLA_FLAG },
    [NL80211_ATTR_EXTERNAL_AUTH_SUPPORT] = { .type = NLA_FLAG },

    [NL80211_ATTR_TXQ_LIMIT] = { .type = NLA_U32 },
    [NL80211_ATTR_TXQ_MEMORY_LIMIT] = { .type = NLA_U32 },
    [NL80211_ATTR_TXQ_QUANTUM] = { .type = NLA_U32 },
    [NL80211_ATTR_HE_CAPABILITY] =
        NLA_POLICY_VALIDATE_FN(NLA_BINARY, validate_he_capa,
                       NL80211_HE_MAX_CAPABILITY_LEN),
    [NL80211_ATTR_FTM_RESPONDER] =
        NLA_POLICY_NESTED(nl80211_ftm_responder_policy),
    [NL80211_ATTR_TIMEOUT] = NLA_POLICY_MIN(NLA_U32, 1),
    [NL80211_ATTR_PEER_MEASUREMENTS] =
        NLA_POLICY_NESTED(nl80211_pmsr_attr_policy),
    [NL80211_ATTR_AIRTIME_WEIGHT] = NLA_POLICY_MIN(NLA_U16, 1),
    [NL80211_ATTR_SAE_PASSWORD] = { .type = NLA_BINARY,
                    .len = SAE_PASSWORD_MAX_LEN },
    [NL80211_ATTR_TWT_RESPONDER] = { .type = NLA_FLAG },
    [NL80211_ATTR_HE_OBSS_PD] = NLA_POLICY_NESTED(he_obss_pd_policy),
    [NL80211_ATTR_VLAN_ID] = NLA_POLICY_RANGE(NLA_U16, 1, 4096 - 2), //VLAN_N_VID
    [NL80211_ATTR_HE_BSS_COLOR] = NLA_POLICY_NESTED(he_bss_color_policy),
    [NL80211_ATTR_TID_CONFIG] =
        NLA_POLICY_NESTED_ARRAY(nl80211_tid_config_attr_policy),
    [NL80211_ATTR_CONTROL_PORT_NO_PREAUTH] = { .type = NLA_FLAG },
    [NL80211_ATTR_PMK_LIFETIME] = NLA_POLICY_MIN(NLA_U32, 1),
    [NL80211_ATTR_PMK_REAUTH_THRESHOLD] = NLA_POLICY_RANGE(NLA_U8, 1, 100),
    [NL80211_ATTR_RECEIVE_MULTICAST] = { .type = NLA_FLAG },
    [NL80211_ATTR_WIPHY_FREQ_OFFSET] = NLA_POLICY_RANGE(NLA_U32, 0, 999),
    [NL80211_ATTR_SCAN_FREQ_KHZ] = { .type = NLA_NESTED },
    [NL80211_ATTR_HE_6GHZ_CAPABILITY] =
        NLA_POLICY_EXACT_LEN(sizeof(struct ieee80211_he_6ghz_capa)),
    [NL80211_ATTR_FILS_DISCOVERY] =
        NLA_POLICY_NESTED(nl80211_fils_discovery_policy),
    [NL80211_ATTR_UNSOL_BCAST_PROBE_RESP] =
        NLA_POLICY_NESTED(nl80211_unsol_bcast_probe_resp_policy),
    [NL80211_ATTR_S1G_CAPABILITY] =
        NLA_POLICY_EXACT_LEN(IEEE80211_S1G_CAPABILITY_LEN),
    [NL80211_ATTR_S1G_CAPABILITY_MASK] =
        NLA_POLICY_EXACT_LEN(IEEE80211_S1G_CAPABILITY_LEN),
    [NL80211_ATTR_SAE_PWE] =
        NLA_POLICY_RANGE(NLA_U8, NL80211_SAE_PWE_HUNT_AND_PECK,
                 NL80211_SAE_PWE_BOTH),
    [NL80211_ATTR_RECONNECT_REQUESTED] = { .type = NLA_REJECT },
    [NL80211_ATTR_SAR_SPEC] = NLA_POLICY_NESTED(sar_policy),
    [NL80211_ATTR_DISABLE_HE] = { .type = NLA_FLAG },
    [NL80211_ATTR_OBSS_COLOR_BITMAP] = { .type = NLA_U64 },
    [NL80211_ATTR_COLOR_CHANGE_COUNT] = { .type = NLA_U8 },
    [NL80211_ATTR_COLOR_CHANGE_COLOR] = { .type = NLA_U8 },
    [NL80211_ATTR_COLOR_CHANGE_ELEMS] = NLA_POLICY_NESTED(test_policy),
    [NL80211_ATTR_MBSSID_CONFIG] =
            NLA_POLICY_NESTED(nl80211_mbssid_config_policy),
    [NL80211_ATTR_MBSSID_ELEMS] = { .type = NLA_NESTED },
    [NL80211_ATTR_RADAR_BACKGROUND] = { .type = NLA_FLAG },
    [NL80211_ATTR_AP_SETTINGS_FLAGS] = { .type = NLA_U32 },
    [NL80211_ATTR_EHT_CAPABILITY] =
        NLA_POLICY_RANGE(NLA_BINARY,
                 NL80211_EHT_MIN_CAPABILITY_LEN,
                 NL80211_EHT_MAX_CAPABILITY_LEN),
    [NL80211_ATTR_DISABLE_EHT] = { .type = NLA_FLAG },
    [NL80211_ATTR_MLO_LINKS] =
        NLA_POLICY_NESTED_ARRAY(test_policy),
    [NL80211_ATTR_MLO_LINK_ID] =
        NLA_POLICY_RANGE(NLA_U8, 0, IEEE80211_MLD_MAX_NUM_LINKS),
};

static struct cfg80211_mbssid_elems *
nl80211_parse_mbssid_elems(struct nlattr *attrs)
{
    struct nlattr *nl_elems;
    struct cfg80211_mbssid_elems *elems;
    int rem_elems;
    u8 i = 0, num_elems = 0;

    nla_for_each_nested(nl_elems, attrs, rem_elems)
        num_elems++;

    elems = kzalloc(struct_size(elems, elem, num_elems), GFP_KERNEL);
    if (!elems)
        return ERR_PTR(-ENOMEM);

    nla_for_each_nested(nl_elems, attrs, rem_elems) {
        elems->elem[i].data = nla_data(nl_elems);
        elems->elem[i].len = nla_len(nl_elems);
        i++;
    }
    elems->cnt = num_elems;
    return elems;
}

static int nl80211_parse_he_bss_color(struct nlattr *attrs,
                      struct cfg80211_he_bss_color *he_bss_color)
{
    struct nlattr *tb[NL80211_HE_BSS_COLOR_ATTR_MAX + 1];
    int err;

    err = nla_parse_nested(tb, NL80211_HE_BSS_COLOR_ATTR_MAX, attrs,
                   he_bss_color_policy, NULL);
    if (err)
        return err;

    if (!tb[NL80211_HE_BSS_COLOR_ATTR_COLOR])
        return -EINVAL;

    he_bss_color->color =
        nla_get_u8(tb[NL80211_HE_BSS_COLOR_ATTR_COLOR]);
    he_bss_color->enabled =
        !nla_get_flag(tb[NL80211_HE_BSS_COLOR_ATTR_DISABLED]);
    he_bss_color->partial =
        nla_get_flag(tb[NL80211_HE_BSS_COLOR_ATTR_PARTIAL]);

    return 0;
}

static int nl80211_parse_beacon(struct nlattr *attrs[], struct cfg80211_beacon_data *bcn)
{
    bool haveinfo = false;
    int err;

    memset(bcn, 0, sizeof(*bcn));

    bcn->link_id = 0;

    if (attrs[NL80211_ATTR_BEACON_HEAD]) {
        bcn->head = nla_data(attrs[NL80211_ATTR_BEACON_HEAD]);
        bcn->head_len = nla_len(attrs[NL80211_ATTR_BEACON_HEAD]);
        if (!bcn->head_len)
            return -EINVAL;
        haveinfo = true;
    }

    if (attrs[NL80211_ATTR_BEACON_TAIL]) {
        bcn->tail = nla_data(attrs[NL80211_ATTR_BEACON_TAIL]);
        bcn->tail_len = nla_len(attrs[NL80211_ATTR_BEACON_TAIL]);
        haveinfo = true;
    }

    if (!haveinfo)
        return -EINVAL;

    if (attrs[NL80211_ATTR_IE]) {
        bcn->beacon_ies = nla_data(attrs[NL80211_ATTR_IE]);
        bcn->beacon_ies_len = nla_len(attrs[NL80211_ATTR_IE]);
    }

    if (attrs[NL80211_ATTR_IE_PROBE_RESP]) {
        bcn->proberesp_ies =
            nla_data(attrs[NL80211_ATTR_IE_PROBE_RESP]);
        bcn->proberesp_ies_len =
            nla_len(attrs[NL80211_ATTR_IE_PROBE_RESP]);
    }

    if (attrs[NL80211_ATTR_IE_ASSOC_RESP]) {
        bcn->assocresp_ies =
            nla_data(attrs[NL80211_ATTR_IE_ASSOC_RESP]);
        bcn->assocresp_ies_len =
            nla_len(attrs[NL80211_ATTR_IE_ASSOC_RESP]);
    }

    if (attrs[NL80211_ATTR_PROBE_RESP]) {
        bcn->probe_resp = nla_data(attrs[NL80211_ATTR_PROBE_RESP]);
        bcn->probe_resp_len = nla_len(attrs[NL80211_ATTR_PROBE_RESP]);
    }

    if (attrs[NL80211_ATTR_FTM_RESPONDER]) {
        struct nlattr *tb[NL80211_FTM_RESP_ATTR_MAX + 1];

        err = nla_parse_nested_deprecated(tb,
                          NL80211_FTM_RESP_ATTR_MAX,
                          attrs[NL80211_ATTR_FTM_RESPONDER],
                          NULL, NULL);
        if (err)
            return err;

        //wiphy_ext_feature_isset(&rdev->wiphy, NL80211_EXT_FEATURE_ENABLE_FTM_RESPONDER))
        if (tb[NL80211_FTM_RESP_ATTR_ENABLED])
            bcn->ftm_responder = 1;
        else
            return -EOPNOTSUPP;

        if (tb[NL80211_FTM_RESP_ATTR_LCI]) {
            bcn->lci = nla_data(tb[NL80211_FTM_RESP_ATTR_LCI]);
            bcn->lci_len = nla_len(tb[NL80211_FTM_RESP_ATTR_LCI]);
        }

        if (tb[NL80211_FTM_RESP_ATTR_CIVICLOC]) {
            bcn->civicloc = nla_data(tb[NL80211_FTM_RESP_ATTR_CIVICLOC]);
            bcn->civicloc_len = nla_len(tb[NL80211_FTM_RESP_ATTR_CIVICLOC]);
        }
    } else {
        bcn->ftm_responder = -1;
    }

    if (attrs[NL80211_ATTR_HE_BSS_COLOR]) {
        err = nl80211_parse_he_bss_color(attrs[NL80211_ATTR_HE_BSS_COLOR],
                         &bcn->he_bss_color);
        if (err)
            return err;
        bcn->he_bss_color_valid = true;
    }

    if (attrs[NL80211_ATTR_MBSSID_ELEMS]) {
        struct cfg80211_mbssid_elems *mbssid =
            nl80211_parse_mbssid_elems(attrs[NL80211_ATTR_MBSSID_ELEMS]);

        if (IS_ERR(mbssid))
            return PTR_ERR(mbssid);

        bcn->mbssid_ies = mbssid;
    }

    return 0;
}

static bool nl80211_valid_wpa_versions(u32 wpa_versions)
{
    return !(wpa_versions & ~(NL80211_WPA_VERSION_1 |
                  NL80211_WPA_VERSION_2 |
                  NL80211_WPA_VERSION_3));
}

static int nl80211_crypto_settings(struct nlattr *tb[], struct cfg80211_crypto_settings *settings, int cipher_limit)
{
    memset(settings, 0, sizeof(*settings));

    settings->control_port = tb[NL80211_ATTR_CONTROL_PORT];

    if (tb[NL80211_ATTR_CONTROL_PORT_ETHERTYPE]) {
        u16 proto;

        proto = nla_get_u16( tb[NL80211_ATTR_CONTROL_PORT_ETHERTYPE]);
        settings->control_port_ethertype = cpu_to_be16(proto);

        if (tb[NL80211_ATTR_CONTROL_PORT_NO_ENCRYPT])
            settings->control_port_no_encrypt = true;
    } else
        settings->control_port_ethertype = cpu_to_be16(ETH_P_PAE);

    if (tb[NL80211_ATTR_CONTROL_PORT_OVER_NL80211]) {
        settings->control_port_over_nl80211 = true;

        if (tb[NL80211_ATTR_CONTROL_PORT_NO_PREAUTH])
            settings->control_port_no_preauth = true;
    }

    if (tb[NL80211_ATTR_CIPHER_SUITES_PAIRWISE]) {
        void *data;
        int len;

        data = nla_data(tb[NL80211_ATTR_CIPHER_SUITES_PAIRWISE]);
        len = nla_len(tb[NL80211_ATTR_CIPHER_SUITES_PAIRWISE]);
        settings->n_ciphers_pairwise = len / sizeof(u32);

        if (len % sizeof(u32))
            return -EINVAL;

        if (settings->n_ciphers_pairwise > cipher_limit)
            return -EINVAL;

        memcpy(settings->ciphers_pairwise, data, len);
    }

    if (tb[NL80211_ATTR_CIPHER_SUITE_GROUP]) {
        settings->cipher_group =
            nla_get_u32(tb[NL80211_ATTR_CIPHER_SUITE_GROUP]);
    }

    if (tb[NL80211_ATTR_WPA_VERSIONS]) {
        settings->wpa_versions = nla_get_u32(tb[NL80211_ATTR_WPA_VERSIONS]);
        if (!nl80211_valid_wpa_versions(settings->wpa_versions))
            return -EINVAL;
    }

    if (tb[NL80211_ATTR_AKM_SUITES]) {
        void *data;
        int len;

        data = nla_data(tb[NL80211_ATTR_AKM_SUITES]);
        len = nla_len(tb[NL80211_ATTR_AKM_SUITES]);
        settings->n_akm_suites = len / sizeof(u32);

        if (len % sizeof(u32))
            return -EINVAL;

        if (settings->n_akm_suites > NL80211_MAX_NR_AKM_SUITES)
            return -EINVAL;

        memcpy(settings->akm_suites, data, len);
    }

    if (tb[NL80211_ATTR_PMK]) {
        if (nla_len(tb[NL80211_ATTR_PMK]) != WLAN_PMK_LEN)
            return -EINVAL;
        settings->psk = nla_data(tb[NL80211_ATTR_PMK]);
    }

    if (tb[NL80211_ATTR_SAE_PASSWORD]) {
        settings->sae_pwd = nla_data(tb[NL80211_ATTR_SAE_PASSWORD]);
        settings->sae_pwd_len = nla_len(tb[NL80211_ATTR_SAE_PASSWORD]);
    }

    if (tb[NL80211_ATTR_SAE_PWE])
        settings->sae_pwe = nla_get_u8(tb[NL80211_ATTR_SAE_PWE]);
    else
        settings->sae_pwe = NL80211_SAE_PWE_UNSPECIFIED;

    return 0;
}

int nl80211_parse_chandef(struct genl_info *info, struct cfg80211_chan_def *chandef)
{
        int freq;
    u32 control_freq;
    struct nlattr **attrs = info->attrs;

    freq = nla_get_u32(info->attrs[NL80211_ATTR_WIPHY_FREQ]);
    control_freq = MHZ_TO_KHZ(freq);
    if (info->attrs[NL80211_ATTR_WIPHY_FREQ_OFFSET])
        control_freq += nla_get_u32(info->attrs[NL80211_ATTR_WIPHY_FREQ_OFFSET]);

    memset(chandef, 0, sizeof(*chandef));
    //XXX get rdev by index?
    //chandef->chan = ieee80211_get_channel_khz(&rdev->wiphy, control_freq);
    chandef->width = NL80211_CHAN_WIDTH_20_NOHT;
    chandef->center_freq1 = KHZ_TO_MHZ(control_freq);
    chandef->freq1_offset = control_freq % 1000;
    chandef->center_freq2 = 0;

    if (attrs[NL80211_ATTR_WIPHY_CHANNEL_TYPE]) {
        enum nl80211_channel_type chantype;

        chantype = nla_get_u32(attrs[NL80211_ATTR_WIPHY_CHANNEL_TYPE]);

        switch (chantype) {
        case NL80211_CHAN_NO_HT:
        case NL80211_CHAN_HT20:
        case NL80211_CHAN_HT40PLUS:
        case NL80211_CHAN_HT40MINUS:
            cfg80211_chandef_create(chandef, chandef->chan, chantype);
            break;
        default:
            return -EINVAL;
        }
    } else if (attrs[NL80211_ATTR_CHANNEL_WIDTH]) {
        chandef->width = nla_get_u32(attrs[NL80211_ATTR_CHANNEL_WIDTH]);

        if (attrs[NL80211_ATTR_CENTER_FREQ1]) {
            chandef->center_freq1 = nla_get_u32(attrs[NL80211_ATTR_CENTER_FREQ1]);
            if (attrs[NL80211_ATTR_CENTER_FREQ1_OFFSET])
                chandef->freq1_offset = nla_get_u32(attrs[NL80211_ATTR_CENTER_FREQ1_OFFSET]);
            else
                chandef->freq1_offset = 0;
        }
        if (attrs[NL80211_ATTR_CENTER_FREQ2])
            chandef->center_freq2 = nla_get_u32(attrs[NL80211_ATTR_CENTER_FREQ2]);
    }

    if (attrs[NL80211_ATTR_WIPHY_EDMG_CHANNELS]) {
        chandef->edmg.channels = nla_get_u8(attrs[NL80211_ATTR_WIPHY_EDMG_CHANNELS]);

        if (attrs[NL80211_ATTR_WIPHY_EDMG_BW_CONFIG])
            chandef->edmg.bw_config = nla_get_u8(attrs[NL80211_ATTR_WIPHY_EDMG_BW_CONFIG]);
    } else {
        chandef->edmg.bw_config = 0;
        chandef->edmg.channels = 0;
    }

    return 0;
}

static int validate_acl_mac_addrs(struct nlattr *nl_attr)
{
    struct nlattr *attr;
    int n_entries = 0, tmp;

    nla_for_each_nested(attr, nl_attr, tmp) {
        if (nla_len(attr) != ETH_ALEN)
            return -EINVAL;

        n_entries++;
    }

    return n_entries;
}

static struct cfg80211_acl_data *parse_acl_data(struct genl_info *info)
{
    enum nl80211_acl_policy acl_policy;
    struct nlattr *attr;
    struct cfg80211_acl_data *acl;
    int i = 0, n_entries, tmp;

    if (!info->attrs[NL80211_ATTR_ACL_POLICY])
        return ERR_PTR(-EINVAL);

    acl_policy = nla_get_u32(info->attrs[NL80211_ATTR_ACL_POLICY]);
    if (acl_policy != NL80211_ACL_POLICY_ACCEPT_UNLESS_LISTED &&
        acl_policy != NL80211_ACL_POLICY_DENY_UNLESS_LISTED)
        return ERR_PTR(-EINVAL);

    if (!info->attrs[NL80211_ATTR_MAC_ADDRS])
        return ERR_PTR(-EINVAL);

    n_entries = validate_acl_mac_addrs(info->attrs[NL80211_ATTR_MAC_ADDRS]);
    if (n_entries < 0)
        return ERR_PTR(n_entries);

    acl = kzalloc(struct_size(acl, mac_addrs, n_entries), GFP_KERNEL);
    if (!acl)
        return ERR_PTR(-ENOMEM);

    nla_for_each_nested(attr, info->attrs[NL80211_ATTR_MAC_ADDRS], tmp) {
        memcpy(acl->mac_addrs[i].addr, nla_data(attr), ETH_ALEN);
        i++;
    }

    acl->n_acl_entries = n_entries;
    acl->acl_policy = acl_policy;

    return acl;
}

static int nl80211_parse_he_obss_pd(struct nlattr *attrs,
                    struct ieee80211_he_obss_pd *he_obss_pd)
{
    struct nlattr *tb[NL80211_HE_OBSS_PD_ATTR_MAX + 1];
    int err;

    err = nla_parse_nested(tb, NL80211_HE_OBSS_PD_ATTR_MAX, attrs,
                   he_obss_pd_policy, NULL);
    if (err)
        return err;

    if (!tb[NL80211_HE_OBSS_PD_ATTR_SR_CTRL])
        return -EINVAL;

    he_obss_pd->sr_ctrl = nla_get_u8(tb[NL80211_HE_OBSS_PD_ATTR_SR_CTRL]);

    if (tb[NL80211_HE_OBSS_PD_ATTR_MIN_OFFSET])
        he_obss_pd->min_offset =
            nla_get_u8(tb[NL80211_HE_OBSS_PD_ATTR_MIN_OFFSET]);
    if (tb[NL80211_HE_OBSS_PD_ATTR_MAX_OFFSET])
        he_obss_pd->max_offset =
            nla_get_u8(tb[NL80211_HE_OBSS_PD_ATTR_MAX_OFFSET]);
    if (tb[NL80211_HE_OBSS_PD_ATTR_NON_SRG_MAX_OFFSET])
        he_obss_pd->non_srg_max_offset =
            nla_get_u8(tb[NL80211_HE_OBSS_PD_ATTR_NON_SRG_MAX_OFFSET]);

    if (he_obss_pd->min_offset > he_obss_pd->max_offset)
        return -EINVAL;

    if (tb[NL80211_HE_OBSS_PD_ATTR_BSS_COLOR_BITMAP])
        memcpy(he_obss_pd->bss_color_bitmap,
               nla_data(tb[NL80211_HE_OBSS_PD_ATTR_BSS_COLOR_BITMAP]),
               sizeof(he_obss_pd->bss_color_bitmap));

    if (tb[NL80211_HE_OBSS_PD_ATTR_PARTIAL_BSSID_BITMAP])
        memcpy(he_obss_pd->partial_bssid_bitmap,
               nla_data(tb[NL80211_HE_OBSS_PD_ATTR_PARTIAL_BSSID_BITMAP]),
               sizeof(he_obss_pd->partial_bssid_bitmap));

    he_obss_pd->enable = true;

    return 0;
}

static int nl80211_parse_fils_discovery(struct nlattr *attrs,
                    struct cfg80211_ap_settings *params)
{
    struct nlattr *tb[NL80211_FILS_DISCOVERY_ATTR_MAX + 1];
    int ret;
    struct cfg80211_fils_discovery *fd = &params->fils_discovery;

    ret = nla_parse_nested(tb, NL80211_FILS_DISCOVERY_ATTR_MAX, attrs,
                   NULL, NULL);
    if (ret)
        return ret;

    if (!tb[NL80211_FILS_DISCOVERY_ATTR_INT_MIN] ||
        !tb[NL80211_FILS_DISCOVERY_ATTR_INT_MAX] ||
        !tb[NL80211_FILS_DISCOVERY_ATTR_TMPL])
        return -EINVAL;

    fd->tmpl_len = nla_len(tb[NL80211_FILS_DISCOVERY_ATTR_TMPL]);
    fd->tmpl = nla_data(tb[NL80211_FILS_DISCOVERY_ATTR_TMPL]);
    fd->min_interval = nla_get_u32(tb[NL80211_FILS_DISCOVERY_ATTR_INT_MIN]);
    fd->max_interval = nla_get_u32(tb[NL80211_FILS_DISCOVERY_ATTR_INT_MAX]);

    return 0;
}

static int
nl80211_parse_unsol_bcast_probe_resp(struct nlattr *attrs,
                     struct cfg80211_ap_settings *params)
{
    struct nlattr *tb[NL80211_UNSOL_BCAST_PROBE_RESP_ATTR_MAX + 1];
    int ret;
    struct cfg80211_unsol_bcast_probe_resp *presp =
                    &params->unsol_bcast_probe_resp;

    ret = nla_parse_nested(tb, NL80211_UNSOL_BCAST_PROBE_RESP_ATTR_MAX,
                   attrs, NULL, NULL);
    if (ret)
        return ret;

    if (!tb[NL80211_UNSOL_BCAST_PROBE_RESP_ATTR_INT] ||
        !tb[NL80211_UNSOL_BCAST_PROBE_RESP_ATTR_TMPL])
        return -EINVAL;

    presp->tmpl = nla_data(tb[NL80211_UNSOL_BCAST_PROBE_RESP_ATTR_TMPL]);
    presp->tmpl_len = nla_len(tb[NL80211_UNSOL_BCAST_PROBE_RESP_ATTR_TMPL]);
    presp->interval = nla_get_u32(tb[NL80211_UNSOL_BCAST_PROBE_RESP_ATTR_INT]);
    return 0;
}

static int nl80211_parse_mbssid_config(struct nlattr *attrs,
                       struct cfg80211_mbssid_config *config,
                       u8 num_elems)
{
    struct nlattr *tb[NL80211_MBSSID_CONFIG_ATTR_MAX + 1];

    if (nla_parse_nested(tb, NL80211_MBSSID_CONFIG_ATTR_MAX, attrs, NULL,
                 NULL) ||
        !tb[NL80211_MBSSID_CONFIG_ATTR_INDEX])
        return -EINVAL;

    config->ema = nla_get_flag(tb[NL80211_MBSSID_CONFIG_ATTR_EMA]);

    config->index = nla_get_u8(tb[NL80211_MBSSID_CONFIG_ATTR_INDEX]);


    return 0;
}

static void nl80211_check_ap_rate_selectors(struct cfg80211_ap_settings *params,
                        const struct element *rates)
{
    int i;

    if (!rates)
        return;

    for (i = 0; i < rates->datalen; i++) {
        if (rates->data[i] == BSS_MEMBERSHIP_SELECTOR_HT_PHY)
            params->ht_required = true;
        if (rates->data[i] == BSS_MEMBERSHIP_SELECTOR_VHT_PHY)
            params->vht_required = true;
        if (rates->data[i] == BSS_MEMBERSHIP_SELECTOR_HE_PHY)
            params->he_required = true;
        if (rates->data[i] == BSS_MEMBERSHIP_SELECTOR_SAE_H2E)
            params->sae_h2e_required = true;
    }
}

static void nl80211_calculate_ap_params(struct cfg80211_ap_settings *params)
{
    const struct cfg80211_beacon_data *bcn = &params->beacon;
    size_t ies_len = bcn->tail_len;
    const u8 *ies = bcn->tail;
    const struct element *rates;
    const struct element *cap;

    rates = cfg80211_find_elem(WLAN_EID_SUPP_RATES, ies, ies_len);
    nl80211_check_ap_rate_selectors(params, rates);

    rates = cfg80211_find_elem(WLAN_EID_EXT_SUPP_RATES, ies, ies_len);
    nl80211_check_ap_rate_selectors(params, rates);

    cap = cfg80211_find_elem(WLAN_EID_HT_CAPABILITY, ies, ies_len);
    if (cap && cap->datalen >= sizeof(*params->ht_cap))
        params->ht_cap = (void *)cap->data;
    cap = cfg80211_find_elem(WLAN_EID_VHT_CAPABILITY, ies, ies_len);
    if (cap && cap->datalen >= sizeof(*params->vht_cap))
        params->vht_cap = (void *)cap->data;
    cap = cfg80211_find_ext_elem(WLAN_EID_EXT_HE_CAPABILITY, ies, ies_len);
    if (cap && cap->datalen >= sizeof(*params->he_cap) + 1)
        params->he_cap = (void *)(cap->data + 1);
    cap = cfg80211_find_ext_elem(WLAN_EID_EXT_HE_OPERATION, ies, ies_len);
    if (cap && cap->datalen >= sizeof(*params->he_oper) + 1)
        params->he_oper = (void *)(cap->data + 1);
}

static void parse_start_ap(struct genl_info *info, struct cfg80211_ap_settings *ap_params)
{
    int  idx = 0;

    if (!info->attrs[NL80211_ATTR_BEACON_INTERVAL] || !info->attrs[NL80211_ATTR_DTIM_PERIOD] || !info->attrs[NL80211_ATTR_BEACON_HEAD])
    return;

    ap_params = kzalloc(sizeof(*ap_params), GFP_KERNEL);
    if (!ap_params)
    return;

    if (info->attrs[NL80211_ATTR_IFINDEX])
    idx = nla_get_u32(info->attrs[NL80211_ATTR_IFINDEX]);

    if (nl80211_parse_beacon(info->attrs, &ap_params->beacon))
    return;

    ap_params->beacon_interval =
    nla_get_u32(info->attrs[NL80211_ATTR_BEACON_INTERVAL]);
    ap_params->dtim_period =
    nla_get_u32(info->attrs[NL80211_ATTR_DTIM_PERIOD]);

    if (info->attrs[NL80211_ATTR_SSID]) {
    ap_params->ssid = nla_data(info->attrs[NL80211_ATTR_SSID]);
    ap_params->ssid_len = nla_len(info->attrs[NL80211_ATTR_SSID]);
    if (ap_params->ssid_len == 0)
        return;
    }

    if (info->attrs[NL80211_ATTR_HIDDEN_SSID])
    ap_params->hidden_ssid = nla_get_u32(info->attrs[NL80211_ATTR_HIDDEN_SSID]);

    ap_params->privacy = !!info->attrs[NL80211_ATTR_PRIVACY];

    if (info->attrs[NL80211_ATTR_AUTH_TYPE]) {
    ap_params->auth_type = nla_get_u32(info->attrs[NL80211_ATTR_AUTH_TYPE]);
    } else
    ap_params->auth_type = NL80211_AUTHTYPE_AUTOMATIC;

    if (nl80211_crypto_settings(info->attrs, &ap_params->crypto, NL80211_MAX_NR_CIPHER_SUITES))
    return;

    if (info->attrs[NL80211_ATTR_INACTIVITY_TIMEOUT])
    ap_params->inactivity_timeout = nla_get_u16(info->attrs[NL80211_ATTR_INACTIVITY_TIMEOUT]);

    if (info->attrs[NL80211_ATTR_P2P_CTWINDOW])
    ap_params->p2p_ctwindow = nla_get_u8(info->attrs[NL80211_ATTR_P2P_CTWINDOW]);

    if (info->attrs[NL80211_ATTR_P2P_OPPPS])
    ap_params->p2p_opp_ps = nla_get_u8(info->attrs[NL80211_ATTR_P2P_OPPPS]);

    if (info->attrs[NL80211_ATTR_WIPHY_FREQ])
    nl80211_parse_chandef(info, &ap_params->chandef);

#if 0
    if (info->attrs[NL80211_ATTR_TX_RATES]) {
    if (nl80211_parse_tx_bitrate_mask(info, info->attrs, NL80211_ATTR_TX_RATES, &params->beacon_rate, dev, false, link_id))
        return;
    }
#endif

    if (info->attrs[NL80211_ATTR_SMPS_MODE])
    ap_params->smps_mode = nla_get_u8(info->attrs[NL80211_ATTR_SMPS_MODE]);
    else
    ap_params->smps_mode = NL80211_SMPS_OFF;

    ap_params->pbss = nla_get_flag(info->attrs[NL80211_ATTR_PBSS]);

    if (info->attrs[NL80211_ATTR_ACL_POLICY])
    ap_params->acl = parse_acl_data(info);

    ap_params->twt_responder =
    nla_get_flag(info->attrs[NL80211_ATTR_TWT_RESPONDER]);

    if (info->attrs[NL80211_ATTR_HE_OBSS_PD])
    nl80211_parse_he_obss_pd(info->attrs[NL80211_ATTR_HE_OBSS_PD], &ap_params->he_obss_pd);

    if (info->attrs[NL80211_ATTR_FILS_DISCOVERY])
    nl80211_parse_fils_discovery(info->attrs[NL80211_ATTR_FILS_DISCOVERY], ap_params);

    if (info->attrs[NL80211_ATTR_UNSOL_BCAST_PROBE_RESP])
    nl80211_parse_unsol_bcast_probe_resp(info->attrs[NL80211_ATTR_UNSOL_BCAST_PROBE_RESP], ap_params);

    if (info->attrs[NL80211_ATTR_MBSSID_CONFIG]) {
    nl80211_parse_mbssid_config(info->attrs[NL80211_ATTR_MBSSID_CONFIG],
        &ap_params->mbssid_config, ap_params->beacon.mbssid_ies ?  ap_params->beacon.mbssid_ies->cnt : 0);
    }

    nl80211_calculate_ap_params(ap_params);

    if (info->attrs[NL80211_ATTR_AP_SETTINGS_FLAGS])
    ap_params->flags = nla_get_u32(info->attrs[NL80211_ATTR_AP_SETTINGS_FLAGS]);
    else if (info->attrs[NL80211_ATTR_EXTERNAL_AUTH_SUPPORT])
    ap_params->flags |= NL80211_AP_SETTINGS_EXTERNAL_AUTH_SUPPORT;

    printk("\t\tReceived start ap idx: %d ssid: %s width: %d freq1: %d head_len: %d tail_len: %d\n", idx, ap_params->ssid ? (char *)(ap_params->ssid) : "NA",
    ap_params->chandef.width, ap_params->chandef.center_freq1, ap_params->beacon.head_len, ap_params->beacon.tail_len);
}

static netdev_tx_t nlmon_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct nlmsghdr *nlh;
    struct genlmsghdr *gnlh;
    struct nlattr *tb[NL80211_ATTR_MAX + 1];
    int err;
    struct cfg80211_ap_settings *ap_params;
    struct genl_info info = {};

    nlh = nlmsg_hdr(skb);
    gnlh = nlmsg_data(nlh);

    if ((ntohs(skb->protocol) != NETLINK_GENERIC) || (nlh->nlmsg_type < NLMSG_MIN_TYPE))
        goto exit;

    printk("Nlmon xmit nl_msg len: %d msg_type: %d proto: %d cmd: %d\n", nlh->nlmsg_len, nlh->nlmsg_type, ntohs(skb->protocol), gnlh->cmd);

    err = nlmsg_parse_deprecated(nlh, GENL_HDRLEN, tb, NL80211_ATTR_MAX, test_policy, NULL);

    if (err) {
        printk("\t\tFailed to parse genl\n");
        goto exit;
    }

    info.attrs = tb;

    if (gnlh->cmd== NL80211_CMD_START_AP)
        parse_start_ap(&info, ap_params);

exit:
    dev_lstats_add(dev, skb->len);
    dev_kfree_skb(skb);

    return NETDEV_TX_OK;
}

static int nlmon_dev_init(struct net_device *dev)
{
    dev->lstats = netdev_alloc_pcpu_stats(struct pcpu_lstats);
    return dev->lstats == NULL ? -ENOMEM : 0;
}

static void nlmon_dev_uninit(struct net_device *dev)
{
    free_percpu(dev->lstats);
}

struct nlmon {
    struct netlink_tap nt;
};

static int nlmon_open(struct net_device *dev)
{
    struct nlmon *nlmon = netdev_priv(dev);

    nlmon->nt.dev = dev;
    nlmon->nt.module = THIS_MODULE;
    return netlink_add_tap(&nlmon->nt);
}

static int nlmon_close(struct net_device *dev)
{
    struct nlmon *nlmon = netdev_priv(dev);

    return netlink_remove_tap(&nlmon->nt);
}

static void
nlmon_get_stats64(struct net_device *dev, struct rtnl_link_stats64 *stats)
{
    u64 packets, bytes;

    printk("Nlmon get stats\n");

    dev_lstats_read(dev, &packets, &bytes);

    stats->rx_packets = packets;
    stats->tx_packets = 0;

    stats->rx_bytes = bytes;
    stats->tx_bytes = 0;
}


static u32 always_on(struct net_device *dev)
{
    return 1;
}

static const struct ethtool_ops nl_msg_mon_ethtool_ops = {
    .get_link = always_on,
};

static const struct net_device_ops nl_msg_mon_ops = {
    .ndo_init = nlmon_dev_init,
    .ndo_uninit = nlmon_dev_uninit,
    .ndo_open = nlmon_open,
    .ndo_stop = nlmon_close,
    .ndo_start_xmit = nlmon_xmit,
    .ndo_get_stats64 = nlmon_get_stats64,
};

static void nl_msg_mon_setup(struct net_device *dev)
{
    dev->type = ARPHRD_NETLINK;
    dev->priv_flags |= IFF_NO_QUEUE;

    dev->netdev_ops = &nl_msg_mon_ops;
    dev->ethtool_ops = &nl_msg_mon_ethtool_ops;
    dev->needs_free_netdev = true;

    dev->features = NETIF_F_SG | NETIF_F_FRAGLIST |
            NETIF_F_HIGHDMA | NETIF_F_LLTX;
    dev->flags = IFF_NOARP;

    dev->mtu = NLMSG_GOODSIZE;
    dev->min_mtu = sizeof(struct nlmsghdr);
}

static struct mac80211_rdkfmac_data *get_hwsim_data_ref_from_addr(const u8 *addr)
{
    return rhashtable_lookup_fast(&hwsim_radios_rht,
                      addr,
                      hwsim_rht_params);
}

static void hwsim_register_wmediumd(struct net *net, u32 portid)
{
    struct mac80211_rdkfmac_data *data;

    hwsim_net_set_wmediumd(net, portid);

    spin_lock_bh(&hwsim_radio_lock);
    list_for_each_entry(data, &hwsim_radios, list) {
        if (data->netgroup == hwsim_net_get_netgroup(net))
            data->wmediumd = portid;
    }
    spin_unlock_bh(&hwsim_radio_lock);
}

static int hwsim_tx_info_frame_received_nl(struct sk_buff *skb_2,
                       struct genl_info *info)
{

    struct ieee80211_hdr *hdr;
    struct mac80211_rdkfmac_data *data2;
    struct ieee80211_tx_info *txi;
    struct hwsim_tx_rate *tx_attempts;
    u64 ret_skb_cookie;
    struct sk_buff *skb, *tmp;
    const u8 *src;
    unsigned int hwsim_flags;
    int i;
    unsigned long flags;
    bool found = false;

    if (!info->attrs[HWSIM_ATTR_ADDR_TRANSMITTER] ||
        !info->attrs[HWSIM_ATTR_FLAGS] ||
        !info->attrs[HWSIM_ATTR_COOKIE] ||
        !info->attrs[HWSIM_ATTR_SIGNAL] ||
        !info->attrs[HWSIM_ATTR_TX_INFO])
        goto out;

    src = (void *)nla_data(info->attrs[HWSIM_ATTR_ADDR_TRANSMITTER]);
    hwsim_flags = nla_get_u32(info->attrs[HWSIM_ATTR_FLAGS]);
    ret_skb_cookie = nla_get_u64(info->attrs[HWSIM_ATTR_COOKIE]);

    data2 = get_hwsim_data_ref_from_addr(src);
    if (!data2)
        goto out;

    if (hwsim_net_get_netgroup(genl_info_net(info)) !=
        data2->netgroup)
        goto out;

    if (info->snd_portid != data2->wmediumd)
        goto out;

    /* look for the skb matching the cookie passed back from user */
    spin_lock_irqsave(&data2->pending.lock, flags);
    skb_queue_walk_safe(&data2->pending, skb, tmp) {
        uintptr_t skb_cookie;

        txi = IEEE80211_SKB_CB(skb);
        skb_cookie = (uintptr_t)txi->rate_driver_data[0];

        if (skb_cookie == ret_skb_cookie) {
            __skb_unlink(skb, &data2->pending);
            found = true;
            break;
        }
    }
    spin_unlock_irqrestore(&data2->pending.lock, flags);

    /* not found */
    if (!found)
        goto out;

    /* Tx info received because the frame was broadcasted on user space,
     so we get all the necessary info: tx attempts and skb control buff */

    tx_attempts = (struct hwsim_tx_rate *)nla_data(
               info->attrs[HWSIM_ATTR_TX_INFO]);

    /* now send back TX status */
    txi = IEEE80211_SKB_CB(skb);

    ieee80211_tx_info_clear_status(txi);

    for (i = 0; i < IEEE80211_TX_MAX_RATES; i++) {
        txi->status.rates[i].idx = tx_attempts[i].idx;
        txi->status.rates[i].count = tx_attempts[i].count;
    }

    txi->status.ack_signal = nla_get_u32(info->attrs[HWSIM_ATTR_SIGNAL]);

    if (!(hwsim_flags & HWSIM_TX_CTL_NO_ACK) &&
       (hwsim_flags & HWSIM_TX_STAT_ACK)) {
        if (skb->len >= 16) {
            hdr = (struct ieee80211_hdr *) skb->data;
            mac80211_hwsim_monitor_ack(data2->channel,
                           hdr->addr2);
        }
        txi->flags |= IEEE80211_TX_STAT_ACK;
    }

    if (hwsim_flags & HWSIM_TX_CTL_NO_ACK)
        txi->flags |= IEEE80211_TX_STAT_NOACK_TRANSMITTED;

    ieee80211_tx_status_irqsafe(data2->hw, skb);
    return 0;
out:
    return -EINVAL;

}

static int hwsim_cloned_frame_received_nl(struct sk_buff *skb_2,
                      struct genl_info *info)
{
    struct mac80211_rdkfmac_data *data2;
    struct ieee80211_rx_status rx_status;
    struct ieee80211_hdr *hdr;
    const u8 *dst;
    int frame_data_len;
    void *frame_data;
    struct sk_buff *skb = NULL;
    struct ieee80211_channel *channel = NULL;

    if (!info->attrs[HWSIM_ATTR_ADDR_RECEIVER] ||
        !info->attrs[HWSIM_ATTR_FRAME] ||
        !info->attrs[HWSIM_ATTR_RX_RATE] ||
        !info->attrs[HWSIM_ATTR_SIGNAL])
        goto out;

    dst = (void *)nla_data(info->attrs[HWSIM_ATTR_ADDR_RECEIVER]);
    frame_data_len = nla_len(info->attrs[HWSIM_ATTR_FRAME]);
    frame_data = (void *)nla_data(info->attrs[HWSIM_ATTR_FRAME]);

    /* Allocate new skb here */
    skb = alloc_skb(frame_data_len, GFP_KERNEL);
    if (skb == NULL)
        goto err;

    if (frame_data_len > IEEE80211_MAX_DATA_LEN)
        goto err;

    /* Copy the data */
    skb_put_data(skb, frame_data, frame_data_len);

    data2 = get_hwsim_data_ref_from_addr(dst);
    if (!data2)
        goto out;

    if (data2->use_chanctx) {
        if (data2->tmp_chan)
            channel = data2->tmp_chan;
        else if (data2->chanctx)
            channel = data2->chanctx->def.chan;
    } else {
        channel = data2->channel;
    }
    if (!channel)
        goto out;

    if (hwsim_net_get_netgroup(genl_info_net(info)) !=
        data2->netgroup)
        goto out;

    if (info->snd_portid != data2->wmediumd)
        goto out;

    /* check if radio is configured properly */

    if ((data2->idle && !data2->tmp_chan) || !data2->started)
        goto out;

    /* A frame is received from user space */
    memset(&rx_status, 0, sizeof(rx_status));
    if (info->attrs[HWSIM_ATTR_FREQ]) {
        /* throw away off-channel packets, but allow both the temporary
         * ("hw" scan/remain-on-channel) and regular channel, since the
         * internal datapath also allows this
         */
        mutex_lock(&data2->mutex);
        rx_status.freq = nla_get_u32(info->attrs[HWSIM_ATTR_FREQ]);

        if (rx_status.freq != channel->center_freq) {
            mutex_unlock(&data2->mutex);
            goto out;
        }
        mutex_unlock(&data2->mutex);
    } else {
        rx_status.freq = channel->center_freq;
    }

    rx_status.band = channel->band;
    rx_status.rate_idx = nla_get_u32(info->attrs[HWSIM_ATTR_RX_RATE]);
    if (rx_status.rate_idx >= data2->hw->wiphy->bands[rx_status.band]->n_bitrates)
        goto out;
    rx_status.signal = nla_get_u32(info->attrs[HWSIM_ATTR_SIGNAL]);

    hdr = (void *)skb->data;

    if (ieee80211_is_beacon(hdr->frame_control) ||
        ieee80211_is_probe_resp(hdr->frame_control))
        rx_status.boottime_ns = ktime_get_boottime_ns();

    memcpy(IEEE80211_SKB_RXCB(skb), &rx_status, sizeof(rx_status));
    data2->rx_pkts++;
    data2->rx_bytes += skb->len;
    ieee80211_rx_irqsafe(data2->hw, skb);

    return 0;
err:
    pr_debug("mac80211_hwsim: error occurred in %s\n", __func__);
out:
    dev_kfree_skb(skb);
    return -EINVAL;
}

static int hwsim_register_received_nl(struct sk_buff *skb_2,
                      struct genl_info *info)
{
    struct net *net = genl_info_net(info);
    struct mac80211_rdkfmac_data *data;
    int chans = 1;

    spin_lock_bh(&hwsim_radio_lock);
    list_for_each_entry(data, &hwsim_radios, list)
        chans = max(chans, data->channels);
    spin_unlock_bh(&hwsim_radio_lock);

    /* In the future we should revise the userspace API and allow it
     * to set a flag that it does support multi-channel, then we can
     * let this pass conditionally on the flag.
     * For current userspace, prohibit it since it won't work right.
     */
    if (chans > 1)
        return -EOPNOTSUPP;

    if (hwsim_net_get_wmediumd(net))
        return -EBUSY;

    hwsim_register_wmediumd(net, info->snd_portid);

    pr_debug("mac80211_hwsim: received a REGISTER, "
           "switching to wmediumd mode with pid %d\n", info->snd_portid);

    return 0;
}

/* ensures ciphers only include ciphers listed in 'hwsim_ciphers' array */
static bool hwsim_known_ciphers(const u32 *ciphers, int n_ciphers)
{
    int i;

    for (i = 0; i < n_ciphers; i++) {
        int j;
        int found = 0;

        for (j = 0; j < ARRAY_SIZE(hwsim_ciphers); j++) {
            if (ciphers[i] == hwsim_ciphers[j]) {
                found = 1;
                break;
            }
        }

        if (!found)
            return false;
    }

    return true;
}

static int hwsim_new_radio_nl(struct sk_buff *msg, struct genl_info *info)
{
    struct hwsim_new_radio_params param = { 0 };
    const char *hwname = NULL;
    int ret;

    param.reg_strict = info->attrs[HWSIM_ATTR_REG_STRICT_REG];
    param.p2p_device = info->attrs[HWSIM_ATTR_SUPPORT_P2P_DEVICE];
    param.channels = 1;
    param.destroy_on_close =
        info->attrs[HWSIM_ATTR_DESTROY_RADIO_ON_CLOSE];

    if (info->attrs[HWSIM_ATTR_CHANNELS])
        param.channels = nla_get_u32(info->attrs[HWSIM_ATTR_CHANNELS]);

    if (param.channels < 1) {
        GENL_SET_ERR_MSG(info, "must have at least one channel");
        return -EINVAL;
    }

    if (info->attrs[HWSIM_ATTR_NO_VIF])
        param.no_vif = true;

    if (info->attrs[HWSIM_ATTR_USE_CHANCTX])
        param.use_chanctx = true;
    else
        param.use_chanctx = (param.channels > 1);

    if (info->attrs[HWSIM_ATTR_REG_HINT_ALPHA2])
        param.reg_alpha2 =
            nla_data(info->attrs[HWSIM_ATTR_REG_HINT_ALPHA2]);

    if (info->attrs[HWSIM_ATTR_REG_CUSTOM_REG]) {
        u32 idx = nla_get_u32(info->attrs[HWSIM_ATTR_REG_CUSTOM_REG]);

        if (idx >= ARRAY_SIZE(hwsim_world_regdom_custom))
            return -EINVAL;

        idx = array_index_nospec(idx,
                     ARRAY_SIZE(hwsim_world_regdom_custom));
        param.regd = hwsim_world_regdom_custom[idx];
    }

    if (info->attrs[HWSIM_ATTR_PERM_ADDR]) {
        if (!is_valid_ether_addr(
                nla_data(info->attrs[HWSIM_ATTR_PERM_ADDR]))) {
            GENL_SET_ERR_MSG(info,"MAC is no valid source addr");
            NL_SET_BAD_ATTR(info->extack,
                    info->attrs[HWSIM_ATTR_PERM_ADDR]);
            return -EINVAL;
        }

        param.perm_addr = nla_data(info->attrs[HWSIM_ATTR_PERM_ADDR]);
    }

    if (info->attrs[HWSIM_ATTR_IFTYPE_SUPPORT]) {
        param.iftypes =
            nla_get_u32(info->attrs[HWSIM_ATTR_IFTYPE_SUPPORT]);

        if (param.iftypes & ~HWSIM_IFTYPE_SUPPORT_MASK) {
            NL_SET_ERR_MSG_ATTR(info->extack,
                        info->attrs[HWSIM_ATTR_IFTYPE_SUPPORT],
                        "cannot support more iftypes than kernel");
            return -EINVAL;
        }
    } else {
        param.iftypes = HWSIM_IFTYPE_SUPPORT_MASK;
    }

    /* ensure both flag and iftype support is honored */
    if (param.p2p_device ||
        param.iftypes & BIT(NL80211_IFTYPE_P2P_DEVICE)) {
        param.iftypes |= BIT(NL80211_IFTYPE_P2P_DEVICE);
        param.p2p_device = true;
    }

    if (info->attrs[HWSIM_ATTR_CIPHER_SUPPORT]) {
        u32 len = nla_len(info->attrs[HWSIM_ATTR_CIPHER_SUPPORT]);

        param.ciphers =
            nla_data(info->attrs[HWSIM_ATTR_CIPHER_SUPPORT]);

        if (len % sizeof(u32)) {
            NL_SET_ERR_MSG_ATTR(info->extack,
                        info->attrs[HWSIM_ATTR_CIPHER_SUPPORT],
                        "bad cipher list length");
            return -EINVAL;
        }

        param.n_ciphers = len / sizeof(u32);

        if (param.n_ciphers > ARRAY_SIZE(hwsim_ciphers)) {
            NL_SET_ERR_MSG_ATTR(info->extack,
                        info->attrs[HWSIM_ATTR_CIPHER_SUPPORT],
                        "too many ciphers specified");
            return -EINVAL;
        }

        if (!hwsim_known_ciphers(param.ciphers, param.n_ciphers)) {
            NL_SET_ERR_MSG_ATTR(info->extack,
                        info->attrs[HWSIM_ATTR_CIPHER_SUPPORT],
                        "unsupported ciphers specified");
            return -EINVAL;
        }
    }

    if (info->attrs[HWSIM_ATTR_RADIO_NAME]) {
        hwname = kstrndup((char *)nla_data(info->attrs[HWSIM_ATTR_RADIO_NAME]),
                  nla_len(info->attrs[HWSIM_ATTR_RADIO_NAME]),
                  GFP_KERNEL);
        if (!hwname)
            return -ENOMEM;
        param.hwname = hwname;
    }

    param.use_chanctx = 0;
    param.channels = 1;

    ret = mac80211_hwsim_new_radio(info, &param);
    kfree(hwname);
    return ret;
}

static int hwsim_del_radio_nl(struct sk_buff *msg, struct genl_info *info)
{
    struct mac80211_rdkfmac_data *data;
    s64 idx = -1;
    const char *hwname = NULL;

    if (info->attrs[HWSIM_ATTR_RADIO_ID]) {
        idx = nla_get_u32(info->attrs[HWSIM_ATTR_RADIO_ID]);
    } else if (info->attrs[HWSIM_ATTR_RADIO_NAME]) {
        hwname = kstrndup((char *)nla_data(info->attrs[HWSIM_ATTR_RADIO_NAME]),
                  nla_len(info->attrs[HWSIM_ATTR_RADIO_NAME]),
                  GFP_KERNEL);
        if (!hwname)
            return -ENOMEM;
    } else
        return -EINVAL;

    spin_lock_bh(&hwsim_radio_lock);
    list_for_each_entry(data, &hwsim_radios, list) {
        if (idx >= 0) {
            if (data->idx != idx)
                continue;
        } else {
            if (!hwname ||
                strcmp(hwname, wiphy_name(data->hw->wiphy)))
                continue;
        }

        if (!net_eq(wiphy_net(data->hw->wiphy), genl_info_net(info)))
            continue;

        list_del(&data->list);
        rhashtable_remove_fast(&hwsim_radios_rht, &data->rht,
                       hwsim_rht_params);
        hwsim_radios_generation++;
        spin_unlock_bh(&hwsim_radio_lock);
        mac80211_hwsim_del_radio(data, wiphy_name(data->hw->wiphy),
                     info);
        kfree(hwname);
        return 0;
    }
    spin_unlock_bh(&hwsim_radio_lock);

    kfree(hwname);
    return -ENODEV;
}

static int hwsim_get_radio_nl(struct sk_buff *msg, struct genl_info *info)
{
    struct mac80211_rdkfmac_data *data;
    struct sk_buff *skb;
    int idx, res = -ENODEV;

    if (!info->attrs[HWSIM_ATTR_RADIO_ID])
        return -EINVAL;
    idx = nla_get_u32(info->attrs[HWSIM_ATTR_RADIO_ID]);

    spin_lock_bh(&hwsim_radio_lock);
    list_for_each_entry(data, &hwsim_radios, list) {
        if (data->idx != idx)
            continue;

        if (!net_eq(wiphy_net(data->hw->wiphy), genl_info_net(info)))
            continue;

        skb = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_ATOMIC);
        if (!skb) {
            res = -ENOMEM;
            goto out_err;
        }

        res = mac80211_hwsim_get_radio(skb, data, info->snd_portid,
                           info->snd_seq, NULL, 0);
        if (res < 0) {
            nlmsg_free(skb);
            goto out_err;
        }

        res = genlmsg_reply(skb, info);
        break;
    }

out_err:
    spin_unlock_bh(&hwsim_radio_lock);

    return res;
}

static int hwsim_dump_radio_nl(struct sk_buff *skb,
                   struct netlink_callback *cb)
{
    int last_idx = cb->args[0] - 1;
    struct mac80211_rdkfmac_data *data = NULL;
    int res = 0;
    void *hdr;

    spin_lock_bh(&hwsim_radio_lock);
    cb->seq = hwsim_radios_generation;

    if (last_idx >= hwsim_radio_idx-1)
        goto done;

    list_for_each_entry(data, &hwsim_radios, list) {
        if (data->idx <= last_idx)
            continue;

        if (!net_eq(wiphy_net(data->hw->wiphy), sock_net(skb->sk)))
            continue;

        res = mac80211_hwsim_get_radio(skb, data,
                           NETLINK_CB(cb->skb).portid,
                           cb->nlh->nlmsg_seq, cb,
                           NLM_F_MULTI);
        if (res < 0)
            break;

        last_idx = data->idx;
    }

    cb->args[0] = last_idx + 1;

    /* list changed, but no new element sent, set interrupted flag */
    if (skb->len == 0 && cb->prev_seq && cb->seq != cb->prev_seq) {
        hdr = genlmsg_put(skb, NETLINK_CB(cb->skb).portid,
                  cb->nlh->nlmsg_seq, &hwsim_genl_family,
                  NLM_F_MULTI, HWSIM_CMD_GET_RADIO);
        if (hdr) {
            genl_dump_check_consistent(cb, hdr);
            genlmsg_end(skb, hdr);
        } else {
            res = -EMSGSIZE;
        }
    }

done:
    spin_unlock_bh(&hwsim_radio_lock);
    return res ?: skb->len;
}

/* Generic Netlink operations array */
static const struct genl_small_ops hwsim_ops[] = {
    {
        .cmd = HWSIM_CMD_REGISTER,
        .validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
        .doit = hwsim_register_received_nl,
        .flags = GENL_UNS_ADMIN_PERM,
    },
    {
        .cmd = HWSIM_CMD_FRAME,
        .validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
        .doit = hwsim_cloned_frame_received_nl,
    },
    {
        .cmd = HWSIM_CMD_TX_INFO_FRAME,
        .validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
        .doit = hwsim_tx_info_frame_received_nl,
    },
    {
        .cmd = HWSIM_CMD_NEW_RADIO,
        .validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
        .doit = hwsim_new_radio_nl,
        .flags = GENL_UNS_ADMIN_PERM,
    },
    {
        .cmd = HWSIM_CMD_DEL_RADIO,
        .validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
        .doit = hwsim_del_radio_nl,
        .flags = GENL_UNS_ADMIN_PERM,
    },
    {
        .cmd = HWSIM_CMD_GET_RADIO,
        .validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
        .doit = hwsim_get_radio_nl,
        .dumpit = hwsim_dump_radio_nl,
    },
};

static struct genl_family hwsim_genl_family __ro_after_init = {
    .name = "MAC80211_HWSIM",
    .version = 1,
    .maxattr = HWSIM_ATTR_MAX,
    .policy = hwsim_genl_policy,
    .netnsok = true,
    .module = THIS_MODULE,
    .small_ops = hwsim_ops,
    .n_small_ops = ARRAY_SIZE(hwsim_ops),
    .mcgrps = hwsim_mcgrps,
    .n_mcgrps = ARRAY_SIZE(hwsim_mcgrps),
};

static void remove_user_radios(u32 portid)
{
    struct mac80211_rdkfmac_data *entry, *tmp;
    LIST_HEAD(list);

    spin_lock_bh(&hwsim_radio_lock);
    list_for_each_entry_safe(entry, tmp, &hwsim_radios, list) {
        if (entry->destroy_on_close && entry->portid == portid) {
            list_move(&entry->list, &list);
            rhashtable_remove_fast(&hwsim_radios_rht, &entry->rht,
                           hwsim_rht_params);
            hwsim_radios_generation++;
        }
    }
    spin_unlock_bh(&hwsim_radio_lock);

    list_for_each_entry_safe(entry, tmp, &list, list) {
        list_del(&entry->list);
        mac80211_hwsim_del_radio(entry, wiphy_name(entry->hw->wiphy),
                     NULL);
    }
}

static int mac80211_hwsim_netlink_notify(struct notifier_block *nb,
                     unsigned long state,
                     void *_notify)
{
    struct netlink_notify *notify = _notify;

    if (state != NETLINK_URELEASE)
        return NOTIFY_DONE;

    remove_user_radios(notify->portid);

    if (notify->portid == hwsim_net_get_wmediumd(notify->net)) {
        printk(KERN_INFO "mac80211_hwsim: wmediumd released netlink"
               " socket, switching to perfect channel medium\n");
        hwsim_register_wmediumd(notify->net, 0);
    }
    return NOTIFY_DONE;

}

static struct notifier_block hwsim_netlink_notifier = {
    .notifier_call = mac80211_hwsim_netlink_notify,
};

static int __init hwsim_init_netlink(void)
{
    int rc;

    printk(KERN_INFO "mac80211_hwsim: initializing netlink\n");

    rc = genl_register_family(&hwsim_genl_family);
    if (rc)
        goto failure;

    rc = netlink_register_notifier(&hwsim_netlink_notifier);
    if (rc) {
        genl_unregister_family(&hwsim_genl_family);
        goto failure;
    }

    return 0;

failure:
    pr_debug("mac80211_hwsim: error occurred in %s\n", __func__);
    return -EINVAL;
}

static __net_init int hwsim_init_net(struct net *net)
{
    return hwsim_net_set_netgroup(net);
}

static void __net_exit hwsim_exit_net(struct net *net)
{
    struct mac80211_rdkfmac_data *data, *tmp;
    LIST_HEAD(list);

    spin_lock_bh(&hwsim_radio_lock);
    list_for_each_entry_safe(data, tmp, &hwsim_radios, list) {
        if (!net_eq(wiphy_net(data->hw->wiphy), net))
            continue;

        if (data->netgroup == hwsim_net_get_netgroup(&init_net))
            continue;

        list_move(&data->list, &list);
        rhashtable_remove_fast(&hwsim_radios_rht, &data->rht,
                       hwsim_rht_params);
        hwsim_radios_generation++;
    }
    spin_unlock_bh(&hwsim_radio_lock);

    list_for_each_entry_safe(data, tmp, &list, list) {
        list_del(&data->list);
        mac80211_hwsim_del_radio(data,
                     wiphy_name(data->hw->wiphy),
                     NULL);
    }

    ida_simple_remove(&hwsim_netgroup_ida, hwsim_net_get_netgroup(net));
}

static struct pernet_operations hwsim_net_ops = {
    .init = hwsim_init_net,
    .exit = hwsim_exit_net,
    .id   = &hwsim_net_id,
    .size = sizeof(struct hwsim_net),
};

static void hwsim_exit_netlink(void)
{
    /* unregister the notifier */
    netlink_unregister_notifier(&hwsim_netlink_notifier);
    /* unregister the family */
    genl_unregister_family(&hwsim_genl_family);
}

static int __init rdkfmac_init_module(void)
{
    int err;
    struct hwsim_new_radio_params param = { 0 };

    err = rhashtable_init(&hwsim_radios_rht, &hwsim_rht_params);
    if (err)
        return err;

    err = register_pernet_device(&hwsim_net_ops);
    if (err)
        goto out_free_rht;

    err = platform_driver_register(&mac80211_hwsim_driver);
    if (err)
        goto out_unregister_pernet;

    err = hwsim_init_netlink();
    if (err)
        goto out_unregister_driver;

    hwsim_class = class_create(THIS_MODULE, "mac80211_hwsim");
    if (IS_ERR(hwsim_class)) {
        err = PTR_ERR(hwsim_class);
        goto out_exit_netlink;
    }

    hwsim_init_s1g_channels(rdkfmac_channels_s1g);


    err = mac80211_hwsim_new_radio(NULL, &param);
    if (err < 0)
        goto out_free_radios;
printk("Radio 1 create\n");
    err = mac80211_hwsim_new_radio(NULL, &param);
    if (err < 0)
        goto out_free_radios;
printk("Radio 2 create\n");

    hwsim_mon = alloc_netdev(0, "hwsim%d", NET_NAME_UNKNOWN,
                 hwsim_mon_setup);
    if (hwsim_mon == NULL) {
        err = -ENOMEM;
        goto out_free_radios;
    }

    rtnl_lock();
    err = dev_alloc_name(hwsim_mon, hwsim_mon->name);
    if (err < 0) {
        rtnl_unlock();
        goto out_free_mon;
    }

    err = register_netdevice(hwsim_mon);
    if (err < 0) {
        rtnl_unlock();
        goto out_free_mon;
    }
    rtnl_unlock();

    ieee80211_iface_init();

    //XXX no uninit
    init_rdkfmac_cdev();

    nl_msg_mon = alloc_netdev(0, "nl_msg_mon%d", NET_NAME_UNKNOWN,
                    nl_msg_mon_setup);

    if (nl_msg_mon == NULL) {
        err = -ENOMEM;
        goto out_free_nl_msg_mon;
    }

    rtnl_lock();
    err = dev_alloc_name(nl_msg_mon, nl_msg_mon->name);
    if (err < 0) {
        rtnl_unlock();
        goto out_free_nl_msg_mon;
    }

    err = register_netdevice(nl_msg_mon);
    if (err < 0) {
        rtnl_unlock();
        goto out_free_nl_msg_mon;
    }
    rtnl_unlock();

    return 0;

out_free_nl_msg_mon:
    free_netdev(nl_msg_mon);
out_free_mon:
    free_netdev(hwsim_mon);
out_free_radios:
    mac80211_hwsim_free();
out_exit_netlink:
    hwsim_exit_netlink();
out_unregister_driver:
    platform_driver_unregister(&mac80211_hwsim_driver);
out_unregister_pernet:
    unregister_pernet_device(&hwsim_net_ops);
out_free_rht:
    rhashtable_destroy(&hwsim_radios_rht);
    return err;
}

static void __exit rdkfmac_cleanup_module(void)
{
    hwsim_exit_netlink();

    mac80211_hwsim_free();

    rhashtable_destroy(&hwsim_radios_rht);
    unregister_netdev(hwsim_mon);
    platform_driver_unregister(&mac80211_hwsim_driver);
    unregister_pernet_device(&hwsim_net_ops);

    printk("%s:%d Unloading module: %s success\n", __func__, __LINE__, NETDEV_DRV_NAME);
}

module_init(rdkfmac_init_module);
module_exit(rdkfmac_cleanup_module);
MODULE_LICENSE("GPL");
MODULE_ALIAS_RTNL_LINK(NETDEV_DRV_NAME);
