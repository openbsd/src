/*	$OpenBSD: aplhidev.c,v 1.1 2021/11/01 09:02:46 kettenis Exp $	*/
/*
 * Copyright (c) 2021 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/timeout.h>

#include <lib/libkern/crc16.h>

#include <machine/fdt.h>

#include <dev/spi/spivar.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_gpio.h>
#include <dev/ofw/ofw_pinctrl.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsmousevar.h>

#include <dev/hid/hid.h>
#include <dev/hid/hidkbdsc.h>
#include <dev/hid/hidmsvar.h>

#include "aplhidev.h"

#define APLHIDEV_READ_PACKET	0x20
#define APLHIDEV_WRITE_PACKET	0x40

#define APLHIDEV_KBD_DEVICE	1
#define APLHIDEV_TP_DEVICE	2
#define APLHIDEV_INFO_DEVICE	208

#define APLHIDEV_GET_DESCRIPTOR	0x1020
#define  APLHIDEV_DESC_MAX	512
#define APLHIDEV_KBD_REPORT	0x0110
#define APLHIDEV_TP_REPORT	0x0210

struct aplhidev_attach_args {
	uint8_t	aa_reportid;
	void	*aa_desc;
	size_t	aa_desclen;
};

struct aplhidev_spi_packet {
	uint8_t		flags;
	uint8_t		device;
	uint16_t	offset;
	uint16_t	remaining;
	uint16_t	len;
	uint8_t		data[246];
	uint16_t	crc;
};

struct aplhidev_spi_status {
	uint8_t		status[4];
};

struct aplhidev_msghdr {
	uint16_t	type;
	uint8_t		device;
	uint8_t		msgid;
	uint16_t	rsplen;
	uint16_t	cmdlen;
};

struct aplhidev_get_desc {
	struct aplhidev_msghdr	hdr;
	uint16_t		crc;
};

struct aplhidev_softc {
	struct device		sc_dev;
	int			sc_node;

	spi_tag_t		sc_spi_tag;
	struct spi_config	sc_spi_conf;

	uint8_t			sc_msgid;

	uint32_t		*sc_gpio;
	size_t			sc_gpiolen;

	struct device 		*sc_kbd;
	uint8_t			sc_kbddesc[APLHIDEV_DESC_MAX];
	size_t			sc_kbddesclen;

	struct device		*sc_ms;
	uint8_t			sc_tpdesc[APLHIDEV_DESC_MAX];
	size_t			sc_tpdesclen;
};

int	 aplhidev_match(struct device *, void *, void *);
void	 aplhidev_attach(struct device *, struct device *, void *);

struct cfattach aplhidev_ca = {
	sizeof(struct aplhidev_softc), aplhidev_match, aplhidev_attach
};

struct cfdriver aplhidev_cd = {
	NULL, "aplhidev", DV_DULL
};

void	aplhidev_get_descriptor(struct aplhidev_softc *, uint8_t);

int	aplhidev_intr(void *);
void	aplkbd_intr(struct device *, void *, size_t);
void	aplms_intr(struct device *, void *, size_t);

int
aplhidev_match(struct device *parent, void *match, void *aux)
{
	struct spi_attach_args *sa = aux;

	if (strcmp(sa->sa_name, "apple,keyboard") == 0)
		return 1;

	return 0;
}

void
aplhidev_attach(struct device *parent, struct device *self, void *aux)
{
	struct aplhidev_softc *sc = (struct aplhidev_softc *)self;
	struct spi_attach_args *sa = aux;
	struct aplhidev_attach_args aa;
	int retry;

	sc->sc_spi_tag = sa->sa_tag;
	sc->sc_node = *(int *)sa->sa_cookie;

	sc->sc_gpiolen = OF_getproplen(sc->sc_node, "spien-gpios");
	if (sc->sc_gpiolen > 0) {
		sc->sc_gpio = malloc(sc->sc_gpiolen, M_TEMP, M_WAITOK);
		OF_getpropintarray(sc->sc_node, "spien-gpios",
		    sc->sc_gpio, sc->sc_gpiolen);
		gpio_controller_config_pin(sc->sc_gpio, GPIO_CONFIG_OUTPUT);

		/* Reset */
		gpio_controller_set_pin(sc->sc_gpio, 1);
		delay(5000);
		gpio_controller_set_pin(sc->sc_gpio, 0);
		delay(5000);

		/* Enable. */
		gpio_controller_set_pin(sc->sc_gpio, 1);
		delay(50000);
	}

	sc->sc_spi_conf.sc_bpw = 8;
	sc->sc_spi_conf.sc_freq = OF_getpropint(sc->sc_node,
	    "spi-max-frequency", 0);
	sc->sc_spi_conf.sc_cs = OF_getpropint(sc->sc_node, "reg", 0);
	sc->sc_spi_conf.sc_cs_delay = 100;

	fdt_intr_establish(sc->sc_node, IPL_TTY,
	    aplhidev_intr, sc, sc->sc_dev.dv_xname);

	aplhidev_get_descriptor(sc, APLHIDEV_KBD_DEVICE);
	for (retry = 10; retry > 0; retry--) {
		aplhidev_intr(sc);
		delay(1000);
		if (sc->sc_kbddesclen > 0)
			break;
	}

	aplhidev_get_descriptor(sc, APLHIDEV_TP_DEVICE);
	for (retry = 10; retry > 0; retry--) {
		aplhidev_intr(sc);
		delay(1000);
		if (sc->sc_tpdesclen > 0)
			break;
	}

	printf("\n");

	if (sc->sc_kbddesclen > 0) {
		aa.aa_reportid = APLHIDEV_KBD_DEVICE;
		aa.aa_desc = sc->sc_kbddesc;
		aa.aa_desclen = sc->sc_kbddesclen;
		sc->sc_kbd = config_found(self, &aa, NULL);
	}

	if (sc->sc_tpdesclen > 0) {
		aa.aa_reportid = APLHIDEV_TP_DEVICE;
		aa.aa_desc = sc->sc_tpdesc;
		aa.aa_desclen = sc->sc_tpdesclen;
		sc->sc_ms = config_found(self, &aa, NULL);
	}
}

