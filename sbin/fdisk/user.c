/*	$OpenBSD: user.c,v 1.36 2015/02/09 04:27:15 krw Exp $	*/

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
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "disk.h"
#include "part.h"
#include "mbr.h"
#include "misc.h"
#include "cmd.h"
#include "user.h"

/* Our command table */
struct cmd cmd_table[] = {
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
USER_init(struct disk *disk, struct mbr *tt, int preserve)
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
USER_edit(struct disk *disk, struct mbr *tt, off_t offset, off_t reloff)
{
	static int editlevel;
	struct dos_mbr dos_mbr;
	struct mbr mbr;
	char *cmd, *args;
	int i, st, fd, error;

	/* One level deeper */
	editlevel += 1;

	/* Read MBR & partition */
	fd = DISK_open(disk->name, O_RDONLY);
	error = MBR_read(fd, offset, &dos_mbr);
	close(fd);
	if (error == -1)
		goto done;

	/* Parse the sucker */
	MBR_parse(disk, &dos_mbr, offset, reloff, &mbr);

	printf("Enter 'help' for information\n");

	/* Edit cycle */
	do {
again:
		printf("fdisk:%c%d> ", (modified)?'*':' ', editlevel);
		fflush(stdout);
		ask_cmd(&cmd, &args);

		if (cmd[0] == '\0')
			goto again;
		for (i = 0; cmd_table[i].cmd != NULL; i++)
			if (strstr(cmd_table[i].cmd, cmd) == cmd_table[i].cmd)
				break;

		/* Quick hack to put in '?' == 'help' */
		if (!strcmp(cmd, "?"))
			i = 0;

		/* Check for valid command */
		if (cmd_table[i].cmd == NULL) {
			printf("Invalid command '%s'.  Try 'help'.\n", cmd);
			continue;
		}

		/* Call function */
		st = cmd_table[i].fcn(args, disk, &mbr, tt, offset);

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

done:
	/* One level less */
	editlevel -= 1;

	return (0);
}

int
USER_print_disk(struct disk *disk)
{
	off_t offset, firstoff;
	int fd, i, error;
	struct dos_mbr dos_mbr;
	struct mbr mbr;

	fd = DISK_open(disk->name, O_RDONLY);
	offset = firstoff = 0;

	DISK_printgeometry(disk, NULL);

	do {
		error = MBR_read(fd, offset, &dos_mbr);
		if (error == -1)
			break;
		MBR_parse(disk, &dos_mbr, offset, firstoff, &mbr);

		printf("Offset: %lld\t", offset);
		MBR_print(&mbr, NULL);

		/* Print out extended partitions too */
		for (offset = i = 0; i < 4; i++)
			if (mbr.part[i].id == DOSPTYP_EXTEND ||
			    mbr.part[i].id == DOSPTYP_EXTENDL) {
				offset = (off_t)mbr.part[i].bs;
				if (firstoff == 0)
					firstoff = offset;
			}
	} while (offset);

	return (close(fd));
}
