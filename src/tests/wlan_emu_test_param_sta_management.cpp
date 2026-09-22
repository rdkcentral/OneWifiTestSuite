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
#include "wlan_emu_log.h"
#include "wlan_emu_sta_mgr.h"
#include "wlan_emu_test_params.h"
#include "wlan_emu_err_code.h"
#include "wlan_common_utils.h"
#include <assert.h>
#include <memory>
#define STATION_STEP_EXEC_TIMEOUT 3

// Adding extra time time for external simulated client
static int external_sta_grace_timeout = 3;

int test_step_param_sta_management::decode_user_ap_config(cJSON *sta_root_json,
    wifi_vap_info_t *ap_vap_info)
{
    test_step_params_t *step_config = this;
    webconfig_cci_t *webconfig_data = step_config->m_ui_mgr->get_webconfig_data();
    int radio_index, band;
    cJSON *vap_json_config = NULL, *security_config = NULL;

    vap_json_config = cJSON_GetObjectItem(sta_root_json, "WifiVapConfig");
    if (vap_json_config != NULL) {
        if (step_config->m_ui_mgr->update_vap_common_object(vap_json_config, ap_vap_info) !=
            RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: Json parse failed for update_vap_common_object\n", __func__, __LINE__);
            step_config->m_ui_mgr->dump_json(vap_json_config, __func__, __LINE__);
            return RETURN_ERR;
        }

        radio_index = convert_vap_name_to_radio_array_index(&webconfig_data->hal_cap.wifi_prop,
            ap_vap_info->vap_name);
        if (radio_index < 0) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid radio Index for vap_name : %s\n",
                __func__, __LINE__, ap_vap_info->vap_name);
            return webconfig_error_decode;
        }

        if (convert_radio_index_to_freq_band(&webconfig_data->hal_cap.wifi_prop, radio_index,
                &band) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: Unable to fetch proper band for radio_index : %d\n", __func__, __LINE__,
                radio_index);
            return webconfig_error_decode;
        }

        security_config = cJSON_GetObjectItem(vap_json_config, "Security");
        if (step_config->m_ui_mgr->update_vap_security_object(security_config,
                &ap_vap_info->u.bss_info.security, band) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: Json parse failed for update_vap_security_object\n", __func__, __LINE__);
            step_config->m_ui_mgr->dump_json(vap_json_config, __func__, __LINE__);
            return RETURN_ERR;
        }
    }

    return RETURN_OK;
}

int test_step_param_sta_management::update_sta_config(wifi_vap_info_t *ap_vap_config)
{
    test_step_params_t *step_config = this;
    wifi_vap_info_t *sta_vap_config = step_config->u.sta_test->sta_vap_config;

    sta_vap_config->vap_mode = wifi_vap_mode_sta;
    snprintf(sta_vap_config->bridge_name, sizeof(sta_vap_config->bridge_name), "%s",
        ap_vap_config->bridge_name);
    sta_vap_config->u.sta_info.enabled = true;
    sta_vap_config->u.sta_info.scan_params.period = 10;
    snprintf(sta_vap_config->u.sta_info.ssid, sizeof(sta_vap_config->u.sta_info.ssid), "%s",
        ap_vap_config->u.bss_info.ssid);
    // since station does not support wifi_security_mode_wpa3_transition,
    // So changing the security mode to wifi_security_mode_wpa3_personal
    if (ap_vap_config->u.bss_info.security.mode == wifi_security_mode_wpa3_transition) {
        sta_vap_config->u.sta_info.security.mode = wifi_security_mode_wpa3_personal;
    }
    sta_vap_config->u.sta_info.security.mode = ap_vap_config->u.bss_info.security.mode;
    sta_vap_config->u.sta_info.security.encr = ap_vap_config->u.bss_info.security.encr;
    sta_vap_config->u.sta_info.security.mfp = ap_vap_config->u.bss_info.security.mfp;
    if ((sta_vap_config->u.sta_info.security.mode == wifi_security_mode_wpa3_enterprise) ||
        (sta_vap_config->u.sta_info.security.mode == wifi_security_mode_wpa2_enterprise)) {
        snprintf(sta_vap_config->u.sta_info.security.u.radius.identity,
            sizeof(ap_vap_config->u.bss_info.security.u.radius.identity), "%s",
            ap_vap_config->u.bss_info.security.u.radius.identity);
        snprintf(sta_vap_config->u.sta_info.security.u.radius.key,
            sizeof(sta_vap_config->u.sta_info.security.u.radius.key), "%s",
            ap_vap_config->u.bss_info.security.u.radius.key);
        sta_vap_config->u.sta_info.security.u.radius.eap_type =
            ap_vap_config->u.bss_info.security.u.radius.eap_type;
        sta_vap_config->u.sta_info.security.u.radius.phase2 =
            ap_vap_config->u.bss_info.security.u.radius.phase2;
    } else {
        snprintf(sta_vap_config->u.sta_info.security.u.key.key,
            sizeof(sta_vap_config->u.sta_info.security.u.key.key), "%s",
            ap_vap_config->u.bss_info.security.u.key.key);
    }
    // snprintf((char *)sta_vap_config->u.sta_info.bssid, sizeof(sta_vap_config->u.sta_info.bssid),
    // "%s", ap_vap_config->u.bss_info.bssid);
    memcpy(sta_vap_config->u.sta_info.bssid, ap_vap_config->u.bss_info.bssid,
        sizeof(sta_vap_config->u.sta_info.bssid));
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: ssid : %s\n", __func__, __LINE__,
        sta_vap_config->u.sta_info.ssid);

    wlan_emu_print(wlan_emu_log_level_dbg,
        "%s:%d: radio_index : %d bridge_name : %s mode : %d enc : %d\n", __func__, __LINE__,
        sta_vap_config->radio_index, sta_vap_config->bridge_name,
        sta_vap_config->u.sta_info.security.mode, sta_vap_config->u.sta_info.security.encr);
    return RETURN_OK;
}

bool is_op_mode_supported(const char *op_mode)
{
    const char *supported_op_modes[] = { "HT", "VHT", "HE", "EHT" };
    for (int i = 0; i < (sizeof(supported_op_modes) / sizeof(supported_op_modes[0])); i++) {
        if (strcmp(supported_op_modes[i], op_mode) == 0)
            return true;
    }
    return false;
}

