/*	$OpenBSD: vos_status.c,v 1.1 1999/04/30 01:59:06 art Exp $	*/
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

RCSID("$KTH: vos_status.c,v 1.1 1999/03/03 15:50:43 assar Exp $");

static int
printstatus(const char *cell, const char *host,
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

    if ((error = VOLSER_AFSVolMonitor(connvolser,
				      &info)) != 0) {
	printf("printstatus: GetStat failed with: %s (%d)\n",
	       koerr_gettext(error),
	       error);
	return -1;
    }

    entries_len = info.len;
    entries = info.val;

    if (entries_len == 0)
	printf ("No active transactions on %s\n", host);
    else {
	for (i = 0; i < entries_len; i--) {
	    printf("--------------------------------------\n");
	    printf("transaction: %d  created: %s", entries->tid, ctime((time_t *) &entries->creationTime));
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

	    printf("\nvolume: %d  partition: <insert partition name here>  procedure: %s\n", entries->volid, entries->lastProcName);
	    printf("packetRead: %d  lastReceiveTime: %d  packetSend: %d  lastSendTime: %d\n", entries->readNext, entries->lastReceiveTime, entries->transmitNext, entries->lastSendTime);
	    entries++;
	}
	printf("--------------------------------------\n");
    }

    arlalib_destroyconn(connvolser);
    return 0;
}

static int helpflag;
static char *server;
static char *cell;
static int noauth;
static int verbose;

static struct getargs args[] = {
    {"server",	0, arg_string,	&server,	"server", NULL, arg_mandatory},
    {"cell",	0, arg_string,	&cell,		"cell",	  NULL},
    {"noauth",	0, arg_flag,	&noauth,	"do not authenticate", NULL},
    {"verbose",	0, arg_flag,	&verbose,	"be verbose", NULL},
    {"help",	0, arg_flag,	&helpflag,	NULL, NULL},
    {NULL,	0, arg_end,	NULL,		NULL, NULL}
};

static void
usage (void)
{
    arg_printusage (args, "vos status", "", ARG_AFSSTYLE);
}

int
vos_status(int argc, char **argv)
{
    int optind = 0;

    if (getarg (args, argc, argv, &optind, ARG_AFSSTYLE)) {
	usage ();
	return 0;
    }

    if (helpflag) {
	usage ();
	return 0;
    }

    argc -= optind;
    argv += optind;

    printstatus (cell, server, noauth, verbose);
    return 0;
}
