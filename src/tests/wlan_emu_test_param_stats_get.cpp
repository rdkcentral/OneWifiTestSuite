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

#include "wifi_base.h"
#include "wlan_emu_common.h"
#include "wlan_emu_log.h"
#include "wlan_emu_test_params.h"
#include "wlan_emu_bus.h"
#include "wlan_emu_err_code.h"
#include <assert.h>
#include <cjson/cJSON.h>
#include <errno.h>
#include <fcntl.h>
#include <sstream>
#include <string>

int test_step_param_get_stats_t::step_frame_filter(wlan_emu_msg_t *msg)
{
    test_step_params_t *step = this;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step number : %d\n", __func__, __LINE__,
        step->step_number);
    if (msg == NULL) {
        return RETURN_UNHANDLED;
    }
    switch (msg->get_msg_type()) {
    case wlan_emu_msg_type_webconfig: // onewifi_webconfig
    case wlan_emu_msg_type_cfg80211: // beacon
    case wlan_emu_msg_type_frm80211: // mgmt
    default:
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Not supported msg_type : %d\n", __func__,
            __LINE__, msg->get_msg_type());
        break;
    }
    return RETURN_UNHANDLED;
}

char *test_step_param_get_stats_t::getStatsClass()
{
    test_step_params_t *step = this;
    switch (step->u.wifi_stats_get->data_type) {
    case wifi_mon_stats_type_t::mon_stats_type_radio_channel_stats:
        return "Radio_Channel_Stats";
    case wifi_mon_stats_type_t::mon_stats_type_neighbor_stats:
        return "Neighbor_Stats";
    case wifi_mon_stats_type_t::mon_stats_type_associated_device_stats:
        return "Assoc_Clients_Stats";
    case wifi_mon_stats_type_t::mon_stats_type_radio_diagnostic_stats:
        return "Radio_Diag_Stats";
    case wifi_mon_stats_type_t::mon_stats_type_radio_temperature:
        return "Radio_Temperature_Stats";
    default:
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Unknown data type %d\n", __func__, __LINE__,
            step->u.wifi_stats_get->data_type);
        step->m_ui_mgr->cci_error_code = EUNKNOWNTYPE;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return nullptr;
    }
}

char *test_step_param_get_stats_t::get_scanmode()
{
    test_step_params_t *step = this;

    switch (step->u.wifi_stats_get->scan_mode) {
    case wifi_neighborScanMode_t::WIFI_RADIO_SCAN_MODE_ONCHAN:
        return "on_channel";
    case wifi_neighborScanMode_t::WIFI_RADIO_SCAN_MODE_OFFCHAN:
        return "off_channel";
    case wifi_neighborScanMode_t::WIFI_RADIO_SCAN_MODE_FULL:
        return "full_channel";
    case wifi_neighborScanMode_t::WIFI_RADIO_SCAN_MODE_SURVEY:
        return "Survey";
    default:
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Unknown scan mode %d\n", __func__, __LINE__,
            step->u.wifi_stats_get->scan_mode);
        step->m_ui_mgr->cci_error_code = EUNKNOWNSCAN;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return nullptr;
    }
}

