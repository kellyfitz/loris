# The Spc file format

*Reverse-documented 2026-09-07 from `src/SpcFile.cpp` and `src/AiffData.h` at Loris 2.1
development. There is no external specification; this file is the reading of the code.*

An **Spc** file carries reassigned bandwidth-enhanced Partial data in the form Kyma's
*Envelope Reader* sounds expect. It is not a general-purpose analysis interchange format —
that is SDIF. Spc is a delivery format with hard, awkward limits, and it is structured as an
**AIFF file whose sample data is not audio**: each 24-bit "sample" is a packed envelope
breakpoint. An ordinary AIFF reader will open one and play noise.

The export path is `Loris::SpcFile::write` (`loris.exportSpc` from Python); the import path
is `SpcFile::readSpcData` (`loris.importSpc`).

---

## 1. Container and chunk layout

A standard AIFF `FORM`/`AIFF` container. All multi-byte fields are big-endian; the sample
rate is an IEEE 754 80-bit extended value, per AIFF.

Chunks are written in this order (`SpcFile.cpp:265`):

| Order | ID | Purpose |
|---|---|---|
| 1 | `FORM` | container, form type `AIFF` |
| 2 | `COMM` | channels, frame count, bit depth, sample rate |
| 3 | `MARK` | markers — **written only if the file has any** |
| 4 | `INST` | base note and detune, from the fractional MIDI note number |
| 5 | `APPL` / `SOSe` | the Spc-specific chunk: stream count, hop, enhanced flag |
| 6 | `SSND` | the packed envelope streams |

`SOSe` before `SSND` is worth noting: a reader needs `SOSe` to know how to divide the sample
data, and it arrives first.

### `COMM`

Fixed for Spc: `channels = 1`, `bitsPerSample = 24`. `srate` is the rate the data is meant
to play back at, defaulting to 44100 Hz (`SpcFile::DefaultRate`); it is a playback hint, not
an audio sample rate.

`sampleFrames` is the total number of 24-bit words in `SSND`:

```
sampleFrames = frames × fileNumPartials × (enhanced ? 2 : 1)
```

### `INST`

`baseNote` and `detune` encode one fractional MIDI note number. The fraction becomes cents,
rounded into the neighbouring semitone when it exceeds 50, and the sign is inverted because
AIFF's `detune` is "cents to subtract":

```cpp
ck.baseNote = Byte(midiNoteNum);
ck.detune   = Byte(long(100 * midiNoteNum) % 100);
if (ck.detune > 50) { ck.baseNote++; ck.detune -= 100; }
ck.detune *= -1;
```

Import reverses it as `notenum_ = baseNote - 0.01 × detune`. So `setMidiNoteNumber(37.11)`
survives a round trip. Loop fields are all zero — Kyma does sustain looping by marker name,
not through `INST`.

### `MARK`

Marker positions are **not** in seconds and not in frames — they are in *stream slots*,
which is to say AIFF sample-frame positions:

```cpp
m.position = Uint_32(markers[j].time() / spcEI.hop)
             * spcEI.fileNumPartials * (spcEI.enhanced ? 2 : 1);
```

Import inverts it with the same factor. Marker times are therefore quantized to the frame
grid, which is why Spc needs its own `configureSosMarkerCk` rather than the shared AIFF one.
Names are Pascal strings capped at 254 characters and padded to an even total length.

---

## 2. The `SOSe` chunk

An `APPL` chunk with signature `SOSe`. The first three fields are ordinary:

| Field | Type | Meaning |
|---|---|---|
| `signature` | `Int_32` | `'SOSe'` |
| `enhanced` | `Int_32` | 1 = bandwidth-enhanced (two streams per Partial), 0 = sine-only |
| `validPartials` | `Int_32` | **stream** count: `numPartials × (enhanced ? 2 : 1)` |

Then comes the part the source itself calls "unbelievably nasty" (`AiffData.h:161`). What
follows is a fixed-length array of `Int_32` — an obsolete per-Partial initial-phase table
that nothing writes any more — with two live values buried at *variable* offsets inside it:

```
initPhase[validPartials]     = resolution     (frame duration, microseconds)
initPhase[validPartials + 1] = quasiHarmonic  (how many streams are quasi-harmonic)
```

