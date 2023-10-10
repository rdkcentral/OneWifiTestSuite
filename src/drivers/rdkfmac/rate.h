
#ifndef IEEE80211_RATE_H
#define IEEE80211_RATE_H

#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/types.h>
#include <net/mac80211.h>

#include "rdkfmac.h"
#include "driver-ops.h"

struct rate_control_ref {
    const struct rate_control_ops *ops;
    void *priv;
};

void rate_control_get_rate(struct ieee80211_sub_if_data *sdata,
               struct sta_info *sta,
               struct ieee80211_tx_rate_control *txrc);

void rate_control_tx_status(struct ieee80211_local *local,
                struct ieee80211_supported_band *sband,
                struct ieee80211_tx_status *st);

void rate_control_rate_init(struct sta_info *sta);
void rate_control_rate_update(struct ieee80211_local *local,
                    struct ieee80211_supported_band *sband,
                    struct sta_info *sta, u32 changed);

static inline void *rate_control_alloc_sta(struct rate_control_ref *ref,
                       struct sta_info *sta, gfp_t gfp)
{
    spin_lock_init(&sta->rate_ctrl_lock);
    return ref->ops->alloc_sta(ref->priv, &sta->sta, gfp);
}

static inline void rate_control_free_sta(struct sta_info *sta)
{
    struct rate_control_ref *ref = sta->rate_ctrl;
    struct ieee80211_sta *ista = &sta->sta;
    void *priv_sta = sta->rate_ctrl_priv;

    ref->ops->free_sta(ref->priv, ista, priv_sta);
}

static inline void rate_control_add_sta_debugfs(struct sta_info *sta)
{
}

extern const struct file_operations rcname_ops;

static inline void rate_control_add_debugfs(struct ieee80211_local *local)
{
}

void ieee80211_check_rate_mask(struct ieee80211_sub_if_data *sdata);

/* Get a reference to the rate control algorithm. If `name' is NULL, get the
 * first available algorithm. */
int ieee80211_init_rate_ctrl_alg(struct ieee80211_local *local,
                 const char *name);
void rate_control_deinitialize(struct ieee80211_local *local);


static inline int rc80211_minstrel_init(void)
{
    return 0;
}
static inline void rc80211_minstrel_exit(void)
{
}

#endif /* IEEE80211_RATE_H */
