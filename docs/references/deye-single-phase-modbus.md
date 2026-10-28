# Deye Single-Phase Inverter — Modbus RTU Protocol (English Translation)

> **Provenance and reliability**
>
> * This reference was prepared from the two official Deye vendor documents listed below. The original files are not stored in this repository.
> * Chinese-only content was translated into English, and the information was structured into Markdown, with AI assistance.
> * This Markdown is a **secondary** technical reference. The original vendor documents remain the authoritative sources for resolving any ambiguity or suspected translation error.
> * A register listed here is a statement by the vendor. Its availability and behavior have **not** necessarily been verified on the physical inverter used by this project. The project's own verification status is tracked in the [firmware register map](../../firmware/esp32-relay/REGISTER_MAP.md), not here.
> * This file describes the vendor protocol, including write function `0x10` and EMS control registers. The project's ESP32 firmware is read-only and implements only function `0x03`.

English translation and project digest of the two vendor documents:

| Source file | Content | Title (translated) |
|---|---|---|
| `单相协议(1).docx` | Full Modbus RTU register map, alarm and fault codes | "Modbus RTU Protocol" (single-phase protocol), version **V120**, 2025-02-24 |
| `ems控制-单相(1).docx` | Registers for remote (EMS) control of the inverter | "EMS control — single-phase" |

Vendor: Deye (Chinese name 德业). The Modbus RTU protocol document is bilingual Chinese/English; the EMS document is Chinese only. Chinese-only content was translated here. Where the source text is unclear or looks like a copy-paste error, a **Translator's note** is added. Nothing in this file has been verified on hardware.

## 1. Which devices this applies to

The document states: *"this protocol is for Microinverter, string inverter and storage inverter"*. The device family is identified by register `000` (read-only):

| Register `000` value | Device type |
|---|---|
| `0x0200` | String inverter |
| `0x0300` | **Single-phase low-voltage storage (hybrid) inverter** |
| `0x0400` | Micro-inverter |
| `0x0500` | Three-phase low-voltage storage inverter |
| `0x0600` | Three-phase high-voltage storage inverter |

Rows marked "MI" in the source apply to micro-inverters only. Sections named "storage inverter" apply to hybrid inverters (registers `150`–`197`, `200`–`492`, `500`+, `10000`+).

**Relevance to this project (HomePowerGuard):** the monitored device is a Deye SUN-6K-SG05LP1-EU-AM2-P, a single-phase low-voltage hybrid inverter. It should report `0x0300` in register `000` (expectation, not yet read from the device). The vendor document describes the protocol family and does not name this model. The single-phase protocol also covers split-phase/US variants, which is why many registers have `L1`/`L2` and `Gen` variants. On a single-phase EU unit the `L2` values are expected to be 0 or unused.

## 2. Modbus port parameters

| Parameter | Value | Source |
|---|---|---|
| Protocol | Modbus RTU | document title |
| Physical layer | RS232 or RS485 | "Baud rate: 9600bps RS232 or RS485" |
| Baud rate | **9600 bps** | document |
| Data format | Not specified in the document. The project uses 8-N-1 | — |
| Slave address | Register `001`, read-only, range `[1, 247]` | register map |
| Byte order inside a register | Big-endian (Hi byte first) | examples in section 3 |
| Reserved words/bytes/bits, unsupported registers | Filled with `0x00` | document remark |
| Supported function codes | `0x03` Read Holding Registers, `0x10` Write Multiple Registers | section 2.6 |
| Max registers per `0x03` request | 125 (`0x007D`) | section 2.6.1 |
| Max registers per `0x10` request | 123 (`0x007B`) | section 2.6.2 |
| Error response | Function code + `0x80` (`0x83` for read, `0x90` for write), then exception code | section 2.6 |

Notes:

- Exception codes are only referenced ("see exception code") and are **not defined** in the document. The examples use `0x04`, which in standard Modbus means "slave device failure".
- Function `0x06` (write single register) is **not** listed. Use `0x10` for writes, even for a single register.
- Registers `037` (communication address) and `038` (communication baud rate; `0` = ZigBee, `1` = PLC) are micro-inverter fields and read as `0x0000`.

## 3. Function codes and frames (PDU only)

### 3.1 Read registers — `0x03`

Request PDU:

| Field | Length | Value |
|---|---|---|
| Function code | 1 byte | `0x03` |
| Starting register address | 2 bytes | `0x0000`–`0xFFFF` |
| Number of registers | 2 bytes | `0x0001`–`0x007D` |

Normal response PDU: function code `0x03` (1 byte), byte count `N×2` (1 byte), register values `N×2` bytes (`N` = number of registers).

Exception response PDU: error code `0x83` (1 byte), exception code (1 byte).

Example — read 3 registers starting at address 107:

| Request | | Normal response | | Exception response | |
|---|---|---|---|---|---|
| Function code | `0x03` | Function code | `0x03` | Error code | `0x83` |
| Start address Hi | `0x00` | Byte count | `0x06` | Exception code | `0x04` |
| Start address Lo | `0x6B` | Register[107] Hi / Lo | `0x02` / `0x2B` | | |
| Quantity Hi | `0x00` | Register[108] Hi / Lo | `0x00` / `0x00` | | |
| Quantity Lo | `0x03` | Register[109] Hi / Lo | `0x00` / `0x64` | | |

### 3.2 Write registers — `0x10`

Request PDU:

| Field | Length | Value |
|---|---|---|
| Function code | 1 byte | `0x10` |
| Starting register address | 2 bytes | `0x0000`–`0xFFFF` |
| Number of registers | 2 bytes | `0x0001`–`0x007B` |
| Byte count | 1 byte | `N×2` |
| Register values | `N×2` bytes | |

Normal response PDU: function code `0x10`, starting address (2 bytes), number of registers (2 bytes). Exception response PDU: error code `0x90`, exception code.

Example — write `0x000A` and `0x0102` to the two registers starting at address 1:

