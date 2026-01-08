#!/bin/sh
output=$(dmcli eRT getv Device.DeviceInfo.X_RDKCENTRAL-COM_Syndication.PartnerId)

json_data='{
  "PhyList": [
    {
      "Index": 0,
      "RadioList": [
        {
          "Index": 0,
          "RadioName": "wlan0",
          "InterfaceList": [
            {
              "InterfaceName": "wlan0",
              "Bridge": "brlan0",
              "vlanId": 0,
              "vapIndex": 0,
              "vapName": "mesh_sta_5g"
            }
          ]
        }
      ]
    }
  ]
}'

echo "$json_data" > /nvram/InterfaceMap.json
if echo "$output" | grep -q "RDKM"; then
	dmcli eRT setv Device.DeviceInfo.X_RDKCENTRAL-COM_Syndication.PartnerId string "sky-uk"
	dmcli eRT setv Device.DeviceInfo.X_RDKCENTRAL-COM_Syndication.RDKB_Control.ActivatePartnerId bool true
fi

sleep 2
cci &
brctl delif brlan0 eth1
udhcpc -i eth1 -x hostname:raspberrypi_ext -b &

brctl delif brlan0 eth2
udhcpc -i eth2 -x hostname:raspberrypi_ext -b &
