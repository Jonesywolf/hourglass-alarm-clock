// Rasterizes SVGs for generate_icons.py. Usage: node render.mjs <jobs.json>
// Each job: {"svg": path, "size": px, "out": raw RGBA path, "png": optional preview path}.
import { readFileSync, writeFileSync } from "node:fs";
import { createRequire } from "node:module";

const require = createRequire(process.env.HG_RESVG_PREFIX + "/node_modules/");
const { Resvg } = require("@resvg/resvg-js");

for (const job of JSON.parse(readFileSync(process.argv[2], "utf8"))) {
    const resvg = new Resvg(readFileSync(job.svg, "utf8"), {
        fitTo: { mode: "width", value: job.size },
        font: { loadSystemFonts: false },
    });
    const image = resvg.render();
    if (image.width !== job.size || image.height !== job.size) {
        throw new Error(`${job.svg}: rendered ${image.width}x${image.height}, expected ${job.size}`);
    }
    writeFileSync(job.out, image.pixels);
    if (job.png) {
        writeFileSync(job.png, image.asPng());
    }
}
