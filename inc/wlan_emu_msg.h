#ifndef _WLAN_EMU_MSG_H
#define _WLAN_EMU_MSG_H

#include "wlan_emu_test_params.h"
#include "wlan_emu_msg_data.h"
#include <stdbool.h>
#include <string.h>

class test_step_params_t;

class wlan_emu_msg_t {
    wlan_emu_msg_data_t m_msg;
    int dump(test_step_params_t *step_config);
    static bool eapol_msg1;
    static bool enable_beacon_dump;
    char msg_name[64];

public:
    const char *cfg80211_ops_type_to_string();
    const char *mac80211_ops_type_to_string();
    const char *emu80211_ops_type_to_string();

    char *get_msg_name()
    {
        return msg_name;
    }

    int get_msgname_from_msgtype();

    inline void update_msg_data(wlan_emu_msg_data_t *data)
    {
        memcpy(&m_msg, data, sizeof(wlan_emu_msg_data_t));
        return;
    }

    inline wlan_emu_msg_type_t get_msg_type()
    {
        return m_msg.type;
    }

    const char *get_ops_string_by_msg_type();

    inline wlan_emu_mac80211_ops_type_t get_mac80211_ops_type()
    {
        return m_msg.u.mac80211.ops;
    }

    inline wlan_emu_cfg80211_ops_type_t get_cfg80211_ops_type()
    {
        return m_msg.u.cfg80211.ops;
    }

    inline wlan_emu_emu80211_ops_type_t get_emu80211_ops_type()
    {
        return m_msg.u.emu80211.ops;
    }

    inline wlan_emu_frm80211_ops_type_t get_frm80211_ops_type()
    {
        return m_msg.u.frm80211.ops;
    }

    inline webconfig_subdoc_type_t get_webconfig_subdoc_type()
    {
        return m_msg.u.ow_webconfig.subdoc_type;
    }

    bool operator==(wlan_emu_msg_mac80211_t *grd);
    bool operator==(wlan_emu_msg_cfg80211_t *grd);
    bool operator==(wlan_emu_msg_emu80211_t *grd);

    inline wlan_emu_msg_data_t *get_msg()
    {
        return &m_msg;
    }

    inline wlan_emu_msg_mac80211_t *get_mac80211_msg()
    {
        return &m_msg.u.mac80211;
    }

    inline wlan_emu_msg_cfg80211_t *get_cfg80211_msg()
    {
        return &m_msg.u.cfg80211;
    }

    inline wlan_emu_msg_emu80211_t *get_emu80211_msg()
    {
        return &m_msg.u.emu80211;
    }

    void send_ctrl_msg(wlan_emu_test_coverage_t coverage, wlan_emu_test_type_t type,
        wlan_emu_emu80211_ctrl_type_t ctrl);
    void send_ctrl_msg(unsigned char *buff, unsigned int buff_len,
        wlan_emu_emu80211_cmd_type_t ctrl);

    void load_cfg80211_start_ap(const char *pcap);
    void unload_cfg80211_start_ap(test_step_params_t *step_config);
    void unload_frm80211_msg(test_step_params_t *step_config);

    wlan_emu_msg_t(wlan_emu_msg_data_t *msg);

    wlan_emu_msg_t(wlan_emu_msg_t &msg);
    wlan_emu_msg_t();

    ~wlan_emu_msg_t();
};

#endif // _WLAN_EMU_MSG_H