| Request | | Normal response | | Exception response | |
|---|---|---|---|---|---|
| Function code | `0x10` | Function code | `0x10` | Error code | `0x90` |
| Start address Hi / Lo | `0x00` / `0x01` | Start address Hi / Lo | `0x00` / `0x01` | Exception code | `0x04` |
| Quantity Hi / Lo | `0x00` / `0x02` | Quantity Hi / Lo | `0x00` / `0x02` | | |
| Byte count | `0x04` | | | | |
| Values | `0x00 0x0A 0x01 0x02` | | | | |

## 4. Document revision history

| Version | Change | Date |
|---|---|---|
| V107 | Register 55 changed from "firmware update enable" to "whole-unit function test command 1"; register 100 changed to "test flag return" | 2018-11-28 / 2018-12-04 |
| V108 | Added registers needed for fan control; added California registers | 2019-01-14 |
| V111 | Added battery SN registers; added monthly/yearly data registers for Delong Neng (Dyness) battery | 2019-04-28 / 04-29 |
| V112 | Translated to English; added parallel function; added micro-inverter over-frequency derating (MPPT high/low voltage removed) | 2019-05 / 06-21 / 07-19 |
| V113 | Added daily and historical energy registers for 8 modules | 2019-07-24 |
| V114 | Table layout cleaned up; added 16-string power and meter registers | 2019-09-29 |
| V115 | Added per-pack battery information | 2019-11-12 |
| V116 | Time-of-Use Selling enable bit 8 = work mode 3 (Spanish customer); bits 2/3/4 of registers 274–279 = GM/BU/CH mode; register 181 = Gen port voltage | 2020-12-24 |
| V117 | Register 62 = generator daily energy; 92 = generator total energy low word; 95 = generator total energy high word | 2021-04-08 |
| V119 | Register 28: bit 2 low power mode, bit 5 MPPT multi-point scan; register 34: low noise mode; register 46: absorption time | 2024-02-19 |
| V120 | Added registers 223, 224, 27, 489, 490, 491 | 2025-02-24 |


## 5. Register map

Conventions used in the tables below:

- Addresses are **decimal**, as in the source, and are the values placed directly in the PDU (the read example uses "address 107" = `0x006B`).
- `R` = read-only, `R/W` = read/write.
- "Signed" = signed 16-bit integer (S16). "Low word"/"High word" = the two halves of a 32-bit value stored in two registers.
- Several registers carry an **offset of +1000** so they can be sent unsigned: temperatures (`091`, `182`, `319`, 0.1 °C) and power factor (`039`, 0.001).
- Power scale depends on register `054`: if `054` = `10`, the registers listed in its note are in units of **10 W**; if `1`, in **1 W**.

### 5.1 Intrinsic attributes (read-only) — `000`–`019`

| Addr | Meaning | R/W | Range | Unit | Note |
|---|---|---|---|---|---|
| 000 | Device type | R | | | See section 1 |
| 001 | Modbus address | R | 1–247 | | MI |
| 002 | Communication protocol version | R | `'0'`–`'9'`, `'A'`–`'Z'` | | Protocol version the firmware complies with; `0x0102` = version 1.2 (MI) |
| 003–007 | Serial number, bytes 01–10 (2 bytes per register) | R | ASCII | | 10 ASCII characters. For `AH12345678`: byte 01 = `0x41` ('A'), byte 02 = `0x48` ('H') … byte 10 = `0x38` ('8') |
| 008 | NO | R | | | |
| 009 | Chip type | R | | | Low 4 bits: 1 = AT32F403A, 2 = SXX32F103, 3 = GD32F103, 4 = GD32F303 |
| 010 | Communication board firmware version, field 2 | | | | |
| 011 | Control board auxiliary program version | | | | |
| 012 | Control board firmware version, field 2 | R | | | |
| 013 | Control board firmware version | R | | | |
| 014 | Communication board firmware version | R | | | |
| 015 | Battery voltage type | R | | | `<3`: 48 V battery; `=3`: 24 V battery |
| 016 | Rated power, low word | R | | 0.1 W | |
| 017 | Rated power, high word | R | | 0.1 W | |
| 018 | MPPT count and phases | R | 1–8 / 1–3 | | `0x0503` = five MPPTs, three-phase (high byte = MPPTs, low byte = phases) |
| 019 | Battery manufacturer | R | | | |

### 5.2 Variable attributes (settings) — `020`–`058`

Writing wrong values to these registers can change grid-protection and safety behavior. Treat all as write-restricted.

