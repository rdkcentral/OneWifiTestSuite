/**
 * Copyright 2025 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// This file should have implementations related to state machine
#include "wlan_ext_emu.h"
#include "wlan_emu_ext_agent_interface.h"
#include "wlan_ext_emu_sta_mgr.h"
#include "wlan_ext_emu_test_step_params.h"
#include "wlan_ext_emu_tests.h"
#include <memory>
#include <curl/curl.h>
#include <linux/netlink.h>
#include <linux/genetlink.h>
#include <linux/nl80211.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netinet/ether.h>
#include <errno.h>
#include <fstream>

#define MAX_NUM_EXTERNAL_CLIENTS 1

extern "C" {
INT wifi_hal_init();
INT wifi_hal_getHalCapability(wifi_hal_capability_t *hal);
void wifi_hal_staConnectionStatus_callback_register(wifi_staConnectionStatus_callback func);
}

using namespace httplib;
class wlan_ext_emu_tests_t;
static wlan_ext_emu_tests_t *ext_emu_test_obj = NULL;

wlan_ext_emu_t *wlan_ext_emu_t::instance = nullptr;

unsigned int wlan_ext_emu_t::m_ext_emu_token = 0xdeadbeef;

void wlan_ext_emu_t::ext_emu_send_signal(wlan_ext_emu_test_sig_type_t sig)
{
    struct sockaddr_un sock;
    memset(&sock, 0, sizeof(struct sockaddr_un));
    sock.sun_family = AF_UNIX;
    strncpy(sock.sun_path, (char *)m_pollables[sig].name, sizeof(sock.sun_path));

    ssize_t bytes_sent = sendto(m_pollables[sig].fd,
        (unsigned char *)&wlan_ext_emu_t::m_ext_emu_token, sizeof(wlan_ext_emu_t::m_ext_emu_token),
        0, (struct sockaddr *)&sock, sizeof(sock));
}

void wlan_ext_emu_t::ext_emu_io_prep()
{
    unsigned int i;
    struct sockaddr_un sock;

    m_num_fds = 0;
    FD_ZERO(&m_fd_set);

    for (i = 0; i < wlan_ext_emu_test_sig_type_max; i++) {
        memset(&sock, 0, sizeof(struct sockaddr_un));
        sock.sun_family = AF_UNIX;
        strncpy(sock.sun_path, (char *)m_pollables[i].name, sizeof(sock.sun_path));
        if (m_pollables[i].fd != 0) {
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: Adding already open pollable: %s, desc: %d to descriptor\n", __func__,
                __LINE__, m_pollables[i].name, m_pollables[i].fd);
            FD_SET(m_pollables[i].fd, &m_fd_set);
            if (m_num_fds < m_pollables[i].fd) {
                m_num_fds = m_pollables[i].fd;
            }
        } else if ((m_pollables[i].fd = socket(AF_UNIX, SOCK_DGRAM, 0)) != -1) {
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: Adding pollable: %s, desc: %d to descriptor\n", __func__, __LINE__,
                m_pollables[i].name, m_pollables[i].fd);
            bind(m_pollables[i].fd, (struct sockaddr *)&sock, sizeof(struct sockaddr_un));
            FD_SET(m_pollables[i].fd, &m_fd_set);
            if (m_num_fds < m_pollables[i].fd) {
                m_num_fds = m_pollables[i].fd;
            }
        } else {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: Failed to open pollable: %s, error: %d\n", __func__, __LINE__,
                m_pollables[i].name, errno);
            assert(0);
        }
    }

    return;
}

wlan_ext_emu_test_sig_type_t wlan_ext_emu_t::ext_emu_io_wait()
{
    int ret;
    unsigned int i;
    unsigned char buff[256] = { 0 };
    wlan_ext_emu_test_sig_type_t type = wlan_ext_emu_test_sig_type_max;
    ssize_t bytes_read = 0;

    ext_emu_io_prep();

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Waiting ... max desc: %d\n", __func__, __LINE__,
        m_num_fds + 1);
    if ((ret = select(m_num_fds + 1, &m_fd_set, NULL, NULL, NULL)) > 0) {
        for (i = 0; i < wlan_ext_emu_test_sig_type_max; i++) {
            if (FD_ISSET(m_pollables[i].fd, &m_fd_set)) {
                wlan_emu_print(wlan_emu_log_level_info, "%s:%d: Descriptor: %s signalled\n",
                    __func__, __LINE__, m_pollables[i].name);
                type = (wlan_ext_emu_test_sig_type_t)i;
                bytes_read = read(m_pollables[i].fd, (unsigned char *)buff, sizeof(buff));
                break;
            }
        }
    }

    return type;
}

void wlan_ext_emu_t::ext_emu_update_io_wait_to_state(wlan_ext_emu_test_sig_type_t val)
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Received signal : %d\n", __func__, __LINE__,
        val);
    switch (val) {
    case wlan_ext_emu_test_sig_type_idle:
        set_agent_test_state(ext_agent_test_state_idle);
        break;
    case wlan_ext_emu_test_sig_type_ready:
        set_agent_test_state(ext_agent_test_state_ready);
        break;
    case wlan_ext_emu_test_sig_type_running:
        set_agent_test_state(ext_agent_test_state_running);
        break;
    case wlan_ext_emu_test_sig_type_complete:
        set_agent_test_state(ext_agent_test_state_complete);
        break;
    case wlan_ext_emu_test_sig_type_fail:
        set_agent_test_state(ext_agent_test_state_fail);
        break;
    default:
        set_agent_test_state(ext_agent_test_state_idle);
        break;
    }
}

int wlan_ext_emu_t::ext_emu_state_machine()
{
    bool exit = false;
    while (exit == false) {
        switch (get_agent_test_state()) {
        case ext_agent_test_state_idle:
            if (ext_emu_test_obj != NULL) {
                delete ext_emu_test_obj;
                m_ext_msg_mgr.stop();
                ext_emu_test_obj = NULL;
            }
            break;
        case ext_agent_test_state_ready:
            send_agent_msg_start();
            break;
        case ext_agent_test_state_running:
            break;
        case ext_agent_test_state_complete:
            break;
        case ext_agent_test_state_fail:
            break;
        default:
            break;
        }
        ext_emu_update_io_wait_to_state(ext_emu_io_wait());
    }
    return RETURN_OK;
}

void wlan_ext_emu_t::agent_endpoint_capability(const httplib::Request &req, httplib::Response &res)
{
    std::string cap_json_string;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d:\n", __func__, __LINE__);

    // list_interfaces_in_namespace();

    if (m_ext_agent_interface.encode_external_agent_capability(cap_json_string) == RETURN_ERR) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: encode for external client\n", __func__,
            __LINE__);
        http_report_bad_request(req, res);
        return;
    }

    http_report_status_ok(req, res, cap_json_string);

    return;
}

void wlan_ext_emu_t::agent_endpoint_status(const httplib::Request &req, httplib::Response &res)
{
    std::string status_str;

    status_str = get_agent_step_status_json_str();
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: status : %s\n", __func__, __LINE__,
        status_str.c_str());

    http_report_status_ok(req, res, status_str);

    return;
}

void wlan_ext_emu_t::agent_endpoint_results(const httplib::Request &req, httplib::Response &res)
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d:\n", __func__, __LINE__);

    std::string filename = req.matches[1];
    std::string filepath = filename;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Requested file : %s\n", __func__, __LINE__,
        filename.c_str());

    std::ifstream file(filename, std::ios::binary);
    if (file) {
        std::ostringstream buffer;
        buffer << file.rdbuf();
        if (file.fail()) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: File : %s reading failed\n", __func__,
                __LINE__, filename.c_str());
            return;
        }
        file.close();
        res.set_content(buffer.str(), "application/octet-stream");
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Requested file : %s not found\n", __func__,
            __LINE__, filename.c_str());
        http_report_internal_svr_err(req, res);
    }

    return;
}

int wlan_ext_emu_t::send_agent_msg_start()
{
    int fd;
    ssize_t sz;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d:\n", __func__, __LINE__);

    wlan_emu_msg_data_t m_msg;

    memset(&m_msg, 0, sizeof(wlan_emu_msg_data_t));

    m_msg.type = wlan_emu_msg_type_agent;
    m_msg.u.agent_msg.ops = wlan_emu_msg_agent_ops_type_cmd;
    m_msg.u.agent_msg.u.cmd = wlan_emu_msg_agent_cmd_type_start;

    if ((fd = open("/dev/rdkfmac_dev", O_RDWR)) < 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to open char dev, err: %d\n",
            __func__, __LINE__, errno);
        return RETURN_ERR;
    }

    if ((sz = write(fd, &m_msg, sizeof(wlan_emu_msg_data_t))) < 0) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Failed to write to char dev, err: %d\n",
            __func__, __LINE__, errno);
    }

    close(fd);
    return RETURN_OK;
}

int wlan_ext_emu_t::send_agent_msg_stop()
{
    int fd;
    ssize_t sz;

    wlan_emu_msg_data_t m_msg;

    memset(&m_msg, 0, sizeof(wlan_emu_msg_data_t));

    m_msg.type = wlan_emu_msg_type_agent;
    m_msg.u.agent_msg.ops = wlan_emu_msg_agent_ops_type_cmd;
    m_msg.u.agent_msg.u.cmd = wlan_emu_msg_agent_cmd_type_stop;

    if ((fd = open("/dev/rdkfmac_dev", O_RDWR)) < 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to open char dev, err: %d\n",
            __func__, __LINE__, errno);
        return RETURN_ERR;
    }

    if ((sz = write(fd, &m_msg, sizeof(wlan_emu_msg_data_t))) < 0) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Failed to write to char dev, err: %d\n",
            __func__, __LINE__, errno);
    }

    close(fd);
    return RETURN_OK;
}

int wlan_ext_emu_t::send_agent_msg_buffer(void *buff_ptr)
{
    int fd;
    ssize_t sz;

    wlan_emu_msg_data_t m_msg;

    memset(&m_msg, 0, sizeof(wlan_emu_msg_data_t));

    m_msg.type = wlan_emu_msg_type_agent;
    m_msg.u.agent_msg.ops = wlan_emu_msg_agent_ops_type_data;
    m_msg.u.agent_msg.u.buf = buff_ptr;

    if ((fd = open("/dev/rdkfmac_dev", O_RDWR)) < 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to open char dev, err: %d\n",
            __func__, __LINE__, errno);
        return RETURN_ERR;
    }

    if ((sz = write(fd, &m_msg, sizeof(wlan_emu_msg_data_t))) < 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to write to char dev, err: %d\n",
            __func__, __LINE__, errno);
        close(fd);
        return RETURN_ERR;
    }

    close(fd);
    return RETURN_OK;
}

wlan_ext_test_step_params_t *wlan_ext_emu_t::get_ext_step_from_step_number(int step_number)
{
    unsigned int count = 0;
    int i = 0;
    wlan_ext_test_step_params_t *tmp_step = NULL;

    if (test_steps_q == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: test_steps_q is NULL\n", __func__, __LINE__);
        return NULL;
    }

    count = queue_count(test_steps_q);
    if (count == 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: step queue count is 0\n", __func__,
            __LINE__);
        return NULL;
    }
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step queue count is %d\n", __func__, __LINE__,
        count);

    for (i = 0; i < count; i++) {
        tmp_step = (wlan_ext_test_step_params_t *)queue_peek(test_steps_q, i);
        if (tmp_step == NULL) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: step is null at count %d\n", __func__,
                __LINE__, i);
            return NULL;
        }

        if (tmp_step->step_number == step_number) {
            return tmp_step;
        }
    }

    return NULL;
}

int wlan_ext_emu_t::send_agent_wifi_sta_notification(wlan_emu_msg_type_ext_wifi_sta_notif_t *notif)
{
    int fd;
    ssize_t sz;

    wlan_emu_msg_data_t m_msg;

    memset(&m_msg, 0, sizeof(wlan_emu_msg_data_t));

    m_msg.type = wlan_emu_msg_type_agent;
    m_msg.u.agent_msg.ops = wlan_emu_msg_agent_ops_type_notification;
    m_msg.u.agent_msg.u.agent_notif.sub_ops_type =
        wlan_msg_ext_agent_ops_sub_type_wifi_notification;
    memcpy(&m_msg.u.agent_msg.u.agent_notif.u.wifi_sta_notif, notif,
        sizeof(wlan_emu_msg_type_ext_wifi_sta_notif_t));

    if ((fd = open("/dev/rdkfmac_dev", O_RDWR)) < 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to open char dev, err: %d\n",
            __func__, __LINE__, errno);
        return RETURN_ERR;
    }

    if ((sz = write(fd, &m_msg, sizeof(wlan_emu_msg_data_t))) < 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to write to char dev, err: %d\n",
            __func__, __LINE__, errno);
        close(fd);
        return RETURN_ERR;
    }

    close(fd);
    return RETURN_OK;
}

int wlan_ext_emu_t::agent_start()
{

    if (ext_emu_test_obj != NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Already in start state\n", __func__,
            __LINE__);
        return RETURN_OK;
    }

    m_ext_msg_mgr.start();
    // initialization of queues
    ext_emu_test_obj = new wlan_ext_emu_tests_t(&m_ext_msg_mgr, this, &m_agent_sta_mgr);
    if (ext_emu_test_obj->get_agent_test_initialized() == false) {
        return RETURN_ERR;
    }
    return RETURN_OK;
}

int wlan_ext_emu_t::agent_stop()
{
    send_agent_msg_stop();
    /*
        delete ext_emu_test_obj;
        m_ext_msg_mgr.stop();
        ext_emu_test_obj = NULL;
        */
    return RETURN_OK;
}

