/*	$OpenBSD: zaurus_apm.c,v 1.4 2005/03/30 21:44:08 uwe Exp $	*/

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

#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0_apm.h>
#include <arm/xscale/pxa2x0_gpio.h>

#include <zaurus/dev/zaurus_scoopvar.h>
#include <zaurus/dev/zaurus_sspvar.h>

#if defined(APMDEBUG)
#define DPRINTF(x)	printf x
#else
#define	DPRINTF(x)	/**/
#endif

int	apm_match(struct device *, void *, void *);
void	apm_attach(struct device *, struct device *, void *);

struct cfattach apm_pxaip_ca = {
        sizeof (struct pxa2x0_apm_softc), apm_match, apm_attach
};

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
#define GPIO_AC_IN_C3000	115	/* active low */
#define GPIO_CHRG_FULL_C3000	101
#define GPIO_BATT_COVER_C3000	90	/* active low */

struct battery_threshold {
	int	bt_volt;
	int	bt_life;
	int	bt_state;
};

struct battery_info {
	int	bi_minutes;	/* minutes left at 100% battery life */
	const	struct battery_threshold *bi_thres;
};

const struct battery_threshold zaurus_battery_life_c3000[] = {
#if 0
	{224,	125,	APM_BATT_HIGH},	/* XXX untested */
#endif
	{194,	100,	APM_BATT_HIGH},
	{188,	75,	APM_BATT_HIGH},
	{184,	50,	APM_BATT_HIGH},
	{180,	25,	APM_BATT_LOW},
	{176,	5,	APM_BATT_LOW},
	{0,	0,	APM_BATT_CRITICAL},
};

const struct battery_info zaurus_battery_c3000 = {
	180 /* minutes; pessimistic estimate */,
	zaurus_battery_life_c3000
};

const struct battery_info *zaurus_main_battery = &zaurus_battery_c3000;

#if 0
void	zapm_shutdown(void *);
int	zapm_acintr(void *);
#endif
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
int	zapm_batt_full(void);

int	zapm_curbattvolt;	/* updated periodically when A/C is on */
int	zapm_battcharging;
int	zapm_battfullcount;

struct timeout zapm_charge_off_to;
struct timeout zapm_charge_on_to;

void	zapm_charge_enable(void);
void	zapm_charge_disable(void);
void	zapm_charge_restart(void);
void	zapm_charge_off(void *);
void	zapm_charge_on(void *);

void	zapm_power_check(struct pxa2x0_apm_softc *);
void	zapm_power_info(struct pxa2x0_apm_softc *,
    struct apm_power_info *);

int
apm_match(struct device *parent, void *match, void *aux)
{
	return 1;
}

void
apm_attach(struct device *parent, struct device *self, void *aux)
{
	struct pxa2x0_apm_softc *sc = (struct pxa2x0_apm_softc *)self;

	pxa2x0_gpio_set_function(GPIO_AC_IN_C3000, GPIO_IN);
	pxa2x0_gpio_set_function(GPIO_CHRG_FULL_C3000, GPIO_IN);
	pxa2x0_gpio_set_function(GPIO_BATT_COVER_C3000, GPIO_IN);

#if 0
	(void)pxa2x0_gpio_intr_establish(GPIO_AC_IN_C3000, IST_EDGE_BOTH,
	    IPL_BIO, zapm_acintr, sc, "apm_ac");
#endif

	sc->sc_periodic_check = zapm_power_check;
	sc->sc_power_info = zapm_power_info;

	timeout_set(&zapm_charge_off_to, &zapm_charge_off, NULL);
	timeout_set(&zapm_charge_on_to, &zapm_charge_on, NULL);

	zapm_charge_disable();
	zapm_battcharging = 0;
	zapm_battfullcount = 0;

	/* C3000: discharge 100 ms when AC is on. */
	if (zapm_ac_on()) {
		scoop_discharge_battery(1);
		delay(100000);
	}

	zapm_curbattvolt = zapm_batt_volt();
	scoop_discharge_battery(0);

	zapm_power_check(sc);

	pxa2x0_apm_attach_sub(sc);

#if 0
	(void)shutdownhook_establish(zapm_shutdown, NULL);
#endif
}

#if 0
void
zapm_shutdown(void *v)
{
	zapm_charge_disable();
}

int
zapm_acintr(void *v)
{
	return 1;
}
#endif

int
zapm_ac_on(void)
{
	return (!pxa2x0_gpio_get_bit(GPIO_AC_IN_C3000));
}

