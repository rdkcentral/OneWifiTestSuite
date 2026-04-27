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

int wlan_ext_test_step_param_iperf_client::wlan_ext_step_execute()
{
    wlan_ext_test_step_params_t *step = this;
    char timestamp[24] = { 0 };
    wlan_ext_test_step_params_t *serv_init_step = NULL;
    wlan_ext_emu_t *ext_emu = get_ext_emu();
    std::string iperf_client_cmd;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step_number : %d\n", __func__, __LINE__,
        step->step_number);

    if (step->u.iperf_client->input_operation == iperf_operation_type_start) {

        if (get_current_time_string(timestamp, sizeof(timestamp)) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: get_current_time_string failed\n",
                __func__, __LINE__);
            step->step_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }

        snprintf(step->u.iperf_client->u.start_conf.result_file,
            sizeof(step->u.iperf_client->u.start_conf.result_file), "/tmp/cci_res/%s_%d_%s_%s.txt",
            step->test_case_id, step->step_number, timestamp,
            step->u.iperf_client->u.start_conf.input_filename);

        iperf_client_cmd = std::string("/usr/bin/iperf3 ") +
            step->u.iperf_client->u.start_conf.cmd_options + std::string(" --logfile ") +
            std::string(step->u.iperf_client->u.start_conf.result_file);

        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step number : %d iperf client cmd : %s\n",
            __func__, __LINE__, step->step_number, iperf_client_cmd.c_str());

        if (step->u.iperf_client->interface_type == interface_type_ethernet) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: In ethernet namespace\n", __func__,
                __LINE__);
            // list_interfaces_in_namespace();
            ext_emu->enter_namespace("/var/run/netns/ots");
            // list_interfaces_in_namespace();
        }

        int result = execute_process_once(iperf_client_cmd,
            &step->u.iperf_client->u.start_conf.iperf_client_pid, false);
        if (result != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: step number : %d iperf client cmd : %s failed\n", __func__, __LINE__,
                step->step_number, step->u.iperf_client->u.start_conf.cmd_options);
            if (step->u.iperf_client->interface_type == interface_type_ethernet) {
                ext_emu->leave_namespace();
            }
            return RETURN_ERR;
        }
        if (step->u.iperf_client->interface_type == interface_type_ethernet) {
            ext_emu->leave_namespace();
            // list_interfaces_in_namespace();
        }
        cJSON_AddItemToArray(step->artifact_json_list,
            cJSON_CreateString(step->u.iperf_client->u.start_conf.result_file));
        wlan_emu_print(wlan_emu_log_level_dbg,
            "%s:%d: step number : %d iperf client pid : %d cmd : %s\n", __func__, __LINE__,
            step->step_number, step->u.iperf_client->u.start_conf.iperf_client_pid,
            iperf_client_cmd.c_str());

        if (step->fork == false) {
            step->step_state = wlan_emu_tests_state_cmd_continue;
        } else {
            step->step_state = wlan_emu_tests_state_cmd_wait;
        }
    } else if (step->u.iperf_client->input_operation == iperf_operation_type_stop) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step number : %d stop step number : %d\n",
            __func__, __LINE__, step->step_number,
            step->u.iperf_client->u.stop_conf.stop_step_number);

        serv_init_step = ext_emu->get_ext_step_from_step_number(
            step->u.iperf_client->u.stop_conf.stop_step_number);
        if (serv_init_step == NULL) {
            // spawn killall iperf3 process
            std::string iperf_kill_cmd = std::string("killall ") + std::string("/usr/bin/iperf3");
            if (step->u.iperf_client->interface_type == interface_type_ethernet) {
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: In ethernet namespace\n", __func__,
                    __LINE__);
                ext_emu->enter_namespace("/var/run/netns/ots");
            }
            int result = execute_process_once(iperf_kill_cmd,
                &step->u.iperf_client->u.start_conf.iperf_client_pid, false);
            if (result != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: step number : %d iperf kill cmd : %s failed\n", __func__, __LINE__,
                    step->step_number, iperf_kill_cmd.c_str());
                step->step_state = wlan_emu_tests_state_cmd_abort;
                if (step->u.iperf_client->interface_type == interface_type_ethernet) {
                    ext_emu->leave_namespace();
                }
                return RETURN_ERR;
            }
            if (step->u.iperf_client->interface_type == interface_type_ethernet) {
                ext_emu->leave_namespace();
            }
            step->step_state = wlan_emu_tests_state_cmd_results;
            return RETURN_OK;
        }
        wlan_emu_print(wlan_emu_log_level_dbg,
            "%s:%d: step number : %d stop step number : %d pid to stop : %d\n", __func__, __LINE__,
            step->step_number, step->u.iperf_client->u.stop_conf.stop_step_number,
            serv_init_step->u.iperf_client->u.start_conf.iperf_client_pid);

        if (kill(serv_init_step->u.iperf_client->u.start_conf.iperf_client_pid, SIGTERM) == 0) {
            wlan_emu_print(wlan_emu_log_level_info,
                "%s:%d: step number : %d iperf process pid : %d terminate succesful\n", __func__,
                __LINE__, step->step_number,
                serv_init_step->u.iperf_client->u.start_conf.iperf_client_pid);
        } else {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: step number : %d iperf process pid : %d terminate not succesful\n",
                __func__, __LINE__, step->step_number,
                serv_init_step->u.iperf_client->u.start_conf.iperf_client_pid);
        }
        waitpid(serv_init_step->u.iperf_client->u.start_conf.iperf_client_pid, NULL, 0);
        serv_init_step->step_state = wlan_emu_tests_state_cmd_results;
        step->step_state = wlan_emu_tests_state_cmd_results;
    }

    return RETURN_OK;
}

