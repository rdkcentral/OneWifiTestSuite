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

#include "wlan_emu.h"
#include "wlan_emu_bus.h"
#include "wlan_emu_tests.h"
#include <assert.h>
#include <csignal>
#include <stdio.h>
#include <string.h>
#include <memory>
#include <curl/curl.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

#if !defined(CONFIG_EXT_AGENT_CCI)
#include "syscfg/syscfg.h"
#endif // CONFIG_EXT_AGENT_CCI

#define CCI_SIM_CLI_COUNT_SYS_ENTRY "onewifi_suite_sim_cli_count"

extern "C" {
INT wifi_hal_init();
INT wifi_hal_getInterfaceMap(wifi_interface_name_idex_map_t *if_map, unsigned int max_entries,
    unsigned int *if_map_size);
INT wifi_hal_disconnect(INT ap_index);
}

wlan_emu_tests_state_t wlan_emu_t::m_state = wlan_emu_tests_state_cmd_wait;
wlan_emu_ui_mgr_t wlan_emu_t::m_ui_mgr;
wlan_emu_dml_tests_state_t wlan_emu_t::dml_state = wlan_emu_dml_tests_state_idle;

#if 0
void StationDisconnectHandler(int signal) {
//    wifi_hal_disconnect(16);
/*    wifi_hal_disconnect(17);
    wifi_hal_disconnect(18);*/
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Disconnecting the station interface via signal handler\n", __func__, __LINE__);
    exit(EXIT_SUCCESS);
}
#endif

void wlan_emu_t::update_state_io_wait_done(wlan_emu_sig_type_t val)
{
    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: Received signal: %d m_state : 0x%x\n", __func__,
        __LINE__, val, m_state);
    //    assert(m_state != wlan_emu_tests_state_cmd_wait);

    switch (val) {
    case wlan_emu_sig_type_fail:
    case wlan_emu_sig_type_max:
        m_state = wlan_emu_tests_state_cmd_abort;
        break;
    case wlan_emu_sig_type_input:
        m_state = wlan_emu_tests_state_cmd_request;
        break;
    case wlan_emu_sig_type_analysis:
        m_state = wlan_emu_tests_state_cmd_start;
        break;
    case wlan_emu_sig_type_coverage_1:
    case wlan_emu_sig_type_coverage_2:
    case wlan_emu_sig_type_coverage_3:
    case wlan_emu_sig_type_coverage_4:
    case wlan_emu_sig_type_coverage_5:
        m_state = wlan_emu_tests_state_cmd_results;
        break;
    case wlan_emu_sig_type_results:
    default:
        m_state = wlan_emu_tests_state_cmd_wait;
        break;
    }
}

