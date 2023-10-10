#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "wlan_emu_sta_mgr.h"
#include "wlan_emu_sta.h"
#include "cci_wifi_utils.hpp"
#include "wlan_emu_log.h"

extern "C" {
    INT wifi_hal_createVAP(wifi_radio_index_t index, wifi_vap_info_map_t *map);
    INT wifi_hal_connect(INT ap_index, wifi_bss_info_t *bss);
    INT wifi_hal_disconnect(INT ap_index);
    INT wifi_hal_getRadioVapInfoMap(wifi_radio_index_t index, wifi_vap_info_map_t *map);
}

static void ovs_fdb_flush(char *bridge)
{
    FILE *fp;
    char ovs_cmd[BUFSIZ];

    snprintf(ovs_cmd, BUFSIZ, "ovs-appctl fdb/flush %s", bridge);

    fp = popen(ovs_cmd, "r");

    if (fp == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to flush ovs fdb\n", __func__, __LINE__);
        return;
    }

    pclose(fp);
}

int wlan_emu_sta_mgr_t::find_first_free_dev()
{
    unsigned int i;
    sta_info_t *sta_info;

    for (i = 0; i < m_sta_info_count; i++) {
       sta_info = (sta_info_t *)queue_peek(m_sta_info_map, i);
       if (sta_info->status == sta_state_free) {
           return i;
       }
    }

    return -1;

}

void wlan_emu_sta_mgr_t::set_dev_busy(unsigned int dev_id)
{
    sta_info_t *sta_info;
    sta_info = (sta_info_t *)queue_peek(m_sta_info_map, dev_id);
    sta_info->status = sta_state_in_use;
}

void wlan_emu_sta_mgr_t::set_dev_free(unsigned int dev_id)
{
    sta_info_t *sta_info;
    sta_info = (sta_info_t *)queue_peek(m_sta_info_map, dev_id);
    sta_info->status = sta_state_free;
}


sta_info_t *wlan_emu_sta_mgr_t::get_devid_sta_info(unsigned int dev_id)
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: dev_id : %d\n", __func__, __LINE__, dev_id);
    return (sta_info_t *)queue_peek(m_sta_info_map, dev_id);

}

void wlan_emu_sta_mgr_t::send_heart_beat(char *key, heart_beat_data_t *heart_beat_data)
{
    wlan_emu_sta_t *sta;
    sta_info_t *sta_info = NULL;

    sta_info = get_devid_sta_info(get_dev_id_from_key(key));

    sta = (wlan_emu_sta_t *)hash_map_get(m_sta_map, key);
    if (sta != NULL) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Sending heartbeat to the key : %s\n", __func__, __LINE__, key);
        sta->handle_heart_beat(heart_beat_data);
        send_raw_packet(dummy_data, sizeof(dummy_data), INADDR_ANY, DUMMY_PORT_IN, INADDR_BROADCAST,
            DUMMY_PORT_OUT, MAC_BCAST_ADDR, if_nametoindex(sta_info->interface_name));
    }

}

void add_to_bridge(char *interface, char *bridge)
{
    FILE *fp;
    char buf[BUFSIZ];
    char ovs_cmd[BUFSIZ];

    snprintf(ovs_cmd, BUFSIZ, "ovs-vsctl add-port %s %s", bridge, interface);

    if ((fp = popen(ovs_cmd, "r")) != NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to add %s from bridge %s\n", __func__, __LINE__, interface, bridge);
        return;
    }

    pclose(fp);
};

void remove_from_bridge(char *interface, char *bridge)
{
    FILE *fp;
    char buf[BUFSIZ];
    char ovs_cmd[BUFSIZ];

    snprintf(ovs_cmd, BUFSIZ, "ovs-vsctl del-port %s %s", bridge, interface);

    if ((fp = popen(ovs_cmd, "r")) == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to remove %s from bridge %s\n", __func__, __LINE__, interface, bridge);
        return;
    }

    pclose(fp);
};

