#ifndef _WLAN_EMU_MSG_HDLR_H
#define _WLAN_EMU_MSG_HDLR_H

#include <pthread.h>
#include "nl80211.h"
#include <net/if.h>
#include <linux/rtnetlink.h>
#include <netpacket/packet.h>
#include <linux/netlink.h>
#include <linux/genetlink.h>
#include <netlink/handlers.h>
#include <netlink/attr.h>
#include <netlink/genl/genl.h>
#include "wlan_emu_msg.h"
#include "wifi_hal.h"

class wlan_emu_msg_hdlr_t {
    int m_fd;
    pthread_t   m_tid;
    bool    m_threadExit;

private:
    const char *cfg80211_ops_type_to_string(wlan_emu_cfg80211_ops_type_t type);
    const char *mac80211_ops_type_to_string(wlan_emu_mac80211_ops_type_t type);
    const char *emu80211_ops_type_to_string(wlan_emu_emu80211_ops_type_t type);

public:
    int start();
    void stop();

    static void *msg_hdlr_thread_func(void *data);
    void msg_hdlr_thread_func();

    int get_message_interface_desc() { return m_fd; }
    virtual void queue_msg(wlan_emu_msg_t *data) = 0;

    wlan_emu_msg_hdlr_t();
    ~wlan_emu_msg_hdlr_t();
};

#endif // _WLAN_EMU_MSG_HDLR_H
