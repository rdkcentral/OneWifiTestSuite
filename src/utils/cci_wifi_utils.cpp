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

#include "cci_wifi_utils.hpp"
#include "common/ieee802_11_defs.h"
#include "wlan_emu_log.h"
#include "wlan_emu_err_code.h"
#include <ctime>
#include <sys/time.h>
#include <curl/curl.h>
#include <fstream>
#include <memory>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include <sys/wait.h>
#include <sstream>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sched.h>

#include <ifaddrs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

char *mac_to_str(unsigned char *mac, char *s_mac)
{
    if ((mac == NULL) || (s_mac == NULL)) {
        return NULL;
    }

    snprintf(s_mac, 18, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx", mac[0], mac[1], mac[2], mac[3],
        mac[4], mac[5]);

    return s_mac;
}

char *mac_str_without_colon(mac_address_t mac, mac_addr_str_t key)
{
    snprintf(key, 18, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return (char *)key;
}

int get_current_time_string(char *time_str, int time_str_len)
{
    struct timeval tv_now;
    struct tm *timeinfo;
    char timestamp[24];
    int ret = 0;

    if ((time_str == NULL) || (time_str_len == 0)) {
        return RETURN_ERR;
    }

    memset(time_str, 0, time_str_len);
    gettimeofday(&tv_now, NULL);
    timeinfo = localtime(&tv_now.tv_sec);

    // Format the timestamp
    strftime(timestamp, sizeof(timestamp), "%Y%m%d%H%M%S", timeinfo);

    ret = snprintf(time_str, time_str_len, "%s%06lu", timestamp, tv_now.tv_usec);
    if ((ret < 0) || (ret >= time_str_len)) {
        return RETURN_ERR;
    }

    return RETURN_OK;
}

int dmcli_get(char *cmd, char *value, unsigned int val_len)
{
    FILE *fp;
    char ptr[128];
    char str[1024];
    int bytes_written = 0;
    char *temp_str = NULL;
    char search_str[] = "value: ";
    int length = 0;
    int i = 0;

    if ((cmd == NULL) || (value == NULL)) {
        printf(" %s:%d input arguements are NULL cmd : %p value : %p", __func__, __LINE__, cmd,
            value);
        return RETURN_ERR;
    }

    printf("cmd : %s\n", cmd);

    memset(ptr, 0, sizeof(ptr));
    memset(str, 0, sizeof(str));

    fp = popen(cmd, "r");
    if (fp == NULL) {
        printf(" %s:%d popen failed for cmd %s", __func__, __LINE__, cmd);
        return RETURN_ERR;
    }

    // Read and print the output of the cmd
    while (fgets(ptr, sizeof(ptr), fp) != NULL) {
        bytes_written += snprintf(&str[bytes_written], sizeof(str) - bytes_written, "%s", ptr);
    }

    pclose(fp);
    temp_str = strstr(str, search_str);
    if (temp_str == NULL) {
        printf(" %s:%d unable to find search_str : %s for cmd %s", __func__, __LINE__, search_str,
            cmd);
        return RETURN_ERR;
    } else {
        temp_str += strlen(search_str);
        length = strlen(temp_str);

        while (i < length) {
            if (((temp_str[i] == ' ') || (temp_str[i] == '\n') || (temp_str[i] == '\r'))) {
                temp_str[i] = '\0';
                break;
            }
            i++;
        }

        snprintf(value, val_len, "%s", temp_str);
        return RETURN_OK;
    }

    return RETURN_ERR;
}

uint16_t inet_csum(const void *buf, size_t hdr_len)
{
    unsigned long sum = 0;
    const uint16_t *ip1;

    ip1 = (const uint16_t *)buf;
    while (hdr_len > 1) {
        sum += *ip1++;
        if (sum & 0x80000000) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
        hdr_len -= 2;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (~sum);
}

int send_raw_packet(const void *data, size_t data_len, uint32_t source_nip, int source_port,
    uint32_t dest_nip, int dest_port, const uint8_t *dest_arp, int ifindex)
{
    struct sockaddr_ll dest_sll;
    int fd;
    int res = -1;
    char *buf = NULL;
    size_t sz;

    fd = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));
    if (fd < 0) {
        printf("%s:%d: Failed to open socket\n", __func__, __LINE__);
        return res;
    }

    sz = data_len;
    buf = (char *)malloc(sz);

    if (buf == NULL)
        goto ret_close;

    memset(&dest_sll, 0, sizeof(dest_sll));

    dest_sll.sll_family = AF_PACKET;
    dest_sll.sll_protocol = htons(ETH_P_IP);
    dest_sll.sll_ifindex = ifindex;
    dest_sll.sll_halen = 6;
    memcpy(dest_sll.sll_addr, dest_arp, 6);

    if (bind(fd, (struct sockaddr *)&dest_sll, sizeof(dest_sll)) < 0) {
        free(buf);
        printf("%s:%d: Failed to bind\n", __func__, __LINE__);
        goto ret_close;
    }

    res = sendto(fd, buf, sz, 0, (struct sockaddr *)&dest_sll, sizeof(dest_sll));

    if (res < 0) {
        printf("%s:%d: Failed to send data, error %d\n", __func__, __LINE__, res);
    }

    free(buf);

ret_close:
    close(fd);
    return res;
}

