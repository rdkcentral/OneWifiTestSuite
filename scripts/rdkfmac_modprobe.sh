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

CCI_LOG_FILE="/tmp/cci_init_script"

log_msg() {
    echo "[`date '+%Y-%m-%d %H:%M:%S'`] $*" >> "$CCI_LOG_FILE"
}

# run a command, log it and its output/exit status
run_cmd() {
    log_msg "CMD: $*"
    out=`"$@" 2>&1`
    rc=$?
    if [ -n "$out" ]; then
        log_msg "OUT: $out"
    fi
    log_msg "RC : $rc"
    return $rc
}

touch "$CCI_LOG_FILE" 2>/dev/null
log_msg "==================== script invoked: $0 $* ===================="

ONEWIFI_TESTSUITE_TMPFILE="/tmp/onewifi_testsuite_configured"
action=$1
temp_max_sim_clients=3

log_msg "action=$action"

# syscfg can be unpopulated when this runs early at boot, before RFC applies the
# feature flag. Poll while the value is empty; "true"/"false" means it is resolved.
ONEWIFI_TESTSUITE_WAIT_SECS=30
wait_for_testsuite_cfg() {
    i=0
    while [ "$i" -lt "$ONEWIFI_TESTSUITE_WAIT_SECS" ]; do
        ONEWIFI_TESTSUITE_CFG="`syscfg get onewifi_testsuite`"
        if [ "$ONEWIFI_TESTSUITE_CFG" = "true" ] || [ "$ONEWIFI_TESTSUITE_CFG" = "false" ]; then
            log_msg "onewifi_testsuite resolved to [$ONEWIFI_TESTSUITE_CFG] after ${i}s"
            return 0
        fi
        if [ "$i" -eq 0 ]; then
            log_msg "onewifi_testsuite empty, waiting up to ${ONEWIFI_TESTSUITE_WAIT_SECS}s for RFC/syscfg to populate it"
        fi
        i=`expr "$i" + 1`
        sleep 1
    done
    log_msg "onewifi_testsuite still unresolved after ${ONEWIFI_TESTSUITE_WAIT_SECS}s"
    return 1
}

if [ "$action" = "start" ]; then
    log_msg "Handling 'start' action"

    wait_for_testsuite_cfg
    log_msg "onewifi_testsuite=[$ONEWIFI_TESTSUITE_CFG]"

    case "$ONEWIFI_TESTSUITE_CFG" in
        true)  ;;
        false) log_msg "feature disabled, exiting"; echo "Exiting the script..."; exit 0 ;;
        # Still empty after the wait; exit non-zero so systemd retries the unit.
        *)     log_msg "onewifi_testsuite not populated yet"; exit 1 ;;
    esac

    ONEWIFI_SIM_CLI_COUNT="`syscfg get onewifi_suite_sim_cli_count`"
    log_msg "onewifi_suite_sim_cli_count=[$ONEWIFI_SIM_CLI_COUNT]"

    if [[ "$ONEWIFI_SIM_CLI_COUNT" =~ ^[0-9]+$ ]]; then
        if [ "$ONEWIFI_SIM_CLI_COUNT" -ge 1 ] && [ "$ONEWIFI_SIM_CLI_COUNT" -le 300 ]; then
            temp_max_sim_clients=$ONEWIFI_SIM_CLI_COUNT
        else
            log_msg "sim client count out of range (1-300), using default $temp_max_sim_clients"
        fi
    else
        log_msg "sim client count not numeric, using default $temp_max_sim_clients"
    fi
    log_msg "max_sim_clients=$temp_max_sim_clients"

    echo "Starting Test Suite with max clients $temp_max_sim_clients.."
    run_cmd modprobe rdkfmac max_sim_clients=$temp_max_sim_clients || exit 1
    run_cmd ifconfig nl_msg_mon0 up || exit 1
    run_cmd ifconfig hwsim0 up || exit 1
    run_cmd touch $ONEWIFI_TESTSUITE_TMPFILE

    log_msg "exec cci"
    # exec so cci becomes the unit's main process and its exit status reaches systemd
    exec cci >> "$CCI_LOG_FILE" 2>&1
elif [ "$action" = "stop" ]; then
    log_msg "Handling 'stop' action"
    echo "Stopping Test Suite.."
    ifconfig -a | grep wlan | cut -d ' ' -f 1 | while IFS= read -r line; do
        log_msg "Bringing down interface $line"
        run_cmd ifconfig $line down
    done
    run_cmd ifconfig nl_msg_mon0 down
    run_cmd ifconfig hwsim0 down
    run_cmd killall cci
    if [ -e "$ONEWIFI_TESTSUITE_TMPFILE" ]; then
        run_cmd rm -rf $ONEWIFI_TESTSUITE_TMPFILE
    else
        log_msg "$ONEWIFI_TESTSUITE_TMPFILE not present, nothing to remove"
    fi
    run_cmd rmmod rdkfmac
    log_msg "stop action completed"
else
    log_msg "Unknown action '$action', nothing to do"
fi

log_msg "==================== script finished: $0 $* ===================="
