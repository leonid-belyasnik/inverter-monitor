# Deye SUN-6K-SG05LP1-EU-AM2-P Register Map

The firmware definitions live in [`include/RegisterMap.h`](include/RegisterMap.h). This document records where each register comes from and how much it can be trusted.

Deye does not publish a complete, model-specific register map for the SG05LP1. Public community tables are written for the related single-phase LP1 family (Sunsynk/Deye 5–8 kW) and do not always agree. Treat every register that is not marked **verified** as a hypothesis.

All registers are read with Modbus function `0x03` (Read Holding Registers), slave `1`, one register per request.

## Verified Registers

Checked against the physical inverter display (see README, "Verified Hardware Test").

| Register | Meaning | Type / scale | Firmware use |
|---|---|---|---|
| `150` | Grid voltage | U16, × 0.1 V | `grid_voltage_v` |
| `184` | Battery SOC | U16, % | `battery_soc_pct` |
| `190` | Battery power | S16, W; `+` discharge, `−` charge | `battery_power_w` |
| `194` | Grid relay status | Bit 0: `1` = ON, `0` = OFF | `grid_relay_on` (relay state, not grid presence) |

Battery power sign and SOC scaling were checked under charging conditions. `battery_power_w` is published with the `+` discharge / `−` charge convention, but the discharge sign still requires hardware verification with the battery discharging.

## Candidate Registers (Unverified)

Read only in diagnostic mode and printed to the serial console. They are **not** published.

| Register | Public interpretation | Notes |
|---|---|---|
| `167` | Grid L1/LD power | Some tables name this an internal CT channel instead. |
| `169` | Grid power (L1 + L2) | Scale ×1 vs ×10 is reported differently across sub-families. |
| `172` | Grid CT power (external CT) | Different quantity from `169`; depends on CT installation. |
| `175` | Inverter AC output power | Not the same as load power. |
| `178` | Load power (L1 + L2) | At least one community table maps this to grid power instead. |
| `179` | Unknown | No public source found. |
| `186` | PV1 power | Consistent across LP1-family tables. |
| `187` | PV2 power | Consistent across LP1-family tables. |

Primary source: kellerza/sunsynk single-phase definitions,
<https://github.com/kellerza/sunsynk/blob/main/src/sunsynk/definitions/single_phase.py>.

## Production Gates

The backend parser (`backend/protocol/TelemetryParser.cpp`) requires `battery_power_w` and `battery_soc_pct`. `pv_power_w`, `grid_power_w`, and `load_power_w` may be absent or `null`; the backend stores them as `NULL` and returns `null` through REST and WebSocket.

While a gate below is `false`, the firmware omits that field from the payload. It never fills a missing field with zero or an estimate. Battery SOC and battery power are published regardless of these gates.

**Current state: all three gates are `true` temporarily, and the registers are still UNCONFIRMED.** They were enabled by decision for the local beta, not after comparison with the inverter display. The published PV, grid, and load values may have the wrong scale, sign, or meaning. Candidate `172` (external grid CT) is the alternative to `169` for grid power.

| Field | Gate in `RegisterMap.h` | Register(s) used | Status |
|---|---|---|---|
| `pv_power_w` | `PV_POWER_VERIFIED` | `PV1_POWER` (186) + `PV2_POWER` (187) | Unconfirmed |
| `grid_power_w` | `GRID_POWER_VERIFIED` | `GRID_POWER` (169) | Unconfirmed |
| `load_power_w` | `LOAD_POWER_VERIFIED` | `LOAD_POWER` (178) | Unconfirmed |

All three are read as S16 watts with scale ×1. If verification shows a different register, scale, or sign, update `RegisterMap.h` (and `pollRegisters()` in `src/main.cpp` if the scale differs) before enabling the gate.

The backend does not yet define the sign convention for `grid_power_w` (see `docs/ui/telemetry-and-freshness.md`). The firmware publishes the register value as read.

## Verification Procedure

1. Keep `DIAGNOSTIC_MODE_ENABLED = true` in `include/Config.h` and open the serial monitor.
2. For each candidate, compare the printed raw/signed value with the inverter display (or the Deye app) under several conditions:
   - PV producing vs. night;
   - battery charging vs. discharging;
   - grid importing vs. exporting (if export is enabled);
   - low vs. high household load.
3. Record the confirmed register, scale, and sign in this document.
4. Set the corresponding gate in `RegisterMap.h` to `true`, rebuild, and confirm the field appears in the published payload on the broker.
