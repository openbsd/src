/*	$OpenBSD: pca9554.c,v 1.17 2008/09/10 16:13:43 reyk Exp $	*/

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

/* Philips 9554/6/7 registers */
#define PCA9554_IN		0x00
#define PCA9554_OUT		0x01
#define PCA9554_POLARITY	0x02
#define PCA9554_CONFIG		0x03

/* Philips 9555 registers */
#define PCA9555_IN0		0x00
#define PCA9555_IN1		0x01
#define PCA9555_OUT0		0x02
#define PCA9555_OUT1		0x03
#define PCA9555_POLARITY0	0x04
#define PCA9555_POLARITY1	0x05
#define PCA9555_CONFIG0		0x06
#define PCA9555_CONFIG1		0x07

/* Sensors */
#define PCAGPIO_NPINS	16

#define PCAGPIO_NPORTS	2
#define PCAGPIO_PORT(_pin)	((_pin) > 7 ? 1 : 0)
#define PCAGPIO_BIT(_pin)	(1 << ((_pin) % 8))

/* Register mapping index */
enum pcigpio_cmd {
	PCAGPIO_IN		= 0,
	PCAGPIO_OUT,
	PCAGPIO_POLARITY,
	PCAGPIO_CONFIG,
	PCAGPIO_MAX
};

struct pcagpio_softc {
	struct device	sc_dev;
	i2c_tag_t	sc_tag;
	i2c_addr_t	sc_addr;

	u_int8_t	sc_npins;
	u_int8_t	sc_control[PCAGPIO_NPORTS];
	u_int8_t	sc_polarity[PCAGPIO_NPORTS];
	u_int8_t	sc_regs[PCAGPIO_NPORTS][PCAGPIO_MAX];

	struct gpio_chipset_tag sc_gpio_gc;
        gpio_pin_t sc_gpio_pins[PCAGPIO_NPINS];

	struct ksensor sc_sensor[PCAGPIO_NPINS];
	struct ksensordev sc_sensordev;
};

int	pcagpio_match(struct device *, void *, void *);
void	pcagpio_attach(struct device *, struct device *, void *);
int	pcagpio_init(struct pcagpio_softc *, int, u_int8_t *);
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
	int outputs = 0, i, port, bit;
	u_int8_t data[PCAGPIO_NPORTS];

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	if (strcmp(ia->ia_name, "pca9555") == 0) {
		/* The pca9555 has two 8 bit ports */
		sc->sc_regs[0][PCAGPIO_IN] = PCA9555_IN0;
		sc->sc_regs[0][PCAGPIO_OUT] = PCA9555_OUT0;
		sc->sc_regs[0][PCAGPIO_POLARITY] = PCA9555_POLARITY0;
		sc->sc_regs[0][PCAGPIO_CONFIG] = PCA9555_CONFIG0;
		sc->sc_regs[1][PCAGPIO_IN] = PCA9555_IN1;
		sc->sc_regs[1][PCAGPIO_OUT] = PCA9555_OUT1;
		sc->sc_regs[1][PCAGPIO_POLARITY] = PCA9555_POLARITY1;
		sc->sc_regs[1][PCAGPIO_CONFIG] = PCA9555_CONFIG1;
		sc->sc_npins = 16;
	} else {
		/* All other supported devices have one 8 bit port */
		sc->sc_regs[0][PCAGPIO_IN] = PCA9554_IN;
		sc->sc_regs[0][PCAGPIO_OUT] = PCA9554_OUT;
		sc->sc_regs[0][PCAGPIO_POLARITY] = PCA9554_POLARITY;
		sc->sc_regs[0][PCAGPIO_CONFIG] = PCA9554_CONFIG;
		sc->sc_npins = 8;
	}
	if (pcagpio_init(sc, 0, &data[0]) != 0)
		return;
	if (sc->sc_npins > 8 && pcagpio_init(sc, 1, &data[1]) != 0)
		return;

	/* Initialize sensor data. */
	strlcpy(sc->sc_sensordev.xname, sc->sc_dev.dv_xname,
	    sizeof(sc->sc_sensordev.xname));

	for (i = 0; i < sc->sc_npins; i++) {
		port = PCAGPIO_PORT(i);
		bit = PCAGPIO_BIT(i);
		sc->sc_sensor[i].type = SENSOR_INDICATOR;
		if ((sc->sc_control[port] & bit) == 0) {
			strlcpy(sc->sc_sensor[i].desc, "out",
			    sizeof(sc->sc_sensor[i].desc));
			outputs++;
		} else
			strlcpy(sc->sc_sensor[i].desc, "in",
			    sizeof(sc->sc_sensor[i].desc));
	}

	if (sensor_task_register(sc, pcagpio_refresh, 5) == NULL) {
		printf(", unable to register update task\n");
		return;
	}

