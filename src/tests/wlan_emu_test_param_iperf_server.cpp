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
#include "wlan_emu_test_params.h"
#include "wlan_emu_ext_sta_mgr.h"
#include "wlan_emu_ext_agent_interface.h"
#include "wlan_emu_err_code.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>

#define IPERF_STOP_STEP_NUMBER 7
static int external_sta_grace_timeout = 3;

int test_step_param_iperf_server::step_execute()
{
    test_step_params_t *step = this;
    //    test_step_params_t *server_interface_step = NULL;
    std::string agent_subdoc;

    if (step->u.iperf_server->input_operation == iperf_operation_type_stop) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: stop_step_number : %d\n", __func__,
            __LINE__, step->u.iperf_server->u.stop_conf.stop_step_number);

        if (encode_external_iperf_server_stop_subdoc(agent_subdoc) == RETURN_ERR) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: encode external iperf server failed for step : %d\n", __func__, __LINE__,
                step->step_number);
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }
    } else if (step->u.iperf_server->input_operation == iperf_operation_type_start) {
        wlan_emu_print(wlan_emu_log_level_dbg,
            "%s:%d: interface_step_number : %d ServerLogResultName : %s cmd_options : %s\n",
            __func__, __LINE__,
            step->u.iperf_server->u.start_conf.interface_step_number,
            step->u.iperf_server->u.start_conf.input_filename,
            step->u.iperf_server->u.start_conf.cmd_options);

        if (encode_external_iperf_server_start_subdoc(agent_subdoc) == RETURN_ERR) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: encode external iperf server failed for step : %d\n", __func__, __LINE__,
                step->step_number);
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }
    }

    if (step->fork == true) {
        step->test_state = wlan_emu_tests_state_cmd_wait;
    } else {
        step->test_state = wlan_emu_tests_state_cmd_continue;
    }

    return RETURN_OK;
}

int test_step_param_iperf_server::push_ext_iperf_server_result_files(
    const std::vector<std::string> &files)
{
    int ret;
    test_step_params_t *step = this;

    for (const std::string &file : files) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: result file: %s\n", __func__, __LINE__,
            file.c_str());

        ret = snprintf(step->u.iperf_server->u.start_conf.result_file,
            sizeof(step->u.iperf_server->u.start_conf.result_file), "%s", file.c_str());
        if (ret < 0 || ret >= sizeof(step->u.iperf_server->u.start_conf.result_file)) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed write file name\n", __func__,
                __LINE__);
            return RETURN_ERR;
        }
    }
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: result file is : %s\n", __func__, __LINE__,
        step->u.iperf_server->u.start_conf.result_file);
    if (step->u.iperf_server->input_operation != iperf_operation_type_stop) {
        if (step->m_ui_mgr->step_upload_files(
                step->u.iperf_server->u.start_conf.result_file) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: step_upload_files failed\n", __func__,
                __LINE__);
            step->m_ui_mgr->cci_error_code = EPUSHTSTRESFILE;
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }
    }

    return RETURN_OK;
}

