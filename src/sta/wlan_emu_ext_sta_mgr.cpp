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

#include "wlan_emu_ext_sta_mgr.h"
#include "wlan_emu_err_code.h"
#include "cci_wifi_utils.hpp"
#include "wlan_emu_log.h"
// #include "http_request.h"
#include "wlan_emu_ext_agent_interface.h"
#include <string.h>
#include <thread>
#include <chrono>

wlan_emu_ext_agent_interface_t *wlan_emu_ext_sta_mgr_t::find_first_free_dev()
{
    queue_t *sta_queue;
    sta_info_t *sta_info = NULL;
    unsigned int count = 0, itr = 0;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);
    // get the hash map and find the dev in queue_t *client_info; //sta_info_t
    wlan_emu_ext_agent_interface_t *ext_sta;
    ext_sta = (wlan_emu_ext_agent_interface_t *)hash_map_get_first(ext_agent_map);
    while (ext_sta != NULL) {
        sta_queue = ext_sta->client_info;
        if (sta_queue != NULL) {
            count = queue_count(sta_queue);
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: count: %d\n", __func__, __LINE__, count);
            for (itr = 0; itr < count; itr++) {
                sta_info = (sta_info_t *)queue_peek(sta_queue, itr);
                if (sta_info != NULL) {
                    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: sta_info->status: %d\n",
                        __func__, __LINE__, sta_info->status);
                    if (sta_info->status == sta_state_free) {
                        sta_info->status = sta_state_in_use;
                        return ext_sta;
                    }
                }
            }
        }
        ext_sta = (wlan_emu_ext_agent_interface_t *)hash_map_get_next(ext_agent_map, ext_sta);
    }
    return NULL;
}

wlan_emu_ext_agent_interface_t *wlan_emu_ext_sta_mgr_t::find_first_free_eth_dev(
    eth_dev_info_t *eth_dev_info)
{

    queue_t *eth_dev_q;
    eth_dev_info_t *eth_dev = NULL;
    unsigned int count = 0, itr = 0;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);
    wlan_emu_ext_agent_interface_t *ext_agent;
    ext_agent = (wlan_emu_ext_agent_interface_t *)hash_map_get_first(ext_agent_map);
    while (ext_agent != NULL) {
        eth_dev_q = ext_agent->eth_client_interfaces;
        if (eth_dev_q != NULL) {
            count = queue_count(eth_dev_q);
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: count: %d\n", __func__, __LINE__, count);
            for (itr = 0; itr < count; itr++) {
                eth_dev = (eth_dev_info_t *)queue_peek(eth_dev_q, itr);
                if (eth_dev != NULL) {
                    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: eth_dev->state: %d\n", __func__,
                        __LINE__, eth_dev->state);
                    if (eth_dev->state == eth_interface_state_free) {
                        eth_dev->state = eth_interface_state_in_use;
                        eth_dev_info->interface_name = eth_dev->interface_name;
                        std::copy(std::begin(eth_dev->interface_mac),
                            std::end(eth_dev->interface_mac),
                            std::begin(eth_dev_info->interface_mac));
                        eth_dev_info->ip_address = eth_dev->ip_address;
                        eth_dev_info->state = eth_dev->state;
                        return ext_agent;
                    }
                }
            }
        }
        ext_agent = (wlan_emu_ext_agent_interface_t *)hash_map_get_next(ext_agent_map, ext_agent);
    }
    return NULL;
}

void wlan_emu_ext_sta_mgr_t::remove_eth_intf(eth_lan_interface_t *eth_lan_interface)
{
    wlan_emu_ext_agent_interface_t *ext_agent;
    eth_dev_info_t *tmp_eth_dev;

    ext_agent = get_ext_agent_from_eth_dev_mac(eth_lan_interface->key);
    if (ext_agent == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to find external agent for key: %s\n",
            __func__, __LINE__, eth_lan_interface->key);
        return;
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: send stop command\n", __func__, __LINE__);
    if (ext_agent->send_external_agent_stop_command() != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: failed to send external agent stop command\n", __func__, __LINE__);
    }

    for (int i = 0; i < queue_count(ext_agent->eth_client_interfaces); i++) {
        tmp_eth_dev = (eth_dev_info_t *)queue_peek(ext_agent->eth_client_interfaces, i);
        if (tmp_eth_dev != NULL &&
            memcmp(tmp_eth_dev->interface_mac, eth_lan_interface->eth_dev_info.interface_mac,
                sizeof(mac_addr_t)) == 0) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: found eth_dev at count %d \n", __func__,
                __LINE__, i);
            tmp_eth_dev->state = eth_interface_state_free;
            break;
        }
    }
    return;
}

