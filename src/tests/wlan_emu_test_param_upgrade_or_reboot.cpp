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
#include "wlan_common_utils.h"
#include "wlan_emu_common.h"
#include "wlan_emu.h"
#include "wlan_emu_err_code.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include <unistd.h>
#include <string.h>
#include <cjson/cJSON.h>

int test_step_param_upgrade_or_reboot::create_reboot_case_json(test_step_params_t *step)
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Entered create_reboot_case_json\n", __func__,
        __LINE__);

    if (step == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: step is NULL\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    // Root JSON object
    cJSON *root = cJSON_CreateObject();

    // config file (with localhost handling)
    if (step->m_ui_mgr->get_local_host_enabled() == true) {
        cJSON_AddStringToObject(root, "config file",
            ("http://localhost" + std::string(step->m_ui_mgr->get_tda_url())).c_str());
    } else {
        cJSON_AddStringToObject(root, "config file", step->m_ui_mgr->get_tda_url());
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Config file: %s\n", __func__, __LINE__,
        step->m_ui_mgr->get_tda_url());

    // Other fields
    cJSON_AddStringToObject(root, "server_address", step->m_ui_mgr->get_server_address());
    cJSON_AddStringToObject(root, "interface", step->m_ui_mgr->get_tda_interface());
    cJSON_AddNumberToObject(root, "simulated_client_count",
        step->m_ui_mgr->get_simulated_client_count());
    cJSON_AddStringToObject(root, "ResultsFileName", step->m_ui_mgr->get_tda_output_file());

    // logging_step_numbers
    queue_t *q = step->u.upgrade_or_reboot->logging_step_numbers;
    size_t count = q ? queue_count(q) : 0;

    cJSON_AddBoolToObject(root, "is_logging_enabled", count > 0);
    cJSON_AddNumberToObject(root, "count_logging_step_numbers", count);

    cJSON *log_array = cJSON_CreateArray();
    if (count > 0) {
        size_t i = 0;
        while (queue_count(q) > 0) {
            step_number_entry_t *entry = (step_number_entry_t *)queue_pop(q);
            cJSON *obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(obj, "step_number", entry->step_number);
            cJSON_AddItemToArray(log_array, obj);
            free(entry);
            i++;
        }
    }
    cJSON_AddItemToObject(root, "logging_step_numbers", log_array);

    // last executed step
    cJSON_AddNumberToObject(root, "last_executed_step_number", step->step_number);

    // Convert to string and write to file
    char *json_str = cJSON_Print(root); // pretty formatted, use cJSON_PrintUnformatted for compact
    FILE *fp = fopen(CCI_TEST_REBOOT_STEP_CONFIG_JSON, "w");
    if (fp == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to open file for writing\n", __func__,
            __LINE__);
        cJSON_Delete(root);
        free(json_str);
        return RETURN_ERR;
    }
    fputs(json_str, fp);
    fclose(fp);

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Created JSON file successfully\n", __func__,
        __LINE__);

    // Cleanup
    cJSON_Delete(root);
    free(json_str);

    return RETURN_OK;
}

