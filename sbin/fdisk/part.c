/*	$OpenBSD: part.c,v 1.9 1998/09/14 03:54:35 rahnds Exp $	*/

/*
 * Copyright (c) 1997 Tobias Weingartner
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Tobias Weingartner.
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

#include <err.h>
#include <util.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/disklabel.h>
#include <machine/param.h>
#include "disk.h"
#include "misc.h"
#include "mbr.h"


static struct part_type {
	int	type;
	char	*sname;
	char	*lname;
} part_types[] = {
	{ 0x00, "unused      ", "unused"},
	{ 0x01, "DOS FAT-12  ", "Primary DOS with 12 bit FAT"},
	{ 0x02, "XENIX /     ", "XENIX / filesystem"},
	{ 0x03, "XENIX /usr  ", "XENIX /usr filesystem"},
	{ 0x04, "DOS FAT-16  ", "Primary DOS with 16 bit FAT"},
	{ 0x05, "Extended DOS", "Extended DOS"},
	{ 0x06, "DOS > 32MB  ", "Primary 'big' DOS (> 32MB)"},
	{ 0x07, "HPFS/QNX/AUX", "OS/2 HPFS, QNX or Advanced UNIX"},
	{ 0x08, "AIX fs      ", "AIX filesystem"},
	{ 0x09, "AIX/Coherent", "AIX boot partition or Coherent"},
	{ 0x0A, "OS/2 Bootmgr", "OS/2 Boot Manager or OPUS"},
	{ 0x0B, "Win95 FAT-32", "Primary Win95 w/ 32-bit FAT"},
	{ 0x0E, "DOS FAT-16  ", "Primary DOS w/ 16-bit FAT, CHS-mapped"},
	{ 0x10, "OPUS        ", "OPUS"},
	{ 0x12, "Compaq Diag.", "Compaq Diagnostics"},
	{ 0x40, "VENIX 286   ", "VENIX 286"},
	{ 0x50, "DM          ", "DM"},
	{ 0x51, "DM          ", "DM"},
	{ 0x52, "CP/M or SysV", "CP/M or Microport SysV/AT"},
	{ 0x54, "Ontrack     ", "Ontrack"},
	{ 0x56, "GB          ", "GB"},
	{ 0x61, "Speed       ", "Speed"},
	{ 0x63, "ISC, HURD, *", "ISC, System V/386, GNU HURD or Mach"},
	{ 0x64, "Netware 2.xx", "Novell Netware 2.xx"},
	{ 0x65, "Netware 3.xx", "Novell Netware 3.xx"},
	{ 0x75, "PCIX        ", "PCIX"},
	{ 0x80, "Minix (old) ", "Minix 1.1 ... 1.4a"},
	{ 0x81, "Minix (new) ", "Minix 1.4b ... 1.5.10"},
	{ 0x82, "Linux swap  ", "Linux swap"},
	{ 0x83, "Linux files*", "Linux filesystem"},
	{ 0x93, "Amoeba file*", "Amoeba filesystem"},
	{ 0x94, "Amoeba BBT  ", "Amoeba bad block table"},
	{ 0xA5, "FreeBSD",	"FreeBSD"},
	{ 0xA6, "OpenBSD     ", "OpenBSD"},
	{ 0xA7, "NEXTSTEP    ", "NEXTSTEP"},
	{ 0xA9, "NetBSD",	"NetBSD"},
	{ 0xB7, "BSDI filesy*", "BSDI BSD/386 filesystem"},
	{ 0xB8, "BSDI swap   ", "BSDI BSD/386 swap"},
	{ 0xDB, "CPM/C.DOS/C*", "Concurrent CPM or C.DOS or CTOS"},
	{ 0xE1, "Speed       ", "Speed"},
	{ 0xE3, "Speed       ", "Speed"},
	{ 0xE4, "Speed       ", "Speed"},
	{ 0xF1, "Speed       ", "Speed"},
	{ 0xF2, "DOS 3.3+ Sec", "DOS 3.3+ Secondary"},
	{ 0xF4, "Speed       ", "Speed"},
	{ 0xFF, "BBT         ", "BBT (Bad Blocks Table)"},
};

void
PRT_printall()
{
	int i;

	printf("Choose from the following Partition id values:\n");
	for (i = 0; i < sizeof(part_types)/sizeof(struct part_type); i++) {
		printf("%02X %s%s", part_types[i].type,
		    part_types[i].sname, (i+1) % 4 ? "   " : "\n");
	}
	if (i % 4)
		printf("\n");
}

char *
PRT_ascii_id(id)
	int id;
{
	static char unknown[] = "<Unknown ID>";
	int i;

	for (i = 0; i < sizeof(part_types)/sizeof(struct part_type); i++) {
		if (part_types[i].type == id)
			return (part_types[i].sname);
	}

	return (unknown);
}

void
PRT_parse(disk, prt, offset, reloff, partn)
	disk_t *disk;
	void *prt;
	off_t offset;
	off_t reloff;
	prt_t *partn;
{
	unsigned char *p = prt;
	off_t off;
	int need_fix_chs = 0;

	/* dont check fields 0 and 4, they are flag and id, always preserved */
	if ((p[1] == 0xff) && 
	    (p[2] == 0xff) && 
	    (p[3] == 0xff) && 
	    (p[5] == 0xff) && 
	    (p[6] == 0xff) && 
	    (p[7] == 0xff))
	{
		/* CHS values invalid */
		need_fix_chs =1;
	}
	partn->flag = *p++;
	partn->shead = *p++;

	partn->ssect = (*p) & 0x3F;
	partn->scyl = ((*p << 2) & 0xFF00) | (*(p+1));
	p += 2;

	partn->id = *p++;
	partn->ehead = *p++;
	partn->esect = (*p) & 0x3F;
	partn->ecyl = ((*p << 2) & 0xFF00) | (*(p+1));
	p += 2;

	off = partn->id != DOSPTYP_EXTEND ? offset : reloff;

	partn->bs = getlong(p) + off;
	partn->ns = getlong(p+4);

	if (need_fix_chs == 1) {
		printf("warning MBR CHS values invalid, translating LBA values\n");
		PRT_fix_CHS(disk, partn);
	}
}

