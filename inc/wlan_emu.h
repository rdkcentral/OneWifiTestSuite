#ifndef _WLAN_EMU_H
#define _WLAN_EMU_H

#include "cci_wifi_utils.hpp"
#include "wifi_hal.h"
#include "wlan_emu_common.h"
#include "wlan_emu_log.h"
#include "wlan_emu_msg_mgr.h"
#include "wlan_emu_sta_mgr.h"
#include "wlan_emu_ext_sta_mgr.h"
#include "wlan_emu_ui_mgr.h"
#include "wlan_emu_bus.h"

#define CCI_TEST_CONFIG_URL "Device.WiFi.Tests.TestConfigURL"
#define CCI_TEST_RESULT_FILENAME "Device.WiFi.Tests.ResultsFileName"
#define CCI_TEST_INTERFACE "Device.WiFi.Tests.Interface"
#define CCI_TEST_SIMULATED_CLIENTDEVICES "Device.WiFi.Tests.SimulatedClientDevices"
#define CCI_TEST_START "Device.WiFi.Tests.Start"
#define CCI_TEST_STATUS "Device.WiFi.Tests.Status"

class wlan_emu_tests_radio_t;
class wlan_emu_tests_private_vap_t;

class wlan_emu_t {
    wifi_hal_capability_t m_cap;
    wlan_emu_msg_mgr_t m_msg_mgr;
    wlan_emu_bus_t m_bus_mgr;
    wlan_emu_sim_sta_mgr_t m_sim_sta_mgr;
    wlan_emu_ext_sta_mgr_t m_ext_sta_mgr;
#ifdef LINUX_VM
    wlan_emu_standalone_ui_mgr_t m_ui_mgr;
#else
    static wlan_emu_ui_mgr_t m_ui_mgr;
#endif
    static wlan_emu_tests_state_t m_state;
    static wlan_emu_dml_tests_state_t dml_state;
    bus_handle_t handle;

private:
    void update_state_io_wait_done(wlan_emu_sig_type_t val);
    int start_test();
    void abort_test();

public:
    int init();
    int run();
    void bus_register_handlers();
    static bus_error_t get_cci_handler(char *event_name, raw_data_t *p_data);
    static bus_error_t set_cci_handler(char *event_name, raw_data_t *p_data);

    // wifi_platform_type_t get_platform_type() { return m_cap.wifi_prop.platform_type; }
    inline wlan_emu_tests_state_t get_state()
    {
        return m_state;
    }

    inline static wlan_emu_dml_tests_state_t get_dml_state()
    {
        return dml_state;
    }

    wlan_emu_t();
    ~wlan_emu_t();
};

#endif // _WLAN_EMU_H