int test_step_param_get_stats_t::get_subscription_string(char *str, int str_len)
{
    test_step_params_t *step = this;
    char *scan_mode = get_scanmode();
    switch (step->u.wifi_stats_get->data_type) {
    case wifi_mon_stats_type_t::mon_stats_type_radio_channel_stats:
        if (get_scanmode() != nullptr) {
            snprintf(str, str_len, "Device.WiFi.CollectStats.Radio.%d.ScanMode.%s.ChannelStats",
                step->u.wifi_stats_get->radio_index + 1, scan_mode);
            return RETURN_OK;
        }
        break;
    case wifi_mon_stats_type_t::mon_stats_type_neighbor_stats:
        if (get_scanmode() != nullptr) {
            snprintf(str, str_len, "Device.WiFi.CollectStats.Radio.%d.ScanMode.%s.NeighborStats",
                step->u.wifi_stats_get->radio_index + 1, scan_mode);
            return RETURN_OK;
        }
        break;
    case wifi_mon_stats_type_t::mon_stats_type_associated_device_stats:
        snprintf(str, str_len, "Device.WiFi.CollectStats.AccessPoint.%d.AssociatedDeviceStats",
            step->u.wifi_stats_get->vap_index + 1);
        return RETURN_OK;
    case wifi_mon_stats_type_t::mon_stats_type_radio_diagnostic_stats:
        snprintf(str, str_len, "Device.WiFi.CollectStats.Radio.%d.RadioDiagnosticStats",
            step->u.wifi_stats_get->radio_index + 1);
        return RETURN_OK;
    case wifi_mon_stats_type_t::mon_stats_type_radio_temperature:
        snprintf(str, str_len, "Device.WiFi.CollectStats.Radio.%d.RadioTemperatureStats",
            step->u.wifi_stats_get->radio_index + 1);
        return RETURN_OK;
        break;
    default:
        break;
    }
    return RETURN_ERR;
}

char *test_step_param_get_stats_t::get_scanmode_str()
{
    test_step_params_t *step = this;

    switch (step->u.wifi_stats_get->scan_mode) {
    case wifi_neighborScanMode_t::WIFI_RADIO_SCAN_MODE_ONCHAN:
        return "OnChan";
    case wifi_neighborScanMode_t::WIFI_RADIO_SCAN_MODE_OFFCHAN:
        return "OffChan";
    case wifi_neighborScanMode_t::WIFI_RADIO_SCAN_MODE_FULL:
        return "FullChan";
    case wifi_neighborScanMode_t::WIFI_RADIO_SCAN_MODE_SURVEY:
        return "Survey";
    default:
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Unknown scan mode %d\n", __func__, __LINE__,
            step->u.wifi_stats_get->scan_mode);
        step->m_ui_mgr->cci_error_code = EUNKNOWNSCAN;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return nullptr;
    }
}

char *test_step_param_get_stats_t::get_stats_response_type()
{
    test_step_params_t *step = this;

    if (step->u.wifi_stats_get->is_stat_response_type_set == true) {
        return "SET";
    }

    return "GET";
}

int test_step_param_get_stats_t::update_output_file_name()
{
    test_step_params_t *step = this;
    char *scan_mode = nullptr;
    switch (step->u.wifi_stats_get->data_type) {
    case wifi_mon_stats_type_t::mon_stats_type_radio_channel_stats:
        scan_mode = get_scanmode_str();
        if (scan_mode == nullptr) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Unknown scan mode for type %d\n",
                __func__, __LINE__, step->u.wifi_stats_get->data_type);
            return RETURN_ERR;
        }
        snprintf(step->u.wifi_stats_get->output_file_name,
            sizeof(step->u.wifi_stats_get->output_file_name), "%s_RadioChannelStats_r%d_%s",
            get_stats_response_type(), step->u.wifi_stats_get->radio_index, scan_mode);
        return RETURN_OK;
    case wifi_mon_stats_type_t::mon_stats_type_neighbor_stats:
        scan_mode = get_scanmode_str();
        if (scan_mode == nullptr) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Unknown scan mode for type %d\n",
                __func__, __LINE__, step->u.wifi_stats_get->data_type);
            return RETURN_ERR;
        }
        snprintf(step->u.wifi_stats_get->output_file_name,
            sizeof(step->u.wifi_stats_get->output_file_name), "%s_NeighborStats_r%d_%s",
            get_stats_response_type(), step->u.wifi_stats_get->radio_index, scan_mode);
        return RETURN_OK;
    case wifi_mon_stats_type_t::mon_stats_type_associated_device_stats:
        snprintf(step->u.wifi_stats_get->output_file_name,
            sizeof(step->u.wifi_stats_get->output_file_name), "%s_AssocClientStats_v%d",
            get_stats_response_type(), step->u.wifi_stats_get->vap_index);
        return RETURN_OK;
    case wifi_mon_stats_type_t::mon_stats_type_radio_diagnostic_stats:
        snprintf(step->u.wifi_stats_get->output_file_name,
            sizeof(step->u.wifi_stats_get->output_file_name), "%s_RadioDiagStats_r%d",
            get_stats_response_type(), step->u.wifi_stats_get->radio_index);
        return RETURN_OK;
    case wifi_mon_stats_type_t::mon_stats_type_radio_temperature:
        snprintf(step->u.wifi_stats_get->output_file_name,
            sizeof(step->u.wifi_stats_get->output_file_name), "%s_RadioTempStats_r%d",
            get_stats_response_type(), step->u.wifi_stats_get->radio_index);
        return RETURN_OK;
    default:
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Unknown data type %d\n", __func__, __LINE__,
            step->u.wifi_stats_get->data_type);
        step->m_ui_mgr->cci_error_code = EUNKNOWNTYPE;
        step->test_state = wlan_emu_tests_state_cmd_abort;
    }

    return RETURN_ERR;
}

