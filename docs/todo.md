To do:
+ Prepare for OCPN Plugin Catalog:
++ Add CI builds for Windows, macOS, Ubuntu, and Flatpak packaging.
++ Add Cloudsmith upload/publish workflow.
++ Create annotated tag `v0.1.0`.
++ Prepare and submit the catalog PR.
+ Verify Windows and Linux runtime config path handling. `GetpPrivateApplicationDataLocation()` has only been exercised on macOS so far.
+ Translations / .po files? Ask Rick how to deal with that.

Future wishes for additional functionality:
+ Deal with non connectivity: keep last successfull sent, also after restart. Ask confirmation to send positions since then. Collect positions since then and send. No other NMEA data in that case. (See Plan-Connectivity-Resilient Sending.md)
+ Setting to send averages for specific NMEA-fields.
+ Option to select other NMEA data fields.

Known issues:
- In the tracker detail screens the type is not reachable via tab, nor is the checkbox for including AWA/AWS. Minor issue; the workaround is not yet complete, especially on macOS.