unsigned int ieee_frame_hdr_len(__le16 fc)
{
    unsigned int hdrlen = 24;
    unsigned short type = 0;
    type = WLAN_FC_GET_TYPE(fc);

    if (type == WLAN_FC_TYPE_DATA) {
        if ((fc & (WLAN_FC_TODS | WLAN_FC_FROMDS)) == (WLAN_FC_TODS | WLAN_FC_FROMDS)) {
            hdrlen = 30;
        }

        // QOSDATA
        if (((fc & 0x00f0) & 0x80) == 0x80) {
            // IEEE80211_QOS_CTL_LEN
            hdrlen += 2;
            // IEEE80211_FC1_ORDER     0x80
            if (((fc & 0xf000) & 0x8000) != 0) {
                hdrlen += 0x04;
            }
            goto out;
        }
    }

    if (type == WLAN_FC_TYPE_MGMT) {
        // IEEE80211_FC1_ORDER
        if (((fc & 0xf000) & 0x8000) != 0) {
            hdrlen += 0x04;
        }
        goto out;
    }

    if (type == WLAN_FC_TYPE_CTRL) {
        /*
         * ACK and CTS are 10 bytes, all others 16. To see how
         * to get this condition consider
         *   subtype mask:   0b0000000011110000 (0x00F0)
         *   ACK subtype:    0b0000000011010000 (0x00D0)
         *   CTS subtype:    0b0000000011000000 (0x00C0)
         *   bits that matter:         ^^^      (0x00E0)
         *   value of those: 0b0000000011000000 (0x00C0)
         */
        if (((fc & 0xff00) & 0x00E0) == 0x00C) {
            hdrlen = 10;
        } else {
            hdrlen = 16;
        }
    }

out:
    return hdrlen;
}

bool is_zero_mac(mac_address_t mac)
{
    for (int i = 0; i < MAC_ADDR_LEN; i++) {
        if (mac[i] != 0x00) {
            return false; // If any byte is not zero, return false
        }
    }
    return true; // All bytes are zero
}

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    return fwrite(ptr, size, nmemb, stream);
}

void copy_string(char *destination, char *source, int len)
{
    if (source == NULL) {
        destination[0] = 0;
    } else {
        strncpy(destination, source, len);
    }
}

int copy_file(const char *source_path, long source_offset, const char *destination_path)
{
    if ((source_path == NULL) || (destination_path == NULL)) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: input arguements are NULL source_path : %p destination_path : %p\n", __func__,
            __LINE__, source_path, destination_path);
        return RETURN_ERR;
    }

    std::ifstream source_file(source_path);

    if (!source_file.is_open()) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Unable to open the source file : %s\n",
            __func__, __LINE__, source_path);
        return RETURN_ERR;
    }

    std::ofstream destination_file(destination_path);

    if (!destination_file.is_open()) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Error opening destination file %s\n",
            __func__, __LINE__, destination_path);
        source_file.close();
        return RETURN_ERR;
    }

    wlan_emu_print(wlan_emu_log_level_dbg,
        "%s:%d: copying from source_path : %s destination_path : %s\n", __func__, __LINE__,
        source_path, destination_path);

    source_file.seekg(source_offset, std::ios::beg);
    std::string line;

    while (std::getline(source_file, line)) {
        destination_file << line << std::endl;
    }

    source_file.close();
    destination_file.close();

    return RETURN_OK;
}

