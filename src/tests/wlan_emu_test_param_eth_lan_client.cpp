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
#include <ctime>
#include <errno.h>
#include <fcntl.h>
#include <memory>

static int external_sta_grace_timeout = 3;

int test_step_param_eth_lan_client::step_execute()
{
    test_step_params_t *step = this;
    std::string agent_subdoc;
    wlan_emu_ext_agent_interface_t *agent_info = NULL;
    std::string cli_subdoc;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Called for Test Step Num : %d\n", __func__,
        __LINE__, step->step_number);

    // find first free dev
    if ((agent_info = step->m_ext_sta_mgr->find_first_free_eth_dev(
             &step->u.eth_lan_client->eth_dev_info)) == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to get first free device\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }
    step->execution_time = step->u.eth_lan_client->duration;

    uint8_mac_to_string_mac(step->u.eth_lan_client->eth_dev_info.interface_mac,
        step->u.eth_lan_client->key);
    wlan_emu_print(wlan_emu_log_level_dbg,
        "%s:%d: eth_dev_info: MAC=%s, state=%d, name=%s, ip=%s\n", __func__, __LINE__,
        step->u.eth_lan_client->key, step->u.eth_lan_client->eth_dev_info.state,
        step->u.eth_lan_client->eth_dev_info.interface_name.c_str(),
        step->u.eth_lan_client->eth_dev_info.ip_address.c_str());

    if (encode_external_eth_lan_client_subdoc(cli_subdoc) == RETURN_ERR) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: encode failed for external eth client for step : %d\n", __func__, __LINE__,
            step->step_number);
        return RETURN_ERR;
    }

    if (step->m_ext_sta_mgr->add_eth_lan_device(step, cli_subdoc) == RETURN_ERR) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: add_eth_lan_device failed for external client for step : %d\n", __func__,
            __LINE__, step->step_number);
        return RETURN_ERR;
    }

    if ((step->fork == true) && (step->u.eth_lan_client->eth_dev_info.is_ip_assigned == true)) {
        step->test_state = wlan_emu_tests_state_cmd_wait;
    } else {
        step->test_state = wlan_emu_tests_state_cmd_continue;
    }

    return RETURN_OK;
}

int test_step_param_eth_lan_client::parse_step_private_data(std::string private_data_str)
{
    cJSON *root_json = NULL, *param = NULL;
    test_step_params_t *step = this;

    root_json = cJSON_Parse(private_data_str.c_str());
    if (root_json == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: NULL json pointer\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> json_wrapper(root_json, cJSON_Delete);
    decode_param_string(root_json, "EthInterfaceName", param);
    step->u.eth_lan_client->eth_dev_info.interface_name = std::string(param->valuestring);

    param = cJSON_GetObjectItem(root_json, "EthIpAddress");
    if (param != NULL && cJSON_IsString(param)) {
        step->u.eth_lan_client->eth_dev_info.is_ip_assigned = true;
        step->u.eth_lan_client->eth_dev_info.ip_address = std::string(param->valuestring);
        wlan_emu_print(wlan_emu_log_level_info, "%s:%d: ip_address : %s\n", __func__, __LINE__,
            step->u.eth_lan_client->eth_dev_info.ip_address.c_str());
    }

    return RETURN_OK;
}

int test_step_param_eth_lan_client::push_ext_eth_lan_client_result_files(
    const std::vector<std::string> &files)
{
    int ret;
    test_step_params_t *step = this;
    char *tmp_file = NULL;

    for (const std::string &file : files) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: result file: %s\n", __func__, __LINE__,
            file.c_str());
        step->u.eth_lan_client->result_file = file;
    }
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: result file is : %s\n", __func__, __LINE__,
        step->u.eth_lan_client->result_file.c_str());
    tmp_file = strdup(step->u.eth_lan_client->result_file.c_str());
    if (tmp_file == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: strdup failed\n", __func__, __LINE__);
        return RETURN_ERR;
    }
    if (step->m_ui_mgr->step_upload_files(tmp_file) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: step_upload_files failed\n", __func__,
            __LINE__);
        free(tmp_file);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        step->m_ui_mgr->cci_error_code = EPUSHTSTRESFILE;
        return RETURN_ERR;
    }
    free(tmp_file);
    return RETURN_OK;
}

