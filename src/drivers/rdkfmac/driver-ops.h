#ifndef __MAC80211_DRIVER_OPS
#define __MAC80211_DRIVER_OPS

#include <net/mac80211.h>
#include "rdkfmac.h"

#define check_sdata_in_driver(sdata)    ({                  \
    !WARN_ONCE(!(sdata->flags & IEEE80211_SDATA_IN_DRIVER),         \
           "%s: Failed check-sdata-in-driver check, flags: 0x%x\n", \
           sdata->dev ? sdata->dev->name : sdata->name, sdata->flags);  \
})

static inline struct ieee80211_sub_if_data *
get_bss_sdata(struct ieee80211_sub_if_data *sdata)
{
    if (sdata->vif.type == NL80211_IFTYPE_AP_VLAN)
        sdata = container_of(sdata->bss, struct ieee80211_sub_if_data,
                     u.ap);

    return sdata;
}

static inline void drv_tx(struct ieee80211_local *local,
              struct ieee80211_tx_control *control,
              struct sk_buff *skb)
{
    local->ops->tx(&local->hw, control, skb);
}

static inline void drv_sync_rx_queues(struct ieee80211_local *local,
                      struct sta_info *sta)
{
    if (local->ops->sync_rx_queues) {
        local->ops->sync_rx_queues(&local->hw);
    }
}

static inline void drv_get_et_strings(struct ieee80211_sub_if_data *sdata,
                      u32 sset, u8 *data)
{
    struct ieee80211_local *local = sdata->local;
    if (local->ops->get_et_strings) {
        local->ops->get_et_strings(&local->hw, &sdata->vif, sset, data);
    }
}

static inline void drv_get_et_stats(struct ieee80211_sub_if_data *sdata,
                    struct ethtool_stats *stats,
                    u64 *data)
{
    struct ieee80211_local *local = sdata->local;
    if (local->ops->get_et_stats) {
        local->ops->get_et_stats(&local->hw, &sdata->vif, stats, data);
    }
}

static inline int drv_get_et_sset_count(struct ieee80211_sub_if_data *sdata,
                    int sset)
{
    struct ieee80211_local *local = sdata->local;
    int rv = 0;
    if (local->ops->get_et_sset_count) {
        rv = local->ops->get_et_sset_count(&local->hw, &sdata->vif,
                           sset);
    }
    return rv;
}

int drv_start(struct ieee80211_local *local);
void drv_stop(struct ieee80211_local *local);

#ifdef CONFIG_PM
static inline int drv_suspend(struct ieee80211_local *local,
                  struct cfg80211_wowlan *wowlan)
{
    int ret;

    might_sleep();

    ret = local->ops->suspend(&local->hw, wowlan);
    return ret;
}

static inline int drv_resume(struct ieee80211_local *local)
{
    int ret;

    might_sleep();

    ret = local->ops->resume(&local->hw);
    return ret;
}

static inline void drv_set_wakeup(struct ieee80211_local *local,
                  bool enabled)
{
    might_sleep();

    if (!local->ops->set_wakeup)
        return;

    local->ops->set_wakeup(&local->hw, enabled);
}
#endif

int drv_add_interface(struct ieee80211_local *local,
              struct ieee80211_sub_if_data *sdata);

int drv_change_interface(struct ieee80211_local *local,
             struct ieee80211_sub_if_data *sdata,
             enum nl80211_iftype type, bool p2p);

void drv_remove_interface(struct ieee80211_local *local,
              struct ieee80211_sub_if_data *sdata);

static inline int drv_config(struct ieee80211_local *local, u32 changed)
{
    int ret;

    might_sleep();

    ret = local->ops->config(&local->hw, changed);
    return ret;
}

static inline void drv_bss_info_changed(struct ieee80211_local *local,
                    struct ieee80211_sub_if_data *sdata,
                    struct ieee80211_bss_conf *info,
                    u32 changed)
{
    might_sleep();

    if (WARN_ON_ONCE(changed & (BSS_CHANGED_BEACON |
                    BSS_CHANGED_BEACON_ENABLED) &&
             sdata->vif.type != NL80211_IFTYPE_AP &&
             sdata->vif.type != NL80211_IFTYPE_ADHOC &&
             sdata->vif.type != NL80211_IFTYPE_MESH_POINT &&
             sdata->vif.type != NL80211_IFTYPE_OCB))
        return;

    if (WARN_ON_ONCE(sdata->vif.type == NL80211_IFTYPE_P2P_DEVICE ||
             sdata->vif.type == NL80211_IFTYPE_NAN ||
             (sdata->vif.type == NL80211_IFTYPE_MONITOR &&
              !sdata->vif.bss_conf.mu_mimo_owner &&
              !(changed & BSS_CHANGED_TXPOWER))))
        return;

    if (!check_sdata_in_driver(sdata))
        return;

    if (local->ops->bss_info_changed)
        local->ops->bss_info_changed(&local->hw, &sdata->vif, info, changed);
}

