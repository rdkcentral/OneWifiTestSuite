#ifndef WLAN_EXT_EMU_TEST_STEP_PARAMS_H
#define WLAN_EXT_EMU_TEST_STEP_PARAMS_H
#include "cJSON.h"
#include "cci_wifi_utils.hpp"
#include "wlan_emu_common.h"
#include "wlan_emu_ext_agent_interface.h"
#include "wlan_emu_msg.h"
#include "wlan_ext_emu_sta_mgr.h"
#include <cjson/cJSON.h>
#include <cstdio>
#include <cstring>
#include <string>

class wlan_ext_emu_msg_mgr_t;
class wlan_ext_emu_t;

class wlan_ext_test_step_params_t {
private:
    wlan_ext_emu_msg_mgr_t *m_msg_mgr;
    wlan_ext_emu_sta_mgr_t *m_sta_mgr;
    wlan_ext_emu_t *m_ext_emu;

public:
    // along with location
    std::string step_config_file_name;
    //refer to inc/wlan_emu_tests.h
    char test_case_name[128];
    char test_case_id[8];
    unsigned int radio_index; //EA not aware of radio index, Gateway needs to provide it
    unsigned int step_number; // User input
    unsigned int step_seq_num; // step Sequence number
    bool is_step_initialized; // To check errors in constructor
    step_param_type_t param_type;
    int execution_time;
    int timeout_count;
    wlan_emu_tests_state_t step_state;
    cJSON *artifact_json_list;
    std::string step_result_file_name;
    bool is_step_result_updated;
    bool capture_frames;
    frame_capture_request_t frame_request;

    union {
        sta_test_t *sta_test;
    } u;

    virtual int wlan_ext_step_execute() = 0;
    virtual int wlan_ext_step_timeout() = 0;
    virtual int wlan_ext_step_frame_filter(wlan_emu_msg_t *msg) = 0;
    virtual void wlan_ext_step_remove() = 0;
    virtual void wlan_ext_step_status_update(cJSON *agent_status) = 0;
    virtual int wlan_ext_step_decode_subdocument(cJSON *root_json,
        wlan_ext_test_step_params_t *step) = 0;

    inline void param_add_msg_mgr(wlan_ext_emu_msg_mgr_t *msg_mgr)
    {
        m_msg_mgr = msg_mgr;
    }

    inline void param_add_ext_emu(wlan_ext_emu_t *ext_emu)
    {
        m_ext_emu = ext_emu;
    }

    inline void param_add_agent_sta_mgr(wlan_ext_emu_sta_mgr_t *sta_mgr)
    {
        m_sta_mgr = sta_mgr;
    }

    inline wlan_ext_emu_msg_mgr_t *get_msg_mgr()
    {
        return m_msg_mgr;
    }

    inline wlan_ext_emu_t *get_ext_emu()
    {
        return m_ext_emu;
    }

    inline wlan_ext_emu_sta_mgr_t *get_agent_sta_mgr()
    {
        return m_sta_mgr;
    }

    inline void wlan_ext_step_add_mgr_data(wlan_ext_emu_msg_mgr_t *msg_mgr, wlan_ext_emu_t *ext_emu,
        wlan_ext_emu_sta_mgr_t *sta_mgr)
    {
        param_add_msg_mgr(msg_mgr);
        param_add_ext_emu(ext_emu);
        param_add_agent_sta_mgr(sta_mgr);
    }

    void update_step_status_json(cJSON *agent_status);
    int create_pcap(wlan_emu_msg_t *msg);

    virtual ~wlan_ext_test_step_params_t()
    {
    }
};

class wlan_ext_test_step_param_sta : public wlan_ext_test_step_params_t {
public:
    virtual int wlan_ext_step_execute() = 0;
    virtual int wlan_ext_step_timeout() = 0;
    virtual int wlan_ext_step_frame_filter(wlan_emu_msg_t *msg) = 0;
    virtual void wlan_ext_step_remove() = 0;
    virtual void wlan_ext_step_status_update(cJSON *agent_status) = 0;
    virtual int wlan_ext_step_decode_subdocument(cJSON *root_json,
        wlan_ext_test_step_params_t *step) = 0;
};

class wlan_ext_test_step_param_sta_management : public wlan_ext_test_step_param_sta {
public:
    int wlan_ext_step_execute();
    int wlan_ext_step_timeout();
    void wlan_ext_step_remove();
    int wlan_ext_step_frame_filter(wlan_emu_msg_t *msg);
    void wlan_ext_step_status_update(cJSON *agent_status);
    int wlan_ext_step_decode_subdocument(cJSON *root_json, wlan_ext_test_step_params_t *step);
    wlan_ext_test_step_param_sta_management();
    ~wlan_ext_test_step_param_sta_management();
};

#endif // WLAN_EXT_EMU_TEST_STEP_PARAMS_H
