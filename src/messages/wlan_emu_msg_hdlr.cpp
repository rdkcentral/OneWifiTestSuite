#include "wlan_emu_msg_hdlr.h"
#include "common/ieee802_11_defs.h"
#include "wlan_emu_log.h"
#include "wlan_emu_msg.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pcap.h>
#include <poll.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>

#define WLAN_PMK_LEN 32

static void handle_cfg80211_start_ap(char *f_tmp, wlan_emu_msg_data_t *f_data)
{
    struct ieee80211_mgmt *head;
    if ((f_tmp == NULL) || (f_data == NULL)) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: NULL Pointer \n", __func__, __LINE__);
        return;
    }

    memcpy(&(f_data->u.cfg80211.u.start_ap.ifindex), f_tmp, sizeof(int));
    f_tmp += sizeof(int);

    memcpy(&(f_data->u.cfg80211.u.start_ap.phy_index), f_tmp, sizeof(int));
    f_tmp += sizeof(int);

    memcpy(&(f_data->u.cfg80211.u.start_ap.head_len), f_tmp, sizeof(size_t));
    f_tmp += sizeof(size_t);

    memcpy(&(f_data->u.cfg80211.u.start_ap.tail_len), f_tmp, sizeof(size_t));
    f_tmp += sizeof(size_t);

    f_data->u.cfg80211.u.start_ap.beacon_head = (char *)(malloc(
        f_data->u.cfg80211.u.start_ap.head_len));
    memcpy((void *)(f_data->u.cfg80211.u.start_ap.beacon_head), f_tmp,
        f_data->u.cfg80211.u.start_ap.head_len);
    f_tmp += f_data->u.cfg80211.u.start_ap.head_len;

    f_data->u.cfg80211.u.start_ap.beacon_tail = (char *)(malloc(
        f_data->u.cfg80211.u.start_ap.tail_len));
    memcpy((void *)f_data->u.cfg80211.u.start_ap.beacon_tail, f_tmp,
        f_data->u.cfg80211.u.start_ap.tail_len);
    f_tmp += f_data->u.cfg80211.u.start_ap.tail_len;

    head = (struct ieee80211_mgmt *)f_data->u.cfg80211.u.start_ap.beacon_head;
    memcpy(f_data->u.cfg80211.u.start_ap.macaddr, head->bssid, sizeof(mac_address_t));

    return;
}

void handle_cfg80211_msg(char *f_tmp, wlan_emu_msg_data_t *f_data)
{
    if ((f_tmp == NULL) || (f_data == NULL)) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: NULL Pointer \n", __func__, __LINE__);
        return;
    }

    memcpy(&(f_data->u.cfg80211.ops), f_tmp, sizeof(wlan_emu_cfg80211_ops_type_t));
    f_tmp += sizeof(wlan_emu_cfg80211_ops_type_t);

    switch (f_data->u.cfg80211.ops) {
    case wlan_emu_cfg80211_ops_type_start_ap:
        handle_cfg80211_start_ap(f_tmp, f_data);
        break;
    default:
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Not handling any other messages\n", __func__,
            __LINE__);
        break;
    }

    return;
}

void handle_frm80211_msg(char *f_tmp, wlan_emu_msg_data_t *f_data)
{
    if ((f_tmp == NULL) || (f_data == NULL)) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: NULL Pointer \n", __func__, __LINE__);
        return;
    }
    memcpy(&(f_data->u.frm80211.ops), f_tmp, sizeof(wlan_emu_frm80211_ops_type_t));
    f_tmp += sizeof(wlan_emu_frm80211_ops_type_t);

    memcpy(&f_data->u.frm80211.u.frame.frame_len, f_tmp, sizeof(unsigned int));
    f_tmp += sizeof(unsigned int);

    f_data->u.frm80211.u.frame.frame = (unsigned char *)malloc(
        f_data->u.frm80211.u.frame.frame_len);
    memset(f_data->u.frm80211.u.frame.frame, 0, f_data->u.frm80211.u.frame.frame_len);
    memcpy(f_data->u.frm80211.u.frame.frame, f_tmp, f_data->u.frm80211.u.frame.frame_len);

    /* //Debug purpose
    char dump_buff[4196];
    unsigned int temp_count = 0;
    unsigned int count = 0;
    memset(dump_buff, 0, sizeof(dump_buff));
    for ( count = 0; count < f_data->u.frm80211.u.frame.frame_len; count++ ) {
        temp_count += snprintf(&dump_buff[temp_count], sizeof(dump_buff)-temp_count, " %02X",
    f_tmp[count]);
    }
    dump_buff[temp_count] = '\0';
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: len  : %d complete buff : %s\n", __func__,
    __LINE__, f_data->u.frm80211.u.frame.frame_len, dump_buff);
    */

    f_tmp += f_data->u.frm80211.u.frame.frame_len;

    memcpy(f_data->u.frm80211.u.frame.macaddr, f_tmp, ETH_ALEN);
    f_tmp += ETH_ALEN;

    memcpy(f_data->u.frm80211.u.frame.client_macaddr, f_tmp, ETH_ALEN);
    f_tmp += ETH_ALEN;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: ops is %d\n", __func__, __LINE__,
        f_data->u.frm80211.ops);
    return;
}

