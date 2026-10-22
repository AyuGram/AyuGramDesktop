import json
import tempfile
import unittest
from pathlib import Path

from check_macos_release import check


class ReleaseCheckTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.build = Path(self.directory.name)
        (self.build / "CMakeCache.txt").write_text(
            "CMAKE_BUILD_TYPE:STRING=Release\n"
            "CMAKE_OSX_ARCHITECTURES:STRING=arm64\n"
            "DESKTOP_APP_DISABLE_AUTOUPDATE:BOOL=ON\n"
        )
        (self.build / "build.ninja").write_text("libQt6Widgets.a")
        self.command = {
            "file": "/src/Telegram/SourceFiles/core/application.cpp",
            "command": "clang++ -O3 -DNDEBUG -arch arm64 -c application.cpp",
        }

    def run_check(self, architectures=("arm64",), disable_autoupdate=False):
        (self.build / "compile_commands.json").write_text(json.dumps([self.command]))
        check(self.build, architectures, disable_autoupdate)

    def test_release(self):
        self.run_check()

    def test_debug_optimization_rejected(self):
        self.command["command"] += " -O0"
        with self.assertRaises(ValueError):
            self.run_check()

    def test_debug_defines_rejected(self):
        self.command["command"] += " -D_DEBUG"
        with self.assertRaises(ValueError):
            self.run_check()

    def test_intel_rejected(self):
        self.command["command"] = self.command["command"].replace("arm64", "x86_64")
        with self.assertRaises(ValueError):
            self.run_check()

    def test_debug_qt_rejected(self):
        (self.build / "build.ninja").write_text("libQt6Widgets_debug.a")
        with self.assertRaises(ValueError):
            self.run_check()

    def test_wrong_build_type_rejected(self):
        path = self.build / "CMakeCache.txt"
        path.write_text(path.read_text().replace("=Release", "=Debug"))
        with self.assertRaises(ValueError):
            self.run_check()

    def test_each_target(self):
        for architectures in (("x86_64",), ("x86_64", "arm64")):
            with self.subTest(architectures=architectures):
                (self.build / "CMakeCache.txt").write_text(
                    "CMAKE_BUILD_TYPE:STRING=Release\n"
                    "CMAKE_OSX_ARCHITECTURES:STRING=" + ";".join(architectures) + "\n"
                )
                self.command["command"] = "clang++ -O3 -DNDEBUG " + " ".join("-arch " + arch for arch in architectures)
                self.run_check(architectures)

    def test_extra_architecture_rejected(self):
        self.command["command"] += " -arch x86_64"
        with self.assertRaises(ValueError):
            self.run_check()

    def test_debug_define_with_value_rejected(self):
        self.command["command"] += " -D_DEBUG=1"
        with self.assertRaises(ValueError):
            self.run_check()

    def test_undefined_ndebug_rejected(self):
        self.command["command"] += " -UNDEBUG"
        with self.assertRaises(ValueError):
            self.run_check()

    def test_preview_requires_disabled_updater(self):
        path = self.build / "CMakeCache.txt"
        path.write_text(path.read_text().replace("=ON", "=OFF"))
        self.run_check()
        with self.assertRaises(ValueError):
            self.run_check(disable_autoupdate=True)

    def test_cache_value_must_match_exactly(self):
        path = self.build / "CMakeCache.txt"
        path.write_text(path.read_text().replace("=Release", "=ReleaseDebug"))
        with self.assertRaises(ValueError):
            self.run_check()


if __name__ == "__main__":
    unittest.main()
