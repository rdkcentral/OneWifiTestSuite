#include <assert.h>
#include "wlan_emu_log.h"
#include "wlan_emu_test_params.h"
#include <rbus.h>
#include "wifi_base.h"
#include "wlan_emu_common.h"
#include <fcntl.h>
#include <errno.h>
#include <cjson/cJSON.h>
#include <string>
#include <sstream>

char* test_step_param_get_stats_t::getStatsClass()
{
    test_step_params_t* step = this;
    switch (step->u.wifi_stats_get->data_type)
    {
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
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Unknown data type %d\n", __func__, __LINE__, step->u.wifi_stats_get->data_type);
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return nullptr;
    }
}

char* test_step_param_get_stats_t::get_scanmode()
{
    test_step_params_t* step = this;

    switch (step->u.wifi_stats_get->scan_mode)
    {
        case wifi_neighborScanMode_t::WIFI_RADIO_SCAN_MODE_ONCHAN:
            return "on_channel";
        case wifi_neighborScanMode_t::WIFI_RADIO_SCAN_MODE_OFFCHAN:
            return "off_channel";
        case wifi_neighborScanMode_t::WIFI_RADIO_SCAN_MODE_FULL:
            return "full_channel";
        case wifi_neighborScanMode_t::WIFI_RADIO_SCAN_MODE_SURVEY:
            return "Survey";
        default:
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Unknown scan mode %d\n", __func__, __LINE__, step->u.wifi_stats_get->scan_mode);
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return nullptr;
    }
}

int test_step_param_get_stats_t::get_subscription_string(char *str, int str_len)
{
    test_step_params_t* step = this;
    char *scan_mode = get_scanmode();
    switch (step->u.wifi_stats_get->data_type)
    {
        case wifi_mon_stats_type_t::mon_stats_type_radio_channel_stats:
            if (get_scanmode() != nullptr) {
                snprintf(str, str_len, "Device.WiFi.CollectStats.Radio.%d.ScanMode.%s.ChannelStats", step->u.wifi_stats_get->radio_index, scan_mode);
                return RETURN_OK;
            }
            break;
        case wifi_mon_stats_type_t::mon_stats_type_neighbor_stats:
            if (get_scanmode() != nullptr) {
                snprintf(str, str_len, "Device.WiFi.CollectStats.AccessPoint.%d.ScanMode.%s.NeighborStats", step->u.wifi_stats_get->vap_index, scan_mode);
                return RETURN_OK;
            }
            break;
        case wifi_mon_stats_type_t::mon_stats_type_associated_device_stats:
            snprintf(str, str_len, "Device.WiFi.CollectStats.AccessPoint.%d.AssociatedDeviceStats", step->u.wifi_stats_get->vap_index);
            return RETURN_OK;
        case wifi_mon_stats_type_t::mon_stats_type_radio_diagnostic_stats:
            snprintf(str, str_len, "Device.WiFi.CollectStats.Radio.%d.RadioDiagnosticStats", step->u.wifi_stats_get->radio_index);
            return RETURN_OK;
        case wifi_mon_stats_type_t::mon_stats_type_radio_temperature:
            snprintf(str, str_len, "Device.WiFi.CollectStats.Radio.%d.RadioTemperature", step->u.wifi_stats_get->radio_index);
            return RETURN_OK;
        break;
        default:
        break;
    }
    return RETURN_ERR;
}

