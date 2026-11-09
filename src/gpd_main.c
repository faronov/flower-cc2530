/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026, cc2530-zigbee contributors. See LICENSE.
 */
/* FlowerX Green Power Device PROTOTYPE (branch flower-gp): replaces the
 * stack's join_smoke_main unit (cc2530-app.json). No Zigbee join, no
 * stack runtime: the radio only transmits GPDFs (include/gpd_frame.h,
 * NoSecurity, unidirectional) on GPD_CHANNEL. After power-on it sends a
 * Commissioning GPDF and a report every GPD_COMMISSION_PERIOD seconds for
 * GPD_COMMISSION_ROUNDS rounds (the sink accepts it only while its
 * commissioning window, ZHA permit join, is open), then one Multi-Cluster
 * Reporting GPDF every GPD_INTERVAL seconds. Each GPDF goes out
 * GPD_REPEAT times with the same MAC sequence number (the sink drops the
 * duplicates). The device sleeps in PM2 between rounds; any bounded wait
 * that expires restarts it through the watchdog. */
#include "join_smoke.h"
#include "zcl_sensor.h"
#include "clock.h"
#include "timebase.h"
#include "gpd_frame.h"

#ifndef GPD_CHANNEL
#define GPD_CHANNEL 15u
#endif
#ifndef GPD_INTERVAL
#define GPD_INTERVAL 60u
#endif
#ifndef GPD_COMMISSION_PERIOD
#define GPD_COMMISSION_PERIOD 10u
#endif
#ifndef GPD_COMMISSION_ROUNDS
#define GPD_COMMISSION_ROUNDS 12u
#endif
#ifndef GPD_REPEAT
#define GPD_REPEAT 3u
#endif
/* SWRS081B Table 2: F5 is +4.5 dBm. */
#ifndef GPD_TXPOWER
#define GPD_TXPOWER 0xf5u
#endif

/* SWRU191F 16.3 WDCTL, 4.10 SLEEPSTA, 11.2 STLOAD. */
#define SOC_WDCTL_ADDRESS 0xc9u
#define SOC_SLEEPSTA_ADDRESS 0x9du
#define SOC_STLOAD_ADDRESS 0xadu
#define WDCTL_RESET 0x0bu
/* Factory IEEE address in the information page, LSB first (SWRU191F 2.3.3). */
#define INFO_IEEE 0x780cu
/* SWRU191F 23.15 command strobes, RFIRQF1.TXDONE. */
#define RFST_ISTXON 0xe9u
#define RFST_ISFLUSHTX 0xeeu
#define RFST_ISRFOFF 0xefu
#define TXDONE 0x02u
#define SECOND 32768UL
#define WARMUP 1639UL      /* ZCL_FLOWER_WARMUP: 50 ms */
#define TX_WAIT 164UL      /* 5 ms per GPDF (about 1.6 ms on air) */
#define GAP 66UL           /* 2 ms between repetitions */
#define LED_ON 655UL       /* 20 ms commissioning flash */
#define SPIN_WAIT 4096UL   /* 125 ms for clock and Sleep Timer handshakes */
#define SLEEP_MIN 8UL

/* Read by a debugger: progress and the last failure. */
typedef struct {
    uint8_t stage, failure, rounds, seq;
    uint16_t sent, lost;
    uint8_t src[4];
} gpd_status_t;
MCU_XDATA gpd_status_t gpd_status;

static MCU_XDATA uint8_t frame[GPD_REPORT_LENGTH], ieee[8];
static MCU_XDATA gpd_values_t values;
static MCU_XDATA clock_diagnostics_t clock_diagnostics;
static MCU_XDATA uint32_t deadline, mark;
#define S gpd_status
#define NOW() timebase_read_awake_ticks24()
#define SINCE(start) ((NOW()-(start))&TIMEBASE_TICKS_MASK)

/* Kept for the image audits, which locate both by name. */
__attribute__((naked, noinline)) void join_smoke_wait(void)
{
    __asm__ volatile("nop\n\tret");
}
__attribute__((naked, noinline)) void join_smoke_fault(void)
{
    __asm__ volatile("nop\n\tsjmp join_smoke_fault");
}

