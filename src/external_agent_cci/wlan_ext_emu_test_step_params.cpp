#include "wlan_ext_emu_test_step_params.h"
#include "wlan_emu_ext_agent_interface.h"
#include "cci_wifi_utils.hpp"
#include "wlan_emu_log.h"
#include "wlan_ext_emu.h"
#include "common/ieee802_11_defs.h"
#include "common/defs.h"
#include <fstream>
#include <iostream>
#include <pcap.h>
#include <cjson/cJSON.h>

static unsigned char radio_tap_header[] = { 0x00, 0x00, 0x3c, 0x00, 0x2f, 0x40, 0x10, 0xa0, 0x20,
    0x08, 0x00, 0xa0, 0x20, 0x08, 0x00, 0xa0, 0x20, 0x08, 0x00, 0xa0, 0x20, 0x08, 0x00, 0x00, 0x71,
    0xa1, 0x64, 0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x3c, 0x14, 0x40, 0x01, 0xe4, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xc3, 0x00, 0xf6, 0x05, 0x0c, 0x00, 0x00, 0x00, 0xe1, 0x00, 0xdb, 0x01, 0xd9,
    0x02, 0xe0, 0x03 };

static unsigned char traffic_indication_map[] = { 0x05, 0x04, 0x00, 0x01, 0x00, 0x00 };

static unsigned char eapol_qos_info[] = { 0x88, 0x02, 0x3c, 0x00, 0x04, 0xf0, 0x21, 0x5f, 0x03,
    0x7c, 0xe2, 0xdb, 0xd1, 0xe4, 0xdf, 0x53, 0xe2, 0xdb, 0xd1, 0xe4, 0xdf, 0x53, 0x10, 0x00, 0x05,
    0x00 };

static unsigned char llc_info[] = { 0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8e };

void wlan_ext_test_step_params_t::update_step_status_json(cJSON *agent_status)
{
    wlan_ext_test_step_params_t *step = this;
    cJSON *step_status_json;
    wlan_emu_ext_agent_interface_t *agent_interface;
    std::string state;
    wlan_ext_emu_t *ext_emu = get_ext_emu();
    char *data;

    std::string dir_path;
    std::string host_name;

    step_status_json = cJSON_CreateObject();
    cJSON_AddNumberToObject(step_status_json, "StepNumber", step->step_number);

    agent_interface = ext_emu->get_agent_interface();
    state = agent_interface->step_state_as_string(step->step_state);

    cJSON_AddStringToObject(step_status_json, "StepStatus", state.c_str());

    if (step->step_state == wlan_emu_tests_state_cmd_results) {
        cJSON_AddItemToObject(step_status_json, "ResultFiles", step->artifact_json_list);
    }
    cJSON_AddItemToArray(agent_status, step_status_json);
}

