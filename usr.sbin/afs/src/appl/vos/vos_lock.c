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

RCSID("$arla: vos_lock.c,v 1.5 2001/11/16 16:52:02 mattiasa Exp $");

/*
 * lock a volume
 */

static char *vol;
static char *cell;
static int noauth;
static int localauth;
static int helpflag;
static int verbose;

static struct agetargs args[] = {
    {"id",      0, aarg_string, &vol,
     "id or name of volume",   NULL, aarg_mandatory},
    {"cell",    0, aarg_string,  &cell, 
     "cell", NULL},
    {"noauth",  0, aarg_flag,    &noauth, 
     "do not authenticate", NULL},
    {"localauth",       0, aarg_flag,    &localauth, 
     "use local authentication", NULL},
    {"verbose", 0, aarg_flag,    &verbose, 
     "verbose output", NULL},
    {"help",    0, aarg_flag,    &helpflag,
     NULL, NULL},
    {NULL,      0, aarg_end, NULL}
};

static int
vos_lock_volume(char *volname)
{
    struct rx_connection *connvldb;
    const char *host = NULL;
    arlalib_authflags_t auth;
    int error;
    int32_t dbhost;
    vldbentry vol;

    find_db_cell_and_host((const char **)&cell, &host);
    if(cell == NULL) {
	fprintf(stderr, "unable to find cell\n");
	return -1;
    }
    if(host == NULL) {
	fprintf(stderr, "unable to find a vldb host in cell %s", cell);
	return -1;
    }

    auth = arlalib_getauthflag(noauth, localauth, 0, 0);

    error = arlalib_getsyncsite(cell, NULL, afsvldbport, 
				&dbhost, auth);
    if (error) {
	fprintf (stderr, "vos_createvolume: arla_getsyncsite: %s\n", 
	       koerr_gettext(error));
	return -1;
    }
    
    connvldb = arlalib_getconnbyaddr(cell, dbhost, NULL,
				     afsvldbport,
				     VLDB_SERVICE_ID,
				     auth);

    if(connvldb == NULL) {
	fprintf(stderr, "can't connect to vldb server %s in cell %s\n",
		host, cell);
	return -1;
    }

    error = VL_GetEntryByName(connvldb, volname, &vol);
    if (error) {
        fprintf(stderr, "vos_lock: error %s (%d)\n",
		koerr_gettext(error), error);
        return 1;
    } 

    /* XXX Are you really supposed to use RWVOL here? */
    error = VL_SetLock(connvldb, vol.volumeId[RWVOL], RWVOL, VLOP_DELETE);

    if(!error) {
	printf("Locked VLDB entry for volume %s\n", vol.name);
    } else {
        fprintf(stderr, "vos_lock: error %s (%d)\n",
		koerr_gettext(error), error);
        return 1;
    }

    arlalib_destroyconn(connvldb);
    return 0;
}

static void
usage(void)
{
    aarg_printusage(args, "vos dump", "", AARG_AFSSTYLE);
}

int
vos_lock(int argc, char **argv)
{
    int optind = 0;

    cell = vol = NULL;
    noauth = localauth = helpflag = verbose = 0;

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

    if (argc) {
	printf("unknown option %s\n", *argv);
	return 0;
    }

    /* don't allow any bogus volname */
    if (vol == NULL || vol[0] == '\0') {
	usage ();
	return 0;
    }

    vos_lock_volume(vol);

    return 0;
}
