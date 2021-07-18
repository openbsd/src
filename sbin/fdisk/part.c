/*	$OpenBSD: part.c,v 1.100 2021/07/18 21:40:13 krw Exp $	*/

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

int			 check_chs(const struct prt *);
const char		*ascii_id(const int);

static const struct part_type {
	int	pt_type;
	char	pt_sname[14];
	char	pt_guid[UUID_STR_LEN + 1];
} part_types[] = {
	{ 0x00, "unused      ", "00000000-0000-0000-0000-000000000000" },
	{ 0x01, "FAT12       ", "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7" },
	{ 0x02, "XENIX /     "},   /* XENIX / filesystem */
	{ 0x03, "XENIX /usr  "},   /* XENIX /usr filesystem */
	{ 0x04, "FAT16S      ", "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7" },
	{ 0x05, "Extended DOS"},   /* Extended DOS */
	{ 0x06, "FAT16B      ", "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7" },
	{ 0x07, "NTFS        ", "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7" },
	{ 0x08, "AIX fs      "},   /* AIX filesystem */
	{ 0x09, "AIX/Coherent"},   /* AIX boot partition or Coherent */
	{ 0x0A, "OS/2 Bootmgr"},   /* OS/2 Boot Manager or OPUS */
	{ 0x0B, "FAT32       ", "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7" },
	{ 0x0C, "FAT32L      ", "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7" },
	{ 0x0D, "BIOS Boot   ", "21686148-6449-6e6f-744e-656564454649" },
	{ 0x0E, "FAT16L      ", "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7" },
	{ 0x0F, "Extended LBA"},   /* Extended DOS LBA-mapped */
	{ 0x10, "OPUS        "},   /* OPUS */
	{ 0x11, "OS/2 hidden ", "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7" },
	{ 0x12, "Compaq Diag."},   /* Compaq Diagnostics */
	{ 0x14, "OS/2 hidden ", "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7" },
	{ 0x16, "OS/2 hidden ", "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7" },
	{ 0x17, "OS/2 hidden ", "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7" },
	{ 0x18, "AST swap    "},   /* AST Windows swapfile */
	{ 0x19, "Willowtech  "},   /* Willowtech Photon coS */
	{ 0x1C, "ThinkPad Rec", "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7" },
	{ 0x24, "NEC DOS     "},   /* NEC DOS */
	{ 0x27, "Win Recovery", "de94bba4-06d1-4d40-a16a-bfd50179d6ac" },
	{ 0x20, "Willowsoft  "},   /* Willowsoft OFS1 */
	{ 0x38, "Theos       "},   /* Theos */
	{ 0x39, "Plan 9      "},   /* Plan 9 */
	{ 0x40, "VENIX 286   "},   /* VENIX 286 or LynxOS */
	{ 0x41, "Lin/Minux DR"},   /* Linux/MINIX (sharing disk with DRDOS) or Personal RISC boot */
	{ 0x42, "LinuxSwap DR", "af9b60a0-1431-4f62-bc68-3311714a69ad" },
	{ 0x43, "Linux DR    "},   /* Linux native (sharing disk with DRDOS) */
	{ 0x4D, "QNX 4.2 Pri "},   /* QNX 4.2 Primary */
	{ 0x4E, "QNX 4.2 Sec "},   /* QNX 4.2 Secondary */
	{ 0x4F, "QNX 4.2 Ter "},   /* QNX 4.2 Tertiary */
	{ 0x50, "DM          "},   /* DM (disk manager) */
	{ 0x51, "DM          "},   /* DM6 Aux1 (or Novell) */
	{ 0x52, "CP/M or SysV"},   /* CP/M or Microport SysV/AT */
	{ 0x53, "DM          "},   /* DM6 Aux3 */
	{ 0x54, "Ontrack     "},   /* Ontrack */
	{ 0x55, "EZ-Drive    "},   /* EZ-Drive (disk manager) */
	{ 0x56, "Golden Bow  "},   /* Golden Bow (disk manager) */
	{ 0x5C, "Priam       "},   /* Priam Edisk (disk manager) */
	{ 0x61, "SpeedStor   "},   /* SpeedStor */
	{ 0x63, "ISC, HURD, *"},   /* ISC, System V/386, GNU HURD or Mach */
	{ 0x64, "NetWare 2.xx"},   /* Novell NetWare 2.xx */
	{ 0x65, "NetWare 3.xx"},   /* Novell NetWare 3.xx */
	{ 0x66, "NetWare 386 "},   /* Novell 386 NetWare */
	{ 0x67, "Novell      "},   /* Novell */
	{ 0x68, "Novell      "},   /* Novell */
	{ 0x69, "Novell      "},   /* Novell */
	{ 0x70, "DiskSecure  "},   /* DiskSecure Multi-Boot */
	{ 0x75, "PCIX        "},   /* PCIX */
	{ 0x7f, "ChromeKernel", "fe3a2a5d-4f32-41a7-b725-accc3285a309" },
	{ 0x80, "Minix (old) "},   /* Minix 1.1 ... 1.4a */
	{ 0x81, "Minix (new) "},   /* Minix 1.4b ... 1.5.10 */
	{ 0x82, "Linux swap  ", "0657fd6d-a4ab-43c4-84e5-0933c84b4f4f" },
	{ 0x83, "Linux files*", "0fc63daf-8483-4772-8e79-3d69d8477de4" },
	{ 0x84, "OS/2 hidden "},   /* OS/2 hidden C: drive */
	{ 0x85, "Linux ext.  "},   /* Linux extended */
	{ 0x86, "NT FAT VS   "},   /* NT FAT volume set */
	{ 0x87, "NTFS VS     "},   /* NTFS volume set or HPFS mirrored */
	{ 0x8E, "Linux LVM   ", "e6d6d379-f507-44c2-a23c-238f2a3df928" },
	{ 0x93, "Amoeba FS   "},   /* Amoeba filesystem */
	{ 0x94, "Amoeba BBT  "},   /* Amoeba bad block table */
	{ 0x99, "Mylex       "},   /* Mylex EISA SCSI */
	{ 0x9F, "BSDI        "},   /* BSDI BSD/OS */
	{ 0xA0, "NotebookSave"},   /* Phoenix NoteBIOS save-to-disk */
	{ 0xA5, "FreeBSD     ", "516e7cb4-6ecf-11d6-8ff8-00022d09712b" },
	{ 0xA6, "OpenBSD     ", "824cc7a0-36a8-11e3-890a-952519ad3f61" },
	{ 0xA7, "NEXTSTEP    "},   /* NEXTSTEP */
	{ 0xA8, "MacOS X     ", "55465300-0000-11aa-aa11-00306543ecac" },
	{ 0xA9, "NetBSD      ", "516e7cb4-6ecf-11d6-8ff8-00022d09712b" },
	{ 0xAB, "MacOS X boot", "426f6f74-0000-11aa-aa11-00306543ecac" },
	{ 0xAF, "MacOS X HFS+", "48465300-0000-11aa-aa11-00306543ecac" },
	{ 0xB0, "APFS        ", "7c3457ef-0000-11aa-aa11-00306543ecac" },
	{ 0xB1, "APFS ISC    ", "69646961-6700-11aa-aa11-00306543ecac" },
	{ 0xB2, "APFS Recovry", "52637672-7900-11aa-aa11-00306543ecac" },
	{ 0xB3, "HiFive FSBL ", "5b193300-fc78-40cd-8002-e86c45580b47" },
	{ 0xB4, "HiFive BBL  ", "2e54b353-1271-4842-806f-e436d6af6985" },
	{ 0xB7, "BSDI filesy*"},   /* BSDI BSD/386 filesystem */
	{ 0xB8, "BSDI swap   "},   /* BSDI BSD/386 swap */
	{ 0xBF, "Solaris     ", "6a85cf4d-1dd2-11b2-99a6-080020736631" },
	{ 0xC0, "CTOS        "},   /* CTOS */
	{ 0xC1, "DRDOSs FAT12"},   /* DRDOS/sec (FAT-12) */
	{ 0xC4, "DRDOSs < 32M"},   /* DRDOS/sec (FAT-16, < 32M) */
	{ 0xC6, "DRDOSs >=32M"},   /* DRDOS/sec (FAT-16, >= 32M) */
	{ 0xC7, "HPFS Disbled"},   /* Syrinx (Cyrnix?) or HPFS disabled */
	{ 0xDB, "CPM/C.DOS/C*"},   /* Concurrent CPM or C.DOS or CTOS */
	{ 0xDE, "Dell Maint  "},   /* Dell maintenance partition */
	{ 0xE1, "SpeedStor   "},   /* DOS access or SpeedStor 12-bit FAT extended partition */
	{ 0xE3, "SpeedStor   "},   /* DOS R/O or SpeedStor or Storage Dimensions */
	{ 0xE4, "SpeedStor   "},   /* SpeedStor 16-bit FAT extended partition < 1024 cyl. */
	{ 0xEB, "BeOS/i386   ", "42465331-3ba3-10f1-802a-4861696b7521" },
	{ 0xEE, "EFI GPT     "},   /* EFI Protective Partition */
	{ 0xEF, "EFI Sys     ", "c12a7328-f81f-11d2-ba4b-00a0c93ec93b" },
	{ 0xF1, "SpeedStor   "},   /* SpeedStor or Storage Dimensions */
	{ 0xF2, "DOS 3.3+ Sec"},   /* DOS 3.3+ Secondary */
	{ 0xF4, "SpeedStor   "},   /* SpeedStor >1024 cyl. or LANstep or IBM PS/2 IML */
	{ 0xFF, "Xenix BBT   "},   /* Xenix Bad Block Table */
};

