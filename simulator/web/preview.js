"use strict";

const canvas = document.getElementById("display");
const context = canvas.getContext("2d", { willReadFrequently: true });
const framebuffer = new Uint8ClampedArray(320 * 240 * 4);
let moduleInstance;
let backlight = 100;
let playing = false;
let loading = false;
let lastRealTime = 0;
let instanceGeneration = 0;

function showError(error) {
    const message = error instanceof Error ? error.message : String(error);
    document.getElementById("errors").textContent += `${message}\n`;
    console.error(message);
}
function number(value, min, max, name, integer = true) {
    if (!Number.isFinite(value) || value < min || value > max || (integer && !Number.isInteger(value))) {
        throw new Error(`${name} must be ${integer ? "an integer" : "a number"} from ${min} to ${max}`);
    }
    return value;
}
function object(value, name) {
    if (!value || typeof value !== "object" || Array.isArray(value)) {
        throw new Error(`${name} must be an object`);
    }
}
function call(name, result, types = [], args = []) {
    if (!moduleInstance) throw new Error("Simulator is not ready");
    return moduleInstance.ccall(name, result, types, args);
}
function paint() {
    if (backlight) {
        context.putImageData(new ImageData(framebuffer, 320, 240), 0, 0);
    } else {
        context.fillStyle = "black";
        context.fillRect(0, 0, 320, 240);
    }
}
function pixels(x, y, width, height, bytes) {
    for (let row = 0; row < height; ++row) {
        for (let col = 0; col < width; ++col) {
            const source = (row * width + col) * 2;
            const rgb = (bytes[source] << 8) | bytes[source + 1];
            const target = ((y + row) * 320 + x + col) * 4;
            const r = rgb >>> 11;
            const g = (rgb >>> 5) & 63;
            const b = rgb & 31;
            framebuffer[target] = (r << 3) | (r >>> 2);
            framebuffer[target + 1] = (g << 2) | (g >>> 4);
            framebuffer[target + 2] = (b << 3) | (b >>> 2);
            framebuffer[target + 3] = 255;
        }
    }
}
function state() {
    return JSON.parse(call("sim_state", "string"));
}
function update() {
    paint();
    const snapshot = state();
    document.getElementById("diagnostics").textContent = JSON.stringify(snapshot, null, 2);
    document.getElementById("status").textContent =
        `Ready | ${playing ? "Playing" : "Paused"} | virtual ${snapshot.now} ms | ${snapshot.frames} rendered frames`;
    return snapshot;
}
function advance(ms) {
    number(ms, 0, 3600000, "Advance duration");
    call("sim_advance", null, ["number"], [ms]);
    return update();
}
function input(kind, value) {
    const id = { encoder: 0, touch: 1, rotate: 2 }[kind];
    if (id === undefined) throw new Error("Input must be encoder, touch or rotate");
    number(value, id === 2 ? -2147483648 : 0, id === 2 ? 2147483647 : 1, "Input value");
    if (!call("sim_input", "number", ["number", "number"], [id, value])) {
        throw new Error("Input rejected; see simulator diagnostics");
    }
    return advance(0);
}
function press(duration = 80, kind = "encoder") {
    number(duration, 1, 60000, "Press duration");
    input(kind, 1);
    advance(duration);
    input(kind, 0);
    return advance(5);
}
function setTime(hour, minute, second = 0, valid = true) {
    number(hour, 0, 23, "Hour");
    number(minute, 0, 59, "Minute");
    number(second, 0, 59, "Second");
    if (typeof valid !== "boolean") throw new Error("Time validity must be boolean");
    call("sim_time", null, ["number", "number", "number", "number"], [hour, minute, second, +valid]);
    document.getElementById("hour").value = hour;
    document.getElementById("minute").value = minute;
    document.getElementById("valid-time").checked = valid;
}
function setEnvironment(options = {}) {
    object(options, "Environment");
    const values = { wifi: 2, status: 1, lux: 100, outdoor: 18, indoor: 22, humidity: 45, aqi: 32, ...options };
    number(values.wifi, 0, 2, "Wi-Fi state");
    number(values.status, 0, 5, "Sample status");
    number(values.lux, 0, 100000, "Lux", false);
    number(values.outdoor, -100, 100, "Outdoor temperature", false);
    number(values.indoor, -100, 100, "Indoor temperature", false);
    number(values.humidity, 0, 100, "Humidity", false);
    number(values.aqi, 0, 500, "AQI");
    call("sim_environment", null, Array(7).fill("number"),
        [values.wifi, values.status, values.lux, values.outdoor, values.indoor, values.humidity, values.aqi]);
    for (const key of ["wifi", "lux", "outdoor", "indoor", "humidity", "aqi"]) {
        document.getElementById(key).value = values[key];
    }
    document.getElementById("sample").value = values.status;
}
function cancelInputs() {
    call("sim_cancel_inputs", null);
    return advance(5);
}
function setPlaying(value) {
    playing = value;
    lastRealTime = 0;
    document.getElementById("play").textContent = value ? "Pause" : "Play";
}
async function reset(options = {}) {
    if (loading) throw new Error("A simulator instance is already loading");
    object(options, "Reset options");
    const config = { enabled: true, hour: 7, minute: 0, volume: 75, sound: 0, fault: false, ...options };
    if (typeof config.enabled !== "boolean") throw new Error("Alarm enabled must be boolean");
    if (typeof config.fault !== "boolean") throw new Error("Fault must be boolean");
    if (config.time !== undefined) object(config.time, "Time options");
    number(config.hour, 0, 23, "Alarm hour");
    number(config.minute, 0, 59, "Alarm minute");
    number(config.volume, 5, 100, "Alarm volume");
    number(config.sound, 0, 2, "Alarm sound");
    loading = true;
    setPlaying(false);
    document.getElementById("status").textContent = "Loading fresh simulator instance...";
    document.getElementById("errors").textContent = "";
    framebuffer.fill(0);
    const generation = ++instanceGeneration;
    try {
        moduleInstance = await createHourglass({
            onPixels: (x, y, w, h, bytes) => {
                if (generation === instanceGeneration) pixels(x, y, w, h, bytes);
            },
            onBacklight: (value) => { backlight = value; },
            onError: showError,
            printErr: showError,
            onAbort: (reason) => showError(`WebAssembly aborted: ${reason}`),
        });
        setEnvironment(config.environment);
        setTime(config.time?.hour ?? 10, config.time?.minute ?? 8,
            config.time?.second ?? 0, config.time?.valid ?? true);
        if (config.fault) call("sim_fault", null, ["number"], [1]);
        call("sim_start", null, Array(5).fill("number"),
            [+config.enabled, config.hour, config.minute, config.volume, config.sound]);
        return advance(0);
    } finally {
        loading = false;
    }
}
const scenarios = {
    colors: async () => { await reset(); call("sim_colors", null); },
    clock: async () => { await reset(); },
    disabled: async () => { await reset({ enabled: false }); },
    weather: async () => { await reset(); press(40, "touch"); advance(320); },
    unavailable: async () => { await reset({ environment: { status: 4 } }); press(40, "touch"); advance(320); },
    stale: async () => { await reset({ environment: { status: 2 } }); press(40, "touch"); advance(320); },
    boot: async () => { await reset({ environment: { wifi: 0 } }); },
    syncing: async () => { await reset({ time: { valid: false } }); },
    fault: async () => { await reset({ fault: true }); },
    minute: async () => { await reset(); press(1000); },
    hour: async () => { await reset(); press(1000); press(); },
    sound: async () => { await reset(); press(1000); press(); press(); },
    ringing: async () => { await reset({ time: { hour: 7, minute: 0 } }); },
    snooze: async () => { await reset({ time: { hour: 7, minute: 0 } }); press(40, "touch"); },
    dismiss: async () => { await reset({ time: { hour: 7, minute: 0 } }); press(); },
    midnight: async () => { await reset({ time: { hour: 0, minute: 0 } }); },
    noon: async () => { await reset({ time: { hour: 12, minute: 0 } }); },
    night: async () => { await reset({ environment: { lux: 1 } }); },
    off: async () => { await reset({ environment: { lux: 1 } }); advance(3600000); },
    slide: async () => { await reset(); input("touch", 1); advance(150); input("touch", 0); },
};
for (const volume of [5, 50, 55, 100]) {
    scenarios[`volume${volume}`] = async () => {
        await reset({ volume: volume === 5 ? 10 : volume - 5 });
        input("rotate", volume === 5 ? -1 : 1);
        advance(160);
    };
}
async function scenario(name) {
    if (!Object.hasOwn(scenarios, name)) throw new Error(`Unknown scenario: ${name}`);
    await scenarios[name]();
    document.getElementById("scenario").value = name;
    return update();
}
function capture() {
    paint();
    return canvas.toDataURL("image/png");
}
window.hgPreview = {
    reset, scenario, advance, input, press, cancelInputs, state, setTime, setEnvironment, capture,
    play: () => setPlaying(true), pause: () => setPlaying(false),
    scenarios: Object.keys(scenarios),
};

