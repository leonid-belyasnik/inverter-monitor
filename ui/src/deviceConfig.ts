export const deviceConfig = {
  deviceId: 'simulator-001',
  source: 'simulator',
} as const

export const sourceLabel = deviceConfig.source === 'simulator'
  ? 'Симулятор · тестовые данные'
  : 'Реальное устройство'