int https_get_file(http_info_t *http_info, const char *url, const char *output_file,
    int &error_code)
{
    CURL *curl;
    CURLcode res;
    FILE *fp = NULL;
    char *test_config_URL = NULL;
    long code = 0;
    int ret = RETURN_ERR;
    char local_host_pat[128] = { 0 };
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: url : %s output_file : %s\n", __func__, __LINE__,
        url, output_file);

    if ((url == NULL) || (output_file == NULL)) {
        error_code = ENULL;
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: input arguements are NULL url : %p output_file : %p\n", __func__, __LINE__, url,
            output_file);
        return RETURN_ERR;
    }
    if (http_info->is_local_host_enabled == true) {
        if (copy_file(url, 0, output_file) != RETURN_OK) {
            error_code = EFCOPY;
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to copy from %s to %s\n",
                __func__, __LINE__, url, output_file);
            return RETURN_ERR;
        }
        ret = RETURN_OK;

    } else {
        fp = fopen(output_file, "wb");
        if (fp == NULL) {
            error_code = EFOPEN;
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to open the file :%s\n", __func__,
                __LINE__, output_file);
            return RETURN_ERR;
        }
        test_config_URL = strdup(url);

        curl = curl_easy_init();
        if (curl) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: using get_tda_interface() :%s\n",
                __func__, __LINE__, http_info->interface);
            res = curl_easy_setopt(curl, CURLOPT_INTERFACE, http_info->interface);
            curl_easy_setopt(curl, CURLOPT_URL, test_config_URL);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
            curl_easy_setopt(curl, CURLOPT_SSLCERT, http_info->ssl_cert);
            curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "P12");
            curl_easy_setopt(curl, CURLOPT_KEYPASSWD, http_info->ssl_key);
            // CURLOPT_VERBOSE is for debugging
            // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
            res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 25L);
            res = curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_WHATEVER);
            // disconnect if it is failed to validate server's cert
            res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
            // Verify the certificate's name against host
            res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
            res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                error_code = ECURLGET;
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: curl_easy_perform() failed curl errno : %d errstr : %s for url : %s "
                    "len : %d\n",
                    __func__, __LINE__, res, curl_easy_strerror(res), test_config_URL,
                    strlen(test_config_URL));
                free(test_config_URL);
                fclose(fp);
                return RETURN_ERR;
            }

            // CURLINFO_RESPONSE_CODE Causing segmentation fault, need to fix this
            //        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
            //      *res_code = code;
            curl_easy_cleanup(curl);
            curl = NULL;
            wlan_emu_print(wlan_emu_log_level_info, "%s:%d: ==> Downloaded file : %s\n", __func__,
                __LINE__, output_file);
            ret = RETURN_OK;
        } else {
            error_code = ECURLGET;
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to initialize libcurl.\n",
                __func__, __LINE__);
            ret = RETURN_ERR;
        }

        free(test_config_URL);
        fclose(fp);
    }
    return ret;
}

int https_post_file(http_info_t *http_info, const char *post_url, const char *input_file,
    int &error_code)
{
    int ret = RETURN_ERR;
    CURL *curl;
    CURLcode res;
    char *test_config_URL = NULL;
    char file_name[128] = { 0 };

    if ((post_url == NULL) || (input_file == NULL)) {
        error_code = ENULL;
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: input arguements are NULL url : %p input_file : %p\n", __func__, __LINE__,
            post_url, input_file);
        return RETURN_ERR;
    }

    if (http_info->is_local_host_enabled == true) {
        if (copy_file(input_file, 0, post_url) != RETURN_OK) {
            error_code = EFCOPY;
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to copy from %s to %s\n",
                __func__, __LINE__, input_file, post_url);
            ret = RETURN_ERR;
        } else {
            wlan_emu_print(wlan_emu_log_level_info, "%s:%d: file copied from %s to %s\n", __func__,
                __LINE__, input_file, post_url);
            ret = RETURN_OK;
        }
    } else {

        if (get_last_substring_after_slash(input_file, file_name, sizeof(file_name), error_code) !=
            RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: get_last_substring_after_slash failed for input_file : %s\n", __func__,
                __LINE__, input_file);
            return RETURN_ERR;
        }

        test_config_URL = strdup(post_url);
        curl = curl_easy_init();
        if (curl) {
            // Set the URL for the POST request
            curl_easy_setopt(curl, CURLOPT_URL, test_config_URL);

            // Set the HTTP method to POST
            curl_easy_setopt(curl, CURLOPT_POST, 1L);

            // Set the file to be sent
            curl_mime *mime;
            curl_mimepart *part;

            mime = curl_mime_init(curl);
            part = curl_mime_addpart(mime);

            // curl_mime_name(part, "file");   // Set the field name
            curl_mime_name(part, "data"); // Set the field name
            curl_mime_filename(part, file_name); // Set the filename
            curl_mime_filedata(part, input_file); // Set the file to be uploaded

            curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
            // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
            curl_easy_setopt(curl, CURLOPT_SSLCERT, http_info->ssl_cert);
            curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "P12");
            curl_easy_setopt(curl, CURLOPT_KEYPASSWD, http_info->ssl_key);
            res = curl_easy_setopt(curl, CURLOPT_INTERFACE, http_info->interface);
            res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 25L);
            res = curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_WHATEVER);
            // disconnect if it is failed to validate server's cert
            res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
            // Verify the certificate's name against host
            res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

            // Perform the POST request
            res = curl_easy_perform(curl);

            if (res != CURLE_OK) {
                error_code = ECURLPOST;
                wlan_emu_print(wlan_emu_log_level_err,
                    "%s:%d: curl_easy_perform() failed: errno : %d error_str : %s for url : %s for "
                    "%s\n",
                    __func__, __LINE__, res, curl_easy_strerror(res), test_config_URL, input_file);
                ret = RETURN_ERR;
            } else {
                wlan_emu_print(wlan_emu_log_level_info,
                    "%s:%d: curl_easy_perform() success for url : %s for %s\n", __func__, __LINE__,
                    test_config_URL, input_file);
                ret = RETURN_OK;
            }
            // Clean up
            curl_easy_cleanup(curl);
            curl_mime_free(mime);
        }

        free(test_config_URL);
    }

    return ret;
}

