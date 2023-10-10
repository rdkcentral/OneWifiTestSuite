#include <net/mac80211.h>
#include <linux/ieee80211.h>
#include <linux/rtnetlink.h>
#include <net/genetlink.h>
#include <net/netns/generic.h>
#include <linux/etherdevice.h>
#include <linux/list.h>
#include <net/fq.h>
//#include "wlan_emu_msg_data.h"
#include "rdkfmac.h"
#include "rdkfmac_cmd.h"
#include "rdkfmac_cfg80211.h"
#include "driver-ops.h"
#include "led.h"

void ieee80211_configure_filter(struct ieee80211_local *local)
{
    u64 mc;
    unsigned int changed_flags;
    unsigned int new_flags = 0;

    if (atomic_read(&local->iff_allmultis))
        new_flags |= FIF_ALLMULTI;

    if (local->monitors || test_bit(SCAN_SW_SCANNING, &local->scanning) ||
        test_bit(SCAN_ONCHANNEL_SCANNING, &local->scanning))
        new_flags |= FIF_BCN_PRBRESP_PROMISC;

    if (local->fif_probe_req || local->probe_req_reg)
        new_flags |= FIF_PROBE_REQ;

    if (local->fif_fcsfail)
        new_flags |= FIF_FCSFAIL;

    if (local->fif_plcpfail)
        new_flags |= FIF_PLCPFAIL;

    if (local->fif_control)
        new_flags |= FIF_CONTROL;

    if (local->fif_other_bss)
        new_flags |= FIF_OTHER_BSS;

    if (local->fif_pspoll)
        new_flags |= FIF_PSPOLL;

    if (local->rx_mcast_action_reg)
        new_flags |= FIF_MCAST_ACTION;

    spin_lock_bh(&local->filter_lock);
    changed_flags = local->filter_flags ^ new_flags;

    mc = drv_prepare_multicast(local, &local->mc_list);
    spin_unlock_bh(&local->filter_lock);

    /* be a bit nasty */
    new_flags |= (1<<31);

    drv_configure_filter(local, changed_flags, &new_flags, mc);

    WARN_ON(new_flags & (1<<31));

    local->filter_flags = new_flags & ~(1<<31);
}

static void ieee80211_reconfig_filter(struct work_struct *work)
{
    struct ieee80211_local *local =
        container_of(work, struct ieee80211_local, reconfig_filter);

    ieee80211_configure_filter(local);
}

static u32 ieee80211_hw_conf_chan(struct ieee80211_local *local)
{
    struct ieee80211_sub_if_data *sdata;
    struct cfg80211_chan_def chandef = {};
    u32 changed = 0;
    int power;
    u32 offchannel_flag;

    offchannel_flag = local->hw.conf.flags & IEEE80211_CONF_OFFCHANNEL;

    if (local->scan_chandef.chan) {
        chandef = local->scan_chandef;
    } else if (local->tmp_channel) {
        chandef.chan = local->tmp_channel;
        chandef.width = NL80211_CHAN_WIDTH_20_NOHT;
        chandef.center_freq1 = chandef.chan->center_freq;
        chandef.freq1_offset = chandef.chan->freq_offset;
    } else
        chandef = local->_oper_chandef;

    WARN(!cfg80211_chandef_valid(&chandef),
         "control:%d.%03d MHz width:%d center: %d.%03d/%d MHz",
         chandef.chan->center_freq, chandef.chan->freq_offset,
         chandef.width, chandef.center_freq1, chandef.freq1_offset,
         chandef.center_freq2);

    if (!cfg80211_chandef_identical(&chandef, &local->_oper_chandef))
        local->hw.conf.flags |= IEEE80211_CONF_OFFCHANNEL;
    else
        local->hw.conf.flags &= ~IEEE80211_CONF_OFFCHANNEL;

    offchannel_flag ^= local->hw.conf.flags & IEEE80211_CONF_OFFCHANNEL;

    if (offchannel_flag ||
        !cfg80211_chandef_identical(&local->hw.conf.chandef,
                    &local->_oper_chandef)) {
        local->hw.conf.chandef = chandef;
        changed |= IEEE80211_CONF_CHANGE_CHANNEL;
    }

    if (!conf_is_ht(&local->hw.conf)) {
        /*
         * mac80211.h documents that this is only valid
         * when the channel is set to an HT type, and
         * that otherwise STATIC is used.
         */
        local->hw.conf.smps_mode = IEEE80211_SMPS_STATIC;
    } else if (local->hw.conf.smps_mode != local->smps_mode) {
        local->hw.conf.smps_mode = local->smps_mode;
        changed |= IEEE80211_CONF_CHANGE_SMPS;
    }

    power = ieee80211_chandef_max_power(&chandef);

    rcu_read_lock();
    list_for_each_entry_rcu(sdata, &local->interfaces, list) {
        if (!rcu_access_pointer(sdata->vif.bss_conf.chanctx_conf))
            continue;
        if (sdata->vif.type == NL80211_IFTYPE_AP_VLAN)
            continue;
        if (sdata->vif.bss_conf.txpower == INT_MIN)
            continue;
        power = min(power, sdata->vif.bss_conf.txpower);
    }
    rcu_read_unlock();

    if (local->hw.conf.power_level != power) {
        changed |= IEEE80211_CONF_CHANGE_POWER;
        local->hw.conf.power_level = power;
    }

    return changed;
}

