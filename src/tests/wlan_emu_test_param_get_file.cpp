#include "wlan_emu_log.h"
#include "wlan_emu_test_params.h"
#include "wlan_emu_err_code.h"
#include <assert.h>
#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;

int test_step_param_get_file::step_execute()
{

    FILE *src, *dst;
    char *buff;

    test_step_params_t *step = this;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Called for Test Step Num : %d\n", __func__,
        __LINE__, step->step_number);

    if (access(step->u.get_file->source_file, F_OK) == -1) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: File %s is not present for step : %d\n",
            __func__, __LINE__, step->u.get_file->source_file, step->step_number);
        step->m_ui_mgr->cci_error_code = EFPRESENCE;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    if (copy_file(step->u.get_file->source_file, 0, step->u.get_file->dest_filename) !=
        RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: File copy failed from src %s to dest %s step : %d\n", __func__, __LINE__,
            step->u.get_file->source_file, step->u.get_file->dest_filename, step->step_number);
        step->m_ui_mgr->cci_error_code = EFCOPY;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }
    if (step->m_ui_mgr->step_upload_files(step->u.get_file->dest_filename) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: step_upload_files failed for step : %d\n",
            __func__, __LINE__, step->step_number);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        step->m_ui_mgr->cci_error_code = EPUSHTSTRESFILE;
        return RETURN_ERR;
    }

    wlan_emu_print(wlan_emu_log_level_err,
        "%s:%d: File copy succesful from src %s to dest %s step : %d\n", __func__, __LINE__,
        step->u.get_file->source_file, step->u.get_file->dest_filename, step->step_number);

    if (step->u.get_file->delete_source_file == true) {
        try {
            if (fs::is_regular_file(step->u.get_file->source_file)) {
                // Delete the file
                fs::remove(step->u.get_file->source_file);
            }
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Deleted the src file : %s step : %d\n",
                __func__, __LINE__, step->u.get_file->source_file, step->step_number);
        } catch (const fs::filesystem_error &ex) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Error deleting the file %s for %d\n",
                __func__, __LINE__, step->u.get_file->source_file, step->step_number);
            step->m_ui_mgr->cci_error_code = EFDELETE;
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }
    }

    step->test_state = wlan_emu_tests_state_cmd_results;
    return RETURN_OK;
}

int test_step_param_get_file::step_timeout()
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

void test_step_param_get_file::step_remove()
{
    test_step_param_get_file *step = dynamic_cast<test_step_param_get_file *>(this);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Destructor for command called\n", __func__,
        __LINE__);

    if (step == NULL) {
        return;
    }
    if (step->is_step_initialized == true) {
        delete step->u.get_file;
    }
    delete step;
    step = NULL;

    return;
}

int test_step_param_get_file::step_frame_filter(wlan_emu_msg_t *msg)
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

test_step_param_get_file::test_step_param_get_file()
{
    test_step_params_t *step = this;
    step->is_step_initialized = true;
    step->u.get_file = new (std::nothrow) get_file_t;
    if (step->u.get_file == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: allocation of memory for get_file failed for %d\n", __func__, __LINE__,
            step->step_number);
        step->is_step_initialized = false;
        return;
    }
    memset(step->u.get_file, 0, sizeof(get_file_t));
    step->execution_time = 5;
    step->timeout_count = 0;
    step->capture_frames = false;
    step->u.get_file->delete_source_file = false;
}

test_step_param_get_file::~test_step_param_get_file()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Destructor for command called\n", __func__,
        __LINE__);
}