static inline u64 drv_prepare_multicast(struct ieee80211_local *local,
                    struct netdev_hw_addr_list *mc_list)
{
    u64 ret = 0;


    if (local->ops->prepare_multicast)
        ret = local->ops->prepare_multicast(&local->hw, mc_list);


    return ret;
}

static inline void drv_configure_filter(struct ieee80211_local *local,
                    unsigned int changed_flags,
                    unsigned int *total_flags,
                    u64 multicast)
{
    might_sleep();

    local->ops->configure_filter(&local->hw, changed_flags, total_flags,
                     multicast);
}

static inline void drv_config_iface_filter(struct ieee80211_local *local,
                       struct ieee80211_sub_if_data *sdata,
                       unsigned int filter_flags,
                       unsigned int changed_flags)
{
    might_sleep();

    if (local->ops->config_iface_filter)
        local->ops->config_iface_filter(&local->hw, &sdata->vif,
                        filter_flags,
                        changed_flags);
}

static inline int drv_set_tim(struct ieee80211_local *local,
                  struct ieee80211_sta *sta, bool set)
{
    int ret = 0;
    if (local->ops->set_tim)
        ret = local->ops->set_tim(&local->hw, sta, set);
    return ret;
}

static inline int drv_set_key(struct ieee80211_local *local,
                  enum set_key_cmd cmd,
                  struct ieee80211_sub_if_data *sdata,
                  struct ieee80211_sta *sta,
                  struct ieee80211_key_conf *key)
{
    int ret;

    might_sleep();

    sdata = get_bss_sdata(sdata);
    if (!check_sdata_in_driver(sdata))
        return -EIO;

    ret = local->ops->set_key(&local->hw, cmd, &sdata->vif, sta, key);
    return ret;
}

static inline void drv_update_tkip_key(struct ieee80211_local *local,
                       struct ieee80211_sub_if_data *sdata,
                       struct ieee80211_key_conf *conf,
                       struct sta_info *sta, u32 iv32,
                       u16 *phase1key)
{
    struct ieee80211_sta *ista = NULL;

    if (sta)
        ista = &sta->sta;

    sdata = get_bss_sdata(sdata);
    if (!check_sdata_in_driver(sdata))
        return;

    if (local->ops->update_tkip_key)
        local->ops->update_tkip_key(&local->hw, &sdata->vif, conf,
                        ista, iv32, phase1key);
}

static inline int drv_hw_scan(struct ieee80211_local *local,
                  struct ieee80211_sub_if_data *sdata,
                  struct ieee80211_scan_request *req)
{
    int ret;

    might_sleep();

    if (!check_sdata_in_driver(sdata))
        return -EIO;

    ret = local->ops->hw_scan(&local->hw, &sdata->vif, req);
    return ret;
}

static inline void drv_cancel_hw_scan(struct ieee80211_local *local,
                      struct ieee80211_sub_if_data *sdata)
{
    might_sleep();

    if (!check_sdata_in_driver(sdata))
        return;

    local->ops->cancel_hw_scan(&local->hw, &sdata->vif);
}

static inline int
drv_sched_scan_start(struct ieee80211_local *local,
             struct ieee80211_sub_if_data *sdata,
             struct cfg80211_sched_scan_request *req,
             struct ieee80211_scan_ies *ies)
{
    int ret;

    might_sleep();

    if (!check_sdata_in_driver(sdata))
        return -EIO;

    ret = local->ops->sched_scan_start(&local->hw, &sdata->vif,
                          req, ies);
    return ret;
}

static inline int drv_sched_scan_stop(struct ieee80211_local *local,
                      struct ieee80211_sub_if_data *sdata)
{
    int ret;

    might_sleep();

    if (!check_sdata_in_driver(sdata))
        return -EIO;

    ret = local->ops->sched_scan_stop(&local->hw, &sdata->vif);

    return ret;
}

static inline void drv_sw_scan_start(struct ieee80211_local *local,
                     struct ieee80211_sub_if_data *sdata,
                     const u8 *mac_addr)
{
    might_sleep();

    if (local->ops->sw_scan_start)
        local->ops->sw_scan_start(&local->hw, &sdata->vif, mac_addr);
}

static inline void drv_sw_scan_complete(struct ieee80211_local *local,
                    struct ieee80211_sub_if_data *sdata)
{
    might_sleep();

    if (local->ops->sw_scan_complete)
        local->ops->sw_scan_complete(&local->hw, &sdata->vif);
}

static inline int drv_get_stats(struct ieee80211_local *local,
                struct ieee80211_low_level_stats *stats)
{
    int ret = -EOPNOTSUPP;

    might_sleep();

    if (local->ops->get_stats)
        ret = local->ops->get_stats(&local->hw, stats);

    return ret;
}

static inline void drv_get_key_seq(struct ieee80211_local *local,
                   struct ieee80211_key *key,
                   struct ieee80211_key_seq *seq)
{
    if (local->ops->get_key_seq)
        local->ops->get_key_seq(&local->hw, &key->conf, seq);
}

