Alarm clip attribution
======================

`alarm_clip_16k_mono.wav` is derived from "Short alarm" by yd.

- Source page: https://opengameart.org/content/short-alarm
- Direct download: https://opengameart.org/sites/default/files/alarm_0.ogg
- License: CC0 1.0 Universal (public domain dedication),
  http://creativecommons.org/publicdomain/zero/1.0/ (the source page lists "License(s): CC0").
  Attribution is not required; credit is given voluntarily.

Processing: stereo to mono, 44.1 kHz to 16 kHz, first two-tone beep extracted and
repeated four times at 0.5 s spacing, padded with silence to 3.0 s so the loop point is
silent, DC removed, peak-normalized to about -3 dBFS, 5 ms fades at the ends.
Output: mono, 16000 Hz, signed 16-bit PCM, 48000 frames.

To use a different clip, replace the WAV (mono, 16-bit, HG_AUDIO_SAMPLE_RATE) and run
tools/audio/wav_to_pcm.py (see README, "Alarm audio").
