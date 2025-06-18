#include "wlan_emu_msg.h"
#include "wlan_emu_log.h"
#include "cci_wifi_utils.hpp"
#include "common/ieee802_11_defs.h"
#include "common/defs.h"
#include "common/wpa_common.h"
#include "wifi_hal.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pcap.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

bool wlan_emu_msg_t::enable_beacon_dump = false;

bool wlan_emu_msg_t::operator==(wlan_emu_msg_mac80211_t *grd)
{
    return false;
}

bool wlan_emu_msg_t::operator==(wlan_emu_msg_cfg80211_t *grd)
{
    return false;
}

bool wlan_emu_msg_t::operator==(wlan_emu_msg_emu80211_t *grd)
{
    return false;
}

const char *wlan_emu_msg_t::cfg80211_ops_type_to_string()
{
#define CFG80211_TO_S(x) \
    case x:              \
        return #x;
    switch (m_msg.u.cfg80211.ops) {
        CFG80211_TO_S(wlan_emu_cfg80211_ops_type_none)
        CFG80211_TO_S(wlan_emu_cfg80211_ops_type_add_intf)
        CFG80211_TO_S(wlan_emu_cfg80211_ops_type_del_intf)
        CFG80211_TO_S(wlan_emu_cfg80211_ops_type_change_intf)
        CFG80211_TO_S(wlan_emu_cfg80211_ops_type_start_ap)
        CFG80211_TO_S(wlan_emu_cfg80211_ops_type_change_beacon)
        CFG80211_TO_S(wlan_emu_cfg80211_ops_type_stop_ap)
    default:
        break;
    }

    return "wlan_emu_cfg80211_ops_type_unknown";
}

const char *wlan_emu_msg_t::mac80211_ops_type_to_string()
{
#define MAC80211_TO_S(x) \
    case x:              \
        return #x;
    switch (m_msg.u.mac80211.ops) {
        MAC80211_TO_S(wlan_emu_mac80211_ops_type_none)
        MAC80211_TO_S(wlan_emu_mac80211_ops_type_tx)
        MAC80211_TO_S(wlan_emu_mac80211_ops_type_start)
        MAC80211_TO_S(wlan_emu_mac80211_ops_type_stop)
        MAC80211_TO_S(wlan_emu_mac80211_ops_type_add_intf)
        MAC80211_TO_S(wlan_emu_mac80211_ops_type_change_intf)
        MAC80211_TO_S(wlan_emu_mac80211_ops_type_remove_intf)
        MAC80211_TO_S(wlan_emu_mac80211_ops_type_config)
        MAC80211_TO_S(wlan_emu_mac80211_ops_type_bss_info_changed)
        MAC80211_TO_S(wlan_emu_mac80211_ops_type_start_ap)
        MAC80211_TO_S(wlan_emu_mac80211_ops_type_stop_ap)
    default:
        break;
    }

    return "wlan_emu_mac80211_ops_type_unknown";
}

const char *wlan_emu_msg_t::emu80211_ops_type_to_string()
{
#define EMU80211_TO_S(x) \
    case x:              \
        return #x;
    switch (m_msg.u.emu80211.ops) {
        EMU80211_TO_S(wlan_emu_emu80211_ops_type_none)
        EMU80211_TO_S(wlan_emu_emu80211_ops_type_tctrl)
        EMU80211_TO_S(wlan_emu_emu80211_ops_type_close)
    default:
        break;
    }

    return "wlan_emu_emu80211_ops_type_unknown";
}

const char *wlan_emu_msg_t::get_ops_string_by_msg_type()
{
    const char *str;

    switch (get_msg_type()) {
    case wlan_emu_msg_type_cfg80211:
        str = cfg80211_ops_type_to_string();
        break;
    case wlan_emu_msg_type_mac80211:
        str = mac80211_ops_type_to_string();
        break;
    case wlan_emu_msg_type_emu80211:
        str = emu80211_ops_type_to_string();
        break;
    case wlan_emu_msg_type_webconfig:
        str = "webconfig_update";
        break;
    case wlan_emu_msg_type_agent:
        str = "external_agent";
        break;
    default:
        str = "unknown";
        break;
    }

    return str;
}

void wlan_emu_msg_t::send_ctrl_msg(wlan_emu_test_coverage_t coverage, wlan_emu_test_type_t type,
    wlan_emu_emu80211_ctrl_type_t ctrl)
{
    int fd;
    ssize_t sz;

    m_msg.type = wlan_emu_msg_type_emu80211;
    m_msg.u.emu80211.ops = wlan_emu_emu80211_ops_type_tctrl;

    // set the test control parameters
    m_msg.u.emu80211.u.ctrl.ctrl = ctrl;
    m_msg.u.emu80211.u.ctrl.coverage = coverage;
    m_msg.u.emu80211.u.ctrl.type = type;

    if ((fd = open("/dev/rdkfmac_dev", O_RDWR)) < 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to open char dev, err: %d\n",
            __func__, __LINE__, errno);
        return;
    }

    if ((sz = write(fd, &m_msg, sizeof(wlan_emu_msg_data_t))) < 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to write to char dev, err: %d\n",
            __func__, __LINE__, errno);
    }

    close(fd);
}

