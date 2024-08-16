#include "wlan_emu.h"
#include "wlan_emu_tests.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <csignal>


extern "C" {
    INT wifi_hal_init();
    INT wifi_hal_getHalCapability(wifi_hal_capability_t *hal);
    INT wifi_hal_disconnect(INT ap_index);
}

wlan_emu_tests_state_t wlan_emu_t::m_state = wlan_emu_tests_state_cmd_wait;
wlan_emu_ui_mgr_t wlan_emu_t::m_ui_mgr;
wlan_emu_dml_tests_state_t wlan_emu_t::dml_state  = wlan_emu_dml_tests_state_idle;

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
    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: Received signal: %d m_state : %d\n", __func__, __LINE__, val, m_state);
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
    // wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Test program started on %d\n", __func__, __LINE__,
    //        get_platform_type());

    m_msg_mgr.start();
    m_sta_mgr.start();
//    std::signal(SIGTERM, StationDisconnectHandler);
    while (exit == false) {
        switch (get_state()) {
            case wlan_emu_tests_state_cmd_wait:
                dml_state = wlan_emu_dml_tests_state_idle;
                break;
            case wlan_emu_tests_state_cmd_request:
                //download the test and respective configs will be downloaded
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: in case wlan_emu_tests_state_cmd_request\n", __func__, __LINE__);
                if (m_ui_mgr.analyze_request() == RETURN_OK) {
                    //Download succesfull, trigger the start
                    m_ui_mgr.signal_downloaded_test_data();
                    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: analyze_request succesful\n", __func__, __LINE__);
                } else {
                    wlan_emu_print(wlan_emu_log_level_err, "%s:%d: analyze_request failed\n", __func__, __LINE__);
                    m_ui_mgr.cci_report_failure_to_tda();
                }
                dml_state = wlan_emu_dml_tests_state_running;
                break;
            case wlan_emu_tests_state_cmd_start:
                // here the execution of the test cases happend
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: in case wlan_emu_tests_state_cmd_start\n", __func__, __LINE__);
                if (start_test() == RETURN_OK) {
                    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: start_test succesful\n", __func__, __LINE__);
                } else {
                    wlan_emu_print(wlan_emu_log_level_err, "%s:%d: start_test failed\n", __func__, __LINE__);
                    m_ui_mgr.cci_report_failure_to_tda();
                }
                //Irrespective of success (or) failed the testcase needs to wait
                m_state = wlan_emu_tests_state_cmd_wait;
                dml_state = wlan_emu_dml_tests_state_running;
                break;
            case wlan_emu_tests_state_cmd_results:
                if (m_ui_mgr.upload_results() == RETURN_OK) {
                    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: upload_results succesful\n", __func__, __LINE__);
                    m_ui_mgr.signal_uploaded_test_results();
                    m_ui_mgr.cci_report_complete_to_tda();
                    dml_state = wlan_emu_dml_tests_state_complete_success;
                } else {
                    wlan_emu_print(wlan_emu_log_level_err, "%s:%d: upload_results failed\n", __func__, __LINE__);
                    m_ui_mgr.cci_report_failure_to_tda();
                    m_state = wlan_emu_tests_state_cmd_wait;
                    dml_state = wlan_emu_dml_tests_state_complete_failure;
                }
                break;
            case wlan_emu_tests_state_cmd_abort:
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d:\n", __func__, __LINE__);
                abort_test();
                m_state = wlan_emu_tests_state_cmd_wait;
                dml_state = wlan_emu_dml_tests_state_complete_failure;
                //exit = true; //cci needs to run all the time even upon failure
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

    tmp_test_cov_q =  m_ui_mgr.get_test_cov_cases_queue();

    count = queue_count(tmp_test_cov_q);
    if (count == 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: queue count is 0\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    for (i = 0; i < count; i++) {
        config = (wlan_emu_test_case_config *)queue_peek(tmp_test_cov_q, i);

        wlan_emu_print(wlan_emu_log_level_dbg,"%s:%d: config->test_steps_q : %d\n",
                __func__, __LINE__, queue_count(config->test_steps_q));

        config->test_state = wlan_emu_tests_state_cmd_start;

        switch (config->test_type) {
            case wlan_emu_test_1_subtype_radio:
                  test = new wlan_emu_tests_radio_t(&m_msg_mgr, &m_ui_mgr, &m_sta_mgr, config);
                  test->start();
            break;
            case wlan_emu_test_1_subtype_ns_private:
                  test = new wlan_emu_tests_private_vap_t(&m_msg_mgr, &m_ui_mgr, &m_sta_mgr, config);
                  test->start();
            break;
            case wlan_emu_test_1_subtype_ns_public_xfinity_open:
                  test = new wlan_emu_tests_xfinity_open_t(&m_msg_mgr, &m_ui_mgr, &m_sta_mgr, config);
                  test->start();
            break;
            case wlan_emu_test_1_subtype_ns_public_xfinity_secure:
                  test = new wlan_emu_tests_xfinity_secure_t(&m_msg_mgr, &m_ui_mgr, &m_sta_mgr, config);
                  test->start();
            break;
            case wlan_emu_test_1_subtype_ns_managed_xhs:
                  test = new wlan_emu_tests_xhs_t(&m_msg_mgr, &m_ui_mgr, &m_sta_mgr, config);
                  test->start();
            break;
            case wlan_emu_test_1_subtype_ns_managed_lnf_enterprise:
                  test = new wlan_emu_tests_lnf_enterprise_t(&m_msg_mgr, &m_ui_mgr, &m_sta_mgr, config);
                  test->start();
            break;
            case wlan_emu_test_1_subtype_ns_managed_lnf_secure:
                  test = new wlan_emu_tests_lnf_secure_t(&m_msg_mgr, &m_ui_mgr, &m_sta_mgr, config);
                  test->start();
            break;
            case wlan_emu_test_1_subtype_ns_managed_mesh_backhaul:
                  test = new wlan_emu_tests_mesh_backhaul_t(&m_msg_mgr, &m_ui_mgr, &m_sta_mgr, config);
                  test->start();
            break;
            case wlan_emu_test_1_subtype_ns_managed_mesh_client:
                  test = new wlan_emu_tests_mesh_client_t(&m_msg_mgr, &m_ui_mgr, &m_sta_mgr, config);
                  test->start();
            break;
            case wlan_emu_test_1_subtype_cc_probe_response:
                  test = new wlan_emu_tests_cli_probe_response(&m_msg_mgr, &m_ui_mgr, &m_sta_mgr, config);
                  test->start();
            break;
            case wlan_emu_test_1_subtype_cc_authentication:
                  test = new wlan_emu_tests_cli_auth(&m_msg_mgr, &m_ui_mgr, &m_sta_mgr, config);
                  test->start();
            break;
            case wlan_emu_test_3_subtype_pm_stats_get:
                  test = new wlan_emu_tests_stats_get(&m_msg_mgr, &m_ui_mgr, &m_sta_mgr, config);
                    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Config test type : wlan_emu_test_3_subtype_pm_stats_get\n", __func__, __LINE__);
                    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: test = new wlan_emu_tests_stats_get\n", __func__, __LINE__);
                  test->start();
            break;
            case wlan_emu_test_3_subtype_pm_stats_set:
                  test = new wlan_emu_tests_stats_set(&m_msg_mgr, &m_ui_mgr, &m_sta_mgr, config);
                    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Config test type : wlan_emu_test_3_subtype_pm_stats_set\n", __func__, __LINE__);
                    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: test = new wlan_emu_tests_stats_set\n", __func__, __LINE__);
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
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: VAP Test stopped\n", __func__, __LINE__);
}

#define STR_LEN 128
rbusError_t wlan_emu_t::set_cci_handler(rbusHandle_t handle, rbusProperty_t property, rbusSetHandlerOptions_t* opts)
{
    (void)opts;
    char const* name;
    rbusValue_t value;
    rbusValueType_t type;
    char parameter[STR_LEN] = {0};
    char const* pTmp = NULL;
    int len = 0;

    name = rbusProperty_GetName(property);
    value = rbusProperty_GetValue(property);
    type = rbusValue_GetType(value);

    if (!name) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s: Invalid Rbus property\n", __func__);
        return RBUS_ERROR_INVALID_INPUT;
    }

    memset(parameter, 0, STR_LEN);
    sscanf(name, "Device.WiFi.Tests.%200s", parameter);
    if(strstr(parameter, "TestConfigURL")) {
        if (type != RBUS_STRING) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s: Invalid Rbus property\n", __func__);
            return RBUS_ERROR_INVALID_INPUT;
        }
        pTmp = rbusValue_GetString(value, &len);
        strncpy(m_ui_mgr.get_tda_url(), pTmp, STR_LEN);
    }else if(strstr(parameter, "ResultsFileName")) {
        if (type != RBUS_STRING) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s: Invalid Rbus property\n", __func__);
            return RBUS_ERROR_INVALID_INPUT;
        }
        pTmp = rbusValue_GetString(value, &len);
        strncpy(m_ui_mgr.get_tda_output_file(), pTmp, STR_LEN);
    }else if(strstr(parameter, "Interface")) {
        if (type != RBUS_STRING) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s: Invalid Rbus property\n", __func__);
            return RBUS_ERROR_INVALID_INPUT;
        }
        pTmp = rbusValue_GetString(value, &len);
        strncpy(m_ui_mgr.get_tda_interface(), pTmp, STR_LEN);
    }else if(strstr(parameter, "SimulatedClientDevices")) {
        if (type != RBUS_UINT32) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s: Invalid Rbus property\n", __func__);
            return RBUS_ERROR_INVALID_INPUT;
        }
        int count = rbusValue_GetUInt32(value);
        if (count <= 0) {
            return RBUS_ERROR_INVALID_INPUT;
        }
        memcpy(m_ui_mgr.get_simulated_client_count(), &count, sizeof(int));
    }else if(strstr(parameter, "Start")) {
        if (type != RBUS_BOOLEAN) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s: Invalid Rbus property\n", __func__);
            return RBUS_ERROR_INVALID_INPUT;
        }
        bool start = rbusValue_GetBoolean(value);
        if (start) {
            m_ui_mgr.signal_input();
        }
    }
    return RBUS_ERROR_SUCCESS;
}

