/*	$NetBSD: umass_isdata.c,v 1.1 2001/12/24 13:43:25 augustss Exp $	*/

/*
 * TODO:
 *  get ATA registers on any kind of error
 *  implement more commands (what is needed)
 */

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: umass_isdata.c,v 1.1 2001/12/24 13:43:25 augustss Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/disklabel.h>
#include <sys/malloc.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#include <dev/usb/umassvar.h>
#include <dev/usb/umass_isdata.h>

int umass_wd_attach(struct umass_softc *);

#include <dev/ata/atareg.h>
#include <dev/ata/atavar.h>
#include <dev/ata/wdvar.h>
#include <dev/ic/wdcreg.h>

/* XXX move this */
struct isd200_config {
        uByte EventNotification;
        uByte ExternalClock;
        uByte ATAInitTimeout;
        uByte ATAMisc1;
#define ATATiming		0x0f
#define ATAPIReset		0x10
#define MasterSlaveSelection	0x20
#define ATAPICommandBlockSize	0xc0
        uByte ATAMajorCommand;
        uByte ATAMinorCommand;
	uByte ATAMisc2;
#define LastLUNIdentifier	0x07
#define DescriptOverride	0x08
#define ATA3StateSuspend	0x10
#define SkipDeviceBoot		0x20
#define ConfigDescriptor2	0x40
#define InitStatus		0x80
	uByte ATAMisc3;
#define SRSTEnable		0x01
};

struct uisdata_softc {
	struct umassbus_softc	base;

	struct ata_drive_datas	sc_drv_data;
	struct isd200_config	sc_isd_config;
	void			*sc_ata_bio;
	u_long			sc_skip;
};

#undef DPRINTF
#undef DPRINTFN
#ifdef UISDATA_DEBUG
#define DPRINTF(x)	if (uisdatadebug) logprintf x
#define DPRINTFN(n,x)	if (uisdatadebug>(n)) logprintf x
int	uisdatadebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

int  uisdata_bio(struct ata_drive_datas *, struct ata_bio *);
int  uisdata_bio1(struct ata_drive_datas *, struct ata_bio *);
void uisdata_reset_channel(struct ata_drive_datas *);
int  uisdata_exec_command(struct ata_drive_datas *, struct wdc_command *);
int  uisdata_get_params(struct ata_drive_datas *, u_int8_t, struct ataparams *);
int  uisdata_addref(struct ata_drive_datas *);
void uisdata_delref(struct ata_drive_datas *);
void uisdata_kill_pending(struct ata_drive_datas *);

void uisdata_bio_cb(struct umass_softc *, void *, int, int);
void uisdata_exec_cb(struct umass_softc *, void *, int, int);
int  uwdprint(void *, const char *);

const struct ata_bustype uisdata_bustype = {
	SCSIPI_BUSTYPE_ATA,
	uisdata_bio,
	uisdata_reset_channel,
	uisdata_exec_command,
	uisdata_get_params,
	uisdata_addref,
	uisdata_delref,
	uisdata_kill_pending,
};

struct ata_cmd {
	u_int8_t ac_signature0;
	u_int8_t ac_signature1;

	u_int8_t ac_action_select;
#define AC_ReadRegisterAccess		0x01
#define AC_NoDeviceSelectionBit		0x02
#define AC_NoBSYPollBit			0x04
#define AC_IgnorePhaseErrorBit		0x08
#define AC_IgnoreDeviceErrorBit		0x10

	u_int8_t ac_register_select;
#define AC_SelectAlternateStatus	0x01 /* R */
#define AC_SelectDeviceControl		0x01 /* W */
#define AC_SelectError			0x02 /* R */
#define AC_SelectFeatures		0x02 /* W */
#define AC_SelectSectorCount		0x04 /* RW */
#define AC_SelectSectorNumber		0x08 /* RW */
#define AC_SelectCylinderLow		0x10 /* RW */
#define AC_SelectCylinderHigh		0x20 /* RW */
#define AC_SelectDeviceHead		0x40 /* RW */
#define AC_SelectStatus			0x80 /* R */
#define AC_SelectCommand		0x80 /* W */

