/*
 * Copyright (c) 2002, Stockholms Universitet
 * (Stockholm University, Stockholm Sweden)
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
 * 3. Neither the name of the university nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "at_locl.h"

RCSID("$arla: at_cm_localcell.c,v 1.5 2003/03/05 14:55:47 lha Exp $");

static int helpflag;
static char *portstr = NULL;
static char *cell = NULL;
static int auth = 1;
static int helpflag = 0;

static struct getargs args[] = {
    {"port",	0, arg_string,  &portstr, "what port to use", NULL},
    {"cell",	0, arg_string,  &cell, "what cell to use", NULL},
    {"auth",	0, arg_negative_flag, &auth, "no authentication", NULL},
    {"help",	0, arg_flag,    &helpflag, NULL, NULL}
};

static void
usage(void)
{
    char helpstring[100];

    snprintf(helpstring, sizeof(helpstring), "%s localcell", getprogname());
    arg_printusage(args, sizeof(args)/sizeof(args[0]), helpstring, "host ...");
}

int
cm_localcell_cmd (int argc, char **argv)
{
    struct rx_connection *conn;
    int optind = 0;
    int ret, i;
    char cellname[AFSNAMEMAX];

    if (getarg (args, sizeof(args)/sizeof(args[0]), argc, argv, &optind)) {
	usage ();
	return 0;
    }

    if (helpflag) {
	usage ();
	return 0;
    }

    argc -= optind;
    argv += optind;

    if (argc == 0) {
	printf("missing host\n");
	return 0;
    }

    for (i = 0 ; i < argc; i++) {
	conn = cbgetconn(cell, argv[i], portstr, CM_SERVICE_ID, auth);
	
	ret = RXAFSCB_GetLocalCell(conn, cellname);
	if (ret == 0)
	    printf("localcell: %s\n", cellname);
	else
	    printf("%s returned %s %d\n", argv[i], koerr_gettext(ret), ret);
	
	arlalib_destroyconn(conn);
	argc--; 
	argv++;
    }

    return 0;
}