void wlan_emu_msg_t::send_ctrl_msg(unsigned char *buff, unsigned int buff_len,
    wlan_emu_emu80211_cmd_type_t ctrl)
{
    int fd;
    ssize_t sz;

    m_msg.type = wlan_emu_msg_type_emu80211;
    m_msg.u.emu80211.ops = wlan_emu_emu80211_ops_type_cmnd;

    m_msg.u.emu80211.u.cmd.type = ctrl;
    memcpy(m_msg.u.emu80211.u.cmd.cmd_buffer, buff, sizeof(m_msg.u.emu80211.u.cmd.cmd_buffer));
    m_msg.u.emu80211.u.cmd.buff_len = buff_len;

    if ((fd = open("/dev/rdkfmac_dev", O_RDWR)) < 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to open char dev, err: %d\n",
            __func__, __LINE__, errno);
        return;
    }

    if ((sz = write(fd, &m_msg, sizeof(wlan_emu_msg_data_t))) < 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to write to char dev, err: %d\n",
            __func__, __LINE__, errno);
    }

    close(fd);
}

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

int wlan_emu_msg_t::dump(test_step_params_t *step)
{
    pcap_t *handle = NULL;
    pcap_dumper_t *dump_handle = NULL;
    struct pcap_pkthdr pkthdr;
    struct ieee80211_mgmt *head;
    void *buff = NULL;
    struct timeval tv;
    unsigned char *tmp_buff;
    size_t cap_len = 0;

    wlan_emu_msg_data_t *f_data = NULL;
    f_data = &(this->m_msg);

    char fname[248];
    mac_addr_str_t mac_str, c_mac_str;
    char *to_queue;
    char timestamp[24] = { 0 };
    unsigned int radio_index = 0;

    memset(fname, 0, sizeof(fname));
    to_queue = (char *)malloc(248);
    if (to_queue == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: NULL Pointer\n", __func__, __LINE__);
        return -1;
    }
    memset(to_queue, 0, 248);
    // get the time.
    gettimeofday(&tv, NULL);

    wlan_emu_msg_type_t msg_type = get_msg_type();
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
        wlan_emu_frm80211_ops_type_t ops_type = get_frm80211_ops_type();

        if (step->param_type == step_param_type_mgmt_frame_capture) {
            if (step->u.mgmt_frame_capture == nullptr) {
                return RETURN_ERR;
            }
            // in mgmt frame capture already the validation is done as part of test, so assigning
            // the input value
            radio_index = step->u.mgmt_frame_capture->radio_index;
        } else {
            step->m_ui_mgr->get_radioindex_from_bssid(f_data->u.frm80211.u.frame.macaddr,
                &radio_index);
        }

        if (step->param_type == step_param_type_station_management) {
            if (step->u.sta_test->capture_sta_requests == true) {
                step->m_ui_mgr->get_radioindex_from_bssid(f_data->u.frm80211.u.frame.client_macaddr,
                    &radio_index);
            }
        }

        mac_str_without_colon(f_data->u.frm80211.u.frame.client_macaddr, c_mac_str);
        if (get_msgname_from_msgtype() == RETURN_ERR) {
            return RETURN_ERR;
        }
        snprintf(fname, sizeof(fname), "/tmp/cci_res/%s_%d_%s_%s_%s_%s-%s_%d.pcap",
            step->test_case_id, step->step_number, timestamp, mac_str, c_mac_str,
            step->test_case_name, get_msg_name(), radio_index);
        memcpy(buff, f_data->u.frm80211.u.frame.frame, f_data->u.frm80211.u.frame.frame_len);
    } else if (msg_type == wlan_emu_msg_type_cfg80211) {
        mac_str_without_colon(f_data->u.cfg80211.u.start_ap.macaddr, mac_str);
        if (get_msgname_from_msgtype() == RETURN_ERR) {
            return RETURN_ERR;
        }

        step->m_ui_mgr->get_radioindex_from_bssid(f_data->u.cfg80211.u.start_ap.macaddr,
            &radio_index);

        snprintf(fname, sizeof(fname), "/tmp/cci_res/%s_%d_%s_%s_%s_%s-%s_%d.pcap",
            step->test_case_id, step->step_number, timestamp, mac_str, "NA", step->test_case_name,
            get_msg_name(), radio_index);

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

    wlan_emu_pcap_captures *capture = NULL;
    capture = new wlan_emu_pcap_captures;
    if (capture == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: unable to allocate memory for capture\n",
            __func__, __LINE__);
        return RETURN_ERR;
    }

    if (msg_type == wlan_emu_msg_type_frm80211) {
        capture->type = msg_type;
        capture->u.frm80211_ops = get_frm80211_ops_type();
    } else if (msg_type == wlan_emu_msg_type_cfg80211) {
        capture->type = msg_type;
        capture->u.cfg80211_ops = get_cfg80211_ops_type();
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

    pcap_dump((u_char *)dump_handle, &pkthdr, tmp_buff);
    pcap_dump_flush(dump_handle);
    pcap_close(handle);

    if (step->m_ui_mgr->step_upload_files(capture->pcap_file) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: step_upload_files failed\n", __func__,
            __LINE__);
        delete capture;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }
    delete capture;
    return RETURN_OK;
}

