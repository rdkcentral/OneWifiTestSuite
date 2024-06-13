#include <stdio.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include "wlan_emu_tests.h"
#include "wlan_emu_log.h"

test_step_params_t *wlan_emu_tests_t::get_step_from_index(int index)
{
    wlan_emu_test_case_config * test_config = this->get_test_config();
    int step_total = queue_count(test_config->test_steps_q);

    if ((index >= step_total) || (step_total < 0)) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid index : %d\n", __func__, __LINE__,
                index);
        return NULL;
    }

    test_step_params_t *step = NULL;

    step = (test_step_params_t *)queue_peek(test_config->test_steps_q, index);
    if (step == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: step is NULL for index : %d\n", __func__, __LINE__,
                index);
        return NULL;
    }

    return step;
}

void wlan_emu_tests_t::clear_pending_step(unsigned int step_seq_num)
{
    unsigned int mask = 0;
    wlan_emu_test_case_config * test_config = this->get_test_config();

    mask = ~(1<<step_seq_num);
    test_config->pending_steps &= mask;
    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: cleared the step_seq_num : %d pending steps : 0x%x\n", __func__, __LINE__, step_seq_num, test_config->pending_steps);
}

int wlan_emu_tests_t::get_next_pending_step(test_step_params_t **next_step)
{
    wlan_emu_test_case_config *test_config = this->get_test_config();
    test_step_params_t *cur_step = get_current_step();
    unsigned int step_seq_num = 0;
    unsigned int step_count =0;
    unsigned int check_bit = 0;
    test_step_params_t *temp_step = NULL;
    int step_total = queue_count(test_config->test_steps_q);

    if (cur_step == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Cur Step is NULL\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    if (test_config->pending_steps == 0) {
        wlan_emu_print(wlan_emu_log_level_info, "%s:%d: All steps executed\n", __func__, __LINE__);
        *next_step = NULL;
        return RETURN_OK;
    }

    if (cur_step->fork == false) {
        step_seq_num = (step_total-(cur_step->step_seq_num)-1);

        if (step_seq_num == 0) {
            step_seq_num = (step_total-1);
        } else {
            step_seq_num--;
        }
        *next_step = get_step_from_index(step_seq_num);
        if (next_step == NULL) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: step is NULL for %d\n", __func__, __LINE__);
            return RETURN_ERR;
        } else {
            return RETURN_OK;
        }

    }

    if (cur_step->fork == true) {
        step_seq_num = (step_total-(cur_step->step_seq_num)-1);
        for (step_count = (step_total-1); step_count >= 0; step_count--) {
            if (step_seq_num == 0) {
                step_seq_num = (step_total-1);
            } else {
                step_seq_num--;
            }
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Checking for seq_num : %d\n",
                    __func__, __LINE__, step_seq_num);
            temp_step = get_step_from_index(step_seq_num);
            if (temp_step == NULL) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: step is NULL at %d\n",
                        __func__, __LINE__, step_seq_num);
                *next_step = NULL;
                return RETURN_ERR;
            }
            if (temp_step->test_state != wlan_emu_tests_state_cmd_results) {
                wlan_emu_print(wlan_emu_log_level_info, "%s:%d: Found step %d in wait state from pending_steps : %d \n",
                        __func__, __LINE__, temp_step->step_number, test_config->pending_steps);
                *next_step = temp_step;
                return RETURN_OK;
            }
        }
    }

    wlan_emu_print(wlan_emu_log_level_err, "%s:%d: error  pending steps : 0x%x cur step Seq Num : %d\n",
            __func__, __LINE__, test_config->pending_steps, cur_step->step_seq_num);

    return RETURN_ERR;
}


test_step_params_t *wlan_emu_tests_t::get_current_step()
{
    wlan_emu_test_case_config * test_config = this->get_test_config();
    int current_test_step = test_config->current_test_step;

    return get_step_from_index(current_test_step);
}