static inline int drv_set_frag_threshold(struct ieee80211_local *local,
                    u32 value)
{
    int ret = 0;

    might_sleep();

    if (local->ops->set_frag_threshold)
        ret = local->ops->set_frag_threshold(&local->hw, value);
    return ret;
}

static inline int drv_set_rts_threshold(struct ieee80211_local *local,
                    u32 value)
{
    int ret = 0;

    might_sleep();

    if (local->ops->set_rts_threshold)
        ret = local->ops->set_rts_threshold(&local->hw, value);
    return ret;
}

static inline int drv_set_coverage_class(struct ieee80211_local *local,
                     s16 value)
{
    int ret = 0;
    might_sleep();

    if (local->ops->set_coverage_class)
        local->ops->set_coverage_class(&local->hw, value);
    else
        ret = -EOPNOTSUPP;

    return ret;
}

static inline void drv_sta_notify(struct ieee80211_local *local,
                  struct ieee80211_sub_if_data *sdata,
                  enum sta_notify_cmd cmd,
                  struct ieee80211_sta *sta)
{
    sdata = get_bss_sdata(sdata);
    if (!check_sdata_in_driver(sdata))
        return;

    if (local->ops->sta_notify)
        local->ops->sta_notify(&local->hw, &sdata->vif, cmd, sta);
}

static inline int drv_sta_add(struct ieee80211_local *local,
                  struct ieee80211_sub_if_data *sdata,
                  struct ieee80211_sta *sta)
{
    int ret = 0;

    might_sleep();

    sdata = get_bss_sdata(sdata);
    if (!check_sdata_in_driver(sdata))
        return -EIO;

    if (local->ops->sta_add)
        ret = local->ops->sta_add(&local->hw, &sdata->vif, sta);


    return ret;
}

static inline void drv_sta_remove(struct ieee80211_local *local,
                  struct ieee80211_sub_if_data *sdata,
                  struct ieee80211_sta *sta)
{
    might_sleep();

    sdata = get_bss_sdata(sdata);
    if (!check_sdata_in_driver(sdata))
        return;

    if (local->ops->sta_remove)
        local->ops->sta_remove(&local->hw, &sdata->vif, sta);

}

#ifdef CONFIG_MAC80211_DEBUGFS
static inline void drv_sta_add_debugfs(struct ieee80211_local *local,
                       struct ieee80211_sub_if_data *sdata,
                       struct ieee80211_sta *sta,
                       struct dentry *dir)
{
    might_sleep();

    sdata = get_bss_sdata(sdata);
    if (!check_sdata_in_driver(sdata))
        return;

    if (local->ops->sta_add_debugfs)
        local->ops->sta_add_debugfs(&local->hw, &sdata->vif,
                        sta, dir);
}
#endif

static inline void drv_sta_pre_rcu_remove(struct ieee80211_local *local,
                      struct ieee80211_sub_if_data *sdata,
                      struct sta_info *sta)
{
    might_sleep();

    sdata = get_bss_sdata(sdata);
    if (!check_sdata_in_driver(sdata))
        return;

    if (local->ops->sta_pre_rcu_remove)
        local->ops->sta_pre_rcu_remove(&local->hw, &sdata->vif,
                           &sta->sta);
}

__must_check
int drv_sta_state(struct ieee80211_local *local,
          struct ieee80211_sub_if_data *sdata,
          struct sta_info *sta,
          enum ieee80211_sta_state old_state,
          enum ieee80211_sta_state new_state);

__must_check
int drv_sta_set_txpwr(struct ieee80211_local *local,
              struct ieee80211_sub_if_data *sdata,
              struct sta_info *sta);

void drv_sta_rc_update(struct ieee80211_local *local,
               struct ieee80211_sub_if_data *sdata,
               struct ieee80211_sta *sta, u32 changed);

static inline void drv_sta_rate_tbl_update(struct ieee80211_local *local,
                       struct ieee80211_sub_if_data *sdata,
                       struct ieee80211_sta *sta)
{
    sdata = get_bss_sdata(sdata);
    if (!check_sdata_in_driver(sdata))
        return;

    if (local->ops->sta_rate_tbl_update)
        local->ops->sta_rate_tbl_update(&local->hw, &sdata->vif, sta);

}

static inline void drv_sta_statistics(struct ieee80211_local *local,
                      struct ieee80211_sub_if_data *sdata,
                      struct ieee80211_sta *sta,
                      struct station_info *sinfo)
{
    sdata = get_bss_sdata(sdata);
    if (!check_sdata_in_driver(sdata))
        return;

    if (local->ops->sta_statistics)
        local->ops->sta_statistics(&local->hw, &sdata->vif, sta, sinfo);
}

int drv_conf_tx(struct ieee80211_local *local,
        struct ieee80211_sub_if_data *sdata, u16 ac,
        const struct ieee80211_tx_queue_params *params);