void wlan_ext_emu_t::http_report_bad_request(const httplib::Request &req, httplib::Response &res)
{
    std::string msg;
    std::string descript = ": bad request";
    msg = m_ext_agent_interface.agent_hostname + descript;

    res.status = BadRequest_400;
    res.set_content(msg, "text/plain");
}

void wlan_ext_emu_t::http_report_status_ok(const httplib::Request &req, httplib::Response &res,
    std::string msg)
{
    res.status = OK_200;
    res.set_content(msg, "text/plain");
}

void wlan_ext_emu_t::http_report_internal_svr_err(const httplib::Request &req,
    httplib::Response &res)
{
    std::string msg;
    std::string descript = ": Internal server error";
    msg = m_ext_agent_interface.agent_hostname + descript;

    res.status = InternalServerError_500;
    res.set_content(msg, "text/plain");
}

void wlan_ext_emu_t::agent_endpoint_command(const httplib::Request &req, httplib::Response &res)
{
    // start and stop
    wlan_ext_emu_t *ext_emu = this;
    std::string data = req.body;
    std::string msg;
    //    std::string steps_status_json_str_src;
    std::string descript = ": status ok";
    msg = m_ext_agent_interface.agent_hostname + descript;
    char *steps_status_json_str_src = NULL;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d:\n", __func__, __LINE__);

    // As of now assuming to be just start string.
    if (data.empty()) {
        http_report_bad_request(req, res);
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: empty data\n", __func__, __LINE__);
        return;
    }

    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: Received input %s\n", __func__, __LINE__,
        data.c_str());
    if (data.compare("start") == 0) {
        if (agent_start() != RETURN_OK) {
            http_report_bad_request(req, res);
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: agent_start failed\n", __func__,
                __LINE__);
            return;
        }
        http_report_status_ok(req, res, msg);
    } else if (data.compare("stop") == 0) {

        if (get_agent_step_status_json_str().empty()) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d:\n", __func__, __LINE__);
            return;
        }
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d:\n", __func__, __LINE__);

        if (agent_stop() != RETURN_OK) {
            http_report_bad_request(req, res);
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: agent_stop failed\n", __func__,
                __LINE__);
            return;
        }
        cJSON *agent_status = cJSON_CreateObject();
        wlan_emu_ext_agent_interface_t *agent_interface;
        agent_interface = ext_emu->get_agent_interface();
        cJSON_AddStringToObject(agent_status, "ExternalAgentHostName",
            agent_interface->agent_hostname.c_str());
        cJSON_AddNumberToObject(agent_status, "ExternalAgentStepCount", 0);
        cJSON *steps_status_arr = cJSON_CreateArray();
        cJSON_AddItemToObject(agent_status, "ExternalAgentStepStatus", steps_status_arr);
        cJSON_AddStringToObject(agent_status, "ExternalAgentStatus",
            agent_interface->agent_state_as_string(ext_agent_test_state_ready).c_str());
        steps_status_json_str_src = cJSON_Print(agent_status);
        ext_emu->set_agent_step_status_json_str(steps_status_json_str_src);
        cJSON_free(steps_status_json_str_src);
        cJSON_free(agent_status);

        http_report_status_ok(req, res, msg);
    } else {
        http_report_bad_request(req, res);
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid incoming request\n", __func__,
            __LINE__);
    }

    return;
}

