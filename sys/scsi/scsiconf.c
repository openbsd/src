/*	$NetBSD: scsiconf.c,v 1.46 1995/12/31 02:40:53 thorpej Exp $	*/

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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#if 0
#if NCALS > 0
	{ T_PROCESSOR, T_FIXED, 1,
	  0, 0, 0 },
#endif	/* NCALS */
#if NBLL > 0
	{ T_PROCESSOR, T_FIXED, 1,
	  "AEG     ", "READER          ", "V1.0" },
#endif	/* NBLL */
#if NKIL > 0
	{ T_SCANNER, T_FIXED, 0,
	  "KODAK   ", "IL Scanner 900  ", 0 },
#endif	/* NKIL */
#endif

/*
 * Declarations
 */
void scsi_probedev __P((struct scsibus_softc *, int, int));
int scsi_probe_bus __P((int bus, int target, int lun));

struct scsi_device probe_switch = {
	NULL,
	NULL,
	NULL,
	NULL,
};

int scsibusmatch __P((struct device *, void *, void *));
void scsibusattach __P((struct device *, struct device *, void *));

struct cfdriver scsibuscd = {
	NULL, "scsibus", scsibusmatch, scsibusattach, DV_DULL,
	sizeof(struct scsibus_softc)
};

int
scsibusmatch(parent, match, aux)
        struct device *parent;
        void *match, *aux;
{

	return 1;
}

/*
 * The routine called by the adapter boards to get all their
 * devices configured in.
 */
void
scsibusattach(parent, self, aux)
        struct device *parent, *self;
        void *aux;
{
	struct scsibus_softc *sb = (struct scsibus_softc *)self;
	struct scsi_link *sc_link_proto = aux;

	sc_link_proto->scsibus = sb->sc_dev.dv_unit;
	sb->adapter_link = sc_link_proto;
	printf("\n");

#if defined(SCSI_DELAY) && SCSI_DELAY > 2
	printf("%s: waiting for scsi devices to settle\n",
		sb->sc_dev.dv_xname);
#else	/* SCSI_DELAY > 2 */
#undef	SCSI_DELAY
#define SCSI_DELAY 2
#endif	/* SCSI_DELAY */
	delay(1000000 * SCSI_DELAY);

	scsi_probe_bus(sb->sc_dev.dv_unit, -1, -1);
}

int
scsibussubmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct scsibus_attach_args *sa = aux;
	struct scsi_link *sc_link = sa->sa_sc_link;

	if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != sc_link->target)
		return 0;
	if (cf->cf_loc[1] != -1 && cf->cf_loc[1] != sc_link->lun)
		return 0;
	return ((*cf->cf_driver->cd_match)(parent, match, aux));
}

/*
 * Probe the requested scsi bus. It must be already set up.
 * -1 requests all set up scsi busses.
 * target and lun optionally narrow the search if not -1
 */
int
scsi_probe_busses(bus, target, lun)
	int bus, target, lun;
{

	if (bus == -1) {
		for (bus = 0; bus < scsibuscd.cd_ndevs; bus++)
			if (scsibuscd.cd_devs[bus])
				scsi_probe_bus(bus, target, lun);
		return 0;
	} else {
		return scsi_probe_bus(bus, target, lun);
	}
}

/*
 * Probe the requested scsi bus. It must be already set up.
 * target and lun optionally narrow the search if not -1
 */
int
scsi_probe_bus(bus, target, lun)
	int bus, target, lun;
{
	struct scsibus_softc *scsi;
	int maxtarget, mintarget, maxlun, minlun;
	u_int8_t scsi_addr;

	if (bus < 0 || bus >= scsibuscd.cd_ndevs)
		return ENXIO;
	scsi = scsibuscd.cd_devs[bus];
	if (!scsi)
		return ENXIO;

	scsi_addr = scsi->adapter_link->adapter_target;

	if (target == -1) {
		maxtarget = 7;
		mintarget = 0;
	} else {
		if (target < 0 || target > 7)
			return EINVAL;
		maxtarget = mintarget = target;
	}

	if (lun == -1) {
		maxlun = 7;
		minlun = 0;
	} else {
		if (lun < 0 || lun > 7)
			return EINVAL;
		maxlun = minlun = lun;
	}

	for (target = mintarget; target <= maxtarget; target++) {
		if (target == scsi_addr)
			continue;
		for (lun = minlun; lun <= maxlun; lun++) {
			/*
			 * See if there's a device present, and configure it.
			 */
			scsi_probedev(scsi, target, lun);
			if ((scsi->moreluns & (1 << target)) == 0)
				break;
			/* otherwise something says we should look further */
		}
	}
	return 0;
}