int wlan_ext_test_step_param_iperf_client::wlan_ext_step_timeout()
{
    wlan_ext_test_step_params_t *step = this;

    step->timeout_count++;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Test Step Num : %d timeout_count : %d\n",
        __func__, __LINE__, step->step_number, step->timeout_count);

    if (step->u.iperf_client->input_operation == iperf_operation_type_start) {
        if (is_process_running(step->u.iperf_client->u.start_conf.iperf_client_pid) == true) {
            if (step->fork == false) {
                step->step_state = wlan_emu_tests_state_cmd_continue;
            } else {
                step->step_state = wlan_emu_tests_state_cmd_wait;
            }
        } else {
            step->step_state = wlan_emu_tests_state_cmd_results;
        }

    } else if (step->u.iperf_client->input_operation == iperf_operation_type_stop) {
        step->step_state = wlan_emu_tests_state_cmd_results;
    }

    return RETURN_OK;
}

void wlan_ext_test_step_param_iperf_client::wlan_ext_step_remove()
{
    wlan_ext_test_step_params_t *step = this;
#if 0
    wlan_ext_test_step_params_t *step = this;
    wlan_ext_emu_sta_mgr_t *m_sta_mgr = get_agent_sta_mgr();
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step number : %d\n", __func__, __LINE__,
            step->step_number);
    if (step->u.sta_test->is_station_associated == true) {
        if (kill(step->u.sta_test->dhcp_pid, SIGTERM) == 0) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step number : %d intf : %s kill success dhcp pid : %d\n",
                    __func__, __LINE__, step->step_number, step->u.sta_test->sta_interface_name.c_str(),
                    step->u.sta_test->dhcp_pid);
        } else {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step number : %d intf : %s kill failed for dhcp pid : %d\n",
                    __func__, __LINE__, step->step_number, step->u.sta_test->sta_interface_name.c_str(),
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

#endif
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step_number : %d\n", __func__, __LINE__,
        step->step_number);
    cJSON_free(step->artifact_json_list);
    delete step;
    step = NULL;
}

int wlan_ext_test_step_param_iperf_client::wlan_ext_step_frame_filter(wlan_emu_msg_t *msg)
{
    wlan_ext_test_step_params_t *step = this;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: unhandled frame for step number : %d\n",
        __func__, __LINE__, step->step_number);

    return RETURN_UNHANDLED;
}

void wlan_ext_test_step_param_iperf_client::wlan_ext_step_status_update(cJSON *agent_status)
{
    char *json_str;
    wlan_ext_test_step_params_t *step = this;
    cJSON *step_private = NULL;
    step->update_step_status_json(agent_status, step_private);
}

int wlan_ext_test_step_param_iperf_client::wlan_ext_step_decode_client_start(cJSON *root_json,
    wlan_ext_test_step_params_t *step)
{
    cJSON *param = NULL;
    iperf_client_t *test_params = step->u.iperf_client;

    decode_param_string(root_json, "IperfClientCmd", param);
    copy_string(test_params->u.start_conf.cmd_options, param->valuestring,
        sizeof(test_params->u.start_conf.cmd_options) - 1);

    decode_param_string(root_json, "InputLogFileName", param);
    copy_string(test_params->u.start_conf.input_filename, param->valuestring,
        sizeof(test_params->u.start_conf.input_filename) - 1);

    decode_param_integer(root_json, "InterfaceType", param);
    test_params->interface_type = static_cast<interface_type_t>(param->valuedouble);

    return RETURN_OK;
}

int wlan_ext_test_step_param_iperf_client::wlan_ext_step_decode_client_stop(cJSON *root_json,
    wlan_ext_test_step_params_t *step)
{
    cJSON *param = NULL;
    iperf_client_t *test_params = step->u.iperf_client;

    decode_param_integer(root_json, "StopClientStepNumber", param);
    test_params->u.stop_conf.stop_step_number = param->valuedouble;

    return RETURN_OK;
}

int wlan_ext_test_step_param_iperf_client::wlan_ext_step_decode_subdocument(cJSON *root_json,
    wlan_ext_test_step_params_t *step)
{
    cJSON *param = NULL;
    iperf_client_t *test_params = step->u.iperf_client;

    decode_param_integer(root_json, "IperfClientOperation", param);
    test_params->input_operation = (iperf_operation_type_t)param->valuedouble;

    if (test_params->input_operation == iperf_operation_type_start) {
        if (wlan_ext_step_decode_client_start(root_json, step) == RETURN_ERR) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: decoder client start failed\n", __func__,
                __LINE__);
            return RETURN_ERR;
        }
    } else if (test_params->input_operation == iperf_operation_type_stop) {
        if (wlan_ext_step_decode_client_stop(root_json, step) == RETURN_ERR) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: decoder client stop failed\n", __func__,
                __LINE__);
            return RETURN_ERR;
        }
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid input operation\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    return RETURN_OK;
}

wlan_ext_test_step_param_iperf_client::wlan_ext_test_step_param_iperf_client()
{
    wlan_ext_test_step_params_t *step = this;
    step->is_step_initialized = true;

    step->u.iperf_client = new (std::nothrow) iperf_client_t;
    if (step->u.iperf_client == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: allocation of memory for log failed for %d\n", __func__, __LINE__,
            step->step_number);
        step->is_step_initialized = false;
        return;
    }
    memset(step->u.iperf_client, 0, sizeof(iperf_client_t));
    step->artifact_json_list = cJSON_CreateArray();
    step->execution_time = 5;
    step->timeout_count = 0;
    step->capture_frames = false;
}

wlan_ext_test_step_param_iperf_client::~wlan_ext_test_step_param_iperf_client()
{
}
