/**
 * Copyright 2025 Comcast Cable Communications Management, LLC
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

#ifndef WLAN_EMU_COMMON_H
#define WLAN_EMU_COMMON_H

#include "collection.h"
#include "wifi_base.h"
#include "wifi_webconfig.h"
#include "wlan_emu_common.h"
#include <new> // For std::nothrow
#include <pthread.h>
#include <vector>
#include <string>
#include <cstdio>
#include <variant>
#include <unordered_map>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_EMU_DEVICES 256
#define UINT_BITS_SZ 32
#define DEV_STATUS_ARR_SZ MAX_EMU_DEVICES / UINT_BITS_SZ

#define HEART_BEAT_TIME 5
#define RETURN_OK 0
#define RETURN_ERR -1
#define RETURN_UNHANDLED RETURN_ERR
#define RETURN_HANDLED RETURN_OK
#define CURRENT_CONFIGURATION "PRESENT"
#define ARRAY_SIZE(x) ((unsigned int)(sizeof(x) / sizeof(x[0])))

typedef enum {
    wlan_emu_msg_type_none,
    wlan_emu_msg_type_emu80211,
    wlan_emu_msg_type_cfg80211,
    wlan_emu_msg_type_mac80211,
    wlan_emu_msg_type_frm80211,
    wlan_emu_msg_type_webconfig,
    wlan_emu_msg_type_agent
} wlan_emu_msg_type_t;

typedef enum {
    wlan_emu_frm80211_ops_type_prb_resp,
    wlan_emu_frm80211_ops_type_prb_req,
    wlan_emu_frm80211_ops_type_assoc_resp,
    wlan_emu_frm80211_ops_type_assoc_req,
    wlan_emu_frm80211_ops_type_auth,
    wlan_emu_frm80211_ops_type_deauth,
    wlan_emu_frm80211_ops_type_disassoc,
    wlan_emu_frm80211_ops_type_eapol,
    wlan_emu_frm80211_ops_type_reassoc_req,
    wlan_emu_frm80211_ops_type_reassoc_resp, // Not implemented in hal
    wlan_emu_frm80211_ops_type_action
} wlan_emu_frm80211_ops_type_t;

typedef enum {
    wlan_emu_cfg80211_ops_type_none,
    wlan_emu_cfg80211_ops_type_add_intf,
    wlan_emu_cfg80211_ops_type_del_intf,
    wlan_emu_cfg80211_ops_type_change_intf,
    wlan_emu_cfg80211_ops_type_start_ap,
    wlan_emu_cfg80211_ops_type_change_beacon,
    wlan_emu_cfg80211_ops_type_stop_ap,
    wlan_emu_cfg80211_ops_type_set_wiphy,
    wlan_emu_cfg80211_ops_type_unknown
} wlan_emu_cfg80211_ops_type_t;

typedef enum {
    wlan_emu_test_1_subtype_radio,
    wlan_emu_test_1_subtype_ns_private,
    wlan_emu_test_1_subtype_ns_public_xfinity_open,
    wlan_emu_test_1_subtype_ns_public_xfinity_secure,
    wlan_emu_test_1_subtype_ns_managed_xhs,
    wlan_emu_test_1_subtype_ns_managed_lnf_enterprise,
    wlan_emu_test_1_subtype_ns_managed_lnf_secure,
    wlan_emu_test_1_subtype_ns_managed_mesh_backhaul,
    wlan_emu_test_1_subtype_ns_managed_mesh_client,
    wlan_emu_test_1_subtype_cc_probe_response,
    wlan_emu_test_1_subtype_cc_authentication,
    wlan_emu_test_2_subtype_af_connection_admission,
    wlan_emu_test_2_subtype_af_access_control,
    wlan_emu_test_2_subtype_af_stats_manager,
    wlan_emu_test_2_subtype_af_steering_manager,
    wlan_emu_test_2_subtype_af_optimization,
    wlan_emu_test_2_subtype_af_grey_listing,
    wlan_emu_test_2_subtype_af_active_passive_msrmnts,
    wlan_emu_test_2_subtype_af_whix,
    wlan_emu_test_2_subtype_af_blaster,
    wlan_emu_test_2_subtype_af_motion,
    wlan_emu_test_2_subtype_af_finger_printing,
    wlan_emu_test_2_subtype_af_tr_181,
    wlan_emu_test_2_subtype_af_webconfig,
    wlan_emu_test_2_subtype_af_webpa,
    wlan_emu_test_3_subtype_pm_stats_get,
    wlan_emu_test_3_subtype_pm_stats_set,
    wlan_emu_test_1_subtype_max
} wlan_emu_test_type_t;

typedef enum {
    wlan_emu_test_coverage_1 = 1,
    wlan_emu_test_coverage_2,
    wlan_emu_test_coverage_3 = 3,
    wlan_emu_test_coverage_4,
    wlan_emu_test_coverage_5,
    wlan_emu_test_coverage_max,
} wlan_emu_test_coverage_t;

typedef enum {
    wlan_emu_sig_type_input,
    wlan_emu_sig_type_analysis,
    wlan_emu_sig_type_coverage_1,
    wlan_emu_sig_type_coverage_2,
    wlan_emu_sig_type_coverage_3,
    wlan_emu_sig_type_coverage_4,
    wlan_emu_sig_type_coverage_5,
    wlan_emu_sig_type_results,
    wlan_emu_sig_type_fail,
    wlan_emu_sig_type_max,
} wlan_emu_sig_type_t;

typedef enum {
    wlan_emu_tests_state_cmd_none,
    wlan_emu_tests_state_cmd_wait,
    wlan_emu_tests_state_cmd_request,
    wlan_emu_tests_state_cmd_start = 0x000000A0,
    wlan_emu_tests_state_cmd_results = 0x000000B0,
    wlan_emu_tests_state_cmd_continue = 0x000000C0,
    wlan_emu_tests_state_cmd_abort = 0x0000F000
} wlan_emu_tests_state_t;

//  Enum to String mapping
static inline const std::unordered_map<wlan_emu_tests_state_t, std::string> step_state_to_str = {
    { wlan_emu_tests_state_cmd_none,     "none"     },
    { wlan_emu_tests_state_cmd_wait,     "wait"     },
    { wlan_emu_tests_state_cmd_request,  "request"  },
    { wlan_emu_tests_state_cmd_start,    "start"    },
    { wlan_emu_tests_state_cmd_results,  "results"  },
    { wlan_emu_tests_state_cmd_continue, "continue" },
    { wlan_emu_tests_state_cmd_abort,    "abort"    }
};

// String to Enum mapping
static inline const std::unordered_map<std::string, wlan_emu_tests_state_t> str_to_step_state = {
    { "none",     wlan_emu_tests_state_cmd_none     },
    { "wait",     wlan_emu_tests_state_cmd_wait     },
    { "request",  wlan_emu_tests_state_cmd_request  },
    { "start",    wlan_emu_tests_state_cmd_start    },
    { "results",  wlan_emu_tests_state_cmd_results  },
    { "continue", wlan_emu_tests_state_cmd_continue },
    { "abort",    wlan_emu_tests_state_cmd_abort    }
};

// Convert Enum to String
static std::string step_state_as_string(wlan_emu_tests_state_t state)
{
    auto it = step_state_to_str.find(state);
    return (it != step_state_to_str.end()) ? it->second : "Unknown";
}

// Convert String to Enum
static wlan_emu_tests_state_t step_state_as_enum(const std::string &str)
{
    auto it = str_to_step_state.find(str);
    return (it != str_to_step_state.end()) ? it->second : wlan_emu_tests_state_cmd_none;
}


typedef enum {
    wlan_emu_dml_tests_state_idle,
    wlan_emu_dml_tests_state_running,
    wlan_emu_dml_tests_state_complete_success,
    wlan_emu_dml_tests_state_complete_failure,
} wlan_emu_dml_tests_state_t;

typedef enum {
    client_connection_type_no_user_input = 0,
    client_connection_type_internal,
    client_connection_type_external
} wlan_emu_connection_type_t;

typedef char configurator_t[32];
typedef char reference_t[8][32];

typedef enum {
    sta_model_type_iphone,
    sta_model_type_pixel,
} sta_model_type_t;

typedef enum {
    sta_mobility_profile_type_static,
    sta_mobility_profile_type_to_ap,
    sta_mobility_profile_type_from_ap,
    sta_mobility_profile_type_to_from_ap,
} sta_mobility_profile_type_t;

typedef enum {
    sta_management_profile_type_connected,
    sta_management_profile_type_connect_disconnect,
} sta_management_profile_type_t;

typedef struct {
    sta_mobility_profile_type_t mob;
    sta_management_profile_type_t conn;
} sta_profile_t;

typedef enum {
    sta_test_type_management,
    sta_test_type_mobility,
} sta_test_type_t;

typedef unsigned char oui_t[3];

typedef struct {
    unsigned char type;
    unsigned char len;
    unsigned char val[30];
} __attribute__((packed)) sta_tlv_t;

typedef enum {
    step_param_type_radio = 0,
    step_param_type_vap,
    step_param_type_station_management,
    step_param_type_time,
    step_param_type_command,
    step_param_type_log_redirection,
    step_param_type_dml_subdoc,
    step_param_type_dml_reset,
    step_param_type_stats_get,
    step_param_type_stats_set,
    step_param_type_get_file,
    step_param_type_mgmt_frame_capture,
    step_param_type_get_pattern_files,
    step_param_type_timed_wait,
    step_param_type_config_onewifi,
    step_param_type_gateway_performance,
    step_param_type_packet_generator,
    step_param_type_config_iperf_server,
    step_param_type_config_iperf_client,
    step_param_type_ethernet_lan_interface,
    step_param_type_upgrade_or_reboot
} step_param_type_t;

typedef struct {
    unsigned int seconds;
} timed_wait_t;

typedef enum {
    log_operation_type_start = 0,
    log_operation_type_stop,
    log_operation_type_timer,
    log_operation_type_invalid,
} log_operation_type_t;

typedef enum {
    tc_endpoint_type_heartbeat = 0,
    tc_endpoint_type_complete,
    tc_endpoint_type_fail,
    tc_endpoint_type_factory_reset,
    tc_endpoint_type_reboot,
    tc_endpoint_type_pause,
    tc_endpoint_type_resume
} tc_endpoint_type_t;

typedef struct {
    log_operation_type_t log_operation;
    char log_redirect_filename[128];
    unsigned int stop_step_number;
    int redirect_fd;
    char log_result_file[128];
    pthread_t thread_id;
} log_redirect_t;

typedef struct {
    wifi_mon_stats_type_t data_type;
    wifi_neighborScanMode_t scan_mode;
    log_operation_type_t log_operation;
    int timeout;
    int radio_index;
    int vap_index;
    char output_file_name[128];
    int stop_log_step_number;
    bool is_stat_response_type_set;
    unsigned int get_stats_count = 0;
} wifi_stats_get_t;

typedef struct {
    int stats_duration; // Duration of the particular instance/list of a step
    char input_file_json[128];
} stat_set_config_t;

typedef struct {
    wifi_mon_stats_type_t data_type;
    wifi_neighborScanMode_t scan_mode;
    queue_t *stats_set_q; // stat_set_config_t;
    int current_stats_set_count; // number of instances/lists of a step
    int total_duration; // Total duration of a particular step
    int radio_index;
    int vap_index;
    int set_exec_duration;
} wifi_stats_set_t;

typedef struct {
    char test_cmd[1024];
    char cmd_exec_log_filename[128];
} command_t;

typedef struct {
    char source_file[256];
    char dest_filename[256];
    bool delete_source_file;
} get_file_t;

typedef enum {
    test_state_pending = 0,
    test_state_active,
    test_state_complete,
} test_state_t;

typedef struct {
    int rssi;
    int noise;
    unsigned int duration;
    unsigned int counter;
    test_state_t test_state;
} station_connectivity_profile_t;

typedef struct {
    int pre_assoc_rssi;
    int pre_assoc_noise;
    int pre_assoc_bitrate;
} pre_station_connectivity_profile_t;

typedef struct {
    unsigned int radio_index;
    unsigned int duration;
} mgmt_frame_capture_t;

typedef enum {
    wlan_emu_mode_ht = 1,
    wlan_emu_mode_vht = 2,
    wlan_emu_mode_he = 4,
    wlan_emu_mode_eht = 8,
} wlan_emu_op_modes;

// Connectivity is called as management
typedef struct {
    char ap_vap_name[32];
    //	char ap_profile[1024];
    //	sta_management_profile_type_t manage_type;
    unsigned int test_duration;
    queue_t *connectivity_q; // station_configuration_t
    pre_station_connectivity_profile_t *pre_assoc_stats;
    unsigned int current_profile_count;
    bool is_sta_management_timer;
    int op_modes;
} sta_management_t;

typedef struct {
    char file_location[128];
    char file_pattern[128];
    bool delete_pattern_files;
} get_pattern_files_t;

typedef struct {
    unsigned int msg_type; // wlan_emu_msg_type_t
    unsigned int frm80211_ops; // wlan_emu_frm80211_ops_type_t
    unsigned int cfg80211_ops; // wlan_emu_cfg80211_ops_type_t
    webconfig_subdoc_type_t subdoc_type; // webconfig_subdoc_type_t
} frame_capture_request_t;

typedef struct {
    frame_capture_request_t fc_request;
    char file_location[256];
} station_prototype_pcaps_t;

typedef struct {
    frame_capture_request_t fc_request_summary;
    queue_t *fc_prototype_q; // station_prototype_pcaps_t
} station_prototype_t;

typedef struct {
    unsigned int test_id;
    unsigned int phy_index;
    bool is_decoded;
    sta_key_t key;
    sta_model_type_t sta_type;
    char sta_name[32];
    char test_station_config[128];
    sta_test_type_t sta_test_type;
    wifi_vap_info_t *sta_vap_config;
    wifi_radio_operationParam_t *radio_oper_param;
    sta_profile_t profile;
    bool is_station_associated;
    bool is_station_prototype_enabled;
    station_prototype_t *station_prototype;
    bool capture_sta_requests;
    bool wait_connection;
    mac_address_t custom_mac;
    wlan_emu_connection_type_t connection_type;
    pid_t dhcp_pid;
    std::string sta_interface_name;
    bool is_ip_assigned;
    std::string ip_address;

    union {
        sta_management_t sta_management;
        // sta_mobility_t
    } u;
} sta_test_t;

typedef struct __attribute__((packed)) {
    uint8_t destination[6];
    uint8_t source[6];
    uint16_t protocol;
} ethernet_header_t;

typedef struct {
    char pcap_location[128];
} packet_capture_t;

typedef struct {
    std::string vapname;
    int interval;
    int iteration;
    queue_t *pcap_queue; // packet_capture_t
    mac_addr_t brlan0_mac;
    pthread_t p_tid;
    std::string bridge_name;
} packet_generator_t;

typedef struct {
    wlan_emu_msg_type_t type;

    union {
        wlan_emu_frm80211_ops_type_t frm80211_ops;
        wlan_emu_cfg80211_ops_type_t cfg80211_ops;
    } u;

    char pcap_file[128];
} wlan_emu_pcap_captures;

typedef struct {
    wlan_emu_test_coverage_t coverage_type;
    wlan_emu_test_type_t test_type;
    char test_json[128];
    wlan_emu_tests_state_t test_state;
    char test_case_name[64];
    char test_case_id[8];
    queue_t *test_steps_q; // struct test_step_params_t
    int current_test_step;
    unsigned int pending_steps;
} wlan_emu_test_case_config;

typedef struct {
    mac_address_t mac;
    int rssi;
    int noise;
    int bitrate;
} heart_beat_data_t;

typedef struct {
    mac_address_t old_mac;
    mac_address_t new_mac;
    int op_modes;
    char bridge_name[32];
} mac_update_t;

typedef enum { sta_state_free = 0, sta_state_in_use } sta_state_t;

typedef struct {
    unsigned int phy_index; /* actual index of the phy device */
    unsigned int rdk_radio_index; /* radio index of upper layer */
    wifi_interface_name_t interface_name;
    wifi_interface_name_t bridge_name;
    int vlan_id;
    unsigned int index;
    wifi_vap_name_t vap_name;
    mac_address_t mac;
    sta_state_t status;
} sta_info_t;

