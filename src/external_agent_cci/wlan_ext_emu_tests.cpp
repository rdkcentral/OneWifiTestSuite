#include "wlan_ext_emu_tests.h"
#include "timespec_macro.h"
#include "wlan_ext_emu.h"
#include "wlan_ext_emu_test_step_params.h"
#include <sys/time.h>

#define POLL_PERIOD_1_SEC 1

/*
  void wlan_ext_emu_tests_t::test_start()
  {
  }

*/

void wlan_ext_emu_tests_t::test_stop()
{
    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: received stop\n", __func__, __LINE__);
    this->m_exit = true;
    m_msg_mgr->unsubscribe(this);
}

wlan_ext_test_step_params_t *wlan_ext_emu_tests_t::get_ext_step_from_index(int index)
{
    int step_total = queue_count(this->m_ext_emu->get_test_steps_q());

    if ((index >= step_total) || (index < 0)) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid index : %d\n", __func__, __LINE__,
            index);
        return NULL;
    }

    wlan_ext_test_step_params_t *step = NULL;

    step = (wlan_ext_test_step_params_t *)queue_peek(this->m_ext_emu->get_test_steps_q(), index);
    if (step == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: step is NULL for index : %d\n", __func__,
            __LINE__, index);
        return NULL;
    }

    return step;
}

void wlan_ext_emu_tests_t::wlan_emu_handle_agent_msg(wlan_emu_msg_t *msg)
{
    wlan_emu_msg_type_agent_msg_t *data;
    wlan_ext_test_step_params_t *step;
    queue_t *step_q;
    wlan_ext_emu_tests_t *test = this;
    wlan_ext_emu_t *ext_emu = test->m_ext_emu;
    wlan_ext_emu_msg_mgr_t *msg_mgr = test->m_msg_mgr;
    wlan_ext_emu_sta_mgr_t *sta_mgr = test->m_agent_sta_mgr;

    switch (msg->get_agent_ops_type()) {
    case wlan_emu_msg_agent_ops_type_cmd:
        data = msg->get_agent_msg();
        if (data->u.cmd == wlan_emu_msg_agent_cmd_type_start) {
            wlan_emu_print(wlan_emu_log_level_info, "%s:%d: received start\n", __func__, __LINE__);
            // update the state to start for agent
        } else if (data->u.cmd == wlan_emu_msg_agent_cmd_type_stop) {
            wlan_emu_print(wlan_emu_log_level_info, "%s:%d: received stop\n", __func__, __LINE__);
            test_stop();
        }
        break;
    case wlan_emu_msg_agent_ops_type_data:
        data = msg->get_agent_msg();
        wlan_emu_print(wlan_emu_log_level_info, "%s:%d: recevied buffer to be pushed : %p\n",
            __func__, __LINE__, data->u.buf);
        step = (wlan_ext_test_step_params_t *)data->u.buf;
        step_q = this->m_ext_emu->get_test_steps_q();
        wlan_emu_print(wlan_emu_log_level_info,
            "%s:%d: recevied buffer to be pushed : %p step : %p\n", __func__, __LINE__, data->u.buf,
            step);
        queue_push(step_q, step);
        step->step_state = wlan_emu_tests_state_cmd_start;
        step->wlan_ext_step_add_mgr_data(msg_mgr, ext_emu, sta_mgr);
        if (step->wlan_ext_step_execute() == RETURN_ERR) {
            step->step_state = wlan_emu_tests_state_cmd_abort;
        }

        break;
    default:
        break;
    }
}

void wlan_ext_emu_tests_t::process_msg(wlan_emu_msg_t *msg)
{
    int step_count = 0;
    wlan_ext_test_step_params_t *step;
    wlan_ext_emu_tests_t *test = this;
    wlan_ext_emu_t *ext_emu = test->m_ext_emu;
    int ret;
    int step_total = queue_count(ext_emu->get_test_steps_q());
    if (msg->get_msg_type() == wlan_emu_msg_type_agent) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: \n", __func__, __LINE__);
        wlan_emu_handle_agent_msg(msg);
    } else {
        // Need to add for frames as well.
        for (step_count = (step_total - 1); step_count >= 0; step_count--) {
            step = test->get_ext_step_from_index(step_count);
            if (step == NULL) {
                continue;
            }
            // Instead of going through all the steps, Check the state
            if ((step->step_state == wlan_emu_tests_state_cmd_continue) ||
                (step->step_state == wlan_emu_tests_state_cmd_wait)) {

                ret = step->wlan_ext_step_frame_filter(msg);
                if (ret == RETURN_UNHANDLED) {
                    // frame is not handled
                    wlan_emu_print(wlan_emu_log_level_dbg,
                        "%s:%d: Frame unhandled by step_number : %d\n", __func__, __LINE__,
                        step->step_number, step->step_seq_num);
                    continue;
                } else if (ret == RETURN_HANDLED) {
                    // frame is handled
                    wlan_emu_print(wlan_emu_log_level_dbg,
                        "%s:%d: Frame handled by step_number : %d\n", __func__, __LINE__,
                        step->step_number, step->step_seq_num);
                    return;
                }

                wlan_emu_print(wlan_emu_log_level_dbg,
                    "%s:%d: step_count : %d "
                    "%d step_number : %d\n",
                    __func__, __LINE__, step_count, step->step_number);
            } else {
                wlan_emu_print(wlan_emu_log_level_dbg,
                    "%s:%d: step_count : %d "
                    "%d step_number : %d in state : 0x%x\n",
                    __func__, __LINE__, step_count, step->step_number, step->step_state);
                continue;
            }
        }
    }
}