int test_step_param_iperf_server::step_timeout()
{
    test_step_params_t *step = this;
    wlan_emu_ext_agent_interface_t *ext_agent;
    ext_agent_status_resp_t status = {};

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: sta test key: %s\n", __func__, __LINE__,
        step->u.iperf_server->sta_key.c_str());

    if (step->u.iperf_server->interface_type == interface_type_wifi) {

        ext_agent = step->m_ext_sta_mgr->get_ext_agent(
            (char *)step->u.iperf_server->sta_key.c_str());
        if (ext_agent == NULL) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: failed to find external agent for key: %s\n", __func__, __LINE__,
                step->u.iperf_server->sta_key.c_str());
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }

    } else if (step->u.iperf_server->interface_type == interface_type_ethernet) {
        ext_agent = step->m_ext_sta_mgr->get_ext_agent_from_eth_dev_mac(
            (char *)step->u.iperf_server->sta_key.c_str());
        if (ext_agent == NULL) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: failed to find external agent for key: %s\n", __func__, __LINE__,
                step->u.iperf_server->sta_key.c_str());
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }
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
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    if (status.state == ext_agent_test_state_fail) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: external agent state failed for %d\n",
            __func__, __LINE__, step->step_number);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    auto step_iter = status.steps.begin();
    for (; step_iter != status.steps.end(); ++step_iter) {
        wlan_emu_print(wlan_emu_log_level_dbg,
            "%s:%d: step_iter->step_number : %d step_state : %s agent_state : %s\n", __func__,
            __LINE__, step_iter->step_number, step_state_as_string(step_iter->state).c_str(),
            ext_agent->agent_state_as_string(status.state).c_str());
        if (step_iter->step_number == IPERF_STOP_STEP_NUMBER &&
            step_iter->state == wlan_emu_tests_state_cmd_results) {
            if (ext_agent->send_external_agent_stop_command() != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: failed to send external agent stop command\n", __func__, __LINE__);
            }
        }
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

        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: test state: %s step number: %d\n", __func__,
            __LINE__, step_state_as_string(step->test_state).c_str(), step_number);

        return RETURN_OK;
    }


    if (step->test_state == wlan_emu_tests_state_cmd_results) {

        if (step->u.iperf_server->input_operation == iperf_operation_type_stop) {
            return RETURN_OK;
        }

        if (ext_agent->download_external_agent_result_files(step_iter->result_files, step->m_ui_mgr->cci_error_code) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to download test results\n",
                __func__, __LINE__);
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }

        if (push_ext_iperf_server_result_files(step_iter->result_files) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to push test results\n", __func__,
                __LINE__);
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }
        return RETURN_OK;
    } else if (step->test_state == wlan_emu_tests_state_cmd_abort) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: abort step number: %d\n", __func__, __LINE__,
            step_number);
        return RETURN_ERR;
    } else {
        wlan_emu_print(wlan_emu_log_level_info, "%s:%d: test state: %s step number: %d\n", __func__,
            __LINE__, step_state_as_string(step->test_state).c_str(), step_number);
    }

    if (step->fork == true) {
        step->test_state = wlan_emu_tests_state_cmd_wait;
    } else {
        step->test_state = wlan_emu_tests_state_cmd_continue;
    }

    return RETURN_OK;
}

void test_step_param_iperf_server::step_remove()
{
    test_step_param_iperf_server *step = dynamic_cast<test_step_param_iperf_server *>(this);
    if (step == NULL) {
        return;
    }
    if (step->is_step_initialized == true) {
        delete step->u.iperf_server;
    }
    delete step;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Destructor for command called\n", __func__,
        __LINE__);
    step = NULL;

    return;
}

int test_step_param_iperf_server::step_frame_filter(wlan_emu_msg_t *msg)
{

    test_step_params_t *step = this;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: unhandled frame for step number : %d\n",
        __func__, __LINE__, step->step_number);

    return RETURN_UNHANDLED;
}

