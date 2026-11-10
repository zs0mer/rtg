"""Exercise the real binary in a pseudo-terminal; no third-party dependencies."""

import errno
import fcntl
import os
import pty
import re
import select
import signal
import struct
import subprocess
import sys
import tempfile
import termios
import time


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def check_pickup_hud_colors(output, name):
    marker = output.rfind(b"T STYLE " + name.encode())
    start = output.rfind(b"\033[H", 0, marker)
    colors = {}
    fg = None
    for token in re.finditer(rb"\x1b\[([0-9;]*)m|([^\x1b]+)", output[start:marker]):
        if token[1] is not None:
            if token[1].startswith(b"38;2;"):
                fg = tuple(map(int, token[1].split(b";")[2:]))
        else:
            for label in (b"HP", b"AMMO"):
                if label in token[2]:
                    colors[label] = fg
    check(b"HP" in colors and b"AMMO" in colors, f"missing HUD colors in {name}")
    health, ammo = colors[b"HP"], colors[b"AMMO"]
    check(
        health[0] == 255 and health[1] <= 60 and health[2] >= 215,
        f"health must stand out in neon magenta in {name}",
    )
    check(
        ammo[0] == 255 and ammo[1] >= 190 and ammo[2] <= 60,
        f"ammo must remain warm gold, distinct from health in {name}",
    )


def run(binary, interrupt=False):
    master, slave = pty.openpty()
    original = termios.tcgetattr(slave)
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 38, 110, 0, 0))
    with tempfile.TemporaryDirectory() as state:
        proc = subprocess.Popen(
            [binary, "--seed", "42", "--theme", "reactor"],
            stdin=slave,
            stdout=slave,
            stderr=slave,
            env={**os.environ, "XDG_STATE_HOME": state},
        )
        output = bytearray()

        def until(marker, timeout=4):
            deadline = time.monotonic() + timeout
            start = len(output)
            while time.monotonic() < deadline:
                if select.select([master], [], [], 0.1)[0]:
                    try:
                        data = os.read(master, 65536)
                    except OSError as exc:
                        if exc.errno == errno.EIO:
                            break
                        raise
                    output.extend(data)
                    if marker in output[start:]:
                        return
                if proc.poll() is not None:
                    break
            raise AssertionError(f"Missing terminal output: {marker!r}")

        try:
            until(b"CLICK TO DROP IN")
            until(b"READY  [")
            until(b"T STYLE reactor")
            start = len(output)
            os.write(master, b"sS")
            until(b"T STYLE reactor")
            check(
                b"NORMAL" in output[start:]
                and b"LONGSHOT" not in output[start:]
                and b"SCATTER" not in output[start:],
                "S must not switch weapon modes",
            )
            for name in (
                "synthwave",
                "acid",
                "glacier",
                "inferno",
                "sakura",
                "ultraviolet",
                "outrun",
                "matrix",
                "prism",
                "reactor",
            ):
                os.write(master, b"t")
                until(b"T STYLE " + name.encode())
                check_pickup_hud_colors(output, name)
            os.write(master, b"[")
            until(b"T STYLE prism")
            with open(os.path.join(state, "zapy", "theme")) as saved:
                check(saved.read().strip() == "prism", "style preference was not saved")
            os.write(master, b"]")
            until(b"T STYLE reactor")
            check(b"\033[49m" in output, "terminal default background is not used")
            plain = re.sub(rb"\x1b\[[0-9;?]*[A-Za-z]", b"", bytes(output))
            check(
                b"HP" in plain and b"AMMO" in plain and "▀".encode() in plain,
                "large symbolic HP/ammo displays are missing",
            )
            check(b"STREAK" in plain, "large side streak display is missing")
            check(
                b"HP 3" not in plain and b"AMMO 4" not in plain,
                "HP/ammo should not contain numeric counters",
            )
            lines = plain.split(b"\r\n")
            header = next(
                line for line in lines if b"BEST " in line and b"HEIGHT " in line
            )
            check(b"zapy" in lines[0].lower(), "Zapy header branding is missing")
            check(
                b"backblast" not in plain.lower(), "legacy branding is still rendered"
            )
            check(
                header.index(b"BEST ") < header.index(b"HEIGHT "),
                "best and current height placements were not swapped",
            )
            check(
                any(b"HP" in line and b"AMMO" in line for line in lines[-7:]),
                "HP/ammo displays are not in the bottom corners",
            )
            os.write(master, b"\r")
            until(b"AIM BELOW YOURSELF")
            os.write(master, b"p")
            until(b"P A U S E D")
            os.write(master, b"p")
            until(b"AIM BELOW YOURSELF")
            fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 18, 60, 0, 0))
            until(b"Resize terminal")
            fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 38, 110, 0, 0))
            until(b"AIM BELOW YOURSELF")
            # Real SGR mouse packets, including press, drag, and release.
            os.write(master, b"\033[<0;55;32M\033[<32;55;32M")
            until(b"RELOAD [")
            os.write(master, b"\033[<0;55;32m")
            until(b"HEIGHT 2 m")
            fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 180, 0, 0))
            until(b"AIM BELOW YOURSELF")
            os.write(master, b"r")
            until(b"AIM BELOW YOURSELF")
            if interrupt:
                proc.send_signal(signal.SIGINT)
            else:
                os.write(master, b"q")
            until(b"\033[?1049l")
            proc.wait(timeout=4)
            check(proc.returncode == 0, "game exited unsuccessfully")
            check(
                termios.tcgetattr(slave) == original,
                "terminal settings were not restored",
            )
            check(
                b"\033[?1006l" in output and b"\033[?25h" in output,
                "mouse/cursor were not restored",
            )
            check(
                os.path.exists(os.path.join(state, "zapy", "best-height")),
                "high score was not persisted",
            )
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.wait()
            os.close(master)
            os.close(slave)


