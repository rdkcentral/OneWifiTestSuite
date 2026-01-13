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

#ifndef _WLAN_EMU_STA_H
#define _WLAN_EMU_STA_H

#include "wifi_hal.h"
#include "wlan_emu_common.h"
#include "wlan_emu_msg.h"
#include <string.h>

class wlan_emu_sta_t {
    wifi_vap_info_t m_sta;
    unsigned int m_dev;
    unsigned int m_test;
    unsigned int m_alive; // units of heart_beat_time

private:
    int load_profiles();

public:
    sta_model_type_t m_model;
    oui_t m_oui;
    sta_tlv_t m_vedor_specific;
    sta_profile_t m_profile;

public:
    virtual int init() = 0;
    virtual int get_dev_mac(unsigned int dev_id, uint8_t *mac) = 0;
    int authenticate(ssid_t *ssid, wifi_vap_security_t *sec);
    int deauthenticate();

    inline void set_dev_id(unsigned int id)
    {
        m_dev = id;
    }

    inline void set_test_id(unsigned int id)
    {
        m_test = id;
    }

    inline void add_profile(sta_profile_t *profile)
    {
        memcpy(&m_profile, profile, sizeof(sta_profile_t));
    }

    inline unsigned int get_dev_id()
    {
        return m_dev;
    }

    inline unsigned int get_test_id()
    {
        return m_test;
    }

    inline wifi_vap_info_t *get_sta()
    {
        return &m_sta;
    }

    inline void set_vap(wifi_vap_info_t *vap)
    {
        memcpy(&m_sta, vap, sizeof(wifi_vap_info_t));
    }

    void handle_heart_beat(heart_beat_data_t *heart_beat_data);
    void send_radio_tap_header(heart_beat_data_t *);
    void send_mac_update(mac_update_t *mac_update);

    wlan_emu_sta_t();
    ~wlan_emu_sta_t();
};

class wlan_emu_sta_iphone_t : public wlan_emu_sta_t {
public:
    int init();
    int get_dev_mac(unsigned int dev_id, uint8_t *mac);

    wlan_emu_sta_iphone_t(unsigned int dev_id, unsigned int test_id, wifi_vap_info_t *vap,
        sta_profile_t *profile, bool is_custom_mac_enabled);
    ~wlan_emu_sta_iphone_t();
};

class wlan_emu_sta_pixel_t : public wlan_emu_sta_t {
public:
    int init();
    int get_dev_mac(unsigned int dev_id, uint8_t *mac);

    wlan_emu_sta_pixel_t(unsigned int dev_id, unsigned int test_id, wifi_vap_info_t *vap,
        sta_profile_t *profile, bool is_custom_mac_enabled);
    ~wlan_emu_sta_pixel_t();
};

#endif // _WLAN_EMU_STA_H
