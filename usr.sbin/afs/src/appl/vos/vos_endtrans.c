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
#include "vos_local.h"

RCSID("$arla: vos_endtrans.c,v 1.5 2000/10/03 00:08:46 lha Exp $");

/*
 * end a transaction on `host' with transaction id `trans'
 */

int
vos_endtransaction (const char *cell, const char *host, int32_t trans, 
		    arlalib_authflags_t auth, int verbose)
{
    struct rx_connection *volser = NULL;
    int error;
    int32_t rcode;

    if (host == NULL)
	return EINVAL;

    if (cell == NULL)
	cell = (char *)cell_getthiscell();

    volser = arlalib_getconnbyname (cell, host,
				    afsvolport,
				    VOLSERVICE_ID,
				    auth);
    if (volser == NULL) {
	fprintf (stderr, 
		 "vos_endtransaction: arlalib_getconnbyname: volser: %s\n",
		 host);
	return -1;
    }


    error = VOLSER_AFSVolEndTrans(volser, trans, &rcode);
    if (error) {
	fprintf (stderr, "vos_createvolume: VOLSER_AFSVolEndTrans: %s\n", 
	       koerr_gettext (error));
	return -1;
    }
    if (verbose)
	printf ("vos_endtransaction: VOLSER_AFSVolEndTrans: rcode = %d",
		rcode);

    arlalib_destroyconn(volser);
    return error;
}

/*
 * list vldb
 */

static char *server;
static char *cell;
static int transid;
static int noauth;
static int localauth;
static int helpflag;
static int verbose;

static struct agetargs args[] = {
    {"server",	0, aarg_string,  &server,  
     "server", NULL, aarg_mandatory},
    {"trans",	0, aarg_integer,  &transid,  
     "trans", NULL, aarg_mandatory},
    {"cell",	0, aarg_string,  &cell, 
     "cell", NULL},
    {"noauth",	0, aarg_flag,    &noauth, 
     "do not authenticate", NULL},
    {"localauth",	0, aarg_flag,    &localauth, 
     "use local authentication", NULL},
    {"verbose",	0, aarg_flag,    &verbose, 
     "verbose output", NULL},
    {"help",	0, aarg_flag,    &helpflag,
     NULL, NULL},
    {NULL,      0, aarg_end, NULL}
};

/*
 * print usage
 */

static void
usage(void)
{
    aarg_printusage (args, "vos endtransaction", "", AARG_AFSSTYLE);
}

/*
 * end a transaction, to be called from libsl
 */

int
vos_endtrans(int argc, char **argv)
{
    int optind = 0;
    int error;

    server = cell = NULL;
    noauth = localauth = helpflag = 0;

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

    if (argc > 0) {
	fprintf (stderr, "vos_endtrans: unparsed arguments\n");
	return 0;
    }

    error = vos_endtransaction (cell, server, transid,
				arlalib_getauthflag (noauth, localauth, 0, 0),
				verbose);
    if (error) {
	fprintf (stderr, "vos_endtrans failed (%d)\n", error);
	return 0;
    }

    return 0;
}


