#include "wlan_emu_log.h"
#include "wlan_emu_test_params.h"
#include "wlan_emu_err_code.h"
#include <assert.h>
#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;

int test_step_param_mgmt_frame_capture::step_execute()
{

    FILE *src, *dst;
    char *buff;

    test_step_params_t *step = this;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Called for Test Step Num : %d\n", __func__,
        __LINE__, step->step_number);

    if (step->capture_frames == false) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Test Step Num : %d Invalid capture frames\n",
            __func__, __LINE__, step->step_number);
        step->m_ui_mgr->cci_error_code = EFRAMECAP;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    if (step->u.mgmt_frame_capture->duration > 0) {
        step->execution_time = step->u.mgmt_frame_capture->duration;
        step->test_state = wlan_emu_tests_state_cmd_continue;
    } else {
        step->test_state = wlan_emu_tests_state_cmd_results;
    }
    return RETURN_OK;
}

int test_step_param_mgmt_frame_capture::step_timeout()
{

    test_step_params_t *step = this;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Test Step Num : %d timeout_count : %d\n",
        __func__, __LINE__, step->step_number, step->timeout_count);

    if (step->test_state != wlan_emu_tests_state_cmd_results) {
        step->timeout_count++;

        if (step->execution_time == step->timeout_count) {
            step->test_state = wlan_emu_tests_state_cmd_results;
            wlan_emu_print(wlan_emu_log_level_info,
                "%s:%d: Test duration of %d  completed for step %d\n", __func__, __LINE__,
                step->execution_time, step->step_number);
            return RETURN_OK;
        }
    }
    return RETURN_OK;
}

void test_step_param_mgmt_frame_capture::step_remove()
{
    test_step_param_mgmt_frame_capture *step = dynamic_cast<test_step_param_mgmt_frame_capture *>(
        this);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Destructor for command called\n", __func__,
        __LINE__);

    if (step == nullptr) {
        return;
    }
    if (step->is_step_initialized == true) {
        delete step->u.mgmt_frame_capture;
    }
    delete step;
    step = nullptr;

    return;
}

int test_step_param_mgmt_frame_capture::step_frame_filter(wlan_emu_msg_t *msg)
{
    test_step_params_t *step = this;
    wlan_emu_msg_data_t *f_data = nullptr;
    unsigned int radio_index = 0;
    unsigned char ap_macaddr[ETH_ALEN];
    bool is_radio_index_found = false;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step number : %d\n", __func__, __LINE__,
        step->step_number);
    if (msg == nullptr) {
        return RETURN_UNHANDLED;
    }

    if ((step->capture_frames != true) ||
        (!(step->frame_request.msg_type & (1 << msg->get_msg_type())))) {
        return RETURN_UNHANDLED;
    }

    switch (msg->get_msg_type()) {
    case wlan_emu_msg_type_webconfig: // onewifi_webconfig
        break;
    case wlan_emu_msg_type_cfg80211: // beacon
        f_data = msg->get_msg();
        if (f_data->u.cfg80211.u.start_ap.phy_index == step->u.mgmt_frame_capture->radio_index) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Handled frame of type : %d\n", __func__,
                __LINE__, msg->get_frm80211_ops_type());

            msg->unload_cfg80211_start_ap(step);
            return RETURN_HANDLED;
        }
        break;
    case wlan_emu_msg_type_frm80211: // mgmt

        if (!(step->frame_request.frm80211_ops & (1 << msg->get_frm80211_ops_type()))) {
            return RETURN_UNHANDLED;
        }

        f_data = msg->get_msg();
        switch (f_data->u.frm80211.ops) {
        case wlan_emu_frm80211_ops_type_prb_req:
            is_radio_index_found = true;
            break;
        case wlan_emu_frm80211_ops_type_assoc_req:
        case wlan_emu_frm80211_ops_type_reassoc_req:
        case wlan_emu_frm80211_ops_type_action:
            if (step->m_ui_mgr->get_radioindex_from_bssid(f_data->u.frm80211.u.frame.client_macaddr,
                    &radio_index) == RETURN_OK) {
                if (radio_index == step->u.mgmt_frame_capture->radio_index) {
                    is_radio_index_found = true;
                }
            }
            break;
        case wlan_emu_frm80211_ops_type_auth:
        case wlan_emu_frm80211_ops_type_eapol:
        case wlan_emu_frm80211_ops_type_deauth:
        case wlan_emu_frm80211_ops_type_disassoc:
            if (step->m_ui_mgr->get_radioindex_from_bssid(f_data->u.frm80211.u.frame.macaddr,
                    &radio_index) == RETURN_OK) {
                if (radio_index == step->u.mgmt_frame_capture->radio_index) {
                    is_radio_index_found = true;
                }
            }

            if (is_radio_index_found == false) {
                if (step->m_ui_mgr->get_radioindex_from_bssid(
                        f_data->u.frm80211.u.frame.client_macaddr, &radio_index) == RETURN_OK) {
                    if (radio_index == step->u.mgmt_frame_capture->radio_index) {
                        is_radio_index_found = true;
                    }
                }
            }
            break;
        case wlan_emu_frm80211_ops_type_prb_resp:
        case wlan_emu_frm80211_ops_type_assoc_resp:
        case wlan_emu_frm80211_ops_type_reassoc_resp:
            if (step->m_ui_mgr->get_radioindex_from_bssid(f_data->u.frm80211.u.frame.macaddr,
                    &radio_index) == RETURN_OK) {
                if (radio_index == step->u.mgmt_frame_capture->radio_index) {
                    is_radio_index_found = true;
                }
            }
            break;

        default:
            return RETURN_UNHANDLED;
        }

        if (is_radio_index_found == true) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Handled frame of type : %d\n", __func__,
                __LINE__, msg->get_msg_type());
            msg->unload_frm80211_msg(step);
            return RETURN_HANDLED;
        }

        break;

    default:
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Not supported msg_type : %d\n", __func__,
            __LINE__, msg->get_msg_type());
        break;
    }
    return RETURN_UNHANDLED;
}

test_step_param_mgmt_frame_capture::test_step_param_mgmt_frame_capture()
{
    test_step_params_t *step = this;
    step->is_step_initialized = true;
    step->u.mgmt_frame_capture = new (std::nothrow) mgmt_frame_capture_t;
    if (step->u.mgmt_frame_capture == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: allocation of memory for mgmt_frame_capture failed for %d\n", __func__,
            __LINE__, step->step_number);
        step->is_step_initialized = false;
        return;
    }
    memset(step->u.mgmt_frame_capture, 0, sizeof(mgmt_frame_capture_t));
    step->execution_time = 5;
    step->timeout_count = 0;
    step->capture_frames = false;
}

test_step_param_mgmt_frame_capture::~test_step_param_mgmt_frame_capture()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Destructor for command called\n", __func__,
        __LINE__);
}
