/*	$OpenBSD: zaurus_apm.c,v 1.15 2010/04/21 03:11:30 deraadt Exp $	*/

/*
 * Copyright (c) 2005 Uwe Stuehler <uwe@bsdx.de>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/timeout.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/sysctl.h>

#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0var.h>
#include <arm/xscale/pxa2x0_apm.h>
#include <arm/xscale/pxa2x0_gpio.h>

#include <zaurus/dev/zaurus_scoopvar.h>
#include <zaurus/dev/zaurus_sspvar.h>
void zssp_init(void);	/* XXX */

#include <zaurus/dev/zaurus_apm.h>

#include <dev/wscons/wsdisplayvar.h>

#include "wsdisplay.h"

#if defined(APMDEBUG)
#define DPRINTF(x)	printf x
#else
#define	DPRINTF(x)	/**/
#endif

struct zapm_softc {
	struct pxa2x0_apm_softc sc;
	struct timeout sc_poll;
	struct timeval sc_lastbattchk;
	int	sc_suspended;
	int	sc_ac_on;
	int	sc_charging;
	int	sc_discharging;
	int	sc_batt_full;
	int	sc_batt_volt;
	u_int	sc_event;
};

int	apm_match(struct device *, void *, void *);
void	apm_attach(struct device *, struct device *, void *);

struct cfattach apm_pxaip_ca = {
        sizeof (struct zapm_softc), apm_match, apm_attach
};
extern struct cfdriver apm_cd;

/* MAX1111 command word */
#define MAXCTRL_PD0		(1<<0)
#define MAXCTRL_PD1		(1<<1)
#define MAXCTRL_SGL		(1<<2)
#define MAXCTRL_UNI		(1<<3)
#define MAXCTRL_SEL_SHIFT	4
#define MAXCTRL_STR		(1<<7)

/* MAX1111 ADC channels */
#define	BATT_THM		2
#define	BATT_AD			4
#define JK_VAD			6

/* battery-related GPIO pins */
#define GPIO_AC_IN_C3000	115	/* 0=AC connected */
#define GPIO_CHRG_CO_C3000	101	/* 1=battery full */
#define GPIO_BATT_COVER_C3000	90	/* 0=unlocked */

/*
 * Battery-specific information
 */

struct battery_threshold {
	int	bt_volt;
	int	bt_life;
	int	bt_state;
};

struct battery_info {
	int	bi_minutes;		/* 100% life time */
	const	struct battery_threshold *bi_thres;
};

const struct battery_threshold zaurus_battery_life_c3000[] = {
#if 0
	{224,	125,	APM_BATT_HIGH}, /* XXX unverified */
#endif
	{194,	100,	APM_BATT_HIGH},
	{188,	75,	APM_BATT_HIGH},
	{184,	50,	APM_BATT_HIGH},
	{180,	25,	APM_BATT_LOW},
	{178,	5,	APM_BATT_LOW},
	{0,	0,	APM_BATT_CRITICAL},
};

const struct battery_info zaurus_battery_c3000 = {
	180 /* minutes; pessimistic estimate */,
	zaurus_battery_life_c3000
};

const struct battery_info *zaurus_main_battery = &zaurus_battery_c3000;

/* Restart charging this many times before accepting BATT_FULL. */
#define MIN_BATT_FULL 2

/* Discharge 100 ms before reading the voltage if AC is connected. */
#define DISCHARGE_TIMEOUT (hz / 10)

/* Check battery voltage and "kick charging" every minute. */
const	struct timeval zapm_battchkrate = { 60, 0 };

/* Prototypes */

#if 0
void	zapm_shutdown(void *);
#endif
int	zapm_acintr(void *);
int	zapm_bcintr(void *);
int	zapm_ac_on(void);
int	max1111_adc_value(int);
int	max1111_adc_value_avg(int, int);
#if 0
int	zapm_jkvad_voltage(void);
int	zapm_batt_temp(void);
#endif
int	zapm_batt_volt(void);
int	zapm_batt_state(int);
int	zapm_batt_life(int);
int	zapm_batt_minutes(int);
void	zapm_enable_charging(struct zapm_softc *, int);
int	zapm_charge_complete(struct zapm_softc *);
void	zapm_poll(void *);
int	zapm_get_event(struct pxa2x0_apm_softc *, u_int *);
void	zapm_power_info(struct pxa2x0_apm_softc *, struct apm_power_info *);
void	zapm_suspend(struct pxa2x0_apm_softc *);
int	zapm_resume(struct pxa2x0_apm_softc *);
void	pxa2x0_setperf(int);
int	pxa2x0_cpuspeed(int *);