static int curl_debug(CURL *handle, curl_infotype type, char *data, size_t size, void *clientp)
{
    if (type != CURLINFO_TEXT) {
        return 0;
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: %-.*s", __func__, __LINE__, size, data);

    return 0;
}

static size_t write_callback(void *contents, size_t size, size_t nmemb, std::string *output)
{
    size_t total_size = size * nmemb;

    try {
        output->append(static_cast<const char *>(contents), total_size);
    } catch (const std::exception &e) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to write response: %s\n", __func__,
            __LINE__, e.what());
        return 0;
    }

    return total_size;
}

int http_get(const std::string &url, std::string &response, long &status_code, int &error_code)
{
    CURLcode ret;
    CURL *curl = NULL;

    curl = curl_easy_init();
    if (curl == NULL) {
        error_code = ECURLGET;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d failed to initialize curl\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl_wrapper(curl, curl_easy_cleanup);

    ret = curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, curl_debug);
    if (ret != CURLE_OK) {
        error_code = ECURLGET;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to set curl opt: %s\n", __func__,
            __LINE__, curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    ret = curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    if (ret != CURLE_OK) {
        error_code = ECURLGET;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to set curl opt: %s\n", __func__,
            __LINE__, curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    ret = curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    if (ret != CURLE_OK) {
        error_code = ECURLGET;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to set curl opt: %s\n", __func__,
            __LINE__, curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    ret = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 25L);
    if (ret != CURLE_OK) {
        error_code = ECURLGET;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to set curl opt: %s\n", __func__,
            __LINE__, curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    ret = curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_WHATEVER);
    if (ret != CURLE_OK) {
        error_code = ECURLGET;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: curl setopt failed: %s\n", __func__,
            __LINE__, curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    ret = curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    if (ret != CURLE_OK) {
        error_code = ECURLGET;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to set curl opt: %s\n", __func__,
            __LINE__, curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    ret = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    if (ret != CURLE_OK) {
        error_code = ECURLGET;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to set curl opt: %s\n", __func__,
            __LINE__, curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    ret = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    if (ret != CURLE_OK) {
        error_code = ECURLGET;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to set curl opt: %s\n", __func__,
            __LINE__, curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    ret = curl_easy_perform(curl);
    if (ret != CURLE_OK) {
        error_code = ECURLGET;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: GET %s request failed: %s\n", __func__,
            __LINE__, url.c_str(), curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    ret = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    if (ret != CURLE_OK) {
        error_code = ECURLGET;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: curl getinfo failed: %s\n", __func__,
            __LINE__, curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    return RETURN_OK;
}

static size_t write_file_callback(void *contents, size_t size, size_t nmemb,
    std::ofstream *out_file)
{
    size_t total_size = size * nmemb;

    out_file->write(static_cast<const char *>(contents), total_size);
    if (out_file->fail()) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed write to file, error: %d (%s)\n",
            __func__, __LINE__, errno, strerror(errno));
        return 0;
    }

    return total_size;
}

int http_get_file(const std::string &url, const std::string &file_path, long &status_code,
    int &error_code)
{
    CURLcode ret;
    CURL *curl = NULL;

    curl = curl_easy_init();
    if (curl == NULL) {
        error_code = ECURLGET;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d failed to initialize curl\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl_wrapper(curl, curl_easy_cleanup);

    ret = curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, curl_debug);
    if (ret != CURLE_OK) {
        error_code = ECURLGET;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to set curl opt: %s\n", __func__,
            __LINE__, curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    ret = curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    if (ret != CURLE_OK) {
        error_code = ECURLGET;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to set curl opt: %s\n", __func__,
            __LINE__, curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    ret = curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    if (ret != CURLE_OK) {
        error_code = ECURLGET;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to set curl opt: %s\n", __func__,
            __LINE__, curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    ret = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_callback);
    if (ret != CURLE_OK) {
        error_code = ECURLGET;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to set curl opt: %s\n", __func__,
            __LINE__, curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    std::ofstream out_file(file_path, std::ofstream::out | std::ios::binary);
    if (!out_file.is_open()) {
        error_code = EFOPEN;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to open %s file, error: %d (%s)\n",
            __func__, __LINE__, file_path.c_str(), errno, strerror(errno));
        return RETURN_ERR;
    }

    ret = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out_file);
    if (ret != CURLE_OK) {
        error_code = ECURLGET;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to set curl opt: %s\n", __func__,
            __LINE__, curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    ret = curl_easy_perform(curl);
    if (ret != CURLE_OK) {
        error_code = ECURLGET;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: GET %s request failed: %s\n", __func__,
            __LINE__, url.c_str(), curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    out_file.close();
    if (out_file.fail()) {
        error_code = EFWRITE;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed write to %s file, error: %d (%s)\n",
            __func__, __LINE__, file_path.c_str(), errno, strerror(errno));
        return RETURN_ERR;
    }

    ret = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    if (ret != CURLE_OK) {
        error_code = ECURLGET;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: curl getinfo failed: %s\n", __func__,
            __LINE__, curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    return RETURN_OK;
}

int http_post(const std::string &url, const std::string &data, long &status_code, int &error_code)
{
    CURLcode ret;
    CURL *curl = NULL;
    curl_slist *headers = NULL;

    curl = curl_easy_init();
    if (curl == NULL) {
        error_code = ECURLPOST;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d failed to initialize curl\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl_wrapper(curl, curl_easy_cleanup);

    ret = curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, curl_debug);
    if (ret != CURLE_OK) {
        error_code = ECURLPOST;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to set curl opt: %s\n", __func__,
            __LINE__, curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    ret = curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    if (ret != CURLE_OK) {
        error_code = ECURLPOST;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to set curl opt: %s\n", __func__,
            __LINE__, curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    ret = curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    if (ret != CURLE_OK) {
        error_code = ECURLPOST;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to set curl opt: %s\n", __func__,
            __LINE__, curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    ret = curl_easy_setopt(curl, CURLOPT_POST, 1L);
    if (ret != CURLE_OK) {
        error_code = ECURLPOST;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to set curl opt: %s\n", __func__,
            __LINE__, curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (headers == NULL) {
        error_code = ECURLPOST;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to create headers\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headers_wrapper(headers,
        curl_slist_free_all);

    ret = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    if (ret != CURLE_OK) {
        error_code = ECURLPOST;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to set curl opt: %s\n", __func__,
            __LINE__, curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    ret = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    if (ret != CURLE_OK) {
        error_code = ECURLPOST;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to set curl opt: %s\n", __func__,
            __LINE__, curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    ret = curl_easy_perform(curl);
    if (ret != CURLE_OK) {
        error_code = ECURLPOST;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: POST %s request failed: %s\n", __func__,
            __LINE__, url.c_str(), curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    ret = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    if (ret != CURLE_OK) {
        error_code = ECURLPOST;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: curl getinfo failed: %s\n", __func__,
            __LINE__, curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    return RETURN_OK;
}

int http_post_file(const std::string &url, const std::string &file_path, long &status_code,
    int &error_code)
{
    CURLcode ret;
    CURL *curl = NULL;
    curl_mime *mime = NULL;
    curl_mimepart *part = NULL;
    char file_name[128] = { 0 };
    std::string url_with_filename;

    if (get_last_substring_after_slash(file_path.c_str(), file_name, sizeof(file_name),
            error_code) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: get_last_substring_after_slash failed for input_file: %s\n", __func__, __LINE__,
            file_path.c_str());
        return RETURN_ERR;
    }

    url_with_filename = url + std::string(file_name);

    curl = curl_easy_init();
    if (curl == NULL) {
        error_code = ECURLPOST;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d failed to initialize curl\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl_wrapper(curl, curl_easy_cleanup);

    ret = curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, curl_debug);
    if (ret != CURLE_OK) {
        error_code = ECURLPOST;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to set curl opt: %s\n", __func__,
            __LINE__, curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    ret = curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    if (ret != CURLE_OK) {
        error_code = ECURLPOST;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to set curl opt: %s\n", __func__,
            __LINE__, curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    ret = curl_easy_setopt(curl, CURLOPT_URL, url_with_filename.c_str());
    if (ret != CURLE_OK) {
        error_code = ECURLPOST;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to set curl opt: %s\n", __func__,
            __LINE__, curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    ret = curl_easy_setopt(curl, CURLOPT_POST, 1L);
    if (ret != CURLE_OK) {
        error_code = ECURLPOST;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to set curl opt: %s\n", __func__,
            __LINE__, curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        error_code = EFOPEN;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: unabled to open the file %s\n", __func__,
            __LINE__, curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    std::string file_contents((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, file_contents.size());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, file_contents.c_str());

    ret = curl_easy_perform(curl);
    if (ret != CURLE_OK) {
        error_code = ECURLPOST;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: POST %s request failed: %s\n", __func__,
            __LINE__, url.c_str(), curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    file.close();

    ret = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    if (ret != CURLE_OK) {
        error_code = ECURLPOST;
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: curl getinfo failed: %s\n", __func__,
            __LINE__, curl_easy_strerror(ret));
        return RETURN_ERR;
    }

    return RETURN_OK;
}

int get_last_substring_after_slash(const char *str, char *sub_string, int sub_str_len,
    int &error_code)
{
    char *last_slash = NULL;
    int ret = 0;

    if ((str == NULL) || (sub_string == NULL) || (sub_str_len == 0)) {
        error_code = ENULL;
        wlan_emu_print(wlan_emu_log_level_err,
            "%s:%d: input arguements are NULL str : %p file_name : %p  sub_str_len : %d\n",
            __func__, __LINE__, str, sub_string, sub_str_len);
        return RETURN_ERR;
    }

    last_slash = strrchr((char *)str, '/');
    if (last_slash != NULL) {
        // Calculate the position of the last '/'
        int position = last_slash - str;

        // Extract the substring after the last '/'
        const char *tempstring = last_slash + 1;

        ret = snprintf(sub_string, sub_str_len, "%s", tempstring);

        if ((ret < 0) || (ret >= sub_str_len)) {
            error_code = ESNPRINTF;
            wlan_emu_print(wlan_emu_log_level_err,
                "%s:%d: snprintf failed return : %d input len : %d\n", __func__, __LINE__, ret,
                sub_str_len);
            return RETURN_ERR;
        }

        return RETURN_OK;
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: No '/' found in the string : %s\n", __func__,
            __LINE__, str);
    }

    error_code = EFPRESENCE;

    return RETURN_ERR;
}

int decode_param_string_fn(cJSON *json, const char *key, cJSON *&value)
{
    value = cJSON_GetObjectItem(json, key);
    if ((value == NULL) || (cJSON_IsString(value) == false) || (value->valuestring == NULL) ||
        (strcmp(value->valuestring, "") == 0)) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Validation failed for key:%s\n", __func__,
            __LINE__, key);
        return RETURN_ERR;
    }
    return RETURN_OK;
}

int WaitForDuration(int timeInMs)
{
    struct timespec ts;
    pthread_condattr_t cond_attr;
    pthread_cond_t cond;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    int ret = -1;

    pthread_condattr_init(&cond_attr);
    pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC);
    pthread_cond_init(&cond, &cond_attr);
    pthread_condattr_destroy(&cond_attr);
    clock_gettime(CLOCK_MONOTONIC, &ts);
    /* Add wait duration*/
    if (timeInMs > 1000) {
        ts.tv_sec += (timeInMs / 1000);
    } else {
        ts.tv_nsec = ts.tv_nsec + (timeInMs * CONVERT_MILLI_TO_NANO);
        ts.tv_sec = ts.tv_sec + ts.tv_nsec / 1000000000L;
        ts.tv_nsec = ts.tv_nsec % 1000000000L;
    }
    pthread_mutex_lock(&mutex);

    while (ret != 0 && ret != ETIMEDOUT) {
        ret = pthread_cond_timedwait(&cond, &mutex, &ts);
    }

    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: WaitForDuration completed successfully\n",
        __func__, __LINE__);
    pthread_mutex_unlock(&mutex);

    return ret;
}

int execute_process_once(std::string dhcp_cmd, pid_t *pid, bool wait_pid)
{
    pid_t pid_ret;
    int status;
    std::string temp_cmd = dhcp_cmd.c_str();

    std::istringstream iss(dhcp_cmd);
    std::vector<std::string> tokens;
    std::string token;
    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);

    while (iss >> token)
        tokens.push_back(token);

    // Convert to char* array
    std::vector<char *> argv;
    for (auto &t : tokens)
        argv.push_back(&t[0]); // get mutable char* pointer
    argv.push_back(NULL); // argv must be NULL-terminated

    // Set child to run in new process group
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&attr, 0); // 0 = new pgid == pid

    //  int ret = posix_spawn(&pid_ret, "/sbin/udhcpc", NULL, NULL, argv.data(), NULL);
    //    int ret = posix_spawn(&pid_ret, argv[0], NULL, NULL, argv.data(), NULL);
    int ret = posix_spawn(&pid_ret, argv[0], NULL, &attr, argv.data(), NULL);
    if (ret != 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: spawn failed with %d \n", __func__, __LINE__,
            ret);
        return RETURN_ERR;
    }

    posix_spawnattr_destroy(&attr);

    if (wait_pid == false) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: waitpid is false\n", __func__, __LINE__);
        *pid = pid_ret;
        return RETURN_OK;
    }

    if (waitpid(pid_ret, &status, 0) == -1) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: waitpid failed\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        wlan_emu_print(wlan_emu_log_level_info, "%s:%d: %s exited with code %d\n", __func__,
            __LINE__, argv[0], exit_code);
        *pid = pid_ret;
        return exit_code;
    } else if (WIFSIGNALED(status)) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: %s killed by signal %d\n", __func__,
            __LINE__, argv[0], WTERMSIG(status));
        return RETURN_ERR;
    }

    return RETURN_ERR;
}

int get_ip_from_interface_name(const std::string &if_name, std::string &ip_address)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Socket Error\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name.c_str(), IFNAMSIZ - 1);

    if (ioctl(fd, SIOCGIFADDR, &ifr) == -1) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Ioctl Error\n", __func__, __LINE__);
        close(fd);
        return RETURN_ERR;
    }

    close(fd);

    struct sockaddr_in *ipaddr = (struct sockaddr_in *)&ifr.ifr_addr;
    ip_address = inet_ntoa(ipaddr->sin_addr);

    return RETURN_OK;
}

int is_process_running(pid_t pid)
{
    struct stat info;
    std::string path = "/proc/" + std::to_string(pid);
    return stat(path.c_str(), &info) == 0;
}

// Get MAC and IPv4 address from interface name
int get_mac_ip_from_ifname(const char *ifname, unsigned char *mac, char *ip)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to open socket for %s\n", __func__,
            __LINE__, ifname);
        return RETURN_ERR;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

    // Get MAC address
    if (ioctl(fd, SIOCGIFHWADDR, &ifr) != 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: ioctl SIOCGIFHWADDR failed for %s\n",
            __func__, __LINE__, ifname);
        close(fd);
        return RETURN_ERR;
    }
    memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);

    // Get IP address
    if (ioctl(fd, SIOCGIFADDR, &ifr) != 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: ioctl SIOCGIFADDR failed for %s\n", __func__,
            __LINE__, ifname);
        close(fd);
        return RETURN_ERR;
    }
    struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;
    strncpy(ip, inet_ntoa(sin->sin_addr), 16);
    close(fd);
    return RETURN_OK;
}

int list_interfaces_in_namespace()
{
    struct ifaddrs *ifaddr, *ifa;

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return -1;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_name)
            continue;

        // wlan_emu_print(wlan_emu_log_level_info, "\n Interface: %s\t", ifa->ifa_name);

        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
            char ip[INET_ADDRSTRLEN];
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            inet_ntop(AF_INET, &(sa->sin_addr), ip, INET_ADDRSTRLEN);
            wlan_emu_print(wlan_emu_log_level_info, "\n Interface: %s  IP: %s", ifa->ifa_name, ip);
        }
    }

    freeifaddrs(ifaddr);
    return 0;
}

