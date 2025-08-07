#include "wifi_base.h"
#include "wlan_emu_common.h"
#include "wlan_emu_log.h"
#include "wlan_emu_test_params.h"
#include "wlan_emu_bus.h"
#include "wlan_emu_err_code.h"
#include <assert.h>
#include <cjson/cJSON.h>
#include <errno.h>
#include <fcntl.h>
#include <sstream>
#include <string>

#if !defined(CONFIG_EXT_AGENT_CCI)
extern "C" {
int wifi_hal_emu_set_radio_channel_stats(unsigned int radio_index, bool emu_state,
    wifi_channelStats_t *chan_stat, unsigned int count, unsigned int phy_index,
    unsigned int interface_index);
int wifi_hal_emu_set_neighbor_stats(unsigned int radio_index, bool emu_state,
    wifi_neighbor_ap2_t *neighbor_stats, unsigned int count);
int wifi_hal_emu_set_assoc_clients_stats(unsigned int vap_index, bool emu_state,
    wifi_associated_dev3_t *assoc_cli_stat, unsigned int count, unsigned int phy_index,
    unsigned int interface_index);
int wifi_hal_emu_set_radio_diag_stats(unsigned int radio_index, bool emu_state,
    wifi_radioTrafficStats2_t *radio_diag_stat, unsigned int count, unsigned int phy_index,
    unsigned int interface_index);
int wifi_hal_emu_set_radio_temp(unsigned int radio_index, bool emu_state, int temperature,
    unsigned int phy_index, unsigned int interface_index);
void copy_chanstats_to_chandata(radio_chan_data_t *chan_data, wifi_channelStats_t *chan_stats);
}
int test_step_param_set_stats_t::step_frame_filter(wlan_emu_msg_t *msg)
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

int test_step_param_set_radio_channel_stats::webconfig_stats_set_execute_stop()
{
    test_step_params_t *step = this;
    bool emu_state;
    webconfig_cci_t *webconfig_data = NULL;
    webconfig_data = step->m_ui_mgr->get_webconfig_data();

    emu_state = false;
    int radio_index = step->u.wifi_stats_set->radio_index;
    int phy_index = webconfig_data->hal_cap.wifi_prop.radio_interface_map[radio_index].phy_index;
    int interface_index = if_nametoindex(
        webconfig_data->hal_cap.wifi_prop.radio_interface_map[radio_index].interface_name);

    if (wifi_hal_emu_set_radio_channel_stats(radio_index, emu_state, NULL, 0, phy_index,
            interface_index) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: Stop for Radio_Channel_Stats failed. Radio index : %d, Step number : %d\n",
            __func__, __LINE__, radio_index, step->step_number);
        step->m_ui_mgr->cci_error_code = ERADIOSTATS;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    } else {
        wlan_emu_print(wlan_emu_log_level_dbg,
            "%s:%d: Stop for Radio_Channel_Stats success. Radio index : %d, Step number : %d\n",
            __func__, __LINE__, radio_index, step->step_number);
    }

    return RETURN_OK;
}

int test_step_param_set_neighbor_stats::webconfig_stats_set_execute_stop()
{
    test_step_params_t *step = this;
    bool emu_state;
    webconfig_cci_t *webconfig_data = NULL;
    webconfig_data = step->m_ui_mgr->get_webconfig_data();

    emu_state = false;
    int radio_index = step->u.wifi_stats_set->radio_index;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Stop hal api for Neighbor_Stats\n", __func__,
        __LINE__);

    if (wifi_hal_emu_set_neighbor_stats(radio_index, emu_state, NULL, 0) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: Stop for Neighbor_Stats failed. Radio index : %d, Step number : %d\n", __func__,
            __LINE__, radio_index, step->step_number);
        step->m_ui_mgr->cci_error_code = ENEIGHSTATS;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    } else {
        wlan_emu_print(wlan_emu_log_level_dbg,
            "%s:%d: Stop for Neighbor_Stats success. Radio index : %d, Step number : %d\n",
            __func__, __LINE__, radio_index, step->step_number);
    }

    return RETURN_OK;
}

int test_step_param_set_assoc_clients_stats::webconfig_stats_set_execute_stop()
{
    test_step_params_t *step = this;
    bool emu_state;
    webconfig_cci_t *webconfig_data = NULL;
    webconfig_data = step->m_ui_mgr->get_webconfig_data();

    emu_state = false;
    int vap_index = step->u.wifi_stats_set->vap_index;
    int phy_index = webconfig_data->hal_cap.wifi_prop.interface_map[vap_index].phy_index;
    int interface_index = if_nametoindex(
        webconfig_data->hal_cap.wifi_prop.interface_map[vap_index].interface_name);

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Stop hal api for Assoc_Clients_Stats\n",
        __func__, __LINE__);

    if (wifi_hal_emu_set_assoc_clients_stats(vap_index, emu_state, NULL, 0, phy_index,
            interface_index) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: Stop for Assoc_Clients_Stats failed. VAP index : %d, Step number : %d\n",
            __func__, __LINE__, vap_index, step->step_number);
        step->m_ui_mgr->cci_error_code = ECLIENTSTATS;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    } else {
        wlan_emu_print(wlan_emu_log_level_dbg,
            "%s:%d: Stop for Assoc_Clients_Stats success. VAP index : %d, Step number : %d\n",
            __func__, __LINE__, vap_index, step->step_number);
    }

    return RETURN_OK;
}

int test_step_param_set_radio_diag_stats::webconfig_stats_set_execute_stop()
{
    test_step_params_t *step = this;
    bool emu_state;
    webconfig_cci_t *webconfig_data = NULL;
    webconfig_data = step->m_ui_mgr->get_webconfig_data();

    emu_state = false;
    int radio_index = step->u.wifi_stats_set->radio_index;
    int phy_index = webconfig_data->hal_cap.wifi_prop.radio_interface_map[radio_index].phy_index;
    int interface_index = if_nametoindex(
        webconfig_data->hal_cap.wifi_prop.radio_interface_map[radio_index].interface_name);

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Stop hal api for Radio_Diag_Stats\n", __func__,
        __LINE__);

    if (wifi_hal_emu_set_radio_diag_stats(radio_index, emu_state, NULL, 0, phy_index,
            interface_index) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: Stop for Radio_Diag_Stats failed. Radio index : %d, Step number : %d\n",
            __func__, __LINE__, radio_index, step->step_number);
        step->m_ui_mgr->cci_error_code = ERADIOSTATS;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    } else {
        wlan_emu_print(wlan_emu_log_level_dbg,
            "%s:%d: Stop for Radio_Diag_Stats success. Radio index : %d, Step number : %d\n",
            __func__, __LINE__, radio_index, step->step_number);
    }

    return RETURN_OK;
}

int test_step_param_set_radio_temperature_stats::webconfig_stats_set_execute_stop()
{
    test_step_params_t *step = this;
    bool emu_state;
    webconfig_cci_t *webconfig_data = NULL;
    webconfig_data = step->m_ui_mgr->get_webconfig_data();

    emu_state = false;
    int radio_index = step->u.wifi_stats_set->radio_index;
    int phy_index = webconfig_data->hal_cap.wifi_prop.radio_interface_map[radio_index].phy_index;
    int interface_index = if_nametoindex(
        webconfig_data->hal_cap.wifi_prop.radio_interface_map[radio_index].interface_name);
    int temperature = 0;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Stop hal api for Radio_Temperature_Stats\n",
        __func__, __LINE__);

    if (wifi_hal_emu_set_radio_temp(radio_index, emu_state, temperature, phy_index,
            interface_index) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: Stop for Radio_Temperature_Stats failed. Radio index : %d, Step number : %d\n",
            __func__, __LINE__, radio_index, step->step_number);
        step->m_ui_mgr->cci_error_code = ERADIOTEMP;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    } else {
        wlan_emu_print(wlan_emu_log_level_dbg,
            "%s:%d: Stop for Radio_Temperature_Stats success. Radio index : %d, Step number : %d\n",
            __func__, __LINE__, radio_index, step->step_number);
    }

    return RETURN_OK;
}