int get_op_mode(const char *op_mode)
{
    if (strcmp(op_mode, "HT") == 0) {
        return wlan_emu_mode_ht;
    } else if (strcmp(op_mode, "VHT") == 0) {
        return wlan_emu_mode_vht;
    } else if (strcmp(op_mode, "HE") == 0) {
        return wlan_emu_mode_he;
    } else if (strcmp(op_mode, "EHT") == 0) {
        return wlan_emu_mode_eht;
    }

    return 0;
}

bool is_bitrate_supported(const char *bitrate)
{
    const char *supported_bitrates[] = { "1.0", "2.0", "5.5", "6.0", "9.0", "11.0", "12.0", "18.0",
        "24.0", "36.0", "48.0", "54.0" };
    for (int i = 0; i < (sizeof(supported_bitrates) / sizeof(supported_bitrates[0])); i++) {
        if (strcmp(supported_bitrates[i], bitrate) == 0)
            return true;
    }
    return false;
}

int test_step_param_sta_management::decode_step_sta_management_config()
{
    cJSON *param = NULL;
    cJSON *pattern_root = NULL;
    cJSON *pattern = NULL;
    cJSON *ap_json_config = NULL;
    cJSON *sta_root_json = NULL;
    cJSON *op_mode = NULL;
    cJSON *op_modes = NULL;
    char *json_data;
    int rc;
    wifi_radio_operationParam_t *radio_oper_param = NULL;

    test_step_params_t *step_config = this;
    wifi_vap_info_t *ap_vap_info = NULL;

    rc = step_config->m_ui_mgr->read_config_file(step_config->u.sta_test->test_station_config,
        &json_data);
    if (rc != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed reading from file : %s\n", __func__,
            __LINE__, step_config->u.sta_test->test_station_config);
        return RETURN_ERR;
    }

    sta_root_json = cJSON_Parse(json_data);
    if (sta_root_json == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Json parse failed for %s\n", __func__,
            __LINE__, json_data);
        free(json_data);
        return RETURN_ERR;
    }
    free(json_data);

    param = cJSON_GetObjectItem(sta_root_json, "ConnectionType");
    if (param != NULL && (cJSON_IsString(param) == true) && (param->valuestring != NULL)) {
        if (strcmp(param->valuestring, "Internal") == 0) {
            step_config->u.sta_test->connection_type = client_connection_type_internal;
        } else if (strcmp(param->valuestring, "External") == 0) {
            step_config->u.sta_test->connection_type = client_connection_type_external;
        } else {
            step_config->u.sta_test->connection_type = client_connection_type_no_user_input;
        }
    } else {
        if (step_config->m_ext_sta_mgr->get_num_free_clients() > 0) {
            step_config->u.sta_test->connection_type = client_connection_type_external;
        } else {
            step_config->u.sta_test->connection_type = client_connection_type_internal;
        }
    }

    decode_param_string(sta_root_json, "StationType", param);
    if (strcmp(param->valuestring, "Iphone") == 0) {
        step_config->u.sta_test->sta_type = sta_model_type_iphone;
    } else if (strcmp(param->valuestring, "Pixel") == 0) {
        step_config->u.sta_test->sta_type = sta_model_type_pixel;
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid valuestring for clienttype : %s\n",
            __func__, __LINE__, param->valuestring);
        return RETURN_ERR;
    }
    decode_param_string(sta_root_json, "StationName", param);
    snprintf(step_config->u.sta_test->sta_name, sizeof(step_config->u.sta_test->sta_name), "%s",
        param->valuestring);

    decode_param_string(sta_root_json, "VapName", param);
    snprintf(step_config->u.sta_test->u.sta_management.ap_vap_name,
        sizeof(step_config->u.sta_test->u.sta_management.ap_vap_name), "%s", param->valuestring);

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Station AP Vapname : %s\n", __func__, __LINE__,
        step_config->u.sta_test->u.sta_management.ap_vap_name);
    ap_json_config = cJSON_GetObjectItem(sta_root_json, "WifiVapConfig");
    ap_vap_info = step_config->m_ui_mgr->get_cci_vap_info(
        step_config->u.sta_test->u.sta_management.ap_vap_name);

    decode_user_ap_config(sta_root_json, ap_vap_info);

    step_config->u.sta_test->sta_test_type = sta_test_type_management;
    step_config->u.sta_test->profile.mob = sta_mobility_profile_type_static;
    step_config->u.sta_test->profile.conn = sta_management_profile_type_connected;
    update_sta_config(ap_vap_info);

    memcpy(step_config->u.sta_test->radio_oper_param,
        step_config->m_ui_mgr->cci_get_radio_operation_param(ap_vap_info->radio_index),
        sizeof(wifi_radio_operationParam_t));

    param = cJSON_GetObjectItem(sta_root_json, "CustomStationMac");
    if (param != NULL && (cJSON_IsString(param) == true) && (param->valuestring != NULL) &&
        ((WiFi_IsValidMacAddr(param->valuestring) == TRUE))) {
        string_mac_to_uint8_mac(step_config->u.sta_test->custom_mac, param->valuestring);
    }

    param = cJSON_GetObjectItem(sta_root_json, "TestDuration");
    if ((param == NULL) || (cJSON_IsNumber(param) == false)) {
        step_config->u.sta_test->u.sta_management.test_duration = 0;
    } else {
        step_config->u.sta_test->u.sta_management.test_duration = param->valuedouble;
        step_config->execution_time = step_config->u.sta_test->u.sta_management.test_duration;
        step_config->u.sta_test->u.sta_management.is_sta_management_timer = true;

        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: sta test : %d execution time is : %d\n",
            __func__, __LINE__, step_config->step_number, step_config->execution_time);

        step_config->u.sta_test->u.sta_management.connectivity_q = NULL;
        pattern_root = cJSON_GetObjectItem(sta_root_json, "SignalPattern");
        if ((pattern_root != NULL) && (cJSON_IsArray(pattern_root) == true)) {
            step_config->u.sta_test->u.sta_management.connectivity_q = queue_create();
            if (step_config->u.sta_test->u.sta_management.connectivity_q == NULL) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: Invalid valuestring for clienttype : %s\n", __func__, __LINE__,
                    param->valuestring);
                cJSON_Delete(sta_root_json);
                return RETURN_ERR;
            }

            cJSON_ArrayForEach(pattern, pattern_root) {
                station_connectivity_profile_t *connect_profile = new (std::nothrow)
                    station_connectivity_profile_t;
                if (connect_profile == NULL) {
                    wlan_emu_print(wlan_emu_log_level_err,
                        "%s:%d: allocation for connect profile failed\n", __func__, __LINE__);
                    cJSON_Delete(sta_root_json);
                    return RETURN_ERR;
                }

                decode_param_string(pattern, "Rssi", param);
                connect_profile->rssi = atoi(param->valuestring);
                /*
                                decode_param_string(pattern, "Noise", param);
                                connect_profile->noise = atoi(param->valuestring);
                */
                // This is a temporary change, As controller team might take time to do add noise so
                // making Noise as optional
                param = cJSON_GetObjectItem(pattern, "Noise");
                if (param != NULL && (cJSON_IsString(param) == true) &&
                    (param->valuestring != NULL)) {
                    connect_profile->noise = atoi(param->valuestring);
                } else {
                    connect_profile->noise = -85;
                }
                decode_param_string(pattern, "Duration", param);
                connect_profile->duration = atoi(param->valuestring);
                wlan_emu_print(wlan_emu_log_level_dbg,
                    "%s:%d: Rssi : %d Duration : %d Noise : %d\n", __func__, __LINE__,
                    connect_profile->rssi, connect_profile->duration, connect_profile->noise);
                connect_profile->test_state = test_state_pending;
                connect_profile->counter = 0;
                queue_push(step_config->u.sta_test->u.sta_management.connectivity_q,
                    connect_profile);
            }
            step_config->u.sta_test->u.sta_management.current_profile_count =
                queue_count(step_config->u.sta_test->u.sta_management.connectivity_q) - 1;
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Queue_count : %d\n", __func__, __LINE__,
                step_config->u.sta_test->u.sta_management.current_profile_count);
        }
    }
    step_config->u.sta_test->u.sta_management.pre_assoc_stats = new (std::nothrow)
        pre_station_connectivity_profile_t;
    if (step_config->u.sta_test->u.sta_management.pre_assoc_stats == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: allocation for pre_assoc_stats failed\n",
            __func__, __LINE__);
        return RETURN_ERR;
    }
    memset(step_config->u.sta_test->u.sta_management.pre_assoc_stats, 0,
        sizeof(pre_station_connectivity_profile_t));
    pre_station_connectivity_profile_t *pre_connect_profile = new (std::nothrow)
        pre_station_connectivity_profile_t;
    if (pre_connect_profile == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: allocation for connect profile failed\n",
            __func__, __LINE__);
        return RETURN_ERR;
    }
    pattern = cJSON_GetObjectItem(sta_root_json, "PreAssociationSignalPattern");
    if (pattern == NULL) {
        pre_connect_profile->pre_assoc_rssi = -25;
        pre_connect_profile->pre_assoc_noise = -85;
        pre_connect_profile->pre_assoc_bitrate = 2;
    } else {
        if (decode_param_string_fn(pattern, "Rssi", param) == RETURN_ERR) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to decode Rssi\n", __func__,
                __LINE__);
            delete (pre_connect_profile);
            return RETURN_ERR;
        }
        pre_connect_profile->pre_assoc_rssi = atoi(param->valuestring);
        if (decode_param_string_fn(pattern, "Noise", param) == RETURN_ERR) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to decode Noise\n", __func__,
                __LINE__);
            delete (pre_connect_profile);
            return RETURN_ERR;
        }
        pre_connect_profile->pre_assoc_noise = atoi(param->valuestring);
        if (decode_param_string_fn(pattern, "Bitrate", param) == RETURN_ERR) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to decode Bitrate\n", __func__,
                __LINE__);
            delete (pre_connect_profile);
            return RETURN_ERR;
        }
        if (!is_bitrate_supported(param->valuestring)) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d The given bitrate %d is not supported\n",
                __func__, __LINE__, atoi(param->valuestring));
            return RETURN_ERR;
        }
        pre_connect_profile->pre_assoc_bitrate = atoi(param->valuestring) * 2;
    }

    param = cJSON_GetObjectItem(sta_root_json, "OperatingModes");

    if (param != NULL) {
        cJSON_ArrayForEach(op_modes, param) {
            op_mode = cJSON_GetObjectItem(op_modes, "OperatingMode");
            if (!is_op_mode_supported(op_mode->valuestring)) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d Unable to decode operating mode: %s\n", __func__, __LINE__,
                    param->valuestring);
            } else {
                step_config->u.sta_test->u.sta_management.op_modes |= get_op_mode(
                    op_mode->valuestring);
            }
        }
    } else {
        if (step_config->u.sta_test->radio_oper_param->variant & WIFI_80211_VARIANT_N) {
            step_config->u.sta_test->u.sta_management.op_modes |= wlan_emu_mode_ht;
        }
        if (step_config->u.sta_test->radio_oper_param->variant & WIFI_80211_VARIANT_AC) {
            step_config->u.sta_test->u.sta_management.op_modes |= wlan_emu_mode_vht;
        }
        if (step_config->u.sta_test->radio_oper_param->variant & WIFI_80211_VARIANT_AX) {
            step_config->u.sta_test->u.sta_management.op_modes |= wlan_emu_mode_he;
        }
        if (step_config->u.sta_test->radio_oper_param->variant & WIFI_80211_VARIANT_BE) {
            step_config->u.sta_test->u.sta_management.op_modes |= wlan_emu_mode_eht;
        }
    }

    param = cJSON_GetObjectItem(sta_root_json, "auto_reconnect");
    if (param != NULL && (cJSON_IsBool(param) == true)) {
        step_config->u.sta_test->is_reconnect_enabled = cJSON_IsTrue(param) ? true : false;
    }
    if (step_config->u.sta_test->is_reconnect_enabled == true) {
        param = cJSON_GetObjectItem(sta_root_json, "reconnect_interval");
        if (param != NULL && (cJSON_IsNumber(param) == true)) {
            step_config->u.sta_test->reconnect_interval = param->valueint;
        }
    }
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Auto Reconnect : %s Reconnect Interval : %d\n",
        __func__, __LINE__, step_config->u.sta_test->is_reconnect_enabled ? "Enabled" : "Disabled",
        step_config->u.sta_test->reconnect_interval);

    wlan_emu_print(wlan_emu_log_level_dbg,
        "%s:%d: Rssi : %d Noise : %d Bitrate : %d op_modes : 0x%x\n", __func__, __LINE__,
        pre_connect_profile->pre_assoc_rssi, pre_connect_profile->pre_assoc_noise,
        pre_connect_profile->pre_assoc_bitrate, step_config->u.sta_test->u.sta_management.op_modes);
    step_config->u.sta_test->u.sta_management.pre_assoc_stats = pre_connect_profile;

    cJSON_Delete(sta_root_json);
    return RETURN_OK;
}

