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
    throw new Error('WebSocket прислал сообщение в неожиданном формате.')
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
  throw new Error('WebSocket прислал неизвестное сообщение.')
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
    title: 'Напряжение сети',
    icon: 'grid',
    tone: 'violet',
    description: 'Значение как получено от устройства',
    key: 'grid_voltage',
    unit: 'В',
    fractionDigits: 1,
    emptyText: 'Нет данных',
  },
  {
    title: 'Мощность сети',
    icon: 'grid',
    tone: 'violet',
    description: 'Значение как получено; направление не определено',
    key: 'grid_power',
    unit: 'Вт',
  },
  {
    title: 'Заряд батареи',
    icon: 'battery',
    tone: 'mint',
    description: 'Состояние аккумулятора',
    key: 'battery_soc',
    unit: '%',
  },
  {
    title: 'Мощность батареи',
    icon: 'battery',
    tone: 'mint',
    description: 'Значение как получено; направление не подтверждено',
    key: 'battery_power',
    unit: 'Вт',
  },
  {
    title: 'Мощность солнечных панелей',
    icon: 'solar',
    tone: 'amber',
    description: 'Текущая генерация PV',
    key: 'pv_power',
    unit: 'Вт',
  },
  {
    title: 'Потребление дома',
    icon: 'home',
    tone: 'blue',
    description: 'Текущая нагрузка',
    key: 'load_power',
    unit: 'Вт',
  },
]