rbusError_t wlan_emu_t::get_cci_handler(rbusHandle_t handle, rbusProperty_t property, rbusGetHandlerOptions_t* opts)
{
    char const* name = rbusProperty_GetName(property);
    rbusValue_t value;
    char null_string[3] = {0};

    wlan_emu_print(wlan_emu_log_level_dbg, "%s: Rbus property=%s\n",__FUNCTION__,name);

    if (get_dml_state()  == wlan_emu_dml_tests_state_running) {
        wlan_emu_print(wlan_emu_log_level_info, "%s: Execution not allowed as test case is in running state\n",__FUNCTION__);
        return RBUS_ERROR_SUCCESS;
    }

    char extension[64] = {0};
    sscanf(name, "Device.WiFi.Tests.%s", extension);

    rbusValue_Init(&value);
    if (strstr(extension , "TestConfigURL")) {
        if (m_ui_mgr.get_tda_url()[0] == '\0') {
            rbusValue_SetString(value, null_string);
        } else {
            rbusValue_SetString(value, m_ui_mgr.get_tda_url());
        }
    } else if (strstr(extension, "ResultsFileName")) {
        if (m_ui_mgr.get_tda_output_file()[0] == '\0') {
            rbusValue_SetString(value, "results.txt");
        } else {
            rbusValue_SetString(value, m_ui_mgr.get_tda_output_file());
        }
    } else if (strstr(extension, "Interface")) {
        if (m_ui_mgr.get_tda_interface()[0] == '\0') {
            rbusValue_SetString(value, "erouter0");
        } else {
            rbusValue_SetString(value, m_ui_mgr.get_tda_interface());
        }
    }else if (strstr(extension, "SimulatedClientDevices")) {
        int count = 0;
        memcpy(&count, m_ui_mgr.get_simulated_client_count(), sizeof(int));
        wlan_emu_print(wlan_emu_log_level_dbg, "%s: count is %d\n", __func__, count);
        if (count == 0) {
            count = 100;
        }
        rbusValue_SetUInt32(value,  count);
    }else if (strstr(extension, "Start")) {
        rbusValue_SetBoolean(value, false);
    }else if (strstr(extension, "Status")) {
        rbusValue_SetUInt32(value, wlan_emu_t::get_dml_state());
    }

    rbusProperty_SetValue(property, value);
    rbusValue_Release(value);

    return RBUS_ERROR_SUCCESS;
}
    