static void set_bridge_mac(char *bridge_name)
{
    FILE *fp;
    FILE *ptr;
    char buf[BUFSIZ];
    char ovs_cmd[BUFSIZ];
    char ifconf_cmd[BUFSIZ];
    char search_str[] = "HWaddr ";
    char *temp_str = NULL;

    snprintf(ifconf_cmd, BUFSIZ, "ifconfig %s", bridge_name);

    if ((ptr = popen(ifconf_cmd, "r")) != NULL) {
        size_t byte_count = fread(buf, 1, BUFSIZ - 1, ptr);
        buf[byte_count] = 0;

        temp_str = strstr(buf, "HWaddr ");
        if (temp_str == NULL) {
            wlan_emu_print(wlan_emu_log_level_err, " %s:%d No MAC found for %s", __func__, __LINE__, bridge_name);
            pclose(ptr);
            return;
        } else {
            temp_str += strlen(search_str);
        }
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Ifconfig failed for %s\n", __func__, __LINE__, bridge_name);
        return;
    }

    pclose(ptr);

    snprintf(ovs_cmd, BUFSIZ, "ovs-vsctl set bridge %s other_config:hwaddr=%s", bridge_name, temp_str);

    fp = popen(ovs_cmd, "r");

    if (fp == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to set mac for %s\n", __func__, __LINE__, bridge_name);
        return;
    }

    pclose(fp);
}

void wlan_emu_sta_mgr_t::remove_all_sta(unsigned int test_id)
{
    wlan_emu_sta_t *sta, *tmp;
    sta_key_t key;
    sta_info_t *sta_info = NULL;

    sta = (wlan_emu_sta_t *)hash_map_get_first(m_sta_map);
    while (sta != NULL) {
        tmp = sta;
        sta = (wlan_emu_sta_t *)hash_map_get_next(m_sta_map, tmp);

        if (tmp->get_test_id() == test_id) {
            wifi_vap_info_t *sta_vap_info = tmp->get_sta();
            wifi_hal_disconnect(sta_vap_info->vap_index);

            sta_info = get_devid_sta_info(sta->get_dev_id());
            remove_from_bridge(sta_info->interface_name, sta_vap_info->bridge_name);
            ovs_fdb_flush(sta_vap_info->bridge_name);

            wlan_emu_sta_mgr_t::create_key(key, tmp->get_dev_id(), test_id);

            set_dev_free(tmp->get_dev_id());
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Disconnect sta vap %d Freeing the device : %d \n",
                __func__, __LINE__, sta_vap_info->vap_index, tmp->get_dev_id());
            hash_map_remove(m_sta_map, key);

            delete tmp;
        }

    }
}

void wlan_emu_sta_mgr_t::remove_sta(sta_test_t *sta_test)
{
    wlan_emu_sta_t *sta;
    sta_key_t key;
    sta_info_t *sta_info = NULL;

    if (sta_test == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: sta_test is NULL\n", __func__, __LINE__);
        return;
    }

   // key
    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: Request to remove the sta with key : %s\n", __func__, __LINE__, sta_test->key);
    sta = (wlan_emu_sta_t *)hash_map_get(m_sta_map, sta_test->key);
    if (sta == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: sta is NULL\n", __func__, __LINE__);
        return;
    }

    //Disconnect the device
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Disconnect sta vap %d Freeing the device : %d \n", __func__, __LINE__, sta_test->sta_vap_config->vap_index, sta->get_dev_id());
    wifi_hal_disconnect(sta_test->sta_vap_config->vap_index);

    sta_info = get_devid_sta_info(sta->get_dev_id());
    remove_from_bridge(sta_info->interface_name, sta_test->sta_vap_config->bridge_name);
    ovs_fdb_flush(sta_test->sta_vap_config->bridge_name);

    set_dev_free(sta->get_dev_id());
    hash_map_remove(m_sta_map, sta_test->key);

    return;
}



