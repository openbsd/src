/*	$OpenBSD: part.c,v 1.149 2023/04/02 18:44:13 miod Exp $	*/

/*
 * Copyright (c) 1997 Tobias Weingartner
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

#include <sys/types.h>
#include <sys/disklabel.h>

#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uuid.h>

#include "part.h"
#include "disk.h"
#include "misc.h"
#include "gpt.h"

struct mbr_type {
	int	 mt_type;
	char	*mt_desc;
};

/*
* MBR type sources:
*	OpenBSD Historical usage
* 	https://en.wikipedia.org/wiki/Partition_type#List_of_partition_IDs
*	https://www.win.tue.nl/~aeb/partitions/partition_types-1.html
*/
const struct mbr_type		mbr_types[] = {
	{ 0x00, NULL	},   /* unused */
	{ 0x01, NULL	},   /* Primary DOS with 12 bit FAT */
	{ 0x02, NULL	},   /* XENIX / filesystem */
	{ 0x03, NULL	},   /* XENIX /usr filesystem */
	{ 0x04, NULL	},   /* Primary DOS with 16 bit FAT */
	{ 0x05, NULL	},   /* Extended DOS */
	{ 0x06, NULL	},   /* Primary 'big' DOS (> 32MB) */
	{ 0x07, NULL	},   /* NTFS */
	{ 0x08, NULL	},   /* AIX filesystem */
	{ 0x09, NULL	},   /* AIX boot partition or Coherent */
	{ 0x0A, NULL	},   /* OS/2 Boot Manager or OPUS */
	{ 0x0B, NULL	},   /* Primary Win95 w/ 32-bit FAT */
	{ 0x0C, NULL	},   /* Primary Win95 w/ 32-bit FAT LBA-mapped */
	{ 0x0E, NULL	},   /* Primary DOS w/ 16-bit FAT, CHS-mapped */
	{ 0x0F, NULL	},   /* Extended DOS LBA-mapped */
	{ 0x10, NULL	},   /* OPUS */
	{ 0x11, NULL	},   /* OS/2 BM: hidden DOS 12-bit FAT */
	{ 0x12, NULL	},   /* Compaq Diagnostics */
	{ 0x14, NULL	},   /* OS/2 BM: hidden DOS 16-bit FAT <32M or Novell DOS 7.0 bug */
	{ 0x16, NULL	},   /* OS/2 BM: hidden DOS 16-bit FAT >=32M */
	{ 0x17, NULL	},   /* OS/2 BM: hidden IFS */
	{ 0x18, NULL	},   /* AST Windows swapfile */
	{ 0x19, NULL	},   /* Willowtech Photon coS */
	{ 0x1C, NULL	},   /* IBM ThinkPad recovery partition */
	{ 0x20, NULL	},   /* Willowsoft OFS1 */
	{ 0x24, NULL	},   /* NEC DOS */
	{ 0x27, NULL	},   /* Windows hidden Recovery Partition */
	{ 0x38, NULL	},   /* Theos */
	{ 0x39, NULL	},   /* Plan 9 */
	{ 0x40, NULL	},   /* VENIX 286 or LynxOS */
	{ 0x41, NULL 	},   /* Linux/MINIX (sharing disk with DRDOS) or Personal RISC boot */
	{ 0x42, NULL 	},   /* SFS or Linux swap (sharing disk with DRDOS) */
	{ 0x43, NULL 	},   /* Linux native (sharing disk with DRDOS) */
	{ 0x4D, NULL	},   /* QNX 4.2 Primary */
	{ 0x4E, NULL	},   /* QNX 4.2 Secondary */
	{ 0x4F, NULL	},   /* QNX 4.2 Tertiary */
	{ 0x50, NULL	},   /* DM (disk manager) */
	{ 0x51, NULL	},   /* DM6 Aux1 (or Novell) */
	{ 0x52, NULL	},   /* CP/M or Microport SysV/AT */
	{ 0x53, NULL	},   /* DM6 Aux3 */
	{ 0x54, NULL	},   /* Ontrack */
	{ 0x55, NULL	},   /* EZ-Drive (disk manager) */
	{ 0x56, NULL	},   /* Golden Bow (disk manager) */
	{ 0x5C, NULL	},   /* Priam Edisk (disk manager) */
	{ 0x61, NULL	},   /* SpeedStor */
	{ 0x63, NULL	},   /* ISC, System V/386, GNU HURD or Mach */
	{ 0x64, NULL	},   /* Novell NetWare 2.xx */
	{ 0x65, NULL	},   /* Novell NetWare 3.xx */
	{ 0x66, NULL	},   /* Novell 386 NetWare */
	{ 0x67, NULL	},   /* Novell */
	{ 0x68, NULL	},   /* Novell */
	{ 0x69, NULL	},   /* Novell */
	{ 0x70, NULL	},   /* DiskSecure Multi-Boot */
	{ 0x75, NULL	},   /* PCIX */
	{ 0x80, NULL	},   /* Minix 1.1 ... 1.4a */
	{ 0x81, NULL	},   /* Minix 1.4b ... 1.5.10 */
	{ 0x82, NULL	},   /* Linux swap */
	{ 0x83, NULL	},   /* Linux filesystem */
	{ 0x84, NULL	},   /* OS/2 hidden C: drive */
	{ 0x85, NULL	},   /* Linux extended */
	{ 0x86, NULL	},   /* NT FAT volume set */
	{ 0x87, NULL	},   /* NTFS volume set or HPFS mirrored */
	{ 0x8E, NULL	},   /* Linux LVM */
	{ 0x93, NULL	},   /* Amoeba filesystem */
	{ 0x94, NULL	},   /* Amoeba bad block table */
	{ 0x99, NULL	},   /* Mylex EISA SCSI */
	{ 0x9F, NULL	},   /* BSDI BSD/OS */
	{ 0xA0, NULL	},   /* Phoenix NoteBIOS save-to-disk */
	{ 0xA5, NULL	},   /* FreeBSD */
	{ 0xA6, NULL	},   /* OpenBSD */
	{ 0xA7, NULL	},   /* NEXTSTEP */
	{ 0xA8, NULL	},   /* MacOS X main partition */
	{ 0xA9, NULL	},   /* NetBSD */
	{ 0xAB, NULL	},   /* MacOS X boot partition */
	{ 0xAF, NULL	},   /* MacOS X HFS+ partition */
	{ 0xB7, NULL	},   /* BSDI BSD/386 filesystem */
	{ 0xB8, NULL	},   /* BSDI BSD/386 swap */
	{ 0xBF, NULL	},   /* Solaris */
	{ 0xC0, NULL	},   /* CTOS */
	{ 0xC1, NULL	},   /* DRDOS/sec (FAT-12) */
	{ 0xC4, NULL	},   /* DRDOS/sec (FAT-16, < 32M) */
	{ 0xC6, NULL	},   /* DRDOS/sec (FAT-16, >= 32M) */
	{ 0xC7, NULL	},   /* Syrinx (Cyrnix?) or HPFS disabled */
	{ 0xDB, NULL	},   /* Concurrent CPM or C.DOS or CTOS */
	{ 0xDE, NULL	},   /* Dell maintenance partition */
	{ 0xE1, NULL	},   /* DOS access or SpeedStor 12-bit FAT extended partition */
	{ 0xE3, NULL	},   /* DOS R/O or SpeedStor or Storage Dimensions */
	{ 0xE4, NULL	},   /* SpeedStor 16-bit FAT extended partition < 1024 cyl. */
	{ 0xEB, NULL	},   /* BeOS for Intel */
	{ 0xEE, NULL	},   /* EFI Protective Partition */
	{ 0xEF, NULL	},   /* EFI System Partition */
	{ 0xF1, NULL	},   /* SpeedStor or Storage Dimensions */
	{ 0xF2, NULL	},   /* DOS 3.3+ Secondary */
	{ 0xF4, NULL	},   /* SpeedStor >1024 cyl. or LANstep or IBM PS/2 IML */
	{ 0xFF, NULL	},   /* Xenix Bad Block Table */
};