int wlan_ext_test_step_params_t::create_pcap(wlan_emu_msg_t *msg)
{
    wlan_ext_test_step_params_t *step = this;
    pcap_t *handle = NULL;
    pcap_dumper_t *dump_handle = NULL;
    struct pcap_pkthdr pkthdr;
    struct ieee80211_mgmt *head;
    void *buff = NULL;
    struct timeval tv;
    unsigned char *tmp_buff;
    size_t cap_len = 0;

    wlan_emu_msg_data_t *f_data = NULL;
    f_data = msg->get_msg();

    char fname[248];
    mac_addr_str_t mac_str, c_mac_str;
    char *to_queue;
    char timestamp[24] = { 0 };
    unsigned int radio_index = 0;

    memset(fname, 0, sizeof(fname));
    to_queue = (char *)malloc(248);
    if (to_queue == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: NULL Pointer\n", __func__, __LINE__);
        return RETURN_ERR;
    }
    memset(to_queue, 0, 248);
    // get the time.
    gettimeofday(&tv, NULL);

    radio_index = step->radio_index;

    wlan_emu_msg_type_t msg_type = msg->get_msg_type();
    if (msg_type == wlan_emu_msg_type_frm80211) {
        cap_len = sizeof(radio_tap_header) + f_data->u.frm80211.u.frame.frame_len;
    } else {
        cap_len = sizeof(radio_tap_header) +
          f_data->u.cfg80211.u.start_ap.head_len +
          sizeof(traffic_indication_map) +
          f_data->u.cfg80211.u.start_ap.tail_len;
    }

    buff = malloc(cap_len);
    memset(buff, 0, cap_len);
    tmp_buff = (unsigned char *)buff;

    memcpy(buff, radio_tap_header, sizeof(radio_tap_header));
    buff += sizeof(radio_tap_header);

    handle = pcap_open_dead(DLT_IEEE802_11_RADIO, 4000);
    if (handle == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Error creating pcap file\n", __func__,
                __LINE__);
        return RETURN_ERR;
    }

    if (get_current_time_string(timestamp, sizeof(timestamp)) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: get_current_time_string failed\n", __func__,
                __LINE__);
        return RETURN_ERR;
    }

    if (msg_type == wlan_emu_msg_type_frm80211) {
        mac_str_without_colon(f_data->u.frm80211.u.frame.macaddr, mac_str);
        wlan_emu_frm80211_ops_type_t ops_type = msg->get_frm80211_ops_type();
        mac_str_without_colon(f_data->u.frm80211.u.frame.client_macaddr, c_mac_str);

        if (msg->get_msgname_from_msgtype() == RETURN_ERR) {
            return RETURN_ERR;
        }
        snprintf(fname, sizeof(fname), "/tmp/cci_res/%s_%d_%s_%s_%s_%s-%s_%d.pcap",
                step->test_case_id, step->step_number, timestamp, mac_str, c_mac_str,
                step->test_case_name, msg->get_msg_name(), radio_index);
        memcpy(buff, f_data->u.frm80211.u.frame.frame, f_data->u.frm80211.u.frame.frame_len);
    } else if (msg_type == wlan_emu_msg_type_cfg80211) {
        mac_str_without_colon(f_data->u.cfg80211.u.start_ap.macaddr, mac_str);
        if (msg->get_msgname_from_msgtype() == RETURN_ERR) {
            return RETURN_ERR;
        }

        snprintf(fname, sizeof(fname), "/tmp/cci_res/%s_%d_%s_%s_%s_%s-%s_%d.pcap",
                step->test_case_id, step->step_number, timestamp, mac_str, "NA", step->test_case_name,
                msg->get_msg_name(), radio_index);

        head = (struct ieee80211_mgmt *)f_data->u.cfg80211.u.start_ap.beacon_head;

        // Filling timestamp.
        memcpy(head->u.beacon.timestamp, &tv.tv_sec, sizeof(head->u.beacon.timestamp));

        // Filling Head
        memcpy(buff, head, f_data->u.cfg80211.u.start_ap.head_len);
        buff += f_data->u.cfg80211.u.start_ap.head_len;

        // Filling TIM.
        memcpy(buff, traffic_indication_map, sizeof(traffic_indication_map));
        buff += sizeof(traffic_indication_map);

        // Filling Tail.
        memcpy(buff, f_data->u.cfg80211.u.start_ap.beacon_tail,
                f_data->u.cfg80211.u.start_ap.tail_len);
        buff += f_data->u.cfg80211.u.start_ap.tail_len;


	if (f_data->u.cfg80211.u.start_ap.beacon_head != NULL) {
            free(f_data->u.cfg80211.u.start_ap.beacon_head);
        }

        if (f_data->u.cfg80211.u.start_ap.beacon_tail != NULL) {
            free(f_data->u.cfg80211.u.start_ap.beacon_tail);
        }

    }

    memcpy(to_queue, fname, 248);
    dump_handle = pcap_dump_open(handle, fname);
    if (dump_handle == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Error creating pcap file\n", __func__,
                __LINE__);
        return -1;
    }

    pkthdr.caplen = cap_len;
    pkthdr.len = cap_len;
    if (step->artifact_json_list != NULL) {
        wlan_emu_pcap_captures *capture = NULL;
        capture = new wlan_emu_pcap_captures;
        if (capture == NULL) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: unable to allocate memory for capture\n",
                    __func__, __LINE__);
            return RETURN_ERR;
        }

        if (msg_type == wlan_emu_msg_type_frm80211) {
            capture->type = msg_type;
            capture->u.frm80211_ops = msg->get_frm80211_ops_type();
        } else if (msg_type == wlan_emu_msg_type_cfg80211) {
            capture->type = msg_type;
            capture->u.cfg80211_ops = msg->get_cfg80211_ops_type();
        }

        int ret = snprintf(capture->pcap_file, sizeof(capture->pcap_file), "%s", fname);

        if ((ret < 0) || (ret >= sizeof(capture->pcap_file))) {
            wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: snprintf failed return : %d input len : %d\n", __func__, __LINE__, ret,
                    sizeof(capture->pcap_file));
            return RETURN_ERR;
        }
        wlan_emu_print(wlan_emu_log_level_info,
                "%s:%d: updated the file : %s type : %d at step_number : %d\n", __func__, __LINE__,
                capture->pcap_file, capture->type, step->step_number);
        cJSON_AddItemToArray(step->artifact_json_list, cJSON_CreateString(capture->pcap_file));
        delete capture;
    }

    pcap_dump((u_char *)dump_handle, &pkthdr, tmp_buff);
    pcap_dump_flush(dump_handle);
    pcap_close(handle);

    return RETURN_OK;
}
