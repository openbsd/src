/*	$OpenBSD: scsiconf.c,v 1.168 2011/03/17 21:30:24 deraadt Exp $	*/
/*	$NetBSD: scsiconf.c,v 1.57 1996/05/02 01:09:01 neil Exp $	*/

/*
 * Copyright (c) 1994 Charles Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Originally written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 */

#include "bio.h"
#include "mpath.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/lock.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#if NBIO > 0
#include <sys/ioctl.h>
#include <sys/scsiio.h>
#include <dev/biovar.h>
#endif

/*
 * Declarations
 */
int	scsi_probedev(struct scsibus_softc *, int, int);

void	scsi_devid(struct scsi_link *);
int	scsi_devid_pg83(struct scsi_link *);

int	scsibusmatch(struct device *, void *, void *);
void	scsibusattach(struct device *, struct device *, void *);
int	scsibusactivate(struct device *, int);
int	scsibusdetach(struct device *, int);

int	scsibussubmatch(struct device *, void *, void *);

#if NBIO > 0
int	scsibus_bioctl(struct device *, u_long, caddr_t);
#endif

struct cfattach scsibus_ca = {
	sizeof(struct scsibus_softc), scsibusmatch, scsibusattach,
	scsibusdetach, scsibusactivate
};

struct cfdriver scsibus_cd = {
	NULL, "scsibus", DV_DULL
};

#ifdef SCSIDEBUG
int scsidebug_buses = SCSIDEBUG_BUSES;
int scsidebug_targets = SCSIDEBUG_TARGETS;
int scsidebug_luns = SCSIDEBUG_LUNS;
int scsidebug_level = SCSIDEBUG_LEVEL;
#endif

int scsi_autoconf = SCSI_AUTOCONF;

int scsibusprint(void *, const char *);
void scsibus_printlink(struct scsi_link *);

int scsi_activate_bus(struct scsibus_softc *, int);
int scsi_activate_target(struct scsibus_softc *, int, int);
int scsi_activate_lun(struct scsibus_softc *, int, int, int);

const u_int8_t version_to_spc [] = {
	0, /* 0x00: The device does not claim conformance to any standard. */
	1, /* 0x01: (Obsolete) SCSI-1 in olden times. */
	2, /* 0x02: (Obsolete) SCSI-2 in olden times. */
	3, /* 0x03: The device complies to ANSI INCITS 301-1997 (SPC-3). */
	2, /* 0x04: The device complies to ANSI INCITS 351-2001 (SPC-2). */
	3, /* 0x05: The device complies to ANSI INCITS 408-2005 (SPC-3). */
	4, /* 0x06: The device complies to SPC-4. */
	0, /* 0x07: RESERVED. */
};

int
scsiprint(void *aux, const char *pnp)
{
	/* only "scsibus"es can attach to "scsi"s; easy. */
	if (pnp)
		printf("scsibus at %s", pnp);

	return (UNCONF);
}

int
scsibusmatch(struct device *parent, void *match, void *aux)
{
	return (1);
}

/*
 * The routine called by the adapter boards to get all their
 * devices configured in.
 */
void
scsibusattach(struct device *parent, struct device *self, void *aux)
{
	struct scsibus_softc		*sb = (struct scsibus_softc *)self;
	struct scsibus_attach_args	*saa = aux;
	struct scsi_link		*sc_link_proto = saa->saa_sc_link;

	if (!cold)
		scsi_autoconf = 0;

	sc_link_proto->bus = sb;
	sc_link_proto->scsibus = sb->sc_dev.dv_unit;
	sb->adapter_link = sc_link_proto;
	if (sb->adapter_link->adapter_buswidth == 0)
		sb->adapter_link->adapter_buswidth = 8;
	sb->sc_buswidth = sb->adapter_link->adapter_buswidth;
	if (sb->adapter_link->luns == 0)
		sb->adapter_link->luns = 8;

	printf(": %d targets", sb->sc_buswidth);
	if (sb->adapter_link->adapter_target < sb->sc_buswidth)
		printf(", initiator %d", sb->adapter_link->adapter_target);
	if (sb->adapter_link->port_wwn != 0x0 &&
	    sb->adapter_link->node_wwn != 0x0) {
		printf(", WWPN %016llx, WWNN %016llx",
		    sb->adapter_link->port_wwn, sb->adapter_link->node_wwn);
	}
	printf("\n");

	/* Initialize shared data. */
	scsi_init();

	SLIST_INIT(&sb->sc_link);

#if NBIO > 0
	if (bio_register(&sb->sc_dev, scsibus_bioctl) != 0)
		printf("%s: unable to register bio\n", sb->sc_dev.dv_xname);
#endif

	scsi_probe_bus(sb);
}

int
scsibusactivate(struct device *dev, int act)
{
	struct scsibus_softc *sc = (struct scsibus_softc *)dev;

	return scsi_activate(sc, -1, -1, act);
}

int
scsi_activate(struct scsibus_softc *sc, int target, int lun, int act)
{
	if (target == -1 && lun == -1)
		return scsi_activate_bus(sc, act);

	if (target == -1)
		return 0;

	if (lun == -1)
		return scsi_activate_target(sc, target, act);

	return scsi_activate_lun(sc, target, lun, act);
}