void *wlan_emu_tests_t::test_function(void *arg)
{
    struct timespec time_to_wait;
    int rc, nbytes;
    struct timeval tv_now;
    int ret;

    wlan_emu_tests_t *test = (wlan_emu_tests_t *)arg;
    test_step_params_t *step;
    test_step_params_t *temp_step;


    //m_test_config
    wlan_emu_test_case_config * test_config = test->get_test_config();
    int step_total = queue_count(test_config->test_steps_q);
    int step_count = (step_total-1);
    test_config->pending_steps = (1<<step_total)-1;

    wlan_emu_ui_mgr_t *tmp_ui_mgr = test->get_ui_mgr();
    wlan_emu_msg_mgr_t *tmp_msg_mgr = test->get_msg_mgr();
    wlan_emu_sta_mgr_t *tmp_sta_mgr = test->get_sta_mgr();
    wlan_emu_test_case_config * temp_test_config = test->get_test_config();


    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step total : %d\n", __func__, __LINE__,
            step_total);

    step = test->get_step_from_index(step_count);
    if (step == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: \n", __func__, __LINE__);
        pthread_exit(NULL);
        return NULL;
    }

    pthread_mutex_lock(&test->m_lock);
    while (test->should_exit() == false) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step_number : %d test_state  : %d step_count : %d \n",
                __func__, __LINE__, step->step_number, step->test_state, test_config->current_test_step );
        if (step->test_state == wlan_emu_tests_state_cmd_request){
            step->step_add_mgr_data(tmp_msg_mgr, tmp_ui_mgr, tmp_sta_mgr, temp_test_config);
            test_config->current_test_step = (step_total-(step->step_seq_num)-1);
            if (step->capture_frames == true) {
                step->test_results_queue = queue_create();
            }
            snprintf(step->test_case_name, sizeof(step->test_case_name), "%s", test_config->test_case_name);
            snprintf(step->test_case_id, sizeof(step->test_case_id), "%s", test_config->test_case_id);
            test->send_ctrl_msg(wlan_emu_emu80211_ctrl_tstart);
            wlan_emu_print(wlan_emu_log_level_info, "%s:%d: Triggering step_number : %d test_state  : %d step_count : %d \n",
                    __func__, __LINE__, step->step_number, step->test_state, test_config->current_test_step );
            step->test_state = wlan_emu_tests_state_cmd_start;
        }
        gettimeofday(&tv_now, NULL);
        time_to_wait.tv_sec = tv_now.tv_sec + 1; //wait for 1 seconds
        time_to_wait.tv_nsec = 0;
        rc = test->test_wait(&time_to_wait);
        if (rc == 0) {
            switch (step->test_state) {
                case wlan_emu_tests_state_cmd_start:
                case wlan_emu_tests_state_cmd_wait:
                    //case wlan_emu_tests_state_cmd_results:
                case wlan_emu_tests_state_cmd_continue:
                    wlan_emu_msg_t *msg;
                    while ((msg = test->pop_msg()) != NULL) {
                        test->run(msg);
                        delete msg;
                    }
                    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step_number : %d test_state  : %d step_count : %d \n",
                            __func__, __LINE__, step->step_number, step->test_state, test_config->current_test_step );
                    continue;
                case wlan_emu_tests_state_cmd_results:
                    test->clear_pending_step(step->step_seq_num);
                    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: Results for step_number : %d test_state  : %d step_count : %d \n",
                            __func__, __LINE__, step->step_number, step->test_state, test_config->current_test_step );
                    ret = test->get_next_pending_step(&step);
                    if (ret == RETURN_ERR) {
                        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: next pending step failed\n", __func__, __LINE__);
                        step->test_state =  wlan_emu_tests_state_cmd_abort;
                        test_config->test_state =  wlan_emu_tests_state_cmd_abort;
                        pthread_mutex_unlock(&test->m_lock);
                        pthread_exit(NULL);
                        return NULL;
                    }
                    if (step == NULL) {
                        test_config->test_state =  wlan_emu_tests_state_cmd_results;
                        wlan_emu_print(wlan_emu_log_level_info, "%s:%d: Tests Done\n",
                                __func__, __LINE__);
                        test->stop();
                        test->test_done();
                        pthread_mutex_unlock(&test->m_lock);
                        pthread_exit(NULL);
                        return NULL;
                    }
                    test_config->current_test_step = (step_total-(step->step_seq_num)-1);
                    continue;

                case wlan_emu_tests_state_cmd_abort:
                default:
                    wlan_emu_print(wlan_emu_log_level_err, "%s:%d: abort for step_number : %d test_state  : %d step_count : %d \n",
                            __func__, __LINE__, step->step_number, step->test_state, test_config->current_test_step );
                    test->stop();
                    test->test_fail();
                break;
            }
        } else if (rc == ETIMEDOUT) {
             //First check if the step state is abort ?
            if (step->test_state == wlan_emu_tests_state_cmd_abort) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Fail for step_number : %d test_state  : %d step_count : %d \n",
                        __func__, __LINE__, step->step_number, step->test_state, test_config->current_test_step );
                test->stop();
                test->test_fail();
                step->test_state =  wlan_emu_tests_state_cmd_abort;
                test_config->test_state =  wlan_emu_tests_state_cmd_abort;
                pthread_mutex_unlock(&test->m_lock);
                pthread_exit(NULL);
                return NULL;
            }

            //perform respective step timeout function
            step->step_timeout();

            if (step->test_state == wlan_emu_tests_state_cmd_results) {
                test->clear_pending_step(step->step_seq_num);
                ret = test->get_next_pending_step(&step);
                if (ret == RETURN_ERR) {
                    wlan_emu_print(wlan_emu_log_level_err, "%s:%d: next pending step failed\n", __func__, __LINE__);
                    step->test_state =  wlan_emu_tests_state_cmd_abort;
                    test_config->test_state =  wlan_emu_tests_state_cmd_abort;
                    pthread_mutex_unlock(&test->m_lock);
                    pthread_exit(NULL);
                    return NULL;
                }
                //All the tests are done
                if (step == NULL) {
                    test_config->test_state =  wlan_emu_tests_state_cmd_results;
                    test->stop();
                    test->test_done();
                    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: Tests Done\n",
                            __func__, __LINE__);
                    pthread_mutex_unlock(&test->m_lock);
                    pthread_exit(NULL);
                    return NULL;
                }
                test_config->current_test_step = (step_total-(step->step_seq_num)-1);
                continue;

            } else if ((step->test_state == wlan_emu_tests_state_cmd_wait)) { 
                //Here wait means get the available next step
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: wait for step_number : %d test_state  : %d step_count : %d \n",
                        __func__, __LINE__, step->step_number, step->test_state, test_config->current_test_step );
                if (step->fork == true) {
                    //upate the next available step
                    ret = test->get_next_pending_step(&step);
                    if (ret == RETURN_ERR) {
                        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: next pending step failed\n", __func__, __LINE__);
                        step->test_state =  wlan_emu_tests_state_cmd_abort;
                        test_config->test_state =  wlan_emu_tests_state_cmd_abort;
                        pthread_mutex_unlock(&test->m_lock);
                        pthread_exit(NULL);
                        return NULL;
                    }
                    //here temp_step cant be null
                    if (step == NULL) {
                        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: pending step is NULL\n", __func__, __LINE__);
                        step->test_state =  wlan_emu_tests_state_cmd_abort;
                        test_config->test_state =  wlan_emu_tests_state_cmd_abort;
                        pthread_mutex_unlock(&test->m_lock);
                        pthread_exit(NULL);
                        return NULL;
                    }

                    test_config->current_test_step = (step_total-(step->step_seq_num)-1);
                    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: In fork new current_test_step : %d\n", __func__, __LINE__, test_config->current_test_step);
                }
                continue;
            } else if ((step->test_state == wlan_emu_tests_state_cmd_continue)) {
                //continue executing the same step
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: continue for step_number : %d test_state  : %d step_count : %d \n",
                        __func__, __LINE__, step->step_number, step->test_state, test_config->current_test_step );
                continue;
            } else {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Fail for step_number : %d test_state  : %d step_count : %d \n",
                        __func__, __LINE__, step->step_number, step->test_state, test_config->current_test_step );
                test->stop();
                test->test_fail();
                step->test_state =  wlan_emu_tests_state_cmd_abort;
                test_config->test_state =  wlan_emu_tests_state_cmd_abort;
                pthread_mutex_unlock(&test->m_lock);
                pthread_exit(NULL);
                return NULL;
            }
        }
    }
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: number : %d step->test_state  : %d\n", __func__, __LINE__, step->step_number, step->test_state );
    pthread_mutex_unlock(&test->m_lock);


    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: thread exit\n", __func__, __LINE__);
    pthread_exit(NULL);

    return NULL;
}


