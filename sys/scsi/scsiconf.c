/*	$OpenBSD: scsiconf.c,v 1.223 2020/02/05 16:29:30 krw Exp $	*/
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/atomic.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#if NBIO > 0
#include <sys/ioctl.h>
#include <sys/scsiio.h>
#include <dev/biovar.h>
#endif /* NBIO > 0 */

/*
 * Declarations
 */
void	scsi_get_target_luns(struct scsi_link *, struct scsi_lun_array *);
int	scsi_probedev(struct scsibus_softc *, int, int, int);
void	scsi_add_link(struct scsibus_softc *, struct scsi_link *);
void	scsi_remove_link(struct scsibus_softc *, struct scsi_link *);
int	scsi_activate_link(struct scsibus_softc *, struct scsi_link *, int);
int	scsi_detach_link(struct scsibus_softc *, struct scsi_link *, int);

void	scsi_devid(struct scsi_link *);
int	scsi_devid_pg80(struct scsi_link *);
int	scsi_devid_pg83(struct scsi_link *);
int	scsi_devid_wwn(struct scsi_link *);

int	scsibusmatch(struct device *, void *, void *);
void	scsibusattach(struct device *, struct device *, void *);
int	scsibusactivate(struct device *, int);
int	scsibusdetach(struct device *, int);

int	scsibussubmatch(struct device *, void *, void *);

#if NBIO > 0
int	scsibus_bioctl(struct device *, u_long, caddr_t);
#endif /* NBIO > 0 */

struct cfattach scsibus_ca = {
	sizeof(struct scsibus_softc), scsibusmatch, scsibusattach,
	scsibusdetach, scsibusactivate
};

struct cfdriver scsibus_cd = {
	NULL, "scsibus", DV_DULL
};

int	scsi_autoconf = SCSI_AUTOCONF;

int	scsibusprint(void *, const char *);
void	scsibus_printlink(struct scsi_link *);

int	scsi_activate_bus(struct scsibus_softc *, int);
int	scsi_activate_target(struct scsibus_softc *, int, int);
int	scsi_activate_lun(struct scsibus_softc *, int, int, int);

int
scsiprint(void *aux, const char *pnp)
{
	/* Only "scsibus"es can attach to "scsi"s. */
	if (pnp)
		printf("scsibus at %s", pnp);

	return UNCONF;
}

int
scsibusmatch(struct device *parent, void *match, void *aux)
{
	return 1;
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
	if (sb->adapter_link->luns == 0)
		sb->adapter_link->luns = 8;

	printf(": %d targets", sb->adapter_link->adapter_buswidth);
	if (sb->adapter_link->adapter_target <
	    sb->adapter_link->adapter_buswidth)
		printf(", initiator %d", sb->adapter_link->adapter_target);
	if (sb->adapter_link->port_wwn != 0x0 &&
	    sb->adapter_link->node_wwn != 0x0) {
		printf(", WWPN %016llx, WWNN %016llx",
		    sb->adapter_link->port_wwn, sb->adapter_link->node_wwn);
	}
	printf("\n");

	/* Initialize shared data. */
	scsi_init();

	SLIST_INIT(&sb->sc_link_list);

#if NBIO > 0
	if (bio_register(&sb->sc_dev, scsibus_bioctl) != 0)
		printf("%s: unable to register bio\n", sb->sc_dev.dv_xname);
#endif /* NBIO > 0 */

	scsi_probe(sb, -1, -1);
}

int
scsibusactivate(struct device *dev, int act)
{
	struct scsibus_softc *sb = (struct scsibus_softc *)dev;

	return scsi_activate(sb, -1, -1, act);
}

int
scsi_activate(struct scsibus_softc *sb, int target, int lun, int act)
{
	if (target == -1 && lun == -1)
		return scsi_activate_bus(sb, act);

	if (target == -1)
		return 0;

	if (lun == -1)
		return scsi_activate_target(sb, target, act);

	return scsi_activate_lun(sb, target, lun, act);
}

int
scsi_activate_bus(struct scsibus_softc *sb, int act)
{
	int target, r, rv = 0;

	for (target = 0; target < sb->adapter_link->adapter_buswidth;
	     target++) {
		r = scsi_activate_target(sb, target, act);
		if (r)
			rv = r;
	}
	return rv;
}