int test_step_param_eth_lan_client::step_timeout()
{
    test_step_params_t *step = this;
    wlan_emu_ext_agent_interface_t *ext_agent;
    ext_agent_status_resp_t status = {};

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: sta test key: %s\n", __func__, __LINE__,
        step->u.eth_lan_client->key);

    ext_agent = step->m_ext_sta_mgr->get_ext_agent_from_eth_dev_mac(
        (char *)step->u.eth_lan_client->key);
    if (ext_agent == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to find external agent for key: %s\n",
            __func__, __LINE__, step->u.eth_lan_client->key);
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
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    if ((step->timeout_count == 3) && (status.state == ext_agent_test_state_idle)) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: agent state : %s for step : %d\n", __func__,
            __LINE__, ext_agent->agent_state_as_string(status.state).c_str(), step->step_number);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    if (status.state == ext_agent_test_state_running) {
        if ((step->execution_time + external_sta_grace_timeout) == (step->timeout_count)) {
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
                step->test_state = wlan_emu_tests_state_cmd_abort;
                return RETURN_ERR;
            }

            if (push_ext_eth_lan_client_result_files(step_iter->result_files) != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to push test results\n",
                    __func__, __LINE__);
                step->test_state = wlan_emu_tests_state_cmd_abort;
                return RETURN_ERR;
            }

            return RETURN_OK;
        } else {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: state: %s step number: %d\n", __func__,
                __LINE__, step_state_as_string(step->test_state).c_str(), step_number);
        }
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
        if (step_iter->step_number == step_number) {
            step->test_state = step_iter->state;
            break;
        }
    }

    if (step_iter == status.steps.end()) {
        wlan_emu_print(wlan_emu_log_level_info,
            "%s:%d: failed to get step state for number: %d agent_state : %s\n", __func__, __LINE__,
            step_number, ext_agent->agent_state_as_string(status.state).c_str());
        if ((step->fork == true) && (step->u.eth_lan_client->eth_dev_info.is_ip_assigned == true)) {
            step->test_state = wlan_emu_tests_state_cmd_wait;
        } else {
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
        // char *json_string = cJSON_Print(step_iter->step_private_json_data);
        wlan_emu_print(wlan_emu_log_level_info, "%s:%d: step private data is %s\n", __func__,
            __LINE__, step_iter->step_private_json_data.c_str());
        parse_step_private_data(step_iter->step_private_json_data);
    }

    if (step->test_state == wlan_emu_tests_state_cmd_results) {

        if (ext_agent->download_external_agent_result_files(step_iter->result_files, step->m_ui_mgr->cci_error_code) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to download test results\n",
                __func__, __LINE__);
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }

        if (push_ext_eth_lan_client_result_files(step_iter->result_files) != RETURN_OK) {
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

    if ((step->fork == true) && (step->u.eth_lan_client->eth_dev_info.is_ip_assigned == true)) {
        step->test_state = wlan_emu_tests_state_cmd_wait;
    } else {
        step->test_state = wlan_emu_tests_state_cmd_continue;
    }

    return RETURN_OK;
}

void test_step_param_eth_lan_client::step_remove()
{
    test_step_param_eth_lan_client *step = this;
    if (step == NULL) {
        return;
    }
    if (step->is_step_initialized == true) {
        step->m_ext_sta_mgr->remove_eth_intf(step->u.eth_lan_client);
        if (step->u.eth_lan_client) {
            delete step->u.eth_lan_client;
            step->u.eth_lan_client = NULL;
        }
        step->u.eth_lan_client = nullptr;
    }
    // Do NOT delete this! Let the owner manage the object's lifetime.
    // step = NULL; // not needed
    return;
}

int test_step_param_eth_lan_client::step_frame_filter(wlan_emu_msg_t *msg)
{

    test_step_params_t *step = this;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: unhandled frame for step number : %d\n",
        __func__, __LINE__, step->step_number);

    return RETURN_UNHANDLED;
}

int test_step_param_eth_lan_client::encode_external_eth_lan_client_subdoc(std::string &agent_subdoc)
{
    cJSON *json = NULL;
    test_step_params_t *step = this;
    char mac_str[32] = { 0 };
    char *str = NULL;

    json = cJSON_CreateObject();
    if (json == NULL) {
        return RETURN_ERR;
    }

    cJSON_AddStringToObject(json, "SubDocName", "EthernetLanClient");
    cJSON_AddNumberToObject(json, "StepNumber", step->step_number);
    cJSON_AddBoolToObject(json, "Fork", step->fork);
    cJSON_AddStringToObject(json, "TestCaseID", step->test_case_id);
    cJSON_AddStringToObject(json, "TestCaseName", step->test_case_name);
    cJSON_AddStringToObject(json, "EthInterfaceName",
        step->u.eth_lan_client->eth_dev_info.interface_name.c_str());

    cJSON_AddStringToObject(json, "EthIpAddress",
        step->u.eth_lan_client->eth_dev_info.ip_address.c_str());

    uint8_mac_to_string_mac(step->u.eth_lan_client->eth_dev_info.interface_mac, mac_str);
    cJSON_AddStringToObject(json, "EthMacAddress", mac_str);
    cJSON_AddNumberToObject(json, "Duration", step->u.eth_lan_client->duration);

    str = cJSON_Print(json);

    if (str == nullptr) {
        cJSON_Delete(json);
        return RETURN_ERR;
    }

    agent_subdoc = std::string(str);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Encode External lan client : %s\n", __func__,
        __LINE__, agent_subdoc.c_str());

    cJSON_free(str);
    cJSON_Delete(json);

    return RETURN_OK;
}

test_step_param_eth_lan_client::test_step_param_eth_lan_client()
{
    test_step_params_t *step = this;
    step->is_step_initialized = true;

    step->u.eth_lan_client = new (std::nothrow) eth_lan_interface_t;
    if (step->u.eth_lan_client == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: allocation of memory for eth lan client failed for %d\n", __func__, __LINE__,
            step->step_number);
        step->is_step_initialized = false;
        return;
    }
    //    memset(step->u.eth_lan_client, 0, sizeof(eth_lan_interface_t));
    step->u.eth_lan_client->eth_dev_info.is_ip_assigned = false;
    step->execution_time = 5;
    step->timeout_count = 0;
    step->capture_frames = false;
}

test_step_param_eth_lan_client::~test_step_param_eth_lan_client()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Destructor for eth lan interface  called\n",
        __func__, __LINE__);
}