void test_step_param_get_stats_t::stats_get_event_handler(rbusHandle_t handle, rbusEvent_t const* event, rbusEventSubscription_t* subscription)
{
    const char* event_name = event->name;
    rbusValue_t value;
    const char* json_str;
    int len = 0;
    test_step_params_t *step;
    unsigned int count =0;
    char file_name[128] = {0};
    char *temp_file_name;
    FILE *fp = NULL;

    wlan_emu_print(wlan_emu_log_level_dbg,"%s: %d rbus event callback Event is %s \n", __func__, __LINE__, event_name);

    value = rbusObject_GetValue(event->data, NULL );
    if(!value) {
        wlan_emu_print(wlan_emu_log_level_err,"%s FAIL: value is NULL\n",__FUNCTION__);
        return;
    }
    json_str = rbusValue_GetString(value, &len);
    if (json_str== NULL) {
        wlan_emu_print(wlan_emu_log_level_err,"%s Null pointer,Rbus get string len=%d\n",__FUNCTION__,len);
        return;
    }
    step = static_cast<test_step_params_t *> (subscription->userData);

    pthread_mutex_lock(&step->s_lock);
    count = queue_count(step->u.wifi_stats_get->get_stats_queue);

    snprintf(file_name, sizeof(file_name), "/tmp/cci_res/%s_%d.json", step->u.wifi_stats_get->output_file_name, count);

    fp = fopen(file_name, "w");
    if (fp == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: fopen failed for %s\n",
                __func__, __LINE__, file_name);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        pthread_mutex_unlock(&step->s_lock);
        return;
    }
    fputs((const char*)json_str, fp);

    fclose(fp);
    temp_file_name = strdup(file_name);
    queue_push(step->u.wifi_stats_get->get_stats_queue, temp_file_name);

    pthread_mutex_unlock(&step->s_lock);

}

int test_step_param_get_stats_t::start_subscription()
{
    char subscription[128] = {0};
    int index = 0;
    int rc = RBUS_ERROR_SUCCESS;
    test_step_params_t *step = this;
    webconfig_cci_t  *cci_webconfig;
    cci_webconfig = step->m_ui_mgr->get_webconfig_data();

    if (get_subscription_string(subscription, sizeof(subscription)) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Get Subscription string failed for step : %d \n",
                __func__, __LINE__, step->step_number);
        return RETURN_ERR;
    }
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Subscription string : %s\n", __func__, __LINE__, subscription);

    rc = rbusEvent_Subscribe(cci_webconfig->rbus_handle, subscription, test_step_param_get_stats_t::stats_get_event_handler, this, 0);
    if (rc != RBUS_ERROR_SUCCESS) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Subscription failed for string : %s for step : %d\n", __func__, __LINE__, subscription, step->step_number);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    } else {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Successfully subscribed to %s\n", __func__, __LINE__, subscription);
    }
    return RETURN_OK;
}

int test_step_param_get_stats_t::stop_subscription(test_step_params_t *step)
{
    char subscription[128] = {0};
    int index = 0;
    int rc = RBUS_ERROR_SUCCESS;
    webconfig_cci_t  *cci_webconfig;
    cci_webconfig = step->m_ui_mgr->get_webconfig_data();
  
    if (get_subscription_string(subscription, sizeof(subscription)) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Get UnSubscription string failed for step : %d \n",
                __func__, __LINE__, step->step_number);
        return RETURN_ERR;
    }
    wlan_emu_print(wlan_emu_log_level_dbg, " UnSubscription string : %s\n", subscription);
  
    rc = rbusEvent_Unsubscribe(cci_webconfig->rbus_handle, subscription);
    if (rc != RBUS_ERROR_SUCCESS) {
        wlan_emu_print(wlan_emu_log_level_err, "Failed to UnSubscribe to %s: %d\n", subscription, rc);
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
    rbusHandle_t handle;
    int rc = RBUS_ERROR_SUCCESS;
    test_step_params_t *step = this;
    test_step_params_t *step_to_be_stopped = NULL;
    int timeout = 0;
  
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: %s\n", __func__, __LINE__, getStatsClass());

    if (step == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: step is NULL\n",
                __func__, __LINE__);
        return RETURN_ERR;
    }

    if (step->u.wifi_stats_get == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: step->u.wifi_stats_get is NULL\n",
                __func__, __LINE__);
        return RETURN_ERR;
    }

    if (step->u.wifi_stats_get->log_operation == log_operation_type_t::log_operation_type_start) {
        if (start_subscription() == RETURN_ERR) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: start subscription failed for step : %d \n",
                    __func__, __LINE__, step->step_number);
            return RETURN_ERR;
        }
        step->test_state = wlan_emu_tests_state_cmd_wait;
    } else if (step->u.wifi_stats_get->log_operation == log_operation_type_t::log_operation_type_stop) {
        wlan_emu_test_case_config *test_case_config = (wlan_emu_test_case_config *)step->param_get_test_case_config();
        step_to_be_stopped = (test_step_params_t *)step->m_ui_mgr->get_step_from_step_number(test_case_config, step->u.wifi_stats_get->stop_log_step_number);
        if (step_to_be_stopped == NULL) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid log stop step  : %d in step : %d\n", __func__, __LINE__, step->u.wifi_stats_get->stop_log_step_number, step->step_number);
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }

        if (stop_subscription(step_to_be_stopped) == RETURN_ERR) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: start subscription failed for step : %d \n",
                    __func__, __LINE__, step->step_number);
            return RETURN_ERR;
        }
        step->test_state = wlan_emu_tests_state_cmd_results;
    } else if (step->u.wifi_stats_get->log_operation == log_operation_type_timer) {
        if (start_subscription() == RETURN_ERR) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: start subscription failed for step : %d \n",
                    __func__, __LINE__, step->step_number);
            return RETURN_ERR;
        }
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step->u.wifi_stats_get->timeout : %d\n", __func__, __LINE__, step->u.wifi_stats_get->timeout);
        step->execution_time = step->u.wifi_stats_get->timeout;
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: execution time : %d\n", __func__, __LINE__, step->execution_time);
        step->test_state = wlan_emu_tests_state_cmd_continue;
    }

    return RETURN_OK;
}

