/**
 * Copyright 2026 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef WLAN_UI_MGR_H
#define WLAN_UI_MGR_H
#include "cci_wifi_utils.hpp"
#include "wifi_util.h"
#include "wifi_webconfig.h"
#include "wlan_common_utils.h"
#include "wlan_emu_bus.h"
#include "wlan_emu_cjson.h"
#include "wlan_emu_common.h"
#include <rbus.h>
#include <sys/select.h>
#include <unistd.h>

typedef char wlan_emu_pollable_name_t[512];

typedef struct {
    void *acl_vap_context;
    queue_t *new_entry_queue[MAX_NUM_RADIOS][MAX_NUM_VAP_PER_RADIO];
} acl_data_t;

typedef struct {
    webconfig_t webconfig;
    wifi_global_config_t config;
    wifi_hal_capability_t hal_cap;
    rdk_wifi_radio_t radios[MAX_NUM_RADIOS];
    active_msmt_t blaster;
    hash_map_t *assoc_dev_hash_map[MAX_NUM_RADIOS][MAX_NUM_VAP_PER_RADIO];
    acl_data_t acl_data;
    bus_handle_t handle;
    instant_measurement_config_t harvester;
    queue_t *csi_data_queue;
} webconfig_cci_t;

typedef struct {
    wlan_emu_pollable_name_t name;
    unsigned int fd;
} wlan_emu_pollables_t;

class test_step_params_t;
class test_step_param_vap;
class test_step_param_radio;
class test_step_param_sta_management;
class test_step_param_dmlsubdoc;
class test_step_param_logredirect;
class test_step_param_dml_reset;
class test_step_param_get_stats_t;
class test_step_param_get_radio_channel_stats;
class test_step_param_get_neighbor_stats;
class test_step_param_get_assoc_clients_stats;
class test_step_param_get_radio_diag_stats;
class test_step_param_get_radio_temperature_stats;
class test_step_param_set_stats_t;
class test_step_param_set_radio_channel_stats;
class test_step_param_set_neighbor_stats;
class test_step_param_set_assoc_clients_stats;
class test_step_param_set_radio_diag_stats;
class test_step_param_set_radio_temperature_stats;
class test_step_param_get_file;
class test_step_param_mgmt_frame_capture;
class test_step_param_get_pattern_files;
class test_step_param_timed_wait;
class test_step_param_config_onewifi;

class wlan_emu_ui_ssl_config {
public:
    char ssl_cert[128];
    char ssl_key[64];
    inline char *get_ssl_cert()
    {
        return ssl_cert;
    }
    inline char *get_ssl_key()
    {
        return ssl_key;
    }

    void set_ssl_cert(const char *cert)
    {
        if (cert) {
            strncpy(ssl_cert, cert, sizeof(ssl_cert) - 1);
            ssl_cert[sizeof(ssl_cert) - 1] = '\0';
        } else {
            ssl_cert[0] = '\0';
        }
    }

    void set_ssl_key(const char *key)
    {
        if (key) {
            strncpy(ssl_key, key, sizeof(ssl_key) - 1);
            ssl_key[sizeof(ssl_key) - 1] = '\0';
        } else {
            ssl_key[0] = '\0';
        }
    }
    int get_mtls_configuration();
    wlan_emu_ui_ssl_config();
    ~wlan_emu_ui_ssl_config();
};

class wlan_emu_ui_mgr_t {
    static unsigned int m_token;
    char m_path[128]; // tmp directory
    fd_set m_set;
    unsigned int m_nfds;
    wlan_emu_pollables_t m_pollables[wlan_emu_sig_type_max];
    char input_test_buff[256];
    char tda_url[128];
    char cci_out_file_list[128];
    char test_config_file[128]; // Config file used for testing
    char interface[16];
    // Configuration present in Config file
    char version[8];
    int simulated_clients;
    char subdoc_name[32];
    char server_address[128];
    queue_t *test_cov_cases_q; // wlan_emu_test_case_config
    char test_results_dir_path[64];
    bool is_local_host_enabled;
    char remote_test_results_loc[256];
    webconfig_cci_t *m_webconfig_data;
    wifi_hal_capability_t *m_sta_hal_cap;
    pthread_cond_t m_heartbeat_cond;
    pthread_mutex_t m_heartbeat_lock;
    pthread_t m_heartbeat_tid;
    wlan_emu_bus_t *m_bus_mgr;
    long log_file_offset;
    std::string reboot_url_file_name;
    FILE *res_output_file_fp = NULL;
    char res_output_file[128] = { 0 };
    uint paused_logging_steps_count = 0;
    uint *paused_logging_steps = NULL; // step_number_entry_t
    uint last_executed_step_num = 0;
    bool reboot_test_executed = false;

private:
    int io_prep(void);
    int bus_init();
    int process_input_request(void);
    void send_signal(wlan_emu_sig_type_t sig);
    int decode_config_file(void);
    int decode_json_config(char *json_str);
    int decode_coverage_1_config(cJSON *conf);
    int decode_coverage_2_config(cJSON *conf);
    int decode_coverage_3_config(cJSON *conf);
    int decode_coverage_config(cJSON *config_entry, wlan_emu_test_coverage_t coverage_type,
        wlan_emu_test_type_t type, wlan_emu_test_case_config **config);
    void push_config_to_queue(wlan_emu_test_case_config *test);
    int download_test_files(void);
    int parse_test_config_json_parameters(cJSON *root_json);
    int get_file_name_from_url(char *url, char *file_name, int len);
    int decode_step_common_config(cJSON *entry, test_step_params_t *step_config);
    int decode_step_param_config(cJSON *entry, test_step_params_t **step_config);
    int decode_step_config(cJSON *config_entry, wlan_emu_test_case_config *configuration);
    int decode_step_radio_config(cJSON *step, test_step_params_t *step_config);
    int decode_step_vap_config(cJSON *step, test_step_params_t *step_config);
    int decode_step_packet_generator_config(cJSON *step, test_step_params_t *step_config);
    int decode_step_station_management_config(cJSON *step, test_step_params_t *step_config);
    int decode_step_time_config(cJSON *step, test_step_params_t *step_config);
    int decode_step_command_config(cJSON *step, test_step_params_t *step_config);
    int decode_step_log_redirect(cJSON *step, test_step_params_t *step_config);
    int decode_step_dmlsubdoc_config(cJSON *step, test_step_params_t *step_config);
    int decode_stats_get_common_params(cJSON *step, test_step_params_t *step_config);
    int decode_step_radio_channel_stats_get(cJSON *step, test_step_params_t *step_config);
    int decode_step_neighbor_stats_get(cJSON *step, test_step_params_t *step_config);
    int decode_step_assoc_client_stats_get(cJSON *step, test_step_params_t *step_config);
    int decode_step_radio_diag_stats_get(cJSON *step, test_step_params_t *step_config);
    int decode_step_radio_temperature_stats_get(cJSON *step, test_step_params_t *step_config);
    int decode_step_radio_channel_stats_set(cJSON *step, test_step_params_t *step_config);
    int decode_step_neighbor_stats_set(cJSON *step, test_step_params_t *step_config);
    int decode_step_assoc_client_stats_set(cJSON *step, test_step_params_t *step_config);
    int decode_step_radio_diag_stats_set(cJSON *step, test_step_params_t *step_config);
    int decode_step_radio_temperature_stats_set(cJSON *step, test_step_params_t *step_config);
    int decode_stats_set_common_params(cJSON *step, test_step_params_t *step_config);
    int download_file(char *input_file_name, unsigned int input_file_name_len);
    int download_step_param_config(test_step_params_t *step);
    int download_step_common_config(test_step_params_t *step);
    int cci_post_result_to_tda(unsigned int type, char *str);
    int decode_step_get_file(cJSON *step, test_step_params_t *step_config);
    int decode_step_mgmt_frame_capture(cJSON *step, test_step_params_t *step_config);
    int decode_step_get_pattern_files(cJSON *step, test_step_params_t *step_config);
    int decode_step_timed_wait(cJSON *step, test_step_params_t *step_config);
    int decode_step_config_onewifi(cJSON *step, test_step_params_t *step_config);
    int decode_step_gateway_performance(cJSON *step, test_step_params_t *step_config);
    int decode_step_configure_upgrade_or_reboot(cJSON *step, test_step_params_t *step_config);

public:
    int init(void);
    wlan_emu_sig_type_t io_wait(void);
    int analyze_request(void);
    unsigned int upload_results(void);
    int upload_cci_log(char *test_case_id, char *test_case_name, FILE *fp);
    int decode_reboot_case_json();
    int cci_error_code;

    inline void add_bus_mgr(wlan_emu_bus_t *bus_mgr)
    {
        m_bus_mgr = bus_mgr;
    }

    inline void signal_received_request(void)
    {
        send_signal(wlan_emu_sig_type_input);
    }

    inline void signal_downloaded_test_data(void)
    {
        send_signal(wlan_emu_sig_type_analysis);
    }

    void signal_test_done(wlan_emu_test_coverage_t coverage);

    inline void signal_uploaded_test_results(void)
    {
        send_signal(wlan_emu_sig_type_results);
    }

    inline void signal_report_test_fail(void)
    {
        send_signal(wlan_emu_sig_type_fail);
    }

    inline void signal_input(void)
    {
        send_signal(wlan_emu_sig_type_input);
    }

    inline char *get_input_url(void)
    {
        return input_test_buff;
    }

    inline void set_reboot_test_executed(bool value)
    {
        reboot_test_executed = value;
    }

    inline bool get_reboot_test_executed(void)
    {
        return reboot_test_executed;
    }

    inline int alloc_paused_logging_steps(int count)
    {
        if (paused_logging_steps != NULL) {
            free(paused_logging_steps);
            paused_logging_steps = NULL;
        }
        paused_logging_steps = (uint *)malloc(count * sizeof(uint));
        if (paused_logging_steps == NULL) {
            return RETURN_ERR;
        }
        memset(paused_logging_steps, 0, count * sizeof(uint));
        return RETURN_OK;
    }

    inline uint *get_paused_logging_steps()
    {
        return paused_logging_steps;
    }

    inline bool get_local_host_enabled()
    {
        return is_local_host_enabled;
    }

    int read_config_file(char *file_path, char **data); // can be moved it to utils classes
    int check_reboot_step_config( cJSON *step);
    int cci_report_failure_to_tda(void);
    int cci_report_complete_to_tda(void);
    int cci_report_reboot_to_tda(void);
    int cci_report_heartbeat_to_tda(void);
    void update_log_file_offset(void);
    int cci_report_factory_reset_to_tda(void);
    int cci_report_pause_to_tda(void);
    int cci_report_resume_to_tda(void);

    long get_log_file_offset()
    {
        return log_file_offset;
    }

    void set_log_file_offset(long new_offset)
    {
        log_file_offset = new_offset;
    }

    int update_tda_url_to_file(void);

    const std::string &get_reboot_url_file_name()
    {
        return reboot_url_file_name;
    }

    void set_reboot_url_file_name(const std::string &path)
    {
        reboot_url_file_name = path;
    }

    inline queue_t *get_test_cov_cases_queue(void)
    {
        return test_cov_cases_q;
    }

    inline bus_handle_t get_bus_handle(void)
    {
        return m_webconfig_data->handle;
    }

    int decode_pcap_frame_type(char *frame_type_str, frame_capture_request_t *frame_capture_req);
    int bus_send(char *data, wlan_emu_bus_t *bus_mgr);

    inline webconfig_cci_t *get_webconfig_data()
    {
        return m_webconfig_data;
    }

    inline void update_webconfig_data(webconfig_cci_t *cci_webconfig)
    {
        m_webconfig_data = cci_webconfig;
    }

    inline pthread_cond_t* get_heartbeat_cond_var() {
        return &m_heartbeat_cond;
    }
    static void *heartbeat_function(void *arg);

    static void set_webconfig_cci_data(char *event_name, raw_data_t *data, void *userData);
    void (*func_ptr)(char *event_name, raw_data_t *data);
    void cci_cache_update(webconfig_subdoc_data_t *data);
    void mac_filter_cci_cache_update(webconfig_subdoc_data_t *data);
    void update_cci_subdoc_vap_data(webconfig_subdoc_data_t *data);
    webconfig_error_t cci_webconfig_data_free(webconfig_subdoc_data_t *data);
    void mac_filter_cci_vap_cache_update(int radio_index, int vap_array_index);
    hash_map_t **get_cci_acl_hash_map(unsigned int radio_index, unsigned int vap_index);
    static webconfig_error_t webconfig_cci_apply(struct webconfig_subdoc *doc,
        webconfig_subdoc_data_t *data);
    queue_t **get_acl_new_entry_queue(wifi_vap_info_t *vap_info);
    queue_t **get_cci_acl_new_entry_queue(unsigned int radio_index, unsigned int vap_index);
    wifi_vap_info_t *get_cci_vap_info(char *vap_name);
    webconfig_error_t update_vap_security_object(cJSON *security,
        wifi_vap_security_t *security_info, int band);
    webconfig_error_t update_vap_common_object(cJSON *vap, wifi_vap_info_t *vap_info);
    int update_vap_param_integer(cJSON *json, const char *key, cJSON **value);
    int update_vap_param_string(cJSON *json, const char *key, cJSON **value);
    int update_vap_param_bool(cJSON *json, const char *key, cJSON **value);
    int get_radioindex_from_bssid(mac_address_t ap_bssid, unsigned int *radio_index);
    wifi_radio_operationParam_t *cci_get_radio_operation_param(unsigned int radio_index);
    void dump_json(cJSON *json_buff, const char *func, int line);

    char *get_remote_test_results_loc()
    {
        return remote_test_results_loc;
    }

    int upload_file_to_server(char *file_name, char *path);
    int step_upload_files(char *output_file);
    int wlan_emu_get_station_capability(wifi_hal_capability_t *sta_hal_cap);

    inline void update_station_capability(wifi_hal_capability_t *sta_hal_cap)
    {
        m_sta_hal_cap = sta_hal_cap;
    }

    test_step_params_t *get_step_from_step_number(wlan_emu_test_case_config *test_case_config,
        int step_number);

    inline char *get_test_results_dir_path()
    {
        return test_results_dir_path;
    }

    int decode_step_dml_reset_config(cJSON *step, test_step_params_t *step_config);

    inline char *get_tda_url()
    {
        return tda_url;
    }

    inline char *get_server_address()
    {
        return server_address;
    }

    inline char *get_tda_interface()
    {
        return interface;
    }

    inline char *get_tda_output_file()
    {
        return cci_out_file_list;
    }

    inline int get_simulated_client_count()
    {
        return simulated_clients;
    }

    inline void set_simulated_client_count(int count)
    {
        simulated_clients = count;
    }

    void send_webconfig_ctrl_msg(webconfig_subdoc_type_t subdoc_type);

    inline int get_cci_error_code()
    {
        return cci_error_code;
    }

    inline void set_cci_error_code(int error_code)
    {
        cci_error_code = error_code;
        return;
    }

    inline pthread_t get_heartbeat_thread_id()
    {
        return m_heartbeat_tid;
    }

    http_info_t *fill_http_info();
    wlan_emu_ui_ssl_config m_ssl_config;

    wlan_emu_ui_mgr_t();
    ~wlan_emu_ui_mgr_t();
};

class wlan_emu_standalone_ui_mgr_t : public wlan_emu_ui_mgr_t {

public:
    int init();
    wlan_emu_sig_type_t io_wait();
    unsigned int analyze_request();
    unsigned int upload_results();

    wlan_emu_standalone_ui_mgr_t();
    ~wlan_emu_standalone_ui_mgr_t();
};

#endif // WLAN_UI_MGR_H
