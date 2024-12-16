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

#ifndef _WLAN_EMU_STA_MGR_H
#define _WLAN_EMU_STA_MGR_H

#include <cstdio>
#include "collection.h"
#include "wifi_base.h"
#include "wifi_hal.h"
#include "wlan_emu_common.h"
#include "wlan_emu_msg_data.h"

class wlan_emu_sim_sta_mgr_t {
    hash_map_t *m_sta_map;
    wifi_hal_capability_t *m_cap;
    queue_t *m_sta_info_map; // sta_info_t
    unsigned int m_sta_info_count;
    unsigned int m_dev_status[DEV_STATUS_ARR_SZ];
    int find_first_free_dev();
    void set_dev_busy(unsigned int dev_id);
    void set_dev_free(unsigned int dev_id);

    inline static void create_key(sta_key_t key, unsigned int dev_id, unsigned int test_id)
    {
        snprintf(key, sizeof(sta_key_t), "%d-%d", test_id, dev_id);
    }

    inline unsigned int get_test_id_from_key(sta_key_t key)
    {
        unsigned int test_id, dev_id;

        sscanf(key, "%d-%d", &test_id, &dev_id);
        return test_id;
    }

    inline unsigned int get_dev_id_from_key(sta_key_t key)
    {
        unsigned int test_id, dev_id;

        sscanf(key, "%d-%d", &test_id, &dev_id);
        return dev_id;
    }

public:
    int init(wifi_interface_name_idex_map_t *if_map, int if_map_size);
    int start();
    void stop();

    int add_sta(sta_test_t *sta_test_config);
    void remove_all_sta(unsigned int vap_id);
    void send_heart_beat(char *key, heart_beat_data_t *heart_beat_data);
    sta_info_t *get_devid_sta_info(unsigned int dev_id);
    void remove_sta(sta_test_t *sta_test);
    station_prototype_pcaps_t *get_frm80211_proto_pcap(station_prototype_t *station_prototype,
        unsigned int type);
    int configure_proto_types_on_sta(sta_test_t *sta_test_config);
    int send_proto_pcap(wlan_emu_emu80211_cmd_type_t cmd_type, mac_address_t mac_addr,
        char *proto_file);

    wlan_emu_sim_sta_mgr_t();
    ~wlan_emu_sim_sta_mgr_t();
};

#endif //_WLAN_EMU_STA_MGR_H
