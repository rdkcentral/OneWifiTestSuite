#include "wlan_emu_log.h"
#include "wlan_emu_test_params.h"
#include "common/ieee802_11_defs.h"
#include "wlan_common_utils.h"
#include "wlan_emu_common.h"
#include "wlan_emu_err_code.h"
#include "radiotap_iter.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <pcap.h>
#include <string>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

static const uint8_t u8aRadiotapHeader[] = {
    0x00, 0x00, // version + pad
    0x13, 0x00, // size
    0x6e, 0x48, 0x00, 0x00, // preset flags
    0x00, // flags
    0x00, // data rate
    0x00, 0x00, // channel
    0x00, 0x00, // chan flags
    0x00, // signal
    0x00, // noise
    0x01, // antena
    0x00, 0x00, // rx flags
};

bool is_assoc_req(uint16_t frame_control)
{

    uint16_t type = frame_control & 0x000c; // Bits 2 and 3 (type mask)
    uint16_t subtype = frame_control & 0x00f0; // Bits 4–7 (subtype mask)

    return (type == 0x0000) && (subtype == 0x0000);
}

bool is_probe_req(uint16_t frame_control)
{
    uint16_t type = frame_control & 0x000c; // Bits 2 and 3
    uint16_t subtype = frame_control & 0x00f0; // Bits 4–7

    return (type == 0x0000) && (subtype == 0x0040); // subtype 0x40 = 4 << 4
}

int construct_assoc_request(struct ieee80211_mgmt **mgmt_ptr, size_t *mgmt_len,
    const char *new_ssid)
{
    if (mgmt_ptr == NULL || (*mgmt_ptr) == NULL || mgmt_len == 0 || new_ssid == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Null pointer argument\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    struct ieee80211_mgmt *mgmt = *mgmt_ptr;

    size_t new_ssid_len = strlen(new_ssid);
    if (new_ssid_len > WIFI_AP_MAX_SSID_LEN) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: SSID length exceeds maximum allowed (%zu)\n",
            __func__, __LINE__, new_ssid_len);
        return RETURN_ERR;
    }

    size_t offset = offsetof(struct ieee80211_mgmt, u.assoc_req.variable);
    if (*mgmt_len < offset) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: mgmt_len (%zu) less than offset (%zu)\n",
            __func__, __LINE__, *mgmt_len, offset);
        return RETURN_ERR;
    }

    size_t remaining_len = *mgmt_len - offset;
    uint8_t *ie_ptr = mgmt->u.assoc_req.variable;

    size_t cursor = 0;
    int ssid_found = 0;

    // Find SSID IE (Element ID 0x00)
    while (cursor + 2 <= remaining_len) {
        uint8_t eid = ie_ptr[cursor];
        uint8_t len = ie_ptr[cursor + 1];

        if (cursor + 2 + len > remaining_len) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: IE length exceeds buffer, malformed frame\n", __func__, __LINE__);
            return RETURN_ERR;
        }

        if (eid == 0x00) {
            ssid_found = 1;
            break;
        }

        cursor += 2 + len;
    }

    if (ssid_found == 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: SSID IE not found\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    size_t original_ssid_ie_len = 2 + ie_ptr[cursor + 1];
    size_t new_ie_len = 2 + new_ssid_len;
    size_t new_mgmt_len = *mgmt_len - original_ssid_ie_len + new_ie_len;

    uint8_t *updated_mgmt_bytes = (uint8_t *)malloc(new_mgmt_len);
    if (updated_mgmt_bytes == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Memory allocation failed\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    // Copy data before SSID IE
    memcpy(updated_mgmt_bytes, mgmt, offset + cursor);

    // Insert new SSID IE
    updated_mgmt_bytes[offset + cursor] = 0x00;
    updated_mgmt_bytes[offset + cursor + 1] = (uint8_t)new_ssid_len;
    memcpy(updated_mgmt_bytes + offset + cursor + 2, new_ssid, new_ssid_len);

    // Copy remaining IEs
    size_t tail_start = cursor + original_ssid_ie_len;
    size_t tail_len = remaining_len - tail_start;
    memcpy(updated_mgmt_bytes + offset + cursor + 2 + new_ssid_len, ie_ptr + tail_start, tail_len);

    // Reallocate and update
    free(*mgmt_ptr);
    *mgmt_ptr = (struct ieee80211_mgmt *)calloc(1, new_mgmt_len);
    if (*mgmt_ptr == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Memory allocation for mgmt failed\n",
            __func__, __LINE__);
        free(updated_mgmt_bytes);
        return RETURN_ERR;
    }

    memcpy(*mgmt_ptr, updated_mgmt_bytes, new_mgmt_len);
    *mgmt_len = new_mgmt_len;

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: SSID updated successfully, new length %zu\n",
        __func__, __LINE__, new_mgmt_len);

    free(updated_mgmt_bytes);
    return RETURN_OK;
}

