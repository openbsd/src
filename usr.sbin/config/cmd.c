/*	$OpenBSD: cmd.c,v 1.13 2004/06/08 20:59:28 mcbride Exp $ */

/*
 * Copyright (c) 1999-2001 Mats O Jansson.  All rights reserved.
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

#ifndef LINT
static char rcsid[] = "$OpenBSD: cmd.c,v 1.13 2004/06/08 20:59:28 mcbride Exp $";
#endif

#include <sys/types.h>
#include <sys/device.h>
#include <sys/time.h>
#include <ctype.h>
#include <stdio.h>
#include <limits.h>
#include <nlist.h>
#include <string.h>
#include "misc.h"
#define	CMD_NOEXTERN
#include "cmd.h"
#include "ukc.h"
#include "exec.h"

extern int ukc_mod_kernel;
static void int_variable_adjust(const cmd_t *, int, const char *);

/* Our command table */
cmd_table_t cmd_table[] = {
	{"help",   Xhelp,	"",		"Command help list"},
	{"add",	   Xadd,	"dev",		"Add a device"},
	{"base",   Xbase,	"8|10|16",	"Base on large numbers"},
	{"change", Xchange,	"devno|dev",	"Change device"},
	{"disable",Xdisable,	"attr val|devno|dev",	"Disable device"},
	{"enable", Xenable,	"attr val|devno|dev",	"Enable device"},
	{"find",   Xfind,	"devno|dev",	"Find device"},
	{"list",   Xlist,	"",		"List configuration"},
	{"lines",  Xlines,	"count",	"# of lines per page"},
	{"show",   Xshow,	"[attr [val]]",	"Show attribute"},
	{"exit",   Xexit,	"",		"Exit, without saving changes"},
	{"quit",   Xquit,	"",		"Quit, saving current changes"},
	{"timezone", Xtimezone,	"[mins [dst]]",	"Show/change timezone"},
	{"cachepct", Xbufcachepct, "[number]",	"Show/change BUFCACHEPERCENT"},
	{"nkmempg", Xnkmempg,	"[number]",	"Show/change NKMEMPAGES"},
	{"shmseg", Xshmseg,	"[number]",	"Show/change SHMSEG"},
	{"shmmaxpgs", Xshmmaxpgs,"[number]",	"Show/change SHMMAXPGS"},
	{NULL,     NULL,	NULL,		NULL}
};

int
Xhelp(cmd_t *cmd)
{
	cmd_table_t *cmd_table = cmd->table;
	int i;

	/* Hmm, print out cmd_table here... */
	for (i = 0; cmd_table[i].cmd != NULL; i++)
		printf("\t%-12s%-20s%s\n", cmd_table[i].cmd,
		    cmd_table[i].opt, cmd_table[i].help);
	return (CMD_CONT);
}

int
Xadd(cmd_t *cmd)
{
	short unit, state;
	int a;

	if (strlen(cmd->args) == 0)
		printf("Dev expected\n");
	else if (device(cmd->args, &a, &unit, &state) == 0)
		add(cmd->args, a, unit, state);
	else
		printf("Unknown argument\n");
	return (CMD_CONT);
}

int
Xbase(cmd_t *cmd)
{
	int a;

	if (strlen(cmd->args) == 0)
		printf("8|10|16 expected\n");
	else if (number(&cmd->args[0], &a) == 0) {
		if (a == 8 || a == 10 || a == 16) {
			base = a;
		} else {
			printf("8|10|16 expected\n");
		}
	} else
		printf("Unknown argument\n");
	return (CMD_CONT);
}

int
Xchange(cmd_t *cmd)
{
	short unit, state;
	int a;

	if (strlen(cmd->args) == 0)
		printf("DevNo or Dev expected\n");
	else if (number(cmd->args, &a) == 0)
		change(a);
	else if (device(cmd->args, &a, &unit, &state) == 0)
		common_dev(cmd->args, a, unit, state, UC_CHANGE);
	else
		printf("Unknown argument\n");
	return (CMD_CONT);
}

int
Xdisable(cmd_t *cmd)
{
	short unit, state;
	int a;

	if (strlen(cmd->args) == 0)
		printf("Attr, DevNo or Dev expected\n");
	else if (attr(cmd->args, &a) == 0)
		common_attr(cmd->args, a, UC_DISABLE);
	else if (number(cmd->args, &a) == 0)
		disable(a);
	else if (device(cmd->args, &a, &unit, &state) == 0)
		common_dev(cmd->args, a, unit, state, UC_DISABLE);
	else
		printf("Unknown argument\n");
	return (CMD_CONT);
}

