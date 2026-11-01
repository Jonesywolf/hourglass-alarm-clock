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
// [min, max, integer, label]; ids match the form controls. airStatus defaults to status.
const environmentSpec = {
    wifi: [0, 2, true, "Wi-Fi state"], lux: [0, 100000, false, "Lux"],
    status: [0, 5, true, "Weather status"], sky: [0, 12, true, "Condition"],
    outdoor: [-60, 60, false, "Outdoor temperature"], feels: [-60, 60, false, "Feels like"],
    high: [-60, 60, false, "High"], low: [-60, 60, false, "Low"],
    wind: [0, 250, false, "Wind"], gust: [0, 300, false, "Gust"], uv: [0, 15, false, "UV index"],
    rain: [0, 100, true, "Rain chance"], aqi: [0, 500, true, "Outdoor AQI"],
    airStatus: [0, 5, true, "Air status"], indoorStatus: [0, 5, true, "Indoor status"],
    indoor: [-40, 80, false, "Indoor temperature"], humidity: [0, 100, false, "Humidity"],
    co2: [0, 40000, true, "CO2"], pm25: [0, 1000, false, "PM2.5"], voc: [0, 500, true, "VOC index"],
    units: [0, 1, true, "Units"],
};
const environmentDefaults = {
    wifi: 2, lux: 100, status: 1, sky: 2, outdoor: 18, feels: 17, high: 22, low: 11, wind: 12,
    gust: 20, uv: 5, rain: 10, aqi: 32, indoorStatus: 1, indoor: 22, humidity: 45, co2: 650,
    pm25: 4, voc: 90, units: 0,
};
let started = false;
let environment = { ...environmentDefaults };
function setEnvironment(options = {}) {
    object(options, "Environment");
    for (const key of Object.keys(options)) {
        if (!Object.hasOwn(environmentSpec, key)) throw new Error(`Unknown environment key: ${key}`);
    }
    const values = { ...environmentDefaults, airStatus: options.status ?? environmentDefaults.status, ...options };
    for (const [key, [min, max, integer, label]] of Object.entries(environmentSpec)) {
        number(values[key], min, max, label, integer);
    }
    if (started && values.units !== environment.units) {
        throw new Error("Units are read at boot; pass them to reset({ environment })");
    }
    environment = values;
    call("sim_environment", null, ["number", "number"], [values.wifi, values.lux]);
    call("sim_weather", null, Array(12).fill("number"), [values.status, values.sky, values.outdoor,
        values.feels, values.high, values.low, values.wind, values.gust, values.uv, values.rain,
        values.aqi, values.airStatus]);
    call("sim_indoor", null, Array(6).fill("number"), [values.indoorStatus, values.indoor,
        values.humidity, values.co2, values.pm25, values.voc]);
    if (!started) call("sim_units", null, ["number"], [values.units]);
    for (const key of Object.keys(environmentSpec)) {
        document.getElementById(key).value = values[key];
    }
}function cancelInputs() {
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
        started = false;
        setTime(config.time?.hour ?? 10, config.time?.minute ?? 8,
            config.time?.second ?? 0, config.time?.valid ?? true);
        setEnvironment(config.environment);
        if (config.fault) call("sim_fault", null, ["number"], [1]);
        call("sim_start", null, Array(5).fill("number"),
            [+config.enabled, config.hour, config.minute, config.volume, config.sound]);
        started = true;
        return advance(0);
    } finally {
        loading = false;
    }
}
const toWeather = () => { press(40, "touch"); advance(320); };
const weatherAt = (environment, time) => async () => { await reset({ environment, time }); toWeather(); };
const scenarios = {
    colors: async () => { await reset(); call("sim_colors", null); },
    clock: async () => { await reset(); },
    disabled: async () => { await reset({ enabled: false }); },
    weather: weatherAt({}),
    weatherNight: weatherAt({ sky: 1, outdoor: 9, feels: 7, high: 15, low: 4 }, { hour: 22, minute: 30 }),
    weatherRain: weatherAt({ sky: 6, rain: 85, outdoor: 12, feels: 10, high: 14, low: 9, wind: 24, gust: 46 }),
    weatherScroll: async () => { await weatherAt({ rain: 70 })(); input("rotate", 3); advance(50); },
    weatherImperial: weatherAt({ units: 1, sky: 1, outdoor: 31, feels: 34, high: 33, low: 20, uv: 9 }),
    weatherNightlight: weatherAt({ lux: 1, sky: 6, rain: 80 }, { hour: 23, minute: 15 }),
    weatherNoSensor: weatherAt({ indoorStatus: 4 }),
    fetching: weatherAt({ status: 0, airStatus: 0 }),
    unavailable: weatherAt({ status: 4 }),
    stale: weatherAt({ status: 2 }),
    glanceStorm: async () => { await reset({ environment: { sky: 11, rain: 90 } }); },
    glanceSmoke: async () => { await reset({ environment: { sky: 3, aqi: 172 } }); },
    glanceStuffy: async () => { await reset({ environment: { co2: 1720 } }); },
    glanceGusts: async () => { await reset({ environment: { sky: 3, wind: 38, gust: 72 } }); },
    glanceHeat: async () => { await reset({ environment: { sky: 1, outdoor: 34, high: 38, units: 1 } }); },
    glanceNight: async () => { await reset({ environment: { lux: 1, sky: 1, outdoor: -3 } }); },
    briefing: async () => {
        await reset({ time: { hour: 7, minute: 0 }, environment: { rain: 70, high: 19, low: 8 } });
        press();
        advance(3000);
    },
    briefingOver: async () => {
        await reset({ time: { hour: 7, minute: 0 }, environment: { rain: 70, high: 19, low: 8 } });
        press();
        advance(15 * 60000 + 1000);
    },
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
handle("apply", async () => {
    const value = (id) => Number(document.getElementById(id).value);
    const values = Object.fromEntries(Object.keys(environmentSpec).map((key) => [key, value(key)]));
    const time = { hour: value("hour"), minute: value("minute"),
        valid: document.getElementById("valid-time").checked };
    if (values.units !== environment.units) {
        await reset({ environment: values, time });
        return;
    }
    setTime(time.hour, time.minute, 0, time.valid);
    setEnvironment(values);
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
