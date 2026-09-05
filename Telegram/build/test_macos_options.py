import unittest

from prepare.macos_options import qt_options


class MacOptionsTest(unittest.TestCase):
    def test_default_preserves_debug_release_and_intel(self):
        self.assertEqual(qt_options([]), ("-debug-and-release", "x86_64;arm64"))

    def test_release_only(self):
        self.assertEqual(qt_options(["qt-release-only"]), ("-release", "x86_64;arm64"))

    def test_debug_only(self):
        self.assertEqual(qt_options(["skip-release"]), ("-debug", "x86_64;arm64"))

    def test_each_architecture(self):
        for arch in ("arm64", "x86_64"):
            with self.subTest(arch=arch):
                self.assertEqual(qt_options(["mac-" + arch, "qt-release-only"]), ("-release", arch))

    def test_conflicting_architectures(self):
        with self.assertRaises(ValueError):
            qt_options(["mac-arm64", "mac-x86_64"])

    def test_conflicting_configurations(self):
        with self.assertRaises(ValueError):
            qt_options(["qt-release-only", "skip-release"])
