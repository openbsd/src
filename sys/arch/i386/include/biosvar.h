/*	$OpenBSD: biosvar.h,v 1.55 2011/03/23 16:54:35 pirofti Exp $	*/

/*
 * Copyright (c) 1997-1999 Michael Shalayeff
 * All rights reserved.
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
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MACHINE_BIOSVAR_H_
#define _MACHINE_BIOSVAR_H_

	/* some boxes put apm data seg in the 2nd page */
#define	BOOTARG_OFF	(NBPG*2)
#define	BOOTARG_LEN	(NBPG*1)
#define	BOOTBIOS_ADDR	(0x7c00)
#define	BOOTBIOS_MAXSEC	((1 << 28) - 1)

	/* physical page for ptp 0 need for various tramps */
#define PTP0_PA		(NBPG*3)	

	/* BIOS configure flags */
#define	BIOSF_BIOS32	0x0001
#define	BIOSF_PCIBIOS	0x0002
#define	BIOSF_PROMSCAN	0x0004
#define	BIOSF_SMBIOS	0x0008

/* BIOS media ID */
#define BIOSM_F320K	0xff	/* floppy ds/sd  8 spt */
#define	BIOSM_F160K	0xfe	/* floppy ss/sd  8 spt */
#define	BIOSM_F360K	0xfd	/* floppy ds/sd  9 spt */
#define	BIOSM_F180K	0xfc	/* floppy ss/sd  9 spt */
#define	BIOSM_ROMD	0xfa	/* ROM disk */
#define	BIOSM_F120M	0xf9	/* floppy ds/hd 15 spt 5.25" */
#define	BIOSM_F720K	0xf9	/* floppy ds/dd  9 spt 3.50" */
#define	BIOSM_HD	0xf8	/* hard drive */
#define	BIOSM_F144K	0xf0	/* floppy ds/hd 18 spt 3.50" */
#define	BIOSM_OTHER	0xf0	/* any other */

/*
 * BIOS memory maps
 */
#define	BIOS_MAP_END	0x00	/* End of array XXX - special */
#define	BIOS_MAP_FREE	0x01	/* Usable memory */
#define	BIOS_MAP_RES	0x02	/* Reserved memory */
#define	BIOS_MAP_ACPI	0x03	/* ACPI Reclaim memory */
#define	BIOS_MAP_NVS	0x04	/* ACPI NVS memory */

/*
 * Optional ROM header
 */
typedef
struct bios_romheader {
	u_int16_t	signature;	/* 0xaa55 */
	u_int8_t	len;		/* length in pages (512 bytes) */
	u_int32_t	entry;		/* initialization entry point */
	u_int8_t	reserved[19];
	u_int16_t	pnpheader;	/* offset to PnP expansion header */
} __packed *bios_romheader_t;

/*
 * BIOS32
 */
typedef
struct bios32_header {
	u_int32_t	signature;	/* 00: signature "_32_" */
	u_int32_t	entry;		/* 04: entry point */
	u_int8_t	rev;		/* 08: revision */
	u_int8_t	length;		/* 09: header length */
	u_int8_t	cksum;		/* 0a: modulo 256 checksum */
	u_int8_t	reserved[5];
} __packed *bios32_header_t;

typedef
struct bios32_entry_info {
	u_int32_t	bei_base;
	u_int32_t	bei_size;
	u_int32_t	bei_entry;
} __packed *bios32_entry_info_t;

typedef
struct bios32_entry {
	u_int32_t	offset;
	u_int16_t	segment;
} __packed *bios32_entry_t;

#define	BIOS32_START	0xe0000
#define	BIOS32_SIZE	0x20000
#define	BIOS32_END	(BIOS32_START + BIOS32_SIZE - 0x10)

#define	BIOS32_MAKESIG(a, b, c, d) \
	((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))
#define	BIOS32_SIGNATURE	BIOS32_MAKESIG('_', '3', '2', '_')
#define	PCIBIOS_SIGNATURE	BIOS32_MAKESIG('$', 'P', 'C', 'I')
#define	SMBIOS_SIGNATURE	BIOS32_MAKESIG('_', 'S', 'M', '_')

/*
 * CTL_BIOS definitions.
 */
#define	BIOS_DEV		1	/* int: BIOS boot device */
#define	BIOS_DISKINFO		2	/* struct: BIOS boot device info */
#define BIOS_CKSUMLEN		3	/* int: disk cksum block count */
#define	BIOS_MAXID		4	/* number of valid machdep ids */

#define	CTL_BIOS_NAMES { \
	{ 0, 0 }, \
	{ "biosdev", CTLTYPE_INT }, \
	{ "diskinfo", CTLTYPE_STRUCT }, \
	{ "cksumlen", CTLTYPE_INT }, \
}

