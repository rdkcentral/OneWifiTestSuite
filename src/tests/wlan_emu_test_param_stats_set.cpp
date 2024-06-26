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

int test_step_param_set_stats_t::step_frame_filter(wlan_emu_msg_t *msg)
{
    test_step_params_t *step = this;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step number : %d\n", __func__, __LINE__, step->step_number);
    if (msg == NULL) {
        return RETURN_UNHANDLED;
    }
    switch (msg->get_msg_type()) {
        case wlan_emu_msg_type_webconfig://onewifi_webconfig
        case wlan_emu_msg_type_cfg80211: //beacon
        case wlan_emu_msg_type_frm80211: //mgmt
        default:
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Not supported msg_type : %d\n", __func__, __LINE__, msg->get_msg_type());
        break;
    }
    return RETURN_UNHANDLED;
}

int test_step_param_set_radio_channel_stats::webconfig_stats_set_execute()
{
    test_step_params_t *step = this;
    
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d - test_step_param_set_radio_channel_stats\n", __func__, __LINE__);
    return RETURN_OK;
}

int test_step_param_set_neighbor_stats::webconfig_stats_set_execute()
{
    test_step_params_t *step = this;
    
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d - test_step_param_set_neighbor_stats\n", __func__, __LINE__);
    return RETURN_OK;
}

int test_step_param_set_assoc_clients_stats::webconfig_stats_set_execute()
{
    test_step_params_t *step = this;
    
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d - test_step_param_set_assoc_clients_stats\n", __func__, __LINE__);
    return RETURN_OK;
}

int test_step_param_set_radio_diag_stats::webconfig_stats_set_execute()
{
    test_step_params_t *step = this;
    
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d - test_step_param_set_radio_diag_stats\n", __func__, __LINE__);
    return RETURN_OK;
}

int test_step_param_set_radio_temperature_stats::webconfig_stats_set_execute()
{
    test_step_params_t *step = this;
    
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d - test_step_param_set_radio_temperature_stats\n", __func__, __LINE__);
    return RETURN_OK;
}

int test_step_param_set_stats_t::webconfig_stats_set_instance()
{
    test_step_params_t *step = this;
    int ret = 0;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);

    if (step->u.wifi_stats_set->stats_set_q != NULL) {
        if ((queue_count(step->u.wifi_stats_set->stats_set_q) > 0) && (step->u.wifi_stats_set->current_stats_set_count >= 0)) {
            stat_set_config_t *reference = (stat_set_config_t *)queue_peek(step->u.wifi_stats_set->stats_set_q, step->u.wifi_stats_set->current_stats_set_count);
            if (reference == NULL) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: queue_peek failed\n", __func__, __LINE__);
                step->test_state = wlan_emu_tests_state_cmd_abort;
                return RETURN_ERR;
            }
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: File name : %s\n", __func__, __LINE__, reference->input_file_json);

            ret = webconfig_stats_set_execute();
            if (ret != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: webconfig_stats_set_execute failed\n", __func__, __LINE__);
                step->test_state = wlan_emu_tests_state_cmd_abort;
                return RETURN_ERR;
            } else {
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: webconfig_stats_set_execute success\n", __func__, __LINE__);
            }

            if (step->u.wifi_stats_set->set_exec_duration == reference->stats_duration) {
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Moving to next instance\n", __func__, __LINE__);
                stat_set_config_t *reference = (stat_set_config_t *)queue_pop(step->u.wifi_stats_set->stats_set_q);
                if (reference == NULL) {
                    wlan_emu_print(wlan_emu_log_level_err, "%s:%d: queue_pop failed\n", __func__, __LINE__);
                    step->test_state = wlan_emu_tests_state_cmd_abort;
                    return RETURN_ERR;
                }
                int count = queue_count(step->u.wifi_stats_set->stats_set_q);
                step->u.wifi_stats_set->current_stats_set_count = count - 1;
                step->u.wifi_stats_set->set_exec_duration = 0;
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Next instance for step number : %d\n", __func__, __LINE__, step->step_number);
            }
        }
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: stats_set_q is NULL\n", __func__, __LINE__);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    return RETURN_OK;
}

int test_step_param_set_stats_t::step_execute()
{
    test_step_params_t *step = this;
    int ret = 0;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);

    if (step == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: step is NULL\n", __func__, __LINE__);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    if (step->u.wifi_stats_set == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: wifi_stats_set is NULL\n", __func__, __LINE__);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }
    
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Executing step %d\n", __func__, __LINE__, step->step_number);

    if (step->u.wifi_stats_set->stats_set_q != NULL) {
        int count = queue_count(step->u.wifi_stats_set->stats_set_q);
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: queue_count or total instances for step number %d : %d\n", __func__, __LINE__, step->step_number, count);
        step->u.wifi_stats_set->current_stats_set_count = count - 1;
        step->execution_time = step->u.wifi_stats_set->total_duration;
        step->u.wifi_stats_set->set_exec_duration = 0;
        if ((queue_count(step->u.wifi_stats_set->stats_set_q) > 0) && (step->u.wifi_stats_set->current_stats_set_count >= 0)) {
            stat_set_config_t *reference = (stat_set_config_t *)queue_peek(step->u.wifi_stats_set->stats_set_q, step->u.wifi_stats_set->current_stats_set_count);
            if (reference == NULL) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: queue_peek failed\n", __func__, __LINE__);
                step->test_state = wlan_emu_tests_state_cmd_abort;
                return RETURN_ERR;
            }
            if (step->u.wifi_stats_set->set_exec_duration < reference->stats_duration) {
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: First instance should wait for the duration before webconfig_decode & hal api trigger\n", __func__, __LINE__);
                return RETURN_OK; //First instance should wait for the duration before webconfig_decode & hal api trigger
            }
        }
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: stats_set_q is NULL\n", __func__, __LINE__);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    return RETURN_OK;
}

