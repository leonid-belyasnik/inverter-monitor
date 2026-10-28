# Inverter Monitor
[![CI](https://github.com/leonid-belyasnik/inverter-monitor/actions/workflows/ci.yml/badge.svg)](https://github.com/leonid-belyasnik/inverter-monitor/actions/workflows/ci.yml)

Inverter Monitor is part of the HomePowerGuard project.

Copyright (c) 2026 Leonid Belyasnik.

It collects inverter telemetry over MQTT, stores it in PostgreSQL, and serves
it through a REST API, a WebSocket stream, and a read-only web UI.

> **Status: beta.** The UI and API are read-only, have no authentication, and
> are intended for a trusted network. See [Current limitations](#current-limitations).

## Components

| Component | Path | Technology |
|---|---|---|
| Backend (`inverter-monitor`) | `backend/`, `domain/`, `database/` | C++20, CMake, Boost, libmosquitto, libpq |
| Web UI | `ui/` | React, TypeScript, Vite, served by nginx |
| Telemetry simulator | `simulator/` | C++ test publisher (default device `simulator-001`) |
| ESP32 firmware | `firmware/esp32-relay/` | PlatformIO/Arduino; reads a Deye inverter over Modbus RTU |
| Infrastructure | `compose.yaml`, `docker/` | Docker Compose, Mosquitto (MQTT), PostgreSQL 17 |

## How it works

```text
ESP32 firmware / simulator
        | MQTT  v1/devices/{deviceId}/telemetry
        v
   Mosquitto  -->  inverter-monitor  -->  PostgreSQL
                        |
                        +-- REST  :8080  (/api/v1/...)
                        +-- WebSocket :8081
                                |
                         Web UI (nginx, /api and /ws proxied to the backend)
```

Measurements are saved to PostgreSQL first and only then pushed to WebSocket
clients.

## Documentation

* [Architecture](docs/architecture.md) – components, data flow, schema migrations
* [API](docs/api.md) – REST and WebSocket reference
* [Development](docs/development.md) – build, tests, workflow
* [Deployment](docs/deployment.md) – Docker Compose on a Linux VPS
* [Configuration](docs/configuration.md) – environment variables, ports, secrets
* [UI documentation](docs/ui/beta-scope.md) – beta scope, [interface spec](docs/ui/interface-spec.md), [telemetry and freshness](docs/ui/telemetry-and-freshness.md), [local development](docs/ui/local-development.md)
* [ESP32 firmware](firmware/esp32-relay/README.md)

## Quick start (development)

Prerequisites: a C++ toolchain, CMake, `libpq-dev`, `libboost-json-dev`,
`libmosquitto-dev` and `libgtest-dev` (see the [`Dockerfile`](Dockerfile) for the
exact package list), Docker, and Node.js for the UI.

```bash
cp .env.example .env                 # then edit the placeholder values
docker compose up -d mqtt postgres   # MQTT broker and database
cmake -S . -B build && cmake --build build
```

Running the backend, simulator and UI is described in
[Development](docs/development.md) and
[UI local development](docs/ui/local-development.md).

## Current limitations

* Beta software; interfaces may change.
* No authentication or TLS in the backend, the UI, or the bundled Mosquitto
  configuration (anonymous access). Do not expose it to the internet as is.
* The root `compose.yaml` provides only MQTT and PostgreSQL (plus a `test`
  profile). The backend and UI are deployed from separate images; a production
  Compose file is not yet part of the repository. See [Deployment](docs/deployment.md).
* The UI device ID is set at build time in `ui/src/deviceConfig.ts`.
* ESP32 firmware reads PV, grid and load power from registers that are not yet
  confirmed against a physical inverter (see the firmware README).
* Backend ports `8080` (HTTP) and `8081` (WebSocket) are fixed in the code.
