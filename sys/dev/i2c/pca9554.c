/*	$OpenBSD: pca9554.c,v 1.11 2007/06/24 05:34:35 dlg Exp $	*/

/*
 * Copyright (c) 2005 Theo de Raadt
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
#include <sys/device.h>
#include <sys/gpio.h>
#include <sys/sensors.h>

#include <dev/i2c/i2cvar.h>

#include <dev/gpio/gpiovar.h>

/* Phillips 9554 registers */
#define PCA9554_IN		0x00
#define PCA9554_OUT		0x01
#define PCA9554_POLARITY	0x02
#define PCA9554_CONFIG		0x03

/* Sensors */
#define PCAGPIO_NPINS	8

struct pcagpio_softc {
	struct device	sc_dev;
	i2c_tag_t	sc_tag;
	i2c_addr_t	sc_addr;
	u_int8_t	sc_control;
	u_int8_t	sc_polarity;

	struct gpio_chipset_tag sc_gpio_gc;
        gpio_pin_t sc_gpio_pins[PCAGPIO_NPINS];

	struct ksensor sc_sensor[PCAGPIO_NPINS];
	struct ksensordev sc_sensordev;
};

int	pcagpio_match(struct device *, void *, void *);
void	pcagpio_attach(struct device *, struct device *, void *);
int	pcagpio_check(struct i2c_attach_args *, u_int8_t *, u_int8_t *);
void	pcagpio_refresh(void *);

int     pcagpio_gpio_pin_read(void *, int);
void    pcagpio_gpio_pin_write(void *, int, int);
void    pcagpio_gpio_pin_ctl(void *, int, int);

struct cfattach pcagpio_ca = {
	sizeof(struct pcagpio_softc), pcagpio_match, pcagpio_attach
};

struct cfdriver pcagpio_cd = {
	NULL, "pcagpio", DV_DULL
};

int
pcagpio_match(struct device *parent, void *match, void *aux)
{
	struct i2c_attach_args *ia = aux;

	if (strcmp(ia->ia_name, "PCA9554") == 0 ||
	    strcmp(ia->ia_name, "PCA9554M") == 0 ||
	    strcmp(ia->ia_name, "pca9555") == 0 ||
	    strcmp(ia->ia_name, "pca9556") == 0 ||
	    strcmp(ia->ia_name, "pca9557") == 0)
		return (1);
	return (0);
}

void
pcagpio_attach(struct device *parent, struct device *self, void *aux)
{
	struct pcagpio_softc *sc = (struct pcagpio_softc *)self;
	struct i2c_attach_args *ia = aux;
	struct gpiobus_attach_args gba;
	u_int8_t cmd, data;
	int outputs = 0, i;

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	cmd = PCA9554_CONFIG;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0)) {
		printf(": failed to initialize\n");
		return;
	}
	sc->sc_control = data;
	cmd = PCA9554_POLARITY;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0)) {
		printf(": failed to initialize\n");
		return;
	}
	sc->sc_polarity = data;
	cmd = PCA9554_OUT;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0)) {
		printf(": failed to initialize\n");
		return;
	}

	/* Initialize sensor data. */
	strlcpy(sc->sc_sensordev.xname, sc->sc_dev.dv_xname,
	    sizeof(sc->sc_sensordev.xname));

	for (i = 0; i < PCAGPIO_NPINS; i++) {
		sc->sc_sensor[i].type = SENSOR_INTEGER;
		if ((sc->sc_control & (1 << i)) == 0) {
			snprintf(sc->sc_sensor[i].desc,
			    sizeof(sc->sc_sensor[i].desc), "out%d", i);
			outputs++;
		} else
			snprintf(sc->sc_sensor[i].desc,
			    sizeof(sc->sc_sensor[i].desc), "in%d", i);

	}

	if (sensor_task_register(sc, pcagpio_refresh, 5) == NULL) {
		printf(", unable to register update task\n");
		return;
	}

#if 0
	for (i = 0; i < PCAGPIO_NPINS; i++)
		sensor_attach(&sc->sc_sensordev, &sc->sc_sensor[i]);
	sensordev_install(&sc->sc_sensordev);
