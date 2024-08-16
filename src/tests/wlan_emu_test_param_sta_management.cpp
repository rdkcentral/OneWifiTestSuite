#include <assert.h>
#include "wlan_emu_log.h"
#include "wlan_emu_test_params.h"
#include "wlan_emu_sta_mgr.h"

#define STATION_STEP_EXEC_TIMEOUT 3

int test_step_param_sta_management::decode_user_ap_config(cJSON *sta_root_json, wifi_vap_info_t *ap_vap_info)
{
    test_step_params_t *step_config = this;
    webconfig_cci_t *webconfig_data = step_config->m_ui_mgr->get_webconfig_data();
    int radio_index, band;
    cJSON *vap_json_config = NULL, *security_config = NULL;


    vap_json_config = cJSON_GetObjectItem(sta_root_json, "WifiVapConfig");
    if (vap_json_config != NULL) {
        if (step_config->m_ui_mgr->update_vap_common_object(vap_json_config, ap_vap_info) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Json parse failed for update_vap_common_object\n", __func__, __LINE__);
            step_config->m_ui_mgr->dump_json(vap_json_config, __func__, __LINE__);
            return RETURN_ERR;
        }

        radio_index = convert_vap_name_to_radio_array_index(&webconfig_data->hal_cap.wifi_prop, ap_vap_info->vap_name);
        if (radio_index < 0) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid radio Index for vap_name : %s\n", __func__, __LINE__, ap_vap_info->vap_name);
            return webconfig_error_decode;
        }

        if (convert_radio_index_to_freq_band(&webconfig_data->hal_cap.wifi_prop, radio_index, &band) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Unable to fetch proper band for radio_index : %d\n", __func__, __LINE__, radio_index);
            return webconfig_error_decode;
        }

        security_config = cJSON_GetObjectItem(vap_json_config, "Security");
        if (step_config->m_ui_mgr->update_vap_security_object(security_config, &ap_vap_info->u.bss_info.security, band) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Json parse failed for update_vap_security_object\n", __func__, __LINE__);
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

    sta_vap_config->radio_index = ap_vap_config->radio_index;
    sta_vap_config->vap_mode = wifi_vap_mode_sta;
    snprintf(sta_vap_config->bridge_name, sizeof(sta_vap_config->bridge_name), "%s", ap_vap_config->bridge_name);
    sta_vap_config->u.sta_info.enabled = true;
    sta_vap_config->u.sta_info.scan_params.period = 10;
    snprintf(sta_vap_config->u.sta_info.ssid, sizeof(sta_vap_config->u.sta_info.ssid), "%s", ap_vap_config->u.bss_info.ssid);
    sta_vap_config->u.sta_info.security.mode = ap_vap_config->u.bss_info.security.mode;
    sta_vap_config->u.sta_info.security.encr = ap_vap_config->u.bss_info.security.encr;
    sta_vap_config->u.sta_info.security.mfp = ap_vap_config->u.bss_info.security.mfp;
    snprintf(sta_vap_config->u.sta_info.security.u.key.key, sizeof(sta_vap_config->u.sta_info.security.u.key.key), "%s", ap_vap_config->u.bss_info.security.u.key.key);
    //snprintf((char *)sta_vap_config->u.sta_info.bssid, sizeof(sta_vap_config->u.sta_info.bssid), "%s", ap_vap_config->u.bss_info.bssid);
    memcpy(sta_vap_config->u.sta_info.bssid, ap_vap_config->u.bss_info.bssid, sizeof(sta_vap_config->u.sta_info.bssid));
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: ssid : %s\n", __func__, __LINE__, sta_vap_config->u.sta_info.ssid);

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: radio_index : %d bridge_name : %s mode : %d enc : %d\n", __func__, __LINE__, sta_vap_config->radio_index,
            sta_vap_config->bridge_name, sta_vap_config->u.sta_info.security.mode, sta_vap_config->u.sta_info.security.encr);
    return RETURN_OK;
}


