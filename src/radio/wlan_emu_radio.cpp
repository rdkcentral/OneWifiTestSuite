#include "wlan_emu_radio.h"
#include <stdio.h>
#include <assert.h>

int wlan_emu_radio_t::init()
{
    printf("%s:%d: initiated\n", __func__, __LINE__);
    return 0;
}

wlan_emu_radio_t::wlan_emu_radio_t(wifi_freq_bands_t band)
{
    m_op.band = band;

    switch (band) {
    case WIFI_FREQUENCY_2_4_BAND:
        m_op.channelWidth = WIFI_CHANNELBANDWIDTH_20MHZ;
        m_op.channel = 6;
        break;
    case WIFI_FREQUENCY_5_BAND:
        m_op.channelWidth = WIFI_CHANNELBANDWIDTH_80MHZ;
        m_op.channel = 44;
        break;
    case WIFI_FREQUENCY_5L_BAND:
        m_op.channelWidth = WIFI_CHANNELBANDWIDTH_80MHZ;
        m_op.channel = 44;
        break;
    case WIFI_FREQUENCY_5H_BAND:
        m_op.channelWidth = WIFI_CHANNELBANDWIDTH_80MHZ;
        m_op.channel = 157;
        break;
    case WIFI_FREQUENCY_6_BAND:
        m_op.channelWidth = WIFI_CHANNELBANDWIDTH_160MHZ;
        m_op.channel = 6;
        break;
    default:
        assert((band < WIFI_FREQUENCY_2_4_BAND) || (band > WIFI_FREQUENCY_6_BAND));
        break;
    }
}

wlan_emu_radio_t::~wlan_emu_radio_t()
{

}
