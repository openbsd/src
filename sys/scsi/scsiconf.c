/*	$OpenBSD: scsiconf.c,v 1.127 2007/11/06 02:49:19 krw Exp $	*/
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/device.h>

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

struct scsi_device probe_switch = {
	NULL,
	NULL,
	NULL,
	NULL,
};

int	scsibusmatch(struct device *, void *, void *);
void	scsibusattach(struct device *, struct device *, void *);
int	scsibusactivate(struct device *, enum devact);
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
	int				nbytes, i;

	if (!cold)
		scsi_autoconf = 0;

	sc_link_proto->scsibus = sb->sc_dev.dv_unit;
	sb->adapter_link = sc_link_proto;
	if (sb->adapter_link->adapter_buswidth == 0)
		sb->adapter_link->adapter_buswidth = 8;
	sb->sc_buswidth = sb->adapter_link->adapter_buswidth;
	if (sb->adapter_link->luns == 0)
		sb->adapter_link->luns = 8;

	printf(": %d targets\n", sb->sc_buswidth);

	/* Initialize shared data. */
	scsi_init();

	nbytes = sb->sc_buswidth * sizeof(struct scsi_link **);
	sb->sc_link = malloc(nbytes, M_DEVBUF, M_NOWAIT);
	if (sb->sc_link == NULL)
		panic("scsibusattach: can't allocate target links");
	nbytes = sb->adapter_link->luns * sizeof(struct scsi_link *);
	for (i = 0; i < sb->sc_buswidth; i++) {
		sb->sc_link[i] = malloc(nbytes, M_DEVBUF, M_NOWAIT | M_ZERO);
		if (sb->sc_link[i] == NULL)
			panic("scsibusattach: can't allocate lun links");
	}

#if NBIO > 0
	if (bio_register(&sb->sc_dev, scsibus_bioctl) != 0)
		printf("%s: unable to register bio\n", sb->sc_dev.dv_xname);
#endif

	scsi_probe_bus(sb);
}

int
scsibusactivate(struct device *dev, enum devact act)
{
	return (config_activate_children(dev, act));
}

