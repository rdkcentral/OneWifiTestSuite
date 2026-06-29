#include "wlan_emu_log.h"
#include "wlan_emu_test_params.h"
#include "wlan_emu_err_code.h"
#include <assert.h>

extern "C" {
webconfig_subdoc_type_t find_subdoc_type(webconfig_t *config, cJSON *json);
int get_list_of_vap_names(wifi_platform_property_t *wifi_prop, wifi_vap_name_t vap_names[],
    int list_size, int num_types, ...);
}

int test_step_param_vap::step_execute()
{
    char file_to_read[128] = { 0 };
    char *json_data;
    int ret = 0;
    webconfig_cci_t *cci_webconfig;
    cJSON *root_json = NULL;

    test_step_params_t *step = this;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Called for Test Step Num : %d\n", __func__,
        __LINE__, step->step_number);

    if ((strncmp(step->u.test_webconfig_json, CURRENT_CONFIGURATION,
            strlen(CURRENT_CONFIGURATION))) != 0) {

        ret = snprintf(file_to_read, sizeof(file_to_read), "%s", step->u.test_webconfig_json);
        if ((ret < 0) || (ret >= sizeof(file_to_read))) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: snprintf failed return : %d input len : %d\n", __func__, __LINE__, ret,
                sizeof(file_to_read));
            step->m_ui_mgr->cci_error_code = ESNPRINTF;
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }

        ret = step->m_ui_mgr->read_config_file(file_to_read, &json_data);
        if (ret != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: test_read_config_file failed\n",
                __func__, __LINE__);
            step->m_ui_mgr->cci_error_code = EFREAD;
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        }

        cci_webconfig = step->m_ui_mgr->get_webconfig_data();

        root_json = cJSON_Parse(json_data);

        step->frame_request.msg_type |= 1 << wlan_emu_msg_type_webconfig;
        step->frame_request.subdoc_type = find_subdoc_type(&cci_webconfig->webconfig,
            root_json);
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: subdoc_type : %d\n", __func__, __LINE__,
            step->frame_request.subdoc_type);

        if (step->frame_request.subdoc_type == webconfig_subdoc_type_xfinity) {
            step->execution_time = 40;
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: new execution time : %d for xfinity\n", __func__, __LINE__,
                    step->execution_time);
        }

        ret = step->m_ui_mgr->bus_send(json_data, step->m_bus_mgr);
        if (ret != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: bus_send failed\n", __func__, __LINE__);
            step->m_ui_mgr->cci_error_code = EBUS;
            step->test_state = wlan_emu_tests_state_cmd_abort;
            free(json_data);
            cJSON_Delete(root_json);
            return RETURN_ERR;
        }
        free(json_data);
        cJSON_Delete(root_json);

        //moved to wlan_emu_tests_state_cmd_results when subdoc received by step_frame_filter
        step->test_state = wlan_emu_tests_state_cmd_continue;

        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step->test_state : %d\n", __func__, __LINE__,
            step->test_state);

    } else {
        step->test_state = wlan_emu_tests_state_cmd_results;
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: No Configuration change\n", __func__,
            __LINE__);
    }

    return RETURN_OK;
}

int test_step_param_vap::step_timeout()
{
    test_step_params_t *step = this;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Test Step Num : %d timeout_count : %d\n",
        __func__, __LINE__, step->step_number, step->timeout_count);

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
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: abort for step : %d execution_time : %d timeout_count : %d\n", __func__,
                __LINE__, step->step_number, step->execution_time, step->timeout_count);
        }
    }
    return RETURN_OK;
}

void test_step_param_vap::step_remove()
{
    test_step_param_vap *step = dynamic_cast<test_step_param_vap *>(this);
    unsigned int results_count = 0;
    wlan_emu_pcap_captures *res_file = NULL;
    if (step == NULL) {
        return;
    }

    delete step;
    step = NULL;

    return;
}

char *get_subdoc_string_from_type(unsigned int type)
{
    switch (type) {
    case webconfig_subdoc_type_private:
        return VAP_PREFIX_PRIVATE;
    case webconfig_subdoc_type_home:
        return VAP_PREFIX_IOT;
    case webconfig_subdoc_type_xfinity:
        return VAP_PREFIX_HOTSPOT;
    case webconfig_subdoc_type_mesh_backhaul:
        return VAP_PREFIX_MESH_BACKHAUL;
    case webconfig_subdoc_type_lnf:
        return VAP_PREFIX_LNF;
    default:
        return NULL;
    }
    return NULL;
}