int
scsi_activate_bus(struct scsibus_softc *sc, int act)
{
	int target, rv = 0, r;

	for (target = 0; target < sc->sc_buswidth; target++) {
		r = scsi_activate_target(sc, target, act);
		if (r)
			rv = r;
	}
	return (rv);
}

int
scsi_activate_target(struct scsibus_softc *sc, int target, int act)
{
	int lun, rv = 0, r;

	for (lun = 0; lun < sc->adapter_link->luns; lun++) {
		r = scsi_activate_lun(sc, target, lun, act);
		if (r)
			rv = r;
	}
	return (rv);
}

int
scsi_activate_lun(struct scsibus_softc *sc, int target, int lun, int act)
{
	struct scsi_link *link;
	struct device *dev;

	link = scsi_get_link(sc, target, lun);
	if (link == NULL)
		return (0);

	dev = link->device_softc;
	switch (act) {
	case DVACT_ACTIVATE:
		atomic_clearbits_int(&link->state, SDEV_S_DYING);
#if NMPATH > 0
		if (dev == NULL)
			mpath_path_activate(link);
		else
#endif /* NMPATH */
			config_activate(dev);
		break;
	case DVACT_QUIESCE:
	case DVACT_SUSPEND:
	case DVACT_RESUME:
		config_suspend(dev, act);
		break;
	case DVACT_DEACTIVATE:
		atomic_setbits_int(&link->state, SDEV_S_DYING);
#if NMPATH > 0
		if (dev == NULL)
			mpath_path_deactivate(link);
		else
#endif /* NMPATH */
			config_deactivate(dev);
		break;
	default:
		break;
	}

	return (0);
}

int
scsibusdetach(struct device *dev, int type)
{
	struct scsibus_softc		*sb = (struct scsibus_softc *)dev;
	int				error;

#if NBIO > 0
	bio_unregister(&sb->sc_dev);
#endif

	error = scsi_detach_bus(sb, type);
	if (error != 0)
		return (error);

	KASSERT(SLIST_EMPTY(&sb->sc_link));

	return (0);
}

int
scsibussubmatch(struct device *parent, void *match, void *aux)
{
	struct cfdata			*cf = match;
	struct scsi_attach_args		*sa = aux;
	struct scsi_link		*sc_link = sa->sa_sc_link;

	if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != sc_link->target)
		return (0);
	if (cf->cf_loc[1] != -1 && cf->cf_loc[1] != sc_link->lun)
		return (0);

	return ((*cf->cf_attach->ca_match)(parent, match, aux));
}

#if NBIO > 0
int
scsibus_bioctl(struct device *dev, u_long cmd, caddr_t addr)
{
	struct scsibus_softc		*sc = (struct scsibus_softc *)dev;
	struct sbioc_device		*sdev;

	switch (cmd) {
	case SBIOCPROBE:
		sdev = (struct sbioc_device *)addr;

		if (sdev->sd_target == -1 && sdev->sd_lun == -1)
			return (scsi_probe_bus(sc));

		/* specific lun and wildcard target is bad */
		if (sdev->sd_target == -1)
			return (EINVAL);

		if (sdev->sd_lun == -1)
			return (scsi_probe_target(sc, sdev->sd_target));

		return (scsi_probe_lun(sc, sdev->sd_target, sdev->sd_lun));

	case SBIOCDETACH:
		sdev = (struct sbioc_device *)addr;

		if (sdev->sd_target == -1 && sdev->sd_lun == -1)
			return (scsi_detach_bus(sc, 0));

		if (sdev->sd_target == -1)
			return (EINVAL);

		if (sdev->sd_lun == -1)
			return (scsi_detach_target(sc, sdev->sd_target, 0));

		return (scsi_detach_lun(sc, sdev->sd_target, sdev->sd_lun, 0));

	default:
		return (ENOTTY);
	}
}
#endif

int
scsi_probe_bus(struct scsibus_softc *sc)
{
	struct scsi_link *alink = sc->adapter_link;
	int i;

	for (i = 0; i < alink->adapter_buswidth; i++)
		scsi_probe_target(sc, i);

	return (0);
}

