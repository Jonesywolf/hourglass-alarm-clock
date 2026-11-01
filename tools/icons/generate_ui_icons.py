#!/usr/bin/env python3
"""Generate named Phosphor LVGL UI masks; only the selected build-time weight is emitted."""
import argparse
import copy
import hashlib
import json
import os
import re
import sys
import urllib.request
import xml.etree.ElementTree as ET
from pathlib import Path

from generate_icons import (
    INCLUDE, ROOT, byte_rows, die, ensure_renderer, fetch, image_block, run, write_if_changed,
)

DEFAULT_MANIFEST = Path(__file__).with_name("ui_icons.json")
WEIGHTS = ("thin", "light", "regular", "bold", "fill", "duotone")
SVG_NS = "http://www.w3.org/2000/svg"
ET.register_namespace("", SVG_NS)
LICENSE = """MIT License

Copyright (c) 2023 Phosphor Icons

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
"""


def validate(manifest):
    if manifest.get("kind") != "phosphor" or manifest.get("weight") not in WEIGHTS:
        raise ValueError(f"expected kind 'phosphor' and weight in {', '.join(WEIGHTS)}")
    if not re.fullmatch(r"[0-9a-f]{40}", manifest["source"]["revision"]):
        raise ValueError("source revision must be an immutable 40-character commit hash")
    seen = set()
    for icon in manifest["icons"]:
        name = icon["name"]
        if not re.fullmatch(r"[a-z][a-z0-9]*(?:-[a-z0-9]+)*", name) or name in seen:
            raise ValueError(f"invalid or duplicate icon name: {name}")
        seen.add(name)
        sizes = icon["sizes"]
        if not sizes or len(set(sizes)) != len(sizes) or any(
            type(size) is not int or not 1 <= size <= 256 for size in sizes
        ):
            raise ValueError(f"{name}: sizes must be unique integers in 1..256")
        if set(icon["sha256"]) != set(WEIGHTS) or any(
            not re.fullmatch(r"[0-9a-f]{64}", value) for value in icon["sha256"].values()
        ):
            raise ValueError(f"{name}: pin a SHA-256 for every weight")


def source_url(manifest, name, weight):
    filename = f"{name}.svg" if weight == "regular" else f"{name}-{weight}.svg"
    return manifest["source"]["url"].format(
        revision=manifest["source"]["revision"], weight=weight, file=filename,
    )


def register(manifest, names, sizes, download):
    """Collect all weights before mutating the manifest, so a failed fetch is atomic."""
    additions = []
    existing = {icon["name"]: icon for icon in manifest["icons"]}
    for name in names:
        if not re.fullmatch(r"[a-z][a-z0-9]*(?:-[a-z0-9]+)*", name):
            raise ValueError(f"invalid icon name: {name}")
        chosen_sizes = sizes or existing.get(name, {}).get("sizes")
        if not chosen_sizes:
            raise ValueError(f"{name}: supply --sizes when adding a new icon")
        hashes = {}
        for weight in WEIGHTS:
            data = download(source_url(manifest, name, weight))
            split_svg(data.decode("utf-8"), weight)
            hashes[weight] = hashlib.sha256(data).hexdigest()
        additions.append({"name": name, "sizes": chosen_sizes, "sha256": hashes})
    updated = copy.deepcopy(manifest)
    by_name = {icon["name"]: icon for icon in updated["icons"]}
    by_name.update({icon["name"]: icon for icon in additions})
    updated["icons"] = sorted(by_name.values(), key=lambda icon: icon["name"])
    validate(updated)
    return updated


def split_svg(text, weight):
    """Return full-coverage foreground/background SVGs and the background opacity."""
    root = ET.fromstring(text)
    if root.tag != f"{{{SVG_NS}}}svg" or root.get("viewBox") != "0 0 256 256":
        raise ValueError("expected an official Phosphor SVG with viewBox 0 0 256 256")
    allowed = {"svg", "g", "path", "circle", "ellipse", "rect", "polygon", "polyline", "line"}
    for element in root.iter():
        if element.tag.rsplit("}", 1)[-1] not in allowed:
            raise ValueError(f"unsupported SVG element: {element.tag}")
        if "style" in element.attrib or any(
            attr in element.attrib for attr in ("filter", "mask", "clip-path", "fill-opacity", "stroke-opacity")
        ):
            raise ValueError("unsupported SVG style/filter/mask/opacity")
        for attr in ("fill", "stroke"):
            if element.get(attr) == "currentColor":
                element.set(attr, "#ffffff")
    if root.get("opacity") is not None:
        raise ValueError("unsupported root opacity")

    foreground = copy.deepcopy(root)
    background = copy.deepcopy(root)
    foreground[:] = []
    background[:] = []

    def select(element, inherited, want_background):
        opacity = float(element.get("opacity", inherited))
        if opacity not in (0.2, 1.0):
            raise ValueError(f"unsupported Phosphor opacity: {opacity}")
        if inherited != 1.0 and element.get("opacity") is not None:
            raise ValueError("nested SVG opacity is not supported")
        clone = copy.deepcopy(element)
        clone.attrib.pop("opacity", None)
        if len(element):
            clone[:] = []
            for child in element:
                selected = select(child, opacity, want_background)
                if selected is not None:
                    clone.append(selected)
            return clone if len(clone) else None
        return clone if (opacity < 1.0) == want_background else None

    for child in root:
        for target, want_background in ((foreground, False), (background, True)):
            selected = select(child, 1.0, want_background)
            if selected is not None:
                target.append(selected)
    if not len(foreground):
        raise ValueError("icon has no foreground")
    # Some official Duotone icons (e.g. wifi-none) intentionally have no shaded layer.
    if weight != "duotone" and len(background):
        raise ValueError("monochrome icon unexpectedly contains an opacity layer")
    return (
        ET.tostring(foreground, encoding="unicode"),
        ET.tostring(background, encoding="unicode") if len(background) else None,
        51 if len(background) else 0,
    )