function parseMeasurements(payload: unknown): Measurement[] {
  if (typeof payload !== 'object' || payload === null || Array.isArray(payload)) {
    throw new Error('Сервер вернул ответ в неожиданном формате.')
  }

  const response = payload as Record<string, unknown>
  if (response.device_id !== deviceConfig.deviceId || !Array.isArray(response.measurements)) {
    throw new Error('Ответ сервера не соответствует выбранному устройству.')
  }

  return response.measurements.map((value, index) => {
    if (typeof value !== 'object' || value === null || Array.isArray(value)) {
      throw new Error(`Измерение №${index + 1} имеет неверный формат.`)
    }

    const record = value as Record<string, unknown>
    if (typeof record.measured_at !== 'string' || Number.isNaN(Date.parse(record.measured_at))) {
      throw new Error(`У измерения №${index + 1} отсутствует корректное время.`)
    }

    const readValue = (key: MeasurementKey): number | null => {
      const field = record[key]
      if (field === undefined || field === null) return null
      if (typeof field !== 'number' || !Number.isFinite(field)) {
        throw new Error(`Поле ${key} в измерении №${index + 1} имеет неверный формат.`)
      }
      return field
    }

    const readBoolean = (key: 'grid_relay_on'): boolean | null => {
      const field = record[key]
      if (field === undefined || field === null) return null
      if (typeof field !== 'boolean') {
        throw new Error(`Поле ${key} в измерении №${index + 1} имеет неверный формат.`)
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
  if (value === null) return 'Неизвестно'
  return value ? 'Включено' : 'Отключено'
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
  if (value === null) return 'Недоступно'
  if (fractionDigits === undefined) return String(value)
  return value.toLocaleString('ru-RU', {
    minimumFractionDigits: fractionDigits,
    maximumFractionDigits: fractionDigits,
  })
}

function formatMeasurementTime(value: string): string {
  return `${new Intl.DateTimeFormat('ru-RU', {
    timeZone: 'UTC',
    dateStyle: 'medium',
    timeStyle: 'medium',
  }).format(new Date(value))} UTC`
}

function formatBrowserTime(value: number | null): string {
  if (value === null) return 'Ещё не получено'
  return new Intl.DateTimeFormat('ru-RU', { dateStyle: 'medium', timeStyle: 'medium' }).format(value)
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
        throw new Error(`Не удалось загрузить измерения (HTTP ${response.status}).`)
      }

      const measurements = parseMeasurements(await response.json())
      return { status: 'success', measurements }
    } catch (error) {
      const message = error instanceof SyntaxError
        ? 'Сервер вернул некорректный JSON-ответ.'
        : error instanceof TypeError
          ? 'Не удалось связаться с API. Проверьте подключение и попробуйте ещё раз.'
          : error instanceof Error
            ? error.message
            : 'Не удалось загрузить измерения.'
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
              throw new Error('Подтверждение подписки относится к другому устройству.')
            }
            subscribed = true
            retry = 0
            setSocketState('connected')
            setSocketError(null)
            setSubscriptionStartedAt((startedAt) => startedAt ?? Date.now())
          } else if (message.type === 'error') {
            throw new Error(`Ошибка подписки: ${message.message}`)
          } else {
            if (message.device_id !== deviceConfig.deviceId) return
            if (!subscribed) throw new Error('Измерение получено до подтверждения подписки.')
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
          setSocketError(error instanceof Error ? error.message : 'Ошибка данных WebSocket.')
          currentSocket.close()
        }
      }
      currentSocket.onerror = () => {
        setSocketState('error')
        setSocketError('Не удалось подключиться к WebSocket.')
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
    : 'Нет данных'

  return (
    <div className="app-shell">
      <header className="topbar">
        <a className="brand" href="#overview" aria-label="HomePowerGuard — главная">
          <span className="brand-mark" aria-hidden="true">
            <svg fill="none" viewBox="0 0 36 36">
              <path d="m5 16 13-10 13 10v14H5V16Z" />
              <path d="M20 12 13 21h5l-2 7 8-10h-5l1-6Z" />
            </svg>
          </span>
          <span className="brand-name">HomePowerGuard</span>
        </a>

        <nav className="main-nav" aria-label="Основная навигация">
          <a className="nav-link active" href="#overview">Обзор</a>
          <a className="nav-link" href="#history">История</a>
        </nav>

        <div className={`connection-pill ${apiState.status === 'success' ? 'connected' : ''}`} role="status">
          <span className="connection-dot" />
          <span>{apiState.status === 'loading' ? 'Загрузка' : apiState.status === 'error' ? 'Ошибка API' : 'API доступен'}</span>
        </div>
      </header>

      <main>
        <section className="overview-section" id="overview" aria-labelledby="page-title">
          <div className="page-heading">
            <div>
              <p className="eyebrow">МОНИТОРИНГ ЭНЕРГОСИСТЕМЫ</p>
              <h1 id="page-title">Энергия вашего дома</h1>
              <p className="page-description">
                Основные показатели системы и состояние телеметрии в одном месте.
              </p>
              <p className="source-label">Источник: {sourceLabel} <span>·</span> Устройство: {deviceConfig.deviceId}</p>
            </div>
            <div className="data-state-card">
              <span className="state-icon"><Icon name="history" /></span>
              <span className="state-copy">
                <span className="state-label">Состояние данных</span>
                <strong>{telemetryState === 'stale' ? 'Нет недавней телеметрии' : telemetryState === 'waiting' ? 'Ожидаем телеметрию' : 'Телеметрия обновляется'}</strong>
              </span>
            </div>
          </div>

          <div className="metrics-grid" aria-label="Показатели энергосистемы">
            {metrics.map((metric) => (
              <Fragment key={metric.title}>
                <article className="metric-card">
                  <div className="metric-card-top">
                    <span className={`metric-icon ${metric.tone}`}><Icon name={metric.icon} /></span>
                    <span className="unavailable-tag">
                      {metric.key === null ? 'Недоступно' : apiState.status === 'loading' ? 'Загрузка' : latestMeasurement?.[metric.key] !== null && latestMeasurement?.[metric.key] !== undefined ? 'Данные API' : metric.emptyText ?? 'Недоступно'}
                    </span>
                  </div>
                  <h2>{metric.title}</h2>
                  <p className="metric-value">
                    {metric.key === null
                      ? 'Недоступно'
                      : apiState.status === 'loading'
                      ? 'Загрузка…'
                      : latestMeasurement?.[metric.key] !== null && latestMeasurement?.[metric.key] !== undefined
                        ? `${formatMetricNumber(latestMeasurement[metric.key], metric.fractionDigits)} ${metric.unit}`
                        : metric.emptyText ?? 'Недоступно'}
                  </p>
                  <p className="metric-description">{metric.description}</p>
                  {metric.key && latestMeasurement && <p className="metric-time">Измерено: {latestTime}</p>}
                </article>
                {metric.key === 'grid_voltage' && (
                  <article className="metric-card">
                    <div className="metric-card-top">
                      <span className="metric-icon violet"><Icon name="grid" /></span>
                      <span className="unavailable-tag">
                        {apiState.status === 'loading' ? 'Загрузка' : latestMeasurement?.grid_relay_on != null ? 'Данные API' : 'Нет данных'}
                      </span>
                    </div>
                    <h2>Состояние реле сети</h2>
                    <p className="metric-value">
                      {apiState.status === 'loading' ? 'Загрузка…' : formatGridRelay(latestMeasurement?.grid_relay_on ?? null)}
                    </p>
                    <p className="metric-description">Реле инвертора; не подтверждает наличие или отсутствие напряжения во внешней сети</p>
                    {latestMeasurement && <p className="metric-time">Измерено: {latestTime}</p>}
                  </article>
                )}
              </Fragment>
            ))}
          </div>

          <section className="timestamps-card" aria-labelledby="timestamps-title">
            <div className="timestamps-heading">
              <span className="timestamps-icon"><Icon name="clock" /></span>
              <div>
                <h2 id="timestamps-title">Обновление данных</h2>
                <p>Время измерения из API; получение браузером — отдельная отметка. Порог свежести 15 секунд предварительный.</p>
              </div>
            </div>
            <div className="timestamp-values">
              <div className="timestamp-item">
                <span>Последнее измерение</span>
                <strong>{latestTime}</strong>
              </div>
              <div className="timestamp-item">
                <span>Получено браузером</span>
                <strong>{formatBrowserTime(lastReceiptAt)}</strong>
              </div>
              <div className="timestamp-item">
                <span>WebSocket</span>
                <strong>{socketState === 'connected' ? 'Подписка активна' : socketState === 'connecting' ? 'Подключение' : socketState === 'reconnecting' ? 'Повторное подключение' : socketState === 'disconnected' ? 'Отключён' : 'Ошибка соединения'}</strong>
              </div>
              <div className="timestamp-item">
                <span>Актуальность</span>
                <strong>{telemetryState === 'updating' ? `Обновлено ${Math.floor((now - (lastReceiptAt ?? now)) / 1000)} с назад` : telemetryState === 'stale' ? 'Нет обновлений более 15 секунд' : 'Ожидание первого сообщения'}</strong>
              </div>
            </div>
          </section>

          {socketError && <p className="request-message" role="status">WebSocket: {socketError} Повторное подключение выполняется автоматически.</p>}

          {apiState.status === 'loading' && <p className="request-message" role="status">Загружаем измерения устройства…</p>}
          {apiState.status === 'error' && (
            <div className="request-error" role="alert">
              <p>{apiState.message}</p>
              <button type="button" onClick={() => void loadMeasurements()}>Повторить запрос</button>
            </div>
          )}
          {apiState.status === 'success' && chartHistory.length === 0 && (
            <p className="request-message">История измерений пока пуста.</p>
          )}
        </section>

        <section className="history-section" id="history" aria-labelledby="history-title">
          <div className="section-heading">
            <div>
              <p className="eyebrow">ДИНАМИКА</p>
              <h2 id="history-title">История измерений</h2>
            </div>
            <span className="history-period">{apiState.status === 'success' ? `Записей: ${chartHistory.length}` : 'Последние измерения'}</span>
          </div>
          {apiState.status === 'success' && chartHistory.length > 0 ? (
            <div className="history-charts">
              <p className="history-note">
                Показано {chartHistory.length} последних записей (не более {HISTORY_LIMIT}) — более старые измерения в
                выборку не входят. История загружается по REST при открытии страницы и дополняется новыми измерениями
                по WebSocket без перезагрузки.
              </p>
              <PowerHistoryChart measurements={chartHistory} />
              <SocHistoryChart measurements={chartHistory} />
            </div>
          ) : (
            <div className="history-empty">
              <span className="empty-icon"><Icon name="history" /></span>
              <h3>{apiState.status === 'loading' ? 'Загружаем историю' : apiState.status === 'error' ? 'История недоступна' : 'История измерений'}</h3>
              <p>{apiState.status === 'error'
                ? 'Не удалось получить записи истории.'
                : 'История появится здесь, когда будут доступны измерения.'}</p>
            </div>
          )}
        </section>
      </main>

      <footer className="page-footer">
        <span>HomePowerGuard</span>
        <span>Локальный мониторинг энергосистемы</span>
      </footer>
    </div>
  )
}

export default App
