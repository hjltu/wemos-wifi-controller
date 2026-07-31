# Wemos Automation - Developer Documentation

**Project:** hjhome / wemos_automation
**Version:** 23-jul-26 (July 26, 2023)
**Author:** hjltu@ya.ru
**License:** GNU GPL v3
**Target Hardware:** ESP8266 (Wemos D1 mini / NodeMCU V3)
**Single File:** `wemos_automation.ino` (~1300 lines)

---

## 1. PURPOSE & SCOPE

This firmware transforms an ESP8266 board into a **multi-purpose
MQTT-controlled home automation node** supporting:
- Relay switching (lights, appliances)
- PWM dimming (LED strips)
- Somfy RTS blind/shutter control
- Motorized shutter position control
- Temperature/humidity monitoring (DS18B20, DHT22)
- Analog sensor reading (0-5V or 0-10V via divider)
- Pulse counting (water/gas/electricity meters)
- Button input handling (short/long press detection)
- Web-based status dashboard
- Over-the-air configuration via MQTT

**Production Context:** Designed for 24/7 autonomous operation in
residential environments. Devices run for years without physical
access. Reliability, watchdog recovery, and non-volatile state
persistence are critical.

---

## 2. ARCHITECTURE OVERVIEW

### 2.1 High-Level Structure

```
+-------------------------------------------------------------+
|                      ESP8266 Firmware                        |
+-------------------------------------------------------------+
|  setup()                                                     |
|  +-- EEPROM init (256 bytes)                                 |
|  +-- Pin mode configuration (dynamic per mode)              |
|  +-- Network selection (Default / Saved / AP)               |
|  +-- WiFi connection (STA or AP)                            |
|  +-- MQTT client init + connect                             |
|  +-- Sensor init (OneWire DS18B20, DHT22)                   |
|  +-- Web server start (port 80)                             |
+-------------------------------------------------------------+
|  loop() - Non-blocking cooperative multitasking via millis() |
|  +-- MQTT client loop (reconnect logic)                     |
|  +-- HTTP web server handler                                |
|  +-- Button polling (GPIO0,2,12,13,A0)                      |
|  +-- Periodic sensor reads (configurable intervals)         |
|  +-- PWM fade engine (smooth transitions)                   |
|  +-- Somfy blind state machine                              |
|  +-- Shutter position control                               |
|  +-- Anti-freeze/overheat protection                        |
|  +-- Memory watchdog (reset if heap < 10KB)                 |
|  +-- Hardware reset detection (dual-button 20s/30s hold)    |
+-------------------------------------------------------------+
```

### 2.2 Operating Modes (Mutually Exclusive Groups)

| Group | Modes | EEPROM Addr | Description |
|-------|-------|-------------|-------------|
| **Network** | `default`, `apmode` | 109, 112 | WiFi/MQTT source selection |
| **GPIO12/13** | `blinds`, `buttons` | 103, 106 | Somfy vs Button inputs |
| **GPIO14/16** | `shutters`, `term` | 110, 104 | Shutter motor vs Thermal protection |
| **Sensors** | `period`, `sensor`, `count` | 101, 108, 105 | Periodic, analog, pulse count |

> **Constraint:** `blinds` + `buttons` cannot both be 1.
> `shutters` + `term` cannot both be 1. Enforced in callback.

### 2.3 Sensor Simulation (Testing/Debug)

MQTT topics allow injecting simulated sensor values (added 29-jul-26):

| Topic | Payload | Effect |
|-------|---------|--------|
| `/<name>/in/_tmp4` | int (-99 to 99) | Override DHT temperature |
| `/<name>/in/_hum4` | int (0-100) | Override DHT humidity |
| `/<name>/in/_temp5` | int (-99 to 99) | Override DS18B20 temperature |

These are **input-only** (no EEPROM persist), useful for testing logic
without physical sensors. Published back on `/out/tmp4`, `/out/hum4`,
`/out/temp5` respectively.

---

## 3. HARDWARE & PIN MAPPING

| GPIO | Label | Function (Default) | Alt: Blinds | Alt: Buttons | Alt: Shutters | Notes |
|------|-------|-------------------|-------------|--------------|---------------|-------|
| 0 | D3 | Button (INPUT_PULLUP) | - | - | - | Flash button, bootstrap |
| 2 | D4 | Button (INPUT_PULLUP) | - | - | - | Boot strap, LED on some boards |
| 4 | D2 | DHT22 data | - | - | - | DHT lib handles pin (timeout=15) |
| 5 | D1 | DS18B20 data | - | - | - | OneWire bus |
| 12 | D6 | PWM out (0-255) | Somfy TX | Button in | - | `analogWriteRange(255)` |
| 13 | D7 | PWM out (0-255) | Somfy TX | Button in | - | |
| 14 | D5 | Relay out (HIGH=ON) | - | - | Shutter UP | Active HIGH by default |
| 16 | D0 | Relay out (HIGH=ON) | - | - | Shutter DOWN | No boot if pulled LOW |
| A0 | - | Analog in (0-1023) | - | Button (thresh) | - | 1V max, divider for 5V/10V |
| VU | 5V | Power out | - | - | - | For sensors/relays |

**Bootstrap Notes:**
- GPIO0 (D3) LOW at boot -> flash mode
- GPIO2 (D4) LOW at boot -> flash mode
- GPIO15 (D8) HIGH at boot -> prevents boot (relay modules!)
- GPIO16 (D0) no internal pullup, used for deep sleep wake