int get_mac_ip_from_ifname_ns(const char *ifname, const char *netns_path, unsigned char *mac,
    char *ip)
{
    int orig_ns = -1, ns_fd = -1, fd = -1;
    int ret = RETURN_ERR;
    struct ifreq ifr;
    struct sockaddr_in *sin;

    if (!ifname || !netns_path || !mac || !ip) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid arguments\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    // Open current namespace to restore later
    orig_ns = open("/proc/self/ns/net", O_RDONLY);
    if (orig_ns == -1) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to open current netns\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    // Open target namespace
    ns_fd = open(netns_path, O_RDONLY);
    if (ns_fd == -1) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to open netns %s\n", __func__,
            __LINE__, netns_path);
        close(orig_ns);
        return RETURN_ERR;
    }

    // Switch to target namespace
    if (setns(ns_fd, CLONE_NEWNET) == -1) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: setns failed for %s\n", __func__, __LINE__,
            netns_path);
        close(ns_fd);
        close(orig_ns);
        return RETURN_ERR;
    }
    close(ns_fd);

    // Open socket
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to open socket\n", __func__,
            __LINE__);
        goto cleanup; // jump to cleanup to restore namespace and close fds
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

    // Get MAC address
    if (ioctl(fd, SIOCGIFHWADDR, &ifr) != 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: ioctl SIOCGIFHWADDR failed for %s\n",
            __func__, __LINE__, ifname);
        goto cleanup;
    }
    memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);

    // Get IP address
    if (ioctl(fd, SIOCGIFADDR, &ifr) != 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: ioctl SIOCGIFADDR failed for %s\n", __func__,
            __LINE__, ifname);
        goto cleanup;
    }
    sin = (struct sockaddr_in *)&ifr.ifr_addr;
    strncpy(ip, inet_ntoa(sin->sin_addr), 16);

    ret = RETURN_OK;

