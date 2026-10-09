# ADR 001: Device-Aware Measurements

## Status

Accepted

## Context

The initial measurement model did not contain information identifying the source device.

This was sufficient while the system was effectively treated as a single-device prototype, but it does not scale to a system that can monitor multiple inverters.

The MQTT topic already contains the device identity:

```text
v1/devices/{deviceId}/telemetry
```

The device identity therefore needs to be preserved from the transport layer through persistence and API responses.

## Decision

Introduce an explicit `domain::DeviceId` value and associate every ingested measurement with a device.

The application-level representation is:

```text
application::IngestedMeasurement
    ├── deviceId
    └── measurement
```

The device ID is extracted from the MQTT topic and remains associated with the measurement throughout the ingestion flow.

The database stores the device ID together with each measurement.

Repository queries can therefore be scoped to a specific device.

The API also exposes the device ID in REST and WebSocket responses.

## Consequences

### Positive

* Multiple devices can be monitored using the same backend.
* Device identity is preserved across all layers.
* Stored measurements can be queried per device.
* WebSocket clients can subscribe to a specific device.
* Device-specific routing does not need to be implemented separately in every layer.

### Negative

* Existing measurement persistence and queries must include the device ID.
* Existing test data and database records require a device identifier.
* Application types are slightly more explicit than the original single-device model.

## Alternatives Considered

### Keep Device ID only in the MQTT layer

Rejected.

The device identity would be lost after message parsing and could not reliably be associated with persisted measurements or API responses.

### Add Device ID directly to `Measurement`

Rejected.

`Measurement` represents measurement data independently of the source device. Device identity is a separate concept and should not make the core measurement type transport- or device-oriented.

### Use a plain string instead of `DeviceId`

Rejected.

A dedicated value type makes the distinction between a device identifier and arbitrary strings explicit and leaves room for validation or additional behavior later without changing all consumers.
