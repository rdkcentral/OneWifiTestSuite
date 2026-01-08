#ifndef WLAN_EXT_EMU_MSG_MGR_H
#define WLAN_EXT_EMU_MSG_MGR_H

#include "collection.h"
#include "wlan_emu_msg.h"
#include "wlan_emu_msg_hdlr.h"
// #include "wlan_ext_emu_tests.h"
#include <stdbool.h>

class wlan_ext_emu_tests_t;

class wlan_ext_emu_msg_mgr_t : public wlan_emu_msg_hdlr_t {

private:
    queue_t *m_tests;

public:
    int start();
    void stop();

    void subscribe(wlan_ext_emu_tests_t *test);
    void unsubscribe(wlan_ext_emu_tests_t *test);
    void queue_msg(wlan_emu_msg_t *msg);

    wlan_ext_emu_msg_mgr_t *clone();

    wlan_ext_emu_msg_mgr_t();
    ~wlan_ext_emu_msg_mgr_t();
};

#endif // WLAN_EXT_EMU_MSG_MGR_H