int wlan_ext_emu_t::handle_file_post_request(const httplib::Request &req, httplib::Response &res,
    std::string &out_file)
{
    auto file = req.body;
    std::string directory(get_ext_emu_test_results_dir_path());
    std::string filename = req.matches[1];
    std::string file_str = directory + filename;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Saving file %s\n", __func__, __LINE__,
        file_str.c_str());

    std::ofstream ofs(file_str, std::ios::binary);
    if (!ofs) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: saving file %s failed due to %s\n", __func__,
            __LINE__, file_str.c_str(), std::string(std::strerror(errno)));
        return RETURN_ERR;
    }

    ofs.write(file.data(), file.size());
    ofs.close();

    out_file = file_str;

    return RETURN_OK;
}

void wlan_ext_emu_t::agent_endpoint_reffiles(const httplib::Request &req, httplib::Response &res)
{

    std::string out_file;
    std::string msg;
    std::string descript = ": status ok";
    msg = m_ext_agent_interface.agent_hostname + descript;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d:\n", __func__, __LINE__);

    // should be at the end least priority
    if (handle_file_post_request(req, res, out_file) != RETURN_OK) {
        http_report_internal_svr_err(req, res);
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid request\n", __func__, __LINE__);
        return;
    }
    http_report_status_ok(req, res, msg);
    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: file %s saved\n", __func__, __LINE__,
        out_file.c_str());

    return;
}

