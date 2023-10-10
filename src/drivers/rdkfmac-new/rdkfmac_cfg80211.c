#include "rdkfmac.h"

/* Supported rates to be advertised to the cfg80211 */
static struct ieee80211_rate rdkfmac_rates_2g[] = {
    {.bitrate = 10, .hw_value = 2, },
    {.bitrate = 20, .hw_value = 4, },
    {.bitrate = 55, .hw_value = 11, },
    {.bitrate = 110, .hw_value = 22, },
    {.bitrate = 60, .hw_value = 12, },
    {.bitrate = 90, .hw_value = 18, },
    {.bitrate = 120, .hw_value = 24, },
    {.bitrate = 180, .hw_value = 36, },
    {.bitrate = 240, .hw_value = 48, },
    {.bitrate = 360, .hw_value = 72, },
    {.bitrate = 480, .hw_value = 96, },
    {.bitrate = 540, .hw_value = 108, },
};

/* Supported rates to be advertised to the cfg80211 */
static struct ieee80211_rate rdkfmac_rates_5g[] = {
    {.bitrate = 60, .hw_value = 12, },
    {.bitrate = 90, .hw_value = 18, },
    {.bitrate = 120, .hw_value = 24, },
    {.bitrate = 180, .hw_value = 36, },
    {.bitrate = 240, .hw_value = 48, },
    {.bitrate = 360, .hw_value = 72, },
    {.bitrate = 480, .hw_value = 96, },
    {.bitrate = 540, .hw_value = 108, },
};

/* Supported crypto cipher suits to be advertised to cfg80211 */
static const u32 rdkfmac_cipher_suites[] = {
    WLAN_CIPHER_SUITE_TKIP,
    WLAN_CIPHER_SUITE_CCMP,
    WLAN_CIPHER_SUITE_AES_CMAC,
};

/* Supported mgmt frame types to be advertised to cfg80211 */
static const struct ieee80211_txrx_stypes
rdkfmac_mgmt_stypes[NUM_NL80211_IFTYPES] = {
    [NL80211_IFTYPE_STATION] = {
        .tx = BIT(IEEE80211_STYPE_ACTION >> 4) |
              BIT(IEEE80211_STYPE_AUTH >> 4),
        .rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
              BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
              BIT(IEEE80211_STYPE_AUTH >> 4),
    },
    [NL80211_IFTYPE_AP] = {
        .tx = BIT(IEEE80211_STYPE_ACTION >> 4) |
              BIT(IEEE80211_STYPE_AUTH >> 4),
        .rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
              BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
              BIT(IEEE80211_STYPE_ASSOC_REQ >> 4) |
              BIT(IEEE80211_STYPE_REASSOC_REQ >> 4) |
              BIT(IEEE80211_STYPE_AUTH >> 4),
    },
};

static void rdkfmac_cfg80211_reg_notifier(struct wiphy *wiphy,
                       struct regulatory_request *req)
{

}


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

static int rdkfmac_stop_ap(struct wiphy *wiphy, struct net_device *dev,
            unsigned int link_id)
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
    // .stop_ap        = rdkfmac_stop_ap,
    .set_wiphy_params   = rdkfmac_set_wiphy_params,
    // .update_mgmt_frame_registrations =
    // rdkfmac_update_mgmt_frame_registrations,
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

static int
rdkfmac_wiphy_setup_if_comb(struct wiphy *wiphy, rdkfmac_mac_info_t *mac_info)
{
#if 0
    struct ieee80211_iface_combination *if_comb;
    size_t n_if_comb;
    u16 interface_modes = 0;
    size_t i, j;

    if_comb = mac_info->if_comb;
    n_if_comb = mac_info->n_if_comb;

    if (!if_comb || !n_if_comb)
        return -ENOENT;

    for (i = 0; i < n_if_comb; i++) {
        if_comb[i].radar_detect_widths = mac_info->radar_detect_widths;

        for (j = 0; j < if_comb[i].n_limits; j++)
            interface_modes |= if_comb[i].limits[j].types;
    }

    wiphy->iface_combinations = if_comb;
    wiphy->n_iface_combinations = n_if_comb;
    wiphy->interface_modes = interface_modes;
#endif

    return 0;
}


struct wiphy *rdkfmac_wiphy_allocate(rdkfmac_bus_t *bus, struct platform_device *pdev)
{
    struct wiphy *wiphy;

    wiphy = wiphy_new(&rdkfmac_cfg80211_ops, sizeof(rdkfmac_wmac_t));
    if (!wiphy)
        return NULL;

    if (pdev)
        set_wiphy_dev(wiphy, &pdev->dev);
    else
        set_wiphy_dev(wiphy, bus->dev);

    return wiphy;
}