int wlan_emu_sta_mgr_t::add_sta(sta_test_t *sta_test_config)
{
    wlan_emu_sta_t *sta;
    int dev_id;
    wifi_vap_info_map_t map;
    wifi_bss_info_t bss;
    sta_info_t *sta_info = NULL;
    mac_update_t mac_update;

    if ((dev_id = find_first_free_dev()) == -1) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: could not find free device\n", __func__, __LINE__);
        return -1;
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: found free device at pos: %d\n", __func__, __LINE__, dev_id);

    create_key(sta_test_config->key, dev_id, sta_test_config->test_id);

    sta_info = get_devid_sta_info(dev_id);
    sta_test_config->sta_vap_config->vap_index =  sta_info->index;
    sta_test_config->phy_index = sta_info->phy_index;

    memcpy(sta_test_config->sta_vap_config->u.sta_info.mac, sta_info->mac, sizeof(mac_address_t));

    add_to_bridge(sta_info->interface_name, sta_test_config->sta_vap_config->bridge_name);
    set_bridge_mac(sta_test_config->sta_vap_config->bridge_name);

    switch (sta_test_config->sta_type) {
        case sta_model_type_iphone:
            sta = new wlan_emu_sta_iphone_t(dev_id, sta_test_config->test_id, sta_test_config->sta_vap_config, &sta_test_config->profile);
            break;

        case sta_model_type_pixel:
            sta = new wlan_emu_sta_pixel_t(dev_id, sta_test_config->test_id, sta_test_config->sta_vap_config, &sta_test_config->profile);
            break;

        default:
            assert(0);
    }

    map.num_vaps = 1;
    memcpy(&map.vap_array[0], sta_test_config->sta_vap_config, sizeof(wifi_vap_info_t));
    if (wifi_hal_createVAP(dev_id, &map) == RETURN_OK) {
        sta->set_vap(sta_test_config->sta_vap_config);
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: wifi_hal_createVAP failed for dev_id : %d\n", __func__, __LINE__, dev_id);
        return RETURN_ERR;
    }

    memset(&mac_update, 0, sizeof(mac_update_t));
    memcpy(mac_update.old_mac, sta_info->mac, sizeof(mac_address_t));
    memcpy(sta_info->mac, map.vap_array[0].u.sta_info.mac, sizeof(mac_address_t));
    memcpy(mac_update.new_mac, sta_info->mac, sizeof(mac_address_t));
    memcpy(mac_update.bridge_name, map.vap_array[0].bridge_name, sizeof(mac_update.bridge_name));
    sta->send_mac_update(&mac_update);

    memset(&bss, 0, sizeof(bss));

    bss.freq  = chann_to_freq(sta_test_config->radio_oper_param->channel);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: vap_index : %d phy_index : %d channel : %d bss.freq : %d \n",
            __func__, __LINE__, sta_test_config->sta_vap_config->vap_index, sta_test_config->phy_index, sta_test_config->radio_oper_param->channel, bss.freq);

    snprintf(bss.ssid, sizeof(bss.ssid), "%s", sta_test_config->sta_vap_config->u.sta_info.ssid);
    memcpy(bss.bssid, sta_test_config->sta_vap_config->u.sta_info.bssid, sizeof(mac_address_t));
    wifi_hal_connect(sta_test_config->sta_vap_config->vap_index, &bss);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: hal connect called\n", __func__, __LINE__);

    hash_map_put(m_sta_map, strdup(sta_test_config->key), sta);
    set_dev_busy(dev_id);

    return 0;
}

