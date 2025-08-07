#include "wlan_emu_test_params.h"
#include "wlan_emu_log.h"
#include "wlan_emu_err_code.h"
#include "cci_wifi_utils.hpp"
#include <secure_wrapper.h>

int test_step_param_gateway_performance::write_to_file(FILE *out_file, FILE *in_file)
{
    char buffer[4096];
    char *res_file = NULL;
    char separator[] = "==========\n";
    size_t n;

    if (!out_file) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Outfile is pointer is NULL\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }
    while ((n = fread(buffer, 1, sizeof(buffer), in_file)) > 0) {
        if (fwrite(buffer, 1, n, out_file) != n) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: fwrite failed\n", __func__, __LINE__);
            return RETURN_ERR;
        }
    }
    if (fwrite(separator, 1, strlen(separator), out_file) != strlen(separator)) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to write separator\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }
    return RETURN_OK;
}

int test_step_param_gateway_performance::get_process_status(char *process_name, FILE *out)
{
    char pid[8] = { 0 };
    char proc_status_path[64] = { 0 };
    FILE *fp_pid = NULL, *fp_status = NULL;

    // ps | grep OneWifi | grep -v grep | awk '{print $1}'
    fp_pid = v_secure_popen("r", "%s | \"%s\" \"%s\" | \"%s\" \"%s\" \"%s\" | \"%s\" '{print $1}'",
        "ps", "grep", process_name, "grep", "-v", "grep", "awk");
    if (fp_pid == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: popen failed for %s\n", __func__, __LINE__,
            process_name);
        return RETURN_ERR;
    }

    if (fgets(pid, sizeof(pid) - 1, fp_pid) == NULL) {
        if (feof(fp_pid)) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Command produced no output\n", __func__,
                __LINE__);
        } else if (ferror(fp_pid)) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Error reading command output\n",
                __func__, __LINE__);
        }
        v_secure_pclose(fp_pid);
        return RETURN_ERR;
    }
    v_secure_pclose(fp_pid);

    pid[strcspn(pid, "\r\n")] = 0;
    snprintf(proc_status_path, sizeof(proc_status_path), "/proc/%s/status", pid);
    fp_status = v_secure_popen("r", "%s \"%s\"", "cat", proc_status_path);
    if (fp_status == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: popen failed for %s\n", __func__, __LINE__,
            proc_status_path);
        return RETURN_ERR;
    }

    if (test_step_param_gateway_performance::write_to_file(out, fp_status) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to write the output to file\n",
            __func__, __LINE__);
        v_secure_pclose(fp_status);
        return RETURN_ERR;
    }
    v_secure_pclose(fp_status);

    return RETURN_OK;
}

void *test_step_param_gateway_performance::performance_log(void *arg)
{
    test_step_params_t *step = (test_step_params_t *)arg;
    FILE *fp_process = NULL;
    char *res_file = NULL;
    char *process_name = NULL;
    char timestamp[24] = { 0 };
    int iteration = 0;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: iteration is %d and step_iteration is %d\n",
        __func__, __LINE__, iteration, step->u.gw_performance->iteration);
    while (iteration < step->u.gw_performance->iteration) {
        if (get_current_time_string(timestamp, sizeof(timestamp)) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: get_current_time_string failed\n",
                __func__, __LINE__);
            step->m_ui_mgr->cci_error_code = ETIMESTAMP;
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return NULL;
        }

        step->u.gw_performance->result_file = std::string("/tmp/cci_res/") + step->test_case_id +
            "_" + std::to_string(step->step_number) + "_" + timestamp + "_" +
            step->u.gw_performance->result_file_name + "_" + std::to_string(iteration) + ".txt";

        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: The result_file name is %s\n", __func__,
            __LINE__, step->u.gw_performance->result_file.c_str());

        FILE *out = fopen(step->u.gw_performance->result_file.c_str(), "a+");
        if (!out) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to open the result file %s\n",
                __func__, __LINE__, step->u.gw_performance->result_file.c_str());
            step->m_ui_mgr->cci_error_code = EFOPEN;
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return NULL;
        }

        if (step->u.gw_performance->cmd_option == cmd_option_cpu) {
            fp_process = v_secure_popen("r", "%s \"%s\" \"%s\"", "top", "-b", "-n1");
        } else if (step->u.gw_performance->cmd_option == cmd_option_mem) {
            fp_process = v_secure_popen("r", "%s \"%s\" \"%s\"", "top", "-bm", "-n1");
        } else {
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return NULL;
        }

        if (fp_process == NULL) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to open pipe\n", __func__,
                __LINE__);
            fclose(out);
            step->m_ui_mgr->cci_error_code = EPOPEN;
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return NULL;
        }

        if (test_step_param_gateway_performance::write_to_file(out, fp_process) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to write the output to file\n",
                __func__, __LINE__);
            v_secure_pclose(fp_process);
            fclose(out);
            step->m_ui_mgr->cci_error_code = EFWRITE;
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return NULL;
        }
        v_secure_pclose(fp_process);

        if (test_step_param_gateway_performance::get_process_status(const_cast<char *>("OneWifi"),
                out) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to get the process status\n",
                __func__, __LINE__);

            fclose(out);
            step->m_ui_mgr->cci_error_code = EPROCSTATUS;
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return NULL;
        }
        int qcount = queue_count(step->u.gw_performance->process_status);
        if (qcount > 0) {
            for (int itr = 0; itr < qcount; itr++) {
                process_name = (char *)queue_peek(step->u.gw_performance->process_status, itr);
                if (process_name != NULL) {
                    if (test_step_param_gateway_performance::get_process_status(process_name,
                            out) != RETURN_OK) {
                        wlan_emu_print(wlan_emu_log_level_err,
                            "%s:%d: failed to get the process status\n", __func__, __LINE__);

                        fclose(out);
                        step->m_ui_mgr->cci_error_code = EPROCSTATUS;
                        step->test_state = wlan_emu_tests_state_cmd_abort;
                        return NULL;
                    }
                } else {
                    wlan_emu_print(wlan_emu_log_level_err,
                        "%s:%d failed to get process_name from queue\n", __func__, __LINE__);
                }
            }
        }
        fclose(out);

        res_file = strdup(step->u.gw_performance->result_file.c_str());
        if (step->m_ui_mgr->step_upload_files(res_file) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: step_upload_files failed for %s\n",
                __func__, __LINE__, res_file);
            free(res_file);
            step->test_state = wlan_emu_tests_state_cmd_abort;
            step->m_ui_mgr->cci_error_code = EPUSHTSTRESFILE;
            return NULL;
        }
        iteration++;
        // Skip wait for last iteration
        if (iteration < step->u.gw_performance->iteration) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Waiting for %d duration\n", __func__,
                __LINE__, step->u.gw_performance->interval * 1000);
            WaitForDuration(step->u.gw_performance->interval * 1000); // Time in millisec
        }
    }
    step->test_state = wlan_emu_tests_state_cmd_results;
    return NULL;
}

