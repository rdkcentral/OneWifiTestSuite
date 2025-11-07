#ifndef WLAN_EMU_REAL_STA_MGR_H
#define WLAN_EMU_REAL_STA_MGR_H

#include "wlan_emu_test_params.h"

class wlan_emu_real_sta_mgr_t {
    inline static void create_key(sta_key_t key, char *ssid, unsigned int test_id)
        {
            snprintf(key, sizeof(sta_key_t), "%s-%d", ssid, test_id);
        }
public:
	int add_sta(test_step_params_t *step);
	void remove_sta(test_step_params_t *step);

	wlan_emu_real_sta_mgr_t();
	~wlan_emu_real_sta_mgr_t();
};

#endif //WLAN_EMU_REAL_STA_MGR_H
