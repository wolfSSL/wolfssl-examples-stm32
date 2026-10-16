# Raspberry Pi wolfDemo benchmark display

This turns a Raspberry Pi Zero (or any other Raspberry Pi) into a standalone
display for the wolfDemo board. Plug the board into the Pi's USB port, connect
a monitor to the HDMI port, and [bench_tui.py](../bench_tui.py) runs full
screen on boot with large text and the wolfSSL logo.

There is no desktop. A systemd service runs the TUI in the
[foot](https://codeberg.org/dnkl/foot) terminal under the
[cage](https://github.com/cage-kiosk/cage) kiosk compositor, at the monitor's
native resolution. foot gives it 24-bit colour, TrueType fonts (JetBrains
Mono, with slashed zeros) and sixel graphics, which bench_tui.py uses to draw
the wolfSSL logo image ([wolfssl_logo.png](../wolfssl_logo.png)). It shows a
waiting message when the board is not plugged in, and the board can be
unplugged and plugged back in at any time.

The display appears about 90 seconds after the Pi Zero is switched on.

## Hardware

* Raspberry Pi Zero W (the board is marked "Zero W V1.1") or Raspberry Pi Zero.
* microSD card, 8 GB or larger.
* Mini HDMI to HDMI cable or adapter.
* A 5V 2.5A micro USB power supply. The wolfDemo board is powered from the
  Pi, so a weak supply will cause under-voltage problems.
* A micro USB OTG adapter (micro USB male to USB-A female) and a USB-A to
  USB-C cable for the wolfDemo board.

The Pi Zero has two micro USB ports. The one in the middle marked "USB" is the
data port for the wolfDemo board, the one at the corner marked "PWR IN" is for
the power supply.

## Install

1. Use [Raspberry Pi Imager](https://www.raspberrypi.com/software/) to write
   **Raspberry Pi OS Lite (32-bit)** to the microSD card. In the customisation
   settings set a hostname (such as `wolfdemo`), a username and password,
   enable SSH and, for the Zero W, set up WiFi.

   The plain Pi Zero has no WiFi. For setup, use a USB Ethernet adapter on the
   OTG port in place of the wolfDemo board.

2. Boot the Pi and copy `bench_tui.py`, `wolfssl_logo.png` and this directory
   to it. From `wolfDemo/examples`:

   ```
   scp -rp bench_tui.py wolfssl_logo.png pi-display <user>@wolfdemo.local:
   ```

3. Run the setup script on the Pi and reboot:

   ```
   ssh <user>@wolfdemo.local
   sudo ./pi-display/setup.sh
   sudo reboot
   ```

The setup script:

* Installs cage, foot, seatd, the JetBrains Mono font, Pillow and pyserial
  from the Raspberry Pi OS packages.
* Copies `bench_tui.py`, `wolfssl_logo.png` and the foot settings
  ([foot.ini](foot.ini)) to `/opt/wolfdemo`, along with an invisible mouse
  pointer. cage shows a pointer in the middle of the screen even when there is
  no mouse.
* Adds a udev rule so that the wolfDemo board's CH340 USB UART always appears
  as `/dev/wolfdemo`.
* Installs and enables `wolfdemo-display.service`, which replaces the login
  prompt on tty1. It runs as the `wolfdemo` system user, and cage gets the
  display from seatd. A logind login session would also work, but it waits for
  a per-user service manager, which delays the display by over a minute while
  a Pi Zero is booting.
* Hides the boot logo and rainbow splash. The system console moves from tty1
  to tty3, so boot messages are not shown on the HDMI display. The original
  `cmdline.txt` is saved as `cmdline.txt.orig` in `/boot/firmware`.

## Colours and text size

The display uses black text on a white background. Set `THEME=dark` for white
text on a black background instead.

The text size is set by the font height in pixels, `FONT_SIZE`. On a 1080p
monitor:

| FONT_SIZE | Screen (columns x rows) | Table rows shown |
|-----------|-------------------------|------------------|
| 28 | 112 x 33 | 21 |
| 32 (default) | 101 x 29 | 17 |
| 36 | 87 x 26 | 14 |

The table needs 88 columns, and shrinks its columns to fit narrower screens.
To change either setting, run the setup script again with the variables set,
then restart the display:

```
sudo THEME=dark FONT_SIZE=36 ./pi-display/setup.sh
sudo systemctl restart wolfdemo-display
```

## Troubleshooting

* `journalctl -u wolfdemo-display` shows any errors from cage and foot, and
  `journalctl -b` shows the boot messages. Errors from bench_tui.py are shown
  on the display.
* `ls -l /dev/wolfdemo` checks that the board was detected.
* `vcgencmd get_throttled` should return `throttled=0x0`. Anything else means
  the power supply is not good enough.
* `sudo systemctl restart wolfdemo-display` restarts the display after
  changing `/opt/wolfdemo/bench_tui.py`.
* To take a screenshot of the display, install `grim` and run:

  ```
  sudo -u wolfdemo XDG_RUNTIME_DIR=/run/wolfdemo-display WAYLAND_DISPLAY=wayland-0 grim /tmp/screen.png
  ```

The Pi is usually switched off by pulling the power, which can corrupt the
microSD card. Once everything is working, enable the read-only overlay file
system using `sudo raspi-config` under Performance Options, Overlay File
System. Turn it off again in the same place before making any changes.
