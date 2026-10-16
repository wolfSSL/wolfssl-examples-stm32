#!/usr/bin/env python3
import os, re, sys, time, argparse, shutil, signal, unicodedata
from collections import OrderedDict

# -------- args / runtime/IO ----------
ap = argparse.ArgumentParser(description="wolfCrypt benchmark live TUI (single table, static, newest-row highlight)")
ap.add_argument("port", nargs="?", help="Serial port like /dev/ttyACM0")
ap.add_argument("baud", nargs="?", type=int, default=115200, help="Baud rate (default 115200)")
ap.add_argument("--demo", action="store_true", help="Run a local demo data source to preview layout")
themes = ap.add_mutually_exclusive_group()
themes.add_argument("--dark", action="store_true", help="Use 24-bit colours designed for a black background")
themes.add_argument("--light", action="store_true", help="Use 24-bit colours designed for a white background")
ap.add_argument("--logo", metavar="PNG", help="Draw this image as the logo, in terminals with sixel graphics (needs Pillow)")
args = ap.parse_args()

PORT = args.port if args.port and (args.port.startswith("/dev/") or args.port.upper().startswith("COM")) else None
BAUD = args.baud
USE_SERIAL = bool(PORT)

# ---- layout knobs ----
COL_ALGO   = 16          # width of "Algo"
COL_MODE   = 20          # width of "Mode/Op"
COL_RESULT = 16          # width of "Result"
COL_HEAP   = 10          # width of "Heap"
COL_STACK  = 10          # width of "Stack"
CELL_PAD   = 1           # left/right padding inside each cell

HDR_PERIOD = 0.5         # header heartbeat period (dots)

# The Linux framebuffer console has no emoji glyphs or underlining
LINUX_CONSOLE = os.environ.get("TERM") == "linux"

def line_iter():
    if args.demo:
        # generate fake rows periodically to fill and overflow
        algos = [
            "AES-128-CBC-enc", "AES-128-CBC-dec", "AES-256-CBC-enc", "AES-256-CBC-dec",
            "AES-128-CTR-enc", "AES-128-CTR-dec", "RSA 2048 public", "RSA 2048 private",
            "ECDHE [SECP256R1] 256 agree", "ECDSA [SECP256R1] 256 sign",
            "ML-KEM 768 192 encap", "ML-DSA 65 sign", "CURVE 25519 agree", "ED 25519 verify",
            "GMAC-AES128", "GMAC-AES256", "AES-CCM-enc", "AES-GCM-enc"
        ]
        i = 0
        while True:
            time.sleep(0.35)
            name = algos[i % len(algos)]
            if "AES" in name or "GMAC" in name:
                amt = "64 KiB"; took = f"{1.00 + (i%5)*0.01:.3f}"; rate = f"{60.0 + (i%20)*0.5:.3f} MiB/s"
                yield f"{name} {amt} took {took} seconds, {rate} [heap {512 + (i%8)*32} bytes, stack {1024 + (i%10)*64} bytes]"
            elif name.startswith("RSA"):
                bits, op = ("2048", "public") if "public" in name else ("2048","private")
                ops = 50 + (i%10)
                took = f"{1.2 + (i%7)*0.03:.3f}"
                rate = f"{ops / float(took):.3f}"
                yield f"RSA {bits} {op} {ops} ops took {took} sec, ..., {rate} ops/sec [heap 4096 bytes, stack 3072 bytes]"
            elif name.startswith("ECDHE") or name.startswith("ECDSA"):
                curve = "SECP256R1"; bits = "256"; op = "agree" if "ECDHE" in name else ("sign" if "ECDSA" in name else "verify")
                ops = 100 + (i%30)
                took = f"{1.0 + (i%9)*0.02:.3f}"
                rate = f"{ops / float(took):.3f}"
                prefix = "ECDHE" if "ECDHE" in name else "ECDSA"
                yield f"E{prefix[1:]} [{curve}] {bits} {op} {ops} ops took {took} sec, ..., {rate} ops/sec [heap 2304 bytes, stack 1984 bytes]"
            elif name.startswith("ML-KEM"):
                param, sec = "768", "192"
                op = "encap"
                ops = 80 + (i%20)
                took = f"{1.0 + (i%9)*0.02:.3f}"
                rate = f"{ops / float(took):.3f}"
                yield f"ML-KEM {param} {sec} {op} {ops} ops took {took} sec, ..., {rate} ops/sec [heap 8192 bytes, stack 4096 bytes]"
            elif name.startswith("ML-DSA"):
                param = "65"; op = "sign"
                ops = 75 + (i%25)
                took = f"{1.0 + (i%9)*0.02:.3f}"
                rate = f"{ops / float(took):.3f}"
                yield f"ML-DSA {param} {op} {ops} ops took {took} sec, ..., {rate} ops/sec [heap 7168 bytes, stack 6144 bytes]"
            elif name.startswith("CURVE"):
                param = "25519"; op = "agree"; ops = 120 + (i%20)
                took = f"{1.0 + (i%9)*0.02:.3f}"; rate = f"{ops / float(took):.3f}"
                yield f"CURVE {param} {op} {ops} ops took {took} sec, ..., {rate} ops/sec [heap 1024 bytes, stack 2048 bytes]"
            elif name.startswith("ED"):
                param = "25519"; op = "verify"; ops = 110 + (i%20)
                took = f"{1.0 + (i%9)*0.02:.3f}"; rate = f"{ops / float(took):.3f}"
                yield f"ED {param} {op} {ops} ops took {took} sec, ..., {rate} ops/sec [heap 1536 bytes, stack 2560 bytes]"
            i += 1
    elif USE_SERIAL:
        import serial  # type: ignore
        ser = None
        buf = b""
        while True:
            if ser is None:
                # wait for the board to be plugged in
                try:
                    ser = serial.Serial(PORT, BAUD, timeout=0.1)
                    info["connected"] = True
                except (serial.SerialException, OSError):
                    time.sleep(0.5)
                yield ""; continue
            try:
                # returns as soon as anything has arrived, rather than waiting
                # for the timeout
                chunk = ser.read(ser.in_waiting or 1)
            except (serial.SerialException, OSError):
                # the board was unplugged
                ser.close()
                ser = None; buf = b""
                info["connected"] = False
                yield ""; continue
            if not chunk:
                yield ""; continue
            buf += chunk
            while b"\n" in buf:
                ln, buf = buf.split(b"\n", 1)
                yield ln.decode("utf-8", "ignore").strip()
    else:
        for ln in sys.stdin:
            yield ln.strip()
        while True:
            time.sleep(0.1); yield ""