Everything else in the array is zero. The array is `LargestLabel + 8` = **264** `Int_32`, so
the chunk body is 12 + 1056 = 1068 bytes. The two live values move depending on how many
streams the file has, which is what makes the layout fragile — see §5.

`resolution` is the authoritative hop time on import; `quasiHarmonic` equals `validPartials`
in every file Loris writes.

---

## 3. Envelope data in `SSND`

`SSND` holds `frames × fileNumPartials × (enhanced ? 2 : 1)` big-endian 24-bit words, laid
out **frame-major**: all streams for frame 0, then all streams for frame 1, and so on. Within
a frame, streams run in label order 1…`fileNumPartials`. In an enhanced file each Partial
contributes two consecutive words, *left* then *right*.

### Frames and hop

```
hop    = 2 × numPartials / sampleRate          seconds
frames = int((endTime − startTime) / hop) + 1
```

The hop scales with the Partial count because Kyma's reader consumes one word per Partial per
frame at a fixed word rate — more Partials means each frame takes longer to stream. A
128-Partial file at 44.1 kHz has a 5.8 ms hop; a 32-Partial file has 1.45 ms. This is why
`computeNumPartials` enforces a floor of 32: fewer Partials would give a hop so short that
the envelope data outruns anything useful.

`startTime` and `endTime` are the earliest and latest times over all labelled, non-empty
Partials.

### Padding to a power of two

```cpp
fileNumPartials(n) = 32, 64, 128, or 256   // the smallest power of two ≥ n
```

Slots with no Partial are not left empty. `packEnvelopes` synthesizes them by
frequency-multiplying the *reference Partial* — the lowest-labelled non-empty one — at
magnitude zero:

```cpp
double freqMult = (double)label / (double)refLabel;
double magMult  = 0.0;
afbp(refPar, tim, phaseRefTime, magMult, freqMult, amp, freq, bw, phase);
```

So a pad slot is silent but carries a plausible harmonic frequency, which keeps Kyma's
interpolation from lurching if the slot is ever unmuted.

### Word packing

Each 24-bit word packs two quantities, 7 bits of log magnitude above 16 bits of a linear or
log parameter (`pack`, `SpcFile.cpp:648`):

| Word | bits 16–22 | bits 0–15 |
|---|---|---|
| **left** | log sine magnitude | log frequency, normalized to 22050 Hz |
| **right** (enhanced only) | log noise magnitude | phase, scaled to 0…0xFFFF |

Magnitudes are split from Loris's amplitude/bandwidth pair as

```
sineMag  = amp × sqrt(1 − bw)
noiseMag = min(1.0, 64 × amp × sqrt(bw))
```

The factor of 64 gives the noise channel usable resolution at low bandwidth; import divides
it back out. Note that `noiseMag` **clips** — a loud, wholly noisy Partial saturates, and the
round trip is not exact there.

The logarithmic magnitude companding is

```cpp
coeff     = 65535 / log(32768)
envLog(v) = coeff × log(32768 × v + 1)      // 0…1  →  0x0000…0xFFFF
envExp(i) = (exp(i / coeff) − 1) / 32768    // inverse
```

Only the top 7 bits survive (`& 0xFE00`, then `<< 7`), so magnitude resolution is coarse:
128 steps across the whole log range.

Phase is shifted by one hop and by π/2 on write to match Kyma's convention, wrapped into
0…2π, then scaled to 16 bits:

```cpp
phase -= 2π × hop × freq;
phase += π/2;
```

Import subtracts the π/2 back. Before the first breakpoint of a Partial (and on the final
frame) `afbp` writes zero amplitude and back-extrapolates phase at constant frequency, so
the stream stays phase-coherent through silence.

### Sine-only files

With `enhanced = 0` only the left word is written. The right word — noise magnitude and
phase — is simply absent, so a sine-only Spc file carries no phase at all and half the data.

---

## 4. Limits

These are real constraints of the format, and they bite in a way that is easy to
misattribute.

| Constraint | Value | Enforced at |
|---|---|---|
| Minimum Partials | 32 (padded up) | `computeNumPartials` |
| Maximum label | 256 | `SpcFile::addPartial` |
| Maximum Partials, **sine-only** | **256** | file layout |
| Maximum Partials, **bandwidth-enhanced** | **128** | `configureExportStruct` |
| Labels must be | 1 … 256, no zeros | `SpcFile::addPartial` |
| Bit depth | 24, always | `Bps` |
| Channels | 1, always | `Nchans` |

