#ifndef WLAN_EMU_TEST_PARAMS_H
#define WLAN_EMU_TEST_PARAMS_H
#include "cci_wifi_utils.hpp"
#include "wlan_emu_common.h"
#include "wlan_emu_msg.h"
#include "wlan_emu_ui_mgr.h"
#include "wlan_emu_bus.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

class wlan_emu_msg_mgr_t;
class wlan_emu_ui_mgr_t;
class wlan_emu_sim_sta_mgr_t;
class wlan_emu_ext_sta_mgr_t;
class wlan_emu_msg_t;

class test_step_params_t {
public:
    wlan_emu_msg_mgr_t *m_msg_mgr;
    wlan_emu_ui_mgr_t *m_ui_mgr;
    wlan_emu_sim_sta_mgr_t *m_sim_sta_mgr;
    wlan_emu_ext_sta_mgr_t *m_ext_sta_mgr;
    wlan_emu_test_case_config *m_step_parent_test_config;
    wlan_emu_bus_t *m_bus_mgr;
    step_param_type_t param_type;
    /* Common Start*/
    char test_case_name[128];
    char test_case_id[8];
    unsigned int step_number; // User input
    unsigned int step_seq_num; // step Sequence number
    bool is_step_initialized; // To check errors in constructor

    bool capture_frames; // TestCapture
    queue_t *test_results_queue; // is output wlan_emu_pcap_captures
    queue_t *test_reference_queue; // is of type  wlan_emu_pcap_captures
    frame_capture_request_t frame_request;
    wlan_emu_tests_state_t test_state;
    bool fork; // true will be running in parallel else in serial
    pthread_mutex_t s_lock; // if running in parallel
    int execution_time;
    int timeout_count;

    /* Common End*/
    union {
        // Based on type : wlan_emu_test_param_type.
        // test_json will be applicable for  test_param_type_radio, test_param_type_vap,
        char test_webconfig_json[128]; // location
        char test_onewifi_subdoc[128];

        sta_test_t *sta_test;
        timed_wait_t *timed_wait;
        log_redirect_t *log_capture;
        command_t *cmd;
        wifi_stats_get_t *wifi_stats_get;
        wifi_stats_set_t *wifi_stats_set;
        get_file_t *get_file;
        mgmt_frame_capture_t *mgmt_frame_capture;
        get_pattern_files_t *get_pattern_files;
    } u;

    virtual int step_execute() = 0;
    virtual int step_timeout() = 0;
    virtual int step_upload_files(FILE *output_file, bool *update_to_tda) = 0;
    virtual void step_remove() = 0;
    virtual int step_frame_filter(wlan_emu_msg_t *msg) = 0;
    int bus_send(char *data, wlan_emu_bus_t *bus_mgr);

    inline void param_add_bus_mgr(wlan_emu_bus_t *mgr)
    {
        m_bus_mgr = mgr;
    }

    inline void param_add_msg_mgr(wlan_emu_msg_mgr_t *mgr)
    {
        m_msg_mgr = mgr;
    }

    inline void param_add_ui_mgr(wlan_emu_ui_mgr_t *mgr)
    {
        m_ui_mgr = mgr;
    }

    inline void param_add_sta_mgr(wlan_emu_sim_sta_mgr_t *mgr)
    {
        m_sim_sta_mgr = mgr;
    }

    inline void param_add_ext_sta_mgr(wlan_emu_ext_sta_mgr_t *mgr)
    {
        m_ext_sta_mgr = mgr;
    }

    inline void param_add_test_config(wlan_emu_test_case_config *mgr)
    {
        m_step_parent_test_config = mgr;
    }

    inline wlan_emu_sim_sta_mgr_t *param_get_sta_mgr()
    {
        return m_sim_sta_mgr;
    }

    inline wlan_emu_test_case_config *param_get_test_case_config()
    {
        return m_step_parent_test_config;
    }

    inline void step_add_mgr_data(wlan_emu_msg_mgr_t *msg_mgr, wlan_emu_ui_mgr_t *ui_mgr,
        wlan_emu_sim_sta_mgr_t *sta_mgr, wlan_emu_ext_sta_mgr_t *ext_sta_mgr,
        wlan_emu_test_case_config *test_config, wlan_emu_bus_t *bus_mgr)
    {
        param_add_msg_mgr(msg_mgr);
        param_add_ui_mgr(ui_mgr);
        param_add_sta_mgr(sta_mgr);
        param_add_ext_sta_mgr(ext_sta_mgr);
        param_add_test_config(test_config);
        param_add_bus_mgr(bus_mgr);
    }
};