void test_step_param_get_stats_t::stats_get_event_handler(char *event_name, raw_data_t *p_data, void *userData)
{
    bus_error_t rc = bus_error_success;
    const char *json_str = NULL;
    int len = 0;
    test_step_params_t *step = NULL;
    char file_name[128] = { 0 };
    char *temp_file_name;
    FILE *fp = NULL;
    char timestamp[24] = { 0 };
    webconfig_cci_t *cci_webconfig;
    raw_data_t data;

    step = static_cast<test_step_params_t *>(userData);
    cci_webconfig = step->m_ui_mgr->get_webconfig_data();

    memset(&data, 0, sizeof(raw_data_t));
    wlan_emu_print(wlan_emu_log_level_dbg, "%s: %d bus event callback Event is %s \n", __func__,
        __LINE__, event_name);

    rc = step->m_bus_mgr->desc.bus_data_get_fn(&cci_webconfig->handle, event_name, &data);

    if (rc != bus_error_success || (data.data_type != bus_data_type_string)) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: bus_data_get_fn failed for %s\n", __func__,
            __LINE__, event_name);
        return;
    }

    json_str = new char[data.raw_data_len + 1]();
    strncpy((char *)json_str, (char *)data.raw_data.bytes, data.raw_data_len);
    len = data.raw_data_len;

    if (json_str == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s Null pointer,bus get string len=%d\n",
            __FUNCTION__, len);
        return;
    }

    pthread_mutex_lock(&step->s_lock);
    step->u.wifi_stats_get->get_stats_count++;

    if (get_current_time_string(timestamp, sizeof(timestamp)) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: get_current_time_string failed\n", __func__,
            __LINE__);
        free((void *)json_str);
        return;
    }

    snprintf(file_name, sizeof(file_name), "%s/%s_%d_%s_%s_%d.json",
        step->m_ui_mgr->get_test_results_dir_path(), step->test_case_id, step->step_number,
        timestamp, step->u.wifi_stats_get->output_file_name, step->u.wifi_stats_get->get_stats_count);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d file_name : %s\n", __FUNCTION__, __LINE__,
        file_name);

    fp = fopen(file_name, "w");
    if (fp == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: fopen failed for %s\n", __func__, __LINE__,
            file_name);
        step->m_ui_mgr->cci_error_code = EFOPEN;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        pthread_mutex_unlock(&step->s_lock);
        free((void *)json_str);
        return;
    }
    fputs((const char *)json_str, fp);

    fclose(fp);
    temp_file_name = strdup(file_name);
    if (step->m_ui_mgr->step_upload_files(temp_file_name) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: step_upload_files failed for %s\n",
            __func__, __LINE__, temp_file_name);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        step->m_ui_mgr->cci_error_code = EPUSHTSTRESFILE;
        free((void *)json_str);
        pthread_mutex_unlock(&step->s_lock);
        return;
    }

    pthread_mutex_unlock(&step->s_lock);
}

