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
#include "bos_local.h"
#include <bos.h>
#include <bos.cs.h>


RCSID("$arla: bos_listusers.c,v 1.7 2001/04/18 13:49:06 lha Exp $");

static int
printusers(const char *cell, const char *host,
	   int noauth, int localauth, int verbose)
{
    struct rx_connection *conn = NULL;
    int i, error, done;
    char user_name[BOZO_BSSIZE];
    
    conn = arlalib_getconnbyname(cell,
				 host,
				 afsbosport,
				 BOS_SERVICE_ID,
				 arlalib_getauthflag(noauth, localauth,0,0));
    if (conn == NULL)
	return -1;
    
    printf("SUsers are: ");
    done = i = 0;
    while (!done) {
	error = BOZO_ListSUsers(conn, i, user_name);
	switch (error) {
	case 1:
	    if (i == 0) {
		printf ("bos listusers: UserList could not be opened or no "
			"users on this server\n");
		return 0;
	    }
	    done = 1;
	    break;
	case 0:
	    printf("%s ", user_name);
	    break;
	default:
	    printf ("bos listusers: %s(%d)\n", 
		    koerr_gettext (error), error);
	    return 0;
	}

	i++;
    }
    printf("\n");

    arlalib_destroyconn(conn);
    return 0;
}


static int helpflag;
static const char *server;
static const char *cell;
static int noauth;
static int localauth;
static int verbose;

static struct agetargs args[] = {
    {"server",	0, aarg_string,	&server,	"server", NULL, aarg_mandatory},
    {"cell",	0, aarg_string,	&cell,		"cell",	  NULL},
    {"noauth",	0, aarg_flag,	&noauth,	"do not authenticate", NULL},
    {"local",	0, aarg_flag,	&localauth,	"localauth"},
    {"verbose",	0, aarg_flag,	&verbose,	"be verbose", NULL},
    {"help",	0, aarg_flag,	&helpflag,	NULL, NULL},
    {NULL,	0, aarg_end,	NULL,		NULL, NULL}
};

static void
usage (void)
{
    aarg_printusage (args, "bos listusers", "", AARG_AFSSTYLE);
}

int
bos_listusers(int argc, char **argv)
{
    int optind = 0;
    
    if (agetarg (args, argc, argv, &optind, AARG_AFSSTYLE)) {
	usage ();
	return 0;
    }
    
    if (helpflag) {
	usage ();
	return 0;
    }
    
    argc -= optind;
    argv += optind;
    
    if (server == NULL) {
	printf ("bos listusers: missing -server\n");
	return 0;
    }
    
    if (cell == NULL)
	cell = cell_getcellbyhost (server);
    
    printusers (cell, server, noauth, localauth, verbose);
    return 0;
}