int test_step_param_sta_management::step_execute()
{
    char file_to_read[128] = { 0 };
    int ret = 0;
    std::string cli_subdoc;
    char timestamp[24] = { 0 };
    cJSON *json;
    char mac_str[32] = { 0 };
    FILE *fp;
    char *json_str;
    char sta_connect_info[256] = { 0 };

    test_step_params_t *step = this;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Called for Test Step Num : %d\n", __func__,
        __LINE__, step->step_number);

    //    step->frame_capture.msg_type |= 1<<wlan_emu_msg_type_webconfig;
    //    step->frame_capture.subdoc_type = webconfig_subdoc_type_associated_clients;
    if (step->u.sta_test->is_decoded == false) {
        if (decode_step_sta_management_config() == RETURN_ERR) {
            return RETURN_ERR;
        }
        step->u.sta_test->is_decoded = true;
        step->u.sta_test->test_id = step->step_number;

        if (step->u.sta_test->connection_type == client_connection_type_internal) {
            if (step->m_sim_sta_mgr->add_sta(step->u.sta_test) == RETURN_ERR) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: add_sta failed for simulated client for step : %d\n", __func__,
                    __LINE__, step->step_number);
                return RETURN_ERR;
            }

            if (step->capture_frames == true) {
                step->test_state = wlan_emu_tests_state_cmd_continue;
            } else {
                step->test_state = wlan_emu_tests_state_cmd_results;
            }
        } else if (step->u.sta_test->connection_type == client_connection_type_external) {
            if (is_zero_mac(step->u.sta_test->custom_mac) == false) { 
                memcpy(step->u.sta_test->sta_vap_config->u.sta_info.mac, step->u.sta_test->custom_mac, sizeof(mac_address_t));
            }
            if (encode_external_sta_management_subdoc(cli_subdoc) == RETURN_ERR) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: encode failed for external client for step : %d\n", __func__, __LINE__,
                    step->step_number);
                return RETURN_ERR;
            }
            if (step->m_ext_sta_mgr->add_sta(step, cli_subdoc) == RETURN_ERR) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: add_sta failed for external client for step : %d\n", __func__, __LINE__,
                    step->step_number);
                return RETURN_ERR;
            }
            step->test_state = wlan_emu_tests_state_cmd_continue;
        }
    }

    // Creation of json for station
    if (get_current_time_string(timestamp, sizeof(timestamp)) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: get_current_time_string failed\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "StepNumber", step->step_number);
    uint8_mac_to_string_mac(step->u.sta_test->sta_vap_config->u.sta_info.mac, mac_str);
    cJSON_AddStringToObject(json, "StationMacAddress", mac_str);

    snprintf(sta_connect_info, sizeof(sta_connect_info), "%s/%s_%d_%s_STATION_%d.json",
        step->m_ui_mgr->get_test_results_dir_path(), step->test_case_id, step->step_number,
        timestamp, step->u.sta_test->sta_vap_config->radio_index);

    fp = fopen(sta_connect_info, "w");
    if (fp == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: fopen failed for %s\n", __func__, __LINE__,
            sta_connect_info);
        step->m_ui_mgr->cci_error_code = EFOPEN;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    json_str = cJSON_Print(json);

    fputs(json_str, fp);
    fclose(fp);
    free(json_str);
    cJSON_Delete(json);

    if (step->m_ui_mgr->step_upload_files(sta_connect_info) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: step_upload_files failed\n", __func__,
            __LINE__);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        step->m_ui_mgr->cci_error_code = EPUSHTSTRESFILE;
        return RETURN_ERR;
    }

    if (remove(sta_connect_info) != 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Error Removing the file : %s\n", __func__,
            __LINE__, sta_connect_info);
    }
    return RETURN_OK;
}