static const struct protected_guid {
	char	pg_guid[UUID_STR_LEN + 1];
} protected_guid[] = {
	{ "7c3457ef-0000-11aa-aa11-00306543ecac" },	/* APFS		*/
	{ "69646961-6700-11aa-aa11-00306543ecac" },	/* APFS ISC	*/
	{ "52637672-7900-11aa-aa11-00306543ecac" },	/* APFS Recovry */
	{ "5b193300-fc78-40cd-8002-e86c45580b47" },	/* HiFive FSBL	*/
	{ "2e54b353-1271-4842-806f-e436d6af6985" },	/* HiFive BBL	*/
};

#ifndef nitems
#define	nitems(_a)	(sizeof((_a)) / sizeof((_a)[0]))
#endif

int
PRT_protected_guid(const struct uuid *uuid)
{
	char			*str = NULL;
	int			 rslt;
	unsigned int		 i;
	uint32_t		 status;

	uuid_to_string(uuid, &str, &status);
	if (status != uuid_s_ok) {
		rslt = 1;
		goto done;
	}

	rslt = 0;
	for(i = 0; i < nitems(protected_guid); i++) {
		if (strncmp(str, protected_guid[i].pg_guid, UUID_STR_LEN) == 0) {
			rslt = 1;
			break;
		}
	}

 done:
	free(str);
	return rslt;
}