int wlan_ext_emu_t::decode_step_param_common(cJSON *root_json,
    wlan_ext_test_step_params_t *step_config)
{
    cJSON *param = NULL;
    cJSON *frame_type_list = NULL;
    cJSON *frame_type = NULL;
    cJSON *frame = NULL;
    decode_param_integer(root_json, "StepNumber", param);
    step_config->step_number = param->valueint;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step number : %d\n", __func__, __LINE__,
        step_config->step_number);

    decode_param_string(root_json, "TestCaseName", param);
    copy_string(step_config->test_case_name, param->valuestring,
        sizeof(step_config->test_case_name) - 1);

    decode_param_string(root_json, "TestCaseID", param);
    copy_string(step_config->test_case_id, param->valuestring,
        sizeof(step_config->test_case_id) - 1);

    decode_param_bool(root_json, "Fork", param);
    step_config->fork = (param->type & cJSON_True) ? true : false;

    memset(&step_config->frame_request, 0, sizeof(step_config->frame_request));
    param = cJSON_GetObjectItem(root_json, "TestCapture");

    if (param != NULL && cJSON_IsBool(param)) {
        step_config->capture_frames = (param->type & cJSON_True) ? true : false;
        if (step_config->capture_frames == true) {
            if ((frame_type_list = cJSON_GetObjectItem(root_json, "TestCaptureFrame")) != NULL) {
                cJSON_ArrayForEach(frame_type, frame_type_list) {
                    frame = cJSON_GetObjectItem(frame_type, "FrameType");
                    if (wlan_common_utils::decode_pcap_frame_type(frame->valuestring,
                            &step_config->frame_request) != RETURN_OK) {
                        wlan_emu_print(wlan_emu_log_level_err,
                            "%s:%d Unable to decode the pcap frametype : %s\n", __func__, __LINE__,
                            param->valuestring);
                        return RETURN_ERR;
                    }
                }
            }
        }
    }

    return RETURN_OK;
}

