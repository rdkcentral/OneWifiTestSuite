#include "wlan_emu_log.h"
#include "wlan_emu_test_params.h"
#include <assert.h>

int test_step_param_command::step_execute()
{

    FILE *fp, *destination_file;
    char *buff;
    char log_file[128] = { 0 };
    char timestamp[24] = { 0 };

    bool is_file_capture_required = false;

    test_step_params_t *step = this;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Called for Test Step Num : %d\n", __func__,
        __LINE__, step->step_number);

    fp = popen(step->u.cmd->test_cmd, "r");
    if (fp == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: popen failed for %s\n", __func__, __LINE__,
            step->u.cmd->test_cmd);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    if (step->u.cmd->cmd_exec_log_filename[0] != '\0') {
        is_file_capture_required = true;

        if (get_current_time_string(timestamp, sizeof(timestamp)) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: get_current_time_string failed\n",
                __func__, __LINE__);
            return RETURN_ERR;
        }

        snprintf(log_file, sizeof(log_file), "%s/%s_%d_%s_%s",
            step->m_ui_mgr->get_test_results_dir_path(), step->test_case_id, step->step_number,
            timestamp, step->u.cmd->cmd_exec_log_filename);

        snprintf(step->u.cmd->cmd_exec_log_filename, sizeof(step->u.cmd->cmd_exec_log_filename),
            "%s", log_file);

        destination_file = fopen(step->u.cmd->cmd_exec_log_filename, "w");
        if (destination_file == NULL) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: fopen failed for %s\n", __func__,
                __LINE__, step->u.cmd->cmd_exec_log_filename);
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }
    }

    buff = (char *)calloc(4096, sizeof(char));
    if (buff == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: buff allocation failed\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    // Read and print the output of the cmd
    while (fgets(buff, sizeof(buff), fp) != NULL) {
        if (is_file_capture_required == true) {
            fputs(buff, destination_file);
        }
    }

    pclose(fp);

    if (is_file_capture_required == true) {
        fclose(destination_file);
    }
    step->test_state = wlan_emu_tests_state_cmd_results;
    free(buff);

    return RETURN_OK;
}

int test_step_param_command::step_timeout()
{

    test_step_params_t *step = this;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Test Step Num : %d timeout_count : %d\n",
        __func__, __LINE__, step->step_number, step->timeout_count);

    if (step->test_state != wlan_emu_tests_state_cmd_results) {
        step->test_state = wlan_emu_tests_state_cmd_abort;
    }
    return RETURN_OK;
}

int test_step_param_command::step_upload_files(FILE *output_file, bool *update_to_tda)
{
    char *temp_res_file = NULL;
    char res_file_name[128] = { 0 };
    char *remote_test_results_loc = NULL;
    test_step_params_t *step = this;

    if (step->u.cmd->cmd_exec_log_filename[0] != '\0') {

        remote_test_results_loc = step->m_ui_mgr->get_remote_test_results_loc();

        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: File: %s\n", __func__, __LINE__,
            step->u.cmd->cmd_exec_log_filename);
        if (step->m_ui_mgr->upload_file_to_server(step->u.cmd->cmd_exec_log_filename,
                remote_test_results_loc) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to upload %s\n", __func__,
                __LINE__, step->u.cmd->cmd_exec_log_filename);
            return RETURN_ERR;
        } else {
            wlan_emu_print(wlan_emu_log_level_info, "%s:%d: uploaded %s\n", __func__, __LINE__,
                step->u.cmd->cmd_exec_log_filename);
            *update_to_tda = true;
            temp_res_file = strdup(step->u.cmd->cmd_exec_log_filename);
            if (step->m_ui_mgr->get_last_substring_after_slash(temp_res_file, res_file_name,
                    sizeof(res_file_name)) != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: get_last_substring_after_slash failed for str : %s\n", __func__,
                    __LINE__, temp_res_file);
                free(temp_res_file);
                return RETURN_ERR;
            }
            fprintf(output_file, "%s\n", res_file_name);
            free(temp_res_file);
        }
    }

    return RETURN_OK;
}

void test_step_param_command::step_remove()
{
    test_step_param_command *step = dynamic_cast<test_step_param_command *>(this);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Destructor for command called\n", __func__,
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

int test_step_param_command::step_frame_filter(wlan_emu_msg_t *msg)
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

test_step_param_command::test_step_param_command()
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

test_step_param_command::~test_step_param_command()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Destructor for command called\n", __func__,
        __LINE__);
}
