/*	$OpenBSD: user.c,v 1.13 1998/09/08 11:03:16 pefo Exp $	*/

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
#include <string.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/disklabel.h>
#include <machine/param.h>
#include "user.h"
#include "disk.h"
#include "misc.h"
#include "mbr.h"
#include "cmd.h"


/* Our command table */
static cmd_table_t cmd_table[] = {
	{"help",   Xhelp,	"Command help list"},
	{"manual", Xmanual,	"Show entire OpenBSD man page for fdisk"},
	{"reinit", Xreinit,	"Re-initialize loaded MBR (to defaults)"},
	{"disk",   Xdisk,	"Edit current drive stats"},
	{"edit",   Xedit,	"Edit given table entry"},
	{"flag",   Xflag,	"Flag given table entry as bootable"},
	{"update", Xupdate,	"Update machine code in loaded MBR"},
	{"select", Xselect,	"Select extended partition table entry MBR"},
	{"print",  Xprint,	"Print loaded MBR partition table"},
	{"write",  Xwrite,	"Write loaded MBR to disk"},
	{"exit",   Xexit,	"Exit edit of current MBR, without saving changes"},
	{"quit",   Xquit,	"Quit edit of current MBR, saving current changes"},
	{"abort",  Xabort,	"Abort program without saving current changes"},
	{NULL,     NULL,	NULL}
};


int
USER_init(disk, tt)
	disk_t *disk;
	mbr_t *tt;
{
	int fd;
	char mbr_buf[DEV_BSIZE];

	/* Fix up given mbr for this disk */
	tt->part[0].flag = 0;
	tt->part[1].flag = 0;
	tt->part[2].flag = 0;
	tt->part[3].flag = DOSACTIVE;
	tt->signature = DOSMBR_SIGNATURE;

	/* Use whole disk, save for first head, on first cyl. */
	tt->part[3].id = DOSPTYP_OPENBSD;
	tt->part[3].scyl = 0;
	tt->part[3].shead = 1;
	tt->part[3].ssect = 1;

	/* Go right to the end */
	tt->part[3].ecyl = disk->real->cylinders;
	tt->part[3].ehead = disk->real->heads;
	tt->part[3].esect = disk->real->sectors;

	/* Fix up start/length fields */
	PRT_fix_BN(disk, &tt->part[3]);

#if defined(__powerpc__)
	/* Now fix up for the MS-DOS boot partition on PowerPC. */
	tt->part[0].flag = DOSACTIVE;  /* Boot from dos part */
	tt->part[3].flag = 0;
	tt->part[3].ns += tt->part[3].bs;
	tt->part[3].bs = tt->part[0].bs + tt->part[0].ns;
	tt->part[3].ns -= tt->part[3].bs;
	PRT_fix_CHS(disk, &tt->part[3]);
#endif
	/* Write sector 0 */
	printf("\a\n"
	   "\t-----------------------------------------------------\n"
	   "\t------ ATTENTION - UPDATING MASTER BOOT RECORD ------\n"
	   "\t-----------------------------------------------------\n");
	if (ask_yn("\nDo you wish to write new MBR?")) {
		fd = DISK_open(disk->name, O_RDWR);
		MBR_make(tt, mbr_buf);
		MBR_write(fd, (off_t)0, mbr_buf);
		DISK_close(fd);
	} else
		printf("MBR is unchanged\n");

	return (0);
}

int modified;

int
USER_modify(disk, tt, offset, reloff)
	disk_t *disk;
	mbr_t *tt;
	off_t offset;
	off_t reloff;
{
	static int editlevel;
	char mbr_buf[DEV_BSIZE];
	mbr_t mbr;
	cmd_t cmd;
	int i, st, fd;

	/* One level deeper */
	editlevel += 1;

	/* Set up command table pointer */
	cmd.table = cmd_table;

	/* Read MBR & partition */
	fd = DISK_open(disk->name, O_RDONLY);
	MBR_read(fd, offset, mbr_buf);
	DISK_close(fd);

	/* Parse the sucker */
	MBR_parse(mbr_buf, offset, reloff, &mbr);

	printf("Enter 'help' for information\n");

	/* Edit cycle */
	do {
again:
		printf("fdisk:%c%d> ", (modified)?'*':' ', editlevel);
		fflush(stdout);
		ask_cmd(&cmd);

		if (cmd.cmd[0] == '\0')
			goto again;
		for (i = 0; cmd_table[i].cmd != NULL; i++)
			if (strstr(cmd_table[i].cmd, cmd.cmd)==cmd_table[i].cmd)
				break;

		/* Quick hack to put in '?' == 'help' */
		if (!strcmp(cmd.cmd, "?"))
			i = 0;

		/* Check for valid command */
		if (cmd_table[i].cmd == NULL) {
			printf("Invalid command '%s'.  Try 'help'.\n", cmd.cmd);
			continue;
		} else
			strcpy(cmd.cmd, cmd_table[i].cmd);

		/* Call function */
		st = cmd_table[i].fcn(&cmd, disk, &mbr, tt, offset);

		/* Update status */
		if (st == CMD_EXIT)
			break;
		if (st == CMD_SAVE)
			break;
		if (st == CMD_CLEAN)
			modified = 0;
		if (st == CMD_DIRTY)
			modified = 1;
	} while (1);

	/* Write out MBR */
	if (modified) {
		if (st == CMD_SAVE) {
			printf("Writing current MBR to disk.\n");
			fd = DISK_open(disk->name, O_RDWR);
			MBR_make(&mbr, mbr_buf);
			MBR_write(fd, offset, mbr_buf);
			close(fd);
		} else
			printf("Aborting changes to current MBR.\n");
	}

	/* One level less */
	editlevel -= 1;

	return (0);
}

int
USER_print_disk(disk)
	disk_t *disk;
{
	int fd, offset, firstoff, i;
	char mbr_buf[DEV_BSIZE];
	mbr_t mbr;

	fd = DISK_open(disk->name, O_RDONLY);
	offset = firstoff = 0;

	DISK_printmetrics(disk);

	do {
		MBR_read(fd, (off_t)offset, mbr_buf);
		MBR_parse(mbr_buf, offset, firstoff, &mbr);

		printf("Offset: %d\t", (int)offset);
		MBR_print(&mbr);

		/* Print out extended partitions too */
		for (offset = i = 0; i < 4; i++)
			if (mbr.part[i].id == DOSPTYP_EXTEND) {
				offset = mbr.part[i].bs;
				if (firstoff == 0)
					firstoff = offset;
			}
	} while (offset);

	return (DISK_close(fd));
}