int
apm_match(struct device *parent, void *match, void *aux)
{
	return (1);
}

void
apm_attach(struct device *parent, struct device *self, void *aux)
{
	struct zapm_softc *sc = (struct zapm_softc *)self;

	pxa2x0_gpio_set_function(GPIO_AC_IN_C3000, GPIO_IN);
	pxa2x0_gpio_set_function(GPIO_CHRG_CO_C3000, GPIO_IN);
	pxa2x0_gpio_set_function(GPIO_BATT_COVER_C3000, GPIO_IN);

	(void)pxa2x0_gpio_intr_establish(GPIO_AC_IN_C3000,
	    IST_EDGE_BOTH, IPL_BIO, zapm_acintr, sc, "apm_ac");
	(void)pxa2x0_gpio_intr_establish(GPIO_BATT_COVER_C3000,
	    IST_EDGE_BOTH, IPL_BIO, zapm_bcintr, sc, "apm_bc");

	sc->sc_event = APM_NOEVENT;
	sc->sc.sc_get_event = zapm_get_event;
	sc->sc.sc_power_info = zapm_power_info;
	sc->sc.sc_suspend = zapm_suspend;
	sc->sc.sc_resume = zapm_resume;

	timeout_set(&sc->sc_poll, &zapm_poll, sc);

	/* Get initial battery voltage. */
	zapm_enable_charging(sc, 0);
	if (zapm_ac_on()) {
		/* C3000: discharge 100 ms when AC is on. */
		scoop_discharge_battery(1);
		delay(100000);
	}
	sc->sc_batt_volt = zapm_batt_volt();
	scoop_discharge_battery(0);

	pxa2x0_apm_attach_sub(&sc->sc);

#if 0
	(void)shutdownhook_establish(zapm_shutdown, NULL);
#endif

	cpu_setperf = pxa2x0_setperf;
	cpu_cpuspeed = pxa2x0_cpuspeed;
}

#if 0
void
zapm_shutdown(void *v)
{
	struct zapm_softc *sc = v;

	zapm_enable_charging(sc, 0);
}
#endif

int
zapm_acintr(void *v)
{
	zapm_poll(v);
	return (1);
}

int
zapm_bcintr(void *v)
{
	zapm_poll(v);
	return (1);
}

int
zapm_ac_on(void)
{
	return (!pxa2x0_gpio_get_bit(GPIO_AC_IN_C3000));
}

int
max1111_adc_value(int chan)
{

	return ((int)zssp_ic_send(ZSSP_IC_MAX1111, MAXCTRL_PD0 |
	    MAXCTRL_PD1 | MAXCTRL_SGL | MAXCTRL_UNI |
	    (chan << MAXCTRL_SEL_SHIFT) | MAXCTRL_STR));
}

/* XXX simplify */
int
max1111_adc_value_avg(int chan, int pause)
{
	int val[5];
	int i, j, k, x;
	int sum = 0;

	for (i = 0; i < 5; i++) {
		val[i] = max1111_adc_value(chan);
		if (i != 4)
			delay(pause * 1000);
	}

	x = val[0];
	j = 0;
	for (i = 1; i < 5; i++) {
		if (x < val[i]) {
			x = val[i];
			j = i;
		}
	}

	x = val[4];
	k = 4;
	for (i = 3; i >= 0; i--) {
		if (x > val[i]) {
			x = val[i];
			k = i;
		}
	}

	for (i = 0; i < 5; i++) {
		if (i == j || i == k)
			continue;
		sum += val[i];
	}

	return (sum / 3);
}

#if 0
/*
 * Return the voltage available for charging.  This will be zero,
 * unless A/C power is connected.
 */
int
zapm_jkvad_voltage(void)
{

	return (max1111_adc_value_avg(JK_VAD, 10));
}