int
scsi_probe_target(struct scsibus_softc *sc, int target)
{
	struct scsi_link *alink = sc->adapter_link;
	struct scsi_link *link;
	struct scsi_report_luns_data *report;
	int i, nluns, lun;

	if (scsi_probe_lun(sc, target, 0) == EINVAL)
		return (EINVAL);

	link = scsi_get_link(sc, target, 0);
	if (link == NULL)
		return (ENXIO);

	if ((link->flags & (SDEV_UMASS | SDEV_ATAPI)) == 0 &&
	    SCSISPC(link->inqdata.version) > 2) {
		report = dma_alloc(sizeof(*report), PR_WAITOK);
		if (report == NULL)
			goto dumbscan;

		if (scsi_report_luns(link, REPORT_NORMAL, report,
		    sizeof(*report), scsi_autoconf | SCSI_SILENT |
		    SCSI_IGNORE_ILLEGAL_REQUEST | SCSI_IGNORE_NOT_READY |
		    SCSI_IGNORE_MEDIA_CHANGE, 10000) != 0) {
			dma_free(report, sizeof(*report));
			goto dumbscan;
		}

		/*
		 * XXX In theory we should check if data is full, which
		 * would indicate it needs to be enlarged and REPORT
		 * LUNS tried again. Solaris tries up to 3 times with
		 * larger sizes for data.
		 */
		nluns = _4btol(report->length) / RPL_LUNDATA_SIZE;
		for (i = 0; i < nluns; i++) {
			if (report->luns[i].lundata[0] != 0)
				continue;
			lun = report->luns[i].lundata[RPL_LUNDATA_T0LUN];
			if (lun == 0)
				continue;

			/* Probe the provided LUN. Don't check LUN 0. */
			scsi_remove_link(sc, link);
			scsi_probe_lun(sc, target, lun);
			scsi_add_link(sc, link);
		}

		dma_free(report, sizeof(*report));
		return (0);
	}

dumbscan:
	for (i = 1; i < alink->luns; i++) {
		if (scsi_probe_lun(sc, target, i) == EINVAL)
			break;
	}

	return (0);
}

int
scsi_probe_lun(struct scsibus_softc *sc, int target, int lun)
{
	struct scsi_link *alink = sc->adapter_link;

	if (target < 0 || target >= alink->adapter_buswidth ||
	    target == alink->adapter_target ||
	    lun < 0 || lun >= alink->luns)
		return (ENXIO);

	return (scsi_probedev(sc, target, lun));
}

int
scsi_detach_bus(struct scsibus_softc *sc, int flags)
{
	struct scsi_link *alink = sc->adapter_link;
	int i, err, rv = 0;

	for (i = 0; i < alink->adapter_buswidth; i++) {
		err = scsi_detach_target(sc, i, flags);
		if (err != 0 && err != ENXIO)
			rv = err;
	}

	return (rv);
}

int
scsi_detach_target(struct scsibus_softc *sc, int target, int flags)
{
	struct scsi_link *alink = sc->adapter_link;
	int i, err, rv = 0;

	if (target < 0 || target >= alink->adapter_buswidth ||
	    target == alink->adapter_target)
		return (ENXIO);

	for (i = 0; i < alink->luns; i++) { /* nicer backwards? */
		if (scsi_get_link(sc, target, i) == NULL)
			continue;

		err = scsi_detach_lun(sc, target, i, flags);
		if (err != 0 && err != ENXIO)
			rv = err;
	}

	return (rv);
}

int
scsi_detach_lun(struct scsibus_softc *sc, int target, int lun, int flags)
{
	struct scsi_link *alink = sc->adapter_link;
	struct scsi_link *link;
	int rv;

	if (target < 0 || target >= alink->adapter_buswidth ||
	    target == alink->adapter_target ||
	    lun < 0 || lun >= alink->luns)
		return (ENXIO);

	link = scsi_get_link(sc, target, lun);
	if (link == NULL)
		return (ENXIO);

	if (((flags & DETACH_FORCE) == 0) && (link->flags & SDEV_OPEN))
		return (EBUSY);

	/* detaching a device from scsibus is a five step process... */

	/* 1. wake up processes sleeping for an xs */
	scsi_link_shutdown(link);

	/* 2. detach the device */
#if NMPATH > 0
	if (link->device_softc == NULL)
		rv = mpath_path_detach(link, flags);
	else
#endif /* NMPATH */
		rv = config_detach(link->device_softc, flags);

	if (rv != 0)
		return (rv);

	/* 3. if its using the openings io allocator, clean it up */
	if (ISSET(link->flags, SDEV_OWN_IOPL)) {
		scsi_iopool_destroy(link->pool);
		free(link->pool, M_DEVBUF);
	}

	/* 4. free up its state in the adapter */
	if (alink->adapter->dev_free != NULL)
		alink->adapter->dev_free(link);

	/* 5. free up its state in the midlayer */
	if (link->id != NULL)
		devid_free(link->id);
	scsi_remove_link(sc, link);
	free(link, M_DEVBUF);

	return (0);
}

struct scsi_link *
scsi_get_link(struct scsibus_softc *sc, int target, int lun)
{
	struct scsi_link *link;

	SLIST_FOREACH(link, &sc->sc_link, bus_list)
		if (link->target == target && link->lun == lun)
			return (link);

	return (NULL);
}

void
scsi_add_link(struct scsibus_softc *sc, struct scsi_link *link)
{
	SLIST_INSERT_HEAD(&sc->sc_link, link, bus_list);
}

void
scsi_remove_link(struct scsibus_softc *sc, struct scsi_link *link)
{
	SLIST_REMOVE(&sc->sc_link, link, scsi_link, bus_list);
}