struct gpt_type {
	int	 gt_attr;
#define	GTATTR_PROTECT		(1 << 0)
#define	GTATTR_PROTECT_EFISYS	(1 << 1)
	char	*gt_desc;
	char	*gt_guid;
};

/*
 * GPT GUID sources:
 *
 * UEFI: UEFI Specification 2.9, March 2021, Section 5.3.3, Table 5.7
 * Wikipedia: https://en.wikipedia.org/wiki/GUID_Partition_Table
 * NetBSD: /usr/src/sys/sys/disklabel_gpt.h
 * FreeBSD: /usr/src/sys/sys/disk/gpt.h.
 * DragonFlyBSD: /usr/src/sys/sys/disk/gpt.h.
 * Systemd:https://uapi-group.org/specifications/specs/discoverable_partitions_specification/
 *         https://www.freedesktop.org/software/systemd/man/systemd-gpt-auto-generator.html
 */

#define UNUSED_GUID		"00000000-0000-0000-0000-000000000000"
#define LEGACY_MBR_GUID		"024dee41-33e7-11d3-9d69-0008c781f39f"
#define LINUX_SWAP_GUID		"0657fd6d-a4ab-43c4-84e5-0933c84b4f4f"
#define LINUX_FILES_GUID	"0fc63daf-8483-4772-8e79-3d69d8477de4"
#define BIOS_BOOT_GUID		"21686148-6449-6e6f-744e-656564454649"
#define HIFIVE_BBL_GUID		"2e54b353-1271-4842-806f-e436d6af6985"
#define BEOS_I386_GUID		"42465331-3ba3-10f1-802a-4861696b7521"
#define MACOS_X_BOOT_GUID	"426f6f74-0000-11aa-aa11-00306543ecac"
#define MACOS_X_HFS_GUID	"48465300-0000-11aa-aa11-00306543ecac"
#define NETBSD_GUID		"49f48d5a-b10e-11dc-b99b-0019d1879648"
#define FREEBSD_GUID		"516e7cb4-6ecf-11d6-8ff8-00022d09712b"
#define APFS_RECOVERY_GUID	"52637672-7900-11aa-aa11-00306543ecac"
#define MACOS_X_GUID		"55465300-0000-11aa-aa11-00306543ecac"
#define HIFIVE_FSBL_GUID	"5b193300-fc78-40cd-8002-e86c45580b47"
#define APFS_ISC_GUID		"69646961-6700-11aa-aa11-00306543ecac"
#define SOLARIS_GUID		"6a85cf4d-1dd2-11b2-99a6-080020736631"
#define APFS_GUID		"7c3457ef-0000-11aa-aa11-00306543ecac"
#define OPENBSD_GUID		"824cc7a0-36a8-11e3-890a-952519ad3f61"
#define LINUXSWAP_DR_GUID	"af9b60a0-1431-4f62-bc68-3311714a69ad"
#define EFI_SYSTEM_PARTITION_GUID "c12a7328-f81f-11d2-ba4b-00a0c93ec93b"
#define WIN_RECOVERY_GUID	"de94bba4-06d1-4d40-a16a-bfd50179d6ac"
#define LINUX_LVM_GUID		"e6d6d379-f507-44c2-a23c-238f2a3df928"
#define MICROSOFT_BASIC_DATA_GUID "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7"
#define CHROME_KERNEL_GUID	"fe3a2a5d-4f32-41a7-b725-accc3285a309"