# -------- parsing ----------
mem_suffix = r"(?:\s*\[heap\s+(?P<heap>\d+)\s+bytes(?:\s+\(\d+\s+allocs\))?,\s+stack\s+(?P<stack>\d+)\s+bytes\])?$"

re_stream = re.compile(
    r"^(?P<algo>[A-Za-z0-9_\-\[\]/ ]+?)\s+"
    r"(?P<amnt>\d+)\s*(?P<amnt_unit>MiB|KiB)\s+took\s+(?P<sec>[\d.]+)\s+seconds?,\s+"
    r"(?P<rate>[\d.]+)\s+(?P<rate_unit>MiB/s|KiB/s)"
    + mem_suffix
)
re_rsa = re.compile(
    r"^RSA\s+(?P<bits>\d+)\s+(?P<op>public|private)\s+(?P<ops>\d+)\s+ops took\s+(?P<sec>[\d.]+)\s+sec,.*?,\s+(?P<rate>[\d.]+)\s+ops/sec"
    + mem_suffix
)
re_dh  = re.compile(
    r"^DH\s+(?P<bits>\d+)\s+(?P<op>key gen|agree)\s+(?P<ops>\d+)\s+ops took\s+(?P<sec>[\d.]+)\s+sec,.*?,\s+(?P<rate>[\d.]+)\s+ops/sec"
    + mem_suffix
)
re_ecc = re.compile(
    r"^E(CDHE|CDSA|CC)\s+\[\s*(?P<curve>[A-Z0-9_]+)\]\s+(?P<bits>\d+)\s+"
    r"(?P<op>key gen|agree|sign|verify)\s+(?P<ops>\d+)\s+ops took\s+(?P<sec>[\d.]+)\s+sec,.*?,\s+(?P<rate>[\d.]+)\s+ops/sec"
    + mem_suffix
)
re_mlkem = re.compile(
    r"^ML\-KEM\s+(?P<param>\d+)\s+(?P<secbits>\d+)\s+(?P<op>key gen|encap|decap)\s+"
    r"(?P<ops>\d+)\s+ops took\s+(?P<sec>[\d.]+)\s+sec,.*?,\s+(?P<rate>[\d.]+)\s+ops/sec"
    + mem_suffix
)
re_mldsa = re.compile(
    r"^ML\-DSA\s+(?P<param>\d+)\s+(?P<op>key gen|sign|verify)\s+"
    r"(?P<ops>\d+)\s+ops took\s+(?P<sec>[\d.]+)\s+sec,.*?,\s+(?P<rate>[\d.]+)\s+ops/sec"
    + mem_suffix
)
re_curve25519 = re.compile(
    r"^CURVE\s+(?P<param>\d+)\s+(?P<op>key gen|agree)\s+"
    r"(?P<ops>\d+)\s+ops took\s+(?P<sec>[\d.]+)\s+sec,.*?,\s+(?P<rate>[\d.]+)\s+ops/sec"
    + mem_suffix
)
re_ed25519 = re.compile(
    r"^ED\s+(?P<param>\d+)\s+(?P<op>key gen|sign|verify)\s+"
    r"(?P<ops>\d+)\s+ops took\s+(?P<sec>[\d.]+)\s+sec,.*?,\s+(?P<rate>[\d.]+)\s+ops/sec"
    + mem_suffix
)
# DRBG instantiate + free, e.g. "RNG 256 SHA256 Init/Free 404 ops took ..."
re_rng_ops = re.compile(
    r"^RNG\s+(?P<bits>\d+)\s+(?P<hash>SHA\d+)\s+(?P<op>Init/Free)\s+"
    r"(?P<ops>\d+)\s+ops took\s+(?P<sec>[\d.]+)\s+sec,.*?,\s+(?P<rate>[\d.]+)\s+ops/sec"
    + mem_suffix
)

