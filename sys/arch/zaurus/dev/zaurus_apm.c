/*	$OpenBSD: zaurus_apm.c,v 1.1 2005/01/26 06:34:54 uwe Exp $	*/

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
#include <sys/conf.h>

#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0_apm.h>
#include <arm/xscale/pxa2x0_gpio.h>

#include <zaurus/dev/zaurus_scoopvar.h>
#include <zaurus/dev/zaurus_sspvar.h>

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

/* Battery-related GPIO pins */
#define GPIO_AC_IN_C3000	115	/* active low */
#define GPIO_CHRG_FULL_C3000	101
#define GPIO_BATT_COVER_C3000	90	/* active low */

/* Internal software power states */
#define PS_UNKNOWN		0
#define PS_BATT_ABSENT		1
#define PS_NOT_CHARGING		2
#define PS_CHARGING		3
#define PS_BATT_FULL		4

#ifdef APMDEBUG
const	char *zaurus_power_state_names[5] = {
	"unknown", "absent", "not charging", "charging", "full"
};
#endif

int	zaurus_power_state = PS_UNKNOWN;

struct battery_voltage_threshold {
	int	voltage;
	int	life;
	int	state;
};

struct battery_voltage_threshold zaurus_battery_c3000[] = {
	{194,	100,	APM_BATT_HIGH},
	{188,	75,	APM_BATT_HIGH},
	{184,	50,	APM_BATT_HIGH},
	{180,	25,	APM_BATT_LOW},
	{176,	5,	APM_BATT_LOW},
	{0,	0,	APM_BATT_CRITICAL},
};

struct battery_voltage_threshold *zaurus_main_battery =
    zaurus_battery_c3000;

int	max1111_adc_value(int);
int	max1111_adc_value_avg(int, int);
#if 0
int	zaurus_jkvad_voltage(void);
int	zaurus_battery_temp(void);
#endif
int	zaurus_battery_voltage(void);
int	zaurus_battery_life(void);
int	zaurus_battery_state(void);
int	zaurus_ac_present(void);
int	zaurus_charge_complete(void);
void	zaurus_charge_control(int);
void	zaurus_power_check(struct pxa2x0_apm_softc *);
void	zaurus_power_info(struct pxa2x0_apm_softc *,
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
	    IPL_BIO, apm_intr, sc, "apm_ac");
#endif

	sc->sc_periodic_check = zaurus_power_check;
	sc->sc_power_info = zaurus_power_info;

	/* Initialize the battery status before APM is enabled. */
	zaurus_power_check(sc);

	pxa2x0_apm_attach_sub(sc);
}

int
max1111_adc_value(int chan)
{

	return zssp_read_max1111(MAXCTRL_PD0 | MAXCTRL_PD1 |
	    MAXCTRL_SGL | MAXCTRL_UNI | (chan << MAXCTRL_SEL_SHIFT) |
	    MAXCTRL_STR);
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
zaurus_jkvad_voltage(void)
{

	return max1111_adc_value_avg(JK_VAD, 10);
}

int
zaurus_battery_temp(void)
{
	int temp;

	scoop_battery_temp_adc(1);
	delay(10000);
	temp = max1111_adc_value_avg(BATT_THM, 1);
	scoop_battery_temp_adc(0);

	return temp;
}
#endif

int
zaurus_battery_voltage(void)
{

	return max1111_adc_value_avg(BATT_AD, 10);
}

int
zaurus_battery_life(void)
{
	int i;
	int voltage;

	voltage = zaurus_battery_voltage();

	for (i = 0; zaurus_main_battery[i].voltage > 0; i++) {
		if (voltage >= zaurus_main_battery[i].voltage)
			break;
	}

	return zaurus_main_battery[i].life;
}

int
zaurus_battery_state(void)
{
	int i;
	int voltage;

	voltage = zaurus_battery_voltage();

	for (i = 0; zaurus_main_battery[i].voltage > 0; i++) {
		if (voltage >= zaurus_main_battery[i].voltage)
			break;
	}

	return zaurus_main_battery[i].state;
}

int
zaurus_ac_present(void)
{

	return !pxa2x0_gpio_get_bit(GPIO_AC_IN_C3000);
}

/*
 * Return non-zero if the charge complete signal is set.  This signal
 * is valid only after charging is restarted.
 */
int
zaurus_charge_complete(void)
{

	return pxa2x0_gpio_get_bit(GPIO_CHRG_FULL_C3000);
}

void
zaurus_charge_control(int state)
{

	switch (state) {
	case PS_CHARGING:
		scoop_charge_battery(1, 0);
		scoop_led_set(SCOOP_LED_ORANGE, 1);
		break;
	case PS_NOT_CHARGING:
	case PS_BATT_FULL:
		scoop_charge_battery(0, 0);
		scoop_led_set(SCOOP_LED_ORANGE, 0);
		/* Always force a 15 ms delay before charging again. */
		delay(15000);
		break;
	default:
		printf("zaurus_charge_control: bad state %d\n", state);
		break;
	}

	zaurus_power_state = state;
}

/*
 * Check A/C power and control battery charging.  This gets called once
 * from apm_attach(), and once per second from the APM kernel thread.
 */
void
zaurus_power_check(struct pxa2x0_apm_softc *sc)
{
	int state = zaurus_power_state;

	switch (state) {
	case PS_UNKNOWN:
	case PS_BATT_ABSENT:
		state = PS_NOT_CHARGING;
		zaurus_charge_control(state);
		/* FALLTHROUGH */

	case PS_NOT_CHARGING:
		if (zaurus_ac_present())
			state = PS_CHARGING;
		break;

	case PS_CHARGING:
		if (!zaurus_ac_present())
			state = PS_NOT_CHARGING;
		else if (zaurus_charge_complete())
			state = PS_BATT_FULL;
		break;

	case PS_BATT_FULL:
		if (!zaurus_ac_present())
			state = PS_NOT_CHARGING;
		break;

	default:
		printf("zaurus_power_check: bad state %d\n", state);
		break;
	}

	if (state != zaurus_power_state) {
#ifdef APMDEBUG
		printf("zaurus_power_check: battery state %s -> %s volt %d\n",
		    zaurus_power_state_names[zaurus_power_state],
		    zaurus_power_state_names[state],
		    zaurus_battery_voltage());
#endif
		zaurus_charge_control(state);
	}
}

/*
 * Report A/C and battery state in response to a request from apmd.
 */
void
zaurus_power_info(struct pxa2x0_apm_softc *sc,
    struct apm_power_info *power)
{

	if (zaurus_power_state == PS_CHARGING) {
		power->ac_state = APM_AC_ON;
		power->battery_state = APM_BATT_CHARGING;
		power->battery_life = 100;
	} else {
		power->ac_state = zaurus_ac_present() ? APM_AC_ON :
		    APM_AC_OFF;
		power->battery_state = zaurus_battery_state();
		power->battery_life = zaurus_battery_life();
	}
}