def coverage(rgba, size):
    if len(rgba) != size * size * 4:
        raise ValueError(f"invalid RGBA payload length for {size}px")
    alpha = rgba[3::4]
    if not any(alpha):
        raise ValueError("rendered icon layer is empty")
    return alpha


def symbol(name, size, layer):
    return f"hg_ui_{name.replace('-', '_')}_{size}_{layer}"


def render_assets(manifest, payloads):
    header = [
        "#ifndef HG_UI_UI_ICON_ASSETS_H\n#define HG_UI_UI_ICON_ASSETS_H\n\n#include <lvgl.h>\n\n",
        "/* Generated by tools/icons/generate_ui_icons.py. Do not edit. */\n",
        f'#define HG_UI_ICON_WEIGHT "{manifest["weight"]}"\n',
        f'#define HG_UI_ICON_WEIGHT_ID {WEIGHTS.index(manifest["weight"])}\n',
        f'#define HG_UI_ICON_DUOTONE {int(manifest["weight"] == "duotone")}\n\n',
        "typedef enum {\n",
    ]
    for icon in manifest["icons"]:
        header.append(f"    HG_UI_ICON_{icon['name'].replace('-', '_').upper()},\n")
    header += [
        "    HG_UI_ICON_COUNT\n} hg_ui_icon_id_t;\n\n",
        "typedef struct {\n    hg_ui_icon_id_t id;\n    uint16_t size;\n",
        "    const lv_img_dsc_t *foreground;\n    const lv_img_dsc_t *background;\n",
        "    lv_opa_t background_opacity;\n} hg_ui_icon_asset_t;\n\n",
        "#ifdef __cplusplus\nextern \"C\" {\n#endif\n",
        "extern const hg_ui_icon_asset_t hg_ui_icon_assets[];\n",
        "extern const unsigned hg_ui_icon_asset_count;\n",
        "#ifdef __cplusplus\n}\n#endif\n\n#endif\n",
    ]
    source = [
        f"/* Generated Phosphor {manifest['weight']} assets. Do not edit.\n{LICENSE}*/\n",
        INCLUDE, '\n#include "hg/ui/ui_icon_assets.h"\n',
        f'\n#if HG_UI_ICON_WEIGHT_ID != {WEIGHTS.index(manifest["weight"])}\n'
        '#error "Phosphor header/source weights differ; regenerate the full UI icon set"\n#endif\n',
    ]
    rows = []
    for icon in manifest["icons"]:
        name = icon["name"]
        for size in icon["sizes"]:
            fg, bg, opacity = payloads[(name, size)]
            for layer, data in (("foreground", fg), ("background", bg)):
                if data is None:
                    continue
                base = symbol(name, size, layer)
                source += [
                    f"\nstatic const uint8_t {base}_map[] = {{\n{byte_rows(data)}\n}};\n",
                    "\nstatic " + image_block(base, size, "LV_IMG_CF_ALPHA_8BIT", f"{base}_map"),
                ]
            foreground = "&" + symbol(name, size, "foreground")
            background = "&" + symbol(name, size, "background") if bg is not None else "NULL"
            rows.append(
                f"    {{HG_UI_ICON_{name.replace('-', '_').upper()}, {size}, "
                f"{foreground}, {background}, {opacity}}},\n"
            )
    source += ["\nconst hg_ui_icon_asset_t hg_ui_icon_assets[] = {\n", *rows, "};\n",
               f"const unsigned hg_ui_icon_asset_count = {len(rows)};\n"]
    return "".join(header).encode(), "".join(source).encode()