int test_step_param_sta_management::decode_step_sta_management_config()
{
    cJSON *param = NULL;
    cJSON *pattern_root = NULL;
    cJSON *pattern = NULL;
    cJSON *ap_json_config= NULL;
    cJSON *sta_root_json = NULL;
    char *json_data;
    int rc;

    test_step_params_t *step_config = this;
    wifi_vap_info_t *ap_vap_info = NULL;

    rc = step_config->m_ui_mgr->read_config_file(step_config->u.sta_test->test_station_config, &json_data);
    if (rc != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed reading from file : %s\n", __func__, __LINE__, step_config->u.sta_test->test_station_config);
        return RETURN_ERR;
    }

    sta_root_json = cJSON_Parse(json_data);
    if (sta_root_json == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Json parse failed for %s\n", __func__, __LINE__, json_data);
        free(json_data);
        return RETURN_ERR;
    }
    free(json_data);

    decode_param_string(sta_root_json, "StationType", param);

    if (strcmp(param->valuestring, "Iphone") == 0) {
        step_config->u.sta_test->sta_type = sta_model_type_iphone;
    } else if (strcmp(param->valuestring, "Pixel") == 0) {
        step_config->u.sta_test->sta_type = sta_model_type_pixel;
    } else {
        wlan_emu_print(wlan_emu_log_level_err,"%s:%d: Invalid valuestring for clienttype : %s\n", __func__, __LINE__, param->valuestring);
        return RETURN_ERR;
    }
    decode_param_string(sta_root_json, "StationName", param);
    snprintf(step_config->u.sta_test->sta_name, sizeof(step_config->u.sta_test->sta_name), "%s", param->valuestring);

    decode_param_string(sta_root_json, "VapName", param);
    snprintf(step_config->u.sta_test->u.sta_management.ap_vap_name, sizeof(step_config->u.sta_test->u.sta_management.ap_vap_name), "%s", param->valuestring);

    wlan_emu_print(wlan_emu_log_level_dbg,"%s:%d: Station AP Vapname : %s\n", __func__, __LINE__, step_config->u.sta_test->u.sta_management.ap_vap_name);
    ap_json_config = cJSON_GetObjectItem(sta_root_json, "WifiVapConfig");
    ap_vap_info = step_config->m_ui_mgr->get_cci_vap_info(step_config->u.sta_test->u.sta_management.ap_vap_name);

    decode_user_ap_config(sta_root_json, ap_vap_info);

    step_config->u.sta_test->sta_test_type = sta_test_type_management;
    step_config->u.sta_test->profile.mob  = sta_mobility_profile_type_static;
    step_config->u.sta_test->profile.conn  = sta_management_profile_type_connected;
    update_sta_config(ap_vap_info);
    step_config->u.sta_test->radio_oper_param = step_config->m_ui_mgr->cci_get_radio_operation_param(ap_vap_info->radio_index);

    param = cJSON_GetObjectItem(sta_root_json, "TestDuration");
    if ((param == NULL) || (cJSON_IsNumber(param) == false)) {
        step_config->u.sta_test->u.sta_management.test_duration = 0;
    } else {
        step_config->u.sta_test->u.sta_management.test_duration = param->valuedouble;
        step_config->execution_time = step_config->u.sta_test->u.sta_management.test_duration;
        step_config->u.sta_test->u.sta_management.is_sta_management_timer = true;

        wlan_emu_print(wlan_emu_log_level_dbg,"%s:%d: sta test : %d execution time is : %d\n", __func__, __LINE__, step_config->step_number, step_config->execution_time);

        step_config->u.sta_test->u.sta_management.connectivity_q = NULL;
        pattern_root = cJSON_GetObjectItem(sta_root_json, "SignalPattern");
        if ((pattern_root != NULL) && (cJSON_IsArray(pattern_root) == true)) {
            step_config->u.sta_test->u.sta_management.connectivity_q = queue_create();
            if (step_config->u.sta_test->u.sta_management.connectivity_q == NULL) {
                wlan_emu_print(wlan_emu_log_level_err,"%s:%d: Invalid valuestring for clienttype : %s\n", __func__, __LINE__, param->valuestring);
                return RETURN_ERR;
            }

            cJSON_ArrayForEach(pattern, pattern_root) {
                station_connectivity_profile_t *connect_profile = new station_connectivity_profile_t;
                if (connect_profile == NULL) {
                    wlan_emu_print(wlan_emu_log_level_err,"%s:%d: allocation for connect profile failed\n", __func__, __LINE__);
                    return RETURN_ERR;
                }

                decode_param_string(pattern, "Rssi", param);
                connect_profile->rssi = atoi(param->valuestring);
                decode_param_string(pattern, "Duration", param);
                connect_profile->duration = atoi(param->valuestring);
                wlan_emu_print(wlan_emu_log_level_dbg,"%s:%d: Rssi : %d Duration : %d\n", __func__, __LINE__, connect_profile->rssi, connect_profile->duration);
                connect_profile->test_state = test_state_pending;
                connect_profile->counter = 0;
                queue_push(step_config->u.sta_test->u.sta_management.connectivity_q, connect_profile);
            }
            step_config->u.sta_test->u.sta_management.current_profile_count = queue_count(step_config->u.sta_test->u.sta_management.connectivity_q)-1;
            wlan_emu_print(wlan_emu_log_level_dbg,"%s:%d: Queue_count : %d\n", __func__, __LINE__, step_config->u.sta_test->u.sta_management.current_profile_count);
        }
    }

    return RETURN_OK;
}