int wlan_emu_t::run()
{
    bool exit = false;
    // wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Test program started on %d\n", __func__,
    // __LINE__,
    //        get_platform_type());

    m_msg_mgr.start();
    m_sim_sta_mgr.start();
    //    std::signal(SIGTERM, StationDisconnectHandler);
    while (exit == false) {
        switch (get_state()) {
        case wlan_emu_tests_state_cmd_wait:
            dml_state = wlan_emu_dml_tests_state_idle;
            break;
        case wlan_emu_tests_state_cmd_request:
            // download the test and respective configs will be downloaded
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: in case wlan_emu_tests_state_cmd_request\n", __func__, __LINE__);
            if (m_ext_sta_mgr.init() != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: update_external_agent_capabilities failed\n", __func__, __LINE__);
                m_ui_mgr.signal_report_test_fail();
                dml_state = wlan_emu_dml_tests_state_complete_failure;
                break;
            }

            if (m_ui_mgr.analyze_request() == RETURN_OK) {
                // Download succesfull, trigger the start
                m_ui_mgr.signal_downloaded_test_data();
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: analyze_request succesful\n",
                    __func__, __LINE__);
                dml_state = wlan_emu_dml_tests_state_running;
            } else {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: analyze_request failed\n", __func__,
                    __LINE__);
                m_ui_mgr.signal_report_test_fail();
                dml_state = wlan_emu_dml_tests_state_complete_failure;
            }
            break;
        case wlan_emu_tests_state_cmd_start:
            // here the execution of the test cases happend
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: in case wlan_emu_tests_state_cmd_start\n", __func__, __LINE__);
            if (start_test() == RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: start_test succesful\n", __func__,
                    __LINE__);
                m_state = wlan_emu_tests_state_cmd_wait;
                dml_state = wlan_emu_dml_tests_state_running;
            } else {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: start_test failed\n", __func__,
                    __LINE__);
                m_ui_mgr.signal_report_test_fail();
                dml_state = wlan_emu_dml_tests_state_complete_failure;
            }
            // Irrespective of success (or) failed the testcase needs to wait
            break;
        case wlan_emu_tests_state_cmd_results:
            if (m_ui_mgr.upload_results() == RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: upload_results succesful\n",
                    __func__, __LINE__);
                m_ui_mgr.signal_uploaded_test_results();
                m_ui_mgr.cci_report_complete_to_tda();
                dml_state = wlan_emu_dml_tests_state_complete_success;
                m_ui_mgr.update_log_file_offset();
            } else {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: upload_results failed\n", __func__,
                    __LINE__);
                m_ui_mgr.cci_report_failure_to_tda();
                m_state = wlan_emu_tests_state_cmd_wait;
                dml_state = wlan_emu_dml_tests_state_complete_failure;
            }
            system("rm -rf /tmp/cci_res/*");
            break;
        case wlan_emu_tests_state_cmd_abort:
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d:\n", __func__, __LINE__);

            if (m_ui_mgr.upload_results() != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: upload_results failed for abort\n",
                        __func__, __LINE__);
            }

            abort_test();
            m_state = wlan_emu_tests_state_cmd_wait;
            dml_state = wlan_emu_dml_tests_state_complete_failure;
            m_ui_mgr.update_log_file_offset();
            system("rm -rf /tmp/cci_res/*");
            // exit = true; //cci needs to run all the time even upon failure
            break;
        default:
            break;
        }

        update_state_io_wait_done(m_ui_mgr.io_wait());
    }

    m_msg_mgr.stop();
    return 0;
}