def state_compatibility(binary):
    with tempfile.TemporaryDirectory() as state:
        legacy = os.path.join(state, "backblast")
        current = os.path.join(state, "zapy")
        os.mkdir(legacy)
        preferences = {"best-height": b"42\n", "theme": b"prism\n"}
        for name, contents in preferences.items():
            with open(os.path.join(legacy, name), "wb") as saved:
                saved.write(contents)

        def launch(best, style, change_style=False, streak=0, retry=False):
            master, slave = pty.openpty()
            original = termios.tcgetattr(slave)
            fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 38, 110, 0, 0))
            proc = subprocess.Popen(
                [binary, "--seed", "42"],
                stdin=slave,
                stdout=slave,
                stderr=slave,
                env={**os.environ, "XDG_STATE_HOME": state},
            )
            output = bytearray()

            def until(marker, timeout=4):
                deadline = time.monotonic() + timeout
                while time.monotonic() < deadline:
                    if marker in output:
                        return
                    if select.select([master], [], [], 0.1)[0]:
                        try:
                            data = os.read(master, 65536)
                        except OSError as exc:
                            if exc.errno == errno.EIO:
                                break
                            raise
                        if not data:
                            break
                        output.extend(data)
                    elif proc.poll() is not None:
                        break
                raise AssertionError(f"Missing compatibility output: {marker!r}")

            def check_streak():
                marker = output.rfind(b"T STYLE ")
                start = output.rfind(b"\033[H", 0, marker)
                frame = (
                    re.sub(rb"\x1b\[[0-9;?]*[A-Za-z]", b"", bytes(output[start:]))
                    .decode()
                    .split("\r\n")
                )
                x = frame[4].index("STREAK")
                label = f"BEST {streak}"
                check(
                    frame[5][x : x + len(label)] == label,
                    "saved streak record must appear directly below STREAK, not in height header",
                )

            try:
                until(b"CLICK TO DROP IN")
                until(b"T STYLE " + style.encode())
                check_streak()
                if retry:
                    output.clear()
                    os.write(master, b"R")
                    until(b"T STYLE " + style.encode())
                    check_streak()
                plain = re.sub(rb"\x1b\[[0-9;?]*[A-Za-z]", b"", bytes(output))
                header = next(
                    line
                    for line in plain.split(b"\r\n")
                    if b"BEST " in line and b"HEIGHT " in line
                )
                check(
                    b"zapy" in plain.split(b"\r\n")[0].lower(),
                    "compatibility launch lacks Zapy header",
                )
                check(
                    re.search(rb"BEST " + str(best).encode() + rb"\b", header),
                    "saved best height was not loaded",
                )
                check(b"backblast" not in plain.lower(), "legacy branding is rendered")
                if change_style:
                    output.clear()
                    os.write(master, b"T")
                    until(b"T STYLE reactor")
                    with open(os.path.join(current, "theme"), "rb") as saved:
                        check(
                            saved.read() == b"reactor\n",
                            "style was not saved under zapy",
                        )
                os.write(master, b"q")
                until(b"\033[?1049l")
                proc.wait(timeout=4)
                check(
                    proc.returncode == 0, "compatibility launch exited unsuccessfully"
                )
                check(
                    termios.tcgetattr(slave) == original,
                    "compatibility launch did not restore terminal",
                )
            finally:
                if proc.poll() is None:
                    proc.kill()
                    proc.wait()
                os.close(master)
                os.close(slave)

        def check_legacy():
            check(
                set(os.listdir(legacy)) == set(preferences),
                "legacy preference files changed",
            )
            for name, contents in preferences.items():
                with open(os.path.join(legacy, name), "rb") as saved:
                    check(
                        saved.read() == contents, "legacy preference contents changed"
                    )

        launch(42, "prism", change_style=True)
        check_legacy()

        with open(os.path.join(current, "best-height"), "wb") as saved:
            saved.write(b"99\n")
        with open(os.path.join(current, "best-streak"), "wb") as saved:
            saved.write(b"7\n")
        launch(99, "reactor", streak=7, retry=True)
        launch(99, "reactor", streak=7)
        with open(os.path.join(current, "best-streak"), "rb") as saved:
            check(saved.read() == b"7\n", "retry/quit/relaunch changed streak record")
        with open(os.path.join(current, "best-streak"), "wb") as saved:
            saved.write(b"-7\n")
        launch(99, "reactor", streak=0)
        check_legacy()
        for name, contents in {"best-height": b"99\n", "theme": b"reactor\n"}.items():
            with open(os.path.join(current, name), "rb") as saved:
                check(
                    saved.read() == contents, "new preferences did not take precedence"
                )


