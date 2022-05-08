/*	$OpenBSD: part.c,v 1.130 2022/05/08 18:01:23 krw Exp $	*/

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
	int	mt_type;
	char	mt_sname[14];
};

const struct mbr_type		mbr_types[] = {
	{ 0x00, "unused"	},   /* unused */
	{ 0x01, "DOS FAT-12"	},   /* Primary DOS with 12 bit FAT */
	{ 0x02, "XENIX /"	},   /* XENIX / filesystem */
	{ 0x03, "XENIX /usr"	},   /* XENIX /usr filesystem */
	{ 0x04, "DOS FAT-16"	},   /* Primary DOS with 16 bit FAT */
	{ 0x05, "Extended DOS"	},   /* Extended DOS */
	{ 0x06, "DOS > 32MB"	},   /* Primary 'big' DOS (> 32MB) */
	{ 0x07, "NTFS"		},   /* NTFS */
	{ 0x08, "AIX fs"	},   /* AIX filesystem */
	{ 0x09, "AIX/Coherent"	},   /* AIX boot partition or Coherent */
	{ 0x0A, "OS/2 Bootmgr"	},   /* OS/2 Boot Manager or OPUS */
	{ 0x0B, "Win95 FAT-32"	},   /* Primary Win95 w/ 32-bit FAT */
	{ 0x0C, "Win95 FAT32L"	},   /* Primary Win95 w/ 32-bit FAT LBA-mapped */
	{ 0x0E, "DOS FAT-16"	},   /* Primary DOS w/ 16-bit FAT, CHS-mapped */
	{ 0x0F, "Extended LBA"	},   /* Extended DOS LBA-mapped */
	{ 0x10, "OPUS"		},   /* OPUS */
	{ 0x11, "OS/2 hidden"	},   /* OS/2 BM: hidden DOS 12-bit FAT */
	{ 0x12, "Compaq Diag"	},   /* Compaq Diagnostics */
	{ 0x14, "OS/2 hidden"	},   /* OS/2 BM: hidden DOS 16-bit FAT <32M or Novell DOS 7.0 bug */
	{ 0x16, "OS/2 hidden"	},   /* OS/2 BM: hidden DOS 16-bit FAT >=32M */
	{ 0x17, "OS/2 hidden"	},   /* OS/2 BM: hidden IFS */
	{ 0x18, "AST swap"	},   /* AST Windows swapfile */
	{ 0x19, "Willowtech"	},   /* Willowtech Photon coS */
	{ 0x1C, "ThinkPad Rec"	},   /* IBM ThinkPad recovery partition */
	{ 0x20, "Willowsoft"	},   /* Willowsoft OFS1 */
	{ 0x24, "NEC DOS"	},   /* NEC DOS */
	{ 0x27, "Win Recovery"	},   /* Windows hidden Recovery Partition */
	{ 0x38, "Theos"		},   /* Theos */
	{ 0x39, "Plan 9"	},   /* Plan 9 */
	{ 0x40, "VENIX 286"	},   /* VENIX 286 or LynxOS */
	{ 0x41, "Lin/Minux DR" 	},   /* Linux/MINIX (sharing disk with DRDOS) or Personal RISC boot */
	{ 0x42, "LinuxSwap DR" 	},   /* SFS or Linux swap (sharing disk with DRDOS) */
	{ 0x43, "Linux DR" 	},   /* Linux native (sharing disk with DRDOS) */
	{ 0x4D, "QNX 4.2 Pri"	},   /* QNX 4.2 Primary */
	{ 0x4E, "QNX 4.2 Sec"	},   /* QNX 4.2 Secondary */
	{ 0x4F, "QNX 4.2 Ter"	},   /* QNX 4.2 Tertiary */
	{ 0x50, "DM"		},   /* DM (disk manager) */
	{ 0x51, "DM"		},   /* DM6 Aux1 (or Novell) */
	{ 0x52, "CP/M or SysV"	},   /* CP/M or Microport SysV/AT */
	{ 0x53, "DM"		},   /* DM6 Aux3 */
	{ 0x54, "Ontrack"	},   /* Ontrack */
	{ 0x55, "EZ-Drive"	},   /* EZ-Drive (disk manager) */
	{ 0x56, "Golden Bow"	},   /* Golden Bow (disk manager) */
	{ 0x5C, "Priam"		},   /* Priam Edisk (disk manager) */
	{ 0x61, "SpeedStor"	},   /* SpeedStor */
	{ 0x63, "ISC, HURD, *"	},   /* ISC, System V/386, GNU HURD or Mach */
	{ 0x64, "NetWare 2.xx"	},   /* Novell NetWare 2.xx */
	{ 0x65, "NetWare 3.xx"	},   /* Novell NetWare 3.xx */
	{ 0x66, "NetWare 386"	},   /* Novell 386 NetWare */
	{ 0x67, "Novell"	},   /* Novell */
	{ 0x68, "Novell"	},   /* Novell */
	{ 0x69, "Novell"	},   /* Novell */
	{ 0x70, "DiskSecure"	},   /* DiskSecure Multi-Boot */
	{ 0x75, "PCIX"		},   /* PCIX */
	{ 0x80, "Minix (old)"	},   /* Minix 1.1 ... 1.4a */
	{ 0x81, "Minix (new)"	},   /* Minix 1.4b ... 1.5.10 */
	{ 0x82, "Linux swap"	},   /* Linux swap */
	{ 0x83, "Linux files*"	},   /* Linux filesystem */
	{ 0x84, "OS/2 hidden"	},   /* OS/2 hidden C: drive */
	{ 0x85, "Linux ext."	},   /* Linux extended */
	{ 0x86, "NT FAT VS"	},   /* NT FAT volume set */
	{ 0x87, "NTFS VS"	},   /* NTFS volume set or HPFS mirrored */
	{ 0x8E, "Linux LVM"	},   /* Linux LVM */
	{ 0x93, "Amoeba FS"	},   /* Amoeba filesystem */
	{ 0x94, "Amoeba BBT"	},   /* Amoeba bad block table */
	{ 0x99, "Mylex"		},   /* Mylex EISA SCSI */
	{ 0x9F, "BSDI"		},   /* BSDI BSD/OS */
	{ 0xA0, "NotebookSave"	},   /* Phoenix NoteBIOS save-to-disk */
	{ 0xA5, "FreeBSD"	},   /* FreeBSD */
	{ 0xA6, "OpenBSD"	},   /* OpenBSD */
	{ 0xA7, "NEXTSTEP"	},   /* NEXTSTEP */
	{ 0xA8, "MacOS X"	},   /* MacOS X main partition */
	{ 0xA9, "NetBSD"	},   /* NetBSD */
	{ 0xAB, "MacOS X boot"	},   /* MacOS X boot partition */
	{ 0xAF, "MacOS X HFS+"	},   /* MacOS X HFS+ partition */
	{ 0xB7, "BSDI filesy*"	},   /* BSDI BSD/386 filesystem */
	{ 0xB8, "BSDI swap"	},   /* BSDI BSD/386 swap */
	{ 0xBF, "Solaris"	},   /* Solaris */
	{ 0xC0, "CTOS"		},   /* CTOS */
	{ 0xC1, "DRDOSs FAT12"	},   /* DRDOS/sec (FAT-12) */
	{ 0xC4, "DRDOSs < 32M"	},   /* DRDOS/sec (FAT-16, < 32M) */
	{ 0xC6, "DRDOSs >=32M"	},   /* DRDOS/sec (FAT-16, >= 32M) */
	{ 0xC7, "HPFS Disbled"	},   /* Syrinx (Cyrnix?) or HPFS disabled */
	{ 0xDB, "CPM/C.DOS/C*"	},   /* Concurrent CPM or C.DOS or CTOS */
	{ 0xDE, "Dell Maint"	},   /* Dell maintenance partition */
	{ 0xE1, "SpeedStor"	},   /* DOS access or SpeedStor 12-bit FAT extended partition */
	{ 0xE3, "SpeedStor"	},   /* DOS R/O or SpeedStor or Storage Dimensions */
	{ 0xE4, "SpeedStor"	},   /* SpeedStor 16-bit FAT extended partition < 1024 cyl. */
	{ 0xEB, "BeOS/i386"	},   /* BeOS for Intel */
	{ 0xEE, "EFI GPT"	},   /* EFI Protective Partition */
	{ 0xEF, "EFI Sys"	},   /* EFI System Partition */
	{ 0xF1, "SpeedStor"	},   /* SpeedStor or Storage Dimensions */
	{ 0xF2, "DOS 3.3+ Sec"	},   /* DOS 3.3+ Secondary */
	{ 0xF4, "SpeedStor"	},   /* SpeedStor >1024 cyl. or LANstep or IBM PS/2 IML */
	{ 0xFF, "Xenix BBT"	},   /* Xenix Bad Block Table */
};

