/******************************************************************************
**
** File      deviceConfig.ts
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

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
    ? 'Simulator · test data'
    : 'Deye inverter'