re_reset = re.compile(r"wolfcrypt benchmark", re.IGNORECASE)
re_bench_cfg = re.compile(r"block bytes\s+(?P<block>\d+),\s+min\s+(?P<min>[\d.]+)\s+sec")

# The banner the wolfDemo firmware prints before each run, for example:
#   wolfSSL STM32U585 @ 160 MHz, hardware accelerated
#   wolfSSL 5.9.4 (CONFIG=hw), benchmark run 1
re_board = re.compile(r"^wolfSSL\s+(?P<board>\S+\s+@\s+\d+\s*MHz),\s+(?P<accel>.+)$")
re_version = re.compile(r"^wolfSSL\s+(?P<version>\d\S*)\s+\(CONFIG=\w+\),\s+benchmark run\s+(?P<run>\d+)")
re_ansi = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")

def split_algo_mode(algo: str):
    a = algo.strip()
    if a.startswith("AES-"):
        parts = a.split("-")
        if len(parts) >= 3:
            return f"{parts[0]}-{parts[1]}", "-".join(parts[2:]) or "—"
    if a.startswith("GMAC"):
        return "GMAC", a.replace("GMAC", "").strip() or "—"
    return a, "—"

def _format_mem(val: str) -> str:
    try:
        n = int(val)
    except (TypeError, ValueError):
        return ""
    if n >= 1024:
        return f"{n/1024:.1f} KiB"
    else:
        return f"{n} B"

def _attach_mem(d, m):
    heap = m.groupdict().get("heap")
    stack = m.groupdict().get("stack")
    if heap and stack:
        d["heap"] = _format_mem(heap)
        d["stack"] = _format_mem(stack)
    return d

def norm_stream(m):
    name, mode = split_algo_mode(m.group("algo"))
    d = {"algo": name, "mode": mode, "result": f'{float(m.group("rate")):.3f} {m.group("rate_unit")}'}
    return _attach_mem(d, m)

def norm_rsa(m):
    d = {"algo": f'RSA-{m.group("bits")}', "mode": m.group("op"), "result": f'{float(m.group("rate")):.3f} ops/s'}
    return _attach_mem(d, m)

def norm_dh(m):
    d = {"algo": f'DH-{m.group("bits")}', "mode": m.group("op"), "result": f'{float(m.group("rate")):.3f} ops/s'}
    return _attach_mem(d, m)

def norm_ecc(m):
    kind = "ECDHE" if "CDHE" in m.group(0) else ("ECDSA" if "CDSA" in m.group(0) else "ECC")
    d = {"algo": f"{kind}-{m.group('curve')}", "mode": m.group("op"), "result": f'{float(m.group("rate")):.3f} ops/s'}
    return _attach_mem(d, m)

def norm_mlkem(m):
    d = {"algo": f"ML-KEM-{m.group('param')}", "mode": f"{m.group('op')} (sec {m.group('secbits')})", "result": f'{float(m.group("rate")):.3f} ops/s'}
    return _attach_mem(d, m)

def norm_mldsa(m):
    d = {"algo": f"ML-DSA-{m.group('param')}", "mode": m.group("op"), "result": f'{float(m.group("rate")):.3f} ops/s'}
    return _attach_mem(d, m)

def norm_curve25519(m):
    d = {"algo": f"CURVE {m.group('param')}", "mode": m.group("op"), "result": f'{float(m.group("rate")):.3f} ops/s'}
    return _attach_mem(d, m)

def norm_ed25519(m):
    d = {"algo": f"ED {m.group('param')}", "mode": m.group("op"), "result": f'{float(m.group("rate")):.3f} ops/s'}
    return _attach_mem(d, m)

def norm_rng_ops(m):
    d = {"algo": "RNG", "mode": f"{m.group('hash')} {m.group('op')}", "result": f'{float(m.group("rate")):.3f} ops/s'}
    return _attach_mem(d, m)