int wlan_emu_sta_mgr_t::init(wifi_hal_capability_t *sta_hal_cap)
{
    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: initiated\n", __func__, __LINE__);
    int array_size;
    unsigned int i = 0;
    sta_info_t *sta_info;
    wifi_interface_name_idex_map_t *interface_map;
    wifi_vap_info_map_t vap_info_map;
    mac_addr_str_t mac_str;

    m_sta_map = hash_map_create();

    m_sta_info_map = queue_create();

    if ((m_sta_map == NULL) || (m_sta_info_map == NULL)) {
        wlan_emu_print(wlan_emu_log_level_info, "%s:%d: object creation failed m_sta_map : %p m_sta_info_map : %p\n",
                __func__, __LINE__, m_sta_map, m_sta_info_map);
        return RETURN_ERR;
    }

    array_size = ARRAY_SIZE(sta_hal_cap->wifi_prop.interface_map);
    for (i = 0; i<array_size; i++) {
        interface_map = &sta_hal_cap->wifi_prop.interface_map[i];
        if (interface_map->vap_name[0] != '\0') {
            sta_info = (sta_info_t *)malloc(sizeof(sta_info_t));
            if (sta_info == NULL) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: sta_info allocation failed\n", __func__, __LINE__);
                return RETURN_ERR;
            }
            snprintf(sta_info->vap_name, sizeof(sta_info->vap_name), "%s", interface_map->vap_name);
            sta_info->phy_index = interface_map->phy_index;
            sta_info->rdk_radio_index = interface_map->rdk_radio_index;
            snprintf(sta_info->interface_name, sizeof(sta_info->interface_name), "%s", interface_map->interface_name);
            snprintf(sta_info->bridge_name, sizeof(sta_info->bridge_name), "%s", interface_map->bridge_name);
            sta_info->vlan_id = interface_map->vlan_id;
            sta_info->index = interface_map->index;
            sta_info->status = sta_state_free;

            memset(&vap_info_map, 0, sizeof(wifi_vap_info_map_t));
            if (wifi_hal_getRadioVapInfoMap(sta_info->rdk_radio_index, &vap_info_map) != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d: vap info map failed for radio index : %d\n", __func__, __LINE__, sta_info->rdk_radio_index);
                free(sta_info);
                return RETURN_ERR;
            }

            for (unsigned int itr = 0; itr < vap_info_map.num_vaps; itr++) {
                if (vap_info_map.vap_array[itr].vap_index == sta_info->index) {
                    memcpy(sta_info->mac, vap_info_map.vap_array[itr].u.sta_info.mac, sizeof(mac_address_t));
                    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: itr : %d vap_index : %d mac : %s\n", __func__, __LINE__, itr,  vap_info_map.vap_array[itr].vap_index,
                            to_mac_str(sta_info->mac, mac_str));
                }
            }

            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: m_sta_info_count : %d sta_info->index : %d\n", __func__, __LINE__, m_sta_info_count, sta_info->index);
            m_sta_info_count++;
            queue_push(m_sta_info_map, sta_info);

        }
    }

    return 0;
}

void wlan_emu_sta_mgr_t::stop()
{

}

int wlan_emu_sta_mgr_t::start()
{
/*    unsigned int i, j;
    wifi_interface_name_idex_map_t  *map;
    wifi_vap_info_t vap;

    for (i = 0; i < m_cap->wifi_prop.numRadios; i++) {
        for (j = 0; j < MAX_NUM_VAP_PER_RADIO; j++) {
            map = &m_cap->wifi_prop.interface_map[i*MAX_NUM_VAP_PER_RADIO + j];
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Phy Index: %d\tRadio Index: %d\tInterface Name: %s\tVAP Name: %s\tVAP Index: %d\n",
                __func__, __LINE__,
                map->phy_index, map->rdk_radio_index, map->interface_name, map->vap_name, map->index);
            if ((map->index == 14) || (map->index == 15)) {
                vap.vap_index = map->index;
                strncpy(vap.vap_name, map->vap_name, sizeof(vap.vap_name));
                strncpy(vap.bridge_name, map->bridge_name, sizeof(vap.bridge_name));
                vap.vap_mode = wifi_vap_mode_sta;
                strncpy(vap.u.sta_info.ssid, "test_ssid", sizeof(vap.u.sta_info.ssid));
                add_sta(map->rdk_radio_index, &vap, sta_model_type_iphone);
            }
        }
    }*/

    return 0;
}

wlan_emu_sta_mgr_t::wlan_emu_sta_mgr_t()
{

}

wlan_emu_sta_mgr_t::~wlan_emu_sta_mgr_t()
{

}
