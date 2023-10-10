#include <linux/kernel.h>
#include <linux/rtnetlink.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "rate.h"
#include "rdkfmac.h"

struct rate_control_alg {
    struct list_head list;
    const struct rate_control_ops *ops;
};

static LIST_HEAD(rate_ctrl_algs);
static DEFINE_MUTEX(rate_ctrl_mutex);

static char *ieee80211_default_rc_algo = CONFIG_MAC80211_RC_DEFAULT;
module_param(ieee80211_default_rc_algo, charp, 0644);
MODULE_PARM_DESC(ieee80211_default_rc_algo,
         "Default rate control algorithm for mac80211 to use");

void rate_control_rate_init(struct sta_info *sta)
{
    struct ieee80211_local *local = sta->sdata->local;
    struct rate_control_ref *ref = sta->rate_ctrl;
    struct ieee80211_sta *ista = &sta->sta;
    void *priv_sta = sta->rate_ctrl_priv;
    struct ieee80211_supported_band *sband;
    struct ieee80211_chanctx_conf *chanctx_conf;

    ieee80211_sta_set_rx_nss(sta);

    if (!ref)
        return;

    rcu_read_lock();

    chanctx_conf = rcu_dereference(sta->sdata->vif.bss_conf.chanctx_conf);
    if (WARN_ON(!chanctx_conf)) {
        rcu_read_unlock();
        return;
    }

    sband = local->hw.wiphy->bands[chanctx_conf->def.chan->band];

    /* TODO: check for minstrel_s1g ? */
    if (sband->band == NL80211_BAND_S1GHZ) {
        ieee80211_s1g_sta_rate_init(sta);
        rcu_read_unlock();
        return;
    }

    spin_lock_bh(&sta->rate_ctrl_lock);
    ref->ops->rate_init(ref->priv, sband, &chanctx_conf->def, ista,
                priv_sta);
    spin_unlock_bh(&sta->rate_ctrl_lock);
    rcu_read_unlock();
    set_sta_flag(sta, WLAN_STA_RATE_CONTROL);
}

void rate_control_tx_status(struct ieee80211_local *local,
                struct ieee80211_supported_band *sband,
                struct ieee80211_tx_status *st)
{
    struct rate_control_ref *ref = local->rate_ctrl;
    struct sta_info *sta = container_of(st->sta, struct sta_info, sta);
    void *priv_sta = sta->rate_ctrl_priv;

    if (!ref || !test_sta_flag(sta, WLAN_STA_RATE_CONTROL))
        return;

    spin_lock_bh(&sta->rate_ctrl_lock);
    if (ref->ops->tx_status_ext)
        ref->ops->tx_status_ext(ref->priv, sband, priv_sta, st);
    else if (st->skb)
        ref->ops->tx_status(ref->priv, sband, st->sta, priv_sta, st->skb);
    else
        WARN_ON_ONCE(1);

    spin_unlock_bh(&sta->rate_ctrl_lock);
}

void rate_control_rate_update(struct ieee80211_local *local,
                    struct ieee80211_supported_band *sband,
                    struct sta_info *sta, u32 changed)
{
    struct rate_control_ref *ref = local->rate_ctrl;
    struct ieee80211_sta *ista = &sta->sta;
    void *priv_sta = sta->rate_ctrl_priv;
    struct ieee80211_chanctx_conf *chanctx_conf;

    if (ref && ref->ops->rate_update) {
        rcu_read_lock();

        chanctx_conf = rcu_dereference(sta->sdata->vif.bss_conf.chanctx_conf);
        if (WARN_ON(!chanctx_conf)) {
            rcu_read_unlock();
            return;
        }

        spin_lock_bh(&sta->rate_ctrl_lock);
        ref->ops->rate_update(ref->priv, sband, &chanctx_conf->def,
                      ista, priv_sta, changed);
        spin_unlock_bh(&sta->rate_ctrl_lock);
        rcu_read_unlock();
    }
    drv_sta_rc_update(local, sta->sdata, &sta->sta, changed);
}

static const struct rate_control_ops *
ieee80211_try_rate_control_ops_get(const char *name)
{
    struct rate_control_alg *alg;
    const struct rate_control_ops *ops = NULL;

    if (!name)
        return NULL;

    mutex_lock(&rate_ctrl_mutex);
    list_for_each_entry(alg, &rate_ctrl_algs, list) {
        if (!strcmp(alg->ops->name, name)) {
            ops = alg->ops;
            break;
        }
    }
    mutex_unlock(&rate_ctrl_mutex);
    return ops;
}

