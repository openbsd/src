/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appear in all copies and        *
* that both that copyright notice and this permission notice appear in     *
* supporting documentation, and that the name of IBM not be used in        *
* advertising or publicity pertaining to distribution of the software      *
* without specific, written prior permission.                              *
*                                                                          *
* IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL IBM *
* BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY      *
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER  *
* IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING   *
* OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.    *
****************************************************************************
*/
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <agetarg.h>

#include <stdio.h>
#include <err.h>

#include "rx_user.h"
#include "rx_clock.h"
#include "rx_queue.h"
#include "rx.h"

#include "config.h"
#include "roken.h"

RCSID ("$arla: rxdebug.c,v 1.15 2003/01/19 08:50:12 lha Exp $");

#define	TIMEOUT	    20

static uint16_t
PortNumber(char *aport)
{
    uint16_t port = atoi(aport);
    return htons(port);
}

static uint16_t
PortName(char *aname)
{
    struct servent *ts = getservbyname(aname, NULL);
    if (ts == NULL)
	return 0;
    return ts->s_port;	/* returns it in network byte order */
}

static int
MakeCall (int asocket, uint32_t ahost, uint16_t aport, char *adata, 
	  long alen, char *aresult, long aresultLen)
{
    static long counter = 100;
    long endTime;
    struct rx_header theader;
    char tbuffer[1500];
    int code;
    struct timeval tv;
    struct sockaddr_in taddr, faddr;
    int faddrLen;
    fd_set imask;
    char *tp;

    endTime = time(0) +	TIMEOUT; /* try for N seconds */
    counter++;
    tp = &tbuffer[sizeof(struct rx_header)];
    taddr.sin_family = AF_INET;
    taddr.sin_port = aport;
    taddr.sin_addr.s_addr = ahost;
    while(1) {
	memset(&theader, 0, sizeof(theader));
	theader.epoch = htonl(999);
	theader.cid = 0;
	theader.callNumber = htonl(counter);
	theader.seq = 0;
	theader.serial = 0;
	theader.type = RX_PACKET_TYPE_DEBUG;
	theader.flags = RX_CLIENT_INITIATED | RX_LAST_PACKET;
	theader.serviceId = 0;
	
	if (sizeof(theader) + alen > sizeof(tbuffer))
	    errx(1, "message to large");

	memcpy(tbuffer, &theader, sizeof(theader));
	memcpy(tp, adata, alen);
	code = sendto(asocket, tbuffer, alen+sizeof(struct rx_header), 0,
		      (struct sockaddr *)&taddr, sizeof(struct sockaddr_in));
	if (code == -1) {
	    err(1, "sendto");
	}
	/* see if there's a packet available */
	
	if (asocket >= FD_SETSIZE) {
	    printf("rxdebug: socket fd too large\n");
	    exit (1);
	}

	FD_ZERO(&imask);
	FD_SET(asocket,&imask); 
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	code = select(asocket + 1, &imask, NULL, NULL, &tv);
	if (code == -1) {
	    err(1, "select");
	}
	if (code > 0) {
	    /* now receive a packet */
	    faddrLen = sizeof(struct sockaddr_in);
	    code = recvfrom(asocket, tbuffer, sizeof(tbuffer), 0,
			    (struct sockaddr *)&faddr, &faddrLen);
	    if (code == -1) {
		err(1, "recvfrom");
	    }
	    memcpy(&theader, tbuffer, sizeof(struct rx_header));
	    if (counter == ntohl(theader.callNumber)) 
		break;
	}

	/* see if we've timed out */
	if (endTime < time(0)) return -1;
    }
    code -= sizeof(struct rx_header);
    if (code > aresultLen) code = aresultLen;
    memcpy(aresult, tp, code);
    return code;
}

