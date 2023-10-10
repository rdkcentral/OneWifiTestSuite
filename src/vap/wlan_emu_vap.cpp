#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wlan_emu_vap.h"
#include "wlan_emu_log.h"

// int wlan_emu_vap_t::add_sta(mac_address_t mac, bool add)
// {
//     return 0;
// }

int wlan_emu_vap_t::init()
{
    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: initiated\n", __func__, __LINE__);

    return 0;
}

wlan_emu_vap_t::wlan_emu_vap_t(mac_address_t mac, wifi_vap_mode_t mode)
{
    m_info.vap_mode = mode;

    if (mode == wifi_vap_mode_ap) {
        memcpy(m_info.u.bss_info.bssid, mac, sizeof(mac_address_t));
    } else if (mode == wifi_vap_mode_sta) {
        memcpy(m_info.u.sta_info.bssid, mac, sizeof(mac_address_t));
    }
}

wlan_emu_vap_t::~wlan_emu_vap_t()
{

}