int test_step_param_get_stats_t::start_subscription()
{
    char subscription[128] = { 0 };
    int index = 0;
    int rc = bus_error_success;
    test_step_params_t *step = this;
    webconfig_cci_t *cci_webconfig;
    cci_webconfig = step->m_ui_mgr->get_webconfig_data();
    raw_data_t data;

    memset(&data, 0, sizeof(raw_data_t));
    if (get_subscription_string(subscription, sizeof(subscription)) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: Get Subscription string failed for step : %d \n", __func__, __LINE__,
            step->step_number);
        return RETURN_ERR;
    }
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Subscription string : %s\n", __func__, __LINE__,
        subscription);

    rc = step->m_bus_mgr->desc.bus_event_subs_fn(&cci_webconfig->handle, subscription,
        (void *)test_step_param_get_stats_t::stats_get_event_handler, this, 0);

    if (rc != bus_error_success) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: Subscription failed for string : %s for step : %d rc : %d\n", __func__,
            __LINE__, subscription, step->step_number, rc);
        step->m_ui_mgr->cci_error_code = EBUS;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    } else {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Successfully subscribed to %s\n", __func__,
            __LINE__, subscription);
    }
    return RETURN_OK;
}

int test_step_param_get_stats_t::stop_subscription(test_step_params_t *step)
{
    char subscription[128] = { 0 };
    int index = 0;
    int rc = bus_error_success;
    webconfig_cci_t *cci_webconfig;
    cci_webconfig = step->m_ui_mgr->get_webconfig_data();

    if (get_subscription_string(subscription, sizeof(subscription)) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: Get UnSubscription string failed for step : %d \n", __func__, __LINE__,
            step->step_number);
        return RETURN_ERR;
    }
    wlan_emu_print(wlan_emu_log_level_dbg, " UnSubscription string : %s\n", subscription);

    rc = step->m_bus_mgr->desc.bus_event_unsubs_fn(&cci_webconfig->handle, subscription);
    if (rc != bus_error_success) {
        wlan_emu_print(wlan_emu_log_level_err, "Failed to UnSubscribe to %s: %d\n", subscription,
            rc);
        step->m_ui_mgr->cci_error_code = EBUS;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    } else {
        wlan_emu_print(wlan_emu_log_level_dbg, "Successfully UnSubscribed to %s\n", subscription);
        step->test_state = wlan_emu_tests_state_cmd_results;
    }
    return RETURN_OK;
}

int test_step_param_get_stats_t::step_execute()
{
    test_step_params_t *step = this;
    test_step_params_t *step_to_be_stopped = NULL;
    int timeout = 0;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: %s\n", __func__, __LINE__, getStatsClass());

    if (step == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: step is NULL\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    if (step->u.wifi_stats_get == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: step->u.wifi_stats_get is NULL\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    if (step->u.wifi_stats_get->log_operation == log_operation_type_t::log_operation_type_start) {
        if (start_subscription() == RETURN_ERR) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: start subscription failed for step : %d \n", __func__, __LINE__,
                step->step_number);
            return RETURN_ERR;
        }
        step->test_state = wlan_emu_tests_state_cmd_wait;
        if (update_output_file_name() == RETURN_ERR) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: update of output file name failed for step : %d \n", __func__, __LINE__,
                step->step_number);
            return RETURN_ERR;
        }
    } else if (step->u.wifi_stats_get->log_operation ==
        log_operation_type_t::log_operation_type_stop) {
        wlan_emu_test_case_config *test_case_config =
            (wlan_emu_test_case_config *)step->param_get_test_case_config();
        step_to_be_stopped = (test_step_params_t *)step->m_ui_mgr->get_step_from_step_number(
            test_case_config, step->u.wifi_stats_get->stop_log_step_number);
        if (step_to_be_stopped == NULL) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: Invalid log stop step  : %d in step : %d\n", __func__, __LINE__,
                step->u.wifi_stats_get->stop_log_step_number, step->step_number);
            step->m_ui_mgr->cci_error_code = ESTEPSTOPPED;
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }

        if (stop_subscription(step_to_be_stopped) == RETURN_ERR) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: start subscription failed for step : %d \n", __func__, __LINE__,
                step->step_number);
            return RETURN_ERR;
        }
        step->test_state = wlan_emu_tests_state_cmd_results;
    } else if (step->u.wifi_stats_get->log_operation == log_operation_type_timer) {
        if (start_subscription() == RETURN_ERR) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: start subscription failed for step : %d \n", __func__, __LINE__,
                step->step_number);
            return RETURN_ERR;
        }
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step->u.wifi_stats_get->timeout : %d\n",
            __func__, __LINE__, step->u.wifi_stats_get->timeout);
        step->execution_time = step->u.wifi_stats_get->timeout;
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: execution time : %d\n", __func__, __LINE__,
            step->execution_time);
        step->test_state = wlan_emu_tests_state_cmd_continue;
        if (update_output_file_name() == RETURN_ERR) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: update of output file name failed for step : %d \n", __func__, __LINE__,
                step->step_number);
            return RETURN_ERR;
        }
    }

    return RETURN_OK;
}

