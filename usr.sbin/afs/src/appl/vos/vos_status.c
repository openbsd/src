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

RCSID("$arla: vos_status.c,v 1.11 2003/03/08 02:33:34 lha Exp $");

static int
printstatus (const char *cell, const char *host,
	     int noauth, int verbose)
{
    struct rx_connection *connvolser = NULL;
    struct transDebugInfo *entries;
    transDebugEntries info;
    unsigned int entries_len, i;
    int error;

    connvolser = arlalib_getconnbyname(cell,
				       host,
				       afsvolport,
				       VOLSERVICE_ID,
				       0);

    if (connvolser == NULL)
	return -1;

    error = VOLSER_AFSVolMonitor(connvolser, &info);
    if (error) {
	printf("printstatus: GetStat failed with: %s (%d)\n",
	       koerr_gettext(error), error);
	return -1;
    }
    
    entries_len = info.len;
    entries = info.val;

    if (entries_len == 0)
	printf ("No active transactions on %s\n", host);
    else {
	char *line = "--------------------------------------";

	printf("Total transactions: %d\n", i);
	for (i = 0; i < entries_len; i++) {
	    char timestr[128];
	    char part[100];
	    struct tm tm;
	    time_t t;

	    printf("%s\n", line);
	    
	    memset (&tm, 0, sizeof(tm));
	    t = entries->creationTime;
	    if (strftime (timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S %Z",
			  localtime_r(&t, &tm)) <= 0)
		strlcpy(timestr, "unknown-time", sizeof(timestr));
	    printf("transaction: %d  created: %s\n", entries->tid, timestr);
	    printf("attachFlags:  ");

	    if ((entries->iflags & ITOffline) == ITOffline)
		printf(" offline");
	    if ((entries->iflags & ITBusy) == ITBusy)
		printf(" busy");
	    if ((entries->iflags & ITReadOnly) == ITReadOnly)
		printf("read-only");
	    if ((entries->iflags & ITCreate) == ITCreate)
		printf("create");
	    if ((entries->iflags & ITCreateVolID) == ITCreateVolID)
		printf("create-VolID");

	    partition_num2name(entries->partition, part, sizeof(part));
	    printf("\nvolume: %d  partition: %s procedure: %s\n", 
		   entries->volid, part, entries->lastProcName);
	    printf("packetRead: %d  lastReceiveTime: %d  "
		   "packetSend: %d  lastSendTime: %d\n",
		   entries->readNext, 
		   entries->lastReceiveTime, 
		   entries->transmitNext, 
		   entries->lastSendTime);
	    entries++;
	}
	printf("%s\n", line);
    }

    arlalib_destroyconn(connvolser);
    return 0;
}

static int helpflag;
static const char *server;
static const char *cell;
static int noauth;
static int verbose;

static struct agetargs args[] = {
    {"server",	0, aarg_string,	&server,	"server", NULL,aarg_mandatory},
    {"cell",	0, aarg_string,	&cell,		"cell",	  NULL},
    {"noauth",	0, aarg_flag,	&noauth,	"do not authenticate", NULL},
    {"verbose",	0, aarg_flag,	&verbose,	"be verbose", NULL},
    {"help",	0, aarg_flag,	&helpflag,	NULL, NULL},
    {NULL,	0, aarg_end,	NULL,		NULL, NULL}
};

static void
usage (void)
{
    aarg_printusage (args, "vos status", "", AARG_AFSSTYLE);
}

int
vos_status(int argc, char **argv)
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
	printf ("vos status: missing -server\n");
	return 0;
    }

    if (cell == NULL)
	cell = cell_getcellbyhost (server);

    printstatus (cell, server, noauth, verbose);
    return 0;
}
