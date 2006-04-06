/* $OpenBSD: mfi.c,v 1.1 2006/04/06 20:22:53 marco Exp $ */
/*
 * Copyright (c) 2006 Marco Peereboom <marco@peereboom.us>
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

#include "bio.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/lock.h>

#include <machine/bus.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>

#include <dev/ic/mfireg.h>
#include <dev/ic/mfivar.h>

#if NBIO > 0
#include <dev/biovar.h>
#endif

struct cfdriver mfi_cd = {
	NULL, "mfi", DV_DULL
};

int	mfi_scsi_cmd(struct scsi_xfer *);
int	mfi_scsi_ioctl(struct scsi_link *, u_long, caddr_t, int, struct proc *);
void	mfiminphys(struct buf *bp);

struct scsi_adapter mfi_switch = {
	mfi_scsi_cmd, mfiminphys, 0, 0, mfi_scsi_ioctl
};

struct scsi_device mfi_dev = {
	NULL, NULL, NULL, NULL
};

void
mfiminphys(struct buf *bp)
{
#define MFI_MAXFER 4096
	if (bp->b_bcount > MFI_MAXFER)
		bp->b_bcount = MFI_MAXFER;
	minphys(bp);
}

int
mfi_attach(struct mfi_softc *sc)
{
	return (1);
}

int
mfi_intr(void *v)
{
	return (0); /* XXX unclaimed */
}

int
mfi_scsi_ioctl(struct scsi_link *link, u_long cmd, caddr_t addr, int flag,
    struct proc *p)
{
#if 0
	struct ami_softc *sc = (struct ami_softc *)link->adapter_softc;

	if (sc->sc_ioctl)
		return (sc->sc_ioctl(link->adapter_softc, cmd, addr));
	else
		return (ENOTTY);
#endif
		return (ENOTTY);
}

int
mfi_scsi_cmd(struct scsi_xfer *xs)
{
#if 0
	struct scsi_link *link = xs->sc_link;
	struct ami_softc *sc = link->adapter_softc;
	struct device *dev = link->device_softc;
#endif
	return (0);
}
