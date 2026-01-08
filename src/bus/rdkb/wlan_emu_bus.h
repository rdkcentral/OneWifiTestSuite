#ifndef WLAN_BUS_H
#define WLAN_BUS_H

#include "wlan_emu_bus_common.h"
#include "wlan_emu_ui_mgr.h"
#include <rbus.h>

class wlan_emu_bus_t {

public:
    wifi_emu_bus_desc_t desc;
    bus_cb_multiplexing_t bus_cb_mux;
    bus_error_t bus_init();
    wlan_emu_bus_t();
    ~wlan_emu_bus_t();
};

wlan_emu_bus_t *get_emu_bus_obj(void);
wifi_emu_bus_desc_t *get_emu_bus_descriptor(void);
#endif // WLAN_BUS_H