int construct_probe_request(struct ieee80211_mgmt **mgmt_ptr, size_t *mgmt_len,
    const char *new_ssid)
{
    if (mgmt_ptr == NULL || *mgmt_ptr == NULL || mgmt_len == 0 || new_ssid == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Null pointer argument\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    struct ieee80211_mgmt *mgmt = *mgmt_ptr;
    size_t new_ssid_len = strlen(new_ssid);
    if (new_ssid_len > WIFI_AP_MAX_SSID_LEN) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: SSID length exceeds max (%zu)\n", __func__,
            __LINE__, new_ssid_len);
        return RETURN_ERR;
    }

    size_t offset = 24; // Fixed size of 802.11 mgmt header
    if (*mgmt_len < offset) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: mgmt_len (%zu) less than header offset (%zu)\n", __func__, __LINE__, *mgmt_len,
            offset);
        return RETURN_ERR;
    }

    size_t remaining_len = *mgmt_len - offset;
    uint8_t *ie_ptr = ((uint8_t *)(*mgmt_ptr)) + offset;

    size_t cursor = 0;
    int ssid_found = 0;

    // Search for SSID IE (Element ID 0x00)
    while (cursor + 2 <= remaining_len) {
        uint8_t eid = ie_ptr[cursor];
        uint8_t len = ie_ptr[cursor + 1];

        if (cursor + 2 + len > remaining_len) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: IE length exceeds buffer\n", __func__,
                __LINE__);
            return RETURN_ERR;
        }

        if (eid == 0x00) {
            ssid_found = 1;
            break;
        }

        cursor += 2 + len;
    }

    if (ssid_found == 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: SSID IE not found in probe request\n",
            __func__, __LINE__);
        return RETURN_ERR;
    }

    size_t original_ssid_ie_len = 2 + ie_ptr[cursor + 1];
    size_t new_ie_len = 2 + new_ssid_len;
    size_t new_mgmt_len = *mgmt_len - original_ssid_ie_len + new_ie_len;

    uint8_t *updated_mgmt_bytes = (uint8_t *)malloc(new_mgmt_len);
    if (updated_mgmt_bytes == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Memory allocation failed\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    // Copy everything before SSID IE
    memcpy(updated_mgmt_bytes, *mgmt_ptr, offset + cursor);

    // Insert new SSID IE
    updated_mgmt_bytes[offset + cursor] = 0x00;
    updated_mgmt_bytes[offset + cursor + 1] = (uint8_t)new_ssid_len;
    memcpy(updated_mgmt_bytes + offset + cursor + 2, new_ssid, new_ssid_len);

    // Copy the remaining IEs
    size_t tail_start = cursor + original_ssid_ie_len;
    size_t tail_len = remaining_len - tail_start;
    memcpy(updated_mgmt_bytes + offset + cursor + 2 + new_ssid_len, ie_ptr + tail_start, tail_len);

    // Replace old mgmt
    free(*mgmt_ptr);
    *mgmt_ptr = (struct ieee80211_mgmt *)calloc(1, new_mgmt_len);
    if (*mgmt_ptr == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Memory allocation for mgmt failed\n",
            __func__, __LINE__);
        free(updated_mgmt_bytes);
        return RETURN_ERR;
    }

    memcpy(*mgmt_ptr, updated_mgmt_bytes, new_mgmt_len);
    *mgmt_len = new_mgmt_len;

    wlan_emu_print(wlan_emu_log_level_dbg,
        "%s:%d: Probe SSID updated successfully to '%s', new length %zu\n", __func__, __LINE__,
        new_ssid, new_mgmt_len);

    free(updated_mgmt_bytes);
    return RETURN_OK;
}

