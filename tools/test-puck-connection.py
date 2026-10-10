#!/usr/bin/env python3
"""Run the firmware's actual SteamPuckController::task with a fake USB bus.

Usage: python tools/test-puck-connection.py --cxx clang++
       python tools/test-puck-connection.py --cxx /path/to/zig --zig

The generated include comes directly from puck_hid.cpp; the test does not
duplicate its connection logic. Each scenario starts a new process to reset
the task's function-local static state.
"""

import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="c++")
    parser.add_argument("--zig", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    source = (root / "OpenPuck/puck_hid.cpp").read_text(encoding="utf-8")
    signature = "void SteamPuckController::task()"
    start = source.index(signature)
    # task() is the final definition in this translation unit.
    task = source[start:]
    if not task.rstrip().endswith("}"):
        raise RuntimeError("Could not locate the complete task definition")
    with tempfile.TemporaryDirectory(prefix="puck-connection-") as work:
        work = Path(work)
        (work / "puck_task.inc").write_text(task, encoding="utf-8")
        exe = work / "puck-connection.exe"
        command = [args.cxx]
        if args.zig:
            command.extend(["c++", "-nostdlib++", "-fno-exceptions",
                            "-fno-threadsafe-statics"])
        subprocess.run(
            command + ["-std=c++11", "-Wall", "-Wextra", "-Werror",
                       "-I", str(work), "-I", str(root / "OpenPuck"),
                       str(root / "tools/test-puck-connection.cpp"),
                       "-o", str(exe)], check=True,
        )
        failures = []
        for scenario in ("busy", "remount", "rf-drop", "rebind",
                         "multi-slot", "power-off", "suspend"):
            result = subprocess.run([str(exe), scenario], check=False)
            if result.returncode:
                failures.append(scenario)
        if failures:
            raise SystemExit("Failed: " + ", ".join(failures))


if __name__ == "__main__":
    main()
