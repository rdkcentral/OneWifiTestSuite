#ifndef __NET_CFG80211_H
#define __NET_CFG80211_H

#include "nl80211_copy.h"
#include <stdint.h>
#include <linux/types.h>
#include "android_vendor_copy.h"

#define CFG80211_MAX_NUM_AKM_SUITES 10
#define IEEE80211_HT_MCS_MASK_LEN 10
#define ETH_ALEN 6

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef uint16_t u16;

struct mac_address {
    u8 addr[ETH_ALEN];
};

struct ieee80211_he_obss_pd {
    bool enable;
    u8 sr_ctrl;
    u8 non_srg_max_offset;
    u8 min_offset;
    u8 max_offset;
    u8 bss_color_bitmap[8];
    u8 partial_bssid_bitmap[8];
};

struct cfg80211_fils_discovery {
    u32 min_interval;
    u32 max_interval;
    size_t tmpl_len;
    const u8 *tmpl;
};

struct cfg80211_mbssid_config {
    struct wireless_dev *tx_wdev;
    u8 index;
    bool ema;
};

struct cfg80211_unsol_bcast_probe_resp {
    u32 interval;
    size_t tmpl_len;
    const u8 *tmpl;
};

/*
 * cfg80211_bitrate_mask - masks for bitrate control
 */
struct cfg80211_bitrate_mask {
    struct {
        u32 legacy;
        u8 ht_mcs[IEEE80211_HT_MCS_MASK_LEN];
        u16 vht_mcs[NL80211_VHT_NSS_MAX];
        u16 he_mcs[NL80211_HE_NSS_MAX];
        enum nl80211_txrate_gi gi;
        enum nl80211_he_gi he_gi;
        enum nl80211_he_ltf he_ltf;
    } control[NUM_NL80211_BANDS];
};

struct cfg80211_crypto_settings {
    u32 wpa_versions;
    u32 cipher_group;
    int n_ciphers_pairwise;
    u32 ciphers_pairwise[NL80211_MAX_NR_CIPHER_SUITES];
    int n_akm_suites;
    u32 akm_suites[CFG80211_MAX_NUM_AKM_SUITES];
    bool control_port;
    __be16 control_port_ethertype;
    bool control_port_no_encrypt;
    bool control_port_over_nl80211;
    bool control_port_no_preauth;
    struct key_params *wep_keys;
    int wep_tx_key;
    const u8 *psk;
    const u8 *sae_pwd;
    u8 sae_pwd_len;
    enum nl80211_sae_pwe_mechanism sae_pwe;

    ANDROID_BACKPORT_RESERVED(1);
    ANDROID_BACKPORT_RESERVED(2);

    ANDROID_VENDOR_DATA(1);
    ANDROID_VENDOR_DATA(2);

    ANDROID_KABI_RESERVE(1);
};

struct cfg80211_he_bss_color {
    u8 color;
    bool enabled;
    bool partial;
};

struct cfg80211_beacon_data {
    unsigned int link_id;

    const u8 *head, *tail;
    const u8 *beacon_ies;
    const u8 *proberesp_ies;
    const u8 *assocresp_ies;
    const u8 *probe_resp;
    const u8 *lci;
    const u8 *civicloc;
    struct cfg80211_mbssid_elems *mbssid_ies;
    u8 ftm_responder;

    size_t head_len, tail_len;
    size_t beacon_ies_len;
    size_t proberesp_ies_len;
    size_t assocresp_ies_len;
    size_t probe_resp_len;
    size_t lci_len;
    size_t civicloc_len;
    struct cfg80211_he_bss_color he_bss_color;
    bool he_bss_color_valid;

    ANDROID_BACKPORT_RESERVED(1);
    ANDROID_BACKPORT_RESERVED(2);
    ANDROID_BACKPORT_RESERVED(3);
    ANDROID_BACKPORT_RESERVED(4);

    ANDROID_VENDOR_DATA(1);
    ANDROID_VENDOR_DATA(2);
    ANDROID_VENDOR_DATA(3);
    ANDROID_VENDOR_DATA(4);

    ANDROID_KABI_RESERVE(1);
};

/**
 * struct ieee80211_channel_sw_ie
 *
 * This structure refers to "Channel Switch Announcement information element"
 */
struct ieee80211_channel_sw_ie {
    u8 mode;
    u8 new_ch_num;
    u8 count;
} __attribute__((packed));

/**
 * struct ieee80211_mcs_info - MCS information
 * @rx_mask: RX mask
 * @rx_highest: highest supported RX rate. If set represents
 * the highest supported RX data rate in units of 1 Mbps.
 * If this field is 0 this value should not be used to
 * consider the highest RX data rate supported.
 * @tx_params: TX parameters
 */
struct ieee80211_mcs_info {
    u8 rx_mask[IEEE80211_HT_MCS_MASK_LEN];
    __le16 rx_highest;
    u8 tx_params;
    u8 reserved[3];
} __attribute__((packed));

struct ieee80211_ht_cap {
    __le16 cap_info;
    u8 ampdu_params_info;

    /* 16 bytes MCS information */
    struct ieee80211_mcs_info mcs;

    __le16 extended_ht_cap_info;
    __le32 tx_BF_cap_info;
    u8 antenna_selection_info;
} __attribute__((packed));

