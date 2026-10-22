// History charts rendered as plain SVG (no charting dependency).
// Data is supplied by App (REST history merged with live WebSocket
// measurements); this view does not fetch or merge on its own.

export type HistoryMeasurement = {
  measured_at: string
  pv_power: number | null
  grid_power: number | null
  battery_power: number | null
  load_power: number | null
  battery_soc: number | null
}

type PowerSeriesKey = 'pv_power' | 'grid_power' | 'battery_power' | 'load_power'

const POWER_SERIES: { key: PowerSeriesKey; label: string; color: string }[] = [
  { key: 'pv_power', label: 'PV', color: '#a87832' },
  { key: 'grid_power', label: 'Сеть', color: '#7564a7' },
  { key: 'battery_power', label: 'Батарея', color: '#4a806c' },
  { key: 'load_power', label: 'Нагрузка', color: '#587c9b' },
]

// Logical coordinate system for the chart geometry (grid, lines, markers).
// Axis labels are rendered as separate HTML elements (see below) so their
// font size stays constant in real pixels instead of scaling with the SVG
// viewBox - the viewBox approach otherwise makes labels unreadable once the
// chart is shrunk to a narrow (e.g. ~300px) mobile container.
const CHART_WIDTH = 480
const CHART_HEIGHT = 220
const PADDING = { top: 10, right: 8, bottom: 10, left: 8 }
const INNER_WIDTH = CHART_WIDTH - PADDING.left - PADDING.right
const INNER_HEIGHT = CHART_HEIGHT - PADDING.top - PADDING.bottom
const MARKER_RADIUS = 3.4
// Small, fixed-size spread used only to separate overlapping markers that
// share the exact same timestamp. It never scales with the number of points
// or the time range, so it cannot be mistaken for a time axis.
const MARKER_JITTER_WIDTH = 14

function sortAscending(measurements: HistoryMeasurement[]): HistoryMeasurement[] {
  return [...measurements].sort((a, b) => Date.parse(a.measured_at) - Date.parse(b.measured_at))
}

function computeXPositions(times: number[]): number[] {
  if (times.length === 0) return []
  if (times.length === 1) return [INNER_WIDTH / 2]
  const min = Math.min(...times)
  const max = Math.max(...times)
  if (max === min) {
    // All timestamps are identical: there is no time range to plot against,
    // so every point shares the same X instead of being spread out evenly
    // (spreading would fabricate a time axis that doesn't exist).
    return times.map(() => INNER_WIDTH / 2)
  }
  return times.map((t) => ((t - min) / (max - min)) * INNER_WIDTH)
}

// Points with identical timestamps naturally land on the same X above. To
// keep overlapping markers visible, nudge them apart by a small bounded
// amount - this offset is only ever used for marker positions, never for the
// line path or axis ticks, so the time coordinate itself is unaffected.
function computeMarkerOffsets(times: number[]): number[] {
  const offsets = new Array(times.length).fill(0)
  let groupStart = 0
  for (let i = 1; i <= times.length; i++) {
    if (i === times.length || times[i] !== times[groupStart]) {
      const size = i - groupStart
      if (size > 1) {
        for (let j = 0; j < size; j++) {
          offsets[groupStart + j] = (j / (size - 1) - 0.5) * MARKER_JITTER_WIDTH
        }
      }
      groupStart = i
    }
  }
  return offsets
}

function computeYDomain(values: number[]): [number, number] {
  const withZero = [0, ...values]
  let min = Math.min(...withZero)
  let max = Math.max(...withZero)
  if (min === max) {
    min -= 1
    max += 1
  }
  const padding = (max - min) * 0.1
  return [min - padding, max + padding]
}

function evenTicks([min, max]: [number, number], count: number): number[] {
  return Array.from({ length: count }, (_, i) => min + ((max - min) * i) / (count - 1))
}

function yFor(value: number, [min, max]: [number, number]): number {
  return PADDING.top + INNER_HEIGHT - ((value - min) / (max - min)) * INNER_HEIGHT
}

// Groups consecutive non-null points into segments that can be safely
// connected with a line. A new segment starts whenever there's a null gap or
// a repeated timestamp. Each point keeps its original index `i` so isolated
// (single-point) segments can still look up their source measurement to
// render a marker and a tooltip without fabricating a line.
function buildSegments(xs: number[], ys: (number | null)[], times: number[]): { x: number; y: number; i: number }[][] {
  const segments: { x: number; y: number; i: number }[][] = []
  let current: { x: number; y: number; i: number }[] = []
  ys.forEach((y, i) => {
    if (y === null) {
      if (current.length > 0) segments.push(current)
      current = []
      return
    }
    // Never draw a line between two measurements that share the exact same
    // timestamp - such a line would not represent a change over time.
    const sameTimeAsPrevious = i > 0 && times[i] === times[i - 1]
    if (sameTimeAsPrevious && current.length > 0) {
      segments.push(current)
      current = []
    }
    current.push({ x: xs[i], y, i })
  })
  if (current.length > 0) segments.push(current)
  return segments
}