void wlan_emu_tests_t::wlan_emu_handle_emu80211_msg(wlan_emu_msg_t *msg)
{
    wlan_emu_msg_emu80211_t *data;
    wlan_emu_emu80211_ctrl_t *ctrl;
    test_step_params_t *curr_step = get_current_step();

    switch (msg->get_emu80211_ops_type()) {
        case wlan_emu_emu80211_ops_type_tctrl:
            data = msg->get_emu80211_msg();
            ctrl = &data->u.ctrl;
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: received test control command: %s\n", __func__, __LINE__,
                    msg->emu80211_ops_type_to_string());

            if (ctrl->coverage < wlan_emu_test_coverage_1 || ctrl->coverage >= wlan_emu_test_coverage_max) {
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: invalid input : : %d type : %d\n", __func__, __LINE__,
                        ctrl->coverage, ctrl->type);
                return;
            }

            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: triggering update for step : %d\n", __func__, __LINE__, curr_step->step_number);
            // received control message for test coverage 1 and VAP tests
            if (ctrl->ctrl == wlan_emu_emu80211_ctrl_tstart) {
                if (curr_step->step_execute() != RETURN_OK) {
                    wlan_emu_print(wlan_emu_log_level_err, "%s:%d: execute step failed for %d\n", __func__, __LINE__, curr_step->step_number);
                    curr_step->test_state = wlan_emu_tests_state_cmd_abort;
                } else {
                    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: execute step successful for %d\n", __func__, __LINE__, curr_step->step_number);
                }
            } else if (ctrl->ctrl == wlan_emu_emu80211_ctrl_tstop) {
            }
        break;
        default:
        break;
    }
}

