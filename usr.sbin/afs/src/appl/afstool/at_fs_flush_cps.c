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

RCSID("$arla: at_fs_flush_cps.c,v 1.4 2003/03/10 20:58:07 lha Exp $");

static char *cellname;
static int auth;
static int helpflag;
static int verbose;
static struct getarg_strings viceids_strings, hosts_strings;

static struct getargs args[] = {
    {"viceids",    0, arg_strings, &viceids_strings, "id", NULL},
    {"hosts",    0, arg_strings, &hosts_strings, "host", NULL},
    {"cell",    0, arg_string,  &cellname, "cell", NULL},
    {"auth",  0, arg_negative_flag,    &auth, "don't authenticate", NULL},
    {"verbose", 0, arg_flag,    &verbose, "verbose output", NULL},
    {"help",    0, arg_flag,    &helpflag, NULL, NULL}
};

static void
usage(void)
{
    arg_printusage(args, sizeof(args)/sizeof(args[0]),
		   "afstool fileserver flushcps", "fileserver ...");
}

int
fs_FlushCPS_cmd(int argc, char **argv)
{
    struct rx_connection *conn;
    int32_t viceids[FLUSHMAX];
    int32_t hosts[FLUSHMAX];
    arlalib_authflags_t auth;
    int32_t spare2,spare3;
    const char *cell;
    ViceIds vids;
    IPAddrs ipaddrs;
    int optind = 0;
    int i, ret;

    memset(viceids, 0, sizeof(vids));
    memset(hosts, 0, sizeof(hosts));
    memset(&vids, 0, sizeof(vids));
    memset(&ipaddrs, 0, sizeof(ipaddrs));

    conn = NULL;

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

    if (viceids_strings.num_strings == 0 && hosts_strings.num_strings == 0) {
	printf("missing viceid or hostid\n");
	return 0;
    }

    if (viceids_strings.num_strings > FLUSHMAX) {
	printf("too many vice id strings\n");
	return 0;
    }

    vids.len = 0;
    vids.val = viceids;

    for(i = 0; i < viceids_strings.num_strings; i++) {
	if (verbose)
	    printf("viceid: %s\n", viceids_strings.strings[i]);
	ret = arlalib_get_viceid(viceids_strings.strings[i],
				 cell, &vids.val[vids.len]);
	if (ret == 0) {
	    if (verbose)
		printf("%s is userid %d\n", 
		       viceids_strings.strings[i], viceids[vids.len]);
	    vids.len++;
	} else
	    printf("failed to parse user `%s', got error %d\n", 
		   viceids_strings.strings[i], ret);
    }

    ipaddrs.len = 0;
    ipaddrs.val = hosts;

    for(i = 0; i < hosts_strings.num_strings; i++) {
	struct addrinfo hints, *res0, *res;
	int error;

	if (verbose)
	    printf("hosts: %s\n", hosts_strings.strings[i]);

	memset (&hints, 0, sizeof(hints));
	hints.ai_family = PF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	
	error = getaddrinfo(hosts_strings.strings[i], NULL, &hints, &res0);
	if (error) {
	    fprintf (stderr, "Cannot find host %s: %s\n", 
		     hosts_strings.strings[i], gai_strerror(error));
	    continue;
	}

	for (res = res0; res != NULL; res = res->ai_next) {
	    if (res->ai_socktype == AF_INET)
		break;
	}
	if (res) {
	    ipaddrs.val[ipaddrs.len] = 
		((struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr;
	    ipaddrs.len++;
	} else
	    printf("Failed to find IPv4 address for %s.\n", 
		   hosts_strings.strings[i]);

	freeaddrinfo(res0);
    }

    argc -= optind;
    argv += optind;

    if (argc == 0) {
	printf("no fileserver given\n");
	return 0;
    }
    
    for (i = 0; i < argc; i++) {
	struct rx_connection *conn;
	int32_t ret;

	if (verbose)
	    printf("fileserver: %s\n", argv[i]);

	conn = cbgetconn(cell, argv[i], "7000", FS_SERVICE_ID, auth);
	if (conn == NULL) {
	    printf("failed to get conn for %s", argv[i]);
	    continue;
	}

	ret = RXAFS_FlushCPS(conn, &vids, &ipaddrs, 0, &spare2, &spare3);
	if (ret)
	    printf("FlushCPS failed with %d for host %s", ret, argv[i]);

	arlalib_destroyconn(conn);
    }

    return 0;
}
