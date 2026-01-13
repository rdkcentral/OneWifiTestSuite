/**
 * Copyright 2026 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef WLAN_EMU_MSG_DATA_H
#define WLAN_EMU_MSG_DATA_H

#include <stdint.h>
#include "ieee80211_radiotap.h"
#include "wifi_hal.h"
#include "wlan_emu_common.h"
#include <linux/types.h>
#include "nl80211_copy.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WLAN_EMU_INTF_NAME_SZ   64
#define WLAN_EMU_BEACON_SZ      4096
#define ETH_ALEN 6

typedef enum {
    wlan_emu_emu80211_ctrl_tstart,
    wlan_emu_emu80211_ctrl_tstop,
} wlan_emu_emu80211_ctrl_type_t;

typedef enum {
    wlan_emu_emu80211_cmd_radiotap,
    wlan_emu_emu80211_cmd_mac_update,
    wlan_emu_emu80211_cmd_frame_auth_req,
    wlan_emu_emu80211_cmd_frame_assoc_req
} wlan_emu_emu80211_cmd_type_t;

typedef struct {
    char name[WLAN_EMU_INTF_NAME_SZ];
} wlan_emu_wiphy_t;

typedef struct {
    unsigned char data[WLAN_EMU_BEACON_SZ];
} wlan_emu_beacon_data_t;

typedef struct {
    int ifindex;
    int phy_index;
    char name[WLAN_EMU_INTF_NAME_SZ];
    uint64_t wdev_id;
    enum nl80211_iftype type;
    uint8_t macaddr[ETH_ALEN];
    int generation;
    uint8_t use_4addr;
    int freq;
    int center_freq1;
    int width;
} wlan_emu_cfg80211_add_intf_t;

typedef struct {
    int ifindex;
    int phy_index;
} wlan_emu_cfg80211_del_intf_t;

typedef struct {
    int ifindex;
    int phy_index;
    enum nl80211_iftype type;
} wlan_emu_cfg80211_change_intf_t;

typedef struct {
    int ifindex;
    int phy_index;
    mac_address_t macaddr;
    char *beacon_head;
    size_t head_len;
    char *beacon_tail;
    size_t tail_len;
} wlan_emu_cfg80211_start_ap_t;

typedef struct {
    int ifindex;
    int phy_index;
} wlan_emu_cfg80211_change_beacon_t;

typedef struct {
    //    wlan_emu_wiphy_t wiphy;
    int ifindex;
} wlan_emu_cfg80211_stop_ap_t;

typedef struct {
    int ifindex;
    int phy_index;
    int freq;
    int center_freq1;
    int center_freq2;
    int width;
} wlan_emu_cfg80211_set_wiphy_t;

struct radiotap_header {
    struct ieee80211_radiotap_header rt_hdr;
    uint8_t dbm_signal;
    uint8_t dbm_noise;
    // Add more fields as needed
} __attribute__((packed));

typedef struct {
    char name[WLAN_EMU_INTF_NAME_SZ];
} wlan_emu_hw_t;

typedef struct {
    char name[WLAN_EMU_INTF_NAME_SZ];
} wlan_emu_vif_t;

typedef struct {
    wlan_emu_hw_t hw;
} wlan_emu_mac80211_tx_t;

typedef struct {
    unsigned char macaddr[ETH_ALEN];
    unsigned char client_macaddr[ETH_ALEN];
    unsigned char *frame;
    unsigned int frame_len;
} wlan_emu_frm80211_frm_t;

typedef struct {
    wlan_emu_hw_t hw;
} wlan_emu_mac80211_start_t;

typedef struct {
    wlan_emu_hw_t hw;
} wlan_emu_mac80211_stop_t;

typedef struct {
    wlan_emu_hw_t hw;
} wlan_emu_mac80211_add_intf_t;

typedef struct {
    wlan_emu_hw_t hw;
} wlan_emu_mac80211_change_intf_t;

typedef struct {
    wlan_emu_hw_t hw;
} wlan_emu_mac80211_remove_intf_t;

typedef struct {
    wlan_emu_hw_t hw;
} wlan_emu_mac80211_config_t;

typedef struct {
    wlan_emu_hw_t hw;
} wlan_emu_mac80211_configure_filter_t;

typedef struct {
    wlan_emu_hw_t hw;
} wlan_emu_mac80211_bss_info_changed_t;

typedef struct {
    wlan_emu_hw_t hw;
} wlan_emu_mac80211_start_ap_t;

typedef struct {
    wlan_emu_hw_t hw;
} wlan_emu_mac80211_stop_ap_t;

typedef struct {
    wlan_emu_emu80211_ctrl_type_t ctrl;
    wlan_emu_test_coverage_t coverage;
    wlan_emu_test_type_t type;
} wlan_emu_emu80211_ctrl_t;

typedef struct {
    int fd;
} wlan_emu_emu80211_close_t;

typedef struct {
    wlan_emu_emu80211_cmd_type_t type;
    unsigned char cmd_buffer[1024];
    unsigned int buff_len;
} wlan_emu_emu80211_command_t;

typedef enum {
    wlan_emu_mac80211_ops_type_none,
    wlan_emu_mac80211_ops_type_tx,
    wlan_emu_mac80211_ops_type_start,
    wlan_emu_mac80211_ops_type_stop,
    wlan_emu_mac80211_ops_type_add_intf,
    wlan_emu_mac80211_ops_type_change_intf,
    wlan_emu_mac80211_ops_type_remove_intf,
    wlan_emu_mac80211_ops_type_config,
    wlan_emu_mac80211_ops_type_configure_filter,
    wlan_emu_mac80211_ops_type_bss_info_changed,
    wlan_emu_mac80211_ops_type_start_ap,
    wlan_emu_mac80211_ops_type_stop_ap,
    wlan_emu_mac80211_ops_type_unknown
} wlan_emu_mac80211_ops_type_t;

typedef enum {
    wlan_emu_emu80211_ops_type_none,
    wlan_emu_emu80211_ops_type_tctrl,
    wlan_emu_emu80211_ops_type_close,
    wlan_emu_emu80211_ops_type_cmnd,
    wlan_emu_emu80211_ops_type_unnown
} wlan_emu_emu80211_ops_type_t;

typedef struct {
    wlan_emu_emu80211_ops_type_t ops;

    union {
        wlan_emu_emu80211_ctrl_t ctrl;
        wlan_emu_emu80211_close_t close;
        wlan_emu_emu80211_command_t cmd;
    } u;
} wlan_emu_msg_emu80211_t;

typedef struct {
    wlan_emu_frm80211_ops_type_t ops;

    union {
        wlan_emu_frm80211_frm_t frame;
    } u;
} wlan_emu_msg_frm80211_t;

typedef struct {
    wlan_emu_cfg80211_ops_type_t ops;

    union {
        wlan_emu_cfg80211_add_intf_t add_intf;
        wlan_emu_cfg80211_del_intf_t del_intf;
        wlan_emu_cfg80211_change_intf_t change_intf;
        wlan_emu_cfg80211_start_ap_t start_ap;
        wlan_emu_cfg80211_change_beacon_t change_beacon;
        wlan_emu_cfg80211_stop_ap_t stop_ap;
        wlan_emu_cfg80211_set_wiphy_t set_wiphy;
    } u;
} wlan_emu_msg_cfg80211_t;

typedef struct {
    wlan_emu_mac80211_ops_type_t ops;

    union {
        wlan_emu_mac80211_tx_t tx;
        wlan_emu_mac80211_start_t start;
        wlan_emu_mac80211_stop_t stop;
        wlan_emu_mac80211_add_intf_t add_intf;
        wlan_emu_mac80211_change_intf_t change_intf;
        wlan_emu_mac80211_remove_intf_t remove_intf;
        wlan_emu_mac80211_config_t config;
        wlan_emu_mac80211_bss_info_changed_t bss_info_changed;
        wlan_emu_mac80211_start_ap_t start_ap;
        wlan_emu_mac80211_stop_ap_t stop_ap;
    } u;
} wlan_emu_msg_mac80211_t;

typedef struct {
    webconfig_subdoc_type_t subdoc_type;
} wlan_emu_msg_type_webconfig_t;

typedef enum {
    wlan_emu_msg_agent_ops_type_cmd,
    wlan_emu_msg_agent_ops_type_data
} wlan_emu_msg_agent_ops_t;

typedef enum {
    wlan_emu_msg_agent_cmd_type_start,
    wlan_emu_msg_agent_cmd_type_stop
} wlan_emu_msg_agent_cmd_t;

typedef struct {
    wlan_emu_msg_agent_ops_t ops;

    union {
        wlan_emu_msg_agent_cmd_t cmd;
        void *buf;
    } u;
} wlan_emu_msg_type_agent_msg_t;

typedef struct {
    wlan_emu_msg_type_t type;

    union {
        wlan_emu_msg_cfg80211_t cfg80211;
        wlan_emu_msg_mac80211_t mac80211;
        wlan_emu_msg_emu80211_t emu80211;
        wlan_emu_msg_frm80211_t frm80211;
        wlan_emu_msg_type_webconfig_t ow_webconfig;
        wlan_emu_msg_type_agent_msg_t agent_msg;
    } u;
} wlan_emu_msg_data_t;

#ifdef __cplusplus
}
#endif

#endif