	u_int8_t ac_transfer_blocksize;

	u_int8_t ac_alternate_status;
#define ac_device_control ac_alternate_status
	u_int8_t ac_error;
#define ac_features ac_error

	u_int8_t ac_sector_count;
	u_int8_t ac_sector_number;
	u_int8_t ac_cylinder_low;
	u_int8_t ac_cylinder_high;
	u_int8_t ac_device_head;

	u_int8_t ac_status;
#define ac_command ac_status

	u_int8_t ac_reserved[3];
};

#define ATA_DELAY 10000 /* 10s for a drive I/O */

int
umass_isdata_attach(struct umass_softc *sc)
{
	usb_device_request_t req;
	usbd_status err;
	struct ata_device adev;
	struct uisdata_softc *scbus;
	struct isd200_config *cf;

	scbus = malloc(sizeof *scbus, M_DEVBUF, M_WAITOK | M_ZERO);
	sc->bus = &scbus->base;
	cf = &scbus->sc_isd_config;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = 0x02;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 2);
	USETW(req.wLength, sizeof *cf);

	err = usbd_do_request(sc->sc_udev, &req, cf);
	if (err)
		return (EIO);
	DPRINTF(("umass_wd_attach info:\n  EventNotification=0x%02x "
		 "ExternalClock=0x%02x ATAInitTimeout=0x%02x\n"
		 "  ATAMisc1=0x%02x ATAMajorCommand=0x%02x "
		 "ATAMinorCommand=0x%02x\n"
		 "  ATAMisc2=0x%02x ATAMisc3=0x%02x\n",
		 cf->EventNotification, cf->ExternalClock, cf->ATAInitTimeout,
		 cf->ATAMisc1, cf->ATAMajorCommand, cf->ATAMinorCommand,
		 cf->ATAMisc2, cf->ATAMisc3));

	memset(&adev, 0, sizeof(struct ata_device));
	adev.adev_bustype = &uisdata_bustype;
	adev.adev_channel = 1;	/* XXX */
	adev.adev_openings = 1;
	adev.adev_drv_data = &scbus->sc_drv_data;
	scbus->sc_drv_data.drive_flags = DRIVE_ATA;
	scbus->sc_drv_data.chnl_softc = sc;
	scbus->base.sc_child = config_found(&sc->sc_dev, &adev, uwdprint);

	return (0);
}


void
uisdata_bio_cb(struct umass_softc *sc, void *priv, int residue, int status)
{
	struct uisdata_softc *scbus = (struct uisdata_softc *)sc->bus;
	struct ata_bio *ata_bio = priv;
	int s;

	DPRINTF(("%s: residue=%d status=%d\n", __FUNCTION__, residue, status));

	s = splbio();
	scbus->sc_ata_bio = NULL;
	if (status != STATUS_CMD_OK)
		ata_bio->error = ERR_DF; /* ??? */
	else
		ata_bio->error = NOERROR;
	ata_bio->flags |= ATA_ITSDONE;

	ata_bio->blkdone += ata_bio->nblks;
	ata_bio->blkno += ata_bio->nblks;
	ata_bio->bcount -= ata_bio->nbytes;
	scbus->sc_skip += ata_bio->nbytes;
	if (residue != 0) {
		ata_bio->bcount += residue;
	} else if (ata_bio->bcount > 0) {
		DPRINTF(("%s: continue\n", __FUNCTION__));
		(void)uisdata_bio1(&scbus->sc_drv_data, ata_bio); /*XXX save drv*/
		splx(s);
		return;
	}

	if (ata_bio->flags & ATA_POLL) {
		DPRINTF(("%s: wakeup %p\n", __FUNCTION__, ata_bio));
		wakeup(ata_bio);
	} else {
		wddone(scbus->sc_drv_data.drv_softc);
	}
	splx(s);
}

