/*	$OpenBSD: vos_syncsite.c,v 1.1 1999/04/30 01:59:06 art Exp $	*/
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
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

RCSID("$KTH: vos_syncsite.c,v 1.2 1999/03/06 16:41:02 lha Exp $");

static int helpflag;
static char *cell;
static int resolvep = 1;

static struct getargs args[] = {
    {"cell",	'c',  arg_string,	    &cell, "cell", NULL},
    {"help",	'h',  arg_flag,             &helpflag, NULL, NULL},
    {"resolve", 'n',  arg_negative_flag,    &resolvep, NULL, NULL}
};

static void
usage(void)
{
    arg_printusage(args, "vos syncsite", "", ARG_AFSSTYLE);
}

int 
vos_syncsite (int argc, char **argv)
{
    struct in_addr saddr;
    int error;
    int optind = 0;

    helpflag = 0;
    cell = NULL;

    if (getarg (args, argc, argv, &optind, ARG_AFSSTYLE)) {
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
	const char *name = ipgetname(&saddr);
	printf("%s's vldb syncsite is %s (%s).\n", cell, name, inet_ntoa(saddr));
    }
    return 0;
}