wlan_emu_ext_agent_interface_t *wlan_emu_ext_sta_mgr_t::get_ext_agent_from_eth_dev_mac(char *key)
{
    queue_t *sta_queue;
    eth_dev_info_t *tmp_eth_dev;
    unsigned int count = 0, itr = 0;
    wlan_emu_ext_agent_interface_t *ext_agent;
    mac_addr_t mac;
    char tmp_mac[32];

    string_mac_to_uint8_mac(mac, key);

    ext_agent = (wlan_emu_ext_agent_interface_t *)hash_map_get_first(ext_agent_map);
    while (ext_agent != NULL) {
        sta_queue = ext_agent->eth_client_interfaces;
        count = queue_count(ext_agent->eth_client_interfaces);
        for (itr = 0; itr < count; itr++) {
            tmp_eth_dev = (eth_dev_info_t *)queue_peek(sta_queue, itr);
            uint8_mac_to_string_mac(tmp_eth_dev->interface_mac, tmp_mac);
            if (tmp_eth_dev != NULL &&
                memcmp(tmp_eth_dev->interface_mac, mac, sizeof(mac_addr_t)) == 0) {
                return ext_agent;
            }
        }
        ext_agent = (wlan_emu_ext_agent_interface_t *)hash_map_get_next(ext_agent_map, ext_agent);
    }

    return NULL;
}

int wlan_emu_ext_sta_mgr_t::add_eth_lan_device(test_step_params_t *step,
    const std::string &cli_subdoc)
{
    wlan_emu_ext_agent_interface_t *ext_agent = NULL;
    eth_lan_interface_t *eth_lan_interface = NULL;
    eth_dev_info_t *eth_dev_info = NULL;
    unsigned int raw_data_size = 2048;
    FILE *fp;
    char eth_connect_info[256] = { 0 };
    ext_agent_status_resp_t status = {};
    char file_upload_url[256] = { 0 };

    // Find the agent using the key from eth_lan_client
    ext_agent = get_ext_agent_from_eth_dev_mac(step->u.eth_lan_client->key);
    if (ext_agent == NULL) {
        step->m_ui_mgr->cci_error_code = EEXTAGENT;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to find external agent for key: %s\n",
            __func__, __LINE__, step->u.eth_lan_client->key);
        return RETURN_ERR;
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: send start command\n", __func__, __LINE__);
    if (ext_agent->send_external_agent_start_command() != RETURN_OK) {
        step->m_ui_mgr->cci_error_code = EEXTAGENT;
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: failed to send external agent start command\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    if (ext_agent->get_external_agent_test_status(status, step->m_ui_mgr->cci_error_code) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to get external agent status\n",
            __func__, __LINE__);
        return RETURN_ERR;
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: external agent state: %d\n", __func__, __LINE__,
        status.state);

    std::string agent_url = ext_agent->get_agent_proto() +
        std::string(ext_agent->agent_ip_address) + ext_agent->get_agent_port() +
        std::string("/Testconfig/");

    snprintf(eth_connect_info, sizeof(eth_connect_info), "/tmp/cci_res/%s_%d.json",
        ext_agent->agent_hostname.c_str(), step->step_number);

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: eth_connect_info : %s\n", __func__, __LINE__,
        eth_connect_info);

    if ((fp = fopen(eth_connect_info, "w")) == NULL) {
        step->m_ui_mgr->cci_error_code = EFOPEN;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: fopen failed for %s\n", __func__, __LINE__,
            eth_connect_info);
        return RETURN_ERR;
    }
    if (fwrite(cli_subdoc.c_str(), cli_subdoc.length(), 1, fp) != 1) {
        step->m_ui_mgr->cci_error_code = EFWRITE;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: fwrite failed\n", __func__, __LINE__);
        fclose(fp);
        return RETURN_ERR;
    }
    fclose(fp);
    long status_code;

    http_post_file(agent_url, eth_connect_info, status_code, step->m_ui_mgr->cci_error_code);
    if (status_code != http_status_code_ok) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: http_post_file failed : %d\n", __func__,
            __LINE__, status_code);
        return RETURN_ERR;
    }

    return RETURN_OK;
}

