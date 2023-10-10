#ifndef _WLAN_EMU_VAP_MGR_H
#define _WLAN_EMU_VAP_MGR_H

#include "collection.h"
#include "wifi_hal.h"

class wlan_emu_vap_mgr_t {
    hash_map_t  *m_vap_map;

public:
    int init();

    wlan_emu_vap_mgr_t();
    ~wlan_emu_vap_mgr_t();
};

#endif // _WLAN_EMU_VAP_MGR_H
