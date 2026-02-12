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

#include "wlan_emu_log.h"
#include "wlan_ext_emu.h"
#include "wlan_ext_emu_test_step_params.h"
#include <cjson/cJSON.h>
#include <spawn.h>
#include <signal.h>
#include <sys/wait.h>

int wlan_ext_test_step_param_sta_management::wlan_ext_step_execute()
{
    wlan_ext_test_step_params_t *step = this;
    wlan_ext_emu_sta_mgr_t *m_sta_mgr = get_agent_sta_mgr();
    step->u.sta_test->test_id = step->step_number;

    if (m_sta_mgr->add_sta(step->u.sta_test) == RETURN_ERR) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: add_sta failed for external agent for step : %d\n", __func__, __LINE__,
            step->step_number);
        step->step_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    if ((step->u.sta_test->is_ip_assigned == false) || (step->fork == false)) {
        step->step_state = wlan_emu_tests_state_cmd_continue;
    } else {
        step->step_state = wlan_emu_tests_state_cmd_wait;
    }

    return RETURN_OK;
}

int wlan_ext_test_step_param_sta_management::wlan_ext_step_timeout()
{
    wlan_ext_test_step_params_t *step = this;
    wlan_ext_emu_sta_mgr_t *m_sta_mgr = get_agent_sta_mgr();
    step->timeout_count++;
    if (step->timeout_count >= step->execution_time) {
        step->step_state = wlan_emu_tests_state_cmd_results;
        wlan_emu_print(wlan_emu_log_level_dbg,
            "%s:%d: step number : %d timeout count : %d in results\n", __func__, __LINE__,
            step->step_number, step->timeout_count);
    } else {

        if ((step->u.sta_test->is_ip_assigned == false) || (step->fork == false)) {
            step->step_state = wlan_emu_tests_state_cmd_continue;
        } else {
            step->step_state = wlan_emu_tests_state_cmd_wait;
        }
    }
    return RETURN_OK;
}

void wlan_ext_test_step_param_sta_management::wlan_ext_step_remove()
{
    wlan_ext_test_step_params_t *step = this;
    wlan_ext_emu_sta_mgr_t *m_sta_mgr = get_agent_sta_mgr();
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step number : %d\n", __func__, __LINE__,
        step->step_number);
    if (step->u.sta_test->is_station_associated == true) {
        if (kill(step->u.sta_test->dhcp_pid, SIGTERM) == 0) {
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: step number : %d intf : %s sigterm success for dhcp pid : %d\n", __func__,
                __LINE__, step->step_number, step->u.sta_test->sta_interface_name.c_str(),
                step->u.sta_test->dhcp_pid);
        } else {
            wlan_emu_print(wlan_emu_log_level_info,
                "%s:%d: step number : %d intf : %s sigterm failed for dhcp pid : %d\n", __func__,
                __LINE__, step->step_number, step->u.sta_test->sta_interface_name.c_str(),
                step->u.sta_test->dhcp_pid);
        }
        m_sta_mgr->remove_sta(step->u.sta_test);
        step->u.sta_test->is_station_associated = false;
    }

    if (step->is_step_initialized == true) {
        delete step->u.sta_test->radio_oper_param;
        delete step->u.sta_test->sta_vap_config;
        delete step->u.sta_test;
        step->is_step_initialized = false;
    }

    cJSON_free(step->artifact_json_list);

    delete step;
    step = NULL;
}