int test_step_param_sta_management::push_ext_sta_result_files(const std::vector<std::string> &files)
{
    int ret;
    wlan_emu_pcap_captures *capture;
    test_step_params_t *step = this;

    for (const std::string &file : files) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: result file: %s\n", __func__, __LINE__,
            file.c_str());

        capture = new (std::nothrow) wlan_emu_pcap_captures;
        if (capture == nullptr) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to allocate memory for capture\n",
                __func__, __LINE__);
            return RETURN_ERR;
        }
        memset(capture, 0, sizeof(wlan_emu_pcap_captures));

        ret = snprintf(capture->pcap_file, sizeof(capture->pcap_file), "%s", file.c_str());
        if (ret < 0 || ret >= sizeof(capture->pcap_file)) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed write file name\n", __func__,
                __LINE__);
            delete capture;
            return RETURN_ERR;
        }

        if (step->m_ui_mgr->step_upload_files(capture->pcap_file) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: step_upload_files failed\n", __func__,
                __LINE__);
            delete capture;
            step->test_state = wlan_emu_tests_state_cmd_abort;
            step->m_ui_mgr->cci_error_code = EPUSHTSTRESFILE;
            return RETURN_ERR;
        }
        delete capture;
    }

    return RETURN_OK;
}

int test_step_param_sta_management::parse_step_private_data(std::string private_data_str)
{
    cJSON *root_json = NULL, *param = NULL;
    test_step_params_t *step = this;

    root_json = cJSON_Parse(private_data_str.c_str());
    if (root_json == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: NULL json pointer\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> json_wrapper(root_json, cJSON_Delete);
    decode_param_string(root_json, "InterfaceName", param);
    step->u.sta_test->sta_interface_name = std::string(param->valuestring);

    param = cJSON_GetObjectItem(root_json, "IpAddress");
    if (param != NULL && cJSON_IsString(param)) {
        step->u.sta_test->is_ip_assigned = true;
        step->u.sta_test->ip_address = std::string(param->valuestring);
        wlan_emu_print(wlan_emu_log_level_info, "%s:%d: ip_address : %s\n", __func__, __LINE__,
            step->u.sta_test->ip_address.c_str());
    }

    return RETURN_OK;
}

int test_step_param_sta_management::step_timeout_ext_sta()
{
    wlan_emu_ext_agent_interface_t *ext_agent;
    ext_agent_status_resp_t status = {};
    test_step_params_t *step = this;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: sta test key: %s\n", __func__, __LINE__,
        step->u.sta_test->key);

    ext_agent = step->m_ext_sta_mgr->get_ext_agent(step->u.sta_test->key);
    if (ext_agent == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to find external agent for key: %s\n",
            __func__, __LINE__, step->u.sta_test->key);
        step->m_ui_mgr->cci_error_code = EEXTAGENT;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    if (ext_agent->get_external_agent_test_status(status, step->m_ui_mgr->cci_error_code) == RETURN_ERR) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to get external agent status\n",
            __func__, __LINE__);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: name: %s agent status: %s step count: %d\n",
        __func__, __LINE__, status.agent_name.c_str(),
        ext_agent->agent_state_as_string(status.state).c_str(), status.step_count);

    for (const ext_agent_step_status_t &step : status.steps) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step num: %d step state: %s\n", __func__,
            __LINE__, step.step_number, step_state_as_string(step.state).c_str());
        for (const std::string &file : step.result_files) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: result file: %s\n", __func__, __LINE__,
                file.c_str());
        }
    }

    step->timeout_count++;

    if ((step->timeout_count == 3) && (status.state == ext_agent_test_state_idle)) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: agent state : %s for step : %d\n", __func__,
            __LINE__, ext_agent->agent_state_as_string(status.state).c_str(), step->step_number);
        step->m_ui_mgr->cci_error_code = ETIMEOUT;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    if (status.state == ext_agent_test_state_running) {
        if ((step->execution_time + external_sta_grace_timeout) <= (step->timeout_count)) {
            step->test_state = wlan_emu_tests_state_cmd_results;
            wlan_emu_print(wlan_emu_log_level_info,
                "%s:%d: Test duration of %d completed for step %d\n", __func__, __LINE__,
                step->execution_time, step->step_number);

            auto step_iter = status.steps.begin();
            for (; step_iter != status.steps.end(); ++step_iter) {
                wlan_emu_print(wlan_emu_log_level_dbg,
                    "%s:%d: step_iter->step_number : %d step_state : %s agent_state : %s\n",
                    __func__, __LINE__, step_iter->step_number,
                    step_state_as_string(step_iter->state).c_str(),
                    ext_agent->agent_state_as_string(status.state).c_str());
                if (step_iter->step_number == step_number) {
                    step->test_state = step_iter->state;
                    break;
                }
            }

            if (step_iter->step_private_json_data.empty()) {
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step private data is NULL\n",
                    __func__, __LINE__);
            } else {
                wlan_emu_print(wlan_emu_log_level_info, "%s:%d: step private data is %s\n",
                    __func__, __LINE__, step_iter->step_private_json_data.c_str());
                parse_step_private_data(step_iter->step_private_json_data);
            }

            if (ext_agent->download_external_agent_result_files(step_iter->result_files, step->m_ui_mgr->cci_error_code) !=
                RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to download test results\n",
                    __func__, __LINE__);
                step->m_ui_mgr->cci_error_code = EDNLDTSTRESFILE;
                step->test_state = wlan_emu_tests_state_cmd_abort;
                return RETURN_ERR;
            }

            if (push_ext_sta_result_files(step_iter->result_files) != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to push test results\n",
                    __func__, __LINE__);
                step->m_ui_mgr->cci_error_code = EPUSHTSTRESFILE;
                step->test_state = wlan_emu_tests_state_cmd_abort;
                return RETURN_ERR;
            }

            return RETURN_OK;
        } else {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: state: %s step number: %d\n", __func__,
                __LINE__, step_state_as_string(step->test_state).c_str(), step_number);
        }
    }

    if (status.state == ext_agent_test_state_fail) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: external agent state failed for %d\n",
            __func__, __LINE__, step->step_number);
        step->m_ui_mgr->cci_error_code = EEXTAGENT;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }
    /*
        if (status.state != ext_agent_test_state_complete) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: unexpected external agent state: %d\n",
                __func__, __LINE__, status.state);
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }
        */

    auto step_iter = status.steps.begin();
    for (; step_iter != status.steps.end(); ++step_iter) {
        wlan_emu_print(wlan_emu_log_level_dbg,
            "%s:%d: step_iter->step_number : %d step_state : %s agent_state : %s\n", __func__,
            __LINE__, step_iter->step_number, step_state_as_string(step_iter->state).c_str(),
            ext_agent->agent_state_as_string(status.state).c_str());
        if (step_iter->step_number == step_number) {
            step->test_state = step_iter->state;
            break;
        }
    }

    if (step_iter == status.steps.end()) {
        wlan_emu_print(wlan_emu_log_level_info,
            "%s:%d: failed to get step state for number: %d agent_state : %s\n", __func__, __LINE__,
            step_number, ext_agent->agent_state_as_string(status.state).c_str());
        if (step->fork == true) {
            step->test_state = wlan_emu_tests_state_cmd_wait;
        } else {
            step->test_state = wlan_emu_tests_state_cmd_continue;
        }

        if (step->u.sta_test->is_ip_assigned == false) {
            step->test_state = wlan_emu_tests_state_cmd_continue;
        }
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: test state: %s step number: %d\n", __func__,
            __LINE__, step_state_as_string(step->test_state).c_str(), step_number);

        return RETURN_OK;
    }

    if (step_iter->step_private_json_data.empty()) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step private data is NULL\n", __func__,
            __LINE__);
    } else {
        //        char *json_string = cJSON_Print(step_iter->step_private_json_data);
        wlan_emu_print(wlan_emu_log_level_info, "%s:%d: step private data is %s\n", __func__,
            __LINE__, step_iter->step_private_json_data.c_str());
        parse_step_private_data(step_iter->step_private_json_data);
    }

    if (step->test_state == wlan_emu_tests_state_cmd_results) {
        if (ext_agent->download_external_agent_result_files(step_iter->result_files, step->m_ui_mgr->cci_error_code) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to download test results\n",
                __func__, __LINE__);
            step->m_ui_mgr->cci_error_code = EDNLDTSTRESFILE;
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }

        if (push_ext_sta_result_files(step_iter->result_files) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to push test results\n", __func__,
                __LINE__);
            step->m_ui_mgr->cci_error_code = EPUSHTSTRESFILE;
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }
        return RETURN_OK;
    } else if (step->test_state == wlan_emu_tests_state_cmd_abort) {
        step->m_ui_mgr->cci_error_code = ESTEPSTOPPED;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: abort step number: %d\n", __func__, __LINE__,
            step_number);
        return RETURN_ERR;
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: test state: %s step number: %d\n", __func__,
            __LINE__, step_state_as_string(step->test_state).c_str(), step_number);
        //__LINE__, ext_agent->step_state_as_string(step->test_state).c_str(), step_number);
        // step->test_state = wlan_emu_tests_state_cmd_abort;
    }

    if (step->fork == true) {
        step->test_state = wlan_emu_tests_state_cmd_wait;
    } else {
        step->test_state = wlan_emu_tests_state_cmd_continue;
    }

    if (step->u.sta_test->is_ip_assigned == false) {
        step->test_state = wlan_emu_tests_state_cmd_continue;
    }

    return RETURN_OK;
}

