/*	$OpenBSD: biovar.h,v 1.3 2005/03/29 22:13:37 marco Exp $	*/

/*
 * Copyright (c) 2002 Niklas Hallqvist.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * Devices getting ioctls through this interface should use ioctl class 'B'
 * and command numbers starting from 32, lower ones are reserved for generic
 * ioctls.  All ioctl data must be structures which start with a void *
 * cookie.
 */

#include <sys/types.h>

struct bio_common {
	void *cookie;
};

#define BIOCLOCATE _IOWR('B', 0, struct bio_locate)
struct bio_locate {
	void *cookie;
	char *name;
};

#ifdef _KERNEL
int	bio_register(struct device *, int (*)(struct device *, u_long,
    caddr_t));
#endif

/* RAID section */

#define BIOC_MAX_CDB   16
#define BIOC_MAX_SENSE 32
#define BIOC_MAX_PHYSDISK 128	/* based on FC arrays */
#define BIOC_MAX_VIRTDISK 128	/* based on FC arrays */

/* ioctl tunnel defines */
/* SHALL be implemented */
#define BIOCPING _IOWR('B', 32, bioc_ping)
typedef struct _bioc_ping {
	void *cookie;
	int x;
} bioc_ping;

/* SHALL be implemented */
#define BIOCCAPABILITIES _IOWR('B', 33, bioc_capabilities)
typedef struct _bioc_capabilities {
	void *cookie;
	u_int64_t ioctls; /* bit field, 1 ioctl supported */
#define BIOC_PING         0x01
#define BIOC_ALARM        0x02
#define BIOC_PREP_REMOVAL 0x04
#define BIOC_REBUILD      0x08
#define BIOC_STATUS       0x10
#define BIOC_SCSICMD      0x20
#define BIOC_STARTSTOP    0x40
	u_int32_t raid_types; /* bit field, 1 supported raid type */
#define BIOC_RAID0  0x01
#define BIOC_RAID1  0x02
#define BIOC_RAID3  0x04
#define BIOC_RAID5  0x08
#define BIOC_RAID10 0x10
#define BIOC_RAID01 0x20
#define BIOC_RAID50 0x40
} bioc_capabilities;

/* OPTIONAL */
#define BIOCALARM _IOWR('B', 34, bioc_alarm)
typedef struct _bioc_alarm {
	void *cookie;
	u_int32_t opcode;
#define BIOCSALARM_DISABLE 0x00
#define BIOCSALARM_ENABLE  0x01
#define BIOCSALARM_SILENCE 0x02
#define BIOCGALARM_STATE   0x03
#define BIOCSALARM_TEST    0x04
	u_int8_t state; /* only used with GET function */
} bioc_alarm;

/* OPTIONAL */
#define BIOCSCSICMD _IOWR('B', 35, bioc_scsicmd)
typedef struct _bioc_scsicmd {
	void *cookie;

	/* in (kernel centric) */
	u_int8_t channel;
	u_int8_t target;
	u_int8_t cdb[BIOC_MAX_CDB];
	u_int8_t cdblen;
	u_int8_t direction; /* 0 = out, 1 = in, this is userland centric */
#define BIOC_DIROUT  0x00
#define BIOC_DIRIN   0x01
#define BIOC_DIRNONE 0x02

	/* out (kernel centric) */
	u_int8_t status;
	u_int8_t sensebuf[BIOC_MAX_SENSE];
	u_int8_t senselen;

	/* in & out (kernel centric) */
	void *data;
	u_int32_t datalen; /* going in it governs the maximum buffer size
			      going out it contains actual bytes transfered */
} bioc_scsicmd;

/* OPTIONAL */
#define BIOCSTARTSTOP _IOWR('B', 36, bioc_startstop)
typedef struct _bioc_startstop {
	void *cookie;
	u_int8_t opcode;
#define BIOCSUNIT_START 0x00
#define BIOCSUNIT_STOP  0x01
	u_int8_t channel;
	u_int8_t target;
} bioc_startstop;

/* SHALL be implemented */
#define BIOCSTATUS _IOWR('B', 37, bioc_status)
typedef struct _bioc_status {
	void *cookie;
	u_int8_t opcode;
#define BIOCGSTAT_CHANGE	0x00	/* any changes since last call? */
#define BIOCGSTAT_ALL		0x01	/* get all status */
#define BIOCGSTAT_PHYSDISK	0x02	/* get physical disk status only */
#define BIOCGSTAT_VIRTDISK	0x03	/* get virtual disk status only */
#define BIOCGSTAT_BATTERY	0x04	/* get battery status only */
#define BIOCGSTAT_ENCLOSURE	0x05	/* get enclosure status only */
#define BIOCGSTAT_TEMPERATURE	0x06	/* get temperature status only */
	u_int8_t status;		/* global status flag */
#define BIOC_STATOK	0x00		/* status is OK */
#define BIOC_STATDEGRAD 0x01		/* status is degraded */
#define BIOC_STATCRIT	0x02		/* status is critical */
#define BIOC_STATBAT	0x04		/* something wrong with battery */
#define BIOC_STATENC	0x08		/* something wrong with enclosure */
#define BIOC_STATTEMP	0x10		/* something is over/under heating */
	/* return fields used per request define in opcode */
	u_int8_t	channels;	/* max channels */
	u_int8_t	buswidth;	/* max physical drives per channel */
	/* filled in when called with BIOCGSTAT_PHYSDISK set */
	u_int8_t pdcount;		/* physical disk counter */
	u_int8_t physdisk[BIOC_MAX_PHYSDISK];
#define BIOC_PDUNUSED	0x00		/* disk not present */
#define BIOC_PDONLINE	0x01		/* disk present */
#define BIOC_PDOFFLINE	0x02		/* disk present but offline */
#define BIOC_PDINUSE	0x04		/* critical operation in progress */
	/* filled in when called with BIOCGSTAT_VIRTDISK set */
	u_int8_t vdcount;		/* virtual disk counter */
	u_int8_t virtdisk[BIOC_MAX_VIRTDISK];
#define BIOC_VDUNUSED	0x00		/* disk not present */
#define BIOC_VDONLINE	0x01		/* disk present */
#define BIOC_VDOFFLINE	0x02		/* disk present but offline */
#define BIOC_VDINUSE	0x04		/* critical operation in progress */
	/* filled in when called with BIOCGSTAT_BATTERY set */
	u_int8_t	batstat;	/* battery status */
#define BIOC_BATNOTPRES	0x00		/* battery not present */
#define BIOC_BATMISSING 0x01		/* battery removed */
#define BIOC_BATVOLTERR	0x02		/* battery low/high power */
#define BIOC_BATTEMP	0x04		/* battery over/under temp*/
	/* NOTYET: encloure status & temperature status */
} bioc_status;
