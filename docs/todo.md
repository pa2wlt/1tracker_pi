To do:
+ Prepare for OCPN Plugin Catalog:
++ Add CI builds for Windows, macOS, Ubuntu, and Flatpak packaging.
++ Add Cloudsmith upload/publish workflow.
++ Add `CHANGELOG.md`.
++ Create annotated tag `v0.1.0`.
++ Prepare and submit the catalog PR.
+ Add min distance between positions (60m), to avoid too many calls.
+ Create a manual with screenshots and links. I.e. this link: https://www.noforeignland.com/map/settings/boat/tracking/api 
+ Add link to online help / manual in help text.
+ Add link to report issues in help text.
+ Verify Windows and Linux runtime config path handling. `GetpPrivateApplicationDataLocation()` has only been exercised on macOS so far.
+ Prepare for non connectivity: keep last successfull sent, also after restart. Ask confirmation to send positions since then. Collect positions since then and send. No other NMEA data in that case. (See Plan-Connectivity-Resilient Sending.md)
+ Translations / .po files? Ask Rick how to deal with that.

Future wishes for additional functionality:
+ Option to send averages for specific data fields.
+ Option to select other NMEA data fields.

Known issues:
- In the tracker detail screens the type is not reachable via tab, nor is the checkbox for including AWA/AWS. Minor issue; the workaround is not yet complete, especially on macOS.

Done:
- Generated plugin metadata from CMake (`plugin.xml.in`, `version.h.in`, plugin manager descriptions).
- Added plugin preferences.
- Double checked for no local changes in submodules.
- Added logic to not fail upon first install for not having config file of folder.
- Added toolbar button with icon (neutral/green/red)
- Added help text with location of config file.
- Added simple option to include AWA/AWS.
- Added heros (1tracker/httpjson/NFL).
- Prepared codebase for extra tracker types.
- Added automated tests.
- Refactored large and complex routines.
