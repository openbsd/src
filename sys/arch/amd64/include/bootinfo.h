/*	$OpenBSD: bootinfo.h,v 1.1 2004/01/28 01:39:39 mickey Exp $	*/
/*	$NetBSD: bootinfo.h,v 1.2 2003/04/16 19:16:42 dsl Exp $	*/

/*
 * Copyright (c) 1997
 *	Matthias Drochner.  All rights reserved.
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
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
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
 *
 */

#ifndef _LOCORE

struct btinfo_common {
	int len;
	int type;
};

#define BTINFO_BOOTPATH 0
#define BTINFO_BOOTDISK 3
#define BTINFO_NETIF 4
#define BTINFO_CONSOLE 6
#define BTINFO_BIOSGEOM 7
#define BTINFO_SYMTAB 8
#define BTINFO_MEMMAP 9

struct btinfo_bootpath {
	struct btinfo_common common;
	char bootpath[80];
};

struct btinfo_bootdisk {
	struct btinfo_common common;
	int labelsector; /* label valid if != -1 */
	struct {
		u_int16_t type, checksum;
		char packname[16];
	} label;
	int biosdev;
	int partition;
};

struct btinfo_netif {
	struct btinfo_common common;
	char ifname[16];
	int bus;
#define BI_BUS_ISA 0
#define BI_BUS_PCI 1
	union {
		unsigned int iobase; /* ISA */
		unsigned int tag; /* PCI, BIOS format */
	} addr;
};

struct btinfo_console {
	struct btinfo_common common;
	char devname[16];
	int addr;
	int speed;
};

struct btinfo_symtab {
	struct btinfo_common common;
	int nsym;
	int ssym;
	int esym;
};

struct bi_memmap_entry {
	u_int64_t addr;		/* beginning of block */	/* 8 */
	u_int64_t size;		/* size of block */		/* 8 */
	u_int32_t type;		/* type of block */		/* 4 */
} __attribute__((packed));				/*	== 20 */

#define	BIM_Memory	1	/* available RAM usable by OS */
#define	BIM_Reserved	2	/* in use or reserved by the system */
#define	BIM_ACPI	3	/* ACPI Reclaim memory */
#define	BIM_NVS		4	/* ACPI NVS memory */

struct btinfo_memmap {
	struct btinfo_common common;
	int num;
	struct bi_memmap_entry entry[1]; /* var len */
};

#include <machine/disklabel.h>

/*
 * Structure describing disk info as seen by the BIOS.
 */
struct bi_biosgeom_entry {
	int		sec, head, cyl;		/* geometry */
	u_int64_t	totsec;			/* LBA sectors from ext int13 */
	int		flags, dev;		/* flags, BIOS device # */
#define BI_GEOM_INVALID		0x000001
#define BI_GEOM_EXTINT13	0x000002
#ifdef BIOSDISK_EXT13INFO_V3
#define BI_GEOM_BADCKSUM	0x000004	/* v3.x checksum invalid */
#define BI_GEOM_BUS_MASK	0x00ff00	/* connecting bus type */
#define BI_GEOM_BUS_ISA		0x000100
#define BI_GEOM_BUS_PCI		0x000200
#define BI_GEOM_BUS_OTHER	0x00ff00
#define BI_GEOM_IFACE_MASK	0xff0000	/* interface type */
#define BI_GEOM_IFACE_ATA	0x010000
#define BI_GEOM_IFACE_ATAPI	0x020000
#define BI_GEOM_IFACE_SCSI	0x030000
#define BI_GEOM_IFACE_USB	0x040000
#define BI_GEOM_IFACE_1394	0x050000	/* Firewire */
#define BI_GEOM_IFACE_FIBRE	0x060000	/* Fibre channel */
#define BI_GEOM_IFACE_OTHER	0xff0000	
	unsigned int	cksum;			/* MBR checksum */
	u_int		interface_path;		/* ISA iobase PCI bus/dev/fun */
	u_int64_t	device_path;
	int		res0;			/* future expansion; 0 now */
#else
	unsigned int	cksum;			/* MBR checksum */
	int		res0, res1, res2, res3;	/* future expansion; 0 now */
#endif
	struct dos_partition dosparts[NDOSPART]; /* MBR itself */
} __attribute__((packed));

struct btinfo_biosgeom {
	struct btinfo_common common;
	int num;
	struct bi_biosgeom_entry disk[1]; /* var len */
};

#ifdef _KERNEL
void *lookup_bootinfo __P((int));
#endif
#endif /* _LOCORE */

#ifdef _KERNEL
#define BOOTINFO_MAXSIZE 4096
#endif
