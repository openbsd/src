/*	$OpenBSD: cmd.c,v 1.72 2015/02/09 04:27:15 krw Exp $	*/

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

#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/disklabel.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include "disk.h"
#include "misc.h"
#include "part.h"
#include "mbr.h"
#include "user.h"
#include "cmd.h"

int
Xreinit(char *args, struct disk *disk, struct mbr *mbr, struct mbr *tt,
    int offset)
{
	struct dos_mbr dos_mbr;

	/* Copy template MBR */
	MBR_make(tt, &dos_mbr);
	MBR_parse(disk, &dos_mbr, mbr->offset, mbr->reloffset, mbr);

	MBR_init(disk, mbr);

	/* Tell em we did something */
	printf("In memory copy is initialized to:\n");
	printf("Offset: %d\t", offset);
	MBR_print(mbr, args);
	printf("Use 'write' to update disk.\n");

	return (CMD_DIRTY);
}

int
Xdisk(char *args, struct disk *disk, struct mbr *mbr, struct mbr *tt,
    int offset)
{
	int maxcyl  = 1024;
	int maxhead = 256;
	int maxsec  = 63;

	/* Print out disk info */
	DISK_printgeometry(disk, args);

#if defined (__powerpc__) || defined (__mips__)
	maxcyl  = 9999999;
	maxhead = 9999999;
	maxsec  = 9999999;
#endif

	/* Ask for new info */
	if (ask_yn("Change disk geometry?")) {
		disk->cylinders = ask_num("BIOS Cylinders",
		    disk->cylinders, 1, maxcyl);
		disk->heads = ask_num("BIOS Heads",
		    disk->heads, 1, maxhead);
		disk->sectors = ask_num("BIOS Sectors",
		    disk->sectors, 1, maxsec);

		disk->size = disk->cylinders * disk->heads * disk->sectors;
	}

	return (CMD_CONT);
}

int
Xswap(char *args, struct disk *disk, struct mbr *mbr, struct mbr *tt,
    int offset)
{
	const char *errstr;
	char *from, *to;
	int pf, pt;
	struct prt pp;

	to = args;
	from = strsep(&to, " \t");

	if (to == NULL) {
		printf("partition number is invalid:\n");
		return (CMD_CONT);
	}

	pf = strtonum(from, 0, 3, &errstr);
	if (errstr) {
		printf("partition number is %s: %s\n", errstr, from);
		return (CMD_CONT);
	}
	pt = strtonum(to, 0, 3, &errstr);
	if (errstr) {
		printf("partition number is %s: %s\n", errstr, to);
		return (CMD_CONT);
	}

	if (pt == pf) {
		printf("%d same partition as %d, doing nothing.\n", pt, pf);
		return (CMD_CONT);
	}

	pp = mbr->part[pt];
	mbr->part[pt] = mbr->part[pf];
	mbr->part[pf] = pp;

	return (CMD_DIRTY);
}

int
Xedit(char *args, struct disk *disk, struct mbr *mbr, struct mbr *tt,
    int offset)
{
	const char *errstr;
	int pn, num, ret;
	struct prt *pp;

	pn = strtonum(args, 0, 3, &errstr);
	if (errstr) {
		printf("partition number is %s: %s\n", errstr, args);
		return (CMD_CONT);
	}
	pp = &mbr->part[pn];

	/* Edit partition type */
	ret = Xsetpid(args, disk, mbr, tt, offset);

#define	EDIT(p, v, n, m)					\
	if ((num = ask_num(p, v, n, m)) != v)	\
		ret = CMD_DIRTY;				\
	v = num;

	/* Unused, so just zero out */
	if (pp->id == DOSPTYP_UNUSED) {
		memset(pp, 0, sizeof(*pp));
		printf("Partition %d is disabled.\n", pn);
		return (ret);
	}

	/* Change table entry */
	if (ask_yn("Do you wish to edit in CHS mode?")) {
		int maxcyl, maxhead, maxsect;

		/* Shorter */
		maxcyl = disk->cylinders - 1;
		maxhead = disk->heads - 1;
		maxsect = disk->sectors;

		/* Get data */
		EDIT("BIOS Starting cylinder", pp->scyl,  0, maxcyl);
		EDIT("BIOS Starting head",     pp->shead, 0, maxhead);
		EDIT("BIOS Starting sector",   pp->ssect, 1, maxsect);
		EDIT("BIOS Ending cylinder",   pp->ecyl,  0, maxcyl);
		EDIT("BIOS Ending head",       pp->ehead, 0, maxhead);
		EDIT("BIOS Ending sector",     pp->esect, 1, maxsect);
		/* Fix up off/size values */
		PRT_fix_BN(disk, pp, pn);
		/* Fix up CHS values for LBA */
		PRT_fix_CHS(disk, pp);
	} else {
		pp->bs = getuint(disk, "Partition offset", pp->bs,
		    disk->size);
		pp->ns = getuint(disk, "Partition size", pp->ns,
		    disk->size - pp->bs);
		/* Fix up CHS values */
		PRT_fix_CHS(disk, pp);
	}
#undef EDIT
	return (ret);
}

int
Xsetpid(char *args, struct disk *disk, struct mbr *mbr, struct mbr *tt,
    int offset)
{
	const char *errstr;
	int pn, num;
	struct prt *pp;

	pn = strtonum(args, 0, 3, &errstr);
	if (errstr) {
		printf("partition number is %s: %s\n", errstr, args);
		return (CMD_CONT);
	}
	pp = &mbr->part[pn];

	/* Print out current table entry */
	PRT_print(0, NULL, NULL);
	PRT_print(pn, pp, NULL);

	/* Ask for partition type */
	num = ask_pid(pp->id);
	if (num == pp->id)
		return (CMD_CONT);

	pp->id = num;
	return (CMD_DIRTY);
}

