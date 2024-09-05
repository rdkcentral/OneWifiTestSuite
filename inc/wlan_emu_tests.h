#ifndef WLAN_EMU_TESTS_H
#define WLAN_EMU_TESTS_H

#include "cci_wifi_utils.hpp"
#include "collection.h"
#include "wlan_emu_msg.h"
#include "wlan_emu_msg_mgr.h"
#include "wlan_emu_sta_mgr.h"
#include "wlan_emu_ui_mgr.h"
#include <pthread.h>

class wlan_emu_msg_mgr_t;

class wlan_emu_tests_t {
    wlan_emu_test_type_t m_type;
    wlan_emu_test_coverage_t m_coverage;
    bool m_exit;
    pthread_cond_t m_cond;
    pthread_mutex_t m_lock;
    pthread_t m_tid;
    queue_t *m_results;
    queue_t *m_reference;
    wlan_emu_msg_mgr_t *m_msg_mgr;
    wlan_emu_ui_mgr_t *m_ui_mgr;
    wlan_emu_sta_mgr_t *m_sta_mgr;
    wlan_emu_test_case_config *m_test_config;
    void wlan_emu_handle_emu80211_msg(wlan_emu_msg_t *msg);

public:
    int start();
    void stop();

    inline void test_done()
    {
        m_ui_mgr->signal_test_done(m_coverage);
    }

    inline void test_fail()
    {
        m_ui_mgr->signal_report_test_fail();
    }

    bool should_exit()
    {
        return m_exit;
    }

    inline int test_wait(struct timespec *time_to_wait)
    {
        return pthread_cond_timedwait(&m_cond, &m_lock, time_to_wait);
    }

    void push_msg(wlan_emu_msg_t *msg);
    wlan_emu_msg_t *pop_msg();

    inline void add_msg_mgr(wlan_emu_msg_mgr_t *mgr)
    {
        m_msg_mgr = mgr;
    }

    inline void add_ui_mgr(wlan_emu_ui_mgr_t *mgr)
    {
        m_ui_mgr = mgr;
    }

    inline void add_sta_mgr(wlan_emu_sta_mgr_t *mgr)
    {
        m_sta_mgr = mgr;
    }

    inline void add_test_config(wlan_emu_test_case_config *test_config)
    {
        m_test_config = test_config;
    }

    inline wlan_emu_test_case_config *get_test_config()
    {
        return m_test_config;
    }

    inline void remove_msg_mgr()
    {
        m_msg_mgr = NULL;
    }

    inline void set_coverage(wlan_emu_test_coverage_t coverage, wlan_emu_test_type_t type)
    {
        m_coverage = coverage;
        m_type = type;
    }

    inline queue_t *get_reference_queue()
    {
        return m_reference;
    }

    inline wlan_emu_ui_mgr_t *get_ui_mgr()
    {
        return m_ui_mgr;
    }

    inline wlan_emu_msg_mgr_t *get_msg_mgr()
    {
        return m_msg_mgr;
    }

    inline wlan_emu_sta_mgr_t *get_sta_mgr()
    {
        return m_sta_mgr;
    }

    static void *test_function(void *arg);

    void run(wlan_emu_msg_t *msg);
    void send_ctrl_msg(wlan_emu_emu80211_ctrl_type_t type);
    void detroy_reference_sequence();
    test_step_params_t *get_step_from_index(int index);
    test_step_params_t *get_current_step();

    wlan_emu_tests_t();
    ~wlan_emu_tests_t();
    wlan_emu_tests_t(wlan_emu_msg_mgr_t *msg_mgr, wlan_emu_ui_mgr_t *ui_mgr,
        wlan_emu_sta_mgr_t *sta_mgr, wlan_emu_test_case_config *config);
    void clear_pending_step(unsigned int step_seq_num);
    int get_next_pending_step(test_step_params_t **next_step);
};

class wlan_emu_tests_private_vap_t : public wlan_emu_tests_t {
public:
    wlan_emu_tests_private_vap_t(wlan_emu_msg_mgr_t *msg_mgr, wlan_emu_ui_mgr_t *ui_mgr,
        wlan_emu_sta_mgr_t *sta_mgr, wlan_emu_test_case_config *config) :
        wlan_emu_tests_t(msg_mgr, ui_mgr, sta_mgr, config)
    {
        if (config->test_type == wlan_emu_test_1_subtype_ns_private) {
            snprintf(config->test_case_name, sizeof(config->test_case_name), "%s", "NC1-Private");
        }
    }

    wlan_emu_tests_private_vap_t()
    {
        remove_msg_mgr();
    }

    ~wlan_emu_tests_private_vap_t();
};

