/*	$OpenBSD: apldc.c,v 1.1 2022/08/31 14:47:22 kettenis Exp $	*/
/*
 * Copyright (c) 2022 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/evcount.h>
#include <sys/malloc.h>
#include <sys/timeout.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsmousevar.h>

#include <dev/hid/hid.h>
#include <dev/hid/hidkbdsc.h>
#include <dev/hid/hidmsvar.h>

#include <arm64/dev/rtkit.h>
#include <arm64/dev/simplebusvar.h>

#define DC_IRQ_MASK		0x0000
#define DC_IRQ_STAT		0x0004

#define DC_CONFIG_TX_THRESH	0x0000
#define DC_CONFIG_RX_THRESH	0x0004

#define DC_DATA_TX_FREE		0x0014
#define DC_DATA_RX8		0x001c
#define  DC_DATA_RX8_COUNT(d)	((d) & 0x7f)
#define  DC_DATA_RX8_DATA(d)	(((d) >> 8) & 0xff)
#define DC_DATA_RX32		0x0028
#define DC_DATA_RX_COUNT	0x002c

#define APLDC_MAX_INTR		32

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))

struct apldchidev_attach_args {
	const char *aa_name;
	void	*aa_desc;
	size_t	aa_desclen;
};

struct intrhand {
	int (*ih_func)(void *);
	void *ih_arg;
	int ih_ipl;
	int ih_irq;
	int ih_level;
	struct evcount ih_count;
	char *ih_name;
	void *ih_sc;
};

struct apldc_softc {
	struct simplebus_softc	sc_sbus;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	void			*sc_ih;
	struct intrhand		*sc_handlers[APLDC_MAX_INTR];
	struct interrupt_controller sc_ic;
};

int	apldc_match(struct device *, void *, void *);
void	apldc_attach(struct device *, struct device *, void *);

const struct cfattach apldc_ca = {
	sizeof (struct apldc_softc), apldc_match, apldc_attach
};

struct cfdriver apldc_cd = {
	NULL, "apldc", DV_DULL
};

int	apldc_intr(void *);
void	*apldc_intr_establish(void *, int *, int, struct cpu_info *,
	    int (*)(void *), void *, char *);
void	apldc_intr_enable(void *);
void	apldc_intr_disable(void *);
void	apldc_intr_barrier(void *);

int
apldc_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "apple,dockchannel");
}

void
apldc_attach(struct device *parent, struct device *self, void *aux)
{
	struct apldc_softc *sc = (struct apldc_softc *)self;
	struct fdt_attach_args *faa = aux;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	/* Disable and clear all interrupts. */
	HWRITE4(sc, DC_IRQ_MASK, 0);
	HWRITE4(sc, DC_IRQ_STAT, 0xffffffff);

	sc->sc_ih = fdt_intr_establish(faa->fa_node, IPL_TTY,
	    apldc_intr, sc, sc->sc_sbus.sc_dev.dv_xname);

	sc->sc_ic.ic_node = faa->fa_node;
	sc->sc_ic.ic_cookie = sc;
	sc->sc_ic.ic_establish = apldc_intr_establish;
	sc->sc_ic.ic_enable = apldc_intr_enable;
	sc->sc_ic.ic_disable = apldc_intr_disable;
	sc->sc_ic.ic_barrier = apldc_intr_barrier;
	fdt_intr_register(&sc->sc_ic);

	simplebus_attach(parent, &sc->sc_sbus.sc_dev, faa);
}

int
apldc_intr(void *arg)
{
	struct apldc_softc *sc = arg;
	struct intrhand *ih;
	uint32_t stat, pending;
	int irq, s;

	stat = HREAD4(sc, DC_IRQ_STAT);

	pending = stat;
	while (pending) {
		irq = ffs(pending) - 1;
		ih = sc->sc_handlers[irq];
		if (ih) {
			s = splraise(ih->ih_ipl);
			if (ih->ih_func(ih->ih_arg))
				ih->ih_count.ec_count++;
			splx(s);
		}

		pending &= ~(1 << irq);
	}

	HWRITE4(sc, DC_IRQ_STAT, stat);

	return 1;
}

void *
apldc_intr_establish(void *cookie, int *cells, int ipl,
    struct cpu_info *ci, int (*func)(void *), void *arg, char *name)
{
	struct apldc_softc *sc = cookie;
	struct intrhand *ih;
	int irq = cells[0];
	int level = cells[1];