int test_step_param_sta_management::step_timeout()
{
    test_step_params_t *step = this;
    heart_beat_data_t *heart_beat_data;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Test Step Num : %d timeout_count : %d\n",
        __func__, __LINE__, step->step_number, step->timeout_count);

    if (step->u.sta_test->connection_type == client_connection_type_external) {
        return step_timeout_ext_sta();
    }

    if (step->test_state != wlan_emu_tests_state_cmd_results) {
        step->timeout_count++;

        if (step->execution_time == step->timeout_count) {
            step->test_state = wlan_emu_tests_state_cmd_results;
            wlan_emu_print(wlan_emu_log_level_info,
                "%s:%d: Test duration of %d  completed for step %d\n", __func__, __LINE__,
                step->execution_time, step->step_number);
            step->m_sim_sta_mgr->remove_sta(step->u.sta_test);
            step->u.sta_test->is_decoded = false;
            return RETURN_OK;
        }

        if (step->u.sta_test->is_reconnect_enabled && step->timeout_count != 0 &&
            step->u.sta_test->reconnect_interval > 0 &&
            (step->timeout_count % step->u.sta_test->reconnect_interval) == 0) {
            if (step->u.sta_test->is_station_associated == true &&
                step->m_sim_sta_mgr->disconnect_sta(step->u.sta_test) == RETURN_ERR) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: disconnect_sta failed for step %d\n",
                    __func__, __LINE__, step->step_number);
            } else if (step->u.sta_test->is_station_associated == false) {
                step->m_sim_sta_mgr->clear_interface_data(step->u.sta_test);
                step->m_sim_sta_mgr->reconnect_sta(step->u.sta_test);
                step->u.sta_test->is_decoded = false;
            }
        }

        if (step->u.sta_test->is_station_associated == true) {
            // add the logic  connectivity profile
            if (step->u.sta_test->u.sta_management.is_sta_management_timer == true) {
                if (step->u.sta_test->u.sta_management.connectivity_q != NULL) {
                    if ((queue_count(step->u.sta_test->u.sta_management.connectivity_q) > 0) &&
                        (step->u.sta_test->u.sta_management.current_profile_count >= 0)) {
                        station_connectivity_profile_t *connect_profile =
                            (station_connectivity_profile_t *)queue_peek(
                                step->u.sta_test->u.sta_management.connectivity_q,
                                step->u.sta_test->u.sta_management.current_profile_count);
                        if (connect_profile == NULL) {
                            wlan_emu_print(wlan_emu_log_level_err,
                                "%s:%d: connect_profile is NULL for  %d\n", __func__, __LINE__,
                                step->u.sta_test->u.sta_management.current_profile_count);
                            return RETURN_OK;
                        }
                        wlan_emu_print(wlan_emu_log_level_dbg,
                            "%s:%d: Rssi : %d Duration : %d Counter : %d\n", __func__, __LINE__,
                            connect_profile->rssi, connect_profile->duration,
                            connect_profile->counter);
                        connect_profile->counter++;
                        // create the heart beat data
                        heart_beat_data = new (std::nothrow) heart_beat_data_t;
                        if (heart_beat_data == NULL) {
                            wlan_emu_print(wlan_emu_log_level_err,
                                "%s:%d: Unable to send the heart beat for step %d\n", __func__,
                                __LINE__, step->step_number);
                            return RETURN_ERR;
                        }
                        memset(heart_beat_data, 0, sizeof(heart_beat_data_t));
                        memcpy(heart_beat_data->mac,
                            step->u.sta_test->sta_vap_config->u.sta_info.mac,
                            sizeof(mac_address_t));
                        heart_beat_data->rssi = connect_profile->rssi;
                        heart_beat_data->noise = connect_profile->noise;

                        step->m_sim_sta_mgr->send_heart_beat(step->u.sta_test->key,
                            heart_beat_data);
                        delete (heart_beat_data);
                        if (connect_profile->counter == connect_profile->duration) {
                            connect_profile->test_state = test_state_complete;
                            step->u.sta_test->u.sta_management.current_profile_count--;
                            if (step->u.sta_test->u.sta_management.current_profile_count == -1) {
                                step->test_state = wlan_emu_tests_state_cmd_results;
                                wlan_emu_print(wlan_emu_log_level_info,
                                    "%s:%d: connectivity test case completed for step : %d\n",
                                    __func__, __LINE__, step->step_number);
                                return RETURN_OK;
                            }
                        } else {
                            connect_profile->test_state = test_state_active;
                        }
                    }
                }
            } else {
                heart_beat_data = new (std::nothrow) heart_beat_data_t;
                if (heart_beat_data == NULL) {
                    wlan_emu_print(wlan_emu_log_level_err,
                        "%s:%d: Unable to send the heart beat for step %d\n", __func__, __LINE__,
                        step->step_number);
                    return RETURN_ERR;
                }
                memset(heart_beat_data, 0, sizeof(heart_beat_data_t));
                memcpy(heart_beat_data->mac, step->u.sta_test->sta_vap_config->u.sta_info.mac,
                    sizeof(mac_address_t));
                heart_beat_data->rssi = -25;
                heart_beat_data->noise = -85;

                step->m_sim_sta_mgr->send_heart_beat(step->u.sta_test->key, heart_beat_data);
                delete (heart_beat_data);
            }
        }

        if (step->fork == true) {
            if (step->u.sta_test->wait_connection == true) {
                // Dont go to next step until station is connected.
                if (step->u.sta_test->is_station_associated == false) {
                    wlan_emu_print(wlan_emu_log_level_dbg,
                        "%s:%d: Waiting for STA to Associate for step %d\n", __func__, __LINE__,
                        step->step_number);
                    step->test_state = wlan_emu_tests_state_cmd_continue;
                    return RETURN_OK; // Exit and wait for the next timeout
                }
            }
            // execute next available step
            step->test_state = wlan_emu_tests_state_cmd_wait;
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: wait for test Step Num : %d execution_time : %d timeout_count %d\n",
                __func__, __LINE__, step->step_number, step->execution_time, step->timeout_count);
        } else { // step->fork == false
            /*Until the execution is done remain in the same step*/
            if (step->execution_time > step->timeout_count) {
                step->test_state = wlan_emu_tests_state_cmd_continue;
                wlan_emu_print(wlan_emu_log_level_dbg,
                    "%s:%d: Test Step Num : %d execution_time : %d timeout_count %d\n", __func__,
                    __LINE__, step->step_number, step->execution_time, step->timeout_count);
            }
        }
    }

    return RETURN_OK;
}