int wlan_emu_t::start_test()
{
    unsigned int count = 0;
    unsigned int i = 0;
    queue_t *tmp_test_cov_q;
    wlan_emu_test_case_config *config;
    wlan_emu_tests_t *test = NULL;

    tmp_test_cov_q = m_ui_mgr.get_test_cov_cases_queue();

    count = queue_count(tmp_test_cov_q);
    if (count == 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: queue count is 0\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    for (i = 0; i < count; i++) {
        config = (wlan_emu_test_case_config *)queue_peek(tmp_test_cov_q, i);

        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: config->test_steps_q : %d\n", __func__,
            __LINE__, queue_count(config->test_steps_q));

        config->test_state = wlan_emu_tests_state_cmd_start;

        switch (config->test_type) {
        case wlan_emu_test_1_subtype_radio:
            test = new wlan_emu_tests_radio_t(&m_msg_mgr, &m_ui_mgr, &m_sim_sta_mgr, &m_ext_sta_mgr,
                config, &m_bus_mgr);
            test->start();
            break;
        case wlan_emu_test_1_subtype_ns_private:
            test = new wlan_emu_tests_private_vap_t(&m_msg_mgr, &m_ui_mgr, &m_sim_sta_mgr,
                &m_ext_sta_mgr, config, &m_bus_mgr);
            test->start();
            break;
        case wlan_emu_test_1_subtype_ns_public_xfinity_open:
            test = new wlan_emu_tests_xfinity_open_t(&m_msg_mgr, &m_ui_mgr, &m_sim_sta_mgr,
                &m_ext_sta_mgr, config, &m_bus_mgr);
            test->start();
            break;
        case wlan_emu_test_1_subtype_ns_public_xfinity_secure:
            test = new wlan_emu_tests_xfinity_secure_t(&m_msg_mgr, &m_ui_mgr, &m_sim_sta_mgr,
                &m_ext_sta_mgr, config, &m_bus_mgr);
            test->start();
            break;
        case wlan_emu_test_1_subtype_ns_managed_xhs:
            test = new wlan_emu_tests_xhs_t(&m_msg_mgr, &m_ui_mgr, &m_sim_sta_mgr, &m_ext_sta_mgr,
                config, &m_bus_mgr);
            test->start();
            break;
        case wlan_emu_test_1_subtype_ns_managed_lnf_enterprise:
            test = new wlan_emu_tests_lnf_enterprise_t(&m_msg_mgr, &m_ui_mgr, &m_sim_sta_mgr,
                &m_ext_sta_mgr, config, &m_bus_mgr);
            test->start();
            break;
        case wlan_emu_test_1_subtype_ns_managed_lnf_secure:
            test = new wlan_emu_tests_lnf_secure_t(&m_msg_mgr, &m_ui_mgr, &m_sim_sta_mgr,
                &m_ext_sta_mgr, config, &m_bus_mgr);
            test->start();
            break;
        case wlan_emu_test_1_subtype_ns_managed_mesh_backhaul:
            test = new wlan_emu_tests_mesh_backhaul_t(&m_msg_mgr, &m_ui_mgr, &m_sim_sta_mgr,
                &m_ext_sta_mgr, config, &m_bus_mgr);
            test->start();
            break;
        case wlan_emu_test_1_subtype_ns_managed_mesh_client:
            test = new wlan_emu_tests_mesh_client_t(&m_msg_mgr, &m_ui_mgr, &m_sim_sta_mgr,
                &m_ext_sta_mgr, config, &m_bus_mgr);
            test->start();
            break;
        case wlan_emu_test_1_subtype_cc_probe_response:
            test = new wlan_emu_tests_cli_probe_response(&m_msg_mgr, &m_ui_mgr, &m_sim_sta_mgr,
                &m_ext_sta_mgr, config, &m_bus_mgr);
            test->start();
            break;
        case wlan_emu_test_1_subtype_cc_authentication:
            test = new wlan_emu_tests_cli_auth(&m_msg_mgr, &m_ui_mgr, &m_sim_sta_mgr,
                &m_ext_sta_mgr, config, &m_bus_mgr);
            test->start();
            break;
        case wlan_emu_test_2_subtype_af_connection_admission:
            test = new wlan_emu_tests_connection_admission(&m_msg_mgr, &m_ui_mgr, &m_sim_sta_mgr,
                &m_ext_sta_mgr, config, &m_bus_mgr);
            test->start();
            break;
        case wlan_emu_test_2_subtype_af_access_control:
            test = new wlan_emu_tests_access_control(&m_msg_mgr, &m_ui_mgr, &m_sim_sta_mgr,
                &m_ext_sta_mgr, config, &m_bus_mgr);
            test->start();
            break;
        case wlan_emu_test_2_subtype_af_stats_manager:
            test = new wlan_emu_tests_stats_manager(&m_msg_mgr, &m_ui_mgr, &m_sim_sta_mgr,
                &m_ext_sta_mgr, config, &m_bus_mgr);
            test->start();
            break;
        case wlan_emu_test_2_subtype_af_steering_manager:
            test = new wlan_emu_tests_steering_manager(&m_msg_mgr, &m_ui_mgr, &m_sim_sta_mgr,
                &m_ext_sta_mgr, config, &m_bus_mgr);
            test->start();
            break;
        case wlan_emu_test_2_subtype_af_optimization:
            test = new wlan_emu_tests_optimization(&m_msg_mgr, &m_ui_mgr, &m_sim_sta_mgr,
                &m_ext_sta_mgr, config, &m_bus_mgr);
            test->start();
            break;
        case wlan_emu_test_2_subtype_af_grey_listing:
            test = new wlan_emu_tests_grey_listing(&m_msg_mgr, &m_ui_mgr, &m_sim_sta_mgr,
                &m_ext_sta_mgr, config, &m_bus_mgr);
            test->start();
            break;
        case wlan_emu_test_2_subtype_af_active_passive_msrmnts:
            test = new wlan_emu_tests_active_passive_msrmt(&m_msg_mgr, &m_ui_mgr, &m_sim_sta_mgr,
                &m_ext_sta_mgr, config, &m_bus_mgr);
            test->start();
            break;
        case wlan_emu_test_2_subtype_af_whix:
            test = new wlan_emu_tests_whix(&m_msg_mgr, &m_ui_mgr, &m_sim_sta_mgr, &m_ext_sta_mgr,
                config, &m_bus_mgr);
            test->start();
            break;
        case wlan_emu_test_2_subtype_af_blaster:
            test = new wlan_emu_tests_blaster(&m_msg_mgr, &m_ui_mgr, &m_sim_sta_mgr, &m_ext_sta_mgr,
                config, &m_bus_mgr);
            test->start();
            break;
        case wlan_emu_test_2_subtype_af_motion:
            test = new wlan_emu_tests_motion(&m_msg_mgr, &m_ui_mgr, &m_sim_sta_mgr, &m_ext_sta_mgr,
                config, &m_bus_mgr);
            test->start();
            break;
        case wlan_emu_test_2_subtype_af_finger_printing:
            test = new wlan_emu_tests_finger_printing(&m_msg_mgr, &m_ui_mgr, &m_sim_sta_mgr,
                &m_ext_sta_mgr, config, &m_bus_mgr);
            test->start();
            break;
        case wlan_emu_test_2_subtype_af_tr_181:
            test = new wlan_emu_tests_tr_181(&m_msg_mgr, &m_ui_mgr, &m_sim_sta_mgr, &m_ext_sta_mgr,
                config, &m_bus_mgr);
            test->start();
            break;
        case wlan_emu_test_2_subtype_af_webconfig:
            test = new wlan_emu_tests_webconfig(&m_msg_mgr, &m_ui_mgr, &m_sim_sta_mgr,
                &m_ext_sta_mgr, config, &m_bus_mgr);
            test->start();
            break;
        case wlan_emu_test_2_subtype_af_webpa:
            test = new wlan_emu_tests_webpa(&m_msg_mgr, &m_ui_mgr, &m_sim_sta_mgr, &m_ext_sta_mgr,
                config, &m_bus_mgr);
            test->start();
            break;
        case wlan_emu_test_3_subtype_pm_stats_get:
            test = new wlan_emu_tests_stats_get(&m_msg_mgr, &m_ui_mgr, &m_sim_sta_mgr,
                &m_ext_sta_mgr, config, &m_bus_mgr);
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: Config test type : wlan_emu_test_3_subtype_pm_stats_get\n", __func__,
                __LINE__);
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: test = new wlan_emu_tests_stats_get\n",
                __func__, __LINE__);
            test->start();
            break;
        case wlan_emu_test_3_subtype_pm_stats_set:
            test = new wlan_emu_tests_stats_set(&m_msg_mgr, &m_ui_mgr, &m_sim_sta_mgr,
                &m_ext_sta_mgr, config, &m_bus_mgr);
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: Config test type : wlan_emu_test_3_subtype_pm_stats_set\n", __func__,
                __LINE__);
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: test = new wlan_emu_tests_stats_set\n",
                __func__, __LINE__);
            test->start();
            break;

        default:
            break;
        }
    }

    // store the test somewhere
    return RETURN_OK;
}

