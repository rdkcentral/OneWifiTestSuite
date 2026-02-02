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

#include "wlan_emu_ui_mgr.h"
#include "wifi_util.h"
#include "wlan_emu.h"
#include "wlan_emu_err_code.h"
#ifndef BANANA_PI_PORT
#include <secure_wrapper.h>
#endif
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <experimental/filesystem>
#include <fcntl.h>
#include <iostream>
#include "wlan_emu_bus.h"
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#include <fstream>
#include <getopt.h>

namespace fs = std::experimental::filesystem;

unsigned char cci_test_dir[] = "cci_res";

extern "C" {
INT wifi_hal_disconnect(INT ap_index);
char *str_tolower(char *str);
int get_number_of_radios(wifi_platform_property_t *wifi_prop);
webconfig_subdoc_type_t find_subdoc_type(webconfig_t *config, cJSON *json);
}

unsigned int wlan_emu_ui_mgr_t::m_token = 0xdeadbeef;
webconfig_cci_t cci_webconfig;

http_info_t *wlan_emu_ui_mgr_t::fill_http_info()
{
    http_info_t *http_info = NULL;
    http_info = (http_info_t *)malloc(sizeof(http_info_t));

    if (http_info == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d Failed to allocate memory\n", __func__,
            __LINE__);
        return NULL;
    }
    http_info->is_local_host_enabled = is_local_host_enabled;
    if (this->m_ssl_config.get_ssl_cert() == NULL ||
        this->m_ssl_config.get_ssl_key() == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d SSL cert or key is NULL\n", __func__, __LINE__);
        free(http_info);
        return NULL;
    }
    strncpy(http_info->ssl_cert, this->m_ssl_config.get_ssl_cert(), sizeof(http_info->ssl_cert) - 1);
    strncpy(http_info->ssl_key, this->m_ssl_config.get_ssl_key(), sizeof(http_info->ssl_key) - 1);
    strncpy(http_info->interface, interface, sizeof(http_info->interface) - 1);
    strncpy(http_info->tda_url, tda_url, sizeof(http_info->tda_url) - 1);

    return http_info;
}

test_step_params_t *wlan_emu_ui_mgr_t::get_step_from_step_number(
    wlan_emu_test_case_config *test_case_config, int step_number)
{
    unsigned int count = 0;
    int i = 0;
    test_step_params_t *tmp_step = NULL;

    if (test_case_config == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: test_case_config is NULL\n", __func__,
            __LINE__);
        return NULL;
    }

    if (test_case_config->test_steps_q == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: test_steps_q is NULL\n", __func__, __LINE__);
        return NULL;
    }

    count = queue_count(test_case_config->test_steps_q);
    if (count == 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: step queue count is 0\n", __func__,
            __LINE__);
        return NULL;
    }
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step queue count is %d\n", __func__, __LINE__,
        count);

    for (i = 0; i < count; i++) {
        tmp_step = (test_step_params_t *)queue_peek(test_case_config->test_steps_q, i);
        if (tmp_step == NULL) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: step is null at count %d\n", __func__,
                __LINE__, i);
            return NULL;
        }

        if (tmp_step->step_number == step_number) {
            return tmp_step;
        }
    }

    return NULL;
}

void wlan_emu_ui_mgr_t::push_config_to_queue(wlan_emu_test_case_config *test)
{
    queue_push(test_cov_cases_q, test);
}

wifi_vap_info_t *wlan_emu_ui_mgr_t::get_cci_vap_info(char *vap_name)
{
    wifi_vap_info_t *vap_info = NULL;
    int radio_index = 0;
    int vap_array_index = 0;
    webconfig_cci_t *cci_webconfig = NULL;
    cci_webconfig = get_webconfig_data();
    radio_index = convert_vap_name_to_radio_array_index(&cci_webconfig->hal_cap.wifi_prop,
        vap_name);
    vap_array_index = convert_vap_name_to_array_index(&cci_webconfig->hal_cap.wifi_prop, vap_name);
    if (((int)radio_index < 0) || ((int)vap_array_index < 0)) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid index for vap_name : %s\n", __func__,
            __LINE__, vap_name);
        return NULL;
    }
    vap_info = &cci_webconfig->radios[radio_index].vaps.vap_map.vap_array[vap_array_index];
    return vap_info;
}

