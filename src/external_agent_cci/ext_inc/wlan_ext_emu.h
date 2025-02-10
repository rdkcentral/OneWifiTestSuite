#ifndef WLAN_EXT_EMU_H
#define WLAN_EXT_EMU_H

#include "cci_wifi_utils.hpp"
#include "collection.h"
#include "httplib.h"
#include "wifi_hal.h"
#include "wlan_emu_common.h"
#include "wlan_emu_ext_agent_interface.h"
#include "wlan_emu_log.h"
#include "wlan_ext_emu_msg_mgr.h"
#include "wlan_ext_emu_sta_mgr.h"

typedef char wlan_ext_emu_pollable_name_t[128];

typedef struct {
    wlan_ext_emu_pollable_name_t name;
    unsigned int fd;
} wlan_ext_emu_pollables_t;

typedef enum {
    wlan_ext_emu_test_sig_type_idle,
    wlan_ext_emu_test_sig_type_ready,
    wlan_ext_emu_test_sig_type_running,
    wlan_ext_emu_test_sig_type_complete,
    wlan_ext_emu_test_sig_type_fail,
    wlan_ext_emu_test_sig_type_max
} wlan_ext_emu_test_sig_type_t;

class wlan_ext_emu_msg_mgr_t;
class wlan_ext_emu_tests_t;
class wlan_ext_emu_sta_mgr_t;
class wlan_ext_test_step_params_t;

class wlan_ext_emu_t {
private:
    static unsigned int m_ext_emu_token;
    wlan_ext_emu_sta_mgr_t m_agent_sta_mgr;
    wlan_ext_emu_msg_mgr_t m_ext_msg_mgr;
    wlan_emu_ext_agent_interface_t m_ext_agent_interface;

    fd_set m_fd_set;
    unsigned int m_num_fds;
    pthread_t m_http_server_tid;
    wlan_ext_emu_pollables_t m_pollables[wlan_ext_emu_test_sig_type_max];
    std::string m_dir_path; // directory for all ext_agent cci conf,input,artefact location
    std::string ext_emu_test_results_dir_path; // test artifacts directory path

    queue_t *test_steps_q; // wlan_ext_test_step_params_t

    std::mutex agent_step_status_mutex;
    std::string agent_step_status_json_str;

    ext_agent_test_state_t m_state;
    std::mutex m_state_lock; // lock for m_state

    /*Endpoint operations*/
    // Here get means gateway will request for get
    // Here post means gateway will request for post
    // As ext_agent acts as server; so get is download and post is upload for gateway

    // get endpoints
    void agent_endpoint_capability(const httplib::Request &req, httplib::Response &res);
    void agent_endpoint_status(const httplib::Request &req, httplib::Response &res);
    void agent_endpoint_results(const httplib::Request &req, httplib::Response &res);
    // post endpoints
    void agent_endpoint_reffiles(const httplib::Request &req, httplib::Response &res);
    void agent_endpoint_testconfig(const httplib::Request &req, httplib::Response &res);
    void agent_endpoint_command(const httplib::Request &req, httplib::Response &res);

    /*state machine*/
    int ext_emu_state_machine();

    /*io operations for thread communication*/
    void ext_emu_io_prep();
    wlan_ext_emu_test_sig_type_t ext_emu_io_wait();
    void ext_emu_update_io_wait_to_state(wlan_ext_emu_test_sig_type_t val);
    void ext_emu_send_signal(wlan_ext_emu_test_sig_type_t sig);
    int send_agent_msg_start();
    int send_agent_msg_stop();
    int agent_start();
    int agent_stop();
    int send_agent_msg_buffer(void *buff_ptr);
    int handle_file_post_request(const httplib::Request &req, httplib::Response &res,
        std::string &out_file);
    void http_report_bad_request(const httplib::Request &req, httplib::Response &res);
    void http_report_status_ok(const httplib::Request &req, httplib::Response &res,
        std::string msg);
    void http_report_internal_svr_err(const httplib::Request &req, httplib::Response &res);

public:
    int init();

    inline void ext_emu_send_signal_idle(void)
    {
        ext_agent_test_state_t state = get_agent_test_state();
        if (state == ext_agent_test_state_idle) {
            return;
        }

        ext_emu_send_signal(wlan_ext_emu_test_sig_type_idle);
    }

    inline void ext_emu_send_signal_ready(void)
    {
        ext_agent_test_state_t state = get_agent_test_state();
        if (state == ext_agent_test_state_ready) {
            return;
        }
        ext_emu_send_signal(wlan_ext_emu_test_sig_type_ready);
    }

    inline void ext_emu_send_signal_running(void)
    {
        ext_agent_test_state_t state = get_agent_test_state();
        if (state == ext_agent_test_state_running) {
            return;
        }
        ext_emu_send_signal(wlan_ext_emu_test_sig_type_running);
    }

    inline void ext_emu_send_signal_complete(void)
    {
        ext_agent_test_state_t state = get_agent_test_state();
        if (state == ext_agent_test_state_complete) {
            return;
        }
        ext_emu_send_signal(wlan_ext_emu_test_sig_type_complete);
    }

    inline void ext_emu_send_signal_fail(void)
    {
        ext_agent_test_state_t state = get_agent_test_state();
        if (state == ext_agent_test_state_fail) {
            return;
        }
        ext_emu_send_signal(wlan_ext_emu_test_sig_type_fail);
    }

    static void *http_server_handler(void *arg);

    const std::string &get_ext_emu_test_results_dir_path()
    {
        return ext_emu_test_results_dir_path;
    }

    void set_ext_emu_test_results_dir_path(const std::string &path)
    {
        ext_emu_test_results_dir_path = path;
    }

    queue_t *get_test_steps_q()
    {
        return test_steps_q;
    }

    void create_q_for_test_steps_q()
    {
        test_steps_q = queue_create();
    }

    wlan_emu_ext_agent_interface_t *get_agent_interface()
    {
        return &m_ext_agent_interface;
    }

    std::string get_agent_step_status_json_str()
    {
        std::lock_guard<std::mutex> lock(agent_step_status_mutex);
        return agent_step_status_json_str;
    }

    void set_agent_step_status_json_str(const std::string &step_status_json_str)
    {
        std::lock_guard<std::mutex> lock(agent_step_status_mutex);
        agent_step_status_json_str = step_status_json_str;
    }

    inline ext_agent_test_state_t get_agent_test_state()
    {
        std::lock_guard<std::mutex> lock(agent_step_status_mutex);

        return m_state;
    }

    inline void set_agent_test_state(ext_agent_test_state_t state)
    {
        std::lock_guard<std::mutex> lock(agent_step_status_mutex);
        m_state = state;
    }
    int decode_step_param(std::string &cjson_str, wlan_ext_test_step_params_t **step_config);
    int decode_step_param_common(cJSON *root_json, wlan_ext_test_step_params_t *step_config);
    int decode_step_param_ext_sta_management(cJSON *root_json, wlan_ext_test_step_params_t *step_config);

    wlan_ext_emu_t();
    ~wlan_ext_emu_t();
};

#endif // WLAN_EXT_EMU_H