int test_step_param_gateway_performance::step_execute()
{
    test_step_params_t *step = this;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Called for Test Step Num : %d\n", __func__,
        __LINE__, step->step_number);

    if (step->test_state == wlan_emu_tests_state_cmd_start) {
        // create a thread
        if (pthread_create(&step->u.gw_performance->process_tid, NULL,
                test_step_param_gateway_performance::performance_log, this) != 0) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Thread create error for step : %d\n",
                __func__, __LINE__, step->step_number);
            step->m_ui_mgr->cci_error_code = ETHREAD;
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }

        pthread_setname_np(step->u.gw_performance->process_tid, "gateway_performance");

        if (pthread_detach(step->u.gw_performance->process_tid)) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: thread detach failed\n", __func__,
                __LINE__);
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

int test_step_param_gateway_performance::step_timeout()
{
    test_step_params_t *step = this;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Test Step Num : %d step_state %d\n", __func__,
        __LINE__, step->step_number, step->test_state);

    if (step->test_state == wlan_emu_tests_state_cmd_results) {
        wlan_emu_print(wlan_emu_log_level_info, "%s:%d: Test completed for step %d\n", __func__,
            __LINE__, step->step_number);
        return RETURN_OK;
    } else if (step->test_state == wlan_emu_tests_state_cmd_abort) {
        step->m_ui_mgr->cci_error_code = ESTEPTIMEOUT;
        wlan_emu_print(wlan_emu_log_level_info, "%s:%d: Test aborted for step %d\n", __func__,
            __LINE__, step->step_number);
        pthread_cancel(step->u.gw_performance->process_tid);
        return RETURN_ERR;
    } else {
        if (step->fork == true) {
            step->test_state = wlan_emu_tests_state_cmd_wait;
        } else {
            step->test_state = wlan_emu_tests_state_cmd_continue;
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: continue for step : %d\n", __func__,
                __LINE__, step->step_number);
        }
    }

    return RETURN_OK;
}

void test_step_param_gateway_performance::step_remove()
{
    test_step_params_t *step = dynamic_cast<test_step_params_t *>(this);

    if (step == NULL) {
        return;
    }

    if (step->is_step_initialized == true) {
        if (step->u.gw_performance->process_status != nullptr) {
            queue_destroy(step->u.gw_performance->process_status);
            step->u.gw_performance->process_status = nullptr;
        }
    }
    delete step;
    step = NULL;
    return;
}

int test_step_param_gateway_performance::step_frame_filter(wlan_emu_msg_t *msg)
{
    return RETURN_UNHANDLED;
}

test_step_param_gateway_performance::test_step_param_gateway_performance()
{
    test_step_params_t *step = this;
    step->is_step_initialized = true;

    step->u.gw_performance = new (std::nothrow) gw_performance_t;
    if (step->u.gw_performance == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d Allocation failed for gateway performance\n",
            __func__, __LINE__);
        step->is_step_initialized = false;
        return;
    }
    step->u.gw_performance->interval = 1;
    step->u.gw_performance->iteration = 1;
    step->timeout_count = 0;
    step->capture_frames = false;
    step->u.gw_performance->cmd_option = cmd_option_mem;
    step->u.gw_performance->process_status = queue_create();
    if (step->u.gw_performance->process_status == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to create queue\n", __func__,
            __LINE__);
        step->is_step_initialized = false;
        return;
    }
}

test_step_param_gateway_performance::~test_step_param_gateway_performance()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Destructor for gateway performance\n", __func__,
        __LINE__);
}