void wlan_emu_tests_t::run(wlan_emu_msg_t *msg)
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: \n", __func__, __LINE__);
    wlan_emu_test_case_config * test_config = get_test_config();
    unsigned int pending_steps = test_config->pending_steps;
    unsigned int step_index_number = 0;
    unsigned int step_seq_number = 0;
    int step_total = queue_count(test_config->test_steps_q);
    test_step_params_t *step;
    int ret;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step_total : %d\n", __func__, __LINE__, step_total);

    if (msg == NULL) {
        return;
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: msg_type : %d \n", __func__, __LINE__, msg->get_msg_type());
    if (msg->get_msg_type() == wlan_emu_msg_type_emu80211) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: \n", __func__, __LINE__);
        wlan_emu_handle_emu80211_msg(msg);
    } else {
        step_seq_number = 0;
        //Process the msg for all the pending steps
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: pending_steps : %d\n", __func__, __LINE__, pending_steps);
        while (pending_steps) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: \n", __func__, __LINE__);
            if (!(pending_steps & 1)) {
                step_seq_number++;
                pending_steps >>= 1;
                continue;
            }
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: \n", __func__, __LINE__);
            step_index_number = step_total-step_seq_number-1;
            step = get_step_from_index(step_index_number);
            if (step == NULL) {
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step NULL at index %d\n", __func__, __LINE__, step_index_number);
                return;
            }

            ret = step->step_frame_filter(msg);
            if (ret == RETURN_UNHANDLED) {
                //frame is not handled
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Frame unhandled by step_index_number : %d step_number : %d step_seq_num : %d pending_steps : %d step_seq_number : %d\n",
                        __func__, __LINE__, step_index_number, step->step_number, step->step_seq_num, pending_steps, step_seq_number);
                pending_steps >>= 1;
                continue;
            } else if (ret == RETURN_HANDLED) {
                //frame is handled
                wlan_emu_print(wlan_emu_log_level_info, "%s:%d: Frame handled by step_index_number : %d step_number : %d step_seq_num : %d\n",
                        __func__, __LINE__, step_index_number, step->step_number, step->step_seq_num);
                return;
            }

            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step_index_number : %d step_number : %d step_seq_num : %d pending_steps : %d step_seq_number : %d\n",
                    __func__, __LINE__, step_index_number, step->step_number, step->step_seq_num, pending_steps, step_seq_number);
