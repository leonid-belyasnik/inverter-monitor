# API Reference

## Overview

The API layer contains REST HTTP and WebSocket interfaces for accessing inverter telemetry.

**Current runtime status:**

* WebSocket server is started by `inverter-monitor` and listens on port `8081`.
* `inverter-monitor` starts the HTTP server on port `8080` and WebSocket server on port `8081`.
* The `api-server` development executable currently starts the WebSocket server only; its HTTP server `run()` call is disabled.

REST endpoints described below are available from the running `inverter-monitor` executable.

## REST API

### Health Check

**Endpoint:** `GET /api/v1/health`

The health endpoint is implemented in the REST API code.

The endpoint is available on `http://localhost:8080/api/v1/health` when `inverter-monitor` is running.

### Get Device Measurements

**Endpoint:** `GET /api/v1/devices/{deviceId}/measurements`

Returns stored measurements for the specified device.

**Path parameters:**

* `deviceId` — identifier of the device whose measurements are requested.

**Query parameters:**

* `limit` — maximum number of measurements to return.

  * Default: `100`.
  * Valid range: `1–1000`.
  * Missing, invalid, or out-of-range values fall back to the default limit of `100`.

**Example request:**

```http
GET /api/v1/devices/simulator-001/measurements?limit=3
```

**Example response:**

```json
{
  "device_id": "simulator-001",
  "measurements": [
    {
      "measured_at": "2026-10-05T11:49:37.859Z",
      "pv_power": 2274.72,
      "grid_power": -200.0,
      "battery_power": -543.28,
      "load_power": 1531.44,
      "battery_soc": 73.06
    }
  ]
}
```

The example shows the response structure; actual values and the number of returned measurements depend on stored data and the requested limit.

Timestamps use ISO 8601 UTC format.

Every measurement object always contains all six keys. `measured_at` is always a string; `battery_power` and `battery_soc` are always numbers. `pv_power`, `grid_power`, and `load_power` are `null` when the telemetry source did not provide the value (see [Telemetry Input](architecture.md#protocol)); `null` means "unavailable", never zero. Example of a record from a source that publishes only battery values:

```json
{
  "measured_at": "2026-10-22T09:15:00.120Z",
  "pv_power": null,
  "grid_power": null,
  "battery_power": -203.0,
  "load_power": null,
  "battery_soc": 98.0
}
```

## WebSocket API

**Endpoint:** `ws://localhost:8081`

The WebSocket API provides real-time measurement updates for subscribed devices.

### Subscribe to a Device

The client sends:

```json
{
  "type": "subscribe",
  "device_id": "simulator-001"
}
```

On successful subscription, the server responds:

```json
{
  "type": "subscribed",
  "device_id": "simulator-001"
}
```

If the subscription command is invalid, the server responds with an error:

```json
{
  "type": "error",
  "message": "invalid subscribe command"
}
```

### Measurement Updates

After subscribing, the client receives measurement messages for the selected device.

Example:

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

The `device_id` identifies the source device. Measurement values are contained in the `data` object, which has the same shape as a REST measurement object, including `null` for unavailable `pv_power`, `grid_power`, and `load_power`.

Timestamps use ISO 8601 UTC format.

### Delivery Behavior

* Clients explicitly subscribe to a device.
* A client receives measurements for its subscribed device.
* The ingestion service publishes measurements after successful persistence.
* WebSocket operations are handled through the WebSocket server's own Boost.Asio execution context.
* Outgoing messages are queued per session to preserve write ordering and avoid overlapping asynchronous writes.

## Responsibilities and Boundaries

* REST provides access to stored measurements and health information.
* WebSocket provides real-time telemetry updates.
* The API layer handles network requests, message serialization, and client communication.
* Database access is performed through `database::MeasurementRepository`.
* The WebSocket server does not subscribe directly to MQTT; measurements reach it through the application ingestion flow.

## Current Limitations

* HTTP is exposed by `inverter-monitor` on port `8080`.
* The WebSocket API is available on port `8081`.
* The API currently provides device-specific measurement access and real-time subscription rather than a general-purpose device management interface.
