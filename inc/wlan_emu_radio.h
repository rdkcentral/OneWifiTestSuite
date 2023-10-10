#ifndef _WLAN_EMU_RADIO_H
#define _WLAN_EMU_RADIO_H

#include "wifi_hal.h"
#include "wlan_emu_vap_mgr.h"

class wlan_emu_radio_t {
    wifi_radio_operationParam_t m_op;
    wlan_emu_vap_mgr_t m_vap_mgr;

public:
    int init();

    wlan_emu_radio_t(wifi_freq_bands_t band);
    ~wlan_emu_radio_t();
};

#endif // _WLAN_EMU_RADIO_H
