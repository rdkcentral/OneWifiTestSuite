#ifndef WLAN_BUS_H
#define WLAN_BUS_H

#include "wlan_emu_bus_common.h"

class wlan_emu_bus_t {

public:
    wifi_emu_bus_desc_t desc;
    bus_cb_multiplexing_t bus_cb_mux;
    bus_error_t bus_init();
    wlan_emu_bus_t();
    ~wlan_emu_bus_t();
};

#endif // WLAN_BUS_H