| Addr | Meaning | R/W | Range | Unit | Note |
|---|---|---|---|---|---|
| 020 | Remote lock enable | R/W | | | Off = `0x0002`, On = `0x0000` (as written in the source) |
| 021 | Start-up self-check time | R/W | 0–1000 | s | |
| 022 | System time: byte 01 = year (base 2000), byte 02 = month | R/W | 0–255 / 1–12 | | Two bytes packed per register; byte 01 assumed to be the high byte |
| 023 | System time: byte 03 = day, byte 04 = hour | R/W | 1–31 / 0–23 | | |
| 024 | System time: byte 05 = minute, byte 06 = second | R/W | 0–59 | | |
| 026 | DC voltage upper limit | R/W | 2000–10000 | 0.1 V | |
| 027 | Gen current number | R | | | If `0x1000 & reg == 0x1000`: two Gen currents, otherwise one. Gen L2 power is in register `491` |
| 028 | Bit0 ATS enable; Bit1 ATS status; Bit2 low-power mode (low battery); Bit5 MPPT multi-point scan; Bit7/8/9 PV1/PV2/PV3 self-check | R/W | | | `0` disable, `1` enable. *Translator's note: the range `1600–5500` and unit `0.1 V` in the source row look left over from an older definition ("DC voltage lower limit").* |
| 029 | Grid frequency upper limit | R/W | 4500–6500 | 0.01 Hz | |
| 030 | Grid frequency lower limit | R/W | 4500–6500 | 0.01 Hz | |
| 031 | Grid current upper limit | R/W | 10–20000 | 0.1 A | |
| 032 | Start-up voltage upper limit | R/W | 7000–9000 | 0.1 V | |
| 033 | Start-up voltage lower limit | R/W | 4500–9000 | 0.1 V | |
| 034 | Low-noise mode | R/W | 0–1 | | `1` enable, `0` disable |
| 035 | Over-frequency derating percentage | R/W | 0–100 | % | |
| 036 | Import power limiter | R/W | 0–0xFFFF | 1 W | |
| 037 | Communication address | R | | | MI |
| 038 | Communication baud rate | R | | | MI: `0` ZigBee, `1` PLC |
| 039 | Power factor regulation | R/W | 0–2000 | 0.001 | True value + 1000. `-0.852` → 148; `0` → 1000; `0.982` → 1982 |
| 040 | Active power regulation | R/W | 0–1200 | 0.1 % / 1 % | `800` = 80.0 % |
| 041 | Reactive power regulation | R/W | 0–1200 | 0.1 % | `800` = 80.0 % |
| 042 | Apparent power regulation | R/W | 0–1200 | 0.1 % | `800` = 80.0 % |
| 043 | Power on/off enable | R/W | 0–1 | | `0` power off, `1` power on (the source also lists `2` = power off) |
| 044 | Factory reset enable | R/W | 0–1 | | `0` disable, `1` enable |
| 045 | Self-check time | R/W | | s | 0–360 s |
| 046 | Absorption charge time | R/W | | | Added in V119. *Translator's note: "[0,1]" and "0: disable / 1: enable" in the source look copied from a neighboring row.* |
| 048 | Italian (CEI) self-check enable | R/W | 0–1 | | `0` disable, `1` enable |
| 049 | Relay-stuck (welded contact) detection | R/W | 0–1 | | |
| 050 | RISO (insulation resistance) enable | R/W | 0–1 | | `0` disable, `1` enable |
| 051 | Grid standard | R/W | 0–20 | | 1 INMETRO, 2 EN50549, 3 EN50438, 4 IEC61727, 5 CUSTOM, 6 VDE_AR_N_4105, 7 UTE_C15_712_1, 8 RD_1699, 9 CEI_0_21, 10 G98_G99, 11 AS4777 |
| 053 | Max PV power | R/W | 0–65536 | | |
| 054 | AC power ratio | R | 0–2 | | `10`: registers 167–172, 176–179, 185, 190 are in 10 W units. `1`: the same registers are in 1 W units |
| 055 | Command 1 for function test (**Factory only**) | R/W | 0–3 | | Bit0 test enable (required for the others); Bit1 all fans on; Bit2 flash all LEDs/buzzer/backlight (red/yellow/blue); Bit3 Li-battery interface test; Bit4 Gen signal relay on; Bit5 restart LCD program. **Do not write.** |
| 056 | Limiter function enable | R/W | | | |
| 057 | Energy correction factor (PowerWH factor) | R/W | | 0.01 | `100` = 1.00, `111` = 1.11 |

Registers `025`, `047`, `052`, `058` are unused or marked "No".

### 5.3 Real-time operating data — `059`–`124`

| Addr | Meaning | R/W | Range | Unit | Note |
|---|---|---|---|---|---|
| 059 | Run state | R | 0–5 | | `0` standby, `1` self-check, `2` normal, `3` warning, `4` fault |
| 060 | Daily active energy | R | signed | 0.1 kWh | Signed |
| 061 | Daily reactive energy | R | signed | 0.1 kVarh | Signed |
| 062 | Daily generator energy | R | 0–65535 | 0.1 kWh | |
| 063 / 064 | Total active energy, low / high word | R | | 0.1 kWh | |
| 065 | Monthly PV energy | R | | 1 kWh | |
| 066 | Monthly load energy | R | | 1 kWh | |
| 067 | Monthly grid-sell energy | R | | 1 kWh | |
| 068 / 069 | Yearly PV energy, low / high word | R | | 0.1 kWh | LCD statistics |
| 070 / 071 | Battery daily charge / discharge energy | R | | 0.1 kWh | |
| 072 / 073 | Battery total charge energy, low / high word | R | | 0.1 kWh | |
| 074 / 075 | Battery total discharge energy, low / high word | R | | 0.1 kWh | |
| 076 / 077 | Grid daily buy / sell energy | R | | 0.1 kWh | |
| 078 | Grid total buy energy, low word | R | | 0.1 kWh | |
| 079 | Grid frequency | R | 0–9999 | 0.01 Hz | |
| 080 | Grid total buy energy, high word | R | | 0.1 kWh | Note: low word is at `078`, high word at `080` (not adjacent) |
| 081 / 082 | Grid total sell energy, low / high word | R | | 0.1 kWh | |
| 083 | Generator daily operating time | R | | 0.1 h | `240` = 24 h |
| 084 | Daily load energy | R | 0–0xFFFF | 0.1 kWh | |
| 085 / 086 | Total load energy, low / high word | R | | 0.1 kWh | |
| 087 | Yearly load energy, low word | R | | 0.1 kWh | |
| 088 | Inverter total reactive power | R | | 1 Var | |
| 089 | Inverter output PF | R | | 1 | |
| 090 | Heat-sink temperature (DC transformer temperature) | R | 0–3000 | 0.1 °C | |
| 091 | IGBT module temperature (storage: AC radiator temperature) | R | 0–3000 | 0.1 °C | +1000 offset: `438` = −56.2 °C, `1000` = 0 °C, `1505` = 50.5 °C |
| 092 / 095 | Generator total energy, low / high word | R | 0–65535 | 0.1 kWh | Not adjacent |
| 093 | Flags | R | | | Bit0: `0` EE65, `1` amorphous; Bit1: `1` grid phase wrong; Bit2: `1` generator phase wrong |
| 094 | SD card status | R | | | `1000` SD fault, `2000` normal |
| 096 / 097 | Historical PV energy, low / high word | R | | 0.1 kWh | |
| 098 / 099 | Yearly grid-sell energy, low / high word | R | | 0.1 kWh | |
| 100 | Other test flags | R | | | Bit0 arc communication; Bit1 parallel CAN (`1` normal); Bit8 Li-battery RS485; Bit9 Li-battery CAN; Bit10 buttons 1–4; Bit11 LCD interrupt (`1` normal) |
| 101 / 102 | Warning words 1 / 2 | R | 0–65535 | | See section 7 |
| 103–106 | Fault words 1–4 | R | 0–65535 | | See section 7 |
| 107 | Corrected battery capacity | R | 0–1000 | 1 Ah | `100` = 100 Ah |
| 108 | Daily PV energy | R | 0–65535 | 0.1 kWh | |
| 109–116 | DC voltage 1–4 (odd addresses 109/111/113/115) and DC current 1–4 (110/112/114/116) | R | 0–65535 | 0.1 V / 0.1 A | |
| 117 / 118 | Alarm status | R | | | Added at the data collector's request. Both rows are labeled "Alarms Status1" in the source |
| 119 | UPS power | R | | 1 W | |
| 120–124 | Debug data | R | | | |


