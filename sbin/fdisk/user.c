/*	$OpenBSD: user.c,v 1.50 2016/01/09 18:10:57 krw Exp $	*/

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
#include <sys/fcntl.h>
#include <sys/disklabel.h>

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "part.h"
#include "mbr.h"
#include "misc.h"
#include "cmd.h"
#include "user.h"
#include "gpt.h"

/* Our command table */
struct cmd cmd_table[] = {
	{"help",   1, Xhelp,   "Command help list"},
	{"manual", 1, Xmanual, "Show entire OpenBSD man page for fdisk"},
	{"reinit", 1, Xreinit, "Re-initialize loaded MBR (to defaults)"},
	{"setpid", 1, Xsetpid, "Set the identifier of a given table entry"},
	{"disk",   0, Xdisk,   "Edit current drive stats"},
	{"edit",   1, Xedit,   "Edit given table entry"},
	{"flag",   1, Xflag,   "Flag given table entry as bootable"},
	{"update", 0, Xupdate, "Update machine code in loaded MBR"},
	{"select", 0, Xselect, "Select extended partition table entry MBR"},
	{"swap",   1, Xswap,   "Swap two partition entries"},
	{"print",  1, Xprint,  "Print loaded MBR partition table"},
	{"write",  1, Xwrite,  "Write loaded MBR to disk"},
	{"exit",   1, Xexit,   "Exit edit of current MBR, without saving changes"},
	{"quit",   1, Xquit,   "Quit edit of current MBR, saving current changes"},
	{"abort",  1, Xabort,  "Abort program without saving current changes"},
	{NULL,     0, NULL,    NULL}
};


int modified;

void
USER_edit(off_t offset, off_t reloff)
{
	static int editlevel;
	struct dos_mbr dos_mbr;
	struct mbr mbr;
	char *cmd, *args;
	int i, st, error;

	/* One level deeper */
	editlevel += 1;

	/* Read MBR & partition */
	error = MBR_read(offset, &dos_mbr);
	if (error == -1)
		goto done;

	/* Parse the sucker */
	MBR_parse(&dos_mbr, offset, reloff, &mbr);

	if (editlevel == 1) {
		memset(&gh, 0, sizeof(gh));
		memset(&gp, 0, sizeof(gp));
		if (MBR_protective_mbr(&mbr) == 0)
			GPT_get_gpt(0);
	}

	printf("Enter 'help' for information\n");

	/* Edit cycle */
again:
	do {
		printf("fdisk:%c%d> ", (modified)?'*':' ', editlevel);
		fflush(stdout);
		ask_cmd(&cmd, &args);

		if (cmd[0] == '\0')
			continue;
		for (i = 0; cmd_table[i].cmd != NULL; i++)
			if (strstr(cmd_table[i].cmd, cmd) == cmd_table[i].cmd)
				break;

		/* Quick hack to put in '?' == 'help' */
		if (!strcmp(cmd, "?"))
			i = 0;

		/* Check for valid command */
		if ((cmd_table[i].cmd == NULL) || (letoh64(gh.gh_sig) ==
		    GPTSIGNATURE && cmd_table[i].gpt == 0)) {
			printf("Invalid command '%s'.  Try 'help'.\n", cmd);
			continue;
		}

		/* Call function */
		st = cmd_table[i].fcn(args, &mbr);

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
			if (Xwrite(NULL, &mbr) == CMD_CONT)
				goto again;
		} else
			printf("Aborting changes to current MBR.\n");
	}

done:
	/* One level less */
	editlevel -= 1;
}

void
USER_print_disk(int verbosity)
{
	off_t offset, firstoff;
	int i, error;
	struct dos_mbr dos_mbr;
	struct mbr mbr;

	offset = firstoff = 0;

	do {
		error = MBR_read(offset, &dos_mbr);
		if (error == -1)
			break;
		MBR_parse(&dos_mbr, offset, firstoff, &mbr);
		if (offset == 0) {
		       if (verbosity || MBR_protective_mbr(&mbr) == 0) {
				if (verbosity) {
					printf("Primary GPT:\n");
					GPT_get_gpt(1); /* Get Primary */
				}
				if (letoh64(gh.gh_sig) == GPTSIGNATURE)
					GPT_print("s", verbosity);
				else
					printf("\tNot Found\n");
				if (verbosity) {
					printf("\n");
					printf("Secondary GPT:\n");
					GPT_get_gpt(2); /* Get Secondary */
					if (letoh64(gh.gh_sig) == GPTSIGNATURE)
						GPT_print("s", verbosity);
					else
						printf("\tNot Found\n");
					printf("\nMBR:\n");
				} else
					break;
		       }
		}

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
}
