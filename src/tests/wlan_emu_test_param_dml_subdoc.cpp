#include <assert.h>
#include "wlan_emu_log.h"
#include "wlan_emu_test_params.h"

int test_step_param_dmlsubdoc::step_execute()
{
    int rc = RBUS_ERROR_SUCCESS;
    rbusValue_t value;
    const char *str;
    FILE *destination_file;
    int len = 0;

    webconfig_cci_t  *webconfig_data = NULL;


    test_step_params_t *step = this;

    webconfig_data = step->m_ui_mgr->get_webconfig_data();

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Called for Test Step Num : %d\n",
                             __func__, __LINE__, step->step_number);
    step->subdoc_type = webconfig_subdoc_type_dml;

    rc = rbus_get(webconfig_data->rbus_handle, WIFI_WEBCONFIG_INIT_DML_DATA, &value);
    if (rc != RBUS_ERROR_SUCCESS) {
        wlan_emu_print(wlan_emu_log_level_err,  "rbus_get failed for [%s] with error [%d]\n", WIFI_WEBCONFIG_INIT_DML_DATA, rc);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d rbus_get WIFI_WEBCONFIG_INIT_DML_DATA successfull \n",__FUNCTION__,__LINE__ );
    str = rbusValue_GetString(value, &len);
    if (str == NULL) {
        wlan_emu_print(wlan_emu_log_level_err,"%s:%d Null pointer,Rbus set string len=%d\n",__FUNCTION__, __LINE__, len);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    destination_file = fopen(step->u.cmd->cmd_exec_log_filename, "w");
    if (destination_file == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: fopen failed for %s\n",
                __func__, __LINE__, step->u.cmd->cmd_exec_log_filename);
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }


    fputs(str, destination_file);
    fclose(destination_file);

    step->test_state = wlan_emu_tests_state_cmd_results;
    return RETURN_OK;
}

int test_step_param_dmlsubdoc::step_timeout()
{

    test_step_params_t *step = this;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Test Step Num : %d timeout_count : %d\n",
                     __func__, __LINE__, step->step_number, step->timeout_count);
    if (step->test_state != wlan_emu_tests_state_cmd_results) {
        step->test_state = wlan_emu_tests_state_cmd_abort;
    }
    return RETURN_OK;
}

int test_step_param_dmlsubdoc::step_upload_files(FILE *output_file, bool *update_to_tda)
{
    char *temp_res_file = NULL;
    char res_file_name[128] = {0};
    char *remote_test_results_loc = NULL;
    test_step_params_t *step = this;
    if (step->u.cmd->cmd_exec_log_filename[0] != '\0') {

        remote_test_results_loc = step->m_ui_mgr->get_remote_test_results_loc();

        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: File: %s\n", __func__, __LINE__, step->u.cmd->cmd_exec_log_filename);
        if (step->m_ui_mgr->upload_file_to_server(step->u.cmd->cmd_exec_log_filename, remote_test_results_loc) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to upload %s\n", __func__, __LINE__, step->u.cmd->cmd_exec_log_filename);
            return RETURN_ERR;
        } else {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: uploaded %s\n", __func__, __LINE__, step->u.cmd->cmd_exec_log_filename);
            *update_to_tda = true;
            temp_res_file = strdup(step->u.cmd->cmd_exec_log_filename);
            if (step->m_ui_mgr->get_last_substring_after_slash(temp_res_file, res_file_name, sizeof(res_file_name)) != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: get_last_substring_after_slash failed for str : %s\n",
                        __func__, __LINE__, temp_res_file);
                free(temp_res_file);
                return RETURN_ERR;
            }
            fprintf(output_file, "%s\n", res_file_name);
            free(temp_res_file);
        }

    }

    return RETURN_OK;
}

void test_step_param_dmlsubdoc::step_remove()
{
    test_step_param_dmlsubdoc *step = dynamic_cast<test_step_param_dmlsubdoc *>(this);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Destructor for dml called\n", __func__, __LINE__);
    if (step == NULL) {
        return;
    }
    delete step->u.cmd;
    delete step;
    step = NULL;

    return;
}

test_step_param_dmlsubdoc::test_step_param_dmlsubdoc()
{
    test_step_params_t *step = this;
    step->execution_time = 5;
    step->timeout_count = 0;
    step->capture_frames = false;
}

test_step_param_dmlsubdoc::~test_step_param_dmlsubdoc()
{
    test_step_params_t *step = this;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: current_test_step : %d\n",
            __func__, __LINE__, step->step_number);
}