def parse_line(s):
    s = re_ansi.sub("", s).strip()
    if not s: return None
    if re_reset.search(s):
        m = re_bench_cfg.search(s)
        return {"reset": True, "info": m.groupdict() if m else {}}
    for rx in (re_board, re_version):
        m = rx.match(s)
        if m: return {"info": m.groupdict()}
    for rx, fn in ((re_stream,norm_stream),(re_rsa,norm_rsa),(re_dh,norm_dh),
                   (re_ecc,norm_ecc),(re_mlkem,norm_mlkem),(re_mldsa,norm_mldsa),
                   (re_curve25519,norm_curve25519),(re_ed25519,norm_ed25519),
                   (re_rng_ops,norm_rng_ops)):
        m = rx.match(s)
        if m: return fn(m)
    return None

# -------- state ----------
rows = OrderedDict()  # key: (algo, mode) -> dict
order = []            # insertion order keys
abs_index = {}        # key -> stable insertion index for row striping
# details from the board's banner, filled in as they are seen
BANNER_DEFAULTS = {"board": None, "accel": None, "version": None, "run": None,
                   "block": "1024", "min": "1"}
info = dict(BANNER_DEFAULTS, connected=not USE_SERIAL)

# -------- rendering ----------
# The screen is drawn with ANSI escape sequences directly. Only the lines that
# changed since the last frame are rewritten, which keeps updates fast on slow
# hosts such as a Raspberry Pi Zero.

# SGR attributes
BOLD, DIM, UNDERLINE = "1", "2", "4"
RED, GREEN, YELLOW, MAGENTA, CYAN, WHITE = "31", "32", "33", "35", "36", "37"

def fg(rgb):
    return "38;2;%d;%d;%d" % tuple(int(rgb[i:i + 2], 16) for i in (0, 2, 4))

def bg(rgb):
    return "48;2;%d;%d;%d" % tuple(int(rgb[i:i + 2], 16) for i in (0, 2, 4))

# Colour themes, giving the SGR attributes for each part of the screen.
# "stripe" and "newest" are added to the text of every other row and the
# newest row, "stripe_row" and "newest_row" to the whole row, borders
# included. Every other row is striped so the eye can follow a row across.
CLASSIC = {
    "border": (), "head": (BOLD,), "algo": (BOLD,), "mode": (), "num": (BOLD,),
    "mem": (), "rate": (CYAN,), "ops": (MAGENTA,),
    "stripe": (DIM,), "stripe_row": (), "newest": (BOLD, GREEN), "newest_row": (),
    "muted": (DIM,), "wolf": (BOLD, WHITE), "ssl": (BOLD, CYAN),
    "bench": (BOLD, GREEN), "live": (BOLD, RED), "hw": (BOLD, GREEN),
    "sw": (BOLD, YELLOW), "waiting": (BOLD,), "underline": (UNDERLINE,),
    "emoji": True, "bg": None, "invert_logo": False,
}
# The Linux console dims by replacing the text colour, so stripe rows with a
# background colour instead, with blue redefined as a dark grey. It has no
# emoji glyphs, and draws underlined text in a single colour.
CONSOLE = dict(CLASSIC, stripe=(), stripe_row=("44",), muted=(), underline=(),
               emoji=False)
STRIPE_RGB = "333333"
# 24-bit colours for a black background, with the wolfSSL teal
DARK = {
    "border": (fg("3a4a54"),), "head": (BOLD, fg("ffffff")),
    "algo": (BOLD, fg("ffffff")), "mode": (fg("c3ccd5"),),
    "num": (BOLD, fg("ffffff")), "mem": (fg("8b98a5"),),
    "rate": (fg("1fbeca"),), "ops": (fg("d68be8"),),
    "stripe": (), "stripe_row": (bg("242d38"),),
    "newest": (BOLD, fg("7ee787")), "newest_row": (bg("10301c"),),
    "muted": (fg("8b98a5"),), "wolf": (BOLD, fg("ffffff")),
    "ssl": (BOLD, fg("1fbeca")), "bench": (BOLD, fg("7ee787")),
    "live": (BOLD, fg("ff6b6b")), "hw": (BOLD, fg("7ee787")),
    "sw": (BOLD, fg("f2cc60")), "waiting": (BOLD, fg("ffffff")),
    "underline": (), "emoji": False, "bg": "000000", "invert_logo": True,
}
# 24-bit colours for a white background. The wolfSSL teal is too light for
# text on white, so the units use a darker shade of it.
LIGHT = {
    "border": (fg("b4bec8"),), "head": (BOLD, fg("1f2328")),
    "algo": (BOLD, fg("1f2328")), "mode": (fg("3d454d"),),
    "num": (BOLD, fg("1f2328")), "mem": (fg("57606a"),),
    "rate": (fg("0a7f8a"),), "ops": (fg("8c3bb3"),),
    "stripe": (), "stripe_row": (bg("e1e7ee"),),
    "newest": (BOLD, fg("1a7f37")), "newest_row": (bg("cdf2d6"),),
    "muted": (fg("57606a"),), "wolf": (BOLD, fg("1f2328")),
    "ssl": (BOLD, fg("1fbeca")), "bench": (BOLD, fg("1a7f37")),
    "live": (BOLD, fg("cf222e")), "hw": (BOLD, fg("1a7f37")),
    "sw": (BOLD, fg("9a6700")), "waiting": (BOLD, fg("1f2328")),
    "underline": (), "emoji": False, "bg": "ffffff", "invert_logo": False,
}
THEME = (DARK if args.dark else LIGHT if args.light else
         CONSOLE if LINUX_CONSOLE else CLASSIC)