int ieee80211_hw_config(struct ieee80211_local *local, u32 changed)
{
    int ret = 0;

    might_sleep();

    if (!local->use_chanctx)
        changed |= ieee80211_hw_conf_chan(local);
    else
        changed &= ~(IEEE80211_CONF_CHANGE_CHANNEL |
                 IEEE80211_CONF_CHANGE_POWER);

    if (changed && local->open_count) {
        ret = drv_config(local, changed);
    }

    return ret;
}

void ieee80211_bss_info_change_notify(struct ieee80211_sub_if_data *sdata,
                      u32 changed)
{
    struct ieee80211_local *local = sdata->local;

    if (!changed || sdata->vif.type == NL80211_IFTYPE_AP_VLAN)
        return;

    drv_bss_info_changed(local, sdata, &sdata->vif.bss_conf, changed);
}

u32 ieee80211_reset_erp_info(struct ieee80211_sub_if_data *sdata)
{
    sdata->vif.bss_conf.use_cts_prot = false;
    sdata->vif.bss_conf.use_short_preamble = false;
    sdata->vif.bss_conf.use_short_slot = false;
    return BSS_CHANGED_ERP_CTS_PROT |
           BSS_CHANGED_ERP_PREAMBLE |
           BSS_CHANGED_ERP_SLOT;
}

static void ieee80211_tasklet_handler(struct tasklet_struct *t)
{
    struct ieee80211_local *local = from_tasklet(local, t, tasklet);
    struct sk_buff *skb;

    while ((skb = skb_dequeue(&local->skb_queue)) ||
           (skb = skb_dequeue(&local->skb_queue_unreliable))) {
        switch (skb->pkt_type) {
        case IEEE80211_RX_MSG:
            /* Clear skb->pkt_type in order to not confuse kernel
             * netstack. */
            skb->pkt_type = 0;
            ieee80211_rx(&local->hw, skb);
            break;
        case IEEE80211_TX_STATUS_MSG:
            skb->pkt_type = 0;
            ieee80211_tx_status(&local->hw, skb);
            break;
        default:
            WARN(1, "mac80211: Packet is of unknown type %d\n",
                 skb->pkt_type);
            dev_kfree_skb(skb);
            break;
        }
    }
}

static void ieee80211_restart_work(struct work_struct *work)
{
    struct ieee80211_local *local =
        container_of(work, struct ieee80211_local, restart_work);
    struct ieee80211_sub_if_data *sdata;
    int ret;

    /* wait for scan work complete */
    flush_workqueue(local->workqueue);
    flush_work(&local->sched_scan_stopped_work);
    flush_work(&local->radar_detected_work);

    rtnl_lock();
    /* we might do interface manipulations, so need both */
    wiphy_lock(local->hw.wiphy);

    WARN(test_bit(SCAN_HW_SCANNING, &local->scanning),
         "%s called with hardware scan in progress\n", __func__);

    list_for_each_entry(sdata, &local->interfaces, list) {
        /*
         * XXX: there may be more work for other vif types and even
         * for station mode: a good thing would be to run most of
         * the iface type's dependent _stop (ieee80211_mg_stop,
         * ieee80211_ibss_stop) etc...
         * For now, fix only the specific bug that was seen: race
         * between csa_connection_drop_work and us.
         */
        if (sdata->vif.type == NL80211_IFTYPE_STATION) {
            /*
             * This worker is scheduled from the iface worker that
             * runs on mac80211's workqueue, so we can't be
             * scheduling this worker after the cancel right here.
             * The exception is ieee80211_chswitch_done.
             * Then we can have a race...
             */
            cancel_work_sync(&sdata->u.mgd.csa_connection_drop_work);
            if (sdata->vif.bss_conf.csa_active) {
                sdata_lock(sdata);
                ieee80211_sta_connection_lost(sdata,
                                  WLAN_REASON_UNSPECIFIED,
                                  false);
                sdata_unlock(sdata);
            }
        }
        flush_delayed_work(&sdata->dec_tailroom_needed_wk);
    }
    ieee80211_scan_cancel(local);

    /* make sure any new ROC will consider local->in_reconfig */
    flush_delayed_work(&local->roc_work);
    flush_work(&local->hw_roc_done);

    /* wait for all packet processing to be done */
    synchronize_net();

    ret = ieee80211_reconfig(local);
    wiphy_unlock(local->hw.wiphy);

    if (ret)
        cfg80211_shutdown_all_interfaces(local->hw.wiphy);

    rtnl_unlock();
}

