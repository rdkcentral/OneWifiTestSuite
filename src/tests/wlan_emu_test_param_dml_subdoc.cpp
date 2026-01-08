#include "wlan_emu_log.h"
#include "wlan_emu_test_params.h"
#include "wlan_emu_bus.h"
#include "wlan_emu_err_code.h"
#include <assert.h>

int test_step_param_dmlsubdoc::step_execute()
{
    int rc = bus_error_success;
    char *str;
    FILE *destination_file;
    int len = 0;
    raw_data_t bus_data;

    webconfig_cci_t *webconfig_data = NULL;

    test_step_params_t *step = this;

    memset(&bus_data, 0, sizeof(raw_data_t));
    webconfig_data = step->m_ui_mgr->get_webconfig_data();

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Called for Test Step Num : %d\n", __func__,
        __LINE__, step->step_number);
    step->frame_request.msg_type |= 1 << wlan_emu_msg_type_webconfig;
    step->frame_request.subdoc_type = webconfig_subdoc_type_dml;

    rc = step->m_bus_mgr->desc.bus_data_get_fn(&webconfig_data->handle, WIFI_WEBCONFIG_INIT_DML_DATA,
        &bus_data);
    if (rc != bus_error_success) {
        wlan_emu_print(wlan_emu_log_level_err, "bus_get failed for [%s] with error [%d]\n",
            WIFI_WEBCONFIG_INIT_DML_DATA, rc);
        step->m_ui_mgr->cci_error_code = EBUS;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    wlan_emu_print(wlan_emu_log_level_dbg,
        "%s:%d bus_get WIFI_WEBCONFIG_INIT_DML_DATA successfull \n", __FUNCTION__, __LINE__);

    if (bus_data.data_type != bus_data_type_string || bus_data.raw_data.bytes == NULL ||
        bus_data.raw_data_len == 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d Null pointer,bus set string len=%d\n",
            __FUNCTION__, __LINE__, len);
        step->m_ui_mgr->cci_error_code = EBUS;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    str = new char[bus_data.raw_data_len + 1]();
    strncpy(str, (char *)bus_data.raw_data.bytes, bus_data.raw_data_len);
    len = bus_data.raw_data_len;

    destination_file = fopen(step->u.cmd->cmd_exec_log_filename, "w");
    if (destination_file == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: fopen failed for %s\n", __func__, __LINE__,
            step->u.cmd->cmd_exec_log_filename);
        step->m_ui_mgr->cci_error_code = EFOPEN;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        free(str);
        return RETURN_ERR;
    }

    fputs(str, destination_file);
    fclose(destination_file);
    if (step->m_ui_mgr->step_upload_files(step->u.cmd->cmd_exec_log_filename) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: step_upload_files failed\n", __func__,
            __LINE__);
        free(str);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        step->m_ui_mgr->cci_error_code = EPUSHTSTRESFILE;
        return RETURN_ERR;
    }
    free(str);

    step->test_state = wlan_emu_tests_state_cmd_results;
    return RETURN_OK;
}

int test_step_param_dmlsubdoc::step_timeout()
{

    test_step_params_t *step = this;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Test Step Num : %d timeout_count : %d\n",
        __func__, __LINE__, step->step_number, step->timeout_count);
    if (step->test_state != wlan_emu_tests_state_cmd_results) {
        step->m_ui_mgr->cci_error_code = ESTEPTIMEOUT;
        step->test_state = wlan_emu_tests_state_cmd_abort;
    }
    return RETURN_OK;
}

void test_step_param_dmlsubdoc::step_remove()
{
    test_step_param_dmlsubdoc *step = dynamic_cast<test_step_param_dmlsubdoc *>(this);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Destructor for dml called\n", __func__,
        __LINE__);
    if (step == NULL) {
        return;
    }
    if (step->is_step_initialized == true) {
        delete step->u.cmd;
    }
    delete step;
    step = NULL;

    return;
}

int test_step_param_dmlsubdoc::step_frame_filter(wlan_emu_msg_t *msg)
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

test_step_param_dmlsubdoc::test_step_param_dmlsubdoc()
{
    test_step_params_t *step = this;
    step->is_step_initialized = true;
    step->u.cmd = new (std::nothrow) command_t;
    if (step->u.cmd == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: allocation of memory for cmd failed for %d\n", __func__, __LINE__,
            step->step_number);
        step->is_step_initialized = false;
        return;
    }
    memset(step->u.cmd, 0, sizeof(command_t));
    step->execution_time = 5;
    step->timeout_count = 0;
    step->capture_frames = false;
}

test_step_param_dmlsubdoc::~test_step_param_dmlsubdoc()
{
    test_step_params_t *step = this;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: current_test_step : %d\n", __func__, __LINE__,
        step->step_number);
}