int wlan_emu_ui_mgr_t::get_file_name_from_url(char *str, char *file_name, int len)
{
    char *lastSlash = NULL;
    time_t current_time;
    struct tm *timeinfo;
    char file_name_substr[64] = { 0 };
    char timestamp[16] = { 0 };
    http_info_t *http_info = NULL;
    if ((str == NULL) || (file_name == NULL)) {
        cci_error_code = EFPRESENCE;
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: input arguements are NULL str : %p file_name : %p\n", __func__, __LINE__, str,
            file_name);
        return RETURN_ERR;
    }

    http_info = fill_http_info();
    if (http_info == NULL) {
        cci_error_code = ECURLGET;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d Failed to fill http_data\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    if (get_last_substring_after_slash(str, file_name_substr, sizeof(file_name_substr), cci_error_code) !=
        RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: get_last_substring_after_slash failed for str : %s\n", __func__, __LINE__, str);
        free(http_info);
        return RETURN_ERR;
    }

    free(http_info);

    snprintf(file_name, len, "%s", file_name_substr);

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_common_config(cJSON *step, test_step_params_t *step_config)
{
    cJSON *param = NULL;
    cJSON *frame_type_list = NULL;
    cJSON *frame_type = NULL;
    cJSON *frame = NULL;

    decode_param_string(step, "StepNumber", param);
    step_config->step_number = atoi(param->valuestring);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: current_test_step : %d\n", __func__, __LINE__,
        step_config->step_number);
    step_config->test_state = wlan_emu_tests_state_cmd_request;

    param = cJSON_GetObjectItem(step, "Fork");
    if (param != NULL && cJSON_IsBool(param)) {
        step_config->fork = (param->type & cJSON_True) ? true : false;
    } else {
        step_config->fork = false;
    }
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: fork value : %d\n", __func__, __LINE__,
        step_config->fork);

    memset(&step_config->frame_request, 0, sizeof(step_config->frame_request));
    param = cJSON_GetObjectItem(step, "TestCapture");

    if (param != NULL && cJSON_IsBool(param)) {
        step_config->capture_frames = (param->type & cJSON_True) ? true : false;
        if (step_config->capture_frames == true) {
            if ((frame_type_list = cJSON_GetObjectItem(step, "TestCaptureFrame")) != NULL) {
                cJSON_ArrayForEach(frame_type, frame_type_list) {
                    frame = cJSON_GetObjectItem(frame_type, "FrameType");
                    if (wlan_common_utils::decode_pcap_frame_type(frame->valuestring,
                            &step_config->frame_request) != RETURN_OK) {
                        wlan_emu_print(wlan_emu_log_level_err,
                            "%s:%d Unable to decode the pcap frametype : %s\n", __func__, __LINE__,
                            param->valuestring);
                        return RETURN_ERR;
                    }
                }
            }
        }
    }
    wlan_emu_print(wlan_emu_log_level_dbg,
        "%s:%d: msg_type : 0x%x frm80211_ops : 0x%x cfg80211_ops : 0x%x\n", __func__, __LINE__,
        step_config->frame_request.msg_type, step_config->frame_request.frm80211_ops,
        step_config->frame_request.cfg80211_ops);
    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_radio_config(cJSON *step, test_step_params_t *step_config)
{
    cJSON *param = NULL;
    step_config->param_type = step_param_type_radio;
    decode_param_string(step, "RadioConfiguration", param);
    snprintf(step_config->u.test_webconfig_json, sizeof(step_config->u.test_webconfig_json), "%s",
        param->valuestring);
    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_vap_config(cJSON *step, test_step_params_t *step_config)
{
    cJSON *param = NULL;
    step_config->param_type = step_param_type_vap;
    decode_param_string(step, "VapConfiguration", param);
    snprintf(step_config->u.test_webconfig_json, sizeof(step_config->u.test_webconfig_json), "%s",
        param->valuestring);
    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_packet_generator_config(cJSON *step,
    test_step_params_t *step_config)
{
    cJSON *param = NULL;
    cJSON *config = NULL;
    cJSON *pcap_loc = NULL;
    cJSON *pcap_file_path = NULL;

    if (step_config == NULL || step_config->u.packet_generator == NULL) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: packet_generator is NULL or step_config is null\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    step_config->param_type = step_param_type_packet_generator;
    config = cJSON_GetObjectItem(step, "PacketGenerator");
    if (config != NULL) {
        if (step_config->u.packet_generator == NULL) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: packet_generator is NULL\n", __func__,
                __LINE__);
            return RETURN_ERR;
        }

        param = cJSON_GetObjectItem(config, "VapName");
        if (param != NULL && cJSON_IsString(param)) {
            step_config->u.packet_generator->vapname = std::string(param->valuestring);
        } else {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: vap_name is NULL\n", __func__, __LINE__);
            return RETURN_ERR;
        }

        param = cJSON_GetObjectItem(config, "Interval");
        if (param != NULL && cJSON_IsNumber(param)) {
            step_config->u.packet_generator->interval = param->valueint;
        } else {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: interval is NULL\n", __func__, __LINE__);
            return RETURN_ERR;
        }

        param = cJSON_GetObjectItem(config, "Iteration");
        if (param != NULL && cJSON_IsNumber(param)) {
            step_config->u.packet_generator->iteration = param->valueint;
        } else {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: iteration is NULL\n", __func__,
                __LINE__);
            return RETURN_ERR;
        }

        param = cJSON_GetObjectItem(config, "ManagementFrameList");
        if (param != NULL && cJSON_IsArray(param)) {
            cJSON_ArrayForEach(pcap_loc, param) {
                if (step_config->u.packet_generator->pcap_queue == NULL) {
                    wlan_emu_print(wlan_emu_log_level_err, "%s:%d: pcap_queue is NULL\n", __func__,
                        __LINE__);
                    return RETURN_ERR;
                }
                decode_param_string(pcap_loc, "PcapLocation", pcap_file_path);
                if (pcap_file_path != NULL && cJSON_IsString(pcap_file_path)) {
                    packet_capture_t *pcap_file_loc;
                    pcap_file_loc = (packet_capture_t *)calloc(1, sizeof(packet_capture_t));
                    if (pcap_file_loc == NULL) {
                        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: pcap_file_loc is NULL\n",
                            __func__, __LINE__);
                        return RETURN_ERR;
                    }

                    strncpy(pcap_file_loc->pcap_location, pcap_file_path->valuestring,
                        sizeof(pcap_file_loc->pcap_location) - 1);
                    queue_push(step_config->u.packet_generator->pcap_queue, pcap_file_loc);
                } else {
                    wlan_emu_print(wlan_emu_log_level_err, "%s:%d: pcap_file_path is NULL\n",
                        __func__, __LINE__);
                    return RETURN_ERR;
                }
            }
        } else {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: ManagementFrameList is NULL\n", __func__,
                __LINE__);
            return RETURN_ERR;
        }
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: object PacketGenerator is not found\n",
            __func__, __LINE__);
        return RETURN_ERR;
    }

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_station_management_config(cJSON *step,
    test_step_params_t *step_config)
{
    cJSON *param = NULL;
    cJSON *param_loc = NULL;
    cJSON *frame_type_list = NULL;
    cJSON *frame_type = NULL;
    cJSON *frame = NULL;
    frame_capture_request_t frame_capture_req;
    station_prototype_pcaps_t *tmp_station_prototype_pcap;
    step_config->param_type = step_param_type_station_management;
    decode_param_string(step, "StationManagement", param);
    snprintf(step_config->u.sta_test->test_station_config,
        sizeof(step_config->u.sta_test->test_station_config), "%s", param->valuestring);

    param = cJSON_GetObjectItem(step, "CaptureStationRequests");
    if (param != NULL && cJSON_IsBool(param)) {
        step_config->u.sta_test->capture_sta_requests = (param->type & cJSON_True) ? true : false;
    }

    param = cJSON_GetObjectItem(step, "WaitConnection");
    if (param != NULL && cJSON_IsBool(param)) {
        step_config->u.sta_test->wait_connection = (param->type & cJSON_True) ? true : false;
    }

    param = cJSON_GetObjectItem(step, "StationPrototypes");

    if (param != NULL && cJSON_IsBool(param)) {
        step_config->u.sta_test->is_station_prototype_enabled = (param->type & cJSON_True) ? true :
                                                                                             false;
        if (step_config->u.sta_test->is_station_prototype_enabled == true) {
            if ((frame_type_list = cJSON_GetObjectItem(step, "StationPrototypeFrames")) != NULL) {
                cJSON_ArrayForEach(frame_type, frame_type_list) {
                    memset(&frame_capture_req, 0, sizeof(frame_capture_request_t));
                    frame = cJSON_GetObjectItem(frame_type, "FrameType");
                    if (wlan_common_utils::decode_pcap_frame_type(frame->valuestring,
                            &frame_capture_req) != RETURN_OK) {
                        wlan_emu_print(wlan_emu_log_level_err,
                            "%s:%d Unable to decode the pcap frametype : %s\n", __func__, __LINE__,
                            param->valuestring);
                        return RETURN_ERR;
                    }

                    // check if msg type is wlan_emu_msg_type_frm80211
                    if (frame_capture_req.msg_type & (1 << wlan_emu_msg_type_frm80211)) {
                        if (frame_capture_req.frm80211_ops &
                                (1 << wlan_emu_frm80211_ops_type_assoc_req) ||
                            (frame_capture_req.frm80211_ops &
                                (1 << wlan_emu_frm80211_ops_type_auth))) {
                            // update the fc_request_summary
                            step_config->u.sta_test->station_prototype->fc_request_summary
                                .msg_type |= (1 << wlan_emu_msg_type_frm80211);
                            step_config->u.sta_test->station_prototype->fc_request_summary
                                .frm80211_ops |= frame_capture_req.frm80211_ops;
                            // Next push to the queue

                            tmp_station_prototype_pcap = (station_prototype_pcaps_t *)calloc(1,
                                sizeof(station_prototype_pcaps_t));
                            if (tmp_station_prototype_pcap == NULL) {
                                wlan_emu_print(wlan_emu_log_level_err,
                                    "%s:%d tmp_station_prototype_pcap is null\n", __func__,
                                    __LINE__);
                                return RETURN_ERR;
                            }
                            memcpy(&tmp_station_prototype_pcap->fc_request, &frame_capture_req,
                                sizeof(frame_capture_request_t));
                            decode_param_string(frame_type, "PcapLocation", param_loc);
                            snprintf(tmp_station_prototype_pcap->file_location,
                                sizeof(tmp_station_prototype_pcap->file_location), "%s",
                                param_loc->valuestring);
                            wlan_emu_print(wlan_emu_log_level_dbg,
                                "%s:%d pcap_location : %s frm80211_ops : 0x%x\n", __func__,
                                __LINE__, tmp_station_prototype_pcap->file_location,
                                tmp_station_prototype_pcap->fc_request.frm80211_ops);
                            queue_push(step_config->u.sta_test->station_prototype->fc_prototype_q,
                                tmp_station_prototype_pcap);
                        }
                    }
                }
            }
        }
    }
    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_time_config(cJSON *step, test_step_params_t *step_config)
{
    step_config->param_type = step_param_type_time;
    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_command_config(cJSON *step, test_step_params_t *step_config)
{
    command_t *cmd;
    cJSON *param;
    step_config->param_type = step_param_type_command;

    decode_param_string(step, "Command", param);
    cmd = step_config->u.cmd;
    snprintf(cmd->test_cmd, sizeof(cmd->test_cmd), "%s", param->valuestring);

    param = cJSON_GetObjectItem(step, "CmdResultFileName");
    if (param != NULL) {
        snprintf(cmd->cmd_exec_log_filename, sizeof(cmd->cmd_exec_log_filename), "%s",
            param->valuestring);
    } else {
        memset(cmd->cmd_exec_log_filename, 0, sizeof(cmd->cmd_exec_log_filename));
    }

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_dmlsubdoc_config(cJSON *step, test_step_params_t *step_config)
{
    command_t *cmd;
    cJSON *param;
    step_config->param_type = step_param_type_dml_subdoc;
    mac_addr_str_t cmac_str;
    mac_addr_t bmac;

    cmd = step_config->u.cmd;
    snprintf(cmd->test_cmd, sizeof(cmd->test_cmd), "%s",
        "rbuscli getvalues Device.WiFi.WebConfig.Data.Init_dml");
    memset(bmac, 0, sizeof(bmac));
    memset(cmac_str, 0, sizeof(cmac_str));
    get_cm_mac_address(cmac_str);
    str_to_mac_bytes(cmac_str, bmac);
    memset(cmac_str, 0, sizeof(cmac_str));
    mac_str_without_colon(bmac, cmac_str);

    snprintf(cmd->cmd_exec_log_filename, sizeof(cmd->cmd_exec_log_filename), "%s/%s_dml.json",
        test_results_dir_path, cmac_str);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: DMCLI Log File name : %s\n", __func__, __LINE__,
        cmd->cmd_exec_log_filename);
    //    step_config->subdoc_type = webconfig_subdoc_type_dml;
    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_dml_reset_config(cJSON *step, test_step_params_t *step_config)
{
    cJSON *param = NULL;
    step_config->param_type = step_param_type_dml_reset;
    decode_param_string(step, "DmlResetConfiguration", param);
    snprintf(step_config->u.test_webconfig_json, sizeof(step_config->u.test_webconfig_json), "%s",
        param->valuestring);
    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_log_redirect(cJSON *step, test_step_params_t *step_config)
{
    log_redirect_t *log_redirect;
    cJSON *config;
    cJSON *param;
    char temp_result_file[128] = { 0 };
    char *extension;
    char text_extension_filename[128] = { 0 };
    int length = 0;

    step_config->param_type = step_param_type_log_redirection;

    log_redirect = step_config->u.log_capture;
    config = cJSON_GetObjectItem(step, "LogOperation");

    log_redirect->log_operation = log_operation_type_invalid;
    if (update_json_param_string(config, "RedirectLogs", &param) == RETURN_OK) {
        log_redirect->log_operation = log_operation_type_start;
        snprintf(log_redirect->log_redirect_filename, sizeof(log_redirect->log_redirect_filename),
            "%s", param->valuestring);
        log_redirect->redirect_fd = -1;
        if (get_last_substring_after_slash(log_redirect->log_redirect_filename, temp_result_file,
                sizeof(temp_result_file), cci_error_code) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: get_last_substring_after_slash failed for file : %s\n", __func__, __LINE__,
                log_redirect->log_redirect_filename);
            delete log_redirect;
            return RETURN_ERR;
        }
        extension = strrchr(temp_result_file, '.');

        if (extension) {
            int length = extension - temp_result_file;
            snprintf(text_extension_filename, sizeof(text_extension_filename), "%.*s.txt", length,
                temp_result_file);
        } else {
            snprintf(text_extension_filename, sizeof(text_extension_filename), "%s.txt",
                temp_result_file);
        }

        snprintf(log_redirect->log_result_file, sizeof(log_redirect->log_result_file), "%s",
            text_extension_filename);
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: log result file name is : %s\n", __func__,
            __LINE__, log_redirect->log_result_file);
    }

    if (update_json_param_string(config, "StopLogStepNumber", &param) == RETURN_OK) {
        log_redirect->log_operation = log_operation_type_stop;
        log_redirect->stop_step_number = atoi(param->valuestring);
    }

    if (log_redirect->log_operation == log_operation_type_invalid) {
        cci_error_code = ESTEP;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid log operation\n", __func__,
            __LINE__);
        delete log_redirect;
        return RETURN_ERR;
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: log operation type : %d\n", __func__, __LINE__,
        log_redirect->log_operation);

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_get_pattern_files(cJSON *step, test_step_params_t *step_config)
{
    cJSON *config;
    cJSON *param;
    char temp_result_file[128] = { 0 };

    step_config->param_type = step_param_type_get_pattern_files;

    config = cJSON_GetObjectItem(step, "GetPatternFiles");

    decode_param_string(config, "Location", param);
    snprintf(step_config->u.get_pattern_files->file_location,
        sizeof(step_config->u.get_pattern_files->file_location), "%s", param->valuestring);

    decode_param_string(config, "FilePattern", param);
    snprintf(step_config->u.get_pattern_files->file_pattern,
        sizeof(step_config->u.get_pattern_files->file_pattern), "%s", param->valuestring);

    decode_param_bool(config, "DeleteFiles", param);
    step_config->u.get_pattern_files->delete_pattern_files = (param->type & cJSON_True) ? true :
                                                                                          false;
    wlan_emu_print(wlan_emu_log_level_dbg,
        "%s:%d: File Location : %s File pattern : %s delete pattern files : %d\n", __func__,
        __LINE__, step_config->u.get_pattern_files->file_location,
        step_config->u.get_pattern_files->file_pattern,
        step_config->u.get_pattern_files->delete_pattern_files);

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::validate_iperf_options(std::string args)
{
    const char *optstring = "p:f:i:D1VJvsc:ub:t:n:k:l:P:Rw:B:M:N46S:L:ZO:F:A:T:C:dI:mhX:";
    int opt;
    int client_flag = 0;
    int server_flag = 0;
    char role = '\0';
    char *slash;
    int argc = 0;

    /* Based on iperf, which is:
     * Copyright (c) 2014-2022, The Regents of the University of California,
     * through Lawrence Berkeley National Laboratory
     * Licensed under the BSD-3 License
     */

    struct option longopts[] = {
        { "port",                  required_argument, NULL, 'p'                               },
        { "format",                required_argument, NULL, 'f'                               },
        { "interval",              required_argument, NULL, 'i'                               },
        { "daemon",                no_argument,       NULL, 'D'                               },
        { "one-off",               no_argument,       NULL, '1'                               },
        { "verbose",               no_argument,       NULL, 'V'                               },
        { "json",                  no_argument,       NULL, 'J'                               },
        { "json-stream",           no_argument,       NULL, IPERF_OPT_JSON_STREAM             },
        { "version",               no_argument,       NULL, 'v'                               },
        { "server",                no_argument,       NULL, 's'                               },
        { "client",                required_argument, NULL, 'c'                               },
        { "udp",                   no_argument,       NULL, 'u'                               },
        { "bitrate",               required_argument, NULL, 'b'                               },
        { "bandwidth",             required_argument, NULL, 'b'                               },
        { "server-bitrate-limit",  required_argument, NULL, IPERF_OPT_SERVER_BITRATE_LIMIT    },
        { "time",                  required_argument, NULL, 't'                               },
        { "bytes",                 required_argument, NULL, 'n'                               },
        { "blockcount",            required_argument, NULL, 'k'                               },
        { "length",                required_argument, NULL, 'l'                               },
        { "parallel",              required_argument, NULL, 'P'                               },
        { "reverse",               no_argument,       NULL, 'R'                               },
        { "bidir",                 no_argument,       NULL, IPERF_OPT_BIDIRECTIONAL           },
        { "window",                required_argument, NULL, 'w'                               },
        { "bind",                  required_argument, NULL, 'B'                               },
        { "bind-dev",              required_argument, NULL, IPERF_OPT_BIND_DEV                },
        { "cport",                 required_argument, NULL, IPERF_OPT_CLIENT_PORT             },
        { "set-mss",               required_argument, NULL, 'M'                               },
        { "no-delay",              no_argument,       NULL, 'N'                               },
        { "version4",              no_argument,       NULL, '4'                               },
        { "version6",              no_argument,       NULL, '6'                               },
        { "tos",                   required_argument, NULL, 'S'                               },
        { "dscp",                  required_argument, NULL, IPERF_OPT_DSCP                    },
        { "extra-data",            required_argument, NULL, IPERF_OPT_EXTRA_DATA              },
        { "flowlabel",             required_argument, NULL, 'L'                               },
        { "zerocopy",              no_argument,       NULL, 'Z'                               },
        { "omit",                  required_argument, NULL, 'O'                               },
        { "file",                  required_argument, NULL, 'F'                               },
        { "repeating-payload",     no_argument,       NULL, IPERF_OPT_REPEATING_PAYLOAD       },
        { "timestamps",            optional_argument, NULL, IPERF_OPT_TIMESTAMPS              },
        { "affinity",              required_argument, NULL, 'A'                               },
        { "title",                 required_argument, NULL, 'T'                               },
        { "congestion",            required_argument, NULL, 'C'                               },
        { "linux-congestion",      required_argument, NULL, 'C'                               },
        { "sctp",                  no_argument,       NULL, IPERF_OPT_SCTP                    },
        { "nstreams",              required_argument, NULL, IPERF_OPT_NUMSTREAMS              },
        { "xbind",                 required_argument, NULL, 'X'                               },
        { "pidfile",               required_argument, NULL, 'I'                               },
        { "logfile",               required_argument, NULL, IPERF_OPT_LOGFILE                 },
        { "forceflush",            no_argument,       NULL, IPERF_OPT_FORCEFLUSH              },
        { "get-server-output",     no_argument,       NULL, IPERF_OPT_GET_SERVER_OUTPUT       },
        { "udp-counters-64bit",    no_argument,       NULL, IPERF_OPT_UDP_COUNTERS_64BIT      },
        { "no-fq-socket-pacing",   no_argument,       NULL, IPERF_OPT_NO_FQ_SOCKET_PACING     },
        { "dont-fragment",         no_argument,       NULL, IPERF_OPT_DONT_FRAGMENT           },
        { "skip-rx-copy",          no_argument,       NULL, IPERF_OPT_SKIP_RX_COPY            },
        { "username",              required_argument, NULL, IPERF_OPT_CLIENT_USERNAME         },
        { "rsa-public-key-path",   required_argument, NULL, IPERF_OPT_CLIENT_RSA_PUBLIC_KEY   },
        { "rsa-private-key-path",  required_argument, NULL, IPERF_OPT_SERVER_RSA_PRIVATE_KEY  },
        { "authorized-users-path", required_argument, NULL, IPERF_OPT_SERVER_AUTHORIZED_USERS },
        { "time-skew-threshold",   required_argument, NULL, IPERF_OPT_SERVER_SKEW_THRESHOLD   },
        { "use-pkcs1-padding",     no_argument,       NULL, IPERF_OPT_USE_PKCS1_PADDING       },
        { "fq-rate",               required_argument, NULL, IPERF_OPT_FQ_RATE                 },
        { "pacing-timer",          required_argument, NULL, IPERF_OPT_PACING_TIMER            },
        { "connect-timeout",       required_argument, NULL, IPERF_OPT_CONNECT_TIMEOUT         },
        { "idle-timeout",          required_argument, NULL, IPERF_OPT_IDLE_TIMEOUT            },
        { "rcv-timeout",           required_argument, NULL, IPERF_OPT_RCV_TIMEOUT             },
        { "snd-timeout",           required_argument, NULL, IPERF_OPT_SND_TIMEOUT             },
        { "cntl-ka",               optional_argument, NULL, IPERF_OPT_CNTL_KA                 },
        { "mptcp",                 no_argument,       NULL, 'm'                               },
        { "debug",                 optional_argument, NULL, 'd'                               },
        { "help",                  no_argument,       NULL, 'h'                               },
        { NULL,                    0,                 NULL, 0                                 }
    };

    std::istringstream iss(args);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }

    tokens.insert(tokens.begin(), "program_name"); // dummy string

    std::vector<std::unique_ptr<char[]>> string_storage;
    std::vector<char *> argv;
    for (const auto &t : tokens) {
        auto buf = std::make_unique<char[]>(t.size() + 1);
        std::strncpy(buf.get(), t.c_str(), t.size());
        buf[t.size()] = '\0';
        argv.push_back(buf.get());
        string_storage.push_back(std::move(buf));
    }
    argv.push_back(nullptr);
    argc = static_cast<int>(argv.size() - 1);
    optind = 0;
    opterr = 0;

    while ((opt = getopt_long(argc, argv.data(), optstring, longopts, NULL)) != -1) {
        switch (opt) {
        case 'p':
            if (atoi(optarg) < 1 || atoi(optarg) > 65535) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d Invalid iperf option 'p' : %d\n",
                    __func__, __LINE__, atoi(optarg));
                return RETURN_ERR;
            }
            break;
        case 'f':
            if (!optarg) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d Invalid iperf option 'f' \n",
                    __func__, __LINE__);
                return RETURN_ERR;
            }
            if (*optarg == 'k' || *optarg == 'K' || *optarg == 'm' || *optarg == 'M' ||
                *optarg == 'g' || *optarg == 'G' || *optarg == 't' || *optarg == 'T') {
                break;
            } else {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d Invalid iperf input for 'f' \n",
                    __func__, __LINE__);
                return RETURN_ERR;
            }
            break;
        case 'i':
            if ((atof(optarg) < IPERF_MIN_INTERVAL || atof(optarg) > IPERF_MAX_INTERVAL) &&
                atof(optarg) != 0) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d Invalid interval : %f\n", __func__,
                    __LINE__, atof(optarg));
                return RETURN_ERR;
            }
            break;
        case 'D':
            server_flag = 1;
            break;
        case '1':
            server_flag = 1;
            break;
        case 'V':
            break;
        case 'J':
            break;
        case IPERF_OPT_JSON_STREAM:
            break;
        case 's':
            if (role == 'c') {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d Invalid c option, both 'c' and 's' are enabled\n", __func__, __LINE__);
                return RETURN_ERR;
            }
            role = 's';
            break;
        case 'c':
            if (role == 's') {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d Invalid s option, both 'c' and 's' are enabled\n", __func__, __LINE__);
                return RETURN_ERR;
            }
            role = 'c';
            if (optarg == NULL || strlen(optarg) == 0) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d Invalid optarg for client\n",
                    __func__, __LINE__);
                return RETURN_ERR;
            }

            if (is_valid_ip(optarg) == RETURN_OK) {
                break;
            }

            if (is_resolvable_hostname(optarg) == RETURN_OK) {
                break;
            }

            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d unable to resolve hostname or ipaddress\n", __func__, __LINE__);
            return RETURN_ERR;
            break;
        case 'u':
            client_flag = 1;
            break;
        case IPERF_OPT_SCTP:
            client_flag = 1;
            break;
        case IPERF_OPT_NUMSTREAMS:
            client_flag = 1;
            break;
        case 'b':
            slash = strchr(optarg, '/');
            if (slash) {
                *slash = '\0';
                ++slash;
                if (atoi(slash) <= 0 || atoi(slash) > IPERF_MAX_BURST) {
                    wlan_emu_print(wlan_emu_log_level_err, "%s:%d Invalid 'b' option : %d\n",
                        __func__, __LINE__, atoi(slash));
                    return RETURN_ERR;
                }
            }
            client_flag = 1;
            break;
        case IPERF_OPT_SERVER_BITRATE_LIMIT:
            slash = strchr(optarg, '/');
            if (slash) {
                *slash = '\0';
                ++slash;
                if (atof(slash) != 0 &&
                    (atof(slash) < IPERF_MIN_INTERVAL || atof(slash) > IPERF_MAX_INTERVAL)) {
                    wlan_emu_print(wlan_emu_log_level_err,
                        "%s:%d Invalid server bitrate limit : %f\n", __func__, __LINE__,
                        atof(slash));
                    return RETURN_ERR;
                }
            }
            server_flag = 1;
            break;
        case 't':
            if (atoi(optarg) > IPERF_MAX_TIME || atoi(optarg) < 0) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d Invalid 't' option : %d\n", __func__,
                    __LINE__, atoi(optarg));
                return RETURN_ERR;
            }
            client_flag = 1;
            break;
        case 'n':
            client_flag = 1;
            break;
        case 'k':
            client_flag = 1;
            break;
        case 'l':
            client_flag = 1;
            break;
        case 'P':
            if (atoi(optarg) > IPERF_MAX_STREAMS) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d Invalid 'P' option : %d\n", __func__,
                    __LINE__, atoi(optarg));
                return RETURN_ERR;
            }
            client_flag = 1;
            break;
        case 'R':
            client_flag = 1;
            break;
        case IPERF_OPT_BIDIRECTIONAL:
            client_flag = 1;
            break;
        case 'w':
            client_flag = 1;
            break;

        case 'B':
            break;
        case IPERF_OPT_CLIENT_PORT:
            if (atoi(optarg) < 1 || atoi(optarg) > 65535) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d Invalid opt client port option : %d\n", __func__, __LINE__,
                    atoi(optarg));
                return RETURN_ERR;
            }
            break;
        case 'M':
            if (atoi(optarg) > IPERF_MAX_MSS) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d Invalid 'M' option : %d\n", __func__,
                    __LINE__, atoi(optarg));
                return RETURN_ERR;
            }
            client_flag = 1;
            break;
        case 'N':
            client_flag = 1;
            break;
        case '4':
            break;
        case '6':
            break;
        case 'S':
            client_flag = 1;
            break;
        case IPERF_OPT_DSCP:
            client_flag = 1;
            break;
        case IPERF_OPT_EXTRA_DATA:
            client_flag = 1;
            break;
        case 'L': // Need to check this
            client_flag = 1;
            break;
        case 'X':
            break;
        case 'Z':
            client_flag = 1;
            break;
        case IPERF_OPT_REPEATING_PAYLOAD:
            client_flag = 1;
            break;
        case IPERF_OPT_TIMESTAMPS:
            break;
        case 'O':
            client_flag = 1;
            break;
        case 'F':
            break;
        case IPERF_OPT_IDLE_TIMEOUT:
            server_flag = 1;
            break;
        case IPERF_OPT_RCV_TIMEOUT:
            break;
        case IPERF_OPT_SND_TIMEOUT:
            break;
        case IPERF_OPT_CNTL_KA:
            break;
            break;
        case 'T':
            client_flag = 1;
            break;
        case 'C':
            client_flag = 1;
            break;
        case 'I':
            break;
        case IPERF_OPT_LOGFILE:
            break;
        case IPERF_OPT_FORCEFLUSH:
            break;
        case IPERF_OPT_GET_SERVER_OUTPUT:
            client_flag = 1;
            break;
        case IPERF_OPT_UDP_COUNTERS_64BIT:
            break;
        case IPERF_OPT_NO_FQ_SOCKET_PACING:
            client_flag = 1;
            break;
        case IPERF_OPT_FQ_RATE:
            client_flag = 1;
            break;
        case IPERF_OPT_DONT_FRAGMENT:
            client_flag = 1;
            break;
        case IPERF_OPT_CLIENT_USERNAME:
            break;
        case IPERF_OPT_CLIENT_RSA_PUBLIC_KEY:
            break;
        case IPERF_OPT_SERVER_RSA_PRIVATE_KEY:
            break;
        case IPERF_OPT_SERVER_AUTHORIZED_USERS:
            break;
        case IPERF_OPT_SERVER_SKEW_THRESHOLD:
            break;
        case IPERF_OPT_USE_PKCS1_PADDING:
            break;
        case IPERF_OPT_SKIP_RX_COPY:
            client_flag = 1;
            break;
        case IPERF_OPT_PACING_TIMER:
            client_flag = 1;
            break;
        case IPERF_OPT_CONNECT_TIMEOUT:
            client_flag = 1;
            break;
        case 'm':
            break;
        default:
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d Invalid option for iperf %c\n", __func__,
                __LINE__, opt);
            return RETURN_ERR;
        }

        if (role == 'c' && server_flag) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d Invalid option '%c' for client role",
                __func__, __LINE__, opt);
            return RETURN_ERR;
        }
        if (role == 's' && client_flag) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d Invalid option '%c' for server role",
                __func__, __LINE__, opt);
            return RETURN_ERR;
        }
    }

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_iperf_server(cJSON *step, test_step_params_t *step_config)
{
    cJSON *config;
    cJSON *param;
    iperf_server_t *iperf_server;
    step_config->param_type = step_param_type_config_iperf_server;

    iperf_server = step_config->u.iperf_server;
    config = cJSON_GetObjectItem(step, "IperfServerOptions");

    iperf_server->input_operation = iperf_operation_type_invalid;
    if (update_json_param_integer(config, "StopServerStepNumber", &param) == RETURN_OK) {
        iperf_server->input_operation = iperf_operation_type_stop;

        iperf_server->u.stop_conf.stop_step_number = param->valuedouble;
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: stop_step_number : %d\n", __func__, __LINE__,
            iperf_server->u.stop_conf.stop_step_number);

    } else if (update_json_param_integer(config, "ServerInterfaceStepNumber", &param) ==
        RETURN_OK) {
        iperf_server->input_operation = iperf_operation_type_start;

        iperf_server->u.start_conf.interface_step_number = param->valuedouble;

        decode_param_string(config, "ServerLogResultName", param);
        snprintf(iperf_server->u.start_conf.input_filename,
            sizeof(iperf_server->u.start_conf.input_filename), "%s", param->valuestring);

        decode_param_string(config, "ServerIperfOptions", param);
        snprintf(iperf_server->u.start_conf.cmd_options,
            sizeof(iperf_server->u.start_conf.cmd_options), "%s", param->valuestring);
        wlan_emu_print(wlan_emu_log_level_dbg,
            "%s:%d: interface_step_number : %d ServerLogResultName : %s cmd_options : %s\n",
            __func__, __LINE__, iperf_server->u.start_conf.interface_step_number,
            iperf_server->u.start_conf.input_filename, iperf_server->u.start_conf.cmd_options);

        if (validate_iperf_options(iperf_server->u.start_conf.cmd_options) == RETURN_ERR) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid iperf server options : %s\n",
                __func__, __LINE__, iperf_server->u.start_conf.cmd_options);
            return RETURN_ERR;
        }
    }

    if (iperf_server->input_operation == iperf_operation_type_invalid) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid server operation\n", __func__,
            __LINE__);
        delete iperf_server;
        return RETURN_ERR;
    }

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_configure_eth_client(cJSON *step,
    test_step_params_t *step_config)
{
    cJSON *param;
    decode_param_bool(step, "ConfigureEthernetClient", param);
    if (param->type & cJSON_False) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: ConfigureEthernetClient is false\n",
            __func__, __LINE__);
        return RETURN_ERR;
    }
    step_config->param_type = step_param_type_ethernet_lan_interface;

    decode_param_integer(step, "TestDuration", param);
    step_config->u.eth_lan_client->duration = param->valuedouble;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: TestDuration : %d\n", __func__, __LINE__,
        step_config->u.eth_lan_client->duration);

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_configure_upgrade_or_reboot(cJSON *step,
    test_step_params_t *step_config)
{
    if (step == NULL || step_config == NULL || step_config->u.upgrade_or_reboot == NULL) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: step or upgrade_or_reboot is NULL\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    cJSON *config = cJSON_GetObjectItem(step, "UpgradeOrReboot");
    if (config == NULL) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: UpgradeOrReboot object not found\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    step_config->param_type = step_param_type_upgrade_or_reboot;

    cJSON *build_name = cJSON_GetObjectItem(config, "build_name");
    if (cJSON_IsString(build_name) && build_name->valuestring != NULL) {
        step_config->u.upgrade_or_reboot->build_name = build_name->valuestring;
    }

    cJSON *fr_enabled = cJSON_GetObjectItem(config, "is_fr_enabled");
    if (fr_enabled != NULL && cJSON_IsBool(fr_enabled)) {
        step_config->u.upgrade_or_reboot->is_fr_enabled = cJSON_IsTrue(fr_enabled);
    }

    cJSON *param = cJSON_GetObjectItem(config, "is_logging_enabled");
    if (cJSON_IsBool(param)) {
        step_config->u.upgrade_or_reboot->is_logging_enabled = cJSON_IsTrue(param);
        if (step_config->u.upgrade_or_reboot->is_logging_enabled == true) {
            step_config->u.upgrade_or_reboot->logging_step_numbers = queue_create();
            if (step_config->u.upgrade_or_reboot->logging_step_numbers == NULL) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: Failed to create logging_step_numbers queue\n", __func__, __LINE__);
                return RETURN_ERR;
            }
        }
    } else {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: is_logging_enabled is missing or invalid\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    if (step_config->u.upgrade_or_reboot->is_logging_enabled == true) {
        cJSON *item;
        cJSON *step_list = cJSON_GetObjectItem(config, "logging_step_numbers");
        if (step_list == NULL || cJSON_IsArray(step_list) == false) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: logging_step_numbers is missing or not an array\n", __func__, __LINE__);
            return RETURN_ERR;
        }

        cJSON_ArrayForEach(item, step_list) {
            cJSON *num = cJSON_GetObjectItem(item, "step_numbers");
            if (num && cJSON_IsNumber(num)) {
                step_number_entry_t *entry = (step_number_entry_t *)calloc(1,
                    sizeof(step_number_entry_t));
                if (entry == NULL) {
                    wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Memory allocation failed\n",
                        __func__, __LINE__);
                    return RETURN_ERR;
                }
                entry->step_number = num->valueint;
                if (step_config->u.upgrade_or_reboot->logging_step_numbers == NULL) {
                    wlan_emu_print(wlan_emu_log_level_err, "%s:%d: logging_step_numbers is NULL\n",
                        __func__, __LINE__);
                    free(entry);
                    return RETURN_ERR;
                }
                queue_push(step_config->u.upgrade_or_reboot->logging_step_numbers, entry);
            } else {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid or missing step_numbers\n",
                    __func__, __LINE__);
                return RETURN_ERR;
            }
        }
    }
    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_iperf_client(cJSON *step, test_step_params_t *step_config)
{
    cJSON *config;
    cJSON *param;
    cJSON *options;
    iperf_client_t *iperf_client;
    step_config->param_type = step_param_type_config_iperf_client;

    iperf_client = step_config->u.iperf_client;
    config = cJSON_GetObjectItem(step, "IperfClientOptions");

    iperf_client->input_operation = iperf_operation_type_invalid;
    if (update_json_param_integer(config, "StopClientStepNumber", &param) == RETURN_OK) {
        iperf_client->input_operation = iperf_operation_type_stop;

        iperf_client->u.stop_conf.stop_step_number = param->valuedouble;
        wlan_emu_print(wlan_emu_log_level_info, "%s:%d: stop_step_number : %d\n", __func__, __LINE__,
            iperf_client->u.stop_conf.stop_step_number);

    } else if (update_json_param_integer(config, "ClientInterfaceStepNumber", &param) ==
        RETURN_OK) {
        iperf_client->input_operation = iperf_operation_type_start;

        iperf_client->u.start_conf.interface_step_number = param->valuedouble;

        decode_param_integer(config, "ServerStepNumber", param);
        iperf_client->u.start_conf.server_step_number = param->valuedouble;

        options = cJSON_GetObjectItem(config, "ClientOptions");

        if (options == NULL) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: ClientOptions object is NULL\n",
                __func__, __LINE__);
            return RETURN_ERR;
        }

        decode_param_string(options, "ClientLogResultName", param);
        snprintf(iperf_client->u.start_conf.input_filename,
            sizeof(iperf_client->u.start_conf.input_filename), "%s", param->valuestring);

        decode_param_string(options, "ClientIperfOptions", param);
        snprintf(iperf_client->u.start_conf.cmd_options,
            sizeof(iperf_client->u.start_conf.cmd_options), "%s", param->valuestring);
        wlan_emu_print(wlan_emu_log_level_info,
            "%s:%d: interface_step_number : %d ServerLogResultName : %s ServerStepNumber : "
            "%d cmd_options : %s\n",
            __func__, __LINE__, iperf_client->u.start_conf.interface_step_number,
            iperf_client->u.start_conf.input_filename,
            iperf_client->u.start_conf.server_step_number, iperf_client->u.start_conf.cmd_options);

        if (validate_iperf_options(iperf_client->u.start_conf.cmd_options) == RETURN_ERR) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid iperf client options : %s\n",
                __func__, __LINE__, iperf_client->u.start_conf.cmd_options);
            return RETURN_ERR;
        }
    }

    if (iperf_client->input_operation == iperf_operation_type_invalid) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid client operation\n", __func__,
            __LINE__);
        delete iperf_client;
        return RETURN_ERR;
    }

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_timed_wait(cJSON *step, test_step_params_t *step_config)
{
    cJSON *config;
    cJSON *param;
    step_config->param_type = step_param_type_timed_wait;

    decode_param_integer(step, "WaitTimeInSeconds", param);
    step_config->u.timed_wait->seconds = param->valuedouble;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: wait for seconds : %d\n", __func__, __LINE__,
        step_config->u.timed_wait->seconds);

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_config_onewifi(cJSON *step, test_step_params_t *step_config)
{
    cJSON *param;

    step_config->param_type = step_param_type_config_onewifi;
    decode_param_string(step, "OneWiFiSubdoc", param);
    snprintf(step_config->u.test_onewifi_subdoc, sizeof(step_config->u.test_onewifi_subdoc), "%s",
        param->valuestring);

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_get_file(cJSON *step, test_step_params_t *step_config)
{
    cJSON *config;
    cJSON *param;
    char temp_result_file[128] = { 0 };

    step_config->param_type = step_param_type_get_file;

    config = cJSON_GetObjectItem(step, "GetFile");

    decode_param_string(config, "SourceFilename", param);
    snprintf(step_config->u.get_file->source_file, sizeof(step_config->u.get_file->source_file),
        "%s", param->valuestring);

    decode_param_string(config, "ToDestinationFilename", param);
    snprintf(step_config->u.get_file->dest_filename, sizeof(step_config->u.get_file->dest_filename),
        "%s/%s/%s", m_path, cci_test_dir, param->valuestring);

    decode_param_bool(config, "DeleteSourceFile", param);
    step_config->u.get_file->delete_source_file = (param->type & cJSON_True) ? true : false;
    wlan_emu_print(wlan_emu_log_level_dbg,
        "%s:%d: Source File : %s Destination File : %s delete_source_file : %d\n", __func__,
        __LINE__, step_config->u.get_file->source_file, step_config->u.get_file->dest_filename,
        step_config->u.get_file->delete_source_file);

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_mgmt_frame_capture(cJSON *step, test_step_params_t *step_config)
{
    cJSON *config;
    cJSON *param;
    char temp_result_file[128] = { 0 };

    step_config->param_type = step_param_type_mgmt_frame_capture;

    decode_param_integer(step, "MgmtFrameCaptureRadioIndex", param);
    if ((param->valuedouble < 0) && (param->valuedouble >= MAX_NUM_RADIOS)) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Invalid radio index : %d\n", __func__,
            __LINE__, param->valuedouble);
        return RETURN_ERR;
    }
    step_config->u.mgmt_frame_capture->radio_index = param->valuedouble;

    decode_param_integer(step, "Duration", param);
    step_config->u.mgmt_frame_capture->duration = param->valuedouble;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: radio_index : %d duration : %d\n", __func__,
        __LINE__, step_config->u.mgmt_frame_capture->radio_index,
        step_config->u.mgmt_frame_capture->duration);

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_stats_get_common_params(cJSON *step, test_step_params_t *step_config)
{
    cJSON *config;
    int stop_step_number = 0;
    char operation[32] = { 0 };

    wifi_stats_get_t *wifi_stats_get = step_config->u.wifi_stats_get;

    config = cJSON_GetObjectItem(step, "Operation");
    if (config != NULL) {
        snprintf(operation, sizeof(operation), "%s", config->valuestring);
        if (strcmp(operation, "Start") == 0) {
            wifi_stats_get->log_operation = log_operation_type_start;
        } else if (strcmp(operation, "Stop") == 0) {
            wifi_stats_get->log_operation = log_operation_type_stop;
            wifi_stats_get->timeout = -1;
        } else {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid operation : %s\n", __func__,
                __LINE__, operation);
            return RETURN_ERR;
        }
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Operation value : %s\n", __func__, __LINE__,
            operation);
    } else {
        config = cJSON_GetObjectItem(step, "Duration");
        if (config != NULL) {
            wifi_stats_get->timeout = atoi(config->valuestring);
            wifi_stats_get->log_operation = log_operation_type_timer;
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Duration value : %d\n", __func__,
                __LINE__, wifi_stats_get->timeout);
        } else {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Duration or Operation is not present\n",
                __func__, __LINE__);
            return RETURN_ERR;
        }

        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Operation is not present\n", __func__,
            __LINE__);
    }

    if (wifi_stats_get->log_operation == log_operation_type_stop) {
        config = cJSON_GetObjectItem(step, "StopLogStepNumber");
        if (config != NULL) {
            stop_step_number = atoi(config->valuestring);
            wifi_stats_get->stop_log_step_number = stop_step_number;
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: StopLogStepNumber value : %d\n",
                __func__, __LINE__, wifi_stats_get->stop_log_step_number);
        } else {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: StopLogStepNumber is not present\n",
                __func__, __LINE__);
            return RETURN_ERR;
        }
    } else {
        decode_param_bool(step, "StatResponseType", config);
        wifi_stats_get->is_stat_response_type_set = (config->type & cJSON_True) ? true : false;
    }

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_radio_channel_stats_get(cJSON *step,
    test_step_params_t *step_config)
{
    cJSON *param;
    cJSON *config;
    int radioindex = 0;
    wifi_stats_get_t *wifi_stats_get;

    step_config->param_type = step_param_type_stats_get;
    wifi_stats_get = step_config->u.wifi_stats_get;

    config = cJSON_GetObjectItem(step, "RadioIndex");
    if (config != NULL) {
        radioindex = atoi(config->valuestring);
        if ((radioindex >= 0) && (radioindex < MAX_NUM_RADIOS)) {
            wifi_stats_get->radio_index = radioindex;
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Decode success RadioIndex value : %d\n",
                __func__, __LINE__, wifi_stats_get->radio_index);
        } else {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid RadioIndex\n", __func__,
                __LINE__);
            return RETURN_ERR;
        }
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: RadioIndex is not present\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    if (decode_stats_get_common_params(step, step_config) != RETURN_OK) {
        return RETURN_ERR;
    } else {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Decode success\n", __func__, __LINE__);
    }

    config = cJSON_GetObjectItem(step, "ScanMode");
    if (config != NULL) {
        if (strcmp(config->valuestring, "OnChannel") == 0) {
            wifi_stats_get->scan_mode = WIFI_RADIO_SCAN_MODE_ONCHAN;
        } else if (strcmp(config->valuestring, "OffChannel") == 0) {
            wifi_stats_get->scan_mode = WIFI_RADIO_SCAN_MODE_OFFCHAN;
        } else if (strcmp(config->valuestring, "Full") == 0) {
            wifi_stats_get->scan_mode = WIFI_RADIO_SCAN_MODE_FULL;
        } else if (strcmp(config->valuestring, "Survey") == 0) {
            wifi_stats_get->scan_mode = WIFI_RADIO_SCAN_MODE_SURVEY;
        } else {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid scan mode : %s\n", __func__,
                __LINE__, config->valuestring);
            return RETURN_ERR;
        }
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: ScanMode value : %s\n", __func__, __LINE__,
            config->valuestring);
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: ScanMode is not present\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    wifi_stats_get->data_type = mon_stats_type_radio_channel_stats;

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_neighbor_stats_get(cJSON *step, test_step_params_t *step_config)
{
    cJSON *param;
    cJSON *config;
    int radioindex = 0;
    wifi_stats_get_t *wifi_stats_get;

    step_config->param_type = step_param_type_stats_get;
    wifi_stats_get = step_config->u.wifi_stats_get;

    config = cJSON_GetObjectItem(step, "RadioIndex");
    if (config != NULL) {
        radioindex = atoi(config->valuestring);
        if ((radioindex >= 0) && (radioindex < MAX_NUM_RADIOS)) {
            wifi_stats_get->radio_index = radioindex;
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Decode success RadioIndex value : %d\n",
                __func__, __LINE__, wifi_stats_get->radio_index);
        } else {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid RadioIndex : %d\n", __func__,
                __LINE__, wifi_stats_get->radio_index);
            return RETURN_ERR;
        }
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: RadioIndex is not present\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    step_config->u.wifi_stats_get = wifi_stats_get;
    if (decode_stats_get_common_params(step, step_config) != RETURN_OK) {
        return RETURN_ERR;
    } else {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Decode success\n", __func__, __LINE__);
    }

    config = cJSON_GetObjectItem(step, "ScanMode");
    if (config != NULL) {
        if (strcmp(config->valuestring, "OnChannel") == 0) {
            wifi_stats_get->scan_mode = WIFI_RADIO_SCAN_MODE_ONCHAN;
        } else if (strcmp(config->valuestring, "OffChannel") == 0) {
            wifi_stats_get->scan_mode = WIFI_RADIO_SCAN_MODE_OFFCHAN;
        } else if (strcmp(config->valuestring, "Full") == 0) {
            wifi_stats_get->scan_mode = WIFI_RADIO_SCAN_MODE_FULL;
        } else if (strcmp(config->valuestring, "Survey") == 0) {
            wifi_stats_get->scan_mode = WIFI_RADIO_SCAN_MODE_SURVEY;
        } else {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid scan mode : %s\n", __func__,
                __LINE__, config->valuestring);
            return RETURN_ERR;
        }
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: ScanMode value : %s\n", __func__, __LINE__,
            config->valuestring);
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: ScanMode is not present\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    wifi_stats_get->data_type = mon_stats_type_neighbor_stats;

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_assoc_client_stats_get(cJSON *step,
    test_step_params_t *step_config)
{
    cJSON *param;
    cJSON *config;
    int vapindex = 0;
    wifi_stats_get_t *wifi_stats_get;

    step_config->param_type = step_param_type_stats_get;
    wifi_stats_get = step_config->u.wifi_stats_get;

    config = cJSON_GetObjectItem(step, "VapIndex");
    if (config != NULL) {
        vapindex = atoi(config->valuestring);
        if ((vapindex >= 0) && (vapindex < (MAX_NUM_VAP_PER_RADIO * MAX_NUM_RADIOS))) {
            wifi_stats_get->vap_index = vapindex;
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Decode success VapIndex value : %d\n",
                __func__, __LINE__, wifi_stats_get->vap_index);
        } else {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid VapIndex\n", __func__, __LINE__);
            return RETURN_ERR;
        }

    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: VapIndex is not present\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    if (decode_stats_get_common_params(step, step_config) != RETURN_OK) {
        return RETURN_ERR;
    } else {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Decode success\n", __func__, __LINE__);
    }

    wifi_stats_get->data_type = mon_stats_type_associated_device_stats;

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_radio_diag_stats_get(cJSON *step,
    test_step_params_t *step_config)
{
    cJSON *param;
    cJSON *config;
    int radioindex = 0;
    wifi_platform_property_t *wifi_prop;
    wifi_stats_get_t *wifi_stats_get;

    step_config->param_type = step_param_type_stats_get;

    wifi_stats_get = step_config->u.wifi_stats_get;

    config = cJSON_GetObjectItem(step, "RadioIndex");
    if (config != NULL) {
        radioindex = atoi(config->valuestring);
        if ((radioindex >= 0) && (radioindex < MAX_NUM_RADIOS)) {
            wifi_stats_get->radio_index = radioindex;
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Decode success RadioIndex value : %d\n",
                __func__, __LINE__, wifi_stats_get->radio_index);
        } else {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid RadioIndex\n", __func__,
                __LINE__);
            return RETURN_ERR;
        }

    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: RadioIndex is not present\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    if (decode_stats_get_common_params(step, step_config) != RETURN_OK) {
        return RETURN_ERR;
    } else {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Decode success\n", __func__, __LINE__);
    }

    wifi_stats_get->data_type = mon_stats_type_radio_diagnostic_stats;

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_radio_temperature_stats_get(cJSON *step,
    test_step_params_t *step_config)
{
    cJSON *param;
    cJSON *config;
    int radioindex = 0;
    wifi_platform_property_t *wifi_prop;
    wifi_stats_get_t *wifi_stats_get;

    step_config->param_type = step_param_type_stats_get;

    wifi_stats_get = step_config->u.wifi_stats_get;

    config = cJSON_GetObjectItem(step, "RadioIndex");
    if (config != NULL) {
        radioindex = atoi(config->valuestring);
        if ((radioindex >= 0) && (radioindex < MAX_NUM_RADIOS)) {
            wifi_stats_get->radio_index = radioindex;
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Decode success RadioIndex value : %d\n",
                __func__, __LINE__, wifi_stats_get->radio_index);
        } else {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid RadioIndex\n", __func__,
                __LINE__);
            return RETURN_ERR;
        }

    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: RadioIndex is not present\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    if (decode_stats_get_common_params(step, step_config) != RETURN_OK) {
        return RETURN_ERR;
    } else {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Decode success\n", __func__, __LINE__);
    }

    wifi_stats_get->data_type = mon_stats_type_radio_temperature;

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_stats_set_common_params(cJSON *step, test_step_params_t *step_config)
{
    cJSON *param;
    cJSON *stats_set_config;
    cJSON *stats_set_list;
    char *str = NULL;

    stats_set_list = cJSON_GetObjectItem(step, "StatsConfiguration");
    if (stats_set_list != NULL) {
        cJSON_ArrayForEach(stats_set_config, stats_set_list) {
            str = cJSON_Print(stats_set_config);
            stat_set_config_t *reference = new stat_set_config_t;
            if (reference == NULL) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: reference allocation failed\n",
                    __func__, __LINE__);
                return RETURN_ERR;
            }
            param = cJSON_GetObjectItem(stats_set_config, "StatsDuration");
            if (param != NULL) {
                reference->stats_duration = atoi(param->valuestring);
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: StatsDuration value : %d\n",
                    __func__, __LINE__, reference->stats_duration);
                step_config->u.wifi_stats_set->total_duration += reference->stats_duration;
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: StatsDuration total_duration : %d\n",
                    __func__, __LINE__, step_config->u.wifi_stats_set->total_duration);
            } else {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: StatsDuration is not present\n",
                    __func__, __LINE__);
                return RETURN_ERR;
            }
            decode_param_string(stats_set_config, "InputFile", param);
            snprintf(reference->input_file_json, sizeof(reference->input_file_json), "%s",
                param->valuestring);
            queue_push(step_config->u.wifi_stats_set->stats_set_q, reference);
        }
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: StatsConfiguration total queue count : %d\n",
            __func__, __LINE__, queue_count(step_config->u.wifi_stats_set->stats_set_q));
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: StatsConfiguration is not present\n",
            __func__, __LINE__);
        return RETURN_ERR;
    }

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_radio_channel_stats_set(cJSON *step,
    test_step_params_t *step_config)
{
    cJSON *param;
    cJSON *config;
    int radioindex = 0;
    wifi_stats_set_t *wifi_stats_set = step_config->u.wifi_stats_set;

    step_config->param_type = step_param_type_stats_set;

    config = cJSON_GetObjectItem(step, "RadioIndex");
    if (config != NULL) {
        wifi_stats_set->radio_index = atoi(config->valuestring);
        if ((wifi_stats_set->radio_index >= 0) && (wifi_stats_set->radio_index < MAX_NUM_RADIOS)) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Decode success RadioIndex value : %d\n",
                __func__, __LINE__, wifi_stats_set->radio_index);
        } else {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid RadioIndex\n", __func__,
                __LINE__);
            return RETURN_ERR;
        }
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: RadioIndex is not present\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    if (decode_stats_set_common_params(step, step_config) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed decoding stats set common params\n",
            __func__, __LINE__);
        return RETURN_ERR;
    } else {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Decode success\n", __func__, __LINE__);
    }

    wifi_stats_set->data_type = mon_stats_type_radio_channel_stats;

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_neighbor_stats_set(cJSON *step, test_step_params_t *step_config)
{
    cJSON *param;
    cJSON *config;
    wifi_stats_set_t *wifi_stats_set = step_config->u.wifi_stats_set;

    step_config->param_type = step_param_type_stats_set;

    config = cJSON_GetObjectItem(step, "RadioIndex");
    if (config != NULL) {
        wifi_stats_set->radio_index = atoi(config->valuestring);
        if ((wifi_stats_set->radio_index >= 0) && (wifi_stats_set->radio_index < MAX_NUM_RADIOS)) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Decode success RadioIndex value : %d\n",
                __func__, __LINE__, wifi_stats_set->radio_index);
        } else {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid RadioIndex : %d\n", __func__,
                __LINE__, wifi_stats_set->radio_index);
            return RETURN_ERR;
        }
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: RadioIndex is not present\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    if (decode_stats_set_common_params(step, step_config) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed decoding stats set common params\n",
            __func__, __LINE__);
        return RETURN_ERR;
    } else {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Decode success\n", __func__, __LINE__);
    }

    wifi_stats_set->data_type = mon_stats_type_neighbor_stats;

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_assoc_client_stats_set(cJSON *step,
    test_step_params_t *step_config)
{
    cJSON *param;
    cJSON *config;
    wifi_stats_set_t *wifi_stats_set = step_config->u.wifi_stats_set;

    step_config->param_type = step_param_type_stats_set;

    config = cJSON_GetObjectItem(step, "VapIndex");
    if (config != NULL) {
        wifi_stats_set->vap_index = atoi(config->valuestring);
        if ((wifi_stats_set->vap_index >= 0) &&
            (wifi_stats_set->vap_index < (MAX_NUM_VAP_PER_RADIO * MAX_NUM_RADIOS))) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Decode success VapIndex value : %d\n",
                __func__, __LINE__, wifi_stats_set->vap_index);
        } else {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid VapIndex\n", __func__, __LINE__);
            return RETURN_ERR;
        }
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: VapIndex is not present\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    if (decode_stats_set_common_params(step, step_config) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed decoding stats set common params\n",
            __func__, __LINE__);
        return RETURN_ERR;
    } else {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Decode success\n", __func__, __LINE__);
    }

    wifi_stats_set->data_type = mon_stats_type_associated_device_stats;

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_radio_diag_stats_set(cJSON *step,
    test_step_params_t *step_config)
{
    cJSON *param;
    cJSON *config;
    wifi_stats_set_t *wifi_stats_set = step_config->u.wifi_stats_set;

    step_config->param_type = step_param_type_stats_set;

    config = cJSON_GetObjectItem(step, "RadioIndex");
    if (config != NULL) {
        wifi_stats_set->radio_index = atoi(config->valuestring);
        if ((wifi_stats_set->radio_index >= 0) && (wifi_stats_set->radio_index < MAX_NUM_RADIOS)) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Decode success RadioIndex value : %d\n",
                __func__, __LINE__, wifi_stats_set->radio_index);
        } else {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid RadioIndex\n", __func__,
                __LINE__);
            return RETURN_ERR;
        }
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: RadioIndex is not present\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    if (decode_stats_set_common_params(step, step_config) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed decoding stats set common params\n",
            __func__, __LINE__);
        return RETURN_ERR;
    } else {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Decode success\n", __func__, __LINE__);
    }

    wifi_stats_set->data_type = mon_stats_type_radio_diagnostic_stats;

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_radio_temperature_stats_set(cJSON *step,
    test_step_params_t *step_config)
{
    cJSON *param;
    cJSON *config;
    wifi_stats_set_t *wifi_stats_set = step_config->u.wifi_stats_set;

    step_config->param_type = step_param_type_stats_set;

    config = cJSON_GetObjectItem(step, "RadioIndex");
    if (config != NULL) {
        wifi_stats_set->radio_index = atoi(config->valuestring);
        if ((wifi_stats_set->radio_index >= 0) && (wifi_stats_set->radio_index < MAX_NUM_RADIOS)) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Decode success RadioIndex value : %d\n",
                __func__, __LINE__, wifi_stats_set->radio_index);
        } else {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid RadioIndex\n", __func__,
                __LINE__);
            return RETURN_ERR;
        }
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: RadioIndex is not present\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    if (decode_stats_set_common_params(step, step_config) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed decoding stats set common params\n",
            __func__, __LINE__);
        return RETURN_ERR;
    } else {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Decode success\n", __func__, __LINE__);
    }

    wifi_stats_set->data_type = mon_stats_type_radio_temperature;

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_gateway_performance(cJSON *step, test_step_params_t *step_config)
{
    cJSON *param = NULL;
    cJSON *config = NULL;
    cJSON *process = NULL;
    cJSON *process_obj = NULL;
    char process_name[16] = { 0 };
    char *p_name = NULL;

    step_config->param_type = step_param_type_gateway_performance;

    config = cJSON_GetObjectItem(step, "GatewayPerformance");

    if (config == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Gateway performance object is NULL\n",
            __func__, __LINE__);
        return RETURN_ERR;
    }

    decode_param_string(config, "PerformanceResultName", param);
    step_config->u.gw_performance->result_file_name = param->valuestring;

    decode_param_integer(config, "Interval", param);
    step_config->u.gw_performance->interval = param->valuedouble;

    decode_param_integer(config, "Iteration", param);
    step_config->u.gw_performance->iteration = param->valuedouble;

    decode_param_string(config, "TopCmdOptions", param);
    if (strcmp(param->valuestring, "cpu") == 0) {
        step_config->u.gw_performance->cmd_option = cmd_option_cpu;
    } else if (strcmp(param->valuestring, "mem") == 0) {
        step_config->u.gw_performance->cmd_option = cmd_option_mem;
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid command option\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    if ((process_obj = cJSON_GetObjectItem(config, "ProcessNameList")) != NULL) {
        cJSON_ArrayForEach(process, process_obj) {
            decode_param_string(process, "ProcessName", param);
            strncpy(process_name, param->valuestring, sizeof(process_name) - 1);
            p_name = strdup(process_name);
            queue_push(step_config->u.gw_performance->process_status, p_name);
        }
    }
    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_param_config(cJSON *step, test_step_params_t **step_config)
{
    cJSON *config = NULL;
    cJSON *param = NULL;
    int rc = 0;

    config = cJSON_GetObjectItem(step, "RadioConfiguration");
    if (config != NULL) {
        *step_config = new (std::nothrow) test_step_param_radio;
        if ((*step_config)->is_step_initialized == false) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: Failed allocating memory for radio step\n", __func__, __LINE__);
            return RETURN_ERR;
        }

        if (decode_step_radio_config(step, *step_config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d decode_step_radio_config failed\n",
                __func__, __LINE__);
            return RETURN_ERR;
        }
        return RETURN_OK;
    }

    config = cJSON_GetObjectItem(step, "VapConfiguration");
    if (config != NULL) {
        *step_config = new (std::nothrow) test_step_param_vap;
        if ((*step_config)->is_step_initialized == false) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed allocating memory for vap step\n",
                __func__, __LINE__);
            return RETURN_ERR;
        }
        if (decode_step_vap_config(step, *step_config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d decode_step_vap_config failed\n",
                __func__, __LINE__);
            return RETURN_ERR;
        }
        return RETURN_OK;
    }

    config = cJSON_GetObjectItem(step, "StationManagement");
    if (config != NULL) {
        *step_config = new (std::nothrow) test_step_param_sta_management;
        if ((*step_config)->is_step_initialized == false) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: Failed allocating memory for station management step\n", __func__,
                __LINE__);
            return RETURN_ERR;
        }
        if (decode_step_station_management_config(step, *step_config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d decode_step_station_config failed\n",
                __func__, __LINE__);
            return RETURN_ERR;
        }
        return RETURN_OK;
    }

    config = cJSON_GetObjectItem(step, "Command");
    if (config != NULL) {
        *step_config = new (std::nothrow) test_step_param_command;
        if ((*step_config)->is_step_initialized == false) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: Failed allocating memory for command step\n", __func__, __LINE__);
            return RETURN_ERR;
        }
        if (decode_step_command_config(step, *step_config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d decode_step_command_config failed\n",
                __func__, __LINE__);
            return RETURN_ERR;
        }
        return RETURN_OK;
    }

    config = cJSON_GetObjectItem(step, "DmlSubDoc");
    if (config != NULL) {
        *step_config = new (std::nothrow) test_step_param_dmlsubdoc;
        if ((*step_config)->is_step_initialized == false) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed allocating memory for dml step\n",
                __func__, __LINE__);
            return RETURN_ERR;
        }
        if (decode_step_dmlsubdoc_config(step, *step_config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d decode_step_dmlsubdoc_config failed\n",
                __func__, __LINE__);
            return RETURN_ERR;
        }
        return RETURN_OK;
    }

    config = cJSON_GetObjectItem(step, "DmlResetConfiguration");
    if (config != NULL) {
        *step_config = new (std::nothrow) test_step_param_dml_reset;
        if ((*step_config)->is_step_initialized == false) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: Failed allocating memory for dml reset step\n", __func__, __LINE__);
            return RETURN_ERR;
        }
        if (decode_step_dml_reset_config(step, *step_config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d decode_step_dml_reset_config failed\n",
                __func__, __LINE__);
            return RETURN_ERR;
        }
        return RETURN_OK;
    }
    /*
      config = cJSON_GetObjectItem(step, "wait_in_seconds");
      if (config != NULL) {
      printf("Time Configuration is present\n");
     *step_config = new test_step_param_time;
     if (*step_config == NULL) {
     wlan_emu_print(wlan_emu_log_level_err,"%s:%d: Failed allocating memory for time step\n",
     __func__, __LINE__); return RETURN_ERR;
     }
     if (decode_step_time_config(step, *step_config) != RETURN_OK) {
     wlan_emu_print(wlan_emu_log_level_err, "%s:%d decode_step_time_config failed\n", __func__,
     __LINE__); return RETURN_ERR;
     }
     return RETURN_OK;
     }
     */
    config = cJSON_GetObjectItem(step, "LogOperation");
    if (config != NULL) {
        *step_config = new (std::nothrow) test_step_param_logredirect;
        if ((*step_config)->is_step_initialized == false) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: Failed allocating memory for log_redirection step\n", __func__, __LINE__);
            return RETURN_ERR;
        }
        if (decode_step_log_redirect(step, *step_config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d decode_step_log_redirect failed\n",
                __func__, __LINE__);
            return RETURN_ERR;
        }
        return RETURN_OK;
    }

    config = cJSON_GetObjectItem(step, "GetStatsType");
    if (config != NULL) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Decoding GetStatsType Params\n", __func__,
            __LINE__);
        decode_param_string(step, "GetStatsType", param);
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Decode GetStatsType : %s\n", __func__,
            __LINE__, param->valuestring);

        if (strcmp(param->valuestring, "RadioChannelStats") == 0) {
            *step_config = new (std::nothrow) test_step_param_get_radio_channel_stats;
            if ((*step_config)->is_step_initialized == false) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: Failed allocating memory for radio channel stats step\n", __func__,
                    __LINE__);
                return RETURN_ERR;
            }
            if (decode_step_radio_channel_stats_get(step, *step_config) != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d decode_step_radio_channel_stats_get failed\n", __func__, __LINE__);
                return RETURN_ERR;
            } else {
                wlan_emu_print(wlan_emu_log_level_dbg,
                    "%s:%d decode_step_radio_channel_stats_get success\n", __func__, __LINE__);
                return RETURN_OK;
            }
        }
        if (strcmp(param->valuestring, "NeighborStats") == 0) {
            *step_config = new (std::nothrow) test_step_param_get_neighbor_stats;
            if ((*step_config)->is_step_initialized == false) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: Failed allocating memory for neighbor stats get step\n", __func__,
                    __LINE__);
                return RETURN_ERR;
            }
            if (decode_step_neighbor_stats_get(step, *step_config) != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d decode_step_neighbor_stats_get failed\n", __func__, __LINE__);
                return RETURN_ERR;
            } else {
                wlan_emu_print(wlan_emu_log_level_dbg,
                    "%s:%d decode_step_neighbor_stats_get success\n", __func__, __LINE__);
                return RETURN_OK;
            }
        }
        if (strcmp(param->valuestring, "AssociatedDeviceStats") == 0) {
            *step_config = new (std::nothrow) test_step_param_get_assoc_clients_stats;
            if ((*step_config)->is_step_initialized == false) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: Failed allocating memory for assoc client stats get step\n", __func__,
                    __LINE__);
                return RETURN_ERR;
            }
            if (decode_step_assoc_client_stats_get(step, *step_config) != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d decode_step_assoc_client_stats_get failed\n", __func__, __LINE__);
                return RETURN_ERR;
            } else {
                wlan_emu_print(wlan_emu_log_level_dbg,
                    "%s:%d decode_step_assoc_client_stats_get success\n", __func__, __LINE__);
                return RETURN_OK;
            }
        }
        if (strcmp(param->valuestring, "RadioDiagnosticStats") == 0) {
            *step_config = new (std::nothrow) test_step_param_get_radio_diag_stats;
            if ((*step_config)->is_step_initialized == false) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: Failed allocating memory for radio diag stats get step\n", __func__,
                    __LINE__);
                return RETURN_ERR;
            }
            if (decode_step_radio_diag_stats_get(step, *step_config) != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d decode_step_radio_diag_stats_get failed\n", __func__, __LINE__);
                return RETURN_ERR;
            } else {
                wlan_emu_print(wlan_emu_log_level_dbg,
                    "%s:%d decode_step_radio_diag_stats_get success\n", __func__, __LINE__);
                return RETURN_OK;
            }
        }
        if (strcmp(param->valuestring, "RadioTemperatureStats") == 0) {
            *step_config = new (std::nothrow) test_step_param_get_radio_temperature_stats;
            if ((*step_config)->is_step_initialized == false) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: Failed allocating memory for radio temperature stats get step\n",
                    __func__, __LINE__);
                return RETURN_ERR;
            }
            if (decode_step_radio_temperature_stats_get(step, *step_config) != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d decode_step_radio_temperature_stats_get failed\n", __func__, __LINE__);
                return RETURN_ERR;
            } else {
                wlan_emu_print(wlan_emu_log_level_dbg,
                    "%s:%d decode_step_radio_temperature_stats_get success\n", __func__, __LINE__);
                return RETURN_OK;
            }
        }
    }