int test_step_param_iperf_server::encode_external_iperf_server_start_subdoc(
    std::string &agent_subdoc)
{
    cJSON *json = NULL;
    char *str = NULL;
    char iperf_config_json[256] = { 0 };
    test_step_params_t *step = this;
    test_step_params_t *server_interface_step = NULL;
    std::string server_cmd;
    sta_test_t *test_params;
    wlan_emu_ext_agent_interface_t *agent_info = NULL;
    std::string agent_url;
    FILE *fp;
    std::string sta_name;

    json = cJSON_CreateObject();
    if (json == NULL) {
        return RETURN_ERR;
    }

    wlan_emu_test_case_config *test_case_config =
        (wlan_emu_test_case_config *)step->param_get_test_case_config();

    server_interface_step = (test_step_params_t *)step->m_ui_mgr->get_step_from_step_number(
        test_case_config, step->u.iperf_server->u.start_conf.interface_step_number);
    if (server_interface_step == NULL) {
        step->m_ui_mgr->cci_error_code = ESTEPSTOPPED;
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: Invalid server interface step  : %d in step : %d\n", __func__, __LINE__,
            step->u.iperf_server->u.start_conf.interface_step_number, step->step_number);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }
    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: server interface step  : %d in step : %d\n",
        __func__, __LINE__, step->u.iperf_server->u.start_conf.interface_step_number,
        step->step_number);

    if (server_interface_step->param_type == step_param_type_station_management) {
        step->u.iperf_server->interface_type = interface_type_wifi;
        if (server_interface_step->u.sta_test == nullptr) {
            step->m_ui_mgr->cci_error_code = ESTEPSTOPPED;
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: NULL for server interface step  : %d in step : %d\n", __func__, __LINE__,
                step->u.iperf_server->u.start_conf.interface_step_number, step->step_number);
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }
        wlan_emu_print(wlan_emu_log_level_info,
            "%s:%d: server interface step  : %d sta_name : %s ip_address : %s sta key : %s\n",
            __func__, __LINE__, step->u.iperf_server->u.start_conf.interface_step_number,
            server_interface_step->u.sta_test->sta_name,
            server_interface_step->u.sta_test->ip_address.c_str(),
            server_interface_step->u.sta_test->key);
        server_cmd = std::string(step->u.iperf_server->u.start_conf.cmd_options) +
            std::string(" -s -B ") + server_interface_step->u.sta_test->ip_address;

        wlan_emu_print(wlan_emu_log_level_info,
            "%s:%d: server interface step  : %d server_cmd : %s\n", __func__, __LINE__,
            step->u.iperf_server->u.start_conf.interface_step_number, server_cmd.c_str());

        agent_info = server_interface_step->m_ext_sta_mgr->get_ext_agent(
            server_interface_step->u.sta_test->key);
        if (agent_info == NULL) {
            step->m_ui_mgr->cci_error_code = EEXTAGENT;
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: failed to find external agent for key: %s\n", __func__, __LINE__,
                server_interface_step->u.sta_test->key);
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }

        step->u.iperf_server->sta_key = std::string(server_interface_step->u.sta_test->key);
        sta_name = std::string(server_interface_step->u.sta_test->sta_name);
    } else if (server_interface_step->param_type == step_param_type_ethernet_lan_interface) {
        step->u.iperf_server->interface_type = interface_type_ethernet;
        if (server_interface_step->u.eth_lan_client == nullptr) {
            step->m_ui_mgr->cci_error_code = ESTEPSTOPPED;
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: NULL for server interface step  : %d in step : %d\n", __func__, __LINE__,
                step->u.iperf_server->u.start_conf.interface_step_number, step->step_number);
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }
        wlan_emu_print(wlan_emu_log_level_info,
            "%s:%d: server interface step  : %d sta_name : %s ip_address : %s sta key : %s\n",
            __func__, __LINE__, step->u.iperf_server->u.start_conf.interface_step_number,
            server_interface_step->u.eth_lan_client->eth_dev_info.interface_name.c_str(),
            server_interface_step->u.eth_lan_client->eth_dev_info.ip_address.c_str(),
            server_interface_step->u.eth_lan_client->key);
        server_cmd = std::string(step->u.iperf_server->u.start_conf.cmd_options) +
            std::string(" -s -B ") +
            server_interface_step->u.eth_lan_client->eth_dev_info.ip_address;
        wlan_emu_print(wlan_emu_log_level_info,
            "%s:%d: server interface step  : %d server_cmd : %s\n", __func__, __LINE__,
            step->u.iperf_server->u.start_conf.interface_step_number, server_cmd.c_str());

        agent_info = server_interface_step->m_ext_sta_mgr->get_ext_agent_from_eth_dev_mac(
            server_interface_step->u.eth_lan_client->key);
        if (agent_info == NULL) {
            step->m_ui_mgr->cci_error_code = EEXTAGENT;
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: failed to find external agent for key: %s\n", __func__, __LINE__,
                server_interface_step->u.eth_lan_client->key);
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }

        step->u.iperf_server->sta_key = std::string(server_interface_step->u.eth_lan_client->key);
        //	test_params = server_interface_step->u.sta_test;
        sta_name = server_interface_step->u.eth_lan_client->eth_dev_info.interface_name;

    } else {
        step->m_ui_mgr->cci_error_code = ESTEPSTOPPED;
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: Invalid server interface step  : %d in step : %d\n", __func__, __LINE__,
            step->u.iperf_server->u.start_conf.interface_step_number, step->step_number);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    agent_url = agent_info->get_agent_proto() + std::string(agent_info->agent_ip_address) +
        agent_info->get_agent_port() + std::string("/Testconfig/");

    cJSON_AddStringToObject(json, "SubDocName", "ExternalIperfServer");
    cJSON_AddNumberToObject(json, "StepNumber", step->step_number);
    cJSON_AddNumberToObject(json, "IperfServerOperation", step->u.iperf_server->input_operation);
    cJSON_AddBoolToObject(json, "Fork", step->fork);
    cJSON_AddStringToObject(json, "StationName", sta_name.c_str());
    cJSON_AddStringToObject(json, "TestCaseID", step->test_case_id);
    cJSON_AddStringToObject(json, "TestCaseName", step->test_case_name);
    cJSON_AddStringToObject(json, "IperfServerCmd", server_cmd.c_str());
    cJSON_AddStringToObject(json, "InputLogFileName",
        step->u.iperf_server->u.start_conf.input_filename);
    cJSON_AddNumberToObject(json, "InterfaceType", step->u.iperf_server->interface_type);

    str = cJSON_Print(json);

    if (str == nullptr) {
        step->m_ui_mgr->cci_error_code = EJSONPARSE;
        cJSON_Delete(json);
        return RETURN_ERR;
    }

    agent_subdoc = std::string(str);
    wlan_emu_print(wlan_emu_log_level_err, "%s:%d: iperf server : %s\n", __func__, __LINE__,
        agent_subdoc.c_str());

    snprintf(iperf_config_json, sizeof(iperf_config_json), "/tmp/cci_res/%s_%d.json",
        agent_info->agent_hostname.c_str(), step->step_number);

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: iperf_config_json : %s\n", __func__, __LINE__,
        iperf_config_json);

    if ((fp = fopen(iperf_config_json, "w")) == NULL) {
        step->m_ui_mgr->cci_error_code = EFOPEN;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: fopen failed for %s\n", __func__, __LINE__,
            iperf_config_json);
        return RETURN_ERR;
    }
    if (fwrite(agent_subdoc.c_str(), agent_subdoc.length(), 1, fp) != 1) {
        step->m_ui_mgr->cci_error_code = EFWRITE;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: fwrite failed\n", __func__, __LINE__);
        fclose(fp);
        return RETURN_ERR;
    }
    fclose(fp);
    long status_code;

    http_post_file(agent_url, iperf_config_json, status_code, step->m_ui_mgr->cci_error_code);
    if (status_code != http_status_code_ok) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: http_post_file failed : %d\n", __func__,
            __LINE__, status_code);
        return RETURN_ERR;
    }

    cJSON_free(str);
    cJSON_Delete(json);

    return RETURN_OK;
}