struct gpt_type {
	int	gt_type;
	int	gt_attr;
#define	GTATTR_PROTECT		(1 << 0)
#define	GTATTR_PROTECT_EFISYS	(1 << 1)
	char	gt_sname[14];
	char	gt_guid[UUID_STR_LEN + 1];
};

const struct gpt_type		gpt_types[] = {
	{ 0x00, 0, "unused",
	  "00000000-0000-0000-0000-000000000000" },
	{ 0x01, 0, "FAT12",
	  "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7" },
	{ 0x04, 0, "FAT16S",
	  "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7" },
	{ 0x06, 0, "FAT16B",
	  "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7" },
	{ 0x07, 0, "NTFS",
	  "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7" },
	{ 0x0B, 0, "FAT32",
	  "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7" },
	{ 0x0C, 0, "FAT32L",
	  "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7" },
	{ 0x0D, GTATTR_PROTECT, "BIOS Boot",
	  "21686148-6449-6e6f-744e-656564454649" },
	{ 0x0E, 0, "FAT16L",
	  "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7" },
	{ 0x11, 0, "OS/2 hidden",
	  "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7" },
	{ 0x14, 0, "OS/2 hidden",
	  "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7" },
	{ 0x16, 0, "OS/2 hidden",
	  "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7" },
	{ 0x17, 0, "OS/2 hidden",
	  "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7" },
	{ 0x1C, 0, "ThinkPad Rec",
	  "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7" },
	{ 0x27, 0, "Win Recovery",
	  "de94bba4-06d1-4d40-a16a-bfd50179d6ac" },
	{ 0x42, 0, "LinuxSwap DR",
	  "af9b60a0-1431-4f62-bc68-3311714a69ad" },
	{ 0x7f, 0, "ChromeKernel",
	  "fe3a2a5d-4f32-41a7-b725-accc3285a309" },
	{ 0x82, 0, "Linux swap",
	  "0657fd6d-a4ab-43c4-84e5-0933c84b4f4f" },
	{ 0x83, 0, "Linux files*",
	  "0fc63daf-8483-4772-8e79-3d69d8477de4" },
	{ 0x8E, 0, "Linux LVM",
	  "e6d6d379-f507-44c2-a23c-238f2a3df928" },
	{ 0xA5, 0, "FreeBSD",
	  "516e7cb4-6ecf-11d6-8ff8-00022d09712b" },
	{ 0xA6, 0, "OpenBSD",
	  "824cc7a0-36a8-11e3-890a-952519ad3f61" },
	{ 0xA8, 0, "MacOS X",
	  "55465300-0000-11aa-aa11-00306543ecac" },
	{ 0xA9, 0, "NetBSD",
	  "516e7cb4-6ecf-11d6-8ff8-00022d09712b" },
	{ 0xAB, 0, "MacOS X boot",
	  "426f6f74-0000-11aa-aa11-00306543ecac" },
	{ 0xAF, 0, "MacOS X HFS+",
	  "48465300-0000-11aa-aa11-00306543ecac" },
	{ 0xB0, GTATTR_PROTECT | GTATTR_PROTECT_EFISYS, "APFS",
	  "7c3457ef-0000-11aa-aa11-00306543ecac" },
	{ 0xB1, GTATTR_PROTECT | GTATTR_PROTECT_EFISYS, "APFS ISC",
	  "69646961-6700-11aa-aa11-00306543ecac" },
	{ 0xB2, GTATTR_PROTECT | GTATTR_PROTECT_EFISYS, "APFS Recovery",
	  "52637672-7900-11aa-aa11-00306543ecac" },
	{ 0xB3, GTATTR_PROTECT, "HiFive FSBL",
	  "5b193300-fc78-40cd-8002-e86c45580b47" },
	{ 0xB4, GTATTR_PROTECT, "HiFive BBL",
	  "2e54b353-1271-4842-806f-e436d6af6985" },
	{ 0xBF, 0, "Solaris",
	  "6a85cf4d-1dd2-11b2-99a6-080020736631" },
	{ 0xEB, 0, "BeOS/i386",
	  "42465331-3ba3-10f1-802a-4861696b7521" },
	{ 0xEF, 0, "EFI Sys",
	  "c12a7328-f81f-11d2-ba4b-00a0c93ec93b" },
};

