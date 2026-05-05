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

#include "wlan_ext_emu_sta_mgr.h"
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

int wlan_ext_emu_sta_mgr_t::find_first_free_dev()
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

void wlan_ext_emu_sta_mgr_t::set_dev_busy(unsigned int dev_id)
{
    sta_info_t *sta_info;
    sta_info = (sta_info_t *)queue_peek(m_sta_info_map, dev_id);
    sta_info->status = sta_state_in_use;
}

void wlan_ext_emu_sta_mgr_t::set_dev_free(unsigned int dev_id)
{
    sta_info_t *sta_info;
    sta_info = (sta_info_t *)queue_peek(m_sta_info_map, dev_id);
    sta_info->status = sta_state_free;
}

sta_info_t *wlan_ext_emu_sta_mgr_t::get_devid_sta_info(unsigned int dev_id)
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: dev_id : %d\n", __func__, __LINE__, dev_id);
    return (sta_info_t *)queue_peek(m_sta_info_map, dev_id);
}

void wlan_ext_emu_sta_mgr_t::remove_sta(sta_test_t *sta_test)
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

    set_dev_free(sta->get_dev_id());
    hash_map_remove(m_sta_map, sta_test->key);

    return;
}

int wlan_ext_emu_sta_mgr_t::add_sta(sta_test_t *sta_test_config)
{
    wlan_emu_sta_t *sta;
    int dev_id;
    unsigned int chan_list[1] = { 0 };
    wifi_vap_info_map_t *map = NULL;
    wifi_bss_info_t bss;
    sta_info_t *sta_info = NULL;
    mac_update_t mac_update;
    bool is_custom_mac_enabled = false;

    /*list_interfaces_in_namespace();
    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: name space : %s\n", __func__, __LINE__,
        get_current_namespace().c_str());
        */

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
    sta_test_config->phy_index = sta_info->phy_index;
    sta_test_config->sta_vap_config->radio_index = sta_info->rdk_radio_index;
    // snprintf(sta_test_config->sta_interface_name, sizeof(sta_test_config->sta_interface_name),
    // "%s", sta_info->interface_name);

    sta_test_config->sta_interface_name = std::string(sta_info->interface_name);

    sta_test_config->sta_interface_name = std::string(sta_info->interface_name);

    memcpy(sta_test_config->sta_vap_config->u.sta_info.mac, sta_info->mac, sizeof(mac_address_t));
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
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: wifi_hal_createVAP failed for dev_id : %d\n",
            __func__, __LINE__, dev_id);
        delete (sta);
        free(map);
        return RETURN_ERR;
    }

    free(map);

    memset(&bss, 0, sizeof(bss));

    bss.freq = convert_channel_to_freq(sta_test_config->radio_oper_param->band,
        sta_test_config->radio_oper_param->channel);
    wlan_emu_print(wlan_emu_log_level_dbg,
        "%s:%d: vap_index : %d phy_index : %d channel : %d bss.freq : %d \n", __func__, __LINE__,
        sta_test_config->sta_vap_config->vap_index, sta_test_config->phy_index,
        sta_test_config->radio_oper_param->channel, bss.freq);

    snprintf(bss.ssid, sizeof(bss.ssid), "%s", sta_test_config->sta_vap_config->u.sta_info.ssid);
    chan_list[0] = sta_test_config->radio_oper_param->channel;
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

int wlan_ext_emu_sta_mgr_t::init_eth_interfaces()
{
    eth_dev_info_t *eth_dev;
    std::string eth_interface_name = "lan3";
    unsigned char eth_interface_mac[ETH_ALEN] = { 0 };
    char eth_ip_address[INET_ADDRSTRLEN] = { 0 };
    m_eth_cli_map = hash_map_create();
    int eth_cli_count = 1;

    eth_dev = new (std::nothrow) eth_dev_info_t;
    if (eth_dev == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: allocation failed for %s \n", __func__,
            __LINE__, eth_interface_name.c_str());
        return RETURN_ERR;
    }

#if 0
    //Use this function if we are not using the namespace
    if (get_mac_ip_from_ifname(eth_interface_name.c_str(), eth_interface_mac, eth_ip_address) !=
        RETURN_OK)
