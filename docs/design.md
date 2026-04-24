# 1tracker_pi Plugin — Technical Design

## Core Functionality

`1tracker_pi` is an OpenCPN plugin that periodically sends the boat position
and optional wind data to configured HTTP tracker endpoints.

From a user perspective, the plugin does four main things:

1. Collect the latest navigation state from OpenCPN.
2. Keep a local list of configured tracker endpoints.
3. Build endpoint-specific payloads from the current state.
4. Send those payloads on a schedule and report status in the UI and toolbar.

The current codebase supports two endpoint types:

- `http_json_with_header_key`
- `noforeignland`

The plugin is designed so that additional endpoint types can be added without
rewriting the scheduler, plugin shell, or state collection logic.

## Key Technical Choices

The current implementation is based on a few deliberate architectural choices.

### Thin OpenCPN shell

The OpenCPN-facing code in `src/plugin.cpp` is intentionally kept relatively
thin. It is responsible for lifecycle integration, toolbar behavior,
preferences/dialog entry points, and configuration orchestration, but most
domain logic lives in regular C++ classes.

### Behavior-driven endpoint types

Endpoint-specific behavior is centralized behind `EndpointTypeBehavior`.

That means endpoint types define:

- defaults
- validation
- UI metadata
- payload generation
- request generation
- response-success interpretation

This keeps the rest of the code generic and makes future endpoint types much
easier to add.

### Worker-thread scheduling

Sending happens in a worker thread, not in the UI thread. The scheduler decides
when each endpoint is due, fetches the current snapshot, builds the payload,
sends it, and reports the result back.

### Local JSON configuration

Configuration is stored as JSON and loaded into a runtime model. Missing config
files are auto-created, and development mode can fall back to an example config
when no private OpenCPN data directory is available.

### Shared tracker/preferences dialog

The plugin uses one shared dialog for tracker management, help/info, and plugin
preferences entry. Different entry modes open the same dialog in different
starting states, which avoids duplicated text and duplicated UI maintenance.

## Codebase Component Map

This is the recommended way to understand the maintainable code in this repo.
The list below maps high-level responsibilities directly to recognizable files.

### 1. Plugin Shell

Files:

- `src/plugin.cpp`
- `src/plugin.h`

Responsibilities:

- OpenCPN lifecycle hooks
- toolbar registration and updates
- preferences entry point
- orchestration of config, scheduler, and endpoint status
- integration between OpenCPN callbacks and the internal components

This is the top-level coordinator of the plugin.

### 2. Configuration Loading And Persistence

Files:

- `src/config_loader.cpp`
- `src/atomic_file_writer.cpp`
- `include/1tracker_pi/atomic_file_writer.h`
- config save logic in `src/plugin.cpp`

Responsibilities:

- parse `config.json`
- convert JSON into `RuntimeConfig`
- load defaults and normalize values
- write the current runtime config back to disk atomically
  (temp file + `std::filesystem::rename`, so a crash mid-write never leaves
  a half-written config)

This component owns the on-disk configuration format.

### 3. Endpoint Model And Shared Policy

Files:

- `include/1tracker_pi/endpoint_config.h`
- `include/1tracker_pi/runtime_config.h`
- `src/endpoint_policy.cpp`
- `include/1tracker_pi/endpoint_policy.h`

Responsibilities:

- endpoint and runtime config data model
- stable endpoint IDs
- shared normalization rules
- effective send interval calculation
- runtime-level validation helpers

This layer contains shared endpoint rules that are not specific to just one
endpoint type implementation.

### 4. Endpoint Type Behavior

Files:

- `include/1tracker_pi/endpoint_type_behavior.h`
- `src/endpoint_type_registry.cpp`

Responsibilities:

- register supported endpoint types
- apply per-type defaults
- validate per-type configuration for sending
- expose per-type UI metadata
- build per-type payloads
- build per-type requests
- interpret per-type responses

This is the primary extension point for adding a third endpoint type.

### 5. State Collection

Files:

- `src/state_store.cpp`
- `include/1tracker_pi/state_store.h`
- snapshot model headers

Responsibilities:

