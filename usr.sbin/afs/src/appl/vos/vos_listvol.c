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

RCSID("$arla: vos_listvol.c,v 1.7 2001/09/17 21:40:26 mattiasa Exp $");

/*
 * list volume on a afs-server
 */

static char *server;
static char *partition;
static int  listvol_machine;
static char *cell;
static int noauth;
static int localauth;
static int helpflag;
static int fast;

static struct agetargs args[] = {
    {"server",	0, aarg_string,  &server,  
     "server", NULL, aarg_mandatory},
    {"partition", 0, aarg_string, &partition,
     "partition", NULL, aarg_optional_swless},
    {"machine", 'm', aarg_flag, &listvol_machine,
     "machineparseableform", NULL},
    {"cell",	0, aarg_string,  &cell, 
     "cell", NULL},
    {"noauth",	0, aarg_flag,    &noauth, 
     "do not authenticate", NULL},
    {"localauth",	0, aarg_flag,    &localauth, 
     "use local authentication", NULL},
    {"fast",		0, aarg_flag,	&fast,
     "only list IDs", NULL},
    {"help",	0, aarg_flag,    &helpflag,
     NULL, NULL},
    {NULL,      0, aarg_end, NULL}
};

static void
usage(void)
{
    aarg_printusage (args, "vos listvol", "", AARG_AFSSTYLE);
}

int
vos_listvol(int argc, char **argv)
{
    struct rx_connection *connvolser;
    int optind = 0;
    int flags = 0;
    int error;
    int part;

    server = partition = cell = NULL;
    listvol_machine = noauth = localauth = helpflag = fast = 0;

    if (agetarg (args, argc, argv, &optind, AARG_AFSSTYLE)) {
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


    connvolser = arlalib_getconnbyname(cell, server, afsvolport,
				       VOLSERVICE_ID,
		 arlalib_getauthflag (noauth, 0, 0, 0));

    if (connvolser == NULL) {
	printf("vos listvolume: failed to contact volser on host %s",
	       server);
	return -1 ;
    }

    error = printlistvol(connvolser, server, part, flags);
    if (error) {
	printf("vos listvolume: partinfo failed with: %s (%d)\n", 
	       koerr_gettext(error), error);
	return -1;
    }

    arlalib_destroyconn(connvolser);

    return 0;
}