int
PRT_check_chs(partn)
	prt_t *partn;
{
	if ( (partn->shead > 255) || 
		(partn->ssect >63) || 
		(partn->scyl > 1023) || 
		(partn->ehead >255) || 
		(partn->esect >63) || 
		(partn->ecyl > 1023) )
	{
		return 0;
	}
	return 1;
}
void
PRT_make(partn, offset, reloff, prt)
	prt_t *partn;
	off_t offset;
	off_t reloff;
	void *prt;
{
	unsigned char *p = prt;
	off_t off = partn->id != DOSPTYP_EXTEND ? offset : reloff; 

	if (PRT_check_chs(partn)) {
		*p++ = partn->flag & 0xFF;

		*p++ = partn->shead & 0xFF;
		*p++ = (partn->ssect & 0x3F) | ((partn->scyl & 0x300) >> 2);
		*p++ = partn->scyl & 0xFF;

		*p++ = partn->id & 0xFF;

		*p++ = partn->ehead & 0xFF;
		*p++ = (partn->esect & 0x3F) | ((partn->ecyl & 0x300) >> 2);
		*p++ = partn->ecyl & 0xFF;
	} else {
		/* should this really keep flag, id and set others to 0xff? */
		*p++ = partn->flag & 0xFF;
		*p++ = 0xFF;
		*p++ = 0xFF;
		*p++ = 0xFF;
		*p++ = partn->id & 0xFF;
		*p++ = 0xFF;
		*p++ = 0xFF;
		*p++ = 0xFF;
		printf("Warning CHS values out of bounds only saving LBA values\n");
	}

	putlong(p, partn->bs - off);
	putlong(p+4, partn->ns);
}

void
PRT_print(num, partn)
	int num;
	prt_t *partn;
{

	if (partn == NULL) {
		printf("         Starting        Ending\n");
		printf(" #: id  cyl  hd sec -  cyl  hd sec [     start -       size]\n");
		printf("-------------------------------------------------------------------------\n");
	} else {
		printf("%c%1d: %.2X %4d %3d %3d - %4d %3d %3d [%10d - %10d] %s\n",
			(partn->flag == 0x80)?'*':' ',
			num, partn->id,
			partn->scyl, partn->shead, partn->ssect,
			partn->ecyl, partn->ehead, partn->esect,
			partn->bs, partn->ns,
			PRT_ascii_id(partn->id));
	}
}

void
PRT_fix_BN(disk, part)
	disk_t *disk;
	prt_t *part;
{
	int spt, tpc, spc;
	int start = 0;
	int end = 0;

	/* Disk metrics */
	spt = disk->real->sectors;
	tpc = disk->real->heads;
	spc = spt * tpc;

	start += part->scyl * spc;
	start += part->shead * spt;
	start += part->ssect - 1;

	end += part->ecyl * spc;
	end += part->ehead * spt;
	end += part->esect - 1;

	/* XXX - Should handle this... */
	if (start > end)
		warn("Start of partition after end!");

	part->bs = start;
	part->ns = (end - start) + 1;
}

void
PRT_fix_CHS(disk, part)
	disk_t *disk;
	prt_t *part;
{
	int spt, tpc, spc;
	int start, end, size;
	int cyl, head, sect;

	/* Disk metrics */
	spt = disk->real->sectors;
	tpc = disk->real->heads;
	spc = spt * tpc;

	start = part->bs;
	size = part->ns;
	end = (start + size) - 1;

	/* Figure out starting CHS values */
	cyl = (start / spc); start -= (cyl * spc);
	head = (start / spt); start -= (head * spt);
	sect = (start + 1);

	part->scyl = cyl;
	part->shead = head;
	part->ssect = sect;

	/* Figure out ending CHS values */
	cyl = (end / spc); end -= (cyl * spc);
	head = (end / spt); end -= (head * spt);
	sect = (end + 1);

	part->ecyl = cyl;
	part->ehead = head;
	part->esect = sect;
}