void
PRT_printall(void)
{
	int			i, idrows;

	idrows = (nitems(part_types) + 3) / 4;

	printf("Choose from the following Partition id values:\n");
	for (i = 0; i < idrows; i++) {
		printf("%02X %s   %02X %s   %02X %s",
		    part_types[i].pt_type, part_types[i].pt_sname,
		    part_types[i+idrows].pt_type, part_types[i+idrows].pt_sname,
		    part_types[i+idrows*2].pt_type, part_types[i+idrows*2].pt_sname);
		if ((i+idrows*3) < (sizeof(part_types)/sizeof(struct part_type))) {
			printf("   %02X %s\n",
			    part_types[i+idrows*3].pt_type,
			    part_types[i+idrows*3].pt_sname);
		} else
			printf( "\n" );
	}
}

const char *
ascii_id(const int id)
{
	static char		unknown[] = "<Unknown ID>";
	int			i;

	for (i = 0; i < nitems(part_types); i++) {
		if (part_types[i].pt_type == id)
			return part_types[i].pt_sname;
	}

	return unknown;
}

void
PRT_parse(const struct dos_partition *dp, const uint64_t lba_self,
    const uint64_t lba_firstembr, struct prt *prt)
{
	off_t			off;
	uint32_t		t;

	prt->prt_flag = dp->dp_flag;
	prt->prt_shead = dp->dp_shd;

	prt->prt_ssect = (dp->dp_ssect) & 0x3F;
	prt->prt_scyl = ((dp->dp_ssect << 2) & 0xFF00) | dp->dp_scyl;

	prt->prt_id = dp->dp_typ;
	prt->prt_ehead = dp->dp_ehd;
	prt->prt_esect = (dp->dp_esect) & 0x3F;
	prt->prt_ecyl = ((dp->dp_esect << 2) & 0xFF00) | dp->dp_ecyl;

	if ((prt->prt_id == DOSPTYP_EXTEND) || (prt->prt_id == DOSPTYP_EXTENDL))
		off = lba_firstembr;
	else
		off = lba_self;

#if 0 /* XXX */
	prt->prt_bs = letoh32(dp->dp_start) + off;
	prt->prt_ns = letoh32(dp->dp_size);
	if (prt->prt_id == DOSPTYP_EFI && partn == UINT32_MAX)
		prt->prt_ns = DL_GETDSIZE(&dl) - prt->prt_bs;
#else
	memcpy(&t, &dp->dp_start, sizeof(uint32_t));
	prt->prt_bs = letoh32(t) + off;
	memcpy(&t, &dp->dp_size, sizeof(uint32_t));
	prt->prt_ns = letoh32(t);
	if (prt->prt_id == DOSPTYP_EFI && prt->prt_ns == UINT32_MAX)
		prt->prt_ns = DL_GETDSIZE(&dl) - prt->prt_bs;
#endif

	PRT_fix_CHS(prt);
}