int wlan_ext_emu_t::decode_step_param_ext_sta_management(cJSON *root_json,
    wlan_ext_test_step_params_t *step_config)
{
    cJSON *param = NULL;

    if (decode_step_param_common(root_json, step_config) == RETURN_ERR) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: decode step decode common failed", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    if (step_config->wlan_ext_step_decode_subdocument(root_json, step_config) == RETURN_ERR) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: decode step sta_management failed", __func__,
            __LINE__);
        return RETURN_ERR;
    }
    return RETURN_OK;
}

int wlan_ext_emu_t::decode_step_param_ext_sta_iperf_server(cJSON *root_json,
    wlan_ext_test_step_params_t *step_config)
{
    cJSON *param = NULL;

    if (decode_step_param_common(root_json, step_config) == RETURN_ERR) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: decode step decode common failed", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    if (step_config->wlan_ext_step_decode_subdocument(root_json, step_config) == RETURN_ERR) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: decode step iperf server failed", __func__,
            __LINE__);
        return RETURN_ERR;
    }
    return RETURN_OK;
}

int wlan_ext_emu_t::decode_step_param_ext_sta_iperf_client(cJSON *root_json,
    wlan_ext_test_step_params_t *step_config)
{
    cJSON *param = NULL;

    if (decode_step_param_common(root_json, step_config) == RETURN_ERR) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: decode step decode common failed", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    if (step_config->wlan_ext_step_decode_subdocument(root_json, step_config) == RETURN_ERR) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: decode step iperf client failed", __func__,
            __LINE__);
        return RETURN_ERR;
    }
    return RETURN_OK;
}

int wlan_ext_emu_t::decode_step_param_ext_ethernet_client(cJSON *root_json,
    wlan_ext_test_step_params_t *step_config)
{
    cJSON *param = NULL;

    if (decode_step_param_common(root_json, step_config) == RETURN_ERR) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: decode step decode common failed", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    if (step_config->wlan_ext_step_decode_subdocument(root_json, step_config) == RETURN_ERR) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: decode step ethernet client failed",
            __func__, __LINE__);
        return RETURN_ERR;
    }
    return RETURN_OK;
}

