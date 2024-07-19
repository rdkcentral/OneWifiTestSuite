#ifndef WLAN_UI_MGR_H
#define WLAN_UI_MGR_H
#include <sys/select.h>
#include <unistd.h>
#include <rbus.h>
#include "wlan_emu_cjson.h"
#include "wifi_webconfig.h"
#include "wlan_emu_common.h"
#include "wifi_util.h"

typedef char wlan_emu_pollable_name_t[512];

typedef struct {
    void    *acl_vap_context;
    queue_t* new_entry_queue[MAX_NUM_RADIOS][MAX_NUM_VAP_PER_RADIO];
} acl_data_t;

typedef struct {
    webconfig_t     webconfig;
    wifi_global_config_t    config;
    wifi_hal_capability_t   hal_cap;
    rdk_wifi_radio_t    radios[MAX_NUM_RADIOS];
    active_msmt_t blaster;
    hash_map_t    *assoc_dev_hash_map[MAX_NUM_RADIOS][MAX_NUM_VAP_PER_RADIO];
    acl_data_t acl_data;
    rbusHandle_t    rbus_handle;
    instant_measurement_config_t harvester;
    queue_t    *csi_data_queue;
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

class wlan_emu_ui_mgr_t {
    static unsigned int m_token;
    char m_path[128]; //tmp directory
    fd_set m_set;
    unsigned int m_nfds;
    wlan_emu_pollables_t m_pollables[wlan_emu_sig_type_max];
    char input_test_buff[256];
    char tda_url[128];
    char cci_out_file_list[64];
    char test_config_file[128]; //Config file used for testing
    char interface[16];
    //Configuration present in Config file
    char version[8];
    int simulated_clients;
    char subdoc_name[32];
    char server_address[128];
    queue_t *test_cov_cases_q; //wlan_emu_test_case_config
    char test_results_dir_path[64];
    bool is_local_host_enabled;
    char remote_test_results_loc[128];
    webconfig_cci_t *m_webconfig_data;
    wifi_hal_capability_t *m_sta_hal_cap;
    char ssl_cert[128];
    char ssl_key[64];


  private:
    int io_prep(void);
    int rbus_init(void);
    int process_input_request(void);
    void send_signal(wlan_emu_sig_type_t sig);
    int http_get(const char *get_url, const char *output_file);
    int http_post(const char *post_url, const char *input_file);
    int decode_config_file(void);
    int decode_json_config(char *json_str);
    int decode_coverage_1_config(cJSON *conf);
    int decode_coverage_3_config(cJSON *conf);
    int decode_coverage_config(cJSON *config_entry, wlan_emu_test_coverage_t coverage_type, wlan_emu_test_type_t type, wlan_emu_test_case_config **config);
    void push_config_to_queue(wlan_emu_test_case_config *test);
    int download_test_files(void);
    int parse_test_config_json_parameters(cJSON *root_json);
    void copy_string(char* destination, char* source, int len);
    int get_file_name_from_url(char *url, char *file_name, int len);
    int decode_step_common_config(cJSON *entry, test_step_params_t *step_config);
    int decode_step_param_config(cJSON *entry, test_step_params_t **step_config);
    int decode_step_config(cJSON *config_entry, wlan_emu_test_case_config *configuration);
    int decode_step_radio_config(cJSON *step, test_step_params_t *step_config);
    int decode_step_vap_config(cJSON *step, test_step_params_t *step_config);
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
    int cci_post_result_to_tda(bool result);
    int get_mlts_configuration();
    int decode_step_get_file(cJSON *step, test_step_params_t *step_config);

  public:
    int init(void);
    wlan_emu_sig_type_t io_wait(void);
    int analyze_request(void);
    unsigned int upload_results(void);

    inline void signal_received_request(void) { send_signal(wlan_emu_sig_type_input); }
    inline void signal_downloaded_test_data(void) { send_signal(wlan_emu_sig_type_analysis); }
    void signal_test_done(wlan_emu_test_coverage_t coverage);
    inline void signal_uploaded_test_results(void) { send_signal(wlan_emu_sig_type_results); }
    inline void signal_report_test_fail(void) { send_signal(wlan_emu_sig_type_fail); }
    inline void signal_input(void) { send_signal(wlan_emu_sig_type_input); }
    inline char *get_input_url(void) { return input_test_buff; }
    int read_config_file(char *file_path, char **data); //can be moved it to utils classes

    int cci_report_failure_to_tda(void);
    int cci_report_complete_to_tda(void);
    inline queue_t *get_test_cov_cases_queue(void) { return test_cov_cases_q; }
    inline rbusHandle_t get_rbus_handle(void) { return  m_webconfig_data->rbus_handle; }
    int decode_pcap_frame_type(char *frame_type_str, frame_capture_request_t *frame_capture_req);
    int rbus_send(char *data);
    inline webconfig_cci_t *get_webconfig_data() { return m_webconfig_data; }
    inline void update_webconfig_data(webconfig_cci_t *cci_webconfig) { m_webconfig_data = cci_webconfig; }
    static void set_webconfig_cci_data(rbusHandle_t handle, const rbusEvent_t * event, rbusEventSubscription_t* subscription);
    void (*func_ptr)(rbusHandle_t handle, const rbusEvent_t * event, rbusEventSubscription_t* subscription);
    void cci_cache_update(webconfig_subdoc_data_t *data);
    void mac_filter_cci_cache_update(webconfig_subdoc_data_t *data);
    void update_cci_subdoc_vap_data(webconfig_subdoc_data_t *data);
    webconfig_error_t  cci_webconfig_data_free(webconfig_subdoc_data_t *data);
    void mac_filter_cci_vap_cache_update(int radio_index, int vap_array_index);
    hash_map_t** get_cci_acl_hash_map(unsigned int radio_index, unsigned int vap_index);
    static webconfig_error_t webconfig_cci_apply(struct webconfig_subdoc *doc, webconfig_subdoc_data_t *data);
    queue_t** get_acl_new_entry_queue(wifi_vap_info_t *vap_info);
    queue_t** get_cci_acl_new_entry_queue(unsigned int radio_index, unsigned int vap_index);
    wifi_vap_info_t *get_cci_vap_info(char *vap_name);
    webconfig_error_t update_vap_security_object(cJSON *security, wifi_vap_security_t *security_info, int band);
    webconfig_error_t update_vap_common_object(cJSON *vap, wifi_vap_info_t *vap_info);
    int update_vap_param_integer(cJSON *json, const char *key,  cJSON  **value);
    int update_vap_param_string(cJSON *json, const char *key, cJSON  **value);
    int update_vap_param_bool(cJSON *json, const char *key,  cJSON  **value);
    int get_radioindex_from_bssid(mac_address_t ap_bssid, unsigned int *radio_index);
    wifi_radio_operationParam_t *cci_get_radio_operation_param(unsigned int radio_index);
    void dump_json(cJSON *json_buff, const char *func, int line);
    char *get_remote_test_results_loc() { return remote_test_results_loc; }
    int upload_file_to_server(char *file_name, char *path);
    int get_last_substring_after_slash(const char *str, char *sub_string, int sub_str_len);
    int wlan_emu_get_station_capability(wifi_hal_capability_t *sta_hal_cap);
    inline void update_station_capability(wifi_hal_capability_t *sta_hal_cap) { m_sta_hal_cap = sta_hal_cap; }
    test_step_params_t *get_step_from_step_number(wlan_emu_test_case_config *test_case_config, int step_number);
    inline char *get_test_results_dir_path() { return test_results_dir_path; }
    int decode_step_dml_reset_config(cJSON *step, test_step_params_t *step_config);

    inline char *get_tda_url() { return tda_url; }
    inline char *get_tda_interface() { return interface; }
    inline char *get_tda_output_file() { return cci_out_file_list; }
    inline int  *get_simulated_client_count() { return &simulated_clients; }
    void send_webconfig_ctrl_msg(webconfig_subdoc_type_t subdoc_type);
    int copy_file(const char *source_path, const char *destination_path);

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