int
uisdata_bio(struct ata_drive_datas *drv, struct ata_bio *ata_bio)
{
	struct umass_softc *sc = drv->chnl_softc;
	struct uisdata_softc *scbus = (struct uisdata_softc *)sc->bus;

	scbus->sc_skip = 0;
	return (uisdata_bio1(drv, ata_bio));
}

int
uisdata_bio1(struct ata_drive_datas *drv, struct ata_bio *ata_bio)
{
	struct umass_softc *sc = drv->chnl_softc;
	struct uisdata_softc *scbus = (struct uisdata_softc *)sc->bus;
	struct isd200_config *cf = &scbus->sc_isd_config;
	struct ata_cmd ata;
	u_int16_t cyl;
	u_int8_t head, sect;
	int dir;
	long nbytes;
	u_int nblks;

	DPRINTF(("%s\n", __FUNCTION__));
	/* XXX */

	if (ata_bio->flags & ATA_NOSLEEP) {
		printf("%s: ATA_NOSLEEP not supported\n", __FUNCTION__);
		ata_bio->error = TIMEOUT;
		ata_bio->flags |= ATA_ITSDONE;
		return (WDC_COMPLETE);
	}

	if (scbus->sc_ata_bio != NULL) {
		printf("%s: multiple uisdata_bio\n", __FUNCTION__);
		return (WDC_TRY_AGAIN);
	} else
		scbus->sc_ata_bio = ata_bio;

	if (ata_bio->flags & ATA_LBA) {
		sect = (ata_bio->blkno >> 0) & 0xff;
		cyl = (ata_bio->blkno >> 8) & 0xffff;
		head = (ata_bio->blkno >> 24) & 0x0f;
		head |= WDSD_LBA;
	} else {
		int blkno = ata_bio->blkno;
		sect = blkno % ata_bio->lp->d_nsectors;
		sect++;    /* Sectors begin with 1, not 0. */
		blkno /= ata_bio->lp->d_nsectors;
		head = blkno % ata_bio->lp->d_ntracks;
		blkno /= ata_bio->lp->d_ntracks;
		cyl = blkno;
		head |= WDSD_CHS;
	}

	nbytes = ata_bio->bcount;
	if (ata_bio->flags & ATA_SINGLE)
		nblks = 1;
	else 
		nblks = min(ata_bio->multi, nbytes / ata_bio->lp->d_secsize);
	nbytes = nblks * ata_bio->lp->d_secsize;
	ata_bio->nblks = nblks;
	ata_bio->nbytes = nbytes;

	memset(&ata, 0, sizeof ata);
	ata.ac_signature0 = cf->ATAMajorCommand;
	ata.ac_signature1 = cf->ATAMinorCommand;
	ata.ac_transfer_blocksize = 1;
	ata.ac_sector_count = nblks;
	ata.ac_sector_number = sect;
	ata.ac_cylinder_high = cyl >> 8;
	ata.ac_cylinder_low = cyl;
	ata.ac_device_head = head;
	ata.ac_register_select = AC_SelectSectorCount | AC_SelectSectorNumber |
	    AC_SelectCylinderLow | AC_SelectCylinderHigh | AC_SelectDeviceHead |
	    AC_SelectCommand;

	dir = DIR_NONE;
	if (ata_bio->bcount != 0) {
		if (ata_bio->flags & ATA_READ)
			dir = DIR_IN;
		else
			dir = DIR_OUT;
	}

	if (ata_bio->flags & ATA_READ) {
		ata.ac_command = WDCC_READ;
	} else {
		ata.ac_command = WDCC_WRITE;
	}
	DPRINTF(("%s: bno=%d LBA=%d cyl=%d head=%d sect=%d count=%d multi=%d\n",
		 __FUNCTION__, ata_bio->blkno,
		 (ata_bio->flags & ATA_LBA) != 0, cyl, head, sect, 
		 ata.ac_sector_count, ata_bio->multi));
	DPRINTF(("    data=%p bcount=%ld, drive=%d\n", ata_bio->databuf,
		 ata_bio->bcount, drv->drive));
	sc->sc_methods->wire_xfer(sc, drv->drive, &ata, sizeof ata,
				  ata_bio->databuf + scbus->sc_skip, nbytes,
				  dir, ATA_DELAY, uisdata_bio_cb, ata_bio);

	while (ata_bio->flags & ATA_POLL) {
		DPRINTF(("%s: tsleep %p\n", __FUNCTION__, ata_bio));
		if (tsleep(ata_bio, PZERO, "uisdatabl", 0)) {
			ata_bio->error = TIMEOUT;
			ata_bio->flags |= ATA_ITSDONE;
			return (WDC_COMPLETE);
		}
	}

	return (ata_bio->flags & ATA_ITSDONE) ? WDC_COMPLETE : WDC_QUEUED;
}