void
scsi_strvis(u_char *dst, u_char *src, int len)
{
	u_char				last;

	/* Trim leading and trailing whitespace and NULs. */
	while (len > 0 && (src[0] == ' ' || src[0] == '\t' || src[0] == '\n' ||
	    src[0] == '\0' || src[0] == 0xff))
		++src, --len;
	while (len > 0 && (src[len-1] == ' ' || src[len-1] == '\t' ||
	    src[len-1] == '\n' || src[len-1] == '\0' || src[len-1] == 0xff))
		--len;

	last = 0xff;
	while (len > 0) {
		switch (*src) {
		case ' ':
		case '\t':
		case '\n':
		case '\0':
		case 0xff:
			/* collapse whitespace and NULs to a single space */
			if (last != ' ')
				*dst++ = ' ';
			last = ' ';
			break;
		case '\\':
			/* quote characters */
			*dst++ = '\\';
			*dst++ = '\\';
			last = '\\';
			break;
		default:
			if (*src < 0x20 || *src >= 0x80) {
				/* non-printable characters */
				*dst++ = '\\';
				*dst++ = ((*src & 0300) >> 6) + '0';
				*dst++ = ((*src & 0070) >> 3) + '0';
				*dst++ = ((*src & 0007) >> 0) + '0';
			} else {
				/* normal characters */
				*dst++ = *src;
			}
			last = *src;
			break;
		}
		++src, --len;
	}

	*dst++ = 0;
}

struct scsi_quirk_inquiry_pattern {
	struct scsi_inquiry_pattern	pattern;
	u_int16_t			quirks;
};

const struct scsi_quirk_inquiry_pattern scsi_quirk_patterns[] = {
	{{T_CDROM, T_REMOV,
	 "PLEXTOR", "CD-ROM PX-40TS", "1.01"},    SDEV_NOSYNC},

	{{T_DIRECT, T_FIXED,
	 "MICROP  ", "1588-15MBSUN0669", ""},     SDEV_AUTOSAVE},
	{{T_DIRECT, T_FIXED,
	 "DEC     ", "RZ55     (C) DEC", ""},     SDEV_AUTOSAVE},
	{{T_DIRECT, T_FIXED,
	 "EMULEX  ", "MD21/S2     ESDI", "A00"},  SDEV_AUTOSAVE},
	{{T_DIRECT, T_FIXED,
	 "IBMRAID ", "0662S",            ""},     SDEV_AUTOSAVE},
	{{T_DIRECT, T_FIXED,
	 "IBM     ", "0663H",            ""},     SDEV_AUTOSAVE},
	{{T_DIRECT, T_FIXED,
	 "IBM",	  "0664",		 ""},	  SDEV_AUTOSAVE},
	{{T_DIRECT, T_FIXED,
	 "IBM     ", "H3171-S2",         ""},	  SDEV_AUTOSAVE},
	{{T_DIRECT, T_FIXED,
	 "IBM     ", "KZ-C",		 ""},	  SDEV_AUTOSAVE},
	/* Broken IBM disk */
	{{T_DIRECT, T_FIXED,
	 ""	   , "DFRSS2F",		 ""},	  SDEV_AUTOSAVE},
	{{T_DIRECT, T_FIXED,
	 "QUANTUM ", "ELS85S          ", ""},	  SDEV_AUTOSAVE},
	{{T_DIRECT, T_REMOV,
	 "iomega", "jaz 1GB",		 ""},	  SDEV_NOTAGS},
        {{T_DIRECT, T_FIXED,
         "MICROP", "4421-07",		 ""},     SDEV_NOTAGS},
        {{T_DIRECT, T_FIXED,
         "SEAGATE", "ST150176LW",        "0002"}, SDEV_NOTAGS},
        {{T_DIRECT, T_FIXED,
         "HP", "C3725S",		 ""},     SDEV_NOTAGS},
        {{T_DIRECT, T_FIXED,
         "IBM", "DCAS",			 ""},     SDEV_NOTAGS},

	{{T_SEQUENTIAL, T_REMOV,
	 "SONY    ", "SDT-5000        ", "3."},   SDEV_NOSYNC|SDEV_NOWIDE},
	{{T_SEQUENTIAL, T_REMOV,
	 "WangDAT ", "Model 1300      ", "02.4"}, SDEV_NOSYNC|SDEV_NOWIDE},
	{{T_SEQUENTIAL, T_REMOV,
	 "WangDAT ", "Model 2600      ", "01.7"}, SDEV_NOSYNC|SDEV_NOWIDE},
	{{T_SEQUENTIAL, T_REMOV,
	 "WangDAT ", "Model 3200      ", "02.2"}, SDEV_NOSYNC|SDEV_NOWIDE},

	/* ATAPI device quirks */
        {{T_CDROM, T_REMOV,
         "CR-2801TE", "", "1.07"},              ADEV_NOSENSE},
        {{T_CDROM, T_REMOV,
         "CREATIVECD3630E", "", "AC101"},       ADEV_NOSENSE},
        {{T_CDROM, T_REMOV,
         "FX320S", "", "q01"},                  ADEV_NOSENSE},
        {{T_CDROM, T_REMOV,
         "GCD-R580B", "", "1.00"},              ADEV_LITTLETOC},
        {{T_CDROM, T_REMOV,
         "MATSHITA CR-574", "", "1.02"},        ADEV_NOCAPACITY},
        {{T_CDROM, T_REMOV,
         "MATSHITA CR-574", "", "1.06"},        ADEV_NOCAPACITY},
        {{T_CDROM, T_REMOV,
         "Memorex CRW-2642", "", "1.0g"},       ADEV_NOSENSE},
        {{T_CDROM, T_REMOV,
         "SANYO CRD-256P", "", "1.02"},         ADEV_NOCAPACITY},
        {{T_CDROM, T_REMOV,
         "SANYO CRD-254P", "", "1.02"},         ADEV_NOCAPACITY},
        {{T_CDROM, T_REMOV,
         "SANYO CRD-S54P", "", "1.08"},         ADEV_NOCAPACITY},
        {{T_CDROM, T_REMOV,
         "CD-ROM  CDR-S1", "", "1.70"},         ADEV_NOCAPACITY}, /* Sanyo */
        {{T_CDROM, T_REMOV,
         "CD-ROM  CDR-N16", "", "1.25"},        ADEV_NOCAPACITY}, /* Sanyo */
        {{T_CDROM, T_REMOV,
         "UJDCD8730", "", "1.14"},              ADEV_NODOORLOCK}, /* Acer */
};