#define	BOOTARG_MEMMAP 0
typedef struct _bios_memmap {
	u_int64_t addr;		/* Beginning of block */
	u_int64_t size;		/* Size of block */
	u_int32_t type;		/* Type of block */
} __packed bios_memmap_t;

/* Info about disk from the bios, plus the mapping from
 * BIOS numbers to BSD major (driver?) number.
 *
 * Also, do not bother with BIOSN*() macros, just parcel
 * the info out, and use it like this.  This makes for less
 * of a dependance on BIOSN*() macros having to be the same
 * across /boot, /bsd, and userland.
 */
#define	BOOTARG_DISKINFO 1
typedef struct _bios_diskinfo {
	/* BIOS section */
	int bios_number;	/* BIOS number of drive (or -1) */
	u_int bios_cylinders;	/* BIOS cylinders */
	u_int bios_heads;	/* BIOS heads */
	u_int bios_sectors;	/* BIOS sectors */
	int bios_edd;		/* EDD support */

	/* BSD section */
	dev_t bsd_dev;		/* BSD device */

	/* Checksum section */
	u_int32_t checksum;	/* Checksum for drive */

	/* Misc. flags */
	u_int32_t flags;
#define BDI_INVALID	0x00000001	/* I/O error during checksumming */
#define BDI_GOODLABEL	0x00000002	/* Had SCSI or ST506/ESDI disklabel */
#define BDI_BADLABEL	0x00000004	/* Had another disklabel */
#define BDI_EL_TORITO	0x00000008	/* 2,048-byte sectors */
#define BDI_PICKED	0x80000000	/* kernel-only: cksum matched */

} __packed bios_diskinfo_t;

#define	BOOTARG_APMINFO 2
typedef struct _bios_apminfo {
	/* APM_CONNECT returned values */
	u_int	apm_detail;
	u_int	apm_code32_base;
	u_int	apm_code16_base;
	u_int	apm_code_len;
	u_int	apm_data_base;
	u_int	apm_data_len;
	u_int	apm_entry;
	u_int	apm_code16_len;
} __packed bios_apminfo_t;

#define	BOOTARG_CKSUMLEN 3		/* u_int32_t */

#define	BOOTARG_PCIINFO 4
typedef struct _bios_pciinfo {
	/* PCI BIOS v2.0+ - Installation check values */
	u_int32_t	pci_chars;	/* Characteristics (%eax) */
	u_int32_t	pci_rev;	/* BCD Revision (%ebx) */
	u_int32_t	pci_entry32;	/* PM entry point for PCI BIOS */
	u_int32_t	pci_lastbus;	/* Number of last PCI bus */
} __packed bios_pciinfo_t;

#define	BOOTARG_CONSDEV	5
typedef struct _bios_consdev {
	dev_t	consdev;
	int	conspeed;
} __packed bios_consdev_t;

#define BOOTARG_SMPINFO 6		/* struct mp_float[] */

#define BOOTARG_BOOTMAC	7
typedef struct _bios_bootmac {
	char	mac[6];
} __packed bios_bootmac_t;

#define BOOTARG_DDB 8
typedef struct _bios_ddb {
	int	db_console;
} __packed bios_ddb_t;

#if defined(_KERNEL) || defined (_STANDALONE)

#ifdef _LOCORE
#define	DOINT(n)	int	$0x20+(n)
#else
#define	DOINT(n)	"int $0x20+(" #n ")"

extern volatile struct BIOS_regs {
	u_int32_t	biosr_ax;
	u_int32_t	biosr_cx;
	u_int32_t	biosr_dx;
	u_int32_t	biosr_bx;
	u_int32_t	biosr_bp;
	u_int32_t	biosr_si;
	u_int32_t	biosr_di;
	u_int32_t	biosr_ds;
	u_int32_t	biosr_es;
} __packed BIOS_regs;

#ifdef _KERNEL
#include <machine/bus.h>

struct bios_attach_args {
	char		*ba_name;
	u_int		ba_func;
	bus_space_tag_t	ba_iot;
	bus_space_tag_t	ba_memt;
	union {
		void		*_p;
		bios_apminfo_t	*_ba_apmp;
		paddr_t		_ba_acpipbase;
	} _;
};

#define	ba_apmp		_._ba_apmp
#define ba_acpipbase	_._ba_acpipbase

struct consdev;
struct proc;

int bios_sysctl(int *, u_int, void *, size_t *, void *, size_t, struct proc *);

void bios_getopt(void);

/* bios32.c */
int  bios32_service(u_int32_t, bios32_entry_t, bios32_entry_info_t);

extern u_int bootapiver;
extern bios_memmap_t *bios_memmap;
extern void *bios_smpinfo;
extern bios_pciinfo_t *bios_pciinfo;

#endif /* _KERNEL */
#endif /* _LOCORE */
#endif /* _KERNEL || _STANDALONE */

#endif /* _MACHINE_BIOSVAR_H_ */