int test_step_param_set_radio_channel_stats::webconfig_stats_set_execute_start()
{
    test_step_params_t *step = this;
    char *json_data;
    int ret = 0;
    char file_to_read[128] = { 0 };
    webconfig_cci_t *webconfig_data = NULL;
    int radio_index;
    bool emu_state;
    int phy_index;
    int interface_index;
    wifi_provider_response_t *response;
    radio_chan_data_t *chan_data;
    wifi_channelStats_t *chan_stat;
    unsigned int chan_count = 0;
    wifi_radio_operationParam_t *radio_op_params;
    unsigned int on_channel_number;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d - test_step_param_set_radio_channel_stats\n",
        __func__, __LINE__);

    stat_set_config_t *reference = (stat_set_config_t *)queue_peek(
        step->u.wifi_stats_set->stats_set_q, step->u.wifi_stats_set->current_stats_set_count);
    if (reference == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: queue_peek failed\n", __func__, __LINE__);
        step->m_ui_mgr->cci_error_code = EQPEEK;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }
    ret = snprintf(file_to_read, sizeof(file_to_read), "%s", reference->input_file_json);
    if ((ret < 0) || (ret >= sizeof(file_to_read))) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: snprintf failed returned : %d, file_length : %d for input_file_json : %s\n",
            __func__, __LINE__, ret, sizeof(file_to_read), reference->input_file_json);
        step->m_ui_mgr->cci_error_code = ESNPRINTF;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    ret = step->m_ui_mgr->read_config_file(file_to_read, &json_data);
    if (ret != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: read_config_file failed for file : %s\n",
            __func__, __LINE__, file_to_read);
        step->m_ui_mgr->cci_error_code = EFREAD;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    webconfig_data = step->m_ui_mgr->get_webconfig_data();
    webconfig_subdoc_data_t subdoc_data;

    if (webconfig_decode(&webconfig_data->webconfig, &subdoc_data, json_data) ==
        webconfig_error_none) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: webconfig_decode success\n", __func__,
            __LINE__);
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: webconfig_decode failed\n", __func__,
            __LINE__);
        step->m_ui_mgr->cci_error_code = EWEBDECODE;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        free(json_data);
        return RETURN_ERR;
    }

    radio_index = step->u.wifi_stats_set->radio_index;
    emu_state = true;
    phy_index = webconfig_data->hal_cap.wifi_prop.radio_interface_map[radio_index].phy_index;
    interface_index = if_nametoindex(
        webconfig_data->hal_cap.wifi_prop.radio_interface_map[radio_index].interface_name);
    response = static_cast<wifi_provider_response_t *>((subdoc_data.u.decoded.collect_stats.stats));
    chan_data = static_cast<radio_chan_data_t *>(response->stat_pointer);

    chan_stat = new (std::nothrow) wifi_channelStats_t[response->stat_array_size];
    if (chan_stat == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: allocation of memory failed for %d\n",
            __func__, __LINE__, step->step_number);
        step->m_ui_mgr->cci_error_code = EMALLOC;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        free(json_data);
        return RETURN_ERR;
    }

    if (response->args.scan_mode == WIFI_RADIO_SCAN_MODE_ONCHAN) {
        radio_op_params = step->m_ui_mgr->cci_get_radio_operation_param(radio_index);
        on_channel_number = radio_op_params->channel;
    }

    for (chan_count = 0; chan_count < (unsigned int)response->stat_array_size; chan_count++) {
        //        copy_chanstats_to_chandata(&chan_data[chan_count], &chan_stat[chan_count]);
        chan_stat[chan_count].ch_in_pool = chan_data[chan_count].ch_in_pool;
        chan_stat[chan_count].ch_radar_noise = chan_data[chan_count].ch_radar_noise;
        if (response->args.scan_mode == WIFI_RADIO_SCAN_MODE_ONCHAN) {
            chan_stat[chan_count].ch_number = on_channel_number;
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: updating onchan channel number to %d for step : %d\n", __func__, __LINE__,
                on_channel_number, step->step_number);
        } else {
            chan_stat[chan_count].ch_number = chan_data[chan_count].ch_number;
        }

        chan_stat[chan_count].ch_number = chan_data[chan_count].ch_number;
        chan_stat[chan_count].ch_noise = chan_data[chan_count].ch_noise;
        chan_stat[chan_count].ch_max_80211_rssi = chan_data[chan_count].ch_max_80211_rssi;
        chan_stat[chan_count].ch_non_80211_noise = chan_data[chan_count].ch_non_80211_noise;
        chan_stat[chan_count].ch_utilization = chan_data[chan_count].ch_utilization;
        chan_stat[chan_count].ch_utilization_busy_tx = chan_data[chan_count].ch_utilization_busy_tx;
        chan_stat[chan_count].ch_utilization_busy_self =
            chan_data[chan_count].ch_utilization_busy_self;
        chan_stat[chan_count].ch_utilization_total = chan_data[chan_count].ch_utilization_total;
        chan_stat[chan_count].ch_utilization_busy = chan_data[chan_count].ch_utilization_busy;
        chan_stat[chan_count].ch_utilization_busy_rx = chan_data[chan_count].ch_utilization_busy_rx;
        chan_stat[chan_count].ch_utilization_busy_ext =
            chan_data[chan_count].ch_utilization_busy_ext;
    }

    for (unsigned int i = 0; i < response->stat_array_size; i++) {
        wlan_emu_print(wlan_emu_log_level_dbg,
            "%s:%d: Channel %u: Noise %d, Radar Noise %s, Max RSSI %d, Non-80211 Noise %d, "
            "Utilization %u, Utilization Total %llu, Utilization Busy %llu, Utilization Busy TX "
            "%llu, Utilization Busy RX %llu, Utilization Busy Self %llu, Utilization Busy Ext "
            "%llu\n",
            __func__, __LINE__, chan_stat[i].ch_number, chan_stat[i].ch_noise,
            chan_stat[i].ch_radar_noise ? "true" : "false", chan_stat[i].ch_max_80211_rssi,
            chan_stat[i].ch_non_80211_noise, chan_stat[i].ch_utilization,
            chan_stat[i].ch_utilization_total, chan_stat[i].ch_utilization_busy,
            chan_stat[i].ch_utilization_busy_tx, chan_stat[i].ch_utilization_busy_rx,
            chan_stat[i].ch_utilization_busy_self, chan_stat[i].ch_utilization_busy_ext);
    }

    if (wifi_hal_emu_set_radio_channel_stats(radio_index, emu_state, chan_stat,
            response->stat_array_size, phy_index, interface_index) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: wifi_hal_emu_set_radio_channel_stats failed\n", __func__, __LINE__);
        step->m_ui_mgr->cci_error_code = ERADIOSTATS;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        delete[] chan_stat;
        free(response->stat_pointer);
        free(response);
        free(json_data);
        return RETURN_ERR;
    }

    delete[] chan_stat;
    free(response->stat_pointer);
    free(response);
    free(json_data);

    return RETURN_OK;
}