int test_step_param_get_stats_t::step_timeout()
{
    test_step_params_t *step = this;
    
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: %s\n", __func__, __LINE__, getStatsClass());

    if ((step->test_state != wlan_emu_tests_state_cmd_results) && (step->u.wifi_stats_get->log_operation == log_operation_type_timer))  {
        step->timeout_count++;
        if (step->execution_time == step->timeout_count) {
            stop_subscription(this);
            step->test_state = wlan_emu_tests_state_cmd_results;
            wlan_emu_print(wlan_emu_log_level_info, "%s:%d: Test duration of %d  completed for step %d\n",
                    __func__, __LINE__, step->execution_time, step->step_number);
            return RETURN_OK;
        }
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Test Step Num : %d execution_time : %d timeout_count %d\n",
                __func__, __LINE__, step->step_number, step->execution_time, step->timeout_count);
        step->test_state = wlan_emu_tests_state_cmd_continue;
    } else if ((step->u.wifi_stats_get->log_operation == log_operation_type_stop)) {
        step->test_state = wlan_emu_tests_state_cmd_results;
    } else if ((step->u.wifi_stats_get->log_operation == log_operation_type_start)) {
        step->test_state = wlan_emu_tests_state_cmd_results;
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: abort Test Step Num : %d execution_time : %d timeout_count %d\n",
                __func__, __LINE__, step->step_number, step->execution_time, step->timeout_count);
        step->test_state = wlan_emu_tests_state_cmd_abort;
    }
    return RETURN_OK;
}

int test_step_param_get_stats_t::step_upload_files(FILE* output_file, bool *update_to_tda)
{
    test_step_params_t *step = this;
    char *remote_test_results_loc = NULL;
    char *res_file_name;
    char *temp_res_file;
    
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: %s\n", __func__, __LINE__, getStatsClass());

    if (step->u.wifi_stats_get == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: abort Test Step Num : %d wifi_stats_get is NULL\n",
                __func__, __LINE__, step->step_number);
        return RETURN_ERR;
    }


    if (step->u.wifi_stats_get->log_operation == log_operation_type_stop) {
        wlan_emu_print(wlan_emu_log_level_info, "%s:%d: Nothing to upload for stop step\n",
                __func__, __LINE__, step->step_number);
        return RETURN_OK;
    }

    if (step->u.wifi_stats_get->get_stats_queue == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: abort Test Step Num : %d get_stats_queue is NULL\n",
                __func__, __LINE__, step->step_number);
        return RETURN_ERR;
    }
    remote_test_results_loc = step->m_ui_mgr->get_remote_test_results_loc();

    pthread_mutex_lock(&step->s_lock);
    res_file_name = (char *)queue_pop(step->u.wifi_stats_get->get_stats_queue);

    while(res_file_name != NULL) {
        if (res_file_name != NULL) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: File: %s\n", __func__, __LINE__, res_file_name);
            if (step->m_ui_mgr->upload_file_to_server(res_file_name, remote_test_results_loc) != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to upload %s\n", __func__, __LINE__, res_file_name);
                pthread_mutex_unlock(&step->s_lock);
                return RETURN_ERR;
            } else {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: uploaded %s\n", __func__, __LINE__, res_file_name);
                *update_to_tda = true;
                temp_res_file = strdup(res_file_name);
                if (step->m_ui_mgr->get_last_substring_after_slash(temp_res_file, res_file_name, 128) != RETURN_OK) {
                    wlan_emu_print(wlan_emu_log_level_err, "%s:%d: get_last_substring_after_slash failed for str : %s\n",
                            __func__, __LINE__, temp_res_file);
                    free(temp_res_file);
                    pthread_mutex_unlock(&step->s_lock);
                    return RETURN_ERR;
                }
                fprintf(output_file, "%s\n", res_file_name);
                free(temp_res_file);
            }
            free(res_file_name);
        }
        res_file_name = (char *)queue_pop(step->u.wifi_stats_get->get_stats_queue);
    }
    pthread_mutex_unlock(&step->s_lock);
    return RETURN_OK;
}

