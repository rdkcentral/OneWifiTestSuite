#ifndef RDKFMAC_CMD_H
#define RDKFMAC_CMD_H

int rdkfmac_cmd_send_add_intf(rdkfmac_vif_t *vif, enum nl80211_iftype iftype,
               int use4addr, u8 *mac_addr);

int rdkfmac_cmd_get_mac_info(rdkfmac_wmac_t *mac);

#endif // RDKFMAC_CMD_H
