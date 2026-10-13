# ADR 002: REST and WebSocket API

## Status

Accepted

## Context

The backend needs to provide two ways to access inverter telemetry:

* Historical measurements stored in PostgreSQL.
* Real-time measurements as new telemetry is ingested.

These use cases have different communication requirements. Historical data is naturally exposed through request-response operations, while real-time updates benefit from a persistent connection.

## Decision

The API design uses two interfaces:

* **REST over HTTP** for health checks and querying stored measurements.
* **WebSocket** for real-time telemetry updates.

The WebSocket server does not subscribe directly to MQTT. Instead, it receives measurements from the application ingestion flow after successful persistence.

The WebSocket server uses its own Boost.Asio `io_context`, running in a dedicated thread. WebSocket session operations are handled within that execution context.

## Current Implementation Status

At the time this decision was recorded, the REST routes and HTTP server existed but were not started by the main runtime. The current `inverter-monitor` runtime now starts REST HTTP on port `8080` and WebSocket on port `8081`.

The `api-server` executable remains a separate development executable: its HTTP server `run()` call is disabled, and it starts only the WebSocket server.

This implementation-status update does not change the architectural decision.

## Consequences

* REST and WebSocket have distinct responsibilities.
* Historical measurements can be queried through REST on the main runtime's port `8080`.
* Real-time measurements are delivered through WebSocket subscriptions.
* The application ingestion flow remains the integration point between MQTT, persistence, and real-time publication.
* WebSocket network operations are separated from the MQTT communication path.
* The main runtime exposes both interfaces; the development `api-server` still requires HTTP to be enabled if REST access is needed there.

## Alternatives Considered

### WebSocket Only

Using WebSocket for both historical queries and real-time updates would combine request-response and streaming responsibilities in one interface.

This was not selected because REST is a more straightforward fit for querying stored measurements and exposing health checks.

### Direct MQTT Subscription from WebSocket Clients

Allowing the WebSocket server to subscribe directly to MQTT would couple the API layer to the telemetry transport and duplicate integration responsibilities.

This was not selected. The application ingestion flow remains responsible for forwarding successfully persisted measurements to WebSocket clients.
