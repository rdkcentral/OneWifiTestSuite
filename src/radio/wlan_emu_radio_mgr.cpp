#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "wlan_emu_radio_mgr.h"
#include "wlan_emu_radio.h"
#include "wlan_emu_log.h"

int wlan_emu_radio_mgr_t::add_radio(unsigned int radio_index, bool add)
{
    wlan_emu_radio_t *radio;
    char    radio_name[16];
    wifi_freq_bands_t   band;

    snprintf(radio_name, sizeof(radio_name), "radio_%d", radio_index);

    if ((radio = (wlan_emu_radio_t *)hash_map_get(m_radio_map, radio_name)) != NULL) {
        wlan_emu_print(wlan_emu_log_level_info, "%s:%d: radio: %s already exists\n", __func__, __LINE__, radio_name);
        return -1;
    }

    switch (radio_index) {
    case 1:
        band = WIFI_FREQUENCY_2_4_BAND;
        break;
    case 2:
        band = WIFI_FREQUENCY_5_BAND;
        break;
    case 3:
        band = WIFI_FREQUENCY_6_BAND;
        break;
    default:
        band = WIFI_FREQUENCY_5_BAND;
        break;
    }

    radio = new wlan_emu_radio_t(band);
    hash_map_put(m_radio_map, strdup(radio_name), radio);

    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: radio: %s added to list\n", __func__, __LINE__, radio_name);

    return 0;
}

int wlan_emu_radio_mgr_t::start()
{
    return 0;
}

int wlan_emu_radio_mgr_t::init()
{
    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: initiated\n", __func__, __LINE__);

    m_radio_map = hash_map_create();

    return 0;
}

wlan_emu_radio_mgr_t::wlan_emu_radio_mgr_t()
{

}

wlan_emu_radio_mgr_t::~wlan_emu_radio_mgr_t()
{

}
