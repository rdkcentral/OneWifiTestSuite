#include "wlan_emu_log.h"
#include "wlan_emu_real_sta_mgr.h"
#include <string>

int wlan_emu_real_sta_mgr_t::add_sta(test_step_params_t *step)
{
    cJSON *json = NULL;
    char value[64] = { 0 };
    long status_code = 0;

    if (step == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d step is NULL\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d json create failed\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    get_cm_mac_address(value);
    value[strcspn(value, "\n")] = '\0';

    cJSON_AddStringToObject(json, "cm_mac", value);
    cJSON_AddStringToObject(json, "device_id", step->u.sta_test->u.sta_management.device_id);
    cJSON_AddStringToObject(json, "platform",
        wlan_common_utils::get_sta_type_string(step->u.sta_test->sta_type));
    cJSON_AddStringToObject(json, "ssid", step->u.sta_test->sta_vap_config->u.sta_info.ssid);
    cJSON_AddStringToObject(json, "security",
        wlan_common_utils::get_secu_mode_string(
            step->u.sta_test->sta_vap_config->u.sta_info.security.mode));
    cJSON_AddStringToObject(json, "password",
        step->u.sta_test->sta_vap_config->u.sta_info.security.u.key.key);

    cJSON *prefer_array = cJSON_AddArrayToObject(json, "prefer");
    cJSON_AddItemToArray(prefer_array,
        cJSON_CreateString(step->u.sta_test->u.sta_management.service_prefer));

    char *json_str = cJSON_Print(json);
    if (json_str == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d cjson print failed\n", __func__, __LINE__);
        cJSON_Delete(json);
        return RETURN_ERR;
    }

    create_key(step->u.sta_test->key, step->u.sta_test->sta_vap_config->u.sta_info.ssid,
        step->u.sta_test->test_id);

    if (step->m_ui_mgr->cci_post_result_to_tda(tc_endpoint_type_conn_request, json_str) !=
        RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: cci_post_result_to_tda failed\n", __func__,
            __LINE__);
        cJSON_free(json_str);
        cJSON_Delete(json);
        return RETURN_ERR;
    }

    cJSON_free(json_str);
    cJSON_Delete(json);

    return RETURN_OK;
}

void wlan_emu_real_sta_mgr_t::remove_sta(test_step_params_t *step)
{
    cJSON *json = NULL;
    char value[64] = { 0 };
    long status_code = 0;

    json = cJSON_CreateObject();
    if (json == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d json create failed\n", __func__, __LINE__);
        return;
    }

    get_cm_mac_address(value);
    value[strcspn(value, "\n")] = '\0';

    cJSON_AddStringToObject(json, "cm_mac", value);
    cJSON_AddStringToObject(json, "device_id", step->u.sta_test->u.sta_management.device_id);
    cJSON_AddStringToObject(json, "platform",
        wlan_common_utils::get_sta_type_string(step->u.sta_test->sta_type));
    cJSON_AddStringToObject(json, "ssid", step->u.sta_test->sta_vap_config->u.sta_info.ssid);

    char *json_str = cJSON_Print(json);
    if (json_str == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d cjson print failed\n", __func__, __LINE__);
        cJSON_Delete(json);
        return;
    }

    wlan_emu_print(wlan_emu_log_level_err, "%s:%d remove_sta blob is %s\n", __func__, __LINE__,
        json_str);

    if (step->m_ui_mgr->cci_post_result_to_tda(tc_endpoint_type_disconn_request, json_str) !=
        RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: cci_post_result_to_tda failed\n", __func__,
            __LINE__);
        cJSON_free(json_str);
        cJSON_Delete(json);
        return;
    }
    cJSON_free(json_str);
    cJSON_Delete(json);
}

wlan_emu_real_sta_mgr_t::wlan_emu_real_sta_mgr_t()
{
}

wlan_emu_real_sta_mgr_t::~wlan_emu_real_sta_mgr_t()
{
}