- store the latest time, position, and wind values
- provide a thread-safe snapshot for sending

This component is intentionally small and generic.

### 6. Payload And Transport

Files:

- `src/payload_builder.cpp`
- `src/endpoint_sender.cpp`

Responsibilities:

- ask the active endpoint behavior for a payload
- send prepared requests with `wxcurl`
- handle transport timeouts and response details
- redact secrets in returned messages

This is the outbound data path after a snapshot is available.

### 7. Scheduling

Files:

- `src/scheduler.cpp`
- `include/1tracker_pi/scheduler.h`

Responsibilities:

- manage the worker-thread loop
- determine when endpoints are due
- fetch a snapshot
- trigger payload building and sending
- report results
- schedule the next send time per endpoint

This component is the runtime engine of periodic sending.

### 8. Tracker Management Dialog

Files:

- `src/tracker_dialog.cpp`
- `src/tracker_dialog.h`
- `src/endpoint_error_summary.cpp`
- `include/1tracker_pi/endpoint_error_summary.h`

Responsibilities:

- tracker list screen
- tracker detail editor
- info/help screen
- preferences entry mode
- endpoint-type-aware UI updates
- translate raw transport errors into short user-facing messages
- decide what the error panel should show for a given endpoint (pure
  `computeEndpointErrorUiState` helper, testable without wx)

This is the main maintainable UI surface of the plugin. The pure
error-summary helpers live in the core library so they can be unit-tested
without wxWidgets.

### 9. UI Utilities And Asset Handling

Files:

- `src/plugin_ui_utils.cpp`
- `src/plugin_ui_utils.h`

Responsibilities:

- locate plugin assets
- load bitmaps and SVGs
- tint bitmaps
- support toolbar and dialog artwork

This component keeps file/path/bitmap mechanics out of the main plugin and
dialog logic.

Assets live in `data/icons/`. Despite the folder name, it holds both toolbar
icons (SVG) and branding/header artwork (PNG). The toolbar icon flow is
described in more detail under "Toolbar Status And Icons" below.

### 10. Tests

Files:

- `test/*`

Responsibilities:

- verify config loading
- verify payload building
- verify endpoint sending (happy path, NFL specifics, connection-refused, secret redaction)
- verify scheduler behavior (intervals, min-distance, enabled/disabled, log hooks)
- verify endpoint identity rules
- verify endpoint type behavior
- verify secret masking
- verify state-store input validation (NaN, range bounds for lat/lon/wind)
- verify the pure error-summary translation and UI-state computation
- verify atomic file writes (happy path, overwrite, binary content, failure paths)

The tests focus on the maintainable core outside the OpenCPN runtime.

Line coverage for the core library (tested `src/` files only) is tracked via
`ci/run-coverage.sh`, which produces an HTML report under
`build-coverage/html/`. As of the last refactor it sits at ~85%.

## Component Interaction

The main runtime interaction looks like this:

```text
OpenCPN input
    ↓
Plugin Shell
    ↓
StateStore
    ↓
Scheduler
    ↓
PayloadBuilder
    ↓
EndpointSender
    ↓
EndpointTypeBehavior
    ↓
HTTP endpoint
```

The configuration and UI interaction looks like this:

```text
Plugin Shell
    ↓
Tracker Dialog
    ↓
RuntimeConfig / EndpointConfig
    ↓
Config save + applyRuntimeConfig()
    ↓
Scheduler + toolbar status update
```

## Data Model

Runtime config:

```text
RuntimeConfig
 ├── enabled
 └── endpoints[]
```

Endpoint config:

```text
EndpointConfig
 ├── id
 ├── name
 ├── type
 ├── enabled
 ├── includeAwaAws
 ├── sendIntervalMinutes
 ├── minDistanceMeters
 ├── url
 ├── timeoutSeconds
 ├── headerName
 └── headerValue
```

Snapshot:

```text
Snapshot
 ├── timevalue
 ├── lat
 ├── lon
 ├── awa
 └── aws
```

## Endpoint Types

### `http_json_with_header_key`

Generic HTTP endpoint with:

