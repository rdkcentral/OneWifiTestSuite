#ifndef _WLAN_EMU_MSG_MGR_H
#define _WLAN_EMU_MSG_MGR_H

#include "collection.h"
#include "wlan_emu_msg.h"
#include "wlan_emu_msg_hdlr.h"
#include "wlan_emu_tests.h"
#include <stdbool.h>

class wlan_emu_tests_t;

class wlan_emu_msg_mgr_t : public wlan_emu_msg_hdlr_t {

private:
    queue_t *m_tests;

public:
    int start();
    void stop();

    void subscribe(wlan_emu_tests_t *test);
    void unsubscribe(wlan_emu_tests_t *test);
    void queue_msg(wlan_emu_msg_t *msg);

    wlan_emu_msg_mgr_t *clone();

    wlan_emu_msg_mgr_t();
    ~wlan_emu_msg_mgr_t();
};

#endif // _WLAN_EMU_MSG_MGR_H