int wlan_ext_test_step_param_sta_management::wlan_ext_step_frame_filter(wlan_emu_msg_t *msg)
{
    wlan_ext_test_step_params_t *step = this;
    wlan_ext_emu_sta_mgr_t *m_sta_mgr = get_agent_sta_mgr();
    wlan_emu_msg_data_t *f_data = NULL;
    char client_macaddr[32] = { 0 };
    char macaddr[32] = { 0 };
    char sta_mac[32] = { 0 };
    std::string dhcp_cmd;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step number : %d\n", __func__, __LINE__,
        step->step_number);
    int ret;
    const char *temp_cmd;

    if (msg == NULL) {
        return RETURN_UNHANDLED;
    }

    switch (msg->get_msg_type()) {
    case wlan_emu_msg_type_frm80211: // mgmt
        f_data = msg->get_msg();

        uint8_mac_to_string_mac(f_data->u.frm80211.u.frame.client_macaddr, client_macaddr);
        uint8_mac_to_string_mac(f_data->u.frm80211.u.frame.macaddr, macaddr);
        uint8_mac_to_string_mac(step->u.sta_test->sta_vap_config->u.sta_info.mac, sta_mac);

        if ((memcmp(step->u.sta_test->sta_vap_config->u.sta_info.mac,
                 f_data->u.frm80211.u.frame.client_macaddr, sizeof(mac_addr_t)) == 0) ||
            (memcmp(step->u.sta_test->sta_vap_config->u.sta_info.mac,
                 f_data->u.frm80211.u.frame.macaddr, sizeof(mac_addr_t)) == 0)) {
            if (msg->get_msgname_from_msgtype() != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: invalid msgname received from macaddr : %s client_macaddr : %s\n",
                    __func__, __LINE__, macaddr, client_macaddr);
                return RETURN_UNHANDLED;
            }

            if ((wlan_emu_frm80211_ops_type_deauth == msg->get_frm80211_ops_type()) ||
                (wlan_emu_frm80211_ops_type_disassoc == msg->get_frm80211_ops_type())) {
                // Added this check to make sure the Station MAC is present as part of frame
                // received.
                if (step->u.sta_test->is_station_associated == true) {
                    step->u.sta_test->is_station_associated = false;
                    m_sta_mgr->remove_sta(step->u.sta_test);
                    step->u.sta_test->is_decoded = false;
                }
            }

            if (memcmp(step->u.sta_test->sta_vap_config->u.sta_info.mac,
                    f_data->u.frm80211.u.frame.client_macaddr, sizeof(mac_addr_t)) == 0) {

                if (step->u.sta_test->sta_vap_config->u.sta_info.security.mode ==
                    wifi_security_mode_none) {
                    if (wlan_emu_frm80211_ops_type_assoc_resp == msg->get_frm80211_ops_type()) {
                        step->u.sta_test->is_station_associated = true;
                        wlan_emu_print(wlan_emu_log_level_dbg,
                            "%s:%d: captured assoc response for open security for mac %s\n",
                            __func__, __LINE__, client_macaddr);
                    }

                } else {
                    if (wlan_emu_frm80211_ops_type_eapol == msg->get_frm80211_ops_type()) {
                        if (strncmp(msg->get_msg_name(), "eapol-msg3", strlen("eapol-msg3")) == 0) {
                            step->u.sta_test->is_station_associated = true;
                            wlan_emu_print(wlan_emu_log_level_dbg,
                                "%s:%d: captured eapol-msg3 for %s\n", __func__, __LINE__,
                                client_macaddr);
                        }
                    }
                }

                if ((step->capture_frames != true) ||
                    (!(step->frame_request.msg_type & (1 << msg->get_msg_type())))) {
                    return RETURN_UNHANDLED;
                }

                if (!(step->frame_request.frm80211_ops & (1 << msg->get_frm80211_ops_type()))) {
                    return RETURN_UNHANDLED;
                }
                create_pcap(msg);
                return RETURN_HANDLED;
            }
            if (step->u.sta_test->capture_sta_requests == true) {
                wlan_emu_print(wlan_emu_log_level_dbg,
                    "%s:%d: capture_sta_requests in macaddr : %s client_macaddr : %s\n", __func__,
                    __LINE__, macaddr, client_macaddr);
                if (memcmp(step->u.sta_test->sta_vap_config->u.sta_info.mac,
                        f_data->u.frm80211.u.frame.macaddr, sizeof(mac_addr_t)) == 0) {
                    if ((step->capture_frames != true) ||
                        (!(step->frame_request.msg_type & (1 << msg->get_msg_type())))) {
                        return RETURN_UNHANDLED;
                    }

                    if (!(step->frame_request.frm80211_ops & (1 << msg->get_frm80211_ops_type()))) {
                        return RETURN_UNHANDLED;
                    }
                    create_pcap(msg);
                    return RETURN_HANDLED;
                }
            }
        } else {
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: unhandled frame for mac received macaddr : %s client_macaddr : %s sta_mac "
                ": %s\n",
                __func__, __LINE__, macaddr, client_macaddr, sta_mac);
        }
        break;
    case wlan_emu_msg_type_agent:
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: agent step number : %d msg_type : %d\n",
            __func__, __LINE__, step->step_number, msg->get_msg_type());
        f_data = msg->get_msg();

        if (f_data->u.agent_msg.u.agent_notif.sub_ops_type ==
            wlan_msg_ext_agent_ops_sub_type_wifi_notification) {

            if ((memcmp(step->u.sta_test->sta_vap_config->u.sta_info.mac,
                     f_data->u.agent_msg.u.agent_notif.u.wifi_sta_notif.sta_mac_addr,
                     sizeof(mac_addr_t)) == 0)) {
                uint8_mac_to_string_mac(step->u.sta_test->sta_vap_config->u.sta_info.mac, sta_mac);
                if (f_data->u.agent_msg.u.agent_notif.u.wifi_sta_notif.sta_state ==
                    wlan_emu_ext_wifi_sta_state_connected) {
                    wlan_emu_print(wlan_emu_log_level_dbg,
                        "%s:%d: step number : %d sta_mac : %s connected for interface : %s\n",
                        __func__, __LINE__, step->step_number, sta_mac,
                        step->u.sta_test->sta_interface_name.c_str());

                    dhcp_cmd = std::string("/sbin/udhcpc -i ") +
                        step->u.sta_test->sta_interface_name + std::string(" -n -q ");
                    wlan_emu_print(wlan_emu_log_level_dbg,
                        "%s:%d: step number : %d sta_mac : %s dhcp_cmd : %s\n", __func__, __LINE__,
                        step->step_number, sta_mac, dhcp_cmd.c_str());

                    int max_retries = 3;
                    for (int attempt = 0; attempt <= 3; ++attempt) {
                        int result = execute_process_once(dhcp_cmd, &step->u.sta_test->dhcp_pid,
                            true);

                        if (result == 0) {
                            wlan_emu_print(wlan_emu_log_level_dbg,
                                "%s:%d: step number : %d  dhcp pid : %d for  interface : %s\n",
                                __func__, __LINE__, step->step_number, step->u.sta_test->dhcp_pid,
                                step->u.sta_test->sta_interface_name.c_str());

                            if (get_ip_from_interface_name(step->u.sta_test->sta_interface_name,
                                    step->u.sta_test->ip_address) != RETURN_OK) {
                                wlan_emu_print(wlan_emu_log_level_dbg,
                                    "%s:%d: step number : %d dhcp pid : %d interface : %s "
                                    "ip_address failed\n",
                                    __func__, __LINE__, step->step_number,
                                    step->u.sta_test->dhcp_pid,
                                    step->u.sta_test->sta_interface_name.c_str(),
                                    step->u.sta_test->ip_address.c_str());

                            } else {
                                wlan_emu_print(wlan_emu_log_level_dbg,
                                    "%s:%d: step number : %d dhcp pid : %d interface : %s "
                                    "ip_address %s\n",
                                    __func__, __LINE__, step->step_number,
                                    step->u.sta_test->dhcp_pid,
                                    step->u.sta_test->sta_interface_name.c_str(),
                                    step->u.sta_test->ip_address.c_str());
                                step->u.sta_test->is_ip_assigned = true;
                            }

                            return RETURN_HANDLED;
                        } else {
                            usleep(500000);
                        }
                    }

                    return RETURN_HANDLED;
                } else if (f_data->u.agent_msg.u.agent_notif.u.wifi_sta_notif.sta_state ==
                    wlan_emu_ext_wifi_sta_state_disconnected) {
                    wlan_emu_print(wlan_emu_log_level_dbg,
                        "%s:%d: step number : %d sta_mac : %s disconnected for interface : %s\n",
                        __func__, __LINE__, step->step_number, sta_mac,
                        step->u.sta_test->sta_interface_name.c_str());

                    if (kill(step->u.sta_test->dhcp_pid, SIGTERM) == 0) {
                        wlan_emu_print(wlan_emu_log_level_dbg,
                            "%s:%d: step number : %d intf : %s kill success dhcp pid : %d\n",
                            __func__, __LINE__, step->step_number,
                            step->u.sta_test->sta_interface_name.c_str(),
                            step->u.sta_test->dhcp_pid);
                    } else {
                        wlan_emu_print(wlan_emu_log_level_dbg,
                            "%s:%d: step number : %d intf : %s kill failed for dhcp pid : %d\n",
                            __func__, __LINE__, step->step_number,
                            step->u.sta_test->sta_interface_name.c_str(),
                            step->u.sta_test->dhcp_pid);
                    }
                    step->u.sta_test->is_ip_assigned = false;
                    step->u.sta_test->ip_address.clear();

                    return RETURN_HANDLED;
                }
            }
        }
        break;
    case wlan_emu_msg_type_cfg80211: // beacon
    case wlan_emu_msg_type_webconfig: // onewifi_webconfig
    default:
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Not supported msg_type : %d\n", __func__,
            __LINE__, msg->get_msg_type());
        break;
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: unhandled step number : %d msg_type : %d\n",
        __func__, __LINE__, step->step_number, msg->get_msg_type());
    return RETURN_UNHANDLED;
}