function handle(id, action) {
    document.getElementById(id).addEventListener("click", async () => {
        try {
            await window.hgPreview.ready;
            if (loading) throw new Error("Wait for the scenario to finish loading");
            await action();
        } catch (error) { showError(error); }
    });
}
const picker = document.getElementById("scenario");
for (const name of Object.keys(scenarios)) picker.add(new Option(name, name));
handle("load", () => scenario(picker.value));
handle("encoder-down", () => input("encoder", 1));
handle("encoder-up", () => input("encoder", 0));
handle("touch-down", () => input("touch", 1));
handle("touch-up", () => input("touch", 0));
handle("press", () => press());
handle("hold", () => press(1000));
handle("tap", () => press(40, "touch"));
handle("left", () => { input("rotate", -1); advance(5); });
handle("right", () => { input("rotate", 1); advance(5); });
handle("play", () => { setPlaying(!playing); update(); });
handle("advance", () => advance(Number(document.getElementById("step").value)));
handle("apply", () => {
    const value = (id) => Number(document.getElementById(id).value);
    setTime(value("hour"), value("minute"), 0, document.getElementById("valid-time").checked);
    setEnvironment({ wifi: value("wifi"), status: value("sample"), lux: value("lux"),
        outdoor: value("outdoor"), indoor: value("indoor"), humidity: value("humidity"), aqi: value("aqi") });
    advance(1000);
});
handle("capture", () => {
    const link = document.createElement("a");
    link.download = `hourglass-${state().now}.png`;
    link.href = capture();
    link.click();
});
document.getElementById("scale").addEventListener("change", (event) => {
    const scale = Number(event.target.value);
    canvas.style.width = `${320 * scale}px`;
    canvas.style.height = `${240 * scale}px`;
});
window.addEventListener("blur", () => {
    if (!moduleInstance || loading) return;
    try {
        cancelInputs();
        setPlaying(false);
    } catch (error) { showError(error); }
});
function animate(timestamp) {
    if (playing && !loading) {
        try {
            if (lastRealTime) advance(Math.min(100, Math.floor(timestamp - lastRealTime)));
            lastRealTime = timestamp;
        } catch (error) { setPlaying(false); showError(error); }
    }
    requestAnimationFrame(animate);
}
requestAnimationFrame(animate);
const initial = new URLSearchParams(location.search).get("scenario") || "clock";
picker.value = initial;
window.hgPreview.ready = scenario(initial).catch((error) => { showError(error); throw error; });