**Pin Limitations (from source comments):**
- D2 (GPIO4): "output not work" - avoid as output
- D5 (GPIO14): "output not work" - but used for relay (works as output?)
- D8 (GPIO15): "rele no boot, not in use" - must not be HIGH at boot

---

## 4. LIBRARIES & DEPENDENCIES

| Library | Version | Purpose | In ESP8266 Core |
|---------|---------|---------|-----------------|
| `EEPROM` | Built-in | Persistent config storage | Yes |
| `OneWire` | 2.3+ | DS18B20 communication | No (install) |
| `DallasTemperature` | 3.9+ | DS18B20 parsing | No (install) |
| `DHT` | 1.4+ | DHT22 temp/humidity | No (install) |
| `ESP8266WiFi` | Core | WiFi STA/AP | Yes |
| `PubSubClient` | 2.8+ | MQTT client | No (install) |

**Arduino IDE Board Manager:** ESP8266 by ESP8266 Community
(v3.0.2+ recommended)

---

## 5. EEPROM MEMORY MAP (256 bytes)

| Address | Size | Type | Description |
|---------|------|------|-------------|
| 0-19 | 20 | byte | I/O states (pwm12, pwm13, rele14, rele16, somfy11, shut15) |
| 50-53 | 4 | byte | MQTT server IP (4 octets) |
| 60-63 | 4 | long | Counter value0 (unsigned long) |
| 65-68 | 4 | long | Counter value1 |
| 70-73 | 4 | long | Counter value2 |
| 75-78 | 4 | long | Counter value3 |
| 80-99 | 20 | char | Device name (str_name) |
| 100 | 1 | bool | `state` - restore outputs on boot |
| 101 | 1 | bool | `period` - periodic sensor publish |
| 102 | 1 | bool | `http` - enable web server |
| 103 | 1 | bool | `blinds` - GPIO12/13 as Somfy |
| 104 | 1 | bool | `term` - thermal protection |
| 105 | 1 | bool | `count` - pulse counters on GPIO0/2 |
| 106 | 1 | bool | `buttons` - GPIO12/13 as buttons |
| 107 | 1 | byte | `gap` - sensor interval multiplier (0-255) |
| 108 | 1 | bool | `sensor` - periodic analog publish |
| 109 | 1 | bool | `net_default` - use hardcoded defaults |
| 110 | 1 | bool | `shutters` - GPIO14/16 as shutter motor |
| 111 | 1 | bool | `revers` - invert relay logic |
| 112 | 1 | bool | `net_ap_mode` - run as AP |
| 200-219 | 20 | char | WiFi SSID (str_ssid) |
| 220-239 | 20 | char | WiFi Password (str_pass) |

**String Storage:** Custom base64-safe encoding
(`0-9a-zA-Z_-`) in `my_str_write()`/`my_str_read()`

---

## 6. CONFIGURATION & SETTINGS

### 6.1 Default Values (Hardcoded)

```cpp
String str_name = "wemos";
String str_ssid = "hjhome";
String str_pass = "pass1234";
IPAddress mqtt_default_server(192,168,0,10);

// AP Mode defaults
String ap_ssid = "ESP8266_hjhome";
String ap_pass = "pass1234";
IPAddress ap_ip(192,168,9,1);
IPAddress ap_gateway(192,168,9,1);
IPAddress ap_subnet(255,255,255,0);
IPAddress mqtt_ap_server(192,168,9,10);
```

### 6.2 Runtime Configuration (via MQTT)

All settings persist to EEPROM immediately on change.

| Setting | Topic | Payload | Range | Effect |
|---------|-------|---------|-------|--------|
| Device name | `/<name>/in/name` | String | 1-19 chars | MQTT topic prefix, web title |
| WiFi SSID | `/<name>/in/ssid` | String | 1-19 chars | Next boot |
| WiFi Password | `/<name>/in/pass` | String | 1-19 chars | Next boot |
| MQTT Server | `/<name>/in/mqtt` | `a.b.c.d` | Valid IPv4 | Next boot |
| Network mode | `/<name>/in/default` | `0\|1` | bool | Use hardcoded defaults |
| AP mode | `/<name>/in/apmode` | `0\|1` | bool | Run as AP hotspot |
| State restore | `/<name>/in/state` | `0\|1` | bool | Restore PWM/relay on boot |
| Periodic publish | `/<name>/in/period` | `0\|1` | bool | Enable temp/hum periodic |
| Web server | `/<name>/in/http` | `0\|1` | bool | Enable HTTP on port 80 |
| Blinds mode | `/<name>/in/blinds` | `0\|1` | bool | GPIO12/13 = Somfy (disables buttons) |
| Buttons mode | `/<name>/in/buttons` | `0\|1` | bool | GPIO12/13 = buttons (disables blinds) |
| Shutters mode | `/<name>/in/shutters` | `0\|1` | bool | GPIO14/16 = shutter (disables term) |
| Thermal protect | `/<name>/in/term` | `0\|1` | bool | Anti-freeze/overheat (disables shutters) |
| Relay invert | `/<name>/in/revers` | `0\|1` | bool | Invert GPIO14/16 logic |
| Pulse count | `/<name>/in/count` | `0\|1` | bool | Count pulses on GPIO0/2 |
| Sensor interval | `/<name>/in/gap` | `0-255` | byte | Multiplier: base 6.6s/7.7s/1.7s |
| Analog publish | `/<name>/in/sensor` | `0\|1` | bool | Periodic A0 publish |