/*void print_hex_dump(unsigned int length, const unsigned char *buffer)
{
    if (buffer == nullptr || length == 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: NULL buffer or zero length (%u)\n", __func__,
            __LINE__, length);
        return;
    }

    std::string output;
    char ascii_buff[17] = { 0 };

    for (unsigned int i = 0; i < length; ++i) {
        // At start of line, add offset
        if (i % 16 == 0) {
            if (i != 0) {
                output += "  ";
                output += ascii_buff;
                output += "\n";
            }
            char offset_str[16];
            snprintf(offset_str, sizeof(offset_str), "%04x ", i);
            output += offset_str;
        }

        // Append hex byte
        char hex_byte[4];
        snprintf(hex_byte, sizeof(hex_byte), " %02x", buffer[i]);
        output += hex_byte;

        // Build ascii buffer
        ascii_buff[i % 16] = (isprint(buffer[i]) ? buffer[i] : '.');
        ascii_buff[(i % 16) + 1] = '\0';
    }

    // Handle last line padding
    int remainder = length % 16;
    if (remainder != 0) {
        for (int i = remainder; i < 16; ++i) {
            output += "   ";
        }
    }

    // Append final ascii chars
    output += "  ";
    output += ascii_buff;
    output += "\n";

    // Single wlan_emu_print call with full dump
    wlan_emu_print(wlan_emu_log_level_dbg, "\n%s\n", output.c_str());
}*/ //enable for debug

int test_step_param_packet_generator::update_brlan0_config()
{
    std::string interface;
    struct ifreq ifr;
    std::memset(&ifr, 0, sizeof(ifr));
    test_step_params_t *step = static_cast<test_step_params_t *>(this);

    wifi_vap_info_t *vap_info = nullptr;
    if (step->u.packet_generator->vapname.empty()) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: vapname is empty\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    vap_info = step->m_ui_mgr->get_cci_vap_info(
            const_cast<char *>(step->u.packet_generator->vapname.c_str()));
    if (vap_info == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: vap_info is NULL\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    step->u.packet_generator->bridge_name = vap_info->bridge_name;
    interface = step->u.packet_generator->bridge_name;
    std::strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to open socket\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    // --- Get MAC address ---
    if (ioctl(fd, SIOCGIFHWADDR, &ifr) == 0) {
        auto *mac = reinterpret_cast<unsigned char *>(ifr.ifr_hwaddr.sa_data);
        std::memcpy(step->u.packet_generator->brlan0_mac, mac, sizeof(mac_addr_t));

        wlan_emu_print(wlan_emu_log_level_dbg,
            "%s:%d: MAC Address of %s: %02x:%02x:%02x:%02x:%02x:%02x\n", __func__, __LINE__,
            interface.c_str(), step->u.packet_generator->brlan0_mac[0],
            step->u.packet_generator->brlan0_mac[1], step->u.packet_generator->brlan0_mac[2],
            step->u.packet_generator->brlan0_mac[3], step->u.packet_generator->brlan0_mac[4],
            step->u.packet_generator->brlan0_mac[5]);
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: ioctl error for MAC address\n", __func__,
            __LINE__);
        close(fd);
        return RETURN_ERR;
    }

    close(fd);
    return RETURN_OK;
}