int test_step_param_get_stats_t::step_timeout()
{
    test_step_params_t *step = this;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: %s\n", __func__, __LINE__, getStatsClass());

    if ((step->test_state != wlan_emu_tests_state_cmd_results) &&
        (step->u.wifi_stats_get->log_operation == log_operation_type_timer)) {
        step->timeout_count++;
        if (step->execution_time == step->timeout_count) {
            stop_subscription(this);
            step->test_state = wlan_emu_tests_state_cmd_results;
            wlan_emu_print(wlan_emu_log_level_info,
                "%s:%d: Test duration of %d  completed for step %d\n", __func__, __LINE__,
                step->execution_time, step->step_number);
            return RETURN_OK;
        }
        wlan_emu_print(wlan_emu_log_level_dbg,
            "%s:%d: Test Step Num : %d execution_time : %d timeout_count %d\n", __func__, __LINE__,
            step->step_number, step->execution_time, step->timeout_count);
        step->test_state = wlan_emu_tests_state_cmd_continue;
    } else if ((step->u.wifi_stats_get->log_operation == log_operation_type_stop)) {
        step->test_state = wlan_emu_tests_state_cmd_results;
    } else if ((step->u.wifi_stats_get->log_operation == log_operation_type_start)) {
        step->test_state = wlan_emu_tests_state_cmd_results;
    } else {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: abort Test Step Num : %d execution_time : %d timeout_count %d\n", __func__,
            __LINE__, step->step_number, step->execution_time, step->timeout_count);
        step->m_ui_mgr->cci_error_code = ESTEPTIMEOUT;
        step->test_state = wlan_emu_tests_state_cmd_abort;
    }
    return RETURN_OK;
}

void test_step_param_get_stats_t::step_remove()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: %s\n", __func__, __LINE__, getStatsClass());
    test_step_params_t *step = dynamic_cast<test_step_params_t *>(this);
    if (step == NULL) {
        return;
    }
    if (step->is_step_initialized == true) {
        pthread_mutex_destroy(&step->s_lock);
        delete step->u.wifi_stats_get;
    }
    delete step;
    step = NULL;
}

test_step_param_get_radio_channel_stats::test_step_param_get_radio_channel_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg,
        "test_step_param_get_radio_channel_stats::test_step_param_get_radio_channel_stats\n");
    test_step_params_t *step = this;
    step->is_step_initialized = true;
    step->u.wifi_stats_get = new (std::nothrow) wifi_stats_get_t;
    if (step->u.wifi_stats_get == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: allocation of memory for failed for %d\n",
            __func__, __LINE__, step->step_number);
        step->is_step_initialized = false;
        return;
    }
    memset(step->u.wifi_stats_get, 0, sizeof(wifi_stats_get_t));
    step->execution_time = 15;
    step->timeout_count = 0;
    step->capture_frames = false;
    pthread_mutex_init(&step->s_lock, NULL);
}

test_step_param_get_radio_channel_stats::~test_step_param_get_radio_channel_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg,
        "test_step_param_get_radio_channel_stats::~test_step_param_get_radio_channel_stats\n");
}