void
aplhidev_get_descriptor(struct aplhidev_softc *sc, uint8_t device)
{
	struct aplhidev_spi_packet packet;
	struct aplhidev_get_desc *msg;
	struct aplhidev_spi_status status;

	memset(&packet, 0, sizeof(packet));
	packet.flags = APLHIDEV_WRITE_PACKET;
	packet.device = APLHIDEV_INFO_DEVICE;
	packet.len = sizeof(*msg);

	msg = (void *)&packet.data[0];
	msg->hdr.type = APLHIDEV_GET_DESCRIPTOR;
	msg->hdr.device = device;
	msg->hdr.msgid = sc->sc_msgid++;
	msg->hdr.cmdlen = 0;
	msg->hdr.rsplen = APLHIDEV_DESC_MAX;
	msg->crc = crc16(0, (void *)msg, sizeof(*msg) - 2);

	packet.crc = crc16(0, (void *)&packet, sizeof(packet) - 2);

	spi_acquire_bus(sc->sc_spi_tag, 0);
	spi_config(sc->sc_spi_tag, &sc->sc_spi_conf);
	spi_transfer(sc->sc_spi_tag, (char *)&packet, NULL, sizeof(packet),
	    SPI_KEEP_CS);
	delay(100);
	spi_read(sc->sc_spi_tag, (char *)&status, sizeof(status));
	spi_release_bus(sc->sc_spi_tag, 0);

	delay(1000);
}

