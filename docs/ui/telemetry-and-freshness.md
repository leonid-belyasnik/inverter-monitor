# HomePowerGuard Telemetry and Freshness

## Current REST Contract

The main runtime serves REST on port `8080`.

* `GET /api/v1/health` returns plain text `OK\n` when the HTTP route responds. It does not report MQTT state or telemetry freshness.
* `GET /api/v1/devices/{deviceId}/measurements?limit=N` returns `{ "device_id": "...", "measurements": [...] }`. `limit` defaults to 100. The code parses it with `std::stoul` and uses values from 1 through 1000; missing values, conversion failures, and parsed values outside that range use the default. Because the code does not check that the whole string was consumed, a numeric prefix with trailing characters can be accepted (for example, `3x` may be parsed as `3`). This is the current implementation behavior; strict validation is an open API issue. Records are ordered newest-first by `measured_at`.
* Each measurement currently contains `measured_at`, `pv_power`, `grid_power`, `battery_power`, `load_power`, and `battery_soc`. Timestamps are serialized as ISO 8601 UTC strings. `pv_power`, `grid_power`, and `load_power` are `null` when the source did not provide them; `battery_power` and `battery_soc` are always numbers. There is no record ID, `backend_received_at`, voltage, quality flag, or source-kind field in this response.

## Current WebSocket Contract

The WebSocket endpoint is `ws://<backend-host>:8081`.

Subscribe with:

```json
{"type":"subscribe","device_id":"simulator-001"}
```

On a valid subscription, the server responds with:

```json
{"type":"subscribed","device_id":"simulator-001"}
```

For an invalid subscribe command, it responds with:

```json
{"type":"error","message":"invalid subscribe command"}
```

The client considers subscription confirmed only when `type` is `subscribed` and `device_id` matches the expected device. After confirmation, new successfully persisted measurements are sent as:

```json
{
  "type": "measurement",
  "device_id": "simulator-001",
  "data": {
    "measured_at": "2026-10-05T11:49:37.859Z",
    "pv_power": 2274.72,
    "grid_power": -200.0,
    "battery_power": -543.28,
    "load_power": 1531.44,
    "battery_soc": 73.06
  }
}
```

`data` uses the same `null` rule as REST for `pv_power`, `grid_power`, and `load_power`. The UI must show `null` as unavailable, never as zero.

Subscription does not send a snapshot of the most recent measurement. The client should load initial/history data through REST and use WebSocket for subsequent updates. The current WebSocket measurement message also has no `backend_received_at`, record ID, sequence number, or source-kind field.

## Units and Sign Conventions

The simulator input field names explicitly suffix power values with `_w` and state of charge with `_pct`. The backend parser reads these values as numbers, and REST/WebSocket remove those suffixes. It is therefore appropriate to display simulator power in W and state of charge in percent. The code does not establish whether every real source uses these same units; confirm the real device adapter/protocol before presenting its values with those units.

The ESP32 beta firmware (`firmware/esp32-relay`) publishes `battery_soc_pct` (%) and `battery_power_w` (W) from verified Deye registers. It also publishes `pv_power_w`, `grid_power_w`, and `load_power_w` (W), temporarily from registers that are **not confirmed** against the inverter, so their scale, sign, and meaning may be wrong. A field is `null` when its read fails or its register is disabled in firmware. See `firmware/esp32-relay/REGISTER_MAP.md`.

The simulator code comments define its battery convention: positive `battery_power` means discharge; negative means charge. The ESP32 firmware uses the same convention, but for the real inverter the sign has only been observed while charging; the discharge sign still requires hardware verification. PV and load are generated as positive values by the simulator. The simulator sets `grid_power` to `-200`, but code does not document whether negative means import or export. The backend does not normalize or validate these signs. Until source conventions are confirmed, preserve signed values and avoid import/export labels or derived flow diagrams. Exact grid-power sign semantics for real telemetry remain an open question.

