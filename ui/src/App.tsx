/******************************************************************************
**
** File      App.tsx
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

import './App.css'
import { Fragment, useCallback, useEffect, useRef, useState } from 'react'
import { deviceConfig, sourceLabel } from './deviceConfig'
import { PowerHistoryChart, SocHistoryChart } from './HistoryCharts'

type IconName = 'grid' | 'battery' | 'solar' | 'home' | 'clock' | 'history'

function Icon({ name }: { name: IconName }) {
  const paths: Record<IconName, string> = {
    grid: 'M12 3v18M3 12h18M5.6 5.6l12.8 12.8m0-12.8L5.6 18.4',
    battery: 'M7 7h10v14H7zM10 4h4v3h-4zM10 11h4m-2-2v4',
    solar: 'M12 8a4 4 0 1 0 0 8 4 4 0 0 0 0-8ZM12 2v2m0 16v2M4.93 4.93l1.42 1.42m11.3 11.3 1.42 1.42M2 12h2m16 0h2M4.93 19.07l1.42-1.42m11.3-11.3 1.42-1.42',
    home: 'm3 10 9-7 9 7v10a1 1 0 0 1-1 1h-5v-7h-6v7H4a1 1 0 0 1-1-1z',
    clock: 'M12 22a10 10 0 1 0 0-20 10 10 0 0 0 0 20Zm0-16v6l4 2',
    history: 'M3 12a9 9 0 1 0 2.6-6.4L3 8m0-5v5h5m4-1v5l3 2',
  }

  return (
    <svg
      aria-hidden="true"
      className="icon"
      fill="none"
      stroke="currentColor"
      strokeLinecap="round"
      strokeLinejoin="round"
      strokeWidth="1.7"
      viewBox="0 0 24 24"
    >
      <path d={paths[name]} />
    </svg>
  )
}

type Measurement = {
  measured_at: string
  pv_power: number | null
  grid_power: number | null
  battery_power: number | null
  load_power: number | null
  battery_soc: number | null
  grid_voltage: number | null
  grid_relay_on: boolean | null
}

type SocketState = 'connecting' | 'connected' | 'reconnecting' | 'disconnected' | 'error'

type TelemetryState = 'waiting' | 'updating' | 'stale'

const STALE_AFTER_MS = 15_000
const RECONNECT_BASE_MS = 1_000
const RECONNECT_MAX_MS = 30_000

function parseWebSocketMessage(payload: unknown):
  | { type: 'subscribed'; device_id: string }
  | { type: 'measurement'; device_id: string; data: unknown }
  | { type: 'error'; message: string } {
  if (typeof payload !== 'object' || payload === null || Array.isArray(payload)) {
    throw new Error('WebSocket sent a message in an unexpected format.')
  }

  const message = payload as Record<string, unknown>
  if (message.type === 'subscribed' && typeof message.device_id === 'string') {
    return { type: 'subscribed', device_id: message.device_id }
  }
  if (message.type === 'error' && typeof message.message === 'string') {
    return { type: 'error', message: message.message }
  }
  if (message.type === 'measurement' && typeof message.device_id === 'string') {
    return { type: 'measurement', device_id: message.device_id, data: message.data }
  }
  throw new Error('WebSocket sent an unknown message.')
}

type ApiState =
  | { status: 'loading' }
  | { status: 'error'; message: string }
  | { status: 'success'; measurements: Measurement[] }

type MetricKey = 'battery_soc' | 'pv_power' | 'load_power' | 'grid_power' | 'battery_power' | 'grid_voltage'
type MeasurementKey = MetricKey

const HISTORY_LIMIT = 100

const metrics: {
  title: string
  icon: IconName
  tone: string
  description: string
  key: MetricKey | null
  unit: string
  fractionDigits?: number
  emptyText?: string
}[] = [
  {
    title: 'Grid voltage',
    icon: 'grid',
    tone: 'violet',
    description: 'Value as received from the device',
    key: 'grid_voltage',
    unit: 'V',
    fractionDigits: 1,
    emptyText: 'No data',
  },
  {
    title: 'Grid power',
    icon: 'grid',
    tone: 'violet',
    description: 'Value as received; direction not determined',
    key: 'grid_power',
    unit: 'W',
  },
  {
    title: 'Battery charge',
    icon: 'battery',
    tone: 'mint',
    description: 'Battery state of charge',
    key: 'battery_soc',
    unit: '%',
  },
  {
    title: 'Battery power',
    icon: 'battery',
    tone: 'mint',
    description: 'Value as received; direction not confirmed',
    key: 'battery_power',
    unit: 'W',
  },
  {
    title: 'Solar panel power',
    icon: 'solar',
    tone: 'amber',
    description: 'Current PV generation',
    key: 'pv_power',
    unit: 'W',
  },
  {
    title: 'Home consumption',
    icon: 'home',
    tone: 'blue',
    description: 'Current load',
    key: 'load_power',
    unit: 'W',
  },
]

function parseMeasurements(payload: unknown): Measurement[] {
  if (typeof payload !== 'object' || payload === null || Array.isArray(payload)) {
    throw new Error('The server returned a response in an unexpected format.')
  }

  const response = payload as Record<string, unknown>
  if (response.device_id !== deviceConfig.deviceId || !Array.isArray(response.measurements)) {
    throw new Error('The server response does not match the selected device.')
  }

  return response.measurements.map((value, index) => {
    if (typeof value !== 'object' || value === null || Array.isArray(value)) {
      throw new Error(`Measurement #${index + 1} has an invalid format.`)
    }

    const record = value as Record<string, unknown>
    if (typeof record.measured_at !== 'string' || Number.isNaN(Date.parse(record.measured_at))) {
      throw new Error(`Measurement #${index + 1} has a missing or invalid timestamp.`)
    }

    const readValue = (key: MeasurementKey): number | null => {
      const field = record[key]
      if (field === undefined || field === null) return null
      if (typeof field !== 'number' || !Number.isFinite(field)) {
        throw new Error(`Field ${key} in measurement #${index + 1} has an invalid format.`)
      }
      return field
    }

    const readBoolean = (key: 'grid_relay_on'): boolean | null => {
      const field = record[key]
      if (field === undefined || field === null) return null
      if (typeof field !== 'boolean') {
        throw new Error(`Field ${key} in measurement #${index + 1} has an invalid format.`)
      }
      return field
    }

    return {
      measured_at: record.measured_at,
      battery_soc: readValue('battery_soc'),
      pv_power: readValue('pv_power'),
      grid_power: readValue('grid_power'),
      battery_power: readValue('battery_power'),
      load_power: readValue('load_power'),
      grid_voltage: readValue('grid_voltage'),
      grid_relay_on: readBoolean('grid_relay_on'),
    }
  })
}

// Relay state only; OFF does not prove that the external grid has no voltage.
function formatGridRelay(value: boolean | null): string {
  if (value === null) return 'Unknown'
  return value ? 'On' : 'Off'
}

// Live points replace REST points with the same parsed measured_at; the API has no stable record ID.
function mergeHistory(restHistory: Measurement[], liveHistory: Measurement[]): Measurement[] {
  const liveTimes = new Set(liveHistory.map((measurement) => Date.parse(measurement.measured_at)))
  return [
    ...restHistory.filter((measurement) => !liveTimes.has(Date.parse(measurement.measured_at))),
    ...liveHistory,
  ]
    .sort((a, b) => Date.parse(a.measured_at) - Date.parse(b.measured_at))
    .slice(-HISTORY_LIMIT)
}

// Without fractionDigits the number is shown exactly as received. null is never shown as a number.
function formatMetricNumber(value: number | null, fractionDigits?: number): string {
  if (value === null) return 'Unavailable'
  if (fractionDigits === undefined) return String(value)
  return value.toLocaleString('en-US', {
    minimumFractionDigits: fractionDigits,
    maximumFractionDigits: fractionDigits,
  })
}

function formatMeasurementTime(value: string): string {
  return `${new Intl.DateTimeFormat('en-US', {
    timeZone: 'UTC',
    dateStyle: 'medium',
    timeStyle: 'medium',
  }).format(new Date(value))} UTC`
}

function formatBrowserTime(value: number | null): string {
  if (value === null) return 'Not received yet'
  return new Intl.DateTimeFormat('en-US', { dateStyle: 'medium', timeStyle: 'medium' }).format(value)
}

function App() {
  const [apiState, setApiState] = useState<ApiState>({ status: 'loading' })
  const [socketState, setSocketState] = useState<SocketState>('connecting')
  const [socketError, setSocketError] = useState<string | null>(null)
  const [liveMeasurement, setLiveMeasurement] = useState<Measurement | null>(null)
  const [liveHistory, setLiveHistory] = useState<Measurement[]>([])
  const [lastReceiptAt, setLastReceiptAt] = useState<number | null>(null)
  const [now, setNow] = useState(0)
  const [subscriptionStartedAt, setSubscriptionStartedAt] = useState<number | null>(null)
  const liveMeasurementRef = useRef<Measurement | null>(null)

  const requestMeasurements = useCallback(async (): Promise<ApiState> => {
    try {
      const response = await fetch(
        `/api/v1/devices/${encodeURIComponent(deviceConfig.deviceId)}/measurements?limit=${HISTORY_LIMIT}`,
      )
      if (!response.ok) {
        throw new Error(`Failed to load measurements (HTTP ${response.status}).`)
      }

      const measurements = parseMeasurements(await response.json())
      return { status: 'success', measurements }
    } catch (error) {
      const message = error instanceof SyntaxError
        ? 'The server returned an invalid JSON response.'
        : error instanceof TypeError
          ? 'Could not reach the API. Check your connection and try again.'
          : error instanceof Error
            ? error.message
            : 'Failed to load measurements.'
      return {
        status: 'error',
        message,
      }
    }
  }, [])

  const loadMeasurements = useCallback(() => {
    setApiState({ status: 'loading' })
    void requestMeasurements().then(setApiState)
  }, [requestMeasurements])

  useEffect(() => {
    let active = true
    let socket: WebSocket | null = null
    let reconnectTimer: number | undefined
    let retry = 0
    let intentionallyClosed = false

    const connect = () => {
      if (!active) return
      setSocketState(retry === 0 ? 'connecting' : 'reconnecting')
      setSocketError(null)
      const scheme = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
      const currentSocket = new WebSocket(`${scheme}//${window.location.host}/ws`)
      socket = currentSocket
      let subscribed = false

      currentSocket.onopen = () => {
        currentSocket.send(JSON.stringify({ type: 'subscribe', device_id: deviceConfig.deviceId }))
      }
      currentSocket.onmessage = (event: MessageEvent<string>) => {
        try {
          const message = parseWebSocketMessage(JSON.parse(event.data) as unknown)
          if (message.type === 'subscribed') {
            if (message.device_id !== deviceConfig.deviceId) {
              throw new Error('The subscription confirmation is for a different device.')
            }
            subscribed = true
            retry = 0
            setSocketState('connected')
            setSocketError(null)
            setSubscriptionStartedAt((startedAt) => startedAt ?? Date.now())
          } else if (message.type === 'error') {
            throw new Error(`Subscription error: ${message.message}`)
          } else {
            if (message.device_id !== deviceConfig.deviceId) return
            if (!subscribed) throw new Error('A measurement was received before the subscription was confirmed.')
            const measurement = parseMeasurements({
              device_id: deviceConfig.deviceId,
              measurements: [message.data],
            })[0]
            const previousMeasurement = liveMeasurementRef.current
            if (previousMeasurement && Date.parse(measurement.measured_at) < Date.parse(previousMeasurement.measured_at)) return
            liveMeasurementRef.current = measurement
            setLiveMeasurement(measurement)
            const measuredAt = Date.parse(measurement.measured_at)
            setLiveHistory((previous) => [
              ...previous.filter((item) => Date.parse(item.measured_at) !== measuredAt),
              measurement,
            ].slice(-HISTORY_LIMIT))
            setLastReceiptAt(Date.now())
          }
        } catch (error) {
          setSocketState('error')
          setSocketError(error instanceof Error ? error.message : 'WebSocket data error.')
          currentSocket.close()
        }
      }
      currentSocket.onerror = () => {
        setSocketState('error')
        setSocketError('Could not connect to WebSocket.')
      }
      currentSocket.onclose = () => {
        if (!active || intentionallyClosed) return
        setSocketState('reconnecting')
        const delay = Math.min(RECONNECT_BASE_MS * 2 ** retry, RECONNECT_MAX_MS)
        retry += 1
        reconnectTimer = window.setTimeout(connect, delay)
      }
    }

    void requestMeasurements().then((result) => {
      if (active) setApiState(result)
    })
    connect()

    return () => {
      active = false
      intentionallyClosed = true
      if (reconnectTimer !== undefined) window.clearTimeout(reconnectTimer)
      socket?.close()
    }
  }, [requestMeasurements])

  useEffect(() => {
    const timer = window.setInterval(() => setNow(Date.now()), 1_000)
    return () => window.clearInterval(timer)
  }, [])

  const history = apiState.status === 'success' ? apiState.measurements : []
  const chartHistory = apiState.status === 'success' ? mergeHistory(history, liveHistory) : []
  const latestRestMeasurement = history.reduce<Measurement | null>((latest, measurement) => (
    latest === null || Date.parse(measurement.measured_at) > Date.parse(latest.measured_at)
      ? measurement
      : latest
  ), null)
  // On equal measured_at timestamps, prefer the live value; the API has no stable record ID.
  const latestMeasurement = liveMeasurement && (
    latestRestMeasurement === null || Date.parse(liveMeasurement.measured_at) >= Date.parse(latestRestMeasurement.measured_at)
  ) ? liveMeasurement : latestRestMeasurement
  const telemetryState: TelemetryState = lastReceiptAt === null
    ? subscriptionStartedAt !== null && now - subscriptionStartedAt >= STALE_AFTER_MS ? 'stale' : 'waiting'
    : now - lastReceiptAt >= STALE_AFTER_MS ? 'stale' : 'updating'
  const latestTime = latestMeasurement
    ? formatMeasurementTime(latestMeasurement.measured_at)
    : 'No data'

  return (
    <div className="app-shell">
      <header className="topbar">
        <a className="brand" href="#overview" aria-label="HomePowerGuard — home">
          <span className="brand-mark" aria-hidden="true">
            <svg fill="none" viewBox="0 0 36 36">
              <path d="m5 16 13-10 13 10v14H5V16Z" />
              <path d="M20 12 13 21h5l-2 7 8-10h-5l1-6Z" />
            </svg>
          </span>
          <span className="brand-name">HomePowerGuard</span>
        </a>

        <nav className="main-nav" aria-label="Main navigation">
          <a className="nav-link active" href="#overview">Overview</a>
          <a className="nav-link" href="#history">History</a>
        </nav>

        <div className={`connection-pill ${apiState.status === 'success' ? 'connected' : ''}`} role="status">
          <span className="connection-dot" />
          <span>{apiState.status === 'loading' ? 'Loading' : apiState.status === 'error' ? 'API error' : 'API available'}</span>
        </div>
      </header>

      <main>
        <section className="overview-section" id="overview" aria-labelledby="page-title">
          <div className="page-heading">
            <div>
              <p className="eyebrow">ENERGY SYSTEM MONITORING</p>
              <h1 id="page-title">Your home energy</h1>
              <p className="page-description">
                Key system metrics and telemetry status in one place.
              </p>
              <p className="source-label">Source: {sourceLabel} <span>·</span> Device: {deviceConfig.deviceId}</p>
            </div>
            <div className="data-state-card">
              <span className="state-icon"><Icon name="history" /></span>
              <span className="state-copy">
                <span className="state-label">Data status</span>
                <strong>{telemetryState === 'stale' ? 'No recent telemetry' : telemetryState === 'waiting' ? 'Waiting for telemetry' : 'Telemetry is updating'}</strong>
              </span>
            </div>
          </div>

          <div className="metrics-grid" aria-label="Energy system metrics">
            {metrics.map((metric) => (
              <Fragment key={metric.title}>
                <article className="metric-card">
                  <div className="metric-card-top">
                    <span className={`metric-icon ${metric.tone}`}><Icon name={metric.icon} /></span>
                    <span className="unavailable-tag">
                      {metric.key === null ? 'Unavailable' : apiState.status === 'loading' ? 'Loading' : latestMeasurement?.[metric.key] !== null && latestMeasurement?.[metric.key] !== undefined ? 'API data' : metric.emptyText ?? 'Unavailable'}
                    </span>
                  </div>
                  <h2>{metric.title}</h2>
                  <p className="metric-value">
                    {metric.key === null
                      ? 'Unavailable'
                      : apiState.status === 'loading'
                      ? 'Loading…'
                      : latestMeasurement?.[metric.key] !== null && latestMeasurement?.[metric.key] !== undefined
                        ? `${formatMetricNumber(latestMeasurement[metric.key], metric.fractionDigits)} ${metric.unit}`
                        : metric.emptyText ?? 'Unavailable'}
                  </p>
                  <p className="metric-description">{metric.description}</p>
                  {metric.key && latestMeasurement && <p className="metric-time">Measured: {latestTime}</p>}
                </article>
                {metric.key === 'grid_voltage' && (
                  <article className="metric-card">
                    <div className="metric-card-top">
                      <span className="metric-icon violet"><Icon name="grid" /></span>
                      <span className="unavailable-tag">
                        {apiState.status === 'loading' ? 'Loading' : latestMeasurement?.grid_relay_on != null ? 'API data' : 'No data'}
                      </span>
                    </div>
                    <h2>Grid relay status</h2>
                    <p className="metric-value">
                      {apiState.status === 'loading' ? 'Loading…' : formatGridRelay(latestMeasurement?.grid_relay_on ?? null)}
                    </p>
                    <p className="metric-description">Inverter relay; does not confirm whether voltage is present on the external grid</p>
                    {latestMeasurement && <p className="metric-time">Measured: {latestTime}</p>}
                  </article>
                )}
              </Fragment>
            ))}
          </div>

          <section className="timestamps-card" aria-labelledby="timestamps-title">
            <div className="timestamps-heading">
              <span className="timestamps-icon"><Icon name="clock" /></span>
              <div>
                <h2 id="timestamps-title">Data updates</h2>
                <p>Measurement time comes from the API; the time received by the browser is a separate timestamp. The 15-second freshness threshold is provisional.</p>
              </div>
            </div>
            <div className="timestamp-values">
              <div className="timestamp-item">
                <span>Latest measurement</span>
                <strong>{latestTime}</strong>
              </div>
              <div className="timestamp-item">
                <span>Received by browser</span>
                <strong>{formatBrowserTime(lastReceiptAt)}</strong>
              </div>
              <div className="timestamp-item">
                <span>WebSocket</span>
                <strong>{socketState === 'connected' ? 'Subscribed' : socketState === 'connecting' ? 'Connecting' : socketState === 'reconnecting' ? 'Reconnecting' : socketState === 'disconnected' ? 'Disconnected' : 'Connection error'}</strong>
              </div>
              <div className="timestamp-item">
                <span>Freshness</span>
                <strong>{telemetryState === 'updating' ? `Updated ${Math.floor((now - (lastReceiptAt ?? now)) / 1000)} s ago` : telemetryState === 'stale' ? 'No updates for over 15 seconds' : 'Waiting for the first message'}</strong>
              </div>
            </div>
          </section>

          {socketError && <p className="request-message" role="status">WebSocket: {socketError} Reconnecting automatically.</p>}

          {apiState.status === 'loading' && <p className="request-message" role="status">Loading device measurements…</p>}
          {apiState.status === 'error' && (
            <div className="request-error" role="alert">
              <p>{apiState.message}</p>
              <button type="button" onClick={() => void loadMeasurements()}>Retry request</button>
            </div>
          )}
          {apiState.status === 'success' && chartHistory.length === 0 && (
            <p className="request-message">The measurement history is empty.</p>
          )}
        </section>

        <section className="history-section" id="history" aria-labelledby="history-title">
          <div className="section-heading">
            <div>
              <p className="eyebrow">TRENDS</p>
              <h2 id="history-title">Measurement history</h2>
            </div>
            <span className="history-period">{apiState.status === 'success' ? `Records: ${chartHistory.length}` : 'Latest measurements'}</span>
          </div>
          {apiState.status === 'success' && chartHistory.length > 0 ? (
            <div className="history-charts">
              <p className="history-note">
                Showing the latest {chartHistory.length} records (at most {HISTORY_LIMIT}); older measurements are
                not included. History is loaded over REST when the page opens and is extended with new measurements
                over WebSocket without a reload.
              </p>
              <PowerHistoryChart measurements={chartHistory} />
              <SocHistoryChart measurements={chartHistory} />
            </div>
          ) : (
            <div className="history-empty">
              <span className="empty-icon"><Icon name="history" /></span>
              <h3>{apiState.status === 'loading' ? 'Loading history' : apiState.status === 'error' ? 'History unavailable' : 'Measurement history'}</h3>
              <p>{apiState.status === 'error'
                ? 'Could not retrieve history records.'
                : 'History will appear here once measurements are available.'}</p>
            </div>
          )}
        </section>
      </main>

      <footer className="page-footer">
        <span>HomePowerGuard</span>
        <span>Local energy system monitoring</span>
      </footer>
    </div>
  )
}

export default App
