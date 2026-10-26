# Deployment

How to run Inverter Monitor on a Linux VPS with Docker Compose.

The deployment consists of three containers in one Compose project and an
external MQTT broker:

| Service | Image source | Purpose |
|---|---|---|
| `postgres` | `postgres:17` | Measurement storage in the named volume `postgres_data` |
| `backend` | Built from [`Dockerfile`](../Dockerfile) (`--target runtime`) | MQTT ingestion, REST API `8080`, WebSocket `8081` |
| `ui` | Built from [`ui/Dockerfile`](../ui/Dockerfile) | nginx serving the web UI and proxying `/api/` and `/ws` to `backend` |
| MQTT broker | Not part of this Compose project | Receives telemetry from the ESP32 firmware |

The root [`compose.yaml`](../compose.yaml) is the development setup (local
Mosquitto with anonymous access, PostgreSQL published on `5432`, and a `test`
profile). It does not contain the `backend` and `ui` services and is not used
for this deployment.

Two directories are used, and every command below names the one it runs in:

| Directory | Contents |
|---|---|
| Source directory, `~/inverter-monitor` | The cloned repository. Used only to build the images and to copy `.env.example`. |
| Deployment directory, `/opt/inverter-monitor` | The deployment `compose.yaml` and `.env`. Compose is always started from here. |

## 1. Server prerequisites

* A Linux VPS (x86-64 or arm64; CI builds the images for both) with Docker
  Engine and the Docker Compose plugin (`docker compose version`).