cleanup:
    if (fd >= 0)
        close(fd);

    // Restore original namespace
    if (orig_ns >= 0) {
        if (setns(orig_ns, CLONE_NEWNET) != 0) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: restore setns failed\n", __func__,
                __LINE__);
        }
        close(orig_ns);
    }

    return ret;
}

int open_current_namespace()
{
    int fd = open("/proc/self/ns/net", O_RDONLY);
    if (fd < 0) {
        perror("open /proc/self/ns/net");
    }
    return fd;
}

int switch_to_namespace(const char *netns_path)
{
    int orig_ns_fd = open_current_namespace();
    if (orig_ns_fd < 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: invalid fd\n", __func__, __LINE__);
        return -1;
    }

    int new_ns_fd = open(netns_path, O_RDONLY);
    if (new_ns_fd < 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: open %s error\n", __func__, __LINE__,
            netns_path);
        close(orig_ns_fd);
        return -1;
    }

    if (setns(new_ns_fd, CLONE_NEWNET) != 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: setns error\n", __func__, __LINE__);
        close(orig_ns_fd);
        close(new_ns_fd);
        return -1;
    }
    close(new_ns_fd);

    return orig_ns_fd;
}

int restore_original_namespace(int orig_ns_fd)
{
    if (orig_ns_fd < 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: invalid fd\n", __func__, __LINE__);
        return -1;
    }

    if (setns(orig_ns_fd, CLONE_NEWNET) != 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: restore setns error\n", __func__, __LINE__);
        close(orig_ns_fd);
        return -1;
    }
    close(orig_ns_fd);
    return 0;
}

