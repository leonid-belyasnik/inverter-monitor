#pragma once

#include <cstddef>
#include <cstdint>

// Deye SUN-6K-SG05LP1-EU-AM2-P Modbus register map.
//
// IMPORTANT: Deye does not publish one official register map for every
// model. SG05LP1 is explicitly called out by community sources as an
// "unconfirmed complete model map" that borrows from the older SG0xLP1
// single-phase family. See REGISTER_MAP.md for full source URLs, the
// disagreements between sources, and the verification procedure.
//
// This header only contains two kinds of entries:
//  - VERIFIED registers: confirmed against the physical inverter display
//    (see README.md "Verified Hardware Test"). Safe to use in production.
//  - CANDIDATE registers: hypotheses collected from public projects, not
//    verified against this inverter. Read-only diagnostic use only. Do NOT
//    treat these as production telemetry until confirmed.
namespace registers {

// ---- Verified registers ----
constexpr uint16_t GRID_VOLTAGE = 150;      // U16, raw x 0.1 V. -> measurement.grid_voltage_v
constexpr uint16_t BATTERY_SOC = 184;       // U16, raw %. -> measurement.battery_soc_pct
constexpr uint16_t BATTERY_POWER = 190;     // S16, W. +discharge / -charge (discharge sign not yet observed). -> measurement.battery_power_w
constexpr uint16_t GRID_RELAY_STATUS = 194; // U16, bit0: 1=ON/0=OFF. -> measurement.grid_relay_on

// ---- Candidate registers (UNVERIFIED) ----
// One row per address under investigation for pv_power_w / grid_power_w /
// load_power_w. "hypothesis" summarizes conflicting public interpretations;
// full citations are in REGISTER_MAP.md.
struct CandidateRegister
{
    uint16_t address;
    const char* hypothesis;
};

constexpr CandidateRegister CANDIDATE_REGISTERS[] = {
    {167, "Grid L/D power (kellerza/sunsynk) vs Internal CT L1 power (MarkLabs) - conflicting names"},
    {169, "Total grid power from inverter port; scale x1 or x10 depending on sub-family (disputed)"},
    {172, "External grid CT (zero-export) power - a different quantity from 169 per MarkLabs"},
    {175, "Inverter AC output power (kellerza/sunsynk) - NOT the same as load power"},
    {178, "Load power (kellerza/sunsynk) vs instantaneous grid power (solarmark.us) - direct conflict"},
    {179, "No public source found defining this register; meaning unknown"},
    {186, "PV1 power - relatively consistent across LP1-family tables, still unverified for SG05LP1"},
    {187, "PV2 power - same confidence note as PV1"},
};

constexpr size_t CANDIDATE_REGISTER_COUNT =
    sizeof(CANDIDATE_REGISTERS) / sizeof(CANDIDATE_REGISTERS[0]);

// ---- Telemetry gates ----
// While a gate is false, the corresponding telemetry field is never read from
// a candidate register and TelemetryPublisher omits it from the payload.
//
// TEMPORARY: all three gates are true so the local beta publishes PV, grid
// and load power. The registers below are STILL UNCONFIRMED against the
// physical inverter display (scale, sign, and meaning are hypotheses; see
// REGISTER_MAP.md). Treat these values as unconfirmed. Once checked, record
// the result in REGISTER_MAP.md; set a gate back to false to stop publishing
// that field.
constexpr bool PV_POWER_VERIFIED = true;   // UNCONFIRMED: PV1 + PV2
constexpr bool GRID_POWER_VERIFIED = true; // UNCONFIRMED: register 169 (172 is the CT alternative)
constexpr bool LOAD_POWER_VERIFIED = true; // UNCONFIRMED: register 178 (a source maps it to grid power)

// Registers used for these fields. All are read as S16 watts with scale x1;
// adjust here if verification shows otherwise.
constexpr uint16_t PV1_POWER = 186;  // pv_power_w = PV1 + PV2 (UNCONFIRMED)
constexpr uint16_t PV2_POWER = 187;
constexpr uint16_t GRID_POWER = 169; // UNCONFIRMED scale/sign; published as read, backend sign convention not yet defined
constexpr uint16_t LOAD_POWER = 178; // UNCONFIRMED meaning

} // namespace registers
