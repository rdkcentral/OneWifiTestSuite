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

#ifndef WLAN_EMU_EXT_AGENT_INTERFACE_H
#define WLAN_EMU_EXT_AGENT_INTERFACE_H

#include "cci_wifi_utils.hpp"
#include "wifi_hal.h"
#include "wifi_util.h"
#include "wlan_emu_common.h"
#include "wlan_emu_log.h"
#include "wlan_emu_test_params.h"
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

class wlan_emu_tests_t;

typedef enum {
    ext_agent_test_state_idle,
    ext_agent_test_state_ready,
    ext_agent_test_state_running,
    ext_agent_test_state_complete,
    ext_agent_test_state_fail,
    ext_agent_test_state_invalid,
} ext_agent_test_state_t;

typedef struct {
    int step_number;
    wlan_emu_tests_state_t state;
    std::vector<std::string> result_files;
} ext_agent_step_status_t;

typedef struct {
    std::string agent_name;
    int step_count;
    ext_agent_test_state_t state;
    std::vector<ext_agent_step_status_t> steps;
} ext_agent_status_resp_t;

class wlan_emu_ext_agent_interface_t {
private:
    // Enum to String mapping
    static inline const std::unordered_map<ext_agent_test_state_t, std::string>
        agent_state_to_str = {
            { ext_agent_test_state_idle,     "idle"     },
            { ext_agent_test_state_ready,    "ready"    },
            { ext_agent_test_state_running,  "running"  },
            { ext_agent_test_state_complete, "complete" },
            { ext_agent_test_state_fail,     "fail"     }
    };

    // String to Enum mapping
    static inline const std::unordered_map<std::string, ext_agent_test_state_t>
        str_to_agent_state = {
            { "idle",     ext_agent_test_state_idle     },
            { "ready",    ext_agent_test_state_ready    },
            { "running",  ext_agent_test_state_running  },
            { "complete", ext_agent_test_state_complete },
            { "fail",     ext_agent_test_state_fail     }
    };

    // Enum to String mapping
    static inline const std::unordered_map<wlan_emu_tests_state_t, std::string>
        step_state_to_str = {
            { wlan_emu_tests_state_cmd_none,     "none"     },
            { wlan_emu_tests_state_cmd_wait,     "wait"     },
            { wlan_emu_tests_state_cmd_request,  "request"  },
            { wlan_emu_tests_state_cmd_start,    "start"    },
            { wlan_emu_tests_state_cmd_results,  "results"  },
            { wlan_emu_tests_state_cmd_continue, "continue" },
            { wlan_emu_tests_state_cmd_abort,    "abort"    }
    };

    // String to Enum mapping
    static inline const std::unordered_map<std::string, wlan_emu_tests_state_t>
        str_to_step_state = {
            { "none",     wlan_emu_tests_state_cmd_none     },
            { "wait",     wlan_emu_tests_state_cmd_wait     },
            { "request",  wlan_emu_tests_state_cmd_request  },
            { "start",    wlan_emu_tests_state_cmd_start    },
            { "results",  wlan_emu_tests_state_cmd_results  },
            { "continue", wlan_emu_tests_state_cmd_continue },
            { "abort",    wlan_emu_tests_state_cmd_abort    }
    };

    const std::string agent_proto = "http://";
    const std::string agent_port = ":1234";
    const std::string status_endpoint = "/Status";
    const std::string command_endpoint = "/Command";

    int send_external_agent_command(const std::string &command);

public:
    char version[8];
    char subdoc_name[32];
    unsigned external_agent_count;
    std::string agent_hostname;
    uint8_t agent_mac[MAC_ADDRESS_LENGTH];
    char agent_ip_address[16];
    // char *encoded_raw;
    queue_t *client_info; // sta_info_t
    unsigned int total_supported_clients;
    unsigned int free_clients; // get_num_free_clients
    wlan_emu_tests_t *test;
    wlan_emu_ui_mgr_t* m_ui_mgr;

    // Get the capability file to decode
    int get_external_agent_capabilities(hash_map_t *ext_agent_map);
    int get_external_agent_test_status(ext_agent_status_resp_t &status);
    // used by the gateway
    int get_external_agent_info(hash_map_t *ext_agent_map, bus_handle_t *handle,
        wlan_emu_bus_t *bus_mgr);
    // used by external agent
    int encode_external_agent_capability(std::string &cap_json_string);
    // used by  gateway
    int decode_external_agent_capability(const std::string &json,
        wlan_emu_ext_agent_interface_t *agent_details);

    int encode_external_client_subdoc(wlan_emu_ext_agent_interface_t *agent_info,
        test_step_params_t *step, std::string &cli_subdoc);
    int decode_external_client_subdoc(wlan_emu_ext_agent_interface_t *agent_info,
        test_step_params_t *step, char *cjson_cap);

    int encode_external_agent_test_status(const ext_agent_status_resp_t &status,
        std::string &json_str);
    int decode_external_agent_test_status(const std::string &json, ext_agent_status_resp_t &status);

    int download_external_agent_result_files(const std::vector<std::string> &files);

    int send_external_agent_start_command();
    int send_external_agent_stop_command();

    std::string get_agent_proto()
    {
        return agent_proto;
    }

    std::string get_agent_port()
    {
        return agent_port;
    }

    // Convert Enum to String
    static std::string agent_state_as_string(ext_agent_test_state_t state)
    {
        auto it = agent_state_to_str.find(state);
        return (it != agent_state_to_str.end()) ? it->second : "Unknown";
    }

    // Convert String to Enum
    static ext_agent_test_state_t agent_state_as_enum(const std::string &str)
    {
        auto it = str_to_agent_state.find(str);
        return (it != str_to_agent_state.end()) ? it->second : ext_agent_test_state_invalid;
    }

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

    const std::string &get_agent_hostname()
    {
        return agent_hostname;
    }

    virtual void remove_all_sta()
    {
        return;
    }

    // used only by agent to initialize
    int agent_init();
    void agent_exit();
    wlan_emu_ext_agent_interface_t();
    ~wlan_emu_ext_agent_interface_t();
};
#endif // WLAN_EMU_EXT_AGENT_H