- JSON request body
- freely configurable URL
- configurable header name and header value
- optional `AWA` and `AWS`
- flexible per-tracker interval

This type is intended for custom endpoints such as your own websites or APIs.

### `noforeignland`

Special endpoint type for noforeignland with fixed transport rules:

- fixed tracking URL
- fixed REST API header name
- fixed plugin-wide REST API key
- per-tracker boat key via `headerValue`
- minimum interval of 10 minutes
- `AWA` and `AWS` disabled

The UI also treats this type differently:

- dedicated header artwork
- custom hints/tooltips
- type-specific defaults

## Config File

Runtime path on macOS:

```text
~/Library/Preferences/opencpn/plugins/1tracker_pi/config.json
```

For development, an example file lives at:

```text
config/1tracker.example.json
```

Example:

```json
{
  "enabled": true,
  "endpoints": [
    {
      "name": "MyPersonalSite",
      "type": "http_json_with_header_key",
      "enabled": true,
      "includeAwaAws": true,
      "sendIntervalMinutes": 10,
      "url": "https://example.com/api/newpositions.php",
      "timeoutSeconds": 10,
      "headerName": "Authorization",
      "headerValue": "Bearer YOUR_SECRET"
    },
    {
      "name": "noforeignland",
      "type": "noforeignland",
      "enabled": true,
      "includeAwaAws": false,
      "sendIntervalMinutes": 15,
      "url": "https://www.noforeignland.com/api/v1/boat/tracking/track",
      "timeoutSeconds": 10,
      "headerName": "X-NFL-API-Key",
      "headerValue": "YOUR_BOAT_API_KEY"
    }
  ]
}
```

## Toolbar Status And Icons

The plugin has three toolbar states:

- neutral / inactive
- green / active
- red / issue

The source is a single SVG template in the plugin data directory. At runtime
the plugin substitutes color tokens in the template string in memory and
renders each variant via `wxBitmapBundle::FromSVG`. No files are written or
cached on disk.

Toolbar status is derived from the known endpoint results:

- red if at least one enabled endpoint fails
- green if enabled endpoints are successful
- neutral if there is no successful or definitive status yet

### Packaging

Every release package must include:

- the plugin binary (`1tracker_pi.dylib` / `1tracker_pi.dll` /
  `lib1tracker_pi.so`)
- `data/icons/1tracker_toolbar_icon.svg` — the template SVG, containing the
  placeholder tokens `__MARKER_FILL__`, `__MARKER_STROKE__`, `__BOAT_FILL__`

Install paths per platform:

- macOS: `Contents/SharedSupport/plugins/1tracker_pi/data/icons/`
- Linux / Windows / Android: `share/opencpn/plugins/1tracker_pi/data/icons/`

### Diagnostics

If the toolbar button is missing or the status colors are wrong:

1. Confirm the template is deployed:
   - macOS: `~/Library/Application Support/OpenCPN/Contents/SharedSupport/plugins/1tracker_pi/data/icons/1tracker_toolbar_icon.svg`
   - Windows: `%LOCALAPPDATA%\opencpn\plugins\1tracker_pi\data\icons\1tracker_toolbar_icon.svg`
2. In `opencpn.log`, look for these plugin log lines:
   - `1tracker_pi: loaded toolbar template from <path> (<N> bytes)` — template
     load succeeded.
   - `1tracker_pi: toolbar template not found for 1tracker_toolbar_icon.svg` —
     asset missing from the install.
   - `1tracker_pi: toolbar tool id=<N>, template_bytes=<N>` — `InsertPlugInTool`
     was called. An id of `-1` means registration failed.
3. Verify the template still contains the three placeholder tokens
   (`__MARKER_FILL__`, `__MARKER_STROKE__`, `__BOAT_FILL__`). Without them
   the variants all render identically to the template's literal colors.

## Thread Model

Design principle:

- no network traffic in the UI thread

Model:

```text
UI thread
 ├── OpenCPN callbacks
 ├── dialog and toolbar interaction
 └── status updates toward the UI

Worker thread
 ├── scheduler loop
 └── endpoint sending
```