void wlan_emu_t::abort_test()
{
    wlan_emu_tests_t *test_v = NULL;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: abort_test called\n", __func__, __LINE__);
    m_ui_mgr.cci_report_failure_to_tda();
    // retrieve the ongoing tests

    if (test_v == NULL) {
        return;
    }

    test_v->stop();
    delete test_v;
    test_v = NULL;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Test stopped\n", __func__, __LINE__);
}

#define STR_LEN 128

bus_error_t wlan_emu_t::set_cci_handler(char *event_name, raw_data_t *data, bus_user_data_t *user_data)
{
    (void)user_data;
    char parameter[STR_LEN] = { 0 };
    int len = 0;
    char buf[8] = { 0 };

    if (get_dml_state() == wlan_emu_dml_tests_state_running) {
        wlan_emu_print(wlan_emu_log_level_info,
            "%s: Execution not allowed as test case is in running state\n", __FUNCTION__);
        return bus_error_invalid_input;
    }

    if (!event_name) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d Invalid bus property\n", __func__, __LINE__);
        return bus_error_invalid_input;
    }

    memset(parameter, 0, STR_LEN);
    sscanf(event_name, "Device.WiFi.Tests.%200s", parameter);
    if (strstr(parameter, "TestConfigURL")) {
        if (data->data_type != bus_data_type_string) {
            wlan_emu_print(wlan_emu_log_level_err, "%s: Invalid bus property\n", __func__);
            return bus_error_invalid_input;
        }
        copy_string(m_ui_mgr.get_tda_url(), (char *)data->raw_data.bytes, STR_LEN);
        if (m_ui_mgr.update_tda_url_to_file() == RETURN_ERR) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d Failed to write to the file\n", __func__,
                __LINE__);
        }
    } else if (strstr(parameter, "ResultsFileName")) {
        if (data->data_type != bus_data_type_string) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d Invalid bus property\n", __func__,
                __LINE__);
            return bus_error_invalid_input;
        }
        copy_string(m_ui_mgr.get_tda_output_file(), (char *)data->raw_data.bytes, STR_LEN);
    } else if (strstr(parameter, "Interface")) {
        if (data->data_type != bus_data_type_string) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d Invalid bus property\n", __func__,
                __LINE__);
            return bus_error_invalid_input;
        }
        copy_string(m_ui_mgr.get_tda_interface(), (char *)data->raw_data.bytes, STR_LEN);
    } else if (strstr(parameter, "SimulatedClientDevices")) {
        if (data->data_type != bus_data_type_uint32) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d Invalid bus property\n", __func__,
                __LINE__);
            return bus_error_invalid_input;
        }
        int count = data->raw_data.u32;
        if ((count <= 0) || (count > 300)) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d unable to set the simulated clients to  %d\n", __FUNCTION__, __LINE__,
                count);
            return bus_error_invalid_input;
        }
        m_ui_mgr.set_simulated_client_count(count);
        snprintf(buf, sizeof(buf), "%d", count);

        if (syscfg_set_commit(NULL, CCI_SIM_CLI_COUNT_SYS_ENTRY, buf) == 0) {
            wlan_emu_print(wlan_emu_log_level_info,
                "%s:%d updated the new supported clients to %d\n", __FUNCTION__, __LINE__, count);
            wlan_emu_print(wlan_emu_log_level_info,
                "%s:%d Restart the rdkfmac.service for new clients into effect\n");
        } else {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d unable to set the simulated clients to  %d\n", __FUNCTION__, __LINE__,
                count);
        }

    } else if (strstr(parameter, "Start")) {
        if (data->data_type != bus_data_type_boolean) {
            wlan_emu_print(wlan_emu_log_level_err, "%s: Invalid bus property\n", __func__);
            return bus_error_invalid_input;
        }
        bool start = data->raw_data.b;
        if (start) {
            m_ui_mgr.signal_input();
        }
    }
    return bus_error_success;
}

