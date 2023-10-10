#include <linux/export.h>
#include <linux/etherdevice.h>
#include <net/mac80211.h>
#include <asm/unaligned.h>
#include "rdkfmac.h"
#include "rate.h"
#include "mesh.h"
#include "led.h"
#include "wme.h"

static int ieee80211_tx_radiotap_len(struct ieee80211_tx_info *info,
                     struct ieee80211_tx_status *status)
{
    struct ieee80211_rate_status *status_rate = NULL;
    int len = sizeof(struct ieee80211_radiotap_header);

    if (status && status->n_rates)
        status_rate = &status->rates[status->n_rates - 1];

    /* IEEE80211_RADIOTAP_RATE rate */
    if (status_rate && !(status_rate->rate_idx.flags &
                        (RATE_INFO_FLAGS_MCS |
                         RATE_INFO_FLAGS_DMG |
                         RATE_INFO_FLAGS_EDMG |
                         RATE_INFO_FLAGS_VHT_MCS |
                         RATE_INFO_FLAGS_HE_MCS)))
        len += 2;
    else if (info->status.rates[0].idx >= 0 &&
         !(info->status.rates[0].flags &
           (IEEE80211_TX_RC_MCS | IEEE80211_TX_RC_VHT_MCS)))
        len += 2;

    /* IEEE80211_RADIOTAP_TX_FLAGS */
    len += 2;

    /* IEEE80211_RADIOTAP_DATA_RETRIES */
    len += 1;

    /* IEEE80211_RADIOTAP_MCS
     * IEEE80211_RADIOTAP_VHT */
    if (status_rate) {
        if (status_rate->rate_idx.flags & RATE_INFO_FLAGS_MCS)
            len += 3;
        else if (status_rate->rate_idx.flags & RATE_INFO_FLAGS_VHT_MCS)
            len = ALIGN(len, 2) + 12;
        else if (status_rate->rate_idx.flags & RATE_INFO_FLAGS_HE_MCS)
            len = ALIGN(len, 2) + 12;
    } else if (info->status.rates[0].idx >= 0) {
        if (info->status.rates[0].flags & IEEE80211_TX_RC_MCS)
            len += 3;
        else if (info->status.rates[0].flags & IEEE80211_TX_RC_VHT_MCS)
            len = ALIGN(len, 2) + 12;
    }

    return len;
}

static void
ieee80211_add_tx_radiotap_header(struct ieee80211_local *local,
                 struct ieee80211_supported_band *sband,
                 struct sk_buff *skb, int retry_count,
                 int rtap_len, int shift,
                 struct ieee80211_tx_status *status)
{
    struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
    struct ieee80211_hdr *hdr = (struct ieee80211_hdr *) skb->data;
    struct ieee80211_radiotap_header *rthdr;
    struct ieee80211_rate_status *status_rate = NULL;
    unsigned char *pos;
    u16 legacy_rate = 0;
    u16 txflags;

    if (status && status->n_rates)
        status_rate = &status->rates[status->n_rates - 1];

    rthdr = skb_push(skb, rtap_len);

    memset(rthdr, 0, rtap_len);
    rthdr->it_len = cpu_to_le16(rtap_len);
    rthdr->it_present =
        cpu_to_le32(BIT(IEEE80211_RADIOTAP_TX_FLAGS) |
                BIT(IEEE80211_RADIOTAP_DATA_RETRIES));
    pos = (unsigned char *)(rthdr + 1);

    /*
     * XXX: Once radiotap gets the bitmap reset thing the vendor
     *  extensions proposal contains, we can actually report
     *  the whole set of tries we did.
     */

    /* IEEE80211_RADIOTAP_RATE */

    if (status_rate) {
        if (!(status_rate->rate_idx.flags &
                        (RATE_INFO_FLAGS_MCS |
                         RATE_INFO_FLAGS_DMG |
                         RATE_INFO_FLAGS_EDMG |
                         RATE_INFO_FLAGS_VHT_MCS |
                         RATE_INFO_FLAGS_HE_MCS)))
            legacy_rate = status_rate->rate_idx.legacy;
    } else if (info->status.rates[0].idx >= 0 &&
         !(info->status.rates[0].flags & (IEEE80211_TX_RC_MCS |
                          IEEE80211_TX_RC_VHT_MCS)))
        legacy_rate =
            sband->bitrates[info->status.rates[0].idx].bitrate;