int test_step_param_set_neighbor_stats::webconfig_stats_set_execute_start()
{
    test_step_params_t *step = this;
    char *json_data;
    int ret = 0;
    char file_to_read[128] = { 0 };
    webconfig_cci_t *webconfig_data = NULL;
    int radio_index;
    bool emu_state;
    int phy_index;
    int interface_index;
    wifi_provider_response_t *response;
    wifi_neighbor_ap2_t *neighbor_data;
    unsigned int neighbor_count = 0;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d - test_step_param_set_neighbor_stats\n", __func__,
        __LINE__);

    stat_set_config_t *reference = (stat_set_config_t *)queue_peek(
        step->u.wifi_stats_set->stats_set_q, step->u.wifi_stats_set->current_stats_set_count);
    if (reference == NULL) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: queue_peek failed for current_stats_set_count : %d\n", __func__, __LINE__,
            step->u.wifi_stats_set->current_stats_set_count);
        step->m_ui_mgr->cci_error_code = EQPEEK;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }
    ret = snprintf(file_to_read, sizeof(file_to_read), "%s", reference->input_file_json);
    if ((ret < 0) || (ret >= sizeof(file_to_read))) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: snprintf failed returned : %d, file_length : %d for input_file_json : %s\n",
            __func__, __LINE__, ret, sizeof(file_to_read), reference->input_file_json);
        step->m_ui_mgr->cci_error_code = ESNPRINTF;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    ret = step->m_ui_mgr->read_config_file(file_to_read, &json_data);
    if (ret != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: read_config_file failed for file : %s\n",
            __func__, __LINE__, file_to_read);
        step->m_ui_mgr->cci_error_code = EFREAD;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    webconfig_data = step->m_ui_mgr->get_webconfig_data();
    webconfig_subdoc_data_t subdoc_data;

    if (webconfig_decode(&webconfig_data->webconfig, &subdoc_data, json_data) ==
        webconfig_error_none) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: webconfig_decode success\n", __func__,
            __LINE__);
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: webconfig_decode failed for file : %s\n",
            __func__, __LINE__, file_to_read);
        step->m_ui_mgr->cci_error_code = EWEBDECODE;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        free(json_data);
        return RETURN_ERR;
    }

    radio_index = step->u.wifi_stats_set->radio_index;
    emu_state = true;
    response = static_cast<wifi_provider_response_t *>((subdoc_data.u.decoded.collect_stats.stats));

    if ((response->stat_array_size == 0) || (response->stat_pointer == NULL)) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Neighbor stats count : %d\n", __func__,
            __LINE__, response->stat_array_size);
        if (wifi_hal_emu_set_neighbor_stats(radio_index, emu_state, neighbor_data,
                response->stat_array_size) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: wifi_hal_emu_set_neighbor_stats failed\n", __func__, __LINE__);
            step->m_ui_mgr->cci_error_code = ENEIGHSTATS;
            step->test_state = wlan_emu_tests_state_cmd_abort;
            free(response->stat_pointer);
            free(response);
            free(json_data);
            return RETURN_ERR;
        }
        free(response->stat_pointer);
        free(response);
        return RETURN_OK;
    }

    neighbor_data = static_cast<wifi_neighbor_ap2_t *>(response->stat_pointer);

    wifi_neighbor_ap2_t *neighbor_stats = new (std::nothrow)
        wifi_neighbor_ap2_t[response->stat_array_size];
    if (neighbor_stats == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: allocation of memory failed for %d\n",
            __func__, __LINE__, step->step_number);
        free(response->stat_pointer);
        free(response);
        free(json_data);
        step->m_ui_mgr->cci_error_code = EMALLOC;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    for (neighbor_count = 0; neighbor_count < (unsigned int)response->stat_array_size;
         neighbor_count++) {
        strncpy(neighbor_stats[neighbor_count].ap_SSID, neighbor_data[neighbor_count].ap_SSID,
            sizeof(neighbor_stats[neighbor_count].ap_SSID) - 1);
        neighbor_stats[neighbor_count].ap_SSID[sizeof(neighbor_stats[neighbor_count].ap_SSID) - 1] =
            '\0';

        strncpy(neighbor_stats[neighbor_count].ap_BSSID, neighbor_data[neighbor_count].ap_BSSID,
            sizeof(neighbor_stats[neighbor_count].ap_BSSID) - 1);
        neighbor_stats[neighbor_count]
            .ap_BSSID[sizeof(neighbor_stats[neighbor_count].ap_BSSID) - 1] = '\0';

        strncpy(neighbor_stats[neighbor_count].ap_Mode, neighbor_data[neighbor_count].ap_Mode,
            sizeof(neighbor_stats[neighbor_count].ap_Mode) - 1);
        neighbor_stats[neighbor_count].ap_Mode[sizeof(neighbor_stats[neighbor_count].ap_Mode) - 1] =
            '\0';

        neighbor_stats[neighbor_count].ap_Channel = neighbor_data[neighbor_count].ap_Channel;

        neighbor_stats[neighbor_count].ap_SignalStrength =
            neighbor_data[neighbor_count].ap_SignalStrength;

        strncpy(neighbor_stats[neighbor_count].ap_SecurityModeEnabled,
            neighbor_data[neighbor_count].ap_SecurityModeEnabled,
            sizeof(neighbor_stats[neighbor_count].ap_SecurityModeEnabled) - 1);
        neighbor_stats[neighbor_count]
            .ap_SecurityModeEnabled[sizeof(neighbor_stats[neighbor_count].ap_SecurityModeEnabled) -
                1] = '\0';

        strncpy(neighbor_stats[neighbor_count].ap_EncryptionMode,
            neighbor_data[neighbor_count].ap_EncryptionMode,
            sizeof(neighbor_stats[neighbor_count].ap_EncryptionMode) - 1);
        neighbor_stats[neighbor_count]
            .ap_EncryptionMode[sizeof(neighbor_stats[neighbor_count].ap_EncryptionMode) - 1] = '\0';

        strncpy(neighbor_stats[neighbor_count].ap_OperatingFrequencyBand,
            neighbor_data[neighbor_count].ap_OperatingFrequencyBand,
            sizeof(neighbor_stats[neighbor_count].ap_OperatingFrequencyBand) - 1);
        neighbor_stats[neighbor_count].ap_OperatingFrequencyBand
            [sizeof(neighbor_stats[neighbor_count].ap_OperatingFrequencyBand) - 1] = '\0';

        strncpy(neighbor_stats[neighbor_count].ap_SupportedStandards,
            neighbor_data[neighbor_count].ap_SupportedStandards,
            sizeof(neighbor_stats[neighbor_count].ap_SupportedStandards) - 1);
        neighbor_stats[neighbor_count]
            .ap_SupportedStandards[sizeof(neighbor_stats[neighbor_count].ap_SupportedStandards) -
                1] = '\0';

        strncpy(neighbor_stats[neighbor_count].ap_OperatingStandards,
            neighbor_data[neighbor_count].ap_OperatingStandards,
            sizeof(neighbor_stats[neighbor_count].ap_OperatingStandards) - 1);
        neighbor_stats[neighbor_count]
            .ap_OperatingStandards[sizeof(neighbor_stats[neighbor_count].ap_OperatingStandards) -
                1] = '\0';

        strncpy(neighbor_stats[neighbor_count].ap_OperatingChannelBandwidth,
            neighbor_data[neighbor_count].ap_OperatingChannelBandwidth,
            sizeof(neighbor_stats[neighbor_count].ap_OperatingChannelBandwidth) - 1);
        neighbor_stats[neighbor_count].ap_OperatingChannelBandwidth
            [sizeof(neighbor_stats[neighbor_count].ap_OperatingChannelBandwidth) - 1] = '\0';

        neighbor_stats[neighbor_count].ap_BeaconPeriod =
            neighbor_data[neighbor_count].ap_BeaconPeriod;

        neighbor_stats[neighbor_count].ap_Noise = neighbor_data[neighbor_count].ap_Noise;

        strncpy(neighbor_stats[neighbor_count].ap_BasicDataTransferRates,
            neighbor_data[neighbor_count].ap_BasicDataTransferRates,
            sizeof(neighbor_stats[neighbor_count].ap_BasicDataTransferRates) - 1);
        neighbor_stats[neighbor_count].ap_BasicDataTransferRates
            [sizeof(neighbor_stats[neighbor_count].ap_BasicDataTransferRates) - 1] = '\0';

        strncpy(neighbor_stats[neighbor_count].ap_SupportedDataTransferRates,
            neighbor_data[neighbor_count].ap_SupportedDataTransferRates,
            sizeof(neighbor_stats[neighbor_count].ap_SupportedDataTransferRates) - 1);
        neighbor_stats[neighbor_count].ap_SupportedDataTransferRates
            [sizeof(neighbor_stats[neighbor_count].ap_SupportedDataTransferRates) - 1] = '\0';

        neighbor_stats[neighbor_count].ap_DTIMPeriod = neighbor_data[neighbor_count].ap_DTIMPeriod;

        neighbor_stats[neighbor_count].ap_ChannelUtilization =
            neighbor_data[neighbor_count].ap_ChannelUtilization;
    }

    for (unsigned int i = 0; i < response->stat_array_size; i++) {
        wlan_emu_print(wlan_emu_log_level_dbg,
            "%s:%d: Neighbor Channel %u: SSID %s, BSSID %s, Mode %s, Channel %u, Signal Strength "
            "%d, Security Mode %s, Encryption Mode %s, Operating Frequency Band %s, Supported "
            "Standards %s,"
            "Operating Standards %s, Operating Channel Bandwidth %s, Beacon Period %u, Noise %d, "
            "Basic Data Transfer Rates %s, Supported Data Transfer Rates %s, DTIM Period %u, "
            "Channel Utilization %u\n",
            __func__, __LINE__, neighbor_stats[i].ap_Channel, neighbor_stats[i].ap_SSID,
            neighbor_stats[i].ap_BSSID, neighbor_stats[i].ap_Mode, neighbor_stats[i].ap_Channel,
            neighbor_stats[i].ap_SignalStrength, neighbor_stats[i].ap_SecurityModeEnabled,
            neighbor_stats[i].ap_EncryptionMode, neighbor_stats[i].ap_OperatingFrequencyBand,
            neighbor_stats[i].ap_SupportedStandards, neighbor_stats[i].ap_OperatingStandards,
            neighbor_stats[i].ap_OperatingChannelBandwidth, neighbor_stats[i].ap_BeaconPeriod,
            neighbor_stats[i].ap_Noise, neighbor_stats[i].ap_BasicDataTransferRates,
            neighbor_stats[i].ap_SupportedDataTransferRates, neighbor_stats[i].ap_DTIMPeriod,
            neighbor_stats[i].ap_ChannelUtilization);
    }

    if (wifi_hal_emu_set_neighbor_stats(radio_index, emu_state, neighbor_stats,
            response->stat_array_size) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: wifi_hal_emu_set_neighbor_stats failed\n",
            __func__, __LINE__);
        step->m_ui_mgr->cci_error_code = ENEIGHSTATS;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        delete[] neighbor_stats;
        free(response->stat_pointer);
        free(response);
        free(json_data);
        return RETURN_ERR;
    }

    delete[] neighbor_stats;
    free(response->stat_pointer);
    free(response);
    free(json_data);

    return RETURN_OK;
}

