/*
 * Copyright (c) 2003, Stockholms Universitet
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

RCSID("$arla: at_fs_gettime.c,v 1.4 2003/03/08 02:05:02 lha Exp $");

static char *cellname;
static int auth = 1;
static int helpflag;
static int verbose;

static struct getargs args[] = {
    {"cell",    0, arg_string,  &cellname, "cell", NULL},
    {"auth",  0, arg_negative_flag,    &auth, "don't authenticate", NULL},
    {"verbose", 0, arg_flag,    &verbose, "verbose output", NULL},
    {"help",    0, arg_flag,    &helpflag, NULL, NULL}
};

static void
usage(void)
{
    arg_printusage(args, sizeof(args)/sizeof(args[0]),
		   "afstool fileserver gettime", "");
}

int
fs_gettime_cmd(int argc, char **argv)
{
    const char *cell;
    int32_t sec, msec;
    int optind = 0;
    int i, ret;

    if (getarg (args, sizeof(args)/sizeof(args[0]), argc, argv, &optind)) {
	usage ();
	return 0;
    }

    if (helpflag) {
	usage ();
	return 0;
    }

    if (cellname)
	cell = cellname;
    else
	cell = cell_getthiscell();

    argc -= optind;
    argv += optind;

    if (argc == 0) {
	printf("no fileserver given\n");
	return 0;
    }
    
    mini_cachemanager_init();

    for (i = 0; i < argc; i++) {
	struct rx_connection *conn;
	char timebuf[128];
	struct tm t;
	time_t ti;

	if (verbose)
	    printf("fileserver: %s\n", argv[i]);

	conn = cbgetconn(cell, argv[i], "7000", FS_SERVICE_ID, auth);
	if (conn == NULL) {
	    printf("failed to get conn for %s", argv[i]);
	    continue;
	}

	ret = RXAFS_GetTime(conn, &sec, &msec);
	if (ret)
	    printf("GetTime failed with %d for host %s", ret, argv[i]);

	ti = sec;
	if (strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S %Z", 
		     localtime_r(&ti, &t)) > 0)
	    printf("%s time is %s", argv[i], timebuf);
	printf("\n");

	arlalib_destroyconn(conn);
    }

    return 0;
}
