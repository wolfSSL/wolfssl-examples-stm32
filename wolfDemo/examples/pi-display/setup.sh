#!/bin/sh
# setup.sh
#
# Copyright (C) 2026 wolfSSL Inc.
#
# This file is part of wolfssl-examples.
#
# wolfssl-examples is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# wolfssl-examples is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
#
# Turns a Raspberry Pi running Raspberry Pi OS Lite into a display for the
# wolfDemo board. After a reboot bench_tui.py runs full screen on the HDMI
# display whenever the board is plugged into the USB port. It runs in the foot
# terminal under the cage kiosk compositor, which gives it 24-bit colour,
# TrueType fonts and sixel graphics for the wolfSSL logo.
#
# Usage: sudo ./setup.sh [directory with bench_tui.py and wolfssl_logo.png]
#
# Variables:
#   FONT_SIZE = 32     (font height in pixels, bigger text shows fewer rows)
#   THEME     = light  (light for a white background, or dark for black)

set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
SRC=${1:-$HERE/..}
FONT_SIZE=${FONT_SIZE:-32}
LINE_HEIGHT=$(((FONT_SIZE * 115 + 50) / 100))
THEME=${THEME:-light}

if [ "$(id -u)" -ne 0 ]; then
    echo "Run this as root: sudo $0" >&2
    exit 1
fi
# The terminal's background and text colours, to match bench_tui.py's theme
case $THEME in
    light) BACKGROUND=ffffff FOREGROUND=1f2328 ;;
    dark) BACKGROUND=000000 FOREGROUND=d0d0d0 ;;
    *)
        echo "THEME must be light or dark" >&2
        exit 1
        ;;
esac
for f in bench_tui.py wolfssl_logo.png; do
    if [ ! -f "$SRC/$f" ]; then
        echo "Cannot find $f in $SRC" >&2
        exit 1
    fi
done
if [ -d /boot/firmware ]; then
    BOOT=/boot/firmware
else
    BOOT=/boot
fi

echo "Installing packages"
apt-get update
apt-get install -y --no-install-recommends cage foot seatd \
    fonts-jetbrains-mono python3-pil python3-serial

echo "Installing the display service"
if ! id wolfdemo >/dev/null 2>&1; then
    useradd --system --no-create-home --home-dir /var/lib/wolfdemo \
        --shell /usr/sbin/nologin wolfdemo
fi
# The serial port, and the display for cage, from seatd (video) and the GPU
usermod -a -G dialout,video,render wolfdemo
install -d /opt/wolfdemo
install -m 0644 "$SRC/bench_tui.py" "$SRC/wolfssl_logo.png" /opt/wolfdemo/
sed -e "s|@FONT_SIZE@|$FONT_SIZE|" -e "s|@LINE_HEIGHT@|$LINE_HEIGHT|" \
    -e "s|@BACKGROUND@|$BACKGROUND|" -e "s|@FOREGROUND@|$FOREGROUND|" \
    "$HERE/foot.ini" > /opt/wolfdemo/foot.ini
install -m 0644 "$HERE/99-wolfdemo.rules" /etc/udev/rules.d/
sed "s|@THEME@|$THEME|" "$HERE/wolfdemo-display.service" \
    > /etc/systemd/system/wolfdemo-display.service

# cage shows a mouse pointer in the middle of the screen, even without a
# mouse. Replace the default cursor theme with one where every pointer is a
# single transparent pixel, in the Xcursor file format.
CURSORS=/opt/wolfdemo/icons/default/cursors
install -d "$CURSORS"
python3 -c 'import struct, sys; sys.stdout.buffer.write(b"Xcur"
    + struct.pack("<6I", 16, 0x10000, 1, 0xfffd0002, 1, 28)
    + struct.pack("<9I", 36, 0xfffd0002, 1, 1, 1, 1, 0, 0, 0) + bytes(4))' \
    > "$CURSORS/blank"
for name in default left_ptr text xterm pointer hand2 wait watch progress; do
    ln -sf blank "$CURSORS/$name"
done

echo "Configuring $BOOT/cmdline.txt"
# Hide the boot messages and logo. The system console moves to tty3 so that
# kernel, systemd and cloud-init messages do not appear on tty1, which the
# display uses.
CMDLINE=$BOOT/cmdline.txt
[ -f "$CMDLINE.orig" ] || cp "$CMDLINE" "$CMDLINE.orig"
ARGS=""
set -f
# shellcheck disable=SC2013 # cmdline.txt is one line of words
for arg in $(cat "$CMDLINE"); do
    case $arg in
        console=tty[0-9]*) ARGS="$ARGS console=tty3" ;;
        quiet|logo.nologo) ;;
        *) ARGS="$ARGS $arg" ;;
    esac
done
set +f
echo "${ARGS# } quiet logo.nologo" > "$CMDLINE"

# Hide the rainbow splash screen. Later lines override earlier ones, so this
# also overrides an earlier disable_splash=0.
if ! grep -q '^disable_splash=1' "$BOOT/config.txt"; then
    printf '\n[all]\ndisable_splash=1\n' >> "$BOOT/config.txt"
fi

udevadm control --reload-rules
udevadm trigger --subsystem-match=tty
systemctl daemon-reload
systemctl disable getty@tty1.service
systemctl enable seatd.service wolfdemo-display.service

echo "Done, reboot to start the display"