const struct gpt_type		gpt_types[] = {
	{ 0,
	  NULL,				/* Unused */
	  UNUSED_GUID },
	{ 0,
	  NULL,				/* Legacy MBR */
	  LEGACY_MBR_GUID },
	{ 0,
	  NULL,				/* Linux swap */
	  LINUX_SWAP_GUID },
	{ 0,
	  NULL,				/* Linux files* */
	  LINUX_FILES_GUID },
	{ GTATTR_PROTECT,
	  NULL,				/* BIOS Boot */
	  BIOS_BOOT_GUID },
	{ GTATTR_PROTECT,
	  NULL,				/* HiFive BBL */
	  HIFIVE_BBL_GUID },
	{ 0,
	  NULL,				/* BeOS/i386 */
	  BEOS_I386_GUID },
	{ 0,
	  NULL,				/* MacOS X boot */
	  MACOS_X_BOOT_GUID },
	{ 0,
	   NULL,				/* MacOS X HFS+ */
	  MACOS_X_HFS_GUID },
	{ 0,
	  NULL,				/* NetBSD */
	  NETBSD_GUID },
	{ 0,
	  NULL,				/* FreeBSD */
	  FREEBSD_GUID },
	{ GTATTR_PROTECT | GTATTR_PROTECT_EFISYS,
	  NULL,				/* APFS Recovery */
	  APFS_RECOVERY_GUID },
	{ 0,
	  NULL,				/* MacOS X */
	  MACOS_X_GUID },
	{ GTATTR_PROTECT,
	  NULL,				/* HiFive FSBL */
	  HIFIVE_FSBL_GUID },
	{ GTATTR_PROTECT | GTATTR_PROTECT_EFISYS,
	  NULL,				/* APFS ISC */
	  APFS_ISC_GUID },
	{ 0,
	  NULL,				/* Solaris */
	  SOLARIS_GUID },
	{ GTATTR_PROTECT | GTATTR_PROTECT_EFISYS,
	  NULL,				/* APFS */
	  APFS_GUID },
	{ 0,
	  NULL,				/* OpenBSD */
	  OPENBSD_GUID },
	{ 0,
	  NULL,				/* LinuxSwap DR */
	  LINUXSWAP_DR_GUID },
	{ 0,
	  NULL,				/* EFI Sys */
	  EFI_SYSTEM_PARTITION_GUID },
	{ 0,
	  NULL,				/* Win Recovery*/
	  WIN_RECOVERY_GUID },
	{ 0,
	  NULL,				/* Linux VM */
	  LINUX_LVM_GUID },
	{ 0,
	  NULL,				/* Microsoft basic data */
	  MICROSOFT_BASIC_DATA_GUID },
	{ 0,
	  NULL,				/* ChromeKernel */
	  CHROME_KERNEL_GUID },
};

