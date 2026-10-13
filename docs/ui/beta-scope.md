# HomePowerGuard Beta Scope

## Goal

The first beta is a responsive, read-only web interface for viewing inverter telemetry from an iPhone or a computer on the home Wi-Fi network. It uses the existing `inverter-monitor` REST and WebSocket APIs. The existing simulator is a development and test source; its readings must be identified as simulated and must not be presented as readings from a real inverter.

## Beta Features

* A home screen with the available measurements: battery state of charge, PV power, and home load power. Grid voltage is shown as unavailable until a real source and field are confirmed. Other available power values may be shown with their sign preserved; see [Telemetry and Freshness](telemetry-and-freshness.md).
* Measurement history with charts built from the existing device measurement endpoint.
* Separate indicators for HTTP/API availability, WebSocket connection state, and whether new telemetry is arriving.
* The measurement time (`measured_at`) and browser-local receipt time as separate marks. Browser receipt time is not backend receive time. Showing `backend_received_at` is not required for the first UI beta; it remains a possible future backend/API extension.
* A visible last-update age and stale-data warning. The initial stale threshold is provisional and must be confirmed against real telemetry cadence.
* WebSocket reconnection after a dropped connection, followed by a new subscription to the selected device.
* Clear indication of the selected device and its explicit source classification (`simulator` or `real`), configured separately from `device_id`.

## Out of Scope

* Relay control, inverter configuration, or any other write operation.
* Authentication, internet access, remote access, or a new hosted infrastructure stack.
* Treating simulator output as representative of a real inverter.
* Inferring unavailable measurements, including grid voltage, from other fields.
* Device discovery. The current API has no device-list endpoint; the beta uses an explicitly configured device ID.
* Changing current REST or WebSocket contracts as part of UI implementation. Any additive contract proposal must be reviewed separately.

## Readiness Criteria

The beta is ready for review when:

* The responsive interface can be opened on a computer and an iPhone connected to the same home Wi-Fi network.
* The home screen shows available measurements with explicit units, sign behavior, `measured_at`, browser receipt time, and data source; browser receipt time is not labeled as backend receipt time.
* The grid-voltage card displays **«Недоступно»** until its source is confirmed.
* The history view renders returned measurements in chronological order and communicates when the API limit truncates the requested history.
* API errors, WebSocket connection loss, waiting for the first update, and stale telemetry have distinct visible states.
* A forced WebSocket interruption is followed by reconnection and a fresh device subscription.
* When no new measurements arrive, the last value remains visibly timestamped and the provisional stale warning appears; it is not replaced with zero.
* The simulator is visibly identified as a test data source throughout the UI or in an equally prominent source indicator.
* The UI distinguishes `measured_at` from browser receipt time and does not present browser receipt as backend receipt. `backend_received_at` is not required for this UI beta.
* No relay controls, inverter settings, authentication, or remote access are present.

## Related Specifications

* [Interface Specification](interface-spec.md)
* [Telemetry and Freshness](telemetry-and-freshness.md)
* [Local Development](local-development.md)
