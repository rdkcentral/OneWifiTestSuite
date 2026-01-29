/**
 * Copyright 2025 Comcast Cable Communications Management, LLC
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

#include "wlan_emu_sta_mgr.h"
#include "cci_wifi_utils.hpp"
#include "utils/common.h"
#include "common/defs.h"
#include "common/ieee802_11_defs.h"
#include "common/wpa_common.h"
#include "radiotap_iter.h"
#include "wlan_emu_log.h"
#include "wlan_emu_sta.h"
#include <assert.h>
#include <pcap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
INT wifi_hal_createVAP(wifi_radio_index_t index, wifi_vap_info_map_t *map);
INT wifi_hal_connect(INT ap_index, wifi_bss_info_t *bss);
INT wifi_hal_disconnect(INT ap_index);
INT wifi_hal_getRadioVapInfoMap(wifi_radio_index_t index, wifi_vap_info_map_t *map);
INT wifi_hal_startScan(wifi_radio_index_t index, wifi_neighborScanMode_t scan_mode, INT dwell_time,
    UINT num, UINT *chan_list);
INT wifi_hal_setRadioOperatingParameters(wifi_radio_index_t index,
    wifi_radio_operationParam_t *operationParam);
int convert_channel_to_freq(int band, unsigned char chan);
}

static void ovs_fdb_flush(char *bridge_name)
{
    FILE *fp;
    char ovs_cmd[64] = { 0 };

    snprintf(ovs_cmd, sizeof(ovs_cmd), "ovs-appctl fdb/flush %s", bridge_name);

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: ovs_cmd : %s\n", __func__, __LINE__, ovs_cmd);
    fp = popen(ovs_cmd, "r");
    if (fp == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to flush ovs fdb cmd : %s\n",
            __func__, __LINE__, ovs_cmd);
        return;
    }

    pclose(fp);
}

int wlan_emu_sim_sta_mgr_t::find_first_free_dev()
{
    unsigned int i;
    sta_info_t *sta_info;

    for (i = 0; i < m_sta_info_count; i++) {
        sta_info = (sta_info_t *)queue_peek(m_sta_info_map, i);
        if (sta_info->status == sta_state_free) {
            return i;
        }
    }

    return -1;
}

void wlan_emu_sim_sta_mgr_t::set_dev_busy(unsigned int dev_id)
{
    sta_info_t *sta_info;
    sta_info = (sta_info_t *)queue_peek(m_sta_info_map, dev_id);
    sta_info->status = sta_state_in_use;
}

void wlan_emu_sim_sta_mgr_t::set_dev_free(unsigned int dev_id)
{
    sta_info_t *sta_info;
    sta_info = (sta_info_t *)queue_peek(m_sta_info_map, dev_id);
    sta_info->status = sta_state_free;
}

sta_info_t *wlan_emu_sim_sta_mgr_t::get_devid_sta_info(unsigned int dev_id)
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: dev_id : %d\n", __func__, __LINE__, dev_id);
    return (sta_info_t *)queue_peek(m_sta_info_map, dev_id);
}

void wlan_emu_sim_sta_mgr_t::send_heart_beat(char *key, heart_beat_data_t *heart_beat_data)
{
    wlan_emu_sta_t *sta;
    sta_info_t *sta_info = NULL;

    sta_info = get_devid_sta_info(get_dev_id_from_key(key));
    if (sta_info == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: sta_info is NULL\n", __func__, __LINE__);
        return;
    }

    sta = (wlan_emu_sta_t *)hash_map_get(m_sta_map, key);
    if (sta != NULL) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Sending heartbeat to the key : %s\n",
            __func__, __LINE__, key);
        sta->handle_heart_beat(heart_beat_data);
        send_raw_packet(dummy_data, sizeof(dummy_data), INADDR_ANY, DUMMY_PORT_IN, INADDR_BROADCAST,
            DUMMY_PORT_OUT, MAC_BCAST_ADDR, if_nametoindex(sta_info->interface_name));
    }
}

void add_to_bridge(char *interface, char *bridge)
{
    FILE *fp;
    char buf[BUFSIZ];
    char ovs_cmd[BUFSIZ];

    snprintf(ovs_cmd, BUFSIZ, "ovs-vsctl add-port %s %s", bridge, interface);

    if ((fp = popen(ovs_cmd, "r")) == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to add %s from bridge %s\n", __func__,
            __LINE__, interface, bridge);
        return;
    }

    pclose(fp);
};

void remove_from_bridge(char *interface, char *bridge)
{
    FILE *fp;
    char buf[BUFSIZ];
    char ovs_cmd[BUFSIZ];

    snprintf(ovs_cmd, BUFSIZ, "ovs-vsctl del-port %s %s", bridge, interface);

    if ((fp = popen(ovs_cmd, "r")) == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to remove %s from bridge %s\n",
            __func__, __LINE__, interface, bridge);
        return;
    }

    pclose(fp);
};

static void set_bridge_mac(char *bridge_name)
{
    FILE *fp;
    FILE *ptr;
    char buf[BUFSIZ];
    char ovs_cmd[BUFSIZ];
    char ifconf_cmd[BUFSIZ];
    char search_str[] = "HWaddr ";
    char *temp_str = NULL;

    snprintf(ifconf_cmd, BUFSIZ, "ifconfig %s", bridge_name);

    if ((ptr = popen(ifconf_cmd, "r")) != NULL) {
        size_t byte_count = fread(buf, 1, BUFSIZ - 1, ptr);
        buf[byte_count] = 0;

        temp_str = strstr(buf, "HWaddr ");
        if (temp_str == NULL) {
            wlan_emu_print(wlan_emu_log_level_err, " %s:%d No MAC found for %s", __func__, __LINE__,
                bridge_name);
            pclose(ptr);
            return;
        } else {
            temp_str += strlen(search_str);
        }
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Ifconfig failed for %s\n", __func__,
            __LINE__, bridge_name);
        return;
    }

    pclose(ptr);

    snprintf(ovs_cmd, BUFSIZ, "ovs-vsctl set bridge %s other_config:hwaddr=%s", bridge_name,
        temp_str);

    fp = popen(ovs_cmd, "r");

    if (fp == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to set mac for %s\n", __func__,
            __LINE__, bridge_name);
        return;
    }

    pclose(fp);
}

void wlan_emu_sim_sta_mgr_t::remove_all_sta(unsigned int test_id)
{
    wlan_emu_sta_t *sta, *tmp;
    sta_key_t key;
    sta_info_t *sta_info = NULL;

    sta = (wlan_emu_sta_t *)hash_map_get_first(m_sta_map);
    while (sta != NULL) {
        tmp = sta;
        sta = (wlan_emu_sta_t *)hash_map_get_next(m_sta_map, tmp);

        if (tmp->get_test_id() == test_id) {
            wifi_vap_info_t *sta_vap_info = tmp->get_sta();
            wifi_hal_disconnect(sta_vap_info->vap_index);

            sta_info = get_devid_sta_info(sta->get_dev_id());
            if (sta_info == NULL) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: sta_info is NULL\n", __func__,
                    __LINE__);
                return;
            }
            remove_from_bridge(sta_info->interface_name, sta_vap_info->bridge_name);
            ovs_fdb_flush(sta_vap_info->bridge_name);

            wlan_emu_sim_sta_mgr_t::create_key(key, tmp->get_dev_id(), test_id);

            set_dev_free(tmp->get_dev_id());
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: Disconnect sta vap %d Freeing the device : %d \n", __func__, __LINE__,
                sta_vap_info->vap_index, tmp->get_dev_id());
            hash_map_remove(m_sta_map, key);

            delete tmp;
        }
    }
}

void wlan_emu_sim_sta_mgr_t::remove_sta(sta_test_t *sta_test)
{
    wlan_emu_sta_t *sta;
    sta_key_t key;
    sta_info_t *sta_info = NULL;

    if (sta_test == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: sta_test is NULL\n", __func__, __LINE__);
        return;
    }

    if (strlen(sta_test->key) == 0) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d:Received key with length 0\n", __func__,
            __LINE__);
        return;
    }

    // key
    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: Request to remove the sta with key : %s\n",
        __func__, __LINE__, sta_test->key);
    sta = (wlan_emu_sta_t *)hash_map_get(m_sta_map, sta_test->key);
    if (sta == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: sta is NULL\n", __func__, __LINE__);
        return;
    }

    // Disconnect the device
    if (sta_test->is_station_associated == true) {
        wlan_emu_print(wlan_emu_log_level_dbg,
            "%s:%d: Disconnect sta vap %d Freeing the device : %d \n", __func__, __LINE__,
            sta_test->sta_vap_config->vap_index, sta->get_dev_id());
        wifi_hal_disconnect(sta_test->sta_vap_config->vap_index);
    }

    sta_info = get_devid_sta_info(sta->get_dev_id());
    if (sta_info == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: sta_info is NULL\n", __func__, __LINE__);
        return;
    }
    remove_from_bridge(sta_info->interface_name, sta_test->sta_vap_config->bridge_name);
    ovs_fdb_flush(sta_test->sta_vap_config->bridge_name);

    set_dev_free(sta->get_dev_id());
    hash_map_remove(m_sta_map, sta_test->key);

    return;
}

int wlan_emu_sim_sta_mgr_t::send_proto_pcap(wlan_emu_emu80211_cmd_type_t cmd_type,
    mac_address_t mac_addr, char *proto_file)
{
    struct pcap_pkthdr header;
    pcap_t *handle;

    wlan_emu_msg_t msg;
    const unsigned char *packet = nullptr;
    struct ieee80211_mgmt *mgmt = nullptr;
    char errbuf[PCAP_ERRBUF_SIZE];
    struct ieee80211_radiotap_header *radiotap;
    unsigned int radiotap_len;
    unsigned int buff_len;
    unsigned char *buff;
    struct ieee80211_radiotap_iterator iterator;
    int ret;
    bool is_fcs_present = false;
    // Send auth data.

    if (proto_file == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: proto file NULL pointer \n", __func__,
            __LINE__);
        return -1;
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: proto_file is :%s\n", __func__, __LINE__,
        proto_file);
    handle = pcap_open_offline(proto_file, errbuf);
    if (!handle) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: pcap_open_offline failed \n", __func__,
            __LINE__);
        return -1;
    }

    packet = pcap_next(handle, &header);
    if (packet == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: packet is null \n", __func__, __LINE__);
        return -1;
    }

    radiotap = (struct ieee80211_radiotap_header *)packet;
    radiotap_len = radiotap->it_len;

    if (ieee80211_radiotap_iterator_init(&iterator, radiotap, header.caplen, NULL)) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: radio tap iterator init failed \n", __func__,
            __LINE__);
        return -1;
    }

    while (!(ret = ieee80211_radiotap_iterator_next(&iterator))) {
        // Check for specific field types
        switch (iterator.this_arg_index) {
        case IEEE80211_RADIOTAP_FLAGS: {
            uint8_t flags = *iterator.this_arg;

            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Flags : 0x%x\n", __func__, __LINE__,
                flags);
            // Check if the FCS flag (0x10) is set
            if (flags & IEEE80211_RADIOTAP_F_FCS) {
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: FCS is present in the packet\n",
                    __func__, __LINE__);
                is_fcs_present = true;
                break;
            } else {
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: FCS is not present in the packet\n",
                    __func__, __LINE__);
            }
            break;
        }
        default:
            break;
        }
    }

    // remove FCS.
    // buff_len = header.len - radiotap_len - 4;
    // check if FCS is present or not
    if (is_fcs_present == true) {
        buff_len = header.len - radiotap_len - 4;
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: fcs is present remove 4 bytes\n", __func__,
            __LINE__);
    } else {
        buff_len = header.len - radiotap_len;
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: fcs is not present remove 4 bytes\n",
            __func__, __LINE__);
    }

    mgmt = (struct ieee80211_mgmt *)(packet + radiotap_len);
    buff = (unsigned char *)malloc(buff_len + ETH_ALEN);
    memset(buff, 0, (buff_len + ETH_ALEN));
    memcpy(buff, mac_addr, ETH_ALEN);
    memcpy(buff + ETH_ALEN, mgmt, buff_len);
    msg.send_ctrl_msg(buff, buff_len + ETH_ALEN, cmd_type);
    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: Src MAC is %02x..%02x \n", __func__, __LINE__,
        mgmt->sa[0], mgmt->sa[5]);
    pcap_close(handle);

    free(buff);

    return 0;
}

station_prototype_pcaps_t *wlan_emu_sim_sta_mgr_t::get_frm80211_proto_pcap(
    station_prototype_t *station_prototype, unsigned int type)
{

    if (station_prototype == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: station_prototype is NULL\n", __func__,
            __LINE__);
        return nullptr;
    }

    if (station_prototype->fc_prototype_q) {
        queue_t *fc_prototype_q = station_prototype->fc_prototype_q;
        element_t *current = fc_prototype_q->head;

        while (current != NULL) {
            station_prototype_pcaps_t *station_prototype_pcap =
                (station_prototype_pcaps_t *)current->data;

            if (station_prototype_pcap != NULL) {

                wlan_emu_print(wlan_emu_log_level_dbg,
                    "%s:%d: found type : %d file : %s msg_type : %d\n", __func__, __LINE__,
                    station_prototype_pcap->fc_request.frm80211_ops,
                    station_prototype_pcap->file_location,
                    station_prototype_pcap->fc_request.msg_type);
                if ((station_prototype_pcap->fc_request.msg_type ==
                        1 << wlan_emu_msg_type_frm80211) &&
                    (station_prototype_pcap->fc_request.frm80211_ops == type)) {

                    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: found type : %d file : %s \n",
                        __func__, __LINE__, type, station_prototype_pcap->file_location);
                    return station_prototype_pcap;
                }
                current = current->next;
            }
        }
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: no prototype with type : %d found\n", __func__,
        __LINE__, type);
    return nullptr;
}

int wlan_emu_sim_sta_mgr_t::configure_proto_types_on_sta(sta_test_t *sta_test_config)
{
    station_prototype_pcaps_t *station_prototype_pcap = nullptr;
    int ret = RETURN_ERR;

    if (sta_test_config->station_prototype == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: station_prototype is NULL\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    if (sta_test_config->station_prototype->fc_prototype_q == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: fc_prototype_q is NULL\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    if (sta_test_config->station_prototype->fc_request_summary.msg_type &
        1 << wlan_emu_msg_type_frm80211) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: frm80211 prototype request\n", __func__,
            __LINE__);

        if (sta_test_config->station_prototype->fc_request_summary.frm80211_ops &
            1 << wlan_emu_frm80211_ops_type_auth) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: auth prototype request\n", __func__,
                __LINE__);
            station_prototype_pcap = get_frm80211_proto_pcap(sta_test_config->station_prototype,
                1 << wlan_emu_frm80211_ops_type_auth);
            if (station_prototype_pcap == nullptr) {
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Failed to find proto pcap\n",
                    __func__, __LINE__);
                return RETURN_ERR;
            }
            ret = send_proto_pcap(wlan_emu_emu80211_cmd_frame_auth_req,
                sta_test_config->sta_vap_config->u.sta_info.mac,
                station_prototype_pcap->file_location);
            if (ret == RETURN_ERR) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: Unable to send auth request prototype frame\n", __func__, __LINE__);
                return ret;
            }
        }

        if (sta_test_config->station_prototype->fc_request_summary.frm80211_ops &
            1 << wlan_emu_frm80211_ops_type_assoc_req) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: assoc req prototype request\n", __func__,
                __LINE__);
            station_prototype_pcap = get_frm80211_proto_pcap(sta_test_config->station_prototype,
                1 << wlan_emu_frm80211_ops_type_assoc_req);
            if (station_prototype_pcap == nullptr) {
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Failed to find proto pcap\n",
                    __func__, __LINE__);
                return RETURN_ERR;
            }
            ret = send_proto_pcap(wlan_emu_emu80211_cmd_frame_assoc_req,
                sta_test_config->sta_vap_config->u.sta_info.mac,
                station_prototype_pcap->file_location);
            if (ret == RETURN_ERR) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: Unable to send assoc request prototype frame\n", __func__, __LINE__);
                return ret;
            }
        }
    }

    return ret;
}

int wlan_emu_sim_sta_mgr_t::add_sta(sta_test_t *sta_test_config)
{
    wlan_emu_sta_t *sta;
    int dev_id;
    unsigned int chan_list[1] = { 0 };
    wifi_vap_info_map_t *map = NULL;
    wifi_bss_info_t bss;
    sta_info_t *sta_info = NULL;
    mac_update_t mac_update;
    bool is_custom_mac_enabled = false;
    heart_beat_data_t *heart_beat_data;

    if ((dev_id = find_first_free_dev()) == -1) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: could not find free device\n", __func__,
            __LINE__);
        return -1;
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: found free device at pos: %d\n", __func__,
        __LINE__, dev_id);

    create_key(sta_test_config->key, dev_id, sta_test_config->test_id);

    sta_info = get_devid_sta_info(dev_id);
    if (sta_info == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: sta_info is NULL\n", __func__, __LINE__);
        return -1;
    }
    sta_test_config->sta_vap_config->vap_index = sta_info->index;
    sta_test_config->sta_vap_config->radio_index = sta_info->rdk_radio_index;
    sta_test_config->phy_index = sta_info->phy_index;

    memcpy(sta_test_config->sta_vap_config->u.sta_info.mac, sta_info->mac, sizeof(mac_address_t));

    add_to_bridge(sta_info->interface_name, sta_test_config->sta_vap_config->bridge_name);
    set_bridge_mac(sta_test_config->sta_vap_config->bridge_name);

    if (is_zero_mac(sta_test_config->custom_mac) == false) {
        memcpy(sta_test_config->sta_vap_config->u.sta_info.mac, sta_test_config->custom_mac,
            sizeof(mac_address_t));
        is_custom_mac_enabled = true;
    }

    switch (sta_test_config->sta_type) {
    case sta_model_type_iphone:
        sta = new wlan_emu_sta_iphone_t(dev_id, sta_test_config->test_id,
            sta_test_config->sta_vap_config, &sta_test_config->profile, is_custom_mac_enabled);
        break;

    case sta_model_type_pixel:
        sta = new wlan_emu_sta_pixel_t(dev_id, sta_test_config->test_id,
            sta_test_config->sta_vap_config, &sta_test_config->profile, is_custom_mac_enabled);
        break;

    default:
        assert(0);
    }

    map = (wifi_vap_info_map_t *)malloc(sizeof(wifi_vap_info_map_t));
    if (map == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to allocate memory\n", __func__,
            __LINE__);
        delete (sta);
        return RETURN_ERR;
    }
    map->num_vaps = 1;
    memcpy(&map->vap_array[0], sta_test_config->sta_vap_config, sizeof(wifi_vap_info_t));

    pre_station_connectivity_profile_t *pre_connect_profile =
        sta_test_config->u.sta_management.pre_assoc_stats;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Rssi : %d Noise : %d Bitrate : %d\n", __func__,
        __LINE__, pre_connect_profile->pre_assoc_rssi, pre_connect_profile->pre_assoc_noise,
        pre_connect_profile->pre_assoc_bitrate);
    // create the heart beat data
    heart_beat_data = new (std::nothrow) heart_beat_data_t;
    if (heart_beat_data == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Unable to send the heart beat\n", __func__,
            __LINE__);
        delete (sta);
        free(map);
        return RETURN_ERR;
    }
    memcpy(heart_beat_data->mac, sta_test_config->sta_vap_config->u.sta_info.mac,
        sizeof(mac_address_t));
    heart_beat_data->rssi = pre_connect_profile->pre_assoc_rssi;
    heart_beat_data->noise = pre_connect_profile->pre_assoc_noise;
    heart_beat_data->bitrate = pre_connect_profile->pre_assoc_bitrate;

    sta->handle_heart_beat(heart_beat_data);
    delete (heart_beat_data);
    delete (pre_connect_profile);

    // Copy Radio Params to HAL.
    if (wifi_hal_setRadioOperatingParameters(sta_info->rdk_radio_index,
            sta_test_config->radio_oper_param) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: wifi_hal_setRadioOperatingParameters failed for radio index : %d\n", __func__,
            __LINE__, sta_info->rdk_radio_index);
        delete (sta);
        free(map);
        return RETURN_ERR;
    }

    if (wifi_hal_createVAP(sta_info->rdk_radio_index, map) == RETURN_OK) {
        sta->set_vap(sta_test_config->sta_vap_config);
    } else {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: wifi_hal_createVAP failed for radio index : %d\n", __func__, __LINE__,
            sta_info->rdk_radio_index);
        delete (sta);
        free(map);
        return RETURN_ERR;
    }

    memset(&mac_update, 0, sizeof(mac_update_t));
    memcpy(mac_update.old_mac, sta_info->mac, sizeof(mac_address_t));
    memcpy(sta_info->mac, map->vap_array[0].u.sta_info.mac, sizeof(mac_address_t));
    memcpy(mac_update.new_mac, sta_info->mac, sizeof(mac_address_t));
    memcpy(mac_update.bridge_name, map->vap_array[0].bridge_name, sizeof(mac_update.bridge_name));
    mac_update.op_modes = sta_test_config->u.sta_management.op_modes;

    sta->send_mac_update(&mac_update);

    free(map);
    if (sta_test_config->is_station_prototype_enabled == true) {
        if (configure_proto_types_on_sta(sta_test_config) == RETURN_ERR) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: Unable to configure proto types on sta for dev_id : %d\n", __func__,
                __LINE__, dev_id);
            delete (sta);
            return RETURN_ERR;
        }
    }
    memset(&bss, 0, sizeof(bss));

    bss.freq = convert_channel_to_freq(sta_test_config->radio_oper_param->band,
        sta_test_config->radio_oper_param->channel);
    wlan_emu_print(wlan_emu_log_level_dbg,
        "%s:%d: vap_index : %d phy_index : %d channel : %d bss.freq : %d \n", __func__, __LINE__,
        sta_test_config->sta_vap_config->vap_index, sta_test_config->phy_index,
        sta_test_config->radio_oper_param->channel, bss.freq);

    snprintf(bss.ssid, sizeof(bss.ssid), "%s", sta_test_config->sta_vap_config->u.sta_info.ssid);
    memcpy(bss.bssid, sta_test_config->sta_vap_config->u.sta_info.bssid, sizeof(mac_address_t));
    chan_list[0] = sta_test_config->radio_oper_param->channel;
    bss.oper_freq_band = sta_test_config->radio_oper_param->band;
    wifi_hal_startScan(sta_info->rdk_radio_index, WIFI_RADIO_SCAN_MODE_OFFCHAN, 500, 1, chan_list);
    usleep(500000);
    if (wifi_hal_connect(sta_test_config->sta_vap_config->vap_index, &bss) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: hal connect failed for dev_id : %d for vap_index : %d\n", __func__, __LINE__,
            dev_id, sta_test_config->sta_vap_config->vap_index);
        delete (sta);
        return RETURN_ERR;
    }

    wlan_emu_print(wlan_emu_log_level_dbg,
        "%s:%d: hal connect succesful for dev_id : %d for vap_index : %d\n", __func__, __LINE__,
        dev_id, sta_test_config->sta_vap_config->vap_index);
    hash_map_put(m_sta_map, strdup(sta_test_config->key), sta);
    set_dev_busy(dev_id);

    return 0;
}

int wlan_emu_sim_sta_mgr_t::init(wifi_interface_name_idex_map_t *if_map, int if_map_size)
{
    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: initiated\n", __func__, __LINE__);
    unsigned int i = 0;
    sta_info_t *sta_info;
    wifi_interface_name_idex_map_t *interface_map;
    wifi_vap_info_map_t *vap_info_map = NULL;
    mac_addr_str_t mac_str;

    m_sta_map = hash_map_create();

    m_sta_info_map = queue_create();

    if ((m_sta_map == NULL) || (m_sta_info_map == NULL)) {
        wlan_emu_print(wlan_emu_log_level_info,
            "%s:%d: object creation failed m_sta_map : %p m_sta_info_map : %p\n", __func__,
            __LINE__, m_sta_map, m_sta_info_map);
        return RETURN_ERR;
    }

    vap_info_map = (wifi_vap_info_map_t *)malloc(sizeof(wifi_vap_info_map_t));
    if (vap_info_map == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d Failed to allocate memory\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    for (i = 0; i < if_map_size; i++) {
        interface_map = &if_map[i];
        if (interface_map->vap_name[0] != '\0') {
            sta_info = (sta_info_t *)malloc(sizeof(sta_info_t));
            if (sta_info == NULL) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: sta_info allocation failed\n",
                    __func__, __LINE__);
                return RETURN_ERR;
            }
            snprintf(sta_info->vap_name, sizeof(sta_info->vap_name), "%s", interface_map->vap_name);
            sta_info->phy_index = interface_map->phy_index;
            sta_info->rdk_radio_index = interface_map->rdk_radio_index;
            snprintf(sta_info->interface_name, sizeof(sta_info->interface_name), "%s",
                interface_map->interface_name);
            snprintf(sta_info->bridge_name, sizeof(sta_info->bridge_name), "%s",
                interface_map->bridge_name);
            sta_info->vlan_id = interface_map->vlan_id;
            sta_info->index = interface_map->index;
            sta_info->status = sta_state_free;

            memset(vap_info_map, 0, sizeof(wifi_vap_info_map_t));
            if (wifi_hal_getRadioVapInfoMap(sta_info->rdk_radio_index, vap_info_map) != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: vap info map failed for radio index : %d\n", __func__, __LINE__,
                    sta_info->rdk_radio_index);
                free(sta_info);
                free(vap_info_map);
                return RETURN_ERR;
            }

            for (unsigned int itr = 0; itr < vap_info_map->num_vaps; itr++) {
                if (vap_info_map->vap_array[itr].vap_index == sta_info->index) {
                    memcpy(sta_info->mac, vap_info_map->vap_array[itr].u.sta_info.mac,
                        sizeof(mac_address_t));
                    wlan_emu_print(wlan_emu_log_level_dbg,
                        "%s:%d: itr : %d vap_index : %d mac : %s\n", __func__, __LINE__, itr,
                        vap_info_map->vap_array[itr].vap_index, to_mac_str(sta_info->mac, mac_str));
                }
            }

            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: m_sta_info_count : %d sta_info->index : %d\n", __func__, __LINE__,
                m_sta_info_count, sta_info->index);
            m_sta_info_count++;
            queue_push(m_sta_info_map, sta_info);
        }
    }
    free(vap_info_map);

    return 0;
}

void wlan_emu_sim_sta_mgr_t::stop()
{
}

int wlan_emu_sim_sta_mgr_t::start()
{
    /*    unsigned int i, j;
        wifi_interface_name_idex_map_t  *map;
        wifi_vap_info_t vap;

        for (i = 0; i < m_cap->wifi_prop.numRadios; i++) {
            for (j = 0; j < MAX_NUM_VAP_PER_RADIO; j++) {
                map = &m_cap->wifi_prop.interface_map[i*MAX_NUM_VAP_PER_RADIO + j];
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Phy Index: %d\tRadio Index:
       %d\tInterface Name: %s\tVAP Name: %s\tVAP Index: %d\n",
                    __func__, __LINE__,
                    map->phy_index, map->rdk_radio_index, map->interface_name, map->vap_name,
       map->index); if ((map->index == 14) || (map->index == 15)) { vap.vap_index = map->index;
                    strncpy(vap.vap_name, map->vap_name, sizeof(vap.vap_name));
                    strncpy(vap.bridge_name, map->bridge_name, sizeof(vap.bridge_name));
                    vap.vap_mode = wifi_vap_mode_sta;
                    strncpy(vap.u.sta_info.ssid, "test_ssid", sizeof(vap.u.sta_info.ssid));
                    add_sta(map->rdk_radio_index, &vap, sta_model_type_iphone);
                }
            }
        }*/

    return 0;
}

wlan_emu_sim_sta_mgr_t::wlan_emu_sim_sta_mgr_t()
{
    m_sta_map = NULL;
    m_cap = NULL;
    m_sta_info_map = NULL;
    m_sta_info_count = 0;
}

wlan_emu_sim_sta_mgr_t::~wlan_emu_sim_sta_mgr_t()
{
}
