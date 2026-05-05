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

#include "wlan_emu_sta.h"
#include "cci_wifi_utils.hpp"
#include "wlan_emu_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
INT wifi_hal_createVAP(wifi_radio_index_t index, wifi_vap_info_map_t *map);
INT wifi_hal_connect(INT ap_index, wifi_bss_info_t *bss);
}

int wlan_emu_sta_t::load_profiles()
{
    return 0;
}

void wlan_emu_sta_t::handle_heart_beat(heart_beat_data_t *heart_beat_data)
{
    send_radio_tap_header(heart_beat_data);
}

int wlan_emu_sta_t::authenticate(ssid_t *ssid, wifi_vap_security_t *sec)
{
    return 0;
}

int wlan_emu_sta_t::deauthenticate()
{

    return 0;
}

wlan_emu_sta_t::wlan_emu_sta_t()
{
}

wlan_emu_sta_t::~wlan_emu_sta_t()
{
}

int wlan_emu_sta_iphone_t::init()
{
    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: iphone initiated\n", __func__, __LINE__);

    return 0;
}

void wlan_emu_sta_t::send_radio_tap_header(heart_beat_data_t *heart_beat_data)
{
    unsigned char char_buf[256];
    unsigned int byte_count = 0;
    unsigned int count = 0;
    wlan_emu_msg_t msg;
    mac_addr_str_t mac_str;

    memset(char_buf, 0, sizeof(char_buf));

    memcpy(&char_buf[byte_count], &heart_beat_data->mac, sizeof(heart_beat_data->mac));
    byte_count += sizeof(heart_beat_data->mac);

    memcpy(&char_buf[byte_count], &heart_beat_data->rssi, sizeof(heart_beat_data->rssi));
    byte_count += sizeof(heart_beat_data->rssi);

    memcpy(&char_buf[byte_count], &heart_beat_data->noise, sizeof(heart_beat_data->noise));
    byte_count += sizeof(heart_beat_data->noise);

    memcpy(&char_buf[byte_count], &heart_beat_data->bitrate, sizeof(heart_beat_data->bitrate));
    byte_count += sizeof(heart_beat_data->bitrate);

    wlan_emu_print(wlan_emu_log_level_dbg,
        "%s:%d: mac_str : %s rssi : %d noise : %d bitrate : %d\n", __func__, __LINE__,
        to_mac_str(heart_beat_data->mac, mac_str), heart_beat_data->rssi, heart_beat_data->noise,
        heart_beat_data->bitrate);
    /*
        for (count = 0; count < byte_count; count++) {
            wlan_emu_print(wlan_emu_log_level_dbg, " %02X", char_buf[count]);
        }
    */
    msg.send_ctrl_msg(char_buf, byte_count, wlan_emu_emu80211_cmd_radiotap);

    return;
}

void wlan_emu_sta_t::send_mac_update(mac_update_t *mac_update)
{
    unsigned char char_buf[256];

    unsigned int byte_count = 0;
    unsigned int count = 0;
    wlan_emu_msg_t msg;

    memset(char_buf, 0, sizeof(char_buf));

    memcpy(&char_buf[byte_count], &mac_update->old_mac, sizeof(mac_update->old_mac));
    byte_count += sizeof(mac_update->old_mac);

    memcpy(&char_buf[byte_count], &mac_update->new_mac, sizeof(mac_update->new_mac));
    byte_count += sizeof(mac_update->new_mac);

    memcpy(&char_buf[byte_count], &mac_update->op_modes, sizeof(mac_update->op_modes));
    byte_count += sizeof(mac_update->op_modes);

    memcpy(&char_buf[byte_count], &mac_update->bridge_name, sizeof(mac_update->bridge_name));
    byte_count += sizeof(mac_update->bridge_name);
    /*
      for (count = 0; count < byte_count; count++) {
      wlan_emu_print(wlan_emu_log_level_info, " %02X", char_buf[count]);
      }
    */
    msg.send_ctrl_msg(char_buf, byte_count, wlan_emu_emu80211_cmd_mac_update);

    return;
}

int wlan_emu_sta_iphone_t::get_dev_mac(unsigned int dev_id, uint8_t *mac)
{
    dev_id += 2;

    mac[0] = 0x00;
    mac[1] = 0x17;
    mac[2] = 0xf2;
    mac[3] = ((dev_id >> 16) & 0xff);
    mac[4] = ((dev_id >> 8) & 0xff);
    mac[5] = (dev_id & 0xff);

    return RETURN_OK;
}

wlan_emu_sta_iphone_t::wlan_emu_sta_iphone_t(unsigned int dev_id, unsigned int test_id,
    wifi_vap_info_t *vap, sta_profile_t *profile, bool is_custom_mac_enabled)
{
    unsigned char val[8] = { 0x0a, 0x00, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00 };
    m_model = sta_model_type_iphone;
    m_oui[0] = 0x00;
    m_oui[1] = 0x17;
    m_oui[2] = 0xf2;

    m_vedor_specific.type = 0xdd;
    m_vedor_specific.len = 0x0b;
    memcpy(m_vedor_specific.val, &m_oui, sizeof(m_oui));
    memcpy(&m_vedor_specific.val[sizeof(m_oui)], val, sizeof(val));

    if (is_custom_mac_enabled == false) {
        get_dev_mac(dev_id, vap->u.sta_info.mac);
    }

    set_dev_id(dev_id);
    set_test_id(test_id);

    add_profile(profile);
}

wlan_emu_sta_iphone_t::~wlan_emu_sta_iphone_t()
{
}

int wlan_emu_sta_pixel_t::init()
{
    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: pixel initiated\n", __func__, __LINE__);

    return 0;
}

int wlan_emu_sta_pixel_t::get_dev_mac(unsigned int dev_id, uint8_t *mac)
{
    dev_id += 2;

    mac[0] = 0xb8;
    mac[1] = 0xdb;
    mac[2] = 0x38;
    mac[3] = ((dev_id >> 16) & 0xff);
    mac[4] = ((dev_id >> 8) & 0xff);
    mac[5] = (dev_id & 0xff);

    return RETURN_OK;
}

wlan_emu_sta_pixel_t::wlan_emu_sta_pixel_t(unsigned int dev_id, unsigned int test_id,
    wifi_vap_info_t *vap, sta_profile_t *profile, bool is_custom_mac_enabled)
{
    m_model = sta_model_type_pixel;
    m_oui[0] = 0xc8;
    m_oui[1] = 0x2a;
    m_oui[2] = 0xdd;
    unsigned char val[8] = { 0x0a, 0x00, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00 };

    m_vedor_specific.type = 0xdd;
    m_vedor_specific.len = 0x00;
    memcpy(m_vedor_specific.val, &m_oui, sizeof(m_oui));
    memcpy(&m_vedor_specific.val[sizeof(m_oui)], val, sizeof(val));

    if (is_custom_mac_enabled == false) {
        get_dev_mac(dev_id, vap->u.sta_info.mac);
    }

    set_dev_id(dev_id);
    set_test_id(test_id);

    add_profile(profile);
}

wlan_emu_sta_pixel_t::~wlan_emu_sta_pixel_t()
{
}
