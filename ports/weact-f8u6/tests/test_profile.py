"""Integration checks for board selection and upstream sketch configuration."""
import sys
import unittest
from pathlib import Path

PORT = Path(__file__).resolve().parents[1]
REPO = PORT.parents[1]
sys.path.insert(0, str(PORT / "platform/builder"))
from chgame_profile import application, find_sketch, validate_sources


def profile(sketch="", project=REPO, **overrides):
    options = {"custom_chgame_sketch": sketch, **overrides}
    return application(REPO, project, lambda key, default=None: options.get(key, default), environ={})


class ProfileTests(unittest.TestCase):
    def test_entry_points_select_same_bringup(self):
        for project in (REPO, PORT):
            app = profile(project=project, custom_chgame_save_pages="0")
            self.assertEqual(app["sketch"], PORT / "src")
            self.assertEqual(app["options"]["usb"], "serial")

    def test_hello_is_a_shared_example(self):
        self.assertEqual(find_sketch(REPO, REPO, "Hello"),
                         REPO / "platform/board/arduino/CHGame/libraries/CHGame/examples/Hello")

    def test_sd_app_retains_required_options(self):
        app = profile("CHSDtoUSB")
        self.assertEqual(app["options"]["opt"], "osstd")
        self.assertEqual(app["options"]["usb"], "serial")
        self.assertEqual(profile("CHSDtoSerial")["defines"], ["GFX_CHUNK_ROWS=1"])

    def test_game_has_variant_and_save_budget(self):
        app = profile("CHFour")
        self.assertEqual(app["board"]["build.board"], "CHGAME_WEACT_F8U6")
        self.assertEqual(app["options"]["usb"], "uploadonly")
        self.assertEqual(app["save_pages"], 2)
        self.assertEqual(int(app["board"]["upload.maximum_size"]) - 256 * app["save_pages"], 50432)

    def test_invalid_save_page_count_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "0, 1 or 2"):
            profile("CHFour", custom_chgame_save_pages="3")

    def test_changed_memory_layout_is_rejected(self):
        def changed(path):
            data = (REPO / path).read_bytes()
            return data.replace(b"50944", b"60000") if path.endswith("link_chgame_app.ld") else data
        with self.assertRaisesRegex(ValueError, "flash layout changed"):
            validate_sources(REPO, changed)


if __name__ == "__main__":
    unittest.main()