struct menu_item {
	int	 mi_menuid;	/* Unique hex octet */
	int	 mi_mbrid;	/* -1 == not on MBR menu */
	char 	*mi_name;	/* Truncated at 14 chars */
	char	*mi_guid;	/* NULL == not on GPT menu */
};

const struct menu_item menu_items[] = {
	{ 0x00,	0x00,	"Unused",	UNUSED_GUID },
	{ 0x01,	0x01,	"DOS FAT-12",	MICROSOFT_BASIC_DATA_GUID },
	{ 0x02,	0x02,	"XENIX /",	NULL },
	{ 0x03,	0x03,	"XENIX /usr",	NULL },
	{ 0x04,	0x04,	"DOS FAT-16",	MICROSOFT_BASIC_DATA_GUID },
	{ 0x05,	0x05,	"Extended DOS",	NULL },
	{ 0x06,	0x06,	"DOS > 32MB",	MICROSOFT_BASIC_DATA_GUID },
	{ 0x07,	0x07,	"NTFS",		MICROSOFT_BASIC_DATA_GUID },
	{ 0x08,	0x08,	"AIX fs",	NULL },
	{ 0x09,	0x09,	"AIX/Coherent",	NULL },
	{ 0x0A,	0x0A,	"OS/2 Bootmgr",	NULL },
	{ 0x0B,	0x0B,	"Win95 FAT-32",	MICROSOFT_BASIC_DATA_GUID },
	{ 0x0C,	0x0C,	"Win95 FAT32L",	MICROSOFT_BASIC_DATA_GUID },
	{ 0x0D,   -1,	"BIOS boot",	BIOS_BOOT_GUID },
	{ 0x0E,	0x0E,	"DOS FAT-16",	MICROSOFT_BASIC_DATA_GUID },
	{ 0x0F,	0x0F,	"Extended LBA",	NULL },
	{ 0x10,	0x10,	"OPUS",		NULL },
	{ 0x11,	0x11,	"OS/2 hidden",	MICROSOFT_BASIC_DATA_GUID },
	{ 0x12,	0x12,	"Compaq Diag",	NULL },
	{ 0x14,	0x14,	"OS/2 hidden",	MICROSOFT_BASIC_DATA_GUID },
	{ 0x16,	0x16,	"OS/2 hidden",	MICROSOFT_BASIC_DATA_GUID },
	{ 0x17,	0x17,	"OS/2 hidden",	MICROSOFT_BASIC_DATA_GUID },
	{ 0x18,	0x18,	"AST swap",	NULL },
	{ 0x19,	0x19,	"Willowtech",	NULL },
	{ 0x1C,	0x1C,	"ThinkPad Rec",	MICROSOFT_BASIC_DATA_GUID },
	{ 0x20,	0x20,	"Willowsoft",	NULL },
	{ 0x24,	0x24,	"NEC DOS",	NULL },
	{ 0x27,	0x27,	"Win Recovery",	WIN_RECOVERY_GUID },
	{ 0x38,	0x38,	"Theos",	NULL },
	{ 0x39,	0x39,	"Plan 9",	NULL },
	{ 0x40,	0x40,	"VENIX 286",	NULL },
	{ 0x41,	0x41,	"Lin/Minux DR",	NULL },
	{ 0x42,	0x42,	"LinuxSwap DR",	LINUXSWAP_DR_GUID },
	{ 0x43,	0x43,	"Linux DR",	NULL },
	{ 0x4D,	0x4D,	"QNX 4.2 Pri",	NULL },
	{ 0x4E,	0x4E,	"QNX 4.2 Sec",	NULL },
	{ 0x4F,	0x4F,	"QNX 4.2 Ter",	NULL },
	{ 0x50,	0x50,	"DM",		NULL },
	{ 0x51,	0x51,	"DM",		NULL },
	{ 0x52,	0x52,	"CP/M or SysV",	NULL },
	{ 0x53,	0x53,	"DM",		NULL },
	{ 0x54,	0x54,	"Ontrack",	NULL },
	{ 0x55,	0x55,	"EZ-Drive",	NULL },
	{ 0x56,	0x56,	"Golden Bow",	NULL },
	{ 0x5C,	0x5C,	"Priam"	,	NULL },
	{ 0x61,	0x61,	"SpeedStor",	NULL },
	{ 0x63,	0x63,	"ISC, HURD, *",	NULL },
	{ 0x64,	0x64,	"NetWare 2.xx",	NULL },
	{ 0x65,	0x65,	"NetWare 3.xx",	NULL },
	{ 0x66,	0x66,	"NetWare 386",	NULL },
	{ 0x67,	0x67,	"Novell",	NULL },
	{ 0x68,	0x68,	"Novell",	NULL },
	{ 0x69,	0x69,	"Novell",	NULL },
	{ 0x70,	0x70,	"DiskSecure",	NULL },
	{ 0x75,	0x75,	"PCIX",		NULL },
	{ 0x7F,   -1,	"Chrome Kernel",CHROME_KERNEL_GUID },
	{ 0x80, 0x80,	"Minix (old)",	NULL },
	{ 0x81, 0x81,	"Minix (new)",	NULL },
	{ 0x82,	0x82,	"Linux swap",	LINUX_SWAP_GUID },
	{ 0x83,	0x83,	"Linux files*",	LINUX_FILES_GUID },
	{ 0x84,	0x84,	"OS/2 hidden",	NULL },
	{ 0x85,	0x85,	"Linux ext.",	NULL },
	{ 0x86, 0x86,	"NT FAT VS",	NULL },
	{ 0x87, 0x87,	"NTFS VS",	NULL },
	{ 0x8E,	0x8E,	"Linux LVM",	LINUX_LVM_GUID },
	{ 0x93,	0x93,	"Amoeba FS",	NULL },
	{ 0x94,	0x94,	"Amoeba BBT",	NULL },
	{ 0x99,	0x99,	"Mylex"	,	NULL },
	{ 0x9F,	0x9F,	"BSDI",		NULL },
	{ 0xA0,	0xA0,	"NotebookSave",	NULL },
	{ 0xA5,	0xA5,	"FreeBSD",	FREEBSD_GUID },
	{ 0xA6,	0xA6,	"OpenBSD",	OPENBSD_GUID },
	{ 0xA7,	0xA7,	"NeXTSTEP",	NULL },
	{ 0xA8,	0xA8,	"MacOS X",	MACOS_X_GUID },
	{ 0xA9,	0xA9,	"NetBSD",	NETBSD_GUID },
	{ 0xAB,	0xAB,	"MacOS X boot",	MACOS_X_BOOT_GUID },
	{ 0xAF,	0xAF,	"MacOS X HFS+",	MACOS_X_HFS_GUID },
	{ 0xB0,	  -1,	"APFS",		APFS_GUID },
	{ 0xB1,	  -1,	"APFS ISC",	APFS_ISC_GUID },
	{ 0xB2,	  -1,	"APFS Recovery",APFS_RECOVERY_GUID },
	{ 0xB3,	  -1,	"HiFive FSBL",	HIFIVE_FSBL_GUID },
	{ 0xB4,	  -1,	"HiFive BBL",	HIFIVE_BBL_GUID },
	{ 0xB7,	0xB7,	"BSDI filesy*",	NULL },
	{ 0xB8,	0xB8,	"BSDI swap",	NULL },
	{ 0xBF,	0xBF,	"Solaris",	SOLARIS_GUID },
	{ 0xC0,	0xC0,	"CTOS",		NULL },
	{ 0xC1,	0xC1,	"DRDOSs FAT12",	NULL },
	{ 0xC4,	0xC4,	"DRDOSs < 32M",	NULL },
	{ 0xC6,	0xC6,	"DRDOSs >=32M",	NULL },
	{ 0xC7,	0xC7,	"HPFS Disbled",	NULL },
	{ 0xDB,	0xDB,	"CPM/C.DOS/C*",	NULL },
	{ 0xDE,	0xDE,	"Dell Maint",	NULL },
	{ 0xE1,	0xE1,	"SpeedStor",	NULL },
	{ 0xE3,	0xE3,	"SpeedStor",	NULL },
	{ 0xE4,	0xE4,	"SpeedStor",	NULL },
	{ 0xEB,	0xEB,	"BeOS/i386",	BEOS_I386_GUID },
	{ 0xEC,	  -1,	"Legacy MBR",	LEGACY_MBR_GUID },
	{ 0xEE,	0xEE,	"EFI GPT",	NULL },
	{ 0xEF,	0xEF,	"EFI Sys",	EFI_SYSTEM_PARTITION_GUID },
	{ 0xF1,	0xF1,	"SpeedStor",	NULL },
	{ 0xF2,	0xF2,	"DOS 3.3+ Sec",	NULL },
	{ 0xF4,	0xF4,	"SpeedStor",	NULL },
	{ 0xFF,	0xFF,	"Xenix BBT",	NULL },
};

