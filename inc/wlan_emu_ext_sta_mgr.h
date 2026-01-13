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

#ifndef WLAN_EMU_EXT_STA_MGR_H
#define WLAN_EMU_EXT_STA_MGR_H

#include "collection.h"
#include "wlan_emu_common.h"
#include "wlan_emu_ext_agent_interface.h"
#include "wlan_emu_test_params.h"

class wlan_emu_ext_sta_mgr_t {
public:
    hash_map_t *ext_agent_map; // wlan_emu_ext_agent_interface_t
    wlan_emu_ext_agent_interface_t *find_first_free_dev();
    wlan_emu_ext_agent_interface_t ext_agent_iface;
    wlan_emu_bus_t *m_bus_mgr;
    bus_handle_t *handle;

    int init();
    int start();
    void stop();

    //int add_sta(test_step_params_t *step);
    int add_sta(test_step_params_t *step,const std::string &cli_subdoc);
    void remove_all_sta(unsigned int vap_id);
    int get_num_free_clients();
    void remove_sta(sta_test_t *sta_test);
    int configure_proto_types_on_sta(sta_test_t *sta_test_config);
    wlan_emu_ext_agent_interface_t *get_ext_agent(char *key);

    wlan_emu_ext_sta_mgr_t();
    ~wlan_emu_ext_sta_mgr_t();
};

#endif // WLAN_EMU_EXT_STA_MGR_H