int wlan_ext_emu_t::decode_step_param(std::string &cjson_str,
    wlan_ext_test_step_params_t **step_config)
{
    cJSON *root_json = NULL;
    cJSON *param = NULL;

    root_json = cJSON_Parse(cjson_str.c_str());
    if (root_json == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: NULL json pointer : %s\n", __func__,
            __LINE__, cjson_str.c_str());
        return RETURN_ERR;
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d:\n", __func__, __LINE__);
    decode_param_string(root_json, "SubDocName", param);
    if (strcmp(param->valuestring, "ExternalStationManagement") == 0) {
        *step_config = new (std::nothrow) wlan_ext_test_step_param_sta_management;
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: *step_config : %p\n", __func__, __LINE__,
            *step_config);
        if (decode_step_param_ext_sta_management(root_json, *step_config) == RETURN_ERR) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: decode sta_management failed", __func__,
                __LINE__);
            cJSON_free(root_json);
            return RETURN_ERR;
        }
    } else if (strcmp(param->valuestring, "ExternalIperfServer") == 0) {
        *step_config = new (std::nothrow) wlan_ext_test_step_param_iperf_server;
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: *step_config : %p\n", __func__, __LINE__,
            *step_config);
        if (decode_step_param_ext_sta_iperf_server(root_json, *step_config) == RETURN_ERR) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: decode iperf server failed", __func__,
                __LINE__);
            cJSON_free(root_json);
            return RETURN_ERR;
        }
    } else if (strcmp(param->valuestring, "ExternalIperfClient") == 0) {
        *step_config = new (std::nothrow) wlan_ext_test_step_param_iperf_client;
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: *step_config : %p\n", __func__, __LINE__,
            *step_config);
        if (decode_step_param_ext_sta_iperf_client(root_json, *step_config) == RETURN_ERR) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: decode iperf client failed", __func__,
                __LINE__);
            cJSON_free(root_json);
            return RETURN_ERR;
        }
    } else if (strcmp(param->valuestring, "EthernetLanClient") == 0) {
        *step_config = new (std::nothrow) wlan_ext_test_step_param_eth_lan_client;
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: *step_config : %p\n", __func__, __LINE__,
            *step_config);
        if (decode_step_param_ext_ethernet_client(root_json, *step_config) == RETURN_ERR) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: decode ethernet client failed", __func__,
                __LINE__);
            cJSON_free(root_json);
            return RETURN_ERR;
        }
    }

    cJSON_free(root_json);

    return RETURN_OK;
}

void wlan_ext_emu_t::agent_endpoint_testconfig(const httplib::Request &req, httplib::Response &res)
{
    std::string out_file;
    std::stringstream buffer;
    std::ifstream file;
    std::string cjson_str;
    wlan_ext_test_step_params_t *step_param;

    std::string msg;
    std::string descript = ": status ok";
    msg = m_ext_agent_interface.agent_hostname + descript;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d:\n", __func__, __LINE__);

    // should be at the end least priority
    if (handle_file_post_request(req, res, out_file) != RETURN_OK) {
        http_report_internal_svr_err(req, res);
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid request\n", __func__, __LINE__);
        return;
    }
    http_report_status_ok(req, res, msg);
    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: file %s saved\n", __func__, __LINE__,
        out_file.c_str());

    // can use read_config_file(), but that code needs to be moved to common
    file.open(out_file);
    if (!file) {
        http_report_internal_svr_err(req, res);
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid request\n", __func__, __LINE__);
        return;
    }

    buffer << file.rdbuf();
    cjson_str = buffer.str();

    file.close();

    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: cjson_str : %s\n", __func__, __LINE__,
        cjson_str.c_str());
    if (decode_step_param(cjson_str, &step_param) == RETURN_ERR) {
        http_report_bad_request(req, res);
        return;
    }

    send_agent_msg_buffer(static_cast<void *>(step_param));
    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: address : %p\n", __func__, __LINE__,
        step_param);

    return;
}

void *wlan_ext_emu_t::http_server_handler(void *arg)
{
    wlan_ext_emu_t *ext_emu = (wlan_ext_emu_t *)arg;
    Server svr;

    svr.Get("/Capability", [&](const Request &req, Response &res) {
        /*get*/
        ext_emu->enter_namespace("/var/run/netns/default");
        ext_emu->agent_endpoint_capability(req, res);
    });

    svr.Get("/Status", [&](const Request &req, Response &res) {
        /*get*/
        ext_emu->enter_namespace("/var/run/netns/default");
        ext_emu->agent_endpoint_status(req, res);
    });

    svr.Get("/Results/(.+)", [&](const Request &req, Response &res) {
        /*get*/
        ext_emu->enter_namespace("/var/run/netns/default");
        ext_emu->agent_endpoint_results(req, res);
    });

    svr.Post("/Command", [&](const Request &req, Response &res) {
        /*post*/
        ext_emu->enter_namespace("/var/run/netns/default");
        ext_emu->agent_endpoint_command(req, res);
    });

    svr.Post("/Reffiles/(.+)", [&](const Request &req, Response &res) {
        /*post*/
        ext_emu->enter_namespace("/var/run/netns/default");
        ext_emu->agent_endpoint_reffiles(req, res);
    });

    svr.Post("/Testconfig/(.+)", [&](const Request &req, Response &res) {
        /*post*/
        ext_emu->enter_namespace("/var/run/netns/default");
        ext_emu->agent_endpoint_testconfig(req, res);
    });

    //	switch_to_namespace("/var/run/netns/ots", &original_ns_fd);
    ext_emu->enter_namespace("/var/run/netns/ots");
    svr.listen("0.0.0.0", 1234);
    return NULL;
}