int test_step_param_set_assoc_clients_stats::webconfig_stats_set_execute_start()
{
    test_step_params_t *step = this;
    char *json_data;
    int ret = 0;
    char file_to_read[128] = { 0 };
    webconfig_cci_t *webconfig_data = NULL;
    int vap_index;
    bool emu_state;
    int phy_index;
    int interface_index;
    wifi_provider_response_t *response;
    wifi_associated_dev3_t *assoc_data;
    unsigned int assoc_count = 0;
    char str[32] = { 0 };

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d - test_step_param_set_assoc_clients_stats\n",
        __func__, __LINE__);

    stat_set_config_t *reference = (stat_set_config_t *)queue_peek(
        step->u.wifi_stats_set->stats_set_q, step->u.wifi_stats_set->current_stats_set_count);
    if (reference == NULL) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: queue_peek failed for current_stats_set_count : %d\n", __func__, __LINE__,
            step->u.wifi_stats_set->current_stats_set_count);
        step->m_ui_mgr->cci_error_code = EQPEEK;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }
    ret = snprintf(file_to_read, sizeof(file_to_read), "%s", reference->input_file_json);
    if ((ret < 0) || (ret >= sizeof(file_to_read))) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: snprintf failed returned : %d, file_length : %d for input_file_json : %s\n",
            __func__, __LINE__, ret, sizeof(file_to_read), reference->input_file_json);
        step->m_ui_mgr->cci_error_code = ESNPRINTF;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    ret = step->m_ui_mgr->read_config_file(file_to_read, &json_data);
    if (ret != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: read_config_file failed for file : %s\n",
            __func__, __LINE__, file_to_read);
        step->m_ui_mgr->cci_error_code = EFREAD;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    webconfig_data = step->m_ui_mgr->get_webconfig_data();
    webconfig_subdoc_data_t subdoc_data;

    if (webconfig_decode(&webconfig_data->webconfig, &subdoc_data, json_data) ==
        webconfig_error_none) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: webconfig_decode success\n", __func__,
            __LINE__);
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: webconfig_decode failed for file : %s\n",
            __func__, __LINE__, file_to_read);
        step->m_ui_mgr->cci_error_code = EWEBDECODE;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        free(json_data);
        return RETURN_ERR;
    }

    vap_index = step->u.wifi_stats_set->vap_index;
    emu_state = true;
    phy_index = webconfig_data->hal_cap.wifi_prop.interface_map[vap_index].phy_index;
    interface_index = if_nametoindex(
        webconfig_data->hal_cap.wifi_prop.interface_map[vap_index].interface_name);
    response = static_cast<wifi_provider_response_t *>((subdoc_data.u.decoded.collect_stats.stats));

    if ((response->stat_array_size == 0) || (response->stat_pointer == NULL)) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Assoc clients stats count : %d\n", __func__,
            __LINE__, response->stat_array_size);
        if (wifi_hal_emu_set_assoc_clients_stats(vap_index, emu_state, assoc_data,
                response->stat_array_size, phy_index, interface_index) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: wifi_hal_emu_set_assoc_clients_stats failed\n", __func__, __LINE__);
            step->m_ui_mgr->cci_error_code = ECLIENTSTATS;
            step->test_state = wlan_emu_tests_state_cmd_abort;
            free(response->stat_pointer);
            free(response);
            free(json_data);
            return RETURN_ERR;
        }
        free(response->stat_pointer);
        free(response);
        return RETURN_OK;
    }

    assoc_data = static_cast<wifi_associated_dev3_t *>(response->stat_pointer);

    wifi_associated_dev3_t *assoc_stats = new (std::nothrow)
        wifi_associated_dev3_t[response->stat_array_size];
    if (assoc_stats == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: allocation of memory failed for %d\n",
            __func__, __LINE__, step->step_number);
        free(response->stat_pointer);
        free(response);
        free(json_data);
        step->m_ui_mgr->cci_error_code = EMALLOC;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }
    memset(assoc_stats, 0 , sizeof(wifi_associated_dev3_t));
    for (assoc_count = 0; assoc_count < (unsigned int)response->stat_array_size; assoc_count++) {
        uint8_mac_to_string_mac((uint8_t *)assoc_data[assoc_count].cli_MACAddress, str);
        string_mac_to_uint8_mac((uint8_t *)assoc_stats[assoc_count].cli_MACAddress, str);

        assoc_stats[assoc_count].cli_AuthenticationState =
            assoc_data[assoc_count].cli_AuthenticationState;

        assoc_stats[assoc_count].cli_LastDataDownlinkRate =
            assoc_data[assoc_count].cli_LastDataDownlinkRate;

        assoc_stats[assoc_count].cli_LastDataUplinkRate =
            assoc_data[assoc_count].cli_LastDataUplinkRate;

        assoc_stats[assoc_count].cli_SignalStrength = assoc_data[assoc_count].cli_SignalStrength;

        assoc_stats[assoc_count].cli_Retransmissions = assoc_data[assoc_count].cli_Retransmissions;

        assoc_stats[assoc_count].cli_Active = assoc_data[assoc_count].cli_Active;

        strncpy(assoc_stats[assoc_count].cli_OperatingStandard,
            assoc_data[assoc_count].cli_OperatingStandard,
            sizeof(assoc_stats[assoc_count].cli_OperatingStandard) - 1);
        assoc_stats[assoc_count]
            .cli_OperatingStandard[sizeof(assoc_stats[assoc_count].cli_OperatingStandard) - 1] =
            '\0';

        strncpy(assoc_stats[assoc_count].cli_OperatingChannelBandwidth,
            assoc_data[assoc_count].cli_OperatingChannelBandwidth,
            sizeof(assoc_stats[assoc_count].cli_OperatingChannelBandwidth) - 1);
        assoc_stats[assoc_count].cli_OperatingChannelBandwidth
            [sizeof(assoc_stats[assoc_count].cli_OperatingChannelBandwidth) - 1] = '\0';

        assoc_stats[assoc_count].cli_SNR = assoc_data[assoc_count].cli_SNR;

        strncpy(assoc_stats[assoc_count].cli_InterferenceSources,
            assoc_data[assoc_count].cli_InterferenceSources,
            sizeof(assoc_stats[assoc_count].cli_InterferenceSources) - 1);
        assoc_stats[assoc_count]
            .cli_InterferenceSources[sizeof(assoc_stats[assoc_count].cli_InterferenceSources) - 1] =
            '\0';

        assoc_stats[assoc_count].cli_DataFramesSentAck =
            assoc_data[assoc_count].cli_DataFramesSentAck;

        assoc_stats[assoc_count].cli_DataFramesSentNoAck =
            assoc_data[assoc_count].cli_DataFramesSentNoAck;

        assoc_stats[assoc_count].cli_BytesSent = assoc_data[assoc_count].cli_BytesSent;

        assoc_stats[assoc_count].cli_BytesReceived = assoc_data[assoc_count].cli_BytesReceived;

        assoc_stats[assoc_count].cli_RSSI = assoc_data[assoc_count].cli_RSSI;

        assoc_stats[assoc_count].cli_MinRSSI = assoc_data[assoc_count].cli_MinRSSI;

        assoc_stats[assoc_count].cli_MaxRSSI = assoc_data[assoc_count].cli_MaxRSSI;

        assoc_stats[assoc_count].cli_Disassociations = assoc_data[assoc_count].cli_Disassociations;

        assoc_stats[assoc_count].cli_AuthenticationFailures =
            assoc_data[assoc_count].cli_AuthenticationFailures;

        assoc_stats[assoc_count].cli_Associations = assoc_data[assoc_count].cli_Associations;

        assoc_stats[assoc_count].cli_PacketsSent = assoc_data[assoc_count].cli_PacketsSent;

        assoc_stats[assoc_count].cli_PacketsReceived = assoc_data[assoc_count].cli_PacketsReceived;

        assoc_stats[assoc_count].cli_ErrorsSent = assoc_data[assoc_count].cli_ErrorsSent;

        assoc_stats[assoc_count].cli_RetransCount = assoc_data[assoc_count].cli_RetransCount;

        assoc_stats[assoc_count].cli_FailedRetransCount =
            assoc_data[assoc_count].cli_FailedRetransCount;

        assoc_stats[assoc_count].cli_RetryCount = assoc_data[assoc_count].cli_RetryCount;

        assoc_stats[assoc_count].cli_MultipleRetryCount =
            assoc_data[assoc_count].cli_MultipleRetryCount;

        assoc_stats[assoc_count].cli_MaxDownlinkRate = assoc_data[assoc_count].cli_MaxDownlinkRate;

        assoc_stats[assoc_count].cli_MaxUplinkRate = assoc_data[assoc_count].cli_MaxUplinkRate;

        assoc_stats[assoc_count].cli_activeNumSpatialStreams =
            assoc_data[assoc_count].cli_activeNumSpatialStreams;

        assoc_stats[assoc_count].cli_TxFrames = assoc_data[assoc_count].cli_TxFrames;

        assoc_stats[assoc_count].cli_RxRetries = assoc_data[assoc_count].cli_RxRetries;

        assoc_stats[assoc_count].cli_RxErrors = assoc_data[assoc_count].cli_RxErrors;
    }

    for (unsigned int i = 0; i < response->stat_array_size; i++) {
        uint8_mac_to_string_mac((uint8_t *)assoc_stats[i].cli_MACAddress, str);
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Associated Client %u\n", __func__, __LINE__,
            i);
        wlan_emu_print(wlan_emu_log_level_dbg, "MAC Address %s\n", str);
        wlan_emu_print(wlan_emu_log_level_dbg, "Authentication State %s\n",
            assoc_stats[i].cli_AuthenticationState ? "true" : "false");
        wlan_emu_print(wlan_emu_log_level_dbg, "Last Data Downlink Rate %u\n",
            assoc_stats[i].cli_LastDataDownlinkRate);
        wlan_emu_print(wlan_emu_log_level_dbg, "Last Data Uplink Rate %u\n",
            assoc_stats[i].cli_LastDataUplinkRate);
        wlan_emu_print(wlan_emu_log_level_dbg, "Signal Strength %d\n",
            assoc_stats[i].cli_SignalStrength);
        wlan_emu_print(wlan_emu_log_level_dbg, "Retransmissions %u\n",
            assoc_stats[i].cli_Retransmissions);
        wlan_emu_print(wlan_emu_log_level_dbg, "Active %s\n",
            assoc_stats[i].cli_Active ? "true" : "false");
        wlan_emu_print(wlan_emu_log_level_dbg, "Operating Standard %s\n",
            assoc_stats[i].cli_OperatingStandard);
        wlan_emu_print(wlan_emu_log_level_dbg, "Operating Channel Bandwidth %s\n",
            assoc_stats[i].cli_OperatingChannelBandwidth);
        wlan_emu_print(wlan_emu_log_level_dbg, "SNR %d\n", assoc_stats[i].cli_SNR);
        wlan_emu_print(wlan_emu_log_level_dbg, "Interference Sources %s\n",
            assoc_stats[i].cli_InterferenceSources);
        wlan_emu_print(wlan_emu_log_level_dbg, "Data Frames Sent Ack %lu\n",
            assoc_stats[i].cli_DataFramesSentAck);
        wlan_emu_print(wlan_emu_log_level_dbg, "Data Frames Sent No Ack %lu\n",
            assoc_stats[i].cli_DataFramesSentNoAck);
        wlan_emu_print(wlan_emu_log_level_dbg, "Bytes Sent %lu\n", assoc_stats[i].cli_BytesSent);
        wlan_emu_print(wlan_emu_log_level_dbg, "Bytes Received %lu\n",
            assoc_stats[i].cli_BytesReceived);
        wlan_emu_print(wlan_emu_log_level_dbg, "RSSI %d\n", assoc_stats[i].cli_RSSI);
        wlan_emu_print(wlan_emu_log_level_dbg, "Min RSSI %d\n", assoc_stats[i].cli_MinRSSI);
        wlan_emu_print(wlan_emu_log_level_dbg, "Max RSSI %d\n", assoc_stats[i].cli_MaxRSSI);
        wlan_emu_print(wlan_emu_log_level_dbg, "Disassociations %u\n",
            assoc_stats[i].cli_Disassociations);
        wlan_emu_print(wlan_emu_log_level_dbg, "Authentication Failures %u\n",
            assoc_stats[i].cli_AuthenticationFailures);
        wlan_emu_print(wlan_emu_log_level_dbg, "Associations %llu\n",
            assoc_stats[i].cli_Associations);
        wlan_emu_print(wlan_emu_log_level_dbg, "Packets Sent %lu\n", assoc_stats[i].cli_PacketsSent);
        wlan_emu_print(wlan_emu_log_level_dbg, "Packets Received %lu\n",
            assoc_stats[i].cli_PacketsReceived);
        wlan_emu_print(wlan_emu_log_level_dbg, "Errors Sent %lu\n", assoc_stats[i].cli_ErrorsSent);
        wlan_emu_print(wlan_emu_log_level_dbg, "Retrans Count %lu\n",
            assoc_stats[i].cli_RetransCount);
        wlan_emu_print(wlan_emu_log_level_dbg, "Failed Retrans Count %lu\n",
            assoc_stats[i].cli_FailedRetransCount);
        wlan_emu_print(wlan_emu_log_level_dbg, "Retry Count %lu\n", assoc_stats[i].cli_RetryCount);
        wlan_emu_print(wlan_emu_log_level_dbg, "Multiple Retry Count %lu\n",
            assoc_stats[i].cli_MultipleRetryCount);
        wlan_emu_print(wlan_emu_log_level_dbg, "Max Downlink Rate %lu\n",
            assoc_stats[i].cli_MaxDownlinkRate);
        wlan_emu_print(wlan_emu_log_level_dbg, "Max Uplink Rate %lu\n",
            assoc_stats[i].cli_MaxUplinkRate);
        wlan_emu_print(wlan_emu_log_level_dbg, "Active Num Spatial Streams %u\n",
            assoc_stats[i].cli_activeNumSpatialStreams);
        wlan_emu_print(wlan_emu_log_level_dbg, "Tx Frames %llu\n", assoc_stats[i].cli_TxFrames);
        wlan_emu_print(wlan_emu_log_level_dbg, "Rx Retries %llu\n", assoc_stats[i].cli_RxRetries);
        wlan_emu_print(wlan_emu_log_level_dbg, "Rx Errors %llu\n", assoc_stats[i].cli_RxErrors);
    }

    if (wifi_hal_emu_set_assoc_clients_stats(vap_index, emu_state, assoc_stats,
            response->stat_array_size, phy_index, interface_index) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: wifi_hal_emu_set_assoc_clients_stats failed\n", __func__, __LINE__);
        step->m_ui_mgr->cci_error_code = ECLIENTSTATS;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        delete[] assoc_stats;
        free(response->stat_pointer);
        free(response);
        free(json_data);
        return RETURN_ERR;
    }

    delete[] assoc_stats;
    free(response->stat_pointer);
    free(response);
    free(json_data);

    return RETURN_OK;
}