u64 drv_get_tsf(struct ieee80211_local *local,
        struct ieee80211_sub_if_data *sdata);
void drv_set_tsf(struct ieee80211_local *local,
         struct ieee80211_sub_if_data *sdata,
         u64 tsf);
void drv_offset_tsf(struct ieee80211_local *local,
            struct ieee80211_sub_if_data *sdata,
            s64 offset);
void drv_reset_tsf(struct ieee80211_local *local,
           struct ieee80211_sub_if_data *sdata);

static inline int drv_tx_last_beacon(struct ieee80211_local *local)
{
    int ret = 0; /* default unsupported op for less congestion */

    might_sleep();

    if (local->ops->tx_last_beacon)
        ret = local->ops->tx_last_beacon(&local->hw);
    return ret;
}

int drv_ampdu_action(struct ieee80211_local *local,
             struct ieee80211_sub_if_data *sdata,
             struct ieee80211_ampdu_params *params);

static inline int drv_get_survey(struct ieee80211_local *local, int idx,
                struct survey_info *survey)
{
    int ret = -EOPNOTSUPP;


    if (local->ops->get_survey)
        ret = local->ops->get_survey(&local->hw, idx, survey);


    return ret;
}

static inline void drv_rfkill_poll(struct ieee80211_local *local)
{
    might_sleep();

    if (local->ops->rfkill_poll)
        local->ops->rfkill_poll(&local->hw);
}

static inline void drv_flush(struct ieee80211_local *local,
                 struct ieee80211_sub_if_data *sdata,
                 u32 queues, bool drop)
{
    struct ieee80211_vif *vif = sdata ? &sdata->vif : NULL;

    might_sleep();

    if (sdata && !check_sdata_in_driver(sdata))
        return;

    if (local->ops->flush)
        local->ops->flush(&local->hw, vif, queues, drop);
}

static inline void drv_channel_switch(struct ieee80211_local *local,
                      struct ieee80211_sub_if_data *sdata,
                      struct ieee80211_channel_switch *ch_switch)
{
    might_sleep();

    local->ops->channel_switch(&local->hw, &sdata->vif, ch_switch);
}


static inline int drv_set_antenna(struct ieee80211_local *local,
                  u32 tx_ant, u32 rx_ant)
{
    int ret = -EOPNOTSUPP;
    might_sleep();
    if (local->ops->set_antenna)
        ret = local->ops->set_antenna(&local->hw, tx_ant, rx_ant);
    return ret;
}

static inline int drv_get_antenna(struct ieee80211_local *local,
                  u32 *tx_ant, u32 *rx_ant)
{
    int ret = -EOPNOTSUPP;
    might_sleep();
    if (local->ops->get_antenna)
        ret = local->ops->get_antenna(&local->hw, tx_ant, rx_ant);
    return ret;
}

static inline int drv_remain_on_channel(struct ieee80211_local *local,
                    struct ieee80211_sub_if_data *sdata,
                    struct ieee80211_channel *chan,
                    unsigned int duration,
                    enum ieee80211_roc_type type)
{
    int ret;

    might_sleep();

    ret = local->ops->remain_on_channel(&local->hw, &sdata->vif,
                        chan, duration, type);

    return ret;
}

static inline int
drv_cancel_remain_on_channel(struct ieee80211_local *local,
                 struct ieee80211_sub_if_data *sdata)
{
    int ret;

    might_sleep();

    ret = local->ops->cancel_remain_on_channel(&local->hw, &sdata->vif);

    return ret;
}

static inline int drv_set_ringparam(struct ieee80211_local *local,
                    u32 tx, u32 rx)
{
    int ret = -ENOTSUPP;

    might_sleep();

    if (local->ops->set_ringparam)
        ret = local->ops->set_ringparam(&local->hw, tx, rx);

    return ret;
}

static inline void drv_get_ringparam(struct ieee80211_local *local,
                     u32 *tx, u32 *tx_max, u32 *rx, u32 *rx_max)
{
    might_sleep();

    if (local->ops->get_ringparam)
        local->ops->get_ringparam(&local->hw, tx, tx_max, rx, rx_max);
}

static inline bool drv_tx_frames_pending(struct ieee80211_local *local)
{
    bool ret = false;

    might_sleep();

    if (local->ops->tx_frames_pending)
        ret = local->ops->tx_frames_pending(&local->hw);

    return ret;
}

static inline int drv_set_bitrate_mask(struct ieee80211_local *local,
                       struct ieee80211_sub_if_data *sdata,
                       const struct cfg80211_bitrate_mask *mask)
{
    int ret = -EOPNOTSUPP;

    might_sleep();

    if (!check_sdata_in_driver(sdata))
        return -EIO;

    if (local->ops->set_bitrate_mask)
        ret = local->ops->set_bitrate_mask(&local->hw,
                           &sdata->vif, mask);

    return ret;
}