def debug_runs(binary):
    for preloaded in (True, False):
        with tempfile.TemporaryDirectory() as state:
            current = os.path.join(state, "zapy")
            baseline = {
                "best-height": b"1\n",
                "best-streak": b"7\n",
                "theme": b"prism\n",
            }
            if preloaded:
                os.mkdir(current)
                for name, contents in baseline.items():
                    with open(os.path.join(current, name), "wb") as saved:
                        saved.write(contents)

            def unchanged():
                if preloaded:
                    check(
                        set(os.listdir(current)) == set(baseline),
                        "unranked run added state files",
                    )
                    for name, contents in baseline.items():
                        with open(os.path.join(current, name), "rb") as saved:
                            check(
                                saved.read() == contents, f"unranked run changed {name}"
                            )
                else:
                    for name in ("best-height", "best-streak"):
                        path = os.path.join(current, name)
                        if os.path.exists(path):
                            with open(path, "rb") as saved:
                                check(
                                    int(saved.read()) == 0,
                                    f"fresh debug run wrote a positive {name}",
                                )
                        check(
                            not os.path.exists(path + ".tmp"),
                            "debug run left a temporary record",
                        )

            def launch(eligible=False):
                master, slave = pty.openpty()
                original = termios.tcgetattr(slave)
                fcntl.ioctl(
                    slave, termios.TIOCSWINSZ, struct.pack("HHHH", 38, 110, 0, 0)
                )
                proc = subprocess.Popen(
                    [binary, "--seed", "42"],
                    stdin=slave,
                    stdout=slave,
                    stderr=slave,
                    env={**os.environ, "XDG_STATE_HOME": state},
                )
                output = bytearray()

                def frame(predicate, timeout=4):
                    start = len(output)
                    deadline = time.monotonic() + timeout
                    while time.monotonic() < deadline:
                        # A following home sequence terminates a complete frame.
                        frames = bytes(output[start:]).split(b"\033[H")[1:-1]
                        for raw in frames:
                            plain = re.sub(rb"\x1b\[[0-9;?]*[A-Za-z]", b"", raw)
                            lines = plain.split(b"\r\n")
                            if len(lines) == 38 and predicate(lines):
                                strip = lines[-1]
                                check(
                                    b"LMB fire" in strip,
                                    "visible control strip is missing",
                                )
                                check(
                                    not re.search(
                                        rb"\b[xX]\b|debug|switch|cycle|unranked",
                                        strip,
                                        re.I,
                                    ),
                                    "hidden mode key is advertised in control strip",
                                )
                                return lines
                        if proc.poll() is not None:
                            break
                        if select.select([master], [], [], 0.1)[0]:
                            try:
                                data = os.read(master, 65536)
                            except OSError as exc:
                                if exc.errno == errno.EIO:
                                    break
                                raise
                            if not data:
                                break
                            output.extend(data)
                    raise AssertionError("Missing debug-run terminal frame")

                def mode(lines, name, unranked=True):
                    return name in lines[-5] and (b"UNRANKED" in lines[0]) == unranked

                def height(lines):
                    match = re.search(rb"HEIGHT (\d+) m", lines[1])
                    return int(match[1]) if match else -1

                def fire():
                    os.write(master, b"\033[<0;55;32M")
                    risen = frame(lambda lines: height(lines) > 1, timeout=6)
                    os.write(master, b"\033[<0;55;32m")
                    return risen

                try:
                    frame(lambda lines: b"CLICK TO DROP IN" in b"\n".join(lines))
                    os.write(master, b"x")
                    frame(
                        lambda lines: (
                            mode(lines, b"LONGSHOT")
                            and b"CLICK TO DROP IN" in b"\n".join(lines)
                        )
                    )
                    os.write(master, b"\rp")
                    frame(lambda lines: b"P A U S E D" in b"\n".join(lines))
                    os.write(master, b"X")
                    frame(
                        lambda lines: (
                            mode(lines, b"SCATTER")
                            and b"P A U S E D" in b"\n".join(lines)
                        )
                    )
                    os.write(master, b"x")
                    frame(
                        lambda lines: (
                            mode(lines, b"NORMAL")
                            and b"P A U S E D" in b"\n".join(lines)
                        )
                    )
                    os.write(master, b"p")
                    frame(
                        lambda lines: (
                            mode(lines, b"NORMAL")
                            and b"P A U S E D" not in b"\n".join(lines)
                        )
                    )
                    risen = fire()
                    check(
                        mode(risen, b"NORMAL"), "cycled Normal shot run lost UNRANKED"
                    )
                    check(
                        re.search(
                            rb"BEST " + (b"1" if preloaded else b"0") + rb" m\b",
                            risen[1],
                        )
                        and re.search(
                            rb"BEST " + (b"7" if preloaded else b"0") + rb"\b", risen[5]
                        ),
                        "unranked HUD replaced legitimate height/streak records",
                    )
                    unchanged()
                    frame(
                        lambda lines: b"R U N  O V E R" in b"\n".join(lines), timeout=25
                    )
                    os.write(master, b"X")
                    frame(
                        lambda lines: (
                            mode(lines, b"NORMAL")
                            and b"R U N  O V E R" in b"\n".join(lines)
                        )
                    )
                    unchanged()
                    if eligible:
                        os.write(master, b"R")
                        frame(
                            lambda lines: (
                                mode(lines, b"NORMAL", False) and height(lines) == 0
                            )
                        )
                        fire()
                        # Disqualify provisional height and reset in the same input batch.
                        os.write(master, b"XRp")
                        frame(
                            lambda lines: (
                                mode(lines, b"NORMAL", False)
                                and b"P A U S E D" in b"\n".join(lines)
                            )
                        )
                        unchanged()
                        os.write(master, b"R")
                        frame(
                            lambda lines: (
                                mode(lines, b"NORMAL", False) and height(lines) == 0
                            )
                        )
                        fire()
                        os.write(master, b"R")
                        frame(
                            lambda lines: (
                                mode(lines, b"NORMAL", False) and height(lines) == 0
                            )
                        )
                        with open(os.path.join(current, "best-height"), "rb") as saved:
                            check(
                                int(saved.read()) > 1,
                                "R did not finish an eligible post-debug run",
                            )
                        for name in ("best-streak", "theme"):
                            with open(os.path.join(current, name), "rb") as saved:
                                check(
                                    saved.read() == baseline[name],
                                    f"eligible retry changed {name}",
                                )
                    os.write(master, b"q")
                    deadline = time.monotonic() + 4
                    while b"\033[?1049l" not in output and time.monotonic() < deadline:
                        if select.select([master], [], [], 0.1)[0]:
                            try:
                                output.extend(os.read(master, 65536))
                            except OSError as exc:
                                if exc.errno == errno.EIO:
                                    break
                                raise
                    proc.wait(timeout=4)
                    check(
                        proc.returncode == 0, "debug-run launch exited unsuccessfully"
                    )
                    check(
                        b"\033[?1049l" in output,
                        "debug-run launch did not leave alternate screen",
                    )
                    check(
                        termios.tcgetattr(slave) == original,
                        "debug-run launch did not restore terminal",
                    )
                finally:
                    if proc.poll() is None:
                        proc.kill()
                        proc.wait(timeout=4)
                    os.close(master)
                    os.close(slave)

            launch()
            unchanged()
            if preloaded:
                launch(eligible=True)


