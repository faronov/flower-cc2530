# FlowerX firmware


This [external application](https://github.com/faronov/cc2530-zigbee/blob/main/docs/EXTERNAL_APP.md) of cc2530-zigbee builds the
LEAN PM2 sleepy end device for the faronov/flower v2 board (E18-MS1-PCB). It
replaces the stack's ESL29 application with `src/zcl_flower.c` and keeps the
DIYRuZ_FlowerX attributes that the flower Z2M converter and ZHA quirk use
(manufacturer `modkam.ru`, model `DIYRuZ_FlowerX`). It has joined ZHA on
hardware, but it is not yet fully qualified (see below).

- Pins ([`include/app_config.h`](../include/app_config.h)): sensor rail P1.1, soil
  excitation P1.4 (Timer 3 alternative 1, channel 1), soil ADC AIN4, light ADC
  AIN7, button P2.0, LED P0.1. BME280 (P0.5/P0.6) and DS18B20 (P1.3) are
  stage 2 and 3: their clusters are only described when detected.
- Cycle (default 300 s, Diagnostics `FF09`, 30–3600 s): rail and excitation
  on, 50 ms warm-up (the device stays awake; PM2 stops Timer 3), then battery
  VDD/3 ×5, die temperature ×8 (one discarded), light ×5 and soil ×3, rail
  off. `zcl_sensor_quiet()` bounds the
  sleep span (sleep reason 5 while the cycle is awake).
- Conversions follow flower: mV = raw·0.443, Alkaline 2S percent curve,
  illuminance 10000·log10(raw)+1 (integer log, within one unit), soil from the
  battery-compensated air/water calibration. Unlike flower, a soil reading
  more than one calibrated span outside air..water is invalid (`FFFF`).
- Chip temperature: EP1 Temperature (0x0402) is described when a BME280 is
  present or when no external sensor is detected (a DS18B20 alone reports on
  EP2 instead). Without a BME280 it carries the CC2530 die sensor: 14-bit
  reading, supply-compensated by 4 counts per 100 mV around 3.0 V, 18 counts
  per °C, referenced to 25 °C. The reference is the info-page word at `0x781E`
  when its 12-bit value is in 1380..1580, otherwise the datasheet nominal 1480.
  That factory word is undocumented by TI (experimental, see [cc2530-zigbee#122](https://github.com/faronov/cc2530-zigbee/pull/122)); the
  read-only Diagnostics attributes `FF12` (averaged die reading) and `FF13`
  (raw factory word) exist to check it against a reference thermometer.
- Simple descriptors come from `CC2530_ZDO_DESCRIPTORS`. Reports are grouped
  per bound cluster; the Basic model is reported once after joining.
- The interval is not persisted yet (NV is stage 4).
- CCA threshold: `BOARD_CCA_THRESHOLD_RAW` is `0xFE` (≈ −75 dBm with the
  datasheet RSSI offset of 73, the IEEE 802.15.4-2006 §6.9.9 maximum) instead
  of the common `0xF8`. On the first flower v2 board the idle RSSI on
  channel 15 was raw −9…−10 (≈ −82 dBm). Energy-only CCA with hysteresis 2
  then reported "clear" in only 29 % of samples at `0xF8`, but 95 % at `0xFE`.
  So every active-scan Beacon Request on channel 15 failed channel access after
  `MAC_SCAN_BUSY_RETRIES`, the channel stayed unscanned and steering ended
  with `BDB_JOIN_NO_PARENT`. This is a single-board, single-location
  measurement. The stack default stays `0xF8`.
- Hardware join status: with the threshold fixed, all channels are scanned and
  the board receives several router beacons from the target PAN on channel 15
  (raw RSSI −6…+14). It did not receive the coordinator's beacon, so the
  direct-coordinator policy ended with `BDB_JOIN_NO_PARENT`. The variant now
  enables [router-parent join](https://github.com/faronov/cc2530-zigbee/blob/main/docs/ED_JOIN.md#router-parent-initial-join-cc2530_router_join).
  With it, the board joined ZHA through router 0x4a76 and reached READY. The
  `zha_quirk_diyruz_flowerx` quirk matched, and battery, soil, illuminance,
  die temperature and TX power entities report. Sleep current, the BME280 and
  the DS18B20 are not qualified yet.

Host coverage: `make test` (a plain and an ASan/UBSan build of
`tests/test_zcl_flower.c`) models the ADC and Timer 3. It covers conversions, descriptors, the cycle across the 32-bit wrap,
reads, writes, reporting configuration, Identify, bind/unbind and grouped
reports.