int test_step_param_upgrade_or_reboot::step_execute()
{
    test_step_params_t *step = this;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: STEP : %d - Called for Step Execute\n", __func__,
        __LINE__, this->step_number);

    if (this->u.upgrade_or_reboot == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: device_upgrade_or_reboot is NULL\n",
            __func__, __LINE__);
        return RETURN_ERR;
    }

    if (queue_count(this->u.upgrade_or_reboot->logging_step_numbers) != 0) {
        // Log the step numbers
        queue_t *q = this->u.upgrade_or_reboot->logging_step_numbers;
        size_t count = queue_count(q);
        for (size_t i = 0; i < count; i++) {
            test_step_params_t *log_step = NULL;
            wlan_emu_test_case_config *test_case_config =
                (wlan_emu_test_case_config *)step->param_get_test_case_config();

            if (test_case_config == NULL) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: test_case_config is NULL for step : %d\n", __func__, __LINE__,
                    step->step_number);
                step->test_state = wlan_emu_tests_state_cmd_abort;
                step->m_ui_mgr->cci_error_code = ENULL;
                return RETURN_ERR;
            }
            step_number_entry_t *entry = (step_number_entry_t *)queue_peek(q, i);
            if (entry == NULL) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: entry is NULL in logging_step_numbers for step : %d\n", __func__,
                    __LINE__, step->step_number);
                step->test_state = wlan_emu_tests_state_cmd_abort;
                step->m_ui_mgr->cci_error_code = ENULL;
                return RETURN_ERR;
            }

            log_step = (test_step_params_t *)step->m_ui_mgr->get_step_from_step_number(
                test_case_config, entry->step_number);
            if (log_step == NULL) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: Invalid log stop step  : %d in step : %d\n", __func__, __LINE__,
                    step->u.log_capture->stop_step_number, step->step_number);
                step->test_state = wlan_emu_tests_state_cmd_abort;
                step->m_ui_mgr->cci_error_code = ENULL;
                return RETURN_ERR;
            }

            log_step->u.log_capture->log_operation = log_operation_type_stop;
            pthread_cancel(log_step->u.log_capture->thread_id);
            close(log_step->u.log_capture->redirect_fd);
            if (step->m_ui_mgr->step_upload_files(log_step->u.log_capture->log_result_file) !=
                RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: step_upload_files failed for %s\n",
                    __func__, __LINE__, log_step->u.log_capture->log_result_file);
                continue;
            }
        }
    }

    if (create_reboot_case_json(step) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to create reboot case JSON\n",
            __func__, __LINE__);
        this->test_state = wlan_emu_tests_state_cmd_abort;
        step->m_ui_mgr->cci_error_code = EJSONPARSE;
        return RETURN_ERR;
    }
    step->m_ui_mgr->upload_results();
    if (step->m_ui_mgr->cci_report_pause_to_tda() != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: cci_report_pause_to_tda failed\n", __func__,
            __LINE__);
        this->test_state = wlan_emu_tests_state_cmd_abort;
        step->m_ui_mgr->cci_error_code = ECURLPOST;
        return RETURN_ERR;
    }

    if (access(CCI_TEST_REBOOT_STEP_CONFIG_JSON, R_OK) == 0) {
        WaitForDuration(5000); // Sleep for 5 seconds
        if (reboot(LINUX_REBOOT_CMD_RESTART) == -1) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Reboot failed: %s\n", __func__, __LINE__,
                strerror(errno));
            this->test_state = wlan_emu_tests_state_cmd_abort;
            step->m_ui_mgr->cci_error_code = ECURLPOST;
            return RETURN_ERR;
        }
        this->test_state = wlan_emu_tests_state_cmd_abort;
        step->m_ui_mgr->cci_error_code = ECURLPOST;
        return RETURN_ERR;
    } else {
        this->test_state = wlan_emu_tests_state_cmd_abort;
        step->m_ui_mgr->cci_error_code = ECURLPOST;
        return RETURN_ERR;
    }

    return RETURN_OK;
}

void test_step_param_upgrade_or_reboot::step_remove()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: STEP : %d - Called for Step Remove\n", __func__,
        __LINE__, this->step_number);

    if (this->u.upgrade_or_reboot != NULL) {
        if (this->u.upgrade_or_reboot->logging_step_numbers != NULL) {
            queue_destroy(this->u.upgrade_or_reboot->logging_step_numbers);
            this->u.upgrade_or_reboot->logging_step_numbers = NULL;
        }
        delete this->u.upgrade_or_reboot;
        this->u.upgrade_or_reboot = NULL;
    }
}

int test_step_param_upgrade_or_reboot::step_timeout()
{
    wlan_emu_print(wlan_emu_log_level_dbg,
        "%s:%d: STEP : %d - Timeout not applicable for upgrade/reboot\n", __func__, __LINE__,
        this->step_number);
    return RETURN_OK; // No timeout handling for upgrade/reboot steps
}

test_step_param_upgrade_or_reboot::test_step_param_upgrade_or_reboot()
{
    this->u.upgrade_or_reboot = new device_upgrade_t();
    if (this->u.upgrade_or_reboot == NULL) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: Memory allocation failed for device_upgrade_t\n", __func__, __LINE__);
    }
    // this->u.upgrade_or_reboot->is_upgrade = false; // Default to reboot
    this->u.upgrade_or_reboot->is_logging_enabled = false;
    this->u.upgrade_or_reboot->logging_step_numbers = NULL;
    this->is_step_initialized = true;
}

int test_step_param_upgrade_or_reboot::step_frame_filter(wlan_emu_msg_t *msg)
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

test_step_param_upgrade_or_reboot::~test_step_param_upgrade_or_reboot()
{
    if (this->u.upgrade_or_reboot != NULL) {
        if (this->u.upgrade_or_reboot->logging_step_numbers != NULL) {
            queue_destroy(this->u.upgrade_or_reboot->logging_step_numbers);
            this->u.upgrade_or_reboot->logging_step_numbers = NULL;
        }
        delete this->u.upgrade_or_reboot;
        this->u.upgrade_or_reboot = NULL;
    }
}