void test_step_param_sta_management::step_remove()
{
    test_step_param_sta_management *step = dynamic_cast<test_step_param_sta_management *>(this);
    unsigned int results_count = 0;
    wlan_emu_pcap_captures *res_file = NULL;

    if (step == NULL) {
        return;
    }

    if (step->is_step_initialized == true) {
        if (step->u.sta_test->sta_vap_config != NULL) {
            wlan_emu_print(wlan_emu_log_level_info,
                "%s:%d: Disconnecting the client at vap index : %d\n", __func__, __LINE__,
                step->u.sta_test->sta_vap_config->vap_index);
            if (step->u.sta_test->is_decoded == true) {
                if (step->u.sta_test->connection_type == client_connection_type_external) {
                    step->m_ext_sta_mgr->remove_sta(step->u.sta_test);
                } else {
                    step->m_sim_sta_mgr->remove_sta(step->u.sta_test);
                }
                step->u.sta_test->is_station_associated = false;
                step->u.sta_test->is_decoded = false;
            }

            delete step->u.sta_test->sta_vap_config;
        }

        delete step->u.sta_test->radio_oper_param;

        if (step->u.sta_test->station_prototype != nullptr) {
            queue_destroy(step->u.sta_test->station_prototype->fc_prototype_q);
            step->u.sta_test->station_prototype->fc_prototype_q = nullptr;
            delete step->u.sta_test->station_prototype;
            step->u.sta_test->station_prototype = nullptr;
        }
        delete step->u.sta_test;
    }

    delete step;
    step = NULL;

    return;
}