int test_step_param_set_radio_diag_stats::webconfig_stats_set_execute_start()
{
    test_step_params_t *step = this;
    char *json_data;
    int ret = 0;
    char file_to_read[128] = { 0 };
    webconfig_cci_t *webconfig_data = NULL;
    int radio_index;
    bool emu_state;
    int phy_index;
    int interface_index;
    wifi_provider_response_t *response;
    radio_data_t *radiodiag_stats;
    unsigned int radiodiag_count = 0;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d - test_step_param_set_radio_diag_stats\n",
        __func__, __LINE__);

    stat_set_config_t *reference = (stat_set_config_t *)queue_peek(
        step->u.wifi_stats_set->stats_set_q, step->u.wifi_stats_set->current_stats_set_count);
    if (reference == NULL) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: queue_peek failed for current_stats_set_count : %d\n", __func__, __LINE__,
            step->u.wifi_stats_set->current_stats_set_count);
        step->m_ui_mgr->cci_error_code = EQPEEK;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }
    ret = snprintf(file_to_read, sizeof(file_to_read), "%s", reference->input_file_json);
    if ((ret < 0) || (ret >= sizeof(file_to_read))) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: snprintf failed returned : %d, file_length : %d for input_file_json : %s\n",
            __func__, __LINE__, ret, sizeof(file_to_read), reference->input_file_json);
        step->m_ui_mgr->cci_error_code = ESNPRINTF;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    ret = step->m_ui_mgr->read_config_file(file_to_read, &json_data);
    if (ret != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: read_config_file failed for file : %s\n",
            __func__, __LINE__, file_to_read);
        step->m_ui_mgr->cci_error_code = EFREAD;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    webconfig_data = step->m_ui_mgr->get_webconfig_data();
    webconfig_subdoc_data_t subdoc_data;

    if (webconfig_decode(&webconfig_data->webconfig, &subdoc_data, json_data) ==
        webconfig_error_none) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: webconfig_decode success\n", __func__,
            __LINE__);
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: webconfig_decode failed for file : %s\n",
            __func__, __LINE__, file_to_read);
        step->m_ui_mgr->cci_error_code = EWEBDECODE;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        free(json_data);
        return RETURN_ERR;
    }

    radio_index = step->u.wifi_stats_set->radio_index;
    emu_state = true;
    phy_index = webconfig_data->hal_cap.wifi_prop.radio_interface_map[radio_index].phy_index;
    interface_index = if_nametoindex(
        webconfig_data->hal_cap.wifi_prop.radio_interface_map[radio_index].interface_name);
    response = static_cast<wifi_provider_response_t *>((subdoc_data.u.decoded.collect_stats.stats));
    radiodiag_stats = static_cast<radio_data_t *>(response->stat_pointer);

    // Convert radio_data_t to wifi_radioTrafficStats2_t
    wifi_radioTrafficStats2_t *radioTrafficStats = new (std::nothrow)
        wifi_radioTrafficStats2_t[response->stat_array_size];
    if (radioTrafficStats == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: allocation of memory failed for %d\n",
            __func__, __LINE__, step->step_number);
        free(response->stat_pointer);
        free(response);
        free(json_data);
        step->m_ui_mgr->cci_error_code = EMALLOC;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    for (unsigned int i = 0; i < response->stat_array_size; i++) {

        radioTrafficStats[i].radio_ActivityFactor = radiodiag_stats[i].RadioActivityFactor;

        radioTrafficStats[i].radio_CarrierSenseThreshold_Exceeded =
            radiodiag_stats[i].CarrierSenseThreshold_Exceeded;

        radioTrafficStats[i].radio_NoiseFloor = radiodiag_stats[i].NoiseFloor;

        radioTrafficStats[i].radio_ChannelUtilization = radiodiag_stats[i].channelUtil;

        radioTrafficStats[i].radio_BytesSent = radiodiag_stats[i].radio_BytesSent;

        radioTrafficStats[i].radio_BytesReceived = radiodiag_stats[i].radio_BytesReceived;

        radioTrafficStats[i].radio_PacketsSent = radiodiag_stats[i].radio_PacketsSent;

        radioTrafficStats[i].radio_PacketsReceived = radiodiag_stats[i].radio_PacketsReceived;

        radioTrafficStats[i].radio_ErrorsSent = radiodiag_stats[i].radio_ErrorsSent;

        radioTrafficStats[i].radio_ErrorsReceived = radiodiag_stats[i].radio_ErrorsReceived;

        radioTrafficStats[i].radio_DiscardPacketsSent = radiodiag_stats[i].radio_DiscardPacketsSent;

        radioTrafficStats[i].radio_DiscardPacketsReceived =
            radiodiag_stats[i].radio_DiscardPacketsReceived;

        radioTrafficStats[i].radio_InvalidMACCount = radiodiag_stats[i].radio_InvalidMACCount;

        radioTrafficStats[i].radio_PacketsOtherReceived =
            radiodiag_stats[i].radio_PacketsOtherReceived;

        radioTrafficStats[i].radio_RetransmissionMetirc =
            radiodiag_stats[i].radio_RetransmissionMetirc;

        radioTrafficStats[i].radio_PLCPErrorCount = radiodiag_stats[i].radio_PLCPErrorCount;

        radioTrafficStats[i].radio_FCSErrorCount = radiodiag_stats[i].radio_FCSErrorCount;

        radioTrafficStats[i].radio_MaximumNoiseFloorOnChannel =
            radiodiag_stats[i].radio_MaximumNoiseFloorOnChannel;

        radioTrafficStats[i].radio_MinimumNoiseFloorOnChannel =
            radiodiag_stats[i].radio_MinimumNoiseFloorOnChannel;

        radioTrafficStats[i].radio_MedianNoiseFloorOnChannel =
            radiodiag_stats[i].radio_MedianNoiseFloorOnChannel;

        radioTrafficStats[i].radio_StatisticsStartTime =
            radiodiag_stats[i].radio_StatisticsStartTime;
    }

    for (unsigned int i = 0; i < response->stat_array_size; i++) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Radio Diag %u\n", __func__, __LINE__, i);
        wlan_emu_print(wlan_emu_log_level_dbg, "Radio Activity Factor %u\n",
            radioTrafficStats[i].radio_ActivityFactor);
        wlan_emu_print(wlan_emu_log_level_dbg, "Carrier Sense Threshold Exceeded %u\n",
            radioTrafficStats[i].radio_CarrierSenseThreshold_Exceeded);
        wlan_emu_print(wlan_emu_log_level_dbg, "Noise Floor %d\n",
            radioTrafficStats[i].radio_NoiseFloor);
        wlan_emu_print(wlan_emu_log_level_dbg, "Channel Util %d\n",
            radioTrafficStats[i].radio_ChannelUtilization);
        wlan_emu_print(wlan_emu_log_level_dbg, "Bytes Sent %lu\n",
            radioTrafficStats[i].radio_BytesSent);
        wlan_emu_print(wlan_emu_log_level_dbg, "Bytes Received %lu\n",
            radioTrafficStats[i].radio_BytesReceived);
        wlan_emu_print(wlan_emu_log_level_dbg, "Packets Sent %lu\n",
            radioTrafficStats[i].radio_PacketsSent);
        wlan_emu_print(wlan_emu_log_level_dbg, "Packets Received %lu\n",
            radioTrafficStats[i].radio_PacketsReceived);
        wlan_emu_print(wlan_emu_log_level_dbg, "Errors Sent %lu\n",
            radioTrafficStats[i].radio_ErrorsSent);
        wlan_emu_print(wlan_emu_log_level_dbg, "Errors Received %lu\n",
            radioTrafficStats[i].radio_ErrorsReceived);
        wlan_emu_print(wlan_emu_log_level_dbg, "Discard Packets Sent %lu\n",
            radioTrafficStats[i].radio_DiscardPacketsSent);
        wlan_emu_print(wlan_emu_log_level_dbg, "Discard Packets Received %lu\n",
            radioTrafficStats[i].radio_DiscardPacketsReceived);
        wlan_emu_print(wlan_emu_log_level_dbg, "Invalid MAC Count %lu\n",
            radioTrafficStats[i].radio_InvalidMACCount);
        wlan_emu_print(wlan_emu_log_level_dbg, "Packets Other Received %lu\n",
            radioTrafficStats[i].radio_PacketsOtherReceived);
        wlan_emu_print(wlan_emu_log_level_dbg, "Retransmission Metric %d\n",
            radioTrafficStats[i].radio_RetransmissionMetirc);
        wlan_emu_print(wlan_emu_log_level_dbg, "PLCP Error Count %lu\n",
            radioTrafficStats[i].radio_PLCPErrorCount);
        wlan_emu_print(wlan_emu_log_level_dbg, "FCS Error Count %lu\n",
            radioTrafficStats[i].radio_FCSErrorCount);
        wlan_emu_print(wlan_emu_log_level_dbg, "Maximum Noise Floor On Channel %d\n",
            radioTrafficStats[i].radio_MaximumNoiseFloorOnChannel);
        wlan_emu_print(wlan_emu_log_level_dbg, "Minimum Noise Floor On Channel %d\n",
            radioTrafficStats[i].radio_MinimumNoiseFloorOnChannel);
        wlan_emu_print(wlan_emu_log_level_dbg, "Median Noise Floor On Channel %d\n",
            radioTrafficStats[i].radio_MedianNoiseFloorOnChannel);
        wlan_emu_print(wlan_emu_log_level_dbg, "Statistics Start Time %lu\n",
            radioTrafficStats[i].radio_StatisticsStartTime);
    }

    if (wifi_hal_emu_set_radio_diag_stats(radio_index, emu_state, radioTrafficStats,
            response->stat_array_size, phy_index, interface_index) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: wifi_hal_emu_set_radio_diag_stats failed\n",
            __func__, __LINE__);
        step->m_ui_mgr->cci_error_code = ERADIOSTATS;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        delete[] radioTrafficStats;
        free(response->stat_pointer);
        free(response);
        free(json_data);
        return RETURN_ERR;
    }

    delete[] radioTrafficStats;
    free(response->stat_pointer);
    free(response);
    free(json_data);

    return RETURN_OK;
}