int
zapm_batt_temp(void)
{
	int temp;

	scoop_battery_temp_adc(1);
	delay(10000);
	temp = max1111_adc_value_avg(BATT_THM, 1);
	scoop_battery_temp_adc(0);

	return (temp);
}
#endif

int
zapm_batt_volt(void)
{

	return (max1111_adc_value_avg(BATT_AD, 10));
}

int
zapm_batt_state(int volt)
{
	const struct battery_threshold *bthr;
	int i;

	bthr = zaurus_main_battery->bi_thres;

	for (i = 0; bthr[i].bt_volt > 0; i++)
		if (bthr[i].bt_volt <= volt)
			break;

	return (bthr[i].bt_state);
}

int
zapm_batt_life(int volt)
{
	const struct battery_threshold *bthr;
	int i;

	bthr = zaurus_main_battery->bi_thres;

	for (i = 0; bthr[i].bt_volt > 0; i++)
		if (bthr[i].bt_volt <= volt)
			break;

	if (i == 0)
		return (bthr[i].bt_life);

	return (bthr[i].bt_life +
	    ((volt - bthr[i].bt_volt) * 100) /
	    (bthr[i-1].bt_volt - bthr[i].bt_volt) *
	    (bthr[i-1].bt_life - bthr[i].bt_life) / 100);
}

int
zapm_batt_minutes(int life)
{

	return (zaurus_main_battery->bi_minutes * life / 100);
}

void
zapm_enable_charging(struct zapm_softc *sc, int enable)
{

	scoop_discharge_battery(0);
	scoop_charge_battery(enable, 0);
	scoop_led_set(SCOOP_LED_ORANGE, enable);
}

/*
 * Return non-zero if the charge complete signal indicates that the
 * battery is fully charged.  Restart charging to clear this signal.
 */
int
zapm_charge_complete(struct zapm_softc *sc)
{

	if (sc->sc_charging && sc->sc_batt_full < MIN_BATT_FULL) {
		if (pxa2x0_gpio_get_bit(GPIO_CHRG_CO_C3000) != 0) {
			if (++sc->sc_batt_full < MIN_BATT_FULL) {
				DPRINTF(("battery almost full\n"));
				zapm_enable_charging(sc, 0);
				delay(15000);
				zapm_enable_charging(sc, 1);
			}
		} else if (sc->sc_batt_full > 0) {
			/* false alarm */
			sc->sc_batt_full = 0;
			zapm_enable_charging(sc, 0);
			delay(15000);
			zapm_enable_charging(sc, 1);
		}
	}

	return (sc->sc_batt_full >= MIN_BATT_FULL);
}

/*
 * Poll power-management related GPIO inputs, update battery life
 * in softc, and/or control battery charging.
 */