int
scsibusdetach(struct device *dev, int type)
{
	struct scsibus_softc		*sb = (struct scsibus_softc *)dev;
	int				i, j, error;

#if NBIO > 0
	bio_unregister(&sb->sc_dev);
#endif

	if ((error = config_detach_children(dev, type)) != 0)
		return (error);

	for (i = 0; i < sb->sc_buswidth; i++) {
		if (sb->sc_link[i] != NULL) {
			for (j = 0; j < sb->adapter_link->luns; j++) {
				if (sb->sc_link[i][j] != NULL)
					free(sb->sc_link[i][j], M_DEVBUF);
			}
			free(sb->sc_link[i], M_DEVBUF);
		}
	}

	free(sb->sc_link, M_DEVBUF);

	/* Free shared data. */
	scsi_deinit();

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

	link = sc->sc_link[target][0];
	if (link == NULL)
		return (ENXIO);

	if ((link->flags & (SDEV_UMASS | SDEV_ATAPI)) == 0 &&
	    SCSISPC(link->inqdata.version) > 2) {
		report = malloc(sizeof(*report), M_TEMP, M_WAITOK);
		if (report == NULL)
			goto dumbscan;

		if (scsi_report_luns(link, REPORT_NORMAL, report,
		    sizeof(*report), scsi_autoconf | SCSI_SILENT |
		    SCSI_IGNORE_ILLEGAL_REQUEST | SCSI_IGNORE_NOT_READY |
		    SCSI_IGNORE_MEDIA_CHANGE, 10000) != 0) {
			free(report, M_TEMP);
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
			sc->sc_link[target][0] = NULL;
			scsi_probe_lun(sc, target, lun);
			sc->sc_link[target][0] = link;
		}

		free(report, M_TEMP);
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
	int i;

	for (i = 0; i < alink->adapter_buswidth; i++)
		scsi_detach_target(sc, i, flags);

	return (0);
}

int
scsi_detach_target(struct scsibus_softc *sc, int target, int flags)
{
	struct scsi_link *alink = sc->adapter_link;
	int i, err, rv = 0, detached = 0;

	if (target < 0 || target >= alink->adapter_buswidth ||
	    target == alink->adapter_target)
		return (ENXIO);

	if (sc->sc_link[target] == NULL)
		return (ENXIO);

	for (i = 0; i < alink->luns; i++) { /* nicer backwards? */
		if (sc->sc_link[target][i] == NULL)
			continue;

		err = scsi_detach_lun(sc, target, i, flags);
		if (err != 0)
			rv = err;
		detached = 1;
	}

	return (detached ? rv : ENXIO);
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

	if (sc->sc_link[target] == NULL)
		return (ENXIO);

	link = sc->sc_link[target][lun];
	if (link == NULL)
		return (ENXIO);

	if (((flags & DETACH_FORCE) == 0) && (link->flags & SDEV_OPEN))
		return (EBUSY);

	/* detaching a device from scsibus is a two step process... */

	/* 1. detach the device */
	rv = config_detach(link->device_softc, flags);
	if (rv != 0)
		return (rv);

	/* 2. free up its state in the midlayer */
	free(link, M_DEVBUF);
	sc->sc_link[target][lun] = NULL;

	return (0);
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
         "ALPS ELECTRIC CO.,LTD. DC544C", "", "SW03D"}, ADEV_NOTUR},
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
         "NEC                 CD-ROM DRIVE:273", "", "4.21"}, ADEV_NOTUR},
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
	struct scsi_inquiry_data	*inqbuf;
	u_int8_t			type;
	int				removable;
	char				*dtype, *qtype;
	char				vendor[33], product[65], revision[17];
	int				target, lun;

	if (pnp != NULL)
		printf("%s", pnp);

	inqbuf = sa->sa_inqbuf;

	target = sa->sa_sc_link->target;
	lun = sa->sa_sc_link->lun;

	type = inqbuf->device & SID_TYPE;
	removable = inqbuf->dev_qual2 & SID_REMOVABLE ? 1 : 0;

	/*
	 * Figure out basic device type and qualifier.
	 */
	dtype = 0;
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
	if (dtype == 0) {
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

	printf(" targ %d lun %d: <%s, %s, %s> SCSI%d %d/%s %s%s",
	    target, lun, vendor, product, revision,
	    SCSISPC(inqbuf->version), type, dtype,
	    removable ? "removable" : "fixed", qtype);

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
	static struct scsi_inquiry_data	inqbuf;
	struct scsi_attach_args sa;
	struct scsi_link *sc_link;
	struct cfdata *cf;
	int priority, rslt = 0;

	/* Skip this slot if it is already attached and try the next LUN. */
	if (scsi->sc_link[target][lun] != NULL)
		return (0);

	sc_link = malloc(sizeof(*sc_link), M_DEVBUF, M_NOWAIT);
	if (sc_link == NULL)
		return (EINVAL);

	*sc_link = *scsi->adapter_link;
	sc_link->target = target;
	sc_link->lun = lun;
	sc_link->device = &probe_switch;

	SC_DEBUG(sc_link, SDEV_DB2, ("scsi_link created.\n"));

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
	    ((1 << target) & scsidebug_targets) &&
	    ((1 << lun) & scsidebug_luns))
		sc_link->flags |= scsidebug_level;
#endif /* SCSIDEBUG */

#if defined(mvme68k)
	if (lun == 0) {
		/* XXX some drivers depend on this */
		scsi_test_unit_ready(sc_link, TEST_READY_RETRIES,
		    scsi_autoconf | SCSI_IGNORE_ILLEGAL_REQUEST |
		    SCSI_IGNORE_NOT_READY | SCSI_IGNORE_MEDIA_CHANGE);
	}