void
scsi_strvis(dst, src, len)
	u_char *dst, *src;
	int len;
{

	/* Trim leading and trailing blanks and NULs. */
	while (len > 0 && (src[0] == ' ' || src[0] == '\0'))
		++src, --len;
	while (len > 0 && (src[len-1] == ' ' || src[len-1] == '\0'))
		--len;

	while (len > 0) {
		if (*src < 0x20 || *src >= 0x80) {
			/* non-printable characters */
			*dst++ = '\\';
			*dst++ = ((*src & 0300) >> 6) + '0';
			*dst++ = ((*src & 0070) >> 3) + '0';
			*dst++ = ((*src & 0007) >> 0) + '0';
		} else if (*src == '\\') {
			/* quote characters */
			*dst++ = '\\';
			*dst++ = '\\';
		} else {
			/* normal characters */
			*dst++ = *src;
		}
		++src, --len;
	}

	*dst++ = 0;
}

struct scsi_quirk_inquiry_pattern {
	struct scsi_inquiry_pattern pattern;
	u_int8_t quirks;
};

struct scsi_quirk_inquiry_pattern scsi_quirk_patterns[] = {
	{T_CDROM, T_REMOV,
	 "CHINON  ", "CD-ROM CDS-431  ", "",     SDEV_NOLUNS},
	{T_CDROM, T_REMOV,
	 "Chinon  ", "CD-ROM CDS-525  ", "",	 SDEV_NOLUNS},
	{T_CDROM, T_REMOV,
	 "CHINON  ", "CD-ROM CDS-535  ", "",     SDEV_NOLUNS},
	{T_CDROM, T_REMOV,
	 "DENON   ", "DRD-25X         ", "V",    SDEV_NOLUNS},
	{T_CDROM, T_REMOV,
	 "IMS     ", "CDD521/10       ", "2.06", SDEV_NOLUNS},
	{T_CDROM, T_REMOV,
	 "MEDIAVIS", "CDR-H93MV       ", "1.31", SDEV_NOLUNS},
	{T_CDROM, T_REMOV,
	 "NEC     ", "CD-ROM DRIVE:55 ", "",     SDEV_NOLUNS},
	{T_CDROM, T_REMOV,
	 "NEC     ", "CD-ROM DRIVE:83 ", "",     SDEV_NOLUNS},
	{T_CDROM, T_REMOV,
	 "NEC     ", "CD-ROM DRIVE:84 ", "",     SDEV_NOLUNS},
	{T_CDROM, T_REMOV,
	 "NEC     ", "CD-ROM DRIVE:841", "",     SDEV_NOLUNS},
	{T_CDROM, T_REMOV,
	 "SONY    ", "CD-ROM CDU-541  ", "",     SDEV_NOLUNS},
	{T_CDROM, T_REMOV,
	 "SONY    ", "CD-ROM CDU-55S  ", "",     SDEV_NOLUNS},
	{T_CDROM, T_REMOV,
	 "SONY    ", "CD-ROM CDU-8003A", "",     SDEV_NOLUNS},
	{T_CDROM, T_REMOV,
	 "SONY    ", "CD-ROM CDU-8012 ", "",     SDEV_NOLUNS},
	{T_CDROM, T_REMOV,
	 "TEAC    ", "CD-ROM          ", "1.06", SDEV_NOLUNS},
	{T_CDROM, T_REMOV,
	 "TEXEL   ", "CD-ROM          ", "1.06", SDEV_NOLUNS},
	{T_CDROM, T_REMOV,
	 "TEXEL   ", "CD-ROM DM-XX24 K", "1.10", SDEV_NOLUNS},