void
scsibus_printlink(struct scsi_link *link)
{
	char				vendor[33], product[65], revision[17];
	struct scsi_inquiry_data	*inqbuf;
	u_int8_t			type;
	int				removable;
	char				*dtype = NULL, *qtype = NULL;

	inqbuf = &link->inqdata;

	type = inqbuf->device & SID_TYPE;
	removable = inqbuf->dev_qual2 & SID_REMOVABLE ? 1 : 0;

	/*
	 * Figure out basic device type and qualifier.
	 */
	switch (inqbuf->device & SID_QUAL) {
	case SID_QUAL_LU_OK:
		qtype = "";
		break;

	case SID_QUAL_LU_OFFLINE:
		qtype = " offline";
		break;

	case SID_QUAL_RSVD:
		panic("scsibusprint: qualifier == SID_QUAL_RSVD");
	case SID_QUAL_BAD_LU:
		panic("scsibusprint: qualifier == SID_QUAL_BAD_LU");

	default:
		qtype = "";
		dtype = "vendor-unique";
		break;
	}
	if (dtype == NULL) {
		switch (type) {
		case T_DIRECT:
			dtype = "direct";
			break;
		case T_SEQUENTIAL:
			dtype = "sequential";
			break;
		case T_PRINTER:
			dtype = "printer";
			break;
		case T_PROCESSOR:
			dtype = "processor";
			break;
		case T_CDROM:
			dtype = "cdrom";
			break;
		case T_WORM:
			dtype = "worm";
			break;
		case T_SCANNER:
			dtype = "scanner";
			break;
		case T_OPTICAL:
			dtype = "optical";
			break;
		case T_CHANGER:
			dtype = "changer";
			break;
		case T_COMM:
			dtype = "communication";
			break;
		case T_ENCLOSURE:
			dtype = "enclosure services";
			break;
		case T_RDIRECT:
			dtype = "simplified direct";
			break;
		case T_NODEVICE:
			panic("scsibusprint: device type T_NODEVICE");
		default:
			dtype = "unknown";
			break;
		}
	}

	scsi_strvis(vendor, inqbuf->vendor, 8);
	scsi_strvis(product, inqbuf->product, 16);
	scsi_strvis(revision, inqbuf->revision, 4);

	printf(" targ %d lun %d: <%s, %s, %s> ", link->target, link->lun,
	    vendor, product, revision);
	if (link->flags & SDEV_ATAPI)
		printf("ATAPI");
	else
		printf("SCSI%d", SCSISPC(inqbuf->version));
	printf(" %d/%s %s%s", type, dtype, removable ? "removable" : "fixed",
	    qtype);

#if NMPATH > 0
	if (link->id != NULL && link->id->d_type != DEVID_NONE) {
		u_int8_t *id = (u_int8_t *)(link->id + 1);
		int i;

		switch (link->id->d_type) {
		case DEVID_NAA:   
			printf(" naa.");
			break;
		case DEVID_EUI:
			printf(" eui.");
			break;
		case DEVID_T10:
			printf(" t10.");
			break;
		}

		if (ISSET(link->id->d_flags, DEVID_F_PRINT)) {
			for (i = 0; i < link->id->d_len; i++) {
				if (id[i] == '\0' || id[i] == ' ')
					printf("_");
				else if (id[i] < 0x20 || id[i] >= 0x80) {
					/* non-printable characters */
					printf("~");
				} else {
					/* normal characters */
					printf("%c", id[i]);
				}
			}
		} else {
			for (i = 0; i < link->id->d_len; i++)
				printf("%02x", id[i]);
		}
	}
#endif /* NMPATH > 0 */
}

/*
 * Print out autoconfiguration information for a subdevice.
 *
 * This is a slight abuse of 'standard' autoconfiguration semantics,
 * because 'print' functions don't normally print the colon and
 * device information.  However, in this case that's better than
 * either printing redundant information before the attach message,
 * or having the device driver call a special function to print out
 * the standard device information.
 */
int
scsibusprint(void *aux, const char *pnp)
{
	struct scsi_attach_args		*sa = aux;

	if (pnp != NULL)
		printf("%s", pnp);

	scsibus_printlink(sa->sa_sc_link);

	return (UNCONF);
}

/*
 * Given a target and lun, ask the device what it is, and find the correct
 * driver table entry.
 *
 * Return 0 if further LUNs are possible, EINVAL if not.
 */
