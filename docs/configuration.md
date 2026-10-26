# Configuration

Settings for the backend (`inverter-monitor`), the UI, and the Compose
infrastructure. Names and defaults come from `backend/main.cpp`,
`database/PostgresConnection.*`, `simulator/main.cpp`, `compose.yaml`,
`ui/nginx.conf` and `ui/src/deviceConfig.ts`.

**Secrets.** Passwords and other credentials belong in the local `.env` file or
the deployment environment, never in Git, Compose files, images or docs.
`.env` and `firmware/esp32-relay/include/secrets.h` are listed in `.gitignore`.
Start from [`.env.example`](../.env.example), which contains placeholders only.

## Compose variables (`.env`)

`docker compose` reads `.env` from the directory of the Compose file. There are
two Compose setups:

* the root `compose.yaml` for development (local Mosquitto, PostgreSQL, `test`
  profile);
* the VPS deployment file described in [Deployment](deployment.md)
  (PostgreSQL, backend, UI, external MQTT broker).

| Variable | Used by | Kind | Notes |
|---|---|---|---|
| `POSTGRES_DB` | `postgres`; deployment `backend` (`PGDATABASE`) | Required | Database created on first start |
| `POSTGRES_USER` | `postgres`; deployment `backend` (`PGUSER`) | Required | Database role created on first start |
| `POSTGRES_PASSWORD` | `postgres`; deployment `backend` (`PGPASSWORD`) | Required, secret | No default; keep out of Git |
| `MQTT_HOST` | deployment `backend` | Required for the deployment | Must not be empty: an empty value is passed to the backend as is, and the default `localhost` applies only when the variable is absent from the container |
| `MQTT_PORT` | deployment `backend` | Has a default | `.env.example` sets `1883`; an empty or non-numeric value makes the backend use `1883` |
| `MQTT_USERNAME` | deployment `backend` | Optional | Empty in `.env.example` (no authentication). If the broker requires authentication, fill in both this and `MQTT_PASSWORD` with the real values |
| `MQTT_PASSWORD` | deployment `backend` | Optional, secret | Empty in `.env.example`. Requires `MQTT_USERNAME`; a password without a user name makes the backend exit |

`.env.example` contains all seven variables. The root `compose.yaml` reads only
the `POSTGRES_*` variables; the four `MQTT_*` variables are read by the
deployment Compose file in [Deployment](deployment.md), which passes them to
the backend unchanged. Keep all four in `.env` even when username and password
are empty: if a variable is undefined, Compose warns and substitutes an empty
string (with empty `POSTGRES_*` values the PostgreSQL image refuses to start).
The `POSTGRES_*` values apply only when the `postgres_data` volume is first
created. In the development setup, `docker/init-db.sql` additionally creates an
`inverter_test` database for the test profile.

## Backend: PostgreSQL