### 6.3 Output Control (Runtime)

| Output | Topic | Payload | Range |
|--------|-------|---------|-------|
| PWM D6 | `/<name>/in/pwm12` | int | 0-255 |
| PWM D7 | `/<name>/in/pwm13` | int | 0-255 |
| Relay D5 | `/<name>/in/rele14` | `0\|1\|ON\|OFF\|true\|false` | bool |
| Relay D0 | `/<name>/in/rele16` | `0\|1\|ON\|OFF\|true\|false` | bool |
| Somfy pos | `/<name>/in/somfy11` | int | 0-255 (sec) |
| Shutter pos | `/<name>/in/shut15` | int | 0-255 (sec) |

---

## 7. MQTT API SPECIFICATION

### 7.1 Topic Convention

- **Inbound (device <- broker):** `/<device_name>/in/<command>`
- **Outbound (device -> broker):** `/<device_name>/out/<status>`

### 7.2 Command Topics (Inbound)

| Command | Payload | Response Topic | Description |
|---------|---------|----------------|-------------|
| `echo` | any | `/out/echo` | Echo test |
| `test` | - | Various | Request all current states |
| `ip` | - | `/out/ip` | Current IP address |
| `mac` | - | `/out/mac` | MAC address |
| `srv` | - | `/out/srv` | MQTT server IP |
| `info` | - | `/out/info` | Full device info dump |
| `reboot` | - | `/out/reboot` | Restart device (publishes "manual restart") |
| `memory` | - | `/out/memory` | Free heap bytes |
| `millis` | - | `/out/millis` | Uptime in milliseconds |
| `name` | new name | `/out/name` | Change device name (1-19 chars) |
| `ssid` | new ssid | `/out/ssid` | Change WiFi SSID (1-19 chars) |
| `pass` | new pass | `/out/pass` | Change WiFi password (1-19 chars) |
| `mqtt` | `a.b.c.d` | `/out/mqtt` | Change MQTT server IPv4 |
| `default` | `0\|1` | `/out/default` | Toggle default network |
| `apmode` | `0\|1` | `/out/apmode` | Toggle AP mode |
| `state` | `0\|1` | `/out/state` | Toggle state restore on boot |
| `period` | `0\|1` | `/out/period` | Toggle periodic sensor publish |
| `http` | `0\|1` | `/out/http` | Toggle web server |
| `blinds` | `0\|1` | `/out/blinds` | Enable Somfy mode (disables buttons) |
| `buttons` | `0\|1` | `/out/buttons` | Enable button mode (disables blinds) |
| `shutters` | `0\|1` | `/out/shutters` | Enable shutter mode (disables term) |
| `revers` | `0\|1` | `/out/revers` | Invert relay logic (GPIO14/16) |
| `term` | `0\|1` | `/out/term` | Enable thermal protection (disables shutters) |
| `count` | `0\|1` | `/out/count` | Enable pulse counting on GPIO0/2 |
| `gap` | `0-255` | `/out/gap` | Sensor interval multiplier |
| `sensor` | `0\|1` | `/out/sensor` | Enable analog A0 periodic publish |
| `value0-3` | unsigned long | `/out/value0-3` | Set pulse counters |
| `pwm12/13` | `0-255` | `/out/pwm12/13` | Set PWM target |
| `rele14/16` | `0/1/ON/OFF` | `/out/rele14/16` | Set relay (true/false/ON/OFF) |
| `somfy11` | `0-255` | `/out/somfy11` | Set blind target position (seconds) |
| `shut15` | `0-255` | `/out/shut15` | Set shutter target position (seconds) |
| `_tmp4` | int (-99..99) | `/out/tmp4` | Simulate DHT temperature |
| `_hum4` | int (0-100) | `/out/hum4` | Simulate DHT humidity |
| `_temp5` | int (-99..99) | `/out/temp5` | Simulate DS18B20 temperature |

**Note:** Button topics (`/in/bt0`, `/in/bt2`, etc.) are **output-only**.
Physical buttons cannot be triggered via MQTT.

### 7.3 Status Topics (Outbound - Automatic)

| Topic | Payload | Trigger |
|-------|---------|---------|
| `/out/start` | `start` | MQTT connect (Last Will: QoS 1, retain 0, "disconnect") |
| `/out/bt0,2,12,13,17` | `0\|1\|2` | Button: release/short/long |
| `/out/value0,1,2,3` | unsigned long | Pulse counter increment |
| `/out/temp4` | degC | DHT temp (periodic) |
| `/out/temp5` | degC | DS18B20 temp (periodic) |
| `/out/hum4` | % | DHT humidity (periodic) |
| `/out/an0` | 0-1023 | Analog A0 (periodic) |
| `/out/pwm12,13` | 0-255 | On PWM change |
| `/out/rele14,16` | `0\|1` | On relay change |
| `/out/somfy11` | 0-255 | On Somfy target change |
| `/out/shut15` | 0-255 | On shutter target change |
| `/out/reset` | reason | Memory watchdog or button reset |
| `/out/tmp4` | degC | On `_tmp4` simulation input |
| `/out/hum4` | % | On `_hum4` simulation input |
| `/out/temp5` | degC | On `_temp5` simulation input |
| `/out/ip` | IP addr | On `/in/ip` request |
| `/out/mac` | MAC addr | On `/in/mac` request |
| `/out/srv` | MQTT IP | On `/in/srv` request |
| `/out/echo` | payload | On `/in/echo` request |
| `/out/memory` | bytes | On `/in/memory` request |
| `/out/millis` | ms | On `/in/millis` request |
| `/out/info` | string | On `/in/info` request |