int
max1111_adc_value(int chan)
{

	return (zssp_read_max1111(MAXCTRL_PD0 | MAXCTRL_PD1 |
	    MAXCTRL_SGL | MAXCTRL_UNI | (chan << MAXCTRL_SEL_SHIFT) |
	    MAXCTRL_STR));
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

/*
 * Return non-zero if the charge complete signal is set.  This signal
 * becomes valid after charging has been stopped and restarted.
 */
int
zapm_batt_full(void)
{

	return (pxa2x0_gpio_get_bit(GPIO_CHRG_FULL_C3000) ? 1 : 0);
}

void
zapm_charge_enable(void)
{

	timeout_del(&zapm_charge_off_to);
	timeout_del(&zapm_charge_on_to);

	scoop_charge_battery(1, 0);
	scoop_discharge_battery(0);
	scoop_led_set(SCOOP_LED_ORANGE, 1);

	/* Restart charging and updating curbattvolt. */
	timeout_add(&zapm_charge_off_to, hz * 60);
}

void
zapm_charge_disable(void)
{

	timeout_del(&zapm_charge_off_to);
	timeout_del(&zapm_charge_on_to);

	scoop_discharge_battery(0);
	scoop_charge_battery(0, 0);
	scoop_led_set(SCOOP_LED_ORANGE, 0);
}

void
zapm_charge_restart(void)
{

	zapm_charge_disable();
	delay(15000);
	zapm_charge_enable();
}

void
zapm_charge_off(void *v)
{

	if (zapm_battcharging)
		zapm_charge_disable();

	/* Discharge 100 ms before updating curbattvolt. */
	if (zapm_ac_on()) {
		scoop_discharge_battery(1);
		timeout_add(&zapm_charge_on_to, hz / 10);
	}
}

void
zapm_charge_on(void *v)
{

	/*
	 * Read battery voltage while the battery is still discharging,
	 * then restart charging or schedule the next curbattvolt update.
	 */
	if (zapm_ac_on()) {
		zapm_curbattvolt = zapm_batt_volt();
		if (zapm_battcharging)
			zapm_charge_enable();
		else
			timeout_add(&zapm_charge_off_to, hz * 60);
	}

	scoop_discharge_battery(0);
}

/*
 * Check A/C power and control battery charging.  This gets called once
 * from apm_attach(), and once per second from the APM kernel thread.
 */
void
zapm_power_check(struct pxa2x0_apm_softc *sc)
{
	int s;

	s = splsoftclock();

	if (zapm_ac_on()) {
		if (zapm_battcharging) {
			/*
			 * Read BATT_FULL once per second until it
			 * stablizes; restart charging between reads.
			 */
			if (zapm_batt_full()) {
				if (++zapm_battfullcount >= 2) {
					/* battery full; stop charging. */
					DPRINTF(("zapm_power_check: battery full\n"));
					zapm_battcharging = 0;
					zapm_charge_disable();
					zapm_charge_off(NULL);
				} else
					zapm_charge_restart();
			} else if (zapm_battfullcount > 0) {
				/* Ignore BATT_FULL glitch. */
				DPRINTF(("zapm_power_check: battery almost full?\n"));
				zapm_battfullcount = 0;
				zapm_charge_restart();
			}
		} else if (zapm_battfullcount == 0) {
			/* Start charging and updating curbattvolt. */
			DPRINTF(("zapm_power_check: start charging\n"));
			zapm_battcharging = 1;
			zapm_charge_off(NULL);
		}
	} else if (zapm_battcharging || zapm_battfullcount != 0) {
		/* Stop charging and updating curbattvolt. */
		DPRINTF(("zapm_power_check: stop charging\n"));
		zapm_battcharging = 0;
		zapm_battfullcount = 0;
		zapm_charge_disable();
	} else {
		/* Running on battery. */
		/* XXX detect battery low condition and take measures. */
	}

	splx(s);
}

/*
 * Report A/C and battery state in response to a request from apmd.
 */
void
zapm_power_info(struct pxa2x0_apm_softc *sc,
    struct apm_power_info *power)
{
	int s;
	int volt;
	int charging;

	s = splsoftclock();
	volt = zapm_curbattvolt;
	charging = zapm_battcharging;
	splx(s);

	power->ac_state = zapm_ac_on() ? APM_AC_ON : APM_AC_OFF;
	if (power->ac_state == APM_AC_OFF)
		volt = zapm_batt_volt();

	if (charging)
		power->battery_state = APM_BATT_CHARGING;
	else
		power->battery_state = zapm_batt_state(volt);

	power->battery_life = zapm_batt_life(volt);
	power->minutes_left = zapm_batt_minutes(power->battery_life);
}