static inline void drv_set_rekey_data(struct ieee80211_local *local,
                      struct ieee80211_sub_if_data *sdata,
                      struct cfg80211_gtk_rekey_data *data)
{
    if (!check_sdata_in_driver(sdata))
        return;

    if (local->ops->set_rekey_data)
        local->ops->set_rekey_data(&local->hw, &sdata->vif, data);
}

static inline void drv_event_callback(struct ieee80211_local *local,
                      struct ieee80211_sub_if_data *sdata,
                      const struct ieee80211_event *event)
{
    if (local->ops->event_callback)
        local->ops->event_callback(&local->hw, &sdata->vif, event);
}

static inline void
drv_release_buffered_frames(struct ieee80211_local *local,
                struct sta_info *sta, u16 tids, int num_frames,
                enum ieee80211_frame_release_type reason,
                bool more_data)
{
    if (local->ops->release_buffered_frames)
        local->ops->release_buffered_frames(&local->hw, &sta->sta, tids,
                            num_frames, reason,
                            more_data);
}

static inline void
drv_allow_buffered_frames(struct ieee80211_local *local,
              struct sta_info *sta, u16 tids, int num_frames,
              enum ieee80211_frame_release_type reason,
              bool more_data)
{
    if (local->ops->allow_buffered_frames)
        local->ops->allow_buffered_frames(&local->hw, &sta->sta,
                          tids, num_frames, reason,
                          more_data);
}

static inline void drv_mgd_prepare_tx(struct ieee80211_local *local,
                      struct ieee80211_sub_if_data *sdata,
                      struct ieee80211_prep_tx_info *info)
{
    might_sleep();

    if (!check_sdata_in_driver(sdata))
        return;
    WARN_ON_ONCE(sdata->vif.type != NL80211_IFTYPE_STATION);

    if (local->ops->mgd_prepare_tx)
        local->ops->mgd_prepare_tx(&local->hw, &sdata->vif, info);
}

static inline void drv_mgd_complete_tx(struct ieee80211_local *local,
                       struct ieee80211_sub_if_data *sdata,
                       struct ieee80211_prep_tx_info *info)
{
    might_sleep();

    if (!check_sdata_in_driver(sdata))
        return;
    WARN_ON_ONCE(sdata->vif.type != NL80211_IFTYPE_STATION);

    if (local->ops->mgd_complete_tx)
        local->ops->mgd_complete_tx(&local->hw, &sdata->vif, info);
}

static inline void
drv_mgd_protect_tdls_discover(struct ieee80211_local *local,
                  struct ieee80211_sub_if_data *sdata)
{
    might_sleep();

    if (!check_sdata_in_driver(sdata))
        return;
    WARN_ON_ONCE(sdata->vif.type != NL80211_IFTYPE_STATION);

    if (local->ops->mgd_protect_tdls_discover)
        local->ops->mgd_protect_tdls_discover(&local->hw, &sdata->vif);
}

static inline int drv_add_chanctx(struct ieee80211_local *local,
                  struct ieee80211_chanctx *ctx)
{
    int ret = -EOPNOTSUPP;

    might_sleep();

    if (local->ops->add_chanctx)
        ret = local->ops->add_chanctx(&local->hw, &ctx->conf);
    if (!ret)
        ctx->driver_present = true;

    return ret;
}

static inline void drv_remove_chanctx(struct ieee80211_local *local,
                      struct ieee80211_chanctx *ctx)
{
    might_sleep();

    if (WARN_ON(!ctx->driver_present))
        return;

    if (local->ops->remove_chanctx)
        local->ops->remove_chanctx(&local->hw, &ctx->conf);
    ctx->driver_present = false;
}

static inline void drv_change_chanctx(struct ieee80211_local *local,
                      struct ieee80211_chanctx *ctx,
                      u32 changed)
{
    might_sleep();

    if (local->ops->change_chanctx) {
        WARN_ON_ONCE(!ctx->driver_present);
        local->ops->change_chanctx(&local->hw, &ctx->conf, changed);
    }
}

static inline int drv_assign_vif_chanctx(struct ieee80211_local *local,
                     struct ieee80211_sub_if_data *sdata,
                     struct ieee80211_chanctx *ctx)
{
    int ret = 0;

    if (!check_sdata_in_driver(sdata))
        return -EIO;

    if (local->ops->assign_vif_chanctx) {
        WARN_ON_ONCE(!ctx->driver_present);
        ret = local->ops->assign_vif_chanctx(&local->hw,
                             &sdata->vif,
                             &ctx->conf);
    }

    return ret;
}

static inline void drv_unassign_vif_chanctx(struct ieee80211_local *local,
                        struct ieee80211_sub_if_data *sdata,
                        struct ieee80211_chanctx *ctx)
{
    might_sleep();

    if (!check_sdata_in_driver(sdata))
        return;

