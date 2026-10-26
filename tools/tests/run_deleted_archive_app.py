#!/usr/bin/env python3
"""Build and run the archive scenario twice in a disposable application profile."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "Telegram/SourceFiles/test/test_scenario.cpp"
SCENARIO = ROOT / "Telegram/SourceFiles/test/scenarios/deleted_message_archive.cpp"


def run(command, log, timeout, env=None):
    print(f"Running: {' '.join(map(str, command))}\nLog: {log}", flush=True)
    started = time.monotonic()
    with log.open("w") as output:
        process = subprocess.Popen(
            list(map(str, command)), cwd=ROOT, env=env,
            stdout=output, stderr=subprocess.STDOUT,
        )
        try:
            while True:
                try:
                    result = process.wait(timeout=min(30, timeout))
                    break
                except subprocess.TimeoutExpired:
                    elapsed = time.monotonic() - started
                    if elapsed >= timeout:
                        raise TimeoutError(f"Timed out after {elapsed:.0f}s: {log}")
                    lines = log.read_text(errors="replace").splitlines()
                    last = lines[-1] if lines else "waiting for output"
                    print(f"{elapsed:.0f}s: {last[:240]}", flush=True)
        except BaseException:
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
            raise
    if result:
        tail = "\n".join(log.read_text(errors="replace").splitlines()[-35:])
        raise RuntimeError(f"Command exited {result}:\n{tail}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "out")
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--evidence-dir", type=Path)
    options = parser.parse_args()
    build_dir = options.build_dir.resolve()
    evidence = (options.evidence_dir or Path(tempfile.mkdtemp(
        prefix="ayu-archive-app-"
    ))).resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    profile = evidence / "profile"
    if profile.exists():
        raise RuntimeError(f"A fresh evidence directory is required: {evidence}")
    profile.mkdir()
    (profile / "testing").touch()
    (evidence / "scenario.cpp").write_bytes(SCENARIO.read_bytes())
    binary = build_dir / "Debug/AyuGram.app/Contents/MacOS/AyuGram"
    build = ["cmake", "--build", build_dir, "--config", "Debug",
             "--target", "Telegram", "--parallel", options.jobs]
    original = SOURCE.read_bytes()
    overlay_built = False
    report = {"evidence_dir": str(evidence), "phases": {}}
    try:
        SOURCE.write_bytes(SCENARIO.read_bytes())
        run(build, evidence / "build-scenario.log", 7200)
        overlay_built = True
        for phase in ("write", "read"):
            phase_dir = evidence / phase
            phase_dir.mkdir()
            env = os.environ.copy()
            env.update({
                "AYU_ARCHIVE_TEST_PHASE": phase,
                "TDESKTOP_TEST_EVIDENCE_DIR": str(phase_dir),
                "TDESKTOP_TEST_WATCHDOG": "600",
            })
            run([binary, "-many", "-workdir", profile, "-testagent", "-debug"],
                phase_dir / "application.log", 630, env)
            log = phase_dir / "test_log.txt"
            text = log.read_text() if log.exists() else ""
            passed = "SCENARIO_RESULT: PASS" in text and "TEST_COMPLETE" in text
            report["phases"][phase] = {
                "passed": passed,
                "log": str(log),
                "memory": [line for line in text.splitlines() if "Footprint=" in line],
                "screenshots": [str(path) for path in (phase_dir / "screenshots").glob("*.png")],
            }
            (evidence / "report.json").write_text(json.dumps(report, indent=2))
            print(text, flush=True)
            if not passed:
                raise RuntimeError(f"Scenario {phase} failed: {log}")
    finally:
        SOURCE.write_bytes(original)
        if overlay_built:
            run(build, evidence / "build-final.log", 1800)
    print(f"Archive application checks passed. Evidence: {evidence}", flush=True)


if __name__ == "__main__":
    main()