static int
GetVersion(int asocket, uint32_t ahost, uint16_t aport, 
	   void *adata, long alen, 
	   char *aresult, long aresultLen)
{
    static long counter = 100;
    long endTime;
    struct rx_header theader;
    char tbuffer[1500];
    int code;
    struct timeval tv;
    struct sockaddr_in taddr, faddr;
    int faddrLen;
    fd_set imask;
    char *tp;

    endTime = time(0) +	TIMEOUT; /* try for N seconds */
    counter++;
    tp = &tbuffer[sizeof(struct rx_header)];
    taddr.sin_family = AF_INET;
    taddr.sin_port = aport;
    taddr.sin_addr.s_addr = ahost;
    while(1) {
	memset(&theader, 0, sizeof(theader));
	theader.epoch = htonl(999);
	theader.cid = 0;
	theader.callNumber = htonl(counter);
	theader.seq = 0;
	theader.serial = 0;
	theader.type = RX_PACKET_TYPE_VERSION;
	theader.flags = RX_CLIENT_INITIATED | RX_LAST_PACKET;
	theader.serviceId = 0;
	
	if (sizeof(theader) + alen > sizeof(tbuffer))
	    errx(1, "message to large");

	memcpy(tbuffer, &theader, sizeof(theader));
	memcpy(tp, adata, alen);

	code = sendto(asocket, tbuffer, alen+sizeof(struct rx_header), 0,
		      (struct sockaddr *)&taddr, sizeof(struct sockaddr_in));
	if (code == -1) {
            err(1, "sendto");
        }

	if (asocket >= FD_SETSIZE) {
	    printf("rxdebug: socket fd too large\n");
	    exit (1);
	}

	/* see if there's a packet available */
	FD_ZERO(&imask);
	FD_SET(asocket, &imask);

/* should be 1 */

	tv.tv_sec = 10;
	tv.tv_usec = 0;

	code = select(asocket + 1, &imask, 0, 0, &tv);
	if (code == -1) {
	    err(1, "select");
	}
	if (code > 0) {
	    /* now receive a packet */
	    faddrLen = sizeof(struct sockaddr_in);

	    code = recvfrom(asocket, tbuffer, sizeof(tbuffer), 0,
			    (struct sockaddr *)&faddr, &faddrLen);
	    if (code == -1) {
		err(1, "recvfrom");
	    }

	    memcpy(&theader, tbuffer, sizeof(struct rx_header));

	    if (counter == ntohl(theader.callNumber)) 
		break;
	}

	/* see if we've timed out */
	if (endTime < time(0)) return -1;
    }
    code -= sizeof(struct rx_header);
    if (code > aresultLen) 
	code = aresultLen;
    memcpy(aresult, tp, code);
    return code;
}

static int
MapOldConn (char version, struct rx_debugConn *tconn)
{
    int i;
    struct rx_debugConn_vL *vL = (struct rx_debugConn_vL *)tconn;
#define MOVEvL(a) (tconn->a = vL->a)
    
    if ((version <= RX_DEBUGI_VERSION_W_UNALIGNED_CONN) ||
	(version > RX_DEBUGI_VERSION)) {
	/* any old or unrecognized version... */
	for (i=0;i<RX_MAXCALLS;i++) {
	    MOVEvL(callState[i]);
	    MOVEvL(callMode[i]);
	    MOVEvL(callFlags[i]);
	    MOVEvL(callOther[i]);
	}
	if (version == RX_DEBUGI_VERSION_W_SECSTATS) {
	    MOVEvL(secStats.type);
	    MOVEvL(secStats.level);
	    MOVEvL(secStats.flags);
	    MOVEvL(secStats.expires);
	    MOVEvL(secStats.packetsReceived);
	    MOVEvL(secStats.packetsSent);
	    MOVEvL(secStats.bytesReceived);
	    MOVEvL(secStats.bytesSent);
	}
    }
    return 0;
}

static char *hostName;
static char *portName;
static int nodally;
static int allconns;
static int rxstats;
static char *onlyPortName;
static char *onlyHostName;
static int onlyServer;
static int onlyClient;
static char *onlyAuthName;
static int version_flag;
static int noConns;
static int helpflag;