### 7.4 Periodic Publishing Intervals

Base intervals (multiplied by `gap+1`):

| Sensor | Base Interval | Effective @ gap=0 | Effective @ gap=255 |
|--------|---------------|-------------------|---------------------|
| DHT temp/hum | 6600 ms | 6.6 sec | ~28 min |
| DS18B20 temp | 7700 ms | 7.7 sec | ~33 min |
| Analog A0 | 1700 ms | 1.7 sec | ~7 min |
| Thermal check | 80000 ms | 80 sec | 80 sec (fixed) |
| Memory check | 90000 ms | 90 sec | 90 sec (fixed) |
| PWM fade step | 9 ms | - | - |

---

## 8. DATA FLOW

### 8.1 Sensor Reading Flow

```
DS18B20 (GPIO5) --> OneWire --> DallasTemperature --> temp (degC)
--> MQTT /out/temp5
     |
     +--> Request every (7700*(gap+1)) ms

DHT22 (GPIO4) --> DHT library --> tmp(degC), hum(%)
--> MQTT /out/temp4, /out/hum4
     |
     +--> Read every (6600*(gap+1)) ms

Analog A0 --> analogRead() x10 avg --> an (0-1023)
--> MQTT /out/an0
     |
     +--> Read every (1700*(gap+1)) ms if sensor=1
```

### 8.2 Button Processing Flow

```
GPIO0,2,12,13 (digital) / A0 (analog threshold)
        |
        v
Debounce counter (bcount[])
        |  Digital: LOW->increment, HIGH->decrement (thresh 10)
        |  Analog: <10->increment, >666->decrement (thresh 333)
        v
State machine:
  bcount > 10  & bt==false  -> bt=true,  lcount=millis() (press detected)
  bcount == 0  & bt==true   -> bt=false, short press (1) if <500ms
  lcount>0 & >1000ms elapsed -> bt=false, long press (2), lcount=0
        |
        v
MQTT publish: /out/bt<GPIO> = "0" (release), "1" (short), "2" (long)
        |
        v
If count=1 & GPIO0/2: increment value0/1 or value1/2 (long press)
        |
        v
EEPROM persist (my_long_write at 60/65/70/75)
```

### 8.3 Output Control Flow

**PWM (Blinds=0, Buttons=0):**
```
MQTT /in/pwm12 -> pwm[12]=val -> EEPROM(12)
        |
        v
loop(): every 9ms -> my_pwm()
        |
        v
ac_pwm[12] <- step +-1 toward pwm[12] -> analogWrite(12, ac_pwm[12])
```

**Somfy (Blinds=1):**
```
MQTT /in/somfy11 > somfy=val (0-255 sec) > EEPROM(11)
        |
        v
loop(): my_somfy() state machine
        |
        +-- ac_somfy > somfy > somfy_close() > D6 LOW pulse, ac_somfy--
        +-- ac_somfy < somfy > somfy_open()  > D7 LOW pulse, ac_somfy++
        +-- ac_somfy == somfy > somfy_stop() > both HIGH, stop_count=200ms
```
**Timing:** `scount` updated every 1000ms (1 step/sec). `stop_count`:
200ms LOW pulse, 400ms total stop delay before new movement.

**Shutters (Shutters=1, Term=0):**
```
MQTT /in/shut15 > shut=val (0-255 sec) > EEPROM(15)
        |
        v
loop(): every 1s > my_shutters()
        |
        +-- ac_shut > shut & rele[16]==0 > rele[14]=1 (UP), ac_shut--
        +-- ac_shut < shut & rele[14]==0 > rele[16]=1 (DOWN), ac_shut++
        +-- ac_shut == shut > both relays OFF after 500ms gap
```
**Timing:** `shcount` updated every 1000ms (1 step/sec). Direction
change requires 500ms gap with both relays OFF.

**Relays (Term=0, Shutters=0):**
```
MQTT /in/rele14 > rele[14]=bool > EEPROM(14) > my_rele(14)
        |
        v
my_rele(): if revers=0 > digitalWrite(pin, rele)
                    if revers=1 > digitalWrite(pin, !rele)
```
**Note:** `ee_wr()` only writes when value changes, reducing EEPROM wear.

**Thermal Protection (Term=1, Shutters=0):**
```
Every 80s > my_term()
        |
        +-- DS18B20 temp < 7degC  > rele[14]=1 (heating ON)
        +-- DS18B20 temp > 10degC > rele[14]=0 (heating OFF)
        +-- DS18B20 temp > 33degC > rele[16]=1 (cooling ON)
        +-- DS18B20 temp < 30degC > rele[16]=0 (cooling OFF)
        |
        v
EEPROM persist + MQTT publish /out/rele14, /out/rele16
```
**Note:** Uses DS18B20 (GPIO5) temperature only, not DHT22.

---

## 9. FUNCTION REFERENCE

### 9.1 Core Functions

| Function | Purpose | Called From |
|----------|---------|-------------|
| `setup()` | Hardware init, network, MQTT, sensors, web | Arduino framework |
| `loop()` | Main cooperative scheduler | Arduino framework |
| `my_connect()` | WiFi STA/AP + MQTT connect/reconnect | setup, loop, callback |
| `my_ap_mode()` | Configure AP hotspot | setup (if net_ap_mode) |
| `callback()` | MQTT message dispatcher | PubSubClient |

