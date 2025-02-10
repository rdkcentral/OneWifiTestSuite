#ifndef WLAN_EXT_EMU_STA_MGR_H
#define WLAN_EXT_EMU_STA_MGR_H

#include "collection.h"
#include "wifi_base.h"
#include "wifi_hal.h"
#include "wlan_emu_common.h"
#include "wlan_emu_msg_data.h"
#include <cstdio>

class wlan_ext_emu_sta_mgr_t {
    hash_map_t *m_sta_map;
    wifi_hal_capability_t *m_cap;
    queue_t *m_sta_info_map; // sta_info_t
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
    int init(wifi_hal_capability_t *sta_hal_cap);
    int start();
    void stop();

    int add_sta(sta_test_t *sta_test_config);
    sta_info_t *get_devid_sta_info(unsigned int dev_id);
    void remove_sta(sta_test_t *sta_test);

    int get_sta_info_count()
    {
        return m_sta_info_count;
    }

    queue_t *get_sta_info_map()
    {
        return m_sta_info_map;
    }

    wlan_ext_emu_sta_mgr_t();
    ~wlan_ext_emu_sta_mgr_t();
};

#endif //_WLAN_EXT_EMU_STA_MGR_H