int
scsi_activate_target(struct scsibus_softc *sb, int target, int act)
{
	struct scsi_link		*link;
	int				 r, rv = 0;

	SLIST_FOREACH(link, &sb->sc_link_list, bus_list) {
		if (link->target == target) {
			r = scsi_activate_link(sb, link, act);
			if (r)
				rv = r;
		}
	}
	return rv;
}

int
scsi_activate_lun(struct scsibus_softc *sb, int target, int lun, int act)
{
	struct scsi_link *link;

	link = scsi_get_link(sb, target, lun);
	if (link == NULL)
		return 0;

	return scsi_activate_link(sb, link, act);
}

int
scsi_activate_link(struct scsibus_softc *sb, struct scsi_link *link, int act)
{
	struct device			*dev;
	int				 rv = 0;

	dev = link->device_softc;
	switch (act) {
	case DVACT_DEACTIVATE:
		atomic_setbits_int(&link->state, SDEV_S_DYING);
		config_deactivate(dev);
		break;
	default:
		rv = config_suspend(dev, act);
		break;
	}
	return rv;
}

int
scsibusdetach(struct device *dev, int type)
{
	struct scsibus_softc		*sb = (struct scsibus_softc *)dev;
	int				 error;

#if NBIO > 0
	bio_unregister(&sb->sc_dev);
#endif /* NBIO > 0 */

	error = scsi_detach(sb, -1, -1, type);
	if (error != 0)
		return error;

	KASSERT(SLIST_EMPTY(&sb->sc_link_list));

	return 0;
}

int
scsibussubmatch(struct device *parent, void *match, void *aux)
{
	struct cfdata			*cf = match;
	struct scsi_attach_args		*sa = aux;
	struct scsi_link		*link = sa->sa_sc_link;

	if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != link->target)
		return 0;
	if (cf->cf_loc[1] != -1 && cf->cf_loc[1] != link->lun)
		return 0;

	return (*cf->cf_attach->ca_match)(parent, match, aux);
}

#if NBIO > 0
int
scsibus_bioctl(struct device *dev, u_long cmd, caddr_t addr)
{
	struct scsibus_softc		*sb = (struct scsibus_softc *)dev;
	struct sbioc_device		*sdev;

	switch (cmd) {
	case SBIOCPROBE:
		sdev = (struct sbioc_device *)addr;
		return scsi_probe(sb, sdev->sd_target, sdev->sd_lun);

	case SBIOCDETACH:
		sdev = (struct sbioc_device *)addr;
		return scsi_detach(sb, sdev->sd_target, sdev->sd_lun, 0);

	default:
		return ENOTTY;
	}
}
#endif /* NBIO > 0 */

void
scsi_probe_bus(struct scsibus_softc *sb)
{
	scsi_probe(sb, -1, -1);
}

void
scsi_get_target_luns(struct scsi_link *link0, struct scsi_lun_array *lunarray)
{
	struct scsi_report_luns_data	*report;
	int				 i, nluns, rv = 0;

	/* Initialize dumbscan result. Just in case. */
	report = NULL;
	for (i = 0; i < link0->luns; i++)
		lunarray->luns[i] = i;
	lunarray->count = link0->luns;
	lunarray->dumbscan = 1;

	/*
	 * ATAPI, USB and pre-SPC (i.e. pre-SCSI-3) devices can't ask
	 * for a report of valid LUNs.
	 */
	if ((link0->flags & (SDEV_UMASS | SDEV_ATAPI)) != 0 ||
	    SID_ANSII_REV(&link0->inqdata) < SCSI_REV_SPC)
		goto dumbscan;

	report = dma_alloc(sizeof(*report), PR_WAITOK);
	if (report == NULL)
		goto dumbscan;

	rv = scsi_report_luns(link0, REPORT_NORMAL, report,
	    sizeof(*report), scsi_autoconf | SCSI_SILENT |
	    SCSI_IGNORE_ILLEGAL_REQUEST | SCSI_IGNORE_NOT_READY |
	    SCSI_IGNORE_MEDIA_CHANGE, 10000);
	if (rv != 0)
		goto dumbscan;

	/*
	 * XXX In theory we should check if data is full, which
	 * would indicate it needs to be enlarged and REPORT
	 * LUNS tried again. Solaris tries up to 3 times with
	 * larger sizes for data.
	 */

	/* Return the reported Type-0 LUNs. Type-0 only! */
	lunarray->count = 0;
	lunarray->dumbscan = 0;
	nluns = _4btol(report->length) / RPL_LUNDATA_SIZE;
	for (i = 0; i < nluns; i++) {
		if (report->luns[i].lundata[0] != 0)
			continue;
		lunarray->luns[lunarray->count++] =
		    report->luns[i].lundata[RPL_LUNDATA_T0LUN];
	}

dumbscan:
	if (report != NULL)
		dma_free(report, sizeof(*report));
}

