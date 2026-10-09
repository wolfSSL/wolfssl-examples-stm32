#!/usr/bin/env python3
import re, sys, time, argparse
from collections import OrderedDict

# -------- args / runtime/IO ----------
ap = argparse.ArgumentParser(description="wolfCrypt benchmark live TUI (single table, static, newest-row highlight)")
ap.add_argument("port", nargs="?", help="Serial port like /dev/ttyACM0")
ap.add_argument("baud", nargs="?", type=int, default=115200, help="Baud rate (default 115200)")
ap.add_argument("--demo", action="store_true", help="Run a local demo data source to preview layout")
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

from rich.console import Console, Group
from rich.live import Live
from rich.table import Table
from rich.layout import Layout
from rich.align import Align
from rich.text import Text
from rich import box

console = Console()

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
        ser = serial.Serial(PORT, BAUD, timeout=0.1)
        buf = b""
        while True:
            chunk = ser.read(4096)
            if not chunk:
                time.sleep(0.02); yield ""; continue
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
    s = s.strip()
    if not s: return None
    if re_reset.search(s): return {"reset": True}
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

# -------- rendering ----------
def build_table():
    t = Table(
        expand=False,
        show_edge=True,
        header_style="bold",
        padding=(0, CELL_PAD),
        pad_edge=False,
        show_lines=False,
        title=None,
        box=box.ROUNDED,
        leading=0
    )
    t.add_column("Algo",    width=COL_ALGO,   no_wrap=True, overflow="ellipsis", style="bold")
    t.add_column("Mode/Op", width=COL_MODE,   no_wrap=True, overflow="ellipsis")
    t.add_column("Result",  width=COL_RESULT, no_wrap=True, overflow="ellipsis", justify="right", style="bold")
    t.add_column("Heap",    width=COL_HEAP,   no_wrap=True, overflow="ellipsis", justify="right")
    t.add_column("Stack",   width=COL_STACK,  no_wrap=True, overflow="ellipsis", justify="right")
    return t

def make_table_for_keys(keys, highlight_key=None):
    t = build_table()
    for k in keys:
        d = rows[k]
        idx = abs_index.get(k, 0)
        # stable striping first
        row_style = "" if (idx % 2 == 0) else "dim"
        # newest-row highlight overrides striping when in view
        if highlight_key is not None and k == highlight_key:
            row_style = "bold green"
        t.add_row(d["algo"], d["mode"], colorize_result(d["result"]), d.get("heap",""), d.get("stack",""), style=row_style)
    return Align.center(t)

def colorize_result(s: str) -> str:
    if "MiB/s" in s or "KiB/s" in s:
        num, unit = s.split(" ", 1)
        return f"[bold]{num}[/bold] [cyan]{unit}[/cyan]"
    if "ops/s" in s:
        num, unit = s.split(" ", 1)
        return f"[bold]{num}[/bold] [magenta]{unit}[/magenta]"
    return s

def layout_frame():
    lay = Layout()
    lay.split_column(
        Layout(name="hdr", size=3),
        Layout(name="tbl", ratio=1),
        Layout(name="ftr", size=1),
    )
    return lay

def header(dots: int):
    dots_str = ""
    txt = Text.from_markup(
            ":wolf: [underline][bold white]wolf[/bold white][bold cyan]SSL[/bold cyan] [bold green]wolfCrypt Benchmark[/bold green] [bold red]LIVE[/bold red][/underline]"
            )
    match dots:
        case 0:
            dots_str = "..."
        case 1:
            dots_str = "·.."
        case 2:
            dots_str = ".·."
        case 3:
            dots_str = "..·"
    txt.justify = "center"
    txt.append(Text.from_markup("[bold red]{0}[/bold red]".format(dots_str)))
    txt2 = Text.from_markup(
            "[dim]Minimum 1 second benchmarks, 1024 byte block size[/dim]", justify="center"
            )
    return Group(txt, txt2)

def footer():
    return Align.center("[dim]STM32U585 @ 160MHz • HW Accel[/dim]")

def visible_capacity():
    term_h = console.size.height
    # 3 (hdr) + 1 (ftr) + 2 (margins) + 3 (table header/borders) = 9
    reserved = 9
    return max(1, term_h - reserved)

# -------- main ----------
def main():
    dot_counter = 0  # cycles 0..3
    lay = layout_frame()
    lay["hdr"].update(header(0))
    lay["ftr"].update(footer())

    last_hdr = time.monotonic()
    newest_key = None

    with Live(lay, refresh_per_second=8, console=console, screen=True):
        for s in line_iter():
            # periodic header update
            now = time.monotonic()
            if now - last_hdr >= HDR_PERIOD:
                dot_counter = (dot_counter + 1) % 4
                lay["hdr"].update(header(dot_counter))
                last_hdr = now

            d = parse_line(s)
            if not d:
                continue

            if d.get("reset"):
                rows.clear(); order.clear(); abs_index.clear()
                newest_key = None
                lay["tbl"].update(make_table_for_keys([]))
                continue

            key = (d["algo"], d["mode"])
            if key not in rows:
                rows[key] = d
                order.append(key)
                abs_index[key] = len(abs_index)  # stable index
            else:
                rows[key] = d  # update existing row content

            newest_key = key  # track most recent update/insert
            cap = visible_capacity()
            window = order[-cap:]
            lay["tbl"].update(make_table_for_keys(window, highlight_key=newest_key if newest_key in window else None))

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
