# Original alarm sounds

These three sounds are original deterministic additive synthesis created for
this project. They contain no external samples, Android audio, or Loftie audio
or melodies. Their source is [generate_alarms.py](../../tools/audio/generate_alarms.py);
they are distributed under the same terms as this project's own source.
No third-party recording license is required for these assets.

| File | Audition | Character | PCM SHA-256 |
|---|---|---|---|
| `warm_mallets.wav` | 1A | Warm mallets, original phrasing (default) | `77319f02385ecd8c37b2a1c8c929e3f550700fd587a9be2754a4dd3631acf37e` |
| `wooden_call.wav` | 1F | Woody call-and-answer strikes | `5086282d6b1f56517508a936d9e0c66c00d01a7880062fcd8f35b003c1593ae4` |
| `plucked_dawn.wav` | 1H | Harp-like plucked strikes | `e300b0688c8d821b50860eced1d6ff64246064c9608f373d1820c67be8088082` |

Each loop is mono signed 16-bit PCM, 16,000 Hz, 96,000 frames (six seconds),
192,000 PCM bytes. All three together use 576,000 flash bytes plus metadata.
Their RMS is -27.33 dBFS; this preserves the approved auditions, not the previous
beep's loudness. Perceived loudness differs despite equal RMS.

Note tails wrap across the loop boundary. Do not fade each cycle or force the
endpoints to zero: that introduces a recurring audible dip. Playback supplies
its existing one-time startup fade and volume ramp.

Regenerate WAVs with Python and `numpy==2.4.6` (offline tooling only), then generate
the firmware constants using the standard-library-only converter:

```powershell
python tools\audio\generate_alarms.py
python tools\audio\wav_to_pcm.py
python -m unittest discover -s tools\audio -p "test_*.py"
```

`src/services/sound_alarm_clip.cpp` is generated; do not edit or open it.
The raw-PCM hashes above identify the exact approved loops.

The obsolete `alarm_clip_16k_mono.wav` was derived from "Short alarm" by yd:
<https://opengameart.org/content/short-alarm>, CC0 1.0 Universal,
<https://creativecommons.org/publicdomain/zero/1.0/>.
It is no longer shipped or used in firmware.
