
type TelemetrySource = 'simulator' | 'deye'

export const deviceConfig: {
  deviceId: string
  source: TelemetrySource
} = {
  deviceId: 'deye-esp32-001',
  source: 'deye',
}

export const sourceLabel =
  deviceConfig.source === 'simulator'
    ? 'Симулятор · тестовые данные'
    : 'Deye inverter'