int
scsi_probe_target(struct scsibus_softc *sb, int target)
{
	/* Wild card target not allowed. */
	if (target == -1)
		return EINVAL;
	else
		return scsi_probe(sb, target, -1);
}

int
scsi_probe(struct scsibus_softc *sb, int target, int lun)
{
	struct scsi_lun_array		 lunarray;
	struct scsi_link		*alink = sb->adapter_link;
	struct scsi_link		*link0;
	int				 i, r, rv = 0;

	if (target == -1 && lun == -1) {
		/* Probe all luns on all targets on bus. */
		for (i = 0; i < alink->adapter_buswidth; i++) {
			r = scsi_probe(sb, i, -1);
			if (r != 0 && r != EINVAL)
				rv = r;
		}
		return rv;
	}

	if (target < 0 || target >= alink->adapter_buswidth ||
	    target == alink->adapter_target)
		return EINVAL;

	if (lun == -1) {
		/* Probe all luns on the target. */
		scsi_probedev(sb, target, 0, 0);
		link0 = scsi_get_link(sb, target, 0);
		if (link0 == NULL)
			return EINVAL;
		scsi_get_target_luns(link0, &lunarray);
		for (i = 0; i < lunarray.count; i++) {
			r = scsi_probedev(sb, target, lunarray.luns[i],
			    lunarray.dumbscan);
			if (r == EINVAL && lunarray.dumbscan == 1)
				return 0;
			if (r != 0 && r != EINVAL)
				rv = r;
		}
		return rv;
	}

	/* Probe lun on target. *NOT* a dumbscan! */
	return scsi_probedev(sb, target, lun, 0);
}

int
scsi_probe_lun(struct scsibus_softc *sb, int target, int lun)
{
	if (target == -1 || lun == -1)
		return EINVAL;
	else
		return scsi_probe(sb, target, lun);
}

int
scsi_detach(struct scsibus_softc *sb, int target, int lun, int flags)
{
	struct scsi_link	*alink = sb->adapter_link;
	struct scsi_link	*link, *tmp;
	int			 r, rv = 0;

	if (target == -1 && lun == -1) {
		/* Detach all links from bus. */
		while (!SLIST_EMPTY(&sb->sc_link_list)) {
			link = SLIST_FIRST(&sb->sc_link_list);
			r = scsi_detach_link(sb, link, flags);
			if (r != 0 && r != ENXIO)
				rv = r;
		}
		return rv;
	}

	if (target < 0 || target >= alink->adapter_buswidth ||
	    target == alink->adapter_target)
		return EINVAL;

	if (lun == -1) {
		/* Detach all links from target. */
		SLIST_FOREACH_SAFE(link, &sb->sc_link_list, bus_list, tmp) {
			if (link->target == target) {
				r = scsi_detach_link(sb, link, flags);
				if (r != 0 && r != ENXIO)
					rv = r;
			}
		}
		return rv;
	}

	/* Detach specific link from target. */
	link = scsi_get_link(sb, target, lun);
	if (link == NULL)
		return EINVAL;
	else
		return scsi_detach_link(sb, link, flags);
}

int
scsi_detach_target(struct scsibus_softc *sb, int target, int flags)
{
	/* Wildcard value is not allowed! */
	if (target == -1)
		return EINVAL;
	else
		return scsi_detach(sb, target, -1, flags);
}

int
scsi_detach_lun(struct scsibus_softc *sb, int target, int lun, int flags)
{
	/* Wildcard values are not allowed! */
	if (target == -1 || lun == -1)
		return EINVAL;
	else
		return scsi_detach(sb, target, lun, flags);
}