void
uisdata_reset_channel(struct ata_drive_datas *drv)
{
	DPRINTFN(-1,("%s\n", __FUNCTION__));
	/* XXX what? */
}

void
uisdata_exec_cb(struct umass_softc *sc, void *priv, int residue, int status)
{
	struct wdc_command *cmd = priv;

	DPRINTF(("%s: status=%d\n", __FUNCTION__, status));
	if (status != STATUS_CMD_OK)
		cmd->flags |= AT_DF; /* XXX */
	cmd->flags |= AT_DONE;
	if (cmd->flags & (AT_POLL | AT_WAIT)) {
		DPRINTF(("%s: wakeup %p\n", __FUNCTION__, cmd));
		wakeup(cmd);
	}
}

int
uisdata_exec_command(struct ata_drive_datas *drv, struct wdc_command *cmd)
{
	struct umass_softc *sc = drv->chnl_softc;
	struct uisdata_softc *scbus = (struct uisdata_softc *)sc->bus;
	struct isd200_config *cf = &scbus->sc_isd_config;
	int dir;
	struct ata_cmd ata;

	DPRINTF(("%s\n", __FUNCTION__));
	DPRINTF(("  r_command=0x%02x timeout=%d flags=0x%x bcount=%d\n",
		 cmd->r_command, cmd->timeout, cmd->flags, cmd->bcount));

	dir = DIR_NONE;
	if (cmd->bcount != 0) {
		if (cmd->flags & AT_READ)
			dir = DIR_IN;
		else
			dir = DIR_OUT;
	}
	
	if (cmd->bcount > UMASS_MAX_TRANSFER_SIZE) {
		printf("uisdata_exec_command: large datalen %d\n", cmd->bcount);
		cmd->flags |= AT_ERROR;
		goto done;
	}

	memset(&ata, 0, sizeof ata);
	ata.ac_signature0 = cf->ATAMajorCommand;
	ata.ac_signature1 = cf->ATAMinorCommand;
	ata.ac_transfer_blocksize = 1;

	switch (cmd->r_command) {
	case WDCC_IDENTIFY:
		ata.ac_register_select |= AC_SelectCommand;
		ata.ac_command = WDCC_IDENTIFY;
		break;
	default:
		printf("uisdata_exec_command: bad command 0x%02x\n",
		       cmd->r_command);
		cmd->flags |= AT_ERROR;
		goto done;
	}

	DPRINTF(("%s: execute ATA command 0x%02x, drive=%d\n", __FUNCTION__,
		 ata.ac_command, drv->drive));
	sc->sc_methods->wire_xfer(sc, drv->drive, &ata,
				  sizeof ata, cmd->data, cmd->bcount, dir,
				  cmd->timeout, uisdata_exec_cb, cmd);
	if (cmd->flags & (AT_POLL | AT_WAIT)) {
#if 0
		if (cmd->flags & AT_POLL)
			printf("%s: AT_POLL not supported\n", __FUNCTION__);
#endif
		DPRINTF(("%s: tsleep %p\n", __FUNCTION__, cmd));
		if (tsleep(cmd, PZERO, "uisdataex", 0)) {
			cmd->flags |= AT_ERROR;
			goto done;
		}
	}

done:
	return (WDC_COMPLETE);
}

