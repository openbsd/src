/*	$OpenBSD: vmd.c,v 1.115 2019/08/14 07:34:49 anton Exp $	*/

/*
 * Copyright (c) 2015 Reyk Floeter <reyk@openbsd.org>
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
/* cmpe */

/* taken from vmd.c */
#include <sys/param.h>	/* nitems */
#include <sys/queue.h>
#include <sys/wait.h>
#include <sys/cdefs.h>
#include <sys/stat.h>
#include <sys/tty.h>
#include <sys/ttycom.h>
#include <sys/ioctl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>
#include <util.h>
#include <ctype.h>
#include <pwd.h>
#include <grp.h>

#include <machine/specialreg.h>
#include <machine/vmmvar.h>

#include "proc.h"
#include "atomicio.h"
#include "vmd.h"

/* taken from viomb.h */
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/task.h>
#include <sys/pool.h>
#include <sys/sensors.h>

#include <uvm/uvm_extern.h>

#include <dev/pv/virtioreg.h>
#include <dev/pv/virtiovar.h>

struct balloon_req {
	bus_dmamap_t	 bl_dmamap;
	struct pglist	 bl_pglist;
	int		 bl_nentries;
	u_int32_t	*bl_pages;
};

struct viombh_softc {
	struct device		sc_dev;
	struct virtio_softc	*sc_virtio;
	struct virtqueue	sc_vq[3]; // three virtqueue for inflate, deflate and stats.
	u_int32_t		sc_npages; /* desired pages */
	u_int32_t		sc_actual; /* current pages */
	struct balloon_req	sc_req;
	struct taskq		*sc_taskq;
	struct task		sc_task;
	struct pglist		sc_balloon_pages;
	/*struct ksensor		sc_sens[2];
	struct ksensordev	sc_sensdev;*/
};

int	viombh_match(struct device *, void *, void *);
void	viombh_attach(struct device *, struct device *, void *);

int
viombh_match(struct device *parent, void *match, void *aux)
{
	struct virtio_softc *va = aux;
	if (va->sc_childdevid == PCI_PRODUCT_QUMRANET_VIO_MEM)
		return (1);
	return (0);
}

void
viombh_attach(struct device *parent, struct device *self, void *aux)
{
	struct viombh_softc *sc = (struct viombh_softc *)self;
	struct virtio_softc *vsc = (struct virtio_softc *)parent;  //virtio is the parent	
}








