#if !defined(CONFIG_EXT_AGENT_CCI)
    config = cJSON_GetObjectItem(step, "SetStatsType");
    if (config != NULL) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Decoding SetStatsType Params\n", __func__,
            __LINE__);
        decode_param_string(step, "SetStatsType", param);
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Decode SetStatsType : %s\n", __func__,
            __LINE__, param->valuestring);

        if (strcmp(param->valuestring, "RadioChannelStats") == 0) {
            *step_config = new (std::nothrow) test_step_param_set_radio_channel_stats;
            if ((*step_config)->is_step_initialized == false) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: Failed allocating memory for radio channel stats set step\n", __func__,
                    __LINE__);
                return RETURN_ERR;
            }
            if (decode_step_radio_channel_stats_set(step, *step_config) != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d decode_stats_set_params failed for %s\n", __func__, __LINE__,
                    param->valuestring);
                return RETURN_ERR;
            } else {
                wlan_emu_print(wlan_emu_log_level_dbg,
                    "%s:%d decode_stats_set_params success for %s\n", __func__, __LINE__,
                    param->valuestring);
                return RETURN_OK;
            }
        }
        if (strcmp(param->valuestring, "NeighborStats") == 0) {
            *step_config = new (std::nothrow) test_step_param_set_neighbor_stats;
            if ((*step_config)->is_step_initialized == false) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: Failed allocating memory for neighbor stats set step\n", __func__,
                    __LINE__);
                return RETURN_ERR;
            }
            if (decode_step_neighbor_stats_set(step, *step_config) != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d decode_stats_set_params failed for %s\n", __func__, __LINE__,
                    param->valuestring);
                return RETURN_ERR;
            } else {
                wlan_emu_print(wlan_emu_log_level_dbg,
                    "%s:%d decode_stats_set_params success for %s\n", __func__, __LINE__,
                    param->valuestring);
                return RETURN_OK;
            }
        }
        if (strcmp(param->valuestring, "AssociatedDeviceStats") == 0) {
            *step_config = new (std::nothrow) test_step_param_set_assoc_clients_stats;
            if ((*step_config)->is_step_initialized == false) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: Failed allocating memory for assoc client stats set step\n", __func__,
                    __LINE__);
                return RETURN_ERR;
            }
            if (decode_step_assoc_client_stats_set(step, *step_config) != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d decode_stats_set_params failed for %s\n", __func__, __LINE__,
                    param->valuestring);
                return RETURN_ERR;
            } else {
                wlan_emu_print(wlan_emu_log_level_dbg,
                    "%s:%d decode_stats_set_params success for %s\n", __func__, __LINE__,
                    param->valuestring);
                return RETURN_OK;
            }
        }
        if (strcmp(param->valuestring, "RadioDiagnosticStats") == 0) {
            *step_config = new (std::nothrow) test_step_param_set_radio_diag_stats;
            if ((*step_config)->is_step_initialized == false) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: Failed allocating memory for radio diag stats set step\n", __func__,
                    __LINE__);
                return RETURN_ERR;
            }
            if (decode_step_radio_diag_stats_set(step, *step_config) != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d decode_stats_set_params failed for %s\n", __func__, __LINE__,
                    param->valuestring);
                return RETURN_ERR;
            } else {
                wlan_emu_print(wlan_emu_log_level_dbg,
                    "%s:%d decode_stats_set_params success for %s\n", __func__, __LINE__,
                    param->valuestring);
                return RETURN_OK;
            }
        }
        if (strcmp(param->valuestring, "RadioTemperatureStats") == 0) {
            *step_config = new (std::nothrow) test_step_param_set_radio_temperature_stats;
            if ((*step_config)->is_step_initialized == false) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: Failed allocating memory for radio temperature stats set step\n",
                    __func__, __LINE__);
                return RETURN_ERR;
            }
            if (decode_step_radio_temperature_stats_set(step, *step_config) != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d decode_stats_set_params failed for %s\n", __func__, __LINE__,
                    param->valuestring);
                return RETURN_ERR;
            } else {
                wlan_emu_print(wlan_emu_log_level_dbg,
                    "%s:%d decode_stats_set_params success for %s\n", __func__, __LINE__,
                    param->valuestring);
                return RETURN_OK;
            }
        }
    }