const struct gpt_type	*find_gpt_type(const struct uuid *);
const char		*ascii_id(const int);
int			 uuid_attr(const struct uuid *);

const struct gpt_type *
find_gpt_type(const struct uuid *uuid)
{
	char			*uuidstr = NULL;
	unsigned int		 i;
	uint32_t		 status;

	uuid_to_string(uuid, &uuidstr, &status);
	if (status == uuid_s_ok) {
		for (i = 0; i < nitems(gpt_types); i++) {
			if (memcmp(gpt_types[i].gt_guid, uuidstr,
			    sizeof(gpt_types[i].gt_guid)) == 0)
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

const char *
ascii_id(const int id)
{
	static char		unknown[] = "<Unknown ID>";
	int			i;

	for (i = 0; i < nitems(mbr_types); i++) {
		if (mbr_types[i].mt_type == id)
			return mbr_types[i].mt_sname;
	}

	return unknown;
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

int
PRT_protected_guid(const struct uuid *uuid)
{
	const struct gpt_type	*gt;
	unsigned int		 pn;

	gt = find_gpt_type(uuid);
	if (gt && gt->gt_attr & GTATTR_PROTECT)
		return 1;

	if (gt && gt->gt_type == DOSPTYP_EFISYS) {
		for (pn = 0; pn < gh.gh_part_num; pn++) {
			if (uuid_attr(&gp[pn].gp_type) & GTATTR_PROTECT_EFISYS)
				return 1;
		}
	}

	return 0;
}

void
PRT_print_mbrtypes(void)
{
	unsigned int		cidx, i, idrows;

	idrows = (nitems(mbr_types) + 3) / 4;

	printf("Choose from the following Partition id values:\n");
	for (i = 0; i < idrows; i++) {
		for (cidx = i; cidx < i + idrows * 3; cidx += idrows) {
			printf("%02X %-*s", mbr_types[cidx].mt_type,
			    (int)sizeof(mbr_types[cidx].mt_sname) + 1,
			    mbr_types[cidx].mt_sname);
		}
		if (cidx < nitems(mbr_types))
			printf("%02X %s", mbr_types[cidx].mt_type,
			    mbr_types[cidx].mt_sname);
		printf("\n");
	}
}

void
PRT_print_gpttypes(void)
{
	unsigned int		cidx, i, idrows;

	idrows = (nitems(gpt_types) + 3) / 4;

	printf("Choose from the following Partition id values:\n");
	for (i = 0; i < idrows; i++) {
		for (cidx = i; cidx < i + idrows * 3; cidx += idrows) {
			printf("%02X %-*s", gpt_types[cidx].gt_type,
			    (int)sizeof(gpt_types[cidx].gt_sname) + 1,
			    gpt_types[cidx].gt_sname);
		}
		if (cidx < nitems(gpt_types))
			printf("%02X %s", gpt_types[cidx].gt_type,
			    gpt_types[cidx].gt_sname);
		printf("\n");
	}
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
	struct chs		 start, end;
	double			 size;

	size = units_size(units, prt->prt_ns, &ut);
	PRT_lba_to_chs(prt, &start, &end);

	printf("%c%1d: %.2X %6llu %3u %3u - %6llu %3u %3u "
	    "[%12llu:%12.0f%s] %s\n",
	    (prt->prt_flag == DOSACTIVE)?'*':' ',
	    num, prt->prt_id,
	    start.chs_cyl, start.chs_head, start.chs_sect,
	    end.chs_cyl, end.chs_head, end.chs_sect,
	    prt->prt_bs, size, ut->ut_abbr, ascii_id(prt->prt_id));
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
PRT_uuid_to_sname(const struct uuid *uuid)
{
	static char		 typename[UUID_STR_LEN + 1];
	const uint8_t		 gpt_uuid_msdos[] = GPT_UUID_MSDOS;
	struct uuid		 uuid_msdos;
	const struct gpt_type	*gt;
	char			*uuidstr;
	uint32_t		 status;

	uuid_dec_be(gpt_uuid_msdos, &uuid_msdos);
	if (uuid_compare(&uuid_msdos, uuid, NULL) == 0)
		return "Microsoft basic data";

	gt = find_gpt_type(uuid);
	if (gt != NULL)
		return gt->gt_sname;

	uuid_to_string(uuid, &uuidstr, &status);
	if (status == uuid_s_ok)
		strlcpy(typename, uuidstr, sizeof(typename));
	else
		typename[0] = '\0';
	free(uuidstr);

	return typename;
}

int
PRT_uuid_to_type(const struct uuid *uuid)
{
	const struct gpt_type	*gt;

	gt = find_gpt_type(uuid);
	if (gt == NULL)
		return 0;
	else
		return gt->gt_type;
}

const struct uuid *
PRT_type_to_guid(const int type)
{
	static struct uuid	guid;
	int			i, entries;
	uint32_t		status = uuid_s_ok;

	memset(&guid, 0, sizeof(guid));

	entries = nitems(gpt_types);

	for (i = 0; i < entries; i++) {
		if (gpt_types[i].gt_type == type)
			break;
	}
	if (i < entries)
		uuid_from_string(gpt_types[i].gt_guid, &guid, &status);
	if (i == entries || status != uuid_s_ok)
		uuid_from_string(gpt_types[0].gt_guid, &guid, &status);

	return &guid;
}
