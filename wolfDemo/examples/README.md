# wolfDemo STM32U585 board examples

The wolfDemo board is designed by wolfSSL as an example board to show wolfSSL
projects at events such as trade shows. These are example projects for the
board.

## [blinky](blinky)

This is a demo to PWM the LED "bark bars" on the wolfDemo board to show a give a
nice visual display.

## [wolfCrypt-bench](wolfCrypt-bench)

This runs the wolfCrypt benchmark in a loop with the STM32 hardware crypto
(AES, HASH, PKA and TRNG) enabled. It is built with `make` against a wolfSSL
source tree and the STM32CubeU5 package, and can also build a pure software
variant and the wolfCrypt self-test. See its [README](wolfCrypt-bench/README.md)
for details.

Each log line output will cycle the LED "bark bars" one step. Log data will be
output on the USB's UART at 115200bps. Hold BT2 during reset for an LED-only
demo.

## [wolfTPM-bench](wolfTPM-bench)

This runs the wolfTPM benchmark. It requires the wolfDemo ST33K TPM addon board
to be inserted in mikroBus socket 1. It talks to this board via SPI.

As with the wolfCrypt-bench, each log line will cycle the LED "bark bars" one
step. The log data will also be output on the USB's UART at 115200bps.

### NOTE

This is using a patch to fix a bug in the SPI driver for the STM32 HAL. If you
regenerate from the `.ioc` file, this patch will be erased it it will no longer
be able to talk to the TPM. Make sure you backup `stm32h7xx_hal_spi.c` first.

The wolfTPM configuration will also be erased by such an action, and that should
be backed-up too.

## [bench_tui.py](bench_tui.py)

This script takes the live wolfCrypt benchmark output via UART and displays it
in an easy-to-consume table form. If the board is not plugged in, or is
unplugged, it waits for the board to be connected. This requires Python
`pyserial` to be installed using the following, or by using your package
manager:

```
pip3 install pyserial
```

Usage (in Linux):

```
python bench_tui.py /dev/ttyUSB0 115200
```

Usage (in Windows):

1. Install Python 3 from the Windows Store
2. Find the COM port for the CH340 on the wolfDemo board (usually `COM3`) by using:

```
pnputil /enum-devices /class Ports
```

3. Install dependencies (PowerShell or Command Prompt):

```
pip install pyserial
```

4. Run:

```
python bench_tui.py COM3 115200
```

Usage (in macOS):

```
ls /dev/tty.usbserial*
```

Note the result, and then do the following (assuming the result is
tty.usbserial-1440):

```
python3 bench_tui.py /dev/tty.usbserial-1440
```

A demo mode can be executed using:

```
python bench_tui.py --demo
```

There are also options for a nicer look in terminals that support 24-bit
colour:

* `--light` or `--dark` use 24-bit colours designed for a white or black
  background, with background colours for the zebra striping and the newest
  result.
* `--logo wolfssl_logo.png` draws the wolfSSL logo image at the top, in
  terminals that support sixel graphics and report their character cell size,
  such as foot. This needs Python `pillow` (`pip3 install pillow`). With
  `--dark`, the black parts of the logo are drawn in white. Other terminals,
  and Windows, show a logo drawn with text characters instead.

```
python bench_tui.py --light --logo wolfssl_logo.png /dev/ttyUSB0 115200
```

## [pi-display](pi-display)

Setup scripts to turn a Raspberry Pi Zero into a standalone display for the
wolfDemo board. It runs `bench_tui.py` full screen on an HDMI monitor at boot,
with the wolfSSL logo, when the board is plugged into the Pi's USB port.

## [bench_tpm_tui.py](bench_tpm_tui.py)

This is very similar to `bench_tui.py`, but is designed to work with the
wolfTPM benchmark output. It also requires Python `rich`:

```
pip3 install rich pyserial
```

Usage (in Linux):

```
python bench_tpm_tui.py /dev/ttyUSB0 115200
```

Usage (in Windows):

```
python bench_tpm_tui.py COM3 115200
```

A demo mode can be executed using:

```
python bench_tpm_tui.py --demo
```
