#ifndef _WLAN_EMU_RADIO_MGR_H
#define _WLAN_EMU_RADIO_MGR_H

#include "collection.h"

class wlan_emu_radio_mgr_t {
    hash_map_t *m_radio_map;

public:
    int init();
    int start();
    int add_radio(unsigned int radio_index, bool add = true);

    wlan_emu_radio_mgr_t();
    ~wlan_emu_radio_mgr_t();
};

#endif // _WLAN_EMU_RADIO_MGR_H
