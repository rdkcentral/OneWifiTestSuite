#include "wlan_emu_log.h"
#include "wlan_ext_emu.h"
#include "wlan_ext_emu_test_step_params.h"
#include <cjson/cJSON.h>
#include <spawn.h>
#include <signal.h>
#include <sys/wait.h>

int wlan_ext_test_step_param_eth_lan_client::wlan_ext_step_execute()
{
    wlan_ext_test_step_params_t *step = this;
    cJSON *res_file_json;
    char timestamp[24] = { 0 };
    FILE *result_file;
    char *json_str;
    char temp_file_name[128] = { 0 };
    char mac_str[32] = { 0 };
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step_number : %d\n", __func__, __LINE__,
        step->step_number);

    if (get_current_time_string(timestamp, sizeof(timestamp)) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: get_current_time_string failed\n", __func__,
            __LINE__);
        step->step_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    snprintf(temp_file_name, sizeof(temp_file_name), "/tmp/cci_res/%s_%d_%s_eth_client.txt",
        step->test_case_id, step->step_number, timestamp);

    step->u.eth_lan_client->result_file = std::string(temp_file_name);

    result_file = fopen(step->u.eth_lan_client->result_file.c_str(), "a");
    if (result_file == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: fopen failed for %s\n", __func__, __LINE__,
            step->u.eth_lan_client->result_file.c_str());
        step->step_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    res_file_json = cJSON_CreateObject();
    cJSON_AddNumberToObject(res_file_json, "StepNumber", step->step_number);
    cJSON_AddStringToObject(res_file_json, "EthInterfaceName",
        step->u.eth_lan_client->eth_dev_info.interface_name.c_str());
    cJSON_AddStringToObject(res_file_json, "EthIpAddress",
        step->u.eth_lan_client->eth_dev_info.ip_address.c_str());
    uint8_mac_to_string_mac(step->u.eth_lan_client->eth_dev_info.interface_mac, mac_str);
    cJSON_AddStringToObject(res_file_json, "EthMacAddress", mac_str);

    json_str = cJSON_Print(res_file_json);

    if (fwrite(json_str, strlen(json_str), 1, result_file) != 1) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: fwrite failed\n", __func__, __LINE__);
        fclose(result_file);
        return RETURN_ERR;
    }
    fclose(result_file);

    cJSON_free(json_str);
    cJSON_Delete(res_file_json);

    cJSON_AddItemToArray(step->artifact_json_list, cJSON_CreateString(temp_file_name));

    if (step->fork == false) {
        step->step_state = wlan_emu_tests_state_cmd_continue;
    } else {
        step->step_state = wlan_emu_tests_state_cmd_wait;
    }

    return RETURN_OK;
}

int wlan_ext_test_step_param_eth_lan_client::wlan_ext_step_timeout()
{
    wlan_ext_test_step_params_t *step = this;

    step->timeout_count++;

    if (step->timeout_count >= step->execution_time) {
        step->step_state = wlan_emu_tests_state_cmd_results;
    } else {
        if (step->fork == false) {
            step->step_state = wlan_emu_tests_state_cmd_continue;
        } else {
            step->step_state = wlan_emu_tests_state_cmd_wait;
        }
    }
    wlan_emu_print(wlan_emu_log_level_dbg,
        "%s:%d: Test Step Num : %d timeout_count : %d state : %s\n", __func__, __LINE__,
        step->step_number, step->timeout_count, step_state_as_string(step->step_state).c_str());

    return RETURN_OK;
}

void wlan_ext_test_step_param_eth_lan_client::wlan_ext_step_remove()
{
    wlan_ext_test_step_params_t *step = this;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step_number : %d\n", __func__, __LINE__,
        step->step_number);
    if (step->u.eth_lan_client != nullptr) {
        delete step->u.eth_lan_client;
        step->u.eth_lan_client = nullptr;
    }
    cJSON_free(step->artifact_json_list);
    delete step;
    step = NULL;
}

int wlan_ext_test_step_param_eth_lan_client::wlan_ext_step_frame_filter(wlan_emu_msg_t *msg)
{
    wlan_ext_test_step_params_t *step = this;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: unhandled frame for step number : %d\n",
        __func__, __LINE__, step->step_number);

    return RETURN_UNHANDLED;
}

void wlan_ext_test_step_param_eth_lan_client::wlan_ext_step_status_update(cJSON *agent_status)
{
    char *json_str;
    wlan_ext_test_step_params_t *step = this;
    cJSON *step_private = NULL;
    step_private = cJSON_CreateObject();

    cJSON_AddStringToObject(step_private, "EthInterfaceName",
        step->u.eth_lan_client->eth_dev_info.interface_name.c_str());
    cJSON_AddStringToObject(step_private, "EthIpAddress",
        step->u.eth_lan_client->eth_dev_info.ip_address.c_str());
    step->update_step_status_json(agent_status, step_private);
}

int wlan_ext_test_step_param_eth_lan_client::wlan_ext_step_decode_subdocument(cJSON *root_json,
    wlan_ext_test_step_params_t *step)
{
    cJSON *param = NULL;
    char buf[128] = { 0 };
    std::string temp;
    eth_lan_interface_t *test_params = step->u.eth_lan_client;

    if ((root_json == NULL) || (step->u.eth_lan_client == NULL)) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: root_json : %p eth_lan_client : %p\n",
            __func__, __LINE__, root_json, step->u.eth_lan_client);
        return RETURN_ERR;
    }

    decode_param_string(root_json, "EthInterfaceName", param);
    test_params->eth_dev_info.interface_name.clear();
    test_params->eth_dev_info.interface_name = std::string(param->valuestring);

    decode_param_string(root_json, "EthIpAddress", param);
    snprintf(buf, sizeof(buf), "%s", param->valuestring);
    test_params->eth_dev_info.ip_address.assign(buf);
    test_params->eth_dev_info.ip_address = std::string(param->valuestring);

    decode_param_string(root_json, "EthMacAddress", param);
    string_mac_to_uint8_mac(test_params->eth_dev_info.interface_mac, param->valuestring);

    decode_param_integer(root_json, "Duration", param);
    test_params->duration = param->valuedouble;
    step->execution_time = test_params->duration;

    return RETURN_OK;
}

wlan_ext_test_step_param_eth_lan_client::wlan_ext_test_step_param_eth_lan_client()
{
    wlan_ext_test_step_params_t *step = this;
    step->is_step_initialized = true;

    step->u.eth_lan_client = new (std::nothrow) eth_lan_interface_t;
    if (step->u.eth_lan_client == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: allocation of memory for eth lan client failed for %d\n", __func__, __LINE__,
            step->step_number);
        step->is_step_initialized = false;
        return;
    }
    step->artifact_json_list = cJSON_CreateArray();
    step->execution_time = 5;
    step->timeout_count = 0;
    step->capture_frames = false;
}

wlan_ext_test_step_param_eth_lan_client::~wlan_ext_test_step_param_eth_lan_client()
{
}
