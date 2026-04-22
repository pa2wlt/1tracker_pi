# 1tracker_pi

OpenCPN plugin for periodically exporting position and wind data to HTTP
endpoints.

License: `GPL-2.0-or-later`

## Build & Test

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build/test --output-on-failure
```

Tests live under `test/` and are linked against the static `onetracker_core`
library, so they don't require OpenCPN or wxWidgets' UI layer.

## Coverage

```sh
./ci/run-coverage.sh
open build-coverage/html/index.html
```

Builds a separate `build-coverage/` tree with clang source-based
instrumentation, runs every test binary, merges profiles, and emits an HTML
report. Line coverage for the core library currently sits at ~85%.

## Metrics

Show the top routines for this repository using the shared `mymetrics` toolkit:

```sh
./scripts/top_routines.sh --top 10 --format md
```

The build uses `opencpn-libs` for the OpenCPN plugin API, JSON support, and
`wxcurl` HTTP transport.

## Status

This repository contains:

- a buildable OpenCPN plugin for macOS, Windows, Linux (Debian 11/12/13),
  Android (arm64/armhf), and Flatpak (22.08/24.08/25.08)
- testable core classes for config, state, payload generation, scheduling,
  endpoint transport, atomic file writes, and error-summary translation
- a working HTTP POST sender using `wxcurl`
- a payload format compatible with the current `addPositions` server contract
- CI that routes releases by git tag (beta/rc → beta repo, other tags → prod,
  no tag → alpha) and an auto-generated `ocpn-plugins.xml` catalog per repo

Beta tester install instructions live in [docs/installation.md](docs/installation.md).
Architecture overview in [docs/design.md](docs/design.md).