int test_step_param_sta_management::step_frame_filter(wlan_emu_msg_t *msg)
{
    test_step_params_t *step = this;
    wlan_emu_msg_data_t *f_data = NULL;
    char client_macaddr[32] = { 0 };
    char macaddr[32] = { 0 };
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step number : %d\n", __func__, __LINE__,
        step->step_number);

    if (msg == NULL) {
        return RETURN_UNHANDLED;
    }

    // expect only wlan_emu_msg_type_cfg80211 or  wlan_emu_msg_type_webconfig
    switch (msg->get_msg_type()) {
    case wlan_emu_msg_type_frm80211: // mgmt

        // irrespective of capture_frames check for eapol-3 to confirm whether the client is
        // associated or not
        f_data = msg->get_msg();

        uint8_mac_to_string_mac(f_data->u.frm80211.u.frame.client_macaddr, client_macaddr);
        uint8_mac_to_string_mac(f_data->u.frm80211.u.frame.macaddr, macaddr);

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
                if (step->u.sta_test->is_reconnect_enabled == true &&
                    (wlan_emu_frm80211_ops_type_disassoc == msg->get_frm80211_ops_type())) {
                    wlan_emu_print(wlan_emu_log_level_err,
                        "%s:%d: STA trying to reconnect within expected time for step %d\n",
                        __func__, __LINE__, step->step_number);
                    step->u.sta_test->is_station_associated = false;
                    step->m_sim_sta_mgr->clear_interface_data(step->u.sta_test);
                    step->m_sim_sta_mgr->reconnect_sta(step->u.sta_test);
                    step->u.sta_test->is_decoded = false;
                } else if (step->u.sta_test->is_reconnect_enabled == false &&
                    step->u.sta_test->is_station_associated == true) {
                    step->u.sta_test->is_station_associated = false;
                    step->m_sim_sta_mgr->remove_sta(step->u.sta_test);
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
                msg->unload_frm80211_msg(step);
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
                    msg->unload_frm80211_msg(step);
                    return RETURN_HANDLED;
                }
            }
        } else {
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: unhandled frame for mac received macaddr : %s client_macaddr : %s\n",
                __func__, __LINE__, macaddr, client_macaddr);
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

test_step_param_sta_management::test_step_param_sta_management()
{
    test_step_params_t *step = this;
    step->is_step_initialized = true;

    step->u.sta_test = new (std::nothrow) sta_test_t;
    if (step->u.sta_test == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: allocation of memory for sta_test failed for %d\n", __func__, __LINE__,
            step->step_number);
        step->is_step_initialized = false;
        return;
    }

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
    memset(step->u.sta_test->radio_oper_param, 0, sizeof(wifi_radio_operationParam_t));

    step->u.sta_test->station_prototype = new (std::nothrow) station_prototype_t;
    if (step->u.sta_test->station_prototype == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: allocation of memory for station_prototype failed for %d\n", __func__, __LINE__,
            step->step_number);
        delete step->u.sta_test->sta_vap_config;
        delete step->u.sta_test->radio_oper_param;
        delete step->u.sta_test;
        step->is_step_initialized = false;
        return;
    }
    memset(step->u.sta_test->station_prototype, 0, sizeof(station_prototype_t));

    step->u.sta_test->station_prototype->fc_prototype_q = queue_create();
    if (step->u.sta_test->station_prototype->fc_prototype_q == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Queue create failed for fc_prototype_q\n",
            __func__, __LINE__);
        delete step->u.sta_test->sta_vap_config;
        delete step->u.sta_test->radio_oper_param;
        delete step->u.sta_test->station_prototype;
        delete step->u.sta_test;
        step->is_step_initialized = false;
        return;
    }

    step->execution_time = 3;
    step->timeout_count = 0;
    step->capture_frames = false;
    memset(step->u.sta_test->sta_vap_config, 0, sizeof(wifi_vap_info_t));
    step->u.sta_test->u.sta_management.is_sta_management_timer = false;
    step->u.sta_test->is_station_associated = false;
    step->u.sta_test->is_station_prototype_enabled = false;
    step->u.sta_test->capture_sta_requests = false;
    step->u.sta_test->wait_connection = false;
    memset(step->u.sta_test->custom_mac, 0, sizeof(mac_address_t));
    step->u.sta_test->u.sta_management.op_modes = 0;
    step->u.sta_test->is_ip_assigned = false;
    step->u.sta_test->reconnect_interval = 20;
    step->u.sta_test->is_reconnect_enabled = false;
}

