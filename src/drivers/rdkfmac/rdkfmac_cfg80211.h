#ifndef RDKFMAC_CFG80211_H
#define RDKFMAC_CFG80211_H

int rdkfmac_wiphy_register(rdkfmac_hw_info_t *hw_info, rdkfmac_wmac_t *mac);
struct wiphy *rdkfmac_wiphy_allocate(rdkfmac_bus_t *bus, struct platform_device *pdev);

#endif // RDKFMAC_CFG80211_H