int test_step_param_set_radio_temperature_stats::webconfig_stats_set_execute_start()
{
    test_step_params_t *step = this;
    char *json_data;
    int ret = 0;
    char file_to_read[128] = { 0 };
    webconfig_cci_t *webconfig_data = NULL;
    int radio_index;
    bool emu_state;
    int phy_index;
    int interface_index;
    wifi_provider_response_t *response;
    radio_data_t *radio_data;
    unsigned int radiotemp_count = 0;
    int radio_temp = 0;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d - test_step_param_set_radio_temperature_stats\n",
        __func__, __LINE__);

    stat_set_config_t *reference = (stat_set_config_t *)queue_peek(
        step->u.wifi_stats_set->stats_set_q, step->u.wifi_stats_set->current_stats_set_count);
    if (reference == NULL) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: queue_peek failed for current_stats_set_count : %d\n", __func__, __LINE__,
            step->u.wifi_stats_set->current_stats_set_count);
        step->m_ui_mgr->cci_error_code = EQPEEK;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }
    ret = snprintf(file_to_read, sizeof(file_to_read), "%s", reference->input_file_json);
    if ((ret < 0) || (ret >= sizeof(file_to_read))) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: snprintf failed returned : %d, file_length : %d for input_file_json : %s\n",
            __func__, __LINE__, ret, sizeof(file_to_read), reference->input_file_json);
        step->m_ui_mgr->cci_error_code = ESNPRINTF;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    ret = step->m_ui_mgr->read_config_file(file_to_read, &json_data);
    if (ret != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: read_config_file failed for file : %s\n",
            __func__, __LINE__, file_to_read);
        step->m_ui_mgr->cci_error_code = EFREAD;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    webconfig_data = step->m_ui_mgr->get_webconfig_data();
    webconfig_subdoc_data_t subdoc_data;

    if (webconfig_decode(&webconfig_data->webconfig, &subdoc_data, json_data) ==
        webconfig_error_none) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: webconfig_decode success\n", __func__,
            __LINE__);
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: webconfig_decode failed for file : %s\n",
            __func__, __LINE__, file_to_read);
        step->m_ui_mgr->cci_error_code = EWEBDECODE;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        free(json_data);
        return RETURN_ERR;
    }

    radio_index = step->u.wifi_stats_set->radio_index;
    emu_state = true;
    phy_index = webconfig_data->hal_cap.wifi_prop.radio_interface_map[radio_index].phy_index;
    interface_index = if_nametoindex(
        webconfig_data->hal_cap.wifi_prop.radio_interface_map[radio_index].interface_name);
    response = static_cast<wifi_provider_response_t *>((subdoc_data.u.decoded.collect_stats.stats));
    radio_data = static_cast<radio_data_t *>(response->stat_pointer);

    radio_data_t *radiotemp_stats = new (std::nothrow) radio_data_t[response->stat_array_size];
    if (radiotemp_stats == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: allocation of memory failed for %d\n",
            __func__, __LINE__, step->step_number);
        free(response->stat_pointer);
        free(response);
        free(json_data);
        step->m_ui_mgr->cci_error_code = EMALLOC;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    for (radiotemp_count = 0; radiotemp_count < (unsigned int)response->stat_array_size;
         radiotemp_count++) {
        radiotemp_stats[radiotemp_count].radio_Temperature =
            radio_data[radiotemp_count].radio_Temperature;
    }

    for (unsigned int i = 0; i < response->stat_array_size; i++) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Radio Temperature %u:\n", __func__, __LINE__,
            radiotemp_stats[i].radio_Temperature);
        radio_temp = (unsigned int)radiotemp_stats[i].radio_Temperature;
    }

    if (wifi_hal_emu_set_radio_temp(radio_index, emu_state, radio_temp, phy_index,
            interface_index) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: wifi_hal_emu_set_radio_temp failed\n",
            __func__, __LINE__);
        step->m_ui_mgr->cci_error_code = ERADIOTEMP;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        delete[] radiotemp_stats;
        free(response->stat_pointer);
        free(response);
        free(json_data);
        return RETURN_ERR;
    }

    delete[] radiotemp_stats;
    free(response->stat_pointer);
    free(response);
    free(json_data);

    return RETURN_OK;
}