#endif

	/* Now go ask the device all about itself. */
	rslt = scsi_inquire(sc_link, &inqbuf, scsi_autoconf | SCSI_SILENT);
	if (rslt != 0) {
		SC_DEBUG(sc_link, SDEV_DB2, ("Bad LUN. rslt = %i\n", rslt));
		if (lun == 0)
			rslt = EINVAL;
		goto bad;
	}

	switch (inqbuf.device & SID_QUAL) {
	case SID_QUAL_RSVD:
	case SID_QUAL_BAD_LU:
	case SID_QUAL_LU_OFFLINE:
		SC_DEBUG(sc_link, SDEV_DB1,
		    ("Bad LUN. SID_QUAL = 0x%02x\n", inqbuf.device & SID_QUAL));
		goto bad;

	case SID_QUAL_LU_OK:
		if ((inqbuf.device & SID_TYPE) == T_NODEVICE) {
			SC_DEBUG(sc_link, SDEV_DB1,
		    	    ("Bad LUN. SID_TYPE = T_NODEVICE\n"));
			goto bad;
		}
		break;

	default:
		break;
	}

	if (lun == 0 || scsi->sc_link[target][0] == NULL)
		;
	else if (sc_link->flags & SDEV_UMASS)
		;
	else if (memcmp(&inqbuf, &scsi->sc_link[target][0]->inqdata,
	    sizeof inqbuf) == 0) {
		/* The device doesn't distinguish between LUNs. */
		SC_DEBUG(sc_link, SDEV_DB1, ("IDENTIFY not supported.\n"));
		rslt = EINVAL;
		goto bad;
	}

	finger = (const struct scsi_quirk_inquiry_pattern *)scsi_inqmatch(
	    &inqbuf, scsi_quirk_patterns,
	    sizeof(scsi_quirk_patterns)/sizeof(scsi_quirk_patterns[0]),
	    sizeof(scsi_quirk_patterns[0]), &priority);

	/*
	 * Based upon the inquiry flags we got back, and if we're
	 * at SCSI-2 or better, remove some limiting quirks.
	 */
	if (SCSISPC(inqbuf.version) >= 2) {
		if ((inqbuf.flags & SID_CmdQue) != 0)
			sc_link->quirks &= ~SDEV_NOTAGS;
		if ((inqbuf.flags & SID_Sync) != 0)
			sc_link->quirks &= ~SDEV_NOSYNC;
		if ((inqbuf.flags & SID_WBus16) != 0)
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
	 * Save INQUIRY.
	 */
	memcpy(&sc_link->inqdata, &inqbuf, sizeof(sc_link->inqdata));

	/*
	 * note what BASIC type of device it is
	 */
	if ((inqbuf.dev_qual2 & SID_REMOVABLE) != 0)
		sc_link->flags |= SDEV_REMOVABLE;

	sa.sa_sc_link = sc_link;
	sa.sa_inqbuf = &sc_link->inqdata;

	if ((cf = config_search(scsibussubmatch, (struct device *)scsi,
	    &sa)) == 0) {
		scsibusprint(&sa, scsi->sc_dev.dv_xname);
		printf(" not configured\n");
		goto bad;
	}

	/*
	 * Braindead USB devices, especially some x-in-1 media readers, try to
	 * 'help' by pretending any LUN is actually LUN 0 until they see a
	 * different LUN used in a command. So do an INQUIRY on LUN 1 at this
	 * point (since we are done with the data in inqbuf) to prevent such
	 * helpfulness before it causes confusion.
	 */
	if (lun == 0 && (sc_link->flags & SDEV_UMASS) &&
	    scsi->sc_link[target][1] == NULL && sc_link->luns > 1) {
		sc_link->lun = 1;
		scsi_inquire(sc_link, &inqbuf, scsi_autoconf | SCSI_SILENT);
	    	sc_link->lun = 0;
	}

	scsi->sc_link[target][lun] = sc_link;

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

bad:
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