/* Get the rate control algorithm. */
static const struct rate_control_ops *
ieee80211_rate_control_ops_get(const char *name)
{
    const struct rate_control_ops *ops;
    const char *alg_name;

    kernel_param_lock(THIS_MODULE);
    if (!name)
        alg_name = ieee80211_default_rc_algo;
    else
        alg_name = name;

    ops = ieee80211_try_rate_control_ops_get(alg_name);
    if (!ops && name)
        /* try default if specific alg requested but not found */
        ops = ieee80211_try_rate_control_ops_get(ieee80211_default_rc_algo);

    /* Note: check for > 0 is intentional to avoid clang warning */
    if (!ops && (strlen(CONFIG_MAC80211_RC_DEFAULT) > 0))
        /* try built-in one if specific alg requested but not found */
        ops = ieee80211_try_rate_control_ops_get(CONFIG_MAC80211_RC_DEFAULT);

    kernel_param_unlock(THIS_MODULE);

    return ops;
}

static struct rate_control_ref *
rate_control_alloc(const char *name, struct ieee80211_local *local)
{
    struct rate_control_ref *ref;

    ref = kmalloc(sizeof(struct rate_control_ref), GFP_KERNEL);
    if (!ref)
        return NULL;
    ref->ops = ieee80211_rate_control_ops_get(name);
    if (!ref->ops)
        goto free;

    ref->priv = ref->ops->alloc(&local->hw);
    if (!ref->priv)
        goto free;
    return ref;

free:
    kfree(ref);
    return NULL;
}

static void rate_control_free(struct ieee80211_local *local,
                  struct rate_control_ref *ctrl_ref)
{
    ctrl_ref->ops->free(ctrl_ref->priv);

    kfree(ctrl_ref);
}

void ieee80211_check_rate_mask(struct ieee80211_sub_if_data *sdata)
{
    struct ieee80211_local *local = sdata->local;
    struct ieee80211_supported_band *sband;
    u32 user_mask, basic_rates = sdata->vif.bss_conf.basic_rates;
    enum nl80211_band band;

    if (WARN_ON(!sdata->vif.bss_conf.chandef.chan))
        return;

    band = sdata->vif.bss_conf.chandef.chan->band;
    if (band == NL80211_BAND_S1GHZ) {
        /* TODO */
        return;
    }

    if (WARN_ON_ONCE(!basic_rates))
        return;

    user_mask = sdata->rc_rateidx_mask[band];
    sband = local->hw.wiphy->bands[band];

    if (user_mask & basic_rates)
        return;

    sdata_dbg(sdata,
          "no overlap between basic rates (0x%x) and user mask (0x%x on band %d) - clearing the latter",
          basic_rates, user_mask, band);
    sdata->rc_rateidx_mask[band] = (1 << sband->n_bitrates) - 1;
}

static bool rc_no_data_or_no_ack_use_min(struct ieee80211_tx_rate_control *txrc)
{
    struct sk_buff *skb = txrc->skb;
    struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);

    return (info->flags & (IEEE80211_TX_CTL_NO_ACK |
                   IEEE80211_TX_CTL_USE_MINRATE)) ||
        !ieee80211_is_tx_data(skb);
}

static void rc_send_low_basicrate(struct ieee80211_tx_rate *rate,
                  u32 basic_rates,
                  struct ieee80211_supported_band *sband)
{
    u8 i;

    if (sband->band == NL80211_BAND_S1GHZ) {
        /* TODO */
        rate->flags |= IEEE80211_TX_RC_S1G_MCS;
        rate->idx = 0;
        return;
    }

    if (basic_rates == 0)
        return; /* assume basic rates unknown and accept rate */
    if (rate->idx < 0)
        return;
    if (basic_rates & (1 << rate->idx))
        return; /* selected rate is a basic rate */

    for (i = rate->idx + 1; i <= sband->n_bitrates; i++) {
        if (basic_rates & (1 << i)) {
            rate->idx = i;
            return;
        }
    }

    /* could not find a basic rate; use original selection */
}

static void __rate_control_send_low(struct ieee80211_hw *hw,
                    struct ieee80211_supported_band *sband,
                    struct ieee80211_sta *sta,
                    struct ieee80211_tx_info *info,
                    u32 rate_mask)
{
    int i;
    u32 rate_flags =
        ieee80211_chandef_rate_flags(&hw->conf.chandef);

    if (sband->band == NL80211_BAND_S1GHZ) {
        info->control.rates[0].flags |= IEEE80211_TX_RC_S1G_MCS;
        info->control.rates[0].idx = 0;
        return;
    }

    if ((sband->band == NL80211_BAND_2GHZ) &&
        (info->flags & IEEE80211_TX_CTL_NO_CCK_RATE))
        rate_flags |= IEEE80211_RATE_ERP_G;

    info->control.rates[0].idx = 0;
    for (i = 0; i < sband->n_bitrates; i++) {
        if (!(rate_mask & BIT(i)))
            continue;

        if ((rate_flags & sband->bitrates[i].flags) != rate_flags)
            continue;

        if (!rate_supported(sta, sband->band, i))
            continue;

        info->control.rates[0].idx = i;
        break;
    }
    WARN_ONCE(i == sband->n_bitrates,
          "no supported rates for sta %pM (0x%x, band %d) in rate_mask 0x%x with flags 0x%x\n",
          sta ? sta->addr : NULL,
          sta ? sta->deflink.supp_rates[sband->band] : -1,
          sband->band,
          rate_mask, rate_flags);

    info->control.rates[0].count =
        (info->flags & IEEE80211_TX_CTL_NO_ACK) ?
        1 : hw->max_rate_tries;

    info->control.skip_table = 1;
}