int
aplhidev_intr(void *arg)
{
	struct aplhidev_softc *sc = arg;
	struct aplhidev_spi_packet packet;
	struct aplhidev_msghdr *hdr = (struct aplhidev_msghdr *)&packet.data[0];

	memset(&packet, 0, sizeof(packet));
	spi_acquire_bus(sc->sc_spi_tag, 0);
	spi_config(sc->sc_spi_tag, &sc->sc_spi_conf);
	spi_read(sc->sc_spi_tag, (char *)&packet, sizeof(packet));
	spi_release_bus(sc->sc_spi_tag, 0);

	/* Treat empty packets as spurious interrupts. */
	if (packet.flags == 0 && packet.device == 0 && packet.crc == 0)
		return 0;

	if (crc16(0, (uint8_t *)&packet, sizeof(packet)))
		return 1;

	/* Keyboard input. */
	if (packet.flags == APLHIDEV_READ_PACKET &&
	    packet.device == APLHIDEV_KBD_DEVICE &&
	    hdr->type == APLHIDEV_KBD_REPORT) {
		if (sc->sc_kbd)
			aplkbd_intr(sc->sc_kbd, &packet.data[9], hdr->cmdlen - 1);
		return 1;
	}

	/* Touchpad input. */
	if (packet.flags == APLHIDEV_READ_PACKET &&
	    packet.device == APLHIDEV_TP_DEVICE &&
	    hdr->type == APLHIDEV_TP_REPORT) {
		if (sc->sc_ms)
			aplms_intr(sc->sc_ms, &packet.data[9], hdr->cmdlen - 1);
		return 1;
	}

	/* Replies to commands we sent. */
	if (packet.flags == APLHIDEV_WRITE_PACKET &&
	    packet.device == APLHIDEV_INFO_DEVICE &&
	    hdr->type == APLHIDEV_GET_DESCRIPTOR) {
		switch (hdr->device) {
		case APLHIDEV_KBD_DEVICE:
			memcpy(sc->sc_kbddesc, &packet.data[8], hdr->cmdlen);
			sc->sc_kbddesclen = hdr->cmdlen;
			break;
		case APLHIDEV_TP_DEVICE:
			memcpy(sc->sc_tpdesc, &packet.data[8], hdr->cmdlen);
			sc->sc_tpdesclen = hdr->cmdlen;
			break;
		}

		return 1;
	}

	/* Valid, but unrecognized packet; ignore for now. */
	return 1;
}

/* Keyboard */

struct aplkbd_softc {
	struct device	sc_dev;
	struct hidkbd	sc_kbd;
	int		sc_spl;
};

void	aplkbd_cngetc(void *, u_int *, int *);
void	aplkbd_cnpollc(void *, int);
void	aplkbd_cnbell(void *, u_int, u_int, u_int);

const struct wskbd_consops aplkbd_consops = {
	aplkbd_cngetc,
	aplkbd_cnpollc,
	aplkbd_cnbell,
};

int	aplkbd_enable(void *, int);
void	aplkbd_set_leds(void *, int);
int	aplkbd_ioctl(void *, u_long, caddr_t, int, struct proc *);

const struct wskbd_accessops aplkbd_accessops = {
	.enable = aplkbd_enable,
	.ioctl = aplkbd_ioctl,
	.set_leds = aplkbd_set_leds,
};

int	 aplkbd_match(struct device *, void *, void *);
void	 aplkbd_attach(struct device *, struct device *, void *);

struct cfattach aplkbd_ca = {
	sizeof(struct aplkbd_softc), aplkbd_match, aplkbd_attach
};

struct cfdriver aplkbd_cd = {
	NULL, "aplkbd", DV_DULL
};

int
aplkbd_match(struct device *parent, void *match, void *aux)
{
	struct aplhidev_attach_args *aa = (struct aplhidev_attach_args *)aux;

	return (aa->aa_reportid == APLHIDEV_KBD_DEVICE);
}

void
aplkbd_attach(struct device *parent, struct device *self, void *aux)
{
	struct aplkbd_softc *sc = (struct aplkbd_softc *)self;
	struct aplhidev_attach_args *aa = (struct aplhidev_attach_args *)aux;
	struct hidkbd *kbd = &sc->sc_kbd;

	if (hidkbd_attach(self, kbd, 1, 0, APLHIDEV_KBD_DEVICE,
	    aa->aa_desc, aa->aa_desclen))
		return;

	printf("\n");

	if (kbd->sc_console_keyboard) {
		extern struct wskbd_mapdata ukbd_keymapdata;

		ukbd_keymapdata.layout = KB_US | KB_DEFAULT;
		wskbd_cnattach(&aplkbd_consops, sc, &ukbd_keymapdata);
		aplkbd_enable(sc, 1);
	}

	hidkbd_attach_wskbd(kbd, KB_US | KB_DEFAULT, &aplkbd_accessops);
}

void
aplkbd_intr(struct device *self, void *report, size_t reportlen)
{
	struct aplkbd_softc *sc = (struct aplkbd_softc *)self;
	struct hidkbd *kbd = &sc->sc_kbd;

	if (kbd->sc_enabled)
		hidkbd_input(kbd, report, reportlen);
}

