#include "wlan_emu_log.h"
#include "wlan_emu_test_params.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>

void *test_step_param_logredirect::log_thread_function(void *arg)
{
    test_step_params_t *step = (test_step_params_t *)arg;
    fd_set log_fd_set;
    unsigned int fd_count = 0;
    unsigned char *buff;
    int ret = 0, bytes_read = 0;
    FILE *destination_file;

    FD_ZERO(&log_fd_set);

    FD_SET(step->u.log_capture->redirect_fd, &log_fd_set);
    if (fd_count < step->u.log_capture->redirect_fd) {
        fd_count = step->u.log_capture->redirect_fd;
    }

    if (lseek(step->u.log_capture->redirect_fd, 0, SEEK_END) == -1) {
        close(step->u.log_capture->redirect_fd);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return NULL;
    }

    buff = (unsigned char *)calloc(4096, sizeof(unsigned char));
    if (buff == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: buff allocation failed\n", __func__,
            __LINE__);
        return NULL;
    }

    while (step->u.log_capture->log_operation == log_operation_type_start) {
        if ((ret = select(fd_count + 1, &log_fd_set, NULL, NULL, NULL)) > 0) {
            if (FD_ISSET(step->u.log_capture->redirect_fd, &log_fd_set)) {
                memset(buff, 0, sizeof(buff));
                bytes_read = read(step->u.log_capture->redirect_fd, (unsigned char *)buff,
                    sizeof(buff));
                if (bytes_read > 0) {
                    destination_file = fopen(step->u.log_capture->log_result_file, "a");
                    if (destination_file == NULL) {
                        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: fopen failed for %s\n",
                            __func__, __LINE__, step->u.log_capture->log_result_file);
                        step->test_state = wlan_emu_tests_state_cmd_abort;
                        free(buff);
                        return NULL;
                    }
                    fputs((const char *)buff, destination_file);
                    fflush(destination_file);
                    fclose(destination_file);
                }
            }
        }
    }
    free(buff);
    return NULL;
}

int test_step_param_logredirect::step_execute()
{
    fd_set poll_set;
    FD_ZERO(&poll_set);
    test_step_params_t *step = this;
    char tmp_log_result_file[128] = { 0 };
    char timestamp[24] = { 0 };

    wlan_emu_print(wlan_emu_log_level_dbg,
        "%s:%d: Called for Test Step Num : %d log_operation : %d\n", __func__, __LINE__,
        step->step_number, step->u.log_capture->log_operation);

    if (step->u.log_capture->log_operation == log_operation_type_start) {
        if (step->test_state == wlan_emu_tests_state_cmd_start) {
            step->u.log_capture->redirect_fd = open(step->u.log_capture->log_redirect_filename,
                O_RDONLY);
            if (step->u.log_capture->redirect_fd == -1) {
                wlan_emu_print(wlan_emu_log_level_err, "open failed for step %d : %d\n",
                    step->step_number, errno);
                step->test_state = wlan_emu_tests_state_cmd_abort;
                return RETURN_ERR;
            }

            if (get_current_time_string(timestamp, sizeof(timestamp)) != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: get_current_time_string failed\n",
                    __func__, __LINE__);
                return RETURN_ERR;
            }

            snprintf(tmp_log_result_file, sizeof(tmp_log_result_file), "%s/%s_%d_%s_%s",
                step->m_ui_mgr->get_test_results_dir_path(), step->test_case_id, step->step_number,
                timestamp, step->u.log_capture->log_result_file);

            snprintf(step->u.log_capture->log_result_file,
                sizeof(step->u.log_capture->log_result_file), "%s", tmp_log_result_file);
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: log_result_file : %s \n", __func__,
                __LINE__, step->u.log_capture->log_result_file);

            if (pthread_create(&step->u.log_capture->thread_id, NULL,
                    test_step_param_logredirect::log_thread_function, this) != 0) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Thread create error for step : %d\n",
                    __func__, __LINE__, step->step_number);
                step->test_state = wlan_emu_tests_state_cmd_abort;
                return RETURN_ERR;
            }

            pthread_setname_np(step->u.log_capture->thread_id, "log_capture");
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: Thread created with Name : log_capture \n", __func__, __LINE__);

            if (pthread_detach(step->u.log_capture->thread_id)) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: thread detach failed\n", __func__,
                    __LINE__);
                return RETURN_ERR;
            }
            step->test_state = wlan_emu_tests_state_cmd_continue;
            return RETURN_OK;
        }
    } else {
        test_step_params_t *log_step = NULL;
        wlan_emu_test_case_config *test_case_config =
            (wlan_emu_test_case_config *)step->param_get_test_case_config();

        if (test_case_config == NULL) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: test_case_config is NULL for step : %d\n", __func__, __LINE__,
                step->step_number);
            return RETURN_ERR;
        }

        log_step = (test_step_params_t *)step->m_ui_mgr->get_step_from_step_number(test_case_config,
            step->u.log_capture->stop_step_number);
        if (log_step == NULL) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: Invalid log stop step  : %d in step : %d\n", __func__, __LINE__,
                step->u.log_capture->stop_step_number, step->step_number);
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }
        log_step->u.log_capture->log_operation = log_operation_type_stop;
        pthread_cancel(log_step->u.log_capture->thread_id);
        close(log_step->u.log_capture->redirect_fd);
        step->test_state = wlan_emu_tests_state_cmd_results;
        return RETURN_OK;
    }

    return RETURN_ERR;
}