The enhanced limit is half the sine-only limit for the obvious reason: **the ceiling is on
streams, not Partials**, and an enhanced Partial costs two streams. 256 streams is the
format's real capacity.

The Partial count is also padded to a power of two before any of this is evaluated, so the
effective ceilings are the power-of-two steps — 128 enhanced, 256 sine-only. A 129-Partial
enhanced export is padded to 256 Partials, which is 512 streams: twice what the format holds.

Two further consequences of the padding, both surprising:

- `growPartials` **labels every padded slot**, so a file built from a single Partial labelled
  200 reports 256 Partials, not 1.
- `computeNumPartials` counts those empty slots on purpose — "We purposely consider partials
  with no breakpoints, to allow a larger number of partials than actually have data."

---

## 5. Fixed bug: enhanced export above 128 Partials corrupted the stack

**Fixed 2026-09-07.** Before the fix, `exportSpc(..., enhanced = 1)` with more than 128
Partials did not raise an exception — it aborted the process. It is recorded here because
the shape of the mistake is a property of the format: the file's capacity is counted in
streams, and almost every check in the code counts Partials.

The limit checks that exist are on *Partials*, never on *streams*. The one that actually
fires is in `SpcFile::addPartial`, long before any writing:

```cpp
if (label > LargestLabel)   // 256
    Throw(InvalidArgument, "Spc Partial label is too large, cannot have more than 256.");
```

There is a second check in `configureExportStruct`:

```cpp
if (spcEI.numPartials < 1 || spcEI.numPartials > LargestLabel)
    Throw(FileIOException, "Partials must be distilled and labeled between 1 and 512.");
```

but on the export path it is unreachable. `addPartial` has already rejected every label above
256, so the padded count can never exceed 256 and this guard never trips. It is live only on
import.

Neither check knows about streams. An enhanced file with 256 Partials passes both and then
writes 512 streams. In
`writeSosEnvelopesChunk` the two live values are indexed by the *stream* count into an array
sized by the *Partial* count:

```cpp
static const int InitPhaseLth = (LargestLabel + 8);   // 264
Int_32 bogus[InitPhaseLth];
std::fill(bogus, bogus + InitPhaseLth, 0);
bogus[ck.validPartials]     = ck.resolution;      // index up to 512
bogus[ck.validPartials + 1] = ck.quasiHarmonic;   // index up to 513
```

`bogus` is a local, so the write lands about a kilobyte up the stack, in
`SpcFile::write`'s frame — where the `std::ofstream` lives. The corruption surfaces only when
that stream is destroyed:

```
___BUG_IN_CLIENT_OF_LIBMALLOC_POINTER_BEING_FREED_WAS_NOT_ALLOCATED
std::__1::ios_base::~ios_base()
std::__1::basic_ofstream<...>::~basic_ofstream()
Loris::SpcFile::write(...)
```

Measured behaviour, exporting the sax riff at increasing label ceilings:

| Partials | padded | enhanced | streams | `bogus` index | result |
|---|---|---|---|---|---|
| ≤ 128 | 128 | yes | 256 | 256, 257 | writes correctly |
| 129 – 256 | 256 | yes | 512 | 512, 513 | **`Abort trap: 6`** |
| ≤ 256 | 256 | no | 256 | 256, 257 | writes correctly |
| > 256 | — | either | — | — | clean `InvalidArgument` from `addPartial` |

The `> 256` row is the important contrast: when a limit *is* checked, the exception
propagates properly and SWIG turns it into a Python `RuntimeError`. Nothing is uncaught. The
abort is a distinct failure that only appears in the window where the guards pass but the
layout does not hold.

A 128-Partial enhanced export round-trips cleanly — `exportSpc` then `importSpc` returns 128
Partials labelled 1…128, from a 418,950-byte file — so the format itself is sound at and
below the real ceiling.

### The fix

`configureExportStruct` now guards on the stream count, so the enhanced case fails the same
clean way the over-256 case always did, and `writeSosEnvelopesChunk` carries an `Assert` as a
backstop at the array itself. Over-capacity exports now raise:

```
Spc files hold at most 256 sinusoidal or 128 bandwidth-enhanced Partials.
```

