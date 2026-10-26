#!/usr/bin/env python3
"""Reproduce the approved 1A/1F/1H loops. Requires numpy==2.4.6; firmware does not."""

import hashlib
import wave
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
RATE = 16000
FRAMES = RATE * 6


def fade(samples, seconds):
    samples = samples.copy()
    count = round(seconds * RATE)
    ramp = np.sin(np.linspace(0, np.pi / 2, count)) ** 2
    samples[:count] *= ramp
    samples[-count:] *= ramp[::-1]
    return samples


def tone(hz, kind):
    t = np.arange(4 * RATE) / RATE
    settings = {
        "mallet": (.012, [(1, 1, 1.05), (2.01, .23, .5), (3.98, .065, .22)]),
        "knock": (.007, [(1, 1, .8), (2.01, .28, .25), (3.98, .09, .13)]),
        "harp": (.005, [(1, 1, 1.1), (2, .33, .7), (3, .16, .42),
                        (4, .085, .27), (5, .035, .18)]),
    }
    attack, partials = settings[kind]
    signal = np.zeros_like(t)
    for ratio, level, decay in partials:
        signal += level * np.sin(2 * np.pi * hz * ratio * t) * np.exp(-t / decay)
    signal *= 1 - np.exp(-(t / attack) ** 2)
    return fade(signal, .04)


def main():
    # 1A uses its original attack and release envelope once, not a cycle-wide fade.
    designs = [
        ("warm_mallets.wav", "mallet", [(.15, 440, 1), (1.15, 554.365, .85),
         (2.15, 659.255, .8), (3.65, 440, 1), (4.65, 554.365, .8)]),
        ("wooden_call.wav", "knock", [(.15, 440, 1), (.80, 659.255, .7),
         (2.15, 493.883, .95), (2.80, 739.989, .65), (4.15, 440, 1), (4.80, 659.255, .7)]),
        ("plucked_dawn.wav", "harp", [(.15, 440, 1), (.85, 554.365, .8),
         (1.55, 659.255, .75), (3.15, 493.883, .95), (3.85, 587.330, .8),
         (4.55, 739.989, .7)]),
    ]
    for filename, kind, strikes in designs:
        cycle = np.zeros(FRAMES)
        for when, hz, gain in strikes:
            note = tone(hz, kind)
            indices = (np.arange(len(note)) + round(when * RATE)) % FRAMES
            np.add.at(cycle, indices, note * gain)
        if kind == "mallet":
            t = np.arange(FRAMES) / RATE
            cycle += .055 * np.sin(2 * np.pi * 196 * t)
            cycle += .025 * np.sin(2 * np.pi * 294 * t)
        cycle -= cycle.mean()
        cycle *= 10 ** (-27.33 / 20) / float(np.sqrt(np.mean(cycle ** 2)))
        pcm = np.rint(cycle * 32767).astype("<i2").tobytes()
        path = ROOT / "assets" / "audio" / filename
        with wave.open(str(path), "wb") as output:
            output.setparams((1, 2, RATE, FRAMES, "NONE", "not compressed"))
            output.writeframes(pcm)
        print(filename, hashlib.sha256(pcm).hexdigest())


if __name__ == "__main__":
    main()
