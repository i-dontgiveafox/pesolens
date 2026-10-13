#!/usr/bin/env python3
"""
generate_english_clips.py — synthesize every English audio clip the pesos firmware needs.

Uses Microsoft Edge's free neural voices via the `edge-tts` package. Saves each clip as a
16 kHz mono 16-bit PCM WAV into ../data/<clip_id>.wav. Idempotent: existing WAVs are skipped.

Special-case clips:
- beep.wav  — 0.2 s 1000 Hz sine confirmation tone (synthesized, not TTS)
- error.wav — 0.4 s descending two-tone 1000 Hz → 500 Hz (synthesized, not TTS)

Dependencies (pip):
    pip install edge-tts pydub
And ideally `ffmpeg` on PATH for the cleanest MP3 → WAV conversion. Without ffmpeg the
script falls back to a basic Python-only path that's less robust but still produces WAVs.

Usage:
    python generate_english_clips.py
    python generate_english_clips.py --voice en-GB-SoniaNeural
    python generate_english_clips.py --voice en-US-GuyNeural --out ../data
"""

from __future__ import annotations

import argparse
import asyncio
import math
import os
import struct
import sys
import wave
from pathlib import Path

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

DEFAULT_VOICE = "en-US-AriaNeural"
SAMPLE_RATE = 16000
CHANNELS = 1
SAMPLE_WIDTH_BYTES = 2  # 16-bit PCM

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_OUT = (SCRIPT_DIR / ".." / "data").resolve()

SPOKEN_CLIPS: dict[str, str] = {
    # digits
    "zero": "zero",
    "one": "one",
    "two": "two",
    "three": "three",
    "four": "four",
    "five": "five",
    "six": "six",
    "seven": "seven",
    "eight": "eight",
    "nine": "nine",
    # teens
    "ten": "ten",
    "eleven": "eleven",
    "twelve": "twelve",
    "thirteen": "thirteen",
    "fourteen": "fourteen",
    "fifteen": "fifteen",
    "sixteen": "sixteen",
    "seventeen": "seventeen",
    "eighteen": "eighteen",
    "nineteen": "nineteen",
    # tens
    "twenty": "twenty",
    "thirty": "thirty",
    "forty": "forty",
    "fifty": "fifty",
    "sixty": "sixty",
    "seventy": "seventy",
    "eighty": "eighty",
    "ninety": "ninety",
    # multipliers
    "hundred": "hundred",
    "thousand": "thousand",
    # connector
    "and": "and",
    # currency
    "peso": "peso",
    "pesos": "pesos",
}

# beep + error are synthesized tones, generated separately
TONE_CLIPS = ("beep", "error")

ALL_CLIP_IDS: tuple[str, ...] = tuple(SPOKEN_CLIPS.keys()) + TONE_CLIPS


# ---------------------------------------------------------------------------
# Tone synthesis (no TTS — pure math)
# ---------------------------------------------------------------------------

def _write_wav_int16(path: Path, samples: list[int]) -> None:
    """Write a list of int16 samples to a 16 kHz mono PCM WAV."""
    with wave.open(str(path), "wb") as w:
        w.setnchannels(CHANNELS)
        w.setsampwidth(SAMPLE_WIDTH_BYTES)
        w.setframerate(SAMPLE_RATE)
        w.writeframes(b"".join(struct.pack("<h", s) for s in samples))


def _sine(freq_hz: float, duration_s: float, amplitude: float = 0.6) -> list[int]:
    n = int(SAMPLE_RATE * duration_s)
    peak = int(32767 * amplitude)
    out: list[int] = []
    # Tiny linear envelope to avoid clicks at edges (5 ms in, 5 ms out)
    fade = max(1, int(SAMPLE_RATE * 0.005))
    for i in range(n):
        env = 1.0
        if i < fade:
            env = i / fade
        elif i > n - fade:
            env = (n - i) / fade
        sample = math.sin(2.0 * math.pi * freq_hz * (i / SAMPLE_RATE)) * env
        out.append(int(sample * peak))
    return out


def synth_beep(out_path: Path) -> None:
    """0.2 s, 1000 Hz sine, ~ -4 dBFS."""
    _write_wav_int16(out_path, _sine(1000.0, 0.2))


def synth_error(out_path: Path) -> None:
    """0.4 s descending two-tone: 0.2 s @ 1000 Hz, 0.2 s @ 500 Hz."""
    samples = _sine(1000.0, 0.2) + _sine(500.0, 0.2)
    _write_wav_int16(out_path, samples)


# ---------------------------------------------------------------------------
# TTS path — edge-tts → MP3, then convert to 16 kHz mono 16-bit PCM WAV
# ---------------------------------------------------------------------------

async def _edge_tts_to_mp3(text: str, voice: str, mp3_path: Path) -> None:
    """Synthesize `text` to an MP3 at `mp3_path` using edge-tts."""
    try:
        import edge_tts  # type: ignore
    except ImportError as exc:
        raise SystemExit(
            "edge-tts is not installed. Run: pip install edge-tts pydub"
        ) from exc

    communicate = edge_tts.Communicate(text=text, voice=voice)
    await communicate.save(str(mp3_path))