if __name__ == "__main__":
    binary = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "build/zapy")
    help_output = subprocess.check_output([binary, "--help"])
    check(b"zapy" in help_output.lower(), "Zapy help branding is missing")
    check(b"backblast" not in help_output.lower(), "legacy branding remains in help")
    check(
        not re.search(
            rb"\b[xX]\s*(?:/\s*[xX]\s*)?:|\b[xX]\s+(?:mode|switch|cycle)|debug|unranked|cycle",
            help_output,
            re.I,
        ),
        "hidden mode key is advertised in --help",
    )
    styles = subprocess.check_output([binary, "--list-themes"], text=True).splitlines()
    check(
        len(styles) == 10 and len(set(styles)) == 10,
        "ten unique styles must be available",
    )
    bad = subprocess.run([binary, "--theme", "not-a-style"], capture_output=True)
    check(
        bad.returncode != 0 and b"Unknown theme" in bad.stderr,
        "invalid style was not rejected",
    )
    state_compatibility(binary)
    debug_runs(binary)
    run(binary)
    run(binary, interrupt=True)
    print(
        "PASS: start, mouse fire, pause/resume, resize, retry, quit, SIGINT, persistence, hidden mode cycling/unranked record isolation/retry eligibility, legacy preference fallback/new-file precedence, terminal restoration"
    )