# Rounded box drawing characters
BOX_TL, BOX_TR, BOX_BL, BOX_BR = "\u256d", "\u256e", "\u2570", "\u256f"
BOX_H, BOX_V = "\u2500", "\u2502"
BOX_TT, BOX_BT, BOX_LT, BOX_RT, BOX_X = "\u252c", "\u2534", "\u251c", "\u2524", "\u253c"
HALF_UP, HALF_DOWN, FULL = "\u2580", "\u2584", "\u2588"
ELLIPSIS = "\u2026"
BULLET = "\u2022"
WOLF = "\U0001F43A"

DOTS = ("...", "\u00b7..", ".\u00b7.", "..\u00b7")
DEFAULT_BOARD = "STM32U585 @ 160 MHz"

# 6 pixel high letters for the big "wolfSSL" logo, with 5 pixel high capitals
# so that the S is symmetrical. Half block characters put two pixels in each
# text row, which makes them square with Terminus fonts.
LOGO_FONT = {
    "w": (".....", ".....", "#...#", "#...#", "#.#.#", ".#.#."),
    "o": ("....", "....", ".##.", "#..#", "#..#", ".##."),
    "l": ("#", "#", "#", "#", "#", "#"),
    "f": (".##", "#..", "###", "#..", "#..", "#.."),
    "S": (".....", ".####", "#....", ".###.", "....#", "####."),
    "L": ("....", "#...", "#...", "#...", "#...", "####"),
}

def logo_rows():
    rows = [[], [], []]
    letters = [(c, THEME["wolf"]) for c in "wolf"] + [(c, THEME["ssl"]) for c in "SSL"]
    for i, (ch, style) in enumerate(letters):
        glyph = LOGO_FONT[ch]
        for r in range(3):
            cells = "".join(
                FULL if top == bot == "#" else HALF_UP if top == "#" else
                HALF_DOWN if bot == "#" else " "
                for top, bot in zip(glyph[2 * r], glyph[2 * r + 1]))
            rows[r].append(((" " if i else "") + cells, style))
    return rows

LOGO = logo_rows()

# -------- sixel logo ----------
# With --logo, the logo image is drawn with sixel graphics, LOGO_IMG_ROWS
# text rows high, in terminals that support them.
LOGO_IMG_ROWS = 5

def sixel_supported():
    # Ask the terminal for its primary device attributes, 4 means sixel
    if os.name == "nt":
        return False
    import select, termios
    try:
        fd = os.open("/dev/tty", os.O_RDWR | os.O_NOCTTY)
    except OSError:
        return False
    reply = b""
    try:
        old = termios.tcgetattr(fd)
        new = termios.tcgetattr(fd)
        new[3] &= ~(termios.ICANON | termios.ECHO)
        termios.tcsetattr(fd, termios.TCSANOW, new)
        try:
            os.write(fd, b"\x1b[c")
            while not reply.endswith(b"c"):
                if not select.select([fd], [], [], 1.0)[0]:
                    break
                reply += os.read(fd, 64)
        finally:
            termios.tcsetattr(fd, termios.TCSANOW, old)
    except termios.error:
        pass
    finally:
        os.close(fd)
    m = re.search(rb"\x1b\[\?([\d;]*)c", reply)
    return bool(m) and b"4" in m.group(1).split(b";")

def cell_pixels():
    # Size of a character cell in pixels, if the terminal reports it
    if os.name == "nt":
        return None
    import fcntl, struct, termios
    try:
        rows, cols, xpix, ypix = struct.unpack(
            "HHHH", fcntl.ioctl(sys.stdout.fileno(), termios.TIOCGWINSZ, bytes(8)))
    except OSError:
        return None
    if not (rows and cols and xpix and ypix):
        return None
    return xpix / cols, ypix / rows

# maps each 6 pixel column (0 to 63) to its sixel character
SIXEL_CHARS = bytes(range(63, 127)) + bytes(192)