### 5.4 Real-time data added for storage (hybrid) inverters — `150`–`197`

This is the block the project firmware reads. "Signed" = S16, positive and negative values.

| Addr | Meaning | R/W | Unit | Note |
|---|---|---|---|---|
| 150 | Grid-side voltage L1-N | R | 0.1 V | |
| 151 | Grid-side voltage L2-N | R | 0.1 V | |
| 152 | Grid-side voltage L1-L2 | R | 0.1 V | |
| 153 | Relay middle-side voltage L1-L2 | R | 0.1 V | |
| 154 | Inverter output voltage L1-N | R | 0.1 V | |
| 155 | Inverter output voltage L2-N | R | 0.1 V | |
| 156 | Inverter output voltage L1-L2 | R | 0.1 V | |
| 157 | Load-side voltage L1 | R | 0.1 V | |
| 158 | Load-side voltage L2 | R | 0.1 V | |
| 159 | Reserved | R | | |
| 160 / 161 | Grid-side current L1 / L2 | R | 0.01 A | Signed |
| 162 / 163 | External limiter (CT) current L1 / L2 | R | 0.01 A | Signed |
| 164 / 165 | Inverter output current L1 / L2 | R | 0.01 A | Signed |
| 166 | Gen port used as micro-inverter input: power | R | 1 W | Positive when Gen port acts as load output; negative when it acts as micro-inverter input (power flowing into the storage inverter) |
| 167 | Grid-side power L1 | R | 1 W (10 W if `054` = 10) | Signed |
| 168 | Grid-side power L2 | R | 1 W (10 W if `054` = 10) | Signed |
| 169 | Grid total power (name truncated in the source; "Grid") | R | 1 W (10 W if `054` = 10) | Signed: **> 0 = buying from grid, < 0 = selling (feeding in)** |
| 170 / 171 | External limiter 1 / 2 power | R | 1 W (10 W if `054` = 10) | Signed |
| 172 | External limiter total power | R | 1 W (10 W if `054` = 10) | Signed |
| 173 / 174 | Inverter output power L1 / L2 | R | 1 W | Signed |
| 175 | Inverter output total power | R | 1 W | Signed |
| 176 / 177 | Load-side power L1 / L2 | R | 1 W (10 W if `054` = 10) | Signed |
| 178 | Load-side total power | R | 1 W (10 W if `054` = 10) | Signed |
| 179 | Load current L1 | R | 0.01 A | Signed (**current, not power**) |
| 180 | Load current L2 | R | 0.01 A | Signed (source label has a typo: "gong L2") |
| 181 | Gen port voltage | R | | Added in V116 |
| 182 | Battery temperature | R | 0.1 °C | +1000 offset; `1200` = 20.0 °C. Range 0–3000 |
| 183 | Battery voltage | R | 0.01 V | `4100` = 41.00 V |
| 184 | Battery capacity (SOC) | R | 1 % | 0–100 |
| 185 | Battery charging status | R | | Values not described in the source |
| 186 / 187 / 188 / 189 | PV1 / PV2 / PV3 / PV4 input power | R | 1 W | |
| 190 | Battery output power | R | 1 W (10 W if `054` = 10) | Signed. The source does not state which sign is charge and which is discharge |
| 191 | Battery output current | R | 0.01 A | Signed |
| 192 | Load frequency | R | 0.01 Hz | |
| 193 | Inverter output frequency | R | 0.01 Hz | |
| 194 | Grid-side relay status | R | | Two states: disconnected / closed. The numeric values are missing from the source text (probably shown as symbols) |
| 195 | Generator-side relay status | R | | Bits 0–3: generator relay state — `0` not closed, `1` closed, `2` vacant, `3` closed while the generator is running. Bits 4–7: switch signal — `0` power off, `1` power on. Bits 8–11: generation signal |
| 196 | Generator port frequency | R | 0.01 Hz | |

### 5.5 Settings added for storage inverters — `200`–`299`

All are R/W. Many of these define battery protection and charge/discharge behavior; do not write without understanding the effect.

