/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "appl_locl.h"
#include <sl.h>
#include "vos_local.h"

RCSID("$arla: vos_listpart.c,v 1.6 2001/09/25 22:05:00 mattiasa Exp $");

static int
printlistparts(const char *cell, const char *server, 
	       arlalib_authflags_t auth, int verbose)
{
    part_entries parts;
    int error;
    int i;
    
    if (cell == NULL)
	cell = cell_getcellbyhost (server);

    error = getlistparts(cell, server, &parts, auth);

    if (error != 0)
	return error;

    printf("The partitions on the server are:\n ");
    for (i = 0; i < parts.len; ++i) {
	char part_name[17];

	partition_num2name (parts.val[i], part_name, sizeof(part_name));
	printf("   %s%c", part_name, i % 6 == 5 ? '\n' : ' ');
    }
    printf("\nTotal: %d\n", parts.len);
    free (parts.val);
    return 0;
}

static int helpflag;
static char *server;
static char *cell;
static int noauth;
static int localauth;
static int verbose;

static struct agetargs listp_args[] = {
    {"server",	0, aarg_string,  &server, "server", NULL, aarg_mandatory},
    {"cell",	0, aarg_string,	&cell,	 "cell", NULL},
    {"noauth",	0, aarg_flag,    &noauth, "no authentication", NULL},
    {"localauth",0, aarg_flag,   &localauth, "local authentication", NULL},
    {"verbose", 0, aarg_flag,	&verbose, "be verbose", NULL},
    {"help",	0, aarg_flag,    &helpflag, NULL, NULL},
    {NULL}
};

static void
usage(void)
{
    aarg_printusage(listp_args, "vos listpart", "", AARG_AFSSTYLE);
}

int
vos_listpart(int argc, char **argv)
{
    int optind = 0;

    helpflag = noauth = localauth = verbose = 0;
    server = cell = NULL;

    if (agetarg (listp_args,argc, argv, &optind, AARG_AFSSTYLE)) {
	usage ();
	return 0;
    }

    argc -= optind;
    argv += optind;

    if (argc) {
	printf("unknown option %s\n", *argv);
	return 0;
    }

    if (server == NULL || server[0] == '\0') {
	usage ();
	return 0;
    }

    printlistparts(cell, server, 
		   arlalib_getauthflag (noauth, localauth, 0, 0),
		   verbose);
    return 0;
}
