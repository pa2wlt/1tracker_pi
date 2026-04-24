To do:
+ Gather feedback from first-round beta testers on v0.1.0-beta1.
+ Verify Windows and Linux runtime config path handling. `GetpPrivateApplicationDataLocation()` has only been exercised on macOS so far.
+ Submit `po/1tracker_pi.pot` to the OpenCPN i18n maintainers so it gets on Crowdin; accept per-locale `.po` files when they return via PR.
+ Prepare and submit the OpenCPN plugin catalog PR once beta feedback is in.
+ Create annotated `v0.1.0` tag once we're ready for production (CI will route
  untagged-beta tags to the beta repo, plain tags to the prod repo automatically).

Future wishes for additional functionality:
+ Deal with non connectivity: keep last successfull sent, also after restart. Ask confirmation to send positions since then. Collect positions since then and send. No other NMEA data in that case. (See Plan-Connectivity-Resilient Sending.md)
+ Setting to send averages for specific NMEA-fields.
+ Option to select other NMEA data fields.

Known issues:
- In the tracker detail screens the type is not reachable via tab, nor is the checkbox for including AWA/AWS. Minor issue; the workaround is not yet complete, especially on macOS.