void
zapm_poll(void *v)
{
	struct zapm_softc *sc = v;
	int ac_on;
	int bc_lock;
	int charging;
	int volt;
	int s;

	s = splhigh();

	/* Check positition of battery compartment lock switch. */
	bc_lock = pxa2x0_gpio_get_bit(GPIO_BATT_COVER_C3000) ? 1 : 0;

	/* Stop discharging. */
	if (sc->sc_discharging) {
		sc->sc_discharging = 0;
		volt = zapm_batt_volt();
		ac_on = zapm_ac_on();
		charging = 0;
		DPRINTF(("zapm_poll: discharge off volt %d\n", volt));
	} else {
		ac_on = zapm_ac_on();
		charging = sc->sc_charging;
		volt = sc->sc_batt_volt;
	}

	/* Start or stop charging as necessary. */
	if (ac_on && bc_lock) {
		if (charging) {
			if (zapm_charge_complete(sc)) {
				DPRINTF(("zapm_poll: batt full\n"));
				charging = 0;
				zapm_enable_charging(sc, 0);
			}
		} else if (!zapm_charge_complete(sc)) {
			charging = 1;
			volt = zapm_batt_volt();
			zapm_enable_charging(sc, 1);
			DPRINTF(("zapm_poll: start charging volt %d\n", volt));
		}
	} else {
		if (charging) {
			charging = 0;
			zapm_enable_charging(sc, 0);
			timerclear(&sc->sc_lastbattchk);
			DPRINTF(("zapm_poll: stop charging\n"));
		}
		sc->sc_batt_full = 0;
	}

	/*
	 * Restart charging once in a while.  Discharge a few milliseconds
	 * before updating the voltage in our softc if A/C is connected.
	 */
	if (bc_lock && ratecheck(&sc->sc_lastbattchk, &zapm_battchkrate)) {
		if (sc->sc_suspended) {
			DPRINTF(("zapm_poll: suspended %lu %lu\n",
			    sc->sc_lastbattchk.tv_sec,
			    pxa2x0_rtc_getsecs()));
			if (charging) {
				zapm_enable_charging(sc, 0);
				delay(15000);
				zapm_enable_charging(sc, 1);
				pxa2x0_rtc_setalarm(pxa2x0_rtc_getsecs() +
				    zapm_battchkrate.tv_sec + 1);
			}
		} else if (ac_on && sc->sc_batt_full == 0) {
			DPRINTF(("zapm_poll: discharge on\n"));
			if (charging)
				zapm_enable_charging(sc, 0);
			sc->sc_discharging = 1;
			scoop_discharge_battery(1);
			timeout_add(&sc->sc_poll, DISCHARGE_TIMEOUT);
		} else if (!ac_on) {
			volt = zapm_batt_volt();
			DPRINTF(("zapm_poll: volt %d\n", volt));
		}
	}

	/* Update the cached power state in our softc. */
	if (ac_on != sc->sc_ac_on || charging != sc->sc_charging ||
	    volt != sc->sc_batt_volt) {
		sc->sc_ac_on = ac_on;
		sc->sc_charging = charging;
		sc->sc_batt_volt = volt;
		if (sc->sc_event == APM_NOEVENT)
			sc->sc_event = APM_POWER_CHANGE;
	}

	/* Detect battery low conditions. */
	if (!ac_on) {
		if (zapm_batt_life(volt) < 5)
			sc->sc_event = APM_BATTERY_LOW;
		if (zapm_batt_state(volt) == APM_BATT_CRITICAL)
			sc->sc_event = APM_CRIT_SUSPEND_REQ;
	}

#ifdef APMDEBUG
	if (sc->sc_event != APM_NOEVENT)
		DPRINTF(("zapm_poll: power event %d\n", sc->sc_event));
#endif
	splx(s);
}

/*
 * apm_thread() calls this routine approximately once per second.
 */
int
zapm_get_event(struct pxa2x0_apm_softc *pxa_sc, u_int *typep)
{
	struct zapm_softc *sc = (struct zapm_softc *)pxa_sc;
	int s;

	s = splsoftclock();

	/* Don't interfere with discharging. */
	if (sc->sc_discharging)
		*typep = sc->sc_event;
	else if (sc->sc_event == APM_NOEVENT) {
		zapm_poll(sc);
		*typep = sc->sc_event;
	}
	sc->sc_event = APM_NOEVENT;

	splx(s);
	return (*typep == APM_NOEVENT);
}

/*
 * Return power status to the generic APM driver.
 */
void
zapm_power_info(struct pxa2x0_apm_softc *pxa_sc, struct apm_power_info *power)
{
	struct zapm_softc *sc = (struct zapm_softc *)pxa_sc;
	int s;
	int ac_on;
	int volt;
	int charging;

	s = splsoftclock();
	ac_on = sc->sc_ac_on;
	volt = sc->sc_batt_volt;
	charging = sc->sc_charging;
	splx(s);

	power->ac_state = ac_on ? APM_AC_ON : APM_AC_OFF;
	if (charging)
		power->battery_state = APM_BATT_CHARGING;
	else
		power->battery_state = zapm_batt_state(volt);

	power->battery_life = zapm_batt_life(volt);
	power->minutes_left = zapm_batt_minutes(power->battery_life);
}

/*
 * Called before suspending when all powerhooks are done.
 */