#endif // CONFIG_EXT_AGENT_CCI

    config = cJSON_GetObjectItem(step, "GetFile");
    if (config != NULL) {
        *step_config = new (std::nothrow) test_step_param_get_file;
        if ((*step_config)->is_step_initialized == false) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: Failed allocating memory for get file step\n", __func__, __LINE__);
            return RETURN_ERR;
        }
        if (decode_step_get_file(step, *step_config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d decode_step_get_file failed\n", __func__,
                __LINE__);
            return RETURN_ERR;
        }
        return RETURN_OK;
    }

    config = cJSON_GetObjectItem(step, "MgmtFrameCaptureRadioIndex");
    if (config != NULL) {
        *step_config = new (std::nothrow) test_step_param_mgmt_frame_capture;
        if ((*step_config)->is_step_initialized == false) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: Failed allocating memory for get file step\n", __func__, __LINE__);
            return RETURN_ERR;
        }
        if (decode_step_mgmt_frame_capture(step, *step_config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d decode_step_mgmt_frame_capture failed\n",
                __func__, __LINE__);
            return RETURN_ERR;
        }
        return RETURN_OK;
    }

    config = cJSON_GetObjectItem(step, "GetPatternFiles");
    if (config != NULL) {
        *step_config = new (std::nothrow) test_step_param_get_pattern_files;
        if ((*step_config)->is_step_initialized == false) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: Failed allocating memory for get pattern files step\n", __func__, __LINE__);
            return RETURN_ERR;
        }
        if (decode_step_get_pattern_files(step, *step_config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d decode_step_get_pattern_files failed\n",
                __func__, __LINE__);
            return RETURN_ERR;
        }
        return RETURN_OK;
    }

    config = cJSON_GetObjectItem(step, "WaitTimeInSeconds");
    if (config != NULL) {
        *step_config = new (std::nothrow) test_step_param_timed_wait;
        if ((*step_config)->is_step_initialized == false) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: Failed allocating memory for timed wait step\n", __func__, __LINE__);
            return RETURN_ERR;
        }
        if (decode_step_timed_wait(step, *step_config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d decode_step_timed_wait failed\n",
                __func__, __LINE__);
            return RETURN_ERR;
        }
        return RETURN_OK;
    }

    config = cJSON_GetObjectItem(step, "OneWiFiSubdoc");
    if (config != NULL) {
        *step_config = new (std::nothrow) test_step_param_config_onewifi;
        if ((*step_config)->is_step_initialized == false) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: Failed allocating memory for config set step\n", __func__, __LINE__);
            return RETURN_ERR;
        }
        if (decode_step_config_onewifi(step, *step_config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d decode_step_config_onewifi failed\n",
                __func__, __LINE__);
            return RETURN_ERR;
        }
        return RETURN_OK;
    }

    config = cJSON_GetObjectItem(step, "GatewayPerformance");
    if (config != NULL) {
        *step_config = new (std::nothrow) test_step_param_gateway_performance;
        if ((*step_config)->is_step_initialized == false) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: Failed allocating memory for gateway performance step\n", __func__,
                __LINE__);
            return RETURN_ERR;
        }
        if (decode_step_gateway_performance(step, *step_config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d Decode of gateway performance failed\n",
                __func__, __LINE__);
            return RETURN_ERR;
        }
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d decode gateway performance succeeded\n",
            __func__, __LINE__);
        return RETURN_OK;
    }

    config = cJSON_GetObjectItem(step, "PacketGenerator");
    if (config != NULL) {
        *step_config = new (std::nothrow) test_step_param_packet_generator;
        if ((*step_config)->is_step_initialized == false) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: Failed allocating memory for packet generator step\n", __func__, __LINE__);
            return RETURN_ERR;
        }
        if (decode_step_packet_generator_config(step, *step_config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d decode_step_packet_generator_config failed\n", __func__, __LINE__);
            return RETURN_ERR;
        }
        wlan_emu_print(wlan_emu_log_level_dbg,
            "%s:%d decode_step_packet_generator_config success\n", __func__, __LINE__);
        return RETURN_OK;
    }

    config = cJSON_GetObjectItem(step, "IperfServerOptions");
    if (config != NULL) {
        *step_config = new (std::nothrow) test_step_param_iperf_server;
        if ((*step_config)->is_step_initialized == false) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: Failed allocating memory for get iperf server step\n", __func__, __LINE__);
            return RETURN_ERR;
        }
        if (decode_step_iperf_server(step, *step_config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d decode_step_iperf_server failed\n",
                __func__, __LINE__);
            return RETURN_ERR;
        }
        return RETURN_OK;
    }

    config = cJSON_GetObjectItem(step, "IperfClientOptions");
    if (config != NULL) {
        *step_config = new (std::nothrow) test_step_param_iperf_client;
        if ((*step_config)->is_step_initialized == false) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: Failed allocating memory for get iperf client step\n", __func__, __LINE__);
            return RETURN_ERR;
        }
        if (decode_step_iperf_client(step, *step_config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d decode_step_iperf_client failed\n",
                __func__, __LINE__);
            return RETURN_ERR;
        }
        return RETURN_OK;
    }

    config = cJSON_GetObjectItem(step, "ConfigureEthernetClient");
    if (config != NULL) {
        *step_config = new (std::nothrow) test_step_param_eth_lan_client;
        if ((*step_config)->is_step_initialized == false) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: Failed allocating memory for get ethernet client step\n", __func__,
                __LINE__);
            return RETURN_ERR;
        }
        if (decode_step_configure_eth_client(step, *step_config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d decode_step_configure_eth_client failed\n", __func__, __LINE__);
            return RETURN_ERR;
        }
        return RETURN_OK;
    }

    config = cJSON_GetObjectItem(step, "UpgradeOrReboot");
    if (config != NULL) {
        *step_config = new (std::nothrow) test_step_param_upgrade_or_reboot;
        if ((*step_config)->is_step_initialized == false) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: Failed allocating memory for get ethernet client step\n", __func__,
                __LINE__);
            return RETURN_ERR;
        }
        if (decode_step_configure_upgrade_or_reboot(step, *step_config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d decode_step_configure_upgrade_or_reboot failed\n", __func__, __LINE__);
            step_config = NULL;
            return RETURN_ERR;
        }
        return RETURN_OK;
    }
    wlan_emu_print(wlan_emu_log_level_err, "%s:%d Invalid step param\n\n", __func__, __LINE__);
    return RETURN_ERR;
}

void wlan_emu_ui_mgr_t::dump_json(cJSON *json_buff, const char *func, int line)
{
    char *str = NULL;
    str = cJSON_Print(json_buff);
    wlan_emu_print(wlan_emu_log_level_err, "%s %d : cjson str : \n %s\n", func, line, str);
    cJSON_free(str);
}