int
scsi_probedev(struct scsibus_softc *scsi, int target, int lun)
{
	const struct scsi_quirk_inquiry_pattern *finger;
	struct scsi_inquiry_data *inqbuf;
	struct scsi_attach_args sa;
	struct scsi_link *sc_link, *link0;
	struct cfdata *cf;
	int priority, rslt = 0;

	/* Skip this slot if it is already attached and try the next LUN. */
	if (scsi_get_link(scsi, target, lun) != NULL)
		return (0);

	sc_link = malloc(sizeof(*sc_link), M_DEVBUF, M_NOWAIT);
	if (sc_link == NULL)
		return (EINVAL);

	*sc_link = *scsi->adapter_link;
	sc_link->target = target;
	sc_link->lun = lun;
	sc_link->interpret_sense = scsi_interpret_sense;
	TAILQ_INIT(&sc_link->queue);
	inqbuf = &sc_link->inqdata;

	SC_DEBUG(sc_link, SDEV_DB2, ("scsi_link created.\n"));

	/* ask the adapter if this will be a valid device */
	if (scsi->adapter_link->adapter->dev_probe != NULL &&
	    scsi->adapter_link->adapter->dev_probe(sc_link) != 0) {
		if (lun == 0)
			rslt = EINVAL;
		goto free;
	}

	/*
	 * If we havent been given an io pool by now then fall back to
	 * using sc_link->openings.
	 */
	if (sc_link->pool == NULL) {
		sc_link->pool = malloc(sizeof(*sc_link->pool),
		    M_DEVBUF, M_NOWAIT);
		if (sc_link->pool == NULL) {
			rslt = ENOMEM;
			goto bad;
		}
		scsi_iopool_init(sc_link->pool, sc_link,
		    scsi_default_get, scsi_default_put);

		SET(sc_link->flags, SDEV_OWN_IOPL);
	}

	/*
	 * Tell drivers that are paying attention to avoid sync/wide/tags until
	 * INQUIRY data has been processed and the quirks information is
	 * complete. Some drivers set bits in quirks before we get here, so
	 * just add NOTAGS, NOWIDE and NOSYNC.
	 */
	sc_link->quirks |= SDEV_NOSYNC | SDEV_NOWIDE | SDEV_NOTAGS;

	/*
	 * Ask the device what it is
	 */
#ifdef SCSIDEBUG
	if (((1 << sc_link->scsibus) & scsidebug_buses) &&
	    ((target < 32) && ((1 << target) & scsidebug_targets)) &&
	    ((lun < 32) && ((1 << lun) & scsidebug_luns)))
		sc_link->flags |= scsidebug_level;
#endif /* SCSIDEBUG */

	if (lun == 0) {
		/* Clear any outstanding errors. */
		scsi_test_unit_ready(sc_link, TEST_READY_RETRIES,
		    scsi_autoconf | SCSI_IGNORE_ILLEGAL_REQUEST |
		    SCSI_IGNORE_NOT_READY | SCSI_IGNORE_MEDIA_CHANGE);
	}

	/* Now go ask the device all about itself. */
	rslt = scsi_inquire(sc_link, inqbuf, scsi_autoconf | SCSI_SILENT);
	if (rslt != 0) {
		SC_DEBUG(sc_link, SDEV_DB2, ("Bad LUN. rslt = %i\n", rslt));
		if (lun == 0)
			rslt = EINVAL;
		goto bad;
	}

	switch (inqbuf->device & SID_QUAL) {
	case SID_QUAL_RSVD:
	case SID_QUAL_BAD_LU:
	case SID_QUAL_LU_OFFLINE:
		SC_DEBUG(sc_link, SDEV_DB1, ("Bad LUN. SID_QUAL = 0x%02x\n",
		    inqbuf->device & SID_QUAL));
		goto bad;

	case SID_QUAL_LU_OK:
		if ((inqbuf->device & SID_TYPE) == T_NODEVICE) {
			SC_DEBUG(sc_link, SDEV_DB1,
		    	    ("Bad LUN. SID_TYPE = T_NODEVICE\n"));
			goto bad;
		}
		break;

	default:
		break;
	}

	scsi_devid(sc_link);

	link0 = scsi_get_link(scsi, target, 0);
	if (lun == 0 || link0 == NULL)
		;
	else if (sc_link->flags & SDEV_UMASS)
		;
	else if (sc_link->id != NULL && !DEVID_CMP(link0->id, sc_link->id))
		;
	else if (memcmp(inqbuf, &link0->inqdata, sizeof(*inqbuf)) == 0) {
		/* The device doesn't distinguish between LUNs. */
		SC_DEBUG(sc_link, SDEV_DB1, ("IDENTIFY not supported.\n"));
		rslt = EINVAL;
		goto free_devid;
	}

#if NMPATH > 0
	/* should multipathing steal the link? */
	if (mpath_path_attach(sc_link) == 0) {
		printf("%s: path to", scsi->sc_dev.dv_xname);
		scsibus_printlink(sc_link);
		printf("\n");

		scsi_add_link(scsi, sc_link);
		return (0);
	}
#endif /* NMPATH */

	finger = (const struct scsi_quirk_inquiry_pattern *)scsi_inqmatch(
	    inqbuf, scsi_quirk_patterns,
	    sizeof(scsi_quirk_patterns)/sizeof(scsi_quirk_patterns[0]),
	    sizeof(scsi_quirk_patterns[0]), &priority);

	/*
	 * Based upon the inquiry flags we got back, and if we're
	 * at SCSI-2 or better, remove some limiting quirks.
	 */
	if (SCSISPC(inqbuf->version) >= 2) {
		if ((inqbuf->flags & SID_CmdQue) != 0)
			sc_link->quirks &= ~SDEV_NOTAGS;
		if ((inqbuf->flags & SID_Sync) != 0)
			sc_link->quirks &= ~SDEV_NOSYNC;
		if ((inqbuf->flags & SID_WBus16) != 0)
			sc_link->quirks &= ~SDEV_NOWIDE;
	} else
		/* Older devices do not have SYNCHRONIZE CACHE capability. */
		sc_link->quirks |= SDEV_NOSYNCCACHE;

	/*
	 * Now apply any quirks from the table.
	 */
	if (priority != 0)
		sc_link->quirks |= finger->quirks;

	/*
	 * If the device can't use tags, >1 opening may confuse it.
	 */
	if (ISSET(sc_link->quirks, SDEV_NOTAGS))
		sc_link->openings = 1;

	/*
	 * note what BASIC type of device it is
	 */
	if ((inqbuf->dev_qual2 & SID_REMOVABLE) != 0)
		sc_link->flags |= SDEV_REMOVABLE;

	sa.sa_sc_link = sc_link;
	sa.sa_inqbuf = &sc_link->inqdata;

	if ((cf = config_search(scsibussubmatch, (struct device *)scsi,
	    &sa)) == 0) {
		scsibusprint(&sa, scsi->sc_dev.dv_xname);
		printf(" not configured\n");
		goto free_devid;
	}

	/*
	 * Braindead USB devices, especially some x-in-1 media readers, try to
	 * 'help' by pretending any LUN is actually LUN 0 until they see a
	 * different LUN used in a command. So do an INQUIRY on LUN 1 at this
	 * point to prevent such helpfulness before it causes confusion.
	 */
	if (lun == 0 && (sc_link->flags & SDEV_UMASS) &&
	    scsi_get_link(scsi, target, 1) == NULL && sc_link->luns > 1) {
		struct scsi_inquiry_data tmpinq;

		sc_link->lun = 1;
		scsi_inquire(sc_link, &tmpinq, scsi_autoconf | SCSI_SILENT);
	    	sc_link->lun = 0;
	}

	scsi_add_link(scsi, sc_link);

	/*
	 * Generate a TEST_UNIT_READY command. This gives drivers waiting for
	 * valid quirks data a chance to set wide/sync/tag options
	 * appropriately. It also clears any outstanding ACA conditions that
	 * INQUIRY may leave behind.
	 *
	 * Do this now so that any messages generated by config_attach() do not
	 * have negotiation messages inserted into their midst.
	 */
	scsi_test_unit_ready(sc_link, TEST_READY_RETRIES,
	    scsi_autoconf | SCSI_IGNORE_ILLEGAL_REQUEST |
	    SCSI_IGNORE_NOT_READY | SCSI_IGNORE_MEDIA_CHANGE);

	config_attach((struct device *)scsi, cf, &sa, scsibusprint);

	return (0);

free_devid:
	if (sc_link->id)
		devid_free(sc_link->id);
bad:
	if (ISSET(sc_link->flags, SDEV_OWN_IOPL))
		free(sc_link->pool, M_DEVBUF);

	if (scsi->adapter_link->adapter->dev_free != NULL)
		scsi->adapter_link->adapter->dev_free(sc_link);
free:
	free(sc_link, M_DEVBUF);
	return (rslt);
}