void
zapm_suspend(struct pxa2x0_apm_softc *pxa_sc)
{
	struct zapm_softc *sc = (struct zapm_softc *)pxa_sc;

	/* Poll in suspended mode and forget the discharge timeout. */
	sc->sc_suspended = 1;
	timeout_del(&sc->sc_poll);

	/* Make sure charging is enabled and RTC alarm is set. */
	timerclear(&sc->sc_lastbattchk);

	zapm_poll(sc);

#if 0
	pxa2x0_rtc_setalarm(pxa2x0_rtc_getsecs() + 5);
#endif
	pxa2x0_wakeup_config(PXA2X0_WAKEUP_ALL, 1);
}

/*
 * Called after wake-up from suspend with interrupts still disabled,
 * before any powerhooks are done.
 */
int
zapm_resume(struct pxa2x0_apm_softc *pxa_sc)
{
	struct zapm_softc *sc = (struct zapm_softc *)pxa_sc;
	int	a, b;
	u_int	wsrc;
	int	wakeup = 0;

	/* C3000 */
	a = pxa2x0_gpio_get_bit(97) ? 1 : 0;
	b = pxa2x0_gpio_get_bit(96) ? 2 : 0;

	wsrc = pxa2x0_wakeup_status();

	/* Resume only if the lid is not closed. */
	if ((a | b) != 3 && (wsrc & PXA2X0_WAKEUP_POWERON) != 0) {
		int timeout = 100; /* 10 ms */
		/* C3000 */
		while (timeout-- > 0 && pxa2x0_gpio_get_bit(95) != 0) {
			if (timeout == 0) {
				wakeup = 1;
				break;
			}
			delay(100);
		}
	}

	/* Initialize the SSP unit before using the MAX1111 again. */
	zssp_init();

	zapm_poll(sc);

	if (wakeup) {
		/* Resume normal polling. */
		sc->sc_suspended = 0;

		pxa2x0_rtc_setalarm(0);
	} else {
#if 0
		DPRINTF(("zapm_resume: suspended %lu %lu\n",
		    sc->sc_lastbattchk.tv_sec, pxa2x0_rtc_getsecs()));
		pxa2x0_rtc_setalarm(pxa2x0_rtc_getsecs() + 5);
#endif
	}

	return (wakeup);
}

void
zapm_poweroff(void)
{
	struct pxa2x0_apm_softc *sc;

	KASSERT(apm_cd.cd_ndevs > 0 && apm_cd.cd_devs[0] != NULL);
	sc = apm_cd.cd_devs[0];

#if NWSDISPLAY > 0
	wsdisplay_suspend();
#endif /* NWSDISPLAY > 0 */

	dopowerhooks(PWR_SUSPEND);

	/* XXX enable charging during suspend */

	/* XXX keep power LED state during suspend */

	/* XXX do the same thing for GPIO 43 (BTTXD) */

	/* XXX scoop power down */

	/* XXX set PGSRn and GPDRn */

	pxa2x0_wakeup_config(PXA2X0_WAKEUP_ALL, 1);

	do {
		pxa2x0_apm_sleep(sc);
	}
	while (!zapm_resume(sc));

	zapm_restart();

	/* NOTREACHED */
	dopowerhooks(PWR_RESUME);

#if NWSDISPLAY > 0
	wsdisplay_resume();
#endif /* NWSDISPLAY > 0 */
}

/*
 * Do a GPIO reset, immediately causing the processor to begin the normal
 * boot sequence.  See 2.7 Reset in the PXA27x Developer's Manual for the
 * summary of effects of this kind of reset.
 */
void
zapm_restart(void)
{
	if (apm_cd.cd_ndevs > 0 && apm_cd.cd_devs[0] != NULL) {
		struct pxa2x0_apm_softc *sc = apm_cd.cd_devs[0];
		int rv;

		/*
		 * Reduce the ROM Delay Next Access and ROM Delay First
		 * Access times for synchronous flash connected to nCS1.
		 */
		rv = bus_space_read_4(sc->sc_iot, sc->sc_memctl_ioh,
		    MEMCTL_MSC0);
		if ((rv & 0xffff0000) == 0x7ff00000)
			bus_space_write_4(sc->sc_iot, sc->sc_memctl_ioh,
			    MEMCTL_MSC0, (rv & 0xffff) | 0x7ee00000);
	}

	/* External reset circuit presumably asserts nRESET_GPIO. */
	pxa2x0_gpio_set_function(89, GPIO_OUT | GPIO_SET);
	delay(1000000);
}