#else
    if (get_mac_ip_from_ifname_ns(eth_interface_name.c_str(), "/var/run/netns/ots",
            eth_interface_mac, eth_ip_address) != RETURN_OK)
#endif
    {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to get MAC/IP for %s\n", __func__,
            __LINE__, eth_interface_name.c_str());
        return RETURN_ERR;
    }

    memcpy(eth_dev->interface_mac, eth_interface_mac, sizeof(mac_address_t));
    eth_dev->interface_name = eth_interface_name;
    eth_dev->ip_address = std::string(eth_ip_address);
    eth_dev->state = eth_interface_state_free;

    hash_map_put(m_eth_cli_map, strdup(eth_dev->interface_mac), eth_dev);
    eth_cli_count = 1;
    set_eth_cli_info_count(eth_cli_count);

    return RETURN_OK;
}

int wlan_ext_emu_sta_mgr_t::init(wifi_hal_capability_t *sta_hal_cap)
{
    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: initiated\n", __func__, __LINE__);
    unsigned int i = 0;
    int array_size;
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

    array_size = ARRAY_SIZE(sta_hal_cap->wifi_prop.interface_map);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: array_size : %d\n", __func__, __LINE__,
        array_size);
    for (i = 0; i < array_size; i++) {
        interface_map = &sta_hal_cap->wifi_prop.interface_map[i];
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

            vap_info_map = (wifi_vap_info_map_t *)malloc(sizeof(wifi_vap_info_map_t));
            if (vap_info_map == NULL) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d Failed to allocate memory\n",
                    __func__, __LINE__);
                free(sta_info);
                return RETURN_ERR;
            }
            memset(vap_info_map, 0, sizeof(wifi_vap_info_map_t));
            if (wifi_hal_getRadioVapInfoMap(sta_info->rdk_radio_index, vap_info_map) != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: vap info map failed for radio index : %d\n", __func__, __LINE__,
                    sta_info->rdk_radio_index);
                free(sta_info);
                free(vap_info_map);
                return RETURN_ERR;
            }

            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: num_vaps : %d, vap_name : %s interface_name : %s\n", __func__, __LINE__,
                vap_info_map->num_vaps, sta_info->vap_name, sta_info->interface_name);

            for (unsigned int itr = 0; itr < vap_info_map->num_vaps; itr++) {
                if (vap_info_map->vap_array[itr].vap_index == sta_info->index) {
                    memcpy(sta_info->mac, vap_info_map->vap_array[itr].u.sta_info.mac,
                        sizeof(mac_address_t));
                    wlan_emu_print(wlan_emu_log_level_dbg,
                        "%s:%d: itr : %d vap_index : %d mac : %s vap_name : %s interface_name : %s "
                        "\n",
                        __func__, __LINE__, itr, vap_info_map->vap_array[itr].vap_index,
                        to_mac_str(sta_info->mac, mac_str), sta_info->vap_name,
                        sta_info->interface_name);
                }
            }

            if (is_zero_mac(sta_info->mac) == true) {
                wlan_emu_print(wlan_emu_log_level_info,
                    "%s:%d: zero mac for  vap_index : %d mac : %s vap_name : %s interface_name : "
                    "%s "
                    "\n",
                    __func__, __LINE__, sta_info->index, to_mac_str(sta_info->mac, mac_str),
                    sta_info->vap_name, sta_info->interface_name);
                free(vap_info_map);
                free(sta_info);
                continue;
            }

            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: m_sta_info_count : %d sta_info->index : %d\n", __func__, __LINE__,
                m_sta_info_count, sta_info->index);
            m_sta_info_count++;
            queue_push(m_sta_info_map, sta_info);
            free(vap_info_map);
        }
    }

    return 0;
}

void wlan_ext_emu_sta_mgr_t::stop()
{
}

int wlan_ext_emu_sta_mgr_t::start()
{
    return 0;
}

wlan_ext_emu_sta_mgr_t::wlan_ext_emu_sta_mgr_t()
{
    m_sta_map = NULL;
    m_cap = NULL;
    m_sta_info_map = NULL;
    m_sta_info_count = 0;
}

wlan_ext_emu_sta_mgr_t::~wlan_ext_emu_sta_mgr_t()
{
}
