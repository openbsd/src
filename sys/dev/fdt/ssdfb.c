/* $OpenBSD: ssdfb.c,v 1.1 2018/07/30 08:14:45 patrick Exp $ */
/*
 * Copyright (c) 2018 Patrick Wildt <patrick@blueri.se>
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
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/stdint.h>
#include <sys/timeout.h>
#include <sys/task.h>

#include <dev/spi/spivar.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_gpio.h>
#include <dev/ofw/ofw_pinctrl.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

#define SSDFB_SET_LOWER_COLUMN_START_ADRESS	0x00
#define SSDFB_SET_HIGHER_COLUMN_START_ADRESS	0x10
#define SSDFB_SET_MEMORY_ADRESSING_MODE		0x20
#define SSDFB_SET_START_LINE			0x40
#define SSDFB_SET_CONTRAST_CONTROL		0x81
#define SSDFB_SET_COLUMN_DIRECTION_REVERSE	0xa1
#define SSDFB_SET_MULTIPLEX_RATIO		0xa8
#define SSDFB_SET_COM_OUTPUT_DIRECTION		0xc0
#define SSDFB_ENTIRE_DISPLAY_ON			0xa4
#define SSDFB_SET_DISPLAY_MODE_NORMAL		0xa6
#define SSDFB_SET_DISPLAY_MODE_INVERS		0xa7
#define SSDFB_SET_DISPLAY_OFF			0xae
#define SSDFB_SET_DISPLAY_ON			0xaf
#define SSDFB_SET_DISPLAY_OFFSET		0xd3
#define SSDFB_SET_DISPLAY_CLOCK_DIVIDE_RATIO	0xd5
#define SSDFB_SET_PRE_CHARGE_PERIOD		0xd9
#define SSDFB_SET_COM_PINS_HARD_CONF		0xda
#define SSDFB_SET_VCOM_DESELECT_LEVEL		0xdb
#define SSDFB_SET_PAGE_START_ADRESS		0xb0

#define SSDFB_WIDTH	128
#define SSDFB_HEIGHT	64

struct ssdfb_softc {
	struct device		 sc_dev;
	spi_tag_t		 sc_tag;
	int			 sc_node;

	struct spi_config	 sc_conf;
	uint32_t		*sc_gpio;
	size_t			 sc_gpiolen;
	int			 sc_cd;

	uint8_t			*sc_fb;
	size_t			 sc_fbsize;
	struct rasops_info	 sc_rinfo;

	struct task		 sc_task;
	struct timeout		 sc_to;
};

int	 ssdfb_match(struct device *, void *, void *);
void	 ssdfb_attach(struct device *, struct device *, void *);
int	 ssdfb_detach(struct device *, int);

void	 ssdfb_write_command(struct ssdfb_softc *, char *, size_t);
void	 ssdfb_write_data(struct ssdfb_softc *, char *, size_t);

void	 ssdfb_init(struct ssdfb_softc *);
void	 ssdfb_update(void *);
void	 ssdfb_timeout(void *);

int	 ssdfb_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t	 ssdfb_mmap(void *, off_t, int);
int	 ssdfb_alloc_screen(void *, const struct wsscreen_descr *, void **,
	    int *, int *, long *);
void	 ssdfb_free_screen(void *, void *);
int	 ssdfb_show_screen(void *, void *, int, void (*cb) (void *, int, int),
	    void *);
int	 ssdfb_list_font(void *, struct wsdisplay_font *);
int	 ssdfb_load_font(void *, void *, struct wsdisplay_font *);

struct cfattach ssdfb_ca = {
	sizeof(struct ssdfb_softc),
	ssdfb_match,
	ssdfb_attach,
	ssdfb_detach,
};

struct cfdriver ssdfb_cd = {
	NULL, "ssdfb", DV_DULL
};

struct wsscreen_descr ssdfb_std_descr = { "std" };
struct wsdisplay_charcell ssdfb_bs[SSDFB_WIDTH * SSDFB_HEIGHT];

const struct wsscreen_descr *ssdfb_descrs[] = {
	&ssdfb_std_descr
};

const struct wsscreen_list ssdfb_screen_list = {
	nitems(ssdfb_descrs), ssdfb_descrs
};

struct wsdisplay_accessops ssdfb_accessops = {
	.ioctl = ssdfb_ioctl,
	.mmap = ssdfb_mmap,
	.alloc_screen = ssdfb_alloc_screen,
	.free_screen = ssdfb_free_screen,
	.show_screen = ssdfb_show_screen,
	.load_font = ssdfb_load_font,
	.list_font = ssdfb_list_font
};

int
ssdfb_match(struct device *parent, void *match, void *aux)
{
	struct spi_attach_args *sa = aux;

	if (strcmp(sa->sa_name, "solomon,ssd1309fb-spi") == 0)
		return 1;

	return 0;
}

void
ssdfb_attach(struct device *parent, struct device *self, void *aux)
{
	struct ssdfb_softc *sc = (struct ssdfb_softc *)self;
	struct spi_attach_args *sa = aux;
	struct wsemuldisplaydev_attach_args aa;
	struct rasops_info *ri;
	size_t len;

	sc->sc_tag = sa->sa_tag;
	sc->sc_node = *(int *)sa->sa_cookie;

	pinctrl_byname(sc->sc_node, "default");

	sc->sc_conf.sc_bpw = 8;
	sc->sc_conf.sc_freq = 1000 * 1000;
	sc->sc_conf.sc_cs = OF_getpropint(sc->sc_node, "reg", 0);
	if (OF_getproplen(sc->sc_node, "spi-cpol") == 0)
		sc->sc_conf.sc_flags |= SPI_CONFIG_CPOL;
	if (OF_getproplen(sc->sc_node, "spi-cpha") == 0)
		sc->sc_conf.sc_flags |= SPI_CONFIG_CPHA;
	if (OF_getproplen(sc->sc_node, "spi-cs-high") == 0)
		sc->sc_conf.sc_flags |= SPI_CONFIG_CS_HIGH;

	len = OF_getproplen(sc->sc_node, "reset-gpio");
	if (len > 0) {
		sc->sc_gpio = malloc(len, M_DEVBUF, M_WAITOK);
		OF_getpropintarray(sc->sc_node, "reset-gpio",
		    sc->sc_gpio, len);
		gpio_controller_config_pin(sc->sc_gpio, GPIO_CONFIG_OUTPUT);
		gpio_controller_set_pin(sc->sc_gpio, 1);
		delay(100 * 1000);
		gpio_controller_set_pin(sc->sc_gpio, 0);
		delay(1000 * 1000);
		free(sc->sc_gpio, M_DEVBUF, len);
	}

	len = OF_getproplen(sc->sc_node, "cd-gpio");
	if (len <= 0)
		return;

	sc->sc_gpio = malloc(len, M_DEVBUF, M_WAITOK);
	OF_getpropintarray(sc->sc_node, "cd-gpio", sc->sc_gpio, len);
	sc->sc_gpiolen = len;
	gpio_controller_config_pin(sc->sc_gpio, GPIO_CONFIG_OUTPUT);
	gpio_controller_set_pin(sc->sc_gpio, 0);

	sc->sc_fbsize = (SSDFB_WIDTH * SSDFB_HEIGHT) / 8;
	sc->sc_fb = malloc(sc->sc_fbsize, M_DEVBUF, M_WAITOK | M_ZERO);

	ri = &sc->sc_rinfo;
	ri->ri_bits = malloc(sc->sc_fbsize, M_DEVBUF, M_WAITOK | M_ZERO);
	ri->ri_bs = ssdfb_bs;
	ri->ri_flg = RI_CLEAR | RI_VCONS;
	ri->ri_depth = 1;
	ri->ri_width = SSDFB_WIDTH;
	ri->ri_height = SSDFB_HEIGHT;
	ri->ri_stride = ri->ri_width * ri->ri_depth / 8;

	rasops_init(ri, SSDFB_HEIGHT, SSDFB_WIDTH);
	ssdfb_std_descr.ncols = ri->ri_cols;
	ssdfb_std_descr.nrows = ri->ri_rows;
	ssdfb_std_descr.textops = &ri->ri_ops;
	ssdfb_std_descr.fontwidth = ri->ri_font->fontwidth;
	ssdfb_std_descr.fontheight = ri->ri_font->fontheight;
	ssdfb_std_descr.capabilities = ri->ri_caps;

	task_set(&sc->sc_task, ssdfb_update, sc);
	timeout_set(&sc->sc_to, ssdfb_timeout, sc);

	printf(": %dx%d, %dbpp\n", ri->ri_width, ri->ri_height, ri->ri_depth);

	memset(&aa, 0, sizeof(aa));
	aa.console = 0;
	aa.scrdata = &ssdfb_screen_list;
	aa.accessops = &ssdfb_accessops;
	aa.accesscookie = sc;
	aa.defaultscreens = 0;

	config_found_sm(self, &aa, wsemuldisplaydevprint,
	    wsemuldisplaydevsubmatch);
	ssdfb_init(sc);
}

int
ssdfb_detach(struct device *self, int flags)
{
	struct ssdfb_softc *sc = (struct ssdfb_softc *)self;
	struct rasops_info *ri = &sc->sc_rinfo;
	free(ri->ri_bits, M_DEVBUF, sc->sc_fbsize);
	free(sc->sc_fb, M_DEVBUF, sc->sc_fbsize);
	free(sc->sc_gpio, M_DEVBUF, sc->sc_gpiolen);
	return 0;
}

void
ssdfb_init(struct ssdfb_softc *sc)
{
	uint8_t reg[2];

	reg[0] = SSDFB_SET_DISPLAY_OFF;
	ssdfb_write_command(sc, reg, 1);

	reg[0] = SSDFB_SET_MEMORY_ADRESSING_MODE;
	reg[1] = 0x10; /* Page Adressing Mode */
	ssdfb_write_command(sc, reg, 2);
	reg[0] = SSDFB_SET_PAGE_START_ADRESS;
	ssdfb_write_command(sc, reg, 1);
	reg[0] = SSDFB_SET_DISPLAY_CLOCK_DIVIDE_RATIO;
	reg[1] = 0xa0;
	ssdfb_write_command(sc, reg, 2);
	reg[0] = SSDFB_SET_MULTIPLEX_RATIO;
	reg[1] = 0x3f;
	ssdfb_write_command(sc, reg, 2);
	reg[0] = SSDFB_SET_DISPLAY_OFFSET;
	reg[1] = 0x00;
	ssdfb_write_command(sc, reg, 2);
	reg[0] = SSDFB_SET_START_LINE | 0x00;
	ssdfb_write_command(sc, reg, 1);
	reg[0] = SSDFB_SET_COLUMN_DIRECTION_REVERSE;
	ssdfb_write_command(sc, reg, 1);
	reg[0] = SSDFB_SET_COM_OUTPUT_DIRECTION | 0x08;
	ssdfb_write_command(sc, reg, 1);
	reg[0] = SSDFB_SET_COM_PINS_HARD_CONF;
	reg[1] = 0x12;
	ssdfb_write_command(sc, reg, 2);
	reg[0] = SSDFB_SET_CONTRAST_CONTROL;
	reg[1] = 223;
	ssdfb_write_command(sc, reg, 2);
	reg[0] = SSDFB_SET_PRE_CHARGE_PERIOD;
	reg[1] = 0x82;
	ssdfb_write_command(sc, reg, 2);
	reg[0] = SSDFB_SET_VCOM_DESELECT_LEVEL;
	reg[1] = 0x34;
	ssdfb_write_command(sc, reg, 2);
	reg[0] = SSDFB_ENTIRE_DISPLAY_ON;
	ssdfb_write_command(sc, reg, 1);
	reg[0] = SSDFB_SET_DISPLAY_MODE_NORMAL;
	ssdfb_write_command(sc, reg, 1);

	ssdfb_update(sc);

	reg[0] = SSDFB_SET_DISPLAY_ON;
	ssdfb_write_command(sc, reg, 1);
}

