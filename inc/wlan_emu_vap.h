#ifndef _WLAN_EMU_VAP_H
#define _WLAN_EMU_VAP_H

#include "wifi_hal.h"
#include "wlan_emu_sta_mgr.h"

class wlan_emu_vap_t {
    wifi_vap_info_t m_info;

public:
    int init();
    mac_address_t *get_bssid() { return &m_info.u.bss_info.bssid; }

    wlan_emu_vap_t(mac_address_t mac, wifi_vap_mode_t mode);
    ~wlan_emu_vap_t();
};

#endif // _WLAN_EMU_VAP_H