int
scsi_detach_link(struct scsibus_softc *sb, struct scsi_link *link, int flags)
{
	struct scsi_link		*alink = sb->adapter_link;
	int				 rv;

	if (!ISSET(flags, DETACH_FORCE) && ISSET(link->flags, SDEV_OPEN))
		return EBUSY;

	/* Detaching a device from scsibus is a five step process. */

	/* 1. Wake up processes sleeping for an xs. */
	scsi_link_shutdown(link);

	/* 2. Detach the device. */
	rv = config_detach(link->device_softc, flags);

	if (rv != 0)
		return rv;

	/* 3. If it's using the openings io allocator, clean that up. */
	if (ISSET(link->flags, SDEV_OWN_IOPL)) {
		scsi_iopool_destroy(link->pool);
		free(link->pool, M_DEVBUF, sizeof(*link->pool));
	}

	/* 4. Free up its state in the adapter. */
	if (alink->adapter->dev_free != NULL)
		alink->adapter->dev_free(link);

	/* 5. Free up its state in the midlayer. */
	if (link->id != NULL)
		devid_free(link->id);
	scsi_remove_link(sb, link);
	free(link, M_DEVBUF, sizeof(*link));

	return 0;
}

struct scsi_link *
scsi_get_link(struct scsibus_softc *sb, int target, int lun)
{
	struct scsi_link *link;

	SLIST_FOREACH(link, &sb->sc_link_list, bus_list) {
		if (link->target == target && link->lun == lun)
			return link;
	}

	return NULL;
}

void
scsi_add_link(struct scsibus_softc *sb, struct scsi_link *link)
{
	SLIST_INSERT_HEAD(&sb->sc_link_list, link, bus_list);
}

