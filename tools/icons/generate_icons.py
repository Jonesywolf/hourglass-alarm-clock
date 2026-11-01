#!/usr/bin/env python3
"""Regenerate the project's full-color LVGL images from tools/icons/icons.json.

SVGs are fetched into the gitignored cache directory when missing and checked against the
SHA-256 pinned in the manifest, then rasterized with resvg (installed into the cache via npm).
Each icon/size produces two images:
  hg_icon_<name>_<size>       LV_IMG_CF_TRUE_COLOR, RGB565 (LV_COLOR_16_SWAP byte order)
                              pre-blended onto the manifest "background" (the theme background)
  hg_icon_<name>_<size>_mono  LV_IMG_CF_ALPHA_8BIT shaded by luminance, recolored by the theme
Standard library only; requires Node.js and npm.
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
DEFAULT_MANIFEST = HERE / "icons.json"

INCLUDE = (
    "#ifdef __has_include\n"
    '    #if __has_include("lvgl.h")\n'
    "        #ifndef LV_LVGL_H_INCLUDE_SIMPLE\n"
    "            #define LV_LVGL_H_INCLUDE_SIMPLE\n"
    "        #endif\n"
    "    #endif\n"
    "#endif\n"
    "\n"
    "#ifdef LV_LVGL_H_INCLUDE_SIMPLE\n"
    '    #include "lvgl.h"\n'
    "#else\n"
    '    #include "lvgl/lvgl.h"\n'
    "#endif\n"
)
# Keeps darker parts of an icon visible when it is drawn as a single theme tint.
MONO_FLOOR = 0.3


def die(message):
    print(f"error: {message}", file=sys.stderr)
    sys.exit(1)


def sha256_of(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fetch(url, sha256, dest, force):
    if dest.exists() and not force:
        if sha256_of(dest) == sha256:
            return
        print(f"  cached {dest.name} does not match the pinned hash; downloading again")
    dest.parent.mkdir(parents=True, exist_ok=True)
    print(f"  downloading {dest.name}")
    request = urllib.request.Request(url, headers={"User-Agent": "hourglass-icon-tool"})
    try:
        with urllib.request.urlopen(request, timeout=60) as response:
            data = response.read()
    except OSError as error:
        die(f"could not download {url}: {error}")
    actual = hashlib.sha256(data).hexdigest()
    if actual != sha256:
        die(f"{dest.name}: sha256 {actual} does not match manifest {sha256}")
    dest.write_bytes(data)


def run(command, **kwargs):
    result = subprocess.run(command, capture_output=True, text=True, **kwargs)
    if result.returncode != 0:
        die(f"{' '.join(str(c) for c in command)}\n{result.stdout}{result.stderr}")
    return result


def ensure_renderer(cache, spec, force):
    node = shutil.which("node")
    npm = shutil.which("npm")
    if not node or not npm:
        die("Node.js and npm are required to run resvg (install from nodejs.org)")
    prefix = cache / "node"
    package = prefix / "node_modules" / Path(*spec["npm_package"].split("/"))
    marker = prefix / "resvg.version"
    installed = marker.exists() and marker.read_text().strip() == spec["version"]
    if force or not package.exists() or not installed:
        print(f"  installing {spec['npm_package']}@{spec['version']}")
        prefix.mkdir(parents=True, exist_ok=True)
        run([npm, "install", "--prefix", str(prefix), f"{spec['npm_package']}@{spec['version']}",
             "--no-audit", "--no-fund", "--loglevel", "error"])
        marker.write_text(spec["version"])
    return node, prefix


def crop_svg(text, viewbox, name):
    cropped, count = re.subn(r'viewBox="[^"]*"', f'viewBox="{viewbox}"', text, count=1)
    if count != 1:
        die(f"{name}: SVG has no viewBox")
    return pose_svg(cropped)


def _midpoint(values):
    frames = [[float(n) for n in re.split(r"[\s,]+", frame.strip()) if n]
              for frame in values.split(";") if frame.strip()]
    position = (len(frames) - 1) * 0.5
    low = int(position)
    high = min(low + 1, len(frames) - 1)
    t = position - low
    return [a + (b - a) * t for a, b in zip(frames[low], frames[high])]


def pose_svg(text):
    """Freezes each animateTransform at the middle of its cycle.

    Static renderers draw the un-animated base pose, where Meteocons tuck precipitation behind
    the cloud. Mid-cycle shows drops and flakes below it, as in the animated icon."""
    def replace(match):
        tag, attrs, animation = match.group(1), match.group(2), match.group(3)
        kind = re.search(r'type="(\w+)"', animation)
        values = re.search(r'values="([^"]*)"', animation)
        if not kind or not values:
            return match.group(0)
        numbers = " ".join(f"{v:g}" for v in _midpoint(values.group(1)))
        attrs = re.sub(r'\stransform="[^"]*"', "", attrs)
        return f'<{tag}{attrs} transform="{kind.group(1)}({numbers})">'
    return re.sub(r"<(\w+)([^<>]*[^/<>])><animateTransform([^<>]*)/>", replace, text)


def to_rgb565(r, g, b):
    return ((r * 31 + 127) // 255) << 11 | ((g * 63 + 127) // 255) << 5 | ((b * 31 + 127) // 255)


def convert(rgba, background):
    """Returns (byte-swapped opaque RGB565, alpha8) payloads for straight-alpha RGBA.

    Color images are pre-blended onto the theme background: LVGL then draws them as plain
    TRUE_COLOR copies instead of splitting color/alpha and mask-blending every frame."""
    swapped, mono = bytearray(), bytearray()
    for i in range(0, len(rgba), 4):
        r, g, b, a = rgba[i], rgba[i + 1], rgba[i + 2], rgba[i + 3]
        blended = [(c * a + bg * (255 - a) + 127) // 255 for c, bg in zip((r, g, b), background)]
        color = to_rgb565(*blended)
        swapped += bytes((color >> 8, color & 0xFF))
        luminance = (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255.0
        mono.append(round(a * (MONO_FLOOR + (1.0 - MONO_FLOOR) * luminance)))
    return swapped, mono


def unpremultiply(rgba):
    out = bytearray(rgba)
    for i in range(0, len(out), 4):
        a = out[i + 3]
        if 0 < a < 255:
            for c in range(3):
                out[i + c] = min(255, (out[i + c] * 255 + a // 2) // a)
    return out


def byte_rows(data, indent="    ", per_row=32):
    # Decimal keeps committed sources much smaller than hex for mostly-transparent images.
    rows = []
    for start in range(0, len(data), per_row):
        chunk = data[start:start + per_row]
        rows.append(indent + ",".join(str(byte) for byte in chunk) + ",")
    return "\n".join(rows)


def image_block(symbol, size, cf, data_name):
    return (f"const lv_img_dsc_t {symbol} = {{\n"
            f"    .header = {{.cf = {cf}, .always_zero = 0, .reserved = 0, .w = {size}, .h = {size}}},\n"
            f"    .data_size = sizeof({data_name}),\n"
            f"    .data = {data_name},\n"
            "};\n")


def render_c(icon, source, renders):
    lines = [f"/* Generated by tools/icons/generate_icons.py from {icon['file']} "
             f"({source['_name']}, {source['license']}). Do not edit. */\n", INCLUDE,
             "\n#if LV_COLOR_DEPTH != 16 || !LV_COLOR_16_SWAP\n"
             "#error \"Generated icons require LV_COLOR_DEPTH 16 with LV_COLOR_16_SWAP (include/lvconf.h)\"\n"
             "#endif\n"]
    for size, (swapped, mono) in renders:
        base = f"hg_icon_{icon['name']}_{size}"
        lines.append(f"\nstatic const uint8_t {base}_map[] = {{\n{byte_rows(swapped)}\n}};\n\n")
        lines.append(image_block(base, size, "LV_IMG_CF_TRUE_COLOR", f"{base}_map"))
        lines.append(f"\nstatic const uint8_t {base}_mono_map[] = {{\n{byte_rows(mono)}\n}};\n\n")
        lines.append(image_block(f"{base}_mono", size, "LV_IMG_CF_ALPHA_8BIT", f"{base}_mono_map"))
    return "".join(lines).encode("utf-8")


def render_header(manifest):
    lines = ["#ifndef HG_UI_ICONS_H\n#define HG_UI_ICONS_H\n\n#include <lvgl.h>\n\n",
             "/* Generated by tools/icons/generate_icons.py from tools/icons/icons.json. Do not edit.\n",
             " * <name>_<size> is full color, opaque on the theme background; <name>_<size>_mono is an\n"
             " * alpha mask for img_recolor. */\n\n"]
    for icon in manifest["icons"]:
        for size in manifest["sizes"]:
            lines.append(f"LV_IMG_DECLARE(hg_icon_{icon['name']}_{size});\n")
            lines.append(f"LV_IMG_DECLARE(hg_icon_{icon['name']}_{size}_mono);\n")
    lines.append("\n#endif\n")
    return "".join(lines).encode("utf-8")


def write_if_changed(target, data):
    if target.exists() and target.read_bytes() == data:
        print(f"{target.name}: unchanged")
    else:
        target.write_bytes(data)
        print(f"{target.name}: written ({len(data)} bytes)")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--force", action="store_true", help="re-download sources and reinstall tools")
    parser.add_argument("--preview", action="store_true", help="also write PNG previews into the cache")
    args = parser.parse_args()

    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    cache = ROOT / manifest["cache_dir"]
    output_dir = ROOT / manifest["output_dir"]
    cache.mkdir(parents=True, exist_ok=True)
    output_dir.mkdir(parents=True, exist_ok=True)
    node, prefix = ensure_renderer(cache, manifest["renderer"], args.force)

    jobs = []
    for icon in manifest["icons"]:
        source = manifest["sources"].get(icon["source"])
        if source is None:
            die(f"{icon['name']}: unknown source '{icon['source']}'")
        original = cache / "svg" / icon["file"]
        fetch(source["url"].format(file=icon["file"]), icon["sha256"], original, args.force)
        cropped = cache / "cropped" / f"{icon['name']}.svg"
        cropped.parent.mkdir(parents=True, exist_ok=True)
        cropped.write_text(crop_svg(original.read_text(encoding="utf-8"), manifest["viewbox"],
                                    icon["name"]), encoding="utf-8")
        for size in manifest["sizes"]:
            job = {"svg": str(cropped), "size": size,
                   "out": str(cache / "raw" / f"{icon['name']}_{size}.rgba")}
            if args.preview:
                job["png"] = str(cache / "preview" / f"{icon['name']}_{size}.png")
            jobs.append(job)
    for folder in ("raw", "preview"):
        (cache / folder).mkdir(parents=True, exist_ok=True)
    jobs_file = cache / "jobs.json"
    jobs_file.write_text(json.dumps(jobs), encoding="utf-8")
    run([node, str(HERE / "render.mjs"), str(jobs_file)],
        env=dict(os.environ, HG_RESVG_PREFIX=str(prefix)))

    premultiplied = manifest["renderer"].get("premultiplied", True)
    background = manifest.get("background", "#000000").lstrip("#")
    if not re.fullmatch(r"[0-9a-fA-F]{6}", background):
        die(f"background must be #rrggbb, got '{manifest.get('background')}'")
    background = tuple(int(background[i:i + 2], 16) for i in (0, 2, 4))
    for icon in manifest["icons"]:
        source = dict(manifest["sources"][icon["source"]], _name=icon["source"])
        renders = []
        for size in manifest["sizes"]:
            rgba = (cache / "raw" / f"{icon['name']}_{size}.rgba").read_bytes()
            if premultiplied:
                rgba = unpremultiply(rgba)
            renders.append((size, convert(rgba, background)))
        write_if_changed(output_dir / f"{icon['name']}.c", render_c(icon, source, renders))
    write_if_changed(ROOT / manifest["header"], render_header(manifest))
    expected = {f"{icon['name']}.c" for icon in manifest["icons"]}
    for stale in output_dir.glob("*.c"):
        if stale.name not in expected:
            stale.unlink()
            print(f"{stale.name}: removed (no longer in manifest)")


if __name__ == "__main__":
    main()
