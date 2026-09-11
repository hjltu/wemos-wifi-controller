# Wemos Automation - ESP8266 MQTT Controller

**Firmware for Wemos D1 mini / NodeMCU V3**
Control relays, PWM, blinds, shutters, and read sensors via MQTT.

---

## Quick Start

### Hardware Needed
- Wemos D1 mini (or NodeMCU V3)
- USB cable or 5V power supply
- Optional: Relay module, DHT22, DS18B20, buttons, Somfy blinds motor

### 1. Flash Firmware
```bash
# Arduino IDE:
# 1. Add board URL: http://arduino.esp8266.com/stable/package_esp8266com_index.json
# 2. Install "ESP8266 by ESP8266 Community" (v3.0.2+)
# 3. Board: "Wemos D1 R1" or "NodeMCU 1.0"
# 4. Install libraries: OneWire, DallasTemperature, DHT, PubSubClient
# 5. Open wemos_automation.ino -> Upload
```

### 2. Power On
- Connect USB or 5V to **VU** pin
- Device creates WiFi AP: **ESP8266_hjhome** (pass: `pass1234`)
- Or connects to **hjhome** / `pass1234` if available

### 3. Configure via MQTT (or Web UI)
Default MQTT broker: `192.168.0.10`

```bash
# Change device name
mosquitto_pub -t 'wemos/in/name' -m 'livingroom'

# Change WiFi
mosquitto_pub -t 'livingroom/in/ssid' -m 'MyWiFi'
mosquitto_pub -t 'livingroom/in/pass' -m 'MyPassword'

# Change MQTT broker
mosquitto_pub -t 'livingroom/in/mqtt' -m '192.168.1.50'
```

### 4. Verify
```bash
# Subscribe to all status
mosquitto_sub -t 'livingroom/out/#' -v

# Should see: livingroom/out/start start
```

---

## Pinout Reference

| Pin | GPIO | Default Function | Alt: Blinds | Alt: Buttons | Alt: Shutters |
|-----|------|------------------|-------------|--------------|---------------|
| D3  | 0    | Button (Flash)   | -           | -            | -             |
| D4  | 2    | Button           | -           | -            | -             |
| D2  | 4    | DHT22 Temp/Hum   | -           | -            | -             |
| D1  | 5    | DS18B20 Temp     | -           | -            | -             |
| D6  | 12   | PWM / LED        | Somfy TX    | Button       | -             |
| D7  | 13   | PWM / LED        | Somfy TX    | Button       | -             |
| D5  | 14   | Relay 1          | -           | -            | Shutter UP    |
| D0  | 16   | Relay 2          | -           | -            | Shutter DOWN  |
| A0  | -    | Analog 0-1023    | -           | Button*      | -             |
| A0  | -    | NTC 10k (term=3) | -           | -            | -             |

*Analog button only when `sensor=0`
*NTC: 10k thermistor + pull-down, ~15-35°C range

---

## Operating Modes (Pick One per Group)

| Group | Modes | Description |
|-------|-------|-------------|
| **GPIO12/13** | `blinds` OR `buttons` OR `pid` | Somfy blinds OR 2 extra buttons OR PID control |
| **GPIO14/16** | `shutters` OR `term` | Shutter motor OR Thermal protection (term=1/2/3) |
| **Sensors** | `period` + `sensor` + `ntc` | Periodic temp/hum + analog + NTC |
| **Network** | `default` OR `apmode` | Use saved WiFi OR create AP |

**Enable via MQTT:**
```bash
mosquitto_pub -t 'livingroom/in/blinds' -m '1'    # Somfy mode
mosquitto_pub -t 'livingroom/in/buttons' -m '1'   # Button mode
mosquitto_pub -t 'livingroom/in/shutters' -m '1'  # Shutter mode
mosquitto_pub -t 'livingroom/in/term' -m '1'      # Thermal protect: DS18B20
mosquitto_pub -t 'livingroom/in/term' -m '2'      # Thermal protect: DHT22
mosquitto_pub -t 'livingroom/in/term' -m '3'      # Thermal protect: NTC (requires ntc=1)
```