void
scsi_remove_link(struct scsibus_softc *sb, struct scsi_link *link)
{
	SLIST_REMOVE(&sb->sc_link_list, link, scsi_link, bus_list);
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
			/* Collapse whitespace and NULs to a single space. */
			if (last != ' ')
				*dst++ = ' ';
			last = ' ';
			break;
		case '\\':
			/* Quote backslashes. */
			*dst++ = '\\';
			*dst++ = '\\';
			last = '\\';
			break;
		default:
			if (*src < 0x20 || *src >= 0x80) {
				/* Non-printable characters to octal. */
				*dst++ = '\\';
				*dst++ = ((*src & 0300) >> 6) + '0';
				*dst++ = ((*src & 0070) >> 3) + '0';
				*dst++ = ((*src & 0007) >> 0) + '0';
			} else {
				/* Copy normal characters. */
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
	char				 visbuf[65];
	struct scsi_inquiry_data	*inqbuf;
	u_int8_t			*id;
	int				 i;

	printf(" targ %d lun %d: ", link->target, link->lun);

	inqbuf = &link->inqdata;

	scsi_strvis(visbuf, inqbuf->vendor, 8);
	printf("<%s, ", visbuf);
	scsi_strvis(visbuf, inqbuf->product, 16);
	printf("%s, ", visbuf);
	scsi_strvis(visbuf, inqbuf->revision, 4);
	printf("%s>", visbuf);

#ifdef SCSIDEBUG
	if (ISSET(link->flags, SDEV_ATAPI))
		printf(" ATAPI");
	else if (SID_ANSII_REV(inqbuf) < SCSI_REV_SPC)
		printf(" SCSI/%d", SID_ANSII_REV(inqbuf));
	else if (SID_ANSII_REV(inqbuf) == SCSI_REV_SPC)
		printf(" SCSI/SPC");
	else
		printf(" SCSI/SPC-%d", SID_ANSII_REV(inqbuf) - 2);
#endif /* SCSIDEBUG */

	if (ISSET(link->flags, SDEV_REMOVABLE))
		printf(" removable");

	if (link->id != NULL && link->id->d_type != DEVID_NONE) {
		id = (u_int8_t *)(link->id + 1);
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
		case DEVID_SERIAL:
			printf(" serial.");
			break;
		case DEVID_WWN:
			printf(" wwn.");
			break;
		}

		if (ISSET(link->id->d_flags, DEVID_F_PRINT)) {
			for (i = 0; i < link->id->d_len; i++) {
				if (id[i] == '\0' || id[i] == ' ') {
					/* skip leading blanks */
					/* collapse multiple blanks into one */
					if (i > 0 && id[i-1] != id[i])
						printf("_");
				} else if (id[i] < 0x20 || id[i] >= 0x80) {
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
#ifdef SCSIDEBUG
	printf("\n");
	sc_print_addr(link);
	printf("state %u, luns %u, openings %u\n",
	    link->state, link->luns, link->openings);

	sc_print_addr(link);
	printf("flags (0x%04x) ", link->flags);
	scsi_show_flags(link->flags, flagnames);
	printf("\n");

	sc_print_addr(link);
	printf("quirks (0x%04x) ", link->quirks);
	scsi_show_flags(link->quirks, quirknames);
#endif /* SCSIDEBUG */
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

	return UNCONF;
}

/*
 * Given a target and lun, ask the device what it is, and find the correct
 * driver table entry.
 *
 * Return 0 if further LUNs are possible, EINVAL if not.
 */
int
scsi_probedev(struct scsibus_softc *sb, int target, int lun, int dumbscan)
{
	struct scsi_attach_args			 sa;
	const struct scsi_quirk_inquiry_pattern	*finger;
	struct scsi_inquiry_data		*inqbuf, *usbinqbuf;
	struct scsi_link			*link, *link0;
	struct cfdata				*cf;
	int					 priority, rslt = 0;
	u_int16_t				 devquirks;

	/* Skip this slot if it is already attached and try the next LUN. */
	if (scsi_get_link(sb, target, lun) != NULL)
		return 0;

	link = malloc(sizeof(*link), M_DEVBUF, M_NOWAIT);
	if (link == NULL) {
		SC_DEBUG(link, SDEV_DB2, ("Bad LUN. can't allocate "
		    "scsi_link.\n"));
		return EINVAL;
	}

	*link = *sb->adapter_link;
	link->target = target;
	link->lun = lun;
	link->interpret_sense = scsi_interpret_sense;
	link->node_wwn = link->port_wwn = 0;
	TAILQ_INIT(&link->queue);

	SC_DEBUG(link, SDEV_DB2, ("scsi_link created.\n"));

	/* Ask the adapter if this will be a valid device. */
	if (sb->adapter_link->adapter->dev_probe != NULL &&
	    sb->adapter_link->adapter->dev_probe(link) != 0) {
		if (lun == 0) {
			SC_DEBUG(link, SDEV_DB2, ("Bad LUN 0. dev_probe() "
			    "failed.\n"));
			rslt = EINVAL;
		}
		goto free;
	}

	/*
	 * If we havent been given an io pool by now then fall back to
	 * using link->openings.
	 */
	if (link->pool == NULL) {
		link->pool = malloc(sizeof(*link->pool),
		    M_DEVBUF, M_NOWAIT);
		if (link->pool == NULL) {
			SC_DEBUG(link, SDEV_DB2, ("Bad LUN. can't allocate "
			    "link->pool.\n"));
			rslt = ENOMEM;
			goto bad;
		}
		scsi_iopool_init(link->pool, link,
		    scsi_default_get, scsi_default_put);

		SET(link->flags, SDEV_OWN_IOPL);
	}

	/*
	 * Tell drivers that are paying attention to avoid sync/wide/tags until
	 * INQUIRY data has been processed and the quirks information is
	 * complete. Some drivers set bits in quirks before we get here, so
	 * just add NOTAGS, NOWIDE and NOSYNC.
	 */
	devquirks = link->quirks;
	SET(link->quirks, SDEV_NOSYNC | SDEV_NOWIDE | SDEV_NOTAGS);

	/*
	 * Ask the device what it is.
	 */
#ifdef SCSIDEBUG
	if (((sb->sc_dev.dv_unit < 32) &&
	    ((1U << sb->sc_dev.dv_unit) & scsidebug_buses)) &&
	    ((target < 32) && ((1U << target) & scsidebug_targets)) &&
	    ((lun < 32) && ((1U << lun) & scsidebug_luns)))
		SET(link->flags, scsidebug_level);
#endif /* SCSIDEBUG */

	if (lun == 0) {
		/* Clear any outstanding errors. */
		scsi_test_unit_ready(link, TEST_READY_RETRIES,
		    scsi_autoconf | SCSI_IGNORE_ILLEGAL_REQUEST |
		    SCSI_IGNORE_NOT_READY | SCSI_IGNORE_MEDIA_CHANGE);
	}

	/* Now go ask the device all about itself. */
	inqbuf = dma_alloc(sizeof(*inqbuf), PR_NOWAIT | PR_ZERO);
	if (inqbuf == NULL) {
		SC_DEBUG(link, SDEV_DB2, ("Bad LUN. can't allocate inqbuf.\n"));
		rslt = ENOMEM;
		goto bad;
	}

	rslt = scsi_inquire(link, inqbuf, scsi_autoconf | SCSI_SILENT);
	memcpy(&link->inqdata, inqbuf, sizeof(link->inqdata));
	dma_free(inqbuf, sizeof(*inqbuf));

	if (rslt != 0) {
		if (lun == 0) {
			SC_DEBUG(link, SDEV_DB2, ("Bad LUN 0. inquiry rslt = "
			    "%i\n", rslt));
			rslt = EINVAL;
		}
		goto bad;
	}
	inqbuf = &link->inqdata;

	switch (inqbuf->device & SID_QUAL) {
	case SID_QUAL_RSVD:
	case SID_QUAL_BAD_LU:
	case SID_QUAL_LU_OFFLINE:
		SC_DEBUG(link, SDEV_DB1, ("Bad LUN. SID_QUAL = 0x%02x\n",
		    inqbuf->device & SID_QUAL));
		goto bad;

	case SID_QUAL_LU_OK:
		break;

	default:
		SC_DEBUG(link, SDEV_DB1, ("Vendor-specific SID_QUAL = 0x%02x\n",
		    inqbuf->device & SID_QUAL));
		break;
	}

	if ((inqbuf->device & SID_TYPE) == T_NODEVICE) {
		SC_DEBUG(link, SDEV_DB1,
		    ("Bad LUN. SID_TYPE = T_NODEVICE\n"));
		goto bad;
	}

	scsi_devid(link);

	link0 = scsi_get_link(sb, target, 0);
	if (lun == 0 || link0 == NULL)
		;
	else if (ISSET(link->flags, SDEV_UMASS))
		;
	else if (link->id != NULL && !DEVID_CMP(link0->id, link->id))
		;
	else if (dumbscan == 1 && memcmp(inqbuf, &link0->inqdata,
	    sizeof(*inqbuf)) == 0) {
		/* The device doesn't distinguish between LUNs. */
		SC_DEBUG(link, SDEV_DB1, ("Bad LUN. IDENTIFY not supported."
		    "\n"));
		rslt = EINVAL;
		goto free_devid;
	}

	link->quirks = devquirks;	/* Restore what the device wanted. */

	finger = (const struct scsi_quirk_inquiry_pattern *)scsi_inqmatch(
	    inqbuf, scsi_quirk_patterns,
	    nitems(scsi_quirk_patterns),
	    sizeof(scsi_quirk_patterns[0]), &priority);
	if (priority != 0)
		SET(link->quirks, finger->quirks);

	switch (SID_ANSII_REV(inqbuf)) {
	case SCSI_REV_0:
	case SCSI_REV_1:
		SET(link->quirks, SDEV_NOTAGS | SDEV_NOSYNC | SDEV_NOWIDE |
		    SDEV_NOSYNCCACHE);
		break;
	case SCSI_REV_2:
	case SCSI_REV_SPC:
	case SCSI_REV_SPC2:
		if (!ISSET(inqbuf->flags, SID_CmdQue))
			SET(link->quirks, SDEV_NOTAGS);
		if (!ISSET(inqbuf->flags, SID_Sync))
			SET(link->quirks, SDEV_NOSYNC);
		if (!ISSET(inqbuf->flags, SID_WBus16))
			SET(link->quirks, SDEV_NOWIDE);
		break;
	case SCSI_REV_SPC3:
	case SCSI_REV_SPC4:
	case SCSI_REV_SPC5:
		/* By this time SID_Sync and SID_WBus16 were obsolete. */
		if (!ISSET(inqbuf->flags, SID_CmdQue))
			SET(link->quirks, SDEV_NOTAGS);
		break;
	default:
		break;
	}

	/*
	 * If the device can't use tags, >1 opening may confuse it.
	 */
	if (ISSET(link->quirks, SDEV_NOTAGS))
		link->openings = 1;

	/*
	 * note what BASIC type of device it is
	 */
	if (ISSET(inqbuf->dev_qual2, SID_REMOVABLE))
		SET(link->flags, SDEV_REMOVABLE);

	sa.sa_sc_link = link;
	sa.sa_inqbuf = &link->inqdata;

	if ((cf = config_search(scsibussubmatch, (struct device *)sb,
	    &sa)) == 0) {
		scsibusprint(&sa, sb->sc_dev.dv_xname);
		printf(" not configured\n");
		goto free_devid;
	}

	/*
	 * Braindead USB devices, especially some x-in-1 media readers, try to
	 * 'help' by pretending any LUN is actually LUN 0 until they see a
	 * different LUN used in a command. So do an INQUIRY on LUN 1 at this
	 * point to prevent such helpfulness before it causes confusion.
	 */
	if (lun == 0 && ISSET(link->flags, SDEV_UMASS) &&
	    scsi_get_link(sb, target, 1) == NULL && link->luns > 1 &&
	    (usbinqbuf = dma_alloc(sizeof(*usbinqbuf), M_NOWAIT)) != NULL) {

		link->lun = 1;
		scsi_inquire(link, usbinqbuf, scsi_autoconf | SCSI_SILENT);
		link->lun = 0;

		dma_free(usbinqbuf, sizeof(*usbinqbuf));
	}

	scsi_add_link(sb, link);

	/*
	 * Generate a TEST_UNIT_READY command. This gives drivers waiting for
	 * valid quirks data a chance to set wide/sync/tag options
	 * appropriately. It also clears any outstanding ACA conditions that
	 * INQUIRY may leave behind.
	 *
	 * Do this now so that any messages generated by config_attach() do not
	 * have negotiation messages inserted into their midst.
	 */
	scsi_test_unit_ready(link, TEST_READY_RETRIES,
	    scsi_autoconf | SCSI_IGNORE_ILLEGAL_REQUEST |
	    SCSI_IGNORE_NOT_READY | SCSI_IGNORE_MEDIA_CHANGE);

	config_attach((struct device *)sb, cf, &sa, scsibusprint);

	return 0;

free_devid:
	if (link->id)
		devid_free(link->id);
bad:
	if (ISSET(link->flags, SDEV_OWN_IOPL))
		free(link->pool, M_DEVBUF, sizeof(*link->pool));

	if (sb->adapter_link->adapter->dev_free != NULL)
		sb->adapter_link->adapter->dev_free(link);
free:
	free(link, M_DEVBUF, sizeof(*link));
	return rslt;
}

/*
 * Return a priority based on how much of the inquiry data matches
 * the patterns for the particular driver.
 */
const void *
scsi_inqmatch(struct scsi_inquiry_data *inqbuf, const void *_base,
    int nmatches, int matchsize, int *bestpriority)
{
	const unsigned char		*base = (const unsigned char *)_base;
	const void			*bestmatch;
	int				 removable;

	/* Include the qualifier to catch vendor-unique types. */
	removable = ISSET(inqbuf->dev_qual2, SID_REMOVABLE) ? T_REMOV : T_FIXED;

	for (*bestpriority = 0, bestmatch = 0; nmatches--; base += matchsize) {
		struct scsi_inquiry_pattern *match = (void *)base;
		int priority, len;

		if (inqbuf->device != match->type)
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

#ifdef SCSIDEBUG
		printf("scsi_inqmatch: ");
		if (_base == &scsi_quirk_patterns)
			printf(" quirk ");
		else
			printf(" match ");

		printf("priority %d. %s %s <\"%s\", \"%s\", \"%s\">", priority,
		    devicetypenames[(match->type & SID_TYPE)],
		    (match->removable == T_FIXED) ? "T_FIXED" : "T_REMOV",
		    match->vendor, match->product, match->revision);

		if (_base == &scsi_quirk_patterns)
			printf(" quirks: 0x%04x",
			    ((struct scsi_quirk_inquiry_pattern *)match)->quirks
			);

		printf("\n");
#endif /* SCSIDEBUG */
		if (priority > *bestpriority) {
			*bestpriority = priority;
			bestmatch = base;
		}
	}

	return bestmatch;
}

void
scsi_devid(struct scsi_link *link)
{
	struct {
		struct scsi_vpd_hdr hdr;
		u_int8_t list[32];
	} __packed			*pg;
	size_t				 len;
	int				 pg80 = 0, pg83 = 0, i;

	if (link->id != NULL)
		return;

	pg = dma_alloc(sizeof(*pg), PR_WAITOK | PR_ZERO);

	if (SID_ANSII_REV(&link->inqdata) >= SCSI_REV_2) {
		if (scsi_inquire_vpd(link, pg, sizeof(*pg), SI_PG_SUPPORTED,
		    scsi_autoconf) != 0)
			goto wwn;

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
		if (pg80 && scsi_devid_pg80(link) == 0)
			goto done;
	}

wwn:
	scsi_devid_wwn(link);
done:
	dma_free(pg, sizeof(*pg));
}

int
scsi_devid_pg83(struct scsi_link *link)
{
	struct scsi_vpd_devid_hdr	 dhdr, chdr;
	struct scsi_vpd_hdr		*hdr = NULL;
	u_int8_t			*pg = NULL, *id;
	int				 len, pos, rv, type;
	int				 idtype = 0;
	u_char				 idflags;

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
	return rv;
}

int
scsi_devid_pg80(struct scsi_link *link)
{
	struct scsi_vpd_hdr		*hdr = NULL;
	u_int8_t			*pg = NULL;
	char				*id;
	size_t				 idlen;
	int				 len, pglen, rv;

	hdr = dma_alloc(sizeof(*hdr), PR_WAITOK | PR_ZERO);

	rv = scsi_inquire_vpd(link, hdr, sizeof(*hdr), SI_PG_SERIAL,
	    scsi_autoconf);
	if (rv != 0)
		goto freehdr;

	len = _2btol(hdr->page_length);
	if (len == 0) {
		rv = EINVAL;
		goto freehdr;
	}

	pglen = sizeof(*hdr) + len;
	pg = dma_alloc(pglen, PR_WAITOK | PR_ZERO);

	rv = scsi_inquire_vpd(link, pg, pglen, SI_PG_SERIAL, scsi_autoconf);
	if (rv != 0)
		goto free;

	idlen = sizeof(link->inqdata.vendor) +
	    sizeof(link->inqdata.product) + len;
	id = malloc(idlen, M_TEMP, M_WAITOK);
	memcpy(id, link->inqdata.vendor, sizeof(link->inqdata.vendor));
	memcpy(id + sizeof(link->inqdata.vendor), link->inqdata.product,
	    sizeof(link->inqdata.product));
	memcpy(id + sizeof(link->inqdata.vendor) +
	    sizeof(link->inqdata.product), pg + sizeof(*hdr), len);

	link->id = devid_alloc(DEVID_SERIAL, DEVID_F_PRINT,
	    sizeof(link->inqdata.vendor) + sizeof(link->inqdata.product) + len,
	    id);

	free(id, M_TEMP, idlen);

free:
	dma_free(pg, pglen);
freehdr:
	dma_free(hdr, sizeof(*hdr));
	return rv;
}

int
scsi_devid_wwn(struct scsi_link *link)
{
	u_int64_t wwnn;

	if (link->lun != 0 || link->node_wwn == 0)
		return EOPNOTSUPP;

	wwnn = htobe64(link->node_wwn);
	link->id = devid_alloc(DEVID_WWN, 0, sizeof(wwnn), (u_int8_t *)&wwnn);

	return 0;
}

struct devid *
devid_alloc(u_int8_t type, u_int8_t flags, u_int8_t len, u_int8_t *id)
{
	struct devid *d;

	d = malloc(sizeof(*d) + len, M_DEVBUF, M_WAITOK|M_CANFAIL);
	if (d == NULL)
		return NULL;

	d->d_type = type;
	d->d_flags = flags;
	d->d_len = len;
	d->d_refcount = 1;
	memcpy(d + 1, id, len);

	return d;
}

struct devid *
devid_copy(struct devid *d)
{
	d->d_refcount++;
	return d;
}

void
devid_free(struct devid *d)
{
	if (--d->d_refcount == 0)
		free(d, M_DEVBUF, sizeof(*d) + d->d_len);
}
