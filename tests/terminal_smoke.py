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


def run(binary, interrupt=False):
    master, slave = pty.openpty()
    original = termios.tcgetattr(slave)
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 38, 110, 0, 0))
    with tempfile.TemporaryDirectory() as state:
        proc = subprocess.Popen(
            [binary, "--seed", "42"], stdin=slave, stdout=slave, stderr=slave,
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
            check(b"\033[49m" in output, "terminal default background is not used")
            plain = re.sub(rb"\x1b\[[0-9;?]*[A-Za-z]", b"", bytes(output))
            check(b"HP" in plain and b"AMMO" in plain and "▀".encode() in plain,
                "large symbolic HP/ammo displays are missing")
            check(b"STREAK" in plain, "large side streak display is missing")
            check(b"HP 3" not in plain and b"AMMO 4" not in plain,
                "HP/ammo should not contain numeric counters")
            lines = plain.split(b"\r\n")
            header = next(line for line in lines if b"BEST " in line and b"HEIGHT " in line)
            check(header.index(b"BEST ") < header.index(b"HEIGHT "),
                "best and current height placements were not swapped")
            check(any(b"HP" in line and b"AMMO" in line for line in lines[-7:]),
                "HP/ammo displays are not in the bottom corners")
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
            check(termios.tcgetattr(slave) == original, "terminal settings were not restored")
            check(b"\033[?1006l" in output and b"\033[?25h" in output,
                  "mouse/cursor were not restored")
            check(os.path.exists(os.path.join(state, "backblast", "best-height")),
                  "high score was not persisted")
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.wait()
            os.close(master)
            os.close(slave)


if __name__ == "__main__":
    binary = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "build/backblast")
    run(binary)
    run(binary, interrupt=True)
    print("PASS: start, mouse fire, pause/resume, resize, retry, quit, SIGINT, persistence, terminal restoration")