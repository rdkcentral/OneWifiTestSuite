#include "cJSON.h"
#include "wlan_emu_common.h"
#include "wlan_emu_log.h"
#include "wlan_emu_test_params.h"
#include "wlan_emu_err_code.h"
#include <assert.h>
#include <iostream>

extern "C" {
webconfig_subdoc_type_t find_subdoc_type(webconfig_t *config, cJSON *json);
webconfig_subdoc_type_t onewifi_subdoc_array[] = { webconfig_subdoc_type_private,
    webconfig_subdoc_type_null, webconfig_subdoc_type_home, webconfig_subdoc_type_xfinity,
    webconfig_subdoc_type_radio, webconfig_subdoc_type_mesh, webconfig_subdoc_type_mesh_backhaul,
    webconfig_subdoc_type_mesh_sta, webconfig_subdoc_type_lnf, webconfig_subdoc_type_dml,
    webconfig_subdoc_type_associated_clients, webconfig_subdoc_type_mac_filter,
    webconfig_subdoc_type_blaster, webconfig_subdoc_type_harvester,
    webconfig_subdoc_type_wifi_config, webconfig_subdoc_type_csi,
    webconfig_subdoc_type_stats_config, webconfig_subdoc_type_steering_config,
    webconfig_subdoc_type_steering_clients, webconfig_subdoc_type_vif_neighbors,
    webconfig_subdoc_type_mesh_backhaul_sta, webconfig_subdoc_type_levl,
    webconfig_subdoc_type_cac };

// subdocs which doesnt has webconfig publish update from onewifi (Temporary)
webconfig_subdoc_type_t no_publish_subdoc_array[] = { webconfig_subdoc_type_radio_stats,
    webconfig_subdoc_type_neighbor_stats, webconfig_subdoc_type_assocdev_stats,
    webconfig_subdoc_type_radiodiag_stats, webconfig_subdoc_type_radio_temperature,
    webconfig_subdoc_type_wifiapiradio, webconfig_subdoc_type_wifiapivap };
}

int test_step_param_config_onewifi::step_execute()
{
    char file_to_read[128] = { 0 };
    char *json_data;
    int ret = 0;
    webconfig_cci_t *cci_webconfig;
    bool no_publish_subdocs = false;
    test_step_params_t *step = this;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Called for Test Step Num : %d\n", __func__,
        __LINE__, step->step_number);

    ret = snprintf(file_to_read, sizeof(file_to_read), "%s", step->u.test_onewifi_subdoc);
    if ((ret < 0) || (ret >= sizeof(file_to_read))) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: STEP : %d - snprintf failed return : %d input len : %d\n", __func__, __LINE__,
            step->step_number, ret, strlen(step->u.test_onewifi_subdoc));
        step->m_ui_mgr->cci_error_code = ESNPRINTF;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    ret = step->m_ui_mgr->read_config_file(file_to_read, &json_data);
    if (ret != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: STEP : %d - read_config_file failed for file : %s\n", __func__, __LINE__,
            step->step_number, file_to_read);
        step->m_ui_mgr->cci_error_code = EFREAD;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    cci_webconfig = step->m_ui_mgr->get_webconfig_data();

    cJSON *json_str = cJSON_Parse(json_data);
    if (json_str == NULL) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: STEP : %d - cJSON_Parse failed for file : %s\n", __func__, __LINE__,
            step->step_number, file_to_read);
        step->m_ui_mgr->cci_error_code = EJSONPARSE;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        free(json_data);
        return RETURN_ERR;
    }

    step->frame_request.msg_type |= (1 << wlan_emu_msg_type_webconfig);
    step->frame_request.subdoc_type = find_subdoc_type(&cci_webconfig->webconfig, json_str);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: STEP : %d - subdoc_type : %d\n", __func__,
        __LINE__, step->step_number, step->frame_request.subdoc_type);

    for (int i = 0; i < sizeof(no_publish_subdoc_array) / sizeof(no_publish_subdoc_array[0]); ++i) {
        if (no_publish_subdoc_array[i] == step->frame_request.subdoc_type) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: STEP : %d - Onewifi publish update is not supported for subdoc_type : %d\n",
                __func__, __LINE__, step->step_number, step->frame_request.subdoc_type);
            step->m_ui_mgr->cci_error_code = EUNSUPPORTEDSUBDOC;
            step->test_state = wlan_emu_tests_state_cmd_abort;
            cJSON_Delete(json_str);
            free(json_data);
            return RETURN_ERR;
        }
    }

    ret = step->m_ui_mgr->bus_send(json_data, step->m_bus_mgr);
    if (ret != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: STEP : %d - bus_send failed for subdoc_type : %d file : %s\n", __func__,
            __LINE__, step->step_number, step->frame_request.subdoc_type, file_to_read);
        step->m_ui_mgr->cci_error_code = EBUS;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        cJSON_Delete(json_str);
        free(json_data);
        return RETURN_ERR;
    }
    free(json_data);
    cJSON_Delete(json_str);

    step->test_state = wlan_emu_tests_state_cmd_continue;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: STEP : %d - Test State : %d\n", __func__,
        __LINE__, step->step_number, step->test_state);

    return RETURN_OK;
}

