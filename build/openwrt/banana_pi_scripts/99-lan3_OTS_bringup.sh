#!/bin/sh

logger -t hotplug-test "Hotplug triggered for INTERFACE=$INTERFACE ACTION=$ACTION DEVICE=$DEVICE"


[ "$INTERFACE" = "lan3" ] || exit 0

if [ "$ACTION" = "ifup" ]; then
    logger -t lan3-monitor "Device connected to lan3, starting namespace"
    ip netns add ots
    ip link set lan3 netns ots
    ip netns exec ots ip link set lo up
    ip netns exec ots ip link set lan3 up
    mac=$(ip netns exec ots ip link show lan3 | awk '/link\/ether/ {gsub(":", "", $2); print $2}')
    hostname="ots_ext_agent-${mac}"
    ip netns exec ots udhcpc -i lan3 -x hostname:${hostname}
    ip netns exec ots ip addr show lan3
    ip netns exec ots pkill udhcpc

elif [ "$ACTION" = "ifdown" ]; then
    logger -t lan3-monitor "Device disconnected from lan3"
fi

exit
