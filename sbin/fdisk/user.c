/*	$OpenBSD: user.c,v 1.65 2021/07/18 15:28:37 krw Exp $	*/

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
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "part.h"
#include "mbr.h"
#include "misc.h"
#include "cmd.h"
#include "user.h"
#include "gpt.h"
#include "disk.h"

/* Our command table */
const struct cmd		cmd_table[] = {
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


int			modified;

void			ask_cmd(char **, char **);

void
USER_edit(const uint64_t lba_self, const uint64_t lba_firstembr)
{
	struct mbr		 mbr;
	char			*cmd, *args;
	int			 i, st, efi, error;
	static int		 editlevel;

	/* One level deeper */
	editlevel += 1;

	error = MBR_read(lba_self, lba_firstembr, &mbr);
	if (error == -1)
		goto done;

	if (editlevel == 1)
		GPT_read(ANYGPT);

	printf("Enter 'help' for information\n");

	/* Edit cycle */
again:
	do {
		printf("%s%s: %d> ", disk.dk_name, modified ? "*" : "", editlevel);
		fflush(stdout);
		ask_cmd(&cmd, &args);

		if (cmd[0] == '\0')
			continue;
		for (i = 0; cmd_table[i].cmd_name != NULL; i++)
			if (strstr(cmd_table[i].cmd_name, cmd) == cmd_table[i].cmd_name)
				break;

		/* Quick hack to put in '?' == 'help' */
		if (!strcmp(cmd, "?"))
			i = 0;

		/* Check for valid command */
		if ((cmd_table[i].cmd_name == NULL) || (letoh64(gh.gh_sig) ==
		    GPTSIGNATURE && cmd_table[i].cmd_gpt == 0)) {
			printf("Invalid command '%s'.  Try 'help'.\n", cmd);
			continue;
		}

		/* Call function */
		st = cmd_table[i].cmd_fcn(args, &mbr);

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
USER_print_disk(const int verbosity)
{
	struct mbr		mbr;
	uint64_t		lba_self, lba_firstembr;
	int			i, error;

	lba_self = lba_firstembr = 0;

	do {
		error = MBR_read(lba_self, lba_firstembr, &mbr);
		if (error == -1)
			break;
		if (lba_self == 0) {
			if (GPT_read(ANYGPT)) {
				if (verbosity == VERBOSE) {
					printf("Primary GPT:\nNot Found\n");
					printf("\nSecondary GPT:\nNot Found\n");
				}
			} else if (verbosity == TERSE) {
				GPT_print("s", verbosity);
				return;
			} else {
				/*. Read & print both primary and secondary GPT. */
				printf("Primary GPT:\n");
				GPT_read(PRIMARYGPT);
				if (letoh64(gh.gh_sig) == GPTSIGNATURE)
					GPT_print("s", verbosity);
				else
					printf("\tNot Found\n");
				printf("\nSecondary GPT:\n");
				GPT_read(SECONDARYGPT);
				if (letoh64(gh.gh_sig) == GPTSIGNATURE)
					GPT_print("s", verbosity);
				else
					printf("\tNot Found\n");
			}
			if (verbosity == VERBOSE)
				printf("\nMBR:\n");
		}

		MBR_print(&mbr, NULL);

		/* Print out extended partitions too */
		for (lba_self = i = 0; i < 4; i++)
			if (mbr.mbr_prt[i].prt_id == DOSPTYP_EXTEND ||
			    mbr.mbr_prt[i].prt_id == DOSPTYP_EXTENDL) {
				lba_self = mbr.mbr_prt[i].prt_bs;
				if (lba_firstembr == 0)
					lba_firstembr = lba_self;
			}
	} while (lba_self);
}

void
ask_cmd(char **cmd, char **arg)
{
	static char		lbuf[100];
	size_t			cmdstart, cmdend, argstart;

	/* Get NUL terminated string from stdin. */
	if (string_from_line(lbuf, sizeof(lbuf)))
		errx(1, "eof");

	cmdstart = strspn(lbuf, " \t");
	cmdend = cmdstart + strcspn(&lbuf[cmdstart], " \t");
	argstart = cmdend + strspn(&lbuf[cmdend], " \t");

	/* *cmd and *arg may be set to point at final NUL! */
	*cmd = &lbuf[cmdstart];
	lbuf[cmdend] = '\0';
	*arg = &lbuf[argstart];
}
