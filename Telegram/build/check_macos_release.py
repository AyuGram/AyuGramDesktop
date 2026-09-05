import argparse
import json
import re
import shlex
from pathlib import Path


def check(build, architectures=("arm64",), disable_autoupdate=False):
    expected = set(architectures)
    if not expected or not expected <= {"arm64", "x86_64"}:
        raise ValueError("Unsupported macOS architecture")
    cache = dict(re.findall(r"^([^:#/][^:=]*):[^=]+=(.*)$", (build / "CMakeCache.txt").read_text(), re.M))
    if cache.get("CMAKE_BUILD_TYPE") != "Release":
        raise ValueError("Expected a Release build")
    if set(cache.get("CMAKE_OSX_ARCHITECTURES", "").split(";")) != expected:
        raise ValueError("CMake architectures do not match the requested target")
    if disable_autoupdate and cache.get("DESKTOP_APP_DISABLE_AUTOUPDATE") != "ON":
        raise ValueError("CI preview artifacts must not install upstream updates")
    commands = json.loads((build / "compile_commands.json").read_text())
    product = [
        entry for entry in commands
        if "/Telegram/SourceFiles/" in entry["file"]
    ]
    if not product:
        raise ValueError("No application compilation commands")
    for entry in product:
        args = entry.get("arguments") or shlex.split(entry["command"])
        optimization = [arg for arg in args if re.fullmatch(r"-O[0-3szg]", arg)]
        if not optimization or optimization[-1] not in ("-O2", "-O3", "-Os", "-Oz"):
            raise ValueError(f"Optimization disabled: {entry['file']}")
        definitions = {arg[2:].split("=")[0] for arg in args if arg.startswith("-D")}
        undefinitions = {arg[2:] for arg in args if arg.startswith("-U")}
        if "_DEBUG" in definitions or "NDEBUG" not in definitions or "NDEBUG" in undefinitions:
            raise ValueError(f"Debug configuration: {entry['file']}")
        actual = {args[i + 1] for i, arg in enumerate(args[:-1]) if arg == "-arch"}
        if actual != expected or args[-1:] == ["-arch"]:
            raise ValueError(f"Wrong architecture: {entry['file']}")
    ninja = (build / "build.ninja").read_text()
    if re.search(r"libQt6\w+_debug\.a", ninja):
        raise ValueError("Release build links Debug Qt")
    print(f"Release {';'.join(sorted(expected))}: checked {len(product)} compilation commands")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("build", type=Path)
    parser.add_argument("--architectures", nargs="+", choices=("arm64", "x86_64"), default=["arm64"])
    parser.add_argument("--disable-autoupdate", action="store_true")
    args = parser.parse_args()
    check(args.build, args.architectures, args.disable_autoupdate)