void wlan_emu_t::rbus_register_handlers()
{
    int rc = RBUS_ERROR_SUCCESS;
    unsigned char num_of_radio = 0, index = 0;
    char *component_name = "Cci";
    rbusDataElement_t dataElements[] = {
        { CCI_TEST_CONFIG_URL, RBUS_ELEMENT_TYPE_PROPERTY,
        { wlan_emu_t::get_cci_handler, wlan_emu_t::set_cci_handler, NULL, NULL, NULL, NULL }},
        { CCI_TEST_RESULT_FILENAME, RBUS_ELEMENT_TYPE_PROPERTY,
        { wlan_emu_t::get_cci_handler, wlan_emu_t::set_cci_handler, NULL, NULL, NULL, NULL }},
        { CCI_TEST_INTERFACE, RBUS_ELEMENT_TYPE_PROPERTY,
        { wlan_emu_t::get_cci_handler, wlan_emu_t::set_cci_handler, NULL, NULL, NULL, NULL }},
        { CCI_TEST_SIMULATED_CLIENTDEVICES, RBUS_ELEMENT_TYPE_PROPERTY,
        { wlan_emu_t::get_cci_handler, wlan_emu_t::set_cci_handler, NULL, NULL, NULL, NULL }},
        { CCI_TEST_START, RBUS_ELEMENT_TYPE_PROPERTY,
        { wlan_emu_t::get_cci_handler, wlan_emu_t::set_cci_handler, NULL, NULL, NULL, NULL }},
        { CCI_TEST_STATUS, RBUS_ELEMENT_TYPE_PROPERTY,
        { wlan_emu_t::get_cci_handler, wlan_emu_t::set_cci_handler, NULL, NULL, NULL, NULL }},
    };

    rc = rbus_open(&this->rbus_handle, component_name);

    if (rc != RBUS_ERROR_SUCCESS) {
        wlan_emu_print(wlan_emu_log_level_dbg,"%s Rbus open failed\n",__FUNCTION__);
        return;
    }

    rc = rbus_regDataElements(this->rbus_handle, sizeof(dataElements)/sizeof(rbusDataElement_t), dataElements);
    if (rc != RBUS_ERROR_SUCCESS) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s rbus_regDataElements failed\n",__FUNCTION__);
        rbus_unregDataElements(this->rbus_handle, sizeof(dataElements)/sizeof(rbusDataElement_t), dataElements);
        rbus_close(this->rbus_handle);
    }

    return;
}

