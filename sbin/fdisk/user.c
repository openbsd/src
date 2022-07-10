/*	$OpenBSD: user.c,v 1.82 2022/07/10 20:34:31 krw Exp $	*/

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
#include <stdio.h>
#include <string.h>

#include "part.h"
#include "mbr.h"
#include "misc.h"
#include "cmd.h"
#include "user.h"
#include "gpt.h"
#include "disk.h"

struct cmd {
	char	*cmd_name;
	int	 cmd_gpt;
	int	(*cmd_fcn)(const char *, struct mbr *);
	char	*cmd_help;
};

const struct cmd		cmd_table[] = {
	{"help",   1, Xhelp,   "Command help list"},
	{"manual", 1, Xmanual, "Show entire OpenBSD man page for fdisk"},
	{"reinit", 1, Xreinit, "Re-initialize loaded MBR (to defaults)"},
	{"setpid", 1, Xsetpid, "Set the identifier of a given table entry"},
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
};

int			modified;

int			ask_cmd(const int, char **);

void
USER_edit(const uint64_t lba_self, const uint64_t lba_firstembr)
{
	struct mbr		 mbr;
	char			*args;
	int			 i, st;
	static int		 editlevel;

	if (MBR_read(lba_self, lba_firstembr, &mbr))
		return;

	editlevel += 1;

	if (editlevel == 1)
		GPT_read(ANYGPT);

	printf("Enter 'help' for information\n");

	for (;;) {
		if (gh.gh_sig == GPTSIGNATURE && editlevel > 1)
			break;	/* 'reinit gpt'. Unwind recursion! */

		i = ask_cmd(editlevel, &args);
		if (i == -1)
			continue;

		st = cmd_table[i].cmd_fcn(args ? args : "", &mbr);

		if (st == CMD_EXIT) {
			if (modified)
				printf("Aborting changes to current MBR\n");
			break;
		}
		if (st == CMD_QUIT) {
			if (modified && Xwrite(NULL, &mbr) == CMD_CONT)
				continue;
			break;
		}
		if (st == CMD_CLEAN)
			modified = 0;
		if (st == CMD_DIRTY)
			modified = 1;
	}

	editlevel -= 1;
}

void
USER_print_disk(const int verbosity)
{
	struct mbr		mbr;
	uint64_t		lba_self, lba_firstembr;
	int			i;

	lba_self = lba_firstembr = 0;

	do {
		if (MBR_read(lba_self, lba_firstembr, &mbr))
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
				printf("Primary GPT:\n");
				GPT_read(PRIMARYGPT);
				if (gh.gh_sig == GPTSIGNATURE)
					GPT_print("s", verbosity);
				else
					printf("\tNot Found\n");
				printf("\nSecondary GPT:\n");
				GPT_read(SECONDARYGPT);
				if (gh.gh_sig == GPTSIGNATURE)
					GPT_print("s", verbosity);
				else
					printf("\tNot Found\n");
			}
			if (verbosity == VERBOSE)
				printf("\nMBR:\n");
		}

		MBR_print(&mbr, "s");

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
USER_help(void)
{
	char			 help[HELPBUFSZ];
	char			*mbrstr;
	int			 i;

	for (i = 0; i < nitems(cmd_table); i++) {
		strlcpy(help, cmd_table[i].cmd_help, sizeof(help));
		if (gh.gh_sig == GPTSIGNATURE) {
			if (cmd_table[i].cmd_gpt == 0)
				continue;
			mbrstr = strstr(help, "MBR");
			if (mbrstr)
				memcpy(mbrstr, "GPT", 3);
		}
		printf("\t%s\t\t%s\n", cmd_table[i].cmd_name, help);
	}
}

int
ask_cmd(const int editlevel, char **arg)
{
	static char		 lbuf[LINEBUFSZ];
	char			*cmd;
	unsigned int		 i;

	printf("%s%s: %d> ", disk.dk_name, modified ? "*" : "", editlevel);
	fflush(stdout);
	string_from_line(lbuf, sizeof(lbuf), TRIMMED);

	*arg = lbuf;
	cmd = strsep(arg, WHITESPACE);

	if (*arg != NULL)
		*arg += strspn(*arg, WHITESPACE);

	if (strlen(cmd) == 0)
		return -1;
	if (strcmp(cmd, "?") == 0)
		cmd = "help";

	for (i = 0; i < nitems(cmd_table); i++) {
		if (gh.gh_sig == GPTSIGNATURE && cmd_table[i].cmd_gpt == 0)
			continue;
		if (strstr(cmd_table[i].cmd_name, cmd) == cmd_table[i].cmd_name)
			return i;
	}

	printf("Invalid command '%s", cmd);
	if (*arg && strlen(*arg) > 0)
		printf(" %s", *arg);
	printf("'. Try 'help'.\n");

	return -1;
}
