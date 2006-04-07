/* $OpenBSD: mfi.c,v 1.3 2006/04/07 16:11:21 marco Exp $ */
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

#define MFI_DEBUG

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

int	mfi_transition_firmware(struct mfi_softc *);

int
mfi_transition_firmware(struct mfi_softc *sc)
{
	int32_t fw_state, cur_state;
	int max_wait, i;

#ifdef MFI_DEBUG
		printf("%s: mfi_transition_mfi\n", DEVNAME(sc));
#endif /* MFI_DEBUG */

	fw_state = bus_space_read_4(sc->sc_iot, sc->sc_ioh, MFI_OMSG0) &
	    MFI_STATE_MASK;
	while (fw_state != MFI_STATE_READY) {
#ifdef MFI_DEBUG
		printf("%s: waiting for firmware to become ready\n",
		    DEVNAME(sc));
#endif /* MFI_DEBUG */
		cur_state = fw_state;
		switch (fw_state) {
		case MFI_STATE_FAULT:
			printf("%s: firmware fault\n", DEVNAME(sc));
			return (ENXIO);
		case MFI_STATE_WAIT_HANDSHAKE:
			bus_space_write_4(sc->sc_iot, sc->sc_ioh,
			    MFI_IDB, MFI_INIT_CLEAR_HANDSHAKE);
			max_wait = 2;
			break;
		case MFI_STATE_OPERATIONAL:
			bus_space_write_4(sc->sc_iot, sc->sc_ioh,
			    MFI_IDB, MFI_INIT_READY);
			max_wait = 10;
			break;
		case MFI_STATE_UNDEFINED:
		case MFI_STATE_BB_INIT:
			max_wait = 2;
			break;
		case MFI_STATE_FW_INIT:
		case MFI_STATE_DEVICE_SCAN:
		case MFI_STATE_FLUSH_CACHE:
			max_wait = 20;
			break;
		default:
			printf("%s: unknown firmware state %d\n",
			    DEVNAME(sc), fw_state);
			return (ENXIO);
		}
		for (i = 0; i < (max_wait * 10); i++) {
			fw_state = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
			    MFI_OMSG0) & MFI_STATE_MASK;
			if (fw_state == cur_state)
				DELAY(100000);
			else
				break;
		}
		if (fw_state == cur_state) {
			printf("%s: firmware stuck in state %#x\n", fw_state,
			    DEVNAME(sc));
			return (ENXIO);
		}
	}

	return (0);
}

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
	if (mfi_transition_firmware(sc))
		return (1);

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
