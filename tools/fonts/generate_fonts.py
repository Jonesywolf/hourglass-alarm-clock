#!/usr/bin/env python3
"""Regenerate the project's LVGL fonts from tools/fonts/fonts.json.

Font files are fetched into the gitignored cache directory when missing and checked
against the SHA-256 pinned in the manifest. Only the generated .c files are tracked.
Requires Node.js (for lv_font_conv); fonttools is installed into the cache on demand
for variable-font instances. Standard library only otherwise.
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
DEFAULT_MANIFEST = Path(__file__).resolve().with_name("fonts.json")

OLD_INCLUDE = '#ifdef LV_LVGL_H_INCLUDE_SIMPLE\n#include "lvgl.h"\n#else\n#include "lvgl/lvgl.h"\n#endif\n'
NEW_INCLUDE = (
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


def die(message):
    print(f"error: {message}", file=sys.stderr)
    sys.exit(1)


def sha256_of(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def fetch(url, sha256, dest, force):
    if dest.exists() and not force:
        if sha256_of(dest) == sha256:
            return
        print(f"  cached {dest.name} does not match the pinned hash; downloading again")
    dest.parent.mkdir(parents=True, exist_ok=True)
    print(f"  downloading {dest.name}")
    partial = dest.with_name(dest.name + ".part")
    request = urllib.request.Request(url, headers={"User-Agent": "hourglass-font-tool"})
    try:
        with urllib.request.urlopen(request, timeout=60) as response, open(partial, "wb") as out:
            shutil.copyfileobj(response, out)
    except OSError as error:
        partial.unlink(missing_ok=True)
        die(f"could not download {url}: {error}")
    actual = sha256_of(partial)
    if actual != sha256:
        partial.unlink(missing_ok=True)
        die(f"{dest.name}: sha256 {actual} does not match manifest {sha256}")
    partial.replace(dest)


def run(command, **kwargs):
    result = subprocess.run(command, capture_output=True, text=True, **kwargs)
    if result.returncode != 0:
        die(f"{' '.join(str(c) for c in command)}\n{result.stdout}{result.stderr}")
    return result


def ensure_converter(cache, spec, force):
    node = shutil.which("node")
    npm = shutil.which("npm")
    if not node or not npm:
        die("Node.js and npm are required to run lv_font_conv (install from nodejs.org)")
    prefix = cache / "node"
    script = prefix / "node_modules" / spec["npm_package"] / "lv_font_conv.js"
    marker = prefix / f"{spec['npm_package']}.version"
    installed = marker.exists() and marker.read_text().strip() == spec["version"]
    if force or not script.exists() or not installed:
        print(f"  installing {spec['npm_package']}@{spec['version']}")
        prefix.mkdir(parents=True, exist_ok=True)
        run([npm, "install", "--prefix", str(prefix), f"{spec['npm_package']}@{spec['version']}",
             "--no-audit", "--no-fund", "--loglevel", "error"])
        marker.write_text(spec["version"])
    if not script.exists():
        die(f"{script} missing after installing {spec['npm_package']}")
    return node, script


def ensure_fonttools(cache, spec, force):
    lib = cache / "pylib"
    marker = lib / "fonttools.version"
    if force or not marker.exists() or marker.read_text().strip() != spec["version"]:
        print(f"  installing fonttools=={spec['version']}")
        lib.mkdir(parents=True, exist_ok=True)
        run([sys.executable, "-m", "pip", "install", "--quiet", "--target", str(lib),
             f"{spec['pip_package']}=={spec['version']}"])
        marker.write_text(spec["version"])
    return lib


def load_icon_names(path):
    names = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        parts = line.split()
        if len(parts) == 2:
            names[parts[0]] = int(parts[1], 16)
    return names


def parse_codepoint(value):
    return int(value, 0) if isinstance(value, str) else int(value)


def glyph_ranges(font, icon_names):
    spans = []
    for name in font.get("icons", []):
        if icon_names is None or name not in icon_names:
            die(f"{font['name']}: unknown icon '{name}' for source '{font['source']}'")
        spans.append((icon_names[name], icon_names[name]))
    for value in font.get("codepoints", []):
        cp = parse_codepoint(value)
        spans.append((cp, cp))
    for text in font.get("ranges", []):
        low, _, high = str(text).partition("-")
        spans.append((parse_codepoint(low), parse_codepoint(high or low)))
    for char in font.get("text", ""):
        spans.append((ord(char), ord(char)))
    if not spans:
        die(f"{font['name']}: no glyphs selected (use icons, codepoints, ranges or text)")
    merged = []
    for low, high in sorted(spans):
        if merged and low <= merged[-1][1] + 1:
            merged[-1] = (merged[-1][0], max(merged[-1][1], high))
        else:
            merged.append((low, high))
    return ",".join(str(lo) if lo == hi else f"{lo}-{hi}" for lo, hi in merged)


def make_instance(cache, source_file, font, tool_spec, force):
    variations = font["variations"]
    axes = [f"{axis}={value}" for axis, value in sorted(variations.items())]
    suffix = "-".join(f"{axis}{value}" for axis, value in sorted(variations.items()))
    target = cache / f"{source_file.stem}-{suffix}.ttf"
    if target.exists() and not force and target.stat().st_mtime >= source_file.stat().st_mtime:
        return target
    lib = ensure_fonttools(cache, tool_spec, force)
    env = dict(os.environ, PYTHONPATH=str(lib))
    run([sys.executable, "-m", "fontTools.varLib.instancer", str(source_file), *axes,
         "-o", str(target), "--quiet"], env=env)
    return target


def convert(font, font_file, cache, node, script, defaults):
    out_name = f"{font['name']}.c"
    (cache / out_name).unlink(missing_ok=True)
    command = [node, str(script), "--bpp", str(font.get("bpp", defaults["bpp"])),
               "--size", str(font["size"])]
    if not font.get("compress", defaults.get("compress", False)):
        command.append("--no-compress")
    command += ["--font", font_file.name, "--range", font["_range"], "--format", "lvgl",
                "-o", out_name]
    run(command, cwd=cache)
    text = (cache / out_name).read_text(encoding="utf-8").replace("\r\n", "\n")
    if OLD_INCLUDE not in text:
        die(f"{out_name}: unexpected lv_font_conv include block; update OLD_INCLUDE in this tool")
    return text.replace(OLD_INCLUDE, NEW_INCLUDE).encode("utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("fonts", nargs="*", help="font names to generate (default: all, or $HG_FONTS)")
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--list", action="store_true", help="list fonts in the manifest and exit")
    parser.add_argument("--force", action="store_true", help="re-download sources and reinstall tools")
    parser.add_argument("--output-dir", type=Path, help="write here instead of the manifest output_dir")
    args = parser.parse_args()

    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    fonts = manifest["fonts"]
    by_name = {font["name"]: font for font in fonts}
    if args.list:
        for font in fonts:
            print(f"{font['name']:16} {font['source']:24} {font['size']:>3}px")
        return

    wanted = args.fonts or re.split(r"[,\s]+", os.environ.get("HG_FONTS", "").strip())
    wanted = [name for name in wanted if name]
    for name in wanted:
        if name not in by_name:
            die(f"unknown font '{name}'; available: {', '.join(by_name)}")
    selected = [by_name[name] for name in wanted] if wanted else fonts

    cache = ROOT / manifest["cache_dir"]
    output_dir = args.output_dir or ROOT / manifest["output_dir"]
    output_dir.mkdir(parents=True, exist_ok=True)
    cache.mkdir(parents=True, exist_ok=True)

    node, script = ensure_converter(cache, manifest["converter"], args.force)
    sources_ready = {}
    for font in selected:
        source_key = font["source"]
        if source_key not in manifest["sources"]:
            die(f"{font['name']}: unknown source '{source_key}'")
        source = manifest["sources"][source_key]
        if source_key not in sources_ready:
            fetch(source["url"], source["sha256"], cache / source["file"], args.force)
            names = None
            if "names" in source:
                fetch(source["names"]["url"], source["names"]["sha256"],
                      cache / source["names"]["file"], args.force)
                names = load_icon_names(cache / source["names"]["file"])
            sources_ready[source_key] = names
        source_file = cache / source["file"]
        font_file = source_file
        if "variations" in font:
            font_file = make_instance(cache, source_file, font, manifest["variation_tool"], args.force)
        font = dict(font, _range=glyph_ranges(font, sources_ready[source_key]))
        data = convert(font, font_file, cache, node, script, manifest["defaults"])
        target = output_dir / f"{font['name']}.c"
        if target.exists() and target.read_bytes() == data:
            print(f"{target.name}: unchanged")
        else:
            target.write_bytes(data)
            print(f"{target.name}: written ({len(data)} bytes)")


if __name__ == "__main__":
    main()