wlan_emu_msg_t *wlan_ext_emu_tests_t::pop_msg()
{
    wlan_emu_msg_t *msg = NULL;

    if (queue_count(m_test_msg_q) == 0) {
        return NULL;
    }

    msg = (wlan_emu_msg_t *)queue_pop(m_test_msg_q);

    return msg;
}

void *wlan_ext_emu_tests_t::test_function(void *arg)
{
    struct timespec interval;
    struct timespec time_to_wait;
    struct timespec t_start;
    int rc;
    int step_total = 0;
    int step_count = 0;
    int step_completed = 0;
    wlan_ext_test_step_params_t *temp_step;
    ext_agent_test_state_t agent_state;
    wlan_emu_ext_agent_interface_t *agent_interface;
    timespecclear(&t_start);
    wlan_ext_emu_tests_t *test = (wlan_ext_emu_tests_t *)arg;
    wlan_ext_emu_t *ext_emu = test->m_ext_emu;
    char *steps_status_json_str_src = NULL;
    agent_interface = ext_emu->get_agent_interface();

    ext_emu->ext_emu_send_signal_ready();
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: thread started\n", __func__, __LINE__);

    pthread_mutex_lock(&test->m_lock);
    while (test->m_exit == false) {
        interval.tv_sec = POLL_PERIOD_1_SEC; // TBD need to change this period
        interval.tv_nsec = 0;
        timespecadd(&t_start, &interval, &time_to_wait);

        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: thread started test->m_exit : %d\n",
            __func__, __LINE__, test->m_exit);
        rc = test->ext_test_wait(&time_to_wait);
        if ((rc == 0) || (queue_count(test->m_test_msg_q) != 0)) {
            // here we need to do pop_msg
            wlan_emu_msg_t *msg;
            while ((msg = test->pop_msg()) != NULL) {
                test->process_msg(msg);
                delete msg;
                if (test->m_exit == true) {
                    wlan_emu_print(wlan_emu_log_level_dbg,
                        "%s:%d: thread started test->m_exit : %d\n", __func__, __LINE__,
                        test->m_exit);
                    ext_emu->ext_emu_send_signal_idle();
                    pthread_mutex_unlock(&test->m_lock);
                    pthread_exit(NULL);
                    return NULL;
                }
            }
        } else if (rc == ETIMEDOUT) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: thread started test->m_exit : %d\n",
                __func__, __LINE__, test->m_exit);
            step_completed = 0;
            clock_gettime(CLOCK_MONOTONIC, &t_start);
            cJSON *agent_status = cJSON_CreateObject();
            step_total = queue_count(ext_emu->get_test_steps_q());
            cJSON_AddStringToObject(agent_status, "ExternalAgentHostName",
                agent_interface->agent_hostname.c_str());
            cJSON_AddNumberToObject(agent_status, "ExternalAgentStepCount", step_total);
            cJSON *steps_status_arr = cJSON_CreateArray();
            cJSON_AddItemToObject(agent_status, "ExternalAgentStepStatus", steps_status_arr);

            for (step_count = (step_total - 1); step_count >= 0; step_count--) {
                temp_step = test->get_ext_step_from_index(step_count);
                if (temp_step == NULL) {
                    ext_emu->ext_emu_send_signal_fail();
                    pthread_mutex_unlock(&test->m_lock);
                    pthread_exit(NULL);
                    return NULL;
                }
                if ((temp_step->step_state == wlan_emu_tests_state_cmd_wait) ||
                    (temp_step->step_state == wlan_emu_tests_state_cmd_continue)) {
                    wlan_emu_print(wlan_emu_log_level_info,
                        "%s:%d: Executing timeout for wait_step_number : %d step_state  : "
                        "%d\n",
                        __func__, __LINE__, temp_step->step_number, temp_step->step_state);
                    temp_step->wlan_ext_step_timeout();
                }
                // check error for all the steps, if in abort state return
                if (temp_step->step_state == wlan_emu_tests_state_cmd_abort) {
                    wlan_emu_print(wlan_emu_log_level_err,
                        "%s:%d: Fail for step_number : %d step_state  : %d\n", __func__, __LINE__,
                        temp_step->step_number, temp_step->step_state);
                    ext_emu->ext_emu_send_signal_fail();
                    cJSON_free(agent_status);
                    pthread_mutex_unlock(&test->m_lock);
                    pthread_exit(NULL);
                    return NULL;
                }
                // if we get results here, then change the state to results.
                if (temp_step->step_state == wlan_emu_tests_state_cmd_results) {
                    wlan_emu_print(wlan_emu_log_level_info,
                        "%s:%d: received results for step_number : %d step_state  : %d\n", __func__,
                        __LINE__, temp_step->step_number, temp_step->step_state);
                    step_completed++;
                }
                temp_step->wlan_ext_step_status_update(steps_status_arr);
            }
            if (step_total > 0) {
                if (step_total == step_completed) {
                    ext_emu->ext_emu_send_signal_complete();
                } else {
                    ext_emu->ext_emu_send_signal_running();
                }
            }

            agent_state = ext_emu->get_agent_test_state();
            cJSON_AddStringToObject(agent_status, "ExternalAgentStatus",
                agent_interface->agent_state_as_string(agent_state).c_str());

            // steps_status_json_str_src = NULL;
            // if (step_total > 0) {
            steps_status_json_str_src = cJSON_Print(agent_status);
            ext_emu->set_agent_step_status_json_str(steps_status_json_str_src);
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step status : %s\n", __func__, __LINE__,
                steps_status_json_str_src);
            // }
            cJSON_free(steps_status_json_str_src);
            cJSON_free(agent_status);
        }
    }
    pthread_mutex_unlock(&test->m_lock);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: thread exit\n", __func__, __LINE__);
    pthread_exit(NULL);

    return NULL;
}

