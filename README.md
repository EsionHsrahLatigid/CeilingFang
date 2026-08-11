# CeilingFang

CeilingFang is a YUP stereo lookahead limiter for forcing Digital Harsh Noise against a controllable ceiling. It samples three linear interpolation points between adjacent samples for inter-sample peak awareness, delays the program path, applies immediate downward control with adaptive recovery, and optionally drives a bounded final clip stage. Hosted builds preserve silence; Standalone adds an audition source and meters only at compile time.

## Identity and formats

- App/plugin ID: `jp.ehl.ceilingfang`
- Vendor: `ehl_`; AU manufacturer: `EHL1`; AU subtype: `ClFn`
- Version: `0.1.0`
- macOS: Standalone, VST3, AUv2
- Windows: Standalone, VST3
- Stereo effect, no MIDI

## Parameters

- `Ceiling`: `-24` to `0` dB output ceiling.
- `Lookahead`: 0–20 ms delayed program path.
- `Release`: 5–800 ms base recovery.
- `Detect`: linked average-to-peak detector blend.
- `ZeroBias`: softens the gain attack toward lower-discontinuity behavior.
- `Adapt`: shortens release when recent overshoot energy rises.
- `Clip`: bounded post-limiter saturation intensity.

## Research basis

[ITU-R BS.1770-5](https://www.itu.int/rec/R-REC-BS.1770-5-202311-I/_page.print) defines the context for true-peak measurement, while [FFmpeg's official limiter documentation](https://ffmpeg.org/ffmpeg-filters.html) provides a practical reference for lookahead, ceiling, attack, and release controls; peak-limiting history was also surveyed through [AES E-Library 5139](https://secure.aes.org/forum/pubs/journal/?elib=5139). CeilingFang uses a lightweight four-position linear inter-sample estimate, not a standards-conformance true-peak meter. Its detector blend, adaptive release memory, zero-bias control, and clip stage are product choices.

## Build and artifacts

```sh
cmake --preset engine-debug
cmake --build --preset engine-debug --parallel
ctest --preset engine-debug --output-on-failure

cmake --preset plugin-release
cmake --build --preset plugin-release --parallel
ctest --preset plugin-release --output-on-failure
```

Human-facing products are staged under `artifacts/plugin-release/<platform-arch>/` in `standalone/`, `vst3/`, and macOS `au/`. `build/` is internal compiler state.

## CI and safety

Caller workflows pin `EsionHsrahLatigid/yup-actions` to a full commit SHA. CI tests and packages macOS arm64 and Windows x64, producing checksummed latest ZIPs; `v*` tags promote exact-SHA CI artifacts without rebuilding. The audio callback allocates no memory and performs no locks, I/O, logging, or UI work. Ceiling behavior, clip response, detector modes, extremes, hosted silence/state, and Standalone audition are tested.
