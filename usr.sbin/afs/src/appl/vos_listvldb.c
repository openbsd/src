/*	$OpenBSD: vos_listvldb.c,v 1.1 1999/04/30 01:59:05 art Exp $	*/
/*
 * Copyright (c) 1999 Kungliga Tekniska Högskolan
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

RCSID("$KTH: vos_listvldb.c,v 1.6 1999/03/14 19:05:34 assar Exp $");

/*
 * listvldb iteration over all entries in the DB
 */

int
vos_listvldb_iter (const char *host, const char *cell,
		   arlalib_authflags_t auth,
		   int (*proc)(void *data, struct vldbentry *), void *data)
{
    struct rx_connection *connvldb = NULL;
    struct vldbentry entry;
    int error;
    int32_t num, count;

    find_db_cell_and_host (&cell, &host);

    if (cell == NULL) {
	fprintf (stderr, "Unable to find cell of host '%s'\n", host);
	return -1;
    }

    if (host == NULL) {
	fprintf (stderr, "Unable to find DB server in cell '%s'\n", cell);
	return -1;
    }
	
    connvldb = arlalib_getconnbyname(cell, host,
				     afsvldbport,
				     VLDB_SERVICE_ID,
				     auth);
    num = 0;
    do { 
	error = VL_ListEntry (connvldb, 
			      num, 
			      &count,
			      &num, 
			      &entry);
	if (error)
	    break;

	entry.name[VLDB_MAXNAMELEN-1] = '\0';

	error = proc (data, &entry);
	if (error)
	    break;

    } while (num != -1);

    if (error) {
	warnx ("listvldb: VL_ListEntry: %s", koerr_gettext(error));
	return -1;
    }

    arlalib_destroyconn(connvldb);
    return 0;
}

/*
 * Print vldbentry on listvldb style
 */

static int
listvldb_print (void *data, struct vldbentry *e)
{
    int i;
    char *hostname;

    assert (e);

    printf ("%s\n", e->name);
    printf ("    Number of sites -> %d\n", e->nServers);
    for (i = 0 ; i < e->nServers ; i++) {
	if (e->serverNumber[i] == 0)
	    continue;
	if (arlalib_getservername(htonl(e->serverNumber[i]), &hostname))
	    continue;
	printf ("       server %s partition /vicep%c Site %s\n",
		hostname,
		'a' + e->serverPartition[i],
		getvolumetype (e->serverFlags[i]));
	free (hostname);
    }

    printf ("\n");
    
    return 0;
}


/*
 * list vldb
 */

char *server;
char *cell;
int noauth;
int localauth;
int helpflag;

static struct getargs args[] = {
    {"server",	0, arg_string,  &server,  
     "server", NULL, arg_mandatory},
    {"cell",	0, arg_string,  &cell, 
     "cell", NULL},
    {"noauth",	0, arg_flag,    &noauth, 
     "do not authenticate", NULL},
    {"localauth",	0, arg_flag,    &localauth, 
     "use local authentication", NULL},
    {"help",	0, arg_flag,    &helpflag,
     NULL, NULL},
    {NULL,      0, arg_end, NULL}
};

static void
usage(void)
{
    arg_printusage (args, "vos listvldb", "", ARG_AFSSTYLE);
}

int
vos_listvldb(int argc, char **argv)
{
    int optind = 0;

    server = cell = NULL;
    noauth = localauth = helpflag = 0;

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


    vos_listvldb_iter (server, cell, 
		       arlalib_getauthflag (noauth, localauth, 0, 0),
		       listvldb_print, NULL);
    
    return 0;
}