`StateStore`, scheduler config, and endpoint status therefore need to be safe
under concurrent access.

All wxWidgets UI calls are done on the UI thread. When the scheduler's
worker thread wants to refresh the toolbar icon after a send finishes, it
posts the work to the UI thread via `wxTheApp->CallAfter(...)` rather than
calling `SetToolbarToolBitmaps` directly — on GTK/Linux a cross-thread wx
call would crash the host.

## Logging And Error Handling

The plugin logs, among other things:

- startup and shutdown
- config loading
- endpoint results
- toolbar icon paths and state transitions
- validation and send failures

Important design choices:

- network failures do not stop the plugin
- a failure in one endpoint does not block other endpoints
- a missing config file is automatically recovered
- secrets are masked in log output where possible
- `Init()` wraps its entire body in try/catch so no plugin exception can
  escape to OpenCPN's event loop (a thrown init used to crash the host)
- config writes are atomic (temp file + rename) so a crash mid-save never
  corrupts the user's endpoints
- `StateStore` rejects NaN / out-of-range lat, lon, AWA, and AWS values so
  a buggy upstream source can't propagate garbage into the payload

## Test Strategy

The test suite covers core components outside OpenCPN, including:

- config parsing and validation
- payload construction
- endpoint transport
- scheduler behavior
- secret masking
- state store behavior
- endpoint identity
- endpoint type behavior

Mock HTTP servers are used for request, response, and timeout scenarios.

## Extensibility

The current architecture is intended to support future changes such as:

- additional endpoint types
- additional payload formats
- additional UI validation
- additional plugin preferences

For new endpoint types, the preferred rule is:

- add behavior in `EndpointTypeBehavior`
- register it in `endpoint_type_registry.cpp`
- extend config loading only where needed
- keep the plugin shell and scheduler generic

That keeps future maintenance localized and prevents type-specific logic from
spreading through the whole codebase.

## Translations

The plugin uses OpenCPN's standard `.po`/`.mo` localization pipeline
(cmake/PluginLocalization.cmake). Workflow:

1. **Marking strings.** User-facing strings in UI code (`src/tracker_dialog.cpp`,
   `src/nfl_endpoint_page.cpp`, `src/http_json_endpoint_page.cpp`,
   `src/endpoint_type_picker.cpp`) are wrapped with wxWidgets' `_()` macro.
   Strings that live in the wx-free core library
   (`src/endpoint_error_summary.cpp`) can't use `_()` — they are marked with
   `TR_NOOP(...)` (see `include/1tracker_pi/translation_markers.h`) which
   is a runtime no-op but is recognised by xgettext. The UI call site
   passes the returned string through `wxGetTranslation(...)` at display
   time so the localised text is looked up in the compiled `.mo` catalog.

2. **Source file list.** Any file containing marked strings must appear in
   `po/POTFILES.in`.

3. **Regenerating the template.** Running
   ```
   cmake --build build --target 1tracker-pot-update
   ```
   (available when `BUILD_TYPE` is set in the CMake configure, i.e. the
   packaging configuration) re-extracts strings into
   `po/1tracker_pi.pot`. Committed alongside the source for convenience.
   The `xgettext` invocation in `cmake/PluginLocalization.cmake` uses
   `--keyword=_ --keyword=TR_NOOP`.

4. **Per-language catalogs.** `.po` files come from the OpenCPN
   [Crowdin project](https://opencpn-manuals.github.io/main/ocpn-dev-manual/0.1/dm-i18n.html),
   where community translators pick up the `.pot` and produce per-locale
   `.po` files that flow back via pull request from the OpenCPN i18n
   maintainers. This plugin therefore does not carry hand-maintained
   `.po` files — they are updated via that PR loop.

5. **Build product.** Each `.po` compiles to a `.mo` at build time and
   is installed under the appropriate OpenCPN locale path
   (`Resources/<locale>.lproj/` on macOS, `share/locale/<locale>/LC_MESSAGES/`
   on Linux). OpenCPN loads them at runtime in addition to its own core
   catalogs; if both the core and the plugin translate the same source
   string, the core translation wins.
