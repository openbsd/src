/*	$OpenBSD: qcrtc.c,v 1.1 2022/11/08 19:47:05 patrick Exp $	*/
/*
 * Copyright (c) 2022 Patrick Wildt <patrick@blueri.se>
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

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/fdt/spmivar.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

#include <dev/clock_subr.h>

/* Registers. */
#define RTC_WRITE		0x40
#define RTC_CTRL		0x46
#define  RTC_CTRL_EN			(1U << 7)
#define RTC_READ		0x48

struct qcrtc_softc {
	struct device		sc_dev;
	int			sc_node;
	spmi_tag_t		sc_tag;
	int8_t			sc_sid;
	uint16_t		sc_addr;

	struct todr_chip_handle sc_todr;
};

int qcrtc_match(struct device *, void *, void *);
void qcrtc_attach(struct device *, struct device *, void *);

const struct cfattach	qcrtc_ca = {
	sizeof (struct qcrtc_softc), qcrtc_match, qcrtc_attach
};

struct cfdriver qcrtc_cd = {
	NULL, "qcrtc", DV_DULL
};

int	qcrtc_gettime(struct todr_chip_handle *, struct timeval *);
int	qcrtc_settime(struct todr_chip_handle *, struct timeval *);

void	qcrtc_tick(void *);

int
qcrtc_match(struct device *parent, void *match, void *aux)
{
	struct spmi_attach_args *saa = aux;

	return OF_is_compatible(saa->sa_node, "qcom,pmk8350-rtc");
}

void
qcrtc_attach(struct device *parent, struct device *self, void *aux)
{
	struct qcrtc_softc *sc = (struct qcrtc_softc *)self;
	struct spmi_attach_args *saa = aux;
	uint32_t reg[2];

	if (OF_getpropintarray(saa->sa_node, "reg",
	    reg, sizeof(reg)) != sizeof(reg)) {
		printf(": can't find registers\n");
		return;
	}

	sc->sc_node = saa->sa_node;
	sc->sc_tag = saa->sa_tag;
	sc->sc_sid = saa->sa_sid;
	sc->sc_addr = reg[0];

	printf("\n");

	sc->sc_todr.cookie = sc;
	sc->sc_todr.todr_gettime = qcrtc_gettime;
	sc->sc_todr.todr_settime = qcrtc_settime;
	sc->sc_todr.todr_quality = 0;
	todr_attach(&sc->sc_todr);
}

int
qcrtc_gettime(struct todr_chip_handle *handle, struct timeval *tv)
{
	struct qcrtc_softc *sc = handle->cookie;
	uint32_t reg;
	int error;

	error = spmi_cmd_read(sc->sc_tag, sc->sc_sid, SPMI_CMD_EXT_READL,
	    sc->sc_addr + RTC_READ, &reg, sizeof(reg));
	if (error) {
		printf("%s: error reading RTC\n", sc->sc_dev.dv_xname);
		return error;
	}

	tv->tv_sec = reg;
	tv->tv_usec = 0;

	/* We need to offset. */
	return EIO;
}

int
qcrtc_settime(struct todr_chip_handle *handle, struct timeval *tv)
{
	/* XXX: We can't set the time for now, hardware reasons. */
	return EPERM;
}