---

## MQTT API

### Topic Format
- **Inbound** (you -> device): `/name/in/command`
- **Outbound** (device -> you): `/name/out/status`

### Commands (Publish to `/name/in/...`)

| Command | Payload | Example |
|---------|---------|---------|
| `echo` | any | Echo test |
| `reboot` | (empty) | Restart device |
| `name` | new name (1-19 chars) | `livingroom` |
| `ssid` | WiFi name (1-19 chars) | `MyWiFi` |
| `pass` | WiFi password (1-19 chars) | `MyPass123` |
| `mqtt` | Broker IP | `192.168.1.50` |
| `default` | `0` or `1` | Use hardcoded defaults |
| `apmode` | `0` or `1` | Create AP hotspot |
| `state` | `0` or `1` | Save outputs on power loss |
| `period` | `0` or `1` | Auto-publish sensors |
| `http` | `0` or `1` | Enable web UI |
| `blinds` | `0` or `1` | Enable Somfy (D6/D7) |
| `buttons` | `0` or `1` | Enable buttons (D6/D7) |
| `shutters` | `0` or `1` | Enable shutters (D5/D0) |
| `revers` | `0` or `1` | Invert relay logic |
| `term` | `0-3` | Thermal protection (1=DS18B20, 2=DHT, 3=NTC) |
| `min` | `0-99` | Antifreeze setpoint (default 11) |
| `max` | `0-99` | Overheat setpoint (default 33) |
| `ntc` | `0` or `1` | Enable NTC 10k on A0 |
| `count` | `0` or `1` | Pulse counter (D3/D4) |
| `gap` | `0-255` | Sensor interval multiplier |
| `sensor` | `0` or `1` | Publish analog A0 |
| `pid_mode` | `0-5` | Enable PID (0=off, 1=DS18, 2=DHT, 3=NTC, 4=An, 5=MQTT) |
| `pid_rev` | `0` or `1` | Invert PID acting |
| `pid_ku` | `1-255` | Ultimate gain (ku/10) |
| `pid_tu` | `0-255` | Ultimate period (sec) |
| `pid_set` | `-99..111` | Target temperature |
| `pid_input` | `-255..255` | External MQTT setpoint |
| `value0-3` | number | Set pulse counters |
| `pwm12` | `0-255` | D6 brightness |
| `pwm13` | `0-255` | D7 brightness |
| `rele14` | `0/1/ON/OFF` | Relay 1 (D5) |
| `rele16` | `0/1/ON/OFF` | Relay 2 (D0) |
| `somfy11` | `0-255` | Blind position (seconds) |
| `shut15` | `0-255` | Shutter position (seconds) |
| `_tmp4` | `-99..99` | Simulate DHT temp |
| `_hum4` | `0-100` | Simulate DHT humidity |
| `_temp5` | `-99..99` | Simulate DS18B20 temp |

### Status Topics (Subscribe to `/name/out/...`)

| Topic | Payload | When |
|-------|---------|------|
| `start` | `start` / `disconnect` | MQTT connect / Last Will |
| `bt0,bt2,bt12,bt13,bt17` | `0`=release `1`=short `2`=long | Button press |
| `value0-3` | counter value | Pulse detected |
| `temp4` | degC | DHT temperature |
| `hum4` | % | DHT humidity |
| `temp5` | degC | DS18B20 temperature |
| `temp_ntc` | degC | NTC temperature (when ntc=1) |
| `an0` | `0-1023` | Analog input |
| `pwm12,13` | `0-255` | PWM changed |
| `pid_out` | float | PID calculation result |
| `rele14,16` | `0/1` | Relay changed |
| `somfy11` | `0-255` | Blind target changed |
| `shut15` | `0-255` | Shutter target changed |
| `reset` | reason | Watchdog or button reset |
| `ip` | IP address | On request |
| `mac` | MAC address | On request |
| `memory` | bytes free | On request |
| `info` | device info | On request |

---

## Common Use Cases