    if (local->ops->unassign_vif_chanctx) {
        WARN_ON_ONCE(!ctx->driver_present);
        local->ops->unassign_vif_chanctx(&local->hw,
                         &sdata->vif,
                         &ctx->conf);
    }
}

int drv_switch_vif_chanctx(struct ieee80211_local *local,
               struct ieee80211_vif_chanctx_switch *vifs,
               int n_vifs, enum ieee80211_chanctx_switch_mode mode);

static inline int drv_start_ap(struct ieee80211_local *local,
                   struct ieee80211_sub_if_data *sdata)
{
    int ret = 0;

    might_sleep();

    if (!check_sdata_in_driver(sdata))
        return -EIO;

    if (local->ops->start_ap)
        ret = local->ops->start_ap(&local->hw, &sdata->vif);
    return ret;
}

static inline void drv_stop_ap(struct ieee80211_local *local,
                   struct ieee80211_sub_if_data *sdata)
{
    if (!check_sdata_in_driver(sdata))
        return;

    if (local->ops->stop_ap)
        local->ops->stop_ap(&local->hw, &sdata->vif);
}

static inline void
drv_reconfig_complete(struct ieee80211_local *local,
              enum ieee80211_reconfig_type reconfig_type)
{
    might_sleep();

    if (local->ops->reconfig_complete)
        local->ops->reconfig_complete(&local->hw, reconfig_type);
}

static inline void
drv_set_default_unicast_key(struct ieee80211_local *local,
                struct ieee80211_sub_if_data *sdata,
                int key_idx)
{
    if (!check_sdata_in_driver(sdata))
        return;

    WARN_ON_ONCE(key_idx < -1 || key_idx > 3);

    if (local->ops->set_default_unicast_key)
        local->ops->set_default_unicast_key(&local->hw, &sdata->vif,
                            key_idx);
}

#if IS_ENABLED(CONFIG_IPV6)
static inline void drv_ipv6_addr_change(struct ieee80211_local *local,
                    struct ieee80211_sub_if_data *sdata,
                    struct inet6_dev *idev)
{
    if (local->ops->ipv6_addr_change)
        local->ops->ipv6_addr_change(&local->hw, &sdata->vif, idev);
}
#endif

static inline void
drv_channel_switch_beacon(struct ieee80211_sub_if_data *sdata,
              struct cfg80211_chan_def *chandef)
{
    struct ieee80211_local *local = sdata->local;

    if (local->ops->channel_switch_beacon) {
        local->ops->channel_switch_beacon(&local->hw, &sdata->vif,
                          chandef);
    }
}

static inline int
drv_pre_channel_switch(struct ieee80211_sub_if_data *sdata,
               struct ieee80211_channel_switch *ch_switch)
{
    struct ieee80211_local *local = sdata->local;
    int ret = 0;

    if (!check_sdata_in_driver(sdata))
        return -EIO;

    if (local->ops->pre_channel_switch)
        ret = local->ops->pre_channel_switch(&local->hw, &sdata->vif,
                             ch_switch);
    return ret;
}

static inline int
drv_post_channel_switch(struct ieee80211_sub_if_data *sdata)
{
    struct ieee80211_local *local = sdata->local;
    int ret = 0;

    if (!check_sdata_in_driver(sdata))
        return -EIO;

    if (local->ops->post_channel_switch)
        ret = local->ops->post_channel_switch(&local->hw, &sdata->vif);
    return ret;
}

static inline void
drv_abort_channel_switch(struct ieee80211_sub_if_data *sdata)
{
    struct ieee80211_local *local = sdata->local;

    if (!check_sdata_in_driver(sdata))
        return;

    if (local->ops->abort_channel_switch)
        local->ops->abort_channel_switch(&local->hw, &sdata->vif);
}

static inline void
drv_channel_switch_rx_beacon(struct ieee80211_sub_if_data *sdata,
                 struct ieee80211_channel_switch *ch_switch)
{
    struct ieee80211_local *local = sdata->local;

    if (!check_sdata_in_driver(sdata))
        return;

    if (local->ops->channel_switch_rx_beacon)
        local->ops->channel_switch_rx_beacon(&local->hw, &sdata->vif,
                             ch_switch);
}

static inline int drv_join_ibss(struct ieee80211_local *local,
                struct ieee80211_sub_if_data *sdata)
{
    int ret = 0;

    might_sleep();
    if (!check_sdata_in_driver(sdata))
        return -EIO;

    if (local->ops->join_ibss)
        ret = local->ops->join_ibss(&local->hw, &sdata->vif);
    return ret;
}

static inline void drv_leave_ibss(struct ieee80211_local *local,
                  struct ieee80211_sub_if_data *sdata)
{
    might_sleep();
    if (!check_sdata_in_driver(sdata))
        return;

    if (local->ops->leave_ibss)
        local->ops->leave_ibss(&local->hw, &sdata->vif);
}

static inline u32 drv_get_expected_throughput(struct ieee80211_local *local,
                          struct sta_info *sta)
{
    u32 ret = 0;