class wlan_emu_tests_radio_t : public wlan_emu_tests_t {
public:
    wlan_emu_tests_radio_t(wlan_emu_msg_mgr_t *msg_mgr, wlan_emu_ui_mgr_t *ui_mgr,
        wlan_emu_sta_mgr_t *sta_mgr, wlan_emu_test_case_config *config) :
        wlan_emu_tests_t(msg_mgr, ui_mgr, sta_mgr, config)
    {
        if (config->test_type == wlan_emu_test_1_subtype_radio) {
            snprintf(config->test_case_name, sizeof(config->test_case_name), "%s", "NC1-Radio");
        }
    }

    wlan_emu_tests_radio_t()
    {
        remove_msg_mgr();
    }

    ~wlan_emu_tests_radio_t();
};

class wlan_emu_tests_xfinity_open_t : public wlan_emu_tests_t {
public:
    wlan_emu_tests_xfinity_open_t(wlan_emu_msg_mgr_t *msg_mgr, wlan_emu_ui_mgr_t *ui_mgr,
        wlan_emu_sta_mgr_t *sta_mgr, wlan_emu_test_case_config *config) :
        wlan_emu_tests_t(msg_mgr, ui_mgr, sta_mgr, config)
    {
        if (config->test_type == wlan_emu_test_1_subtype_ns_public_xfinity_open) {
            snprintf(config->test_case_name, sizeof(config->test_case_name), "%s",
                "NC1-Xfinity-Open");
        }
    }

    wlan_emu_tests_xfinity_open_t()
    {
        remove_msg_mgr();
    }

    ~wlan_emu_tests_xfinity_open_t();
};

class wlan_emu_tests_xfinity_secure_t : public wlan_emu_tests_t {
public:
    wlan_emu_tests_xfinity_secure_t(wlan_emu_msg_mgr_t *msg_mgr, wlan_emu_ui_mgr_t *ui_mgr,
        wlan_emu_sta_mgr_t *sta_mgr, wlan_emu_test_case_config *config) :
        wlan_emu_tests_t(msg_mgr, ui_mgr, sta_mgr, config)
    {
        if (config->test_type == wlan_emu_test_1_subtype_ns_public_xfinity_secure) {
            snprintf(config->test_case_name, sizeof(config->test_case_name), "%s",
                "NC1-Xfinity-Secure");
        }
    }

    wlan_emu_tests_xfinity_secure_t()
    {
        remove_msg_mgr();
    }

    ~wlan_emu_tests_xfinity_secure_t();
};

class wlan_emu_tests_xhs_t : public wlan_emu_tests_t {
public:
    wlan_emu_tests_xhs_t(wlan_emu_msg_mgr_t *msg_mgr, wlan_emu_ui_mgr_t *ui_mgr,
        wlan_emu_sta_mgr_t *sta_mgr, wlan_emu_test_case_config *config) :
        wlan_emu_tests_t(msg_mgr, ui_mgr, sta_mgr, config)
    {
        if (config->test_type == wlan_emu_test_1_subtype_ns_managed_xhs) {
            snprintf(config->test_case_name, sizeof(config->test_case_name), "%s", "NC1-Xhs");
        }
    }

    wlan_emu_tests_xhs_t()
    {
        remove_msg_mgr();
    }

    ~wlan_emu_tests_xhs_t();
};

class wlan_emu_tests_lnf_enterprise_t : public wlan_emu_tests_t {
public:
    wlan_emu_tests_lnf_enterprise_t(wlan_emu_msg_mgr_t *msg_mgr, wlan_emu_ui_mgr_t *ui_mgr,
        wlan_emu_sta_mgr_t *sta_mgr, wlan_emu_test_case_config *config) :
        wlan_emu_tests_t(msg_mgr, ui_mgr, sta_mgr, config)
    {
        if (config->test_type == wlan_emu_test_1_subtype_ns_managed_lnf_enterprise) {
            snprintf(config->test_case_name, sizeof(config->test_case_name), "%s",
                "NC1-Lnf-Enterprise");
        }
    }

    wlan_emu_tests_lnf_enterprise_t()
    {
        remove_msg_mgr();
    }

    ~wlan_emu_tests_lnf_enterprise_t();
};

class wlan_emu_tests_lnf_secure_t : public wlan_emu_tests_t {
public:
    wlan_emu_tests_lnf_secure_t(wlan_emu_msg_mgr_t *msg_mgr, wlan_emu_ui_mgr_t *ui_mgr,
        wlan_emu_sta_mgr_t *sta_mgr, wlan_emu_test_case_config *config) :
        wlan_emu_tests_t(msg_mgr, ui_mgr, sta_mgr, config)
    {
        if (config->test_type == wlan_emu_test_1_subtype_ns_managed_lnf_secure) {
            snprintf(config->test_case_name, sizeof(config->test_case_name), "%s",
                "NC1-Lnf-Secure");
        }
    }

    wlan_emu_tests_lnf_secure_t()
    {
        remove_msg_mgr();
    }

    ~wlan_emu_tests_lnf_secure_t();
};

