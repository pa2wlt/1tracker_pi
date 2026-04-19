# 1tracker_pi

OpenCPN plugin for periodically exporting position and wind data to HTTP
endpoints.

License: `GPL-2.0-or-later`

## Build

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Metrics

Show the top routines for this repository using the shared `mymetrics` toolkit:

```sh
./scripts/top_routines.sh --top 10 --format md
```

The build uses `opencpn-libs` for the OpenCPN plugin API, JSON support, and
`wxcurl` HTTP transport.

## Status

This repository now contains:

- a buildable OpenCPN plugin
- testable core classes for config, state, payload generation, scheduling, and
  endpoint transport
- a working HTTP POST sender using `wxcurl`
- a payload format compatible with the current `addPositions` server contract