    if (local->ops->get_expected_throughput && sta->uploaded)
        ret = local->ops->get_expected_throughput(&local->hw, &sta->sta);

    return ret;
}

static inline int drv_get_txpower(struct ieee80211_local *local,
                  struct ieee80211_sub_if_data *sdata, int *dbm)
{
    int ret;

    if (!local->ops->get_txpower)
        return -EOPNOTSUPP;

    ret = local->ops->get_txpower(&local->hw, &sdata->vif, dbm);

    return ret;
}

static inline int
drv_tdls_channel_switch(struct ieee80211_local *local,
            struct ieee80211_sub_if_data *sdata,
            struct ieee80211_sta *sta, u8 oper_class,
            struct cfg80211_chan_def *chandef,
            struct sk_buff *tmpl_skb, u32 ch_sw_tm_ie)
{
    int ret;

    might_sleep();
    if (!check_sdata_in_driver(sdata))
        return -EIO;

    if (!local->ops->tdls_channel_switch)
        return -EOPNOTSUPP;

    ret = local->ops->tdls_channel_switch(&local->hw, &sdata->vif, sta,
                          oper_class, chandef, tmpl_skb,
                          ch_sw_tm_ie);
    return ret;
}

static inline void
drv_tdls_cancel_channel_switch(struct ieee80211_local *local,
                   struct ieee80211_sub_if_data *sdata,
                   struct ieee80211_sta *sta)
{
    might_sleep();
    if (!check_sdata_in_driver(sdata))
        return;

    if (!local->ops->tdls_cancel_channel_switch)
        return;

    local->ops->tdls_cancel_channel_switch(&local->hw, &sdata->vif, sta);
}

static inline void
drv_tdls_recv_channel_switch(struct ieee80211_local *local,
                 struct ieee80211_sub_if_data *sdata,
                 struct ieee80211_tdls_ch_sw_params *params)
{
    if (local->ops->tdls_recv_channel_switch)
        local->ops->tdls_recv_channel_switch(&local->hw, &sdata->vif,
                             params);
}

static inline void drv_wake_tx_queue(struct ieee80211_local *local,
                     struct txq_info *txq)
{
    struct ieee80211_sub_if_data *sdata = vif_to_sdata(txq->txq.vif);

    /* In reconfig don't transmit now, but mark for waking later */
    if (local->in_reconfig) {
        set_bit(IEEE80211_TXQ_STOP_NETIF_TX, &txq->flags);
        return;
    }

    if (!check_sdata_in_driver(sdata))
        return;

    local->ops->wake_tx_queue(&local->hw, &txq->txq);
}

static inline void schedule_and_wake_txq(struct ieee80211_local *local,
                     struct txq_info *txqi)
{
    ieee80211_schedule_txq(&local->hw, &txqi->txq);
    drv_wake_tx_queue(local, txqi);
}

static inline int drv_can_aggregate_in_amsdu(struct ieee80211_local *local,
                         struct sk_buff *head,
                         struct sk_buff *skb)
{
    if (!local->ops->can_aggregate_in_amsdu)
        return true;

    return local->ops->can_aggregate_in_amsdu(&local->hw, head, skb);
}

static inline int
drv_get_ftm_responder_stats(struct ieee80211_local *local,
                struct ieee80211_sub_if_data *sdata,
                struct cfg80211_ftm_responder_stats *ftm_stats)
{
    u32 ret = -EOPNOTSUPP;

    if (local->ops->get_ftm_responder_stats)
        ret = local->ops->get_ftm_responder_stats(&local->hw,
                             &sdata->vif,
                             ftm_stats);

    return ret;
}

static inline int drv_start_pmsr(struct ieee80211_local *local,
                 struct ieee80211_sub_if_data *sdata,
                 struct cfg80211_pmsr_request *request)
{
    int ret = -EOPNOTSUPP;

    might_sleep();
    if (!check_sdata_in_driver(sdata))
        return -EIO;

    if (local->ops->start_pmsr)
        ret = local->ops->start_pmsr(&local->hw, &sdata->vif, request);

    return ret;
}

static inline void drv_abort_pmsr(struct ieee80211_local *local,
                  struct ieee80211_sub_if_data *sdata,
                  struct cfg80211_pmsr_request *request)
{
    might_sleep();
    if (!check_sdata_in_driver(sdata))
        return;

    if (local->ops->abort_pmsr)
        local->ops->abort_pmsr(&local->hw, &sdata->vif, request);
}

static inline int drv_start_nan(struct ieee80211_local *local,
                struct ieee80211_sub_if_data *sdata,
                struct cfg80211_nan_conf *conf)
{
    int ret;

    might_sleep();
    check_sdata_in_driver(sdata);

    ret = local->ops->start_nan(&local->hw, &sdata->vif, conf);
    return ret;
}

static inline void drv_stop_nan(struct ieee80211_local *local,
                struct ieee80211_sub_if_data *sdata)
{
    might_sleep();
    check_sdata_in_driver(sdata);

