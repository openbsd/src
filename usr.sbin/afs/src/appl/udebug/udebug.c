/*
 * Copyright (c) 1998 - 2000 Kungliga Tekniska Högskolan
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
RCSID("$KTH: udebug.c,v 1.18 2000/10/10 17:30:04 lha Exp $");

static int verbose = 0;

static void
usage(void)
{
    fprintf(stderr,  
	    "udebug: Version $KTH: udebug.c,v 1.18 2000/10/10 17:30:04 lha Exp $\n"
	    "usage: udebug -servers server ... -port port -noauth -long\n");
    exit(1);
}


static void
newhost(u_int32_t **hosts, int *len, char *host)
{
    struct addrinfo hints, *res;
    u_int32_t *ptr;
    int ret;

    if (host == NULL)
	return;
    
    memset (&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    
    ret = getaddrinfo(host, NULL, &hints, &res);
    if (ret < 0) {
	warnx("cant find addr for '%s'.", host);
	return;
    }
    assert (res->ai_family == PF_INET);
    
    ptr = realloc(*hosts, sizeof(u_int32_t) * ++*len);
    if (ptr == NULL) {
	free(*hosts);
	hosts = NULL;
	err(1, "realloc");
    }
    
    *hosts = ptr;
    
    ptr[*len-1] = ((struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr;

    freeaddrinfo(res);
}

static const char *
myctime(time_t time, char *datestr, size_t sz)
{
    struct tm tm;
    strftime (datestr, sz, "%Y-%m-%d %H:%M:%S",
	      localtime_r(&time, &tm));
    return datestr;
}

static void
ProbeHost(u_int32_t host, u_int16_t port, arlalib_authflags_t auth)
{
    struct rx_connection *conn;
    ubik_debug db;
    int error;
    struct in_addr server;

    server.s_addr = host;

    conn = arlalib_getconnbyaddr(NULL,
				 host,
				 NULL,
				 port,
				 VOTE_SERVICE_ID,
				 auth);
    
    if (conn == NULL) {
	warnx("Could not contact host %s", inet_ntoa(server));
	return;
    }

    error = Ubik_Debug(conn, &db);
    if (error) {
	fprintf(stderr, "ProbeHost: Ubik_Debug: %s (%d)\n", 
		koerr_gettext(error), error);

	return;
    }
    
#define ABS_COMP_DB(dbname,field) (abs(db.now - dbname##.##field))

    {
	struct timeval tv;
	char datestring[100];

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
	} else {
	    server.s_addr = htonl(db.syncHost);
	    printf("I'm not the synchost, but %s is.\n",
		   inet_ntoa(server));    
	}

	if (verbose || db.amSyncSite) {
	    int i;
	    ubik_sdebug sdb;

	    if (!db.amSyncSite) {
		arlalib_destroyconn (conn);
		conn = arlalib_getconnbyaddr(NULL, server.s_addr,
					     NULL, port,
					     VOTE_SERVICE_ID, auth);
		if (conn == NULL) {
		    warnx("Could not contact host %s", inet_ntoa(server));
		    return;
		}
		error = Ubik_Debug(conn, &db);
		if (error) {
		    fprintf(stderr, "ProbeHost: Ubik_Debug: %s (%d)\n", 
			    koerr_gettext(error), error);

		    return;
		}
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
		printf("Server %s: (db %u.%u)\n",
		       inet_ntoa(server),
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
}


int
main(int argc, char **argv)
{
    u_int32_t *hosts = NULL;
    int len = 0;
    u_int16_t port = 0;
    enum { START, NOWHERE, HOST, PORT } state  = START;
    
    if (argc < 2)
	usage();

    argv++;
    argc--;
    
    while (argc) {
	if (*argv[0] == '-') {
	    if (strncmp(*argv, "-server", 7) == 0)
		state = HOST;
	    else if (strcmp(*argv, "-port") == 0) 
		state = PORT;
	    else if (strcmp(*argv, "-long") == 0)
		verbose = 1;
	    else 
		usage();
	} else {
	    switch (state) {
	    case START:
		if (argc != 2)
		    usage();
		newhost(&hosts, &len, *argv);
		argv++;
		argc--;
		port = atoi(*argv);
		break;
	    case HOST:
		newhost(&hosts, &len, *argv);
		break;
	    case PORT:
		port = atoi(*argv);
		state = NOWHERE;
		break;
	    case NOWHERE:
		usage();
	    default:
		exit(-1);
	    }	
	}
	argc--;
	argv++;
    }

    if (hosts == NULL)
	errx(1, "No hosts found !");
    
    if (port == 0)
	errx(1, "No port given");

    while (len) {
	ProbeHost(*hosts, port, AUTHFLAGS_NOAUTH); /* XXX */
	hosts++;
	len--;
    }

    return 0;
}
