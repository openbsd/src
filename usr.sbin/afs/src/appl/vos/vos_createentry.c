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

RCSID("$arla: vos_createentry.c,v 1.11 2001/09/14 14:52:03 tol Exp $");

static int helpflag;
static char *vol;
static char *host;
static char *fsserver;
static char *partition;
static char *cell;
static int noauth;
static int localauth;
static int rw_number;
static int ro_number;
static int bk_number;

static struct agetargs args[] = {
    {"id",	0, aarg_string,  &vol,  "id of volume", NULL, aarg_mandatory},
    {"host",	0, aarg_string,  &host, "what host to use", NULL, aarg_mandatory},
    {"fsserver",0, aarg_string,  &fsserver, "fsserver where the volume resides", NULL, aarg_mandatory},
    {"partition",0, aarg_string,  &partition, "partition where the volume resides", NULL, aarg_mandatory},
    {"rw", 0, aarg_integer, &rw_number, "volume RW number", NULL},
    {"ro", 0, aarg_integer, &ro_number, "volume RO number", NULL},
    {"bk", 0, aarg_integer, &bk_number, "volume BK number", NULL},
    {"cell",	0, aarg_string,    &cell, "what cell to use", NULL},
    {"noauth",	0, aarg_flag,    &noauth, "if to use authentication", NULL},
    {"localauth",0,aarg_flag,    &localauth, "localauth", NULL},
    {"help",	0, aarg_flag,    &helpflag, NULL, NULL},
    {NULL,      0, aarg_end, NULL}
};

static void
usage(void)
{
    aarg_printusage(args, "vos createentry", "", AARG_AFSSTYLE);
}

int
vos_createentry(int argc, char **argv)
{
    int optind = 0;
    struct rx_connection *connvldb = NULL;
    struct nvldbentry newentry;
    struct hostent *he;
    int error;

    helpflag = 0;
    vol = NULL;
    host = NULL;
    fsserver = NULL;
    partition = NULL;
    noauth = 0;
    localauth = 0;
    rw_number = 0;
    ro_number = 0;
    bk_number = 0;
    cell = NULL;

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

    connvldb = arlalib_getconnbyname(cell,
				     host,
				     afsvldbport,
				     VLDB_SERVICE_ID,
				     arlalib_getauthflag (noauth, localauth,
							  0, 0));
    if (connvldb == NULL)
	return -1;
    memset (&newentry, 0, sizeof (newentry));
    strlcpy(newentry.name, vol, VLDB_MAXNAMELEN);
    newentry.nServers = 1;
    he = gethostbyname(fsserver);
    if (he == NULL) {
	fprintf(stderr, "unknown host: %s\n", fsserver);
	return -1;
    }

    memcpy (&newentry.serverNumber[0], he->h_addr_list[0], 4);
    newentry.serverNumber[0] = ntohl(newentry.serverNumber[0]);
    newentry.serverPartition[0] = partition_name2num(partition);
    if (newentry.serverPartition[0] == -1) {
	fprintf(stderr, "incorrect partition\n");
	usage();
	return 0;
    }

    newentry.flags = 0;
    newentry.flags |= rw_number ? VLF_RWEXISTS : 0;
    newentry.flags |= ro_number ? VLF_ROEXISTS : 0;
    newentry.flags |= bk_number ? VLF_BOEXISTS : 0;

    newentry.serverFlags[0] = 0;
    newentry.serverFlags[0] |= rw_number ? VLSF_RWVOL : 0;
    newentry.serverFlags[0] |= ro_number ? VLSF_ROVOL : 0;
    newentry.serverFlags[0] |= bk_number ? VLSF_BACKVOL : 0;

    newentry.volumeId[0] = rw_number;
    newentry.volumeId[1] = ro_number;
    newentry.volumeId[2] = bk_number;
    newentry.cloneId = 0;
    error = VL_CreateEntryN(connvldb, &newentry);
    if (error) {
	fprintf(stderr, "vos_createentry: error %s (%d)\n", koerr_gettext(error), error);
	return 1;
    } 

    return 0;
}
