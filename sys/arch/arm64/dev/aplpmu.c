/*	$OpenBSD: aplpmu.c,v 1.2 2021/05/27 08:10:12 kettenis Exp $	*/
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
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/clock_subr.h>
#include <dev/fdt/spmivar.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

/*
 * This driver is based on preliminary device tree bindings and will
 * almost certainly need changes once the official bindings land in
 * mainline Linux.  Support for these preliminary bindings will be
 * dropped as soon as official bindings are available.
 */

/*
 * Apple's "sera" PMU contains an RTC that provides time in 32.16
 * fixed-point format as well as a time offset in 33.15 fixed-point
 * format.  The sum of the two gives us a standard Unix timestamp with
 * sub-second resolution.  The time itself is read-only.  To set the
 * time we need to adjust the time offset.
 */
#define SERA_TIME		0xd002
#define SERA_TIME_OFFSET	0xd100
#define SERA_TIME_LEN		6

struct aplpmu_softc {
	struct device		sc_dev;
	spmi_tag_t		sc_tag;
	int8_t			sc_sid;

	struct todr_chip_handle sc_todr;
	uint64_t		sc_offset;
};

int	aplpmu_match(struct device *, void *, void *);
void	aplpmu_attach(struct device *, struct device *, void *);

struct cfattach	aplpmu_ca = {
	sizeof (struct aplpmu_softc), aplpmu_match, aplpmu_attach
};

struct cfdriver aplpmu_cd = {
	NULL, "aplpmu", DV_DULL
};

int	aplpmu_gettime(struct todr_chip_handle *, struct timeval *);
int	aplpmu_settime(struct todr_chip_handle *, struct timeval *);

int
aplpmu_match(struct device *parent, void *match, void *aux)
{
	struct spmi_attach_args *sa = aux;

	return OF_is_compatible(sa->sa_node, "apple,sera-pmu");
}

void
aplpmu_attach(struct device *parent, struct device *self, void *aux)
{
	struct aplpmu_softc *sc = (struct aplpmu_softc *)self;
	struct spmi_attach_args *sa = aux;
	uint8_t data[8] = {};
	int error;

	sc->sc_tag = sa->sa_tag;
	sc->sc_sid = sa->sa_sid;

	error = spmi_cmd_read(sc->sc_tag, sc->sc_sid, SPMI_CMD_EXT_READL,
	    SERA_TIME_OFFSET, &data, SERA_TIME_LEN);
	if (error) {
		printf(": can't read offset\n");
		return;
	}
	sc->sc_offset = lemtoh64(data);

	printf("\n");

	sc->sc_todr.cookie = sc;
	sc->sc_todr.todr_gettime = aplpmu_gettime;
	sc->sc_todr.todr_settime = aplpmu_settime;
	todr_attach(&sc->sc_todr);
}

int
aplpmu_gettime(struct todr_chip_handle *handle, struct timeval *tv)
{
	struct aplpmu_softc *sc = handle->cookie;
	uint8_t data[8] = {};
	uint64_t time;
	int error;

	error = spmi_cmd_read(sc->sc_tag, sc->sc_sid, SPMI_CMD_EXT_READL,
	    SERA_TIME, &data, SERA_TIME_LEN);
	if (error)
		return error;
	time = lemtoh64(data) + (sc->sc_offset << 1);

	tv->tv_sec = (time >> 16);
	tv->tv_usec = (((time & 0xffff) * 1000000) >> 16);
	return 0;
}

int
aplpmu_settime(struct todr_chip_handle *handle, struct timeval *tv)
{
	struct aplpmu_softc *sc = handle->cookie;
	uint8_t data[8] = {};
	uint64_t time;
	int error;

	error = spmi_cmd_read(sc->sc_tag, sc->sc_sid, SPMI_CMD_EXT_READL,
	    SERA_TIME, &data, SERA_TIME_LEN);
	if (error)
		return error;

	time = ((uint64_t)tv->tv_sec << 16);
	time |= ((uint64_t)tv->tv_usec << 16) / 1000000;
	sc->sc_offset = ((time - lemtoh64(data)) >> 1);

	htolem64(data, sc->sc_offset);
	return spmi_cmd_write(sc->sc_tag, sc->sc_sid, SPMI_CMD_EXT_WRITEL,
	    SERA_TIME_OFFSET, &data, SERA_TIME_LEN);
}
