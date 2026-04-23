# 1tracker_pi — Beta Tester Installation Guide

Thank you for testing **1tracker_pi**, an OpenCPN plugin that sends your boat
position (and optionally apparent wind) to one or more tracking services such
as NoForeignLand or your own HTTP server.

This guide covers the **beta-only install procedure** (installing a downloaded
`.tar.gz` by hand). Once the plugin is in the OpenCPN Plugin Master, the regular
install procedure will differ.

For everything *after* install — configuring trackers, field reference,
troubleshooting, configuration-file location — see the
[full user manual](manual.md).

---

## Requirements

- OpenCPN **5.6 or newer**
- Internet connection during installation

---

## Step 1 — Download the plugin

Pick the build that matches your operating system:

| Operating system | Version | Download |
|---|---|---|
| **macOS** (Apple Silicon & Intel) | v0.1.0-beta1 | [1tracker-v0.1.0-beta1_darwin-universal.tar.gz](https://dl.cloudsmith.io/public/pa2wlt/1tracker-beta/raw/names/1tracker-v0.1.0-beta1-darwin-wx32-universal-10.15-tarball/versions/v0.1.0-beta1/1tracker-v0.1.0-beta1_darwin-wx32-10.15-arm64-x86_64.tar.gz) |
| **Windows** | **v0.1.0-beta7** | [1tracker-v0.1.0-beta7_windows.tar.gz](https://dl.cloudsmith.io/public/pa2wlt/1tracker-beta/raw/names/1tracker-v0.1.0-beta7-msvc-wx32-10-tarball/versions/v0.1.0-beta7/1tracker-v0.1.0-beta7_msvc-wx32-10-x86.tar.gz) |
| **Linux** (Debian/Ubuntu 64-bit) | v0.1.0-beta1 | [1tracker-v0.1.0-beta1_debian12-x86_64.tar.gz](https://dl.cloudsmith.io/public/pa2wlt/1tracker-beta/raw/names/1tracker-v0.1.0-beta1-debian-x86_64-12-tarball/versions/v0.1.0-beta1/1tracker-v0.1.0-beta1_debian-x86_64-12-x86_64.tar.gz) |
| **Android** (64-bit, most phones) | v0.1.0-beta1 | [1tracker-v0.1.0-beta1_android-arm64.tar.gz](https://dl.cloudsmith.io/public/pa2wlt/1tracker-beta/raw/names/1tracker-v0.1.0-beta1-android-arm64-A64-16-tarball/versions/v0.1.0-beta1/1tracker-v0.1.0-beta1_android-arm64-16-arm64.tar.gz) |
| **Android** (32-bit, older phones) | v0.1.0-beta1 | [1tracker-v0.1.0-beta1_android-armhf.tar.gz](https://dl.cloudsmith.io/public/pa2wlt/1tracker-beta/raw/names/1tracker-v0.1.0-beta1-android-armhf-A32-16-tarball/versions/v0.1.0-beta1/1tracker-v0.1.0-beta1_android-armhf-16-armhf.tar.gz) |

---

## Step 2 — Install the plugin

1. Open OpenCPN
2. Go to **Options** → **Plugins** tab
3. Click **Install plugin from file**
4. Select the downloaded `.tar.gz` file
5. Restart OpenCPN if prompted (on macOS you may not be asked)

---

## Step 3 — Enable the plugin

1. Go back to **Options** → **Plugins**
2. Find **1tracker** in the list and click **Enable**
3. Click **OK** and restart OpenCPN

After restarting, a new **1tracker** icon appears in the toolbar.

---

## Step 4 — Configure for NoForeignLand

To get started quickly, add one NoForeignLand tracker:

1. Get your **Boat API key** from NoForeignLand — log in at
   [noforeignland.com](https://www.noforeignland.com/) and open
   [Boat Tracking Settings](https://www.noforeignland.com/map/settings/boat/tracking/api)
2. Click the **1tracker** toolbar icon to open the dialog
3. Click **Add tracker**, then the gear icon to open the detail editor
4. Set **Type** to `noforeignland`, paste the boat API key into
   **My NFL boat key**, tick **Enabled**, click **OK**

The toolbar icon turns green once the first position is sent.

### Toolbar icon at a glance

| Color | Meaning |
|---|---|
| ⚪ Grey | Plugin enabled but no GPS fix yet, or all trackers disabled |
| 🟢 Green | At least one tracker is sending successfully |
| 🔴 Red | At least one active tracker has an error |

---

## Step 5 — Add another tracker (optional)

You can add multiple trackers — each runs on its own schedule with its own
type, URL and credentials. Typical uses:

- A **second NoForeignLand boat** (tender, shared account)
- Your **own server** or a third-party service that accepts JSON over HTTP
- A **test endpoint** while you are experimenting

Click **Add tracker** again and pick either `noforeignland` or
`http_json_with_header_key`. Full field descriptions for each type are in the
[full user manual](manual.md#adding-a-tracker).

---

## Issues or feedback?

Please report bugs and comments on GitHub:
[github.com/pa2wlt/1tracker_pi/issues](https://github.com/pa2wlt/1tracker_pi/issues)

Or contact the developer directly.

---

*Last updated: 2026-04-23 — Windows at v0.1.0-beta7, other platforms at v0.1.0-beta1*
