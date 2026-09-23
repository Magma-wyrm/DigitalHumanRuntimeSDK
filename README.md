# DigitalHumanRuntimeSDK

**Real-time, WebSocket-driven lip sync and behaviour runtime for MetaHuman digital humans — built for Unreal Engine 5.6 by [Parth Sarwade](https://github.com/Magma-wyrm).**

![Engine](https://img.shields.io/badge/Unreal%20Engine-5.6-0e1128?logo=unrealengine)
![Platform](https://img.shields.io/badge/platform-Win64-informational)
![License](https://img.shields.io/badge/license-MIT-green)
![Status](https://img.shields.io/badge/status-beta-orange)

DigitalHumanRuntimeSDK connects a MetaHuman character to **any** backend AI voice pipeline (LLM + TTS + STT — bring your own) over a single WebSocket connection, and drives its face, idle motion, gestures, and emotional state in real time from streamed PCM audio and JSON status events. No third-party lip-sync plugin, no NVIDIA Audio2Face hop, no offline-only baking step — audio arrives, the face responds, in the same frame budget as the rest of the game.

This is the version of the plugin built for and running in a production real-estate sales-avatar deployment ("Aria"), packaged as a **DSP-only, backend-agnostic lip-sync branch** (`MultiBandLipSync`) for general release.

## Why this exists

Every off-the-shelf option had a dealbreaker for a real-time, self-hosted deployment:

| Option | Problem |
|---|---|
| Marketplace lip-sync plugins | Paid, closed-source, or binary-data limits over WebSocket |
| NVIDIA Audio2Face | Extra process + GPU + network hop, adds latency |
| MetaHuman Animator | Offline-only audio-to-face baking, not real-time |
| MetaHuman Live Link audio source | Binds to a device UUID at build time — breaks packaged builds |
| Generative video avatars (HeyGen-style) | Not viable for a real-time, on-prem, single-GPU deployment |

So this plugin does the DSP itself, in C++, on the game thread's audio budget, with zero external inference calls.

## Architecture

```
Backend (your LLM/TTS/STT, any language)
        │  WebSocket: JSON text frames + binary PCM frames
        ▼
DigitalHumanSubsystem            — owns the one connection (UGameInstanceSubsystem)
   internally: FDigitalHumanWebSocketManager — reconnect/backoff, heartbeat, framing
        ▼
DigitalHumanAudioPlayerComponent — USoundWaveProcedural playback, no temp files
        ▼
DigitalHumanFacialDriverComponent — 3-band RBJ biquad filterbank + RMS/ZCR → mouth curves
        ▼
DigitalHumanBehaviorComponent    — 4-state FSM (Idle / Listening / Thinking / Speaking)
        │
        ├── DigitalHumanIdleAnimationComponent — blink, breathing, saccades, head micro-movement
        ├── DigitalHumanEmotionComponent        — named-emotion blend (smile / brow raise / eye squint)
        └── DigitalHumanGestureComponent        — montage gestures (bring your own) + procedural nod
```

## How it works

Facial animation is computed per audio chunk from raw PCM alone — no phoneme timing, no ML model — which is what keeps the plugin backend-agnostic:

- **Mouth openness** — RMS amplitude of the chunk, smoothed with a one-pole exponential filter (`EnvelopeSmoothingSpeed = 12` reaches ~90% of a step in roughly 190 ms), scaled by `MouthOpenGain = 4.0` and clamped to [0, 1]. Saturates at RMS = 0.25 (≈ −12 dBFS).
- **Mouth width** — zero-crossing rate as a cheap high-frequency-content proxy (no FFT), scaled by `MouthWideGain = 6.0`.
- **Rounding / fricative / plosive shaping** — three RBJ-cookbook band-pass biquads (300 Hz / 1500 Hz / 4000 Hz, Q = 0.9, Direct Form II Transposed, stateful across chunks) split the signal into low/mid/high energy bands; their relative energy drives a rounded-vowel, a fricative, and a plosive-pulse amount. All three filters are unconditionally stable. This is frequency-content heuristics, not phoneme classification — it reads as noticeably more alive than jaw-open alone, but won't survive a lip-reader's scrutiny.
- **Plosive pulses** fire on a dual condition — a slow moving-average RMS below a silence threshold, followed by a sharp RMS jump — which is deliberately chunk-granularity-limited; see Known Limitations.

**Idle behaviour** (blink, breathing, eye saccades, head micro-movement) runs entirely independently of the network/audio state — the character keeps blinking and drifting with zero backend connection. Head drift uses Perlin-noise, not uniform-random jitter, so it reads as organic rather than twitchy. It's layered underneath the audio-driven mouth curves, never replacing them.

**Conversation state** (`Idle` / `Listening` / `Thinking` / `Speaking`) is a small FSM: `Speaking` is auto-detected from whether audio is actually playing and always wins over a manual override (forcing it away while the mouth is visibly moving would look broken); the other three states are driven either by a direct Blueprint call or a `{"type":"state",...}` message from the backend. Eye saccades are suppressed while `Speaking` and restored — not force-enabled — once it ends.

Full property-by-property reference: [`Docs/README.md`](Docs/README.md).

## Features

- **Zero-dependency real-time lip sync** — RMS envelope, zero-crossing rate, and a 3-band RBJ biquad filterbank drive five mouth-shape outputs directly from PCM, no phoneme data required.
- **Every DSP gain, threshold, and idle-animation timing is an `EditAnywhere` property** — tune mouth gains, plosive thresholds, blink intervals, and head-movement amplitude from the Details panel, no recompile needed.
- **Independent, always-on idle behaviour** — blinking, breathing, eye saccades, and Perlin-noise head drift that keep running with zero backend connection, layered underneath the audio-driven curves.
- **Resilient WebSocket transport** — exponential backoff reconnect (1s → 30s ceiling), optional heartbeat, and JSON/binary frames routed to the right component by the subsystem so a stray status message can never land in the audio path.
- **Backend-agnostic protocol** — any backend that can speak the small JSON/binary protocol in [`Docs/README.md`](Docs/README.md) can drive this plugin. No coupling to a specific STT/LLM/TTS stack.

## Requirements

- Unreal Engine 5.6
- Win64 (see `PlatformAllowList` in `DigitalHumanRuntimeSDK.uplugin` — porting to other platforms is untested but should be straightforward, the plugin has no Windows-specific code)
- A MetaHuman character in your project (not included — bring your own via Fab/MetaHuman Creator)
- A backend that speaks the WebSocket protocol documented in [`Docs/README.md`](Docs/README.md)

## Installation

1. Copy this repository into your project's `Plugins/DigitalHumanRuntimeSDK/` folder.
2. Regenerate project files and build, or launch the `.uproject` and let Unreal prompt you to rebuild.
3. Enable **Digital Human Runtime SDK** in Edit → Plugins (Animation category).
4. Add the components to your MetaHuman Actor and point `UDigitalHumanSubsystem::Connect()` at your backend's WebSocket URL — see [`Docs/README.md`](Docs/README.md) for the full component and AnimBP wiring walkthrough.

## Documentation

[`Docs/README.md`](Docs/README.md) — full component reference (every property and event), the WebSocket protocol spec, the AnimBP wiring guide, a tuning-parameter table, and troubleshooting.

## Known limitations

- DSP analysis runs once per received audio chunk. If the backend sends whole-sentence chunks (as the reference backend does), feature updates are sentence-rate, not frame-rate. The plosive-onset detector in particular is chunk-granularity-limited: on real sentence-sized chunks it can only ever fire near the very start of a turn, versus firing correctly on every real onset when audio is sub-chunked to ~100 ms. The detector logic itself is correct; the fix is buffering/backend-side (finer chunking), not a DSP change.
- Gesture montages are state-triggered but no montage assets ship with the plugin — bring your own.
- Windows-only `PlatformAllowList` currently; untested on other platforms.

## License

MIT — see [`LICENSE`](LICENSE). Copyright (c) 2026 Parth Sarwade.

## Author

Built and maintained by **Parth Sarwade** — electronics engineer working on AI-driven real-time systems and immersive visualization.
Issues and PRs welcome at the repository's issue tracker.