int test_step_param_sta_management::step_execute()
{
    char  file_to_read[128] = {0};
    int ret = 0;

    test_step_params_t *step = this;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Called for Test Step Num : %d\n",
            __func__, __LINE__, step->step_number);

    //    step->frame_capture.msg_type |= 1<<wlan_emu_msg_type_webconfig;
    //    step->frame_capture.subdoc_type = webconfig_subdoc_type_associated_clients;
    if (step->u.sta_test->is_decoded == false) {
        if (decode_step_sta_management_config() == RETURN_ERR) {
            return RETURN_ERR;
        }
        step->u.sta_test->is_decoded = true;
        step->u.sta_test->test_id = step->step_number;
        if (step->m_sta_mgr->add_sta(step->u.sta_test) == RETURN_ERR) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: add_sta failed for step : %d\n",
                    __func__, __LINE__, step->step_number);
            return RETURN_ERR;
        }
    }

    if (step->capture_frames == true) {
        step->test_state = wlan_emu_tests_state_cmd_continue;
    } else {
        step->test_state = wlan_emu_tests_state_cmd_results;
    }

    return RETURN_OK;
}


int test_step_param_sta_management::step_timeout()
{
    test_step_params_t *step = this;
    heart_beat_data_t *heart_beat_data;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Test Step Num : %d timeout_count : %d\n",
            __func__, __LINE__, step->step_number, step->timeout_count);


    if (step->test_state != wlan_emu_tests_state_cmd_results) {
        step->timeout_count++;

        if (step->execution_time == step->timeout_count) {
            step->test_state = wlan_emu_tests_state_cmd_results;
            wlan_emu_print(wlan_emu_log_level_info, "%s:%d: Test duration of %d  completed for step %d\n",
                    __func__, __LINE__, step->execution_time, step->step_number);
            return RETURN_OK;
        }

        if (step->u.sta_test->is_station_associated == true) {
            //add the logic  connectivity profile
            if (step->u.sta_test->u.sta_management.is_sta_management_timer == true) {
                if (step->u.sta_test->u.sta_management.connectivity_q != NULL) {
                    if ((queue_count(step->u.sta_test->u.sta_management.connectivity_q) > 0) && (step->u.sta_test->u.sta_management.current_profile_count >=0)) {
                        station_connectivity_profile_t *connect_profile = (station_connectivity_profile_t *)queue_peek(step->u.sta_test->u.sta_management.connectivity_q, step->u.sta_test->u.sta_management.current_profile_count);
                        if (connect_profile == NULL) {
                            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: connect_profile is NULL for  %d\n",
                                    __func__, __LINE__, step->u.sta_test->u.sta_management.current_profile_count);
                            return RETURN_OK;
                        }
                        wlan_emu_print(wlan_emu_log_level_dbg,"%s:%d: Rssi : %d Duration : %d Counter : %d\n", __func__, __LINE__, connect_profile->rssi, connect_profile->duration, connect_profile->counter);
                        connect_profile->counter++;
                        //create the heart beat data
                        heart_beat_data = new  heart_beat_data_t;
                        if (heart_beat_data == NULL) {
                            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Unable to send the heart beat for step %d\n",
                                    __func__, __LINE__, step->step_number);
                            return RETURN_ERR;
                        }
                        memcpy(heart_beat_data->mac, step->u.sta_test->sta_vap_config->u.sta_info.mac, sizeof(mac_address_t));
                        heart_beat_data->rssi = connect_profile->rssi;
                        step->m_sta_mgr->send_heart_beat(step->u.sta_test->key, heart_beat_data);
                        delete(heart_beat_data);
                        if (connect_profile->counter == connect_profile->duration) {
                            connect_profile->test_state = test_state_complete;
                            step->u.sta_test->u.sta_management.current_profile_count--;
                            if (step->u.sta_test->u.sta_management.current_profile_count == -1) {
                                step->test_state = wlan_emu_tests_state_cmd_results;
                                wlan_emu_print(wlan_emu_log_level_info, "%s:%d: connectivity test case completed for step : %d\n",
                                        __func__, __LINE__, step->step_number);
                                return RETURN_OK;
                            }
                        } else {
                            connect_profile->test_state = test_state_active;
                        }
                    }
                }
            } else {
                heart_beat_data = new  heart_beat_data_t;
                if (heart_beat_data == NULL) {
                    wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Unable to send the heart beat for step %d\n",
                            __func__, __LINE__, step->step_number);
                    return RETURN_ERR;
                }
                memcpy(heart_beat_data->mac, step->u.sta_test->sta_vap_config->u.sta_info.mac, sizeof(mac_address_t));
                heart_beat_data->rssi = -25;
                step->m_sta_mgr->send_heart_beat(step->u.sta_test->key, heart_beat_data);
                delete(heart_beat_data);
            }

        }

        if (step->fork == true) {
            if ((step->timeout_count % STATION_STEP_EXEC_TIMEOUT) == 0) {
                step->test_state = wlan_emu_tests_state_cmd_continue;
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Continue for test Step Num : %d execution_time : %d timeout_count %d\n",
                        __func__, __LINE__, step->step_number, step->execution_time, step->timeout_count);
            } else {
                step->test_state = wlan_emu_tests_state_cmd_wait;
                //execute next available step
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: wait for test Step Num : %d execution_time : %d timeout_count %d\n",
                        __func__, __LINE__, step->step_number, step->execution_time, step->timeout_count);
            }
        } else { //step->fork == false
            if (step->execution_time > step->timeout_count) {
                step->test_state = wlan_emu_tests_state_cmd_continue;
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Test Step Num : %d execution_time : %d timeout_count %d\n",
                        __func__, __LINE__, step->step_number, step->execution_time, step->timeout_count);
            }
        }

    }

    return RETURN_OK;
}