void wlan_ext_emu_tests_t::push_msg(wlan_emu_msg_t *msg)
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d \n", __func__, __LINE__);
    pthread_mutex_lock(&m_lock);
    queue_push(m_test_msg_q, msg);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d \n", __func__, __LINE__);
    pthread_cond_signal(&m_cond);
    pthread_mutex_unlock(&m_lock);
}

wlan_ext_emu_tests_t::wlan_ext_emu_tests_t(wlan_ext_emu_msg_mgr_t *msg_mgr, wlan_ext_emu_t *ext_emu,
    wlan_ext_emu_sta_mgr_t *sta_mgr)
{
    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: initializing tests\n", __func__, __LINE__);
    m_exit = false;
    agent_test_initialized = false;
    m_test_msg_q = queue_create();
    if (m_test_msg_q == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: queue create failed\n", __func__, __LINE__);
        return;
    }
    if ((msg_mgr == NULL) || (ext_emu == NULL)) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: invalid args msg_mgr : %p ext_emu : %p\n",
            __func__, __LINE__, msg_mgr, ext_emu);
        return;
    }

    add_msg_mgr(msg_mgr);
    add_ext_emu(ext_emu);
    add_agent_sta_mgr(sta_mgr);
    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
    pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC);
    pthread_cond_init(&m_cond, &cond_attr);
    pthread_condattr_destroy(&cond_attr);
    pthread_mutex_init(&m_lock, NULL);

    ext_emu->create_q_for_test_steps_q();

    if (ext_emu->get_test_steps_q() == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: queue create failed\n", __func__, __LINE__);
        return;
    }

    if (pthread_create(&m_tid, NULL, wlan_ext_emu_tests_t::test_function, this) != 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Thread create error\n", __func__, __LINE__);
        return;
    }

    pthread_setname_np(m_tid, "test_function");
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Thread created with Name : test_function\n",
        __func__, __LINE__);

    m_msg_mgr->subscribe(this);
    agent_test_initialized = true;
}

wlan_ext_emu_tests_t::~wlan_ext_emu_tests_t()
{
    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: Calling the delete\n", __func__, __LINE__);
    // TBD : as we are using 'new' for queue element and using free as part of queue_destroy
    // test_stop();
    queue_destroy(m_test_msg_q);
    pthread_mutex_destroy(&m_lock);
    pthread_cond_destroy(&m_cond);
}
