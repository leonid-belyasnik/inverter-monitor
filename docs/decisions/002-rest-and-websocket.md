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

The REST routes and HTTP server implementation exist, but the HTTP server is not currently started by the running executables.

The `inverter-monitor` executable starts the MQTT ingestion flow and WebSocket server. The `api-server` executable is intended for isolated API development and testing, but its HTTP server `run()` call is currently disabled. It currently starts the WebSocket server only.

Consequently, the WebSocket interface is currently exposed on port `8081`, while the REST endpoints are not accessible through the running executables.

This is an implementation status, not a change to the architectural decision.

## Consequences

* REST and WebSocket have distinct responsibilities.
* Historical measurements can be queried through the REST API once the HTTP server is enabled.
* Real-time measurements are delivered through WebSocket subscriptions.
* The application ingestion flow remains the integration point between MQTT, persistence, and real-time publication.
* WebSocket network operations are separated from the MQTT communication path.
* The HTTP server must be enabled in the appropriate runtime before REST endpoints can be used.

## Alternatives Considered

### WebSocket Only

Using WebSocket for both historical queries and real-time updates would combine request-response and streaming responsibilities in one interface.

This was not selected because REST is a more straightforward fit for querying stored measurements and exposing health checks.

### Direct MQTT Subscription from WebSocket Clients

Allowing the WebSocket server to subscribe directly to MQTT would couple the API layer to the telemetry transport and duplicate integration responsibilities.

This was not selected. The application ingestion flow remains responsible for forwarding successfully persisted measurements to WebSocket clients.