static void handle_emu80211_type_msg_tctrl(char *f_tmp, wlan_emu_msg_data_t *f_data)
{
    if ((f_tmp == NULL) || (f_data == NULL)) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: NULL Pointer f_tmp : %p f_data : %p\n",
            __func__, __LINE__, f_tmp, f_data);
        return;
    }
    memcpy(&(f_data->u.emu80211.u.ctrl.ctrl), f_tmp, sizeof(wlan_emu_emu80211_ctrl_type_t));
    f_tmp += sizeof(wlan_emu_emu80211_ctrl_type_t);

    memcpy(&(f_data->u.emu80211.u.ctrl.coverage), f_tmp, sizeof(wlan_emu_test_coverage_t));
    f_tmp += sizeof(wlan_emu_test_coverage_t);

    memcpy(&(f_data->u.emu80211.u.ctrl.type), f_tmp, sizeof(wlan_emu_test_type_t));
    f_tmp += sizeof(wlan_emu_test_type_t);

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: ctrl : %d coverage : %d type : %d\n", __func__,
        __LINE__, f_data->u.emu80211.u.ctrl.ctrl, f_data->u.emu80211.u.ctrl.coverage,
        f_data->u.emu80211.u.ctrl.type);

    return;
}

static void handle_emu80211_type_msg_close(char *f_tmp, wlan_emu_msg_data_t *f_data)
{
    if ((f_tmp == NULL) || (f_data == NULL)) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: NULL Pointer \n", __func__, __LINE__);
        return;
    }

    memcpy(&(f_data->u.emu80211.u.close.fd), f_tmp, sizeof(int));
    f_tmp += sizeof(int);

    wlan_emu_print(wlan_emu_log_level_err, "%s:%d: fd : %d \n", __func__, __LINE__,
        f_data->u.emu80211.u.close.fd);

    return;
}

void handle_emu80211_msg(char *f_tmp, wlan_emu_msg_data_t *f_data)
{
    if ((f_tmp == NULL) || (f_data == NULL)) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: NULL Pointer f_tmp : %p f_data : %p\n",
            __func__, __LINE__, f_tmp, f_data);
        return;
    }

    memcpy(&(f_data->u.emu80211.ops), f_tmp, sizeof(wlan_emu_emu80211_ops_type_t));
    f_tmp += sizeof(wlan_emu_emu80211_ops_type_t);

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: ops : %d\n", __func__, __LINE__,
        f_data->u.emu80211.ops);

    switch (f_data->u.emu80211.ops) {
    case wlan_emu_emu80211_ops_type_tctrl:
        handle_emu80211_type_msg_tctrl(f_tmp, f_data);
        break;
    case wlan_emu_emu80211_ops_type_close:
        handle_emu80211_type_msg_close(f_tmp, f_data);
        break;
    default:
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Not handling any other messages\n", __func__,
            __LINE__);
        break;
    }

    return;
}

void handle_webconfig_msg(char *f_tmp, wlan_emu_msg_data_t *f_data)
{
    if ((f_tmp == NULL) || (f_data == NULL)) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: NULL Pointer f_tmp : %p f_data : %p\n",
            __func__, __LINE__, f_tmp, f_data);
        return;
    }

    memcpy(&(f_data->u.ow_webconfig.subdoc_type), f_tmp, sizeof(webconfig_subdoc_type_t));
    f_tmp += sizeof(webconfig_subdoc_type_t);

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: ops : %d\n", __func__, __LINE__,
        f_data->u.ow_webconfig.subdoc_type);
}

void handle_agent_msg(char *f_tmp, wlan_emu_msg_data_t *f_data)
{
    void *buf_address;

    if ((f_tmp == NULL) || (f_data == NULL)) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: NULL Pointer f_tmp : %p f_data : %p\n",
            __func__, __LINE__, f_tmp, f_data);
        return;
    }

    memcpy(&(f_data->u.agent_msg.ops), f_tmp, sizeof(wlan_emu_msg_agent_ops_t));
    f_tmp += sizeof(wlan_emu_msg_agent_ops_t);

    if (f_data->u.agent_msg.ops == wlan_emu_msg_agent_ops_type_cmd) {
        memcpy(&(f_data->u.agent_msg.u.cmd), f_tmp, sizeof(wlan_emu_msg_agent_cmd_t));
        f_tmp += sizeof(wlan_emu_msg_agent_cmd_t);
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: cmd : %d\n", __func__, __LINE__,
            f_data->u.agent_msg.u.cmd);
    } else if (f_data->u.agent_msg.ops == wlan_emu_msg_agent_ops_type_data) {
        memcpy(&(buf_address), f_tmp, sizeof(void *));
        f_tmp += sizeof(void *);
        f_data->u.agent_msg.u.buf = buf_address;
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: buf : %p\n", __func__, __LINE__,
            (unsigned char *)f_data->u.agent_msg.u.buf);
    }
}