def _mp3_to_wav_pydub(mp3_path: Path, wav_path: Path) -> bool:
    """Convert MP3 → 16 kHz mono 16-bit PCM WAV via pydub. Returns True on success."""
    try:
        from pydub import AudioSegment  # type: ignore
    except ImportError:
        return False
    try:
        import imageio_ffmpeg  # type: ignore
        AudioSegment.converter = imageio_ffmpeg.get_ffmpeg_exe()
    except Exception:
        pass
    try:
        seg = AudioSegment.from_file(str(mp3_path), format="mp3")
    except Exception as exc:
        print(f"  ! pydub couldn't decode {mp3_path.name}: {exc}", file=sys.stderr)
        return False
    seg = seg.set_frame_rate(SAMPLE_RATE).set_channels(CHANNELS).set_sample_width(SAMPLE_WIDTH_BYTES)
    seg.export(str(wav_path), format="wav")
    return True


def _mp3_to_wav_ffmpeg(mp3_path: Path, wav_path: Path) -> bool:
    """Direct ffmpeg invocation as a last-ditch fallback. Returns True on success."""
    import shutil
    import subprocess
    ffmpeg = shutil.which("ffmpeg")
    if ffmpeg is None:
        try:
            import imageio_ffmpeg  # type: ignore
            ffmpeg = imageio_ffmpeg.get_ffmpeg_exe()
        except Exception:
            return False
    cmd = [
        ffmpeg, "-y", "-loglevel", "error",
        "-i", str(mp3_path),
        "-ar", str(SAMPLE_RATE),
        "-ac", str(CHANNELS),
        "-sample_fmt", "s16",
        str(wav_path),
    ]
    try:
        subprocess.run(cmd, check=True)
        return True
    except subprocess.CalledProcessError as exc:
        print(f"  ! ffmpeg conversion failed for {mp3_path.name}: {exc}", file=sys.stderr)
        return False


def _python_only_fallback(text: str, wav_path: Path) -> None:
    """Last-resort: write a 0.6 s 440 Hz placeholder so the build still has *something*."""
    print(
        f"  ! No edge-tts/pydub/ffmpeg pipeline succeeded for '{text}'. "
        f"Writing a 0.6 s placeholder tone — replace this file before shipping.",
        file=sys.stderr,
    )
    _write_wav_int16(wav_path, _sine(440.0, 0.6, amplitude=0.4))


async def synth_word(clip_id: str, text: str, voice: str, out_dir: Path) -> int:
    """Synthesize one spoken clip. Returns the size in bytes of the resulting WAV."""
    wav_path = out_dir / f"{clip_id}.wav"
    mp3_path = out_dir / f"_{clip_id}.mp3"
    try:
        await _edge_tts_to_mp3(text, voice, mp3_path)
    except SystemExit:
        raise
    except Exception as exc:  # network errors, voice unavailable, rate-limit
        print(f"  ! edge-tts failed for '{text}': {exc}", file=sys.stderr)
        _python_only_fallback(text, wav_path)
        return wav_path.stat().st_size

    converted = _mp3_to_wav_pydub(mp3_path, wav_path) or _mp3_to_wav_ffmpeg(mp3_path, wav_path)
    if mp3_path.exists():
        try:
            mp3_path.unlink()
        except OSError:
            pass

    if not converted:
        _python_only_fallback(text, wav_path)

    return wav_path.stat().st_size


# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------

async def _main_async(voice: str, out_dir: Path, force: bool) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)

    generated = 0
    skipped = 0
    total_bytes = 0

    print(f"[edge-tts] voice = {voice}")
    print(f"[output]   {out_dir}")
    if force:
        print(f"[force]    re-generating all clips, including existing ones")
    print()

    # 1. Spoken words
    for clip_id, text in SPOKEN_CLIPS.items():
        wav_path = out_dir / f"{clip_id}.wav"
        if wav_path.exists() and not force:
            print(f"  - {clip_id}.wav (exists, skipping — pass --force to regenerate)")
            skipped += 1
            total_bytes += wav_path.stat().st_size
            continue
        print(f"  + {clip_id}.wav  '{text}'")
        size = await synth_word(clip_id, text, voice, out_dir)
        generated += 1
        total_bytes += size

    # 2. Tones
    for clip_id, fn in (("beep", synth_beep), ("error", synth_error)):
        wav_path = out_dir / f"{clip_id}.wav"
        if wav_path.exists() and not force:
            print(f"  - {clip_id}.wav (exists, skipping — pass --force to regenerate)")
            skipped += 1
            total_bytes += wav_path.stat().st_size
            continue
        print(f"  + {clip_id}.wav  (synth tone)")
        fn(wav_path)
        generated += 1
        total_bytes += wav_path.stat().st_size

    print()
    print(f"[done] generated={generated}  skipped={skipped}  "
          f"total_clips={generated + skipped}  total_size={total_bytes/1024:.1f} KB")
    if skipped > 0 and not force:
        print(f"[note] {skipped} existing clips skipped — pass --force to overwrite "
              f"placeholder/silent stubs with real TTS audio")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n", 1)[0])
    parser.add_argument(
        "--voice",
        default=DEFAULT_VOICE,
        help=f"edge-tts voice name (default: {DEFAULT_VOICE}). "
             f"Run `edge-tts --list-voices` to see all options.",
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=DEFAULT_OUT,
        help=f"Output directory (default: {DEFAULT_OUT})",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Re-generate every clip even if the .wav file already exists. "
             "Use this to overwrite the silent placeholders from make_placeholders.py.",
    )
    args = parser.parse_args()

    if os.name == "nt":
        # edge-tts uses aiohttp; the default Windows event loop policy can refuse it.
        try:
            asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())  # type: ignore[attr-defined]
        except AttributeError:
            pass

    asyncio.run(_main_async(args.voice, args.out.resolve(), args.force))


if __name__ == "__main__":
    main()
