#include <assert.h>
#include "wlan_emu_log.h"
#include "wlan_emu_test_params.h"
#include <filesystem>

namespace fs = std::filesystem;

int test_step_param_get_pattern_files::step_execute()
{

    std::string file_list;
    char *temp_file_info;
    unsigned int file_pattern_len = 0;
    unsigned int temp_file_info_size = 0;

    test_step_params_t *step = this;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Called for Test Step Num : %d\n",
            __func__, __LINE__, step->step_number);

    if (access(step->u.get_pattern_files->file_location, F_OK) == -1) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Directory %s is not present for step : %d\n",
                __func__, __LINE__, step->u.get_pattern_files->file_location, step->step_number);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    file_pattern_len = strlen(step->u.get_pattern_files->file_pattern);
    if (file_pattern_len == 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid string %s for step : %d\n",
                __func__, __LINE__, step->u.get_pattern_files->file_pattern, step->step_number);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    temp_file_info_size = sizeof(step->u.get_pattern_files->file_location) + sizeof(step->u.get_pattern_files->file_pattern);

    try {
        // Iterate over each file in the directory
        for (const auto& entry : fs::directory_iterator(step->u.get_pattern_files->file_location)) {
            // Check if the current entry is a regular file
            if (fs::is_regular_file(entry.path())) {

                file_list = entry.path().filename().string();

                int result = file_list.compare(0, file_pattern_len, step->u.get_pattern_files->file_pattern, 0, file_pattern_len);
                if (result == 0) {
                    temp_file_info = new(std::nothrow) char[temp_file_info_size];
                    if (temp_file_info == nullptr) {
                        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: memory allocation failed for step : %d\n",
                                __func__, __LINE__, step->step_number);
                        step->test_state = wlan_emu_tests_state_cmd_abort;
                        return RETURN_ERR;
                    }
                    snprintf(temp_file_info, temp_file_info_size, "%s/%s", step->u.get_pattern_files->file_location, entry.path().filename().string().c_str());
                    wlan_emu_print(wlan_emu_log_level_err, "%s:%d: file string is : %s\n",
                            __func__, __LINE__, temp_file_info);
                    queue_push(step->u.get_pattern_files->get_pattern_files_queue, temp_file_info);
                }
            }
        }
    } catch (const fs::filesystem_error& ex) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: File system error for step : %d\n",
                __func__, __LINE__, step->step_number);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    step->test_state = wlan_emu_tests_state_cmd_results;
    return RETURN_OK;
}

int test_step_param_get_pattern_files::step_timeout()
{

    test_step_params_t *step = this;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Test Step Num : %d timeout_count : %d\n",
            __func__, __LINE__, step->step_number, step->timeout_count);

    if (step->test_state != wlan_emu_tests_state_cmd_results) {
        step->test_state = wlan_emu_tests_state_cmd_abort;
    }
    return RETURN_OK;
}


int test_step_param_get_pattern_files::step_upload_files(FILE *output_file, bool *update_to_tda)
{
    char *temp_res_file = NULL;
    char *res_file_name = {0};
    char *remote_test_results_loc = NULL;
    test_step_params_t *step = this;

    remote_test_results_loc = step->m_ui_mgr->get_remote_test_results_loc();
    res_file_name = (char *)queue_pop(step->u.get_pattern_files->get_pattern_files_queue);

    while(res_file_name != NULL) {
        if (res_file_name != NULL) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: File: %s\n", __func__, __LINE__, res_file_name);
            if (step->m_ui_mgr->upload_file_to_server(res_file_name, remote_test_results_loc) != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to upload %s\n", __func__, __LINE__, res_file_name);
                pthread_mutex_unlock(&step->s_lock);
                return RETURN_ERR;
            } else {
                wlan_emu_print(wlan_emu_log_level_info, "%s:%d: uploaded %s\n", __func__, __LINE__, res_file_name);
                if (step->u.get_pattern_files->delete_pattern_files == true) {
                    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Deleted %s\n", __func__, __LINE__, res_file_name);
                    fs::remove(res_file_name);
                }
                *update_to_tda = true;
                temp_res_file = strdup(res_file_name);
                if (step->m_ui_mgr->get_last_substring_after_slash(temp_res_file, res_file_name, 128) != RETURN_OK) {
                    wlan_emu_print(wlan_emu_log_level_err, "%s:%d: get_last_substring_after_slash failed for str : %s\n",
                            __func__, __LINE__, temp_res_file);
                    free(temp_res_file);
                    pthread_mutex_unlock(&step->s_lock);
                    return RETURN_ERR;
                }
                fprintf(output_file, "%s\n", res_file_name);
                free(temp_res_file);
            }
            free(res_file_name);
        }
        res_file_name = (char *)queue_pop(step->u.get_pattern_files->get_pattern_files_queue);
    }

    return RETURN_OK;
}


void test_step_param_get_pattern_files::step_remove()
{
    test_step_param_get_pattern_files *step = dynamic_cast<test_step_param_get_pattern_files *>(this);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Destructor for command called\n", __func__, __LINE__);

    if (step == NULL) {
        return;
    }
    if (step->is_step_initialized == true) {
        if (step->u.get_pattern_files->get_pattern_files_queue != nullptr) {
            queue_destroy(step->u.get_pattern_files->get_pattern_files_queue);
            step->u.get_pattern_files->get_pattern_files_queue = nullptr;
        }
        delete step->u.get_pattern_files;
    }
    delete step;
    step = NULL;

    return;
}

int test_step_param_get_pattern_files::step_frame_filter(wlan_emu_msg_t *msg)
{
    test_step_params_t *step = this;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step number : %d\n", __func__, __LINE__, step->step_number);
    if (msg == NULL) {
        return RETURN_UNHANDLED;
    }
    switch (msg->get_msg_type()) {
        case wlan_emu_msg_type_webconfig://onewifi_webconfig
        case wlan_emu_msg_type_cfg80211: //beacon
        case wlan_emu_msg_type_frm80211: //mgmt
        default:
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Not supported msg_type : %d\n", __func__, __LINE__, msg->get_msg_type());
        break;
    }
    return RETURN_UNHANDLED;
}

test_step_param_get_pattern_files::test_step_param_get_pattern_files()
{
    test_step_params_t *step = this;
    step->is_step_initialized = true;
    step->u.get_pattern_files = new(std::nothrow) get_pattern_files_t;
    if (step->u.get_pattern_files == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: allocation of memory for get_pattern_files failed for %d\n",
                __func__, __LINE__, step->step_number);
        step->is_step_initialized = false;
    }
    memset(step->u.get_pattern_files, 0, sizeof(get_pattern_files_t));

    step->u.get_pattern_files->get_pattern_files_queue = queue_create();
    if (step->u.get_pattern_files->get_pattern_files_queue == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: queue create failed for pattern files for %d\n",
                __func__, __LINE__, step->step_number);
        delete step->u.get_pattern_files;
        step->is_step_initialized = false;
    }

    step->execution_time = 5;
    step->timeout_count = 0;
    step->capture_frames = false;
    step->u.get_pattern_files->delete_pattern_files = false;
}

test_step_param_get_pattern_files::~test_step_param_get_pattern_files()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Destructor for command called\n", __func__, __LINE__);
}