__attribute__((noinline)) static void restart(uint8_t failure)
{
    S.failure = failure;
    MMIO_CLEAR(SOC_IEN0, 0x80);
    MMIO_WRITE(SOC_RFST, RFST_ISRFOFF);
    MMIO_WRITE(SOC_WDCTL, WDCTL_RESET);
    for (;;) {}
}

static void pause(uint32_t ticks)
{
    mark = NOW();
    while (SINCE(mark) < ticks) {}
}

/* SWRU191F 23.15: TX-relevant part of the stack's radio profile
 * (radio_autoack.c), AUTOCRC without AUTOACK, then channel and power. */
static void radio_setup(void)
{
    MMIO_WRITE(SOC_RFST, RFST_ISRFOFF);
    MMIO_XWRITE(0x6189u, 0x40);   /* FRMCTRL0 */
    MMIO_XWRITE(0x61b2u, 0x15);   /* AGCCTRL1 */
    MMIO_XWRITE(0x61fau, 0x09);   /* TXFILTCFG */
    MMIO_XWRITE(0x61aeu, 0x00);   /* FSCAL1 */
    MMIO_XWRITE(0x6191u, 0x69);   /* TXCTRL */
    MMIO_XWRITE(0x618fu, 11u + 5u*(GPD_CHANNEL-11u));   /* FREQCTRL */
    MMIO_XWRITE(0x6190u, GPD_TXPOWER);
}

/* One GPDF GPD_REPEAT times: TX FIFO length (with FCS) and body, ISTXON
 * (calibrates, then sends without CCA), TXDONE, radio off. */
static void send(uint8_t length)
{
    uint8_t r, i;
    for (r = 0; r < GPD_REPEAT; r++) {
        if (r) pause(GAP);
        MMIO_WRITE(SOC_RFST, RFST_ISFLUSHTX);
        MMIO_WRITE(SOC_RFD, length + 2u);
        for (i = 0; i < length; i++) MMIO_WRITE(SOC_RFD, frame[i]);
        MMIO_WRITE(SOC_RFIRQF1, (uint8_t)~TXDONE);
        MMIO_WRITE(SOC_RFST, RFST_ISTXON);
        mark = NOW();
        while (!(MMIO_READ(SOC_RFIRQF1) & TXDONE))
            if (SINCE(mark) >= TX_WAIT) break;
        if (MMIO_READ(SOC_RFIRQF1) & TXDONE) S.sent++;
        else S.lost++;
        MMIO_WRITE(SOC_RFIRQF1, (uint8_t)~TXDONE);
        MMIO_WRITE(SOC_RFST, RFST_ISRFOFF);
    }
    S.seq++;
}

static void sample(void)
{
    S.stage = 2;
    zcl_flower_power(1);
    pause(WARMUP);
    zcl_flower_measure();
    zcl_flower_power(0);
    values.temperature = (int16_t)ZCL_SENSOR_STATE.values[ZCL_FLOWER_TEMPERATURE];
    values.soil = ZCL_SENSOR_STATE.values[ZCL_FLOWER_SOIL];
    values.illuminance = ZCL_SENSOR_STATE.values[ZCL_FLOWER_ILLUMINANCE];
    values.voltage = (uint8_t)ZCL_SENSOR_STATE.values[ZCL_FLOWER_VOLTAGE];
    values.percent = (uint8_t)ZCL_SENSOR_STATE.values[ZCL_FLOWER_PERCENT];
}

/* STIF clear; IEN0 = EA|STIE, then PCON.IDLE whose successor must not start
 * on a 4-byte boundary (SWRU191F 4.2); clr EA runs before any ISR (the
 * stack's join_smoke_sleep_isr owns the vector). Common code: the alignment
 * padding stays out of the size-packed banks. */
