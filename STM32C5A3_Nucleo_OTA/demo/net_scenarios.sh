#!/usr/bin/env bash
# Network OTA scenarios: v1 -> v2 (auto at boot), tampered v3 rejected,
# v3 accepted, downgrade to v1 refused. Switches the image the OTA server
# serves by re-pointing a symlink (the server re-reads it on every request).
# Usage: net_scenarios.sh <ST-LINK SN> <uart-monitor label> <serve symlink> <log dir>
set -u
SN=$1; LABEL=$2; SERVE=$3; LOGDIR=$4
OUT=$(cd "$(dirname "$0")" && pwd)/out
PROG=${PROG:-STM32_Programmer_CLI}
LOG=/tmp/uart-monitor/latest/$LABEL.log
PTY=/tmp/uart-monitor/pty/$LABEL
mkdir -p "$LOGDIR"
capture() {    # capture <name> <regex> <count> <timeout s>
    local t0=$SECONDS
    while :; do
        tail -c +$((OFF + 1)) "$LOG" | tr -d '\r' > "$LOGDIR/$1.log"
        [ "$(grep -acE "$2" "$LOGDIR/$1.log")" -ge "$3" ] && break
        if [ $((SECONDS - t0)) -ge "$4" ]; then echo "  (timeout $4 s)"; break; fi
        sleep 0.5
    done
    sleep 1; tail -c +$((OFF + 1)) "$LOG" | tr -d '\r' > "$LOGDIR/$1.log"
}
serve() { ln -sf "$1" "$SERVE"; }

echo "1. factory flash v1; boot, OTA to v2 at boot"
serve "$OUT/app_v2_signed.bin"
"$PROG" -c port=swd sn=$SN mode=UR -ob displ 2>/dev/null | grep -q 'SWAP_BANK *: 0x1' && \
    "$PROG" -c port=swd sn=$SN mode=UR -ob SWAP_BANK=0 > /dev/null 2>&1
OFF=$(stat -c %s "$LOG")
"$PROG" -c port=swd sn=$SN mode=UR -e all -d "$OUT/wolfboot.bin" 0x08000000 \
    -d "$OUT/app_v1_signed.bin" 0x08010000 -v -rst > /dev/null 2>&1
capture net1_v1_boot_ota_to_v2 "firmware version 2" 1 240

echo "2. serve tampered v3, check (wolfBoot must reject)"
serve "$OUT/app_v3_tampered.bin"
OFF=$(stat -c %s "$LOG"); printf 'u' > "$PTY"
capture net2_tampered_v3_rejected "Ready:" 1 180

echo "3. serve v3, check (accepted)"
serve "$OUT/app_v3_signed.bin"
OFF=$(stat -c %s "$LOG"); printf 'u' > "$PTY"
capture net3_update_to_v3 "Ready:" 1 180

echo "4. serve v1, check (downgrade must be refused)"
serve "$OUT/app_v1_signed.bin"
OFF=$(stat -c %s "$LOG"); printf 'u' > "$PTY"
capture net4_downgrade_refused "Ready:" 1 180
serve "$OUT/app_v2_signed.bin"
echo "logs in $LOGDIR"
