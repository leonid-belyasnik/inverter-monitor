# HomePowerGuard Interface Specification

## Navigation and Screens

The beta has two primary views: **Overview** and **History**. The selected device and source label remain visible in both views. The current API does not enumerate devices, so the initial device ID is supplied by local configuration rather than discovered from the backend.

### Overview

Show a compact status area followed by measurement cards:

* **Grid voltage** — `grid_voltage`, read from REST and WebSocket measurements. Display it with one decimal place and the unit **«В»** (for example, «230,4 В»). When the value is `null`, display **«Недоступно»**; do not estimate it or show zero as a substitute. A reported `0` is a real value and is displayed as `0,0 В`. The backend and UI support the field, but the simulator and the current ESP32 firmware do not publish it yet, so the card shows **«Недоступно»** for them. The presence of the field in the API does not confirm that Deye register 150 is a reliable source of grid voltage; see [Telemetry and Freshness](telemetry-and-freshness.md).
* **Battery charge** — `battery_soc`, displayed as a percentage as indicated by the input field `battery_soc_pct`.
* **PV power** — `pv_power`, with watts indicated by the input field `pv_power_w`.
* **Home consumption** — `load_power`, with watts indicated by the input field `load_power_w`.

Other existing values (`grid_power` and `battery_power`) may be included as additional cards or chart series. Preserve their numeric signs. The sign meaning for grid power is not established for every source; do not convert signs into import/export labels until the source convention is confirmed. For the simulator, code comments state that positive battery power means discharge and negative means charge. Confirm that this convention also applies to any real source before describing real readings that way.

Each available measurement card shows its value, unit, and `measured_at`. Separately show the browser-local time at which the most recent WebSocket measurement arrived. Browser receipt time is useful for connection diagnostics and freshness, but it is not backend receive time and must not be labeled as such. Showing the proposed `backend_received_at` is not required for the first UI beta; it is not present in current responses. Timestamp display should make clear that API timestamps are UTC; local-time formatting may be used if the timezone is shown or otherwise clear.

The status area distinguishes:

* API request status;
* WebSocket state (connecting, connected, reconnecting, disconnected, or error);
* telemetry state (waiting for first measurement, updating, or stale);
* last browser receipt of a WebSocket measurement;
* data source, explicitly configured as `simulator` or `real`, independently of `device_id`. The API does not supply this classification.

### History

Plot available values returned by `GET /api/v1/devices/{deviceId}/measurements?limit=N`. The response is newest-first; sort by `measured_at` ascending for a time-axis chart. Use separate axes or charts for state of charge and power. Preserve source values and signs. Grid voltage is not plotted; it appears only on its card.

The endpoint supports at most 1000 measurements and has no time-range or pagination parameters. Indicate the selected sample limit and explain that the view contains only the returned recent records. Show an empty state when the response contains no measurements.

## Responsive Behavior

* On narrow iPhone screens, place status and cards in a single scrollable column. Charts use the available width and remain legible without horizontal page scrolling.
* On wider screens, arrange cards and charts in a compact multi-column layout while preserving the same information order and labels.
* Keep controls and text readable at mobile sizes; status must not rely on color alone.
* The interface is a browser-based web app. iPhone verification uses Safari over the home Wi-Fi network; a MacBook is not required.

## Loading and Error States

* **Initial loading:** show placeholders or a concise loading label while REST history/latest data is requested.
* **No measurements:** show an explicit empty state; do not render missing values as zero.
* **HTTP/API error:** identify that historical or initial data could not be loaded and offer retry. Do not interpret `/api/v1/health` success as proof that telemetry is flowing.
* **WebSocket connecting/reconnecting:** retain the last known values and their timestamps, mark them as not live, and show connection state.
* **Connected, waiting for measurement:** show that the socket is connected but no new measurement has arrived yet.
* **Stale telemetry:** retain the last known values, show the last update time and age, and display a warning based on the provisional threshold in [Telemetry and Freshness](telemetry-and-freshness.md).
* **Unavailable field:** label it unavailable and, where appropriate, explain that the source is not yet supported. Never substitute a plausible-looking estimate.
* **Simulator source:** label test data prominently enough that a user cannot mistake it for a real inverter reading.

## Measurement Presentation

REST and WebSocket measurement names and value shape are defined in [Telemetry and Freshness](telemetry-and-freshness.md). The API currently has no quality flag, source classification, or backend receipt timestamp. The UI must not imply those are supplied by the current contract. The exact display convention for signed `grid_power` and for values from a real inverter remains an open question pending source documentation.