void test_step_param_get_stats_t::step_remove()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: %s\n", __func__, __LINE__, getStatsClass());
    test_step_params_t *step = dynamic_cast<test_step_params_t *>(this);
    pthread_mutex_destroy(&step->s_lock);
    if (step == NULL) {
        return;
    }
    delete step;
    step = NULL;
}

test_step_param_get_radio_channel_stats::test_step_param_get_radio_channel_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "test_step_param_get_radio_channel_stats::test_step_param_get_radio_channel_stats\n");
    test_step_params_t *step = this;
    step->execution_time = 15;
    step->timeout_count = 0;
    step->test_results_queue = NULL;
    step->capture_frames = false;
    pthread_mutex_init(&step->s_lock, NULL);
}
test_step_param_get_radio_channel_stats::~test_step_param_get_radio_channel_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "test_step_param_get_radio_channel_stats::~test_step_param_get_radio_channel_stats\n");
}
test_step_param_get_neighbor_stats::test_step_param_get_neighbor_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "test_step_param_get_neighbor_stats::test_step_param_get_neighbor_stats\n");
    test_step_params_t *step = this;
    step->execution_time = 15;
    step->timeout_count = 0;
    step->test_results_queue = NULL;
    step->capture_frames = false;
    pthread_mutex_init(&step->s_lock, NULL);
}
test_step_param_get_neighbor_stats::~test_step_param_get_neighbor_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "test_step_param_get_neighbor_stats::~test_step_param_get_neighbor_stats\n");
}
test_step_param_get_assoc_clients_stats::test_step_param_get_assoc_clients_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "test_step_param_get_assoc_clients_stats::test_step_param_get_assoc_clients_stats\n");
    test_step_params_t *step = this;
    step->execution_time = 15;
    step->timeout_count = 0;
    step->test_results_queue = NULL;
    step->capture_frames = false;
    pthread_mutex_init(&step->s_lock, NULL);
}
test_step_param_get_assoc_clients_stats::~test_step_param_get_assoc_clients_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "test_step_param_get_assoc_clients_stats::~test_step_param_get_assoc_clients_stats\n");
}
test_step_param_get_radio_diag_stats::test_step_param_get_radio_diag_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "test_step_param_get_radio_diag_stats::test_step_param_get_radio_diag_stats\n");
    test_step_params_t *step = this;
    step->execution_time = 15;
    step->timeout_count = 0;
    step->test_results_queue = NULL;
    step->capture_frames = false;
    pthread_mutex_init(&step->s_lock, NULL);
}
test_step_param_get_radio_diag_stats::~test_step_param_get_radio_diag_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "test_step_param_get_radio_diag_stats::~test_step_param_get_radio_diag_stats\n");
}
test_step_param_get_radio_temperature_stats::test_step_param_get_radio_temperature_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "test_step_param_get_radio_temperature_stats::test_step_param_get_radio_temperature_stats\n");
    test_step_params_t *step = this;
    step->execution_time = 15;
    step->timeout_count = 0;
    step->test_results_queue = NULL;
    step->capture_frames = false;
    pthread_mutex_init(&step->s_lock, NULL);
}
test_step_param_get_radio_temperature_stats::~test_step_param_get_radio_temperature_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "test_step_param_get_radio_temperature_stats::~test_step_param_get_radio_temperature_stats\n");
}