test_step_param_get_neighbor_stats::test_step_param_get_neighbor_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg,
        "test_step_param_get_neighbor_stats::test_step_param_get_neighbor_stats\n");
    test_step_params_t *step = this;
    step->is_step_initialized = true;
    step->u.wifi_stats_get = new (std::nothrow) wifi_stats_get_t;
    if (step->u.wifi_stats_get == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: allocation of memory for failed for %d\n",
            __func__, __LINE__, step->step_number);
        step->is_step_initialized = false;
        return;
    }
    memset(step->u.wifi_stats_get, 0, sizeof(wifi_stats_get_t));
    step->execution_time = 15;
    step->timeout_count = 0;
    step->capture_frames = false;
    pthread_mutex_init(&step->s_lock, NULL);
}

test_step_param_get_neighbor_stats::~test_step_param_get_neighbor_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg,
        "test_step_param_get_neighbor_stats::~test_step_param_get_neighbor_stats\n");
}

test_step_param_get_assoc_clients_stats::test_step_param_get_assoc_clients_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg,
        "test_step_param_get_assoc_clients_stats::test_step_param_get_assoc_clients_stats\n");
    test_step_params_t *step = this;
    step->is_step_initialized = true;
    step->u.wifi_stats_get = new (std::nothrow) wifi_stats_get_t;
    if (step->u.wifi_stats_get == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: allocation of memory for failed for %d\n",
            __func__, __LINE__, step->step_number);
        step->is_step_initialized = false;
        return;
    }
    memset(step->u.wifi_stats_get, 0, sizeof(wifi_stats_get_t));
    step->execution_time = 15;
    step->timeout_count = 0;
    step->capture_frames = false;
    pthread_mutex_init(&step->s_lock, NULL);
}

test_step_param_get_assoc_clients_stats::~test_step_param_get_assoc_clients_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg,
        "test_step_param_get_assoc_clients_stats::~test_step_param_get_assoc_clients_stats\n");
}

test_step_param_get_radio_diag_stats::test_step_param_get_radio_diag_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg,
        "test_step_param_get_radio_diag_stats::test_step_param_get_radio_diag_stats\n");
    test_step_params_t *step = this;
    step->is_step_initialized = true;
    step->u.wifi_stats_get = new (std::nothrow) wifi_stats_get_t;
    if (step->u.wifi_stats_get == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: allocation of memory for failed for %d\n",
            __func__, __LINE__, step->step_number);
        step->is_step_initialized = false;
        return;
    }
    memset(step->u.wifi_stats_get, 0, sizeof(wifi_stats_get_t));
    step->execution_time = 15;
    step->timeout_count = 0;
    step->capture_frames = false;
    pthread_mutex_init(&step->s_lock, NULL);
}

test_step_param_get_radio_diag_stats::~test_step_param_get_radio_diag_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg,
        "test_step_param_get_radio_diag_stats::~test_step_param_get_radio_diag_stats\n");
}

test_step_param_get_radio_temperature_stats::test_step_param_get_radio_temperature_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg,
        "test_step_param_get_radio_temperature_stats::test_step_param_get_radio_temperature_"
        "stats\n");
    test_step_params_t *step = this;
    step->is_step_initialized = true;
    step->u.wifi_stats_get = new (std::nothrow) wifi_stats_get_t;
    if (step->u.wifi_stats_get == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: allocation of memory for failed for %d\n",
            __func__, __LINE__, step->step_number);
        step->is_step_initialized = false;
        return;
    }
    memset(step->u.wifi_stats_get, 0, sizeof(wifi_stats_get_t));

    step->execution_time = 15;
    step->timeout_count = 0;
    step->capture_frames = false;
    pthread_mutex_init(&step->s_lock, NULL);
}

test_step_param_get_radio_temperature_stats::~test_step_param_get_radio_temperature_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg,
        "test_step_param_get_radio_temperature_stats::~test_step_param_get_radio_temperature_"
        "stats\n");
}