| Addr | Meaning | Range | Unit | Note |
|---|---|---|---|---|
| 200 | Battery charging type (control mode) | | | `0x0000` lead battery, four-stage charging; `0x0001` lithium battery |
| 201 | Equalization voltage | 3800–6100 | 0.01 V | `1480` = 14.8 V |
| 202 | Absorption voltage | 3800–6100 | 0.01 V | `1440` = 14.4 V |
| 203 | Float voltage | 3800–6100 | 0.01 V | `1440` = 14.4 V |
| 204 | Battery capacity | 0–2000 | 1 Ah | |
| 205 | Empty voltage | | 0.01 V | |
| 206 | Zero-export: minimum limit power at which it takes effect | | | |
| 207 | Equalization cycle | 0–90 | days | Equalize once every N days |
| 208 | Equalization time | 0–20 | 0.5 h | 0–20 = 0–10 h; the value sent to the MCU is 0–100 |
| 209 | Temperature compensation (TEMPCO) | 0–50 | 1 mV/°C | Signed |
| 210 | Battery max charge current | 0–185 | 1 A | |
| 211 | Battery max discharge current | 0–185 | 1 A | |
| 213 | Battery operates by voltage or capacity | | | Source lists three options: according to voltage, according to capacity, and `2` = no battery. Values `0`/`1` for the first two are implied, not stated |
| 214 | Lithium battery wake-up flag | | | `0` enabled, `1` disabled |
| 215 | Battery internal resistance | 0–6000 | mΩ | |
| 216 | Battery charging efficiency | 0–100 | 0.1 % | `983` = 98.3 % |
| 217 / 218 / 219 | Battery capacity ShutDown / Restart / LowBatt | 0–100 | 1 % | Low-capacity cutoff / protection recovery point / low-battery point |
| 220 / 221 / 222 | Battery voltage ShutDown / Restart / LowBatt | 3800–6100 | 0.01 V | Examples: cutoff 41 V, restart 52 V, low-battery 46 V |
| 223 | Generator scheduled run time (V120) | | | Bits 0–3 run minutes; bits 4–7 minutes; bits 8–12 hour; bits 13–15 week |
| 224 | Generator max run time (V120) | | 10 min | Max 360 min |
| 225 / 226 | Generator charging start voltage / capacity | 0–6300 | 0.01 V / 1 % | Generator starts charging when the battery is below this value |
| 227 | Generator-to-battery charge current | 0–185 | 1 A | |
| 228 / 229 | Grid charging start voltage / capacity | 0–6300 | 0.01 V / 1 % | |
| 230 | Grid-to-battery charge current | 0–185 | 1 A | |
| 231 / 232 | Generator charging enable / Grid charging enable | | | |
| 233 | Solar input is PSU | 0–1 | | `0` solar, `1` PSU |
| 234 | Force generator on as load | | | Requires register `235` to be enabled. `0` not forced, `1` forced |
| 235 | Generator input used as load output | | | `0` generator input only; `1` smart-load output; `2` use as inverter input |
| 236 / 237 | SmartLoad OFF battery voltage / capacity | 3800–6300 / 0–100 | 0.01 V / 1 % | |
| 238 / 239 | SmartLoad ON battery voltage / capacity | 3800–6300 / 0–100 | 0.01 V / 1 % | |
| 240 | PWM test enable | | | `0` default, `1` enter PWM test (factory) |
| 241 | Minimum solar power required to start a generator | 0–8000 | 1 W | |
| 243 | Energy management mode | | | `0` battery-priority, `1` load-priority |
| 244 | Limit control function | | | `0x00` sell electricity enabled; `0x01` built-in limiter enabled; `0x02` external limiter enabled |
| 245 | Export power limit (max grid-connected power output) | 0–8000 | 1 W | Total power |
| 246 | External current-sensor clamp phase | | | Values `[11][12]` |
| 247 | Solar sell | | | `0` PV does not sell, `1` PV sells |
| 248 | Time-of-Use Selling enable | | | Bit0 enable; Bit1 Monday … Bit7 Sunday (`1` enable); Bit8 work mode 3 (Spanish customer) |
| 250–255 | Sell-mode time points 1–6 | 0000–2359 | HHMM | `2359` = 23:59. MCU internal range 0–287 (5-minute steps); the value sent to MCU and collector is e.g. `2355` |
| 256–261 | Sell-mode power for time points 1–6 | 0–8000 | 1 W | Limited by battery max discharge power |
| 262–267 | Sell-mode voltage for time points 1–6 | 0–6300 | 0.01 V | Affected by battery voltage |
| 268–273 | SOC (capacity) for time points 1–6 | 0–100 | 1 % | |
| 274–279 | Charge enable for time points 1–6 | | | Bit0 grid charging; Bit1 generator charging (`0` disable, `1` enable); Bit2 GM mode; Bit3 BU mode; Bit4 CH mode |
| 280 | Micro-inverter export-to-grid cutoff / output-voltage and peak-shaving bits | | | Bits 0–3 inverter output voltage — if `286`=0: 0 = 230 V, 1 = 220 V, 2 = 240 V, 3 = 200 V; if `286`=1: 0 = 120/240 V, 1 = 110/220 V, 2 = 120/240 V, 3 = 110/200 V; if `286`=2: 0 = 120/208 V, 1 = 127/220 V. Bits 4–7 Gen peak-shaving (`1` enable). Bits 8–11 Grid peak-shaving (`1` enable). Bit12 On-grid always on. Bit13 external relay. Bit14 report fault on lithium battery loss. Bit15 DRM enable |
| 281 | External sensor auto-direction detection enable | 0–1 | | |
| 282 | Restore grid-connection time | 10–300 | | |
| 283 | Solar arc-fault mode | 0–1 | | `0` off, `1` on, `2` clear arc fault (inverter sets it back to `1` after the LCD issues the clear flag) |
| 284 | Grid mode | | | `0` general standard, `1` UL1741 & IEEE1547, `2` CPUC Rule 21, `3` SRD-UL1741, `4` CEI 0-21, … |
| 285 | Grid frequency setting | 0–1 | | `0` 50 Hz, `1` 60 Hz |
| 286 | Grid type setting | 0–3 | | `0` single-phase; `1` two-phase 120/240 V; `2` three-phase 208 V (120° / 120 V); `3` 120 V single-phase |
| 287 / 288 | Grid voltage high / low protection point | 1800–2700 | 0.1 V | |
| 289 / 290 | Grid frequency high / low protection point | 4500–6500 | 0.01 Hz | |
| 291 | Generator connected to grid input | 0–1 | | `0` disable, `1` enabled |
| 292 / 293 | GEN / GRID peak-shaving power | 0–16000 | 1 W | |
| 294 | SmartLoad open delay | 1–120 | 1 min | |
| 295 | Output PF setting (active-power regulation) | 800–1200 | | `800` = 80 %, `1200` = 120 % |
| 296 | External relay bits | 0–0xFFFF | | Bits 0–8 = 8 relays |


### 5.6 Settings, battery (BMS) data, grid-code and parallel registers — `297`–`492`