That also retired the stale message quoting "1 and 512", left over from when `LargestLabel`
was 512. The 2005 comment at `SpcFile.cpp:81` — "this cannot actually be 512 for enhanced
Partials … whatever is causing the problem is not using this constant, but some other
hardcoded thing" — describes this bug. Halving `LargestLabel` moved the crash rather than
removing it, because the array and the index into it scale differently: the array shrank
with the constant, the index did not.

`test/test_SpcFile.cpp` pins the limits and round-trips at each ceiling. It aborts against
the unfixed source, so it is a real regression test rather than a restatement of the fix.

Raising the ceiling instead of lowering it is a separate, larger question: `InitPhaseLth`,
`ck.header.size` and Kyma's own reader would all have to agree on a new layout.

---

## 6. Practical notes

- **Distil and label first.** Unlabelled Partials, label 0, or negative labels are rejected.
  `loris.exportSpc` on unchannelized Partials fails immediately.
- **Prune to the ceiling.** Loris offers no "keep the first N labels" operation; the farm
  scripts do it by hand — relabel out-of-range Partials to a sentinel and `removeLabeled`.
- **The round trip is lossy.** 7-bit companded magnitudes, 16-bit frequency and phase, a
  clipping noise channel, and marker times quantized to the hop. Use SDIF to preserve
  analysis data; use Spc to feed Kyma.
- **Sine-only files carry no phase**, so they will not reconstruct transients the way an
  enhanced file does.

---

## 7. Suitability as a fast-synthesis data format

*Assessed 2026-09-08 against the fast synthesizer in `src/fast-synth-src`
(`BlockSynthReader`, `BlockSynthBwe`, `BlockOscillator`), assuming bandwidth-enhanced
Partials throughout.*

The short answer: **Spc is the only format Loris writes whose on-disk structure is already
the fast synthesizer's in-memory structure.** That is a stronger match than it sounds — it
removes the whole load-time preparation stage. What it does not do is scale, and the one
quantity it represents worst is bandwidth, which is exactly the quantity enhanced synthesis
depends on.

One caveat up front, from `src/fast-synth-src/README.md`: `BlockSynthBwe`'s noise generator
is currently a stub (`generate_randi` writes zeros), so the enhanced path cannot be measured
end to end today. Everything below about bandwidth is an assessment of the *format's*
representation, not of rendered output.

### 7.1 Spc is a serialized `BlockSynthReader`

`BlockSynthReader`'s constructor does four things: copies each Partial, runs a
phase-correcting `Resampler` at the block interval, adds a one-block fade in and out, and
fills a rectangular `NumFrames × NumPartials` array of `Breakpoint`s. Every one of those has
an Spc counterpart already applied at export time:

| What the block synthesizer needs | What Spc already holds |
|---|---|
| envelopes sampled uniformly at a fixed block interval | frames at a fixed hop (§3) |
| a rectangular table — a value for every Partial in every frame | a word for every stream in every frame, padded slots included |
| a slot index that maps to one oscillator for the whole run | slot index = label, 1…`fileNumPartials`, fixed |
| zero amplitude outside a Partial's span, phase back-extrapolated at constant frequency so the onset lands on the right phase | exactly what `afbp` writes before a Partial's first breakpoint (§3) |
| a phase at every frame | the right word of each pair — **enhanced files only** |
| the oscillator count before rendering starts | `validPartials / 2`, in `SOSe`, which precedes `SSND` (§1) |

The correspondence extends to the odd details. Spc pads unused slots with a
frequency-multiplied reference Partial at zero magnitude (§3); `BlockSynthBwe::render` skips
any oscillator whose current and target amplitudes are both zero and parks it on the target,
so padded slots cost nothing at render time and are already sitting at a plausible frequency
if one is ever unmuted. `computeNumPartials` counting empty slots on purpose — surprising for
an analysis format — is the right behaviour for a synthesizer that wants a fixed oscillator
bank.

So a `BlockSynthReader` built from an Spc file is an *unpack*, not a resample. Note that
`utils/loris_fastsynth_main.cpp` already accepts `.spc` input, but takes the long way round:
`SpcFile` reconstructs `Partial`s with breakpoints at frame times, and `BlockSynthReader`
then resamples them back onto a grid — discarding the frame geometry and rebuilding it.

### 7.2 Size, access, and what that buys

