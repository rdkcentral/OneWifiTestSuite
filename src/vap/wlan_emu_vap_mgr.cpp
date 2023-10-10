#include <stdio.h>
#include "wlan_emu_vap_mgr.h"
#include "wlan_emu_log.h"

int wlan_emu_vap_mgr_t::init()
{
    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: initiated\n", __func__, __LINE__);

    m_vap_map = hash_map_create();

    return 0;
}

wlan_emu_vap_mgr_t::wlan_emu_vap_mgr_t()
{

}

wlan_emu_vap_mgr_t::~wlan_emu_vap_mgr_t()
{

}