### 1. Simple Relay Control
```bash
# Turn on relay (D5)
mosquitto_pub -t 'livingroom/in/rele14' -m 'ON'

# Turn off
mosquitto_pub -t 'livingroom/in/rele14' -m 'OFF'

# State persists across reboot if enabled:
mosquitto_pub -t 'livingroom/in/state' -m '1'
```

### 2. LED Dimming (PWM)
```bash
# D6 to 50%
mosquitto_pub -t 'livingroom/in/pwm12' -m '128'

# D7 to 100%
mosquitto_pub -t 'livingroom/in/pwm13' -m '255'

# Off
mosquitto_pub -t 'livingroom/in/pwm12' -m '0'
```
*Smooth fade over ~2.3 seconds*

### 3. Somfy Blinds (Requires `blinds=1`)
```bash
# Enable blinds mode
mosquitto_pub -t 'livingroom/in/blinds' -m '1'

# Set position (0-255 seconds)
mosquitto_pub -t 'livingroom/in/somfy11' -m '30'   # 30s open
mosquitto_pub -t 'livingroom/in/somfy11' -m '0'    # Close
mosquitto_pub -t 'livingroom/in/somfy11' -m '128'  # Halfway
```

### 4. Roller Shutters (Requires `shutters=1`)
```bash
# Enable shutters mode
mosquitto_pub -t 'livingroom/in/shutters' -m '1'

# Position 0-255 (seconds)
mosquitto_pub -t 'livingroom/in/shut15' -m '100'  # Open
mosquitto_pub -t 'livingroom/in/shut15' -m '0'    # Close
```
*500ms pause between direction changes*

### 5. Temperature Monitoring
```bash
# Enable periodic publishing
mosquitto_pub -t 'livingroom/in/period' -m '1'

# Readings every ~6.6s (DHT) / ~7.7s (DS18B20)
# Adjust interval: gap=0 (fast) to gap=255 (~30 min)
mosquitto_pub -t 'livingroom/in/gap' -m '10'  # ~1 min intervals

# Subscribe
mosquitto_sub -t 'livingroom/out/temp4' -v  # DHT temp
mosquitto_sub -t 'livingroom/out/hum4' -v   # DHT humidity
mosquitto_sub -t 'livingroom/out/temp5' -v  # DS18B20
```

### 6. Thermal Protection (Requires `term=1/2/3`, `shutters=0`)
```bash
# Enable anti-freeze / overheat
mosquitto_pub -t 'livingroom/in/term' -m '1'   # DS18B20 (GPIO5)
mosquitto_pub -t 'livingroom/in/term' -m '2'   # DHT22 (GPIO4)
mosquitto_pub -t 'livingroom/in/term' -m '3'   # NTC 10k (A0, requires ntc=1)

# Relay 1 (D5): Heating ON < temp_min (default 11C), OFF > temp_min
# Relay 2 (D0): Cooling ON > temp_max (default 33C), OFF < temp_max
# Setpoints configurable:
mosquitto_pub -t 'livingroom/in/min' -m '11'   # Antifreeze setpoint
mosquitto_pub -t 'livingroom/in/max' -m '33'   # Overheat setpoint
```

### 7. Pulse Counting (Requires `count=1`)
```bash
# Enable counters on D3/D4
mosquitto_pub -t 'livingroom/in/count' -m '1'

# Short press -> value0/value2 increments
# Long press (>1s) -> value1/value3 increments

# Set initial values
mosquitto_pub -t 'livingroom/in/value0' -m '1000'
```

### 8. Physical Buttons
| Button | Short Press | Long Press (>1s) |
|--------|-------------|------------------|
| D3 (Flash) | `/out/bt0 = 1` | `/out/bt0 = 2` + counter |
| D4 | `/out/bt2 = 1` | `/out/bt2 = 2` + counter |
| D6* | `/out/bt12 = 1` | `/out/bt12 = 2` + counter |
| D7* | `/out/bt13 = 1` | `/out/bt13 = 2` + counter |
| A0* | `/out/bt17 = 1` | `/out/bt17 = 2` |

*Only in `buttons=1` or `sensor=0` mode

