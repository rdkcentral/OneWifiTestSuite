#include "rdkfmac.h"

rdkfmac_bus_t   rdkfmac_bus;
struct device   rdkfmac_device;

static const struct ethtool_ops rdkfmac_ethtool_ops = {
    .get_drvinfo = cfg80211_get_drvinfo,
};


int rdkfmac_bus_preinit(struct rdkfmac_bus *test)
{
    return 0;
}

void rdkfmac_bus_stop(struct rdkfmac_bus *test)
{

}

int rdkfmac_bus_control_tx(struct rdkfmac_bus *test1, struct sk_buff *test)
{
    return 0;
}

int rdkfmac_bus_data_tx(struct rdkfmac_bus *bus, struct sk_buff *skb,
               unsigned int macid, unsigned int vifid)
{
    return 0;
}

void rdkfmac_bus_data_tx_timeout(struct rdkfmac_bus *test, struct net_device *test1)
{

}

void rdkfmac_bus_data_tx_use_meta_set(struct rdkfmac_bus *bus, bool use_meta)
{

}

void rdkfmac_bus_data_rx_start(struct rdkfmac_bus *test)
{

}

void rdkfmac_bus_data_rx_stop(struct rdkfmac_bus *test)
{

}

rdkfmac_bus_ops_t   bus_ops = {
    .preinit = rdkfmac_bus_preinit,
    .stop = rdkfmac_bus_stop,
    .control_tx = rdkfmac_bus_control_tx,
    .data_tx = rdkfmac_bus_data_tx,
    .data_tx_timeout = rdkfmac_bus_data_tx_timeout,
    .data_tx_use_meta_set = rdkfmac_bus_data_tx_use_meta_set,
    .data_rx_start = rdkfmac_bus_data_rx_start,
    .data_rx_stop = rdkfmac_bus_data_rx_stop,
};

void rdkfmac_bus_pseudo_init_device(rdkfmac_bus_t *bus)
{
    //bus->dev = kzalloc(sizeof(struct device), GFP_KERNEL);
    //bus->dev = &rdkfmac_device;
    //memset(bus->dev, 0, sizeof(struct device));

    //bus->dev->init_name = RDKFMAC_DEVICE_NAME;
    //rdkfmac_device.init_name = RDKFMAC_DEVICE_NAME;
}

void rdkfmac_bus_pseudo_init(rdkfmac_bus_t *bus)
{
    rdkfmac_bus_pseudo_init_device(bus);
    bus->bus_ops = &bus_ops;
    bus->hw_info.num_mac = 3;
}

rdkfmac_bus_t *rdkfmac_get_bus(void)
{
    return &rdkfmac_bus;
}