static int
MainCommand (void)
{
    int i;
    int s;
    int j;
    struct sockaddr_in taddr;
    uint32_t host;
    struct in_addr hostAddr;
    uint16_t port;
    struct hostent *th;
    struct rx_debugIn tin;
    int code;
    uint32_t onlyHost;
    uint16_t onlyPort;
    int onlyAuth;
    int flag;
    int dallyCounter;
    int withSecStats;
    int withAllConn;
    int withRxStats;
    int withWaiters;
    struct rx_debugStats tstats;
    struct rx_debugConn tconn;

    char version[64];
    char nada[64];
    long length=64;
    
    if (onlyPortName) {
	char *name = onlyPortName;
	if ((onlyPort = PortNumber(name)) == 0)
	    onlyPort = PortName(name);
	if (onlyPort == 0) {
	    printf("rxdebug: can't resolve port name %s\n", name);
	    exit(1);
	}
    } else
	onlyPort = 0xffff;
    
    if (onlyHostName) {
	char *name = onlyHostName;
	struct hostent *th;
	th = gethostbyname(name);
	if (!th) {
	    printf("rxdebug: host %s not found in host table\n", name);
	    exit(1);
	}
	memcpy(&onlyHost, th->h_addr, sizeof(onlyHost));
    } else 
	onlyHost = 0xffffffff;

    if (onlyAuthName) {
	char *name = onlyAuthName;
	if (strcmp (name, "clear") == 0) onlyAuth = 0;
	else if (strcmp (name, "auth") == 0) onlyAuth = 1;
	else if (strcmp (name, "crypt") == 0) onlyAuth = 2;
	else if ((strcmp (name, "null") == 0) ||
		 (strcmp (name, "none") == 0) ||
		 (strncmp (name, "noauth", 6) == 0) ||
		 (strncmp (name, "unauth", 6) == 0)) onlyAuth = -1;
	else {
	    fprintf (stderr, "Unknown authentication level: %s\n", name);
	    exit (1);
	}
    } else onlyAuth = 999;

    /* lookup host */
    if (hostName) {
	th = gethostbyname(hostName);
	if (!th) {
	    printf("rxdebug: host %s not found in host table\n", hostName);
	    exit(1);
	}
	memcpy(&host, th->h_addr, sizeof(host));
    }
    else host = htonl(0x7f000001);	/* IP localhost */

    if (!portName)
	port = htons(7000);		/* default is fileserver */
    else {
	if ((port = PortNumber(portName)) == 0)
	    port = PortName(portName);
	if (port == 0) {
	    printf("rxdebug: can't resolve port name %s\n", portName);
	    exit(1);
	}
    }

    dallyCounter = 0; 

    hostAddr.s_addr = host;
    printf("Trying %s (port %d):\n", inet_ntoa(hostAddr), ntohs(port));
    s = socket(AF_INET, SOCK_DGRAM, 0);

    memset(&taddr, 0, sizeof(taddr));
    taddr.sin_family = AF_INET;
    taddr.sin_port = 0;
    taddr.sin_addr.s_addr = 0;

    code = bind(s, (struct sockaddr *)&taddr, sizeof(struct sockaddr_in));
    if (code) {
	perror("bind");
	exit(1);
    }
    
    if(version_flag)
    {
	nada[0] = '\0';
	
	code = GetVersion(s, host, port, nada, length,
			  version, length);
	if (code < 0)
	{
	    printf("get version call failed with code %d, errno %d\n",
		   code,errno);
	    exit(1);
	}
	printf("AFS version: %s\n",version);fflush(stdout);
	
	exit(0);
	
    }
    
    
    tin.type = htonl(RX_DEBUGI_GETSTATS);
    tin.index = 0;
    code = MakeCall(s, host, port, (char *) &tin, sizeof(tin),
		    (char *) &tstats, sizeof(tstats));
    if (code < 0) {
	printf("getstats call failed with code %d\n", code);
	exit(1);
    }

    withSecStats = (tstats.version >= RX_DEBUGI_VERSION_W_SECSTATS);
    withAllConn = (tstats.version >= RX_DEBUGI_VERSION_W_GETALLCONN);
    withRxStats = (tstats.version >= RX_DEBUGI_VERSION_W_RXSTATS);
    withWaiters = (tstats.version >= RX_DEBUGI_VERSION_W_WAITERS);

    printf("Free packets: %ld, packet reclaims: %ld, calls: %ld, "
	   "used FDs: %d\n",
	   (long)ntohl(tstats.nFreePackets),
	   (long)ntohl(tstats.packetReclaims),
	   (long)ntohl(tstats.callsExecuted),
	   tstats.usedFDs);
    if (!tstats.waitingForPackets) printf("not ");
    printf("waiting for packets.\n");
    if (withWaiters)
	printf("%ld calls waiting for a thread\n",
	       (long)ntohl(tstats.nWaiting));
    
    if (rxstats && withRxStats) {
	if(!withRxStats) {
	    fprintf (stderr, "WARNING: Server doens't support "
		     "retrieval of Rx statistics\n");
	} else {
	    struct rx_stats rxstats;
	    int i;
	    uint32_t *lp;
	
	    memset (&rxstats, 0, sizeof(rxstats));
	    tin.type = htonl(RX_DEBUGI_RXSTATS);
	    tin.index = 0;
	    /* should gracefully handle the case where rx_stats grows */
	    code = MakeCall(s, host, port, (char *) &tin, sizeof(tin),
			    (char *) &rxstats, sizeof(rxstats));
	    if (code < 0) {
		printf("rxstats call failed with code %d\n", code);
		exit(1);
	    }

	    if ((code == sizeof(tin)) &&
		(ntohl(((struct rx_debugIn *)(&rxstats))->type) ==
		 RX_DEBUGI_BADTYPE)) {
		fprintf (stderr, "WARNING: Server doens't support "
			 "retrieval of Rx statistics\n");
	    } else {
		if  (code != sizeof(rxstats)) {
		    /* handle other versions?... */
		    fprintf (stderr, "WARNING: returned Rx statistics of "
			     "unexpected size (got %d)\n",
			     code);
		}
		/* Since its all int32's, convert to host order with a loop. */
		lp = (uint32_t*)&rxstats;
		for (i=0; i< sizeof(rxstats)/sizeof(uint32_t); i++)
		    lp[i] = ntohl(lp[i]);
		
		rx_PrintTheseStats (stdout, &rxstats, sizeof(rxstats));
	    }
	}
    }
    
    if (noConns)
	return 0;
    
    tin.type = htonl(RX_DEBUGI_GETCONN);
    if (allconns)
	tin.type = htonl(RX_DEBUGI_GETALLCONN);
    
    if (onlyServer) printf ("Showing only server connections\n");
    if (onlyClient) printf ("Showing only client connections\n");
    if (onlyAuth != 999) {
	static char *name[] =
	    {"unauthenticated", "rxkad_clear", "rxkad_auth", "rxkad_crypt"};
	printf ("Showing only %s connections\n", name[onlyAuth+1]);
    }
    if (onlyHost != 0xffffffff) {
	hostAddr.s_addr = onlyHost;
	printf ("Showing only connections from host %s\n",
		inet_ntoa(hostAddr));
    }
    if (onlyPort != 0xffff)
	printf ("Showing only connections on port %u\n", ntohs(onlyPort));
    
    for(i=0;;i++) {
	tin.index = htonl(i);
	memset (&tconn, 0, sizeof(tconn));
	code = MakeCall(s, host, port, (char *)&tin, sizeof(tin),
			(char *) &tconn, sizeof(tconn));
	if (code < 0) {
	    printf("getconn call failed with code %d\n", code);
	    break;
	}
	MapOldConn (tstats.version, &tconn);
	if (tconn.cid == htonl(0xffffffff)) {
	    printf("Done.\n");
	    break;
	}
	
	/* see if we're in nodally mode and all calls are dallying */
	if (nodally) {
	    flag = 0;
	    for(j=0;j<RX_MAXCALLS;j++) {
		if (tconn.callState[j] != RX_STATE_NOTINIT &&
		    tconn.callState[j] != RX_STATE_DALLY) {
		    flag = 1;
		    break;
		}
	    }
	    if (flag == 0) {
		/* 
		 * this call looks too ordinary, bump skipped count and go
                 * around again
		 */
		dallyCounter++;
		continue;
	    }
	}
	if ((onlyHost != -1) && (onlyHost != tconn.host)) continue;
	if ((onlyPort != 0) && (onlyPort != tconn.port)) continue;
	if (onlyServer && (tconn.type != RX_SERVER_CONNECTION)) continue;
	if (onlyClient && (tconn.type != RX_CLIENT_CONNECTION)) continue;
	if (onlyAuth != 999) {
	    if (onlyAuth == -1) {
		if (tconn.securityIndex != 0) continue;
	    } else {
		if (tconn.securityIndex != 2) continue;
		if (withSecStats && (tconn.secStats.type == 3) &&
		    (tconn.secStats.level != onlyAuth)) continue;
	    }
	}
	
	/* now display the connection */
	hostAddr.s_addr = tconn.host;
	printf("Connection from host %s, port %d, ",
	       inet_ntoa(hostAddr), ntohs(tconn.port));
	if (tconn.epoch)
	    printf ("Cuid %lx/%lx", (unsigned long)ntohl(tconn.epoch),
		    (unsigned long)ntohl(tconn.cid));
	else
	    printf ("cid %lx", (unsigned long)ntohl(tconn.cid));
	if (tconn.error)
	    printf (", error %ld", (long)ntohl(tconn.error));
	printf("\n  serial %ld, ", (long)ntohl(tconn.serial));
	printf(" maxPacketSize %ld, ", (long)ntohl(tconn.maxPacketSize));
	
	if (tconn.flags) {
	    printf ("flags");
	    if (tconn.flags & RX_CONN_MAKECALL_WAITING)
		printf(" MAKECALL_WAITING");
	    if (tconn.flags & RX_CONN_DESTROY_ME) printf(" DESTROYED");
	    if (tconn.flags & RX_CONN_USING_PACKET_CKSUM) printf(" pktCksum");
	    printf (", ");
	}
	printf("security index %d, ", tconn.securityIndex);
	if (tconn.type == RX_CLIENT_CONNECTION) printf("client conn\n");
	else printf("server conn\n");
	
	if (withSecStats) {
	    switch ((int)tconn.secStats.type) {
	    case 0:
		if (tconn.securityIndex == 2)
		    printf ("  no GetStats procedure for security object\n");
		break;
	    case 1:
		printf ("  rxnull level=%d, flags=%d\n",
			tconn.secStats.level, (int)tconn.secStats.flags);
		break;
	    case 2:
		printf ("  rxvab level=%d, flags=%d\n",
			tconn.secStats.level, (int)tconn.secStats.flags);
		break;
	    case 3: {
		char *level;
		char flags = ntohl(tconn.secStats.flags);
		if (tconn.secStats.level == 0) level = "clear";
		else if (tconn.secStats.level == 1) level = "auth";
		else if (tconn.secStats.level == 2) level = "crypt";
		else level = "unknown";
		printf ("  rxkad: level %s", level);
		if (flags) printf (", flags");
		if (flags & 1) printf (" unalloc");
		if (flags & 2) printf (" authenticated");
		if (flags & 4) printf (" expired");
		if (flags & 8) printf (" pktCksum");
		if (tconn.secStats.expires)
		    /* Apparently due to a bug in the RT compiler that
		     * prevents (u_long)0xffffffff => (double) from working,
		     * this code produces negative lifetimes when run on the
		     * RT. */
		    printf (", expires in %.1f hours",
			    ((u_long)ntohl(tconn.secStats.expires) -
			     time(0)) / 3600.0);
		if (!(flags & 1)) {
		    printf ("\n  Received %lu bytes in %lu packets\n", 
			    (long)ntohl(tconn.secStats.bytesReceived),
			    (long)ntohl(tconn.secStats.packetsReceived));
		    printf ("  Sent %lu bytes in %lu packets\n", 
			    (long)ntohl(tconn.secStats.bytesSent),
			    (long)ntohl(tconn.secStats.packetsSent));
		} else
		    printf ("\n");
		break;
	    }
	    
	    default: printf("  unknown\n");
	    }
	}
	
	for(j=0;j<RX_MAXCALLS;j++) {
	    printf("    call %d: # %ld, state ", j,
		   (long)ntohl(tconn.callNumber[j]));
	    if (tconn.callState[j]==RX_STATE_NOTINIT) {
		printf("not initialized\n");
		continue;
	    }
	    else if (tconn.callState[j]==RX_STATE_PRECALL)
		printf("precall, ");
	    else if (tconn.callState[j] == RX_STATE_ACTIVE)
		printf("active, ");
	    else if (tconn.callState[j] == RX_STATE_DALLY)
		printf("dally, ");
	    printf("mode: ");
	    if (tconn.callMode[j]==RX_MODE_SENDING)
		printf("sending");
	    else if (tconn.callMode[j]==RX_MODE_RECEIVING)
		printf("receiving");
	    else if (tconn.callMode[j]==RX_MODE_ERROR)
		printf("error");
	    else if (tconn.callMode[j] == RX_MODE_EOF)
		printf("eof");
	    else printf("unknown");
	    if (tconn.callFlags[j]) {
		printf(", flags:");
		if (tconn.callFlags[j]&RX_CALL_READER_WAIT)
		    printf(" reader_wait");
		if (tconn.callFlags[j]&RX_CALL_WAIT_WINDOW_ALLOC)
		    printf(" window_alloc");
		if (tconn.callFlags[j]&RX_CALL_WAIT_WINDOW_SEND)
		    printf(" window_send");
		if (tconn.callFlags[j]&RX_CALL_WAIT_PACKETS)
		    printf(" wait_packets");
		if (tconn.callFlags[j]&RX_CALL_WAIT_PROC)
		    printf(" waiting_for_process");
		if (tconn.callFlags[j]&RX_CALL_RECEIVE_DONE)
		    printf(" receive_done");
		if (tconn.callFlags[j]&RX_CALL_CLEARED)
		    printf(" call_cleared");
	    }
	    if (tconn.callOther[j] & RX_OTHER_IN)
		printf(", has_input_packets");
	    if (tconn.callOther[j] & RX_OTHER_OUT)
		printf(", has_output_packets");
	    printf("\n");
	}
    }
    if (nodally) printf("Skipped %d dallying connections.\n", dallyCounter);
    return 0;
}