int wlan_emu_ui_mgr_t::check_reboot_step_config(cJSON *step)
{
    cJSON *param = NULL;
    uint step_number = 0;

    decode_param_string(step, "StepNumber", param);
    step_number = atoi(param->valuestring);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: current_test_step : %d\n", __func__, __LINE__,
        step_number);
    if (step_number == 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: StepNumber cannot be zero\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    if (step_number <= last_executed_step_num) {
        for (int i = 0; i < paused_logging_steps_count; i++) {
            if (paused_logging_steps[i] == step_number) {
                wlan_emu_print(wlan_emu_log_level_dbg,
                    "%s:%d: Re-executing the paused logging step %d\n", __func__, __LINE__,
                    step_number);
                return RETURN_OK;
            }
        }
        return RETURN_ERR;
    }
    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_step_config(cJSON *config_entry,
    wlan_emu_test_case_config *configuration)
{
    cJSON *step = NULL;
    test_step_params_t *step_config;
    unsigned int count = 0;

    cJSON_ArrayForEach(step, config_entry) {
        if (last_executed_step_num != 0) {
            if (check_reboot_step_config(step) != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: check_reboot_step_config failed\n",
                    __func__, __LINE__);
                // Note: need to remove all the step config
                dump_json(step, __func__, __LINE__);
                continue;
            }
        }

        if (decode_step_param_config(step, &step_config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: decode_step_param_config failed\n",
                __func__, __LINE__);
            // Note: need to remove all the step config
            dump_json(step, __func__, __LINE__);
            return RETURN_ERR;
        } else {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d decode_step_param_config success\n",
                __func__, __LINE__);
        }

        if (decode_step_common_config(step, step_config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d decode_step_common_config failed\n",
                __func__, __LINE__);
            // Note: need to remove all the step config
            dump_json(step, __func__, __LINE__);
            return RETURN_ERR;
        } else {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d decode_step_common_config success\n",
                __func__, __LINE__);
        }

        step_config->step_seq_num = count;
        count++;

        wlan_emu_print(wlan_emu_log_level_dbg,
            "%s:%d: Seq Number : %d step_config->step_number : %d step param type : %d\n", __func__,
            __LINE__, step_config->step_seq_num, step_config->step_number, step_config->param_type);
        queue_push(configuration->test_steps_q, step_config);
    }
    last_executed_step_num = 0;

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_coverage_config(cJSON *config_entry,
    wlan_emu_test_coverage_t coverage_type, wlan_emu_test_type_t type,
    wlan_emu_test_case_config **config)
{
    cJSON *param = NULL;
    cJSON *sub_entry = NULL;
    cJSON *test_case = NULL;
    wlan_emu_test_case_config *configuration = NULL;
    test_step_params_t *step_config = NULL;
    *config = NULL;

    configuration = new (std::nothrow) wlan_emu_test_case_config;
    if (configuration == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: Failed allocating memory for wlan_emu_coverage_configuration_t\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    configuration->coverage_type = coverage_type;
    configuration->test_type = type;
    configuration->current_test_step = 0;

    configuration->test_steps_q = queue_create();
    if (configuration->test_steps_q == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: New failed for test_steps_q\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    if (decode_step_config(config_entry, configuration) != RETURN_OK) {
        cci_error_code = EWEBDECODE;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d decode_step_config failed\n", __func__,
            __LINE__);
        // Note: delete the configuration and test_steps

        return RETURN_ERR;
    } else {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d decode_step_config success\n", __func__,
            __LINE__);
    }

    configuration->test_state = wlan_emu_tests_state_cmd_request;

    *config = configuration;

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_coverage_1_config(cJSON *test_coverage_entry)
{
    cJSON *test = NULL, *list = NULL, *sub_list, *config_entry = NULL;
    wlan_emu_test_case_config *config = NULL;
    cJSON *conf, *param;

    conf = cJSON_GetObjectItem(test_coverage_entry, "Configuration");

    if ((test = cJSON_GetObjectItem(conf, "Radio")) != NULL) {
        if (decode_coverage_config(test, wlan_emu_test_coverage_1, wlan_emu_test_1_subtype_radio,
                &config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: decode_coverage_config failed for Radio\n", __func__, __LINE__);
            return RETURN_ERR;
        }
    } else if ((test = cJSON_GetObjectItem(conf, "NetworkServiceConfiguration")) != NULL) {
        if ((list = cJSON_GetObjectItem(test, "Private")) != NULL) {
            if (decode_coverage_config(list, wlan_emu_test_coverage_1,
                    wlan_emu_test_1_subtype_ns_private, &config) != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: decode_coverage_config failed for Private\n", __func__, __LINE__);
                return RETURN_ERR;
            }
        } else if ((list = cJSON_GetObjectItem(test, "Public")) != NULL) {
            if ((sub_list = cJSON_GetObjectItem(list, "XfinityOpen")) != NULL) {
                if (decode_coverage_config(sub_list, wlan_emu_test_coverage_1,
                        wlan_emu_test_1_subtype_ns_public_xfinity_open, &config) != RETURN_OK) {
                    wlan_emu_print(wlan_emu_log_level_err,
                        "%s:%d: decode_coverage_config failed for Xfinity\n", __func__, __LINE__);
                    return RETURN_ERR;
                }
            } else if ((sub_list = cJSON_GetObjectItem(list, "XfinitySecure")) != NULL) {
                if (decode_coverage_config(sub_list, wlan_emu_test_coverage_1,
                        wlan_emu_test_1_subtype_ns_public_xfinity_secure, &config) != RETURN_OK) {
                    wlan_emu_print(wlan_emu_log_level_err,
                        "%s:%d: decode_coverage_config failed for Xfinity\n", __func__, __LINE__);
                    return RETURN_ERR;
                }
            } else {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid Configuration for Public\n",
                    __func__, __LINE__);
                return RETURN_ERR;
            }
        } else if ((list = cJSON_GetObjectItem(test, "Managed")) != NULL) {
            if ((sub_list = cJSON_GetObjectItem(list, "Xhs")) != NULL) {
                if (decode_coverage_config(sub_list, wlan_emu_test_coverage_1,
                        wlan_emu_test_1_subtype_ns_managed_xhs, &config) != RETURN_OK) {
                    wlan_emu_print(wlan_emu_log_level_err,
                        "%s:%d: decode_coverage_config failed for xhs\n", __func__, __LINE__);
                    return RETURN_ERR;
                }
            } else if ((sub_list = cJSON_GetObjectItem(list, "LnfEnterprise")) != NULL) {
                if (decode_coverage_config(sub_list, wlan_emu_test_coverage_1,
                        wlan_emu_test_1_subtype_ns_managed_lnf_enterprise, &config) != RETURN_OK) {
                    wlan_emu_print(wlan_emu_log_level_err,
                        "%s:%d: decode_coverage_config failed for LnfEnterprise\n", __func__,
                        __LINE__);
                    return RETURN_ERR;
                }
            } else if ((sub_list = cJSON_GetObjectItem(list, "LnfSecure")) != NULL) {
                if (decode_coverage_config(sub_list, wlan_emu_test_coverage_1,
                        wlan_emu_test_1_subtype_ns_managed_lnf_secure, &config) != RETURN_OK) {
                    wlan_emu_print(wlan_emu_log_level_err,
                        "%s:%d: decode_coverage_config failed for LnfSecure\n", __func__, __LINE__);
                    return RETURN_ERR;
                }
            } else {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid Configuration for Managed\n",
                    __func__, __LINE__);
                return RETURN_ERR;
            }
        } else if ((list = cJSON_GetObjectItem(test, "Mesh")) != NULL) {
            if ((sub_list = cJSON_GetObjectItem(list, "MeshBackhaul")) != NULL) {
                if (decode_coverage_config(sub_list, wlan_emu_test_coverage_1,
                        wlan_emu_test_1_subtype_ns_managed_mesh_backhaul, &config) != RETURN_OK) {
                    wlan_emu_print(wlan_emu_log_level_err,
                        "%s:%d: decode_coverage_config failed for Mesh Backhaul\n", __func__,
                        __LINE__);
                    return RETURN_ERR;
                }
            } else if ((sub_list = cJSON_GetObjectItem(list, "MeshClient")) != NULL) {
                if (decode_coverage_config(sub_list, wlan_emu_test_coverage_1,
                        wlan_emu_test_1_subtype_ns_managed_mesh_client, &config) != RETURN_OK) {
                    wlan_emu_print(wlan_emu_log_level_err,
                        "%s:%d: decode_coverage_config failed for Mesh Client\n", __func__,
                        __LINE__);
                    return RETURN_ERR;
                }
            } else {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid Configuration for Mesh\n",
                    __func__, __LINE__);
                return RETURN_ERR;
            }
        } else {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid Network Configuration\n",
                __func__, __LINE__);
            return RETURN_ERR;
        }
    } else if ((test = cJSON_GetObjectItem(conf, "ClientConnection")) != NULL) {
        if ((list = cJSON_GetObjectItem(test, "Authentication")) != NULL) {
            if (decode_coverage_config(list, wlan_emu_test_coverage_1,
                    wlan_emu_test_1_subtype_cc_authentication, &config) != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: decode_coverage_config failed for Authentication\n", __func__,
                    __LINE__);
                return RETURN_ERR;
            }
        }
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid Configuration\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }
    memset(config->test_case_id, 0, sizeof(config->test_case_id));
    decode_param_string(test_coverage_entry, "TestCaseId", param);
    snprintf(config->test_case_id, sizeof(config->test_case_id), "%s", param->valuestring);

    push_config_to_queue(config);

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_coverage_2_config(cJSON *test_coverage_entry)
{
    cJSON *test = NULL, *list = NULL, *config_entry = NULL;
    wlan_emu_test_case_config *config = NULL;
    cJSON *conf, *param;

    conf = cJSON_GetObjectItem(test_coverage_entry, "ApplicationFeatures");

    if ((test = cJSON_GetObjectItem(conf, "ConnectionAdmission")) != NULL) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: ApplicationFeatures - ConnectionAdmission\n",
            __func__, __LINE__);
        if (decode_coverage_config(test, wlan_emu_test_coverage_2,
                wlan_emu_test_2_subtype_af_connection_admission, &config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: decode_coverage_config failed for ConnectionAdmission\n", __func__,
                __LINE__);
            return RETURN_ERR;
        } else {
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: decode_coverage_config success for ConnectionAdmission\n", __func__,
                __LINE__);
        }
    } else if ((test = cJSON_GetObjectItem(conf, "AccessControl")) != NULL) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: ApplicationFeatures - AccessControl\n",
            __func__, __LINE__);
        if (decode_coverage_config(test, wlan_emu_test_coverage_2,
                wlan_emu_test_2_subtype_af_access_control, &config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: decode_coverage_config failed for AccessControl\n", __func__, __LINE__);
            return RETURN_ERR;
        } else {
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: decode_coverage_config success for AccessControl\n", __func__, __LINE__);
        }
    } else if ((test = cJSON_GetObjectItem(conf, "StatsManager")) != NULL) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: ApplicationFeatures - StatsManager\n",
            __func__, __LINE__);
        if (decode_coverage_config(test, wlan_emu_test_coverage_2,
                wlan_emu_test_2_subtype_af_stats_manager, &config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: decode_coverage_config failed for StatsManager\n", __func__, __LINE__);
            return RETURN_ERR;
        } else {
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: decode_coverage_config success for StatsManager\n", __func__, __LINE__);
        }
    } else if ((test = cJSON_GetObjectItem(conf, "SteeringManager")) != NULL) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: ApplicationFeatures - SteeringManager\n",
            __func__, __LINE__);
        if (decode_coverage_config(test, wlan_emu_test_coverage_2,
                wlan_emu_test_2_subtype_af_steering_manager, &config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: decode_coverage_config failed for SteeringManager\n", __func__, __LINE__);
            return RETURN_ERR;
        } else {
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: decode_coverage_config success for SteeringManager\n", __func__, __LINE__);
        }
    } else if ((test = cJSON_GetObjectItem(conf, "Optimization")) != NULL) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: ApplicationFeatures - Optimization\n",
            __func__, __LINE__);
        if (decode_coverage_config(test, wlan_emu_test_coverage_2,
                wlan_emu_test_2_subtype_af_optimization, &config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: decode_coverage_config failed for Optimization\n", __func__, __LINE__);
            return RETURN_ERR;
        } else {
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: decode_coverage_config success for Optimization\n", __func__, __LINE__);
        }
    } else if ((test = cJSON_GetObjectItem(conf, "GreyListing")) != NULL) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: ApplicationFeatures - GreyListing\n",
            __func__, __LINE__);
        if (decode_coverage_config(test, wlan_emu_test_coverage_2,
                wlan_emu_test_2_subtype_af_grey_listing, &config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: decode_coverage_config failed for GreyListing\n", __func__, __LINE__);
            return RETURN_ERR;
        } else {
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: decode_coverage_config success for GreyListing\n", __func__, __LINE__);
        }
    } else if ((test = cJSON_GetObjectItem(conf, "ActivePassiveMeasurements")) != NULL) {
        wlan_emu_print(wlan_emu_log_level_dbg,
            "%s:%d: ApplicationFeatures - ActivePassiveMeasurements\n", __func__, __LINE__);
        if (decode_coverage_config(test, wlan_emu_test_coverage_2,
                wlan_emu_test_2_subtype_af_active_passive_msrmnts, &config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: decode_coverage_config failed for ActivePassiveMeasurements\n", __func__,
                __LINE__);
            return RETURN_ERR;
        } else {
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: decode_coverage_config success for ActivePassiveMeasurements\n", __func__,
                __LINE__);
        }
    } else if ((test = cJSON_GetObjectItem(conf, "Whix")) != NULL) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: ApplicationFeatures - Whix\n", __func__,
            __LINE__);
        if (decode_coverage_config(test, wlan_emu_test_coverage_2, wlan_emu_test_2_subtype_af_whix,
                &config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: decode_coverage_config failed for Whix\n", __func__, __LINE__);
            return RETURN_ERR;
        } else {
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: decode_coverage_config success for Whix\n", __func__, __LINE__);
        }
    } else if ((test = cJSON_GetObjectItem(conf, "Blaster")) != NULL) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: ApplicationFeatures - Blaster\n", __func__,
            __LINE__);
        if (decode_coverage_config(test, wlan_emu_test_coverage_2,
                wlan_emu_test_2_subtype_af_blaster, &config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: decode_coverage_config failed for Blaster\n", __func__, __LINE__);
            return RETURN_ERR;
        } else {
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: decode_coverage_config success for Blaster\n", __func__, __LINE__);
        }
    } else if ((test = cJSON_GetObjectItem(conf, "Motion")) != NULL) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: ApplicationFeatures - Motion\n", __func__,
            __LINE__);
        if (decode_coverage_config(test, wlan_emu_test_coverage_2,
                wlan_emu_test_2_subtype_af_motion, &config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: decode_coverage_config failed for Motion\n", __func__, __LINE__);
            return RETURN_ERR;
        } else {
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: decode_coverage_config success for Motion\n", __func__, __LINE__);
        }
    } else if ((test = cJSON_GetObjectItem(conf, "FingerPrinting")) != NULL) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: ApplicationFeatures - FingerPrinting\n",
            __func__, __LINE__);
        if (decode_coverage_config(test, wlan_emu_test_coverage_2,
                wlan_emu_test_2_subtype_af_finger_printing, &config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: decode_coverage_config failed for FingerPrinting\n", __func__, __LINE__);
            return RETURN_ERR;
        } else {
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: decode_coverage_config success for FingerPrinting\n", __func__, __LINE__);
        }
    } else if ((test = cJSON_GetObjectItem(conf, "TR-181")) != NULL) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: ApplicationFeatures - TR-181\n", __func__,
            __LINE__);
        if (decode_coverage_config(test, wlan_emu_test_coverage_2,
                wlan_emu_test_2_subtype_af_tr_181, &config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: decode_coverage_config failed for TR-181\n", __func__, __LINE__);
            return RETURN_ERR;
        } else {
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: decode_coverage_config success for TR-181\n", __func__, __LINE__);
        }
    } else if ((test = cJSON_GetObjectItem(conf, "Webconfig")) != NULL) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: ApplicationFeatures - Webconfig\n", __func__,
            __LINE__);
        if (decode_coverage_config(test, wlan_emu_test_coverage_2,
                wlan_emu_test_2_subtype_af_webconfig, &config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: decode_coverage_config failed for Webconfig\n", __func__, __LINE__);
            return RETURN_ERR;
        } else {
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: decode_coverage_config success for Webconfig\n", __func__, __LINE__);
        }
    } else if ((test = cJSON_GetObjectItem(conf, "Webpa")) != NULL) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: ApplicationFeatures - Webpa\n", __func__,
            __LINE__);
        if (decode_coverage_config(test, wlan_emu_test_coverage_2, wlan_emu_test_2_subtype_af_webpa,
                &config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: decode_coverage_config failed for Webpa\n", __func__, __LINE__);
            return RETURN_ERR;
        } else {
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: decode_coverage_config success for Webpa\n", __func__, __LINE__);
        }
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid ApplicationFeatures\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    memset(config->test_case_id, 0, sizeof(config->test_case_id));
    decode_param_string(test_coverage_entry, "TestCaseId", param);
    snprintf(config->test_case_id, sizeof(config->test_case_id), "%s", param->valuestring);
    push_config_to_queue(config);
    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_coverage_3_config(cJSON *test_coverage_entry)
{
    cJSON *test = NULL, *list = NULL, *config_entry = NULL;
    wlan_emu_test_case_config *config = NULL;
    cJSON *conf, *param;

    conf = cJSON_GetObjectItem(test_coverage_entry, "PerformanceMonitor");

    if ((test = cJSON_GetObjectItem(conf, "StatsGet")) != NULL) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: PerformanceMonitor - StatsGet\n", __func__,
            __LINE__);
        if (decode_coverage_config(test, wlan_emu_test_coverage_3,
                wlan_emu_test_3_subtype_pm_stats_get, &config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: decode_coverage_config failed for StatsGet\n", __func__, __LINE__);
            return RETURN_ERR;
        } else {
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: decode_coverage_config success for StatsGet\n", __func__, __LINE__);
        }
    } else if ((test = cJSON_GetObjectItem(conf, "StatsSet")) != NULL) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: PerformanceMonitor - StatsSet\n", __func__,
            __LINE__);
        if (decode_coverage_config(test, wlan_emu_test_coverage_3,
                wlan_emu_test_3_subtype_pm_stats_set, &config) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: decode_coverage_config failed for StatsSet\n", __func__, __LINE__);
            return RETURN_ERR;
        } else {
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: decode_coverage_config success for StatsSet\n", __func__, __LINE__);
        }
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid PerformanceMonitor\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    memset(config->test_case_id, 0, sizeof(config->test_case_id));
    decode_param_string(test_coverage_entry, "TestCaseId", param);
    snprintf(config->test_case_id, sizeof(config->test_case_id), "%s", param->valuestring);
    push_config_to_queue(config);
    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_json_config(char *raw_data)
{
    cJSON *param = NULL;
    cJSON *entry = NULL;
    cJSON *sub_entry = NULL;
    cJSON *test = NULL;
    cJSON *root_json = NULL;
    int ret = RETURN_OK;
    char *temp_tda_url = NULL;

    if (raw_data == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: NULL raw_data pointer\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    root_json = cJSON_Parse(raw_data);
    if (root_json == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: NULL json pointer\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    decode_param_string(root_json, "Version", param);
    copy_string(version, param->valuestring, sizeof(version) - 1);

    decode_param_string(root_json, "SubDocName", param);
    copy_string(subdoc_name, param->valuestring, sizeof(subdoc_name) - 1);

    decode_param_string(root_json, "RemoteServerAddress", param);
    if (is_local_host_enabled == false) {
        copy_string(server_address, param->valuestring, sizeof(server_address) - 1);
    } else {
        temp_tda_url = strdup(tda_url);
        char *last_slash = strrchr(temp_tda_url, '/');
        if (last_slash != NULL) {
            // Terminate the string at the last '/'
            *last_slash = '\0';
        }

        copy_string(server_address, temp_tda_url, sizeof(server_address) - 1);
        if (temp_tda_url != NULL) {
            free(temp_tda_url);
        }
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: server_address : %s\n", __func__, __LINE__,
        server_address);
    decode_param_string(root_json, "ResultsLocation", param);
    copy_string(remote_test_results_loc, param->valuestring, sizeof(remote_test_results_loc) - 1);

    entry = cJSON_GetObjectItem(root_json, "TestCoverages");
    if (cJSON_IsArray(entry) == false) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid number of TestCoverages\n", __func__,
            __LINE__);
        cJSON_Delete(root_json);
        return RETURN_ERR;
    }

    cJSON_ArrayForEach(sub_entry, entry) {
        decode_param_string(sub_entry, "TestCoverageCategory", param);
        switch (atoi(param->valuestring)) {
        case wlan_emu_test_coverage_1:
            decode_param_object(sub_entry, "Configuration", test);
            ret = decode_coverage_1_config(sub_entry);
            break;
        case wlan_emu_test_coverage_2:
            decode_param_object(sub_entry, "ApplicationFeatures", test);
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: TestCoverageCategory = ApplicationFeatures\n", __func__, __LINE__);
            ret = decode_coverage_2_config(sub_entry);
            break;
        case wlan_emu_test_coverage_3:
            decode_param_object(sub_entry, "PerformanceMonitor", test);
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: TestCoverageCategory = PerformanceMonitor\n", __func__, __LINE__);
            ret = decode_coverage_3_config(sub_entry);
            break;
        default:
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid TestCoverageCategory : %s\n",
                __func__, __LINE__, param->valuestring);
            cJSON_Delete(root_json);
            return RETURN_ERR;
            break;
        }

        if (ret == RETURN_ERR) {
            // remove the queue and free it up
            dump_json(test, __func__, __LINE__);
            cJSON_Delete(root_json);
            return RETURN_ERR;
        }
    }
    cJSON_Delete(root_json);

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::read_config_file(char *file_path, char **ret_data)
{
    FILE *fp = NULL;
    int data_len = 0;
    char *data = NULL;
    *ret_data = NULL;

    fp = fopen(file_path, "r");
    if (fp == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Could not open file %s\n", __func__,
            __LINE__, file_path);
        return RETURN_ERR;
    } else {
        fseek(fp, 0, SEEK_END);
        data_len = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (data_len > 0) {
            data = (char *)malloc(sizeof(char) * data_len);
            if (data == NULL) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: malloc failed for : %s\r\n",
                    __func__, __LINE__, file_path);
                fclose(fp);
                return RETURN_ERR;
            }
            memset(data, 0, sizeof(char) * data_len);
            if (fread(data, data_len, 1, fp) != 0) {
                wlan_emu_print(wlan_emu_log_level_dbg,
                    "%s:%d: file read success:%s data len:%d\r\n", __func__, __LINE__, file_path,
                    data_len);
                *ret_data = data;
            } else {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d file read failure:%s data len:%d\r\n",
                    __func__, __LINE__, file_path, data_len);
                free(data);
                data = NULL;
                fclose(fp);
                return RETURN_ERR;
            }
        } else {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Empty file:%s\r\n", __func__, __LINE__,
                file_path);
            fclose(fp);
            return RETURN_ERR;
        }
    }

    fclose(fp);
    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::decode_config_file()
{
    char *raw_data;
    int rc = 0;
    if (strlen(test_config_file) == 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: test_config_file is not present: %s\n",
            __func__, __LINE__, test_config_file);
        return RETURN_ERR;
    }

    rc = read_config_file(test_config_file, &raw_data);
    if (rc != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed reading from file : %s\n", __func__,
            __LINE__, test_config_file);
        return RETURN_ERR;
    }

    rc = decode_json_config(raw_data);
    if (rc < 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed config decoding\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    free(raw_data);

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::download_file(char *input_file_name, unsigned int input_file_name_len)
{
    char file_name[64] = { 0 };
    char download_test_file[128] = { 0 };
    char file_download_url[256] = { 0 };
    http_info_t *http_info = NULL;
    int ret = 0;

    if ((input_file_name == NULL) || (strlen(input_file_name) == 0)) {
        cci_error_code = EFPRESENCE;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: input config file is NULL\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    if (get_file_name_from_url(input_file_name, file_name, sizeof(file_name)) != RETURN_OK) {
        if (cci_error_code == ENONE) {
            cci_error_code = EDNLDTSTRESFILE;
        }
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: get_file_name_from_url failed\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: m_path : %s file_name : %s\n", __func__,
        __LINE__, m_path, file_name);
    memset(download_test_file, 0, sizeof(download_test_file));
    snprintf(download_test_file, sizeof(download_test_file), "%s/%s/%s", m_path, cci_test_dir,
        file_name);

    memset(file_download_url, 0, sizeof(file_download_url));
    snprintf(file_download_url, sizeof(file_download_url), "%s%s", server_address, input_file_name);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: server_address : %s input_file_name : %s\n",
        __func__, __LINE__, server_address, input_file_name);
    wlan_emu_print(wlan_emu_log_level_dbg,
        "%s:%d: file_download_url : %s download_test_file : %s\n", __func__, __LINE__,
        file_download_url, download_test_file);

    http_info = fill_http_info();
    if (http_info == NULL) {
        cci_error_code = EDNLDTSTRESFILE;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to fill http_data\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    if (https_get_file(http_info, file_download_url, download_test_file, cci_error_code) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: https_get_file failed for %s\n", __func__,
            __LINE__, file_download_url);
        free(http_info);
        return RETURN_ERR;
    }
    free(http_info);

    ret = snprintf(input_file_name, input_file_name_len, "%s", download_test_file);
    if ((ret < 0) || (ret >= input_file_name_len)) {
        cci_error_code = ESNPRINTF;
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: snprintf failed return : %d input len : %d\n", __func__, __LINE__, ret,
            input_file_name_len);
        return RETURN_ERR;
    }

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::download_step_param_config(test_step_params_t *step)
{

    if (step == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: step is NULL\n", __func__, __LINE__);
        return RETURN_ERR;
    }
    if ((step->param_type == step_param_type_radio) || (step->param_type == step_param_type_vap) ||
        (step->param_type == step_param_type_dml_reset)) {
        if ((download_file(step->u.test_webconfig_json, sizeof(step->u.test_webconfig_json))) !=
            RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to download %s\n", __func__,
                __LINE__, step->u.test_webconfig_json);
            return RETURN_ERR;
        }
    } else if (step->param_type == step_param_type_station_management) {
        if ((download_file(step->u.sta_test->test_station_config,
                sizeof(step->u.sta_test->test_station_config))) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to download %s\n", __func__,
                __LINE__, step->u.sta_test->test_station_config);
            return RETURN_ERR;
        }
        if ((step->u.sta_test->is_station_prototype_enabled == true) &&
            step->u.sta_test->station_prototype->fc_prototype_q) {
            queue_t *fc_prototype_q = step->u.sta_test->station_prototype->fc_prototype_q;
            element_t *current = fc_prototype_q->head;

            while (current != NULL) {
                station_prototype_pcaps_t *station_prototype_pcap =
                    (station_prototype_pcaps_t *)current->data;

                if (station_prototype_pcap != NULL) {
                    if ((download_file(station_prototype_pcap->file_location,
                            sizeof(station_prototype_pcap->file_location))) != RETURN_OK) {
                        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to download %s\n",
                            __func__, __LINE__, station_prototype_pcap->file_location);
                        return RETURN_ERR;
                    } else {
                        wlan_emu_print(wlan_emu_log_level_dbg,
                            "%s:%d: Download of %s is succesful\n", __func__, __LINE__,
                            station_prototype_pcap->file_location);
                    }
                }
                current = current->next;
            }
        }
    } else if (step->param_type == step_param_type_stats_set) {
        if (step->u.wifi_stats_set && step->u.wifi_stats_set->stats_set_q) {
            queue_t *stats_set_q = step->u.wifi_stats_set->stats_set_q;
            element_t *current = stats_set_q->head;

            while (current != NULL) {
                stat_set_config_t *stat_config = (stat_set_config_t *)current->data;

                if (stat_config != NULL) {
                    if ((download_file(stat_config->input_file_json,
                            sizeof(stat_config->input_file_json))) != RETURN_OK) {
                        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to download %s\n",
                            __func__, __LINE__, stat_config->input_file_json);
                        return RETURN_ERR;
                    }
                }
                current = current->next;
            }
        }
    } else if (step->param_type == step_param_type_config_onewifi) {
        if ((download_file(step->u.test_onewifi_subdoc, sizeof(step->u.test_onewifi_subdoc))) !=
            RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to download %s\n", __func__,
                __LINE__, step->u.test_onewifi_subdoc);
            return RETURN_ERR;
        }
    } else if (step->param_type == step_param_type_packet_generator) {
        queue_t *packet_gen_queue = step->u.packet_generator->pcap_queue;
        element_t *current = packet_gen_queue->head;
        while (current != NULL) {
            packet_capture_t *packet_gen_config = (packet_capture_t *)current->data;

            if (packet_gen_config != NULL) {
                if (download_file(packet_gen_config->pcap_location,
                        sizeof(packet_gen_config->pcap_location)) != RETURN_OK) {
                    wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to download %s\n",
                        __func__, __LINE__, packet_gen_config->pcap_location);
                    return RETURN_ERR;
                }
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Download of %s is succesful\n",
                    __func__, __LINE__, packet_gen_config->pcap_location);
            }
            current = current->next;
        }
    }
    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::download_step_common_config(test_step_params_t *step)
{
    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::download_test_files()
{
    unsigned int i = 0;
    wlan_emu_test_case_config *cov;
    int cov_total = 0;
    int cov_count = 0;
    int step_count = 0;
    int step_total = 0;
    test_step_params_t *step;

    cov_total = queue_count(test_cov_cases_q);
    if (cov_total == 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: coverage queue count is 0\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: cov queue count is %d\n", __func__, __LINE__,
        cov_total);
    for (cov_count = 0; cov_count < cov_total; cov_count++) {
        cov = (wlan_emu_test_case_config *)queue_peek(test_cov_cases_q, cov_count);
        step_total = queue_count(cov->test_steps_q);
        if (step_total == 0) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: step queue count is 0\n", __func__,
                __LINE__);
            return RETURN_ERR;
        }

        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step total count is %d\n", __func__,
            __LINE__, step_total);
        for (step_count = 0; step_count < step_total; step_count++) {
            step = (test_step_params_t *)queue_peek(cov->test_steps_q, step_count);

            if (download_step_param_config(step) != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: Download of step param failed for %d\n", __func__, __LINE__,
                    step->step_number);
                return RETURN_ERR;
            }
            if (download_step_common_config(step) != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: Download of step config failed for %d\n", __func__, __LINE__,
                    step->step_number);
                return RETURN_ERR;
            }
        }
    }
    return RETURN_OK;
}

#define STR_LEN 127
#define INTERFACE_LEN 15

int wlan_emu_ui_mgr_t::decode_reboot_case_json()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Entered decode_reboot_case_json\n", __func__,
        __LINE__);

    char temp_local_buf[STR_LEN + 1] = { 0 };
    char *check_local = NULL;
    FILE *fp = fopen(CCI_TEST_REBOOT_STEP_CONFIG_JSON, "r");
    if (fp == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to open file for reading\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    if (len < 0) {
        fclose(fp);
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: ftell failed\n", __func__, __LINE__);
        return RETURN_ERR;
    }
    rewind(fp);

    char *data = (char *)malloc(len + 1);
    if (data == NULL) {
        fclose(fp);
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: malloc failed\n", __func__, __LINE__);
        return RETURN_ERR;
    }
    fread(data, 1, len, fp);
    data[len] = '\0';
    fclose(fp);

    cJSON *root = cJSON_Parse(data);
    free(data);

    if (root == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: JSON parse error\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    // "config file"
    cJSON *cfg_file = cJSON_GetObjectItem(root, "config file");
    if (cJSON_IsString(cfg_file) && cfg_file->valuestring) {
        strncpy(get_tda_url(), cfg_file->valuestring, STR_LEN);
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Parsed config file: %s\n", __func__,
            __LINE__, cfg_file->valuestring);
        strncpy(temp_local_buf, tda_url, sizeof(temp_local_buf) - 1);
        check_local = strstr(temp_local_buf, "localhost");
        if (check_local) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: locahost is present in \"URL:\" in %s\n",
                __func__, __LINE__, tda_url);
            is_local_host_enabled = true;
        } else {
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: locahost is not present in \"URL:\" in %s\n", __func__, __LINE__, tda_url);
            is_local_host_enabled = false;
        }
    }

        // "result_file"
    cJSON *server_addr = cJSON_GetObjectItem(root, "server_address");
    if (cJSON_IsString(server_addr) && cfg_file->valuestring) {
        strncpy(server_address, server_addr->valuestring, STR_LEN);
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Parsed server address is : %s\n", __func__,
            __LINE__, server_addr->valuestring);
    }

    // "interface"
    cJSON *iface = cJSON_GetObjectItem(root, "interface");
    if (cJSON_IsString(iface) && iface->valuestring) {
        strncpy(get_tda_interface(), iface->valuestring, INTERFACE_LEN);
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Parsed interface: %s\n", __func__, __LINE__,
            iface->valuestring);
    }

    // "simulated_client_count"
    cJSON *sim_count = cJSON_GetObjectItem(root, "simulated_client_count");
    if (cJSON_IsNumber(sim_count)) {
        set_simulated_client_count(sim_count->valueint);
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Parsed simulated_client_count: %d\n",
            __func__, __LINE__, sim_count->valueint);
    }

    // "ResultsFileName"
    cJSON *results_file = cJSON_GetObjectItem(root, "ResultsFileName");
    if (cJSON_IsString(results_file) && results_file->valuestring) {
        strncpy(get_tda_output_file(), results_file->valuestring, STR_LEN);
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Parsed ResultsFileName: %s\n", __func__,
            __LINE__, results_file->valuestring);
    }

    // "is_logging_enabled" (if false, we skip logging_step_numbers)
    cJSON *log_enabled = cJSON_GetObjectItem(root, "is_logging_enabled");
    bool logging_on = cJSON_IsBool(log_enabled) && cJSON_IsTrue(log_enabled);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Parsed is_logging_enabled: %s\n", __func__,
        __LINE__, logging_on ? "true" : "false");

    // "logging_step_numbers"
    if (logging_on) {
        cJSON *no_paused_logging = cJSON_GetObjectItem(root, "count_logging_step_numbers");
        if (cJSON_IsNumber(no_paused_logging)) {
            if (alloc_paused_logging_steps(no_paused_logging->valueint) != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: alloc_paused_logging_steps failed\n",
                    __func__, __LINE__);
                cJSON_Delete(root);
                return RETURN_ERR;
            }
            paused_logging_steps_count = (uint)no_paused_logging->valueint;
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Parsed count_logging_step_numbers: %d\n",
                __func__, __LINE__, no_paused_logging->valueint);
        }

        cJSON *log_array = cJSON_GetObjectItem(root, "logging_step_numbers");
        if (cJSON_IsArray(log_array)) {
            int arr_size = cJSON_GetArraySize(log_array);
            uint *paused_logging_steps = get_paused_logging_steps();

            if (paused_logging_steps == NULL) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: get_paused_logging_steps failed\n",
                    __func__, __LINE__);
                cJSON_Delete(root);
                return RETURN_ERR;
            }
            for (int i = 0; i < arr_size; i++) {
                cJSON *item = cJSON_GetArrayItem(log_array, i);
                cJSON *step_num = cJSON_GetObjectItem(item, "step_number");
                if (cJSON_IsNumber(step_num)) {
                    paused_logging_steps[i] = step_num->valueint;
                    wlan_emu_print(wlan_emu_log_level_dbg,
                        "%s:%d: Parsed logging_step_numbers[%d]: %d\n", __func__, __LINE__, i,
                        step_num->valueint);
                }
            }
        }
    }

    // "last_executed_step_number"
    cJSON *last_step = cJSON_GetObjectItem(root, "last_executed_step_number");
    if (cJSON_IsNumber(last_step)) {
        last_executed_step_num = last_step->valueint;
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Parsed last_executed_step_number: %d\n",
            __func__, __LINE__, last_step->valueint);
    }

    cJSON_Delete(root);

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Decoded JSON successfully\n", __func__,
        __LINE__);

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::upload_file_to_server(char *upload_file, char *path)
{
    unsigned int count = 0;
    unsigned int i = 0;
    wlan_emu_test_case_config *test;
    char file_upload_url[256] = { 0 };
    http_info_t *http_info = NULL;

    if ((upload_file == NULL) || (strlen(upload_file) == 0)) {
        cci_error_code = EFPRESENCE;
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: invalid arguement upload_file : %p or len of upload_file == 0\n", __func__,
            __LINE__, upload_file);
        return RETURN_ERR;
    }

    if ((path == NULL) || (strlen(path) == 0)) {
        cci_error_code = EPUSHTSTRESFILE;
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: invalid arguement path : %p or len of path == 0\n", __func__, __LINE__, path);
        return RETURN_ERR;
    }

    // Download the Testconfig file from the URL
    memset(file_upload_url, 0, sizeof(file_upload_url));
    snprintf(file_upload_url, sizeof(file_upload_url), "%s%s", server_address, path);

    http_info = fill_http_info();

    if (http_info == NULL) {
        cci_error_code = EPUSHTSTRESFILE;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d Failed to fill http_data\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    if (https_post_file(http_info, file_upload_url, upload_file, cci_error_code) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: https_post_file failed for url %s upload_file : %s\n", __func__, __LINE__,
            file_upload_url, upload_file);
        free(http_info);
        return RETURN_ERR;
    }
    free(http_info);

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: succesfully uploaded file  %s at  %s\n",
        __func__, __LINE__, upload_file, file_upload_url);

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::step_upload_files(char *output_file)
{
    char *temp_res_file = NULL;
    char res_file_name[128] = { 0 };
    char *remote_test_results_loc = NULL;
    if (output_file[0] != '\0') {

        remote_test_results_loc = get_remote_test_results_loc();

        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: File: %s\n", __func__, __LINE__,
            output_file);
        if (upload_file_to_server(output_file,
                remote_test_results_loc) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to upload %s\n", __func__,
                __LINE__, output_file);
            return RETURN_ERR;
        } else {
            wlan_emu_print(wlan_emu_log_level_info, "%s:%d: uploaded %s\n", __func__, __LINE__,
                output_file);
            temp_res_file = strdup(output_file);
            if (get_last_substring_after_slash(temp_res_file, res_file_name,
                    sizeof(res_file_name), cci_error_code) != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: get_last_substring_after_slash failed for str : %s\n", __func__,
                    __LINE__, temp_res_file);
                free(temp_res_file);
                return RETURN_ERR;
            }
            fprintf(res_output_file_fp, "%s\n", res_file_name);
            free(temp_res_file);
            if ((is_local_host_enabled == false) && (remove(output_file) != 0)) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to remove the file : %s\n",
                    __func__, __LINE__, output_file);
            }
        }
    }

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::process_input_request()
{
    // check if the URL is valid or not
    char *url_str = NULL;
    const char *search_str = "URL:";
    char file_name[64] = { 0 };
    char temp_local_buf[128] = { 0 };
    char *check_local = NULL;
    http_info_t *http_info = NULL;

    // check if the tda_url is localhost or not
    strncpy(temp_local_buf, tda_url, sizeof(temp_local_buf) - 1);
    check_local = strstr(temp_local_buf, "localhost");
    if (check_local) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: locahost is present in \"URL:\" in %s\n",
            __func__, __LINE__, tda_url);
        is_local_host_enabled = true;
        // update the tda_url to localhost directory, so that the next operations will use the local
        // directory to update
        memset(tda_url, 0, sizeof(tda_url));
        snprintf(tda_url, sizeof(tda_url), "%s", check_local + 9);
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: New tda_url for localhost is %s\n", __func__,
            __LINE__, tda_url);
    } else {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: locahost is not present in \"URL:\" in %s\n",
            __func__, __LINE__, tda_url);
        is_local_host_enabled = false;
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: fetch url: %s\n", __func__, __LINE__, tda_url);

    if (get_file_name_from_url(tda_url, file_name, sizeof(file_name)) != RETURN_OK) {
        if (cci_error_code == ENONE) {
            cci_error_code = EFREAD;
        }
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: get_file_name_from_url failed\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    // Download the Testconfig file from the URL
    snprintf(test_config_file, sizeof(test_config_file), "%s/%s/%s", m_path, cci_test_dir,
        file_name);

    http_info = fill_http_info();
    if (http_info == NULL) {
        cci_error_code = ECURLGET;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to fill http_data\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    if (https_get_file(http_info, tda_url, test_config_file, cci_error_code) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: https_get_file failed for %s\n", __func__,
            __LINE__, tda_url);
        free(http_info);
        return RETURN_ERR;
    }
    free(http_info);

    wlan_emu_print(wlan_emu_log_level_dbg,
        "\n %s:%d: ==> Download Test Config file: %s succesful\n", __func__, __LINE__,
        test_config_file);

    /*
      if (response_code != 200) {
      wlan_emu_print(wlan_emu_log_level_err, "%s:%d: response_code : %ld delete the file : %s\n",
      __func__, __LINE__, response_code, test_config_file); if (remove(test_config_file) == 0) {
      wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Succesfully removed the file : %s\n", __func__,
      __LINE__, test_config_file); } else { wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to
      remove the file : %s\n", __func__, __LINE__, test_config_file);
      }
      return RETURN_ERR;
      }
      */

    if (decode_config_file() != RETURN_OK) {
        if (cci_error_code == ENONE) {
            cci_error_code = EWEBDECODE;
        }
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: decode_config_file failed for %s\n",
            __func__, __LINE__, test_config_file);
        return RETURN_ERR;
    }

    wlan_emu_print(wlan_emu_log_level_dbg,
        "\n %s:%d: ==> Decode of Test Config file: %s succesful\n", __func__, __LINE__,
        test_config_file);

    if (download_test_files() != RETURN_OK) {
        if (cci_error_code == ENONE) {
            cci_error_code = EDNLDTSTRESFILE;
        }
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: download_test_files failed for %s\n",
            __func__, __LINE__, test_config_file);
        return RETURN_ERR;
    }
    wlan_emu_print(wlan_emu_log_level_dbg, "\n %s:%d: ==> Download of Testfiles for %s succesful\n",
        __func__, __LINE__, test_config_file);

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::analyze_request()
{
    // start downloading the test parameters
    if (process_input_request() == RETURN_ERR) {
        return RETURN_ERR;
    }
    if (res_output_file_fp == NULL) {
        snprintf(res_output_file, sizeof(res_output_file), "%s%s", m_path, cci_out_file_list);
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: res_output_file : %s\n", __func__, __LINE__,
            res_output_file);
        res_output_file_fp = fopen(res_output_file, "a");
        if (res_output_file_fp == NULL) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: fopen failed for %s\n", __func__,
                __LINE__, res_output_file);
            return RETURN_ERR;
        }
    }

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::upload_cci_log(char *test_case_id, char *test_case_name, FILE *output_file)
{
    char cci_log_copy[128] = { 0 };
    char timestamp[24] = { 0 };
    char res_file_name[128] = { 0 };
    long src_log_file_offset = 0;

    if ((test_case_id == NULL) || (strlen(test_case_id) == 0)) {
        cci_error_code = EPUSHTSTRESFILE;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: test_case_id is NULL\n", __func__, __LINE__);
        return RETURN_ERR;
    }
    if ((test_case_name == NULL) || (strlen(test_case_name) == 0)) {
        cci_error_code = EPUSHTSTRESFILE;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: test_case_name is NULL\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }
    if (get_current_time_string(timestamp, sizeof(timestamp)) != RETURN_OK) {
        cci_error_code = ETIMESTAMP;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: get_current_time_string failed\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }
    snprintf(cci_log_copy, sizeof(cci_log_copy), "/tmp/%s_%s_%s_CCILOG.txt", test_case_id,
        timestamp, test_case_name);

    src_log_file_offset = get_log_file_offset();

    if (copy_file(CCI_LOG_FILE, src_log_file_offset, cci_log_copy) != RETURN_OK) {
        cci_error_code = EFCOPY;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to copy from %s to %s\n", __func__,
            __LINE__, CCI_LOG_FILE, cci_log_copy);
        return RETURN_ERR;
    }

    if (upload_file_to_server(cci_log_copy, remote_test_results_loc) != RETURN_OK) {
        cci_error_code = EPUSHTSTRESFILE;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to upload %s\n", __func__, __LINE__,
            cci_log_copy);
        return RETURN_ERR;
    } else {
        wlan_emu_print(wlan_emu_log_level_info, "%s:%d: uploaded %s\n", __func__, __LINE__,
            cci_log_copy);
        if (get_last_substring_after_slash(cci_log_copy, res_file_name, sizeof(res_file_name), cci_error_code) !=
            RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: get_last_substring_after_slash failed for str : %s\n", __func__, __LINE__,
                cci_log_copy);
            return RETURN_ERR;
        }
        fprintf(output_file, "%s\n", res_file_name);
    }

    if (remove(cci_log_copy) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to remove %s\n", __func__, __LINE__,
            cci_log_copy);
    }
    return RETURN_OK;
}