std::string get_current_namespace()
{
    char path[PATH_MAX];
    char link[PATH_MAX];
    ssize_t len;

    snprintf(path, sizeof(path), "/proc/self/ns/net");

    len = readlink(path, link, sizeof(link) - 1);
    if (len != -1) {
        link[len] = '\0';
        wlan_emu_print(wlan_emu_log_level_info, "%s:%d: namespace : %s\n", __func__, __LINE__,
            link);
        return std::string(link);
    } else {
        wlan_emu_print(wlan_emu_log_level_info, "%s:%d: unknown namespace\n", __func__, __LINE__);
        return std::string("unknown");
    }
}

int is_valid_ip(const char *addr)
{
    struct sockaddr_in sa;
    struct sockaddr_in6 sa6;

    if (inet_pton(AF_INET, addr, &(sa.sin_addr)) == 1) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: unable to resolve ipv4 addr: %s\n", __func__,
            __LINE__, addr);
        return RETURN_ERR;
    }

    // Try IPv6
    if (inet_pton(AF_INET6, addr, &(sa6.sin6_addr)) == 1) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: unable to resolve ipv6 addr: %s\n", __func__,
            __LINE__, addr);
        return RETURN_ERR;
    }

    return RETURN_OK;
}

int is_resolvable_hostname(const char *hostname)
{
    struct addrinfo hints = { 0 }, *res = NULL;
    hints.ai_family = AF_UNSPEC; // Both IPv4 and IPv6

    if (getaddrinfo(hostname, NULL, &hints, &res) == 0) {
        freeaddrinfo(res);
        return RETURN_OK;
    }

    wlan_emu_print(wlan_emu_log_level_err, "%s:%d: unable to resolve hostname : %s\n", __func__,
        __LINE__, hostname);
    return RETURN_ERR;
}
