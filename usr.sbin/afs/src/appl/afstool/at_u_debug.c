/*
 * Copyright (c) 1998 - 2003 Kungliga Tekniska Högskolan
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

#include "at_locl.h"

RCSID("$arla: at_u_debug.c,v 1.3 2003/03/08 02:03:42 lha Exp $");

static int verbose;
static char *cell;
static char *portstr;
static int auth = 1;
static int helpflag;

static const char *
myctime(time_t time, char *datestr, size_t sz)
{
    struct tm tm;
    if (strftime (datestr, sz, "%Y-%m-%d %H:%M:%S %Z",
		  localtime_r(&time, &tm)) <= 0)
	strlcpy(datestr, "unknown-time", sz);
    return datestr;
}

static int
probehost(struct rx_connection *conn, arlalib_authflags_t auth)
{
    struct in_addr server;
    ubik_debug db;
    uint16_t port;
    int error;

    server.s_addr = rx_HostOf(rx_PeerOf(conn));
    port = rx_PortOf(rx_PeerOf(conn));

    error = Ubik_Debug(conn, &db);
    if (error)
	return error;
    
#define ABS_COMP_DB(dbname,field) (abs(db.now - dbname.field))

    {
	struct timeval tv;
	char datestring[128];

	gettimeofday(&tv, NULL);

	printf("Host %s time is %s\n", 
	       inet_ntoa(server), 
	       myctime(db.now, datestring, sizeof(datestring)));
	printf("Localtime is %s, differ %d seconds\n", 
	       myctime(tv.tv_sec, datestring, sizeof(datestring)),
	       abs(db.now - tv.tv_sec));
	server.s_addr = htonl(db.syncHost);
	printf("Last yes vote for %s secs was %d ago (at %s)\n",
	       inet_ntoa(server),
	       ABS_COMP_DB(db,lastYesTime),
	       myctime(db.lastYesTime, datestring, sizeof(datestring)));
	printf("Last vote started %d secs ago (at %s)\n", 
	       ABS_COMP_DB(db,syncTime),
	       myctime(db.lastYesTime, datestring, sizeof(datestring)));
	printf("Local db version is %u.%u\n", 
	       db.localVersion.epoch, 
	       db.localVersion.counter);
	printf("Syncsite db version is %u.%u\n",
	       db.syncVersion.epoch, 
	       db.syncVersion.counter);
	printf("%d locked pages, %d of them for write\n",
	       db.anyReadLocks + db.anyWriteLocks, db.anyWriteLocks);
	
	if (db.amSyncSite) {

	    printf("I'm the synchost for %d seconds more (%s)\n",
		   db.syncSiteUntil - db.now, 
		   myctime(db.syncSiteUntil, datestring, sizeof(datestring)));
	} else if (db.syncHost == 0) {
	    printf("I'm not the synchost and I don't know who is either!!!\n");
	} else {
	    server.s_addr = htonl(db.syncHost);
	    printf("I'm not the synchost, but %s is.\n",
		   inet_ntoa(server));    
	}

	if ((verbose && db.syncHost != 0) || db.amSyncSite) {
	    int i,j;
	    ubik_sdebug sdb;

	    if (!db.amSyncSite) {
		struct rx_connection *sync_conn;

		sync_conn = arlalib_getconnbyaddr(NULL, server.s_addr,
						  NULL, port,
						  VOTE_SERVICE_ID, auth);
		if (conn == NULL)
		    return ENETDOWN;
		error = Ubik_Debug(conn, &db);
		if (error)
		    return error;

		arlalib_destroyconn(sync_conn);
	    }

	    printf("Recover state is 0x%x\n", db.recoveryState);
	    printf("Last time a new db version was laballed was:\n"
		   "\t\t%d secs ago (at %s)\n\n",
		   ABS_COMP_DB(db,epochTime),
		   myctime(db.epochTime, datestring, sizeof(datestring)));

	    for (i = 0; i < db.nServers - 1; i++) {
		
		error = Ubik_SDebug(conn, i, &sdb);
		if (error) {
		    printf("Problem with host %d\n\n", i);
		    continue;
		}

		server.s_addr = htonl(sdb.addr);
		printf("Server (%s", inet_ntoa(server));
		for(j = 0; (sdb.altAddr[j] && (j < UBIK_MAX_INTERFACE_ADDR-1)) ; j++) {
		    server.s_addr = htonl(sdb.altAddr[j]);
		    printf(" %s", inet_ntoa(server));
		}
		printf("): (db %u.%u)\n",
		       sdb.remoteVersion.epoch,
		       sdb.remoteVersion.counter);
		printf("\tlast vote recived %d secs ago (at %s)\n", 
		       ABS_COMP_DB(sdb,lastVoteTime),
		       myctime(sdb.lastVoteTime, 
			       datestring, sizeof(datestring)));
		printf("\tlast beacon sent %d secs ago (at %s)\n",
		       ABS_COMP_DB(sdb,lastBeaconSent),
		       myctime(sdb.lastBeaconSent, 
			       datestring, sizeof(datestring)));
		printf("\tdbcurrent=%d, up=%d, beaconSince=%u\n",
		       sdb.currentDB,
		       sdb.up,
		       sdb.beaconSinceDown);
		
		printf("\n");

	    }
	}
    }
    return 0;
}

static struct getargs args[] = {
    {"verbose",	0, arg_flag,	&verbose, "verbose", NULL},
    {"port",	0, arg_string,  &portstr, "what port to use", NULL},
    {"cell",	0, arg_string,  &cell, "what cell to use", NULL},
    {"auth",	0, arg_negative_flag,    &auth, "no authentication", NULL},
    {"help",	0, arg_flag,    &helpflag, NULL, NULL}
};

static void
usage(void)
{
    char helpstring[100];

    snprintf(helpstring, sizeof(helpstring), "%s ubik debug", getprogname());
    arg_printusage(args, sizeof(args)/sizeof(args[0]), helpstring, "host ...");
}

int
u_debug_cmd (int argc, char **argv)
{
    int optind = 0;
    int i;

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

    if (portstr == NULL)
	portstr = "7003";

    if (argc == 0) {
	printf("missing host\n");
	return 0;
    }

    for (i = 0 ; i < argc; i++) {
	struct rx_connection *conn;
	int ret;

	if (verbose)
	    printf("Host: %s\n", argv[i]);

	conn = cbgetconn(cell, argv[i], portstr, VOTE_SERVICE_ID, auth);
	
	ret = probehost(conn, auth ? AUTHFLAGS_ANY : AUTHFLAGS_NOAUTH);
	if (ret)
	    printf("\n%s returned %s %d\n", argv[i], koerr_gettext(ret), ret);
	
	arlalib_destroyconn(conn);
    }

    return 0;
}