int
Xenable(cmd_t *cmd)
{
	short unit, state;
	int a;

	if (strlen(cmd->args) == 0)
		printf("Attr, DevNo or Dev expected\n");
	else if (attr(cmd->args, &a) == 0)
		common_attr(cmd->args, a, UC_DISABLE);
	else if (number(cmd->args, &a) == 0)
		enable(a);
	else if (device(cmd->args, &a, &unit, &state) == 0)
		common_dev(cmd->args, a, unit, state, UC_ENABLE);
	else
		printf("Unknown argument\n");
	return (CMD_CONT);
}

int
Xfind(cmd_t *cmd)
{
	short unit, state;
	int a;

	if (strlen(cmd->args) == 0)
		printf("DevNo or Dev expected\n");
	else if (number(cmd->args, &a) == 0)
		pdev(a);
	else if (device(cmd->args, &a, &unit, &state) == 0)
		common_dev(cmd->args, a, unit, state, UC_FIND);
	else
		printf("Unknown argument\n");
	return (CMD_CONT);
}

int
Xlines(cmd_t *cmd)
{
	int a;

	if (strlen(cmd->args) == 0)
		printf("Argument expected\n");
	else if (number(cmd->args, &a) == 0)
		lines = a;
	else
		printf("Unknown argument\n");
	return (CMD_CONT);
}

int
Xlist(cmd_t *cmd)
{
	struct cfdata *cd;
	int	i = 0;

	cnt = 0;
	cd = get_cfdata(0);

	while (cd->cf_attach != 0) {
		if (more())
			break;
		pdev(i++);
		cd++;
	}

	if (nopdev == 0) {
		while (i <= (totdev+maxpseudo)) {
			if (more())
				break;
			pdev(i++);
		}
	}
	cnt = -1;
	return (CMD_CONT);
}

int
Xshow(cmd_t *cmd)
{
	if (strlen(cmd->args) == 0)
		show();
	else
		show_attr(&cmd->args[0]);
	return (CMD_CONT);
}

int
Xquit(cmd_t *cmd)
{
	/* Nothing to do here */
	return (CMD_SAVE);
}

int
Xexit(cmd_t *cmd)
{
	/* Nothing to do here */
	return (CMD_EXIT);
}

int
Xtimezone(cmd_t *cmd)
{
	struct timezone *tz;
	int	num;
	char	*c;

	ukc_mod_kernel = 1;
	tz = (struct timezone *)adjust((caddr_t)(nl[TZ_TZ].n_value));

	if (strlen(cmd->args) == 0) {
		printf("timezone = %d, dst = %d\n",
		    tz->tz_minuteswest, tz->tz_dsttime);
	} else {
		if (number(cmd->args, &num) == 0) {
			tz->tz_minuteswest = num;
			c = cmd->args;
			while ((*c != '\0') && !isspace(*c))
				c++;
			while ((*c != '\0') && isspace(*c))
				c++;
			if (strlen(c) != 0 && number(c, &num) == 0)
				tz->tz_dsttime = num;
			printf("timezone = %d, dst = %d\n",
			    tz->tz_minuteswest, tz->tz_dsttime);
		} else
			printf("Unknown argument\n");
	}
	return (CMD_CONT);
}

void
int_variable_adjust(const cmd_t *cmd, int idx, const char *name)
{
	int *v, num;

	if (nl[idx].n_type != 0) {
		ukc_mod_kernel = 1;

		v = (int *)adjust((caddr_t)(nl[idx].n_value));

		if (strlen(cmd->args) == 0) {
			printf("%s = %d\n", name, *v);
		} else {
			if (number(cmd->args, &num) == 0) {
				*v = num;
				printf("%s = %d\n", name, *v);
			} else
				printf("Unknown argument\n");
		}
	} else
		printf("This kernel does not support modification of %s.\n",
		    name);
}

int
Xbufcachepct(cmd_t *cmd)
{
	int_variable_adjust(cmd, I_BUFCACHEPCT, "bufcachepercent");
	return (CMD_CONT);
}

int
Xnkmempg(cmd_t *cmd)
{
	int_variable_adjust(cmd, I_NKMEMPG, "nkmempages");
	return (CMD_CONT);
}

int
Xshmseg(cmd_t *cmd)
{
	int_variable_adjust(cmd, I_SHMSEG, "shmseg");
	return (CMD_CONT);
}

int
Xshmmaxpgs(cmd_t *cmd)
{
	int_variable_adjust(cmd, I_SHMMAXPGS, "shmmaxpgs");
	return (CMD_CONT);
}