__attribute__((noinline)) MCU_COMMON_CODE static void enter_pm2(void)
{
    __asm__ volatile("clr 0xc7\n\t.p2align 2\n\tmov 0xa8,#0xa0\n\tmov 0x87,#1\n\tclr 0xaf" ::: "memory");
}

#define SPIN(condition, code) do { mark = NOW(); \
    while (condition) if (SINCE(mark) >= SPIN_WAIT) restart(code); } while (0)

/* PM2 until the Sleep Timer reaches wake (as join_smoke_sleep: compare
 * ST2, ST1, ST0 after STLOAD; XOSC back, then a fresh 32-kHz edge). A wake
 * already due or too close is a busy wait instead. */
static void sleep_until(uint32_t wake)
{
    uint32_t left = (wake-NOW())&TIMEBASE_TICKS_MASK;
    S.stage = 4;
    if (left >= TIMEBASE_TICKS_MASK/2u) return;
    if (left > SLEEP_MIN*2u) {
        SPIN(!(MMIO_READ(SOC_STLOAD) & 1u), 5);
        MMIO_WRITE(SOC_ST2, (uint8_t)(wake >> 16));
        MMIO_WRITE(SOC_ST1, (uint8_t)(wake >> 8));
        MMIO_WRITE(SOC_ST0, (uint8_t)wake);
        if (((wake-NOW())&TIMEBASE_TICKS_MASK) >= SLEEP_MIN) {
            MMIO_WRITE(SOC_SLEEPCMD, 0x06);   /* PM2, unused 16-MHz RCOSC off */
            enter_pm2();
            MMIO_WRITE(SOC_IEN0, 0);
            MMIO_WRITE(SOC_SLEEPCMD, 0x04);
            S.stage = 5;
            SPIN(MMIO_READ(SOC_CLKCONSTA) != MMIO_READ(SOC_CLKCONCMD), 6);
            SPIN(MMIO_READ(SOC_SLEEPSTA) & 1u, 7);
            SPIN(!(MMIO_READ(SOC_SLEEPSTA) & 1u), 8);
        }
    }
    while (((NOW()-wake)&TIMEBASE_TICKS_MASK) >= TIMEBASE_TICKS_MASK/2u) {}
}

void main(void)
{
    uint8_t i;
    uint16_t period;
    /* LLVM cc2530_startup does not call the SDCC external-startup hook. */
    CC2530_EXTERNAL_STARTUP();
    S.stage = 1;
    if (clock_select_init(CLOCK_XOSC32, 1024, 4096, &clock_diagnostics) != CLOCK_OK) restart(1);
    zcl_sensor_boot();
    for (i = 0; i < 8u; i++) ieee[i] = MMIO_XREAD(INFO_IEEE + i);
    gpd_source(S.src, ieee);
    S.seq = (uint8_t)(ieee[0] ^ MMIO_READ(SOC_ST0));
    deadline = NOW();
    /* Never taken (stage is never FF): it keeps the join_smoke stack linked,
     * which the image tools (frame, bank, thunk and flash_exec audits)
     * analyse as a whole. The GPD itself uses only the clock, timebase and
     * the application's measurement. */
    if (*(volatile uint8_t MCU_XDATA *)&S.stage == 0xffu) {
        join_smoke_initialize();
        for (;;) join_smoke_poll();
    }
    for (;;) {
        sample();
        S.stage = 3;
        radio_setup();
        if (S.rounds < GPD_COMMISSION_ROUNDS) {
            S.rounds++;
            MMIO_SET(SOC_P0, BOARD_FLOWER_LED);
            send(gpd_commissioning(frame, S.seq, S.src));
            pause(GAP);
            send(gpd_report(frame, S.seq, S.src, &values));
            pause(LED_ON);
            MMIO_CLEAR(SOC_P0, BOARD_FLOWER_LED);
            period = GPD_COMMISSION_PERIOD;
        } else {
            send(gpd_report(frame, S.seq, S.src, &values));
            period = GPD_INTERVAL;
        }
        deadline = (deadline + (uint32_t)period*SECOND) & TIMEBASE_TICKS_MASK;
        sleep_until(deadline);
    }
}