int wlan_ext_emu_t::get_interface_mac_from_ifindex(int ifindex, unsigned char *mac)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (fd < 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: socket failed for index %d\n", __func__,
            __LINE__, ifindex);
        return RETURN_ERR;
    }

    struct ifreq ifr = { 0 };
    if_indextoname(ifindex, ifr.ifr_name);

    if (ioctl(fd, SIOCGIFHWADDR, &ifr) == 0) {
        memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
        wlan_emu_print(wlan_emu_log_level_dbg,
            "interace MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3],
            mac[4], mac[5]);
        close(fd);
        return RETURN_OK;
    }

    wlan_emu_print(wlan_emu_log_level_err, "%s:%d: ioctl failed for index : %d\n", __func__,
        __LINE__, ifindex);
    close(fd);
    return RETURN_ERR;
}

int wlan_ext_emu_t::sta_connection_status(int ap_index, wifi_bss_info_t *bss_dev,
    wifi_station_stats_t *sta)
{

    wlan_ext_emu_t *ext_emu = this;
    unsigned char sta_mac[ETH_ALEN] = { 0 };
    char sta_mac_str[32] = { 0 };
    char bssid_mac_str[32] = { 0 };
    wlan_emu_msg_type_ext_wifi_sta_notif_t sta_notif;

    memset(&sta_notif, 0, sizeof(wlan_emu_msg_type_ext_wifi_sta_notif_t));

    if ((bss_dev == NULL) || (sta == NULL)) {
        wlan_emu_print(wlan_emu_log_level_info, "%s:%d: NULL arguements bss_dev : %p  sta : %p\n",
            __func__, __LINE__, bss_dev, sta);
        return RETURN_ERR;
    }
    uint8_mac_to_string_mac(bss_dev->bssid, bssid_mac_str);
    uint8_mac_to_string_mac(sta_mac, sta_mac_str);
    if (ext_emu->get_interface_mac_from_ifindex(sta->vap_index, sta_mac) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_info, "%s:%d: Unable to get vap_index for mac : %s\n",
            __func__, __LINE__, sta_mac_str);
        return RETURN_ERR;
    }
    wlan_emu_print(wlan_emu_log_level_dbg,
        "%s:%d: apIndex : %d vap_index : %d sta_mac_str : %s bssid_mac_str : %s\n", __func__,
        __LINE__, ap_index, sta->vap_index, sta_mac_str, bssid_mac_str);

    if (sta->connect_status == wifi_connection_status_connected) {
        wlan_emu_print(wlan_emu_log_level_info,
            "%s:%d: connected for apIndex : %d vap_index : %d\n", __func__, __LINE__, ap_index,
            sta->vap_index);
        sta_notif.sta_state = wlan_emu_ext_wifi_sta_state_connected;
        memcpy(sta_notif.sta_mac_addr, sta_mac, ETH_ALEN);
        memcpy(sta_notif.bssid_mac_addr, bss_dev->bssid, ETH_ALEN);
        ext_emu->send_agent_wifi_sta_notification(&sta_notif);

    } else if (sta->connect_status == wifi_connection_status_disconnected) {
        wlan_emu_print(wlan_emu_log_level_info,
            "%s:%d: disconnect for apIndex : %d vap_index : %d\n", __func__, __LINE__, ap_index,
            sta->vap_index);
        sta_notif.sta_state = wlan_emu_ext_wifi_sta_state_connected;
        memcpy(sta_notif.sta_mac_addr, sta_mac, ETH_ALEN);
        memcpy(sta_notif.bssid_mac_addr, bss_dev->bssid, ETH_ALEN);
        ext_emu->send_agent_wifi_sta_notification(&sta_notif);
    }

    return RETURN_OK;
}

