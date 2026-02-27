#include "wlan_emu_log.h"
#include "wlan_emu_test_params.h"
#include "wlan_emu_err_code.h"
#include <assert.h>

webconfig_subdoc_data_t dml_data;
webconfig_subdoc_type_t subdoc_array[] = { webconfig_subdoc_type_private,
    webconfig_subdoc_type_home, webconfig_subdoc_type_xfinity, webconfig_subdoc_type_mesh,
    webconfig_subdoc_type_mesh_backhaul, webconfig_subdoc_type_lnf, webconfig_subdoc_type_radio,
    webconfig_subdoc_type_cac };

int test_step_param_dml_reset::step_execute()
{
    char *json_data;
    FILE *destination_file;
    int len = 0;
    int ret = 0;
    char file_to_read[128] = { 0 };
    unsigned int count = 0;

    webconfig_cci_t *webconfig_data = NULL;

    test_step_params_t *step = this;

    webconfig_data = step->m_ui_mgr->get_webconfig_data();

    memset(&dml_data, 0, sizeof(webconfig_subdoc_data_t));
    dml_data.descriptor = 0;
    dml_data.descriptor |= webconfig_data_descriptor_encoded;

    ret = snprintf(file_to_read, sizeof(file_to_read), "%s", step->u.test_webconfig_json);
    if ((ret < 0) || (ret >= sizeof(file_to_read))) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: snprintf failed return : %d input len : %d\n", __func__, __LINE__, ret,
            sizeof(file_to_read));
        step->m_ui_mgr->cci_error_code = ESNPRINTF;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    ret = step->m_ui_mgr->read_config_file(file_to_read, &json_data);
    if (ret != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: test_read_config_file failed\n", __func__,
            __LINE__);
        step->m_ui_mgr->cci_error_code = EFREAD;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    if (webconfig_decode(&webconfig_data->webconfig, &dml_data, json_data) ==
        webconfig_error_none) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s %d webconfig_decode success \n", __FUNCTION__,
            __LINE__);
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s %d webconfig_decode fail \n", __FUNCTION__,
            __LINE__);
        step->m_ui_mgr->cci_error_code = EWEBDECODE;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    for (count = 0; count < ARRAY_SIZE(subdoc_array); count++) {
        ret = webconfig_encode(&webconfig_data->webconfig, &dml_data, subdoc_array[count]);
        if (ret != webconfig_error_none) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: Encode failed for Test Step Num : %d for subdoc : %d\n", __func__, __LINE__,
                step->step_number, subdoc_array[count]);
            step->m_ui_mgr->cci_error_code = EENCODE;
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }

        json_data = dml_data.u.encoded.raw;
        ret = step->m_ui_mgr->bus_send(json_data, step->m_bus_mgr);
        if (ret != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: bus send failed for Test Step Num : %d for subdoc : %d\n", __func__,
                __LINE__, step->step_number, subdoc_array[count]);
            step->m_ui_mgr->cci_error_code = EBUS;
            step->test_state = wlan_emu_tests_state_cmd_abort;
            free(json_data);
            return RETURN_ERR;
        }
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: subdoc : %d\n", __func__, __LINE__,
            subdoc_array[count]);
        free(json_data);
    }

    if (count == ARRAY_SIZE(subdoc_array)) {
        step->test_state = wlan_emu_tests_state_cmd_results;
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Dml reset Succesful\n", __func__, __LINE__);
    } else {
        step->m_ui_mgr->cci_error_code = EWEBSET;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: Abort called for dml reset, execute update for %d from %d\n", __func__,
            __LINE__, count, ARRAY_SIZE(subdoc_array));
    }

    return RETURN_OK;
}

int test_step_param_dml_reset::step_timeout()
{
    test_step_params_t *step = this;
    if (step->test_state != wlan_emu_tests_state_cmd_results) {
        step->m_ui_mgr->cci_error_code = ESTEPTIMEOUT;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Abort for step : %d\n", __func__, __LINE__,
            step->step_number);
    }

    return RETURN_OK;
}

void test_step_param_dml_reset::step_remove()
{
    test_step_param_dml_reset *step = dynamic_cast<test_step_param_dml_reset *>(this);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Destructor for dml called\n", __func__,
        __LINE__);
    if (step == NULL) {
        return;
    }
    memset(&dml_data, 0, sizeof(webconfig_subdoc_data_t));
    delete step;
    step = NULL;

    return;
}

int test_step_param_dml_reset::step_frame_filter(wlan_emu_msg_t *msg)
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

test_step_param_dml_reset::test_step_param_dml_reset()
{
    test_step_params_t *step = this;
    step->is_step_initialized = true;
    step->execution_time = 5;
    step->timeout_count = 0;
    step->capture_frames = false;
}

test_step_param_dml_reset::~test_step_param_dml_reset()
{
    test_step_params_t *step = this;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: current_test_step : %d\n", __func__, __LINE__,
        step->step_number);
}
