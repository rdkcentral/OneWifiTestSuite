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
# feature flag. Wait while the value is empty (not yet ready); an explicit "false"
# means the feature is off and we exit immediately.
ONEWIFI_TESTSUITE_WAIT_SECS=120
wait_for_testsuite_cfg() {
    i=0
    while [ "$i" -lt "$ONEWIFI_TESTSUITE_WAIT_SECS" ]; do
        ONEWIFI_TESTSUITE_CFG="`syscfg get onewifi_testsuite`"
        if [ -n "$ONEWIFI_TESTSUITE_CFG" ]; then
            return 0
        fi
        if [ "$i" -eq 0 ]; then
            log_msg "onewifi_testsuite empty, waiting up to ${ONEWIFI_TESTSUITE_WAIT_SECS}s for RFC/syscfg to populate it"
        fi
        i=`expr "$i" + 1`
        sleep 1
    done
    ONEWIFI_TESTSUITE_CFG="`syscfg get onewifi_testsuite`"
    return 1
}

if [ "$action" = "start" ]; then
    log_msg "Handling 'start' action"
    wait_for_testsuite_cfg
    ONEWIFI_SIM_CLI_COUNT="`syscfg get onewifi_suite_sim_cli_count`"
    log_msg "onewifi_testsuite=[$ONEWIFI_TESTSUITE_CFG]"
    log_msg "onewifi_suite_sim_cli_count=[$ONEWIFI_SIM_CLI_COUNT]"
    log_msg "default max_sim_clients=$temp_max_sim_clients"
    if [ "$ONEWIFI_TESTSUITE_CFG" != "true" ]; then
        log_msg "onewifi_testsuite is not 'true' (raw value=[$ONEWIFI_TESTSUITE_CFG]), exiting the script"
        echo "Exiting the script..."
        exit 0
    fi

    if [[ "$ONEWIFI_SIM_CLI_COUNT" =~ ^[0-9]+$ ]]; then
        log_msg "sim client count is numeric, validating range"
        if [ "$ONEWIFI_SIM_CLI_COUNT" -ge 1 ] && [ "$ONEWIFI_SIM_CLI_COUNT" -le 300 ]; then
            temp_max_sim_clients=$ONEWIFI_SIM_CLI_COUNT
            log_msg "using configured max_sim_clients=$temp_max_sim_clients"
        else
            log_msg "sim client count out of range (1-300), using default $temp_max_sim_clients"
        fi
    else
        log_msg "sim client count not numeric, using default $temp_max_sim_clients"
    fi

    echo "Starting Test Suite with max clients $temp_max_sim_clients.."
    log_msg "Starting Test Suite with max clients $temp_max_sim_clients"
    run_cmd modprobe rdkfmac max_sim_clients=$temp_max_sim_clients
    run_cmd ifconfig nl_msg_mon0 up
    run_cmd ifconfig hwsim0 up

    # Verify the cci binary is resolvable in the service's PATH before launching
    cci_path=`command -v cci 2>/dev/null`
    if [ -z "$cci_path" ]; then
        log_msg "ERROR: cci binary not found in PATH ($PATH)"
    else
        log_msg "cci resolved to $cci_path"
    fi
    if pgrep -x cci >/dev/null 2>&1; then
        log_msg "WARNING: a cci process is already running (pids: `pgrep -x cci | tr '\n' ' '`)"
    fi

    log_msg "Launching cci in background"
    cci >> "$CCI_LOG_FILE" 2>&1 &
    cci_pid=$!
    log_msg "cci started with pid $cci_pid"

    # Confirm cci survived startup; a background launch alone does not prove it stayed up
    sleep 1
    if kill -0 "$cci_pid" 2>/dev/null; then
        log_msg "cci still alive after 1s (pid $cci_pid)"
    else
        wait "$cci_pid" 2>/dev/null
        log_msg "ERROR: cci EXITED early (pid $cci_pid, rc=$?)"
    fi

    run_cmd touch $ONEWIFI_TESTSUITE_TMPFILE
    log_msg "start action completed"
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
        log_msg "Removing $ONEWIFI_TESTSUITE_TMPFILE"
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