def download(url):
    print(f"  checking {url.rsplit('/', 1)[-1]}")
    request = urllib.request.Request(url, headers={"User-Agent": "hourglass-icon-tool"})
    with urllib.request.urlopen(request, timeout=60) as response:
        return response.read()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--weight", choices=WEIGHTS, help="override manifest weight for this generation")
    parser.add_argument("--add", nargs="+", metavar="NAME", help="register official names and pin all weights")
    parser.add_argument("--sizes", nargs="+", type=int, help="sizes for registered icons")
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--preview", action="store_true")
    parser.add_argument("--force", action="store_true")
    parser.add_argument("--output-dir", type=Path, help="isolated scratch output (header is written there too)")
    args = parser.parse_args(argv)
    try:
        manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
        validate(manifest)
        if args.add:
            if args.weight or args.output_dir:
                raise ValueError("--add cannot be combined with --weight or --output-dir")
            updated = register(manifest, args.add, args.sizes, download)
            write_if_changed(args.manifest, (json.dumps(updated, indent=2) + "\n").encode())
            return
        if args.sizes:
            raise ValueError("--sizes requires --add")
        if args.weight:
            manifest["weight"] = args.weight
        if args.list:
            print(f"weight: {manifest['weight']} (build-time; available: {', '.join(WEIGHTS)})")
            for icon in manifest["icons"]:
                print(f"{icon['name']:20} {', '.join(str(size) for size in icon['sizes'])} px")
            return
        if not manifest["icons"]:
            raise ValueError("no icons registered; use --add NAME --sizes SIZE ...")
        cache = ROOT / manifest["cache_dir"]
        # Share only the resvg installation, not weather conversion or generated output.
        node, prefix = ensure_renderer(cache.parent, manifest["renderer"], args.force)
        weight = manifest["weight"]
        jobs, opacity_by_name = [], {}
        for icon in manifest["icons"]:
            name = icon["name"]
            original = cache / "svg" / weight / f"{name}.svg"
            fetch(source_url(manifest, name, weight), icon["sha256"][weight], original, args.force)
            fg, bg, opacity_by_name[name] = split_svg(original.read_text(encoding="utf-8"), weight)
            for layer, text in (("foreground", fg), ("background", bg)):
                if text is None:
                    continue
                svg = cache / "layers" / weight / f"{name}-{layer}.svg"
                svg.parent.mkdir(parents=True, exist_ok=True)
                svg.write_text(text, encoding="utf-8")
                for size in icon["sizes"]:
                    base = f"{name}-{size}-{layer}"
                    raw = cache / "raw" / weight / f"{base}.rgba"
                    raw.parent.mkdir(parents=True, exist_ok=True)
                    job = {"svg": str(svg), "size": size, "out": str(raw)}
                    if args.preview:
                        png = cache / "preview" / weight / f"{base}.png"
                        png.parent.mkdir(parents=True, exist_ok=True)
                        job["png"] = str(png)
                    jobs.append(job)
            if args.preview:
                # Preview the original combined SVG, not two disconnected layers.
                combined = cache / "preview_svg" / weight / f"{name}.svg"
                combined.parent.mkdir(parents=True, exist_ok=True)
                combined.write_text(original.read_text(encoding="utf-8").replace("currentColor", "#ffffff"),
                                    encoding="utf-8")
                for size in icon["sizes"]:
                    png = cache / "preview" / weight / f"{name}-{size}.png"
                    jobs.append({"svg": str(combined), "size": size,
                                 "out": str(cache / "raw" / weight / f"{name}-{size}-preview.rgba"),
                                 "png": str(png)})
        jobs_file = cache / "jobs.json"
        jobs_file.write_text(json.dumps(jobs), encoding="utf-8")
        run([node, str(Path(__file__).with_name("render.mjs")), str(jobs_file)],
            env=dict(os.environ, HG_RESVG_PREFIX=str(prefix)))
        payloads = {}
        for icon in manifest["icons"]:
            for size in icon["sizes"]:
                def read_layer(layer):
                    return coverage((cache / "raw" / weight /
                                     f"{icon['name']}-{size}-{layer}.rgba").read_bytes(), size)
                payloads[(icon["name"], size)] = (
                    read_layer("foreground"),
                    read_layer("background") if opacity_by_name[icon["name"]] else None,
                    opacity_by_name[icon["name"]],
                )
        header, source = render_assets(manifest, payloads)
        output = args.output_dir or ROOT / manifest["output_dir"]
        output.mkdir(parents=True, exist_ok=True)
        write_if_changed(output / "phosphor.c", source)
        write_if_changed(args.output_dir / "ui_icon_assets.h" if args.output_dir else ROOT / manifest["header"],
                         header)
        print(f"{weight}: {sum(len(fg) + (len(bg) if bg else 0) for fg, bg, _ in payloads.values())}"
              " mask bytes; only this weight is emitted")
    except (ValueError, KeyError, TypeError, ET.ParseError, OSError) as error:
        die(str(error))


if __name__ == "__main__":
    main()