	if (irq < 0 || irq >= APLDC_MAX_INTR)
		return NULL;

	if (ipl != IPL_TTY)
		return NULL;

	if (ci != NULL && !CPU_IS_PRIMARY(ci))
		return NULL;

	if (sc->sc_handlers[irq])
		return NULL;

	ih = malloc(sizeof(*ih), M_DEVBUF, M_WAITOK);
	ih->ih_func = func;
	ih->ih_arg = arg;
	ih->ih_ipl = ipl;
	ih->ih_irq = irq;
	ih->ih_name = name;
	ih->ih_level = level;
	ih->ih_sc = sc;

	sc->sc_handlers[irq] = ih;

	if (name != NULL)
		evcount_attach(&ih->ih_count, name, &ih->ih_irq);

	return ih;
}

void
apldc_intr_enable(void *cookie)
{
	struct intrhand	*ih = cookie;
	struct apldc_softc *sc = ih->ih_sc;

	HSET4(sc, DC_IRQ_MASK, 1 << ih->ih_irq);
}

void
apldc_intr_disable(void *cookie)
{
	struct intrhand	*ih = cookie;
	struct apldc_softc *sc = ih->ih_sc;

	HCLR4(sc, DC_IRQ_MASK, 1 << ih->ih_irq);
}

void
apldc_intr_barrier(void *cookie)
{
	struct intrhand	*ih = cookie;
	struct apldc_softc *sc = ih->ih_sc;

	intr_barrier(sc->sc_ih);
}

#define APLDCHIDEV_DESC_MAX	512
#define APLDCHIDEV_PKT_MAX	1024

struct apldchidev_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_cfg_ioh;
	bus_space_handle_t	sc_data_ioh;

	void			*sc_rx_ih;

	uint8_t			sc_iface_kbd;
	struct device		*sc_kbd;
	uint8_t			sc_kbddesc[APLDCHIDEV_DESC_MAX];
	size_t			sc_kbddesclen;
};

int	apldchidev_match(struct device *, void *, void *);
void	apldchidev_attach(struct device *, struct device *, void *);

const struct cfattach apldchidev_ca = {
	sizeof(struct apldchidev_softc), apldchidev_match, apldchidev_attach
};

struct cfdriver apldchidev_cd = {
	NULL, "apldchidev", DV_DULL
};

int	apldchidev_rx_intr(void *);

int
apldchidev_match(struct device *parent, void *cfdata, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "apple,dockchannel-hid");
}