unsigned int wlan_emu_ui_mgr_t::upload_results()
{
    unsigned int count = 0;
    struct dirent *entry;
    wlan_emu_test_case_config *test;
    int ret = RETURN_OK;

    // start uploading test results
    // Go through the queue
    // Check which are test done
    // update the test results
    count = queue_count(test_cov_cases_q);
    if (count == 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: queue count is 0\n", __func__, __LINE__);
        if (res_output_file_fp != NULL) {
            fclose(res_output_file_fp);
            res_output_file_fp = NULL;
        }
        return RETURN_ERR;
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d:====== TestCoverage count : %d ========= \n",
        __func__, __LINE__, count);
    test = (wlan_emu_test_case_config *)queue_pop(test_cov_cases_q);
    if (test == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Test cases are not present for :%s\n",
            __func__, __LINE__, res_output_file);
        return RETURN_ERR;
    }
    while (test != NULL) {
        // Call function to upload cci file
        if (upload_cci_log(test->test_case_id, test->test_case_name, res_output_file_fp) == RETURN_ERR) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to upload cci log for %s_%s\n",
                __func__, __LINE__, test->test_case_id, test->test_case_name);
        }
        delete test;
        test = (wlan_emu_test_case_config *)queue_pop(test_cov_cases_q);
    }

    if (res_output_file_fp != NULL) {
        fclose(res_output_file_fp);
        res_output_file_fp = NULL;
    }
    if (ret == RETURN_OK) {
        ret = upload_file_to_server(res_output_file, remote_test_results_loc);
        if (ret != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to upload %s\n", __func__,
                __LINE__, res_output_file);
        } else {
            wlan_emu_print(wlan_emu_log_level_info, "%s:%d: uploaded filelist %s\n", __func__,
                __LINE__, res_output_file);
        }
    }

    return ret;
}

void wlan_emu_ui_mgr_t::signal_test_done(wlan_emu_test_coverage_t coverage)
{
    switch (coverage) {
    case wlan_emu_test_coverage_1:
        send_signal(wlan_emu_sig_type_coverage_1);
        break;
    case wlan_emu_test_coverage_2:
        send_signal(wlan_emu_sig_type_coverage_2);
        break;
    case wlan_emu_test_coverage_3:
        send_signal(wlan_emu_sig_type_coverage_3);
        break;
    case wlan_emu_test_coverage_4:
        send_signal(wlan_emu_sig_type_coverage_4);
        break;
    case wlan_emu_test_coverage_5:
        send_signal(wlan_emu_sig_type_coverage_5);
        break;
    default:
        break;
    }
}

wlan_emu_sig_type_t wlan_emu_ui_mgr_t::io_wait()
{
    int ret;
    unsigned int i;
    unsigned char buff[256] = { 0 };
    wlan_emu_sig_type_t type = wlan_emu_sig_type_max;
    ssize_t bytes_read = 0;

    io_prep();

    if (get_reboot_test_executed() == true) {
        set_reboot_test_executed(false);
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: reboot_test_executed set to false\n",
            __func__, __LINE__);
        signal_input();
    }
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Waiting ... max desc: %d\n", __func__, __LINE__,
        m_nfds + 1);
    if ((ret = select(m_nfds + 1, &m_set, NULL, NULL, NULL)) > 0) {
        for (i = 0; i < wlan_emu_sig_type_max; i++) {
            if (FD_ISSET(m_pollables[i].fd, &m_set)) {
                wlan_emu_print(wlan_emu_log_level_info, "%s:%d: Descriptor: %s signalled\n",
                    __func__, __LINE__, m_pollables[i].name);
                type = (wlan_emu_sig_type_t)i;
                bytes_read = read(m_pollables[i].fd, (unsigned char *)buff, sizeof(buff));
                break;
            }
        }
    }

    return type;
}

void *wlan_emu_ui_mgr_t::heartbeat_function(void *arg)
{
    wlan_emu_ui_mgr_t *ui_mgr = (wlan_emu_ui_mgr_t *)arg;
    bool exit = false;
    int rc;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d:Initialized heartbeat\n", __func__, __LINE__);
    pthread_mutex_lock(&ui_mgr->m_heartbeat_lock);
    while (exit == false) {
        rc = pthread_cond_wait(&ui_mgr->m_heartbeat_cond, &ui_mgr->m_heartbeat_lock);
        if (rc == 0) {
            ui_mgr->cci_report_heartbeat_to_tda();
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Sent Heartbeat\n", __func__, __LINE__);
        }
    }
    pthread_mutex_unlock(&ui_mgr->m_heartbeat_lock);

    return NULL;
}

int wlan_emu_ui_mgr_t::init()
{
    unsigned int i;
    char timestamp[24] = { 0 };

    memset(input_test_buff, 0, sizeof(input_test_buff));
    log_file_offset = 0;

    strncpy(m_path, "/tmp", sizeof(m_path));
    snprintf(interface, sizeof(interface), "%s", "erouter0");
    set_reboot_url_file_name("/nvram/cci_reboot_url_file.txt");

    if (get_current_time_string(timestamp, sizeof(timestamp)) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: get_current_time_string failed\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    snprintf(test_results_dir_path, sizeof(test_results_dir_path), "%s/%s", m_path, cci_test_dir);
    if (mkdir(test_results_dir_path, 0777) == 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Test results directory at %s\n", __func__,
            __LINE__, test_results_dir_path);
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: creation of directory failed : %s - %s\n",
            __func__, __LINE__, test_results_dir_path, strerror(errno));
    }

    sprintf(m_pollables[wlan_emu_sig_type_input].name, "%s/input", m_path);
    sprintf(m_pollables[wlan_emu_sig_type_analysis].name, "%s/analysis", m_path);
    sprintf(m_pollables[wlan_emu_sig_type_coverage_1].name, "%s/coverage1", m_path);
    sprintf(m_pollables[wlan_emu_sig_type_coverage_2].name, "%s/coverage2", m_path);
    sprintf(m_pollables[wlan_emu_sig_type_coverage_3].name, "%s/coverage3", m_path);
    sprintf(m_pollables[wlan_emu_sig_type_coverage_4].name, "%s/coverage4", m_path);
    sprintf(m_pollables[wlan_emu_sig_type_coverage_5].name, "%s/coverage5", m_path);
    sprintf(m_pollables[wlan_emu_sig_type_results].name, "%s/results", m_path);
    sprintf(m_pollables[wlan_emu_sig_type_fail].name, "%s/fail", m_path);

    for (i = 0; i < wlan_emu_sig_type_max; i++) {
        unlink(m_pollables[i].name);
        m_pollables[i].fd = 0;
    }
    memset(&cci_webconfig, 0, sizeof(webconfig_cci_t));
    update_webconfig_data(&cci_webconfig);

    if (bus_init() != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: bus_init failed\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    if (m_ssl_config.get_mtls_configuration() != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: get_mtls_configuration failed\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    is_local_host_enabled = false;

    if (pthread_create(&m_heartbeat_tid, NULL, wlan_emu_ui_mgr_t::heartbeat_function, this) != 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Thread create error for heartbeat\n",
            __func__, __LINE__);
        return -1;
    }

    pthread_setname_np(m_heartbeat_tid, "heartbeat");
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Thread created with Name : heartbeat\n",
        __func__, __LINE__);

    if (pthread_detach(m_heartbeat_tid)) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: thread detach failed for hearbeat\n",
            __func__, __LINE__);
        return RETURN_ERR;
    }

    cci_report_reboot_to_tda();
    return 0;
}

int wlan_emu_ui_mgr_t::io_prep()
{
    unsigned int i;
    struct sockaddr_un sock;

    m_nfds = 0;
    FD_ZERO(&m_set);

    for (i = 0; i < wlan_emu_sig_type_max; i++) {
        memset(&sock, 0, sizeof(struct sockaddr_un));
        sock.sun_family = AF_UNIX;
        strncpy(sock.sun_path, (char *)m_pollables[i].name, sizeof(sock.sun_path));
        if (m_pollables[i].fd != 0) {
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: Adding already open pollable: %s, desc: %d to descriptor\n", __func__,
                __LINE__, m_pollables[i].name, m_pollables[i].fd);
            FD_SET(m_pollables[i].fd, &m_set);
            if (m_nfds < m_pollables[i].fd) {
                m_nfds = m_pollables[i].fd;
            }
        } else if ((m_pollables[i].fd = socket(AF_UNIX, SOCK_DGRAM, 0)) != -1) {
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: Adding pollable: %s, desc: %d to descriptor\n", __func__, __LINE__,
                m_pollables[i].name, m_pollables[i].fd);
            bind(m_pollables[i].fd, (struct sockaddr *)&sock, sizeof(struct sockaddr_un));
            FD_SET(m_pollables[i].fd, &m_set);
            if (m_nfds < m_pollables[i].fd) {
                m_nfds = m_pollables[i].fd;
            }
        } else {
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: Failed to open pollable: %s, error: %d\n", __func__, __LINE__,
                m_pollables[i].name, errno);
            assert(0);
        }
    }

    return 0;
}

int wlan_emu_ui_mgr_t::cci_report_resume_to_tda()
{
    cJSON *json;
    char value[64] = { 0 };
    char timestamp[24] = { 0 };
    char *str = NULL;

    if (is_local_host_enabled == false) {
        json = cJSON_CreateObject();

        get_cm_mac_address(value);
        value[strcspn(value, "\n")] = '\0';

        cJSON_AddStringToObject(json, "cm_mac", value);
        cJSON_AddStringToObject(json, "result_file", cci_out_file_list);
        cJSON_AddStringToObject(json, "status", "resume");
        // NOTE : Need to read the different error codes
        // use get_cci_error_code() to get the error code
        // use set_cci_error_code for setting error code
        cJSON_AddStringToObject(json, "error_code", "0");

        if (get_current_time_string(timestamp, sizeof(timestamp)) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: get_current_time_string failed\n",
                __func__, __LINE__);
            return RETURN_ERR;
        }

        cJSON_AddStringToObject(json, "timestamp", timestamp);
        str = cJSON_Print(json);

        if (cci_post_result_to_tda(tc_endpoint_type_resume, str) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: cci_post_result_to_tda failed\n",
                __func__, __LINE__);
            cJSON_free(str);
            cJSON_Delete(json);
            return RETURN_ERR;
        }
        cJSON_free(str);
        cJSON_Delete(json);
    }

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::cci_report_factory_reset_to_tda()
{
    unsigned int count = 0, i = 0;
    wlan_emu_test_case_config *test = NULL;
    unsigned int steps_count = 0;
    cJSON *json = NULL;
    char value[64] = { 0 };
    char timestamp[24] = { 0 };
    char *str = NULL;

    json = cJSON_CreateObject();
    if (json == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: cJSON_CreateObject failed\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    if (get_cm_mac_address(value) != RETURN_OK) {
        snprintf(value, sizeof(value), "NA");
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: get_cm_mac_address failed\n", __func__,
            __LINE__);
    } else {
        value[strcspn(value, "\n")] = '\0';
        cJSON_AddStringToObject(json, "cm_mac", value);
    }

    if (access(CCI_TEST_REBOOT_STEP_CONFIG_JSON, R_OK) == 0) {
        cJSON_AddStringToObject(json, "result_file", cci_out_file_list);
    } else {
        cJSON_AddStringToObject(json, "result_file", "NA");
    }
    cJSON_AddStringToObject(json, "status", "factory_reset");
    cJSON_AddStringToObject(json, "error_code", "0");

    if (get_current_time_string(timestamp, sizeof(timestamp)) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: get_current_time_string failed\n", __func__,
            __LINE__);
        cJSON_Delete(json);
        return RETURN_ERR;
    }

    cJSON_AddStringToObject(json, "timestamp", timestamp);
    str = cJSON_Print(json);
    if (str == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: cJSON_Print failed\n", __func__, __LINE__);
        cJSON_Delete(json);
        return RETURN_ERR;
    }

    if (cci_post_result_to_tda(tc_endpoint_type_factory_reset, str) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: cci_post_result_to_tda failed\n", __func__,
            __LINE__);
        cJSON_free(str);
        cJSON_Delete(json);
        return RETURN_ERR;
    }
    cJSON_free(str);
    cJSON_Delete(json);

    if (test_cov_cases_q == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: test_cov_cases_q is NULL\n", __func__,
            __LINE__);
        return RETURN_OK;
    }

    // Free the queue elements here
    count = queue_count(test_cov_cases_q);
    if (count == 0) {
        wlan_emu_print(wlan_emu_log_level_info, "%s:%d: queue count is 0\n", __func__, __LINE__);
        return RETURN_OK;
    }

    for (i = 0; i < count; i++) {
        test = (wlan_emu_test_case_config *)queue_pop(test_cov_cases_q);
        if (test == NULL) {
            wlan_emu_print(wlan_emu_log_level_info, "%s:%d: Test is NULL at %d of %d\n", __func__,
                __LINE__, i, count);
            continue;
        }

        while (test != NULL) {
            if (test->test_steps_q == NULL) {
                break;
            }
            steps_count = queue_count(test->test_steps_q);
            if (steps_count == 0) {
                wlan_emu_print(wlan_emu_log_level_info,
                    "%s:%d: No test steps are present for %s \n", __func__, __LINE__,
                    test->test_json);
                break;
            } else {
                test_step_params_t *step = (test_step_params_t *)queue_pop(test->test_steps_q);
                while (step != NULL) {
                    step->step_remove();
                    step = (test_step_params_t *)queue_pop(test->test_steps_q);
                }
                wlan_emu_print(wlan_emu_log_level_info, "%s:%d: No test steps are present\n",
                    __func__, __LINE__);
                free(test->test_steps_q);
                test->test_steps_q = NULL;
            }
        }

        delete test;
    }
    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::cci_report_pause_to_tda()
{
    unsigned int count = 0, i = 0;
    wlan_emu_test_case_config *test;
    unsigned int steps_count = 0;
    cJSON *json;
    char value[64] = { 0 };
    char timestamp[24] = { 0 };
    char *str;

    if (is_local_host_enabled == false) {
        json = cJSON_CreateObject();

        get_cm_mac_address(value);
        value[strcspn(value, "\n")] = '\0';

        cJSON_AddStringToObject(json, "cm_mac", value);
        if (access(CCI_TEST_REBOOT_STEP_CONFIG_JSON, R_OK) == 0) {
            cJSON_AddStringToObject(json, "result_file", cci_out_file_list);
        } else {
            cJSON_AddStringToObject(json, "result_file", "NA");
        }
        cJSON_AddStringToObject(json, "status", "pause");
        // NOTE : Need to read the different error codes
        // use get_cci_error_code() to get the error code
        // use set_cci_error_code for setting error code
        cJSON_AddStringToObject(json, "error_code", "0");

        if (get_current_time_string(timestamp, sizeof(timestamp)) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: get_current_time_string failed\n",
                __func__, __LINE__);
            return RETURN_ERR;
        }

        cJSON_AddStringToObject(json, "timestamp", timestamp);
        str = cJSON_Print(json);

        if (cci_post_result_to_tda(tc_endpoint_type_pause, str) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: cci_post_result_to_tda failed\n",
                __func__, __LINE__);
            cJSON_free(str);
            cJSON_Delete(json);
            return RETURN_ERR;
        }
        cJSON_free(str);
        cJSON_Delete(json);
    }

    if (test_cov_cases_q == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: test_cov_cases_q is NULL\n", __func__,
            __LINE__);
        return RETURN_OK;
    }

    // Free the queue elements here
    count = queue_count(test_cov_cases_q);
    if (count == 0) {
        wlan_emu_print(wlan_emu_log_level_info, "%s:%d: queue count is 0\n", __func__, __LINE__);
        return RETURN_OK;
    }

    for (i = 0; i < count; i++) {
        //    test = (wlan_emu_test_case_config *)queue_remove(test_cov_cases_q, i);
        test = (wlan_emu_test_case_config *)queue_pop(test_cov_cases_q);
        if (test == NULL) {
            wlan_emu_print(wlan_emu_log_level_info, "%s:%d: Test is NULL at %d of %d\n", __func__,
                __LINE__, i, count);
            // return RETURN_ERR;
            continue;
        }

        while (test != NULL) {
            if (test->test_steps_q == NULL) {
                break;
            }
            steps_count = queue_count(test->test_steps_q);
            if (steps_count == 0) {
                wlan_emu_print(wlan_emu_log_level_info,
                    "%s:%d: No test steps are present for %s \n", __func__, __LINE__,
                    test->test_json);
                break;
            } else {
                test_step_params_t *step = (test_step_params_t *)queue_pop(test->test_steps_q);
                while (step != NULL) {
                    step->step_remove();
                    step = (test_step_params_t *)queue_pop(test->test_steps_q);
                }
                wlan_emu_print(wlan_emu_log_level_info, "%s:%d: No test steps are present\n",
                    __func__, __LINE__);
                free(test->test_steps_q);
                test->test_steps_q = NULL;
            }
        }

        delete test;
    }

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::cci_report_failure_to_tda()
{
    unsigned int count = 0, i = 0;
    wlan_emu_test_case_config *test;
    unsigned int steps_count = 0;
    cJSON *json;
    char value[64] = { 0 };
    char timestamp[24] = { 0 };
    char *str;
    char error_code_str[12];

    if (is_local_host_enabled == false) {
        cJSON *json;
        char value[64] = { 0 };
        char timestamp[24] = { 0 };
        char *str;
        json = cJSON_CreateObject();

        get_cm_mac_address(value);
        value[strcspn(value, "\n")] = '\0';

        cJSON_AddStringToObject(json, "cm_mac", value);
        cJSON_AddStringToObject(json, "result_file", cci_out_file_list);
        cJSON_AddStringToObject(json, "status", "fail");
        // NOTE : Need to read the different error codes
        // use get_cci_error_code() to get the error code
        // use set_cci_error_code for setting error code
        sprintf(error_code_str, "%d", cci_error_code);
        cJSON_AddStringToObject(json, "error_code", error_code_str);

        if (get_current_time_string(timestamp, sizeof(timestamp)) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: get_current_time_string failed\n",
                __func__, __LINE__);
            return RETURN_ERR;
        }

        cJSON_AddStringToObject(json, "timestamp", timestamp);
        str = cJSON_Print(json);

        if (cci_post_result_to_tda(tc_endpoint_type_fail, str) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: cci_post_result_to_tda failed\n",
                __func__, __LINE__);
            cJSON_free(str);
            cJSON_Delete(json);
            return RETURN_ERR;
        }
        cJSON_free(str);
        cJSON_Delete(json);
    }

    if (test_cov_cases_q == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: test_cov_cases_q is NULL\n", __func__,
            __LINE__);
        return RETURN_OK;
    }

    // Free the queue elements here
    count = queue_count(test_cov_cases_q);
    if (count == 0) {
        wlan_emu_print(wlan_emu_log_level_info, "%s:%d: queue count is 0\n", __func__, __LINE__);
        return RETURN_OK;
    }

    for (i = 0; i < count; i++) {
        //    test = (wlan_emu_test_case_config *)queue_remove(test_cov_cases_q, i);
        test = (wlan_emu_test_case_config *)queue_pop(test_cov_cases_q);
        if (test == NULL) {
            wlan_emu_print(wlan_emu_log_level_info, "%s:%d: Test is NULL at %d of %d\n", __func__,
                __LINE__, i, count);
            // return RETURN_ERR;
            continue;
        }

        while (test != NULL) {
            if (test->test_steps_q == NULL) {
                break;
            }
            steps_count = queue_count(test->test_steps_q);
            if (steps_count == 0) {
                wlan_emu_print(wlan_emu_log_level_info,
                    "%s:%d: No test steps are present for %s \n", __func__, __LINE__,
                    test->test_json);
                break;
            } else {
                test_step_params_t *step = (test_step_params_t *)queue_pop(test->test_steps_q);
                while (step != NULL) {
                    step->step_remove();
                    step = (test_step_params_t *)queue_pop(test->test_steps_q);
                }
                wlan_emu_print(wlan_emu_log_level_info, "%s:%d: No test steps are present\n",
                    __func__, __LINE__);
                free(test->test_steps_q);
                test->test_steps_q = NULL;
            }
        }

        delete test;
    }

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::cci_report_complete_to_tda()
{
    cJSON *json;
    char value[64] = { 0 };
    char timestamp[24] = { 0 };
    char *str;

    if (is_local_host_enabled == false) {
        json = cJSON_CreateObject();

        get_cm_mac_address(value);
        value[strcspn(value, "\n")] = '\0';

        cJSON_AddStringToObject(json, "cm_mac", value);
        cJSON_AddStringToObject(json, "result_file", cci_out_file_list);
        cJSON_AddStringToObject(json, "status", "complete");
        cJSON_AddStringToObject(json, "error_code", "0");

        if (get_current_time_string(timestamp, sizeof(timestamp)) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: get_current_time_string failed\n",
                __func__, __LINE__);
            return RETURN_ERR;
        }

        cJSON_AddStringToObject(json, "timestamp", timestamp);
        str = cJSON_Print(json);

        if (cci_post_result_to_tda(tc_endpoint_type_complete, str) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: cci_post_result_to_tda failed\n",
                __func__, __LINE__);
            cJSON_free(str);
            cJSON_Delete(json);
            return RETURN_ERR;
        }
        cJSON_free(str);
        cJSON_Delete(json);
    }

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::cci_report_heartbeat_to_tda()
{
    cJSON *json;
    char value[64] = { 0 };
    char timestamp[24] = { 0 };
    char *str;

    if (is_local_host_enabled == false) {
        json = cJSON_CreateObject();

        get_cm_mac_address(value);
        value[strcspn(value, "\n")] = '\0';

        cJSON_AddStringToObject(json, "cm_mac", value);
        cJSON_AddStringToObject(json, "result_file", "NA");
        cJSON_AddStringToObject(json, "status", "heartbeat");
        cJSON_AddStringToObject(json, "error_code", "0");

        if (get_current_time_string(timestamp, sizeof(timestamp)) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: get_current_time_string failed\n",
                __func__, __LINE__);
            return RETURN_ERR;
        }

        cJSON_AddStringToObject(json, "timestamp", timestamp);
        str = cJSON_Print(json);

        if (cci_post_result_to_tda(tc_endpoint_type_heartbeat, str) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: cci_post_result_to_tda failed\n",
                __func__, __LINE__);
            cJSON_free(str);
            cJSON_Delete(json);
            return RETURN_ERR;
        }
        cJSON_free(str);
        cJSON_Delete(json);
    }

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::update_tda_url_to_file()
{
    size_t host_start;
    size_t path_start;
    std::string url = get_tda_url();
    std::string url_substring;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: url is : %s\n", __func__, __LINE__, url.c_str());
    size_t scheme_end = url.find("://");
    if (scheme_end == std::string::npos) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: unable to find the url\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    host_start = scheme_end + 3; // skip "://"
    path_start = url.find('/', host_start);
    std::string filename = get_reboot_url_file_name();

    if (path_start != std::string::npos) {
        url_substring = url.substr(0, path_start + 1); // include the slash
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: unable to find the url\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    std::ofstream out_file(reboot_url_file_name);

    if (!out_file.is_open()) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: file open failed\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    out_file << url_substring;

    if (out_file.fail()) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: url write update failed\n", __func__,
            __LINE__);
        out_file.close();
        return RETURN_ERR;
    }

    out_file.close();
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: reboot url update succesful\n", __func__,
        __LINE__);
    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::cci_report_reboot_to_tda()
{
    cJSON *json;
    char value[64] = { 0 };
    char timestamp[24] = { 0 };
    char *str;

    json = cJSON_CreateObject();

    get_cm_mac_address(value);
    value[strcspn(value, "\n")] = '\0';

    cJSON_AddStringToObject(json, "cm_mac", value);
    cJSON_AddStringToObject(json, "result_file", "NA");
    cJSON_AddStringToObject(json, "status", "running");
    cJSON_AddStringToObject(json, "error_code", "1");

    if (get_current_time_string(timestamp, sizeof(timestamp)) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: get_current_time_string failed\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    cJSON_AddStringToObject(json, "timestamp", timestamp);
    str = cJSON_Print(json);

    if (cci_post_result_to_tda(tc_endpoint_type_reboot, str) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: cci_post_result_to_tda failed\n", __func__,
            __LINE__);
        cJSON_free(str);
        cJSON_Delete(json);
        return RETURN_ERR;
    }
    cJSON_free(str);
    cJSON_Delete(json);

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::cci_post_result_to_tda(unsigned int endpoint_type, char *str)
{
    char result_url[128] = { 0 };
    char post_res_file[] = "/tmp/cci_post_file.json";
    FILE *fp = NULL;
    http_info_t *http_info = NULL;
    std::string reboot_url;

    if (str == NULL) {
        cci_error_code = ENULL;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: str is NULL\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: str is %s\n", __func__, __LINE__, str);

    if (endpoint_type == tc_endpoint_type_heartbeat) {
        snprintf(result_url, sizeof(result_url), "%s/heartbeat", server_address);
    } else if (endpoint_type == tc_endpoint_type_complete) {
        snprintf(result_url, sizeof(result_url), "%s/complete", server_address);
    } else if (endpoint_type == tc_endpoint_type_fail) {
        snprintf(result_url, sizeof(result_url), "%s/fail", server_address);
    } else if (endpoint_type == tc_endpoint_type_factory_reset) {
        snprintf(result_url, sizeof(result_url), "%s/factory_reset", server_address);
    } else if (endpoint_type == tc_endpoint_type_pause) {
        snprintf(result_url, sizeof(result_url), "%s/pause", server_address);
    } else if (endpoint_type == tc_endpoint_type_resume) {
        snprintf(result_url, sizeof(result_url), "%s/resume", server_address);
    } else if (endpoint_type == tc_endpoint_type_reboot) {
        std::ifstream source_file(reboot_url_file_name);
        if (source_file) {
            if (std::getline(source_file, reboot_url)) {
                snprintf(result_url, sizeof(result_url), "%sreboot", reboot_url.c_str());
            } else {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: unable to read line of %s\n",
                    __func__, __LINE__, post_res_file);
                source_file.close();
                return RETURN_OK;
            }
            source_file.close();
        }
    } else {
        cci_error_code = ECURLPOST;
        return RETURN_ERR;
    }

    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: url is : %s\n", __func__, __LINE__, result_url);

    fp = fopen(post_res_file, "wb");
    if (fp == NULL) {
        cci_error_code = EFOPEN;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: fopen failed for %s\n", __func__, __LINE__,
            post_res_file);
        return RETURN_ERR;
    }

    fputs(str, fp);
    fclose(fp);

    http_info = fill_http_info();

    if (http_info == NULL) {
        cci_error_code = ECURLPOST;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d Failed to fill http_data\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    if (https_post_file(http_info, result_url, post_res_file, cci_error_code) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: https_post_file failed for url %s upload_file : %s\n", __func__, __LINE__,
            result_url, post_res_file);
        free(http_info);
        return RETURN_ERR;
    }
    free(http_info);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Result post Done\n", __func__, __LINE__);
    return RETURN_OK;
}