const struct gpt_type	*find_gpt_type(const struct uuid *);
const struct mbr_type	*find_mbr_type(const int);
int			 uuid_attr(const struct uuid *);
int			 mbr_item(const unsigned int);
int			 gpt_item(const unsigned int);
int			 nth_menu_item(int (*)(const unsigned int),
    const unsigned int, unsigned int);
void			 print_menu(int (*)(const unsigned int),
    const unsigned int);

const struct gpt_type *
find_gpt_type(const struct uuid *uuid)
{
	char			*uuidstr = NULL;
	unsigned int		 i;
	uint32_t		 status;

	uuid_to_string(uuid, &uuidstr, &status);
	if (status == uuid_s_ok) {
		for (i = 0; i < nitems(gpt_types); i++) {
			if (strcasecmp(gpt_types[i].gt_guid, uuidstr) == 0)
				break;
		}
	} else
		i = nitems(gpt_types);
	free(uuidstr);

	if (i < nitems(gpt_types))
		return &gpt_types[i];
	else
		return NULL;
}

const struct mbr_type *
find_mbr_type(const int id)
{
	unsigned int			i;

	for (i = 0; i < nitems(mbr_types); i++) {
		if (mbr_types[i].mt_type == id)
			return &mbr_types[i];
	}

	return NULL;
}