int test_step_param_set_stats_t::webconfig_stats_set_instance()
{
    test_step_params_t *step = this;
    int ret = 0;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);

    if (step->u.wifi_stats_set->stats_set_q != NULL) {
        if ((queue_count(step->u.wifi_stats_set->stats_set_q) > 0) &&
            (step->u.wifi_stats_set->current_stats_set_count >= 0)) {
            stat_set_config_t *reference = (stat_set_config_t *)queue_peek(
                step->u.wifi_stats_set->stats_set_q,
                step->u.wifi_stats_set->current_stats_set_count);
            if (reference == NULL) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: queue_peek failed\n", __func__,
                    __LINE__);
                step->m_ui_mgr->cci_error_code = EQPEEK;
                step->test_state = wlan_emu_tests_state_cmd_abort;
                return RETURN_ERR;
            }
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: File name : %s\n", __func__, __LINE__,
                reference->input_file_json);

            ret = webconfig_stats_set_execute_start();
            if (ret != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: webconfig_stats_set_execute_start failed\n", __func__, __LINE__);
                step->m_ui_mgr->cci_error_code = EWEBSET;
                step->test_state = wlan_emu_tests_state_cmd_abort;
                return RETURN_ERR;
            } else {
                wlan_emu_print(wlan_emu_log_level_dbg,
                    "%s:%d: webconfig_stats_set_execute_start success\n", __func__, __LINE__);
            }
        }
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: stats_set_q is NULL\n", __func__, __LINE__);
        step->m_ui_mgr->cci_error_code = EQALLOC;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    return RETURN_OK;
}

int test_step_param_set_stats_t::step_execute()
{
    test_step_params_t *step = this;
    int ret = 0;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);

    if (step == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: step is NULL\n", __func__, __LINE__);
        step->m_ui_mgr->cci_error_code = ESTEP;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    if (step->u.wifi_stats_set == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: wifi_stats_set is NULL\n", __func__,
            __LINE__);
        step->m_ui_mgr->cci_error_code = EWIFISTATS;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Executing step %d\n", __func__, __LINE__,
        step->step_number);

    if (step->u.wifi_stats_set->stats_set_q != NULL) {
        int count = queue_count(step->u.wifi_stats_set->stats_set_q);
        wlan_emu_print(wlan_emu_log_level_dbg,
            "%s:%d: queue_count or total instances for step number %d : %d\n", __func__, __LINE__,
            step->step_number, count);
        step->u.wifi_stats_set->current_stats_set_count = count - 1;
        step->execution_time = step->u.wifi_stats_set->total_duration;
        step->u.wifi_stats_set->set_exec_duration = 0;
        ret = webconfig_stats_set_instance();
        if (ret != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: webconfig_stats_set_instance failed\n",
                __func__, __LINE__);
            step->m_ui_mgr->cci_error_code = EWEBSET;
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        } else {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: webconfig_stats_set_instance success\n",
                __func__, __LINE__);
            if (step->fork == true) {
                step->test_state = wlan_emu_tests_state_cmd_wait;
            } else {
                step->test_state = wlan_emu_tests_state_cmd_continue;
            }
            return RETURN_OK;
        }
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: stats_set_q is NULL\n", __func__, __LINE__);
        step->m_ui_mgr->cci_error_code = EQALLOC;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    return RETURN_OK;
}