void wlan_ext_test_step_param_sta_management::wlan_ext_step_status_update(cJSON *agent_status)
{
    char *json_str;
    wlan_ext_test_step_params_t *step = this;

    cJSON *step_private;

    step_private = cJSON_CreateObject();

    cJSON_AddStringToObject(step_private, "InterfaceName",
        step->u.sta_test->sta_interface_name.c_str());
    if (step->u.sta_test->is_ip_assigned == true) {
        cJSON_AddStringToObject(step_private, "IpAddress", step->u.sta_test->ip_address.c_str());
    }
    step->update_step_status_json(agent_status, step_private);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Interface Name : %s\n", __func__, __LINE__,
        step->u.sta_test->sta_interface_name.c_str());
}

int wlan_ext_test_step_param_sta_management::wlan_ext_step_decode_subdocument(cJSON *root_json,
    wlan_ext_test_step_params_t *step)
{
    cJSON *param = NULL;
    cJSON *entry = NULL;
    cJSON *vap_config = NULL;
    cJSON *sec_config = NULL;
    cJSON *radio_operation_params = NULL;
    cJSON *frame = NULL;
    cJSON *frame_type = NULL;
    cJSON *param_loc = NULL;
    frame_capture_request_t frame_capture_req;
    station_prototype_pcaps_t *tmp_station_prototype_pcap;
    sta_test_t *test_params = step->u.sta_test;

    decode_param_string(root_json, "StationType", param);
    test_params->sta_type = (sta_model_type_t)wlan_common_utils::get_sta_type_enum(
        param->valuestring);

    decode_param_string(root_json, "StationName", param);
    copy_string(test_params->sta_name, param->valuestring, sizeof(test_params->sta_name) - 1);

    decode_param_integer(root_json, "TestDuration", param);
    test_params->u.sta_management.test_duration = param->valuedouble;
    step->execution_time = test_params->u.sta_management.test_duration;

    decode_param_string(root_json, "CustomStationMac", param);
    string_mac_to_uint8_mac(test_params->custom_mac, param->valuestring);
    decode_param_object(root_json, "WifiVapConfig", vap_config);

    decode_param_string(vap_config, "VapName", param);
    copy_string(test_params->u.sta_management.ap_vap_name, param->valuestring,
        sizeof(test_params->u.sta_management.ap_vap_name) - 1);

    test_params->sta_vap_config->vap_mode = wifi_vap_mode_sta;

    decode_param_integer(vap_config, "RadioIndex", param);
    test_params->sta_vap_config->radio_index = (wifi_vap_mode_t)param->valueint;
    step->radio_index = test_params->sta_vap_config->radio_index;

    decode_param_string(vap_config, "SSID", param);
    copy_string(test_params->sta_vap_config->u.sta_info.ssid, param->valuestring,
        sizeof(test_params->sta_vap_config->u.sta_info.ssid) - 1);

    decode_param_string(vap_config, "BSSID", param);
    string_mac_to_uint8_mac(test_params->sta_vap_config->u.sta_info.bssid, param->valuestring);

    test_params->sta_vap_config->u.sta_info.enabled = true;

    decode_param_object(vap_config, "Security", sec_config);

    decode_param_string(sec_config, "SecurityMode", param);
    test_params->sta_vap_config->u.sta_info.security.mode =
        (wifi_security_modes_t)wlan_common_utils::get_secu_mode_enum(param->valuestring);
    decode_param_string(sec_config, "MFPConfig", param);
    test_params->sta_vap_config->u.sta_info.security.mfp =
        (wifi_mfp_cfg_t)wlan_common_utils::get_mfp_enum(param->valuestring);

    decode_param_string(sec_config, "EncryptionMethod", param);
    test_params->sta_vap_config->u.sta_info.security.encr =
        (wifi_encryption_method_t)wlan_common_utils::get_encryption_enum(param->valuestring);

    decode_param_bool(sec_config, "Wpa3_transition_disabled", param);
    test_params->sta_vap_config->u.sta_info.security.wpa3_transition_disable =
        (param->type & cJSON_True) ? true : false;

    decode_param_string(sec_config, "Passphrase", param);
    copy_string(test_params->sta_vap_config->u.sta_info.security.u.key.key, param->valuestring,
        sizeof(test_params->sta_vap_config->u.sta_info.security.u.key.key) - 1);

    decode_param_object(root_json, "RadioOperatingParams", radio_operation_params);
    decode_param_integer(radio_operation_params, "Channel", param);
    test_params->radio_oper_param->channel = param->valuedouble;
    test_params->radio_oper_param->enable = true;

    // station proto to be handled later on
    // Handle RSSI values also

    return RETURN_OK;
}