static struct agetargs args[] = {
    {"servers",  0, aarg_string,  &hostName,
     "server machine", NULL, aarg_mandatory},
    {"port", 0, aarg_string, &portName,
     "IP port", NULL, aarg_optional_swless },
    {"nodally", 0, aarg_flag, &nodally,
     "don't show dallying conns", NULL },
    {"allconnections", 0, aarg_flag, &allconns,
     "don't filter out uninteresting connections on server"},
    {"rxstats", 0, aarg_flag, &rxstats,
     "show Rx statistics", NULL },
    {"onlyserver", 0, aarg_flag, &onlyServer,
     "only show server conns", NULL },
    {"onlyclient", 0, aarg_flag, &onlyClient,
     "only show client conns", NULL},
    {"onlyport", 0, aarg_integer, &onlyPortName,
     "show only <port>", NULL },
    {"onlyhost", 0, aarg_string, &onlyHostName,
     "show only <host>", NULL },
    {"onlyauth", 0, aarg_string, &onlyAuthName,
     "show only <auth level>", NULL },
    {"version", 0, aarg_flag, &version_flag,
     "show AFS version id", NULL },
    {"noconns", 0, aarg_flag, &noConns,
     "show no connections", NULL },
    {NULL}
};

static void
usage(void)
{   
    aarg_printusage (args, "rxdebug", "", AARG_AFSSTYLE);
}


/* simple main program */

int
main(int argc, char **argv)
{

    int optind = 0;
    
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
        fprintf (stderr, "create volume: unparsed arguments\n");
        return 0;
    }

    return MainCommand();
}