int
Xselect(char *args, struct disk *disk, struct mbr *mbr, struct mbr *tt,
    int offset)
{
	const char *errstr;
	static int firstoff = 0;
	int off;
	int pn;

	pn = strtonum(args, 0, 3, &errstr);
	if (errstr) {
		printf("partition number is %s: %s\n", errstr, args);
		return (CMD_CONT);
	}

	off = mbr->part[pn].bs;

	/* Sanity checks */
	if ((mbr->part[pn].id != DOSPTYP_EXTEND) &&
	    (mbr->part[pn].id != DOSPTYP_EXTENDL)) {
		printf("Partition %d is not an extended partition.\n", pn);
		return (CMD_CONT);
	}

	if (firstoff == 0)
		firstoff = off;

	if (!off) {
		printf("Loop to offset 0!  Not selected.\n");
		return (CMD_CONT);
	} else {
		printf("Selected extended partition %d\n", pn);
		printf("New MBR at offset %d.\n", off);
	}

	/* Recursion is beautiful! */
	USER_edit(disk, tt, off, firstoff);
	return (CMD_CONT);
}

int
Xprint(char *args, struct disk *disk, struct mbr *mbr, struct mbr *tt,
    int offset)
{

	DISK_printgeometry(disk, args);
	printf("Offset: %d\t", offset);
	MBR_print(mbr, args);

	return (CMD_CONT);
}

int
Xwrite(char *args, struct disk *disk, struct mbr *mbr, struct mbr *tt,
    int offset)
{
	struct dos_mbr dos_mbr;
	int fd, i, n;

	for (i = 0, n = 0; i < NDOSPART; i++)
		if (mbr->part[i].id == 0xA6)
			n++;
	if (n >= 2) {
		warnx("MBR contains more than one OpenBSD partition!");
		if (!ask_yn("Write MBR anyway?"))
			return (CMD_CONT);
	}

	fd = DISK_open(disk->name, O_RDWR);
	MBR_make(mbr, &dos_mbr);

	printf("Writing MBR at offset %d.\n", offset);
	if (MBR_write(fd, offset, &dos_mbr) == -1) {
		int saved_errno = errno;
		warn("error writing MBR");
		close(fd);
		errno = saved_errno;
		return (CMD_CONT);
	}
	close(fd);

	/* Refresh in memory copy to reflect what was just written. */
	MBR_parse(disk, &dos_mbr, mbr->offset, mbr->reloffset, mbr);

	return (CMD_CLEAN);
}

int
Xquit(char *args, struct disk *disk, struct mbr *mbr, struct mbr *tt,
    int offset)
{

	/* Nothing to do here */
	return (CMD_SAVE);
}

int
Xabort(char *args, struct disk *disk, struct mbr *mbr, struct mbr *tt,
    int offset)
{
	exit(0);

	/* NOTREACHED */
	return (CMD_CONT);
}

int
Xexit(char *args, struct disk *disk, struct mbr *mbr, struct mbr *tt,
    int offset)
{

	/* Nothing to do here */
	return (CMD_EXIT);
}

int
Xhelp(char *args, struct disk *disk, struct mbr *mbr, struct mbr *tt,
    int offset)
{
	int i;

	for (i = 0; cmd_table[i].cmd != NULL; i++)
		printf("\t%s\t\t%s\n", cmd_table[i].cmd, cmd_table[i].help);
	return (CMD_CONT);
}

int
Xupdate(char *args, struct disk *disk, struct mbr *mbr, struct mbr *tt,
    int offset)
{
	/* Update code */
	memcpy(mbr->code, tt->code, sizeof(mbr->code));
	mbr->signature = DOSMBR_SIGNATURE;
	printf("Machine code updated.\n");
	return (CMD_DIRTY);
}

int
Xflag(char *args, struct disk *disk, struct mbr *mbr, struct mbr *tt,
    int offset)
{
	const char *errstr;
	int i, pn = -1, val = -1;
	char *part, *flag;

	flag = args;
	part = strsep(&flag, " \t");

	pn = strtonum(part, 0, 3, &errstr);
	if (errstr) {
		printf("partition number is %s: %s.\n", errstr, part);
		return (CMD_CONT);
	}

	if (flag != NULL) {
		val = (int)strtonum(flag, 0, 0xff, &errstr);
		if (errstr) {
			printf("flag value is %s: %s.\n", errstr, flag);
			return (CMD_CONT);
		}
	}

	if (val == -1) {
		/* Set active flag */
		for (i = 0; i < 4; i++) {
			if (i == pn)
				mbr->part[i].flag = DOSACTIVE;
			else
				mbr->part[i].flag = 0x00;
		}
		printf("Partition %d marked active.\n", pn);
	} else {
		mbr->part[pn].flag = val;
		printf("Partition %d flag value set to 0x%x.\n", pn, val);
	}
	return (CMD_DIRTY);
}

int
Xmanual(char *args, struct disk *disk, struct mbr *mbr, struct mbr *tt,
    int offset)
{
	char *pager = "/usr/bin/less";
	char *p;
	sig_t opipe;
	extern const unsigned char manpage[];
	extern const int manpage_sz;
	FILE *f;

	opipe = signal(SIGPIPE, SIG_IGN);
	if ((p = getenv("PAGER")) != NULL && (*p != '\0'))
		pager = p;
	if (asprintf(&p, "gunzip -qc|%s", pager) != -1) {
		f = popen(p, "w");
		if (f) {
			fwrite(manpage, manpage_sz, 1, f);
			pclose(f);
		}
		free(p);
	}

	signal(SIGPIPE, opipe);
	return (CMD_CONT);
}
