/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
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
#include "bos_local.h"
#include <bos.h>
#include <bos.cs.h>


RCSID("$Id: bos_status.c,v 1.1 2000/09/11 14:40:35 art Exp $");

/*
 *
 */

static void
print_instance_status (char *instance_name,
		       int getstatus_int,
		       char *getstatus_str)
{
    switch (getstatus_int) {
    case BSTAT_SHUTDOWN:
	printf("Instance %s, disabled, currently shutdown.\n",
	       instance_name);
	break;
    case BSTAT_NORMAL: 
	printf("Instance %s, currently running normally.\n",
	       instance_name);
	break;
    case BSTAT_SHUTTINGDOWN: 
	printf("Instance %s, temporarily disabled, "
	       "currently shutting down.\n",
	       instance_name);
	break;
    case BSTAT_STARTINGUP: 
	printf("Instance %s, currently starting up.\n",
	       instance_name);
	break;
    default:
	printf (" unknown Status = %d (%s)\n", 
		getstatus_int, getstatus_str);
	break;
    }
}

/*
 *
 */

static int
printstatus(const char *cell,
	    const char *host,
	    int noauth,
	    int localauth,
	    int verbose)
{
    struct rx_connection *conn;
    unsigned int i;
    int error;
    char *instance_name;
    int getstatus_int;
    char getstatus_str[BOZO_BSSIZE];
    
    conn = arlalib_getconnbyname(cell,
				 host,
				 afsbosport,
				 BOS_SERVICE_ID,
				 arlalib_getauthflag(noauth,localauth,0,0));
    
    if (conn == NULL)
	return -1;
    
    i = 0;
    do {
	error = BOZO_EnumerateInstance(conn, i, &instance_name);
	if (error == 0) {
	    error = BOZO_GetStatus(conn, instance_name,
				   &getstatus_int, getstatus_str);
	    if (error == -1) {
		warnx ("failed to contact host's bosserver (%s) "
		       "(communications failure (-1)).", 
		       host);
	    } else if (error != 0) {
		warnx ("GetStatus(%s) failed with: %s (%d)",
		       instance_name,
		       koerr_gettext(error),
		       error);
	    } else {
		print_instance_status (instance_name,
				       getstatus_int,
				       getstatus_str);
	    }
	    error = 0;
	}
	i++;
    } while (error == 0);
    if (error != BZDOM)
	warnx ("%s: %s", host, koerr_gettext (error));

    arlalib_destroyconn(conn);
    return 0;
}

/*
 *
 */

static int helpflag;
static const char *server;
static const char *cell;
static int noauth;
static int localauth;
static int verbose;

static struct getargs args[] = {
  {"server",	0, arg_string,	&server,	"server", NULL, arg_mandatory},
  {"cell",	0, arg_string,	&cell,		"cell",	  NULL},
  {"noauth",	0, arg_flag,	&noauth,	"do not authenticate", NULL},
  {"local",	0, arg_flag,	&localauth,	"localauth"},
  {"verbose",	0, arg_flag,	&verbose,	"be verbose"},
  {"help",	0, arg_flag,	&helpflag,	NULL, NULL},
  {NULL,	0, arg_end,	NULL,		NULL, NULL}
};

/*
 *
 */

static int
usage (int exit_code)
{
    arg_printusage (args, "bos status", "", ARG_AFSSTYLE);
    return exit_code;
}

/*
 *
 */

int
bos_status(int argc, char **argv)
{
    int optind = 0;
    
    if (getarg (args, argc, argv, &optind, ARG_AFSSTYLE))
	return usage (0);
    
    if (helpflag)
	return usage (0);
    
    argc -= optind;
    argv += optind;
    
    if (server == NULL) {
	printf ("bos status: missing -server\n");
	return 0;
    }
    
    if (cell == NULL)
	cell = cell_getcellbyhost (server);
    
    printstatus (cell, server, noauth, localauth, verbose);
    return 0;
}
