/*	$OpenBSD: cmd.c,v 1.3 2000/02/27 21:10:01 deraadt Exp $ */

/*
 * Copyright (c) 1999 Mats O Jansson.  All rights reserved.
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
 *	This product includes software developed by Mats O Jansson.
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

#ifndef LINT
static char rcsid[] = "$OpenBSD: cmd.c,v 1.3 2000/02/27 21:10:01 deraadt Exp $";
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

/* Our command table */
cmd_table_t cmd_table[] = {
	{"help",   Xhelp,	"\t\t",		"Command help list"},
	{"add",	   Xadd,	"dev\t\t",	"Add a device"},
	{"base",   Xbase,	"8|10|16\t\t",	"Base on large numbers"},
	{"change", Xchange,	"devno|dev\t",	"Change device"},
	{"disable",Xdisable,	"attr val|devno|dev",	"Disable device"},
	{"enable", Xenable,	"attr val|devno|dev",	"Enable device"},
	{"find",   Xfind,	"devno|dev\t",	"Find device"},
	{"list",   Xlist,	"\t\t",		"List configuration"},
	{"lines",  Xlines,	"count\t\t",	"# of lines per page"},
	{"show",   Xshow,	"[attr [val]]\t",	"Show attribute"},
	{"exit",   Xexit,	"\t\t",		"Exit, without saving changes"},
	{"quit",   Xquit,	"\t\t",		"Quit, saving current changes"},
	{"timezone", Xtimezone,	"[mins [dst]]\t",	"Show/change timezone"},
	{NULL,     NULL,	NULL,		NULL}
};

int
Xhelp(cmd)
	cmd_t *cmd;
{
	cmd_table_t *cmd_table = cmd->table;
	int i;

	/* Hmm, print out cmd_table here... */
	for (i = 0; cmd_table[i].cmd != NULL; i++)
		printf("\t%s\t%s\t%s\n", cmd_table[i].cmd,
		       cmd_table[i].opt, cmd_table[i].help);
	return (CMD_CONT);
}

int
Xadd(cmd)
	cmd_t *cmd;
{
	int a;
	short unit, state;

	if (strlen(cmd->args) == 0)
		printf("Dev expected\n");
	else if (device(cmd->args, &a, &unit, &state) == 0)
		add(cmd->args, a, unit, state);
	else
		printf("Unknown argument\n");
	return (CMD_CONT);
}

int
Xbase(cmd)
	cmd_t *cmd;
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
Xchange(cmd)
	cmd_t *cmd;
{
	int a;
	short unit, state;

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
Xdisable(cmd)
	cmd_t *cmd;
{
	int a;
	short unit, state;

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
Xenable(cmd)
	cmd_t *cmd;
{
	int a;
	short unit, state;

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
Xfind(cmd)
	cmd_t *cmd;
{
	int a;
	short unit, state;

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
Xlines(cmd)
	cmd_t *cmd;
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
Xlist(cmd)
	cmd_t *cmd;
{
	int	i = 0;
	struct cfdata *cd;

	cnt = 0;

	cd = get_cfdata(0);

	while(cd->cf_attach != 0) {
		if (more())
			break;
		pdev(i++);	  
		cd++;
	}

	cnt = -1;

	return (CMD_CONT);
}

int
Xshow(cmd)
	cmd_t *cmd;
{
	if (strlen(cmd->args) == 0)
		show();
	else
		show_attr(&cmd->args[0]);

	return (CMD_CONT);
}

int
Xquit(cmd)
	cmd_t *cmd;
{
	/* Nothing to do here */
	return (CMD_SAVE);
}

int
Xexit(cmd)
	cmd_t *cmd;
{
	/* Nothing to do here */
	return (CMD_EXIT);
}

int
Xtimezone(cmd)
	cmd_t *cmd;
{
	struct timezone *tz;
	int	num;
	char	*c;

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