int parse_mgmt_frame_from_pcap(const char *proto_file, struct ieee80211_mgmt **mgmt_frame,
    size_t &mgmt_len)
{
    struct pcap_pkthdr header;
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle = pcap_open_offline(proto_file, errbuf);

    if (handle == NULL)
        return RETURN_ERR;

    const unsigned char *packet = pcap_next(handle, &header);
    if (packet == NULL) {
        pcap_close(handle);
        return RETURN_ERR;
    }

    auto *radiotap = (struct ieee80211_radiotap_header *)packet;
    struct ieee80211_radiotap_iterator iterator;
    bool is_fcs_present = false;

    if (ieee80211_radiotap_iterator_init(&iterator, radiotap, header.caplen, nullptr) != 0) {
        pcap_close(handle);
        return RETURN_ERR;
    }

    while (ieee80211_radiotap_iterator_next(&iterator) == 0) {
        if (iterator.this_arg_index == IEEE80211_RADIOTAP_FLAGS &&
            (*iterator.this_arg & IEEE80211_RADIOTAP_F_FCS)) {
            is_fcs_present = true;
            break;
        }
    }

    uint16_t radiotap_len = radiotap->it_len;
    if (radiotap_len >= header.caplen) {
        pcap_close(handle);
        return RETURN_ERR;
    }

    mgmt_len = header.caplen - radiotap_len - (is_fcs_present ? 4 : 0);
    *mgmt_frame = (struct ieee80211_mgmt *)calloc(1, mgmt_len);
    if (*mgmt_frame == NULL) {
        pcap_close(handle);
        return RETURN_ERR;
    }
    memcpy(*mgmt_frame, packet + radiotap_len, mgmt_len);

    pcap_close(handle);
    return RETURN_OK;
}

unsigned int test_step_param_packet_generator::update_pcap_bssids(char *pcap_file,
    test_step_params_t *step)
{
    struct ieee80211_mgmt *mgmt = nullptr;
    size_t mgmt_len = 0;
    mac_addr_t sourceMAC = { 0xe8, 0xd8, 0xd1, 0x33, 0xbb, 0x46 };

    if (parse_mgmt_frame_from_pcap(pcap_file, &mgmt, mgmt_len) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to parse management frame\n",
            __func__, __LINE__);
        if (mgmt != NULL) {
            free(mgmt);
        }
        mgmt = NULL;
        return RETURN_ERR;
    }

    if (mgmt_len < offsetof(struct ieee80211_mgmt, u)) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: mgmt frame too short\n", __func__, __LINE__);
        free(mgmt);
        mgmt = NULL;
        return RETURN_ERR;
    }

    wifi_vap_info_t *vap_info = nullptr;
    if (step->u.packet_generator->vapname.empty()) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: vapname is empty\n", __func__, __LINE__);
        free(mgmt);
        mgmt = NULL;
        return RETURN_ERR;
    }

    vap_info = step->m_ui_mgr->get_cci_vap_info(
        const_cast<char *>(step->u.packet_generator->vapname.c_str()));
    if (vap_info == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: vap_info is NULL\n", __func__, __LINE__);
        free(mgmt);
        mgmt = NULL;
        return RETURN_ERR;
    }

    memcpy(mgmt->da, vap_info->u.bss_info.bssid, ETH_ALEN);
    memcpy(mgmt->sa, sourceMAC, ETH_ALEN);
    memcpy(mgmt->bssid, vap_info->u.bss_info.bssid, ETH_ALEN);

    int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd < 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: socket() failed: %s\n", __func__, __LINE__,
            strerror(errno));
        free(mgmt);
        mgmt = NULL;
        return RETURN_ERR;
    }

    struct ifreq if_idx {};
    strncpy(if_idx.ifr_name, step->u.packet_generator->bridge_name.c_str(), IFNAMSIZ - 1);
    if (ioctl(sockfd, SIOCGIFINDEX, &if_idx) < 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: SIOCGIFINDEX failed: %s\n", __func__,
            __LINE__, strerror(errno));
        close(sockfd);
        free(mgmt);
        mgmt = NULL;
        return RETURN_ERR;
    }

    ethernet_header_t eth;
    memcpy(eth.destination, sourceMAC, ETH_ALEN);
    memcpy(eth.source, step->u.packet_generator->brlan0_mac, ETH_ALEN); // Clear source MAC
    eth.protocol = htons(0x2329);

    if (is_assoc_req(mgmt->frame_control)) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Updating SSID %s for assoc request\n",
            __func__, __LINE__, vap_info->u.bss_info.ssid);
        wlan_emu_print(wlan_emu_log_level_dbg, "\n");
        if (construct_assoc_request(&mgmt, &mgmt_len, vap_info->u.bss_info.ssid) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: Failed to update SSID in Assoc Request\n", __func__, __LINE__);
            close(sockfd);
            free(mgmt);
            mgmt = NULL;
            return RETURN_ERR;
        }
    }

    if (is_probe_req(mgmt->frame_control)) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Updating SSID %s for probe request\n",
            __func__, __LINE__, vap_info->u.bss_info.ssid);
        wlan_emu_print(wlan_emu_log_level_dbg, "\n");
        if (construct_probe_request(&mgmt, &mgmt_len, vap_info->u.bss_info.ssid) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: Failed to update SSID in Assoc Request\n", __func__, __LINE__);
            close(sockfd);
            free(mgmt);
            mgmt = NULL;
            return RETURN_ERR;
        }
    }

    size_t total_len = sizeof(ethernet_header_t) + sizeof(u8aRadiotapHeader) + mgmt_len;
    uint8_t *full_packet = (uint8_t *)malloc(total_len);
    if (full_packet == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: malloc() failed: %s\n", __func__, __LINE__,
            strerror(errno));
        close(sockfd);
        free(mgmt);
        mgmt = NULL;
        return RETURN_ERR;
    }

    memset(full_packet, 0, total_len);
    memcpy(full_packet, &eth, sizeof(ethernet_header_t));
    memcpy(full_packet + sizeof(ethernet_header_t), u8aRadiotapHeader, sizeof(u8aRadiotapHeader));
    memcpy(full_packet + sizeof(ethernet_header_t) + sizeof(u8aRadiotapHeader), mgmt, mgmt_len);

    struct sockaddr_ll socket_address {};
    socket_address.sll_ifindex = if_idx.ifr_ifindex;
    socket_address.sll_halen = ETH_ALEN;
    memcpy(socket_address.sll_addr, step->u.packet_generator->brlan0_mac, ETH_ALEN);

    //print_hex_dump(total_len, full_packet); //enable for debug

    ssize_t sent = sendto(sockfd, full_packet, total_len, 0, (struct sockaddr *)&socket_address,
        sizeof(socket_address));
    if (sent < 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: sendto() failed: for %s with error no: %s\n",
            __func__, __LINE__, pcap_file, strerror(errno));
    } else {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Packet %s sent successfully: %ld bytes\n",
            __func__, __LINE__, pcap_file, sent);
    }

    free(full_packet);
    free(mgmt);
    mgmt = NULL;
    full_packet = NULL;
    close(sockfd);
    return RETURN_OK;
}