	{T_DIRECT, T_FIXED,
	 "EMULEX  ", "MD21/S2     ESDI", "A00",  SDEV_FORCELUNS},
	{T_DIRECT, T_FIXED,
	 "MAXTOR  ", "XT-3280         ", "",     SDEV_NOLUNS},
	{T_DIRECT, T_FIXED,
	 "MAXTOR  ", "XT-4380S        ", "",     SDEV_NOLUNS},
	{T_DIRECT, T_FIXED,
	 "MAXTOR  ", "MXT-1240S       ", "",     SDEV_NOLUNS},
	{T_DIRECT, T_FIXED,
	 "MAXTOR  ", "XT-4170S        ", "",     SDEV_NOLUNS},
	{T_DIRECT, T_FIXED,
	 "MAXTOR  ", "XT-8760S        ", "",     SDEV_NOLUNS},
	{T_DIRECT, T_FIXED,
	 "MAXTOR  ", "LXT-213S        ", "",     SDEV_NOLUNS},
	{T_DIRECT, T_FIXED,
	 "MAXTOR  ", "LXT-213S SUN0207", "",     SDEV_NOLUNS},
	{T_DIRECT, T_FIXED,
	 "MST     ", "SnapLink        ", "",     SDEV_NOLUNS},
	{T_DIRECT, T_FIXED,
	 "QUANTUM ", "LPS525S         ", "",     SDEV_NOLUNS},
	{T_DIRECT, T_FIXED,
	 "QUANTUM ", "P105S 910-10-94x", "",     SDEV_NOLUNS},
	{T_DIRECT, T_FIXED,
	 "QUANTUM ", "PD1225S         ", "",     SDEV_NOLUNS},
	{T_DIRECT, T_FIXED,
	 "QUANTUM ", "PD210S   SUN0207", "",     SDEV_NOLUNS},
	{T_DIRECT, T_FIXED,
	 "RODIME  ", "RO3000S         ", "",     SDEV_NOLUNS},
	{T_DIRECT, T_FIXED,
	 "SEAGATE ", "ST157N          ", "",     SDEV_NOLUNS},
	{T_DIRECT, T_FIXED,
	 "SEAGATE ", "ST296           ", "",     SDEV_NOLUNS},
	{T_DIRECT, T_FIXED,
	 "SEAGATE ", "ST296N          ", "",     SDEV_NOLUNS},
	{T_DIRECT, T_FIXED,
	 "TOSHIBA ", "MK538FB         ", "6027", SDEV_NOLUNS},

	/* XXX: QIC-36 tape behind Emulex adapter.  Very broken. */
	{T_SEQUENTIAL, T_REMOV,
	 "        ", "                ", "    ", SDEV_NOLUNS},
	{T_SEQUENTIAL, T_REMOV,
	 "CALIPER ", "CP150           ", "",     SDEV_NOLUNS},
	{T_SEQUENTIAL, T_REMOV,
	 "EXABYTE ", "EXB-8200        ", "",     SDEV_NOLUNS},
	{T_SEQUENTIAL, T_REMOV,
	 "SONY    ", "SDT-2000        ", "2.09", SDEV_NOLUNS},
	{T_SEQUENTIAL, T_REMOV,
	 "SONY    ", "SDT-5000        ", "3.",   SDEV_NOSYNCWIDE},
	{T_SEQUENTIAL, T_REMOV,
	 "SONY    ", "SDT-5200        ", "3.",   SDEV_NOLUNS},
	{T_SEQUENTIAL, T_REMOV,
	 "TANDBERG", " TDC 3600       ", "",     SDEV_NOLUNS},
	{T_SEQUENTIAL, T_REMOV,
	 "WANGTEK ", "5099ES SCSI",      "",     SDEV_NOLUNS},
	{T_SEQUENTIAL, T_REMOV,
	 "WANGTEK ", "5150ES SCSI",      "",     SDEV_NOLUNS},
	{T_SEQUENTIAL, T_REMOV,
	 "WangDAT ", "Model 1300      ", "02.4", SDEV_NOSYNCWIDE},
	{T_SEQUENTIAL, T_REMOV,
	 "WangDAT ", "Model 2600      ", "01.7", SDEV_NOSYNCWIDE},
	{T_SEQUENTIAL, T_REMOV,
	 "WangDAT ", "Model 3200      ", "02.2", SDEV_NOSYNCWIDE},
};

/*
 * given a target and lu, ask the device what
 * it is, and find the correct driver table
 * entry.
 */