/*
 * Return a priority based on how much of the inquiry data matches
 * the patterns for the particular driver.
 */
const void *
scsi_inqmatch(struct scsi_inquiry_data *inqbuf, const void *_base,
    int nmatches, int matchsize, int *bestpriority)
{
	u_int8_t			type;
	int				removable;
	const void			*bestmatch;
	const unsigned char		*base = (const unsigned char *)_base;

	/* Include the qualifier to catch vendor-unique types. */
	type = inqbuf->device;
	removable = inqbuf->dev_qual2 & SID_REMOVABLE ? T_REMOV : T_FIXED;

	for (*bestpriority = 0, bestmatch = 0; nmatches--; base += matchsize) {
		struct scsi_inquiry_pattern *match = (void *)base;
		int priority, len;

		if (type != match->type)
			continue;
		if (removable != match->removable)
			continue;
		priority = 2;
		len = strlen(match->vendor);
		if (bcmp(inqbuf->vendor, match->vendor, len))
			continue;
		priority += len;
		len = strlen(match->product);
		if (bcmp(inqbuf->product, match->product, len))
			continue;
		priority += len;
		len = strlen(match->revision);
		if (bcmp(inqbuf->revision, match->revision, len))
			continue;
		priority += len;

#if SCSIDEBUG
		printf("scsi_inqmatch: %d/%d/%d <%s, %s, %s>\n",
		    priority, match->type, match->removable,
		    match->vendor, match->product, match->revision);
#endif
		if (priority > *bestpriority) {
			*bestpriority = priority;
			bestmatch = base;
		}
	}

	return (bestmatch);
}