int test_step_param_logredirect::step_timeout()
{

    test_step_params_t *step = this;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Test Step Num : %d timeout_count : %d\n",
        __func__, __LINE__, step->step_number, step->timeout_count);
    if (step->test_state == wlan_emu_tests_state_cmd_abort) {
        step->test_state = wlan_emu_tests_state_cmd_abort;
    } else {
        step->test_state = wlan_emu_tests_state_cmd_results;
    }
    return RETURN_OK;
}

int test_step_param_logredirect::step_upload_files(FILE *output_file, bool *update_to_tda)
{
    char *temp_res_file = NULL;
    char res_file_name[128] = { 0 };
    char *remote_test_results_loc = NULL;
    test_step_params_t *step = this;

    if (step->u.log_capture->log_result_file[0] != '\0') {

        remote_test_results_loc = step->m_ui_mgr->get_remote_test_results_loc();

        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: File: %s\n", __func__, __LINE__,
            step->u.log_capture->log_result_file);
        if (step->m_ui_mgr->upload_file_to_server(step->u.log_capture->log_result_file,
                remote_test_results_loc) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to upload %s\n", __func__,
                __LINE__, step->u.log_capture->log_result_file);
            return RETURN_ERR;
        } else {
            wlan_emu_print(wlan_emu_log_level_info, "%s:%d: uploaded %s\n", __func__, __LINE__,
                step->u.log_capture->log_result_file);
            *update_to_tda = true;
            temp_res_file = strdup(step->u.log_capture->log_result_file);
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

void test_step_param_logredirect::step_remove()
{
    test_step_param_logredirect *step = dynamic_cast<test_step_param_logredirect *>(this);
    if (step == NULL) {
        return;
    }
    if (step->is_step_initialized == true) {
        delete step->u.log_capture;
    }
    delete step;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Destructor for command called\n", __func__,
        __LINE__);
    step = NULL;

    return;
}

int test_step_param_logredirect::step_frame_filter(wlan_emu_msg_t *msg)
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

test_step_param_logredirect::test_step_param_logredirect()
{
    test_step_params_t *step = this;
    step->is_step_initialized = true;

    step->u.log_capture = new (std::nothrow) log_redirect_t;
    if (step->u.log_capture == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: allocation of memory for log failed for %d\n", __func__, __LINE__,
            step->step_number);
        step->is_step_initialized = false;
        return;
    }
    memset(step->u.log_capture, 0, sizeof(log_redirect_t));
    step->execution_time = 5;
    step->timeout_count = 0;
    step->capture_frames = false;
}

test_step_param_logredirect::~test_step_param_logredirect()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Destructor for logredirect called\n", __func__,
        __LINE__);
}