void
apldchidev_attach(struct device *parent, struct device *self, void *aux)
{
	struct apldchidev_softc *sc = (struct apldchidev_softc *)self;
	struct fdt_attach_args *faa = aux;
	struct apldchidev_attach_args aa;
	uint32_t phandle;
	int error, idx, retry;

	if (faa->fa_nreg < 2) {
		printf(": no registers\n");
		return;
	}

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_cfg_ioh)) {
		printf(": can't map registers\n");
		return;
	}
	if (bus_space_map(sc->sc_iot, faa->fa_reg[1].addr,
	    faa->fa_reg[1].size, 0, &sc->sc_data_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	idx = OF_getindex(faa->fa_node, "rx", "interrupt-names");
	if (idx < 0) {
		printf(": no rx interrupt\n");
		return;
	}
	sc->sc_rx_ih = fdt_intr_establish_idx(faa->fa_node, idx, IPL_TTY,
	    apldchidev_rx_intr, sc, sc->sc_dev.dv_xname);
	if (sc->sc_rx_ih == NULL) {
		printf(": can't establish interrupt\n");
		return;
	}

	phandle = OF_getpropint(faa->fa_node, "apple,helper-cpu", 0);
	if (phandle) {
		error = aplrtk_start(phandle);
		if (error) {
			printf(": can't start helper CPU\n");
			return;
		}
	}

	printf("\n");

	/* Poll until we have received the keyboard HID descriptor. */
	for (retry = 10; retry > 0; retry--) {
		apldchidev_rx_intr(sc);
		delay(1000);
		if (sc->sc_kbddesclen > 0)
			break;
	}

	if (sc->sc_kbddesclen > 0) {
		aa.aa_name = "keyboard";
		aa.aa_desc = sc->sc_kbddesc;
		aa.aa_desclen = sc->sc_kbddesclen;
		sc->sc_kbd = config_found(self, &aa, NULL);
	}

	bus_space_write_4(sc->sc_iot, sc->sc_cfg_ioh, DC_CONFIG_RX_THRESH, 8);
	fdt_intr_enable(sc->sc_rx_ih);
}

int
apldchidev_read(struct apldchidev_softc *sc, void *buf, size_t len,
    uint32_t *checksum)
{
	uint8_t *dst = buf;
	uint32_t data;
	int shift = 0;

	while (len > 0) {
		data = bus_space_read_4(sc->sc_iot, sc->sc_data_ioh, DC_DATA_RX8);
		if (DC_DATA_RX8_COUNT(data) > 0) {
			*dst++ = DC_DATA_RX8_DATA(data);
			*checksum += (DC_DATA_RX8_DATA(data) << shift);
			shift += 8;
			if (shift > 24)
				shift = 0;
			len--;
		} else {
			delay(10);
		}
	}

	return 0;
}

struct apldc_hdr {
	uint8_t hdr_len;
	uint8_t chan;
	uint16_t pkt_len;
	uint8_t seq;
	uint8_t iface;
#define APLDC_IFACE_COMM	0
	uint16_t pad;
} __packed;


struct apldc_subhdr {
	uint8_t flags;
#define APLDC_GROUP(x)		((x >> 5) & 0x3)
#define APLDC_GROUP_INPUT	0
	uint8_t unk;
	uint16_t len;
	uint32_t retcode;
} __packed;

struct apldc_init_hdr {
	uint8_t type;
#define APLDC_EVENT_INIT	0xf0
	uint8_t unk1;
	uint8_t unk2;
	uint8_t iface;
	char name[16];
} __packed;

struct apldc_init_block_hdr {
	uint16_t type;
#define APLDC_BLOCK_DESCRIPTOR	0
#define APLDC_BLOCK_END		2
	uint16_t subtype;
	uint16_t len;
} __packed;

void
apldchidev_handle_init(struct apldchidev_softc *sc, void *buf, size_t len)
{
	struct apldc_init_block_hdr *bhdr = buf;

	for (;;) {
		if (len < sizeof(*bhdr))
			return;
		len -= sizeof(*bhdr);

		if (len < bhdr->len)
			return;
		len -= bhdr->len;

		switch (bhdr->type) {
		case APLDC_BLOCK_DESCRIPTOR:
			if (bhdr->len <= sizeof(sc->sc_kbddesc)) {
				memcpy(sc->sc_kbddesc, bhdr + 1, bhdr->len);
				sc->sc_kbddesclen = bhdr->len;
			}
			break;
		case APLDC_BLOCK_END:
			return;
		default:
			printf("%s: unhandled block type 0x%04x\n",
			    sc->sc_dev.dv_xname, bhdr->type);
			break;
		}

		bhdr = (struct apldc_init_block_hdr *)
		    ((uint8_t *)(bhdr + 1) + bhdr->len);
	}
}

void
apldchidev_handle_comm(struct apldchidev_softc *sc, void *buf, size_t len)
{
	struct apldc_init_hdr *ihdr = buf;

	switch (ihdr->type) {
	case APLDC_EVENT_INIT:
		if (strcmp(ihdr->name, "keyboard") == 0) {
			sc->sc_iface_kbd = ihdr->iface;
			apldchidev_handle_init(sc, ihdr + 1,
			    len - sizeof(*ihdr));
		}
		break;
	default:
		printf("%s: unhandled comm event 0x%02x\n",
		    sc->sc_dev.dv_xname, ihdr->type);
		break;
	}
}

void apldckbd_intr(struct device *, uint8_t *, size_t);

int
apldchidev_rx_intr(void *arg)
{
	struct apldchidev_softc *sc = arg;
	struct apldc_hdr hdr;
	struct apldc_subhdr *shdr;
	uint32_t checksum = 0;
	char buf[APLDCHIDEV_PKT_MAX];

	apldchidev_read(sc, &hdr, sizeof(hdr), &checksum);
	apldchidev_read(sc, buf, hdr.pkt_len + 4, &checksum);
	if (checksum != 0xffffffff)
		return 1;

	if (hdr.pkt_len < sizeof(*shdr))
		return 1;

	shdr = (struct apldc_subhdr *)buf;
	if (APLDC_GROUP(shdr->flags) != APLDC_GROUP_INPUT)
		return 1;

	if (hdr.iface == APLDC_IFACE_COMM)
		apldchidev_handle_comm(sc, shdr + 1, shdr->len);
	else if (hdr.iface == sc->sc_iface_kbd)
		apldckbd_intr(sc->sc_kbd, (uint8_t *)(shdr + 1), shdr->len);

	return 1;
}

/* Keyboard */

struct apldckbd_softc {
	struct device		sc_dev;
	struct apldchidev_softc	*sc_hidev;
	struct hidkbd		sc_kbd;
	int			sc_spl;
};

void	apldckbd_cngetc(void *, u_int *, int *);
void	apldckbd_cnpollc(void *, int);
void	apldckbd_cnbell(void *, u_int, u_int, u_int);

const struct wskbd_consops apldckbd_consops = {
	apldckbd_cngetc,
	apldckbd_cnpollc,
	apldckbd_cnbell,
};

int	apldckbd_enable(void *, int);
void	apldckbd_set_leds(void *, int);
int	apldckbd_ioctl(void *, u_long, caddr_t, int, struct proc *);

const struct wskbd_accessops apldckbd_accessops = {
	.enable = apldckbd_enable,
	.ioctl = apldckbd_ioctl,
	.set_leds = apldckbd_set_leds,
};

int	 apldckbd_match(struct device *, void *, void *);
void	 apldckbd_attach(struct device *, struct device *, void *);

const struct cfattach apldckbd_ca = {
	sizeof(struct apldckbd_softc), apldckbd_match, apldckbd_attach
};

struct cfdriver apldckbd_cd = {
	NULL, "apldckbd", DV_DULL
};

int
apldckbd_match(struct device *parent, void *match, void *aux)
{
	struct apldchidev_attach_args *aa = aux;

	return strcmp(aa->aa_name, "keyboard") == 0;
}

void
apldckbd_attach(struct device *parent, struct device *self, void *aux)
{
	struct apldckbd_softc *sc = (struct apldckbd_softc *)self;
	struct apldchidev_attach_args *aa = aux;
	struct hidkbd *kbd = &sc->sc_kbd;

#define APLHIDEV_KBD_DEVICE	1
	sc->sc_hidev = (struct apldchidev_softc *)parent;
	if (hidkbd_attach(self, kbd, 1, 0, APLHIDEV_KBD_DEVICE,
	    aa->aa_desc, aa->aa_desclen))
		return;

	printf("\n");

	if (kbd->sc_console_keyboard) {
		extern struct wskbd_mapdata ukbd_keymapdata;

		ukbd_keymapdata.layout = KB_US | KB_DEFAULT;
		wskbd_cnattach(&apldckbd_consops, sc, &ukbd_keymapdata);
		apldckbd_enable(sc, 1);
	}

	hidkbd_attach_wskbd(kbd, KB_US | KB_DEFAULT, &apldckbd_accessops);
}

void
apldckbd_intr(struct device *self, uint8_t *packet, size_t packetlen)
{
	struct apldckbd_softc *sc = (struct apldckbd_softc *)self;
	struct hidkbd *kbd = &sc->sc_kbd;

	if (kbd->sc_enabled)
		hidkbd_input(kbd, &packet[1], packetlen - 1);
}

int
apldckbd_enable(void *v, int on)
{
	struct apldckbd_softc *sc = v;
	struct hidkbd *kbd = &sc->sc_kbd;

	return hidkbd_enable(kbd, on);
}

int
apldckbd_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct apldckbd_softc *sc = v;
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
apldckbd_set_leds(void *v, int leds)
{
#if 0
	struct apldckbd_softc *sc = v;
	struct hidkbd *kbd = &sc->sc_kbd;
	uint8_t res;

	if (hidkbd_set_leds(kbd, leds, &res))
		aplhidev_set_leds(sc->sc_hidev, res);
#endif
}

/* Console interface. */
void
apldckbd_cngetc(void *v, u_int *type, int *data)
{
	struct apldckbd_softc *sc = v;
	struct hidkbd *kbd = &sc->sc_kbd;

	kbd->sc_polling = 1;
	while (kbd->sc_npollchar <= 0) {
		apldchidev_rx_intr(sc->sc_dev.dv_parent);
		delay(1000);
	}
	kbd->sc_polling = 0;
	hidkbd_cngetc(kbd, type, data);
}

void
apldckbd_cnpollc(void *v, int on)
{
	struct apldckbd_softc *sc = v;

	if (on)
		sc->sc_spl = spltty();
	else
		splx(sc->sc_spl);
}

void
apldckbd_cnbell(void *v, u_int pitch, u_int period, u_int volume)
{
	hidkbd_bell(pitch, period, volume, 1);
}
