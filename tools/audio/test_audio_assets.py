"""Hardware-free checks for the approved alarm assets and conversion guards."""

import array
import contextlib
import hashlib
import io
import math
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock
import wave

import wav_to_pcm

ROOT = Path(__file__).resolve().parents[2]
HASHES = {
    "warm_mallets.wav": "77319f02385ecd8c37b2a1c8c929e3f550700fd587a9be2754a4dd3631acf37e",
    "wooden_call.wav": "5086282d6b1f56517508a936d9e0c66c00d01a7880062fcd8f35b003c1593ae4",
    "plucked_dawn.wav": "e300b0688c8d821b50860eced1d6ff64246064c9608f373d1820c67be8088082",
}


class AudioAssetsTest(unittest.TestCase):
    def test_approved_pcm_and_format(self):
        for name, expected in HASHES.items():
            with self.subTest(name=name), wave.open(str(ROOT / "assets" / "audio" / name)) as clip:
                self.assertEqual((1, 2, 16000, 96000), (
                    clip.getnchannels(), clip.getsampwidth(),
                    clip.getframerate(), clip.getnframes()))
                data = clip.readframes(clip.getnframes())
                self.assertEqual(192000, len(data))
                self.assertEqual(expected, hashlib.sha256(data).hexdigest())

    def test_levels_and_nonexceptional_loop_steps(self):
        for name in HASHES:
            with self.subTest(name=name), wave.open(str(ROOT / "assets" / "audio" / name)) as clip:
                samples = array.array("h", clip.readframes(clip.getnframes()))
                if sys.byteorder != "little":
                    samples.byteswap()
                mean = sum(samples) / len(samples) / 32768
                rms = math.sqrt(sum(s * s for s in samples) / len(samples)) / 32768
                self.assertAlmostEqual(-27.33, 20 * math.log10(rms), delta=.01)
                self.assertLess(abs(mean), 1e-6)
                self.assertLess(max(abs(s) for s in samples), 16384)
                max_step = max(abs(b - a) for a, b in zip(samples, samples[1:]))
                self.assertLessEqual(abs(samples[0] - samples[-1]), max_step)
                self.assertNotEqual(0, samples[0])  # Do not reintroduce cycle-wide fades.

    def convert_fixture(self, channels=1, width=2, rate=16000, frames=96000, truncate=False):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            folder = root / "assets" / "audio"
            folder.mkdir(parents=True)
            path = folder / "warm_mallets.wav"
            with wave.open(str(path), "wb") as clip:
                clip.setparams((channels, width, rate, frames, "NONE", "not compressed"))
                clip.writeframes(b"\0" * (frames * channels * width))
            if truncate:
                path.write_bytes(path.read_bytes()[:-100])
            output = root / "output.cpp"
            with mock.patch.object(wav_to_pcm, "ROOT", root), \
                    mock.patch.object(sys, "argv", ["wav_to_pcm.py", "-o", str(output)]), \
                    contextlib.redirect_stdout(io.StringIO()):
                with self.assertRaises(SystemExit):
                    wav_to_pcm.main()
            self.assertFalse(output.exists())

    def test_converter_rejects_invalid_format(self):
        for change in ({"channels": 2}, {"width": 1}, {"rate": 8000}, {"frames": 0},
                       {"frames": 48000}):
            with self.subTest(change=change):
                self.convert_fixture(**change)

    def test_converter_rejects_truncated_pcm(self):
        self.convert_fixture(truncate=True)

    def test_constant_storage_budget(self):
        self.assertEqual(576000, sum((ROOT / "assets" / "audio" / name).stat().st_size - 44
                                    for name in HASHES))


if __name__ == "__main__":
    unittest.main()