//            step_seq_number++; //This might not be required
//            pending_steps >>= 1; //This might not be required
        }
    }
}

void wlan_emu_tests_t::push_msg(wlan_emu_msg_t *msg)
{
    pthread_mutex_lock(&m_lock);
    queue_push(m_results, msg);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d \n", __func__, __LINE__);
    pthread_cond_signal(&m_cond);
    pthread_mutex_unlock(&m_lock);
}

wlan_emu_msg_t *wlan_emu_tests_t::pop_msg()
{
    wlan_emu_msg_t *msg = NULL;

    if (queue_count(m_results) == 0) {
        return NULL;
    }

    msg = (wlan_emu_msg_t *)queue_pop(m_results);

    return msg;
}

void wlan_emu_tests_t::send_ctrl_msg(wlan_emu_emu80211_ctrl_type_t type)
{
    wlan_emu_msg_t  msg;

    msg.send_ctrl_msg(m_coverage, m_type, type);
}

int wlan_emu_tests_t::start()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Called\n", __func__, __LINE__);
    if (m_msg_mgr == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: No messge manager\n", __func__, __LINE__);
        return -1;
    }

    if (pthread_create(&m_tid, NULL, wlan_emu_tests_t::test_function, this) != 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Thread create error\n", __func__, __LINE__);
        return -1;
    }

    m_msg_mgr->subscribe(this);
    if (pthread_join(m_tid, NULL)) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: pthread detach failed\n", __func__, __LINE__);
        return -1;
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Test start control command send\n", __func__, __LINE__);
    //send_ctrl_msg(wlan_emu_emu80211_ctrl_tstart);

    return 0;
}

void wlan_emu_tests_t::detroy_reference_sequence()
{
    wlan_emu_msg_t *elem;

    while (queue_count(m_reference) != 0) {
        elem = (wlan_emu_msg_t *)queue_pop(m_reference);
        delete elem;
    }
}

void wlan_emu_tests_t::stop()
{
    send_ctrl_msg(wlan_emu_emu80211_ctrl_tstop);

    if (m_msg_mgr != NULL) {
        m_msg_mgr->unsubscribe(this);
    }

    detroy_reference_sequence();
    //    test_done();
    m_exit = true;
}

wlan_emu_tests_t::wlan_emu_tests_t()
{
    m_exit = false;
    pthread_cond_init(&m_cond, NULL);
    pthread_mutex_init(&m_lock, NULL);
    m_results = queue_create();
    m_reference = queue_create();
    m_msg_mgr = NULL;
}

wlan_emu_tests_t::~wlan_emu_tests_t()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Calling the delete\n", __func__, __LINE__);
    queue_destroy(m_reference);
    queue_destroy(m_results);
    pthread_mutex_destroy(&m_lock);
    pthread_cond_destroy(&m_cond);
}

wlan_emu_tests_t::wlan_emu_tests_t(wlan_emu_msg_mgr_t *msg_mgr, wlan_emu_ui_mgr_t *ui_mgr, wlan_emu_sta_mgr_t *sta_mgr, wlan_emu_test_case_config *config)
{
    m_exit = false;
    pthread_cond_init(&m_cond, NULL);
    pthread_mutex_init(&m_lock, NULL);
    m_results = queue_create();
    m_reference = queue_create();
    m_msg_mgr = NULL;
    set_coverage(config->coverage_type, config->test_type);
    add_msg_mgr(msg_mgr);
    add_ui_mgr(ui_mgr);
    add_sta_mgr(sta_mgr);
    add_test_config(config);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step total : %d \n", __func__, __LINE__, queue_count(config->test_steps_q));
}