    if (legacy_rate) {
        rthdr->it_present |= cpu_to_le32(BIT(IEEE80211_RADIOTAP_RATE));
        *pos = DIV_ROUND_UP(legacy_rate, 5 * (1 << shift));
        /* padding for tx flags */
        pos += 2;
    }

    /* IEEE80211_RADIOTAP_TX_FLAGS */
    txflags = 0;
    if (!(info->flags & IEEE80211_TX_STAT_ACK) &&
        !is_multicast_ether_addr(hdr->addr1))
        txflags |= IEEE80211_RADIOTAP_F_TX_FAIL;

    if (info->status.rates[0].flags & IEEE80211_TX_RC_USE_CTS_PROTECT)
        txflags |= IEEE80211_RADIOTAP_F_TX_CTS;
    if (info->status.rates[0].flags & IEEE80211_TX_RC_USE_RTS_CTS)
        txflags |= IEEE80211_RADIOTAP_F_TX_RTS;

    put_unaligned_le16(txflags, pos);
    pos += 2;

    /* IEEE80211_RADIOTAP_DATA_RETRIES */
    /* for now report the total retry_count */
    *pos = retry_count;
    pos++;

    if (status_rate && (status_rate->rate_idx.flags & RATE_INFO_FLAGS_MCS))
    {
        rthdr->it_present |= cpu_to_le32(BIT(IEEE80211_RADIOTAP_MCS));
        pos[0] = IEEE80211_RADIOTAP_MCS_HAVE_MCS |
             IEEE80211_RADIOTAP_MCS_HAVE_GI |
             IEEE80211_RADIOTAP_MCS_HAVE_BW;
        if (status_rate->rate_idx.flags & RATE_INFO_FLAGS_SHORT_GI)
            pos[1] |= IEEE80211_RADIOTAP_MCS_SGI;
        if (status_rate->rate_idx.bw == RATE_INFO_BW_40)
            pos[1] |= IEEE80211_RADIOTAP_MCS_BW_40;
        pos[2] = status_rate->rate_idx.mcs;
        pos += 3;
    } else if (status_rate && (status_rate->rate_idx.flags &
                    RATE_INFO_FLAGS_VHT_MCS))
    {
        u16 known = local->hw.radiotap_vht_details &
            (IEEE80211_RADIOTAP_VHT_KNOWN_GI |
             IEEE80211_RADIOTAP_VHT_KNOWN_BANDWIDTH);

        rthdr->it_present |= cpu_to_le32(BIT(IEEE80211_RADIOTAP_VHT));

        /* required alignment from rthdr */
        pos = (u8 *)rthdr + ALIGN(pos - (u8 *)rthdr, 2);

        /* u16 known - IEEE80211_RADIOTAP_VHT_KNOWN_* */
        put_unaligned_le16(known, pos);
        pos += 2;

        /* u8 flags - IEEE80211_RADIOTAP_VHT_FLAG_* */
        if (status_rate->rate_idx.flags & RATE_INFO_FLAGS_SHORT_GI)
            *pos |= IEEE80211_RADIOTAP_VHT_FLAG_SGI;
        pos++;

        /* u8 bandwidth */
        switch (status_rate->rate_idx.bw) {
        case RATE_INFO_BW_160:
            *pos = 11;
            break;
        case RATE_INFO_BW_80:
            *pos = 4;
            break;
        case RATE_INFO_BW_40:
            *pos = 1;
            break;
        default:
            *pos = 0;
            break;
        }
        pos++;

        /* u8 mcs_nss[4] */
        *pos = (status_rate->rate_idx.mcs << 4) |
                status_rate->rate_idx.nss;
        pos += 4;

        /* u8 coding */
        pos++;
        /* u8 group_id */
        pos++;
        /* u16 partial_aid */
        pos += 2;
    } else if (status_rate && (status_rate->rate_idx.flags &
                    RATE_INFO_FLAGS_HE_MCS))
    {
        struct ieee80211_radiotap_he *he;

        rthdr->it_present |= cpu_to_le32(BIT(IEEE80211_RADIOTAP_HE));

        /* required alignment from rthdr */
        pos = (u8 *)rthdr + ALIGN(pos - (u8 *)rthdr, 2);
        he = (struct ieee80211_radiotap_he *)pos;

        he->data1 = cpu_to_le16(IEEE80211_RADIOTAP_HE_DATA1_FORMAT_SU |
                    IEEE80211_RADIOTAP_HE_DATA1_DATA_MCS_KNOWN |
                    IEEE80211_RADIOTAP_HE_DATA1_DATA_DCM_KNOWN |
                    IEEE80211_RADIOTAP_HE_DATA1_BW_RU_ALLOC_KNOWN);

        he->data2 = cpu_to_le16(IEEE80211_RADIOTAP_HE_DATA2_GI_KNOWN);

#define HE_PREP(f, val) le16_encode_bits(val, IEEE80211_RADIOTAP_HE_##f)

        he->data6 |= HE_PREP(DATA6_NSTS, status_rate->rate_idx.nss);

#define CHECK_GI(s) \
    BUILD_BUG_ON(IEEE80211_RADIOTAP_HE_DATA5_GI_##s != \
    (int)NL80211_RATE_INFO_HE_GI_##s)

        CHECK_GI(0_8);
        CHECK_GI(1_6);
        CHECK_GI(3_2);

        he->data3 |= HE_PREP(DATA3_DATA_MCS, status_rate->rate_idx.mcs);
        he->data3 |= HE_PREP(DATA3_DATA_DCM, status_rate->rate_idx.he_dcm);

        he->data5 |= HE_PREP(DATA5_GI, status_rate->rate_idx.he_gi);

        switch (status_rate->rate_idx.bw) {
        case RATE_INFO_BW_20:
            he->data5 |= HE_PREP(DATA5_DATA_BW_RU_ALLOC,
                         IEEE80211_RADIOTAP_HE_DATA5_DATA_BW_RU_ALLOC_20MHZ);
            break;
        case RATE_INFO_BW_40:
            he->data5 |= HE_PREP(DATA5_DATA_BW_RU_ALLOC,
                         IEEE80211_RADIOTAP_HE_DATA5_DATA_BW_RU_ALLOC_40MHZ);
            break;
        case RATE_INFO_BW_80:
            he->data5 |= HE_PREP(DATA5_DATA_BW_RU_ALLOC,
                         IEEE80211_RADIOTAP_HE_DATA5_DATA_BW_RU_ALLOC_80MHZ);
            break;
        case RATE_INFO_BW_160:
            he->data5 |= HE_PREP(DATA5_DATA_BW_RU_ALLOC,
                         IEEE80211_RADIOTAP_HE_DATA5_DATA_BW_RU_ALLOC_160MHZ);
            break;
        case RATE_INFO_BW_HE_RU:
#define CHECK_RU_ALLOC(s) \
    BUILD_BUG_ON(IEEE80211_RADIOTAP_HE_DATA5_DATA_BW_RU_ALLOC_##s##T != \
    NL80211_RATE_INFO_HE_RU_ALLOC_##s + 4)

            CHECK_RU_ALLOC(26);
            CHECK_RU_ALLOC(52);
            CHECK_RU_ALLOC(106);
            CHECK_RU_ALLOC(242);
            CHECK_RU_ALLOC(484);
            CHECK_RU_ALLOC(996);
            CHECK_RU_ALLOC(2x996);

            he->data5 |= HE_PREP(DATA5_DATA_BW_RU_ALLOC,
                         status_rate->rate_idx.he_ru_alloc + 4);
            break;
        default:
            WARN_ONCE(1, "Invalid SU BW %d\n", status_rate->rate_idx.bw);
        }

        pos += sizeof(struct ieee80211_radiotap_he);
    }

    if (status_rate || info->status.rates[0].idx < 0)
        return;

    /* IEEE80211_RADIOTAP_MCS
     * IEEE80211_RADIOTAP_VHT */
    if (info->status.rates[0].flags & IEEE80211_TX_RC_MCS) {
        rthdr->it_present |= cpu_to_le32(BIT(IEEE80211_RADIOTAP_MCS));
        pos[0] = IEEE80211_RADIOTAP_MCS_HAVE_MCS |
             IEEE80211_RADIOTAP_MCS_HAVE_GI |
             IEEE80211_RADIOTAP_MCS_HAVE_BW;
        if (info->status.rates[0].flags & IEEE80211_TX_RC_SHORT_GI)
            pos[1] |= IEEE80211_RADIOTAP_MCS_SGI;
        if (info->status.rates[0].flags & IEEE80211_TX_RC_40_MHZ_WIDTH)
            pos[1] |= IEEE80211_RADIOTAP_MCS_BW_40;
        if (info->status.rates[0].flags & IEEE80211_TX_RC_GREEN_FIELD)
            pos[1] |= IEEE80211_RADIOTAP_MCS_FMT_GF;
        pos[2] = info->status.rates[0].idx;
        pos += 3;
    } else if (info->status.rates[0].flags & IEEE80211_TX_RC_VHT_MCS) {
        u16 known = local->hw.radiotap_vht_details &
            (IEEE80211_RADIOTAP_VHT_KNOWN_GI |
             IEEE80211_RADIOTAP_VHT_KNOWN_BANDWIDTH);

        rthdr->it_present |= cpu_to_le32(BIT(IEEE80211_RADIOTAP_VHT));

        /* required alignment from rthdr */
        pos = (u8 *)rthdr + ALIGN(pos - (u8 *)rthdr, 2);

        /* u16 known - IEEE80211_RADIOTAP_VHT_KNOWN_* */
        put_unaligned_le16(known, pos);
        pos += 2;

        /* u8 flags - IEEE80211_RADIOTAP_VHT_FLAG_* */
        if (info->status.rates[0].flags & IEEE80211_TX_RC_SHORT_GI)
            *pos |= IEEE80211_RADIOTAP_VHT_FLAG_SGI;
        pos++;

        /* u8 bandwidth */
        if (info->status.rates[0].flags & IEEE80211_TX_RC_40_MHZ_WIDTH)
            *pos = 1;
        else if (info->status.rates[0].flags & IEEE80211_TX_RC_80_MHZ_WIDTH)
            *pos = 4;
        else if (info->status.rates[0].flags & IEEE80211_TX_RC_160_MHZ_WIDTH)
            *pos = 11;
        else /* IEEE80211_TX_RC_{20_MHZ_WIDTH,FIXME:DUP_DATA} */
            *pos = 0;
        pos++;

        /* u8 mcs_nss[4] */
        *pos = (ieee80211_rate_get_vht_mcs(&info->status.rates[0]) << 4) |
            ieee80211_rate_get_vht_nss(&info->status.rates[0]);
        pos += 4;

        /* u8 coding */
        pos++;
        /* u8 group_id */
        pos++;
        /* u16 partial_aid */
        pos += 2;
    }
}

#define STA_LOST_PKT_THRESHOLD  50
#define STA_LOST_PKT_TIME       HZ      /* 1 sec since last ACK */
#define STA_LOST_TDLS_PKT_TIME  (10*HZ) /* 10secs since last ACK */

void ieee80211_tx_monitor(struct ieee80211_local *local, struct sk_buff *skb,
              struct ieee80211_supported_band *sband,
              int retry_count, int shift, bool send_to_cooked,
              struct ieee80211_tx_status *status)
{
    struct sk_buff *skb2;
    struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
    struct ieee80211_sub_if_data *sdata;
    struct net_device *prev_dev = NULL;
    int rtap_len;

    /* send frame to monitor interfaces now */
    rtap_len = ieee80211_tx_radiotap_len(info, status);
    if (WARN_ON_ONCE(skb_headroom(skb) < rtap_len)) {
        pr_err("ieee80211_tx_status: headroom too small\n");
        dev_kfree_skb(skb);
        return;
    }
    ieee80211_add_tx_radiotap_header(local, sband, skb, retry_count,
                     rtap_len, shift, status);

    /* XXX: is this sufficient for BPF? */
    skb_reset_mac_header(skb);
    skb->ip_summed = CHECKSUM_UNNECESSARY;
    skb->pkt_type = PACKET_OTHERHOST;
    skb->protocol = htons(ETH_P_802_2);
    memset(skb->cb, 0, sizeof(skb->cb));

    rcu_read_lock();
    list_for_each_entry_rcu(sdata, &local->interfaces, list) {
        if (sdata->vif.type == NL80211_IFTYPE_MONITOR) {
            if (!ieee80211_sdata_running(sdata))
                continue;

            if ((sdata->u.mntr.flags & MONITOR_FLAG_COOK_FRAMES) &&
                !send_to_cooked)
                continue;

            if (prev_dev) {
                skb2 = skb_clone(skb, GFP_ATOMIC);
                if (skb2) {
                    skb2->dev = prev_dev;
                    netif_rx(skb2);
                }
            }

            prev_dev = sdata->dev;
        }
    }
    if (prev_dev) {
        skb->dev = prev_dev;
        netif_rx(skb);
        skb = NULL;
    }
    rcu_read_unlock();
    dev_kfree_skb(skb);
}

void ieee80211_purge_tx_queue(struct ieee80211_hw *hw,
                  struct sk_buff_head *skbs)
{
    struct sk_buff *skb;

    while ((skb = __skb_dequeue(skbs)))
        ieee80211_free_txskb(hw, skb);
}