int wlan_ext_emu_t::init()
{
    unsigned int i;
    if (m_ext_agent_interface.agent_init() == RETURN_ERR) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: agent initilization failed\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }
    wifi_interface_name_idex_map_t *if_map = NULL;
    wifi_hal_capability_t *sta_cap = NULL;
    queue_t *temp_sta_info_map;
    sta_info_t *temp_sta_info;

    eth_dev_info_t *eth_dev;

    hash_map_t *temp_eth_cli_map;

    m_dir_path = "/tmp/";
    set_ext_emu_test_results_dir_path("/tmp/cci_res/");

    if (mkdir(get_ext_emu_test_results_dir_path().c_str(), 0777) == 0) {
        wlan_emu_print(wlan_emu_log_level_info, "%s:%d: Test results directory at %s\n", __func__,
            __LINE__, get_ext_emu_test_results_dir_path().c_str());
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: creation of directory failed : %s - %s\n",
            __func__, __LINE__, get_ext_emu_test_results_dir_path().c_str(), strerror(errno));
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    static std::shared_ptr<void> curl_cleanup_guard(nullptr, [](void *) { curl_global_cleanup(); });

    snprintf(m_pollables[wlan_ext_emu_test_sig_type_idle].name,
        sizeof(m_pollables[wlan_ext_emu_test_sig_type_idle].name), "%s/ext_idle",
        m_dir_path.c_str());
    snprintf(m_pollables[wlan_ext_emu_test_sig_type_ready].name,
        sizeof(m_pollables[wlan_ext_emu_test_sig_type_ready].name), "%s/ext_ready",
        m_dir_path.c_str());
    snprintf(m_pollables[wlan_ext_emu_test_sig_type_running].name,
        sizeof(m_pollables[wlan_ext_emu_test_sig_type_running].name), "%s/ext_running",
        m_dir_path.c_str());
    snprintf(m_pollables[wlan_ext_emu_test_sig_type_complete].name,
        sizeof(m_pollables[wlan_ext_emu_test_sig_type_complete].name), "%s/ext_complete",
        m_dir_path.c_str());
    snprintf(m_pollables[wlan_ext_emu_test_sig_type_fail].name,
        sizeof(m_pollables[wlan_ext_emu_test_sig_type_fail].name), "%s/ext_fail",
        m_dir_path.c_str());

    for (i = 0; i < wlan_ext_emu_test_sig_type_max; i++) {
        unlink(m_pollables[i].name);
        m_pollables[i].fd = 0;
    }

    sta_cap = (wifi_hal_capability_t *)malloc(sizeof(wifi_hal_capability_t));
    if (sta_cap == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: malloc failed for sta_cap\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    memset(sta_cap, 0, sizeof(wifi_hal_capability_t));
    if (wifi_hal_init() != 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: hal init problem\n", __func__, __LINE__);
        free(sta_cap);
        return RETURN_ERR;
    } else if (wifi_hal_getHalCapability(sta_cap) != 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: platform capability get failed\n", __func__,
            __LINE__);
        free(sta_cap);
        return RETURN_ERR;
    }

    if (m_agent_sta_mgr.init(sta_cap) == RETURN_ERR) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: sta_mgr init failed\n", __func__, __LINE__);
        free(if_map);
        free(sta_cap);
        return RETURN_ERR;
    }
    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: wifi hal init done\n", __func__, __LINE__);

    m_ext_agent_interface.total_supported_clients = m_agent_sta_mgr.get_sta_info_count();
    temp_sta_info_map = m_agent_sta_mgr.get_sta_info_map();

    for (i = 0; i < m_ext_agent_interface.total_supported_clients; i++) {
        temp_sta_info = (sta_info_t *)queue_peek(temp_sta_info_map, i);
        queue_push(m_ext_agent_interface.client_info, temp_sta_info);
    }

    if (m_agent_sta_mgr.init_eth_interfaces() == RETURN_ERR) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: init_eth_interfaces init failed\n", __func__,
            __LINE__);
        free(if_map);
        free(sta_cap);
        return RETURN_ERR;
    }

    m_ext_agent_interface.total_supported_eth_clients = m_agent_sta_mgr.get_eth_cli_info_count();
    wlan_emu_print(wlan_emu_log_level_info, "%s:%d: supported eth clients : %d \n", __func__,
        __LINE__, m_ext_agent_interface.total_supported_eth_clients);

    temp_eth_cli_map = m_agent_sta_mgr.get_eth_map();

    eth_dev = (eth_dev_info_t *)hash_map_get_first(temp_eth_cli_map);
    while (eth_dev != NULL) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: eth_dev : state=%d, name=%s, ip=%s\n",
            __func__, __LINE__, eth_dev->state, eth_dev->interface_name.c_str(),
            eth_dev->ip_address.c_str());
        queue_push(m_ext_agent_interface.eth_client_interfaces, eth_dev);

        eth_dev = (eth_dev_info_t *)hash_map_get_next(temp_eth_cli_map, eth_dev);
    }

    instance = this;

    wifi_hal_staConnectionStatus_callback_register(sta_connection_status_wrapper);

    // initialize the state
    set_agent_test_state(ext_agent_test_state_idle);

    // start the thread for handling of the http requests
    if (pthread_create(&m_http_server_tid, NULL, wlan_ext_emu_t::http_server_handler, this) != 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Thread create error for http_server\n",
            __func__, __LINE__);
        return -1;
    }

    pthread_setname_np(m_http_server_tid, "http_server");
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Thread created with Name : http_server\n",
        __func__, __LINE__);

    if (pthread_detach(m_http_server_tid)) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: thread detach failed for http_server_tid\n",
            __func__, __LINE__);
        return RETURN_ERR;
    }

    ext_emu_state_machine();

    return RETURN_OK;
}

wlan_ext_emu_t::wlan_ext_emu_t()
{
}

wlan_ext_emu_t::~wlan_ext_emu_t()
{
    m_ext_agent_interface.agent_exit();
}
