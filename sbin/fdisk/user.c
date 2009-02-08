/*	$OpenBSD: user.c,v 1.24 2009/02/08 18:03:18 krw Exp $	*/

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

#include <err.h>
#include <errno.h>
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
	{"setpid", Xsetpid,	"Set the identifier of a given table entry"},
	{"disk",   Xdisk,	"Edit current drive stats"},
	{"edit",   Xedit,	"Edit given table entry"},
	{"flag",   Xflag,	"Flag given table entry as bootable"},
	{"update", Xupdate,	"Update machine code in loaded MBR"},
	{"select", Xselect,	"Select extended partition table entry MBR"},
	{"swap",   Xswap,	"Swap two partition entries"},
	{"print",  Xprint,	"Print loaded MBR partition table"},
	{"write",  Xwrite,	"Write loaded MBR to disk"},
	{"exit",   Xexit,	"Exit edit of current MBR, without saving changes"},
	{"quit",   Xquit,	"Quit edit of current MBR, saving current changes"},
	{"abort",  Xabort,	"Abort program without saving current changes"},
	{NULL,     NULL,	NULL}
};


int
USER_init(disk_t *disk, mbr_t *tt, int preserve)
{
	char *query;

	if (preserve) {
		MBR_pcopy(disk, tt);
		query = "Do you wish to write new MBR?";
	} else {
		MBR_init(disk, tt);
		query = "Do you wish to write new MBR and partition table?";
	}

	if (ask_yn(query))
		Xwrite(NULL, disk, tt, NULL, 0); 

	return (0);
}

int modified;

int
USER_modify(disk_t *disk, mbr_t *tt, off_t offset, off_t reloff)
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
	close(fd);

	/* Parse the sucker */
	MBR_parse(disk, mbr_buf, offset, reloff, &mbr);

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
			strlcpy(cmd.cmd, cmd_table[i].cmd, sizeof cmd.cmd);

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
			if (Xwrite(NULL, disk, &mbr, NULL, offset) == CMD_CONT) 
				goto again;
			close(fd);
		} else
			printf("Aborting changes to current MBR.\n");
	}

	/* One level less */
	editlevel -= 1;

	return (0);
}

int
USER_print_disk(disk_t *disk)
{
	int fd, offset, firstoff, i;
	char mbr_buf[DEV_BSIZE];
	mbr_t mbr;

	fd = DISK_open(disk->name, O_RDONLY);
	offset = firstoff = 0;

	DISK_printmetrics(disk, NULL);

	do {
		MBR_read(fd, (off_t)offset, mbr_buf);
		MBR_parse(disk, mbr_buf, offset, firstoff, &mbr);

		printf("Offset: %d\t", (int)offset);
		MBR_print(&mbr, NULL);

		/* Print out extended partitions too */
		for (offset = i = 0; i < 4; i++)
			if (mbr.part[i].id == DOSPTYP_EXTEND ||
			    mbr.part[i].id == DOSPTYP_EXTENDL) {
				offset = mbr.part[i].bs;
				if (firstoff == 0)
					firstoff = offset;
			}
	} while (offset);

	return (close(fd));
}