/**
 * struct ieee80211_vht_mcs_info - VHT MCS information
 * @rx_mcs_map: RX MCS map 2 bits for each stream, total 8 streams
 * @rx_highest: Indicates highest long GI VHT PPDU data rate
 *STA can receive. Rate expressed in units of 1 Mbps.
 *If this field is 0 this value should not be used to
 *consider the highest RX data rate supported.
 *The top 3 bits of this field indicate the Maximum NSTS,total
 *(a beamformee capability.)
 * @tx_mcs_map: TX MCS map 2 bits for each stream, total 8 streams
 * @tx_highest: Indicates highest long GI VHT PPDU data rate
 *STA can transmit. Rate expressed in units of 1 Mbps.
 *If this field is 0 this value should not be used to
 *consider the highest TX data rate supported.
 *The top 2 bits of this field are reserved, the
 *3rd bit from the top indiciates VHT Extended NSS BW
 *Capability.
 */
struct ieee80211_vht_mcs_info {
    __le16 rx_mcs_map;
    __le16 rx_highest;
    __le16 tx_mcs_map;
    __le16 tx_highest;
} __attribute__((packed));

/**
 * struct ieee80211_vht_cap - VHT capabilities
 *
 * This structure is the "VHT capabilities element" as
 * described in 802.11ac D3.0 8.4.2.160
 * @vht_cap_info: VHT capability info
 * @supp_mcs: VHT MCS supported rates
 */
struct ieee80211_vht_cap {
    __le32 vht_cap_info;
    struct ieee80211_vht_mcs_info supp_mcs;
} __attribute__((packed));

/**
 * struct ieee80211_he_cap_elem - HE capabilities element
 *
 * This structure is the "HE capabilities element" fixed fields as
 * described in P802.11ax_D4.0 section 9.4.2.242.2 and 9.4.2.242.3
 */
struct ieee80211_he_cap_elem {
    u8 mac_cap_info[6];
    u8 phy_cap_info[11];
} __attribute__((packed));

enum ieee80211_edmg_bw_config {
    IEEE80211_EDMG_BW_CONFIG_4 = 4,
    IEEE80211_EDMG_BW_CONFIG_5 = 5,
    IEEE80211_EDMG_BW_CONFIG_6 = 6,
    IEEE80211_EDMG_BW_CONFIG_7 = 7,
    IEEE80211_EDMG_BW_CONFIG_8 = 8,
    IEEE80211_EDMG_BW_CONFIG_9 = 9,
    IEEE80211_EDMG_BW_CONFIG_10 = 10,
    IEEE80211_EDMG_BW_CONFIG_11 = 11,
    IEEE80211_EDMG_BW_CONFIG_12 = 12,
    IEEE80211_EDMG_BW_CONFIG_13 = 13,
    IEEE80211_EDMG_BW_CONFIG_14 = 14,
    IEEE80211_EDMG_BW_CONFIG_15 = 15,
};

struct ieee80211_edmg {
    u8 channels;
    enum ieee80211_edmg_bw_config bw_config;
};

struct cfg80211_chan_def {
    struct ieee80211_channel *chan;
    enum nl80211_chan_width width;
    u32 center_freq1;
    u32 center_freq2;
    struct ieee80211_edmg edmg;
    u16 freq1_offset;

    ANDROID_BACKPORT_RESERVED(1);
    ANDROID_BACKPORT_RESERVED(2);
    ANDROID_BACKPORT_RESERVED(3);
    ANDROID_BACKPORT_RESERVED(4);

    ANDROID_VENDOR_DATA(1);
    ANDROID_VENDOR_DATA(2);
    ANDROID_VENDOR_DATA(3);
    ANDROID_VENDOR_DATA(4);
};

struct cfg80211_acl_data {
    enum nl80211_acl_policy acl_policy;
    int n_acl_entries;

    /* Keep it last */
    struct mac_address mac_addrs[];
};

struct cfg80211_ap_settings {
    struct cfg80211_chan_def chandef;

    struct cfg80211_beacon_data beacon;

    int beacon_interval, dtim_period;
    const u8 *ssid;
    size_t ssid_len;
    enum nl80211_hidden_ssid hidden_ssid;
    struct cfg80211_crypto_settings crypto;
    bool privacy;
    enum nl80211_auth_type auth_type;
    enum nl80211_smps_mode smps_mode;
    int inactivity_timeout;
    u8 p2p_ctwindow;
    bool p2p_opp_ps;
    const struct cfg80211_acl_data *acl;
    bool pbss;
    struct cfg80211_bitrate_mask beacon_rate;

    const struct ieee80211_ht_cap *ht_cap;
    const struct ieee80211_vht_cap *vht_cap;
    const struct ieee80211_he_cap_elem *he_cap;
    const struct ieee80211_he_operation *he_oper;
    const struct ieee80211_eht_cap_elem *eht_cap;
    const struct ieee80211_eht_operation *eht_oper;
    bool ht_required, vht_required, he_required, sae_h2e_required;
    bool twt_responder;
    u32 flags;
    struct ieee80211_he_obss_pd he_obss_pd;
    struct cfg80211_fils_discovery fils_discovery;
    struct cfg80211_unsol_bcast_probe_resp unsol_bcast_probe_resp;
    struct cfg80211_mbssid_config mbssid_config;
    u16 punct_bitmap;

    ANDROID_BACKPORT_RESERVED(1);
    ANDROID_BACKPORT_RESERVED(2);
    ANDROID_BACKPORT_RESERVED(3);
    ANDROID_BACKPORT_RESERVED(4);
    ANDROID_BACKPORT_RESERVED(5);

    ANDROID_VENDOR_DATA(1);
    ANDROID_VENDOR_DATA(2);
    ANDROID_VENDOR_DATA(3);
    ANDROID_VENDOR_DATA(4);
    ANDROID_VENDOR_DATA(5);

    ANDROID_KABI_RESERVE(1);
};

#endif //__NET_CFG80211_H