An enhanced Partial costs 6 bytes per frame. A `Breakpoint` is four `double`s, 32 bytes.

| | Spc, packed | `BlockSynthReader` table |
|---|---|---|
| per Partial per frame | 6 bytes | 32 bytes |
| per frame, 128 Partials | 768 bytes | 4096 bytes |
| per second, 128 Partials at the file's own hop | 132 kB | 706 kB |

5.3× denser, frame-major, and contiguous — a frame is one sequential 768-byte read, and a
Partial's two words are adjacent within it. That admits `mmap` plus direct indexing with no
load-time construction at all, and O(1) seek to any time, which matters for scrubbing,
looping, and marker-driven sustain. Markers are already stored as frame positions (§1), so
loop points land on block boundaries without rounding.

Two further consequences worth noting:

- **Morphing is index-aligned.** Two Spc files with the same Partial count share frame
  geometry exactly, and slot *k* is label *k* in both. Frame-by-frame interpolation needs no
  correspondence search. Crossfading the packed magnitude words directly is a dB-linear
  crossfade, which is usually the one wanted.
- **Unpacking is nearly free.** Magnitudes are 7 bits — 128 possible values — so one
  128-entry table retires `envExp` for both magnitude channels. Frequency needs a real
  `exp` (16 bits; a full table is 256 kB and cache-hostile), but that is one transcendental
  per Partial per *block*, against 128–256 samples of oscillator work.

### 7.3 Where the fit breaks

**(a) Capacity: 128 enhanced Partials.** This is the decisive limit (§4). The fast
synthesizer's motivating benchmark is a distilled clarinet at 409 Partials; Spc cannot carry
it, or anything else past 128, and raising the ceiling means renegotiating `InitPhaseLth`,
`ck.header.size` and Kyma's reader (§5).

**(b) The block size is not yours to choose.** `hop = 2 × numPartials / 44100`, so block
length in samples is exactly twice the (unpadded) Partial count:

| Partials | hop, samples | hop at 44.1 kHz |
|---|---|---|
| 32 | 64 | 1.45 ms |
| 64 | 128 | 2.90 ms |
| 100 | 200 | 4.54 ms |
| 128 (enhanced ceiling) | 256 | 5.81 ms |

`Fastsynth_BlockSize_samples` is 128, so a 64-Partial file is an exact match and a
full 128-Partial file is twice the intended block. `BlockOscillator` interpolates amplitude
and frequency linearly within a block, so doubling the block coarsens that approximation —
the direction is certain, the magnitude is unmeasured. The two knobs are coupled in the
wrong direction, too: the files that most need a short block (more Partials, more spectral
detail) are the ones forced to the longest one.

Also note that `configureExportStruct` computes the hop from a hardcoded 44100
(`SpcFile.cpp:1058`) regardless of `rate_`, which is what goes into `COMM`. The hop is an
integer number of samples only when rendering at 44.1 kHz.

**(c) Bandwidth is the format's least accurate quantity.** The noise channel stores
`64 × amp × sqrt(bw)`, clipped at 1.0, in 7 bits (§3). The 64× scaling assumes small
per-Partial amplitudes: headroom runs out at `amp × sqrt(bw) = 1/64`, i.e. amplitude
0.0156 (−36 dBFS) at full bandwidth. Above that the failure is not graceful compression —
bandwidth *collapses*, because import recovers it as a ratio of the two magnitudes:

| amp | bw | → amp | → bw | amp error |
|---|---|---|---|---|
| 0.5 | 0.5 | 0.348 | **0.002** | −3.1 dB |
| 0.5 | 1.0 | 0.014 | 1.000 | **−30.8 dB** |
| 0.1 | 0.5 | 0.070 | **0.042** | −3.1 dB |
| 0.02 | 0.5 | 0.019 | 0.493 | −0.5 dB |
| 0.01 | 0.5 | 0.0099 | 0.494 | −0.1 dB |
| 0.002 | 0.5 | 0.0019 | 0.503 | −0.3 dB |

Below the clip threshold the round trip is good — bandwidth within a few percent, amplitude
within half a dB. Above it, a half-noisy Partial comes back as a pure sinusoid at the
sine-only amplitude. This is fine for the many-quiet-Partials case Spc was designed around
and wrong for anything with a few loud noisy Partials, which is precisely the case
bandwidth enhancement exists to handle. **Scale amplitudes down before export, or the
enhancement the format is carrying will not be the enhancement that was analyzed.**

