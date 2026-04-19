# Changelog

All notable changes to `1tracker_pi` are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- OpenCPN plugin catalog packaging (CMake modules, per-target metadata, CI).

## [0.1.0] - YYYY-MM-DD

### Added
- Periodic sending of position and apparent wind to HTTP endpoints.
- Endpoint type `http_json_with_header_key` for custom JSON endpoints.
- Endpoint type `noforeignland` with its fixed transport rules.
- Per-tracker configuration: name, URL, API key header, interval, enable flag,
  and optional inclusion of apparent wind angle and speed.
- Toolbar button with neutral / green / red status.
- Tracker management dialog opened from the toolbar, and a preferences entry
  point reusing the same dialog.
- Automatic creation of a default `config.json` on first install.
- Secret masking in log output for configured header values.
- Automated unit tests for config, payload, scheduler, sender, endpoint
  identity and behavior, state store, and secret masking.
- Single-source plugin metadata generated from CMake
  (`plugin_metadata.h.in`, `version.h.in`, `plugin.xml.in`).
