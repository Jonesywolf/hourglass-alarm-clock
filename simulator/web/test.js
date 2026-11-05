export async function runSimulationTests() {
    const sim = window.hgPreview;
    await sim.ready;
    const results = [];
    function check(condition, label) {
        if (!condition) throw new Error(`FAIL: ${label}\n${JSON.stringify(sim.state())}`);
        results.push(label);
    }
    function rotate(value) { sim.input("rotate", value); sim.advance(5); }
    function imageBytes() {
        return document.getElementById("display").getContext("2d").getImageData(0, 0, 320, 240).data;
    }
    for (const action of [() => sim.advance(-1), () => sim.input("invalid", 1),
        () => sim.setTime(24, 0), () => sim.setEnvironment({ humidity: 101 }),
        () => sim.reset({ fault: "yes" })]) {
        let rejected = false;
        try { await action(); } catch (error) {
            rejected = error instanceof Error;
        }
        check(rejected, "Invalid simulator input rejects explicitly");
    }
    await sim.scenario("colors");
    const colors = imageBytes();
    for (const [index, expected] of [[0, [255, 0, 0]], [1, [0, 255, 0]],
        [2, [0, 0, 255]], [3, [255, 255, 255]], [4, [0, 0, 0]]]) {
        const offset = (120 * 320 + index * 64 + 32) * 4;
        check(expected.every((value, channel) => colors[offset + channel] === value),
            `LVGL RGB565 diagnostic patch ${index} has exact RGB channels`);
    }
    await sim.scenario("clock");
    check(sim.state().frames > 0 && !sim.state().boot, "Clock rendered and boot cleared");
    const first = sim.capture();
    await sim.scenario("clock");
    check(first === sim.capture(), "Fresh clock captures are byte-identical");
    check(imageBytes().some((value, i) => i % 4 !== 3 && value > 0), "Canvas contains rendered UI pixels");
    check(document.getElementById("display").width === 320 &&
        document.getElementById("display").height === 240, "Capture is exactly 320x240");
    const wifiPictures = new Set();
    for (const [rssi, bars] of [[-86, 0], [-85, 1], [-75, 2], [-65, 3]]) {
        await sim.reset({ environment: { rssi } });
        sim.advance(2500);
        check(sim.state().wifi.bars === bars, `RSSI ${rssi} maps to signal level ${bars}`);
        wifiPictures.add(JSON.stringify(regionPixels(290, 10, 22, 22)));
    }
    check(wifiPictures.size === 4, "Four Wi-Fi levels have distinct Phosphor drawings");
    const wifiPixels = regionPixels(290, 10, 22, 22);
    const wifiColumns = [];
    for (let x = 0; x < 22; ++x) {
        if (Array.from({ length: 22 }, (_, y) => (y * 22 + x) * 3)
            .some(i => Math.max(wifiPixels[i], wifiPixels[i + 1], wifiPixels[i + 2]) > 32)) {
            wifiColumns.push(x);
        }
    }
    check(wifiColumns.at(-1) - wifiColumns[0] + 1 >= 18,
        "Clock Wi-Fi artwork is at least 18 visible pixels wide");
    sim.setEnvironment({ wifi: 0 });
    sim.advance(1000);
    check(sim.state().wifi.bars === 0, "Disconnected Wi-Fi reports no signal");
    await sim.scenario("clock");
    sim.iconSample();
    const normalIcon = regionPixels(0, 0, 18, 18);
    check(normalIcon.some(value => value > 100), "Phosphor fixture has visible foreground");
    const center = (10 * 18 + 9) * 3;
    if (sim.state().uiIconWeight === "duotone") {
        check(normalIcon[center] >= 41 && normalIcon[center] <= 57,
            "True Duotone renders the fully covered interior at 20% opacity");
    } else if (sim.state().uiIconWeight === "fill") {
        check(normalIcon[center] > 200, "Fill drop interior is solid");
    } else {
        check(normalIcon[center] === 0, "Monochrome drop interior stays transparent");
    }
    await sim.scenario("clock");
    sim.iconSample(153, true);
    const nightIcon = regionPixels(0, 0, 18, 18);
    check(nightIcon.some((value, i) => i % 3 === 0 && value > 0), "Night icon has visible warm tint");
    check(nightIcon.every((value, i, p) => i % 3 !== 0 ||
        (p[i + 1] <= p[i] * 0.5 + 8 && p[i + 2] <= p[i] * 0.15 + 8)), "Both faded layers retain night tint");

    await sim.reset({ enabled: false });
    sim.input("encoder", 1);
    sim.advance(50);
    sim.cancelInputs();
    check(!sim.state().alarm.enabled && !sim.state().input.encoderDown,
        "Focus-loss recovery cancels hold without toggling alarm");
    sim.press();
    check(sim.state().alarm.enabled, "Short press toggles on release");
    sim.input("encoder", 1);
    sim.advance(999);
    check(sim.state().edit.field === 0, "999ms hold does not edit");
    sim.advance(1);
    check(sim.state().edit.field === 1, "1000ms hold enters minutes");
    sim.input("encoder", 0);
    sim.advance(5);
    rotate(-1);
    check(sim.state().edit.hour === 6 && sim.state().edit.minute === 59, "Minute borrow updates draft");
    check(sim.state().alarm.hour === 7 && sim.state().alarm.minute === 0, "Draft does not change saved alarm");
    sim.press(40, "touch");
    check(sim.state().page === 0, "Touch ignored while editing");
    sim.press();
    check(sim.state().edit.field === 2, "Short press cycles to hours");
    sim.press();
    check(sim.state().edit.field === 3, "Short press cycles to sound");
    rotate(-1);
    check(sim.state().edit.sound === 2 && sim.state().alarm.sound === 0, "Sound choice wraps in draft");
    sim.press(1000);
    check(sim.state().edit.field === 0 && sim.state().alarm.sound === 2, "Hold commits edited alarm");

    await sim.scenario("minute");
    const releasedAt = sim.state().now;
    sim.advance(9994);
    check(sim.state().edit.field === 1, "Editor remains active before timeout");
    sim.advance(10);
    check(sim.state().edit.field === 0 && sim.state().now > releasedAt, "Ten-second inactivity commits");

    await sim.reset({ volume: 50 });
    rotate(1);
    check(sim.state().alarm.volume === 55 && sim.state().volumeVisible, "Volume step crosses 50/55 boundary");
    sim.advance(150);
    check(!sim.state().volumeAnimating, "Volume entry completes at configured duration");
    rotate(-2147483648);
    check(sim.state().alarm.volume === 5, "Extreme negative rotation clamps at 5");
    rotate(2147483647);
    check(sim.state().alarm.volume === 100, "Extreme positive rotation clamps at 100");
    sim.advance(2990);
    check(sim.state().volumeVisible, "Volume remains visible before three-second timeout");
    sim.advance(10);
    check(!sim.state().volumeVisible, "Volume hides at three seconds");
    sim.advance(155);
    check(!sim.state().volumeAnimating, "Volume exit animation completes");

    await sim.scenario("slide");
    check(sim.state().sliding, "Slide is active at intermediate virtual time");
    sim.advance(160);
    check(sim.state().page === 1 && !sim.state().sliding, "Page transition completes after 300ms");
    const volume = sim.state().alarm.volume;
    rotate(1);
    check(sim.state().alarm.volume === volume && !sim.state().volumeVisible, "Weather ignores volume input");

    await sim.scenario("ringing");
    check(sim.state().alarm.ringing && sim.state().alarmOverlay, "Actual alarm policy rings at matching time");
    function regionPixels(x, y, width, height) {
        const data = imageBytes();
        const pixels = [];
        for (let row = y; row < y + height; ++row) {
            for (let col = x; col < x + width; ++col) {
                const offset = (row * 320 + col) * 4;
                pixels.push(data[offset], data[offset + 1], data[offset + 2]);
            }
        }
        return pixels;
    }
    function centroid(x, y, width, height) {
        const pixels = regionPixels(x, y, width, height);
        let total = 0, sumX = 0, sumY = 0;
        for (let i = 0; i < pixels.length; i += 3) {
            const weight = pixels[i] + pixels[i + 1] + pixels[i + 2];
            const p = i / 3;
            total += weight;
            sumX += weight * (p % width);
            sumY += weight * Math.floor(p / width);
        }
        return { x: sumX / total, y: sumY / total };
    }
    const topArrow = regionPixels(150, 0, 20, 26);
    const stopArrow = regionPixels(190, 198, 34, 26);
    const timeAndMeridiem = regionPixels(55, 141, 210, 57);
    const snoozeHint = centroid(95, 20, 130, 34);
    const stopHint = centroid(86, 198, 114, 30);
    check(!regionPixels(115, 53, 90, 13).some(v => v > 0) &&
        !regionPixels(115, 128, 90, 13).some(v => v > 0),
        "Alarm emblem has no decorative outer circle");
    sim.advance(380);
    const snoozeNudge = centroid(95, 20, 130, 34);
    const stopNudge = centroid(86, 198, 114, 30);
    check(snoozeHint.y - snoozeNudge.y > 5 && Math.abs(snoozeNudge.x - snoozeHint.x) < 0.5,
        "Snooze instruction nudges straight up toward the touch pad");
    check(stopNudge.x - stopHint.x > 5 && Math.abs(stopNudge.y - stopHint.y) < 0.5,
        "Stop instruction nudges straight right toward the knob");
    const nudgePath = [];
    for (let t = 0; t < 600; t += 20) {
        sim.advance(20);
        nudgePath.push(centroid(95, 20, 130, 34).y);
    }
    check(nudgePath.every((y, i) => i === 0 || Math.abs(y - nudgePath[i - 1]) <= 1.01),
        "Hint nudge eases back without multi-pixel jumps");
    sim.advance(20);
    function pulseChanged(before, after) {
        return before.some((v, i) => v !== after[i]) &&
            after.some(v => v > 0) && Math.max(...after) < Math.max(...before);
    }
    check(pulseChanged(topArrow, regionPixels(150, 0, 20, 26)),
        "Top arrow gently dims but remains visible at the pulse trough");
    check(pulseChanged(stopArrow, regionPixels(190, 198, 34, 26)),
        "Stop arrow gently dims but remains visible at the pulse trough");
    function samePixels(before, after) { return before.every((v, i) => v === after[i]); }
    check(samePixels(timeAndMeridiem, regionPixels(55, 141, 210, 57)),
        "Alarm digits and meridiem stay still during pulse");
    const snoozeRest = centroid(95, 20, 130, 34);
    const stopRest = centroid(86, 198, 114, 30);
    check(Math.abs(snoozeRest.y - snoozeHint.y) < 0.6 && Math.abs(stopRest.x - stopHint.x) < 0.6,
        "Instructions rest at home after the nudge");
    sim.advance(1000);
    check(samePixels(topArrow, regionPixels(150, 0, 20, 26)),
        "Arrow pulse returns to its initial brightness after a full cycle");
    const snoozeBack = centroid(95, 20, 130, 34);
    const stopBack = centroid(86, 198, 114, 30);
    check(Math.abs(snoozeBack.y - snoozeHint.y) < 0.01 && Math.abs(stopBack.x - stopHint.x) < 0.01,
        "Instructions return to their starting position after a full cycle");
    sim.press(40, "touch");
    check(sim.state().alarm.snoozed && !sim.state().alarm.ringing &&
        sim.state().alarm.minute === 15, "Touch snoozes for fifteen minutes");
    check(regionPixels(120, 14, 80, 65).some(v => v > 0),
        "Snooze confirmation renders the restored full-size alarm glyph");
    sim.advance(2600);
    rotate(1);
    check(sim.state().alarm.volume === 80, "Snoozed volume allowed after confirmation clears");
    sim.press();
    check(!sim.state().alarm.snoozed && sim.state().alarm.enabled &&
        sim.state().alarm.minute === 0, "Encoder dismisses snooze without disabling saved alarm");
    check(regionPixels(120, 14, 80, 65).some(v => v > 0),
        "Stop confirmation renders the restored full-size alarm glyph");
    sim.press();
    check(sim.state().alarm.enabled, "Post-dismiss lockout prevents accidental disable");

    await sim.scenario("boot");
    sim.press(40, "touch");
    check(sim.state().boot && sim.state().page === 0, "Boot overlay blocks navigation");
    sim.setEnvironment({ wifi: 2 });
    sim.advance(750);
    check(!sim.state().boot && sim.state().page === 0, "Boot clears without replaying blocked input");
    await sim.scenario("fault");
    check(sim.state().boot, "Runtime failure renders fault overlay");

    await sim.scenario("night");
    check(sim.state().displayMode === 1, "Dark lux activates night theme");
    check(imageBytes().some((value, i, data) => i % 4 === 0 && value > 40 &&
        data[i + 1] < value * 0.5 && data[i + 2] < 16), "Night theme has correctly ordered warm RGB565 pixels");
    await sim.scenario("off");
    check(sim.state().displayMode === 2 && sim.state().brightness === 0, "Dark display turns off at one hour");
    check(!imageBytes().some((value, i) => i % 4 !== 3 && value !== 0), "Backlight-off canvas is black");
    sim.press(40, "touch");
    check(sim.state().displayMode === 1 && sim.state().page === 0, "First touch wakes only");
    sim.press(40, "touch");
    sim.advance(320);
    check(sim.state().page === 1, "Next touch navigates normally");

    await sim.scenario("sound");
    sim.advance(100);
    const soundCapture = sim.capture();
    await sim.scenario("sound");
    sim.advance(100);
    check(soundCapture === sim.capture(), "Timed sound-editor captures are deterministic");

    const LINE = { none: 0, thunder: 1, indoor: 2, aqi: 3, gusts: 4, heat: 5, cold: 6, uv: 7, briefing: 8 };
    function anyPixel(x, y, width, height, test) {
        const p = regionPixels(x, y, width, height);
        for (let i = 0; i < p.length; i += 3) if (test(p[i], p[i + 1], p[i + 2])) return true;
        return false;
    }
    const sunny = (r, g, b) => r > 200 && g > 140 && b < 90;
    // Nightlight is warm red/amber: no blue, green well below red.
    const notRed = (r, g, b) => g > r * 0.5 + 8 || b > r * 0.15 + 8;
    await sim.scenario("clock");
    check(sim.state().weather.showWeather && sim.state().weather.glanceLine === LINE.none,
        "Calm weather shows the chip with no context line");
    check(anyPixel(6, 4, 40, 40, sunny), "Clock glance icon renders in full color");
    for (const [name, line] of [["glanceStorm", LINE.thunder], ["glanceStuffy", LINE.indoor],
        ["glanceSmoke", LINE.aqi], ["glanceGusts", LINE.gusts], ["glanceHeat", LINE.heat]]) {
        await sim.scenario(name);
        check(sim.state().weather.glanceLine === line, `${name} selects glance line ${line}`);
    }
    check(sim.state().weather.indoorLevel === 0 || sim.state().weather.indoorLevel === 1,
        "Default indoor air rates Good");
    await sim.reset({ environment: { sky: 1, outdoor: -20, high: -12, low: -24 } });
    check(sim.state().weather.glanceLine === LINE.cold, "Bitter cold selects cold line");
    await sim.reset({ environment: { sky: 1, uv: 9 } });
    check(sim.state().weather.glanceLine === LINE.uv, "High UV selects UV line");
    await sim.reset({ environment: { sky: 11, co2: 1800 } });
    check(sim.state().weather.glanceLine === LINE.thunder, "Active storm outranks stuffy indoor air");
    await sim.scenario("glanceStuffy");
    check(sim.state().weather.indoorLevel === 3, "High CO2 rates indoor air Poor");
    await sim.scenario("briefing");
    check(!sim.state().alarm.ringing && sim.state().weather.glanceLine === LINE.briefing,
        "Stopping the alarm shows the day briefing");
    sim.advance(15 * 60000 - 5000);
    check(sim.state().weather.glanceLine === LINE.briefing, "Briefing persists inside its window");
    sim.advance(3000);
    check(sim.state().weather.glanceLine !== LINE.briefing, "Briefing ends after fifteen minutes");
    await sim.scenario("snooze");
    check(sim.state().weather.glanceLine !== LINE.briefing, "Snoozing does not start the briefing");
    await sim.scenario("glanceNight");
    check(!anyPixel(0, 0, 320, 240, notRed), "Nightlight clock glance renders red only");

    await sim.scenario("weather");
    check(sim.state().page === 1 && sim.state().weather.offset === 0, "Weather page starts at now");
    check(anyPixel(6, 24, 80, 80, sunny), "Weather hero icon renders in full color");
    rotate(1);
    check(sim.state().weather.offset === 1, "Encoder scrolls the hourly strip");
    rotate(1000);
    check(sim.state().weather.offset === 19, "Hourly scroll clamps at the last full window");
    rotate(-1000);
    check(sim.state().weather.offset === 0, "Hourly scroll clamps at now");
    rotate(3);
    sim.press(40, "touch");
    sim.advance(320);
    sim.press(40, "touch");
    sim.advance(320);
    check(sim.state().page === 1 && sim.state().weather.offset === 0,
        "Leaving the weather page resets the hourly scroll");
    const metric = sim.capture();
    await sim.scenario("weatherImperial");
    await sim.reset({ environment: { sky: 1, outdoor: 31, feels: 34, high: 33, low: 20, uv: 9 } });
    sim.press(40, "touch");
    sim.advance(320);
    const metricHot = sim.capture();
    await sim.scenario("weatherImperial");
    check(metricHot !== sim.capture() && metric !== metricHot, "Imperial units change the rendered page");
    let unitsRejected = false;
    try { sim.setEnvironment({ units: 0 }); } catch (error) { unitsRejected = error instanceof Error; }
    check(unitsRejected, "Live unit changes are rejected; units load at boot");
    await sim.scenario("weatherNightlight");
    check(!anyPixel(0, 0, 320, 240, notRed), "Nightlight weather page renders red only");
    await sim.scenario("weatherNoSensor");
    check(sim.state().weather.indoorLevel === 0, "Missing SEN66 reports unknown indoor air");

    function verticalBounds(x, y, width, height) {
        const data = imageBytes();
        let top = Infinity, bottom = -Infinity;
        for (let row = y; row < y + height; ++row) {
            for (let col = x; col < x + width; ++col) {
                const offset = (row * 320 + col) * 4;
                if (Math.max(data[offset], data[offset + 1], data[offset + 2]) > 32) {
                    top = Math.min(top, row);
                    bottom = Math.max(bottom, row);
                }
            }
        }
        return { top, bottom, center: (top + bottom) / 2 };
    }
    for (const night of [false, true]) {
        for (let sky = 1; sky <= 12; ++sky) {
            await sim.reset({
                environment: { sky, outdoor: sky % 2 ? -8 : 18, lux: night ? 1 : 100 },
                time: { hour: night ? 23 : 10 },
            });
            const smallArt = verticalBounds(6, 0, 40, 44);
            const smallTemp = verticalBounds(50, 0, 140, 44);
            check(Math.abs(smallArt.center - smallTemp.center) <= 1.5,
                `Clock artwork/temperature are visually centered: sky ${sky}, night ${night}`);
            sim.press(40, "touch");
            sim.advance(320);
            const largeArt = verticalBounds(6, 4, 80, 100);
            const largeTemp = verticalBounds(94, 4, 140, 74);
            check(Math.abs(largeArt.center - largeTemp.center) <= 1.5,
                `Weather artwork/temperature are visually centered: sky ${sky}, night ${night}`);
            check(largeTemp.bottom < 78, "Centered temperature stays clear of the feels-like detail");
            check(!anyPixel(96, 96, 220, 18, (r, g, b) => Math.max(r, g, b) > 32),
                "Fresh weather has no redundant condition or status line");
        }
    }
    for (const name of ["stale", "fetching", "unavailable"]) {
        await sim.scenario(name);
        check(anyPixel(96, 96, 220, 18, (r, g, b) => Math.max(r, g, b) > 32),
            `${name} weather retains an explicit status message`);
    }
    sim.setEnvironment({ status: 1 });
    sim.advance(1000);
    check(!anyPixel(96, 96, 220, 18, (r, g, b) => Math.max(r, g, b) > 32),
        "Recovered weather clears the exceptional status message");
    await sim.scenario("fetching");
    const placeholderArt = verticalBounds(6, 4, 80, 100);
    const placeholderTemp = verticalBounds(94, 4, 140, 74);
    check(Math.abs(placeholderArt.center - placeholderTemp.center) <= 1.5,
        "Fetching placeholder and unknown artwork are visually centered");

    for (const name of sim.scenarios) {
        await sim.scenario(name);
        check(sim.state().frames > 0, `Scenario ${name} renders`);
        check(!document.getElementById("errors").textContent, `Scenario ${name} has no errors`);
    }
    check(!sim.state().effects.audioAvailable && !sim.state().effects.ledAvailable,
        "Unavailable physical outputs are not reported active");

    const contrast = await sim.contrastAudit();
    for (const token of contrast.tokens) {
        check(token.pass, `WCAG AA ${token.mode} ${token.name} ${token.color} ` +
            `${token.ratio}:1 >= ${token.required}:1`);
    }
    check(contrast.results.length > 300, "Contrast audit covers visible text and graphics");
    const night = contrast.results.filter((r) => ["night", "weatherNightlight", "glanceNight"]
        .includes(r.scenario));
    check(night.length > 0 && night.every((r) => r.pass), "Nightlight scenarios meet WCAG 2.2 AA");
    const slide = contrast.results.filter((r) => r.scenario === "slide");
    check(slide.some((r) => r.clipped), "Contrast audit covers the mid-slide frame and its clipped edges");
    const offenders = contrast.failures.map((r) => `${r.scenario}: ${r.kind} "${r.text}" ` +
        `${r.ratio}:1 < ${r.required}:1 (${r.fg} on ${r.bg})`);
    check(!offenders.length, `Every scenario meets WCAG 2.2 AA contrast${offenders.length ?
        `\n${offenders.join("\n")}` : ""}`);
    await sim.scenario("clock");
    return { passed: results.length, assertions: results };
}
