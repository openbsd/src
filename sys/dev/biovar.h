/*	$OpenBSD: biovar.h,v 1.8 2005/07/29 16:01:29 marco Exp $	*/

/*
 * Copyright (c) 2002 Niklas Hallqvist.  All rights reserved.
 * Copyright (c) 2005 Marco Peereboom.  All rights reserved.
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

#define BIOCINQ _IOWR('B', 32, bioc_inq)
typedef struct _bioc_inq {
	void *cookie;

	int novol;		/* nr of volumes */
	int nodisk;		/* nr of total disks */
} bioc_inq;

#define BIOCDISK _IOWR('B', 33, bioc_disk)
/* structure that represents a disk in a RAID volume */
typedef struct _bioc_disk {
	void *cookie;

	int volid;		/* associate with volume, if -1 unused */
	int diskid;		/* virtual disk id */
	int status;		/* current status */
#define BIOC_SDONLINE		0x00
#define BIOC_SDONLINE_S		"Online"
#define BIOC_SDOFFLINE		0x01
#define BIOC_SDOFFLINE_S	"Offline"
#define BIOC_SDFAILED		0x02
#define BIOC_SDFAILED_S 	"Failed"
#define BIOC_SDREBUILD		0x03
#define BIOC_SDREBUILD_S	"Rebuild"
#define BIOC_SDHOTSPARE		0x04
#define BIOC_SDHOTSPARE_S	"Hot spare"
#define BIOC_SDUNUSED		0x05
#define BIOC_SDUNUSED_S		"Unused"
#define BIOC_SDINVALID		0xff
#define BIOC_SDINVALID_S	"Invalid"
	int resv;		/* align */

	quad_t size;		/* size of the disk */

	/* this is provided by the physical disks if suported */
	char vendor[8];		/* vendor string */
	char product[16];	/* product string */
	char revision[4];	/* revision string */
	char pad[4];		/* zero terminate in here */

	/* XXX get this too? */
				/* serial number */
} bioc_disk;

#define BIOCVOL _IOWR('B', 34, bioc_vol)
/* structure that represents a RAID volume */
typedef struct _bioc_vol {
	void *cookie;

	int volid;		/* volume id */
	int resv1;		/* for binary compatibility */
	int status;		/* current status */
#define BIOC_SVONLINE		0x00
#define BIOC_SVONLINE_S		"Online"
#define BIOC_SVOFFLINE		0x01
#define BIOC_SVOFFLINE_S	"Offline"
#define BIOC_SVDEGRADED		0x02
#define BIOC_SVDEGRADED_S	"Degraded"
#define BIOC_SVINVALID		0xff
#define BIOC_SVINVALID_S	"Invalid"
	int resv2;		/* align */
	quad_t size;		/* size of the disk */
	int level;		/* raid level */
	int nodisk;		/* nr of drives */

	/* this is provided by the RAID card */
	char vendor[8];		/* vendor string */
	char product[16];	/* product string */
	char revision[4];	/* revision string */
	char pad[4];		/* zero terminate in here */
} bioc_vol;

#define BIOC_INQ	0x01
#define BIOC_DISK	0x02
#define BIOC_VOL	0x04