static bool rate_control_send_low(struct ieee80211_sta *pubsta,
                  struct ieee80211_tx_rate_control *txrc)
{
    struct ieee80211_tx_info *info = IEEE80211_SKB_CB(txrc->skb);
    struct ieee80211_supported_band *sband = txrc->sband;
    struct sta_info *sta;
    int mcast_rate;
    bool use_basicrate = false;

    if (!pubsta || rc_no_data_or_no_ack_use_min(txrc)) {
        __rate_control_send_low(txrc->hw, sband, pubsta, info,
                    txrc->rate_idx_mask);

        if (!pubsta && txrc->bss) {
            mcast_rate = txrc->bss_conf->mcast_rate[sband->band];
            if (mcast_rate > 0) {
                info->control.rates[0].idx = mcast_rate - 1;
                return true;
            }
            use_basicrate = true;
        } else if (pubsta) {
            sta = container_of(pubsta, struct sta_info, sta);
            if (ieee80211_vif_is_mesh(&sta->sdata->vif))
                use_basicrate = true;
        }

        if (use_basicrate)
            rc_send_low_basicrate(&info->control.rates[0],
                          txrc->bss_conf->basic_rates,
                          sband);

        return true;
    }
    return false;
}

void rate_control_get_rate(struct ieee80211_sub_if_data *sdata,
               struct sta_info *sta,
               struct ieee80211_tx_rate_control *txrc)
{
    struct rate_control_ref *ref = sdata->local->rate_ctrl;
    void *priv_sta = NULL;
    struct ieee80211_sta *ista = NULL;
    struct ieee80211_tx_info *info = IEEE80211_SKB_CB(txrc->skb);
    int i;

    for (i = 0; i < IEEE80211_TX_MAX_RATES; i++) {
        info->control.rates[i].idx = -1;
        info->control.rates[i].flags = 0;
        info->control.rates[i].count = 0;
    }

    if (rate_control_send_low(sta ? &sta->sta : NULL, txrc))
        return;

    if (ieee80211_hw_check(&sdata->local->hw, HAS_RATE_CONTROL))
        return;

    if (sta && test_sta_flag(sta, WLAN_STA_RATE_CONTROL)) {
        ista = &sta->sta;
        priv_sta = sta->rate_ctrl_priv;
    }

    if (ista) {
        spin_lock_bh(&sta->rate_ctrl_lock);
        ref->ops->get_rate(ref->priv, ista, priv_sta, txrc);
        spin_unlock_bh(&sta->rate_ctrl_lock);
    } else {
        rate_control_send_low(NULL, txrc);
    }

    if (ieee80211_hw_check(&sdata->local->hw, SUPPORTS_RC_TABLE))
        return;

    ieee80211_get_tx_rates(&sdata->vif, ista, txrc->skb,
                   info->control.rates,
                   ARRAY_SIZE(info->control.rates));
}

int ieee80211_init_rate_ctrl_alg(struct ieee80211_local *local,
                 const char *name)
{
    struct rate_control_ref *ref;

    ASSERT_RTNL();

    if (local->open_count)
        return -EBUSY;

    if (ieee80211_hw_check(&local->hw, HAS_RATE_CONTROL)) {
        if (WARN_ON(!local->ops->set_rts_threshold))
            return -EINVAL;
        return 0;
    }

    ref = rate_control_alloc(name, local);
    if (!ref) {
        wiphy_warn(local->hw.wiphy,
               "Failed to select rate control algorithm\n");
        return -ENOENT;
    }

    WARN_ON(local->rate_ctrl);
    local->rate_ctrl = ref;

    wiphy_debug(local->hw.wiphy, "Selected rate control algorithm '%s'\n",
            ref->ops->name);

    return 0;
}

void rate_control_deinitialize(struct ieee80211_local *local)
{
    struct rate_control_ref *ref;

    ref = local->rate_ctrl;

    if (!ref)
        return;

    local->rate_ctrl = NULL;
    rate_control_free(local, ref);
}