void *test_step_param_packet_generator::pkt_gen_cci_thread(void *arg)
{
    test_step_param_packet_generator *packet_gen_step =
        static_cast<test_step_param_packet_generator *>(arg);
    test_step_params_t *step = static_cast<test_step_params_t *>(packet_gen_step);
    test_step_param_packet_generator *self = static_cast<test_step_param_packet_generator *>(arg);

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: STEP : %d - Entered\n", __func__, __LINE__,
        step->step_number);

    if (self->update_brlan0_config() != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to update brlan0 config\n", __func__,
            __LINE__);
        step->m_ui_mgr->cci_error_code = EBRLANCFG;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return NULL;
    }
    queue_t *packet_gen_queue = packet_gen_step->u.packet_generator->pcap_queue;
    uint frame_count = queue_count(packet_gen_queue);

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: value of iteration is %d\n", __func__, __LINE__,
        step->u.packet_generator->iteration);
    for (int iteration = 0; iteration < step->u.packet_generator->iteration; iteration++) {
        for (int index = (int)frame_count - 1; index >= 0; index--) {
            packet_capture_t *packet_gen_config = static_cast<packet_capture_t *>(
                queue_peek(packet_gen_queue, index));
            if (update_pcap_bssids(packet_gen_config->pcap_location, step) != RETURN_OK) {
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: Failed to update BSSIDs in pcap file\n", __func__, __LINE__);
                step->m_ui_mgr->cci_error_code = EBSSID;
                step->test_state = wlan_emu_tests_state_cmd_abort;
                return NULL;
            }
            WaitForDuration(step->u.packet_generator->interval);
        }
    }
    step->test_state = wlan_emu_tests_state_cmd_results;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: STEP : %d - Exiting thread\n", __func__,
        __LINE__, step->step_number);
    return NULL;
}