int
uisdata_addref(struct ata_drive_datas *drv)
{
	DPRINTF(("%s\n", __FUNCTION__));
	/* Nothing to do */
	return (0);
}

void
uisdata_delref(struct ata_drive_datas *drv)
{
	DPRINTF(("%s\n", __FUNCTION__));
	/* Nothing to do */
}

void
uisdata_kill_pending(struct ata_drive_datas *drv)
{
	struct umass_softc *sc = drv->chnl_softc;
	struct uisdata_softc *scbus = (struct uisdata_softc *)sc->bus;
	struct ata_bio *ata_bio = scbus->sc_ata_bio;

	DPRINTFN(-1,("%s\n", __FUNCTION__));

	if (ata_bio == NULL)
		return;
	scbus->sc_ata_bio = NULL;
	ata_bio->flags |= ATA_ITSDONE;
	ata_bio->error = ERR_NODEV;
	ata_bio->r_error = WDCE_ABRT;
	wddone(scbus->sc_drv_data.drv_softc);
}

int
uisdata_get_params(struct ata_drive_datas *drvp, u_int8_t flags,
		struct ataparams *prms)
{
	char tb[DEV_BSIZE];
	struct wdc_command wdc_c;

#if BYTE_ORDER == LITTLE_ENDIAN
	int i;
	u_int16_t *p;
#endif

	DPRINTF(("%s\n", __FUNCTION__));

	memset(tb, 0, DEV_BSIZE);
	memset(prms, 0, sizeof(struct ataparams));
	memset(&wdc_c, 0, sizeof(struct wdc_command));

	wdc_c.r_command = WDCC_IDENTIFY;
	wdc_c.timeout = 1000; /* 1s */
	wdc_c.flags = AT_READ | flags;
	wdc_c.data = tb;
	wdc_c.bcount = DEV_BSIZE;
	if (uisdata_exec_command(drvp, &wdc_c) != WDC_COMPLETE) {
		DPRINTF(("uisdata_get_parms: wdc_exec_command failed\n"));
		return (CMD_AGAIN);
	}
	if (wdc_c.flags & (AT_ERROR | AT_TIMEOU | AT_DF)) {
		DPRINTF(("uisdata_get_parms: wdc_c.flags=0x%x\n",
			 wdc_c.flags));
		return (CMD_ERR);
	} else {
		/* Read in parameter block. */
		memcpy(prms, tb, sizeof(struct ataparams));
#if BYTE_ORDER == LITTLE_ENDIAN
		/* XXX copied from ata.c */
		/*
		 * Shuffle string byte order.
		 * ATAPI Mitsumi and NEC drives don't need this.
		 */
		if ((prms->atap_config & WDC_CFG_ATAPI_MASK) ==
		    WDC_CFG_ATAPI &&
		    ((prms->atap_model[0] == 'N' &&
			prms->atap_model[1] == 'E') ||
		     (prms->atap_model[0] == 'F' &&
			 prms->atap_model[1] == 'X')))
			return 0;
		for (i = 0; i < sizeof(prms->atap_model); i += 2) {
			p = (u_short *)(prms->atap_model + i);
			*p = ntohs(*p);
		}
		for (i = 0; i < sizeof(prms->atap_serial); i += 2) {
			p = (u_short *)(prms->atap_serial + i);
			*p = ntohs(*p);
		}
		for (i = 0; i < sizeof(prms->atap_revision); i += 2) {
			p = (u_short *)(prms->atap_revision + i);
			*p = ntohs(*p);
		}
#endif
		return CMD_OK;
	}
}


/* XXX join with wdc.c routine? */
int
uwdprint(void *aux, const char *pnp)
{
	//struct ata_device *adev = aux;
	if (pnp)
		printf("wd at %s", pnp);
#if 0
	printf(" channel %d drive %d", adev->adev_channel,
	    adev->adev_drv_data->drive);
#endif
	return (UNCONF);
}


#if 0

int umass_wd_attach(struct umass_softc *);

#if NWD > 0
	case UMASS_CPROTO_ISD_ATA:
		return (umass_wd_attach(sc));
#endif

#endif
