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
| **macOS** (Apple Silicon & Intel) | v0.9.1-beta12 | [1tracker-v0.9.1-beta12_darwin-universal.tar.gz](https://dl.cloudsmith.io/public/pa2wlt/1tracker-beta/raw/names/1tracker-v0.9.1-beta12-darwin-wx32-universal-10.15-tarball/versions/v0.9.1-beta12/1tracker-v0.9.1-beta12_darwin-wx32-10.15-arm64-x86_64.tar.gz) |
| **Windows** | v0.9.1-beta12 | [1tracker-v0.9.1-beta12_windows.tar.gz](https://dl.cloudsmith.io/public/pa2wlt/1tracker-beta/raw/names/1tracker-v0.9.1-beta12-msvc-wx32-10-tarball/versions/v0.9.1-beta12/1tracker-v0.9.1-beta12_msvc-wx32-10-x86.tar.gz) |
| **Linux** (Debian/Ubuntu 12, x86_64) | v0.9.1-beta12 | [1tracker-v0.9.1-beta12_debian12-x86_64.tar.gz](https://dl.cloudsmith.io/public/pa2wlt/1tracker-beta/raw/names/1tracker-v0.9.1-beta12-debian-x86_64-12-tarball/versions/v0.9.1-beta12/1tracker-v0.9.1-beta12_debian-x86_64-12-x86_64.tar.gz) |
| **Raspberry Pi** (64-bit, Pi 4/5 on Raspberry Pi OS 12) | v0.9.1-beta12 | [1tracker-v0.9.1-beta12_debian12-arm64.tar.gz](https://dl.cloudsmith.io/public/pa2wlt/1tracker-beta/raw/names/1tracker-v0.9.1-beta12-debian-arm64-A64-12-tarball/versions/v0.9.1-beta12/1tracker-v0.9.1-beta12_debian-arm64-12-arm64.tar.gz) |

> **On Android?** The install procedure is different — Android OpenCPN has no
> "Install from file" button, so the tarball flow below does not apply.
> Jump to [Android installation](#android-installation).

<details>
<summary><b>Other builds</b> — older/newer Linux, Flatpak, Intel-only macOS</summary>

Additional builds are published for less common configurations. If you are not
sure which one you need, the table above is almost certainly right.

| Config | Download |
|---|---|
| **macOS Intel-only** (older Macs without Apple Silicon) | [darwin-wx32-10.15-x86_64](https://dl.cloudsmith.io/public/pa2wlt/1tracker-beta/raw/names/1tracker-v0.9.1-beta12-darwin-wx32-10.15-tarball/versions/v0.9.1-beta12/1tracker-v0.9.1-beta12_darwin-wx32-10.15-x86_64.tar.gz) |
| **Debian/Ubuntu 11** (x86_64) | [debian-x86_64-11](https://dl.cloudsmith.io/public/pa2wlt/1tracker-beta/raw/names/1tracker-v0.9.1-beta12-debian-x86_64-11-tarball/versions/v0.9.1-beta12/1tracker-v0.9.1-beta12_debian-x86_64-11-x86_64.tar.gz) |
| **Debian/Ubuntu 11 (wx3.2 build)** (x86_64) | [debian-wx32-x86_64-11](https://dl.cloudsmith.io/public/pa2wlt/1tracker-beta/raw/names/1tracker-v0.9.1-beta12-debian-wx32-x86_64-11-tarball/versions/v0.9.1-beta12/1tracker-v0.9.1-beta12_debian-wx32-x86_64-11-x86_64.tar.gz) |
| **Debian/Ubuntu 13** (x86_64) | [debian-x86_64-13](https://dl.cloudsmith.io/public/pa2wlt/1tracker-beta/raw/names/1tracker-v0.9.1-beta12-debian-x86_64-13-tarball/versions/v0.9.1-beta12/1tracker-v0.9.1-beta12_debian-x86_64-13-x86_64.tar.gz) |
| **Raspberry Pi 64-bit, Debian 13** (Pi OS 13) | [debian-arm64-13](https://dl.cloudsmith.io/public/pa2wlt/1tracker-beta/raw/names/1tracker-v0.9.1-beta12-debian-arm64-A64-13-tarball/versions/v0.9.1-beta12/1tracker-v0.9.1-beta12_debian-arm64-13-arm64.tar.gz) |
| **Raspberry Pi 32-bit, Debian 12** (older Pi or 32-bit Pi OS 12) | [debian-armhf-12](https://dl.cloudsmith.io/public/pa2wlt/1tracker-beta/raw/names/1tracker-v0.9.1-beta12-debian-armhf-A32-12-tarball/versions/v0.9.1-beta12/1tracker-v0.9.1-beta12_debian-armhf-12-armhf.tar.gz) |
| **Raspberry Pi 32-bit, Debian 13** (older Pi or 32-bit Pi OS 13) | [debian-armhf-13](https://dl.cloudsmith.io/public/pa2wlt/1tracker-beta/raw/names/1tracker-v0.9.1-beta12-debian-armhf-A32-13-tarball/versions/v0.9.1-beta12/1tracker-v0.9.1-beta12_debian-armhf-13-armhf.tar.gz) |
| **Flatpak** (OpenCPN via Flathub, x86_64, runtime 22.08) | [flatpak-x86_64-22.08](https://dl.cloudsmith.io/public/pa2wlt/1tracker-beta/raw/names/1tracker-v0.9.1-beta12-flatpak-x86_64-22.08-tarball/versions/v0.9.1-beta12/1tracker-v0.9.1-beta12_flatpak-x86_64-22.08-x86_64.tar.gz) |
| **Flatpak** (OpenCPN via Flathub, x86_64, runtime 24.08) | [flatpak-x86_64-24.08](https://dl.cloudsmith.io/public/pa2wlt/1tracker-beta/raw/names/1tracker-v0.9.1-beta12-flatpak-x86_64-24.08-tarball/versions/v0.9.1-beta12/1tracker-v0.9.1-beta12_flatpak-x86_64-24.08-x86_64.tar.gz) |
| **Flatpak** (OpenCPN via Flathub, x86_64, runtime 25.08) | [flatpak-x86_64-25.08](https://dl.cloudsmith.io/public/pa2wlt/1tracker-beta/raw/names/1tracker-v0.9.1-beta12-flatpak-x86_64-25.08-tarball/versions/v0.9.1-beta12/1tracker-v0.9.1-beta12_flatpak-x86_64-25.08-x86_64.tar.gz) |
| **Flatpak** (OpenCPN via Flathub, ARM64, runtime 22.08) | [flatpak-aarch64-22.08](https://dl.cloudsmith.io/public/pa2wlt/1tracker-beta/raw/names/1tracker-v0.9.1-beta12-flatpak-aarch64-A64-22.08-tarball/versions/v0.9.1-beta12/1tracker-v0.9.1-beta12_flatpak-aarch64-22.08-aarch64.tar.gz) |
| **Flatpak** (OpenCPN via Flathub, ARM64, runtime 24.08) | [flatpak-aarch64-24.08](https://dl.cloudsmith.io/public/pa2wlt/1tracker-beta/raw/names/1tracker-v0.9.1-beta12-flatpak-aarch64-A64-24.08-tarball/versions/v0.9.1-beta12/1tracker-v0.9.1-beta12_flatpak-aarch64-24.08-aarch64.tar.gz) |
| **Flatpak** (OpenCPN via Flathub, ARM64, runtime 25.08) | [flatpak-aarch64-25.08](https://dl.cloudsmith.io/public/pa2wlt/1tracker-beta/raw/names/1tracker-v0.9.1-beta12-flatpak-aarch64-A64-25.08-tarball/versions/v0.9.1-beta12/1tracker-v0.9.1-beta12_flatpak-aarch64-25.08-aarch64.tar.gz) |

**How to tell which Flatpak runtime you have:** if you installed OpenCPN from
Flathub, run `flatpak info org.opencpn.OpenCPN` and look at the `Runtime`
line — it will say e.g. `org.freedesktop.Platform/x86_64/24.08`.

</details>

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

## Android installation

Android OpenCPN does not expose an "Install plugin from file" button — plugins
can only be installed through the in-app Plugin Catalog. Steps 1–3 above
therefore do not apply on Android. Instead, point OpenCPN's catalog at
1tracker's beta channel and install from the in-app list.

1. Open **OpenCPN** on the Android device.
2. Go to **Options** → **Plugins** tab.
3. Tap **Update Plugin Catalog** (or **Plugin Sources** in newer builds) and
   pick the **Custom** channel.
4. Paste this URL:

   ```
   https://dl.cloudsmith.io/public/pa2wlt/1tracker-beta/raw/names/ocpn-plugins/versions/latest/ocpn-plugins.xml
   ```

5. Tap **Update** / **Refresh**. OpenCPN fetches the catalog and **1tracker**
   appears in the available-plugins list.
6. Tap **1tracker** → **Install**. OpenCPN downloads the correct
   `android-arm64` or `android-armhf` build automatically based on the device.
7. Tap **Enable**, then restart OpenCPN.

After the restart, the **1tracker** toolbar icon appears. Continue with
[Step 4 — Configure for NoForeignLand](#step-4--configure-for-noforeignland).

### Android troubleshooting

- **"Invalid XML" / "Could not parse catalog"** — nearly always a typo in the
  URL. Paste it, do not type.
- **"No plugins found"** after refresh — OpenCPN cached a failed fetch.
  Force-close the app and reopen, then retry the catalog update.
- **Plugin stays "Installed" but does not activate** — tap **Enable**, then
  restart OpenCPN a second time. Android OpenCPN sometimes needs two restarts
  before the toolbar registers the new icon.
- **Toolbar icon still missing** — check **Options → About**: the plugin API
  version must be **1.18 or newer**. Older Android OpenCPN builds cannot load
  1tracker.

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

*Last updated: 2026-04-25 — all platforms at v0.9.1-beta12*