int test_step_param_packet_generator::step_execute()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: STEP : %d - Called for Step Execute\n", __func__,
        __LINE__, this->step_number);

    test_step_params_t *step = this;
    uint frame_count = queue_count(this->u.packet_generator->pcap_queue);
    this->execution_time = frame_count *
        (this->u.packet_generator->iteration * (this->u.packet_generator->interval / 1000));
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: STEP : %d - execution_time : %d\n", __func__,
        __LINE__, this->step_number, this->execution_time);

    if (pthread_create(&this->u.packet_generator->p_tid, NULL,
            test_step_param_packet_generator::pkt_gen_cci_thread, static_cast<void *>(this)) != 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to create thread\n", __func__,
            __LINE__);
        step->m_ui_mgr->cci_error_code = ETHREAD;
        step->test_state = wlan_emu_tests_state_cmd_abort;
        return RETURN_ERR;
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: STEP : %d - Thread Created\n", __func__,
        __LINE__, this->step_number);

    if (step->fork == true) {
        step->test_state = wlan_emu_tests_state_cmd_wait;
    } else {
        step->test_state = wlan_emu_tests_state_cmd_continue;
    }

    return RETURN_OK;
}

int test_step_param_packet_generator::step_frame_filter(wlan_emu_msg_t *msg)
{
    test_step_params_t *step = this;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: step number : %d\n", __func__, __LINE__,
        step->step_number);
    if (msg == NULL) {
        return RETURN_UNHANDLED;
    }
    switch (msg->get_msg_type()) {
    case wlan_emu_msg_type_webconfig: // onewifi_webconfig
    case wlan_emu_msg_type_cfg80211: // beacon
    case wlan_emu_msg_type_frm80211: // mgmt
    default:
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Not supported msg_type : %d\n", __func__,
            __LINE__, msg->get_msg_type());
        break;
    }
    return RETURN_UNHANDLED;
}

void test_step_param_packet_generator::step_remove()
{
    test_step_param_packet_generator *step = static_cast<test_step_param_packet_generator *>(this);

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: STEP : %d - Called for Step Remove\n", __func__,
        __LINE__, step->step_number);

    queue_destroy(step->u.packet_generator->pcap_queue);
    step->u.packet_generator->pcap_queue = NULL;
    pthread_cancel(step->u.packet_generator->p_tid);

    delete step;
    return;
}

int test_step_param_packet_generator::step_timeout()
{
    test_step_params_t *step = static_cast<test_step_param_packet_generator *>(this);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Test Step Num : %d timeout_count : %d\n",
        __func__, __LINE__, this->step_number, this->timeout_count);

    if (step->test_state != wlan_emu_tests_state_cmd_results) {
        step->timeout_count++;

        if (step->execution_time > step->timeout_count) {
            step->test_state = wlan_emu_tests_state_cmd_continue;
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: continue for step : %d execution_time : %d timeout_count : %d\n", __func__,
                __LINE__, step->step_number, step->execution_time, step->timeout_count);
        } else if (step->execution_time == step->timeout_count) {
            step->test_state = wlan_emu_tests_state_cmd_results;
            pthread_cancel(this->u.packet_generator->p_tid);
            wlan_emu_print(wlan_emu_log_level_dbg,
                "%s:%d: timeout for step : %d execution_time : %d timeout_count : %d\n", __func__,
                __LINE__, step->step_number, step->execution_time, step->timeout_count);
        } else {
            if (step->fork == true) {
                step->test_state = wlan_emu_tests_state_cmd_wait;
            } else {
                step->test_state = wlan_emu_tests_state_cmd_continue;
                wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: continue for step : %d\n", __func__,
                    __LINE__, step->step_number);
            }
        }
    }

    return RETURN_OK;
}

test_step_param_packet_generator::test_step_param_packet_generator()
{
    this->u.packet_generator = new packet_generator_t;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: STEP : Constructor for Packet Gen called\n",
        __func__, __LINE__);
    this->u.packet_generator->pcap_queue = queue_create();
    if (this->u.packet_generator->pcap_queue == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to create pcap queue\n", __func__,
            __LINE__);
        delete this->u.packet_generator;
        return;
    }
    this->is_step_initialized = true;
    this->execution_time = 15;
    this->timeout_count = 0;
    this->capture_frames = false;
}

test_step_param_packet_generator::~test_step_param_packet_generator()
{
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: Destructor for Packet Gen called\n", __func__,
        __LINE__);
}