int wlan_emu_msg_t::get_msgname_from_msgtype()
{
    unsigned int frame_hdr_len = 0;
    struct ieee80211_mgmt *mgmt = NULL;
    wlan_emu_msg_type_t msg_type = get_msg_type();
    unsigned char *frame_buf = NULL;
    unsigned char *tmp_frame_buf = NULL;
    u8 tmp_key_info[2] = { 0 };
    u16 key_info = 0;

    unsigned int temp_count = 0;
    unsigned int count = 0;

    memset(msg_name, 0, sizeof(msg_name));

    if (msg_type == wlan_emu_msg_type_frm80211) {
        wlan_emu_frm80211_ops_type_t ops_type = get_frm80211_ops_type();
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: ops_type : %d\n", __func__, __LINE__,
            ops_type);

        frame_buf = (unsigned char *)this->m_msg.u.frm80211.u.frame.frame;

        mgmt = reinterpret_cast<ieee80211_mgmt *>(this->m_msg.u.frm80211.u.frame.frame);

        frame_hdr_len = ieee_frame_hdr_len(le16toh(mgmt->frame_control));

        switch (ops_type) {
        case wlan_emu_frm80211_ops_type_assoc_req:
            strncpy(msg_name, "assoc-request", sizeof(msg_name));
            break;
        case wlan_emu_frm80211_ops_type_prb_req:
            strncpy(msg_name, "probe-request", sizeof(msg_name));
            break;
        case wlan_emu_frm80211_ops_type_prb_resp:
            strncpy(msg_name, "probe-response", sizeof(msg_name));
            break;
        case wlan_emu_frm80211_ops_type_assoc_resp:
            strncpy(msg_name, "assoc-response", sizeof(msg_name));
            break;
        case wlan_emu_frm80211_ops_type_auth:
            u16 auth_alg, auth_transaction, status_code;
            auth_alg = le_to_host16(mgmt->u.auth.auth_alg);
            auth_transaction = le_to_host16(mgmt->u.auth.auth_transaction);
            status_code = le_to_host16(mgmt->u.auth.status_code);
            /*
                            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: auth_alg : %d
               auth_transaction : %d status_code : %d\n",
                                    __func__, __LINE__, auth_alg, auth_transaction, status_code);
            */

            if (auth_alg == WLAN_AUTH_OPEN) {
                if ((auth_transaction == 1) && (status_code == 0)) {
                    strncpy(msg_name, "auth-request", sizeof(msg_name));
                } else if ((auth_transaction == 2) && (status_code == 0)) {
                    strncpy(msg_name, "auth-response", sizeof(msg_name));
                }

            } else if ((auth_alg == WLAN_AUTH_SAE)) {
                if (auth_transaction == 1) {
                    strncpy(msg_name, "auth-commit", sizeof(msg_name));
                } else if (auth_transaction == 2) {
                    strncpy(msg_name, "auth-confirm", sizeof(msg_name));
                }
            }
            break;
        case wlan_emu_frm80211_ops_type_deauth:
            strncpy(msg_name, "deauthentication", sizeof(msg_name));
            break;
        case wlan_emu_frm80211_ops_type_disassoc:
            strncpy(msg_name, "disassoc", sizeof(msg_name));
            break;
        case wlan_emu_frm80211_ops_type_eapol:
            // 8 = sizeof logical link control header
            // 1 = 802.1x auth version
            // 4 = TLV for 802.1x
            tmp_frame_buf = frame_buf + frame_hdr_len + 8 + 1 + 4;

            memcpy(tmp_key_info, tmp_frame_buf, sizeof(key_info));
            key_info = WPA_GET_BE16(tmp_key_info);

            if (key_info & WPA_KEY_INFO_KEY_TYPE) {
                if ((key_info &
                        (WPA_KEY_INFO_KEY_TYPE | WPA_KEY_INFO_INSTALL | WPA_KEY_INFO_ACK |
                            WPA_KEY_INFO_MIC | WPA_KEY_INFO_SECURE)) ==
                    (WPA_KEY_INFO_KEY_TYPE | WPA_KEY_INFO_INSTALL | WPA_KEY_INFO_ACK |
                        WPA_KEY_INFO_MIC | WPA_KEY_INFO_SECURE)) {
                    strncpy(msg_name, "eapol-msg3", sizeof(msg_name));
                } else if ((key_info &
                               (WPA_KEY_INFO_KEY_TYPE | WPA_KEY_INFO_MIC | WPA_KEY_INFO_SECURE)) ==
                    (WPA_KEY_INFO_KEY_TYPE | WPA_KEY_INFO_MIC | WPA_KEY_INFO_SECURE)) {
                    strncpy(msg_name, "eapol-msg4", sizeof(msg_name));
                } else if ((key_info & (WPA_KEY_INFO_KEY_TYPE | WPA_KEY_INFO_MIC)) ==
                    (WPA_KEY_INFO_KEY_TYPE | WPA_KEY_INFO_MIC)) {
                    strncpy(msg_name, "eapol-msg2", sizeof(msg_name));
                } else if ((key_info & (WPA_KEY_INFO_ACK | WPA_KEY_INFO_KEY_TYPE)) ==
                    (WPA_KEY_INFO_KEY_TYPE | WPA_KEY_INFO_ACK)) {
                    strncpy(msg_name, "eapol-msg1", sizeof(msg_name));
                }
            }

            break;
        case wlan_emu_frm80211_ops_type_reassoc_req:
            strncpy(msg_name, "reassoc-request", sizeof(msg_name));
            break;
        case wlan_emu_frm80211_ops_type_reassoc_resp:
            strncpy(msg_name, "reassoc-response", sizeof(msg_name));
            break;
        case wlan_emu_frm80211_ops_type_action:
            strncpy(msg_name, "action", sizeof(msg_name));
            break;
        default:
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid msg type : %d\n", __func__,
                __LINE__, ops_type);
            strncpy(msg_name, "INVALID", sizeof(msg_name));
            break;
        }
    } else if (msg_type == wlan_emu_msg_type_cfg80211) {
        wlan_emu_cfg80211_ops_type_t ops_type = get_cfg80211_ops_type();
        switch (ops_type) {
        case wlan_emu_cfg80211_ops_type_start_ap:
            strncpy(msg_name, "beacon", sizeof(msg_name));
            break;
        default:
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid msg type : %d\n", __func__,
                __LINE__, ops_type);
            strncpy(msg_name, "INVALID", sizeof(msg_name));
            break;
        }
    } else {
        strncpy(msg_name, "INVALID", sizeof(msg_name));
    }

    if (msg_name[0] == '\0') {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid msg type\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    return RETURN_OK;
}