Grid voltage is absent from the domain measurement, parser input, database, REST, and WebSocket contracts. It is unavailable to the beta until its source and field are confirmed.

## Three Distinct Times

* **`measured_at`** is supplied by the telemetry source and represents the time associated with the measurement. The simulator sets it when generating its sample. Source clock accuracy is not guaranteed by the current contract.
* **`backend_received_at` is a proposed extension, not a current field.** If adopted, define it as the backend timestamp captured when the MQTT callback receives a telemetry message, before parsing and persistence. This gives the UI a backend-side receipt time and allows a tentative `backend_received_at - measured_at` delay estimate. That estimate is meaningful only when source and backend clocks are sufficiently synchronized. Persisting it would be necessary if it is to appear on historical REST records; adding it to REST and WebSocket would be an additive JSON contract change that needs separate approval and implementation.
* **Browser receipt time** is captured locally when the browser receives a WebSocket measurement. It is useful for determining how long the UI has gone without a new message. It is not the time the backend received the MQTT message and must not be labeled as such. A browser clock and backend clock may differ, so do not infer one timestamp from the other without an explicit clock-synchronization assumption.

## Freshness Rules

The current API supplies no telemetry status or expected publication interval. Freshness is therefore a UI-derived state, not a backend health guarantee.

* Keep the WebSocket connection state and telemetry freshness as separate state values. A socket can be connected while telemetry is stale, or disconnected while the last measurement is still recent.
* When a subscription is confirmed, start a freshness timer even if no WebSocket measurement has arrived yet. If no measurement arrives within the configured threshold, show stale/no recent telemetry rather than waiting indefinitely for a first sample.
* Record a browser-local receipt instant for each WebSocket measurement and update the displayed last-update age from it. Continue advancing this age and applying the stale threshold regardless of whether the WebSocket is connected, reconnecting, or disconnected.
* Keep the last known value and its `measured_at` visible while data is stale; never replace it with zero. On reconnect, do not reset freshness merely because the socket connected or subscription was confirmed.
* A preliminary threshold may be set to three expected publication intervals. This multiplier and the expected interval are provisional parameters, not confirmed product values. Confirm the interval and threshold on real telemetry before release; the simulator's five-second interval must not be assumed for an inverter.
* After reload, a REST measurement may seed the displayed last value, but its browser fetch time is not a backend receive time. Show its `measured_at`; once the WebSocket subscription is confirmed, start the no-message timer even if no new measurement follows.
* `/api/v1/health` only indicates that the HTTP health route responded. It must not reset the telemetry stale timer or imply MQTT connectivity.

## WebSocket Reconnection

On unexpected close or network error:

1. Mark the WebSocket disconnected/reconnecting and keep existing values with their last update time.
2. Retry with bounded exponential backoff, starting at a short delay and capping the delay. Exact timing and jitter are implementation parameters to verify during UI implementation.
3. On reconnect, send the same `subscribe` command for the selected device again.
4. Mark the socket connected only after the expected subscription confirmation. Keep telemetry freshness separate: if the last sample is stale, it stays stale; if no sample has ever arrived after confirmation, start the first-measurement timer.
5. A received measurement updates its browser receipt time and freshness. Do not claim that reconnecting or resubscribing made an old measurement fresh.

The browser should handle malformed or unexpected messages as a visible connection/data error without crashing the view. Current server messages are JSON subscription confirmations, errors, and measurement updates described above.

## Open Questions

* Which real source supplies grid voltage, and in what field/unit?
* Are all real source power values in watts and state of charge in percent?
* What are the real sign conventions, especially for grid power?
* What is the normal real telemetry interval and acceptable stale threshold?
* Should `backend_received_at` be added to persisted measurements and both read APIs? This is not part of the current contract.
* How will the UI identify simulator versus real source? Current API messages do not carry a source-kind flag. The UI must use an explicit local source classification (`simulator` or `real`) separate from `device_id`; automatic classification is not available from the API.
