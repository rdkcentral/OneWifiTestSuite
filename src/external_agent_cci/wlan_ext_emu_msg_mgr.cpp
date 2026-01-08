#include "wlan_ext_emu_msg_mgr.h"
#include "wlan_emu_log.h"
#include "wlan_ext_emu_tests.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>

void wlan_ext_emu_msg_mgr_t::subscribe(wlan_ext_emu_tests_t *test)
{
    queue_push(m_tests, test);
}

void wlan_ext_emu_msg_mgr_t::unsubscribe(wlan_ext_emu_tests_t *test)
{
    unsigned int i, count;
    wlan_ext_emu_tests_t *elem;

    count = queue_count(m_tests);
    for (i = 0; i < count; i++) {
        elem = (wlan_ext_emu_tests_t *)queue_peek(m_tests, i);
        if (test == elem) {
            queue_remove(m_tests, i);
            break;
        }
    }
}

void wlan_ext_emu_msg_mgr_t::queue_msg(wlan_emu_msg_t *msg)
{
    unsigned int i, count;
    wlan_ext_emu_tests_t *elem;

    count = queue_count(m_tests);

    for (i = 0; i < count; i++) {
        elem = (wlan_ext_emu_tests_t *)queue_peek(m_tests, i);
        elem->push_msg(new wlan_emu_msg_t(*msg));
    }
}

int wlan_ext_emu_msg_mgr_t::start()
{
    return wlan_emu_msg_hdlr_t::start();
}

void wlan_ext_emu_msg_mgr_t::stop()
{
    wlan_emu_msg_hdlr_t::stop();
}

wlan_ext_emu_msg_mgr_t::wlan_ext_emu_msg_mgr_t()
{
    m_tests = queue_create();
}

wlan_ext_emu_msg_mgr_t::~wlan_ext_emu_msg_mgr_t()
{
    queue_destroy(m_tests);
}