static const struct ieee80211_txrx_stypes
ieee80211_default_mgmt_stypes[NUM_NL80211_IFTYPES] = {
    [NL80211_IFTYPE_ADHOC] = {
        .tx = 0xffff,
        .rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
            BIT(IEEE80211_STYPE_AUTH >> 4) |
            BIT(IEEE80211_STYPE_DEAUTH >> 4) |
            BIT(IEEE80211_STYPE_PROBE_REQ >> 4),
    },
    [NL80211_IFTYPE_STATION] = {
        .tx = 0xffff,
        /*
         * To support Pre Association Security Negotiation (PASN) while
         * already associated to one AP, allow user space to register to
         * Rx authentication frames, so that the user space logic would
         * be able to receive/handle authentication frames from a
         * different AP as part of PASN.
         * It is expected that user space would intelligently register
         * for Rx authentication frames, i.e., only when PASN is used
         * and configure a match filter only for PASN authentication
         * algorithm, as otherwise the MLME functionality of mac80211
         * would be broken.
         */
        .rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
            BIT(IEEE80211_STYPE_AUTH >> 4) |
            BIT(IEEE80211_STYPE_PROBE_REQ >> 4),
    },
    [NL80211_IFTYPE_AP] = {
        .tx = 0xffff,
        .rx = BIT(IEEE80211_STYPE_ASSOC_REQ >> 4) |
            BIT(IEEE80211_STYPE_REASSOC_REQ >> 4) |
            BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
            BIT(IEEE80211_STYPE_DISASSOC >> 4) |
            BIT(IEEE80211_STYPE_AUTH >> 4) |
            BIT(IEEE80211_STYPE_DEAUTH >> 4) |
            BIT(IEEE80211_STYPE_ACTION >> 4),
    },
    [NL80211_IFTYPE_AP_VLAN] = {
        /* copy AP */
        .tx = 0xffff,
        .rx = BIT(IEEE80211_STYPE_ASSOC_REQ >> 4) |
            BIT(IEEE80211_STYPE_REASSOC_REQ >> 4) |
            BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
            BIT(IEEE80211_STYPE_DISASSOC >> 4) |
            BIT(IEEE80211_STYPE_AUTH >> 4) |
            BIT(IEEE80211_STYPE_DEAUTH >> 4) |
            BIT(IEEE80211_STYPE_ACTION >> 4),
    },
    [NL80211_IFTYPE_P2P_CLIENT] = {
        .tx = 0xffff,
        .rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
            BIT(IEEE80211_STYPE_PROBE_REQ >> 4),
    },
    [NL80211_IFTYPE_P2P_GO] = {
        .tx = 0xffff,
        .rx = BIT(IEEE80211_STYPE_ASSOC_REQ >> 4) |
            BIT(IEEE80211_STYPE_REASSOC_REQ >> 4) |
            BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
            BIT(IEEE80211_STYPE_DISASSOC >> 4) |
            BIT(IEEE80211_STYPE_AUTH >> 4) |
            BIT(IEEE80211_STYPE_DEAUTH >> 4) |
            BIT(IEEE80211_STYPE_ACTION >> 4),
    },
    [NL80211_IFTYPE_MESH_POINT] = {
        .tx = 0xffff,
        .rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
            BIT(IEEE80211_STYPE_AUTH >> 4) |
            BIT(IEEE80211_STYPE_DEAUTH >> 4),
    },
    [NL80211_IFTYPE_P2P_DEVICE] = {
        .tx = 0xffff,
        .rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
            BIT(IEEE80211_STYPE_PROBE_REQ >> 4),
    },
};

