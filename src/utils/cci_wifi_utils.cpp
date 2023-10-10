#include "cci_wifi_utils.hpp"

char *mac_to_str(unsigned char *mac, char *s_mac)
{
    if((mac == NULL) || (s_mac == NULL)) {
        return NULL;
    }

    snprintf(s_mac, 18, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx", mac[0],mac[1], mac[2], mac[3], mac[4],mac[5]);

    return s_mac;
}

char *mac_str_without_colon(mac_address_t mac, mac_addr_str_t key)
{
    snprintf(key, 18, "%02x%02x%02x%02x%02x%02x",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return (char *)key;
}


int get_current_time_string(char *time_str, int time_str_len)
{
    time_t current_time;
    struct tm *timeinfo;
    char timestamp[16];
    int ret = 0;

    if ((time_str == NULL) || (time_str_len == 0)) {
        return RETURN_ERR;
    }
    time(&current_time);
    timeinfo = localtime(&current_time);

    // Format the timestamp
    strftime(timestamp, sizeof(timestamp), "%Y%m%d%H%M%S", timeinfo);

    ret = snprintf(time_str, time_str_len, "%s", timestamp);
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
        printf(" %s:%d input arguements are NULL cmd : %p value : %p", __func__, __LINE__, cmd, value);
        return RETURN_ERR;
    }

    printf("cmd : %s\n", cmd);

    memset (ptr, 0, sizeof(ptr));
    memset (str, 0, sizeof(str));

    fp = popen(cmd, "r");
    if (fp == NULL) {
        printf(" %s:%d popen failed for cmd %s", __func__, __LINE__, cmd);
        return RETURN_ERR;
    }

    // Read and print the output of the cmd
    while (fgets(ptr, sizeof(ptr), fp) != NULL) {
        bytes_written += snprintf(&str[bytes_written], sizeof(str)-bytes_written, "%s", ptr);
    }

    pclose(fp);
    temp_str = strstr(str, search_str);
    if (temp_str == NULL) {
        printf(" %s:%d unable to find search_str : %s for cmd %s", __func__, __LINE__, search_str, cmd);
        return RETURN_ERR;
    } else {
        temp_str += strlen(search_str);
        length = strlen(temp_str);

        while (i < length){
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



int chann_to_freq(unsigned char chan)
{
    if (chan >= MIN_CHANNEL_2G && chan <= MAX_CHANNEL_2G) {
        return 2407 + 5 * chan;
    }

    if (chan >= MIN_CHANNEL_5G && chan <= MAX_CHANNEL_5G) {
        return 5000 + 5 * chan;
    }

    printf("%s:%d: Failed to convert channel %u to frequency\n", __func__, __LINE__,
            chan);

    return RETURN_OK;
}

uint16_t inet_csum(const void *buf, size_t hdr_len)
{
    unsigned long sum = 0;
    const uint16_t *ip1;

    ip1 = (const uint16_t *) buf;
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

    return(~sum);
}


int send_raw_packet(const void *data, size_t data_len,
        uint32_t source_nip, int source_port,
        uint32_t dest_nip, int dest_port, const uint8_t *dest_arp,
        int ifindex)
{
    struct sockaddr_ll dest_sll;
    int fd;
    int res = -1;
    char *buf;
    size_t sz;
    struct iphdr *ip;
    struct udphdr *udp;

    fd = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));
    if (fd < 0) {
        printf("%s:%d: Failed to open socket\n", __func__, __LINE__);
        return res;
    }

    sz = sizeof(struct iphdr) + sizeof(struct udphdr) + data_len;
    buf = (char *)malloc(sz);

    if (buf == NULL)
        goto ret_close;

    ip = (struct iphdr *)buf;
    udp = (struct udphdr *)(ip + sizeof(iphdr));

    memset(&dest_sll, 0, sizeof(dest_sll));
    memcpy(buf + sizeof(struct iphdr) + sizeof(struct udphdr), data, data_len);

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

    ip->protocol = IPPROTO_UDP;
    ip->saddr = source_nip;
    ip->daddr = dest_nip;
    udp->source = htons(source_port);
    udp->dest = htons(dest_port);
    udp->len = htons(sz);
    ip->tot_len = udp->len;
    udp->check = inet_csum(buf, sz);
    ip->tot_len = htons(sz);
    ip->ihl = sizeof(iphdr) >> 2;
    ip->version = IPVERSION;
    ip->ttl = IPDEFTTL;
    ip->check = inet_csum(ip, sizeof(iphdr));

    res = sendto(fd, buf, sz, 0, (struct sockaddr *) &dest_sll, sizeof(dest_sll));

    if (res < 0) {
        printf("%s:%d: Failed to send data, error %d\n", __func__, __LINE__, res);
    }

    free(buf);

 ret_close:
    close(fd);
    return res;
}