bus_error_t wlan_emu_t::get_cci_handler(char *event_name, raw_data_t *data, bus_user_data_t *user_data)
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s: bus property=%s\n", __FUNCTION__, event_name);
    (void)user_data;
    char extension[64] = { 0 };
    sscanf((char *)event_name, "Device.WiFi.Tests.%63s", extension);

    memset(data, 0, sizeof(raw_data_t));
    if (strstr(extension, "TestConfigURL")) {
        if (m_ui_mgr.get_tda_url()[0] == '\0') {
            data->raw_data.bytes = new char[3]();
            data->data_type = bus_data_type_string;
        } else {
            data->raw_data.bytes = new char[128]();
            strncpy((char *)data->raw_data.bytes, m_ui_mgr.get_tda_url(), 127);
            data->data_type = bus_data_type_string;
        }
    } else if (strstr(extension, "ResultsFileName")) {
        data->raw_data.bytes = new char[64]();
        if (m_ui_mgr.get_tda_output_file()[0] == '\0') {
            strncpy((char *)data->raw_data.bytes, "results.txt", strlen("results.txt") + 1);
            data->data_type = bus_data_type_string;
        } else {
            strncpy((char *)data->raw_data.bytes, m_ui_mgr.get_tda_output_file(), 63);
            data->data_type = bus_data_type_string;
        }
    } else if (strstr(extension, "Interface")) {
        data->raw_data.bytes = new char[16]();
        if (m_ui_mgr.get_tda_interface()[0] == '\0') {
            strncpy((char *)data->raw_data.bytes, "erouter0", strlen("erouter0") + 1);
            data->data_type = bus_data_type_string;
        } else {
            strncpy((char *)data->raw_data.bytes, m_ui_mgr.get_tda_interface(), 15);
            data->data_type = bus_data_type_string;
        }
    } else if (strstr(extension, "SimulatedClientDevices")) {
        data->raw_data.u32 = (uint32_t)m_ui_mgr.get_simulated_client_count();
        data->data_type = bus_data_type_uint32;
    } else if (strstr(extension, "Start")) {
        data->raw_data.b = 0;
        data->data_type = bus_data_type_boolean;
    } else if (strstr(extension, "Status")) {
        data->raw_data.u32 = wlan_emu_t::get_dml_state();
        data->data_type = bus_data_type_uint32;
    }

    return bus_error_success;
}

