/*	$OpenBSD: prtc.c,v 1.2 2008/07/10 08:58:00 kettenis Exp $	*/

/*
 * Copyright (c) 2008 Mark Kettenis
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

/*
 * Driver to get the time-of-day from the PROM for machines that don't
 * have a hardware real-time clock, like the Enterprise 10000,
 * Fire 12K and Fire 15K.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <machine/autoconf.h>
#include <machine/openfirm.h>
#include <machine/sparc64.h>

#include <dev/clock_subr.h>

extern todr_chip_handle_t todr_handle;

int	prtc_match(struct device *, void *, void *);
void	prtc_attach(struct device *, struct device *, void *);

struct cfattach prtc_ca = {
	sizeof(struct device), prtc_match, prtc_attach
};

struct cfdriver prtc_cd = {
	NULL, "prtc", DV_DULL
};

int	prtc_gettime(todr_chip_handle_t, struct timeval *);
int	prtc_settime(todr_chip_handle_t, struct timeval *);

int
prtc_match(struct device *parent, void *match, void *aux)
{
	struct mainbus_attach_args *ma = aux;

	if (strcmp(ma->ma_name, "prtc") == 0)
		return (1);

	return (0);
}

void
prtc_attach(struct device *parent, struct device *self, void *aux)
{
	todr_chip_handle_t handle;

	printf("\n");

	handle = malloc(sizeof(struct todr_chip_handle), M_DEVBUF, M_NOWAIT);
	if (handle == NULL)
		panic("couldn't allocate todr_handle");

	handle->cookie = self;
	handle->todr_gettime = prtc_gettime;
	handle->todr_settime = prtc_settime;

	handle->bus_cookie = NULL;
	handle->todr_setwen = NULL;
	todr_handle = handle;
}

int
prtc_gettime(todr_chip_handle_t handle, struct timeval *tv)
{
	u_int32_t tod = 0;
	char buf[32];

	if (OF_getprop(findroot(), "name", buf, sizeof(buf)) > 0 &&
	    strcmp(buf, "SUNW,SPARC-Enterprise") == 0) {
		tv->tv_sec = prom_opl_get_tod();
		tv->tv_usec = 0;
		return (0);
	}

	snprintf(buf, sizeof(buf), "h# %08x unix-gettod", &tod);
	OF_interpret(buf, 0);

	tv->tv_sec = tod;
	tv->tv_usec = 0;
	return (0);
}

int
prtc_settime(todr_chip_handle_t handle, struct timeval *tv)
{
	return (0);
}		