int rdkfmac_wiphy_register(rdkfmac_hw_info_t *hw_info, rdkfmac_wmac_t *mac)
{
    struct wiphy *wiphy = priv_to_wiphy(mac);
    rdkfmac_mac_info_t *macinfo = &mac->macinfo;
    int ret;
    bool regdomain_is_known;

    if (!wiphy) {
        pr_err("invalid wiphy pointer\n");
        return -EFAULT;
    }

    wiphy->frag_threshold = macinfo->frag_thr;
    wiphy->rts_threshold = macinfo->rts_thr;
    wiphy->retry_short = macinfo->sretry_limit;
    wiphy->retry_long = macinfo->lretry_limit;
    wiphy->coverage_class = macinfo->coverage_class;

    wiphy->max_scan_ssids =
        (macinfo->max_scan_ssids) ? macinfo->max_scan_ssids : 1;
    wiphy->max_scan_ie_len = RDKFMAC_MAX_VSIE_LEN;
    wiphy->mgmt_stypes = rdkfmac_mgmt_stypes;
    wiphy->max_remain_on_channel_duration = 5000;
    wiphy->max_acl_mac_addrs = macinfo->max_acl_mac_addrs;
    wiphy->max_num_csa_counters = 2;

    ret = rdkfmac_wiphy_setup_if_comb(wiphy, macinfo);
    if (ret)
        goto out;

    /* Initialize cipher suits */
    wiphy->cipher_suites = rdkfmac_cipher_suites;
    wiphy->n_cipher_suites = ARRAY_SIZE(rdkfmac_cipher_suites);
    wiphy->signal_type = CFG80211_SIGNAL_TYPE_MBM;
    wiphy->flags |= WIPHY_FLAG_HAVE_AP_SME |
            WIPHY_FLAG_AP_PROBE_RESP_OFFLOAD |
            WIPHY_FLAG_AP_UAPSD |
            WIPHY_FLAG_HAS_CHANNEL_SWITCH |
            WIPHY_FLAG_4ADDR_STATION |
            WIPHY_FLAG_NETNS_OK;
    wiphy->flags &= ~WIPHY_FLAG_PS_ON_BY_DEFAULT;

    if (rdkfmac_dfs_offload_get() &&
        rdkfmac_hwcap_is_set(hw_info, RDKFMAC_HW_CAPAB_DFS_OFFLOAD))
        wiphy_ext_feature_set(wiphy, NL80211_EXT_FEATURE_DFS_OFFLOAD);

    if (rdkfmac_hwcap_is_set(hw_info, RDKFMAC_HW_CAPAB_SCAN_DWELL))
        wiphy_ext_feature_set(wiphy,
                      NL80211_EXT_FEATURE_SET_SCAN_DWELL);

    wiphy->probe_resp_offload = NL80211_PROBE_RESP_OFFLOAD_SUPPORT_WPS |
                    NL80211_PROBE_RESP_OFFLOAD_SUPPORT_WPS2;

    wiphy->available_antennas_tx = macinfo->num_tx_chain;
    wiphy->available_antennas_rx = macinfo->num_rx_chain;

    wiphy->max_ap_assoc_sta = macinfo->max_ap_assoc_sta;
    wiphy->ht_capa_mod_mask = &macinfo->ht_cap_mod_mask;
    wiphy->vht_capa_mod_mask = &macinfo->vht_cap_mod_mask;

    ether_addr_copy(wiphy->perm_addr, mac->macaddr);

    if (rdkfmac_hwcap_is_set(hw_info, RDKFMAC_HW_CAPAB_STA_INACT_TIMEOUT))
        wiphy->features |= NL80211_FEATURE_INACTIVITY_TIMER;

    if (rdkfmac_hwcap_is_set(hw_info, RDKFMAC_HW_CAPAB_SCAN_RANDOM_MAC_ADDR))
        wiphy->features |= NL80211_FEATURE_SCAN_RANDOM_MAC_ADDR;

    if (!rdkfmac_hwcap_is_set(hw_info, RDKFMAC_HW_CAPAB_OBSS_SCAN))
        wiphy->features |= NL80211_FEATURE_NEED_OBSS_SCAN;

    if (rdkfmac_hwcap_is_set(hw_info, RDKFMAC_HW_CAPAB_SAE))
        wiphy->features |= NL80211_FEATURE_SAE;

#ifdef CONFIG_PM
    if (macinfo->wowlan)
        wiphy->wowlan = macinfo->wowlan;
#endif

    regdomain_is_known = isalpha(mac->rd->alpha2[0]) &&
                isalpha(mac->rd->alpha2[1]);

    if (rdkfmac_hwcap_is_set(hw_info, RDKFMAC_HW_CAPAB_REG_UPDATE)) {
        wiphy->reg_notifier = rdkfmac_cfg80211_reg_notifier;

        if (mac->rd->alpha2[0] == '9' && mac->rd->alpha2[1] == '9') {
            wiphy->regulatory_flags |= REGULATORY_CUSTOM_REG |
                REGULATORY_STRICT_REG;
            wiphy_apply_custom_regulatory(wiphy, mac->rd);
        } else if (regdomain_is_known) {
            wiphy->regulatory_flags |= REGULATORY_STRICT_REG;
        }
    } else {
        wiphy->regulatory_flags |= REGULATORY_WIPHY_SELF_MANAGED;
    }

    if (mac->macinfo.extended_capabilities_len) {
        wiphy->extended_capabilities =
            mac->macinfo.extended_capabilities;
        wiphy->extended_capabilities_mask =
            mac->macinfo.extended_capabilities_mask;
        wiphy->extended_capabilities_len =
            mac->macinfo.extended_capabilities_len;
    }

    strscpy(wiphy->fw_version, hw_info->fw_version,
        sizeof(wiphy->fw_version));
    wiphy->hw_version = hw_info->hw_version;

    ret = wiphy_register(wiphy);
    if (ret < 0)
        goto out;

    if (wiphy->regulatory_flags & REGULATORY_WIPHY_SELF_MANAGED)
        ret = regulatory_set_wiphy_regd(wiphy, mac->rd);
    else if (regdomain_is_known)
        ret = regulatory_hint(wiphy, mac->rd->alpha2);

out:
    return ret;

}
