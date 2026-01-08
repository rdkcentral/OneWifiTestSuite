#! /bin/sh

ONEWIFI_TESTSUITE_CFG="`syscfg get onewifi_testsuite`"
ONEWIFI_TESTSUITE_TMPFILE="/tmp/onewifi_testsuite_configured"
ONEWIFI_SIM_CLI_COUNT="`syscfg get onewifi_suite_sim_cli_count`"
action=$1
temp_max_sim_clients=3

if [ "$action" = "start" ]; then
    if [ "$ONEWIFI_TESTSUITE_CFG" != "true" ]; then
        echo "Exiting the script..."
        exit 0
    fi

    if [[ "$ONEWIFI_SIM_CLI_COUNT" =~ ^[0-9]+$ ]]; then
        if [ "$ONEWIFI_SIM_CLI_COUNT" -ge 1 ] && [ "$ONEWIFI_SIM_CLI_COUNT" -le 300 ]; then
            temp_max_sim_clients=$ONEWIFI_SIM_CLI_COUNT
        fi
    fi

    echo "Starting Test Suite with max clients $temp_max_sim_clients.."
    modprobe rdkfmac max_sim_clients=$temp_max_sim_clients
    ifconfig nl_msg_mon0 up
    ifconfig hwsim0 up
    cci &
    touch $ONEWIFI_TESTSUITE_TMPFILE
elif [ "$action" = "stop" ]; then
    echo "Stopping Test Suite.."
    ifconfig -a | grep wlan | cut -d ' ' -f 1 | while IFS= read -r line; do
        ifconfig $line down
    done
    ifconfig nl_msg_mon0 down
    ifconfig hwsim0 down
    killall cci
    if [ -e "$ONEWIFI_TESTSUITE_TMPFILE" ]; then
        rm -rf $ONEWIFI_TESTSUITE_TMPFILE
    fi
    rmmod rdkfmac
fi