#if 0
	for (i = 0; i < sc->sc_npins; i++)
		sensor_attach(&sc->sc_sensordev, &sc->sc_sensor[i]);
	sensordev_install(&sc->sc_sensordev);
#endif

	printf(":");
	if (sc->sc_npins - outputs)
		printf(" %d inputs", sc->sc_npins - outputs);
	if (outputs)
		printf(" %d outputs", outputs);
	printf("\n");

	for (i = 0; i < sc->sc_npins; i++) {
		port = PCAGPIO_PORT(i);
		bit = PCAGPIO_BIT(i);

		sc->sc_gpio_pins[i].pin_num = i;
		sc->sc_gpio_pins[i].pin_caps = GPIO_PIN_INPUT | GPIO_PIN_OUTPUT;

		if ((sc->sc_control[port] & bit) == 0) {
			sc->sc_gpio_pins[i].pin_flags = GPIO_PIN_OUTPUT;
			sc->sc_gpio_pins[i].pin_state = data[port] &
			    bit ? GPIO_PIN_HIGH : GPIO_PIN_LOW;
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
	gba.gba_npins = sc->sc_npins;

	config_found(&sc->sc_dev, &gba, gpiobus_print);

}

int
pcagpio_init(struct pcagpio_softc *sc, int port, u_int8_t *datap)
{
	u_int8_t cmd, data;

	cmd = sc->sc_regs[port][PCAGPIO_CONFIG];
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0)) {
		printf(": failed to initialize\n");
		return (-1);
	}
	sc->sc_control[port] = data;
	cmd = sc->sc_regs[port][PCAGPIO_POLARITY];
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0)) {
		printf(": failed to initialize\n");
		return (-1);
	}
	sc->sc_polarity[port] = data;
	cmd = sc->sc_regs[port][PCAGPIO_OUT];
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0)) {
		printf(": failed to initialize\n");
		return (-1);
	}

	*datap = data;
	return (0);
}

void
pcagpio_refresh(void *arg)
{
	struct pcagpio_softc *sc = arg;
	u_int8_t cmd, bit, in[PCAGPIO_NPORTS], out[PCAGPIO_NPORTS];
	int i, port;

	iic_acquire_bus(sc->sc_tag, 0);

	for (i = 0; i < PCAGPIO_NPORTS; i++) {
		cmd = sc->sc_regs[i][PCAGPIO_IN];
		if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
		    sc->sc_addr, &cmd, sizeof cmd, &in[i], sizeof in[i], 0))
			goto invalid;

		cmd = sc->sc_regs[i][PCAGPIO_OUT];
		if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
		    sc->sc_addr, &cmd, sizeof cmd, &out[i], sizeof out[i], 0))
			goto invalid;
	}

	for (i = 0; i < sc->sc_npins; i++) {
		port = PCAGPIO_PORT(i);
		bit = PCAGPIO_BIT(i);
		if ((sc->sc_control[port] & bit))
			sc->sc_sensor[i].value = (in[port] & bit) ? 1 : 0;
		else
			sc->sc_sensor[i].value = (out[port] & bit) ? 1 : 0;
	}

invalid:
	iic_release_bus(sc->sc_tag, 0);
}


int
pcagpio_gpio_pin_read(void *arg, int pin)
{
	struct pcagpio_softc *sc = arg;
	u_int8_t cmd, in;
	int port, bit;

	port = PCAGPIO_PORT(pin);
	bit = PCAGPIO_BIT(pin);

	cmd = sc->sc_regs[port][PCAGPIO_IN];
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &in, sizeof in, 0))
		return 0;
	return ((in ^ sc->sc_polarity[port]) & bit) ? 1 : 0;
}

void
pcagpio_gpio_pin_write(void *arg, int pin, int value)
{
	struct pcagpio_softc *sc = arg;
	u_int8_t cmd, out, mask;
	int port, bit;

	port = PCAGPIO_PORT(pin);
	bit = PCAGPIO_BIT(pin);

	mask = 0xff ^ bit;
	cmd = sc->sc_regs[port][PCAGPIO_OUT];
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &out, sizeof out, 0))
		return;
	out = (out & mask) | (value ? bit : 0);

	cmd = sc->sc_regs[port][PCAGPIO_OUT];
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