int test_step_param_sta_management::encode_external_sta_management_subdoc(std::string &cli_subdoc)
{
    cJSON *json = NULL;
    char *str = NULL;
    char mac_str[32] = { 0 };
    test_step_params_t *step = this;
    sta_test_t *test_params = step->u.sta_test;
    webconfig_cci_t *webconfig_data = step->m_ui_mgr->get_webconfig_data();
    int radio_index;

    json = cJSON_CreateObject();
    if (json == NULL) {
        return RETURN_ERR;
    }
    cJSON_AddStringToObject(json, "SubDocName", "ExternalStationManagement");
    cJSON_AddStringToObject(json, "StationType",
        wlan_common_utils::get_sta_type_string(test_params->sta_type));
    cJSON_AddStringToObject(json, "StationName", test_params->sta_name);
    cJSON_AddNumberToObject(json, "StepNumber", step->step_number);
    cJSON_AddBoolToObject(json, "Fork", step->fork);
    cJSON_AddStringToObject(json, "StationName", test_params->sta_name);
    cJSON_AddStringToObject(json, "TestCaseID", step->test_case_id);
    cJSON_AddStringToObject(json, "TestCaseName", step->test_case_name);
    cJSON_AddNumberToObject(json, "TestDuration", test_params->u.sta_management.test_duration);
    // Convert uint8 to string and add
    uint8_mac_to_string_mac(test_params->custom_mac, mac_str);
    cJSON_AddStringToObject(json, "CustomStationMac", mac_str);

    cJSON *client_obj = cJSON_CreateObject();
    if (client_obj == NULL) {
        cJSON_Delete(json);
        return RETURN_ERR;
    }
    cJSON_AddItemToObject(json, "WifiVapConfig", client_obj);
    //  cJSON_AddItemToArray(client_arr, client_obj);
    cJSON_AddStringToObject(client_obj, "VapName", test_params->u.sta_management.ap_vap_name);
    radio_index = convert_vap_name_to_radio_array_index(&webconfig_data->hal_cap.wifi_prop,
        test_params->u.sta_management.ap_vap_name);

    cJSON_AddNumberToObject(client_obj, "RadioIndex", radio_index);
    cJSON_AddStringToObject(client_obj, "SSID", test_params->sta_vap_config->u.sta_info.ssid);

    uint8_mac_to_string_mac(test_params->sta_vap_config->u.sta_info.bssid, mac_str);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: bssid : %s %02X:%02X\n", __func__, __LINE__,
        mac_str, test_params->sta_vap_config->u.sta_info.bssid[0],
        test_params->sta_vap_config->u.sta_info.bssid[1]);
    cJSON_AddStringToObject(client_obj, "BSSID", mac_str);

    cJSON *security_obj = cJSON_CreateObject();
    if (security_obj == NULL) {
        cJSON_Delete(json);
        return RETURN_ERR;
    }
    cJSON_AddItemToObject(client_obj, "Security", security_obj);
    cJSON_AddStringToObject(security_obj, "SecurityMode",
        wlan_common_utils::get_secu_mode_string(
            test_params->sta_vap_config->u.sta_info.security.mode));
    cJSON_AddStringToObject(security_obj, "MFPConfig",
        wlan_common_utils::get_mfp_string(test_params->sta_vap_config->u.sta_info.security.mfp));
    cJSON_AddStringToObject(security_obj, "EncryptionMethod",
        wlan_common_utils::get_encryption_string(
            test_params->sta_vap_config->u.sta_info.security.encr));
    cJSON_AddBoolToObject(security_obj, "Wpa3_transition_disabled",
        test_params->sta_vap_config->u.sta_info.security.wpa3_transition_disable);
    cJSON_AddStringToObject(security_obj, "Passphrase",
        test_params->sta_vap_config->u.sta_info.security.u.key.key);

    cJSON_AddBoolToObject(json, "TestCapture", step->capture_frames);

    // if TestCapture is true, then add the below
    if (step->capture_frames == true) {
        cJSON *test_capture_frame_arr = cJSON_CreateArray();
        if (test_capture_frame_arr == NULL) {
            cJSON_Delete(json);
            return RETURN_ERR;
        }
        cJSON_AddItemToObject(json, "TestCaptureFrame", test_capture_frame_arr);
        if (wlan_common_utils::encode_pcap_frame_type(test_capture_frame_arr,
                step->frame_request) == RETURN_ERR) {
            cJSON_Delete(json);
            return RETURN_ERR;
        }
    }
    /*
            cJSON *station_prototype_arr = cJSON_CreateArray();
            if (station_prototype_arr == NULL) {
                cJSON_Delete(json);
                return RETURN_ERR;
            }
            cJSON_AddItemToObject(json, "StationPrototypeFrames", station_prototype_arr);
            if (add_station_proto_type(step, station_prototype_arr) == RETURN_ERR) {
                cJSON_Delete(station_prototype_arr);
                return RETURN_ERR;
            }
            */

    cJSON *radio_obj = cJSON_CreateObject();
    if (radio_obj == NULL) {
        cJSON_Delete(json);
        return RETURN_ERR;
    }
    cJSON_AddItemToObject(json, "RadioOperatingParams", radio_obj);
    cJSON_AddNumberToObject(radio_obj, "Channel", test_params->radio_oper_param->channel);

    str = cJSON_Print(json);

    if (str == nullptr) {
        cJSON_Delete(json);
        return RETURN_ERR;
    }

    cli_subdoc = std::string(str);
    wlan_emu_print(wlan_emu_log_level_err, "%s:%d: external sta : %s\n", __func__, __LINE__,
        cli_subdoc.c_str());

    cJSON_free(str);
    cJSON_Delete(json);

    return RETURN_OK;
}

test_step_param_sta_management::~test_step_param_sta_management()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Destructor for sta management called\n",
        __func__, __LINE__);
}