void wlan_emu_msg_hdlr_t::msg_hdlr_thread_func()
{
    int rc, nbytes;
    struct pollfd pfd;
    wlan_emu_msg_data_t f_data;
    wlan_emu_msg_t *msg;
    char *f_tmp = (char *)malloc(2048);
    char *p_f_tmp;

    if (f_tmp == NULL) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: NULL Pointer Exiting hdlr thread\n",
            __func__, __LINE__);
        return;
    }

    memset(&f_data, 0, sizeof(wlan_emu_msg_data_t));
    memset(f_tmp, 0, 2048);

    while (m_threadExit == false) {
        if ((m_fd == -1) && ((m_fd = open("/dev/rdkfmac_dev", O_RDWR)) < 0)) {
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: failed to open emulator device, err:%d\n", __func__, __LINE__, errno);
            usleep(5000);
            continue;
        }

        pfd.fd = m_fd;
        pfd.events = POLLIN;
        if (poll(&pfd, 1, -1) < 0) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: emulator char device poll error: %d\n",
                __func__, __LINE__, errno);
            continue;
        }

        if ((pfd.revents & POLLIN) == false) {
            continue;
        }

        if ((nbytes = read(m_fd, f_tmp, 2048) == 0)) {
            continue;
        }

        p_f_tmp = f_tmp;

        memcpy(&(f_data.type), p_f_tmp, sizeof(wlan_emu_msg_type_t));
        p_f_tmp += sizeof(wlan_emu_msg_type_t);

        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: f_data.type : %d \n", __func__, __LINE__,
            f_data.type);
        switch (f_data.type) {
        case wlan_emu_msg_type_cfg80211:
            handle_cfg80211_msg(p_f_tmp, &f_data);
            break;
        case wlan_emu_msg_type_emu80211:
            handle_emu80211_msg(p_f_tmp, &f_data);
            break;
        case wlan_emu_msg_type_frm80211:
            handle_frm80211_msg(p_f_tmp, &f_data);
            break;
        case wlan_emu_msg_type_webconfig:
            handle_webconfig_msg(p_f_tmp, &f_data);
            break;
        case wlan_emu_msg_type_agent:
            handle_agent_msg(p_f_tmp, &f_data);
            break;
        default:
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Not handling any other messages\n",
                __func__, __LINE__);
            break;
        }

        msg = new wlan_emu_msg_t;
        msg->update_msg_data(&f_data);

        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: received command: %s\n", __func__, __LINE__,
            msg->get_ops_string_by_msg_type());
        if ((msg->get_msg_type() == wlan_emu_msg_type_emu80211) &&
            (msg->get_emu80211_ops_type() == wlan_emu_emu80211_ops_type_close)) {
            delete msg;
            break;
        }
        queue_msg(msg);
        delete msg;
    }

    close(m_fd);
    m_fd = -1;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: thread exit\n", __func__, __LINE__);
}

void *wlan_emu_msg_hdlr_t::msg_hdlr_thread_func(void *data)
{
    wlan_emu_msg_hdlr_t *obj = (wlan_emu_msg_hdlr_t *)data;

    obj->msg_hdlr_thread_func();
    return NULL;
}

int wlan_emu_msg_hdlr_t::start()
{
    m_threadExit = false;
    if (pthread_create(&m_tid, NULL, wlan_emu_msg_hdlr_t::msg_hdlr_thread_func, this) != 0) {
        return -1;
    }

    pthread_setname_np(m_tid, "msg_hdlr");
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Thread created with Name : msg_hdlr \n",
        __func__, __LINE__);

    return 0;
}

void wlan_emu_msg_hdlr_t::stop()
{
    wlan_emu_msg_data_t spec;

    m_threadExit = true;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: sending close command to emulator char device\n",
        __func__, __LINE__);
    spec.type = wlan_emu_msg_type_emu80211;
    spec.u.emu80211.ops = wlan_emu_emu80211_ops_type_close;

    write(m_fd, &spec, sizeof(wlan_emu_msg_data_t));
}

wlan_emu_msg_hdlr_t::wlan_emu_msg_hdlr_t()
{
    m_threadExit = false;
    m_fd = -1;
}

wlan_emu_msg_hdlr_t::~wlan_emu_msg_hdlr_t()
{
}