int wlan_emu_ext_sta_mgr_t::get_num_free_clients()
{
    queue_t *sta_queue;
    sta_info_t *sta_info = NULL;
    unsigned int count = 0, total_count = 0, itr = 0;

    // get the hash map and find the dev in queue of sta_info_t
    wlan_emu_ext_agent_interface_t *ext_sta;
    ext_sta = (wlan_emu_ext_agent_interface_t *)hash_map_get_first(ext_agent_map);
    while (ext_sta != NULL) {
        sta_queue = ext_sta->client_info;
        count = queue_count(sta_queue);
        for (itr = 0; itr < count; itr++) {
            sta_info = (sta_info_t *)queue_peek(sta_queue, itr);
            if (sta_info != NULL) {
                if (sta_info->status == sta_state_free) {
                    total_count++;
                }
            }
        }
        ext_sta = (wlan_emu_ext_agent_interface_t *)hash_map_get_next(ext_agent_map, ext_sta);
    }
    return total_count;
}

wlan_emu_ext_agent_interface_t *wlan_emu_ext_sta_mgr_t::get_ext_agent(char *key)
{
    queue_t *sta_queue;
    sta_info_t *sta_info = NULL;
    unsigned int count = 0, itr = 0;
    wlan_emu_ext_agent_interface_t *ext_sta;
    mac_addr_t mac;

    string_mac_to_uint8_mac(mac, key);

    ext_sta = (wlan_emu_ext_agent_interface_t *)hash_map_get_first(ext_agent_map);
    while (ext_sta != NULL) {
        sta_queue = ext_sta->client_info;
        count = queue_count(sta_queue);
        for (itr = 0; itr < count; itr++) {
            sta_info = (sta_info_t *)queue_peek(sta_queue, itr);
            if (sta_info != NULL && memcmp(sta_info->mac, mac, sizeof(mac_addr_t)) == 0) {
                return ext_sta;
            }
        }
        ext_sta = (wlan_emu_ext_agent_interface_t *)hash_map_get_next(ext_agent_map, ext_sta);
    }

    return NULL;
}

int wlan_emu_ext_sta_mgr_t::add_sta(test_step_params_t *step, const std::string &cli_subdoc)
{
    char mac_str[32] = { 0 };
    char file_upload_url[256] = { 0 };
    char sta_connect_info[256] = { 0 };
    std::string encoded_raw;
    wlan_emu_ext_agent_interface_t *agent_info = NULL;
    sta_info_t *sta_info;
    unsigned int raw_data_size = 2048;
    FILE *fp;
    int connection_retries = 3;
    ext_agent_status_resp_t status = {};

    // find first free dev
    if ((agent_info = find_first_free_dev()) == NULL) {
        step->m_ui_mgr->cci_error_code = EEXTAGENT;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to get first free device\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: send start command\n", __func__, __LINE__);
    if (agent_info->send_external_agent_start_command() != RETURN_OK) {
        step->m_ui_mgr->cci_error_code = EEXTAGENT;
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: failed to send external agent start command\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    if (agent_info->get_external_agent_test_status(status, step->m_ui_mgr->cci_error_code) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to get external agent status\n",
            __func__, __LINE__);
        return RETURN_ERR;
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: external agent state: %d\n", __func__, __LINE__,
        status.state);

    std::string agent_url = agent_info->get_agent_proto() +
        std::string(agent_info->agent_ip_address) + agent_info->get_agent_port() +
        std::string("/Testconfig/");

    snprintf(sta_connect_info, sizeof(sta_connect_info), "/tmp/cci_res/%s_%d.json",
        agent_info->agent_hostname.c_str(), step->step_number);

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: sta_connect_info : %s\n", __func__, __LINE__,
        sta_connect_info);

    if ((fp = fopen(sta_connect_info, "w")) == NULL) {
        step->m_ui_mgr->cci_error_code = EFOPEN;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: fopen failed for %s\n", __func__, __LINE__,
            sta_connect_info);
        return RETURN_ERR;
    }
    if (fwrite(cli_subdoc.c_str(), cli_subdoc.length(), 1, fp) != 1) {
        step->m_ui_mgr->cci_error_code = EFWRITE;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: fwrite failed\n", __func__, __LINE__);
        fclose(fp);
        return RETURN_ERR;
    }
    fclose(fp);
    long status_code;

    http_post_file(agent_url, sta_connect_info, status_code, step->m_ui_mgr->cci_error_code);
    if (status_code != http_status_code_ok) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: http_post_file failed : %d\n", __func__,
            __LINE__, status_code);
        return RETURN_ERR;
    }

    sta_info = (sta_info_t *)queue_peek(agent_info->client_info, 0);
    sta_info->status = sta_state_in_use;

    uint8_mac_to_string_mac(sta_info->mac, step->u.sta_test->key);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: external agent mac: %s\n", __func__, __LINE__,
        step->u.sta_test->key);

    return RETURN_OK;
}