type AxisTickAlign = 'start' | 'middle' | 'end'
type AxisTick = { index: number; align: AxisTickAlign }

// Picks at most three X-axis labels - the first, middle, and last moments in
// time - so narrow containers never have to fit more than three. A candidate
// is dropped when it shares a timestamp with one already kept, so a repeated
// timestamp (or too few points) never produces a duplicated label or implies
// a time range that doesn't exist.
function xAxisTicks(count: number, times: number[]): AxisTick[] {
  if (count === 0) return []
  const startIndex = 0
  const endIndex = count - 1
  const middleIndex = Math.round((count - 1) / 2)
  const indices = Array.from(new Set([startIndex, middleIndex, endIndex])).sort((a, b) => a - b)

  const kept: number[] = []
  const seenTimes = new Set<number>()
  for (const index of indices) {
    if (seenTimes.has(times[index])) continue
    seenTimes.add(times[index])
    kept.push(index)
  }

  return kept.map((index, position) => {
    // With a single remaining tick there's no real start/end spread left to
    // anchor to, so it's centered instead of pinned to the left edge.
    if (kept.length === 1) return { index, align: 'middle' }
    const align: AxisTickAlign = position === 0 ? 'start' : position === kept.length - 1 ? 'end' : 'middle'
    return { index, align }
  })
}

function formatAxisTime(value: string): string {
  return `${new Intl.DateTimeFormat('ru-RU', {
    timeZone: 'UTC',
    day: '2-digit',
    month: '2-digit',
    hour: '2-digit',
    minute: '2-digit',
  }).format(new Date(value))}`
}

function formatNumber(value: number): string {
  return Number.isInteger(value) ? `${value}` : value.toFixed(1)
}

function ChartEmptyState({ message }: { message: string }) {
  return <p className="chart-empty">{message}</p>
}

export function PowerHistoryChart({ measurements }: { measurements: HistoryMeasurement[] }) {
  const sorted = sortAscending(measurements)

  if (sorted.length === 0) {
    return <ChartEmptyState message="Нет данных для графика мощности." />
  }

  const times = sorted.map((measurement) => Date.parse(measurement.measured_at))
  const xs = computeXPositions(times).map((x) => x + PADDING.left)
  const markerOffsets = computeMarkerOffsets(times)
  const allValues = POWER_SERIES.flatMap(({ key }) =>
    sorted.map((measurement) => measurement[key]).filter((value): value is number => value !== null),
  )
  const domain = computeYDomain(allValues)
  const ticks = evenTicks(domain, 4)
  const zeroInRange = domain[0] <= 0 && domain[1] >= 0
  const xTicks = xAxisTicks(sorted.length, times)

  return (
    <div className="chart-block">
      <h3 className="chart-title">Мощность, Вт</h3>
      <p className="chart-subtitle">
        Знак значений соответствует исходным данным API; направление потока (импорт/экспорт) не определено.
      </p>
      <div className="chart-row">
        <div className="chart-y-axis" aria-hidden="true">
          {ticks.map((tick) => (
            <span key={tick} className="chart-axis-label" style={{ top: `${(yFor(tick, domain) / CHART_HEIGHT) * 100}%` }}>
              {formatNumber(tick)}
            </span>
          ))}
        </div>
        <div className="chart-svg-wrap">
          <svg
            viewBox={`0 0 ${CHART_WIDTH} ${CHART_HEIGHT}`}
            className="history-chart"
            role="img"
            aria-label="График мощности PV, сети, батареи и нагрузки по времени"
          >
            {ticks.map((tick) => (
              <line
                key={tick}
                x1={PADDING.left}
                x2={CHART_WIDTH - PADDING.right}
                y1={yFor(tick, domain)}
                y2={yFor(tick, domain)}
                className="chart-grid-line"
                vectorEffect="non-scaling-stroke"
              />
            ))}
            {zeroInRange && (
              <line
                x1={PADDING.left}
                x2={CHART_WIDTH - PADDING.right}
                y1={yFor(0, domain)}
                y2={yFor(0, domain)}
                className="chart-zero-line"
                vectorEffect="non-scaling-stroke"
              />
            )}
            {POWER_SERIES.map(({ key, color }) => {
              const values = sorted.map((measurement) => measurement[key])
              const segments = buildSegments(
                xs,
                values.map((value) => (value === null ? null : yFor(value, domain))),
                times,
              )
              return (
                <g key={key}>
                  {segments.map((segment, index) =>
                    segment.length >= 2 ? (
                      <path
                        key={index}
                        d={segment.map((point, i) => `${i === 0 ? 'M' : 'L'}${point.x},${point.y}`).join(' ')}
                        className="chart-line"
                        stroke={color}
                        fill="none"
                        vectorEffect="non-scaling-stroke"
                      />
                    ) : (
                      // A single-point segment has no neighbour to connect to
                      // (null gap or a repeated timestamp broke the line), so
                      // it is shown as a dot instead of a fabricated line.
                      <circle
                        key={index}
                        cx={segment[0].x + markerOffsets[segment[0].i]}
                        cy={segment[0].y}
                        r={MARKER_RADIUS}
                        fill={color}
                      >
                        <title>{`${formatAxisTime(sorted[segment[0].i].measured_at)} UTC: ${formatNumber(values[segment[0].i] as number)} Вт`}</title>
                      </circle>
                    ),
                  )}
                </g>
              )
            })}
          </svg>
        </div>
      </div>
      <div className="chart-x-row">
        <div className="chart-x-axis-spacer" aria-hidden="true" />
        <div className="chart-x-axis" aria-hidden="true">
          {xTicks.map(({ index, align }) => (
            <span
              key={index}
              className={`chart-axis-label chart-axis-label--${align}`}
              style={{ left: `${(xs[index] / CHART_WIDTH) * 100}%` }}
            >
              {formatAxisTime(sorted[index].measured_at)}
            </span>
          ))}
        </div>
      </div>
      <ul className="chart-legend">
        {POWER_SERIES.map(({ key, color, label }) => (
          <li key={key}>
            <span className="legend-swatch" style={{ background: color }} />
            {label}
          </li>
        ))}
      </ul>
    </div>
  )
}