void
ssdfb_update(void *v)
{
	struct ssdfb_softc *sc = v;
	struct rasops_info *ri = &sc->sc_rinfo;
	uint8_t *bit, val;
	uint32_t off;
	int i, j, k;

	memset(sc->sc_fb, 0, sc->sc_fbsize);

	for (i = 0; i < ri->ri_height; i += 8) {
		for (j = 0; j < ri->ri_width; j++) {
			bit = &sc->sc_fb[(i / 8) * ri->ri_width + j];
			for (k = 0; k < 8; k++) {
				off = ri->ri_stride * (i + k) + j / 8;
				val = *(ri->ri_bits + off);
				val &= (1 << (8 - (j % 8)));
				*bit |= !!val << k;
			}
		}
	}

	ssdfb_write_data(sc, sc->sc_fb, sc->sc_fbsize);
	timeout_add_msec(&sc->sc_to, 100);
}

void
ssdfb_timeout(void *v)
{
	struct ssdfb_softc *sc = v;
	task_add(systq, &sc->sc_task);
}

void
ssdfb_write_command(struct ssdfb_softc *sc, char *buf, size_t len)
{
	if (sc->sc_cd != 0) {
		gpio_controller_set_pin(sc->sc_gpio, 0);
		sc->sc_cd = 0;
		delay(1);
	}

	spi_acquire_bus(sc->sc_tag, 0);
	spi_config(sc->sc_tag, &sc->sc_conf);
	if (spi_write(sc->sc_tag, buf, len))
		printf("%s: cannot write\n", sc->sc_dev.dv_xname);
	spi_release_bus(sc->sc_tag, 0);
}