void wlan_emu_ui_mgr_t::send_signal(wlan_emu_sig_type_t sig)
{

    struct sockaddr_un sock;
    memset(&sock, 0, sizeof(struct sockaddr_un));
    sock.sun_family = AF_UNIX;
    strncpy(sock.sun_path, (char *)m_pollables[sig].name, sizeof(sock.sun_path));

    ssize_t bytes_sent = sendto(m_pollables[sig].fd, (unsigned char *)&wlan_emu_ui_mgr_t::m_token,
        sizeof(wlan_emu_ui_mgr_t::m_token), 0, (struct sockaddr *)&sock, sizeof(sock));
}

int wlan_emu_ui_mgr_t::bus_send(char *data, wlan_emu_bus_t *m_bus_mgr)
{
#ifndef LINUX_VM
    int rc = bus_error_success;
    bus_handle_t handle = get_bus_handle();

    rc = m_bus_mgr->desc.bus_set_string_fn(&handle, WIFI_WEBCONFIG_DOC_DATA_SOUTH, data);
    if (rc != bus_error_success) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: bus_setStr failed %d\n", __func__, __LINE__,
            rc);
        return RETURN_ERR;
    }
#endif

    return RETURN_OK;
}

webconfig_error_t wlan_emu_ui_mgr_t::cci_webconfig_data_free(webconfig_subdoc_data_t *data)
{
    if (data != NULL) {
        if (data->u.encoded.raw != NULL) {
            free(data->u.encoded.raw);
            data->u.encoded.raw = NULL;
        }
    }

    return webconfig_error_none;
}

hash_map_t **wlan_emu_ui_mgr_t::get_cci_acl_hash_map(unsigned int radio_index,
    unsigned int vap_index)
{
    webconfig_cci_t *webconfig = this->get_webconfig_data();
    if (webconfig == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s %d NULL Pointer\n", __func__, __LINE__);
        return NULL;
    }

    return &(webconfig->radios[radio_index].vaps.rdk_vap_array[vap_index].acl_map);
}

void wlan_emu_ui_mgr_t::mac_filter_cci_vap_cache_update(int radio_index, int vap_array_index)
{
    // webconfig decode allocate mem for the hash map which is getting cleared and destroyed here
    hash_map_t **acl_dev_map = get_cci_acl_hash_map(radio_index, vap_array_index);
    if (*acl_dev_map) {
        acl_entry_t *temp_acl_entry, *acl_entry;
        mac_addr_str_t mac_str;
        acl_entry = static_cast<acl_entry_t *>(hash_map_get_first(*acl_dev_map));
        while (acl_entry != NULL) {
            to_mac_str(acl_entry->mac, mac_str);
            acl_entry = static_cast<acl_entry_t *>(hash_map_get_next(*acl_dev_map, acl_entry));
            temp_acl_entry = static_cast<acl_entry_t *>(hash_map_remove(*acl_dev_map, mac_str));
            if (temp_acl_entry != NULL) {
                free(temp_acl_entry);
            }
        }
        hash_map_destroy(*acl_dev_map);
    }
}

void wlan_emu_ui_mgr_t::send_webconfig_ctrl_msg(webconfig_subdoc_type_t subdoc_type)
{
    int fd;
    ssize_t sz;

    wlan_emu_msg_data_t m_msg;

    memset(&m_msg, 0, sizeof(wlan_emu_msg_data_t));

    m_msg.type = wlan_emu_msg_type_webconfig;
    m_msg.u.ow_webconfig.subdoc_type = subdoc_type;

    if ((fd = open("/dev/rdkfmac_dev", O_RDWR)) < 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to open char dev, err: %d\n",
            __func__, __LINE__, errno);
        return;
    }

    if ((sz = write(fd, &m_msg, sizeof(wlan_emu_msg_data_t))) < 0) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Failed to write to char dev, err: %d\n",
            __func__, __LINE__, errno);
    }

    close(fd);
    return;
}

void wlan_emu_ui_mgr_t::update_cci_subdoc_vap_data(webconfig_subdoc_data_t *data)
{
    webconfig_subdoc_decoded_data_t *params;
    unsigned int i, j;
    wifi_vap_info_map_t *map;
    wifi_vap_info_t *vap;
    wifi_vap_info_map_t *cci_vap_map;
    wifi_vap_info_t *cci_vap;
    webconfig_cci_t *webconfig_cci = this->get_webconfig_data();

    params = &data->u.decoded;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d subdoc parse and update dml global cache:%d\n",
        __func__, __LINE__, data->type);
    for (i = 0; i < params->num_radios; i++) {
        map = &params->radios[i].vaps.vap_map;
        cci_vap_map = &get_webconfig_data()->radios[i].vaps.vap_map;
        for (j = 0; j < map->num_vaps; j++) {
            vap = &map->vap_array[j];
            cci_vap = &cci_vap_map->vap_array[j];

            switch (data->type) {
            case webconfig_subdoc_type_private:
                if (is_vap_private(&params->hal_cap.wifi_prop, vap->vap_index) &&
                    (strlen(vap->vap_name))) {
                    memcpy(cci_vap, vap, sizeof(wifi_vap_info_t));
                }
                break;
            case webconfig_subdoc_type_home:
                if (is_vap_xhs(&params->hal_cap.wifi_prop, vap->vap_index)) {
                    memcpy(cci_vap, vap, sizeof(wifi_vap_info_t));
                }
                break;
            case webconfig_subdoc_type_xfinity:
                if (is_vap_hotspot(&params->hal_cap.wifi_prop, vap->vap_index)) {
                    memcpy(cci_vap, vap, sizeof(wifi_vap_info_t));
                }
                break;
            case webconfig_subdoc_type_lnf:
                if (is_vap_lnf(&params->hal_cap.wifi_prop, vap->vap_index)) {
                    memcpy(cci_vap, vap, sizeof(wifi_vap_info_t));
                }
                break;
            case webconfig_subdoc_type_mesh:
                if (is_vap_mesh(&params->hal_cap.wifi_prop, vap->vap_index)) {
                    mac_filter_cci_vap_cache_update(i, j);
                    memcpy(cci_vap, vap, sizeof(wifi_vap_info_t));
                    webconfig_cci->radios[i].vaps.rdk_vap_array[j].acl_map =
                        params->radios[i].vaps.rdk_vap_array[j].acl_map;
                    webconfig_cci->radios[i].vaps.rdk_vap_array[j].vap_index =
                        params->radios[i].vaps.rdk_vap_array[j].vap_index;
                }
                break;
            case webconfig_subdoc_type_mesh_backhaul:
                if (is_vap_mesh_backhaul(&params->hal_cap.wifi_prop, vap->vap_index)) {
                    mac_filter_cci_vap_cache_update(i, j);
                    memcpy(cci_vap, vap, sizeof(wifi_vap_info_t));
                    webconfig_cci->radios[i].vaps.rdk_vap_array[j].acl_map =
                        params->radios[i].vaps.rdk_vap_array[j].acl_map;
                    webconfig_cci->radios[i].vaps.rdk_vap_array[j].vap_index =
                        params->radios[i].vaps.rdk_vap_array[j].vap_index;
                }
                break;
            case webconfig_subdoc_type_mesh_sta:
                if (is_vap_mesh_sta(&params->hal_cap.wifi_prop, vap->vap_index)) {
                    memcpy(cci_vap, vap, sizeof(wifi_vap_info_t));
                }
                break;
            default:
                wlan_emu_print(wlan_emu_log_level_err, "%s %d Invalid subdoc parse:%d\n", __func__,
                    __LINE__, data->type);
                return;
            }
        }
    }
}

void wlan_emu_ui_mgr_t::mac_filter_cci_cache_update(webconfig_subdoc_data_t *data)
{
    int itr, itrj;

    // webconfig decode allocate mem for the hash map which is getting cleared and destroyed here
    for (itr = 0; itr < (int)data->u.decoded.num_radios; itr++) {
        for (itrj = 0; itrj < MAX_NUM_VAP_PER_RADIO; itrj++) {
            hash_map_t **acl_dev_map = get_cci_acl_hash_map(itr, itrj);
            if (*acl_dev_map) {
                acl_entry_t *temp_acl_entry, *acl_entry;
                mac_addr_str_t mac_str;
                acl_entry = static_cast<acl_entry_t *>(hash_map_get_first(*acl_dev_map));
                while (acl_entry != NULL) {
                    to_mac_str(acl_entry->mac, mac_str);
                    acl_entry = static_cast<acl_entry_t *>(
                        hash_map_get_next(*acl_dev_map, acl_entry));
                    temp_acl_entry = static_cast<acl_entry_t *>(
                        hash_map_remove(*acl_dev_map, mac_str));
                    if (temp_acl_entry != NULL) {
                        free(temp_acl_entry);
                    }
                }
                hash_map_destroy(*acl_dev_map);
            }
        }
    }
}

void wlan_emu_ui_mgr_t::cci_cache_update(webconfig_subdoc_data_t *data)
{
    webconfig_subdoc_decoded_data_t *params;
    unsigned int i;

    webconfig_cci_t *webconfig_cci = get_webconfig_data();
    switch (data->type) {
    case webconfig_subdoc_type_radio:
        params = &data->u.decoded;
        for (i = 0; i < params->num_radios; i++) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s %d dml radio[%d] cache update\r\n", __func__,
                __LINE__, i);
            memcpy(&webconfig_cci->radios[i].oper, &params->radios[i].oper,
                sizeof(params->radios[i].oper));
        }
        break;
    case webconfig_subdoc_type_dml:
        wlan_emu_print(wlan_emu_log_level_dbg,
            "%s:%d subdoc parse and update dml global cache:%d\n", __func__, __LINE__, data->type);
        mac_filter_cci_cache_update(data);
        memcpy((unsigned char *)&webconfig_cci->radios, (unsigned char *)&data->u.decoded.radios,
            data->u.decoded.num_radios * sizeof(rdk_wifi_radio_t));
        memcpy((unsigned char *)&webconfig_cci->config, (unsigned char *)&data->u.decoded.config,
            sizeof(wifi_global_config_t));
        memcpy((unsigned char *)&webconfig_cci->hal_cap, (unsigned char *)&data->u.decoded.hal_cap,
            sizeof(wifi_hal_capability_t));
        webconfig_cci->hal_cap.wifi_prop.numRadios = data->u.decoded.num_radios;
        break;
    case webconfig_subdoc_type_wifi_config:
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d subdoc parse and update global config:%d\n",
            __func__, __LINE__, data->type);
        memcpy((unsigned char *)&webconfig_cci->config, (unsigned char *)&data->u.decoded.config,
            sizeof(wifi_global_config_t));
        break;
    default:
        this->update_cci_subdoc_vap_data(data);
        break;
    }
    if ((data->type > webconfig_subdoc_type_unknown) && (data->type < webconfig_subdoc_type_max)) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d update for subdoc type : %d\n", __func__,
            __LINE__, data->type);
        send_webconfig_ctrl_msg(data->type);
    }
}

void wlan_emu_ui_mgr_t::set_webconfig_cci_data(char *event_name, raw_data_t *bus_data,
    void *userData)
{
    (void)userData;
    int len = 0;
    char *pTmp = NULL;
    webconfig_subdoc_data_t *data = NULL;
    wlan_emu_ui_mgr_t *ptr;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s: %d bus event callback Event is %s \n", __func__,
        __LINE__, event_name);

    ptr = static_cast<wlan_emu_ui_mgr_t *>(userData);
    pTmp = new char[bus_data->raw_data_len + 1]();
    if (pTmp == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: malloc failed\n", __func__, __LINE__);
        return;
    }

    len = bus_data->raw_data_len;
    copy_string(pTmp, (char *)bus_data->raw_data.bytes, bus_data->raw_data_len);

    // setup the raw data
    data = (webconfig_subdoc_data_t *)malloc(sizeof(webconfig_subdoc_data_t));

    if (data == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s %d Failed to allocate memory\n", __func__,
            __LINE__);
        return;
    }
    memset(data, 0, sizeof(webconfig_subdoc_data_t));
    data->descriptor = 0;
    // data.descriptor = webconfig_data_descriptor_encoded |
    // webconfig_data_descriptor_translate_to_tr181;
    data->descriptor = webconfig_data_descriptor_encoded;

    // wlan_emu_print(wlan_emu_log_level_dbg,"%s:%d: dml Json:\n%s\r\n", __func__, __LINE__,
    // data.u.encoded.raw);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: hal capability update\r\n", __func__, __LINE__);
    memcpy(&data->u.decoded.hal_cap, (unsigned char *)&cci_webconfig.hal_cap,
        sizeof(wifi_hal_capability_t));

    data->u.decoded.num_radios = cci_webconfig.hal_cap.wifi_prop.numRadios;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: hal capability update radios : %d\r\n", __func__,
        __LINE__, data->u.decoded.num_radios);
    // tell webconfig to decode
    if (webconfig_decode(&cci_webconfig.webconfig, data, pTmp) == webconfig_error_none) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s %d webconfig_decode success\n", __FUNCTION__,
            __LINE__);
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s %d webconfig_decode fail \n%s\n", __FUNCTION__,
            __LINE__, data->u.encoded.raw);
        delete[] pTmp;
        free(data);
        return;
    }

    ptr->cci_cache_update(data);

    ptr->cci_webconfig_data_free(data);
    free(data);
    delete[] pTmp;

    return;
}

queue_t **wlan_emu_ui_mgr_t::get_cci_acl_new_entry_queue(unsigned int radio_index,
    unsigned int vap_index)
{
    webconfig_cci_t *webconfig_data = get_webconfig_data();
    if (webconfig_data == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s %d NULL Pointer\n", __func__, __LINE__);
        return NULL;
    }

    return &(webconfig_data->acl_data.new_entry_queue[radio_index][vap_index]);
}

queue_t **wlan_emu_ui_mgr_t::get_acl_new_entry_queue(wifi_vap_info_t *vap_info)
{
    if (vap_info == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s %d NULL Pointer\n", __func__, __LINE__);
        return NULL;
    }

    int radio_index = convert_vap_name_to_radio_array_index(
        &((webconfig_cci_t *)get_webconfig_data())->hal_cap.wifi_prop, vap_info->vap_name);
    int vap_array_index = convert_vap_name_to_array_index(
        &((webconfig_cci_t *)get_webconfig_data())->hal_cap.wifi_prop, vap_info->vap_name);

    if ((vap_array_index < 0) || (radio_index < 0)) {
        wlan_emu_print(wlan_emu_log_level_err, "%s %d Invalid array/radio Indices\n", __func__,
            __LINE__);
        return NULL;
    }

    webconfig_cci_t *webconfig_data = get_webconfig_data();
    if (webconfig_data == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s %d NULL Pointer\n", __func__, __LINE__);
        return NULL;
    }

    return &(webconfig_data->acl_data.new_entry_queue[radio_index][vap_array_index]);
}

webconfig_error_t wlan_emu_ui_mgr_t::webconfig_cci_apply(struct webconfig_subdoc *doc,
    webconfig_subdoc_data_t *data)
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d webconfig dml apply\n", __func__, __LINE__);
    return webconfig_error_none;
}

int wlan_emu_ui_mgr_t::update_json_param_string(cJSON *json, const char *key, cJSON **value)
{
    *value = cJSON_GetObjectItem(json, key);
    if ((*value == NULL) || (cJSON_IsString(*value) == false) || ((*value)->valuestring == NULL) ||
        (strcmp((*value)->valuestring, "") == 0)) {
        return RETURN_ERR;
    }
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d update for %s to %s\n", __func__, __LINE__, key,
        (*value)->valuestring);

    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::update_json_param_integer(cJSON *json, const char *key, cJSON **value)
{
    *value = cJSON_GetObjectItem(json, key);
    if ((*value == NULL) || (cJSON_IsNumber(*value) == false)) {
        return RETURN_ERR;
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d update for %s to %f\n", __func__, __LINE__, key,
        (*value)->valuedouble);
    return RETURN_OK;
}

int wlan_emu_ui_mgr_t::update_json_param_bool(cJSON *json, const char *key, cJSON **value)
{
    *value = cJSON_GetObjectItem(json, key);
    if ((*value == NULL) || (cJSON_IsBool(*value) == false)) {
        return RETURN_ERR;
    }
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d update for %s to %d\n", __func__, __LINE__, key,
        (((*value)->type & cJSON_True) ? true : false));
    return RETURN_OK;
}