int
uuid_attr(const struct uuid *uuid)
{
	const struct gpt_type	*gt;

	gt = find_gpt_type(uuid);
	if (gt == NULL)
		return 0;
	else
		return gt->gt_attr;
}

void
print_menu(int (*test)(const unsigned int), const unsigned int columns)
{
	int			 col, col0;
	unsigned int		 count, i, j, rows;

	count = 0;
	for (i = 0; i < nitems(menu_items); i++)
		if (test(i) == 0)
			count++;
	rows = (count + columns - 1) / columns;

	col0 = -1;
	for (i = 0; i < rows; i++) {
		col0 = nth_menu_item(test, col0, 1);
		printf("%02X %-15s", menu_items[col0].mi_menuid,
		    menu_items[col0].mi_name);
		for (j = 1; j < columns; j++) {
			col = nth_menu_item(test, col0, j * rows);
			if (col == -1)
				break;
			printf("%02X %-15s", menu_items[col].mi_menuid,
			    menu_items[col].mi_name);
		}
		printf("\n");
	}
}

int
mbr_item(const unsigned int item)
{
	return menu_items[item].mi_mbrid == -1;
}

int
gpt_item(const unsigned int item)
{
	return menu_items[item].mi_guid == NULL;
}

int
nth_menu_item(int (*test)(const unsigned int), const unsigned int last,
    unsigned int n)
{
	unsigned int			i;

	for (i = last + 1; i < nitems(menu_items); i++) {
		if (test(i) == 0) {
			n--;
			if (n == 0)
				return i;
		}
	}

	return -1;
}

int
PRT_protected_uuid(const struct uuid *uuid)
{
	const struct gpt_type	*gt;
	unsigned int		 pn;

	gt = find_gpt_type(uuid);
	if (gt && gt->gt_attr & GTATTR_PROTECT)
		return 1;

	if (gt && strcasecmp(gt->gt_guid, EFI_SYSTEM_PARTITION_GUID) == 0) {
		for (pn = 0; pn < gh.gh_part_num; pn++) {
			if (uuid_attr(&gp[pn].gp_type) & GTATTR_PROTECT_EFISYS)
				return 1;
		}
	}

	return 0;
}