### 9.2 EEPROM & Storage

| Function | Purpose |
|----------|---------|
| `my_eeprom_states()` | Load all boolean settings from EEPROM 100-112 |
| `my_pin_mode()` | Configure GPIO directions per active modes |
| `ee_wr(addr, val)` | Write byte to EEPROM if changed + commit |
| `my_str_write(str, shift)` | Store base64-safe string at offset |
| `my_str_read(def, shift)` | Read string from EEPROM offset |
| `my_long_write(addr, val)` | Store unsigned long as 4 bytes |
| `my_long_read(addr)` | Read unsigned long from 4 bytes |

### 9.3 Sensor Functions

| Function | Purpose | Interval |
|----------|---------|----------|
| `my_temp()` | Read DS18B20, publish /out/temp5 | 7700*(gap+1) ms |
| `my_dht()` | Read DHT22, publish /out/temp4, /out/hum4 | 6600*(gap+1) ms |
| `my_analog()` | Read A0 (avg 10), publish /out/an0 | 1700*(gap+1) ms |

### 9.4 Output Control

| Function | Purpose | Timing |
|----------|---------|--------|
| `my_pwm()` | Smooth PWM fade (step +-1 every 9ms) | 9ms step |
| `my_somfy()` | Somfy RTS state machine | 1000ms step, 200/400ms stop |
| `my_shutters()` | Shutter position control (1s steps) | 1000ms step, 500ms gap |
| `my_rele(pin)` | Apply relay state with revers logic | Called every loop |
| `somfy_stop/open/close()` | Low-level Somfy pin control | - |

### 9.5 Input & Button Handling

| Function | Purpose | Debounce |
|----------|---------|----------|
| `my_button(gpio)` | Debounce + short/long press | Digital: 10 loops, Analog: 333 loops |
| `my_reset()` | Dual-button 20s/30s hold detection | Uses rcount[0], rcount[2] |

### 9.6 Web & Utility

| Function | Purpose |
|----------|---------|
| `my_web()` | HTTP handler - serves status page |
| `my_print()` | MQTT publish helper (topic + payload) |
| `my_memory()` | Heap watchdog (<10KB > reset) |
| `my_term()` | Thermal protection logic |
| `my_reset_print()` | Log + publish reset reason |
| `str_to_char()` | String > char* helper |

---

## 10. NETWORK BEHAVIOR

### 10.1 Startup Sequence

```
1. WiFi.softAPdisconnect(true)  // Clean up any prior AP
2. EEPROM.begin(256)
3. my_eeprom_states()           // Load all mode flags
4. my_pin_mode()                // Configure GPIOs per mode
5. Network selection:
   if net_default==true    -> Use hardcoded ssid/pass/mqtt
   else if net_ap_mode     -> Start AP, use ap_ssid/ap_pass/mqtt_ap_server
   else                    -> Load saved ssid/pass/mqtt from EEPROM
6. my_connect()                 // WiFi + MQTT
7. Web server begin (if http=1)
8. Sensor begin (OneWire, DHT)
```

### 10.2 Reconnection Logic

- **MQTT disconnected + count=1:** retry every 299 seconds
  (millis()%299000==0)
- **MQTT disconnected + count=0:** retry every 99 seconds
  (millis()%99000==0)
- **WiFi disconnected (STA):** 10 attempts x 1 second in `my_connect()`
- **MQTT connect:** uses Last Will `/name/out/start` = "disconnect"

### 10.3 AP Mode Details

- IP: 192.168.9.1, Gateway: 192.168.9.1, Subnet: 255.255.255.0
- MQTT server expected at 192.168.9.10
- SoftAP MAC used as device identifier
- **Marked TODO** - not fully tested

---

## 11. WEB INTERFACE

### 11.1 HTTP Endpoint

- **Port:** 80
- **Path:** `/` (root only)
- **Response:** HTML status page (auto-refresh via button)

### 11.2 Page Content

| Section | Content |
|---------|---------|
| Header | Device name, Reload button |
| Network | IP, MAC, MQTT server |
| Commands | Full MQTT command reference with descriptions |
| Pin Table | All GPIOs: mode, topic, payload range, current value |
| EEPROM Table | All settings: topic, payload range, current value |
| Footer | Project link + version |

---

## 12. RESET & RECOVERY

### 12.1 Hardware Reset (Button Combination)

| Hold Duration | Buttons | Action |
|---------------|---------|--------|
| 20 seconds | GPIO0 (D3) + GPIO2 (D4) | Reset to default network (net_default=1, net_ap_mode=0) |
| 30 seconds | GPIO0 (D3) + GPIO2 (D4) | Toggle **AP mode** (if already default) |

**Implementation:** Tracks `rcount[0]` and `rcount[2]` (long press
timestamps). Checks in `my_reset()` every loop.

### 12.2 Software Reset Triggers

| Trigger | Topic | Payload |
|---------|-------|---------|
| MQTT `/in/reboot` | `/out/reboot` | "manual restart" |
| Heap < 10KB | `/out/reset` | "Error! free memory = X" |
| Button reset (20s/30s) | `/out/reset` | "Reset: default=X, apmode=X" |

---

## 13. DEVELOPER GUIDE

### 13.1 Building & Flashing

