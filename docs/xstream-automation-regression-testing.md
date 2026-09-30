# XStream automation for end-to-end regression testing

Date: 2026-09-30

## Conclusion

The LeCroy XStream automation interface is suitable for automated end-to-end
regression testing of the x64 replacement driver.

This is substantially more useful than only automating GUI input. XStream exposes
its oscilloscope object model through Microsoft COM automation. Tests can therefore
set acquisition controls, start acquisitions, read settings back, obtain measurement
results and retrieve waveform sample arrays.

The intended use in this project is NOT to replace low-level ABI/IOCTL tests.
Instead, XStream automation should provide the highest regression layer:

1. low-level driver/IOCTL regression tests;
2. real-hardware driver tests;
3. XStream end-to-end tests through the same user-mode application used in normal
   operation.

This third layer is capable of validating observable oscilloscope behavior without
requiring manual GUI interaction for every driver change.

## Relevant LeCroy interfaces

LeCroy documents the XStream COM automation object as the native control interface
of Windows-based XStream oscilloscopes.

A local automation client can create/connect to the application using the
`LeCroy.XStreamDSO` / `LeCroy.XStreamDSO.1` ProgID. The XStream Browser installed
with the oscilloscope exposes the object tree available on the exact instrument
configuration and is therefore the authoritative source for legacy model-specific
properties and optional features.

Examples documented by LeCroy include:

- `app.Acquisition.C1.VerScale` for volts/div;
- `app.Acquisition.C1.Coupling` for input coupling;
- `app.Horizontal.HorScale` for time/div;
- `app.Acquisition.C1.Out.Result.Samples` for waveform sample count;
- `app.Acquisition.C1.Out.Result.DataArray` for waveform samples;
- `app.Measure.P1.Out.Result.Value` for a measurement result;
- acquisition methods capable of starting/waiting for an acquisition.

XStream automation can also be sent through the normal remote-control `VBS`
command. ActiveDSO can communicate with the local instrument through
`IP:127.0.0.1`, providing another possible transport if direct COM activation is
awkward on the migrated x64 Windows installation.

## What this lets us test

### Startup / basic application health

An automated test can:

- start XStream;
- wait until its automation server is reachable;
- read identity/basic properties;
- fail on timeout or COM errors.

This detects crashes, startup hangs and severe driver initialization regressions.

### Acquisition

A test can configure a known channel and acquisition state, trigger an acquisition
and verify that XStream produces a fresh waveform.

Useful assertions include:

- non-zero sample count;
- a finite/non-empty `DataArray`;
- changing acquisition/event state between successive captures;
- expected horizontal and vertical metadata;
- no acquisition timeout.

This is much stronger than merely checking that the XStream process remains alive.

### Vertical controls

Automation can set/read channel controls such as vertical scale, coupling and
offset. We can therefore write tests such as:

1. set C1 to a known V/div;
2. read it back through XStream;
3. acquire;
4. verify that the returned waveform remains valid.

This exercises XStream -> legacy user-mode stack -> replacement driver -> hardware,
rather than directly calling our driver test API.

### Horizontal controls

The same applies to timebase. A test can change horizontal scale, acquire and
validate that returned waveform metadata/sample timing changes consistently.

### Trigger path

The object tree exposes trigger controls. Once the exact WaveRunner 6000 automation
paths are captured from XStream Browser, tests can configure a stable trigger,
perform an acquisition and detect timeout/no-waveform regressions.

### Waveform correctness

Waveform samples are directly exposed through
`app.Acquisition.C1.Out.Result.DataArray`.

For a deterministic input signal this allows numeric assertions instead of
screenshots. For example:

- sample count within expected range;
- peak-to-peak amplitude within tolerance;
- mean/offset within tolerance;
- measured frequency within tolerance;
- no NaN/invalid result;
- waveform differs from an all-zero/stale buffer.

LeCroy measurement parameters can also be configured and read through
`app.Measure.P1.Out.Result.Value`, so amplitude/frequency checks do not necessarily
require implementing DSP in the regression harness.

### Multi-channel and sample-rate configurations

Known-good configurations such as two-channel / 10 GS/s operation can be programmed
and validated. This is particularly useful because these modes exercise acquisition
configuration that a trivial single-channel smoke test does not.

### ProBus

Automation is promising for the XStream-visible part of ProBus regression:
whether XStream exposes the attached probe, probe-related settings, warnings and
possibly probe metadata can be queried if those nodes exist in the legacy
WaveRunner's automation object tree.

The exact object paths MUST be discovered on the actual instrument with XStream
Browser before implementing these assertions. Do not assume modern MAUI object
paths are present unchanged.

Physical plug/unplug remains partly an external stimulus. Automation can verify the
result after the user or a future fixture performs the physical action.

## What it cannot prove by itself

An XStream automation PASS proves externally visible application behavior, not exact
driver-internal equivalence.

It does not replace:

- exact IOCTL buffer-size/error-status regression;
- low-level register ABI tests;
- interrupt/DPC-specific diagnostics;
- differential original-driver IOCTL testing;
- safety checks around currently unknown write paths.

Also, a property readback alone is insufficient. XStream may cache a requested
setting. Important tests should combine control readback with an actual acquisition
and waveform/measurement validation where possible.

