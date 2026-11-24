#!/usr/bin/env bash
# Exercise the wolfBoot scenarios without the network: images are staged in
# UPDATE over SWD and installed with the app's 't' console command.
#   1 first boot v1   2 update to v2   3 tampered v3 rejected
#   4 downgrade to v1 refused   5 power cut during the v3 swap, then resume
# Needs uart-monitor (proxy mode) for the console log and PTY.
#
# Usage: swd_scenarios.sh <ST-LINK SN> <uart-monitor label> <log dir>
set -u
SN=$1; LABEL=$2; LOGDIR=$3
OUT=$(cd "$(dirname "$0")" && pwd)/out
PROG=${PROG:-STM32_Programmer_CLI}
LOG=/tmp/uart-monitor/latest/$LABEL.log
PTY=/tmp/uart-monitor/pty/$LABEL
mkdir -p "$LOGDIR"

capture() {    # capture <name> <regex to stop on> <timeout s>
    local t0=$SECONDS
    while :; do
        tail -c +$((OFF + 1)) "$LOG" | tr -d '\r' > "$LOGDIR/$1.log"
        grep -aqE "$2" "$LOGDIR/$1.log" && break
        if [ $((SECONDS - t0)) -ge "$3" ]; then echo "  (timeout $3 s)"; break; fi
        sleep 0.5
    done
    sleep 1
    tail -c +$((OFF + 1)) "$LOG" | tr -d '\r' > "$LOGDIR/$1.log"
}
stage() {      # stage <image>: erase UPDATE + SWAP (sectors 64-120, which
               # clears a trailer left by a refused update), write the image
    "$PROG" -c port=swd sn=$SN mode=UR -e [64 120] > /dev/null 2>&1
    "$PROG" -c port=swd sn=$SN mode=UR -d "$1" 0x08080000 -v -rst > /dev/null 2>&1
}
ready() { OFF=$(stat -c %s "$LOG"); capture "$1" "Ready:" 90; }

echo "1. factory flash (wolfBoot + v1), first boot"
"$PROG" -c port=swd sn=$SN mode=UR -ob displ 2>/dev/null | grep -q 'SWAP_BANK *: 0x1' && \
    "$PROG" -c port=swd sn=$SN mode=UR -ob SWAP_BANK=0 > /dev/null 2>&1
OFF=$(stat -c %s "$LOG")
"$PROG" -c port=swd sn=$SN mode=UR -e all -d "$OUT/wolfboot.bin" 0x08000000 \
    -d "$OUT/app_v1_signed.bin" 0x08010000 -v -rst > /dev/null 2>&1
capture 1_first_boot_v1 "Ready:" 90

echo "2. stage v2 in UPDATE, install"
stage "$OUT/app_v2_signed.bin"; ready 2_pre
OFF=$(stat -c %s "$LOG"); printf 't' > "$PTY"
capture 2_update_to_v2 "Ready:" 120

echo "3. stage tampered v3, install (must be rejected)"
stage "$OUT/app_v3_tampered.bin"; ready 3_pre
OFF=$(stat -c %s "$LOG"); printf 't' > "$PTY"
capture 3_tampered_v3_rejected "Ready:" 120

echo "4. stage v1 (downgrade), install (must be refused)"
stage "$OUT/app_v1_signed.bin"; ready 4_pre
OFF=$(stat -c %s "$LOG"); printf 't' > "$PTY"
capture 4_downgrade_refused "Ready:" 120

echo "5. stage v3, install, cut the power mid-swap"
stage "$OUT/app_v3_signed.bin"; ready 5_pre
OFF=$(stat -c %s "$LOG"); printf 't' > "$PTY"
capture 5a_swap_started "Copy sector 8 " 60
"$PROG" -c port=swd sn=$SN mode=UR -hardRst > /dev/null 2>&1
OFF=$(stat -c %s "$LOG")
"$PROG" -c port=swd sn=$SN mode=UR -rst > /dev/null 2>&1
capture 5b_resumed_after_power_cut "Ready:" 180
echo "logs in $LOGDIR"
