# Adding a New Endpoint Type

This note describes the current implementation path for adding a third endpoint
type to `1tracker_pi`.

The codebase is already structured around endpoint behaviors, so most of the
work is concentrated in one central area, with a few supporting updates in
config loading, policy checks, UI behavior, and tests.

## Recommended Implementation Order

1. Add the new type identifier.
2. Implement a new endpoint behavior.
3. Register the behavior and expose it in the type list.
4. Extend config loading to accept the new type.
5. Review policy helpers for any type-specific rules.
6. Review the tracker dialog for UI-specific behavior.
7. Extend automated tests.

That order keeps the behavior layer authoritative and reduces the chance of
scattering type-specific logic across the codebase.

## Files To Review

### 1. `include/1tracker_pi/endpoint_policy.h`

Primary reason:

- add the new endpoint type constant

Typical change:

```cpp
inline constexpr const char* kEndpointTypeMyNewType = "my_new_type";
```

If the new type needs public helper functions similar to
`isNoForeignLandType(...)`, this is also where those declarations belong.

### 2. `src/endpoint_type_registry.cpp`

This is the main integration point for a new endpoint type.

Add a new `EndpointTypeBehavior` implementation and define:

- `type()`
- `applyDefaults(...)`
- `validate(...)`
- `uiMetadata()`
- `buildPayload(...)`
- `buildRequest(...)`
- `responseIndicatesSuccess(...)`

Also update:

- `findBehavior(...)`
- `listEndpointTypes()`

In practice, this file is the canonical place where a new type becomes real.
If a third type can be added cleanly here, the rest of the system mostly falls
into place.

### 3. `src/config_loader.cpp`

This file currently contains an explicit allowlist of supported endpoint types.

You must update the type check in `parseEndpoint(...)` so that config files
containing the new type are accepted.

If the new type introduces extra root-level or endpoint-level config fields,
this file may also need parsing extensions.

### 4. `src/endpoint_policy.cpp`

Review this file if the new type has any policy-specific rules such as:

- a minimum send interval
- special runtime validation
- type detection helpers
- generated default names

Examples already present:

- `isNoForeignLandType(...)`
- `effectiveSendIntervalMinutes(...)`
- `validateRuntimeConfig(...)`
- `makeNflEndpointName(...)`

If the new type behaves like the generic JSON type, this file may need only a
small change or none at all.

### 5. `src/tracker_dialog.cpp`

The dialog already uses behavior-driven metadata, so a lot of UI work is
automatic once the new behavior is registered.

Still, review these areas carefully:

- endpoint type list population
- type-specific UI metadata application
- type default application in the editor
- detail header artwork
- error presentation

Relevant routines currently include:

- `updateEndpointTypeUi(...)`
- `applyEndpointTypeDefaultsInEditor(...)`
- `updateDetailHeading(...)`
- `loadEndpointControls(...)`

You will likely need dialog changes if the new type:

- hides or shows transport fields differently
- changes header labels or help text
- needs different default interval behavior
- uses different header artwork

### 6. `include/1tracker_pi/endpoint_config.h`

Only review this file if the new endpoint type needs additional persisted
configuration fields beyond the current model:

- `url`
- `timeoutSeconds`
- `headerName`
- `headerValue`
- `includeAwaAws`
- `sendIntervalMinutes`

If the current shape is sufficient, no change is needed here.

### 7. `src/plugin.cpp`

This usually does not need direct changes for a new endpoint type.

Only touch it if:

- config serialization must include new fields
- plugin-level orchestration gains type-specific behavior

In the current architecture, endpoint type behavior should stay out of the
plugin shell whenever possible.

## What Already Works Automatically

Once a new behavior is registered and exposed through `listEndpointTypes()`,
the following parts already pick it up through the shared behavior layer:

- `PayloadBuilder`
- `EndpointSender`
- endpoint validation before sending
- type choice population in the tracker dialog
- most metadata-driven UI labels and visibility rules

That is the main reason to keep the new type centered around
`EndpointTypeBehavior`.

## Tests To Update

At minimum, extend these tests:

### `test/test_endpoint_type_behavior.cpp`

Add coverage for:

- defaults
- validation
- UI metadata
- type list visibility

### `test/test_payload_builder.cpp`

Add payload generation coverage for the new type.

### `test/test_endpoint_sender.cpp`

Add request construction and response interpretation coverage.

### `test/test_config_loader.cpp`

Add config-loading coverage for the new type.

### `test/test_scheduler.cpp`

Only necessary if the new type introduces special interval or scheduling rules.

## Practical Minimal Change Set

For a simple third type, the smallest realistic implementation set is:

1. `include/1tracker_pi/endpoint_policy.h`
2. `src/endpoint_type_registry.cpp`
3. `src/config_loader.cpp`
4. tests for behavior, payload, sender, and config loading

Then review `src/tracker_dialog.cpp` and `src/endpoint_policy.cpp` for any
type-specific UI or policy rules.

## Design Guideline

When adding a new endpoint type, prefer this rule:

- put type-specific behavior in the behavior layer
- keep the plugin shell generic
- keep the scheduler generic
- keep the dialog metadata-driven where possible

If you find yourself adding many `if (type == "...")` checks outside the
behavior and policy layers, that is a signal to stop and recentralize the
logic.
