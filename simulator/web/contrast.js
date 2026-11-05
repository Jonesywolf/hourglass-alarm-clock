// WCAG 2.2 AA contrast audit over the rendered 320x240 LVGL framebuffer.
// Text: 4.5:1 (SC 1.4.3), large text >= 24 px: 3:1. Meaningful graphics: 3:1 (SC 1.4.11).

export const AA_TEXT = 4.5;
export const AA_LARGE = 3;
export const AA_GRAPHIC = 3;
const WIDTH = 320;

function linear(channel) {
    const c = channel / 255;
    return c <= 0.04045 ? c / 12.92 : ((c + 0.055) / 1.055) ** 2.4;
}
export function luminance([r, g, b]) {
    return 0.2126 * linear(r) + 0.7152 * linear(g) + 0.0722 * linear(b);
}
export function contrast(a, b) {
    const [hi, lo] = [luminance(a), luminance(b)].sort((x, y) => y - x);
    return (hi + 0.05) / (lo + 0.05);
}
export const unpack = (value) => [(value >>> 16) & 255, (value >>> 8) & 255, value & 255];
export const hex = (rgb) => `#${rgb.map((c) => c.toString(16).padStart(2, "0")).join("")}`;

// Matches what the RGB565 panel can show, so declared colours are judged as rendered.
function rgb565([r, g, b]) {
    const r5 = r >> 3, g6 = g >> 2, b5 = b >> 3;
    return [(r5 << 3) | (r5 >> 2), (g6 << 2) | (g6 >> 4), (b5 << 3) | (b5 >> 2)];
}
function blend(fg, bg, opa) {
    return fg.map((c, i) => Math.round((c * opa + bg[i] * (255 - opa)) / 255));
}
export function required(target) {
    if (target.kind === "text") return target.px >= 24 ? AA_LARGE : AA_TEXT;
    return AA_GRAPHIC;
}

export function measure(rgba, target) {
    const counts = new Map();
    const pixels = [];
    for (let y = target.y; y < target.y + target.h; ++y) {
        for (let x = target.x; x < target.x + target.w; ++x) {
            const i = (y * WIDTH + x) * 4;
            const key = (rgba[i] << 16) | (rgba[i + 1] << 8) | rgba[i + 2];
            counts.set(key, (counts.get(key) || 0) + 1);
            pixels.push(key);
        }
    }
    let bgKey = 0, best = -1;
    for (const [key, count] of counts) if (count > best) [bgKey, best] = [key, count];
    const bg = unpack(bgKey);
    let ink = bg, inkRatio = 1;
    for (const key of counts.keys()) {
        const ratio = contrast(unpack(key), bg);
        if (ratio > inkRatio) [ink, inkRatio] = [unpack(key), ratio];
    }
    return { bg, ink, measured: inkRatio, inkPixels: pixels.filter((k) => k !== bgKey).length };
}

// Declared colour (blended by effective opacity over the measured background) is authoritative
// when known; anti-aliased small glyphs may never reach full coverage. Measured ink is the
// fallback for full-colour artwork and a cross-check for unexpected occlusion or dimming.
export function evaluate(rgba, target) {
    const { bg, ink, measured, inkPixels } = measure(rgba, target);
    const need = required(target);
    let fg = ink, ratio = measured, mismatch = false;
    if (target.color !== undefined) {
        fg = rgb565(blend(unpack(target.color), bg, target.opa));
        ratio = contrast(fg, bg);
        // Warn only when rendered pixels disagree with a passing declared verdict (e.g. occlusion).
        mismatch = inkPixels > 0 && ratio >= need && measured < need;
        // Glyph anti-aliasing can keep small text below its declared colour, but artwork must
        // reach the threshold in rendered pixels (alpha-shaded masks, faded layers).
        if (target.kind !== "text") ratio = Math.min(ratio, measured);
    }
    if (!inkPixels) ratio = 1;
    return {
        ...target, fg: hex(fg), bg: hex(bg), ink: hex(ink),
        ratio: Math.round(ratio * 100) / 100, measured: Math.round(measured * 100) / 100,
        required: need, pass: ratio >= need, mismatch,
    };
}

// A clipped target (e.g. mid-slide at the screen edge) whose visible slice draws no ink shows
// nothing in this frame. Unclipped targets without ink still fail (e.g. fg equal to bg).
export function audit(rgba, targets) {
    return targets.filter((t) => t.w > 0 && t.h > 0)
        .filter((t) => !t.clipped || measure(rgba, t).inkPixels > 0)
        .map((t) => evaluate(rgba, t));
}

export function tokenChecks(tokens, mode) {
    const bg = unpack(tokens.bg);
    const rows = [];
    const add = (name, value, need) => {
        const ratio = contrast(rgb565(unpack(value)), bg);
        rows.push({ mode, name, color: hex(unpack(value)), ratio: Math.round(ratio * 100) / 100,
            required: need, pass: ratio >= need });
    };
    for (const name of ["fg", "alarm", "muted"]) add(name, tokens[name], AA_TEXT);
    add("rain", tokens.rain, AA_TEXT);
    tokens.aqi.forEach((value, i) => add(`aqi${i}`, value, AA_TEXT));
    return rows;
}

export function drawOverlay(canvas, results, scale) {
    const ctx = canvas.getContext("2d");
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.font = "11px system-ui";
    for (const r of results) {
        if (r.pass && !r.mismatch) continue;
        ctx.strokeStyle = r.pass ? "#ffcc00" : "#ff2d55";
        ctx.lineWidth = 2;
        ctx.strokeRect(r.x * scale + 1, r.y * scale + 1, r.w * scale - 2, r.h * scale - 2);
        const label = r.pass ? `measured ${r.measured}:1` : `${r.ratio}:1 < ${r.required}`;
        ctx.fillStyle = "rgba(0,0,0,0.75)";
        ctx.fillRect(r.x * scale, r.y * scale, ctx.measureText(label).width + 6, 14);
        ctx.fillStyle = "#fff";
        ctx.fillText(label, r.x * scale + 3, r.y * scale + 11);
    }
}
