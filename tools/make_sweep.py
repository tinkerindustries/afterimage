#!/usr/bin/env python3
"""Writes a 30 second logarithmic sine sweep from 40 Hz to 10 kHz as a 48 kHz 16-bit stereo WAV.

    python make_sweep.py <out.wav>

The script uses only the standard library, so two machines running it produce the same samples
when their libm agrees. Compare the SHA-256 of the output to check.
"""
import math
import struct
import sys
import wave

RATE = 48000
SECONDS = 30
F0 = 40.0
F1 = 10000.0
AMPLITUDE = 0.5


def main(path):
    ratio = F1 / F0
    k = SECONDS / math.log(ratio)
    frames = bytearray()
    for n in range(RATE * SECONDS):
        t = n / RATE
        phase = 2.0 * math.pi * F0 * k * (math.exp(t / k) - 1.0)
        sample = int(round(AMPLITUDE * 32767.0 * math.sin(phase)))
        frames += struct.pack("<hh", sample, sample)
    with wave.open(path, "wb") as out:
        out.setnchannels(2)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(bytes(frames))


if __name__ == "__main__":
    main(sys.argv[1])