int
check_chs(const struct prt *prt)
{
	if ( (prt->prt_shead > 255) ||
		(prt->prt_ssect >63) ||
		(prt->prt_scyl > 1023) ||
		(prt->prt_ehead >255) ||
		(prt->prt_esect >63) ||
		(prt->prt_ecyl > 1023) )
	{
		return -1;
	}
	return 0;
}

void
PRT_make(const struct prt *prt, const uint64_t lba_self, const uint64_t lba_firstembr,
    struct dos_partition *dp)
{
	uint64_t		off, t;
	uint32_t		ecyl, scyl;

	scyl = (prt->prt_scyl > 1023) ? 1023 : prt->prt_scyl;
	ecyl = (prt->prt_ecyl > 1023) ? 1023 : prt->prt_ecyl;

	if ((prt->prt_id == DOSPTYP_EXTEND) || (prt->prt_id == DOSPTYP_EXTENDL))
		off = lba_firstembr;
	else
		off = lba_self;

	if (check_chs(prt) == 0) {
		dp->dp_shd = prt->prt_shead & 0xFF;
		dp->dp_ssect = (prt->prt_ssect & 0x3F) | ((scyl & 0x300) >> 2);
		dp->dp_scyl = scyl & 0xFF;
		dp->dp_ehd = prt->prt_ehead & 0xFF;
		dp->dp_esect = (prt->prt_esect & 0x3F) | ((ecyl & 0x300) >> 2);
		dp->dp_ecyl = ecyl & 0xFF;
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
PRT_print(const int num, const struct prt *prt, const char *units)
{
	const int		secsize = unit_types[SECTORS].ut_conversion;
	double			size;
	int			i;

	i = unit_lookup(units);

	if (prt == NULL) {
		printf("            Starting         Ending    "
		    "     LBA Info:\n");
		printf(" #: id      C   H   S -      C   H   S "
		    "[       start:        size ]\n");
		printf("---------------------------------------"
		    "----------------------------------------\n");
	} else {
		size = ((double)prt->prt_ns * secsize) / unit_types[i].ut_conversion;
		printf("%c%1d: %.2X %6u %3u %3u - %6u %3u %3u "
		    "[%12llu:%12.0f%s] %s\n",
		    (prt->prt_flag == DOSACTIVE)?'*':' ',
		    num, prt->prt_id,
		    prt->prt_scyl, prt->prt_shead, prt->prt_ssect,
		    prt->prt_ecyl, prt->prt_ehead, prt->prt_esect,
		    prt->prt_bs, size,
		    unit_types[i].ut_abbr,
		    ascii_id(prt->prt_id));
	}
}

void
PRT_fix_BN(struct prt *prt, const int pn)
{
	uint32_t		spt, tpc, spc;
	uint32_t		start = 0;
	uint32_t		end = 0;

	/* Zero out entry if not used */
	if (prt->prt_id == DOSPTYP_UNUSED) {
		memset(prt, 0, sizeof(*prt));
		return;
	}

	/* Disk geometry. */
	spt = disk.dk_sectors;
	tpc = disk.dk_heads;
	spc = spt * tpc;

	start += prt->prt_scyl * spc;
	start += prt->prt_shead * spt;
	start += prt->prt_ssect - 1;

	end += prt->prt_ecyl * spc;
	end += prt->prt_ehead * spt;
	end += prt->prt_esect - 1;

	/* XXX - Should handle this... */
	if (start > end)
		warnx("Start of partition #%d after end!", pn);

	prt->prt_bs = start;
	prt->prt_ns = (end - start) + 1;
}

void
PRT_fix_CHS(struct prt *prt)
{
	uint32_t		spt, tpc, spc;
	uint32_t		start, end, size;
	uint32_t		cyl, head, sect;

	/* Zero out entry if not used */
	if (prt->prt_id == DOSPTYP_UNUSED || prt->prt_ns == 0) {
		memset(prt, 0, sizeof(*prt));
		return;
	}

	/* Disk geometry. */
	spt = disk.dk_sectors;
	tpc = disk.dk_heads;
	spc = spt * tpc;

	start = prt->prt_bs;
	size = prt->prt_ns;
	end = (start + size) - 1;

	/* Figure out starting CHS values */
	cyl = (start / spc); start -= (cyl * spc);
	head = (start / spt); start -= (head * spt);
	sect = (start + 1);

	prt->prt_scyl = cyl;
	prt->prt_shead = head;
	prt->prt_ssect = sect;

	/* Figure out ending CHS values */
	cyl = (end / spc); end -= (cyl * spc);
	head = (end / spt); end -= (head * spt);
	sect = (end + 1);

	prt->prt_ecyl = cyl;
	prt->prt_ehead = head;
	prt->prt_esect = sect;
}

char *
PRT_uuid_to_typename(const struct uuid *uuid)
{
	static char		 partition_type[UUID_STR_LEN + 1];
	char			*uuidstr = NULL;
	int			 i, entries, status;

	memset(partition_type, 0, sizeof(partition_type));

	uuid_to_string(uuid, &uuidstr, &status);
	if (status != uuid_s_ok)
		goto done;

	entries = nitems(part_types);

	for (i = 0; i < entries; i++) {
		if (memcmp(part_types[i].pt_guid, uuidstr,
		    sizeof(part_types[i].pt_guid)) == 0)
			break;
	}

	if (i < entries)
		strlcpy(partition_type, part_types[i].pt_sname,
		    sizeof(partition_type));
	else
		strlcpy(partition_type, uuidstr, sizeof(partition_type));

done:
	free(uuidstr);

	return partition_type;
}

int
PRT_uuid_to_type(const struct uuid *uuid)
{
	char			*uuidstr;
	int			 i, status, type;

	type = 0;

	uuid_to_string(uuid, &uuidstr, &status);
	if (status != uuid_s_ok)
		goto done;

	for (i = 0; i < nitems(part_types); i++) {
		if (memcmp(part_types[i].pt_guid, uuidstr,
		    sizeof(part_types[i].pt_guid)) == 0) {
			type = part_types[i].pt_type;
			break;
		}
	}

done:
	free(uuidstr);
	return type;
}

struct uuid *
PRT_type_to_uuid(const int type)
{
	static struct uuid	guid;
	int			i, entries, status = uuid_s_ok;

	memset(&guid, 0, sizeof(guid));

	entries = nitems(part_types);

	for (i = 0; i < entries; i++) {
		if (part_types[i].pt_type == type)
			break;
	}
	if (i < entries)
		uuid_from_string(part_types[i].pt_guid, &guid, &status);
	if (i == entries || status != uuid_s_ok)
		uuid_from_string(part_types[0].pt_guid, &guid, &status);

	return &guid;
}
