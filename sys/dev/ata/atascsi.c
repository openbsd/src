/*	$OpenBSD: atascsi.c,v 1.1 2007/02/19 11:44:24 dlg Exp $ */

/*
 * Copyright (c) 2007 David Gwynne <dlg@openbsd.org>
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
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/queue.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/ata/atascsi.h>

struct atascsi {
	struct device		*ab_dev;
	void			*ab_cookie;

	int			*ab_ports;

	struct atascsi_methods	*ab_methods;
	struct scsi_adapter	ab_switch;
	struct scsi_link	ab_link;
	struct scsibus_softc	*ab_scsibus;
};

int		atascsi_probe(struct atascsi *, int);

int		atascsi_cmd(struct scsi_xfer *);

/* template */
struct scsi_adapter atascsi_switch = {
	atascsi_cmd,		/* scsi_cmd */
	minphys,		/* scsi_minphys */
	NULL,
	NULL,
	NULL			/* ioctl */
};

struct scsi_device atascsi_device = {
	NULL, NULL, NULL, NULL
};

struct atascsi *
atascsi_attach(struct device *self, struct atascsi_attach_args *aaa)
{
	struct scsibus_attach_args	saa;
	struct atascsi			*ab;
	int				i;

	ab = malloc(sizeof(struct atascsi), M_DEVBUF, M_WAITOK);
	bzero(ab, sizeof(struct atascsi));

	ab->ab_dev = self;
	ab->ab_cookie = aaa->aaa_cookie;
	ab->ab_methods = aaa->aaa_methods;

	/* copy from template and modify for ourselves */
	ab->ab_switch = atascsi_switch;
	ab->ab_switch.scsi_minphys = aaa->aaa_minphys;

	/* fill in our scsi_link */
	ab->ab_link.device = &atascsi_device;
	ab->ab_link.adapter = &ab->ab_switch;
	ab->ab_link.adapter_softc = ab;
	ab->ab_link.adapter_buswidth = aaa->aaa_nports;
	ab->ab_link.luns = 1; /* XXX port multiplier as luns */
	ab->ab_link.adapter_target = aaa->aaa_nports;
	ab->ab_link.openings = aaa->aaa_ncmds;

	ab->ab_ports = malloc(sizeof(int) * aaa->aaa_nports,
	    M_DEVBUF, M_WAITOK);
	bzero(ab->ab_ports, sizeof(int) * aaa->aaa_nports);

	/* fill in the port array with the type of devices there */
	for (i = 0; i < ab->ab_link.adapter_buswidth; i++)
		atascsi_probe(ab, i);

	bzero(&saa, sizeof(saa));
	saa.saa_sc_link = &ab->ab_link;

	/* stash the scsibus so we can do hotplug on it */
	ab->ab_scsibus = (struct scsibus_softc *)config_found(self, &saa,
            scsiprint);

	return (ab);
}

int
atascsi_detach(struct atascsi *ab)
{
	return (0);
}

int
atascsi_probe(struct atascsi *ab, int port)
{
	if (port > ab->ab_link.adapter_buswidth)
		return (ENXIO);

	ab->ab_ports[port] = ab->ab_methods->probe(ab->ab_cookie, port);

	return (0);
}

int
atascsi_cmd(struct scsi_xfer *xs)
{
	int			s;

	xs->error = XS_DRIVER_STUFFUP;
	s = splbio();
	scsi_done(xs);
	splx(s);
	return (COMPLETE);
}