int test_step_param_iperf_server::encode_external_iperf_server_stop_subdoc(
    std::string &agent_subdoc)
{
    cJSON *json = NULL;
    test_step_params_t *step = this, *server_start_step = NULL, *server_interface_step = NULL;
    char *str = NULL;
    char iperf_config_json[256] = { 0 };
    wlan_emu_ext_agent_interface_t *agent_info = NULL;
    std::string agent_url;
    FILE *fp;

    wlan_emu_test_case_config *test_case_config =
        (wlan_emu_test_case_config *)step->param_get_test_case_config();

    // Get iperf_start_step
    server_start_step = (test_step_params_t *)step->m_ui_mgr->get_step_from_step_number(
        test_case_config, step->u.iperf_server->u.stop_conf.stop_step_number);
    if (server_start_step == NULL) {
        step->m_ui_mgr->cci_error_code = ESTEPSTOPPED;
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: Invalid server start step  : %d in step : %d\n", __func__, __LINE__,
            step->u.iperf_server->u.stop_conf.stop_step_number, step->step_number);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    // Get interface_step_number
    server_interface_step = (test_step_params_t *)step->m_ui_mgr->get_step_from_step_number(
        test_case_config, server_start_step->u.iperf_server->u.start_conf.interface_step_number);
    if (server_interface_step == NULL) {
        step->m_ui_mgr->cci_error_code = ESTEPSTOPPED;
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: Invalid server interface step  : %d in step : %d\n", __func__, __LINE__,
            server_start_step->u.iperf_server->u.start_conf.interface_step_number,
            step->step_number);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }
    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: server interface step  : %d in step : %d\n",
        __func__, __LINE__, server_start_step->u.iperf_server->u.start_conf.interface_step_number,
        step->step_number);

    if (server_interface_step->param_type == step_param_type_station_management) {
        step->u.iperf_server->interface_type = interface_type_wifi;
        if (server_interface_step->u.sta_test == nullptr) {
            step->m_ui_mgr->cci_error_code = ESTEPSTOPPED;
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: NULL for wifi server interface step in step : %d\n", __func__, __LINE__,
                step->step_number);
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }

        agent_info = server_interface_step->m_ext_sta_mgr->get_ext_agent(
            server_interface_step->u.sta_test->key);
        if (agent_info == NULL) {
            step->m_ui_mgr->cci_error_code = EEXTAGENT;
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: failed to find wifi external agent for key: %s\n", __func__, __LINE__,
                server_interface_step->u.sta_test->key);
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }

        step->u.iperf_server->sta_key = std::string(server_interface_step->u.sta_test->key);
    } else if (server_interface_step->param_type == step_param_type_ethernet_lan_interface) {
        step->u.iperf_server->interface_type = interface_type_ethernet;
        if (server_interface_step->u.eth_lan_client == nullptr) {
            step->m_ui_mgr->cci_error_code = ESTEPSTOPPED;
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: NULL for eth server interface step in step : %d\n", __func__, __LINE__,
                step->step_number);
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }

        agent_info = server_interface_step->m_ext_sta_mgr->get_ext_agent_from_eth_dev_mac(
            server_interface_step->u.eth_lan_client->key);
        if (agent_info == NULL) {
            step->m_ui_mgr->cci_error_code = EEXTAGENT;
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: failed to find eth external agent for key: %s\n", __func__, __LINE__,
                server_interface_step->u.eth_lan_client->key);
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }

        step->u.iperf_server->sta_key = std::string(server_interface_step->u.eth_lan_client->key);

    } else {
        step->m_ui_mgr->cci_error_code = ESTEPSTOPPED;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid server interface step type for %d\n",
            __func__, __LINE__, step->step_number);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    agent_url = agent_info->get_agent_proto() + std::string(agent_info->agent_ip_address) +
        agent_info->get_agent_port() + std::string("/Testconfig/");

    json = cJSON_CreateObject();
    if (json == NULL) {
        step->m_ui_mgr->cci_error_code = EJSONPARSE;
        return RETURN_ERR;
    }
    cJSON_AddStringToObject(json, "SubDocName", "ExternalIperfServer");
    cJSON_AddNumberToObject(json, "StepNumber", step->step_number);
    cJSON_AddBoolToObject(json, "Fork", step->fork);
    cJSON_AddNumberToObject(json, "IperfServerOperation", step->u.iperf_server->input_operation);
    cJSON_AddStringToObject(json, "TestCaseID", step->test_case_id);
    cJSON_AddStringToObject(json, "TestCaseName", step->test_case_name);
    cJSON_AddNumberToObject(json, "StopServerStepNumber",
        step->u.iperf_server->u.stop_conf.stop_step_number);

    str = cJSON_Print(json);

    if (str == nullptr) {
        step->m_ui_mgr->cci_error_code = EJSONPARSE;
        cJSON_Delete(json);
        return RETURN_ERR;
    }

    agent_subdoc = std::string(str);

    snprintf(iperf_config_json, sizeof(iperf_config_json), "/tmp/cci_res/%s_%d.json",
        agent_info->agent_hostname.c_str(), step->step_number);

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: iperf_config_json : %s\n", __func__, __LINE__,
        iperf_config_json);

    if ((fp = fopen(iperf_config_json, "w")) == NULL) {
        step->m_ui_mgr->cci_error_code = EFOPEN;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: fopen failed for %s\n", __func__, __LINE__,
            iperf_config_json);
        return RETURN_ERR;
    }
    if (fwrite(agent_subdoc.c_str(), agent_subdoc.length(), 1, fp) != 1) {
        step->m_ui_mgr->cci_error_code = EFWRITE;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: fwrite failed\n", __func__, __LINE__);
        fclose(fp);
        return RETURN_ERR;
    }
    fclose(fp);
    long status_code;

    http_post_file(agent_url, iperf_config_json, status_code, step->m_ui_mgr->cci_error_code);
    if (status_code != http_status_code_ok) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: http_post_file failed : %d\n", __func__,
            __LINE__, status_code);
        return RETURN_ERR;
    }

    cJSON_free(str);
    cJSON_Delete(json);

    return RETURN_OK;
}

test_step_param_iperf_server::test_step_param_iperf_server()
{
    test_step_params_t *step = this;
    step->is_step_initialized = true;

    step->u.iperf_server = new (std::nothrow) iperf_server_t;
    if (step->u.iperf_server == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: allocation of memory for log failed for %d\n", __func__, __LINE__,
            step->step_number);
        step->is_step_initialized = false;
        return;
    }
    memset(step->u.iperf_server, 0, sizeof(iperf_server_t));
    step->execution_time = 5;
    step->timeout_count = 0;
    step->capture_frames = false;
}

test_step_param_iperf_server::~test_step_param_iperf_server()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Destructor for logredirect called\n", __func__,
        __LINE__);
}