class test_step_param_vap : public test_step_params_t {
public:
    int step_execute();
    int step_timeout();
    int step_upload_files(FILE *output_file, bool *update_to_tda);
    void step_remove();
    int step_frame_filter(wlan_emu_msg_t *msg);
    test_step_param_vap();
    ~test_step_param_vap();
};

class test_step_param_radio : public test_step_params_t {
public:
    int step_execute();
    int step_timeout();
    int step_upload_files(FILE *output_file, bool *update_to_tda);
    void step_remove();
    int step_frame_filter(wlan_emu_msg_t *msg);
    test_step_param_radio();
    ~test_step_param_radio();
};

class test_step_param_sta : public test_step_params_t {
public:
    virtual int step_execute() = 0;
    virtual int step_timeout() = 0;
    virtual int step_upload_files(FILE *output_file, bool *update_to_tda) = 0;
    virtual int step_frame_filter(wlan_emu_msg_t *msg) = 0;
    virtual void step_remove() = 0;
};

class test_step_param_sta_management : public test_step_param_sta {
private:

    int step_timeout_ext_sta();
    int push_ext_sta_result_files(const std::vector<std::string> &files);

public:
    int decode_user_ap_config(cJSON *sta_root_json, wifi_vap_info_t *ap_vap_info);
    int update_sta_config(wifi_vap_info_t *ap_vap_config);
    int decode_step_sta_management_config();
    int step_execute();
    int step_timeout();
    int step_upload_files(FILE *output_file, bool *update_to_tda);
    void step_remove();
    int step_frame_filter(wlan_emu_msg_t *msg);
    int encode_external_sta_management_subdoc(std::string &cli_subdoc);
    test_step_param_sta_management();
    ~test_step_param_sta_management();
};

class test_step_param_command : public test_step_params_t {
public:
    int step_execute();
    int step_timeout();
    int step_upload_files(FILE *output_file, bool *update_to_tda);
    void step_remove();
    int step_frame_filter(wlan_emu_msg_t *msg);
    test_step_param_command();
    ~test_step_param_command();
};

class test_step_param_dmlsubdoc : public test_step_params_t {
public:
    int step_execute();
    int step_timeout();
    int step_upload_files(FILE *output_file, bool *update_to_tda);
    void step_remove();
    int step_frame_filter(wlan_emu_msg_t *msg);
    test_step_param_dmlsubdoc();
    ~test_step_param_dmlsubdoc();
};

class test_step_param_logredirect : public test_step_params_t {
public:
    int step_execute();
    int step_timeout();
    int step_upload_files(FILE *output_file, bool *update_to_tda);
    void step_remove();
    int step_frame_filter(wlan_emu_msg_t *msg);
    test_step_param_logredirect();
    ~test_step_param_logredirect();
    static void *log_thread_function(void *arg);
};

class test_step_param_dml_reset : public test_step_params_t {
public:
    int step_execute();
    int step_timeout();
    int step_upload_files(FILE *output_file, bool *update_to_tda);
    void step_remove();
    int step_frame_filter(wlan_emu_msg_t *msg);
    test_step_param_dml_reset();
    ~test_step_param_dml_reset();
};

class test_step_param_get_stats_t : public test_step_params_t {
public:
    int step_execute();
    int step_timeout();
    int step_upload_files(FILE *output_file, bool *update_to_tda);
    void step_remove();
    int start_subscription();
    int stop_subscription(test_step_params_t *step);
    char *getStatsClass();
    char *get_scanmode();
    char *get_scanmode_str();
    int update_output_file_name();
    int get_subscription_string(char *str, int str_len);
    static void stats_get_event_handler(char *event_name, raw_data_t *data, void *userData);
    int step_frame_filter(wlan_emu_msg_t *msg);
    char *get_stats_response_type();
};

class test_step_param_get_radio_channel_stats : public test_step_param_get_stats_t {
public:
    test_step_param_get_radio_channel_stats();
    ~test_step_param_get_radio_channel_stats();
};

class test_step_param_get_neighbor_stats : public test_step_param_get_stats_t {
public:
    test_step_param_get_neighbor_stats();
    ~test_step_param_get_neighbor_stats();
};