export function SocHistoryChart({ measurements }: { measurements: HistoryMeasurement[] }) {
  const sorted = sortAscending(measurements)

  if (sorted.length === 0) {
    return <ChartEmptyState message="Нет данных для графика заряда батареи." />
  }

  const times = sorted.map((measurement) => Date.parse(measurement.measured_at))
  const xs = computeXPositions(times).map((x) => x + PADDING.left)
  const markerOffsets = computeMarkerOffsets(times)
  const domain: [number, number] = [0, 100]
  const ticks = [0, 25, 50, 75, 100]
  const values = sorted.map((measurement) => measurement.battery_soc)
  const segments = buildSegments(
    xs,
    values.map((value) => (value === null ? null : yFor(value, domain))),
    times,
  )
  const color = '#4a806c'
  const xTicks = xAxisTicks(sorted.length, times)

  return (
    <div className="chart-block">
      <h3 className="chart-title">Заряд батареи, %</h3>
      <div className="chart-row">
        <div className="chart-y-axis" aria-hidden="true">
          {ticks.map((tick) => (
            <span key={tick} className="chart-axis-label" style={{ top: `${(yFor(tick, domain) / CHART_HEIGHT) * 100}%` }}>
              {tick}
            </span>
          ))}
        </div>
        <div className="chart-svg-wrap">
          <svg
            viewBox={`0 0 ${CHART_WIDTH} ${CHART_HEIGHT}`}
            className="history-chart"
            role="img"
            aria-label="График заряда батареи по времени"
          >
            {ticks.map((tick) => (
              <line
                key={tick}
                x1={PADDING.left}
                x2={CHART_WIDTH - PADDING.right}
                y1={yFor(tick, domain)}
                y2={yFor(tick, domain)}
                className="chart-grid-line"
                vectorEffect="non-scaling-stroke"
              />
            ))}
            {segments.map((segment, index) =>
              segment.length >= 2 ? (
                <path
                  key={index}
                  d={segment.map((point, i) => `${i === 0 ? 'M' : 'L'}${point.x},${point.y}`).join(' ')}
                  className="chart-line"
                  stroke={color}
                  fill="none"
                  vectorEffect="non-scaling-stroke"
                />
              ) : (
                // A single-point segment has no neighbour to connect to
                // (null gap or a repeated timestamp broke the line), so it is
                // shown as a dot instead of a fabricated line.
                <circle
                  key={index}
                  cx={segment[0].x + markerOffsets[segment[0].i]}
                  cy={segment[0].y}
                  r={MARKER_RADIUS}
                  fill={color}
                >
                  <title>{`${formatAxisTime(sorted[segment[0].i].measured_at)} UTC: ${formatNumber(values[segment[0].i] as number)} %`}</title>
                </circle>
              ),
            )}
          </svg>
        </div>
      </div>
      <div className="chart-x-row">
        <div className="chart-x-axis-spacer" aria-hidden="true" />
        <div className="chart-x-axis" aria-hidden="true">
          {xTicks.map(({ index, align }) => (
            <span
              key={index}
              className={`chart-axis-label chart-axis-label--${align}`}
              style={{ left: `${(xs[index] / CHART_WIDTH) * 100}%` }}
            >
              {formatAxisTime(sorted[index].measured_at)}
            </span>
          ))}
        </div>
      </div>
    </div>
  )
}