int test_step_param_set_stats_t::step_timeout()
{
    test_step_params_t *step = this;
    int ret = 0;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);

    if (step->u.wifi_stats_set->stats_set_q != NULL) {
        if ((queue_count(step->u.wifi_stats_set->stats_set_q) > 0) && (step->u.wifi_stats_set->current_stats_set_count >= 0)) {
            stat_set_config_t *reference = (stat_set_config_t *)queue_peek(step->u.wifi_stats_set->stats_set_q, step->u.wifi_stats_set->current_stats_set_count);
            if (reference == NULL) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: queue_peek failed\n", __func__, __LINE__);
                step->test_state = wlan_emu_tests_state_cmd_abort;
                return RETURN_ERR;
            }
            if (step->timeout_count < step->execution_time) {
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Timeout count for step number : %d is -> %d\n", __func__, __LINE__, step->step_number, step->timeout_count);
                step->timeout_count++;
                if (step->u.wifi_stats_set->set_exec_duration < reference->stats_duration) {
                    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Duration for current instance: %d\n", __func__, __LINE__, step->u.wifi_stats_set->set_exec_duration);
                    step->u.wifi_stats_set->set_exec_duration++;
                    step->test_state = wlan_emu_tests_state_cmd_continue;
                    return RETURN_OK;
                }
            }
            if (step->u.wifi_stats_set->set_exec_duration == reference->stats_duration) {
                    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Timeout reached stats duration of -> %d for current instance\n", __func__, __LINE__, reference->stats_duration); 
                    ret = webconfig_stats_set_instance();
                    if (ret != RETURN_OK) {
                        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: webconfig_stats_set_instance failed\n", __func__, __LINE__);
                        step->test_state = wlan_emu_tests_state_cmd_abort;
                        return RETURN_ERR;
                    } else {
                        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: webconfig_stats_set_instance success\n", __func__, __LINE__);
                    }
                    step->u.wifi_stats_set->set_exec_duration++;
                    step->test_state = wlan_emu_tests_state_cmd_continue;
                    return RETURN_OK;
            }
        }
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: stats_set_q is NULL\n", __func__, __LINE__);
    }

    if (step->timeout_count == step->execution_time) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Timeout for step number : %d is reached - %d\n", __func__, __LINE__, step->step_number, step->timeout_count);
        step->test_state = wlan_emu_tests_state_cmd_results;
        return RETURN_OK;
    }

    if (step->timeout_count > step->execution_time) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Timeout for step number : %d Exceeded - %d\n", __func__, __LINE__, step->step_number, step->timeout_count);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    return RETURN_OK;

}

int test_step_param_set_stats_t::step_upload_files(FILE* output_file, bool *update_to_tda)
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);
    return RETURN_OK;
}

void test_step_param_set_stats_t::step_remove()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);
    test_step_param_set_radio_channel_stats *step = dynamic_cast<test_step_param_set_radio_channel_stats *>(this);

    if (step != NULL) {
        if (step->u.wifi_stats_set->stats_set_q != NULL) {
            queue_destroy(step->u.wifi_stats_set->stats_set_q);
            step->u.wifi_stats_set->stats_set_q = NULL;
        }
    }

    delete step;
    step = NULL;
    return;
}

test_step_param_set_radio_channel_stats::test_step_param_set_radio_channel_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);
    test_step_params_t *step = this;
    step->execution_time = 0;
    step->timeout_count = 0;
    step->test_results_queue = NULL;
    step->capture_frames = false;
}

test_step_param_set_radio_channel_stats::~test_step_param_set_radio_channel_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);
}

test_step_param_set_neighbor_stats::test_step_param_set_neighbor_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);
    test_step_params_t *step = this;
    step->execution_time = 0;
    step->timeout_count = 0;
    step->test_results_queue = NULL;
    step->capture_frames = false;
}

test_step_param_set_neighbor_stats::~test_step_param_set_neighbor_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);
}

test_step_param_set_assoc_clients_stats::test_step_param_set_assoc_clients_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);
    test_step_params_t *step = this;
    step->execution_time = 0;
    step->timeout_count = 0;
    step->test_results_queue = NULL;
    step->capture_frames = false;
}

test_step_param_set_assoc_clients_stats::~test_step_param_set_assoc_clients_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);
}

test_step_param_set_radio_diag_stats::test_step_param_set_radio_diag_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);
    test_step_params_t *step = this;
    step->execution_time = 0;
    step->timeout_count = 0;
    step->test_results_queue = NULL;
    step->capture_frames = false;
}

test_step_param_set_radio_diag_stats::~test_step_param_set_radio_diag_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);
}

test_step_param_set_radio_temperature_stats::test_step_param_set_radio_temperature_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);
    test_step_params_t *step = this;
    step->execution_time = 0;
    step->timeout_count = 0;
    step->test_results_queue = NULL;
    step->capture_frames = false;
}

test_step_param_set_radio_temperature_stats::~test_step_param_set_radio_temperature_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);
}