int test_step_param_vap::step_frame_filter(wlan_emu_msg_t *msg)
{
    test_step_params_t *step = this;
    char *subdoc_str = NULL;
    int num_radios = 0;
    int num_vaps = 0;
    wifi_vap_name_t vap_names[MAX_NUM_RADIOS * MAX_NUM_VAP_PER_RADIO];
    webconfig_cci_t *webconfig_data;
    wifi_vap_info_t *vap_info;
    int i = 0;
    wlan_emu_msg_data_t *f_data = NULL;
    mac_addr_str_t mac_str;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step number : %d\n", __func__, __LINE__,
        step->step_number);

    if (msg == NULL) {
        return RETURN_UNHANDLED;
    }

    // expect only wlan_emu_msg_type_cfg80211 or  wlan_emu_msg_type_webconfig
    switch (msg->get_msg_type()) {
    case wlan_emu_msg_type_cfg80211: // beacon
        if ((step->capture_frames != true) ||
            (!(step->frame_request.msg_type & (1 << msg->get_msg_type())))) {
            return RETURN_UNHANDLED;
        }
        // Check for the respective AP MAC Addresses
        subdoc_str = get_subdoc_string_from_type(step->frame_request.subdoc_type);
        if (subdoc_str == NULL) {
            return RETURN_UNHANDLED;
        }
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step number : %d subdoc_str : %s\n",
            __func__, __LINE__, step->step_number, subdoc_str);
        webconfig_data = step->m_ui_mgr->get_webconfig_data();
        num_radios = webconfig_data->hal_cap.wifi_prop.numRadios;

        num_vaps = get_list_of_vap_names(&webconfig_data->hal_cap.wifi_prop, vap_names,
            (num_radios * MAX_NUM_VAP_PER_RADIO), 1, subdoc_str);
        f_data = msg->get_msg();
        mac_str_without_colon(f_data->u.cfg80211.u.start_ap.macaddr, mac_str);

        for (i = 0; i < num_vaps; i++) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: %d : %s\n", __func__, __LINE__, i,
                vap_names[i]);

            if (!(step->frame_request.cfg80211_ops & (1 << msg->get_cfg80211_ops_type()))) {
                return RETURN_UNHANDLED;
            }

            vap_info = step->m_ui_mgr->get_cci_vap_info(vap_names[i]);
            if (memcmp(vap_info->u.bss_info.bssid, f_data->u.cfg80211.u.start_ap.macaddr,
                    sizeof(mac_addr_t)) == 0) {
                wlan_emu_print(wlan_emu_log_level_dbg,
                    "%s:%d: succesful mac address %s match with %s for step : %d\n", __func__,
                    __LINE__, mac_str, vap_names[i], step->step_number);
                msg->unload_cfg80211_start_ap(step);
                return RETURN_HANDLED;
            } else {
                wlan_emu_print(wlan_emu_log_level_dbg,
                    "%s:%d: Not matching with mac address %s found for %s for step : %d\n",
                    __func__, __LINE__, mac_str, vap_names[i], step->step_number);
            }
        }

        break;
    case wlan_emu_msg_type_webconfig: // onewifi_webconfig
        if (step->frame_request.subdoc_type != webconfig_subdoc_type_unknown) {
            if (step->frame_request.subdoc_type == msg->get_webconfig_subdoc_type()) {
                wlan_emu_print(wlan_emu_log_level_info,
                    "%s:%d: Received the webconfig update of subdoc %d for step : %d\n", __func__,
                    __LINE__, step->frame_request.subdoc_type, step->step_number);
                step->test_state = wlan_emu_tests_state_cmd_results;
                return RETURN_HANDLED;
            }
        }

        break;
    case wlan_emu_msg_type_frm80211: // mgmt
    default:
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Not supported msg_type : %d\n", __func__,
            __LINE__, msg->get_msg_type());
        break;
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: unhandled step number : %d msg_type : %d\n",
        __func__, __LINE__, step->step_number, msg->get_msg_type());
    return RETURN_UNHANDLED;
}

test_step_param_vap::test_step_param_vap()
{
    test_step_params_t *step = this;
    step->is_step_initialized = true;
    step->execution_time = 25;
    step->timeout_count = 0;
    step->capture_frames = false;
}

test_step_param_vap::~test_step_param_vap()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Destructor for vap called\n", __func__,
        __LINE__);
}
