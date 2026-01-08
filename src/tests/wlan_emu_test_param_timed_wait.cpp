#include "wlan_emu_log.h"
#include "wlan_emu_test_params.h"
#include <assert.h>

int test_step_param_timed_wait::step_execute()
{
    test_step_params_t *step = this;

    step->execution_time = step->u.timed_wait->seconds;
    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: execution time : %d for %d\n", __func__,
        __LINE__, step->execution_time, step->step_number);

    step->test_state = wlan_emu_tests_state_cmd_continue;
    return RETURN_OK;
}

int test_step_param_timed_wait::step_timeout()
{

    test_step_params_t *step = this;

    wlan_emu_print(wlan_emu_log_level_dbg,
        "%s:%d: Test Step Num : %d timeout_count : %d execution_time : %d\n", __func__, __LINE__,
        step->step_number, step->timeout_count, step->execution_time);

    step->timeout_count++;
    if (step->timeout_count >= step->execution_time) {
        step->test_state = wlan_emu_tests_state_cmd_results;
    }
    return RETURN_OK;
}

void test_step_param_timed_wait::step_remove()
{
    test_step_param_timed_wait *step = dynamic_cast<test_step_param_timed_wait *>(this);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Destructor for command called\n", __func__,
        __LINE__);

    if (step == NULL) {
        return;
    }
    if (step->is_step_initialized == true) {
        delete step->u.timed_wait;
    }
    delete step;
    step = NULL;

    return;
}

int test_step_param_timed_wait::step_frame_filter(wlan_emu_msg_t *msg)
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

test_step_param_timed_wait::test_step_param_timed_wait()
{
    test_step_params_t *step = this;
    step->is_step_initialized = true;
    step->u.timed_wait = new (std::nothrow) timed_wait_t;
    if (step->u.timed_wait == nullptr) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: allocation of memory for timed_wait failed for %d\n", __func__, __LINE__,
            step->step_number);
        step->is_step_initialized = false;
        return;
    }
    memset(step->u.timed_wait, 0, sizeof(timed_wait_t));
    step->execution_time = 5;
    step->timeout_count = 0;
    step->capture_frames = false;
}

test_step_param_timed_wait::~test_step_param_timed_wait()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Destructor for command called\n", __func__,
        __LINE__);
}