wlan_ext_test_step_param_sta_management::wlan_ext_test_step_param_sta_management()
{
    wlan_ext_test_step_params_t *step = this;
    step->timeout_count = 0;
    step->artifact_json_list = NULL;
    step->is_step_result_updated = false;
    step->step_state = wlan_emu_tests_state_cmd_request;
    step->artifact_json_list = cJSON_CreateArray();

    step->u.sta_test = new (std::nothrow) sta_test_t;
    if (step->u.sta_test == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: allocation of memory for sta_test failed for %d\n", __func__, __LINE__,
            step->step_number);
        step->is_step_initialized = false;
        return;
    }

    step->u.sta_test->is_station_associated = false;
    step->u.sta_test->is_decoded = false;
    step->u.sta_test->sta_vap_config = new (std::nothrow) wifi_vap_info_t;
    if (step->u.sta_test->sta_vap_config == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: allocation of sta_vap_config  memory for sta failed for %d\n", __func__,
            __LINE__, step->step_number);
        delete step->u.sta_test;
        step->is_step_initialized = false;
        return;
    }
    memset(step->u.sta_test->sta_vap_config, 0, sizeof(wifi_vap_info_t));

    step->u.sta_test->radio_oper_param = new (std::nothrow) wifi_radio_operationParam_t;
    if (step->u.sta_test->radio_oper_param == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: allocation of radio_oper_param memory for sta failed for %d\n", __func__,
            __LINE__, step->step_number);
        delete step->u.sta_test->sta_vap_config;
        delete step->u.sta_test;
        step->is_step_initialized = false;
        return;
    }
    step->u.sta_test->is_ip_assigned = false;
    step->u.sta_test->ip_address.clear();
    memset(step->u.sta_test->radio_oper_param, 0, sizeof(wifi_radio_operationParam_t));
    step->u.sta_test->is_ip_assigned = false;
    step->u.sta_test->ip_address.clear();
}

wlan_ext_test_step_param_sta_management::~wlan_ext_test_step_param_sta_management()
{
}
