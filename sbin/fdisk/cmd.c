/*	$OpenBSD: cmd.c,v 1.20 1998/09/14 03:54:34 rahnds Exp $	*/

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

#include <stdio.h>
#include <ctype.h>
#include <memory.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/fcntl.h>
#include <sys/disklabel.h>
#include "disk.h"
#include "misc.h"
#include "user.h"
#include "part.h"
#include "cmd.h"
#define MAX(a, b) ((a) >= (b) ? (a) : (b))

int
Xreinit(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{
	char buf[DEV_BSIZE];

	/* Copy template MBR */
	MBR_make(tt, buf);
	MBR_parse(disk, buf, 0, 0, mbr);

	MBR_init(disk, mbr);

	/* Tell em we did something */
	printf("In memory copy is initialized to:\n");
	printf("Offset: %d\t", offset);
	MBR_print(mbr);
	printf("Use 'write' to update disk.\n");

	return (CMD_DIRTY);
}

int
Xdisk(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{
	int maxcyl  = 1024;
	int maxhead = 256;
	int maxsec  = 64;

	/* Print out disk info */
	DISK_printmetrics(disk);

#ifdef __powerpc__
	maxcyl  = 9999999;
	maxhead = 9999999;
	maxsec  = 9999999;
#endif

	/* Ask for new info */
	if (ask_yn("Change disk geometry?")) {
		disk->real->cylinders = ask_num("BIOS Cylinders", ASK_DEC,
		    disk->real->cylinders, 1, maxcyl, NULL);
		disk->real->heads = ask_num("BIOS Heads", ASK_DEC,
		    disk->real->heads, 1, maxhead, NULL);
		disk->real->sectors = ask_num("BIOS Sectors", ASK_DEC,
		    disk->real->sectors, 1, maxsec, NULL);

		disk->real->size = disk->real->cylinders * disk->real->heads
			* disk->real->sectors;
	}

	return (CMD_CONT);
}

int
Xedit(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{
	int pn, num, ret;
	prt_t *pp;

	ret = CMD_CONT;

	if (!isdigit(cmd->args[0])) {
		printf("Invalid argument: %s <partition number>\n", cmd->cmd);
		return (ret);
	}
	pn = atoi(cmd->args);

	if (pn < 0 || pn > 3) {
		printf("Invalid partition number.\n");
		return (ret);
	}

	/* Print out current table entry */
	pp = &mbr->part[pn];
	PRT_print(0, NULL);
	PRT_print(pn, pp);

#define	EDIT(p, f, v, n, m, h)				\
	if ((num = ask_num(p, f, v, n, m, h)) != v)	\
		ret = CMD_DIRTY;			\
	v = num;

	/* Ask for partition type */
	EDIT("Partition id ('0' to disable) ", ASK_HEX, pp->id, 0, 0xFF, PRT_printall);

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
		maxcyl = disk->real->cylinders - 1;
		maxhead = disk->real->heads - 1;
		maxsect = disk->real->sectors;

		/* Get data */
		EDIT("BIOS Starting cylinder", ASK_DEC, pp->scyl,  0, maxcyl, NULL);
		EDIT("BIOS Starting head",     ASK_DEC, pp->shead, 0, maxhead, NULL);
		EDIT("BIOS Starting sector",   ASK_DEC, pp->ssect, 1, maxsect, NULL);
		EDIT("BIOS Ending cylinder",   ASK_DEC, pp->ecyl,  0, maxcyl, NULL);
		EDIT("BIOS Ending head",       ASK_DEC, pp->ehead, 0, maxhead, NULL);
		EDIT("BIOS Ending sector",     ASK_DEC, pp->esect, 1, maxsect, NULL);
		/* Fix up off/size values */
		PRT_fix_BN(disk, pp);
	} else {
		u_int m;

		/* Get data */
		EDIT("Partition offset", ASK_DEC, pp->bs, 0,
		    disk->real->size, NULL);
		m = MAX(pp->ns, disk->real->size - pp->bs);
		if ( m > disk->real->size - pp->bs) {
			/* dont have default value extend beyond end of disk */
			m = disk->real->size - pp->bs;
		}
		EDIT("Partition size", ASK_DEC, pp->ns, 1,
		    m, NULL);

		/* Fix up CHS values */
		PRT_fix_CHS(disk, pp);
	}
#undef EDIT
	return (ret);
}

int
Xselect(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{
	static firstoff = 0;
	int off;
	int pn;

	if (!isdigit(cmd->args[0])) {
		printf("Invalid argument: %s <partition number>\n", cmd->cmd);
		return (CMD_CONT);
	}

	pn = atoi(cmd->args);
	off = mbr->part[pn].bs;

	/* Sanity checks */
	if (mbr->part[pn].id != DOSPTYP_EXTEND) {
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

	/* Recursion is beautifull! */
	USER_modify(disk, tt, off, firstoff);
	return (CMD_CONT);
}

int
Xprint(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{

	DISK_printmetrics(disk);
	printf("Offset: %d\t", offset);
	MBR_print(mbr);

	return (CMD_CONT);
}

int
Xwrite(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{
	char mbr_buf[DEV_BSIZE];
	int fd;

	printf("Writing MBR at offset %d.\n", offset);

	fd = DISK_open(disk->name, O_RDWR);
	MBR_make(mbr, mbr_buf);
	MBR_write(fd, offset, mbr_buf);
	close(fd);
	return (CMD_CLEAN);
}

int
Xquit(cmd, disk, r, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *r;
	mbr_t *tt;
	int offset;
{

	/* Nothing to do here */
	return (CMD_SAVE);
}

int
Xabort(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{
	exit(0);

	/* NOTREACHED */
	return (CMD_CONT);
}


int
Xexit(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{

	/* Nothing to do here */
	return (CMD_EXIT);
}

int
Xhelp(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{
	cmd_table_t *cmd_table = cmd->table;
	int i;

	/* Hmm, print out cmd_table here... */
	for (i = 0; cmd_table[i].cmd != NULL; i++)
		printf("\t%s\t\t%s\n", cmd_table[i].cmd, cmd_table[i].help);
	return (CMD_CONT);
}

int
Xupdate(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{

	/* Update code */
	memcpy(mbr->code, tt->code, MBR_CODE_SIZE);
	printf("Machine code updated.\n");
	return (CMD_DIRTY);
}

int
Xflag(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{
	int i, pn = -1;

	/* Parse partition table entry number */
	if (!isdigit(cmd->args[0])) {
		printf("Invalid argument: %s <partition number>\n", cmd->cmd);
		return (CMD_CONT);
	}
	pn = atoi(cmd->args);

	if (pn < 0 || pn > 3) {
		printf("Invalid partition number.\n");
		return (CMD_CONT);
	}

	/* Set active flag */
	for (i = 0; i < 4; i++) {
		if (i == pn)
			mbr->part[i].flag = DOSACTIVE;
		else
			mbr->part[i].flag = 0x00;
	}

	printf("Partition %d marked active.\n", pn);
	return (CMD_DIRTY);
}

int
Xmanual(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{
	char *pager = "/usr/bin/less";
	sig_t opipe = signal(SIGPIPE, SIG_IGN);
	extern char manpage[];
	FILE *f;

	if (getenv("PAGER"))
		pager = getenv("PAGER");
	f = popen(pager, "w");
	if (f) {
		(void) fwrite(manpage, strlen(manpage), 1, f);
		pclose(f);
	}

	(void)signal(SIGPIPE, opipe);
	return (CMD_CONT);
}