| Addr | Meaning | Range | Unit | Note |
|---|---|---|---|---|
| 297–310 | ARC factory parameters B, I, F, D, T, C, Frz (each high word + low word) | 0–65535 | | Factory arc-fault detection parameters; high and low word combine into one number |
| 311 | UPS time | | 1 s | `0` default, `1` = 1 s |
| 312 / 313 | Battery charging / discharging voltage (from BMS) | | 0.01 V | |
| 314 / 315 | Battery charging / discharging current limit (from BMS) | | 1 A | |
| 316 | Battery real-time capacity | | 1 % | |
| 317 | Battery real-time voltage | | 0.01 V | |
| 318 | Battery real-time current | | 1 A | |
| 319 | Battery real-time temperature | | 0.1 °C | `1000` = 0 °C, `1200` = 20.0 °C, `800` = −20.0 °C |
| 320 / 321 | Maximum charge / discharge current limit | | 1 A | |
| 322 | Lithium battery alarm bits | | | |
| 323 | Lithium battery fault bits | 0–65535 | | |
| 324 | Lithium battery flag 2 | 0–65535 | | Bit1: forced-charge flag |
| 325 | Lithium battery type (BMS protocol) | | | `0x0000` ZTE/Pylontech/Dyness/SOLAX generic CAN protocol; `0x0001` Tianbangda RS485 Modbus; `0x0002` KOK; `0x0003` Keith; `0x0004` Tuopai; `0x0005` Pylon RS485; `0x0006` Jielisi RS485; `0x0007` Sunwoda RS485; `0x0008` Xinruineng RS485; `0x0009` Tianbangda RS485; `0x000A` Shenggao Electric CAN |
| 326 | External meter / CT configuration | | | Bit0 `1` enable / `0` enable Meter1 CT; Bit1–3 enable phase A/B/C; Bit6 `1` enable / `0` enable AC-to-grid; Bit7 `1` enable / `0` enable AC-to-load; Bits 8–11 meter 1/2 type: 4 = Chint single-phase DDSU666, 3 = Donghong single-phase SDM230, 2 = Chint three-phase DTSU666, 1 = Donghong three-phase SDM630; Bit12 `1` enable / `0` enable Meter2 CT; Bit13 `1` enable / `0` force-start generator; Bits 14–15 reserved |
| 327 | CT ratio | 200–8000 | | Multiplier of the external CT |
| 328 | Special function bits | | | Bit0: US ground-fault shutdown (`0` stop, `1` do not stop) |
| 329 | AC-couple frequency upper limit | 5000–6500 | | |
| 330 | Communication-board setting bits | | | Bits 0–1 time sync; 2–3 beep; 4–5 AM/PM; 6–7 auto dim; 8–9 solar-arm discern; 10–11 hide battery pack list on web page (`11` hide, `10` show). Per 2-bit field: `00`/`01` no action, `10` disable, `11` enable |
| 331 | California LVRT/HVRT (CA_LHVRT) enable | 0–1 | | `0` disable, `1` enable |
| 332–336 | CA_HV2, CA_HV1, CA_LV1, CA_LV2, CA_LV3 | 1000–3000 | 0.1 V | Voltage ride-through thresholds |
| 337–341 | CA_HV2/HV1/LV1/LV2/LV3 time | 0–300 | | `0` = 0.16 s |
| 342 | California LFRT/HFRT (CA_LHFRT) enable | | | |
| 343–346 | CA_HF2, CA_HF1, CA_LF1, CA_LF2 | 4500–6500 | 0.01 Hz | |
| 347–350 | CA_HF2/HF1/LF1/LF2 time | 0–300 | | |
| 351 | California CA_QV enable | | | |
| 352–355 | CA_QV_V1…V4 | 1000–3000 | | (source gives `[-44,+44]`, 0.01 for V4/Q rows) |
| 356–359 | CA_QV_Q1…Q4 | | | |
| 360–362 | California CA_FW enable, CA_Fstart, CA_Fstop | | | |
| 363–365 | California CA_VW enable, CA_Vstart, CA_Vstop | | | |
| 366 | Normal ramp-up slope | 1–100 | 1 % | |
| 367 | Soft-start ramp-up rate | 1–100 | 1 % | Default 100 % |
| 368 / 369 / 370 | QV / VW / FW response time | 0–90 / 0–60 / — | s | |
| 371–375 | Reserved | | | |
| 376–389 | Calibration channels: Grid1_I, Grid2_I, Grid_V_L1, Grid_V_L2, Limit1_I, Limit2_I, PV1_V, PV1_I, PV2_V, PV2_I, INV_I, INV_V, BAT_I, BAT_V | | | Factory calibration |
| 390 / 391 | Solar1 / Solar2 used as Wind input enable | 0–1 | | `0` disable, `1` enable |
| 392–403 | Wind curve voltage points 1–12 | 500–5000 | 0.1 V | |
| 404–415 | Wind curve current points 1–12 | 0–200 | 0.1 A | |
| 416 | Force off-grid operation bit | | | |
| 417 | Parallel register 1 | | | Bit0 parallel enable (`1`) / disable (`0`); Bit1 `1` master / `0` slave; Bits 8–9 phase (`00` A, `01` B, `10` C, `11` void); Bits 10–15 Modbus SN (0–63) |
| 418 | Parallel register 2 | | | Bits 0–4 phase A inverter count; 5–9 phase B; 10–14 phase C |
| 419 / 420 | Lithium battery version, low / high | | | For several packs in parallel only the last pack is read |
| 421–423 | System time (same layout as `022`–`024`) | | | If the LCD is set to slave and a time is found here, the time is synchronized |
| 489 | EV charge mode | 0–0xFFFF | | High byte: off-grid SOC. Low byte: Bit0 solar energy only; Bit1 free work; Bit4 EV charger connected at Grid port; Bit5 EV charger connected at Load (LD) port |
| 490 | EV charge max power | 0–0xFFFF | 1 W | |
| 491 | Gen L2 power | 0–0xFFFF | 1 W | |
| 492 | Grid total reactive power | 0–0xFFFF | 1 Var | |

### 5.7 Battery pack identification and data — `500`–`809`

Only for the Tian-Power battery ("this is only for TIAN-POWER" in the source); up to 15 packs.

| Addr range | Meaning | Note |
|---|---|---|
| 500–505 | Pack 1 ID | 12 ASCII bytes, 2 per register, `'0'`–`'9'`, `'A'`–`'Z'` |
| 506–589 | Pack 2 … pack 15 ID | 6 registers per pack: pack N starts at `500 + 6×(N−1)` |
| 600–613 | Pack 1 data (14 registers) | See table below |
| 614–809 | Pack 2 … pack 15 data | Same layout; pack N starts at `600 + 14×(N−1)` |