int test_step_param_sta_management::step_upload_files(FILE *output_file, bool *update_to_tda)
{
    wlan_emu_pcap_captures  *res_file = NULL;
    unsigned int results_count = 0;
    char *temp_res_file = NULL;
    char res_file_name[128] = {0};
    char *remote_test_results_loc = NULL;
    char sta_connect_info[256] = {0};
    char timestamp[24] = {0};
    cJSON *json;
    char  mac_str[32] = {0};
    FILE *fp;
    char *json_str;

    test_step_params_t *step = this;

    remote_test_results_loc = step->m_ui_mgr->get_remote_test_results_loc();
    if (step->capture_frames == true) {
        if (step->test_results_queue == NULL) {
            return RETURN_ERR;
        }
        results_count = queue_count(step->test_results_queue);
        if (results_count == 0) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: No test results files to upload for for step case %d \n", __func__, __LINE__, step->step_number);
            return RETURN_ERR;
        }

        res_file = (wlan_emu_pcap_captures *)queue_pop(step->test_results_queue);

        while(res_file != NULL) {
            if (res_file != NULL) {
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: File: %s\n", __func__, __LINE__, res_file->pcap_file);
                if (step->m_ui_mgr->upload_file_to_server(res_file->pcap_file, remote_test_results_loc) != RETURN_OK) {
                    wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to upload %s\n", __func__, __LINE__, res_file->pcap_file);
                    return RETURN_ERR;
                } else {
                    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: uploaded %s\n", __func__, __LINE__, res_file->pcap_file);
                    *update_to_tda = true;
                    temp_res_file = strdup(res_file->pcap_file);
                    if (step->m_ui_mgr->get_last_substring_after_slash(temp_res_file, res_file_name, sizeof(res_file_name)) != RETURN_OK) {
                        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: get_last_substring_after_slash failed for str : %s\n",
                                __func__, __LINE__, temp_res_file);
                        free(temp_res_file);
                        return RETURN_ERR;
                    }
                    fprintf(output_file, "%s\n", res_file_name);
                    free(temp_res_file);
                }
                delete res_file;
            }
            res_file = (wlan_emu_pcap_captures *)queue_pop(step->test_results_queue);
        }
        queue_destroy(step->test_results_queue);
        step->test_results_queue = NULL;
    }

    //Creation of json for station
    if (get_current_time_string(timestamp, sizeof(timestamp)) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: get_current_time_string failed\n",
                __func__, __LINE__);
        return RETURN_ERR;
    }

    json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "StepNumber", step->step_number);
    uint8_mac_to_string_mac(step->u.sta_test->sta_vap_config->u.sta_info.mac, mac_str);
    cJSON_AddStringToObject(json, "StationMacAddress", mac_str);

    snprintf(sta_connect_info, sizeof(sta_connect_info), "%s/%s_%d_%s_STATION_%d.json",
            step->m_ui_mgr->get_test_results_dir_path(), step->test_case_id, step->step_number, timestamp, step->u.sta_test->sta_vap_config->radio_index);

    fp = fopen(sta_connect_info, "w");
    if (fp == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: fopen failed for %s\n",
                __func__, __LINE__, sta_connect_info);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    json_str = cJSON_Print(json);

    fputs(json_str, fp);
    fclose(fp);
    free(json_str);
    cJSON_Delete(json);

    if (step->m_ui_mgr->upload_file_to_server(sta_connect_info, remote_test_results_loc) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to upload %s\n", __func__, __LINE__, sta_connect_info);
        return RETURN_ERR;
    } else {
        wlan_emu_print(wlan_emu_log_level_info, "%s:%d: uploaded %s\n", __func__, __LINE__, sta_connect_info);
        *update_to_tda = true;
        temp_res_file = strdup(sta_connect_info);
        if (step->m_ui_mgr->get_last_substring_after_slash(temp_res_file, res_file_name, sizeof(res_file_name)) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: get_last_substring_after_slash failed for str : %s\n",
                    __func__, __LINE__, temp_res_file);
            free(temp_res_file);
            return RETURN_ERR;
        }
        fprintf(output_file, "%s\n", res_file_name);
        free(temp_res_file);
    }

    if (remove(sta_connect_info) == 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Error Removing the file : %s\n",
                __func__, __LINE__, sta_connect_info);
    }

    return RETURN_OK;
}