void
scsi_probedev(scsi, target, lun)
	struct scsibus_softc *scsi;
	int target, lun;
{
	struct scsi_link *sc_link;
	static struct scsi_inquiry_data inqbuf;
	struct scsi_quirk_inquiry_pattern *finger;
	int priority;
	u_int8_t type;
	boolean removable;
	char *dtype, *qtype;
	char vendor[33], product[65], revision[17];
	struct scsibus_attach_args sa;
	struct cfdata *cf;

	/* Skip this slot if it is already attached. */
	if (scsi->sc_link[target][lun])
		return;

	sc_link = malloc(sizeof(*sc_link), M_DEVBUF, M_NOWAIT);
	*sc_link = *scsi->adapter_link;
	sc_link->target = target;
	sc_link->lun = lun;
	sc_link->device = &probe_switch;

	/*
	 * Ask the device what it is
	 */
#ifdef SCSIDEBUG
	if (target == DEBUGTARGET && lun == DEBUGLUN)
		sc_link->flags |= DEBUGLEVEL;
#endif /* SCSIDEBUG */

	(void) scsi_test_unit_ready(sc_link,
	    SCSI_AUTOCONF | SCSI_IGNORE_ILLEGAL_REQUEST | SCSI_IGNORE_NOT_READY | SCSI_IGNORE_MEDIA_CHANGE);

#ifdef SCSI_2_DEF
	/* some devices need to be told to go to SCSI2 */
	/* However some just explode if you tell them this.. leave it out */
	scsi_change_def(sc_link, SCSI_AUTOCONF | SCSI_SILENT);
#endif /* SCSI_2_DEF */

	/* Now go ask the device all about itself. */
	bzero(&inqbuf, sizeof(inqbuf));
	if (scsi_inquire(sc_link, &inqbuf, SCSI_AUTOCONF) != 0)
		goto bad;

	{
		int len = inqbuf.additional_length;
		while (len < 3)
			inqbuf.unused[len++] = '\0';
		while (len < 3 + 28)
			inqbuf.unused[len++] = ' ';
	}

	finger = (struct scsi_quirk_inquiry_pattern *)scsi_inqmatch(&inqbuf,
	    (caddr_t)scsi_quirk_patterns, 
	    sizeof(scsi_quirk_patterns)/sizeof(scsi_quirk_patterns[0]),
	    sizeof(scsi_quirk_patterns[0]), &priority);
	if (priority != 0)
		sc_link->quirks |= finger->quirks;
	if ((inqbuf.version & SID_ANSII) == 0 &&
	    (sc_link->quirks & SDEV_FORCELUNS) == 0)
		sc_link->quirks |= SDEV_NOLUNS;

	if ((sc_link->quirks & SDEV_NOLUNS) == 0)
		scsi->moreluns |= (1 << target);

	/*
	 * note what BASIC type of device it is
	 */
	type = inqbuf.device & SID_TYPE;
	removable = inqbuf.dev_qual2 & SID_REMOVABLE ? 1 : 0;

	if (removable)
		sc_link->flags |= SDEV_REMOVABLE;

	/*
	 * Any device qualifier that has the top bit set (qualifier&4 != 0)
	 * is vendor specific and won't match in this switch.
	 */
	dtype = 0;
	switch (inqbuf.device & SID_QUAL) {
	case SID_QUAL_LU_OK:
		qtype = "";
		break;

	case SID_QUAL_LU_OFFLINE:
		qtype = " offline";
		break;

	case SID_QUAL_RSVD:
	case SID_QUAL_BAD_LU:
		goto bad;

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
		case T_NODEVICE:
			goto bad;
		default:
			dtype = "unknown";
			break;
		}
	}

	scsi_strvis(vendor, inqbuf.vendor, 8);
	scsi_strvis(product, inqbuf.product, 16);
	scsi_strvis(revision, inqbuf.revision, 4);

	printf("%s targ %d lun %d: <%s, %s, %s> SCSI%d %d/%s %s%s\n",
	    ((struct device *)sc_link->adapter_softc)->dv_xname,
	    target, lun, vendor, product, revision,
	    inqbuf.version & SID_ANSII, type, dtype,
	    removable ? "removable" : "fixed", qtype);

	sa.sa_sc_link = sc_link;
	sa.sa_inqbuf = &inqbuf;

	if ((cf = config_search(scsibussubmatch, (struct device *)scsi, &sa)) != 0) {
		scsi->sc_link[target][lun] = sc_link;
		config_attach((struct device *)scsi, cf, &sa, NULL);
	} else
		goto bad;

	return;

bad:
	free(sc_link, M_DEVBUF);
	return;
}

/*
 * Return a priority based on how much of the inquiry data matches
 * the patterns for the particular driver.
 */
caddr_t
scsi_inqmatch(inqbuf, base, nmatches, matchsize, bestpriority)
	struct scsi_inquiry_data *inqbuf;
	caddr_t base;
	int nmatches, matchsize;
	int *bestpriority;
{
	u_int8_t type;
	boolean removable;
	caddr_t bestmatch;

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