Pack data layout (offset from the pack's first register):

| Offset | Meaning | Unit |
|---|---|---|
| +0 | Module voltage | 0.01 V |
| +1 | Module current | 0.1 A |
| +2 | Average temperature | `1250` = 25.0 °C |
| +3 | SOC | 0.1 |
| +4 | Remaining capacity | 0.1 Ah |
| +5 | Total capacity | 0.1 Ah |
| +6 | Charge voltage | 0.01 V |
| +7 | Charge current | 0.1 A |
| +8 | Discharge current | 0.1 A |
| +9 | Max cell voltage | 0.01 V |
| +10 | Min cell voltage | 0.01 V |
| +11 | Cycle number | 1 |
| +12 | Warning | |
| +13 | Fault | |

### 5.8 Deye battery read-only area — `10000`+

Section title: "Deye battery read-only area" (德业电池只读区).

| Addr | Meaning | Unit | Note |
|---|---|---|---|
| 10000 | Device type | | `0x700` = lithium battery pack protocol |
| 10001 | Protocol version | | |
| 10002 | Number of battery packs | 1 | |
| 10003 | Battery voltage | 0.1 V | |
| 10004 | Battery current | 0.1 A | |
| 10005 | Battery SOC | 1 % | |
| 10006 | Battery SOH | 1 % | |
| 10007 | Battery remaining capacity | 1 Ah | |
| 10008 | Battery temperature | 0.1 °C | |
| 10009 / 10010 | Charge voltage / discharge voltage | 0.1 V | |
| 10011 / 10012 | Charge end voltage / discharge end voltage | 1 V | |
| 10013 / 10014 | Charge / discharge current limit | 1 A | |
| 10015 / 10016 | Off-grid charge / discharge current limit | 1 A | |
| 10017 | Forced-charge flag | | |
| 10018 | SOC calibration flag | | |
| 10019 / 10020 | Battery fault 1 / 2 | | |
| 10021 / 10022 | Battery alarm 1 / 2 | | |
| 10023–10031 | Reserved 1–9 | | |
| 10032–10039 | Pack 1 SN (16 ASCII bytes, 2 per register) | | |
| 10040–10069 | Pack 1 data, 30 registers | | See below |

Pack data layout in the `10040`+ block (offset from the pack's first register): +0 module voltage (0.1 V); +1 module current (0.1 A); +2 average temperature; +3 max cell temperature; +4 min cell temperature; +5 MOS max temperature; +6 heating-element temperature (all `1250` = 25.0 °C); +7 SOC (0.1); +8 SOH (0.1); +9 remaining capacity (0.1 Ah); +10 total capacity (0.1 Ah); +11 charge voltage (0.01 V); +12 discharge voltage (0.01 V); +13 charge current (0.1 A); +14 discharge current (0.1 A); +15 max cell voltage (0.01 V); +16 min cell voltage (0.01 V); +17 cycle number; +18 MOS status; +19/+20 warning 1/2; +21/+22 fault 1/2; +23 software version; +24 hardware version; +25–+29 reserved.

Each further pack takes 8 registers of SN plus 22 registers of data = 30 registers, one pack after the other (second and third packs follow the same pattern).

### 5.9 Memory (event) record — `1000`–`1499`

| Addr | Meaning | R/W | Note |
|---|---|---|---|
| 1000–1499 | Inverter fault information record | R | Length 500 registers; layout is not described in the document |


## 6. EMS remote control (single-phase)

Translation of `ems控制-单相(1).docx`. These registers let an external Energy Management System (EMS) take over battery/grid power control. The EMS document does not name a function code; function `0x10` is the only write function in the main protocol document, so it is assumed here. **This project's firmware does not implement any of them:** it never writes to the inverter.

**Translator's note:** registers `131` and `135`–`142` are **not listed** in the register map of the main protocol document (section 5 has a gap between `124` and `150`). Treat this section as a separate, less-documented extension.

### 6.1 Enter / exit remote mode

| Mode | Register | Function |
|---|---|---|
| Remote mode | `131` | Write `1` to enter remote mode, write `0` to exit |

Notes from the source (the register references in parentheses in notes 1 and 3 are the translator's cross-references, not part of the source text):

1. The battery's configured maximum charge current and maximum discharge current limit the battery power (see registers `210` and `211`).
2. In **off-grid mode** Remote mode has no effect; the battery tries to supply the load consumption.
3. Grid power may exceed the "import power" limit (see register `036`, Import power limiter).

### 6.2 DC control mode (controls the battery, not PV)

| Register | Function | Note |
|---|---|---|
| `135` | Write `1` = control the battery | PV is not controlled |
| `136` | `0` constant-voltage control; `1` constant-current control; `3` control SOC | Values `0` and `1` are ineffective (only SOC control works) |
| `137` | `0` control total power; `1` control per-phase power | Value `1` is ineffective |
| `141` | Target SOC | If `141` < current SOC: the battery feeds the grid at the battery's max discharge current until SOC = target. If `141` > current SOC: the grid charges the battery at max charge current until SOC = target |
| `142` | Control power | Ineffective in this mode |

### 6.3 AC control mode (controls inverter AC input/output)

| Register | Function | Note |
|---|---|---|
| `135` | Write `0` = control the inverter AC input/output | |
| `136` | `0` constant-voltage; `1` constant-current; `3` control SOC | Values `0` and `1` are ineffective |
| `137` | `0` control total power; `1` control per-phase power | Value `1` is ineffective |
| `141` | Target SOC | Ineffective in this mode |
| `142` | Control power. **Negative = buy from grid, positive = feed to grid** | Example for a 5 kW unit: `142 = 800` → power is controlled to 4000 W; the battery discharges and the inverter outputs 4000 W until the battery reaches the minimum SOC. `142 = -800` → also 4000 W, but the grid charges the battery; battery power is limited by the max charge current; stops when the battery is full |

*Translator's note:* the example implies the unit of `142` is 0.1 % of rated power (800 × 0.1 % × 5000 W = 4000 W). This is an inference; the source does not state the unit.

## 7. Alarm and fault codes

Registers `101`–`102` (warnings) and `103`–`106` (faults) carry the codes. The document refers to an "alarm/fault information coding table" but gives only the code list below, **not the bit position of each code** in these registers.

### 7.1 Warning codes

| Code | Description |
|---|---|
| W01 | Reserved |
| W02 | FAN_WARN (fan warning) |
| W03 | Grid phase wrong |
| W04 | meter_Comm_Fail (energy meter communication failure) |

### 7.2 Fault codes

Suggested actions are condensed from the source. "Contact vendor" = "seek help from us if the inverter cannot return to normal".

| Code | Description | Suggested action |
|---|---|---|
| F07 | DC/DC soft-start fault | Check battery fuse; restart and check; contact vendor |
| F10 | Auxiliary power board failure | Wait a few minutes; remove the Wi-Fi plug or other communicator; contact vendor |
| F13 | Working mode changed | Wait a minute and check; contact vendor |
| F17 | Active_Battery_Hold | — |
| F18 | AC over-current (hardware) | Check that backup-load and common-load power are within range; restart; contact vendor |
| F20 | DC over-current (hardware) | Check PV and battery connections; switch off DC and AC switches, wait one minute, switch on; contact vendor |
| F22 | Tz_EmergSStop_Fault: emergency stop (inverter locked) | Contact vendor; rarely happens |
| F23 | AC leakage current transient over-current | Check PV/inverter cables; restart; contact vendor |
| F24 | DC insulation impedance failure (PV array) | PV isolation resistance too low: check PV–inverter connection, check that PE is grounded; contact vendor |
| F25 | AC_Active_Batt_Fault | — |
| F26 | DC busbar unbalanced | Wait and check; switch off DC and AC switches for one minute, then on; contact vendor |
| F29 | Parallel_CANBus_Fault (parallel communication) | Only for parallel mode: check parallel settings and CAN bus connection; contact vendor |
| F31 | Soft_Start_Failed | — |
| F35 | No AC grid | Confirm whether the grid is lost; check grid connection and the switch between inverter and grid; contact vendor |
| F37 | DCLLC_Soft_Over_Cur | — |
| F39 | DCLLC_Over_Current | — |
| F40 | Batt_Over_Current | — |
| F41 | Parallel_system_Stop (parallel system stopped) | Caused by another inverter's fault: wait, check all inverters, record all fault codes; contact vendor |
| F42 | AC line low voltage | Check AC voltage is within the standard range; check grid cables; contact vendor |
| F46 / F49 | Backup_Battery_Fault (source typo "Bcakup") | Check battery capacity and battery–inverter connections; if no recovery after reducing load, contact vendor |
| F47 | AC over-frequency | Check frequency is within specification; check AC cables; contact vendor |
| F48 | AC under-frequency | Same as F47 |
| F56 | DC busbar voltage too low (battery voltage low) | Check battery voltage; if too low, charge from PV or grid; contact vendor |
| F58 | BMS communication fault | — |
| F60 | Gen_Volt_or_Fre_Fault (generator voltage or frequency) | — |
| F61 | Button_Manual_OFF | — |
| F63 | ARC fault | Arc-fault detection is for the US market only; check PV cable connections and clear the fault; contact vendor |
| F64 | Heat-sink over-temperature | Check ambient temperature; switch the inverter off for 10 minutes and restart; contact vendor |

## 8. Cross-reference with the project firmware

The firmware ([`firmware/esp32-relay`](../../firmware/esp32-relay/README.md), register status in its [`REGISTER_MAP.md`](../../firmware/esp32-relay/REGISTER_MAP.md)) reads one holding register per request with function `0x03`, slave `1`, 9600 baud. This table compares its registers with the vendor document. **The vendor document is a manufacturer statement, not hardware verification**; the project's rule is still to compare each value with the inverter display before trusting it.

The "Project status" column is a snapshot of `REGISTER_MAP.md` when this section was written. If they differ, `REGISTER_MAP.md` and the firmware README are authoritative for the project's implementation and verification status. A "consistent" entry below means only that the vendor text agrees with the firmware's interpretation; it does not upgrade a candidate register to verified.

| Register | Project status (REGISTER_MAP.md) | Vendor document says |
|---|---|---|
| `150` | Verified: grid voltage | Grid-side voltage L1-N, 0.1 V — consistent |
| `184` | Verified: battery SOC | Battery capacity, 1 %, 0–100 — consistent |
| `190` | Verified: battery power (discharge sign not observed) | Battery output power, signed, 1 W — **sign convention not stated** |
| `194` | Verified: grid relay status | Grid-side relay status (disconnected / closed) — consistent |
| `167` | Candidate | Grid-side power L1, signed |
| `169` | Candidate, used for `grid_power_w` | Grid total power, signed, **> 0 buying, < 0 selling**. This is the vendor's stated convention; it is not verified on this inverter, and the backend has not defined one yet |
| `172` | Candidate (external CT) | External limiter total power, signed — the alternative to `169` |
| `175` | Candidate | Inverter output total power |
| `178` | Candidate, used for `load_power_w` | **Load-side total power**, signed (not grid power) |
| `179` | Candidate, "unknown" | **Load current L1, 0.01 A — not a power register** |
| `186` / `187` | Candidate, used for `pv_power_w` | PV1 / PV2 input power, 1 W — consistent (`188` / `189` = PV3 / PV4) |

Points worth acting on:

1. **Power scale depends on register `054`.** If `054` = `10`, registers `167`–`172`, `176`–`178`, `190` are in 10 W units; the firmware assumes 1 W. Reading `054` once would settle it. (The vendor's list also names `179` and `185`, which are a current and a status register, so the list is imprecise.)
2. **Block reads are allowed.** One `0x03` request can read up to 125 registers; `150`–`196` fit in a single request (47 registers) instead of many single-register requests.
3. **Not read yet but defined:** run state `059` (`0` standby … `4` fault), warning words `101`–`102`, fault words `103`–`106`, battery voltage `183` (0.01 V) and battery temperature `182` (+1000 offset, 0.1 °C).
4. **Device check:** read register `000`; a single-phase hybrid should return `0x0300`.
5. **Writes** (settings in section 5.2/5.5 and EMS control in section 6) need function `0x10`, which the firmware's Modbus client does not implement; it is read-only by design.
6. **Never write** register `055` (factory function test) or the factory/calibration blocks (`297`–`310`, `376`–`389`).
