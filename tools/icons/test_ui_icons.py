import copy
import hashlib
import json
import tempfile
import unittest
import urllib.error
from pathlib import Path
from unittest.mock import patch

import generate_ui_icons as ui
from generate_icons import convert, fetch

SVG = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256" fill="currentColor">'
FG = '<path d="M0 0H256V256H0Z"/>'
BG = '<path opacity="0.2" d="M10 10H20V20H10Z"/>'


class UiIconTests(unittest.TestCase):
    def setUp(self):
        self.manifest = json.loads(ui.DEFAULT_MANIFEST.read_text(encoding="utf-8"))
        self.icon = copy.deepcopy(self.manifest["icons"][0])

    def test_registered_manifest_pins_every_weight(self):
        ui.validate(self.manifest)
        self.assertGreater(len(self.manifest["icons"]), 0)
        for icon in self.manifest["icons"]:
            self.assertEqual(set(ui.WEIGHTS), set(icon["sha256"]))

    def test_invalid_weight_name_size_or_hash_rejects(self):
        for change in (
            lambda m: m.update(weight="unknown"),
            lambda m: m["icons"][0].update(name="../alarm"),
            lambda m: m["icons"][0].update(sizes=[0]),
            lambda m: m["icons"][0].update(sizes=[14, 14]),
            lambda m: m["icons"][0].update(sha256={}),
            lambda m: m["icons"].append(copy.deepcopy(m["icons"][0])),
            lambda m: m["source"].update(revision="main"),
        ):
            manifest = copy.deepcopy(self.manifest)
            change(manifest)
            with self.assertRaises(ValueError):
                ui.validate(manifest)

    def test_monochrome_resolves_current_color_without_shading(self):
        fg, bg, opacity = ui.split_svg(SVG + FG + "</svg>", "regular")
        self.assertIn('#ffffff', fg)
        self.assertNotIn("currentColor", fg)
        self.assertIsNone(bg)
        self.assertEqual(0, opacity)

    def test_duotone_splits_and_applies_opacity_once(self):
        fg, bg, opacity = ui.split_svg(SVG + BG + FG + "</svg>", "duotone")
        self.assertEqual(51, opacity)
        self.assertNotIn('opacity=', fg)
        self.assertNotIn('opacity=', bg)
        self.assertIn('M10 10', bg)
        self.assertNotIn('M10 10', fg)
        self.assertIn('M0 0', fg)

    def test_group_transforms_are_preserved(self):
        fg, bg, _ = ui.split_svg(
            SVG + '<g transform="translate(2 3)" opacity="0.2">' + FG + "</g>" + FG + "</svg>",
            "duotone",
        )
        self.assertIn('translate(2 3)', bg)
        self.assertNotIn('translate(2 3)', fg)

    def test_official_duotone_can_intentionally_have_no_shade(self):
        _, bg, opacity = ui.split_svg(SVG + FG + "</svg>", "duotone")
        self.assertIsNone(bg)
        self.assertEqual(0, opacity)

    def test_unsupported_structures_fail_instead_of_flattening(self):
        for content in (
            BG, '<path opacity="0.5" d="M0 0H10V10Z"/>',
            '<image href="external.svg"/>', '<path style="fill:red" d="M0 0Z"/>',
            '<g opacity="0.2"><path opacity="0.2" d="M0 0Z"/></g>',
        ):
            with self.assertRaises(ValueError):
                ui.split_svg(SVG + content + "</svg>", "regular")

    def test_coverage_is_raw_alpha_not_weather_luminance(self):
        rgba = bytes([0, 0, 0, 255, 255, 255, 255, 128, 1, 2, 3, 64, 0, 0, 0, 0])
        self.assertEqual(bytes([255, 128, 64, 0]), ui.coverage(rgba, 2))
        _, weather_alpha = convert(rgba, (0, 0, 0))
        self.assertNotEqual(weather_alpha, ui.coverage(rgba, 2))
        with self.assertRaises(ValueError):
            ui.coverage(bytes(16), 2)
        with self.assertRaises(ValueError):
            ui.coverage(bytes(4), 2)

    def test_registration_resolves_names_and_records_all_weight_hashes(self):
        urls = []
        def download(url):
            urls.append(url)
            return (SVG + BG + FG + "</svg>" if "/duotone/" in url else SVG + FG + "</svg>").encode()
        updated = ui.register(self.manifest, ["bell"], [14, 36], download)
        added = next(icon for icon in updated["icons"] if icon["name"] == "bell")
        self.assertEqual([14, 36], added["sizes"])
        self.assertEqual(6, len(urls))
        self.assertTrue(any(url.endswith("/regular/bell.svg") for url in urls))
        self.assertTrue(any(url.endswith("/duotone/bell-duotone.svg") for url in urls))
        self.assertEqual(hashlib.sha256(download(urls[0])).hexdigest(), added["sha256"]["thin"])
        self.assertFalse(any(icon["name"] == "bell" for icon in self.manifest["icons"]))

    def test_failed_registration_never_mutates_manifest(self):
        before = copy.deepcopy(self.manifest)
        def missing(url):
            raise urllib.error.HTTPError(url, 404, "Not found", {}, None)
        with self.assertRaises(urllib.error.HTTPError):
            ui.register(self.manifest, ["missing-icon"], [14], missing)
        self.assertEqual(before, self.manifest)

    def test_every_weight_emits_only_selected_layers_and_typed_metadata(self):
        for weight in ui.WEIGHTS:
            manifest = copy.deepcopy(self.manifest)
            manifest["icons"] = [{"name": "alarm", "sizes": [1]}]
            manifest["weight"] = weight
            payloads = {("alarm", 1): (bytes([255]), bytes([255]) if weight == "duotone" else None,
                                        51 if weight == "duotone" else 0)}
            header, source = ui.render_assets(manifest, payloads)
            self.assertIn(f'#define HG_UI_ICON_WEIGHT "{weight}"'.encode(), header)
            self.assertIn(b"HG_UI_ICON_ALARM", header)
            self.assertIn(b"LV_IMG_CF_ALPHA_8BIT", source)
            self.assertEqual(weight == "duotone", b"hg_ui_alarm_1_background_map" in source)
            self.assertIn(b"Phosphor header/source weights differ", source)
            self.assertEqual((header, source), ui.render_assets(manifest, payloads))

    def test_pinned_fetch_rejects_wrong_hash_without_writing(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "alarm.svg"
            with patch("generate_icons.urllib.request.urlopen") as request:
                request.return_value.__enter__.return_value.read.return_value = b"wrong source"
                with self.assertRaises(SystemExit):
                    fetch("https://example.invalid/alarm.svg", "0" * 64, target, False)
            self.assertFalse(target.exists())


if __name__ == "__main__":
    unittest.main()