void
PRT_print_mbrmenu(char *lbuf, size_t lbuflen)
{
#define	MBR_MENU_COLUMNS	4

	printf("Choose from the following Partition id values:\n");
	print_menu(mbr_item,  MBR_MENU_COLUMNS);

	memset(lbuf, 0, lbuflen);	/* Just continue. */
}

void
PRT_print_gptmenu(char *lbuf, size_t lbuflen)
{
#define	GPT_MENU_COLUMNS	4

	printf("Choose from the following Partition id values:\n");
	print_menu(gpt_item, GPT_MENU_COLUMNS);

	memset(lbuf, 0, lbuflen);	/* Just continue. */
}

void
PRT_parse(const struct dos_partition *dp, const uint64_t lba_self,
    const uint64_t lba_firstembr, struct prt *prt)
{
	off_t			off;
	uint32_t		t;

	prt->prt_flag = dp->dp_flag;
	prt->prt_id = dp->dp_typ;

	if ((prt->prt_id == DOSPTYP_EXTEND) || (prt->prt_id == DOSPTYP_EXTENDL))
		off = lba_firstembr;
	else
		off = lba_self;

	memcpy(&t, &dp->dp_start, sizeof(uint32_t));
	prt->prt_bs = letoh32(t) + off;
	memcpy(&t, &dp->dp_size, sizeof(uint32_t));
	prt->prt_ns = letoh32(t);
	if (prt->prt_id == DOSPTYP_EFI && prt->prt_ns == UINT32_MAX)
		prt->prt_ns = DL_GETDSIZE(&dl) - prt->prt_bs;
}

void
PRT_make(const struct prt *prt, const uint64_t lba_self,
    const uint64_t lba_firstembr, struct dos_partition *dp)
{
	struct chs		start, end;
	uint64_t		off, t;

	if (prt->prt_ns == 0 || prt->prt_id == DOSPTYP_UNUSED) {
		memset(dp, 0, sizeof(*dp));
		return;
	}

	if ((prt->prt_id == DOSPTYP_EXTEND) || (prt->prt_id == DOSPTYP_EXTENDL))
		off = lba_firstembr;
	else
		off = lba_self;

	if (PRT_lba_to_chs(prt, &start, &end) == 0) {
		dp->dp_shd = start.chs_head & 0xFF;
		dp->dp_ssect = (start.chs_sect & 0x3F) | ((start.chs_cyl & 0x300) >> 2);
		dp->dp_scyl = start.chs_cyl & 0xFF;
		dp->dp_ehd = end.chs_head & 0xFF;
		dp->dp_esect = (end.chs_sect & 0x3F) | ((end.chs_cyl & 0x300) >> 2);
		dp->dp_ecyl = end.chs_cyl & 0xFF;
	} else {
		memset(dp, 0xFF, sizeof(*dp));
	}

	dp->dp_flag = prt->prt_flag & 0xFF;
	dp->dp_typ = prt->prt_id & 0xFF;

	t = htole64(prt->prt_bs - off);
	memcpy(&dp->dp_start, &t, sizeof(uint32_t));
	if (prt->prt_id == DOSPTYP_EFI && (prt->prt_bs + prt->prt_ns) >
	    DL_GETDSIZE(&dl))
		t = htole64(UINT32_MAX);
	else
		t = htole64(prt->prt_ns);
	memcpy(&dp->dp_size, &t, sizeof(uint32_t));
}

void
PRT_print_parthdr(void)
{
	printf("            Starting         Ending    "
	    "     LBA Info:\n");
	printf(" #: id      C   H   S -      C   H   S "
	    "[       start:        size ]\n");
	printf("---------------------------------------"
	    "----------------------------------------\n");
}