#endif

	printf(":");
	if (8 - outputs)
		printf(" %d inputs", 8 - outputs);
	if (outputs)
		printf(" %d outputs", outputs);
	printf("\n");

	for (i = 0; i < PCAGPIO_NPINS; i++) {
		sc->sc_gpio_pins[i].pin_num = i;
		sc->sc_gpio_pins[i].pin_caps = GPIO_PIN_INPUT | GPIO_PIN_OUTPUT;

		if ((sc->sc_control & (1 << i)) == 0) {
			sc->sc_gpio_pins[i].pin_flags = GPIO_PIN_OUTPUT;
			sc->sc_gpio_pins[i].pin_state =
			    data & (1 << i) ? GPIO_PIN_HIGH : GPIO_PIN_LOW;
		}
	}

	/* Create controller tag */
	sc->sc_gpio_gc.gp_cookie = sc;
	sc->sc_gpio_gc.gp_pin_read = pcagpio_gpio_pin_read;
	sc->sc_gpio_gc.gp_pin_write = pcagpio_gpio_pin_write;
	sc->sc_gpio_gc.gp_pin_ctl = pcagpio_gpio_pin_ctl;

	gba.gba_name = "gpio";
	gba.gba_gc = &sc->sc_gpio_gc;
	gba.gba_pins = sc->sc_gpio_pins;
	gba.gba_npins = PCAGPIO_NPINS;

	config_found(&sc->sc_dev, &gba, gpiobus_print);

}

void
pcagpio_refresh(void *arg)
{
	struct pcagpio_softc *sc = arg;
	u_int8_t cmd, in, out, bit;
	int i;

	iic_acquire_bus(sc->sc_tag, 0);

	cmd = PCA9554_IN;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &in, sizeof in, 0))
		goto invalid;

	cmd = PCA9554_OUT;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &out, sizeof out, 0))
		goto invalid;

	for (i = 0; i < PCAGPIO_NPINS; i++) {
		bit = 1 << i;
		if ((sc->sc_control & bit))
			sc->sc_sensor[i].value = (in & bit) ? 1 : 0;
		else
			sc->sc_sensor[i].value = (out & bit) ? 1 : 0;
	}

invalid:
	iic_release_bus(sc->sc_tag, 0);
}


int
pcagpio_gpio_pin_read(void *arg, int pin)
{
	struct pcagpio_softc *sc = arg;
	u_int8_t cmd, in;

	cmd = PCA9554_IN;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &in, sizeof in, 0))
		return 0;
	return ((in ^ sc->sc_polarity) & (1 << pin)) ? 1 : 0;
}

void
pcagpio_gpio_pin_write(void *arg, int pin, int value)
{
	struct pcagpio_softc *sc = arg;
	u_int8_t cmd, out, mask;

	mask = 0xff ^ (1 << pin);
	cmd = PCA9554_OUT;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &out, sizeof out, 0))
		return;
	out = (out & mask) | (value << pin);

	cmd = PCA9554_OUT;
	if (iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &out, sizeof out, 0))
		return;
}

void
pcagpio_gpio_pin_ctl(void *arg, int pin, int flags)
{
#if 0
	struct pcagpio_softc *sc = arg;
	u_int32_t conf;

	pcagpio_gpio_pin_select(sc, pin);
	conf = bus_space_read_4(sc->sc_gpio_iot, sc->sc_gpio_ioh,
	    GSCGPIO_CONF);

	conf &= ~(GSCGPIO_CONF_OUTPUTEN | GSCGPIO_CONF_PUSHPULL |
	    GSCGPIO_CONF_PULLUP);
	if ((flags & GPIO_PIN_TRISTATE) == 0)
		conf |= GSCGPIO_CONF_OUTPUTEN;
	if (flags & GPIO_PIN_PUSHPULL)
		conf |= GSCGPIO_CONF_PUSHPULL;
	if (flags & GPIO_PIN_PULLUP)
		conf |= GSCGPIO_CONF_PULLUP;
	bus_space_write_4(sc->sc_gpio_iot, sc->sc_gpio_ioh,
	    GSCGPIO_CONF, conf);
#endif
}