## Recommended harness architecture

Suggested repository layout:

```text
tests/
  abi/
  hardware/
  xstream/
    smoke.ps1
    acquisition.ps1
    vertical.ps1
    horizontal.ps1
    trigger.ps1
    multichannel.ps1
    probus.ps1
```

The first XStream harness should be intentionally small.

### Stage 1: automation connectivity probe

With XStream already running:

1. connect to the local XStream automation server;
2. read C1 vertical scale;
3. read horizontal scale;
4. read C1 waveform sample count;
5. return a clear PASS/FAIL result.

This establishes whether direct COM works on the current Windows x64 installation.

If direct COM activation has a bitness/registration issue, test ActiveDSO against
`127.0.0.1` and send/query XStream automation through `VBS` before changing the
test design.

### Stage 2: deterministic acquisition smoke test

Use the scope's known signal source or another stable generator:

1. configure C1;
2. configure timebase and trigger;
3. perform/wait for one acquisition;
4. verify non-zero samples;
5. verify amplitude/frequency inside generous tolerances.

### Stage 3: known working feature matrix

Encode the currently verified x64 working baseline:

- live waveform acquisition;
- amplitude/frequency;
- V/div;
- timebase;
- coupling;
- bandwidth;
- trigger;
- two-channel / 10 GS/s;
- AP015 pre-attached recognition.

Each feature should report independently so one failure does not hide the rest.

### Stage 4: ProBus scenarios

After identifying legacy automation nodes in XStream Browser:

- probe present at XStream startup;
- probe identity/type;
- unlocked-jaw warning if exposed;
- disconnect/reconnect state if XStream actually refreshes it.

The already observed fact that removing an AP015 while XStream is running was not
noticed must be treated as current baseline behavior until a better understood path
is established. A regression harness must not fail merely because a historical
XStream behavior is undesirable.

## CI terminology

These tests are regression tests. They become part of CI when a machine with the
actual oscilloscope hardware executes them automatically for a commit/build.

Ordinary GitHub-hosted runners cannot execute the hardware/XStream layers. A future
self-hosted runner on the scope could do so, but automatic driver installation or
arbitrary hardware-write tests should not be enabled casually on the only working
instrument.

A safer initial workflow is:

1. GitHub CI: build/static/unit/ABI tests that do not require hardware;
2. scope: one command runs the hardware + XStream regression suite;
3. the suite emits machine-readable JSON/JUnit plus a human summary.

Later, once the driver and tests are mature, the scope can be considered as a
self-hosted hardware-in-the-loop runner.

## Implemented first XStream E2E layer

The repository now contains:

    tests/xstream/test-xstream-e2e.ps1

and the unified runner accepts:

    .\scripts\test-driver.ps1 -Mode XStream
    .\scripts\test-driver.ps1 -Mode All

The suite first tries local COM activation through `LeCroy.XStreamDSO`, then
`LeCroy.XStreamDSO.1`. The current implementation performs the following default
checks:

1. COM automation connection;
2. Acquisition/C1/Horizontal/result object access;
3. positive C1 vertical scale and horizontal scale readback;
4. forced-trigger `Acquisition.Acquire(timeout, 1)`;
5. non-zero C1 waveform sample count;
6. non-empty C1 DataArray containing finite numeric values;
7. C1 vertical-scale change/readback/acquire/restore;
8. horizontal timebase change/readback/acquire/restore;
9. C1 coupling change/readback/acquire/restore;
10. C1 bandwidth-limit change/readback/acquire/restore.

The state-changing tests restore the prior values in `finally` blocks. They can be
disabled with `-SkipXStreamControlChanges`.

Three optional assertions can be enabled from the unified runner:

    .\scripts\test-driver.ps1 -Mode XStream -ExpectedProbeName AP015
    .\scripts\test-driver.ps1 -Mode XStream -ExpectedAmplitudeVpp 1.0
    .\scripts\test-driver.ps1 -Mode XStream -ExpectedFrequencyHz 1000

The probe assertion reads the legacy XStream `Acquisition.C1.ProbeName` property.
Amplitude/frequency assertions temporarily configure P1 for C1 and restore its
previous View/ParamEngine/Source1 settings afterwards.

The suite is source-side implemented but NOT yet owner-run on the scope. Do not
claim an XStream E2E PASS until actual output is supplied.

## Remaining paths to capture from the WaveRunner 6000 XStream Browser

Do not guess these legacy model-specific paths. Capture the exact automation paths
and current values from the actual instrument for:

- trigger source/type/slope/level and trigger mode;
- channel enable/view controls needed for an explicit two-channel regression;
- sample-rate/interleaving or acquisition-mode controls needed to assert 10 GS/s;
- AP015/ProBus jaw/open/closed state or warning state;
- any explicit probe hotplug-refresh state exposed by this XStream build.

Once these are known they can be added as independent E2E checks without weakening
the already working generic COM/acquisition layer.

## References

- Teledyne LeCroy, *X-Stream Automation Manual*.
- Teledyne LeCroy, *Ten Minute Tutorial - XStream Browser*.
- Teledyne LeCroy, *Using Python with ActiveDSO for Remote Communication*.
- Teledyne LeCroy, XStream COM object programming documentation.
