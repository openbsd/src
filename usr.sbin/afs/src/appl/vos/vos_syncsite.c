/*
 * Copyright (c) 1995 - 2001 Kungliga Tekniska Högskolan
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

RCSID("$arla: vos_syncsite.c,v 1.14 2001/07/12 02:36:59 ahltorp Exp $");

static int helpflag;
static char *cell;
static int resolvep = 1;

static struct agetargs args[] = {
    {"cell",	'c',  aarg_string,	    &cell, "cell", NULL},
    {"help",	'h',  aarg_flag,             &helpflag, NULL, NULL},
    {"resolve", 'n',  aarg_negative_flag,    &resolvep, NULL, NULL}
};

static void
usage(void)
{
    aarg_printusage(args, "vos syncsite", "", AARG_AFSSTYLE);
}

int 
vos_syncsite (int argc, char **argv)
{
    struct in_addr saddr;
    int error;
    int optind = 0;

    helpflag = 0;
    cell = NULL;

    if (agetarg (args, argc, argv, &optind, AARG_AFSSTYLE)) {
	usage ();
	return 0;
    }
	
    if (helpflag) {
	usage ();
	return 0;
    }

    if (cell == NULL)
	cell = (char *)cell_getthiscell();

    error = arlalib_getsyncsite(cell, NULL, afsvldbport, &saddr.s_addr, 0);
    if (error) {
	fprintf(stderr, "syncsite: %s (%d)\n", koerr_gettext(error), error);
	return 0;
    }
    
    if (!resolvep)
	printf("%s's vldb syncsite is %s.\n", cell, inet_ntoa(saddr));
    else {
	char server_name[256];

	arlalib_host_to_name (saddr.s_addr, server_name, sizeof(server_name));
	
	printf("%s's vldb syncsite is %s (%s).\n", cell, server_name,
	       inet_ntoa(saddr));

    }
    return 0;
}