void wlan_emu_t::bus_register_handlers()
{
    int rc = bus_error_success;
    unsigned char num_of_radio = 0, index = 0;
    char *component_name = "Cci";

    bus_data_element_t dataElements[] = {
        { const_cast<char *>(CCI_TEST_CONFIG_URL),              bus_element_type_property,
         { wlan_emu_t::get_cci_handler, wlan_emu_t::set_cci_handler, NULL, NULL, NULL, NULL },
         slow_speed, ZERO_TABLE, { bus_data_type_string, true, 0, 0, 0, NULL }  },
        { const_cast<char *>(CCI_TEST_RESULT_FILENAME),         bus_element_type_property,
         { wlan_emu_t::get_cci_handler, wlan_emu_t::set_cci_handler, NULL, NULL, NULL, NULL },
         slow_speed, ZERO_TABLE, { bus_data_type_string, true, 0, 0, 0, NULL }  },
        { const_cast<char *>(CCI_TEST_INTERFACE),               bus_element_type_property,
         { wlan_emu_t::get_cci_handler, wlan_emu_t::set_cci_handler, NULL, NULL, NULL, NULL },
         slow_speed, ZERO_TABLE, { bus_data_type_string, true, 0, 0, 0, NULL }  },
        { const_cast<char *>(CCI_TEST_SIMULATED_CLIENTDEVICES), bus_element_type_property,
         { wlan_emu_t::get_cci_handler, wlan_emu_t::set_cci_handler, NULL, NULL, NULL, NULL },
         slow_speed, ZERO_TABLE, { bus_data_type_uint32, true, 0, 0, 0, NULL }  },
        { const_cast<char *>(CCI_TEST_START),                   bus_element_type_property,
         { wlan_emu_t::get_cci_handler, wlan_emu_t::set_cci_handler, NULL, NULL, NULL, NULL },
         slow_speed, ZERO_TABLE, { bus_data_type_boolean, true, 0, 0, 0, NULL } },
        { const_cast<char *>(CCI_TEST_STATUS),                  bus_element_type_property,
         { wlan_emu_t::get_cci_handler, wlan_emu_t::set_cci_handler, NULL, NULL, NULL, NULL },
         slow_speed, ZERO_TABLE, { bus_data_type_uint32, true, 0, 0, 0, NULL }  },
    };

    rc = this->m_bus_mgr.desc.bus_open_fn(&this->handle, component_name);

    if (rc != bus_error_success) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s bus open failed\n", __FUNCTION__);
        return;
    }

    rc = this->m_bus_mgr.desc.bus_reg_data_element_fn(&this->handle, dataElements,
        sizeof(dataElements) / sizeof(bus_data_element_t));

    if (rc != bus_error_success) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s bus_regDataElements failed\n", __FUNCTION__);
        this->m_bus_mgr.desc.bus_unreg_data_elements_fn(&this->handle,
            sizeof(dataElements) / sizeof(bus_data_element_t), dataElements);
        this->m_bus_mgr.desc.bus_close_fn(&this->handle);
    }

    return;
}