webconfig_error_t wlan_emu_ui_mgr_t::update_vap_common_object(cJSON *vap, wifi_vap_info_t *vap_info)
{
    cJSON *param, *object;
    if (update_json_param_string(vap, "SSID", &param) == RETURN_OK) {
        snprintf(vap_info->u.bss_info.ssid, sizeof(vap_info->u.bss_info.ssid), "%s",
            param->valuestring);
    }

    if (update_json_param_bool(vap, "Enabled", &param) == RETURN_OK) {
        vap_info->u.bss_info.enabled = (param->type & cJSON_True) ? true : false;
    }

    if (update_json_param_bool(vap, "SSIDAdvertisementEnabled", &param) == RETURN_OK) {
        vap_info->u.bss_info.showSsid = (param->type & cJSON_True) ? true : false;
    }

    // Isolation
    if (update_json_param_bool(vap, "IsolationEnable", &param) == RETURN_OK) {
        vap_info->u.bss_info.isolation = (param->type & cJSON_True) ? true : false;
    }

    // ManagementFramePowerControl
    if (update_json_param_integer(vap, "ManagementFramePowerControl", &param) == RETURN_OK) {
        vap_info->u.bss_info.mgmtPowerControl = param->valuedouble;
    }

    // BssMaxNumSta
    if (update_json_param_integer(vap, "BssMaxNumSta", &param) == RETURN_OK) {
        vap_info->u.bss_info.bssMaxSta = param->valuedouble;
    }

    // BSSTransitionActivated
    if (update_json_param_bool(vap, "BSSTransitionActivated", &param) == RETURN_OK) {
        vap_info->u.bss_info.bssTransitionActivated = (param->type & cJSON_True) ? true : false;
    }

    // NeighborReportActivated
    if (update_json_param_bool(vap, "NeighborReportActivated", &param) == RETURN_OK) {
        vap_info->u.bss_info.nbrReportActivated = (param->type & cJSON_True) ? true : false;
    }

    // NetworkGreyList since this is not mandatory field we need
    // check for its existence before decode
    object = cJSON_GetObjectItem(vap, "NetworkGreyList");
    if (object != NULL) {
        if (update_json_param_bool(vap, "NetworkGreyList", &param) == RETURN_OK) {
            vap_info->u.bss_info.network_initiated_greylist = (param->type & cJSON_True) ? true :
                                                                                           false;
        }
    }
    // RapidReconnCountEnable
    if (update_json_param_bool(vap, "RapidReconnCountEnable", &param) == RETURN_OK) {
        vap_info->u.bss_info.rapidReconnectEnable = (param->type & cJSON_True) ? true : false;
    }

    // RapidReconnThreshold
    if (update_json_param_integer(vap, "RapidReconnThreshold", &param) == RETURN_OK) {
        vap_info->u.bss_info.rapidReconnThreshold = param->valuedouble;
    }

    // VapStatsEnable
    if (update_json_param_bool(vap, "VapStatsEnable", &param) == RETURN_OK) {
        vap_info->u.bss_info.vapStatsEnable = (param->type & cJSON_True) ? true : false;
    }

    // MacFilterEnable
    if (update_json_param_bool(vap, "MacFilterEnable", &param) == RETURN_OK) {
        vap_info->u.bss_info.mac_filter_enable = (param->type & cJSON_True) ? true : false;
    }

    // MacFilterMode
    if (update_json_param_integer(vap, "MacFilterMode", &param) == RETURN_OK) {
        vap_info->u.bss_info.mac_filter_mode = static_cast<wifi_mac_filter_mode_t>(
            param->valuedouble);
        if ((vap_info->u.bss_info.mac_filter_mode < 0) ||
            (vap_info->u.bss_info.mac_filter_mode > 1)) {
            wlan_emu_print(wlan_emu_log_level_err,
                "Invalid wifi vap mac filter mode %d, should be between 0 and 1\n",
                vap_info->u.bss_info.mac_filter_mode);
            // strncpy(execRetVal->ErrorMsg, "Invalid wifi vap mac filter mode:
            // 0..1",sizeof(execRetVal->ErrorMsg)-1);
            return webconfig_error_decode;
        }
    }
    // WmmEnabled
    if (update_json_param_bool(vap, "WmmEnabled", &param) == RETURN_OK) {
        vap_info->u.bss_info.wmm_enabled = (param->type & cJSON_True) ? true : false;
    }

    if (update_json_param_bool(vap, "UapsdEnabled", &param) == RETURN_OK) {
        vap_info->u.bss_info.UAPSDEnabled = (param->type & cJSON_True) ? true : false;
    }

    if (update_json_param_integer(vap, "BeaconRate", &param) == RETURN_OK) {
        vap_info->u.bss_info.beaconRate = static_cast<wifi_bitrate_t>(param->valuedouble);
    }

    // WmmNoAck
    if (update_json_param_integer(vap, "WmmNoAck", &param) == RETURN_OK) {
        vap_info->u.bss_info.wmmNoAck = param->valuedouble;
    }

    // WepKeyLength
    if (update_json_param_integer(vap, "WepKeyLength", &param) == RETURN_OK) {
        vap_info->u.bss_info.wepKeyLength = param->valuedouble;
    }

    // BssHotspot
    if (update_json_param_bool(vap, "BssHotspot", &param) == RETURN_OK) {
        vap_info->u.bss_info.bssHotspot = (param->type & cJSON_True) ? true : false;
    }

    // wpsPushButton
    if (update_json_param_integer(vap, "WpsPushButton", &param) == RETURN_OK) {
        vap_info->u.bss_info.wpsPushButton = param->valuedouble;
    }

    // wpsEnable
    if (update_json_param_bool(vap, "WpsEnable", &param) == RETURN_OK) {
        vap_info->u.bss_info.wps.enable = (param->type & cJSON_True) ? true : false;
    }

    // BeaconRateCtl
    if (update_json_param_string(vap, "BeaconRateCtl", &param) == RETURN_OK) {
        snprintf(vap_info->u.bss_info.beaconRateCtl, sizeof(vap_info->u.bss_info.beaconRateCtl),
            "%s", param->valuestring);
    }
    return webconfig_error_none;
}

webconfig_error_t wlan_emu_ui_mgr_t::update_vap_security_object(cJSON *security,
    wifi_vap_security_t *security_info, int band)
{
    cJSON *param, *object;

    if (update_json_param_string(security, "Mode", &param) == RETURN_OK) {

        if (strcmp(param->valuestring, "None") == 0) {
            security_info->mode = wifi_security_mode_none;
        } else if (strcmp(param->valuestring, "Enhanced-Open") == 0) {
            security_info->mode = wifi_security_mode_enhanced_open;
        } else if (strcmp(param->valuestring, "WPA-Personal") == 0) {
            security_info->mode = wifi_security_mode_wpa_personal;
        } else if (strcmp(param->valuestring, "WPA2-Personal") == 0) {
            security_info->mode = wifi_security_mode_wpa2_personal;
        } else if (strcmp(param->valuestring, "WPA-WPA2-Personal") == 0) {
            security_info->mode = wifi_security_mode_wpa_wpa2_personal;
        } else if (strcmp(param->valuestring, "WPA3-Personal") == 0) {
            security_info->mode = wifi_security_mode_wpa3_personal;
            security_info->u.key.type = wifi_security_key_type_sae;
        } else if (strcmp(param->valuestring, "WPA3-Personal-Transition") == 0) {
            security_info->mode = wifi_security_mode_wpa3_transition;
            security_info->u.key.type = wifi_security_key_type_psk_sae;
        } else if (strcmp(param->valuestring, "WPA-Enterprise") == 0) {
            security_info->mode = wifi_security_mode_wpa_enterprise;
        } else if (strcmp(param->valuestring, "WPA2-Enterprise") == 0) {
            security_info->mode = wifi_security_mode_wpa2_enterprise;
        } else if (strcmp(param->valuestring, "WPA-WPA2-Enterprise") == 0) {
            security_info->mode = wifi_security_mode_wpa_wpa2_enterprise;
        } else if (strcmp(param->valuestring, "WPA3-Enterprise") == 0) {
            security_info->mode = wifi_security_mode_wpa3_enterprise;
        } else {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d failed to decode security mode: %s\n",
                __func__, __LINE__, param->valuestring);
            return webconfig_error_decode;
        }

        if (band == WIFI_FREQUENCY_6_BAND &&
            security_info->mode != wifi_security_mode_wpa3_personal &&
            security_info->mode != wifi_security_mode_wpa3_enterprise &&
            security_info->mode != wifi_security_mode_enhanced_open) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d invalid security mode for 6G interface: %d\n", __func__, __LINE__,
                security_info->mode);
            return webconfig_error_decode;
        }

        // Not doing Radius, CHeck this later
        /*
          if (security_info->mode == wifi_security_mode_none ||
          security_info->mode == wifi_security_mode_enhanced_open) {
          object = cJSON_GetObjectItem(security, "RadiusSettings");
          if (object != NULL) {
          decode_param_object(security, "RadiusSettings", param);
          if (decode_open_radius_object(param, &security_info->u.radius) != 0) {
          wlan_emu_print(wlan_emu_log_level_err, "%s:%d failed to decode radius settings\n",
          __func__, __LINE__);
          return webconfig_error_decode;
          }
          }
          }
          */
        /*
          if (security_info->mode == wifi_security_mode_none) {
          return webconfig_error_none;
          }
          */
    }

    //	decode_param_string(security, "MFPConfig", param);
    if (update_json_param_string(security, "MFPConfig", &param) == RETURN_OK) {

        if (strstr(param->valuestring, "Disabled")) {
            security_info->mfp = wifi_mfp_cfg_disabled;
        } else if (strstr(param->valuestring, "Required")) {
            security_info->mfp = wifi_mfp_cfg_required;
        } else if (strstr(param->valuestring, "Optional")) {
            security_info->mfp = wifi_mfp_cfg_optional;
        } else {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d failed to decode MFP value: %s", __func__,
                __LINE__, param->valuestring);
            return webconfig_error_decode;
        }
    }

    if (security_info->mfp != wifi_mfp_cfg_optional &&
        security_info->mode == wifi_security_mode_wpa3_transition) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d invalid MFP value %d for %d mode\n", __func__,
            __LINE__, security_info->mfp, security_info->mode);
        return webconfig_error_encode;
    }

    if (security_info->mfp != wifi_mfp_cfg_required &&
        (security_info->mode == wifi_security_mode_enhanced_open ||
            security_info->mode == wifi_security_mode_wpa3_enterprise ||
            security_info->mode == wifi_security_mode_wpa3_personal)) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d invalid MFP value for %d mode: %d\n",
            __func__, __LINE__, security_info->mfp, security_info->mode);
        return webconfig_error_decode;
    }

    if (update_json_param_string(security, "EncryptionMethod", &param) == RETURN_OK) {

        if (strcmp(param->valuestring, "TKIP") == 0) {
            security_info->encr = wifi_encryption_tkip;
        } else if (strcmp(param->valuestring, "AES") == 0) {
            security_info->encr = wifi_encryption_aes;
        } else if (strcmp(param->valuestring, "AES+TKIP") == 0) {
            security_info->encr = wifi_encryption_aes_tkip;
        } else {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d failed to decode encryption method: %s\n",
                __func__, __LINE__, param->valuestring);
            return webconfig_error_decode;
        }
    }

    if (update_json_param_string(security, "RadiusIdentity", &param) == RETURN_OK) {
        if ((security_info->mode == wifi_security_mode_wpa2_enterprise) ||
            (security_info->mode == wifi_security_mode_wpa3_enterprise)) {
            strncpy(security_info->u.radius.identity, param->valuestring,
                sizeof(security_info->u.radius.identity) - 1);
        }
    }

    if (update_json_param_string(security, "RadiusPassword", &param) == RETURN_OK) {
        if ((security_info->mode == wifi_security_mode_wpa2_enterprise) ||
            (security_info->mode == wifi_security_mode_wpa3_enterprise)) {
            strncpy(security_info->u.radius.key, param->valuestring,
                sizeof(security_info->u.radius.key) - 1);
        }
    }

    if (update_json_param_string(security, "RadiusPhase2", &param) == RETURN_OK) {
        if ((security_info->mode == wifi_security_mode_wpa2_enterprise) ||
            (security_info->mode == wifi_security_mode_wpa3_enterprise)) {
            if (strstr(param->valuestring, "PAP")) {
                security_info->u.radius.phase2 = WIFI_EAP_PHASE2_PAP;
            } else {
                // Defaulting it to PAP as Radius server only support TTLS/PAP.
                security_info->u.radius.phase2 = WIFI_EAP_PHASE2_PAP;
            }
        }
    }

    if (update_json_param_string(security, "RadiusEAPMethod", &param) == RETURN_OK) {
        if ((security_info->mode == wifi_security_mode_wpa2_enterprise) ||
            (security_info->mode == wifi_security_mode_wpa3_enterprise)) {
            if (strstr(param->valuestring, "TTLS")) {
                security_info->u.radius.eap_type = WIFI_EAP_TYPE_TTLS;
            } else if (strstr(param->valuestring, "TLS")) {
                security_info->u.radius.eap_type = WIFI_EAP_TYPE_TLS;
                // Defaulting it to TTLS as of now.
            } else {
                security_info->u.radius.eap_type = WIFI_EAP_TYPE_TTLS;
            }
        }
    }

    if (security_info->encr != wifi_encryption_aes &&
        (security_info->mode == wifi_security_mode_enhanced_open ||
            security_info->mode == wifi_security_mode_wpa3_enterprise ||
            security_info->mode == wifi_security_mode_wpa3_personal)) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d invalid encryption method for %d mode: %d\n",
            __func__, __LINE__, security_info->encr, security_info->mode);
        return webconfig_error_decode;
    }

    if (security_info->encr == wifi_encryption_tkip &&
        security_info->mode == wifi_security_mode_wpa_wpa2_personal) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d invalid encryption method TKIP with "
            "WPA/WPA2 mode\n",
            __func__, __LINE__);
        return webconfig_error_decode;
    }

    if (security_info->mode == wifi_security_mode_wpa_enterprise ||
        security_info->mode == wifi_security_mode_wpa2_enterprise ||
        security_info->mode == wifi_security_mode_wpa_wpa2_enterprise ||
        security_info->mode == wifi_security_mode_wpa3_enterprise) {

        if (update_json_param_integer(security, "RekeyInterval", &param) == RETURN_OK) {
            security_info->rekey_interval = param->valuedouble;
        }

        if (update_json_param_bool(security, "StrictRekey", &param) == RETURN_OK) {
            security_info->strict_rekey = (param->type & cJSON_True) ? true : false;
        }

        if (update_json_param_integer(security, "EapolKeyTimeout", &param) == RETURN_OK) {
            security_info->eapol_key_timeout = param->valuedouble;
        }

        if (update_json_param_integer(security, "EapolKeyRetries", &param) == RETURN_OK) {
            security_info->eapol_key_retries = param->valuedouble;
        }

        if (update_json_param_integer(security, "EapIdentityReqTimeout", &param) == RETURN_OK) {
            security_info->eap_identity_req_timeout = param->valuedouble;
        }

        if (update_json_param_integer(security, "EapIdentityReqRetries", &param) == RETURN_OK) {
            security_info->eap_identity_req_retries = param->valuedouble;
        }

        if (update_json_param_integer(security, "EapReqTimeout", &param) == RETURN_OK) {
            security_info->eap_req_timeout = param->valuedouble;
        }

        if (update_json_param_integer(security, "EapReqRetries", &param) == RETURN_OK) {
            security_info->eap_req_retries = param->valuedouble;
        }

        if (update_json_param_bool(security, "DisablePmksaCaching", &param) == RETURN_OK) {
            security_info->disable_pmksa_caching = (param->type & cJSON_True) ? true : false;
        }

        return webconfig_error_none;
    }

    if (update_json_param_string(security, "Passphrase", &param) == RETURN_OK) {
        strncpy(security_info->u.key.key, param->valuestring, sizeof(security_info->u.key.key) - 1);

        if (security_info->mode != wifi_security_mode_none &&
            (strlen(param->valuestring) < MIN_PWD_LEN ||
                strlen(param->valuestring) > MAX_PWD_LEN)) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d invalid password length: %d\n", __func__,
                __LINE__, strlen(param->valuestring));
            return webconfig_error_decode;
        }
    }

    if (update_json_param_bool(security, "Wpa3_transition_disable", &param) == RETURN_OK) {
        security_info->wpa3_transition_disable = (param->type & cJSON_True) ? true : false;
    }

    if (update_json_param_integer(security, "RekeyInterval", &param) == RETURN_OK) {
        security_info->rekey_interval = param->valuedouble;
    }

    if (update_json_param_string(security, "KeyId", &param) == RETURN_OK) {
        strncpy(security_info->key_id, param->valuestring, sizeof(security_info->key_id) - 1);
    }

    return webconfig_error_none;
}

int wlan_emu_ui_mgr_t::get_radioindex_from_bssid(mac_address_t ap_bssid, unsigned int *radio_index)
{
    unsigned int i, j;
    wifi_vap_info_map_t *map;
    wifi_vap_info_t *vap;
    wifi_vap_info_map_t *cci_vap_map;
    wifi_vap_info_t *cci_vap;
    webconfig_cci_t *webconfig_cci = this->get_webconfig_data();
    mac_addr_str_t input_bssid_str, vap_bssid;

    to_mac_str(ap_bssid, input_bssid_str);
    str_tolower(input_bssid_str);
    for (i = 0; i < webconfig_cci->hal_cap.wifi_prop.numRadios; i++) {
        cci_vap_map = &webconfig_cci->radios[i].vaps.vap_map;
        for (j = 0; j < cci_vap_map->num_vaps; j++) {
            cci_vap = &cci_vap_map->vap_array[j];

            to_mac_str(cci_vap->u.bss_info.bssid, vap_bssid);
            str_tolower(vap_bssid);
            if (strcmp(input_bssid_str, vap_bssid) == 0) {
                *radio_index = webconfig_cci->radios[i].vaps.radio_index;
                return RETURN_OK;
            }
        }
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d invalid ap mac address : %s\n", __func__,
        __LINE__, input_bssid_str);
    return RETURN_ERR;
}

wifi_radio_operationParam_t *wlan_emu_ui_mgr_t::cci_get_radio_operation_param(
    unsigned int radio_index)
{
    webconfig_cci_t *webconfig_cci = this->get_webconfig_data();
    if (radio_index >= get_number_of_radios(&(webconfig_cci->hal_cap.wifi_prop))) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d Input radioIndex = %d not found, out of range\n",
            __FUNCTION__, __LINE__, radio_index);
        return NULL;
    }

    return &webconfig_cci->radios[radio_index].oper;
}

int wlan_emu_ui_mgr_t::bus_init()
{
    int rc = bus_error_success;
    char *component_name = "wifi-emulator-cci";
    char *str;
    wlan_emu_ui_mgr_t obj;
    const char bus_events[][128] = { WIFI_WEBCONFIG_DOC_DATA_NORTH, WIFI_WEBCONFIG_INIT_DML_DATA };
    unsigned int i = 0, itr = 0, itrj = 0;
    int len = 0;
    char *dbg_str;
    webconfig_subdoc_data_t *data = NULL;
    raw_data_t bus_data;

    rc = m_bus_mgr->desc.bus_open_fn(&m_webconfig_data->handle, component_name);
    if (rc != bus_error_success) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d bus open failed\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    for (i = 0; i < ARRAY_SIZE(bus_events); i++) {
        memset(&bus_data, 0, sizeof(raw_data_t));

        rc = m_bus_mgr->desc.bus_event_subs_fn(&m_webconfig_data->handle, bus_events[i],
            (void *)wlan_emu_ui_mgr_t::set_webconfig_cci_data, this, 0);

        if (rc != bus_error_success) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d Unable to subscribe to event  with bus error code : %d for %s\n",
                __FUNCTION__, __LINE__, rc, bus_events[i]);
            return RETURN_ERR;
        } else {
            wlan_emu_print(wlan_emu_log_level_info, "%s:%d Subscription succesful for %s\n",
                __FUNCTION__, __LINE__, bus_events[i]);
        }
    }

    memset(&bus_data, 0, sizeof(raw_data_t));
    rc = m_bus_mgr->desc.bus_data_get_fn(&m_webconfig_data->handle, WIFI_WEBCONFIG_INIT_DML_DATA,
        &bus_data);
    if (rc != bus_error_success) {
        wlan_emu_print(wlan_emu_log_level_err, "bus_get failed for [%s] with error [%d]\n",
            WIFI_WEBCONFIG_INIT_DML_DATA, rc);
        return RETURN_ERR;
    }

    // Initialize Webconfig Framework
    m_webconfig_data->webconfig.initializer = webconfig_initializer_cci;
    m_webconfig_data->webconfig.apply_data = webconfig_cci_apply;

    if (webconfig_init(&m_webconfig_data->webconfig) != webconfig_error_none) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d Init WiFi Web Config  fail\n", __FUNCTION__,
            __LINE__);
        // unregister and deinit everything
        return RETURN_ERR;
    }
    memset(m_webconfig_data->assoc_dev_hash_map, 0, sizeof(m_webconfig_data->assoc_dev_hash_map));

    for (itr = 0; itr < MAX_NUM_RADIOS; itr++) {
        for (itrj = 0; itrj < MAX_NUM_VAP_PER_RADIO; itrj++) {
            queue_t **new_dev_queue = (queue_t **)get_cci_acl_new_entry_queue(itr, itrj);
            *new_dev_queue = queue_create();
        }
    }

    for (itr = 0; itr < MAX_NUM_RADIOS; itr++) {
        for (itrj = 0; itrj < MAX_NUM_VAP_PER_RADIO; itrj++) {
            m_webconfig_data->radios[itr].vaps.rdk_vap_array[itrj].acl_map = NULL;
        }
    }

    wlan_emu_print(wlan_emu_log_level_dbg,
        "%s:%d bus_get WIFI_WEBCONFIG_INIT_DML_DATA successfull \n", __FUNCTION__, __LINE__);
    if (bus_data.data_type != bus_data_type_string && bus_data.raw_data.bytes == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d bus_data is NULL\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    str = new char[bus_data.raw_data_len + 1]();
    if (str == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d Memory allocation failed\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    copy_string(str, (char *)bus_data.raw_data.bytes, bus_data.raw_data_len);
    len = bus_data.raw_data_len;

    if ((dbg_str = static_cast<char *>(malloc(len + 1)))) {
        strncpy(dbg_str, str, len);
        dbg_str[len] = '\0';
        json_param_obscure(dbg_str, "Passphrase");
        json_param_obscure(dbg_str, "RadiusSecret");
        json_param_obscure(dbg_str, "SecondaryRadiusSecret");
        json_param_obscure(dbg_str, "DasSecret");
        //        wlan_emu_print(wlan_emu_log_level_dbg, "%s %d bus_get
        //        value=%s\n",__FUNCTION__,__LINE__,dbg_str);
        free(dbg_str);
    }

    // setup the raw data
    data = (webconfig_subdoc_data_t *)malloc(sizeof(webconfig_subdoc_data_t));

    if (data == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d Failed to allocate memory\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    memset(data, 0, sizeof(webconfig_subdoc_data_t));
    data->descriptor = 0;
    data->descriptor |= webconfig_data_descriptor_encoded;

    // tell webconfig to decode
    if (webconfig_decode(&m_webconfig_data->webconfig, data, str) == webconfig_error_none) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s %d webconfig_decode success \n", __FUNCTION__,
            __LINE__);
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s %d webconfig_decode fail \n", __FUNCTION__,
            __LINE__);
        delete[] str;
        free(data);
        return RETURN_ERR;
    }

    memcpy((unsigned char *)&m_webconfig_data->radios, (unsigned char *)data->u.decoded.radios,
        data->u.decoded.num_radios * sizeof(rdk_wifi_radio_t));
    memcpy((unsigned char *)&m_webconfig_data->config, &data->u.decoded.config,
        sizeof(wifi_global_config_t));
    memcpy((unsigned char *)&m_webconfig_data->hal_cap, &data->u.decoded.hal_cap,
        sizeof(wifi_hal_capability_t));
    m_webconfig_data->hal_cap.wifi_prop.numRadios = data->u.decoded.num_radios;
    m_webconfig_data->harvester.b_inst_client_enabled =
        m_webconfig_data->config.global_parameters.inst_wifi_client_enabled;
    m_webconfig_data->harvester.u_inst_client_reporting_period =
        m_webconfig_data->config.global_parameters.inst_wifi_client_reporting_period;
    m_webconfig_data->harvester.u_inst_client_def_reporting_period =
        m_webconfig_data->config.global_parameters.inst_wifi_client_def_reporting_period;
    snprintf(m_webconfig_data->harvester.mac_address,
        sizeof(m_webconfig_data->harvester.mac_address), "%02x%02x%02x%02x%02x%02x",
        m_webconfig_data->config.global_parameters.inst_wifi_client_mac[0],
        m_webconfig_data->config.global_parameters.inst_wifi_client_mac[1],
        m_webconfig_data->config.global_parameters.inst_wifi_client_mac[2],
        m_webconfig_data->config.global_parameters.inst_wifi_client_mac[3],
        m_webconfig_data->config.global_parameters.inst_wifi_client_mac[4],
        m_webconfig_data->config.global_parameters.inst_wifi_client_mac[5]);

    cci_webconfig_data_free(data);
    free(data);
    delete[] str;
    return RETURN_OK;
}

void wlan_emu_ui_mgr_t::update_log_file_offset()
{
    FILE *log_fp = NULL;
    long data_len = 0;
    log_fp = fopen(CCI_LOG_FILE, "r");
    if (log_fp == NULL) {
        return;
    }
    fseek(log_fp, 0, SEEK_END);
    data_len = ftell(log_fp);
    fclose(log_fp);

    set_log_file_offset(data_len);

    wlan_emu_print(wlan_emu_log_level_dbg, "%s %d data_len : %ld\n", __FUNCTION__, __LINE__,
        data_len);
}

wlan_emu_ui_mgr_t::wlan_emu_ui_mgr_t()
{
    memset(server_address, 0, sizeof(server_address));
    test_cov_cases_q = queue_create();
    pthread_cond_init(&m_heartbeat_cond, NULL);
    pthread_mutex_init(&m_heartbeat_lock, NULL);
}

wlan_emu_ui_mgr_t::~wlan_emu_ui_mgr_t()
{
    queue_destroy(test_cov_cases_q);
}