void wlan_emu_ext_sta_mgr_t::remove_sta(sta_test_t *sta_test)
{
    wlan_emu_ext_agent_interface_t *ext_agent;
    sta_info_t *sta_info;

    ext_agent = get_ext_agent(sta_test->key);
    if (ext_agent == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to find external agent for key: %s\n",
            __func__, __LINE__, sta_test->key);
        return;
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: send stop command\n", __func__, __LINE__);
    if (ext_agent->send_external_agent_stop_command() != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: failed to send external agent stop command\n", __func__, __LINE__);
    }

    // TODO: find allocated sta
    sta_info = (sta_info_t *)queue_peek(ext_agent->client_info, 0);
    for (int i = 0; i < queue_count(ext_agent->client_info); i++) {
        sta_info = (sta_info_t *)queue_peek(ext_agent->client_info, i);
        if (sta_info != NULL && memcmp(sta_info->mac, sta_test->key, sizeof(mac_addr_t)) == 0) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: found sta_info at count %d\n", __func__,
                __LINE__, i);
            sta_info->status = sta_state_free;
            break;
        }
    }
}

int wlan_emu_ext_sta_mgr_t::init()
{
    if (ext_agent_map != NULL) {
        wlan_emu_ext_agent_interface_t *agent_details =
            (wlan_emu_ext_agent_interface_t *)hash_map_get_first(ext_agent_map);
        if (agent_details != NULL) {
            while (agent_details != NULL) {
                if (agent_details->client_info != NULL) {
                    agent_details->agent_exit();
                }
                agent_details = (wlan_emu_ext_agent_interface_t *)hash_map_get_next(ext_agent_map,
                    agent_details);
            }
            hash_map_destroy(ext_agent_map);
            ext_agent_map = NULL;
        }
    }
    ext_agent_map = hash_map_create();

    if (ext_agent_map == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d hash map creation failed\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    if (ext_agent_iface.get_external_agent_info(ext_agent_map, handle, m_bus_mgr) == RETURN_ERR) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d Failed to fetch the external agent info\n",
            __func__, __LINE__);
        return RETURN_ERR;
    }

    if (ext_agent_iface.get_external_agent_capabilities(ext_agent_map) == RETURN_ERR) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d Failed to fetch the external agent capabilities\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    return RETURN_OK;
}

void wlan_emu_ext_sta_mgr_t::stop()
{
}

int wlan_emu_ext_sta_mgr_t::start()
{
    return 0;
}

wlan_emu_ext_sta_mgr_t::wlan_emu_ext_sta_mgr_t()
{
    ext_agent_map = NULL;
}

wlan_emu_ext_sta_mgr_t::~wlan_emu_ext_sta_mgr_t()
{
    if (ext_agent_map != NULL) {
        hash_map_destroy(ext_agent_map);
    }
}
