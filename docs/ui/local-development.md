# HomePowerGuard Local Development

## Proposed UI Stack

The proposed first-beta UI uses React, TypeScript, and Vite. This stack supports browser development in VS Code and testing on an iPhone without building a native iOS app or requiring a MacBook. A chart library may be selected during implementation; keep it limited to rendering the existing measurement values and avoid changing API contracts to fit the library.

This is a proposal only. No UI source, `package.json`, or Vite configuration is defined by this document.

## Existing Backend and Simulator Flow

The backend runtime expects PostgreSQL and Mosquitto, receives simulator messages through MQTT, persists measurements, and publishes accepted measurements over WebSocket:

```text
simulator -> MQTT broker -> inverter-monitor -> PostgreSQL
                                      +-------> WebSocket clients
                                      +-------> REST clients
```

The simulator publishes to `v1/devices/{deviceId}/telemetry`. Its default device ID is `simulator-001`; its default publication interval is five seconds. These are test values, not a claim about a real inverter. Configure the source classification explicitly as `simulator` or `real`, separately from `device_id`, and show a clear simulator/test label when using the simulator.

Use the existing project infrastructure and backend development guide to start PostgreSQL, Mosquitto, `inverter-monitor`, and the simulator. Configure MQTT host/port using the existing `MQTT_HOST` and `MQTT_PORT` environment variables where necessary. Confirm the database connection configuration from the existing backend setup rather than introducing another database path for the UI.

Start the client data flow in this order:

1. Connect to the WebSocket endpoint through the development proxy and send a subscription for the configured device ID.
2. Wait for `{"type":"subscribed","device_id":"..."}` with the expected device ID. Treat an `error` response or a mismatched confirmation as a failed subscription.
3. Request REST history from `GET /api/v1/devices/{deviceId}/measurements?limit=N`.

The subscription has no initial snapshot. Merge REST history and WebSocket measurements by parsed `measured_at` so a REST result cannot replace a newer WebSocket measurement in the latest-value display. The API has no stable measurement ID: if two records have the same `measured_at`, the client cannot reliably tell whether they are the same record or distinct measurements. The implementation must choose and document a tie policy; preferring the WebSocket value for the live display does not prove the records are identical for history. See [Telemetry and Freshness](telemetry-and-freshness.md) for payload and timestamp details.

## Frontend Development Flow

During implementation, run the Vite development server on the development computer with a LAN bind, for example:

```bash
npm run dev -- --host 0.0.0.0
```

This command assumes the eventual Vite package defines a `dev` script; no `package.json` exists yet. Configure the Vite proxy to forward REST paths such as `/api` to `http://<backend-host>:8080`, and forward the UI's WebSocket path (for example `/ws`) to `ws://<backend-host>:8081` with WebSocket proxying enabled (`ws: true`). The UI should use the proxied paths so the browser talks to one origin. The exact Vite config and script will be added with the approved UI scaffold.

The backend listens on ports `8080` (HTTP) and `8081` (WebSocket). If the proxy runs on the same development computer as the backend, its targets can use the backend's local address; the iPhone opens the Vite page using the development computer's LAN IP and Vite port, not `localhost` (which refers to the phone). If backend and Vite run on different computers, configure the proxy target as the backend computer's reachable LAN address.

## iPhone on Home Wi-Fi

Beta verification requires opening the development or beta UI from Safari on an iPhone connected to the same home Wi-Fi as the backend/development computer. Verify that:

* the UI host is bound/reachable from the local network;
* the phone can reach both REST and WebSocket endpoints through the chosen UI origin/proxy arrangement;
* the development computer's LAN IP and Vite port are correct and reachable from the phone;
* the computer firewall permits inbound traffic to the Vite port and the backend ports where applicable;
* the router does not enable Wi-Fi client/AP isolation between the phone and development computer;
* the UI remains usable at a narrow mobile viewport;
* disconnecting and restoring Wi-Fi exercises visible WebSocket reconnect behavior.

The backend currently binds HTTP and WebSocket to `0.0.0.0`; this listens on all host network interfaces and does not by itself restrict access to home Wi-Fi. The beta must not be exposed through router port forwarding or an internet tunnel. Restrict access with local firewall/network configuration and verify reachability from the intended home network. The beta does not add authentication, so this network boundary is important.

## Verification Scenarios

* Start the simulator and confirm the UI labels the displayed source as simulated/test data.
* Stop the simulator while leaving the backend running; confirm the last values remain timestamped and the provisional stale warning appears.
* Interrupt the browser's WebSocket path, restore it, and confirm reconnect, resubscription, and arrival of fresh data.
* Stop or make the REST service unavailable and confirm the initial/history error is distinct from WebSocket state.
* Open the UI on an iPhone over home Wi-Fi and verify layout, REST loading, WebSocket updates, and recovery after Wi-Fi interruption.
* Confirm voltage remains explicitly unavailable until a source is implemented and confirmed.

## Documentation Note

`docs/api.md` and `docs/architecture.md` describe REST as enabled in the main runtime. `docs/development.md` and ADR 002 contain older statements that the runtime does not start HTTP. Verify and reconcile those existing statements when the UI implementation documentation is updated; this beta specification does not change them.