**(d) Magnitude resolution is 7 bits over a 112 dB range, and it truncates.**

| code | value | dBFS | step to previous code |
|---|---|---|---|
| 127 | 0.922 | −0.70 | 0.71 dB |
| 96 | 0.0743 | −22.6 | 0.71 dB |
| 64 | 0.00549 | −45.2 | 0.71 dB |
| 32 | 0.000380 | −68.4 | 0.76 dB |
| 16 | 8.14e−5 | −81.8 | 0.99 dB |
| 8 | 2.79e−5 | −91.1 | 1.55 dB |
| 4 | 1.17e−5 | −98.6 | 2.87 dB |
| 2 | 5.38e−6 | −105.4 | 6.38 dB |
| 1 | 2.58e−6 | −111.8 | — |

About 0.7 dB per step over the top 60 dB, widening to 6 dB at the bottom where `envLog`'s
argument goes linear. Because the mask truncates rather than rounds, every amplitude is
biased low by up to a step — full scale 1.0 stores as 0.922. Block synthesis ramps between
consecutive frame values, so a 0.7 dB staircase becomes a piecewise-linear approximation of
the envelope rather than audible steps; the systematic downward bias is the more interesting
part, and it is a constant offset, not a distortion.

**(e) Frequency and phase are not a problem.** Log frequency in a full 16 bits is 0.275
cents per step anywhere in the musical range (3.5 Hz at 22 kHz, where it does not matter);
phase is 2π/65535 ≈ 9.6e−5 radians, well under the 0.01–0.05 radian agreement the block
synthesizer already achieves against the standard engine.

**(f) The stored phase is a block-start phase — which is what the block oscillator wants.**
`pack` writes `phase − 2π·hop·freq + π/2`, and import removes only the `π/2` (§3). At
roughly constant frequency the stored value is the Partial's phase one hop *before* the
frame time. That is exactly the quantity `BlockOscillator::initOnset` constructs by walking
a target phase back one block, so a direct frame reader can use it as-is. But `importSpc`
hands it to `Partial::insert` as the phase *at* the frame time, so Partials imported the
long way carry phases retarded by one hop. That should largely wash out downstream — the
`Resampler`'s phase-correct pass re-derives phases by integrating frequency from the first
breakpoint, collapsing the offset into one constant per phase-reset region — but it has not
been measured, and a test comparing `importSpc` phases against the source Partials' would be
worth having either way.

**(g) Sine-only files are unusable here**, which is why the question presumes enhanced: with
`enhanced = 0` there is no phase at all (§3), and `initOnset` has nothing to align to.

### 7.4 Verdict

**Use it for:** anything at or below 128 Partials where the file is already an Spc, and as
the vehicle for prototyping a frame-streaming path into `BlockSynthBwe` — the format is
close enough to the internal representation that the prototype is small, and it is the
cheapest way to find out what a real fast-synth container should look like.

**Do not use it for:** the large-Partial-count cases that motivate fast synthesis at all, or
as a general fast-synth container. The 128-Partial ceiling and the coupled block size are
not negotiable within the format, and the bandwidth channel needs amplitudes staged into its
headroom to mean anything.

**If a native fast-synthesis format is ever wanted, Spc has the right shape and the wrong
numbers.** Four changes would make it right, none of which Spc can absorb without ceasing to
be readable by Kyma:

1. Stream count as an ordinary header field, not an index into a fixed-length obsolete array.
2. Hop decoupled from the Partial count, and stated in samples at a declared rate.
3. Magnitudes at 8–16 bits, rounded rather than truncated.
4. Bandwidth stored directly (8 bits of a 0…1 fraction is ample) instead of inferred from a
   second, clipping magnitude.

**Concrete next step, if pursued:** a `BlockSynthReader` constructor that takes packed Spc
frames and unpacks straight into `mBpFrames` — no `SpcFile::partials()`, no `Resampler` —
plus a 128-entry magnitude table. That is a bounded piece of work, it makes
`loris-fastsynth`'s existing `.spc` input honest, and it measures the load-time claim in
§7.1 directly. It should wait until `generate_randi` is live (`README.md`), since until then
there is no enhanced output to compare.
