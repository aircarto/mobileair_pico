# MobileAir Pico 2W — Pinout

```
                        ┌─────────┐
                        │   USB   │
                        └────┬────┘
```

### Legende

| Couleur | Peripherique |
|:--------|:-------------|
| 🟢 | NextPM (capteur PM, UART0) |
| 🔵 | Modem SARA-R500S (UART1) |
| 🟡 | GPS NMEA (PIO UART) |
| 🟠 | Sonde gaz 1 (PIO UART) |
| 🟤 | Sonde gaz 2 (PIO UART) |
| 🟣 | Carte SD (SPI0) |
| 🔴 | BME280 + DS3231 (I2C0) |
| 💡 | LED RGB programmable (WS2812 / NeoPixel) |
| ⚫ | GND |
| ⬜ | Libre |

### Brochage

| Pin | GPIO | Interface | Attribution | | | Attribution | Interface | GPIO | Pin |
|----:|:-----|:----------|:------------|:---:|:---:|:------------|:----------|:-----|:----|
| 1 | **GP0** | UART0 TX | 🟢 NextPM TX | | | VBUS | Alim | — | 40 |
| 2 | **GP1** | UART0 RX | 🟢 NextPM RX | | | VSYS | Alim | — | 39 |
| 3 | GND | — | ⚫ | | | ⚫ | — | GND | 38 |
| 4 | **GP2** | PIO UART TX | 🟡 GPS TX | | | 3V3_EN | Alim | — | 37 |
| 5 | **GP3** | PIO UART RX | 🟡 GPS RX | | | 3V3 OUT | Alim | — | 36 |
| 6 | **GP4** | UART1 TX | 🔵 SARA TX | | | ADC_VREF | Ref | — | 35 |
| 7 | **GP5** | UART1 RX | 🔵 SARA RX | | | **GP28** | ADC2 | ⬜ libre | 34 |
| 8 | GND | — | ⚫ | | | ⚫ | — | GND | 33 |
| 9 | **GP6** | PIO UART TX | 🟠 Sonde gaz 1 TX | | | **GP27** | ADC1 | ⬜ libre | 32 |
| 10 | **GP7** | PIO UART RX | 🟠 Sonde gaz 1 RX | | | **GP26** | ADC0 | ⬜ libre | 31 |
| 11 | **GP8** | PIO UART TX | 🟤 Sonde gaz 2 TX | | | RUN | Reset | — | 30 |
| 12 | **GP9** | PIO UART RX | 🟤 Sonde gaz 2 RX | | | **GP22** | PIO data | 💡 LED RGB | 29 |
| 13 | GND | — | ⚫ | | | ⚫ | — | GND | 28 |
| 14 | **GP10** | — | ⬜ libre | | | **GP21** | I2C0 SCL | 🔴 BME280 + DS3231 | 27 |
| 15 | **GP11** | — | ⬜ libre | | | **GP20** | I2C0 SDA | 🔴 BME280 + DS3231 | 26 |
| 16 | **GP12** | — | ⬜ libre | | | **GP19** | SPI0 MOSI | 🟣 SD MOSI | 25 |
| 17 | **GP13** | — | ⬜ libre | | | **GP18** | SPI0 SCK | 🟣 SD SCK | 24 |
| 18 | GND | — | ⚫ | | | ⚫ | — | GND | 23 |
| 19 | **GP14** | — | ⬜ libre | | | **GP17** | SPI0 CS | 🟣 SD CS | 22 |
| 20 | **GP15** | — | ⬜ libre | | | **GP16** | SPI0 MISO | 🟣 SD MISO | 21 |

## Synthese des peripheriques

| | Peripherique | Interface | GPIO | Config | Status |
|:--|:-------------|:----------|:-----|:-------|:-------|
| 🟢 | NextPM (PM1/2.5/10) | UART0 | GP0 TX, GP1 RX | 115200, 8E1 Modbus RTU | Implemente |
| 🔵 | Modem SARA-R500S | UART1 | GP4 TX, GP5 RX | 115200, 8N1 AT | Implemente |
| 🟡 | GPS (NMEA) | PIO UART | GP2 TX, GP3 RX | 9600, 8N1 | Prevu |
| 🟠 | Sonde gaz 1 (UART) | PIO UART | GP6 TX, GP7 RX | A definir | Prevu |
| 🟤 | Sonde gaz 2 (UART) | PIO UART | GP8 TX, GP9 RX | A definir | Prevu |
| 🟣 | Carte SD | SPI0 | GP16-GP19 | SPI 10 MHz | Implemente |
| 🔴 | BME280 (temp/hum/pression) | I2C0 | GP20 SDA, GP21 SCL | 100-400 kHz, addr 0x76/0x77 | Prevu |
| 🔴 | RTC DS3231 | I2C0 | GP20 SDA, GP21 SCL | 100 kHz, addr 0x68 | Implemente |
| 💡 | LED RGB (WS2812) | PIO data | GP22 | 800 kHz, 1 wire | Prevu |

## Ressources restantes

| Ressource | Total | Utilise | Libre |
|:----------|------:|--------:|------:|
| UART materiel | 2 | 2 | 0 |
| PIO | 3 blocs (12 SM) | 4 SM | 8 SM |
| SPI | 2 | 1 | 1 |
| I2C | 2 | 1 | 1 |
| ADC | 3 (GP26-28) | 0 | 3 |
| GPIO libre | — | — | GP10-15, GP26-28 |

## Notes

- Le BME280 et le DS3231 partagent le meme bus I2C0 (adresses differentes : 0x76/0x77 et 0x68)
- La LED WS2812 (NeoPixel) utilise 1 seul fil de donnees, pilotee par PIO (protocole 800 kHz)
- Les 3 entrees ADC (GP26-28) sont libres pour d'eventuels capteurs analogiques
- GP10-GP15 restent libres pour des extensions futures
- Le WiFi (CYW43) est gere en interne par le Pico 2W (pas de GPIO expose)