static const struct ieee80211_ht_cap mac80211_ht_capa_mod_mask = {
    .ampdu_params_info = IEEE80211_HT_AMPDU_PARM_FACTOR |
                 IEEE80211_HT_AMPDU_PARM_DENSITY,

    .cap_info = cpu_to_le16(IEEE80211_HT_CAP_SUP_WIDTH_20_40 |
                IEEE80211_HT_CAP_MAX_AMSDU |
                IEEE80211_HT_CAP_SGI_20 |
                IEEE80211_HT_CAP_SGI_40 |
                IEEE80211_HT_CAP_TX_STBC |
                IEEE80211_HT_CAP_RX_STBC |
                IEEE80211_HT_CAP_LDPC_CODING |
                IEEE80211_HT_CAP_40MHZ_INTOLERANT),
    .mcs = {
        .rx_mask = { 0xff, 0xff, 0xff, 0xff, 0xff,
                 0xff, 0xff, 0xff, 0xff, 0xff, },
    },
};

static const struct ieee80211_vht_cap mac80211_vht_capa_mod_mask = {
    .vht_cap_info =
        cpu_to_le32(IEEE80211_VHT_CAP_RXLDPC |
                IEEE80211_VHT_CAP_SHORT_GI_80 |
                IEEE80211_VHT_CAP_SHORT_GI_160 |
                IEEE80211_VHT_CAP_RXSTBC_MASK |
                IEEE80211_VHT_CAP_TXSTBC |
                IEEE80211_VHT_CAP_SU_BEAMFORMER_CAPABLE |
                IEEE80211_VHT_CAP_SU_BEAMFORMEE_CAPABLE |
                IEEE80211_VHT_CAP_TX_ANTENNA_PATTERN |
                IEEE80211_VHT_CAP_RX_ANTENNA_PATTERN |
                IEEE80211_VHT_CAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK),
    .supp_mcs = {
        .rx_mcs_map = cpu_to_le16(~0),
        .tx_mcs_map = cpu_to_le16(~0),
    },
};



