/*	$OpenBSD: vos_listvol.c,v 1.1 1999/04/30 01:59:05 art Exp $	*/
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

RCSID("$KTH: vos_listvol.c,v 1.2 1999/03/04 09:17:32 lha Exp $");

/*
 * list volume on a afs-server
 */

char *server;
char *partition;
int  listvol_machine;
char *cell;
int noauth;
int localauth;
int helpflag;
int fast;

static struct getargs args[] = {
    {"server",	0, arg_string,  &server,  
     "server", NULL, arg_mandatory},
    {"partition", 0, arg_string, &partition,
     "partition", NULL},
    {"machine", 'm', arg_flag, &listvol_machine,
     "machineparseableform", NULL},
    {"cell",	0, arg_string,  &cell, 
     "cell", NULL},
    {"noauth",	0, arg_flag,    &noauth, 
     "do not authenticate", NULL},
    {"localauth",	0, arg_flag,    &localauth, 
     "use local authentication", NULL},
    {"fast",		0, arg_flag,	&fast,
     "only list IDs", NULL},
    {"help",	0, arg_flag,    &helpflag,
     NULL, NULL},
    {NULL,      0, arg_end, NULL}
};

static void
usage(void)
{
    arg_printusage (args, "vos listvol", "", ARG_AFSSTYLE);
}

int
vos_listvol(int argc, char **argv)
{
    int optind = 0;
    int flags = 0;
    int part;

    server = partition = cell = NULL;
    listvol_machine = noauth = localauth = helpflag = fast = 0;

    if (getarg (args, argc, argv, &optind, ARG_AFSSTYLE)) {
	usage();
	return 0;
    }

    if(helpflag) {
	usage();
	return 0;
    }
    
    argc -= optind;
    argv += optind;

    if (server == NULL) {
	usage();
	return 0;
    }

    if (partition == NULL) {
	part = -1;
    } else {
	part = partition_name2num(partition);
	if (part == -1) {
	    usage();
	    return 0;
	}
    }

    if (listvol_machine)
	flags |= LISTVOL_PART;
    if (localauth)
	flags |= LISTVOL_LOCALAUTH;
    if (fast)
	flags |= LISTVOL_FAST;

    printlistvol(cell, server, part, flags, 
		 arlalib_getauthflag (noauth, 0, 0, 0));
    return 0;
}