void
PRT_print_part(const int num, const struct prt *prt, const char *units)
{
	const struct unit_type	*ut;
	const struct mbr_type	*mt;
	const char		*desc = NULL;
	struct chs		 start, end;
	double			 size;
	unsigned int		 i;

	size = units_size(units, prt->prt_ns, &ut);
	PRT_lba_to_chs(prt, &start, &end);
	mt = find_mbr_type(prt->prt_id);
	if (mt != NULL) {
		if (mt->mt_desc != NULL)
			desc = mt->mt_desc;
		for (i = 0; i < nitems(menu_items) && desc == NULL; i++) {
			if (mbr_item(i) == 0 &&
			    menu_items[i].mi_mbrid == prt->prt_id)
				desc = menu_items[i].mi_name;
		}
	}

	printf("%c%1d: %.2X %6llu %3u %3u - %6llu %3u %3u [%12llu:%12.0f%s] "
	    "%s\n", (prt->prt_flag == DOSACTIVE) ? '*' : ' ', num, prt->prt_id,
	    start.chs_cyl, start.chs_head, start.chs_sect,
	    end.chs_cyl, end.chs_head, end.chs_sect,
	    prt->prt_bs, size, ut->ut_abbr, desc ? desc : "<Unknown ID>");

	if (prt->prt_bs >= DL_GETDSIZE(&dl))
		printf("partition %d starts beyond the end of %s\n", num,
		    disk.dk_name);
	else if (prt->prt_bs + prt->prt_ns > DL_GETDSIZE(&dl))
		printf("partition %d extends beyond the end of %s\n", num,
		    disk.dk_name);
}

int
PRT_lba_to_chs(const struct prt *prt, struct chs *start, struct chs *end)
{
	uint64_t		lba;

	if (prt->prt_ns == 0 || prt->prt_id == DOSPTYP_UNUSED) {
		memset(start, 0, sizeof(*start));
		memset(end, 0, sizeof(*end));
		return -1;
	}

	/*
	 * C = LBA ÷ (HPC × SPT)
	 * H = (LBA ÷ SPT) mod HPC
	 * S = (LBA mod SPT) + 1
	 */

	lba = prt->prt_bs;
	start->chs_cyl = lba / (disk.dk_sectors * disk.dk_heads);
	start->chs_head = (lba / disk.dk_sectors) % disk.dk_heads;
	start->chs_sect = (lba % disk.dk_sectors) + 1;

	lba = prt->prt_bs + prt->prt_ns - 1;
	end->chs_cyl = lba / (disk.dk_sectors * disk.dk_heads);
	end->chs_head = (lba / disk.dk_sectors) % disk.dk_heads;
	end->chs_sect = (lba % disk.dk_sectors) + 1;

	if (start->chs_head > 255 || end->chs_head > 255 ||
	    start->chs_sect > 63  || end->chs_sect > 63 ||
	    start->chs_cyl > 1023 || end->chs_cyl > 1023)
		return -1;

	return 0;
}

const char *
PRT_uuid_to_name(const struct uuid *uuid)
{
	static char		 typename[UUID_STR_LEN + 1];
	const struct gpt_type	*gt;
	char			*uuidstr;
	unsigned int		 i;
	uint32_t		 status;

	gt = find_gpt_type(uuid);
	if (gt != NULL) {
		if (gt->gt_desc != NULL)
			return gt->gt_desc;
		for (i = 0; i < nitems(menu_items); i++) {
			if (gpt_item(i) == 0 &&
			    strcasecmp(gt->gt_guid, menu_items[i].mi_guid) == 0)
				return menu_items[i].mi_name;
		}
	}

	uuid_to_string(uuid, &uuidstr, &status);
	if (status == uuid_s_ok)
		strlcpy(typename, uuidstr, sizeof(typename));
	else
		typename[0] = '\0';
	free(uuidstr);

	return typename;
}

int
PRT_uuid_to_menuid(const struct uuid *uuid)
{
	const struct gpt_type	*gt;
	unsigned int		 i;

	gt = find_gpt_type(uuid);
	if (gt != NULL) {
		for (i = 0; i < nitems(menu_items); i++) {
			if (gpt_item(i) == 0 &&
			    strcasecmp(menu_items[i].mi_guid, gt->gt_guid) == 0)
				return menu_items[i].mi_menuid;
		}
	}

	return -1;
}

const struct uuid *
PRT_menuid_to_uuid(const int menuid)
{
	static struct uuid	guid;
	unsigned int		i;
	uint32_t		status = uuid_s_ok;

	for (i = 0; i < nitems(menu_items); i++) {
		if (gpt_item(i) == 0 && menu_items[i].mi_menuid == menuid) {
			uuid_from_string(menu_items[i].mi_guid, &guid, &status);
			break;
		}
	}
	if (i == nitems(menu_items) || status != uuid_s_ok)
		uuid_create_nil(&guid, NULL);

	return &guid;
}