int
aplkbd_enable(void *v, int on)
{
	struct aplkbd_softc *sc = v;
	struct hidkbd *kbd = &sc->sc_kbd;

	return hidkbd_enable(kbd, on);
}

int
aplkbd_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct aplkbd_softc *sc = v;
	struct hidkbd *kbd = &sc->sc_kbd;

	switch (cmd) {
	case WSKBDIO_GTYPE:
		/* XXX: should we set something else? */
		*(u_int *)data = WSKBD_TYPE_USB;
		return 0;
	default:
		return hidkbd_ioctl(kbd, cmd, data, flag, p);
	}
}

void
aplkbd_set_leds(void *v, int leds)
{
}

/* Console interface. */
void
aplkbd_cngetc(void *v, u_int *type, int *data)
{
	struct aplkbd_softc *sc = v;
	struct hidkbd *kbd = &sc->sc_kbd;

	kbd->sc_polling = 1;
	while (kbd->sc_npollchar <= 0) {
		aplhidev_intr(sc->sc_dev.dv_parent);
		delay(1000);
	}
	kbd->sc_polling = 0;
	hidkbd_cngetc(kbd, type, data);
}

void
aplkbd_cnpollc(void *v, int on)
{
	struct aplkbd_softc *sc = v;

	if (on)
		sc->sc_spl = spltty();
	else
		splx(sc->sc_spl);
}

void
aplkbd_cnbell(void *v, u_int pitch, u_int period, u_int volume)
{
	hidkbd_bell(pitch, period, volume, 1);
}

#if NAPLMS > 0

/* Touchpad */

struct aplms_softc {
	struct device	sc_dev;
	struct hidms	sc_ms;
};

int	aplms_enable(void *);
void	aplms_disable(void *);
int	aplms_ioctl(void *, u_long, caddr_t, int, struct proc *);

const struct wsmouse_accessops aplms_accessops = {
	.enable = aplms_enable,
	.disable = aplms_disable,
	.ioctl = aplms_ioctl,
};

int	 aplms_match(struct device *, void *, void *);
void	 aplms_attach(struct device *, struct device *, void *);

struct cfattach aplms_ca = {
	sizeof(struct aplms_softc), aplms_match, aplms_attach
};

struct cfdriver aplms_cd = {
	NULL, "aplms", DV_DULL
};

int
aplms_match(struct device *parent, void *match, void *aux)
{
	struct aplhidev_attach_args *aa = (struct aplhidev_attach_args *)aux;

	return (aa->aa_reportid == APLHIDEV_TP_DEVICE);
}

void
aplms_attach(struct device *parent, struct device *self, void *aux)
{
	struct aplms_softc *sc = (struct aplms_softc *)self;
	struct aplhidev_attach_args *aa = (struct aplhidev_attach_args *)aux;
	struct hidms *ms = &sc->sc_ms;

	if (hidms_setup(self, ms, 0, APLHIDEV_TP_DEVICE,
	    aa->aa_desc, aa->aa_desclen))
		return;

	hidms_attach(ms, &aplms_accessops);
}

void
aplms_intr(struct device *self, void *report, size_t reportlen)
{
	struct aplms_softc *sc = (struct aplms_softc *)self;
	struct hidms *ms = &sc->sc_ms;

	if (ms->sc_enabled)
		hidms_input(ms, report, reportlen);
}

int
aplms_enable(void *v)
{
	struct aplms_softc *sc = v;
	struct hidms *ms = &sc->sc_ms;

	return hidms_enable(ms);
}

void
aplms_disable(void *v)
{
	struct aplms_softc *sc = v;
	struct hidms *ms = &sc->sc_ms;

	hidms_disable(ms);
}

int
aplms_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct aplms_softc *sc = v;
	struct hidms *ms = &sc->sc_ms;

	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		*(u_int *)data = WSMOUSE_TYPE_TOUCHPAD;
		return 0;
	default:
		return hidms_ioctl(ms, cmd, data, flag, p);
	}
}

#else

void
aplms_intr(struct device *self, void *report, size_t reportlen)
{
}

#endif