void
ssdfb_write_data(struct ssdfb_softc *sc, char *buf, size_t len)
{
	if (sc->sc_cd != 1) {
		gpio_controller_set_pin(sc->sc_gpio, 1);
		sc->sc_cd = 1;
		delay(1);
	}

	spi_acquire_bus(sc->sc_tag, 0);
	spi_config(sc->sc_tag, &sc->sc_conf);
	if (spi_write(sc->sc_tag, buf, len))
		printf("%s: cannot write\n", sc->sc_dev.dv_xname);
	spi_release_bus(sc->sc_tag, 0);
}

int
ssdfb_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct ssdfb_softc	*sc = v;
	struct rasops_info 	*ri = &sc->sc_rinfo;
	struct wsdisplay_fbinfo	*wdf;

	switch (cmd) {
	case WSDISPLAYIO_GETPARAM:
	case WSDISPLAYIO_SETPARAM:
		return (-1);
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_UNKNOWN;
		break;
	case WSDISPLAYIO_GINFO:
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->width = ri->ri_width;
		wdf->height = ri->ri_height;
		wdf->depth = ri->ri_depth;
		wdf->cmsize = 0;	/* color map is unavailable */
		break;
	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = ri->ri_stride;
		break;
	case WSDISPLAYIO_SMODE:
		break;
	case WSDISPLAYIO_GETSUPPORTEDDEPTH:
		*(u_int *)data = WSDISPLAYIO_DEPTH_1;
		break;
	default:
		return (-1);
	}

	return (0);
}