1. **Arduino IDE Setup:**
   - Board Manager: Add `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
   - Install "ESP8266 by ESP8266 Community" v3.0.2+
   - Select Board: "Wemos D1 R1" or "NodeMCU 1.0 (ESP-12E Module)"
   - Flash Size: 4MB (FS: 1MB or 2MB)
   - CPU Frequency: 80 MHz
   - Upload Speed: 921600

2. **Library Installation (Library Manager):**
   - OneWire (by Paul Stoffregen)
   - DallasTemperature (by Miles Burton)
   - DHT sensor library (by Adafruit) - **requires Adafruit Unified Sensor**
   - PubSubClient (by Nick O'Leary)

3. **Compile & Upload:** Standard Arduino IDE workflow

### 13.2 Code Structure Conventions

- **Global variables** at top (no hardcoded values in functions)
- **EEPROM addresses** as constants in comments only
- **Function prefix:** `my_` for project functions
- **Boolean flags:** lowercase (`state`, `period`, `blinds`...)
- **Pin arrays:** `bt[18]`, `rele[18]`, `pwm[18]`, `ac_pwm[18]`, `bcount[18]`
- **Timing:** `millis() % interval == 0` pattern (no `delay()` in loop except `delay(0)`)

### 13.3 Adding New Features

1. **New MQTT command:** Add `if(inc_topic.indexOf("/cmd")>0)` block in `callback()`
2. **New sensor:** Add reading function, call from `loop()` with `millis()%interval`
3. **New output:** Add to `my_pin_mode()`, `my_rele()` or new control function
4. **New setting:** Reserve EEPROM address, add to `my_eeprom_states()`, add MQTT handler
5. **Web UI:** Extend `my_web()` HTML generation

### 13.4 Debugging

- **Serial:** 115200 baud, extensive debug output in `setup()`, `my_connect()`, `callback()`
- **MQTT:** Subscribe to `/<name>/out/#` for all device traffic
- **Web:** Open `http://<device_ip>/` in browser
- **Memory:** Monitor `/out/memory` or web page "EEPROM" section

---

## 14. TESTER GUIDE

### 14.1 Pre-Flash Checklist

- [ ] Correct board selected (Wemos D1 mini / NodeMCU)
- [ ] Libraries installed (OneWire, DallasTemperature, DHT, PubSubClient)
- [ ] No compilation errors
- [ ] Serial monitor opens at 115200

### 14.2 Functional Test Matrix

| Test Case | Steps | Expected Result |
|-----------|-------|-----------------|
| **Boot - Default** | Flash, power on, no EEPROM | Connects to `hjhome`/`pass1234` |
| **Boot - Saved** | Configure via MQTT, power cycle | Restores name, ssid, pass, mqtt, all modes |
| **Boot - AP Mode** | Set `apmode=1`, power cycle | Creates AP `ESP8266_hjhome` |
| **MQTT Connect** | Start broker, observe Serial | Connected to MQTT, subscribes to `/name/#` |
| **Relay Control** | Publish `/in/rele14` = "1" | GPIO14 HIGH, `/out/rele14` = "1" |
| **PWM Fade** | Publish `/in/pwm12` = "128" | Smooth fade to 50% over ~2.3s |
| **Button Press** | Short press GPIO0 | `/out/bt0` = "1" |
| **Button Long** | Hold GPIO0 >1s | `/out/bt0` = "2" |
| **Pulse Count** | `count=1`, pulse GPIO0 | `/out/value0` increments |
| **DHT Read** | `period=1`, wait | `/out/temp4`, `/out/hum4` periodic |
| **DS18B20** | `period=1`, wait | `/out/temp5` periodic |
| **Analog** | `sensor=1`, wait | `/out/an0` periodic |
| **Somfy** | `blinds=1`, `/in/somfy11=50` | D6/D7 pulse sequence for 50s |
| **Shutters** | `shutters=1`, `/in/shut15=100` | D5/D0 sequence for position 100 |
| **Thermal** | `term=1`, heat sensor | Relay14 ON <7degC, OFF >10degC |
| **Web UI** | Browser to device IP | Full status page loads |
| **Reset 20s** | Hold D3+D4 20s | Reboots, net_default=1 |
| **Reset 30s** | Hold D3+D4 30s (from default) | Reboots, net_ap_mode=1 |
| **MQTT Reboot** | Publish `/in/reboot` | Device restarts |
| **Memory Low** | Simulate heap <10KB | Auto reset with `/out/reset` |
| **Sensor Sim** | Publish `/in/_tmp4=25` | `/out/tmp4` = "25", DHT temp overridden |
| **Relay Persist** | `state=1`, set relay, reboot | Relay state restored on boot |
| **PWM Persist** | `state=1`, set PWM, reboot | PWM value restored on boot |

### 14.3 Edge Cases to Verify

- [ ] MQTT broker down at boot -> periodic reconnect
- [ ] WiFi credentials wrong -> retries 10x then stays disconnected
- [ ] Simultaneous button presses (D3+D4+D6+D7)
- [ ] PWM + Somfy mode conflict (enforced by EEPROM)
- [ ] Shutter + Term conflict (enforced by EEPROM)
- [ ] EEPROM write endurance (writes only on change via `ee_wr()`)
- [ ] String encoding edge cases (special chars in name/ssid/pass)
- [ ] Sensor simulation override via `/in/_tmp4`, `/in/_hum4`, `/in/_temp5`
- [ ] Relay persistence with `state=1` across reboot
- [ ] PWM persistence with `state=1` across reboot
- [ ] AP mode fallback when STA fails (currently: stays disconnected)
- [ ] Base64 validation bug: `my_str_write` index check should be `>=0`
- [ ] Missing braces in `my_str_write` else branch (early return only)
- [ ] PWM loop TODO: `i+=2` comment suggests optimization
- [ ] Analog button only active when `sensor=0`
- [ ] Thermal protection uses DS18B20 only (not DHT22)
- [ ] Reconnect interval differs by `count` mode (299s vs 99s)
- [ ] WiFi reconnect: 10 attempts x 999ms each
- [ ] Web request length limit: <100 chars processed