void
scsi_devid(struct scsi_link *link)
{
	struct {
		struct scsi_vpd_hdr hdr;
		u_int8_t list[32];
	} __packed *pg;
	int pg80 = 0, pg83 = 0, i;
	size_t len;

	if (link->id != NULL)
		return;

	pg = dma_alloc(sizeof(*pg), PR_WAITOK | PR_ZERO);

	if (SCSISPC(link->inqdata.version) >= 2) {
		if (scsi_inquire_vpd(link, pg, sizeof(*pg), SI_PG_SUPPORTED,
		    scsi_autoconf) != 0)
			goto done;

		len = MIN(sizeof(pg->list), _2btol(pg->hdr.page_length));
		for (i = 0; i < len; i++) {
			switch (pg->list[i]) {
			case SI_PG_SERIAL:
				pg80 = 1;
				break;
			case SI_PG_DEVID:
				pg83 = 1;
				break;
			}
		}

		if (pg83 && scsi_devid_pg83(link) == 0)
			goto done;
#ifdef notyet
		if (pg80 && scsi_devid_pg80(link) == 0)
			goto done;
#endif
	}
done:
	dma_free(pg, sizeof(*pg));
}

int
scsi_devid_pg83(struct scsi_link *link)
{
	struct scsi_vpd_hdr *hdr = NULL;
	struct scsi_vpd_devid_hdr dhdr, chdr;
	u_int8_t *pg = NULL, *id;
	int type, idtype = 0;
	u_char idflags;
	int len, pos;
	int rv;

	hdr = dma_alloc(sizeof(*hdr), PR_WAITOK | PR_ZERO);

	rv = scsi_inquire_vpd(link, hdr, sizeof(*hdr), SI_PG_DEVID,
	    scsi_autoconf);
	if (rv != 0)
		goto done;

	len = sizeof(*hdr) + _2btol(hdr->page_length);
	pg = dma_alloc(len, PR_WAITOK | PR_ZERO);

	rv = scsi_inquire_vpd(link, pg, len, SI_PG_DEVID, scsi_autoconf);
	if (rv != 0)
		goto done;

	pos = sizeof(*hdr);

	do {
		if (len - pos < sizeof(dhdr)) {
			rv = EIO;
			goto done;
		}
		memcpy(&dhdr, &pg[pos], sizeof(dhdr));
		pos += sizeof(dhdr);
		if (len - pos < dhdr.len) {
			rv = EIO;
			goto done;
		}

		if (VPD_DEVID_ASSOC(dhdr.flags) == VPD_DEVID_ASSOC_LU) {
			type = VPD_DEVID_TYPE(dhdr.flags);
			switch (type) {
			case VPD_DEVID_TYPE_NAA:
			case VPD_DEVID_TYPE_EUI64:
			case VPD_DEVID_TYPE_T10:
				if (type >= idtype) {
					idtype = type;

					chdr = dhdr;
					id = &pg[pos];
				}
				break;

			default:
				/* skip */
				break;
			}
		}

		pos += dhdr.len;
	} while (idtype != VPD_DEVID_TYPE_NAA && len != pos);

	if (idtype > 0) {
		switch (VPD_DEVID_TYPE(chdr.flags)) {
		case VPD_DEVID_TYPE_NAA:
			idtype = DEVID_NAA;
			break;
		case VPD_DEVID_TYPE_EUI64:
			idtype = DEVID_EUI;
			break;
		case VPD_DEVID_TYPE_T10:
			idtype = DEVID_T10;
			break;
		}
		switch (VPD_DEVID_CODE(chdr.pi_code)) {
		case VPD_DEVID_CODE_ASCII:
		case VPD_DEVID_CODE_UTF8:
			idflags = DEVID_F_PRINT;
			break;
		default:
			idflags = 0;
			break;
		}
		link->id = devid_alloc(idtype, idflags, chdr.len, id);
	} else
		rv = ENODEV;

done:
	if (pg)
		dma_free(pg, len);
	if (hdr)
		dma_free(hdr, sizeof(*hdr));
	return (rv);
}

/*
 * scsi_minphys member of struct scsi_adapter for drivers which don't
 * need any specific routine.
 */
void
scsi_minphys(struct buf *bp, struct scsi_link *sl)
{
	minphys(bp);
}

struct devid *
devid_alloc(u_int8_t type, u_int8_t flags, u_int8_t len, u_int8_t *id)
{
	struct devid *d;

	d = malloc(sizeof(*d) + len, M_DEVBUF, M_WAITOK|M_CANFAIL);
	if (d == NULL)
		return (NULL);

	d->d_type = type;
	d->d_flags = flags;
	d->d_len = len;
	d->d_refcount = 1;
	memcpy(d + 1, id, len);

	return (d);
}

struct devid *
devid_copy(struct devid *d)
{
	d->d_refcount++;
	return (d);
}

void
devid_free(struct devid *d)
{
	if (--d->d_refcount == 0)
		free(d, M_DEVBUF);
}