### 9. PID Temperature Control (Requires `pid_mode=1..5`)
```bash
# Enable PID using DS18B20 sensor (GPIO5)
mosquitto_pub -t 'livingroom/in/pid_mode' -m '1'

# Set target temperature to 22C
mosquitto_pub -t 'livingroom/in/pid_set' -m '22'

# Tune PID (Example: ku=15, tu=100s)
mosquitto_pub -t 'livingroom/in/pid_ku' -m '15'
mosquitto_pub -t 'livingroom/in/pid_tu' -m '100'

# Monitor output
mosquitto_sub -t 'livingroom/out/pid_out' -v
```
*Controls D6 (Analog PWM 0-255) and D7 (Binary PWM 0/255 with 10s dead-band) based on temperature error.*

---

## Web Interface

Open `http://<device_ip>/` in browser when `http=1`

Shows:
- Network info (IP, MAC, MQTT server)
- All GPIO status and MQTT topics
- All EEPROM settings with current values
- Full command reference

---

## Factory Reset (No Tools Needed)

1. Power on device
2. **Hold D3 (Flash) + D4 buttons together:**
   - **20 seconds** -> Reset to default WiFi (`hjhome`/`pass1234`)
   - **30 seconds** (from default) -> Enable AP mode (`ESP8266_hjhome`)
3. Device reboots automatically

---

## Sensor Simulation (Testing)

Inject fake sensor values without hardware:
```bash
# DHT temperature
mosquitto_pub -t 'livingroom/in/_tmp4' -m '25'

# DHT humidity
mosquitto_pub -t 'livingroom/in/_hum4' -m '60'

# DS18B20 temperature
mosquitto_pub -t 'livingroom/in/_temp5' -m '22'
```
*Published back on `/out/tmp4`, `/out/hum4`, `/out/temp5`*

## NTC Resistor (10k, A0)

10k NTC thermistor with pull-down resistor on A0:
```bash
# Enable NTC mode
mosquitto_pub -t 'livingroom/in/ntc' -m '1'

# Use with thermal protection (term=3)
mosquitto_pub -t 'livingroom/in/term' -m '3'

# Read temperature (when sensor=1)
mosquitto_sub -t 'livingroom/out/temp_ntc' -v
```
Range: ~15-35°C. Formula: `25.0 + (512.0 - an) * 0.085`

---

## Persistent Settings

All settings saved to EEPROM automatically. Survives power loss.

| Setting | EEPROM | Survives Reboot |
|---------|--------|-----------------|
| Name, SSID, Pass, MQTT | 80-99, 200-239, 50-53 | Yes |
| Modes (blinds, term, etc.) | 100-112 | Yes |
| PWM values (12,13) | 12, 13 | Only if `state=1` |
| Relay states (14,16) | 14, 16 | Only if `state=1` |
| Somfy position (11) | 11 | Yes |
| Shutter position (15) | 15 | Yes |
| Pulse counters (0-3) | 60-79 | Yes |

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Won't connect to WiFi | Check SSID/password, signal > -70dBm, try AP mode |
| MQTT not connecting | Verify broker IP, port 1883, check firewall |
| Relay not switching | Check `revers` setting, verify wiring, check `term`/`shutters` mode |
| PWM not working | Ensure `blinds=0` and `buttons=0` |
| Buttons not responding | Check mode: `buttons=1` for D6/D7, `sensor=0` for A0 |
| Device keeps resetting | Check free heap (`/out/memory`), power supply stability |
| AP mode not working | Marked TODO in firmware - use STA mode instead |

---

## Safety Notes

- **GPIO15 (D8)** must NOT be pulled HIGH at boot (prevents boot)
- **GPIO0 (D3)** and **GPIO2 (D4)** LOW at boot = flash mode
- **Analog A0** max 1V input - use voltage divider for 3.3V/5V sensors
- **DS18B20** needs 4.7k pullup on data line
- **DHT22** needs 10k pullup on data line
- Relay modules: verify opto-isolation, don't exceed ESP8266 current limits

---

## License

GPLv3 - Copyright (C) 2016 hjltu@ya.ru
Project: https://launchpad.net/hjhome