void test_step_param_sta_management::step_remove()
{
    test_step_param_sta_management *step = dynamic_cast<test_step_param_sta_management *>(this);
    unsigned int results_count = 0;
    wlan_emu_pcap_captures  *res_file = NULL;

    if (step == NULL) {
        return;
    }

    if (step->is_step_initialized == true) {
        if (step->u.sta_test->sta_vap_config != NULL) {
            wlan_emu_print(wlan_emu_log_level_info, "%s:%d: Disconnecting the client at vap index : %d\n",
                    __func__, __LINE__, step->u.sta_test->sta_vap_config->vap_index);
            if (step->u.sta_test->is_station_associated == true) {
                step->m_sta_mgr->remove_sta(step->u.sta_test);
                step->u.sta_test->is_station_associated = false;
            }

            delete step->u.sta_test->sta_vap_config;
        }

        //Below check to remove on error cases
        if (step->capture_frames == true) {
            if (step->test_results_queue != nullptr) {
                queue_destroy(step->test_results_queue);
                step->test_results_queue = nullptr;
            }
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
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step number : %d\n", __func__, __LINE__, step->step_number);

    if (msg == NULL) {
        return RETURN_UNHANDLED;
    }

    //expect only wlan_emu_msg_type_cfg80211 or  wlan_emu_msg_type_webconfig
    switch (msg->get_msg_type()) {
        case wlan_emu_msg_type_frm80211: //mgmt
            if ((step->capture_frames != true) || (!(step->frame_request.msg_type & (1<<msg->get_msg_type())))) {
                return RETURN_UNHANDLED;
            }

            //irrespective of capture_frames check for eapol-3 to confirm whether the client is associated or not
            f_data = msg->get_msg();

            if (memcmp(step->u.sta_test->sta_vap_config->u.sta_info.mac, f_data->u.frm80211.u.frame.client_macaddr, sizeof(mac_addr_t)) == 0) {

                if (wlan_emu_frm80211_ops_type_eapol == msg->get_frm80211_ops_type()) {
                    if (msg->get_msgname_from_msgtype() == RETURN_OK) {
                        if (strncmp(msg->get_msg_name(), "eapol-msg3", strlen("eapol-msg3")) == 0) {
                            step->u.sta_test->is_station_associated = true;
                        }
                    }
                }

                if (!(step->frame_request.frm80211_ops & (1<<msg->get_frm80211_ops_type()))) {
                    return RETURN_UNHANDLED;
                }
                msg->unload_frm80211_msg(step);
                return RETURN_HANDLED;
            } else {
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Not matching with sta mac address\n",
                        __func__, __LINE__);
            }
        break;
        case wlan_emu_msg_type_cfg80211: //beacon
        case wlan_emu_msg_type_webconfig://onewifi_webconfig
        default:
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Not supported msg_type : %d\n", __func__, __LINE__, msg->get_msg_type());
        break;
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: unhandled step number : %d msg_type : %d\n", __func__, __LINE__, step->step_number, msg->get_msg_type());
    return RETURN_UNHANDLED;
}




test_step_param_sta_management::test_step_param_sta_management()
{
    test_step_params_t *step = this;
    step->is_step_initialized = true;

    step->u.sta_test = new sta_test_t;
    if (step->u.sta_test == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: allocation of memory for sta_test failed for %d\n",
                __func__, __LINE__, step->step_number);
        step->is_step_initialized = false;
    }

    step->u.sta_test->is_decoded = false;
    step->u.sta_test->sta_vap_config = new(std::nothrow) wifi_vap_info_t;
    if (step->u.sta_test->sta_vap_config == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: allocation of memory for sta failed for %d\n",
                __func__, __LINE__, step->step_number);
        delete step->u.sta_test;
        step->is_step_initialized = false;
    }
    memset(step->u.sta_test->sta_vap_config, 0, sizeof(wifi_vap_info_t));

    step->execution_time = 3;
    step->timeout_count = 0;
    step->test_results_queue = NULL;
    step->capture_frames = false;
    memset(step->u.sta_test->sta_vap_config, 0, sizeof(wifi_vap_info_t));
    step->u.sta_test->u.sta_management.is_sta_management_timer = false;
    step->u.sta_test->is_station_associated = false;

}

test_step_param_sta_management::~test_step_param_sta_management()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Destructor for sta management called\n", __func__, __LINE__);

}