paddr_t
ssdfb_mmap(void *v, off_t off, int prot)
{
	return -1;
}

int
ssdfb_alloc_screen(void *v, const struct wsscreen_descr *descr,
    void **cookiep, int *curxp, int *curyp, long *attrp)
{
	struct ssdfb_softc	*sc = v;
	struct rasops_info	*ri = &sc->sc_rinfo;

	return rasops_alloc_screen(ri, cookiep, curxp, curyp, attrp);
}

void
ssdfb_free_screen(void *v, void *cookie)
{
	struct ssdfb_softc	*sc = v;
	struct rasops_info	*ri = &sc->sc_rinfo;

	rasops_free_screen(ri, cookie);
}

int
ssdfb_show_screen(void *v, void *cookie, int waitok,
    void (*cb) (void *, int, int), void *cb_arg)
{
	struct ssdfb_softc	*sc = v;
	struct rasops_info	*ri = &sc->sc_rinfo;

	return rasops_show_screen(ri, cookie, waitok, cb, cb_arg);
}

int
ssdfb_load_font(void *v, void *cookie, struct wsdisplay_font *font)
{
	struct ssdfb_softc	*sc = v;
	struct rasops_info	*ri = &sc->sc_rinfo;

	return (rasops_load_font(ri, cookie, font));
}

int
ssdfb_list_font(void *v, struct wsdisplay_font *font)
{
	struct ssdfb_softc	*sc = v;
	struct rasops_info	*ri = &sc->sc_rinfo;

	return (rasops_list_font(ri, font));
}