int test_step_param_config_onewifi::step_timeout()
{
    test_step_params_t *step = this;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Test Step Num : %d timeout_count : %d\n",
        __func__, __LINE__, this->step_number, this->timeout_count);

    if (step->test_state != wlan_emu_tests_state_cmd_results) {
        step->timeout_count++;
        if (step->execution_time > step->timeout_count) {
            step->test_state = wlan_emu_tests_state_cmd_continue;
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: continue for step : %d execution_time : %d timeout_count : %d\n", __func__,
                __LINE__, step->step_number, step->execution_time, step->timeout_count);
        } else {
            step->m_ui_mgr->cci_error_code = ESTEPTIMEOUT;
            step->test_state = wlan_emu_tests_state_cmd_abort;
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: abort for step : %d execution_time : %d timeout_count : %d\n", __func__,
                __LINE__, step->step_number, step->execution_time, step->timeout_count);
        }
    }

    return RETURN_OK;
}

int test_step_param_config_onewifi::step_frame_filter(wlan_emu_msg_t *msg)
{
    test_step_params_t *step = this;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Called for Test Step Num : %d\n", __func__,
        __LINE__, this->step_number);

    if (msg == NULL) {
        return RETURN_UNHANDLED;
    }

    switch (msg->get_msg_type()) {
    case wlan_emu_msg_type_webconfig:
        if (step->frame_request.subdoc_type != webconfig_subdoc_type_unknown) {
            bool webcfg_publish_update = false;
            for (int i = 0; i < sizeof(onewifi_subdoc_array) / sizeof(onewifi_subdoc_array[0]);
                 ++i) {
                if (onewifi_subdoc_array[i] == step->frame_request.subdoc_type) {
                    webcfg_publish_update = true;
                    break;
                }
            }

            if (webcfg_publish_update) {
                if (step->frame_request.subdoc_type == msg->get_webconfig_subdoc_type()) {
                    wlan_emu_print(wlan_emu_log_level_info,
                        "%s:%d: STEP : %d - Received Webconfig Publish Update for subdoc_type : %d "
                        "from OneWiFi\n",
                        __func__, __LINE__, step->step_number, step->frame_request.subdoc_type);
                    step->test_state = wlan_emu_tests_state_cmd_results;
                    return RETURN_HANDLED;
                }
            } else {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: STEP : %d - No Webconfig Publish Update for subdoc_type : %d from "
                    "OneWiFi\n",
                    __func__, __LINE__, step->step_number, step->frame_request.subdoc_type);
                return RETURN_UNHANDLED;
            }
        } else {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: STEP : %d - Invalid subdoc_type : %d\n",
                __func__, __LINE__, step->step_number, step->frame_request.subdoc_type);
            return RETURN_UNHANDLED;
        }
        break;
    case wlan_emu_msg_type_cfg80211:
    case wlan_emu_msg_type_frm80211:
    default:
        wlan_emu_print(wlan_emu_log_level_dbg,
            "%s:%d: STEP : %d - Received Unknown Message Type : %d\n", __func__, __LINE__,
            step->step_number, msg->get_msg_type());
        break;
    }

    return RETURN_UNHANDLED;
}

void test_step_param_config_onewifi::step_remove()
{
    test_step_param_vap *step = dynamic_cast<test_step_param_vap *>(this);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: STEP : %d - Called for Step Remove\n", __func__,
        __LINE__, this->step_number);

    if (step == NULL) {
        return;
    }

    delete step;
    step = NULL;

    return;
}

test_step_param_config_onewifi::test_step_param_config_onewifi()
{
    test_step_params_t *step = this;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: STEP : %d - Constructor for Config Set called\n",
        __func__, __LINE__);
    memset(&step->frame_request, 0, sizeof(step->frame_request));
    step->is_step_initialized = true;
    step->execution_time = 15;
    step->timeout_count = 0;
    step->capture_frames = false;
}

test_step_param_config_onewifi::~test_step_param_config_onewifi()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Destructor for Config Set called\n", __func__,
        __LINE__);
}