---

## 15. USER / OWNER GUIDE

### 15.1 Initial Setup

1. **Power** the Wemos D1 mini via USB or 5V to VU pin
2. **Connect** to WiFi network `hjhome` (pass: `pass1234`) — or configure AP mode
3. **MQTT Broker** must be at `192.168.0.10` (default) or configure via AP/web
4. **Discover** device: subscribe to `wemos/out/#` (default name: `wemos`)

### 15.2 Common Operations

| Task | MQTT Command Example |
|------|---------------------|
| Turn on relay (D5) | `mosquitto_pub -t 'wemos/in/rele14' -m 'ON'` |
| Dim LED (D6) to 50% | `mosquitto_pub -t 'wemos/in/pwm12' -m '128'` |
| Set blind position | `mosquitto_pub -t 'wemos/in/somfy11' -m '30'` |
| Set shutter position | `mosquitto_pub -t 'wemos/in/shut15' -m '100'` |
| Change device name | `mosquitto_pub -t 'wemos/in/name' -m 'livingroom'` |
| Change WiFi | `mosquitto_pub -t 'livingroom/in/ssid' -m 'MyWiFi'` then `.../in/pass` |
| Change MQTT server | `mosquitto_pub -t 'livingroom/in/mqtt' -m '192.168.1.50'` |
| Enable web UI | `mosquitto_pub -t 'livingroom/in/http' -m '1'` |
| View status page | Open `http://<device_ip>/` in browser |
| Reboot device | `mosquitto_pub -t 'livingroom/in/reboot' -m ''` |

### 15.3 Button Functions (Physical)

| Button | Short Press (<1s) | Long Press (>1s) |
|--------|-------------------|------------------|
| D3 (GPIO0) | Publishes `/out/bt0` = "1" | Publishes `/out/bt0` = "2", increments counter |
| D4 (GPIO2) | Publishes `/out/bt2` = "1" | Publishes `/out/bt2` = "2", increments counter |
| D6 (GPIO12)* | If `buttons=1`: same as above | If `buttons=1`: same as above |
| D7 (GPIO13)* | If `buttons=1`: same as above | If `buttons=1`: same as above |
| A0 (ADC)* | If `sensor=0`: threshold button | If `sensor=0`: threshold button |

*Only active when corresponding mode enabled.

### 15.4 Factory Reset

1. **Power on** device
2. **Press and hold** both **D3 (Flash)** and **D4** buttons simultaneously
3. **Hold 20 seconds** -> Resets to default network (connects to `hjhome`)
4. **Hold 30 seconds** (from default) -> Enables AP mode (`ESP8266_hjhome`)

---

## 16. PRODUCTION OPERATIONS

### 16.1 Reliability Features

| Feature | Implementation | Purpose |
|---------|----------------|---------|
| **Watchdog** | `my_memory()` checks heap <10KB -> reset | Prevents memory leak lockup |
| **Last Will** | MQTT `/name/out/start` = "disconnect", QoS 1, retain 0 | Broker detects offline |
| **EEPROM wear** | `ee_wr()` writes only on value change | Extends flash life |
| **Non-blocking** | `millis()` scheduling, no `delay()` in loop | Prevents starvation |
| **Auto-reconnect** | Periodic MQTT/WiFi retry in loop | Survives network outages |
| **State restore** | `state=1` restores PWM/relay on boot | Power loss recovery |
| **Thermal protect** | `term=1` auto-controls relays by DS18B20 temp | Hardware safety |
| **Dual reset** | 20s/30s button combo (D3+D4) | Field recovery without tools |
| **Button debounce** | Digital: 10 counts, Analog: 333 counts | Filters noise |
| **PWM smooth fade** | 9ms step (2.3s full range) | No visible flicker |
| **Direction gap** | 500ms relay off between UP/DOWN | Motor protection |

### 16.2 Deployment Checklist

- [ ] Unique device name per node (MQTT topic isolation)
- [ ] MQTT broker with persistence + QoS 1 for critical topics
- [ ] WiFi signal > -70 dBm at install location
- [ ] Relay modules: verify GPIO15 (D8) not used (boot fail)
- [ ] Analog sensors: voltage divider for >1V inputs
- [ ] DS18B20: 4.7k pullup on data line
- [ ] DHT22: 10k pullup on data line (timeout=15 cycles)
- [ ] Enclosure: ventilation for ESP8266 heat
- [ ] Backup power (UPS) for broker + router
- [ ] Document device name, location, mode config per install
- [ ] Set `gap` for desired sensor interval (0=fast, 255=slow)
- [ ] Enable `state=1` for output persistence across power loss
- [ ] Enable `term=1` for unattended freeze/overheat protection

### 16.3 Monitoring & Maintenance