    local->ops->stop_nan(&local->hw, &sdata->vif);
}

static inline int drv_nan_change_conf(struct ieee80211_local *local,
                       struct ieee80211_sub_if_data *sdata,
                       struct cfg80211_nan_conf *conf,
                       u32 changes)
{
    int ret;

    might_sleep();
    check_sdata_in_driver(sdata);

    if (!local->ops->nan_change_conf)
        return -EOPNOTSUPP;

    ret = local->ops->nan_change_conf(&local->hw, &sdata->vif, conf,
                      changes);
    return ret;
}

static inline int drv_add_nan_func(struct ieee80211_local *local,
                   struct ieee80211_sub_if_data *sdata,
                   const struct cfg80211_nan_func *nan_func)
{
    int ret;

    might_sleep();
    check_sdata_in_driver(sdata);

    if (!local->ops->add_nan_func)
        return -EOPNOTSUPP;

    ret = local->ops->add_nan_func(&local->hw, &sdata->vif, nan_func);

    return ret;
}

static inline void drv_del_nan_func(struct ieee80211_local *local,
                   struct ieee80211_sub_if_data *sdata,
                   u8 instance_id)
{
    might_sleep();
    check_sdata_in_driver(sdata);

    if (local->ops->del_nan_func)
        local->ops->del_nan_func(&local->hw, &sdata->vif, instance_id);
}

static inline int drv_set_tid_config(struct ieee80211_local *local,
                     struct ieee80211_sub_if_data *sdata,
                     struct ieee80211_sta *sta,
                     struct cfg80211_tid_config *tid_conf)
{
    int ret;

    might_sleep();
    ret = local->ops->set_tid_config(&local->hw, &sdata->vif, sta,
                     tid_conf);
    return ret;
}

static inline int drv_reset_tid_config(struct ieee80211_local *local,
                       struct ieee80211_sub_if_data *sdata,
                       struct ieee80211_sta *sta, u8 tids)
{
    int ret;

    might_sleep();
    ret = local->ops->reset_tid_config(&local->hw, &sdata->vif, sta, tids);

    return ret;
}

static inline void drv_update_vif_offload(struct ieee80211_local *local,
                      struct ieee80211_sub_if_data *sdata)
{
    might_sleep();
    check_sdata_in_driver(sdata);

    if (!local->ops->update_vif_offload)
        return;

    local->ops->update_vif_offload(&local->hw, &sdata->vif);
}

static inline void drv_sta_set_4addr(struct ieee80211_local *local,
                     struct ieee80211_sub_if_data *sdata,
                     struct ieee80211_sta *sta, bool enabled)
{
    sdata = get_bss_sdata(sdata);
    if (!check_sdata_in_driver(sdata))
        return;

    if (local->ops->sta_set_4addr)
        local->ops->sta_set_4addr(&local->hw, &sdata->vif, sta, enabled);
}

static inline void drv_sta_set_decap_offload(struct ieee80211_local *local,
                         struct ieee80211_sub_if_data *sdata,
                         struct ieee80211_sta *sta,
                         bool enabled)
{
    sdata = get_bss_sdata(sdata);
    if (!check_sdata_in_driver(sdata))
        return;

    if (local->ops->sta_set_decap_offload)
        local->ops->sta_set_decap_offload(&local->hw, &sdata->vif, sta,
                          enabled);
}

static inline void drv_add_twt_setup(struct ieee80211_local *local,
                     struct ieee80211_sub_if_data *sdata,
                     struct ieee80211_sta *sta,
                     struct ieee80211_twt_setup *twt)
{
    struct ieee80211_twt_params *twt_agrt;

    might_sleep();

    if (!check_sdata_in_driver(sdata))
        return;

    twt_agrt = (void *)twt->params;

    local->ops->add_twt_setup(&local->hw, sta, twt);
}

static inline void drv_twt_teardown_request(struct ieee80211_local *local,
                        struct ieee80211_sub_if_data *sdata,
                        struct ieee80211_sta *sta,
                        u8 flowid)
{
    might_sleep();
    if (!check_sdata_in_driver(sdata))
        return;

    if (!local->ops->twt_teardown_request)
        return;

    local->ops->twt_teardown_request(&local->hw, sta, flowid);
}

static inline int drv_net_fill_forward_path(struct ieee80211_local *local,
                        struct ieee80211_sub_if_data *sdata,
                        struct ieee80211_sta *sta,
                        struct net_device_path_ctx *ctx,
                        struct net_device_path *path)
{
    int ret = -EOPNOTSUPP;

    sdata = get_bss_sdata(sdata);
    if (!check_sdata_in_driver(sdata))
        return -EIO;

    if (local->ops->net_fill_forward_path)
        ret = local->ops->net_fill_forward_path(&local->hw,
                            &sdata->vif, sta,
                            ctx, path);

    return ret;
}

#endif /* __MAC80211_DRIVER_OPS */