#define MAX_NUM_SIMULATED_CLIENT 300

int wlan_emu_t::init()
{
    wifi_interface_name_idex_map_t *if_map = NULL;
    char buf[8] = { 0 };
    int sim_client_count = 0;

    syscfg_init();
    if (0 == syscfg_get(NULL, CCI_SIM_CLI_COUNT_SYS_ENTRY, buf, sizeof(buf)) && (buf[0] != '\0')) {
        sim_client_count = (int)atoi(buf);
    }

    if ((sim_client_count <= 0) || (sim_client_count > 300)) {
        sim_client_count = 3;
    }

    m_ui_mgr.set_simulated_client_count(sim_client_count);

    if_map = (wifi_interface_name_idex_map_t *)malloc(
        sizeof(wifi_interface_name_idex_map_t) * sim_client_count);
    unsigned int if_map_size = 0;
    if (if_map == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: malloc failed for if_map\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    memset(if_map, 0, sizeof(wifi_interface_name_idex_map_t) * sim_client_count);
    if (wifi_hal_init() != 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: hal init problem\n", __func__, __LINE__);
        free(if_map);
        return RETURN_ERR;
    } else if (wifi_hal_getInterfaceMap(if_map, sim_client_count, &if_map_size) != 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: platform capability get failed\n", __func__,
            __LINE__);
        free(if_map);
        return RETURN_ERR;
    }
    curl_global_init(CURL_GLOBAL_DEFAULT);
    static std::shared_ptr<void> curl_cleanup_guard(nullptr, [](void *) { curl_global_cleanup(); });

    m_bus_mgr.bus_init();
    bus_register_handlers();
    m_ui_mgr.add_bus_mgr(&m_bus_mgr);
    m_ui_mgr.init();

    m_ext_sta_mgr.ext_agent_iface.m_ui_mgr = &m_ui_mgr;

    if (m_sim_sta_mgr.init(if_map, if_map_size) == RETURN_ERR) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: sta_mgr init failed\n", __func__, __LINE__);
        free(if_map);
        return RETURN_ERR;
    }
    // Init the external station manager
    if (m_ext_sta_mgr.init() == RETURN_ERR) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d:  external_station manager init failed\n",
            __func__, __LINE__);
        return RETURN_ERR;
    }
    FILE *fp = fopen(CCI_TEST_REBOOT_STEP_CONFIG_JSON, "r");
    if (fp != NULL) {
        fclose(fp);

        if (m_ui_mgr.decode_reboot_case_json() != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: decode_reboot_case_json failed\n",
                __func__, __LINE__);
            free(if_map);
            return RETURN_ERR;
        }

        if (m_ui_mgr.cci_report_resume_to_tda() != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: cci_report_resume_to_tda failed\n",
                __func__, __LINE__);
            free(if_map);
            return RETURN_ERR;
        }

        if (remove(CCI_TEST_REBOOT_STEP_CONFIG_JSON) != 0) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: remove upgrade_step_config.json failed\n", __func__, __LINE__);
            free(if_map);
            return RETURN_ERR;
        }

        m_ui_mgr.set_reboot_test_executed(true);
    }

    // wlan_emu_print(wlan_emu_log_level_info, "%s:%d: wlan emu msg collection started on platform
    // type: %d\n", __func__, __LINE__,
    //        get_platform_type());

    free(if_map);
    return 0;
}

wlan_emu_t::wlan_emu_t()
{
    m_state = wlan_emu_tests_state_cmd_none;
    m_ext_sta_mgr.m_bus_mgr = &m_bus_mgr;
    m_ext_sta_mgr.handle = &handle;
}

wlan_emu_t::~wlan_emu_t()
{
}
