"""Run the production macOS permission function with a fake native API.

Usage: python3 tools/tests/test_mac_permissions.py
Only this standalone harness is compiled; AyuGram and Qt are not built.
"""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "Telegram/SourceFiles/platform/mac/specific_mac.mm"
HARNESS = Path(__file__).with_name("mac_permission_harness.mm")


@unittest.skipUnless(sys.platform == "darwin", "Requires macOS and Xcode tools")
class MacPermissionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="ayu-mac-permissions-")
        cls.addClassCleanup(cls.temp.cleanup)
        directory = Path(cls.temp.name)
        source = SOURCE.read_text(encoding="utf-8")
        start = source.index("void RequestPermission(")
        end = source.index("\n#pragma clang diagnostic pop", start)
        function = source[start:end]
        harness = HARNESS.read_text(encoding="utf-8")
        if harness.count("/* REQUEST_PERMISSION */") != 1:
            raise ValueError("Expected exactly one production-function marker")
        generated = directory / "permissions.mm"
        generated.write_text(
            harness.replace("/* REQUEST_PERMISSION */", function),
            encoding="utf-8",
        )
        cls.binary = directory / "permissions"
        subprocess.run(
            [
                "xcrun", "clang++", "-std=c++17", "-fobjc-arc", "-fblocks",
                "-Wall", "-Wextra", "-Werror", "-framework", "Foundation",
                str(generated), "-o", str(cls.binary),
            ],
            check=True, capture_output=True, text=True, timeout=60,
        )

    def run_scenario(self, media, answer, available="available"):
        result = subprocess.run(
            [str(self.binary), media, answer, available],
            capture_output=True, text=True, timeout=10,
        )
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_microphone_granted_after_native_answer(self):
        self.run_scenario("microphone", "grant")

    def test_microphone_denied_after_native_answer(self):
        self.run_scenario("microphone", "deny")

    def test_camera_granted_after_native_answer(self):
        self.run_scenario("camera", "grant")

    def test_camera_denied_after_native_answer(self):
        self.run_scenario("camera", "deny")

    def test_microphone_without_native_permission_api(self):
        self.run_scenario("microphone", "grant", "unavailable")

    def test_camera_without_native_permission_api(self):
        self.run_scenario("camera", "grant", "unavailable")


if __name__ == "__main__":
    unittest.main(verbosity=2)
