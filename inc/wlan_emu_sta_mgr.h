#ifndef _WLAN_EMU_STA_MGR_H
#define _WLAN_EMU_STA_MGR_H

#include "collection.h"
#include "wifi_hal.h"
#include "wifi_base.h"
#include "wlan_emu_common.h"

typedef enum {
    sta_state_free = 0,
    sta_state_in_use
} sta_state_t;


typedef struct {
    unsigned int     phy_index;           /**< actual index of the phy device */
    unsigned int     rdk_radio_index;     /**< radio index of upper layer */
    wifi_interface_name_t  interface_name;
    wifi_interface_name_t  bridge_name;
    int              vlan_id;
    unsigned int     index;
    wifi_vap_name_t  vap_name;
    mac_address_t   mac;
    sta_state_t status;
} sta_info_t;


class wlan_emu_sta_mgr_t {
    hash_map_t              *m_sta_map;
    wifi_hal_capability_t   *m_cap;
    queue_t *m_sta_info_map; //sta_info_t
    unsigned int m_sta_info_count;
    unsigned int m_dev_status[DEV_STATUS_ARR_SZ];
    int find_first_free_dev();
    void set_dev_busy(unsigned int dev_id);
    void set_dev_free(unsigned int dev_id);

    inline static void create_key(sta_key_t key, unsigned int dev_id, unsigned int test_id)
    {
        snprintf(key, sizeof(sta_key_t), "%d-%d", test_id, dev_id);
    }

    inline unsigned int get_test_id_from_key(sta_key_t key)
    {
        unsigned int test_id, dev_id;

        sscanf(key, "%d-%d", &test_id, &dev_id);
        return test_id;
    }

    inline unsigned int get_dev_id_from_key(sta_key_t key)
    {
        unsigned int test_id, dev_id;

        sscanf(key, "%d-%d", &test_id, &dev_id);
        return dev_id;
    }

public:
    int init(wifi_hal_capability_t *cap);
    int start();
    void stop();

    int add_sta(sta_test_t *sta_test_config);
    void remove_all_sta(unsigned int vap_id);
    void send_heart_beat(char *key, heart_beat_data_t *heart_beat_data);
    sta_info_t *get_devid_sta_info(unsigned int dev_id);
    void remove_sta(sta_test_t *sta_test);

    wlan_emu_sta_mgr_t();
    ~wlan_emu_sta_mgr_t();
};

#endif // _WLAN_EMU_STA_MGR_H
