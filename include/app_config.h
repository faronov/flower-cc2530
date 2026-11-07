/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026, cc2530-zigbee contributors. See LICENSE.
 */
#ifndef FLOWER_APP_CONFIG_H
#define FLOWER_APP_CONFIG_H
/* cc2530-zigbee external application configuration (macros only; included by
 * the stack's cc2530_mmio.h for CC2530_APP, board BOARD_EXTERNAL). */

/* Wire identity of faronov/flower (DIYRuZ FlowerX); the Z2M converter and the
 * ZHA quirk match on these strings. */
#define BOARD_MANUFACTURER "modkam.ru"
#define BOARD_MODEL "DIYRuZ_FlowerX"
#define BOARD_MODEL_LENGTH 14u

/* MAC CSMA CCA threshold, raw CCACTRL0 (about dBm+73, SWRS081B). The first
 * flower v2 (E18-MS1) board idles at raw -9..-10 on channels 15-20
 * (debugger-measured, one board), so the stack default F8 held channel 15
 * busy and the beacon request never left; FE is -75 dBm, the
 * IEEE 802.15.4-2006 6.9.9 maximum (sensitivity -85 dBm plus 10 dB). */
#define BOARD_CCA_THRESHOLD_RAW 0xfeu

/* faronov/flower v2 (E18-MS1-PCB), HAL_BOARD_FLOWER pin map of the reference
 * firmware: LED P0.1 active high, button P2.0 active low (pull-up), sensor
 * rail P1.1 high=on, soil excitation P1.4 (Timer3 alt1 ch1), soil AIN4 P0.4,
 * light AIN7 P0.7, BME280 I2C P0.5 SCL / P0.6 SDA, DS18B20 1-Wire P1.3. */
#define BOARD_FLOWER_LED 0x02u
#define BOARD_FLOWER_BUTTON 0x01u
#define BOARD_FLOWER_RAIL 0x02u
#define BOARD_FLOWER_PWM 0x10u
#define BOARD_FLOWER_SOIL 0x10u
#define BOARD_FLOWER_LIGHT 0x80u
#define BOARD_FLOWER_SCL 0x20u
#define BOARD_FLOWER_SDA 0x40u
#define BOARD_FLOWER_ONEWIRE 0x08u

/* Timer 3 drives the soil excitation; these SFRs are owned by this
 * application only. */
#define CC2530_APP_REGISTER_LIST(X) \
    X(SOC_T3CTL, 0xcb) \
    X(SOC_T3CC0, 0xcd) \
    X(SOC_T3CCTL1, 0xce)

#endif