int wlan_emu_t::init()
{
    wifi_hal_capability_t *sta_cap = NULL;
    sta_cap = (wifi_hal_capability_t *)malloc(sizeof(wifi_hal_capability_t));
    if (sta_cap == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: malloc failed for sta_cap\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    memset(sta_cap, 0, sizeof(wifi_hal_capability_t));
    if (wifi_hal_init() != 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: hal init problem\n", __func__, __LINE__);
        free(sta_cap);
        return RETURN_ERR;
    } else if (wifi_hal_getHalCapability(sta_cap) != 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: platform capability get failed\n", __func__, __LINE__);
        free(sta_cap);
        return RETURN_ERR;
    }

    rbus_register_handlers();
    m_ui_mgr.init();
    if (m_sta_mgr.init(sta_cap) == RETURN_ERR) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: sta_mgr init failed\n", __func__, __LINE__);
        free(sta_cap);
        return RETURN_ERR;
    }
    // wlan_emu_print(wlan_emu_log_level_info, "%s:%d: wlan emu msg collection started on platform type: %d\n", __func__, __LINE__,
    //        get_platform_type());

    free(sta_cap);
    sta_cap = NULL;

    return 0;
}

wlan_emu_t::wlan_emu_t()
{
    m_state = wlan_emu_tests_state_cmd_none;
}

wlan_emu_t::~wlan_emu_t()
{

}
