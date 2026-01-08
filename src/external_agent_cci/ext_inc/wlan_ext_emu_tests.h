#ifndef WLAN_EXT_EMU_TESTS_H
#define WLAN_EXT_EMU_TESTS_H
#include "collection.h"
#include "wlan_emu_log.h"
#include <pthread.h>

class wlan_ext_emu_msg_mgr_t;
class wlan_ext_emu_t;
class wlan_emu_msg_t;
class wlan_ext_test_step_params_t;
class wlan_ext_emu_sta_mgr_t;

class wlan_ext_emu_tests_t {
private:
    pthread_cond_t m_cond;
    pthread_mutex_t m_lock;
    pthread_t m_tid;
    wlan_ext_emu_msg_mgr_t *m_msg_mgr;
    wlan_ext_emu_sta_mgr_t *m_agent_sta_mgr; // TBD
    wlan_ext_emu_t *m_ext_emu;
    bool m_exit;
    queue_t *m_test_msg_q;
    static void *test_function(void *arg);
    bool agent_test_initialized;
    unsigned int current_step_index;

public:
    wlan_emu_msg_t *pop_msg();
    void process_msg(wlan_emu_msg_t *msg);
    void wlan_emu_handle_agent_msg(wlan_emu_msg_t *msg);

    inline void add_msg_mgr(wlan_ext_emu_msg_mgr_t *mgr)
    {
        m_msg_mgr = mgr;
    }

    inline void add_ext_emu(wlan_ext_emu_t *mgr)
    {
        m_ext_emu = mgr;
    }

    inline void add_agent_sta_mgr(wlan_ext_emu_sta_mgr_t *sta_mgr)
    {
        m_agent_sta_mgr = sta_mgr;
    }

    inline int ext_test_wait(struct timespec *time_to_wait)
    {
        return pthread_cond_timedwait(&m_cond, &m_lock, time_to_wait);
    }

    inline int get_agent_test_initialized()
    {
        return agent_test_initialized;
    }

    void test_stop();
    wlan_ext_test_step_params_t *get_ext_step_from_index(int index);

    void push_msg(wlan_emu_msg_t *msg);
    wlan_ext_emu_tests_t(wlan_ext_emu_msg_mgr_t *msg_mgr, wlan_ext_emu_t *ext_emu,
        wlan_ext_emu_sta_mgr_t *agent_sta_mgr);
    ~wlan_ext_emu_tests_t();
};
#endif