int test_step_param_set_stats_t::step_timeout()
{
    test_step_params_t *step = this;
    int ret = 0;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);

    if (step->u.wifi_stats_set->stats_set_q != NULL) {
        if ((queue_count(step->u.wifi_stats_set->stats_set_q) > 0) &&
            (step->u.wifi_stats_set->current_stats_set_count >= 0)) {
            stat_set_config_t *reference = (stat_set_config_t *)queue_peek(
                step->u.wifi_stats_set->stats_set_q,
                step->u.wifi_stats_set->current_stats_set_count);
            if (reference == NULL) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: queue_peek failed\n", __func__,
                    __LINE__);
                step->m_ui_mgr->cci_error_code = EQPEEK;
                step->test_state = wlan_emu_tests_state_cmd_abort;
                return RETURN_ERR;
            }
            if (step->timeout_count < step->execution_time) {
                wlan_emu_print(wlan_emu_log_level_dbg,
                    "%s:%d: Timeout count for step number : %d is -> %d\n", __func__, __LINE__,
                    step->step_number, step->timeout_count);
                step->timeout_count++;
                if (step->u.wifi_stats_set->set_exec_duration < reference->stats_duration) {
                    wlan_emu_print(wlan_emu_log_level_dbg,
                            "%s:%d: Duration for current instance: %d\n", __func__, __LINE__,
                            step->u.wifi_stats_set->set_exec_duration);
                    step->u.wifi_stats_set->set_exec_duration++;
                    if (step->fork == true) {
                        step->test_state = wlan_emu_tests_state_cmd_wait;
                    } else {
                        step->test_state = wlan_emu_tests_state_cmd_continue;
                    }

                    return RETURN_OK;
                }
            }

            if (step->u.wifi_stats_set->set_exec_duration == reference->stats_duration) {
                wlan_emu_print(wlan_emu_log_level_dbg,
                    "%s:%d: Timeout reached stats duration of -> %d for current instance\n",
                    __func__, __LINE__, reference->stats_duration);

                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Moving to next instance\n", __func__,
                    __LINE__);
                stat_set_config_t *new_reference = (stat_set_config_t *)queue_pop(
                    step->u.wifi_stats_set->stats_set_q);
                if (new_reference == NULL) {
                    wlan_emu_print(wlan_emu_log_level_err, "%s:%d: queue_pop failed\n", __func__,
                        __LINE__);
                    step->m_ui_mgr->cci_error_code = EQPOP;
                    step->test_state = wlan_emu_tests_state_cmd_abort;
                    return RETURN_ERR;
                }
                int count = queue_count(step->u.wifi_stats_set->stats_set_q);
                step->u.wifi_stats_set->current_stats_set_count = count - 1;
                step->u.wifi_stats_set->set_exec_duration = 0;
                wlan_emu_print(wlan_emu_log_level_dbg,
                        "%s:%d: Next instance for step number : %d\n", __func__, __LINE__,
                        step->step_number);
                ret = webconfig_stats_set_instance();
                if (ret != RETURN_OK) {
                    wlan_emu_print(wlan_emu_log_level_err,
                            "%s:%d: webconfig_stats_set_instance failed\n", __func__, __LINE__);
                    step->m_ui_mgr->cci_error_code = EWEBSET;
                    step->test_state = wlan_emu_tests_state_cmd_abort;
                    return RETURN_ERR;
                } else {
                    wlan_emu_print(wlan_emu_log_level_dbg,
                            "%s:%d: webconfig_stats_set_instance success\n", __func__, __LINE__);
                }
                step->u.wifi_stats_set->set_exec_duration++;
                if (step->fork == true) {
                    step->test_state = wlan_emu_tests_state_cmd_wait;
                } else {
                    step->test_state = wlan_emu_tests_state_cmd_continue;
                }
                return RETURN_OK;
            }
        }
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: stats_set_q is NULL\n", __func__, __LINE__);
    }

    if (step->timeout_count == step->execution_time) {
        wlan_emu_print(wlan_emu_log_level_dbg,
            "%s:%d: Timeout for step number : %d is reached - %d\n", __func__, __LINE__,
            step->step_number, step->timeout_count);
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Stop the api execution for the instance\n",
            __func__, __LINE__);
        ret = webconfig_stats_set_execute_stop();
        if (ret != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: webconfig_stats_set_execute_stop failed\n", __func__, __LINE__);
            step->m_ui_mgr->cci_error_code = EWEBSET;
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        } else {
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: webconfig_stats_set_execute_stop success\n", __func__, __LINE__);
        }
        step->test_state = wlan_emu_tests_state_cmd_results;
        return RETURN_OK;
    }

    if (step->timeout_count > step->execution_time) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: Timeout for step number : %d Exceeded - %d\n", __func__, __LINE__,
            step->step_number, step->timeout_count);
        ret = webconfig_stats_set_execute_stop();
        if (ret != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: webconfig_stats_set_execute_stop failed\n", __func__, __LINE__);
            step->m_ui_mgr->cci_error_code = EWEBSET;
            step->test_state = wlan_emu_tests_state_cmd_abort;
            return RETURN_ERR;
        } else {
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: webconfig_stats_set_execute_stop success\n", __func__, __LINE__);
        }
        step->m_ui_mgr->cci_error_code = EWEBSET;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    return RETURN_OK;
}

void test_step_param_set_stats_t::step_remove()
{
    test_step_param_set_stats_t *step = dynamic_cast<test_step_param_set_stats_t *>(this);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d : %d\n", __func__, __LINE__,
        step->u.wifi_stats_set->data_type);

    if (step == NULL) {
        return;
    }

    if (step->is_step_initialized == true) {
        if (step->u.wifi_stats_set->stats_set_q != NULL) {
            queue_destroy(step->u.wifi_stats_set->stats_set_q);
            step->u.wifi_stats_set->stats_set_q = NULL;
        }
        delete step->u.wifi_stats_set;
    }

    delete step;
    step = NULL;
    return;
}

test_step_param_set_radio_channel_stats::test_step_param_set_radio_channel_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);
    test_step_params_t *step = this;
    step->is_step_initialized = true;
    step->u.wifi_stats_set = new (std::nothrow) wifi_stats_set_t;
    if (step->u.wifi_stats_set == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: allocation of memory failed for %d\n",
            __func__, __LINE__, step->step_number);
        step->is_step_initialized = false;
        return;
    }
    memset(step->u.wifi_stats_set, 0, sizeof(wifi_stats_set_t));
    step->u.wifi_stats_set->stats_set_q = queue_create();
    if (step->u.wifi_stats_set->stats_set_q == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: stats_set_q failed for %d\n", __func__,
            __LINE__, step->step_number);
        delete step->u.wifi_stats_set;
        step->is_step_initialized = false;
        return;
    }
    step->execution_time = 0;
    step->timeout_count = 0;
    step->capture_frames = false;
}

test_step_param_set_radio_channel_stats::~test_step_param_set_radio_channel_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);
}

test_step_param_set_neighbor_stats::test_step_param_set_neighbor_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);
    test_step_params_t *step = this;
    step->is_step_initialized = true;
    step->u.wifi_stats_set = new (std::nothrow) wifi_stats_set_t;
    if (step->u.wifi_stats_set == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: allocation of memory failed for %d\n",
            __func__, __LINE__, step->step_number);
        step->is_step_initialized = false;
        return;
    }
    memset(step->u.wifi_stats_set, 0, sizeof(wifi_stats_set_t));
    step->u.wifi_stats_set->stats_set_q = queue_create();
    if (step->u.wifi_stats_set->stats_set_q == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: stats_set_q failed for %d\n", __func__,
            __LINE__, step->step_number);
        step->is_step_initialized = false;
        return;
    }
    step->execution_time = 0;
    step->timeout_count = 0;
    step->capture_frames = false;
}

test_step_param_set_neighbor_stats::~test_step_param_set_neighbor_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);
}

test_step_param_set_assoc_clients_stats::test_step_param_set_assoc_clients_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);
    test_step_params_t *step = this;
    step->is_step_initialized = true;
    step->u.wifi_stats_set = new (std::nothrow) wifi_stats_set_t;
    if (step->u.wifi_stats_set == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: allocation of memory failed for %d\n",
            __func__, __LINE__, step->step_number);
        step->is_step_initialized = false;
        return;
    }

    memset(step->u.wifi_stats_set, 0, sizeof(wifi_stats_set_t));
    step->u.wifi_stats_set->stats_set_q = queue_create();
    if (step->u.wifi_stats_set->stats_set_q == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: stats_set_q failed for %d\n", __func__,
            __LINE__, step->step_number);
        step->is_step_initialized = false;
        return;
    }
    step->execution_time = 0;
    step->timeout_count = 0;
    step->capture_frames = false;
}

test_step_param_set_assoc_clients_stats::~test_step_param_set_assoc_clients_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);
}

test_step_param_set_radio_diag_stats::test_step_param_set_radio_diag_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);
    test_step_params_t *step = this;
    step->is_step_initialized = true;
    step->u.wifi_stats_set = new (std::nothrow) wifi_stats_set_t;
    if (step->u.wifi_stats_set == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: allocation of memory failed for %d\n",
            __func__, __LINE__, step->step_number);
        step->is_step_initialized = false;
        return;
    }
    memset(step->u.wifi_stats_set, 0, sizeof(wifi_stats_set_t));

    step->u.wifi_stats_set->stats_set_q = queue_create();
    if (step->u.wifi_stats_set->stats_set_q == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: stats_set_q failed for %d\n", __func__,
            __LINE__, step->step_number);
        step->is_step_initialized = false;
        return;
    }

    step->execution_time = 0;
    step->timeout_count = 0;
    step->capture_frames = false;
}

test_step_param_set_radio_diag_stats::~test_step_param_set_radio_diag_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);
}

test_step_param_set_radio_temperature_stats::test_step_param_set_radio_temperature_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);
    test_step_params_t *step = this;
    step->is_step_initialized = true;
    step->u.wifi_stats_set = new (std::nothrow) wifi_stats_set_t;
    if (step->u.wifi_stats_set == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: allocation of memory failed for %d\n",
            __func__, __LINE__, step->step_number);
        step->is_step_initialized = false;
        return;
    }
    memset(step->u.wifi_stats_set, 0, sizeof(wifi_stats_set_t));
    step->execution_time = 0;
    step->timeout_count = 0;
    step->capture_frames = false;
}

test_step_param_set_radio_temperature_stats::~test_step_param_set_radio_temperature_stats()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d\n", __func__, __LINE__);
}
#endif //CONFIG_EXT_AGENT_CCI