def sixel_encode(img, mask):
    # img is a palette image, mask is a list of booleans, True for the pixels
    # to draw. The rest are left transparent.
    w, h = img.size
    data = img.tobytes()
    pal = img.getpalette()
    used = sorted({c for c, m in zip(data, mask) if m})
    out = ['\x1bP0;1;0q"1;1;%d;%d' % (w, h)]
    for c in used:
        r, g, b = pal[3 * c:3 * c + 3]
        out.append("#%d;2;%d;%d;%d" % (c, r * 100 // 255, g * 100 // 255, b * 100 // 255))
    for y0 in range(0, h, 6):
        bits = {}
        for i in range(min(6, h - y0)):
            row = (y0 + i) * w
            for x in range(w):
                if mask[row + x]:
                    bits.setdefault(data[row + x], bytearray(w))[x] |= 1 << i
        band = []
        for c, b in bits.items():
            chars = b.translate(SIXEL_CHARS).decode()
            chars = re.sub(r"(.)\1{3,}", lambda m: "!%d%s" % (len(m.group(0)), m.group(1)), chars)
            band.append("#%d%s" % (c, chars))
        out.append("$".join(band) + "-")
    out.append("\x1b\\")
    return "".join(out)

class SixelLogo:
    def __init__(self, path):
        from PIL import Image
        logo = Image.open(path).convert("RGBA")
        if THEME["invert_logo"]:
            # Draw the black parts of the logo in white on a dark background
            px = [(255, 255, 255, a) if r + g + b < 96 else (r, g, b, a)
                  for r, g, b, a in logo.getdata()]
            logo.putdata(px)
        self.logo = logo
        self.cache = {}

    def render(self, cell):
        # Returns (columns, sixel data) for the logo LOGO_IMG_ROWS rows high
        key = (round(cell[0], 2), round(cell[1], 2))
        if key not in self.cache:
            from PIL import Image
            h = int(LOGO_IMG_ROWS * cell[1])
            w = max(1, round(self.logo.width * h / self.logo.height))
            matte = "#" + (THEME["bg"] or "ffffff")
            flat = Image.new("RGBA", self.logo.size, matte)
            flat.alpha_composite(self.logo)
            img = flat.convert("RGB").resize((w, h), Image.LANCZOS).quantize(64)
            if THEME["bg"]:
                # leave the background transparent
                alpha = self.logo.getchannel("A").resize((w, h), Image.LANCZOS)
                mask = [a > 16 for a in alpha.getdata()]
            else:
                mask = [True] * (w * h)
            cols = -(-w // int(cell[0]))
            self.cache[key] = (cols, sixel_encode(img, mask))
        return self.cache[key]

def load_logo():
    if not args.logo or not sixel_supported():
        return None
    try:
        return SixelLogo(args.logo)
    except Exception as e:  # missing Pillow or an unreadable image
        print(f"bench_tui: not showing --logo: {e}", file=sys.stderr)
        return None

# -------- screen ----------
re_sgr = re.compile(r"\x1b\[[0-9;]*m")
re_cuf = re.compile(r"\x1b\[(\d+)C")

def text_width(s):
    return sum(2 if unicodedata.east_asian_width(c) in "WF" else 1 for c in s)

def line_width(ln):
    # visible width of a composed line, which can contain SGR sequences and
    # cursor forward moves
    ln = re_sgr.sub("", ln)
    return sum(int(n) for n in re_cuf.findall(ln)) + text_width(re_cuf.sub("", ln))

def spans_width(spans):
    return sum(text_width(t) for t, _ in spans)

def sgr(attrs):
    # Combine SGR attributes into one sequence, keeping only the last
    # foreground and background colours. Some terminals, such as foot, only
    # accept 16 parameters in a sequence.
    flags, fgc, bgc = [], [], []
    for a in attrs:
        head = a.split(";")[0]
        if head == "38" or (len(head) == 2 and head[0] in "39"):
            fgc = [a]
        elif head == "48" or (len(head) == 2 and head[0] == "4"):
            bgc = [a]
        elif a not in flags:
            flags.append(a)
    return f"\x1b[{';'.join(['0'] + flags + fgc + bgc)}m"

def styled(spans):
    # spans is a list of (text, (SGR attributes, ...))
    return "".join(sgr(attrs) + text for text, attrs in spans)

def center(spans, cols):
    return " " * max(0, (cols - spans_width(spans)) // 2) + styled(spans)

def fit(text, width, right=False):
    if len(text) > width:
        text = text[:max(0, width - 1)] + ELLIPSIS
    return text.rjust(width) if right else text.ljust(width)

def table_width(widths):
    return sum(widths) + (2 * CELL_PAD + 1) * len(widths) + 1

def fit_widths(cols):
    # Shrink the widest columns until the table fits the terminal
    widths = [COL_ALGO, COL_MODE, COL_RESULT, COL_HEAP, COL_STACK]
    mins = [8, 8, 13, 8, 8]
    excess = table_width(widths) - cols
    while excess > 0:
        i = max(range(len(widths)), key=lambda i: widths[i] - mins[i])
        if widths[i] <= mins[i]:
            break
        widths[i] -= 1
        excess -= 1
    return widths

def border(widths, left, mid, right):
    line = left + mid.join(BOX_H * (w + 2 * CELL_PAD) for w in widths) + right
    return styled([(line, THEME["border"])])

def table_row(cells, row=()):
    # cells is a list of span lists, each exactly its column width. row is
    # added to the whole row inside the table's outer border.
    pad = " " * CELL_PAD
    spans = [(BOX_V, THEME["border"]), (pad, row)]
    for i, cell in enumerate(cells):
        if i:
            spans += [(pad, row), (BOX_V, THEME["border"] + row), (pad, row)]
        spans += cell
    spans += [(pad, row), (BOX_V, THEME["border"])]
    return styled(spans)

def result_cell(result, width, text, row):
    num, _, unit = result.partition(" ")
    base = THEME["num"] + text + row
    if len(result) > width or not unit:
        return [(fit(result, width, right=True), base)]
    unit_style = THEME["rate"] if unit.endswith("iB/s") else THEME["ops"]
    return [(" " * (width - len(result)), row), (num, base), (" ", row),
            (unit, text + row + unit_style)]

def table_lines(keys, widths, highlight_key=None):
    wa, wm, wr, wh, ws = widths
    lines = [border(widths, BOX_TL, BOX_TT, BOX_TR)]
    head = THEME["head"]
    lines.append(table_row([
        [(fit("Algo", wa), head)], [(fit("Mode/Op", wm), head)],
        [(fit("Result", wr, right=True), head)], [(fit("Heap", wh, right=True), head)],
        [(fit("Stack", ws, right=True), head)]]))
    if keys:
        lines.append(border(widths, BOX_LT, BOX_X, BOX_RT))
    for k in keys:
        d = rows[k]
        # stable striping, with the newest row highlighted
        text, row = (), ()
        if abs_index.get(k, 0) % 2 == 1:
            text, row = THEME["stripe"], THEME["stripe_row"]
        if k == highlight_key:
            text, row = THEME["newest"], THEME["newest_row"] or row
        lines.append(table_row([
            [(fit(d["algo"], wa), THEME["algo"] + text + row)],
            [(fit(d["mode"], wm), THEME["mode"] + text + row)],
            result_cell(d["result"], wr, text, row),
            [(fit(d.get("heap", ""), wh, right=True), THEME["mem"] + text + row)],
            [(fit(d.get("stack", ""), ws, right=True), THEME["mem"] + text + row)]],
            row))
    lines.append(border(widths, BOX_BL, BOX_BT, BOX_BR))
    return lines

def live(dots, ul=()):
    return [("wolfCrypt Benchmark", ul + THEME["bench"]), (" ", ul),
            ("LIVE", ul + THEME["live"]), (DOTS[dots], THEME["live"])]

def title(dots):
    ul = THEME["underline"]
    logo = [(WOLF + " ", ())] if THEME["emoji"] else []
    return logo + [("wolf", ul + THEME["wolf"]), ("SSL", ul + THEME["ssl"]),
                   (" ", ul)] + live(dots, ul)

def header_lines(dots, cols, height, logo_img):
    # Returns the header lines, and the sixel logo to draw as
    # (row, column, data) or None
    min_s = f"{float(info['min']):g}"
    text = [live(dots),
            [(f"Minimum {min_s} second benchmarks", THEME["muted"])],
            [(f"{info['block']} byte block size", THEME["muted"])]]
    text_w = max(spans_width(t) for t in text)
    if logo_img and height >= 20:
        # the logo image with the title beside it, vertically centred
        img_cols, data = logo_img
        width = img_cols + 3 + text_w
        if cols >= width:
            pad = " " * ((cols - width) // 2)
            top = (LOGO_IMG_ROWS - len(text)) // 2
            lines = []
            for i in range(LOGO_IMG_ROWS):
                t = text[i - top] if 0 <= i - top < len(text) else []
                lines.append(f"{pad}\x1b[{img_cols}C   {styled(t)}")
            return lines, (0, len(pad), data)
    # the block logo with the title beside it, when there is room
    width = spans_width(LOGO[0]) + 3 + text_w
    if cols >= width and height >= 20:
        pad = " " * ((cols - width) // 2)
        return [pad + styled(LOGO[i] + [("   ", ())] + text[i]) for i in range(3)], None
    return [center(title(dots), cols),
            center([(f"Minimum {min_s} second benchmarks, {info['block']} byte block size",
                     THEME["muted"])], cols)], None

def footer():
    sep = (f" {BULLET} ", THEME["muted"])
    spans = []
    if info["version"]:
        spans += [(f"wolfSSL {info['version']}", THEME["muted"]), sep]
    spans.append((info["board"] or DEFAULT_BOARD, THEME["muted"]))
    if info["accel"]:
        style = THEME["hw"] if info["accel"].startswith("hardware") else THEME["sw"]
        spans += [sep, (info["accel"], style)]
    if info["run"]:
        spans += [sep, (f"run {info['run']}", THEME["muted"])]
    return spans

def compose(dots, highlight_key, logo):
    # Returns the screen lines, and the sixel logo to draw or None
    cols, height = shutil.get_terminal_size()
    logo_img = None
    if logo:
        cell = cell_pixels()
        logo_img = logo.render(cell) if cell else None
    out = [""] * height
    header, image = header_lines(dots, cols, height, logo_img)
    out[:len(header)] = header[:height]
    top = len(header) + 1
    if info["connected"]:
        # table header/borders (4), margin (1) and footer (1)
        keys = order[-max(1, height - top - 6):]
        widths = fit_widths(cols)
        margin = " " * max(0, (cols - table_width(widths)) // 2)
        for i, ln in enumerate(table_lines(keys, widths, highlight_key), top):
            if i < height - 1:
                out[i] = margin + ln
    else:
        row = min(height - 2, max(top, height // 2))
        if row >= 0:
            out[row] = center([(f"Waiting for the wolfDemo board on {PORT}", THEME["waiting"]),
                               (DOTS[dots], THEME["live"])], cols)
    out[height - 1] = center(footer(), cols)
    return out[:height], image

class Screen:
    def __init__(self):
        self.lines = []
        self.size = None
        self.image = None

    def __enter__(self):
        enable_vt()
        # alternate screen, hide the cursor, no line wrapping
        setup = "\x1b[?1049h\x1b[?25l\x1b[?7l"
        if THEME is CONSOLE:
            setup += "\x1b]P4" + STRIPE_RGB  # the stripe_row colour
        sys.stdout.write(setup)
        return self

    def __exit__(self, *exc):
        restore = "\x1b[0m\x1b[?7h\x1b[?25h\x1b[?1049l"
        if THEME is CONSOLE:
            restore += "\x1b]R"  # reset the palette
        sys.stdout.write(restore)
        sys.stdout.flush()

    def draw(self, lines, image=None):
        out = []
        size = shutil.get_terminal_size()
        if size != self.size or image != self.image:
            # clearing the screen also removes any sixel image
            out.append("\x1b[0m\x1b[2J")
            self.lines = []
            self.size = size
        for i, ln in enumerate(lines):
            if i >= len(self.lines) or self.lines[i] != ln:
                # pad to the full width rather than erasing to the end of the
                # line, which would also erase a sixel image or the last column
                pad = " " * max(0, size.columns - line_width(ln))
                out.append(f"\x1b[{i + 1};1H{ln}\x1b[0m{pad}")
        if image and (not self.lines or image != self.image):
            row, col, data = image
            out.append(f"\x1b[{row + 1};{col + 1}H{data}")
        self.lines = lines
        self.image = image
        if out:
            sys.stdout.write("".join(out))
            sys.stdout.flush()

def enable_vt():
    # Windows consoles need ANSI escape sequence processing turning on
    if os.name != "nt":
        return
    import ctypes
    k32 = ctypes.windll.kernel32
    handle = k32.GetStdHandle(-11)  # STD_OUTPUT_HANDLE
    mode = ctypes.c_uint32()
    if k32.GetConsoleMode(handle, ctypes.byref(mode)):
        k32.SetConsoleMode(handle, mode.value | 0x0004)  # ENABLE_VIRTUAL_TERMINAL_PROCESSING

# -------- main ----------
def main():
    dot_counter = 0  # cycles 0..3
    last_hdr = time.monotonic()
    newest_key = None
    connected = info["connected"]
    logo = load_logo()

    with Screen() as screen:
        screen.draw(*compose(dot_counter, newest_key, logo))
        for s in line_iter():
            changed = False

            # periodic header update, also picks up terminal resizes
            now = time.monotonic()
            if now - last_hdr >= HDR_PERIOD:
                dot_counter = (dot_counter + 1) % 4
                last_hdr = now
                changed = True

            if info["connected"] != connected:
                # the board was plugged in or unplugged, forget the old run
                connected = info["connected"]
                rows.clear(); order.clear(); abs_index.clear()
                info.update(BANNER_DEFAULTS)
                newest_key = None
                changed = True

            d = parse_line(s)
            if d and "info" in d:
                info.update(d["info"])
                changed = True
            if d and d.get("reset"):
                rows.clear(); order.clear(); abs_index.clear()
                newest_key = None
                changed = True
            elif d and "algo" in d:
                key = (d["algo"], d["mode"])
                if key not in rows:
                    rows[key] = d
                    order.append(key)
                    abs_index[key] = len(abs_index)  # stable index
                else:
                    rows[key] = d  # update existing row content

                newest_key = key  # track most recent update/insert
                changed = True

            if changed:
                screen.draw(*compose(dot_counter, newest_key, logo))

if __name__ == "__main__":
    # exit cleanly, restoring the terminal, when stopped by a service manager
    signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))
    try:
        main()
    except KeyboardInterrupt:
        pass