The backend opens its connection with an empty connection string, so it is
configured entirely through the standard
[libpq environment variables](https://www.postgresql.org/docs/current/libpq-envars.html).
Commonly used:

| Variable | Default (libpq) | Notes |
|---|---|---|
| `PGHOST` | local Unix socket | Set to `postgres` inside the Compose network, or `localhost` on a host |
| `PGPORT` | `5432` | |
| `PGDATABASE` | same as the user name | Set explicitly |
| `PGUSER` | OS user name | Set explicitly |
| `PGPASSWORD` | none | Or use `PGPASSFILE` to keep the password in a file |
| `PGCONNECT_TIMEOUT` | `5` (seconds) | The backend applies `5` itself unless this is set |

Behavior at startup: the backend connects and applies schema migrations
automatically, retrying transient failures for up to 60 s (permanent errors,
such as invalid parameters or a missing password, stop it immediately). See
[Architecture](architecture.md#startup-retry-stage-2a).

## Backend: MQTT

| Variable | Default | Required | Notes |
|---|---|---|---|
| `MQTT_HOST` | `localhost` (only if the variable is unset; an empty value is used as is) | Optional | Broker host name or IP address; `mqtt` with the development `compose.yaml`, the external broker in a deployment |
| `MQTT_PORT` | `1883` | Optional | An empty or non-numeric value silently falls back to `1883` |
| `MQTT_USERNAME` | unset | Optional | Empty or unset means no authentication |
| `MQTT_PASSWORD` | unset | Optional | Requires `MQTT_USERNAME`; otherwise the backend exits |

The backend subscribes to `v1/devices/+/telemetry` using the client id
`inverter-monitor-backend`. Credentials are sent over plain MQTT: the backend
has no TLS support. Credentials are never logged.

### Broker

The deployment uses an external broker. It should require a user name and
password, and its port must stay inside a trusted network or VPN, because
neither the backend nor the ESP32 firmware supports TLS.

`docker/mosquitto/config/mosquitto.conf` is the broker configuration of the
development `compose.yaml`. It listens on `1883`, allows anonymous clients and
enables persistence. It does not define users, a password file or TLS, and is
not a production configuration.

## Ports

| Port | Component | Configurable |
|---|---|---|
| `8080` | Backend REST API (`/api/v1/...`), bound to `0.0.0.0` | No, fixed in `backend/main.cpp` |
| `8081` | Backend WebSocket, bound to `0.0.0.0` | No, fixed in `backend/main.cpp` |
| `80` | UI container (nginx) | Container port fixed; the deployment example publishes it as `127.0.0.1:8090` |
| `1883` | MQTT broker | External broker in a deployment; local Mosquitto in the development `compose.yaml` |
| `5432` | PostgreSQL | Not published in the deployment; published on all host interfaces by the development `compose.yaml` |

In the deployment example only the UI is published, and only on `127.0.0.1`.
The UI container reaches the backend over the Compose network, so `8080` and
`8081` are not published on the host. A port published on all interfaces is
reachable by anyone who can reach the server, and none of these services
provides authentication or TLS: restrict access with the bind address, a VPN
and/or firewall rules. See [Network access](deployment.md#network-access).

## UI

The UI has no runtime environment variables.

* nginx (`ui/nginx.conf`) proxies `/api/` to `http://backend:8080` and `/ws` to
  `http://backend:8081`, so the backend must be reachable as `backend`.
* The device ID and source label are compile-time constants in
  `ui/src/deviceConfig.ts` (`deye-esp32-001`, source `deye`). Changing them
  requires rebuilding the UI image.
* The Vite dev server (`npm run dev`) proxies to `127.0.0.1:8080` and
  `127.0.0.1:8081` (`ui/vite.config.ts`).

## Simulator

| Variable | Default | Notes |
|---|---|---|
| `MQTT_HOST` | `localhost` | |
| `MQTT_PORT` | `1883` | |
| `DEVICE_ID` | `simulator-001` | Topic is `v1/devices/<DEVICE_ID>/telemetry` |
| `PUBLISH_INTERVAL_SEC` | `5` | |

The simulator does not support MQTT credentials.

## Firmware

ESP32 Wi-Fi, MQTT and device settings are not environment variables. They come
from the untracked `firmware/esp32-relay/include/secrets.h` (template:
`secrets.example.h`) or from the on-device serial console. The MQTT settings
are broker host, port (`1883` in the example) and an optional user name and
password; an empty user name means anonymous, and a password requires a user
name (as in the backend). The firmware connects over plain TCP (`WiFiClient`)
without TLS. See the [firmware README](../firmware/esp32-relay/README.md).

## Test environment

The `test` Compose profile and CI run `ctest` against PostgreSQL using
`PGHOST`, `PGPORT`, `PGDATABASE`, `PGUSER` and either `PGPASSWORD` (CI) or
`PGPASSFILE` (Compose, from the untracked `docker/test.pgpass`). See
[Development](development.md).