class test_step_param_get_assoc_clients_stats : public test_step_param_get_stats_t {
public:
    test_step_param_get_assoc_clients_stats();
    ~test_step_param_get_assoc_clients_stats();
};

class test_step_param_get_radio_diag_stats : public test_step_param_get_stats_t {
public:
    test_step_param_get_radio_diag_stats();
    ~test_step_param_get_radio_diag_stats();
};

class test_step_param_get_radio_temperature_stats : public test_step_param_get_stats_t {
public:
    test_step_param_get_radio_temperature_stats();
    ~test_step_param_get_radio_temperature_stats();
};

class test_step_param_set_stats_t : public test_step_params_t {
public:
    int step_execute();
    int step_timeout();
    int step_upload_files(FILE *output_file, bool *update_to_tda);
    void step_remove();
    int webconfig_stats_set_instance();
    int step_frame_filter(wlan_emu_msg_t *msg);
    virtual int webconfig_stats_set_execute_stop() = 0;
    virtual int webconfig_stats_set_execute_start() = 0;
};

class test_step_param_set_radio_channel_stats : public test_step_param_set_stats_t {
public:
    int webconfig_stats_set_execute_stop() override;
    int webconfig_stats_set_execute_start() override;
    test_step_param_set_radio_channel_stats();
    ~test_step_param_set_radio_channel_stats();
};

class test_step_param_set_neighbor_stats : public test_step_param_set_stats_t {
public:
    int webconfig_stats_set_execute_stop() override;
    int webconfig_stats_set_execute_start() override;
    test_step_param_set_neighbor_stats();
    ~test_step_param_set_neighbor_stats();
};

class test_step_param_set_assoc_clients_stats : public test_step_param_set_stats_t {
public:
    int webconfig_stats_set_execute_stop() override;
    int webconfig_stats_set_execute_start() override;
    test_step_param_set_assoc_clients_stats();
    ~test_step_param_set_assoc_clients_stats();
};

class test_step_param_set_radio_diag_stats : public test_step_param_set_stats_t {
public:
    int webconfig_stats_set_execute_stop() override;
    int webconfig_stats_set_execute_start() override;
    test_step_param_set_radio_diag_stats();
    ~test_step_param_set_radio_diag_stats();
};

class test_step_param_set_radio_temperature_stats : public test_step_param_set_stats_t {
public:
    int webconfig_stats_set_execute_stop() override;
    int webconfig_stats_set_execute_start() override;
    test_step_param_set_radio_temperature_stats();
    ~test_step_param_set_radio_temperature_stats();
};

class test_step_param_get_file : public test_step_params_t {
public:
    int step_execute();
    int step_timeout();
    int step_upload_files(FILE *output_file, bool *update_to_tda);
    void step_remove();
    int step_frame_filter(wlan_emu_msg_t *msg);
    test_step_param_get_file();
    ~test_step_param_get_file();
};

class test_step_param_mgmt_frame_capture : public test_step_params_t {
public:
    int step_execute();
    int step_timeout();
    int step_upload_files(FILE *output_file, bool *update_to_tda);
    void step_remove();
    int step_frame_filter(wlan_emu_msg_t *msg);
    test_step_param_mgmt_frame_capture();
    ~test_step_param_mgmt_frame_capture();
};

class test_step_param_get_pattern_files : public test_step_params_t {
public:
    int step_execute();
    int step_timeout();
    int step_upload_files(FILE *output_file, bool *update_to_tda);
    void step_remove();
    int step_frame_filter(wlan_emu_msg_t *msg);
    test_step_param_get_pattern_files();
    ~test_step_param_get_pattern_files();
};

class test_step_param_timed_wait : public test_step_params_t {
public:
    int step_execute();
    int step_timeout();
    int step_upload_files(FILE *output_file, bool *update_to_tda);
    void step_remove();
    int step_frame_filter(wlan_emu_msg_t *msg);
    test_step_param_timed_wait();
    ~test_step_param_timed_wait();
};

class test_step_param_config_onewifi : public test_step_params_t {
public:
    int step_execute();
    int step_timeout();
    int step_upload_files(FILE *output_file, bool *update_to_tda);
    int step_frame_filter(wlan_emu_msg_t *msg);
    void step_remove();
    test_step_param_config_onewifi();
    ~test_step_param_config_onewifi();
};
#endif