void wlan_emu_msg_t::unload_cfg80211_start_ap(test_step_params_t *step_config)
{
    wlan_emu_cfg80211_ops_type_t op_type = this->get_cfg80211_ops_type();
    switch (op_type) {
    case wlan_emu_cfg80211_ops_type_start_ap:
        if (wlan_emu_msg_t::enable_beacon_dump == true) {
            wlan_emu_msg_t::enable_beacon_dump = false;
            dump(step_config);
        } else {
            wlan_emu_msg_t::enable_beacon_dump = true;
        }
        break;
    default:
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Not handling operation\n", __func__,
            __LINE__);
        break;
    }
}

void wlan_emu_msg_t::unload_frm80211_msg(test_step_params_t *step_config)
{
    if (step_config != NULL) {
        dump(step_config);
    }

    return;
}

void wlan_emu_msg_t::load_cfg80211_start_ap(const char *pcap)
{
    wlan_emu_msg_data_t *data;

    m_msg.type = wlan_emu_msg_type_cfg80211;
    m_msg.u.cfg80211.ops = wlan_emu_cfg80211_ops_type_start_ap;
}

wlan_emu_msg_t::wlan_emu_msg_t(wlan_emu_msg_t &msg)
{
    memcpy(&m_msg, msg.get_msg(), sizeof(wlan_emu_msg_data_t));
}

wlan_emu_msg_t::wlan_emu_msg_t(wlan_emu_msg_data_t *msg)
{
    memcpy(&m_msg, msg, sizeof(wlan_emu_msg_data_t));
}

wlan_emu_msg_t::wlan_emu_msg_t()
{
}

wlan_emu_msg_t::~wlan_emu_msg_t()
{
}
