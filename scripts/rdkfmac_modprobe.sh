#! /bin/sh
#
# Copyright 2025 Comcast Cable Communications Management, LLC

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
#
#


ONEWIFI_TESTSUITE_TMPFILE="/tmp/onewifi_testsuite_configured"
ONEWIFI_SIM_CLI_COUNT="`syscfg get onewifi_suite_sim_cli_count`"
action=$1
temp_max_sim_clients=3

ONEWIFI_TESTSUITE_WAIT_SECS=40
wait_for_testsuite_cfg() {
    i=0
    while [ "$i" -lt "$ONEWIFI_TESTSUITE_WAIT_SECS" ]; do
        ONEWIFI_TESTSUITE_CFG="`syscfg get onewifi_testsuite`"
        if [ -n "$ONEWIFI_TESTSUITE_CFG" ]; then
            return 0
        fi
        i=`expr "$i" + 1`
        sleep 1
    done
    ONEWIFI_TESTSUITE_CFG="`syscfg get onewifi_testsuite`"
    return 1
}

if [ "$action" = "start" ]; then
    wait_for_testsuite_cfg
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