class wlan_emu_tests_mesh_backhaul_t : public wlan_emu_tests_t {
public:
    wlan_emu_tests_mesh_backhaul_t(wlan_emu_msg_mgr_t *msg_mgr, wlan_emu_ui_mgr_t *ui_mgr,
        wlan_emu_sta_mgr_t *sta_mgr, wlan_emu_test_case_config *config) :
        wlan_emu_tests_t(msg_mgr, ui_mgr, sta_mgr, config)
    {
        if (config->test_type == wlan_emu_test_1_subtype_ns_managed_mesh_backhaul) {
            snprintf(config->test_case_name, sizeof(config->test_case_name), "%s",
                "NC1-Mesh-Backhaul");
        }
    }

    wlan_emu_tests_mesh_backhaul_t()
    {
        remove_msg_mgr();
    }

    ~wlan_emu_tests_mesh_backhaul_t();
};

class wlan_emu_tests_mesh_client_t : public wlan_emu_tests_t {
public:
    wlan_emu_tests_mesh_client_t(wlan_emu_msg_mgr_t *msg_mgr, wlan_emu_ui_mgr_t *ui_mgr,
        wlan_emu_sta_mgr_t *sta_mgr, wlan_emu_test_case_config *config) :
        wlan_emu_tests_t(msg_mgr, ui_mgr, sta_mgr, config)
    {
        if (config->test_type == wlan_emu_test_1_subtype_ns_managed_mesh_client) {
            snprintf(config->test_case_name, sizeof(config->test_case_name), "%s",
                "NC1-Mesh-Client");
        }
    }

    wlan_emu_tests_mesh_client_t()
    {
        remove_msg_mgr();
    }

    ~wlan_emu_tests_mesh_client_t();
};

class wlan_emu_tests_cli_probe_response : public wlan_emu_tests_t {
public:
    wlan_emu_tests_cli_probe_response(wlan_emu_msg_mgr_t *msg_mgr, wlan_emu_ui_mgr_t *ui_mgr,
        wlan_emu_sta_mgr_t *sta_mgr, wlan_emu_test_case_config *config) :
        wlan_emu_tests_t(msg_mgr, ui_mgr, sta_mgr, config)
    {
        if (config->test_type == wlan_emu_test_1_subtype_cc_probe_response) {
            snprintf(config->test_case_name, sizeof(config->test_case_name), "%s",
                "CC1-Probe-Response");
        }
    }

    wlan_emu_tests_cli_probe_response()
    {
        remove_msg_mgr();
    }

    ~wlan_emu_tests_cli_probe_response();
};

class wlan_emu_tests_cli_auth : public wlan_emu_tests_t {
public:
    wlan_emu_tests_cli_auth(wlan_emu_msg_mgr_t *msg_mgr, wlan_emu_ui_mgr_t *ui_mgr,
        wlan_emu_sta_mgr_t *sta_mgr, wlan_emu_test_case_config *config) :
        wlan_emu_tests_t(msg_mgr, ui_mgr, sta_mgr, config)
    {
        if (config->test_type == wlan_emu_test_1_subtype_cc_authentication) {
            snprintf(config->test_case_name, sizeof(config->test_case_name), "%s",
                "CC1-Authentication");
        }
    }

    wlan_emu_tests_cli_auth()
    {
        remove_msg_mgr();
    }

    ~wlan_emu_tests_cli_auth();
};

class wlan_emu_tests_stats_get : public wlan_emu_tests_t {
public:
    wlan_emu_tests_stats_get(wlan_emu_msg_mgr_t *msg_mgr, wlan_emu_ui_mgr_t *ui_mgr,
        wlan_emu_sta_mgr_t *sta_mgr, wlan_emu_test_case_config *config) :
        wlan_emu_tests_t(msg_mgr, ui_mgr, sta_mgr, config)
    {
        if (config->test_type == wlan_emu_test_3_subtype_pm_stats_get) {
            snprintf(config->test_case_name, sizeof(config->test_case_name), "%s", "CC3-Stats-Get");
        }
    }

    wlan_emu_tests_stats_get()
    {
        remove_msg_mgr();
    }

    ~wlan_emu_tests_stats_get();
};

class wlan_emu_tests_stats_set : public wlan_emu_tests_t {
public:
    wlan_emu_tests_stats_set(wlan_emu_msg_mgr_t *msg_mgr, wlan_emu_ui_mgr_t *ui_mgr,
        wlan_emu_sta_mgr_t *sta_mgr, wlan_emu_test_case_config *config) :
        wlan_emu_tests_t(msg_mgr, ui_mgr, sta_mgr, config)
    {
        if (config->test_type == wlan_emu_test_3_subtype_pm_stats_set) {
            snprintf(config->test_case_name, sizeof(config->test_case_name), "%s", "CC3-Stats-Set");
        }
    }

    wlan_emu_tests_stats_set()
    {
        remove_msg_mgr();
    }

    ~wlan_emu_tests_stats_set();
};

#endif // WLAN_EMU_TESTS_H