| Metric | Source | Alert Threshold |
|--------|--------|-----------------|
| Free heap | `/out/memory` (periodic) | < 15 KB |
| Uptime | `/out/start` on reconnect | Frequent reconnects |
| Sensor health | `/out/temp4`, `/out/temp5`, `/out/hum4` | Stale > 2x interval |
| Counter drift | `/out/value0-3` vs expected | Unexpected jumps |
| WiFi RSSI | Not published (add if needed) | < -80 dBm |

**Log Retention:** MQTT broker should retain last known state
(`retain=true` on `/out/` status topics) for new subscribers.

### 16.4 Timing Reference

| Operation | Interval | Note |
|-----------|----------|------|
| MQTT reconnect (count=1) | 299 sec | millis()%299000==0 |
| MQTT reconnect (count=0) | 99 sec | millis()%99000==0 |
| WiFi connect retry | 10 x 999 ms | In my_connect() |
| DHT read | 6600*(gap+1) ms | Base 6.6 sec |
| DS18B20 read | 7700*(gap+1) ms | Base 7.7 sec |
| Analog read | 1700*(gap+1) ms | Base 1.7 sec |
| Thermal check | 80000 ms | Fixed 80 sec |
| Memory check | 90000 ms | Fixed 90 sec |
| PWM fade step | 9 ms | ~2.3s full range |
| Somfy step | 1000 ms | 1 sec per position |
| Somfy stop delay | 200/400 ms | Pulse + hold |
| Shutter step | 1000 ms | 1 sec per position |
| Shutter direction gap | 500 ms | Both relays OFF |
| Button debounce (digital) | 10 counts | ~10 loops |
| Button debounce (analog) | 333 counts | ~333 loops |
| Short press threshold | < 500 ms | lcount > millis()-500 |
| Long press threshold | > 1000 ms | lcount < millis()-999 |
| Reset detection | 20s / 30s | rcount[0] & rcount[2] |
| Web request max | < 100 chars | Longer ignored |
| MQTT Last Will | QoS 1, retain 0 | Topic: /name/out/start |

---

## 17. KNOWN LIMITATIONS & TODOs

| Item | Status | Notes |
|------|--------|-------|
| AP Mode | **TODO** - marked untested | `my_ap_mode()` called but not verified |
| Base64 encoding | **TODO** - test comparison | `my_str_write` has incomplete validation |
| Watchdog timer | Disabled | `ESP.wdtEnable()` commented out |
| OTA updates | Not implemented | Requires ArduinoOTA library |
| TLS/SSL MQTT | Not supported | Plaintext only |
| Multiple DS18B20 | Single sensor only | Index 0 hardcoded |
| Persistent PWM | Only GPIO12/13 saved | Relays saved only if `state=1` |
| Button debounce | Fixed thresholds | Not configurable |
| Shutter position | Open-loop (time-based) | No position feedback sensor |
| Somfy protocol | Timing-based simulation | No true RTS encoding |
| WiFi RSSI reporting | Not exposed | Add to `/out/info` or status |
| String validation bug | **BUG** - `my_str_write` | Index check `>0` should be `>=0` |
| Missing braces | **BUG** - `my_str_write` | Else branch missing braces, early return only |
| PWM loop TODO | **TODO** - `i+=2` comment | Suggests optimization opportunity |
| Web request limit | 100 chars max | Longer requests ignored |
| AP fallback | Not implemented | STA failure leaves device offline |
| Sensor simulation | Input-only, no persist | `_tmp4`, `_hum4`, `_temp5` topics |
| DHT timeout | Fixed at 15 cycles | Constructor param, not adjustable |
| Relay persist bug | `my_rele` only writes on true | `ee_wr` called only when rele[i]==true |
| Analog button mode | Only when `sensor=0` | Shared A0 pin |
| Thermal sensor | DS18B20 only | DHT22 temp ignored for protection |

---

## 18. CHANGELOG (from source comments)

| Date | Version | Changes |
|------|---------|---------|
| 22-aug-16 | Initial | Start |
| 17-mar-18 | | Disable AP, add memory |
| 25-mar-18 | | Long press, Somfy |
| 19-apr-18 | | Anti-freeze |
| 12-may-18 | | Client name edit |
| 13-may-18 | | Counters |
| 24-may-18 | | Buttons |
| 28-may-18 | | Reconnect logic |
| 01-jun-18 | | Gap interval |
| 04-jul-18 | | Last will |
| 08-jul-18 | | Default network |
| 16-sep-18 | | Fix MQTT server |
| 18-sep-18 | | Fix dimm delay |
| 20-sep-18 | | Reset timer 20s |
| 15-oct-18 | | Shutters |
| 08-nov-18 | | Dimm flash fix |
| 22-jan-19 | | Revers, shutter divider |
| 23-jan-19 | | Shutter 1s step fix |
| 25-jul-26 | **23-jul-26** | **Add AP mode** |

---

## 19. FILE INVENTORY

```
/app/arduino/wemos_automation/
├── AGENT.md                    # Agent rules (project constraints)
├── APP_DOCUMENTATION.md        # This file - developer documentation
├── wemos_automation.ino        # Main firmware (DO NOT EDIT - single file)
└── wemos_automation.ino.orig   # Backup/original version
```

---

## 20. LICENSE & ATTRIBUTION

```
hjhome / wemos_automation
Copyright (C) 2016 hjltu@ya.ru
https://launchpad.net/hjhome

GNU General Public License v3.0
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU GPL v3 as published by the FSF.
No warranty. See <http://www.gnu.org/licenses/>.
```

---

*Document generated from source code analysis. Version matches firmware: 23-jul-26*