typedef enum { cmd_option_cpu, cmd_option_mem } cmd_option_t;

typedef struct {
    std::string result_file_name;
    std::string result_file;
    int interval;
    int iteration;
    cmd_option_t cmd_option;
    queue_t *process_status;
    pthread_t process_tid;
} gw_performance_t;

typedef enum {
    interface_type_ethernet = 1,
    interface_type_wifi,
} interface_type_t;

typedef enum {
    iperf_operation_type_start = 1,
    iperf_operation_type_stop,
    iperf_operation_type_invalid,
} iperf_operation_type_t;

typedef struct {
    unsigned int interface_step_number;
    char input_filename[128];
    char interface_name[128];
    char cmd_options[128];
    char result_file[128];
    pid_t iperf_server_pid;
} iperf_server_start_conf_t;

typedef struct {
    unsigned int stop_step_number;
} iperf_server_stop_conf_t;

typedef struct {
    iperf_operation_type_t input_operation;
    interface_type_t interface_type;
    std::string sta_key;

    union {
        iperf_server_stop_conf_t stop_conf;
        iperf_server_start_conf_t start_conf;
    } u;
} iperf_server_t;

typedef struct {
    unsigned int interface_step_number;
    unsigned int server_step_number;
    char input_filename[128];
    char interface_name[128];
    char cmd_options[128];
    char result_file[128];
    pid_t iperf_client_pid;
} iperf_client_start_conf_t;

typedef struct {
    unsigned int stop_step_number;
} iperf_client_stop_conf_t;

typedef struct {
    iperf_operation_type_t input_operation;
    interface_type_t interface_type;
    std::string sta_key;

    union {
        iperf_client_stop_conf_t stop_conf;
        iperf_client_start_conf_t start_conf;
    } u;
} iperf_client_t;

typedef enum { eth_interface_state_free = 0, eth_interface_state_in_use } eth_interface_state_t;

typedef struct {
    std::string interface_name;
    mac_address_t interface_mac;
    std::string ip_address;
    bool is_ip_assigned;
    eth_interface_state_t state;
} eth_dev_info_t;

typedef struct {
    unsigned int test_id;
    eth_dev_info_t eth_dev_info;
    std::string result_file;
    sta_key_t key;
    int duration;
} eth_lan_interface_t;

typedef struct {
    unsigned int step_number;
} step_number_entry_t;

typedef struct {
    std::string build_name;
    bool is_logging_enabled;
    queue_t *logging_step_numbers; // step_number_entry_t
    bool is_fr_enabled;
} device_upgrade_t;

#ifdef __cplusplus
}
#endif

#endif // WLAN_EMU_COMMON_H
