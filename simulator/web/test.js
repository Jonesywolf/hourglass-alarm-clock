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
        data[i + 1] < 10 && data[i + 2] < 10), "Night theme has correctly ordered red RGB565 pixels");
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
    for (const name of sim.scenarios) {
        await sim.scenario(name);
        check(sim.state().frames > 0, `Scenario ${name} renders`);
        check(!document.getElementById("errors").textContent, `Scenario ${name} has no errors`);
    }
    check(!sim.state().effects.audioAvailable && !sim.state().effects.ledAvailable,
        "Unavailable physical outputs are not reported active");
    await sim.scenario("clock");
    return { passed: results.length, assertions: results };
}