* `git`, to get the sources for building the images.
* An MQTT broker reachable from the VPS and from the ESP32 devices (see
  [MQTT broker](#2-mqtt-broker)).
* A firewall. Docker-published ports bypass common host firewalls such as
  `ufw`, so control exposure in the Compose file (the address a port is bound
  to) or with a provider-level firewall.

**Security.** The UI, REST API and WebSocket have no authentication and no TLS,
and this configuration does not add any. It is therefore not suitable as a
public web service. A port published on all interfaces (`8090:80`, or `8080`
and `8081`) is reachable by anyone who can reach the server. Restrict access
with a suitable bind address, a VPN and/or firewall rules, and do not open the
UI, REST, WebSocket, PostgreSQL or MQTT ports to the open internet. The Compose
example below binds the UI to `127.0.0.1` for this reason; see
[Network access](#network-access).

## 2. MQTT broker

The backend connects to an existing broker; this deployment does not start one.

* Use a broker that requires a user name and password, and give the backend
  and each device credentials.
* Neither the backend nor the ESP32 firmware supports MQTT over TLS: the
  firmware connects with a plain `WiFiClient`, and credentials travel
  unencrypted. Keep the broker port (`1883` in the firmware example) inside a
  trusted network or a VPN. Do not open it to the public internet.
* The repository's `docker/mosquitto/config/mosquitto.conf` allows anonymous
  clients and is meant for local development only. It is not a production
  broker configuration.

## 3. Build the images

The repository does not publish images to a public registry, so build them on
the server from a release tag. The build contexts and target match
`.gitlab-ci.yml`. Run in the **source directory**:

```bash
git clone <repository-url> ~/inverter-monitor
cd ~/inverter-monitor
git checkout <release-tag>

docker build --target runtime -t inverter-monitor:local .
docker build -t inverter-monitor-ui:local ui
```

`inverter-monitor:local` and `inverter-monitor-ui:local` are names chosen by
this guide. They exist only in the Docker image store of this server and in no
registry. The deployment `compose.yaml` refers to them with `image:` and has no
`build:` section, so Compose does not build them: both images must exist before
the first `docker compose up`, and they are rebuilt by hand on every update
(see [Update](#7-update)).

## 4. Create the deployment directory and `.env`

Run in the **source directory** (`~/inverter-monitor`), because it copies
`.env.example` from there:

```bash
sudo mkdir -p /opt/inverter-monitor
sudo chown "$USER" /opt/inverter-monitor
cp .env.example /opt/inverter-monitor/.env
chmod 600 /opt/inverter-monitor/.env
```

Compose reads `.env` from the directory of the Compose file, so `.env` must sit
next to `/opt/inverter-monitor/compose.yaml` (step 5). Edit
`/opt/inverter-monitor/.env` and replace every placeholder:

| Variable | What to set |
|---|---|
| `POSTGRES_PASSWORD` | A new password. Required. |
| `POSTGRES_DB`, `POSTGRES_USER` | Keep the defaults or choose names. Required. |
| `MQTT_HOST` | Host name or IP address of your MQTT broker. Required, must not be empty. |
| `MQTT_PORT` | Broker port, `1883` unless your broker differs. |
| `MQTT_USERNAME`, `MQTT_PASSWORD` | Empty in `.env.example`, which is correct for a broker without authentication. If your broker requires authentication, fill in **both** with the real credentials; a password without a user name makes the backend exit. |

`.env` holds all secrets of this deployment. Set them only here, on the server.
Keep the file readable only by the deploying user, never commit it and never
copy its values into the Compose file. All variables are described in
[Configuration](configuration.md).

## 5. Write `compose.yaml`

The repository does not contain this file. After creating
`/opt/inverter-monitor` in step 4, you must create the file
`/opt/inverter-monitor/compose.yaml` yourself and save the example below into it
unchanged (for instance with `nano /opt/inverter-monitor/compose.yaml`). The
example contains no secrets and no broker address: every deployment-specific
value comes from the `.env` next to it. Keep all four `MQTT_*` lines in `.env`
(empty values are allowed for username and password); an undefined variable
makes Compose warn and substitute an empty string.

```yaml
services:
  postgres:
    image: postgres:17
    restart: unless-stopped
    environment:
      POSTGRES_DB: ${POSTGRES_DB}
      POSTGRES_USER: ${POSTGRES_USER}
      POSTGRES_PASSWORD: ${POSTGRES_PASSWORD}
    volumes:
      - postgres_data:/var/lib/postgresql/data
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U \"$${POSTGRES_USER}\" -d \"$${POSTGRES_DB}\""]
      interval: 5s
      timeout: 3s
      retries: 12

  backend:
    image: inverter-monitor:local
    restart: unless-stopped
    depends_on:
      postgres:
        condition: service_healthy
    environment:
      PGHOST: postgres
      PGPORT: "5432"
      PGDATABASE: ${POSTGRES_DB}
      PGUSER: ${POSTGRES_USER}
      PGPASSWORD: ${POSTGRES_PASSWORD}
      MQTT_HOST: ${MQTT_HOST}
      MQTT_PORT: ${MQTT_PORT}
      MQTT_USERNAME: ${MQTT_USERNAME}
      MQTT_PASSWORD: ${MQTT_PASSWORD}

  ui:
    image: inverter-monitor-ui:local
    restart: unless-stopped
    depends_on:
      - backend
    ports:
      - "127.0.0.1:8090:80"

volumes:
  postgres_data:
```

Notes:

* PostgreSQL publishes no port; only the backend reaches it over the Compose
  network.
* The backend publishes no ports. The UI container reaches it over the Compose
  network: `ui/nginx.conf` proxies `/api/` to `backend:8080` and `/ws` to
  `backend:8081`. The service name `backend` is therefore required.
* The UI is published on `127.0.0.1:8090` only, so it is not reachable from
  other machines until you choose a different address (see
  [Network access](#network-access)).
* `MQTT_HOST` and `MQTT_PORT` must be reachable from inside the `backend`
  container. `localhost` there is the container itself, not the VPS.

## 6. Start and check

Run in the **deployment directory**:

```bash
cd /opt/inverter-monitor
docker compose up -d
docker compose ps
docker compose logs -f backend
```

On startup the backend applies database migrations, retrying for up to 60 s
while PostgreSQL becomes available. Its log then shows
`Database ready after N attempt(s)`, `MQTT client is running` and
`Waiting for telemetry...`.

Check the API through the UI proxy, on the server:

```bash
curl -s http://127.0.0.1:8090/api/v1/health
```

The UI is at `http://127.0.0.1:8090/` on the server. The UI shows data for the
device ID compiled into it (`ui/src/deviceConfig.ts`, default `deye-esp32-001`).

## 7. Update

Updates are manual; nothing in this deployment updates the application
automatically.

**Back up PostgreSQL first.** Migrations only move forward, and the repository
provides no rollback and no tested restore procedure
([Architecture](architecture.md#schema-migrations)). Without a backup you cannot
return to the previous database state.

Step 1, in the **source directory**: check out the new release and rebuild the
two images under the same `:local` names.

```bash
cd ~/inverter-monitor
git fetch --tags
git checkout <new-release-tag>
docker build --target runtime -t inverter-monitor:local .
docker build -t inverter-monitor-ui:local ui
```

Step 2, in the **deployment directory**: recreate the containers. Compose uses
the newly built images; the `postgres_data` volume is kept.

```bash
cd /opt/inverter-monitor
docker compose up -d
docker compose logs -f backend
```

If a backend finds a schema newer than it knows, it refuses to start.

## 8. Persistence

* PostgreSQL data lives in the named volume `postgres_data`. Include it in your
  server backups.
* `POSTGRES_*` values are applied only when `postgres_data` is first created;
  changing them later does not change an existing database.
* The backend and UI containers are stateless.

## Network access

The UI has no login and no TLS, so decide deliberately who can reach it. Change
the UI line `"127.0.0.1:8090:80"` in `/opt/inverter-monitor/compose.yaml` only
as far as needed, then run `docker compose up -d` in the deployment directory:

* **Only on the server (default).** Keep `127.0.0.1`. To open the UI from your
  own computer, use an SSH tunnel. Run the command in a terminal on **your
  local computer**, the one whose browser you will use, not on the server:

  ```bash
  ssh -L 8090:127.0.0.1:8090 <user>@<server>
  ```

  Keep that session open and browse to `http://127.0.0.1:8090/` on your local
  computer. The first `8090` is the local port; `127.0.0.1:8090` is where the
  server connects to, and it matches the UI binding `127.0.0.1:8090:80` in the
  Compose file.
* **From a private network or VPN.** Replace `127.0.0.1` with the server's
  address on that private or VPN interface, so the port is not bound to the
  public interface. Additionally restrict it with provider-level firewall
  rules.
* **Not recommended.** `"8090:80"` binds to all interfaces, including the
  public one. Anyone who reaches the server can then read the telemetry. This
  deployment does not provide the authentication or TLS that a public web
  service needs.

The backend ports `8080` (REST) and `8081` (WebSocket) are not published. The
UI does not need them. If you need direct API access from the server itself,
add `ports: ["127.0.0.1:8080:8080"]` to the `backend` service; apply the same
rules as above before using any other address.

## Missing pieces

Not provided by the repository yet:

* A deployment Compose file in the repository (section 5 must be created by
  hand).
* Published public images; images must be built from source.
* Authentication and TLS for the UI, REST API and WebSocket.
* MQTT over TLS in the backend and the ESP32 firmware.
* Health checks for the `backend` and `ui` containers.
* A documented backup and restore procedure, and any rollback of database
  migrations.
