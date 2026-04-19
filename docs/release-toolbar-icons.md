# Toolbar Icons

The `1tracker_pi` toolbar button uses one source SVG plus runtime-generated
color variants. OpenCPN caches rendered toolbar SVGs aggressively by filename.
Because of that, the plugin now uses cache-busting filenames for runtime icons,
and the only reliable local workflow is captured in a dedicated dev target.

## Required In A Release

The package/install must include these files:

- `Contents/PlugIns/1tracker_pi.dylib`
- `Contents/SharedSupport/plugins/1tracker_pi/data/1tracker_icon.svg`

On non-macOS platforms, the icon files are installed under:

- `share/opencpn/plugins/1tracker_pi/data/`

## Why This Matters

OpenCPN does register the toolbar tool when the plugin is loaded, but in
practice the SVG toolbar icons are expected to exist in the plugin data
directory. If those files are not packaged or deployed, the extra toolbar
button may remain invisible.

## CMake support

The CMake configuration now installs automatically:

- the plugin library via `install(TARGETS 1tracker_pi ...)`
- the source toolbar icon via `install(FILES ...)`

## Only Reliable Dev Flow

On macOS, always use:

```sh
cmake --build build-wx32 --target refresh_toolbar_icons_dev
```

This target performs, in a fixed order, everything that is required:

- builds and deploys `1tracker_pi.dylib`
- deploys `1tracker_icon.svg` and `1tracker_toolbar_icon.svg`
- removes the runtime toolbar files:
  - `~/Library/Preferences/opencpn/plugins/1tracker_pi/toolbar_icon_*.svg`
- removes the matching OpenCPN SVG cache files in:
  - `~/Library/Preferences/opencpn/iconCacheSVG/`

After that, only one manual step remains:

1. Restart OpenCPN.

After the restart, the plugin regenerates runtime files such as:

- `toolbar_icon_neutral_<cache-bust>.svg`
- `toolbar_icon_green_<cache-bust>.svg`
- `toolbar_icon_red_<cache-bust>.svg`

This flow is the standard. Do not use `deploy_opencpn_dev` by itself for
toolbar icon iterations, because runtime SVGs or cache files can easily be left
behind.

## Why This Is Needed

There are three layers that can get out of sync:

- the source file in `Contents/SharedSupport/plugins/1tracker_pi/data/1tracker_toolbar_icon.svg`
- the runtime files in `~/Library/Preferences/opencpn/plugins/1tracker_pi/`
- the rendered cache files in `~/Library/Preferences/opencpn/iconCacheSVG/`

A normal build or deploy does not refresh all three layers automatically.
In practice, OpenCPN can also keep showing an old PNG cache while the runtime
SVG is already new. That is why the plugin now uses a cache-bust token in the
runtime filename, so a changed source SVG always leads to a new cache key.

## Diagnostics

If you want to check what is really happening, inspect things in this order:

1. Is the updated source SVG really here:
   - `~/Library/Application Support/OpenCPN/Contents/SharedSupport/plugins/1tracker_pi/data/1tracker_toolbar_icon.svg`
2. Does the plugin rewrite this on startup:
   - `~/Library/Preferences/opencpn/plugins/1tracker_pi/toolbar_icon_neutral.svg`
3. Does the log line in `~/Library/Logs/opencpn.log` point at that runtime file:

Relevant log line in `~/Library/Logs/opencpn.log`:

```text
1tracker_pi: toolbar icon=...
```

If that log line points to
`~/Library/Preferences/opencpn/plugins/1tracker_pi/toolbar_icon_neutral.svg`
and the contents of that file are correct, but the UI still shows something
else, then the OpenCPN SVG cache is the cause.

## Release

For a release, both the binary and the icon assets still need to be included:

- `Contents/PlugIns/1tracker_pi.dylib`
- `Contents/SharedSupport/plugins/1tracker_pi/data/1tracker_icon.svg`
- `Contents/SharedSupport/plugins/1tracker_pi/data/1tracker_toolbar_icon.svg`

Important: the plugin generates runtime color variants from this one source
file for toolbar status:

- neutral
- green when all enabled trackers are successful
- red when at least one enabled tracker fails