struct ieee80211_hw *rdkfmac_alloc_hw(size_t priv_data_len,
                       const struct ieee80211_ops *ops)
{
    struct ieee80211_local *local;
    int priv_size, i;
    struct wiphy *wiphy;
    bool use_chanctx;

    if (WARN_ON(!ops->tx || !ops->start || !ops->stop || !ops->config ||
            !ops->add_interface || !ops->remove_interface ||
            !ops->configure_filter))
        return NULL;

    if (WARN_ON(ops->sta_state && (ops->sta_add || ops->sta_remove)))
        return NULL;

    /* check all or no channel context operations exist */
    i = !!ops->add_chanctx + !!ops->remove_chanctx +
        !!ops->change_chanctx + !!ops->assign_vif_chanctx +
        !!ops->unassign_vif_chanctx;
    if (WARN_ON(i != 0 && i != 5))
        return NULL;
    use_chanctx = i == 5;

    /* Ensure 32-byte alignment of our private data and hw private data.
     * We use the wiphy priv data for both our ieee80211_local and for
     * the driver's private data
     *
     * In memory it'll be like this:
     *
     * +-------------------------+
     * | struct wiphy       |
     * +-------------------------+
     * | struct ieee80211_local  |
     * +-------------------------+
     * | driver's private data   |
     * +-------------------------+
     *
     */
    priv_size = ALIGN(sizeof(*local), NETDEV_ALIGN) + priv_data_len;

    wiphy = wiphy_new(&rdkfmac_config_ops, priv_size);

    if (!wiphy)
        return NULL;

    wiphy->mgmt_stypes = ieee80211_default_mgmt_stypes;

    wiphy->privid = rdkfmac_wiphy_privid;

    wiphy->flags |= WIPHY_FLAG_NETNS_OK |
            WIPHY_FLAG_4ADDR_AP |
            WIPHY_FLAG_4ADDR_STATION |
            WIPHY_FLAG_REPORTS_OBSS |
            WIPHY_FLAG_OFFCHAN_TX;

    if (!use_chanctx || ops->remain_on_channel)
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
    wiphy_ext_feature_set(wiphy,
                  NL80211_EXT_FEATURE_CONTROL_PORT_NO_PREAUTH);
    wiphy_ext_feature_set(wiphy,
                  NL80211_EXT_FEATURE_CONTROL_PORT_OVER_NL80211_TX_STATUS);
    wiphy_ext_feature_set(wiphy,
                  NL80211_EXT_FEATURE_SCAN_FREQ_KHZ);

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

    wiphy->bss_priv_size = sizeof(struct ieee80211_bss);

    local = wiphy_priv(wiphy);

    if (sta_info_init(local))
        goto err_free;

    local->hw.wiphy = wiphy;

    local->hw.priv = (char *)local + ALIGN(sizeof(*local), NETDEV_ALIGN);

    local->ops = ops;
    local->use_chanctx = use_chanctx;

    /*
     * We need a bit of data queued to build aggregates properly, so
     * instruct the TCP stack to allow more than a single ms of data
     * to be queued in the stack. The value is a bit-shift of 1
     * second, so 7 is ~8ms of queued data. Only affects local TCP
     * sockets.
     * This is the default, anyhow - drivers may need to override it
     * for local reasons (longer buffers, longer completion time, or
     * similar).
     */
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
    local->hw.uapsd_queues = IEEE80211_DEFAULT_UAPSD_QUEUES;
    local->hw.uapsd_max_sp_len = IEEE80211_DEFAULT_MAX_SP_LEN;
    local->hw.max_mtu = IEEE80211_MAX_DATA_LEN;
    local->user_power_level = IEEE80211_UNSET_POWER_LEVEL;
    wiphy->ht_capa_mod_mask = &mac80211_ht_capa_mod_mask;
    wiphy->vht_capa_mod_mask = &mac80211_vht_capa_mod_mask;

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

    for (i = 0; i < IEEE80211_NUM_ACS; i++) {
        struct airtime_sched_info *air_sched = &local->airtime[i];

        air_sched->active_txqs = RB_ROOT_CACHED;
        INIT_LIST_HEAD(&air_sched->active_list);
        spin_lock_init(&air_sched->lock);
        air_sched->aql_txq_limit_low = IEEE80211_DEFAULT_AQL_TXQ_LIMIT_L;
        air_sched->aql_txq_limit_high =
            IEEE80211_DEFAULT_AQL_TXQ_LIMIT_H;
    }

    local->airtime_flags = AIRTIME_USE_TX | AIRTIME_USE_RX;
    local->aql_threshold = IEEE80211_AQL_THRESHOLD;
    atomic_set(&local->aql_total_pending_airtime, 0);

    INIT_LIST_HEAD(&local->chanctx_list);
    mutex_init(&local->chanctx_mtx);

    INIT_DELAYED_WORK(&local->scan_work, ieee80211_scan_work);

    INIT_WORK(&local->restart_work, ieee80211_restart_work);

    INIT_WORK(&local->radar_detected_work,
          ieee80211_dfs_radar_detected_work);

    INIT_WORK(&local->reconfig_filter, ieee80211_reconfig_filter);
    local->smps_mode = IEEE80211_SMPS_OFF;

    INIT_WORK(&local->dynamic_ps_enable_work,
          ieee80211_dynamic_ps_enable_work);
    INIT_WORK(&local->dynamic_ps_disable_work,
          ieee80211_dynamic_ps_disable_work);
    timer_setup(&local->dynamic_ps_timer, ieee80211_dynamic_ps_timer, 0);

    INIT_WORK(&local->sched_scan_stopped_work,
          ieee80211_sched_scan_stopped_work);

    spin_lock_init(&local->ack_status_lock);
    idr_init(&local->ack_status_frames);

    for (i = 0; i < IEEE80211_MAX_QUEUES; i++) {
        skb_queue_head_init(&local->pending[i]);
        atomic_set(&local->agg_queue_stop[i], 0);
    }
    tasklet_setup(&local->tx_pending_tasklet, ieee80211_tx_pending);

    if (ops->wake_tx_queue)
        tasklet_setup(&local->wake_txqs_tasklet, ieee80211_wake_txqs);

    tasklet_setup(&local->tasklet, ieee80211_tasklet_handler);

    skb_queue_head_init(&local->skb_queue);
    skb_queue_head_init(&local->skb_queue_unreliable);

    ieee80211_alloc_led_names(local);

    ieee80211_roc_setup(local);

    local->hw.radiotap_timestamp.units_pos = -1;
    local->hw.radiotap_timestamp.accuracy = -1;

    return &local->hw;
 err_free:
    wiphy_free(wiphy);
    return NULL;
}
