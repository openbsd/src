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

/* RX:  Extended Remote Procedure Call */

#include <assert.h>
#include "rx_locl.h"

RCSID("$arla: rx.c,v 1.34 2003/04/08 22:14:29 lha Exp $");

/*
 * quota system: each attached server process must be able to make
 *  progress to avoid system deadlock, so we ensure that we can always
 *  handle the arrival of the next unacknowledged data packet for an
 *  attached call.  rxi_dataQuota gives the max # of packets that must be
 *  reserved for active calls for them to be able to make progress, which is
 *  essentially enough to queue up a window-full of packets (the first packet
 *  may be missing, so these may not get read) + the # of packets the thread
 *  may use before reading all of its input (# free must be one more than send
 *  packet quota).  Thus, each thread allocates rx_Window+1 (max queued input
 *  packets) + an extra for sending data.  The system also reserves
 *  RX_MAX_QUOTA (must be more than RX_PACKET_QUOTA[i], which is 10), so that
 *  the extra packet can be sent (must be under the system-wide send packet
 *  quota to send any packets)
 */
/* # to reserve so that thread with input can still make calls (send packets)
   without blocking */
long rx_tq_dropped = 0;		       /* Solaris only temp variable */
long rxi_dataQuota = RX_MAX_QUOTA;     /* packets to reserve for active
				        * threads */

/*
 * Variables for handling the minProcs implementation.  availProcs gives the
 * number of threads available in the pool at this moment (not counting dudes
 * executing right now).  totalMin gives the total number of procs required
 * for handling all minProcs requests.  minDeficit is a dynamic variable
 * tracking the # of procs required to satisfy all of the remaining minProcs
 * demands.
 */

long rxi_availProcs = 0;	       /* number of threads in the pool */
long rxi_totalMin;		       /* Sum(minProcs) forall services */
long rxi_minDeficit = 0;	       /* number of procs needed to handle
				        * all minProcs */
long Rx0 = 0, Rx1 = 0;

struct rx_serverQueueEntry *rx_waitForPacket = 0;
struct rx_packet *rx_allocedP = 0;

/* ------------Exported Interfaces------------- */


/*
 * This function allows rxkad to set the epoch to a suitably random number
 * which rx_NewConnection will use in the future.  The principle purpose is to
 * get rxnull connections to use the same epoch as the rxkad connections do, at
 * least once the first rxkad connection is established.  This is important now
 * that the host/port addresses aren't used in FindConnection: the uniqueness
 * of epoch/cid matters and the start time won't do.
 */

void
rx_SetEpoch(uint32_t epoch)
{
    rx_epoch = epoch;
}

/*
 * Initialize rx.  A port number may be mentioned, in which case this
 * becomes the default port number for any service installed later.
 * If 0 is provided for the port number, a random port will be chosen
 * by the kernel.  Whether this will ever overlap anything in
 * /etc/services is anybody's guess...  Returns 0 on success, -1 on
 * error.
 */
static int rxinit_status = 1;

int
rx_Init(uint16_t port)
{
    struct timeval tv;
    char *htable, *ptable;
    uint16_t rport;

    SPLVAR;

    if (rxinit_status != 1)
	return rxinit_status;	       /* Already done; return previous error
				        * code. */

    /*
     * Allocate and initialize a socket for client and perhaps server
     * connections
     */
    rx_socket = rxi_GetUDPSocket(port, &rport);

    if (rx_socket == OSI_NULLSOCKET) {
	return RX_ADDRINUSE;
    }
#if	defined(AFS_GLOBAL_SUNLOCK) && defined(KERNEL)
    LOCK_INIT(&afs_rxglobal_lock, "afs_rxglobal_lock");
#endif
#ifdef	RX_ENABLE_LOCKS
    LOCK_INIT(&rx_freePktQ_lock, "rx_freePktQ_lock");
    LOCK_INIT(&freeSQEList_lock, "freeSQEList lock");
    LOCK_INIT(&rx_waitingForPackets_lock, "rx_waitingForPackets lock");
    LOCK_INIT(&rx_freeCallQueue_lock, "rx_waitingForPackets lock");
#endif

    rxi_nCalls = 0;
    rx_connDeadTime = 12;
    memset((char *) &rx_stats, 0, sizeof(struct rx_stats));

    htable = (char *) osi_Alloc(rx_hashTableSize *
				sizeof(struct rx_connection *));
    PIN(htable, rx_hashTableSize *
	sizeof(struct rx_connection *));	/* XXXXX */
    memset(htable, 0, rx_hashTableSize *
	  sizeof(struct rx_connection *));
    ptable = (char *) osi_Alloc(rx_hashTableSize * sizeof(struct rx_peer *));
    PIN(ptable, rx_hashTableSize * sizeof(struct rx_peer *));	/* XXXXX */
    memset(ptable, 0, rx_hashTableSize * sizeof(struct rx_peer *));
    NETPRI;

    osi_GetTime(&tv);
    /*
     * *Slightly* random start time for the cid.  This is just to help
     * out with the hashing function at the peer
     */
    rx_port = rport;
    rx_stats.minRtt.sec = 9999999;
    rx_SetEpoch(tv.tv_sec);	       /*
					* Start time of this package, rxkad
				        * will provide a randomer value. 
					*/
    rxi_dataQuota += rx_extraQuota;    /*
					* + extra packets caller asked to
				        * reserve
					*/
    rx_nPackets = rx_extraPackets + RX_MAX_QUOTA + 2;	/* fudge */
    rx_nextCid = ((tv.tv_sec ^ tv.tv_usec) << RX_CIDSHIFT);
    rx_connHashTable = (struct rx_connection **) htable;
    rx_peerHashTable = (struct rx_peer **) ptable;

    clock_Init();
    rxevent_Init(20, rxi_ReScheduleEvents);

    /* Malloc up a bunch of packet buffers */
    rx_nFreePackets = 0;
    queue_Init(&rx_freePacketQueue);
    rx_nFreeCbufs = 0;
    queue_Init(&rx_freeCbufQueue);
    USERPRI;
    rxi_MorePackets(rx_nPackets);
    rxi_MoreCbufs(rx_nPackets);
    rx_CheckCbufs(0);
    NETPRI;

    rx_lastAckDelay.sec = 0;
    rx_lastAckDelay.usec = 400000; /* 400 ms */
#ifdef SOFT_ACK
    rx_hardAckDelay.sec = 0;
    rx_hardAckDelay.usec = 100000; /* 100 ms */
    rx_softAckDelay.sec = 0;
    rx_softAckDelay.usec = 100000; /* 100 ms */
#endif /* SOFT_ACK */

    /* Initialize various global queues */
    queue_Init(&rx_idleServerQueue);
    queue_Init(&rx_incomingCallQueue);
    queue_Init(&rx_freeCallQueue);

    /*
     * Start listener process (exact function is dependent on the
     * implementation environment--kernel or user space)
     */
    rxi_StartListener();

    USERPRI;
    rxinit_status = 0;
    return rxinit_status;
}

/*
 * called with unincremented nRequestsRunning to see if it is OK to start
 * a new thread in this service.  Could be "no" for two reasons: over the
 * max quota, or would prevent others from reaching their min quota.
 */
static int
QuotaOK(struct rx_service * aservice)
{
    /* check if over max quota */
    if (aservice->nRequestsRunning >= aservice->maxProcs)
	return 0;

    /* under min quota, we're OK */
    if (aservice->nRequestsRunning < aservice->minProcs)
	return 1;

    /*
     * otherwise, can use only if there are enough to allow everyone to go to
     * their min quota after this guy starts.
     */
    if (rxi_availProcs > rxi_minDeficit)
	return 1;
    else
	return 0;
}


/*
 * This routine must be called if any services are exported.  If the
 * donateMe flag is set, the calling process is donated to the server
 * process pool
 */
void
rx_StartServer(int donateMe)
{
    struct rx_service *service;
    int i;

    SPLVAR;
    clock_NewTime();

    NETPRI;
    /*
     * Start server processes, if necessary (exact function is dependent
     * on the implementation environment--kernel or user space).  DonateMe
     * will be 1 if there is 1 pre-existing proc, i.e. this one.  In this
     * case, one less new proc will be created rx_StartServerProcs.
     */
    rxi_StartServerProcs(donateMe);

    /*
     * count up the # of threads in minProcs, and add set the min deficit to
     * be that value, too.
     */
    for (i = 0; i < RX_MAX_SERVICES; i++) {
	service = rx_services[i];
	if (service == (struct rx_service *) 0)
	    break;
	rxi_totalMin += service->minProcs;
	/*
	 * below works even if a thread is running, since minDeficit would
	 * still have been decremented and later re-incremented.
	 */
	rxi_minDeficit += service->minProcs;
    }

    /* Turn on reaping of idle server connections */
    rxi_ReapConnections();

    if (donateMe)
	rx_ServerProc();	       /* Never returns */
    USERPRI;
    return;
}

/*
 * Create a new client connection to the specified service, using the
 * specified security object to implement the security model for this
 * connection.
 */
struct rx_connection *
rx_NewConnection(uint32_t shost, uint16_t sport, uint16_t sservice,
		 struct rx_securityClass * securityObject,
		 int serviceSecurityIndex)
{
    int hashindex, error;
    long cid;
    struct rx_connection *conn;

    SPLVAR;
#if defined(AFS_SGIMP_ENV)
    GLOCKSTATE ms;

#endif

    clock_NewTime();
    dpf(("rx_NewConnection(host %x, port %u, service %u, "
	 "securityObject %x, serviceSecurityIndex %d)\n",
	 shost, sport, sservice, securityObject, serviceSecurityIndex));
    GLOBAL_LOCK();
#if defined(AFS_SGIMP_ENV)
    AFS_GRELEASE(&ms);
    /* NETPRI protects Cid and Alloc */
    NETPRI;
#endif
    cid = (rx_nextCid += RX_MAXCALLS);
    conn = rxi_AllocConnection();
#if !defined(AFS_SGIMP_ENV)
    NETPRI;
#endif
    conn->type = RX_CLIENT_CONNECTION;
    conn->cid = cid;
    conn->epoch = rx_epoch;
    conn->peer = rxi_FindPeer(shost, sport);
    queue_Append(&conn->peer->connQueue, conn);
    conn->serviceId = sservice;
    conn->securityObject = securityObject;
    conn->securityData = (void *) 0;
    conn->securityIndex = serviceSecurityIndex;
    conn->maxPacketSize = MIN(conn->peer->packetSize, OLD_MAX_PACKET_SIZE);
    rx_SetConnDeadTime(conn, rx_connDeadTime);

    error = RXS_NewConnection(securityObject, conn);
    if (error)
	conn->error = error;
    hashindex = CONN_HASH(shost, sport, conn->cid, conn->epoch,
			  RX_CLIENT_CONNECTION);
    conn->next = rx_connHashTable[hashindex];
    rx_connHashTable[hashindex] = conn;
    rx_stats.nClientConns++;

    conn->refCount++;
    USERPRI;
#if defined(AFS_SGIMP_ENV)
    AFS_GACQUIRE(&ms);
#endif
    GLOBAL_UNLOCK();
    return conn;
}

void
rxi_SetConnDeadTime(struct rx_connection * conn, int seconds)
{
    /*
     * This is completely silly; perhaps exponential back off
     * would be more reasonable? XXXX
     */
    conn->secondsUntilDead = seconds;
    conn->secondsUntilPing = seconds / 6;
    if (conn->secondsUntilPing == 0)
	conn->secondsUntilPing = 1;    /* XXXX */
}

/* Destroy the specified connection */
void
rx_DestroyConnection(struct rx_connection * conn)
{
    struct rx_connection **conn_ptr;
    int i;
    int havecalls = 0;

    SPLVAR;

    clock_NewTime();

    NETPRI;
    if (--conn->refCount > 0) {
	/* Busy; wait till the last guy before proceeding */
	USERPRI;
	return;
    }
    /*
     * If the client previously called rx_NewCall, but it is still
     * waiting, treat this as a running call, and wait to destroy the
     * connection later when the call completes.
     */
    if ((conn->type == RX_CLIENT_CONNECTION) &&
	(conn->flags & RX_CONN_MAKECALL_WAITING)) {
	conn->flags |= RX_CONN_DESTROY_ME;
	USERPRI;
	return;
    }
    /* Check for extant references to this connection */
    for (i = 0; i < RX_MAXCALLS; i++) {
	struct rx_call *call = conn->call[i];

	if (call) {
	    havecalls = 1;
	    if (conn->type == RX_CLIENT_CONNECTION) {
		if (call->delayedAckEvent) {
		    /*
		     * Push the final acknowledgment out now--there
                     * won't be a subsequent call to acknowledge the
                     * last reply packets
		     */
		    rxevent_Cancel(call->delayedAckEvent);
		    rxi_AckAll((struct rxevent *) 0, call, 0);
		}
	    }
	}
    }

    if (havecalls) {
	/*
	 * Don't destroy the connection if there are any call
         * structures still in use
	 */
	conn->flags |= RX_CONN_DESTROY_ME;
	USERPRI;
	return;
    }
    /* Remove from connection hash table before proceeding */
    conn_ptr = &rx_connHashTable[CONN_HASH(peer->host, peer->port, conn->cid,
					   conn->epoch, conn->type)];
    for (; *conn_ptr; conn_ptr = &(*conn_ptr)->next) {
	if (*conn_ptr == conn) {
	    *conn_ptr = conn->next;
	    break;
	}
    }

    /*
     * Notify the service exporter, if requested, that this connection
     * is being destroyed
     */
    if (conn->type == RX_SERVER_CONNECTION && conn->service->destroyConnProc)
	(*conn->service->destroyConnProc) (conn);

    /* Notify the security module that this connection is being destroyed */
    RXS_DestroyConnection(conn->securityObject, conn);

    /* Make sure that the connection is completely reset before deleting it. */
    rxi_ResetConnection(conn);

    queue_Remove(conn);
    if (--conn->peer->refCount == 0)
	conn->peer->idleWhen = clock_Sec();

    if (conn->type == RX_SERVER_CONNECTION)
	rx_stats.nServerConns--;
    else
	rx_stats.nClientConns--;


    RX_MUTEX_DESTROY(&conn->lock);
    rxi_FreeConnection(conn);
    USERPRI;
}

/*
 * Start a new rx remote procedure call, on the specified connection.
 * If wait is set to 1, wait for a free call channel; otherwise return
 * 0.  Maxtime gives the maximum number of seconds this call may take,
 * after rx_MakeCall returns.  After this time interval, a call to any
 * of rx_SendData, rx_ReadData, etc. will fail with RX_CALL_TIMEOUT.
 */
struct rx_call *
rx_NewCall(struct rx_connection * conn)
{
    int i;
    struct rx_call *call;

    SPLVAR;
#if defined(AFS_SGIMP_ENV)
    GLOCKSTATE ms;

#endif
    clock_NewTime();
    dpf(("rx_NewCall(conn %x)\n", conn));

    GLOBAL_LOCK();
#if defined(AFS_SGIMP_ENV)
    AFS_GRELEASE(&ms);
#endif
    NETPRI;
    for (;;) {
	for (i = 0; i < RX_MAXCALLS; i++) {
	    call = conn->call[i];
	    if (call) {
		if (call->state == RX_STATE_DALLY) {
		    RX_MUTEX_ENTER(&call->lock);
		    rxi_ResetCall(call);
		    (*call->callNumber)++;
		    break;
		}
	    } else {
		call = rxi_NewCall(conn, i);
		RX_MUTEX_ENTER(&call->lock);
		break;
	    }
	}
	if (i < RX_MAXCALLS) {
	    break;
	}
	conn->flags |= RX_CONN_MAKECALL_WAITING;
#ifdef	RX_ENABLE_LOCKS
	cv_wait(&conn->cv, &conn->lock);
#else
	osi_rxSleep(conn);
#endif
    }

    /* Client is initially in send mode */
    call->state = RX_STATE_ACTIVE;
    call->mode = RX_MODE_SENDING;

    /* remember start time for call in case we have hard dead time limit */
    call->startTime = clock_Sec();

    /* Turn on busy protocol. */
    rxi_KeepAliveOn(call);

    RX_MUTEX_EXIT(&call->lock);
    USERPRI;
#if defined(AFS_SGIMP_ENV)
    AFS_GACQUIRE(&ms);
#endif
    GLOBAL_UNLOCK();
    return call;
}

static int
rxi_HasActiveCalls(struct rx_connection * aconn)
{
    int i;
    struct rx_call *tcall;

    SPLVAR;

    NETPRI;
    for (i = 0; i < RX_MAXCALLS; i++) {
	if ((tcall = aconn->call[i]) != NULL) {
	    if ((tcall->state == RX_STATE_ACTIVE)
		|| (tcall->state == RX_STATE_PRECALL)) {
		USERPRI;
		return 1;
	    }
	}
    }
    USERPRI;
    return 0;
}

int
rxi_GetCallNumberVector(const struct rx_connection * aconn,
			int32_t *alongs)
{
    int i;
    struct rx_call *tcall;

    SPLVAR;

    NETPRI;
    for (i = 0; i < RX_MAXCALLS; i++) {
	if ((tcall = aconn->call[i]) && (tcall->state == RX_STATE_DALLY))
	    alongs[i] = aconn->callNumber[i] + 1;
	else
	    alongs[i] = aconn->callNumber[i];
    }
    USERPRI;
    return 0;
}

int
rxi_SetCallNumberVector(struct rx_connection * aconn,
			int32_t *alongs)
{
    int i;
    struct rx_call *tcall;

    SPLVAR;

    NETPRI;
    for (i = 0; i < RX_MAXCALLS; i++) {
	if ((tcall = aconn->call[i]) && (tcall->state == RX_STATE_DALLY))
	    aconn->callNumber[i] = alongs[i] - 1;
	else
	    aconn->callNumber[i] = alongs[i];
    }
    USERPRI;
    return 0;
}

/*
 * Advertise a new service.  A service is named locally by a UDP port
 * number plus a 16-bit service id.  Returns (struct rx_service *) 0
 * on a failure.
 */
struct rx_service *
rx_NewService(uint16_t port, uint16_t serviceId, char *serviceName,
	      struct rx_securityClass ** securityObjects,
	      int nSecurityObjects,
	      int32_t (*serviceProc) (struct rx_call *))
{
    osi_socket socket = OSI_NULLSOCKET;
    struct rx_securityClass **sersec;
    struct rx_service *tservice;
    int reuse = 0;
    int i;

    SPLVAR;

    clock_NewTime();

    if (serviceId == 0) {
	osi_Msg(("rx_NewService:  service id for service %s is not"
		 " non-zero.\n", serviceName));
	return 0;
    }
    if (port == 0) {
	if (rx_port == 0) {
	    osi_Msg(("rx_NewService: A non-zero port must be specified on "
		     "this call if a non-zero port was not provided at Rx "
		     "initialization (service %s).\n", serviceName));
	    return 0;
	}
	port = rx_port;
	socket = rx_socket;
    }
    sersec = (void *)rxi_Alloc(sizeof(*sersec) * nSecurityObjects);
    for (i = 0; i < nSecurityObjects; i++)
	sersec[i] = securityObjects[i];
    tservice = rxi_AllocService();
    NETPRI;
    for (i = 0; i < RX_MAX_SERVICES; i++) {
	struct rx_service *service = rx_services[i];

	if (service) {
	    if (port == service->servicePort) {
		if (service->serviceId == serviceId) {
		    /*
		     * The identical service has already been
                     * installed; if the caller was intending to
                     * change the security classes used by this
                     * service, he/she loses.
		     */
		    osi_Msg(("rx_NewService: tried to install service %s "
			     "with service id %d, which is already in use "
			     "for service %s\n", serviceName, serviceId,
			     service->serviceName));
		    USERPRI;
		    rxi_FreeService(tservice);
		    return service;
		}
		/*
		 * Different service, same port: re-use the socket which is
		 * bound to the same port
		 */
		socket = service->socket;
		reuse = 1;
	    }
	} else {
	    if (socket == OSI_NULLSOCKET) {

		/*
		 * If we don't already have a socket (from another service on
		 * same port) get a new one
		 */
		socket = rxi_GetUDPSocket(port, NULL);
		if (socket == OSI_NULLSOCKET) {
		    USERPRI;
		    rxi_FreeService(tservice);
		    return 0;
		}
	    }
	    service = tservice;
	    service->socket = socket;
	    service->servicePort = port;
	    service->serviceId = serviceId;
	    service->serviceName = serviceName;
	    service->nSecurityObjects = nSecurityObjects;
	    service->securityObjects = sersec;
	    service->minProcs = 0;
	    service->maxProcs = 1;
	    service->idleDeadTime = 60;
	    service->connDeadTime = rx_connDeadTime;
	    service->executeRequestProc = serviceProc;
	    rx_services[i] = service;  /* not visible until now */
	    for (i = 0; i < nSecurityObjects; i++) {
		if (securityObjects[i])
		    RXS_NewService(securityObjects[i], service, reuse);
	    }
	    USERPRI;
	    return service;
	}
    }
    USERPRI;
    rxi_FreeService(tservice);
    osi_Msg(("rx_NewService: cannot support > %d services\n",
	     RX_MAX_SERVICES));
    return 0;
}

/*
 * This is the server process's request loop.  This routine should
 * either be each server proc's entry point, or it should be called by
 * the server process (depending upon the rx implementation
 * environment).
 */
void
rx_ServerProc(void)
{
    struct rx_call *call;
    int32_t code;
    struct rx_service *tservice;

#if defined(AFS_SGIMP_ENV)
    SPLVAR;
    GLOCKSTATE ms;

    AFS_GRELEASE(&ms);
    NETPRI;
#endif

    rxi_dataQuota += rx_Window + 2;    /*
				        * reserve a window of packets for
				        * hard times
				        */
    rxi_MorePackets(rx_Window + 2);    /* alloc more packets, too */
    rxi_availProcs++;		       /*
				        * one more thread handling incoming
				        * calls
				        */
#if defined(AFS_SGIMP_ENV)
    USERPRI;
#endif
    for (;;) {

	call = rx_GetCall();
#ifdef	KERNEL
	if (afs_termState == AFSOP_STOP_RXCALLBACK) {
	    RX_MUTEX_ENTER(&afs_termStateLock);
	    afs_termState = AFSOP_STOP_AFS;
#ifdef	RX_ENABLE_LOCKS
	    cv_signal(&afs_termStateCv);
#else
	    osi_rxWakeup(&afs_termState);
#endif
	    RX_MUTEX_EXIT(&afs_termStateLock);
	    return;
	}
#endif
	tservice = call->conn->service;
#if defined(AFS_SGIMP_ENV)
	AFS_GLOCK();
#endif
	if (tservice->beforeProc)
	    (*tservice->beforeProc) (call);

	code = call->conn->service->executeRequestProc(call);

	if (tservice->afterProc)
	    (*tservice->afterProc) (call, code);

	rx_EndCall(call, code);
	rxi_nCalls++;
#if defined(AFS_SGIMP_ENV)
	AFS_GUNLOCK();
#endif
    }
}


/*
 * Sleep until a call arrives.  Returns a pointer to the call, ready
 * for an rx_Read.
 */
struct rx_call *
rx_GetCall(void)
{
    struct rx_serverQueueEntry *sq;
    struct rx_call *call = (struct rx_call *) 0;
    struct rx_service *service = NULL;

    SPLVAR;

    GLOBAL_LOCK();
    RX_MUTEX_ENTER(&freeSQEList_lock);

    if ((sq = rx_FreeSQEList) != NULL) {
	rx_FreeSQEList = *(struct rx_serverQueueEntry **) sq;
	RX_MUTEX_EXIT(&freeSQEList_lock);
    } else {			       /* otherwise allocate a new one and
				        * return that */
	RX_MUTEX_EXIT(&freeSQEList_lock);
	sq = (struct rx_serverQueueEntry *) rxi_Alloc(sizeof(struct rx_serverQueueEntry));
#ifdef	RX_ENABLE_LOCKS
	LOCK_INIT(&sq->lock, "server Queue lock");
#endif
    }
    RX_MUTEX_ENTER(&sq->lock);
#if defined(AFS_SGIMP_ENV)
    ASSERT(!isafs_glock());
#endif
    NETPRI;

    if (queue_IsNotEmpty(&rx_incomingCallQueue)) {
	struct rx_call *tcall, *ncall;

	/*
	 * Scan for eligible incoming calls.  A call is not eligible
         * if the maximum number of calls for its service type are
         * already executing
	 */
	for (queue_Scan(&rx_incomingCallQueue, tcall, ncall, rx_call)) {
	    service = tcall->conn->service;
	    if (QuotaOK(service)) {
		call = tcall;
		break;
	    }
	}
    }
    if (call) {
	queue_Remove(call);
	call->flags &= (~RX_CALL_WAIT_PROC);
	service->nRequestsRunning++;
	/*
	 * just started call in minProcs pool, need fewer to maintain
         * guarantee
	 */
	if (service->nRequestsRunning <= service->minProcs)
	    rxi_minDeficit--;
	rxi_availProcs--;
	rx_nWaiting--;
    } else {
	/*
	 * If there are no eligible incoming calls, add this process
         * to the idle server queue, to wait for one
	 */
	sq->newcall = 0;
	queue_Append(&rx_idleServerQueue, sq);
	rx_waitForPacket = sq;
	do {
#ifdef	RX_ENABLE_LOCKS
	    cv_wait(&sq->cv, &sq->lock);
#else
	    osi_rxSleep(sq);
#endif
#ifdef	KERNEL
	    if (afs_termState == AFSOP_STOP_RXCALLBACK) {
		GLOBAL_UNLOCK();
		USERPRI;
		return (struct rx_call *) 0;
	    }
#endif
	} while (!(call = sq->newcall));
    }
    RX_MUTEX_EXIT(&sq->lock);

    RX_MUTEX_ENTER(&freeSQEList_lock);
    *(struct rx_serverQueueEntry **) sq = rx_FreeSQEList;
    rx_FreeSQEList = sq;
    RX_MUTEX_EXIT(&freeSQEList_lock);

    call->state = RX_STATE_ACTIVE;
    call->mode = RX_MODE_RECEIVING;
    if (queue_IsEmpty(&call->rq)) {
	/* we can't schedule a call if there's no data!!! */
	rxi_SendAck(call, 0, 0, 0, 0, RX_ACK_DELAY);
    }
    rxi_calltrace(RX_CALL_START, call);
    dpf(("rx_GetCall(port=%d, service=%d) ==> call %x\n",
	 call->conn->service->servicePort,
	 call->conn->service->serviceId, call));

    GLOBAL_UNLOCK();
    USERPRI;

    return call;
}



/*
 * Establish a procedure to be called when a packet arrives for a
 * call.  This routine will be called at most once after each call,
 * and will also be called if there is an error condition on the or
 * the call is complete.  Used by multi rx to build a selection
 * function which determines which of several calls is likely to be a
 * good one to read from.
 * NOTE: the way this is currently implemented it is probably only a
 * good idea to (1) use it immediately after a newcall (clients only)
 * and (2) only use it once.  Other uses currently void your warranty
 */
void
rx_SetArrivalProc(struct rx_call * call, void (*proc) (),
		   void *handle, void *arg)
{
    call->arrivalProc = proc;
    call->arrivalProcHandle = handle;
    call->arrivalProcArg = arg;
}

/*
 * Call is finished (possibly prematurely).  Return rc to the peer, if
 * appropriate, and return the final error code from the conversation
 * to the caller
 */
int32_t
rx_EndCall(struct rx_call * call, int32_t rc)
{
    struct rx_connection *conn = call->conn;
    struct rx_service *service;
    int32_t error;

    SPLVAR;
#if defined(AFS_SGIMP_ENV)
    GLOCKSTATE ms;

#endif

    dpf(("rx_EndCall(call %x)\n", call));

#if defined(AFS_SGIMP_ENV)
    AFS_GRELEASE(&ms);
#endif
    NETPRI;
    GLOBAL_LOCK();
    RX_MUTEX_ENTER(&call->lock);

    call->arrivalProc = (void (*) ()) 0;
    if (rc && call->error == 0) {
	rxi_CallError(call, rc);
	/*
	 * Send an abort message to the peer if this error code has
         * only just been set.  If it was set previously, assume the
         * peer has already been sent the error code or will request it
         */
	rxi_SendCallAbort(call, (struct rx_packet *) 0);
    }
    if (conn->type == RX_SERVER_CONNECTION) {
	/* Make sure reply or at least dummy reply is sent */
	if (call->mode == RX_MODE_RECEIVING) {
	    RX_MUTEX_EXIT(&call->lock);
	    GLOBAL_UNLOCK();

	    rx_Write(call, 0, 0);

	    GLOBAL_LOCK();
	    RX_MUTEX_ENTER(&call->lock);
	}
	if (call->mode == RX_MODE_SENDING) {
	    rx_FlushWrite(call);
	}
	service = conn->service;
	service->nRequestsRunning--;
	if (service->nRequestsRunning < service->minProcs)
	    rxi_minDeficit++;
	rxi_availProcs++;
	rxi_calltrace(RX_CALL_END, call);
    } else {			       /* Client connection */
	char dummy;

	/*
	 * Make sure server receives input packets, in the case where
         * no reply arguments are expected
	 */
	if (call->mode == RX_MODE_SENDING) {
	    RX_MUTEX_EXIT(&call->lock);
	    GLOBAL_UNLOCK();

	    (void) rx_Read(call, &dummy, 1);

	    GLOBAL_LOCK();
	    RX_MUTEX_ENTER(&call->lock);
	}
	if (conn->flags & RX_CONN_MAKECALL_WAITING) {
	    conn->flags &= (~RX_CONN_MAKECALL_WAITING);
#ifdef	RX_ENABLE_LOCKS
	    cv_signal(&conn->cv);
#else
	    osi_rxWakeup(conn);
#endif
	}
    }
    call->state = RX_STATE_DALLY;
    error = call->error;

    /*
     * currentPacket, nLeft, and NFree must be zeroed here, because
     * ResetCall cannot: ResetCall may be called at splnet(), in the
     * kernel version, and may interrupt the macros rx_Read or
     * rx_Write, which run at normal priority for efficiency.
     */
    if (call->currentPacket) {
	rxi_FreePacket(call->currentPacket);
	call->currentPacket = (struct rx_packet *) 0;
	call->nLeft = call->nFree = 0;
    } else
	call->nLeft = call->nFree = 0;


    RX_MUTEX_EXIT(&call->lock);
    GLOBAL_UNLOCK();
    USERPRI;
#if defined(AFS_SGIMP_ENV)
    AFS_GACQUIRE(&ms);
#endif
    /*
     * Map errors to the local host's errno.h format.
     */
    error = ntoh_syserr_conv(error);
    return error;
}

/*
 * Call this routine when shutting down a server or client (especially
 * clients).  This will allow Rx to gracefully garbage collect server
 * connections, and reduce the number of retries that a server might
 * make to a dead client
 */
void
rx_Finalize(void)
{
    struct rx_connection **conn_ptr, **conn_end;

    SPLVAR;
    NETPRI;
    if (rx_connHashTable)
	for (conn_ptr = &rx_connHashTable[0],
	     conn_end = &rx_connHashTable[rx_hashTableSize];
	     conn_ptr < conn_end; conn_ptr++) {
	    struct rx_connection *conn, *next;

	    for (conn = *conn_ptr; conn; conn = next) {
		next = conn->next;
		if (conn->type == RX_CLIENT_CONNECTION)
		    rx_DestroyConnection(conn);
	    }
	}

#ifdef RXDEBUG
    if (rx_debugFile) {
	fclose(rx_debugFile);
	rx_debugFile = NULL;
    }
#endif
    rxi_flushtrace();
    USERPRI;
}

/*
 * if we wakeup packet waiter too often, can get in loop with two
 * AllocSendPackets each waking each other up (from ReclaimPacket calls)
 */
void
rxi_PacketsUnWait(void)
{

    RX_MUTEX_ENTER(&rx_waitingForPackets_lock);
    if (!rx_waitingForPackets) {
	RX_MUTEX_EXIT(&rx_waitingForPackets_lock);
	return;
    }
    if (rxi_OverQuota(RX_PACKET_CLASS_SEND)) {
	RX_MUTEX_EXIT(&rx_waitingForPackets_lock);
	return;			       /* still over quota */
    }
    rx_waitingForPackets = 0;
#ifdef	RX_ENABLE_LOCKS
    cv_signal(&rx_waitingForPackets_cv);
#else
    osi_rxWakeup(&rx_waitingForPackets);
#endif
    RX_MUTEX_EXIT(&rx_waitingForPackets_lock);
    return;
}


/* ------------------Internal interfaces------------------------- */

/*
 * Return this process's service structure for the
 * specified socket and service
 */
static struct rx_service *
rxi_FindService(osi_socket socket, uint16_t serviceId)
{
    struct rx_service **sp;

    for (sp = &rx_services[0]; *sp; sp++) {
	if ((*sp)->serviceId == serviceId && (*sp)->socket == socket)
	    return *sp;
    }
    return 0;
}

/*
 * Allocate a call structure, for the indicated channel of the
 * supplied connection.  The mode and state of the call must be set by
 * the caller.
 */
struct rx_call *
rxi_NewCall(struct rx_connection * conn, int channel)
{
    struct rx_call *call;

    /*
     * Grab an existing call structure, or allocate a new one.
     * Existing call structures are assumed to have been left reset by
     * rxi_FreeCall
     */
    RX_MUTEX_ENTER(&rx_freeCallQueue_lock);

    if (queue_IsNotEmpty(&rx_freeCallQueue)) {
	call = queue_First(&rx_freeCallQueue, rx_call);
	queue_Remove(call);
	rx_stats.nFreeCallStructs--;
	RX_MUTEX_EXIT(&rx_freeCallQueue_lock);
	RX_MUTEX_ENTER(&call->lock);

#ifdef	ADAPT_PERF
	/* Bind the call to its connection structure */
	call->conn = conn;
#endif
    } else {
	call = (struct rx_call *) rxi_Alloc(sizeof(struct rx_call));

	RX_MUTEX_EXIT(&rx_freeCallQueue_lock);
	RX_MUTEX_INIT(&call->lock, "call lock", RX_MUTEX_DEFAULT, NULL);
	RX_MUTEX_INIT(&call->lockw, "call wlock", RX_MUTEX_DEFAULT, NULL);
	RX_MUTEX_ENTER(&call->lock);

	rx_stats.nCallStructs++;
	/* Initialize once-only items */
	queue_Init(&call->tq);
	queue_Init(&call->rq);
#ifdef	ADAPT_PERF
	/* Bind the call to its connection structure (prereq for reset) */
	call->conn = conn;
#endif
	rxi_ResetCall(call);
    }
#ifndef	ADAPT_PERF
    /* Bind the call to its connection structure (prereq for reset) */
    call->conn = conn;
#endif
    call->channel = channel;
    call->callNumber = &conn->callNumber[channel];
    /*
     * Note that the next expected call number is retained (in
     * conn->callNumber[i]), even if we reallocate the call structure
     */
    conn->call[channel] = call;
    /*
     * if the channel's never been used (== 0), we should start at 1, otherwise
     * the call number is valid from the last time this channel was used
     */
    if (*call->callNumber == 0)
	*call->callNumber = 1;

    RX_MUTEX_EXIT(&call->lock);
    return call;
}

/*
 * A call has been inactive long enough that so we can throw away
 * state, including the call structure, which is placed on the call
 * free list.
 */
void
rxi_FreeCall(struct rx_call * call)
{
    int channel = call->channel;
    struct rx_connection *conn = call->conn;


    RX_MUTEX_ENTER(&call->lock);

    if (call->state == RX_STATE_DALLY)
	(*call->callNumber)++;
    rxi_ResetCall(call);
    call->conn->call[channel] = (struct rx_call *) 0;

    RX_MUTEX_ENTER(&rx_freeCallQueue_lock);

    queue_Append(&rx_freeCallQueue, call);
    rx_stats.nFreeCallStructs++;

    RX_MUTEX_EXIT(&rx_freeCallQueue_lock);
    RX_MUTEX_EXIT(&call->lock);

    /*
     * Destroy the connection if it was previously slated for
     * destruction, i.e. the Rx client code previously called
     * rx_DestroyConnection (client connections), or
     * rxi_ReapConnections called the same routine (server
     * connections).  Only do this, however, if there are no
     * outstanding calls
     */
    if (conn->flags & RX_CONN_DESTROY_ME) {
#if 0
	conn->refCount++;
#endif
	rx_DestroyConnection(conn);
    }
}


long rxi_Alloccnt = 0, rxi_Allocsize = 0;

void *
rxi_Alloc(int size)
{
    void *p;

    rxi_Alloccnt++;
    rxi_Allocsize += size;
    p = osi_Alloc(size);
    if (!p)
	osi_Panic("rxi_Alloc error");
    memset(p, 0, size);
    return p;
}

void
rxi_Free(void *addr, int size)
{
    rxi_Alloccnt--;
    rxi_Allocsize -= size;
#if	(defined(AFS_AIX32_ENV) || defined(AFS_HPUX_ENV)) && defined(KERNEL)
    osi_FreeSmall(addr);
#else
    osi_Free(addr, size);
#endif
}

/*
 * Find the peer process represented by the supplied (host,port)
 * combination.  If there is no appropriate active peer structure, a
 * new one will be allocated and initialized
 */
struct rx_peer *
rxi_FindPeer(uint32_t host, uint16_t port)
{
    struct rx_peer *pp;
    int hashIndex;

    hashIndex = PEER_HASH(host, port);
    for (pp = rx_peerHashTable[hashIndex]; pp; pp = pp->next) {
	if ((pp->host == host) && (pp->port == port))
	    break;
    }
    if (!pp) {
	pp = rxi_AllocPeer();	       /*
					* This bzero's *pp: anything not
				        * explicitly
					*/
	pp->host = host;	       /*
					* set here or in InitPeerParams is
				        * zero
					*/
	pp->port = port;
	queue_Init(&pp->congestionQueue);
	queue_Init(&pp->connQueue);
	pp->next = rx_peerHashTable[hashIndex];
	rx_peerHashTable[hashIndex] = pp;
	rxi_InitPeerParams(pp);
	rx_stats.nPeerStructs++;
    }
    pp->refCount++;
    return pp;
}

/*
 * Remove `peer' from the hash table.
 */

static void
rxi_RemovePeer(struct rx_peer *peer)
{
    struct rx_peer **peer_ptr;

    for (peer_ptr = &rx_peerHashTable[PEER_HASH(peer->host, peer->port)];
	 *peer_ptr; peer_ptr = &(*peer_ptr)->next) {
	if (*peer_ptr == peer) {
	    *peer_ptr = peer->next;
	    return;
	}
    }
}

/*
 * Destroy the specified peer structure, removing it from the peer hash
 * table
 */

static void
rxi_DestroyPeer(struct rx_peer * peer)
{
    rxi_RemovePeer(peer);
    assert(queue_IsEmpty(&peer->connQueue));
    rxi_FreePeer(peer);
    rx_stats.nPeerStructs--;
}

/*
 * Add `peer' to the hash table.
 */

static struct rx_peer *
rxi_InsertPeer(struct rx_peer *peer)
{
    struct rx_peer *pp;
    int hashIndex;

    hashIndex = PEER_HASH(peer->host, peer->port);
    for (pp = rx_peerHashTable[hashIndex]; pp; pp = pp->next) {
	if ((pp->host == peer->host) && (pp->port == peer->port))
	    break;
    }
    if (pp != NULL) {
	struct rx_connection *conn, *next;

	pp->refCount  += peer->refCount;
	pp->nSent     += peer->nSent;
	pp->reSends   += peer->reSends;

	for (queue_Scan(&peer->connQueue, conn, next, rx_connection)) {
	    conn->peer = pp;
	    queue_Remove(conn);
	    queue_Append(&pp->connQueue, conn);
	}

	assert(queue_IsEmpty(&peer->connQueue));
	rxi_FreePeer(peer);
	rx_stats.nPeerStructs--;
	return pp;
    } else {
	peer->next = rx_peerHashTable[hashIndex];
	rx_peerHashTable[hashIndex] = peer;
	return peer;
    }
}

/*
 * Change the key of a given peer
 */

static struct rx_peer *
rxi_ChangePeer(struct rx_peer *peer, uint32_t host, uint16_t port)
{
    rxi_RemovePeer(peer);
    peer->host = host;
    peer->port = port;
    return rxi_InsertPeer(peer);
}

/*
 * Find the connection at (host, port) started at epoch, and with the
 * given connection id.  Creates the server connection if necessary.
 * The type specifies whether a client connection or a server
 * connection is desired.  In both cases, (host, port) specify the
 * peer's (host, pair) pair.  Client connections are not made
 * automatically by this routine.  The parameter socket gives the
 * socket descriptor on which the packet was received.  This is used,
 * in the case of server connections, to check that *new* connections
 * come via a valid (port, serviceId).  Finally, the securityIndex
 * parameter must match the existing index for the connection.  If a
 * server connection is created, it will be created using the supplied
 * index, if the index is valid for this service
 */
static struct rx_connection *
rxi_FindConnection(osi_socket socket, uint32_t host,
		   uint16_t port, uint16_t serviceId, uint32_t cid,
		   uint32_t epoch, int type, u_int securityIndex)
{
    int hashindex;
    struct rx_connection *conn;
    struct rx_peer *pp = NULL;

    hashindex = CONN_HASH(host, port, cid, epoch, type);
    for (conn = rx_connHashTable[hashindex]; conn; conn = conn->next) {
	if ((conn->type == type) && ((cid & RX_CIDMASK) == conn->cid) &&
	    (epoch == conn->epoch) &&
	    (securityIndex == conn->securityIndex)) {
	    pp = conn->peer;

	    if (type == RX_CLIENT_CONNECTION || pp->host == host)
		break;
	}
    }
    if (conn != NULL) {
	if (pp->host != host || pp->port != port)
	    conn->peer = rxi_ChangePeer (pp, host, port);
    } else {
	struct rx_service *service;

	if (type == RX_CLIENT_CONNECTION)
	    return (struct rx_connection *) 0;
	service = rxi_FindService(socket, serviceId);
	if (!service || (securityIndex >= service->nSecurityObjects)
	    || (service->securityObjects[securityIndex] == 0))
	    return (struct rx_connection *) 0;
	conn = rxi_AllocConnection();  /* This bzero's the connection */
#ifdef	RX_ENABLE_LOCKS
	LOCK_INIT(&conn->lock, "conn lock");
#endif
	conn->next = rx_connHashTable[hashindex];
	rx_connHashTable[hashindex] = conn;
	conn->peer = rxi_FindPeer(host, port);
	queue_Append(&conn->peer->connQueue, conn);
	conn->maxPacketSize = MIN(conn->peer->packetSize, OLD_MAX_PACKET_SIZE);
	conn->type = RX_SERVER_CONNECTION;
	conn->lastSendTime = clock_Sec();	/* don't GC immediately */
	conn->epoch = epoch;
	conn->cid = cid & RX_CIDMASK;
	/* conn->serial = conn->lastSerial = 0; */
	/* conn->rock = 0; */
	/* conn->timeout = 0; */
	conn->service = service;
	conn->serviceId = serviceId;
	conn->securityIndex = securityIndex;
	conn->securityObject = service->securityObjects[securityIndex];
	rx_SetConnDeadTime(conn, service->connDeadTime);
	/* Notify security object of the new connection */
	RXS_NewConnection(conn->securityObject, conn);
	/* XXXX Connection timeout? */
	if (service->newConnProc)
	    (*service->newConnProc) (conn);
	rx_stats.nServerConns++;
    }
    conn->refCount++;
    return conn;
}

/*
 * There are two packet tracing routines available for testing and monitoring
 * Rx.  One is called just after every packet is received and the other is
 * called just before every packet is sent.  Received packets, have had their
 * headers decoded, and packets to be sent have not yet had their headers
 * encoded.  Both take two parameters: a pointer to the packet and a sockaddr
 * containing the network address.  Both can be modified.  The return value, if
 * non-zero, indicates that the packet should be dropped.
 */

int (*rx_justReceived)() = NULL;
int (*rx_almostSent)() = NULL;

/*
 * A packet has been received off the interface.  Np is the packet, socket is
 * the socket number it was received from (useful in determining which service
 * this packet corresponds to), and (host, port) reflect the host,port of the
 * sender.  This call returns the packet to the caller if it is finished with
 * it, rather than de-allocating it, just as a small performance hack
 */

struct rx_packet *
rxi_ReceivePacket(struct rx_packet * np, osi_socket socket,
		  uint32_t host, uint16_t port)
{
    struct rx_call *call;
    struct rx_connection *conn;
    int channel;
    unsigned long currentCallNumber;
    int type;
    int skew;

#ifdef RXDEBUG
    char *packetType;

#endif
    struct rx_packet *tnp;

#ifdef RXDEBUG
/*
 * We don't print out the packet until now because (1) the time may not be
 * accurate enough until now in the lwp implementation (rx_Listener only gets
 * the time after the packet is read) and (2) from a protocol point of view,
 * this is the first time the packet has been seen
 */
    packetType = (np->header.type > 0 && np->header.type < RX_N_PACKET_TYPES)
	? rx_packetTypes[np->header.type - 1] : "*UNKNOWN*";
    dpf(("R %d %s: %x.%d.%d.%d.%d.%d.%d flags %d, packet %x",
	 np->header.serial, packetType, host, port, np->header.serviceId,
	 (int)np->header.epoch, np->header.cid, np->header.callNumber,
	 np->header.seq, np->header.flags, np));
#endif

    if (np->header.type == RX_PACKET_TYPE_VERSION)
	return rxi_ReceiveVersionPacket(np, socket, host, port);

    if (np->header.type == RX_PACKET_TYPE_DEBUG)
	return rxi_ReceiveDebugPacket(np, socket, host, port);

#ifdef RXDEBUG

    /*
     * If an input tracer function is defined, call it with the packet and
     * network address.  Note this function may modify its arguments.
     */
    if (rx_justReceived) {
	struct sockaddr_in addr;
	int drop;

	addr.sin_family = AF_INET;
	addr.sin_port = port;
	addr.sin_addr.s_addr = host;
	drop = (*rx_justReceived) (np, &addr);
	/* drop packet if return value is non-zero */
	if (drop)
	    return np;
	port = addr.sin_port;	       /* in case fcn changed addr */
	host = addr.sin_addr.s_addr;
    }
#endif

    /* If packet was not sent by the client, then *we* must be the client */
    type = ((np->header.flags & RX_CLIENT_INITIATED) != RX_CLIENT_INITIATED)
	? RX_CLIENT_CONNECTION : RX_SERVER_CONNECTION;

    /*
     * Find the connection (or fabricate one, if we're the server & if
     * necessary) associated with this packet
     */
    conn = rxi_FindConnection(socket, host, port, np->header.serviceId,
			      np->header.cid, np->header.epoch, type,
			      np->header.securityIndex);

    if (!conn) {
	/*
         * If no connection found or fabricated, just ignore the packet.
         * (An argument could be made for sending an abort packet for
         * the conn)
         */
	return np;
    }
    /* compute the max serial number seen on this connection */
    if (conn->maxSerial < np->header.serial)
	conn->maxSerial = np->header.serial;

    /*
     * If the connection is in an error state, send an abort packet and
     * ignore the incoming packet
     */
    if (conn->error) {
	rxi_ConnectionError (conn, conn->error);
	/* Don't respond to an abort packet--we don't want loops! */
	if (np->header.type != RX_PACKET_TYPE_ABORT)
	    np = rxi_SendConnectionAbort(conn, np);
	conn->refCount--;
	return np;
    }
    /* Check for connection-only requests (i.e. not call specific). */
    if (np->header.callNumber == 0) {
	switch (np->header.type) {
	case RX_PACKET_TYPE_ABORT:
	    /* What if the supplied error is zero? */
	    rxi_ConnectionError(conn, ntohl(rx_SlowGetLong(np, 0)));
	    conn->refCount--;
	    return np;
	case RX_PACKET_TYPE_CHALLENGE:
	    tnp = rxi_ReceiveChallengePacket(conn, np);
	    conn->refCount--;
	    return tnp;
	case RX_PACKET_TYPE_RESPONSE:
	    tnp = rxi_ReceiveResponsePacket(conn, np);
	    conn->refCount--;
	    return tnp;
	case RX_PACKET_TYPE_PARAMS:
	case RX_PACKET_TYPE_PARAMS + 1:
	case RX_PACKET_TYPE_PARAMS + 2:
	    /* ignore these packet types for now */
	    conn->refCount--;
	    return np;


	default:
	    /*
	     * Should not reach here, unless the peer is broken: send an
             * abort packet
	     */
	    rxi_ConnectionError(conn, RX_PROTOCOL_ERROR);
	    tnp = rxi_SendConnectionAbort(conn, np);
	    conn->refCount--;
	    return tnp;
	}
    }
    channel = np->header.cid & RX_CHANNELMASK;
    call = conn->call[channel];
#ifdef	RX_ENABLE_LOCKSX
    if (call)
	mutex_enter(&call->lock);
#endif
    currentCallNumber = conn->callNumber[channel];

    if (type == RX_SERVER_CONNECTION) {/* We're the server */
	if (np->header.callNumber < currentCallNumber) {
	    rx_stats.spuriousPacketsRead++;
#ifdef	RX_ENABLE_LOCKSX
	    if (call)
		mutex_exit(&call->lock);
#endif
	    conn->refCount--;
	    return np;
	}
	if (!call) {
	    call = rxi_NewCall(conn, channel);
#ifdef	RX_ENABLE_LOCKSX
	    mutex_enter(&call->lock);
#endif
	    *call->callNumber = np->header.callNumber;
	    call->state = RX_STATE_PRECALL;
	    rxi_KeepAliveOn(call);
	} else if (np->header.callNumber != currentCallNumber) {
	    /*
	     * If the new call cannot be taken right now send a busy and set
             * the error condition in this call, so that it terminates as
             * quickly as possible
	     */
	    if (call->state == RX_STATE_ACTIVE) {
		struct rx_packet *tp;

		rxi_CallError(call, RX_CALL_DEAD);
		tp = rxi_SendSpecial(call, conn, np, RX_PACKET_TYPE_BUSY, (char *) 0, 0);
#ifdef	RX_ENABLE_LOCKSX
		mutex_exit(&call->lock);
#endif
		conn->refCount--;
		return tp;
	    }
	    /*
	     * If the new call can be taken right now (it's not busy) then
             * accept it.
	     */
	    rxi_ResetCall(call);
	    *call->callNumber = np->header.callNumber;
	    call->state = RX_STATE_PRECALL;
	    rxi_KeepAliveOn(call);
	} else {
	    /* Continuing call; do nothing here. */
	}
    } else {			       /* we're the client */

	/*
	 * Ignore anything that's not relevant to the current call.  If there
	 * isn't a current call, then no packet is relevant.
	 */
	if (!call || (np->header.callNumber != currentCallNumber)) {
	    rx_stats.spuriousPacketsRead++;
#ifdef	RX_ENABLE_LOCKSX
	    if (call)
		mutex_exit(&call->lock);
#endif
	    conn->refCount--;
	    return np;
	}
	/*
	 * If the service security object index stamped in the packet does not
         * match the connection's security index, ignore the packet
	 */
	if (np->header.securityIndex != conn->securityIndex) {
	    conn->refCount--;
#ifdef	RX_ENABLE_LOCKSX
	    mutex_exit(&call->lock);
#endif
	    return np;
	}
	/*
	 * If we're receiving the response, then all transmit packets are
         * implicitly acknowledged.  Get rid of them.
	 */
	if (np->header.type == RX_PACKET_TYPE_DATA) {
#ifdef	AFS_SUN5_ENV
	    /*
	     * XXX Hack. Because we can't release the global rx lock when
	     * sending packets (osi_NetSend) we drop all acks while we're
	     * traversing the tq in rxi_Start sending packets out because
	     * packets may move to the freePacketQueue as result of being
	     * here! So we drop these packets until we're safely out of the
	     * traversing. Really ugly!
	     */
	    if (call->flags & RX_CALL_TQ_BUSY) {
		rx_tq_dropped++;
		return np;	       /* xmitting; drop packet */
	    }
#endif
	    rxi_ClearTransmitQueue(call);
	} else {
	    if (np->header.type == RX_PACKET_TYPE_ACK) {
		/*
		 * now check to see if this is an ack packet acknowledging
		 * that the server actually *lost* some hard-acked data.  If
		 * this happens we ignore this packet, as it may indicate that
		 * the server restarted in the middle of a call.  It is also
		 * possible that this is an old ack packet.  We don't abort
		 * the connection in this case, because this *might* just be
		 * an old ack packet.  The right way to detect a server restart
		 * in the midst of a call is to notice that the server epoch
		 * changed, btw.
		 */
		/*
		 * LWSXXX I'm not sure this is exactly right, since tfirst
		 * LWSXXX **IS** unacknowledged.  I think that this is
		 * LWSXXX off-by-one, but I don't dare change it just yet,
		 * LWSXXX since it will interact badly with the
		 * LWSXXX server-restart detection code in receiveackpacket.
		 */
		if (ntohl(rx_SlowGetLong(np, FIRSTACKOFFSET)) < call->tfirst) {
		    rx_stats.spuriousPacketsRead++;
#ifdef	RX_ENABLE_LOCKSX
		    mutex_exit(&call->lock);
#endif
		    conn->refCount--;
		    return np;
		}
	    }
	}			       /* else not a data packet */
    }

    /* Set remote user defined status from packet */
    call->remoteStatus = np->header.userStatus;

    /*
     * Note the gap between the expected next packet and the actual packet
     * that arrived, when the new packet has a smaller serial number than
     * expected.  Rioses frequently reorder packets all by themselves, so
     * this will be quite important with very large window sizes. Skew is
     * checked against 0 here to avoid any dependence on the type of
     * inPacketSkew (which may be unsigned).  In C, -1 > (unsigned) 0 is
     * always true! The inPacketSkew should be a smoothed running value, not
     * just a maximum.  MTUXXX see CalculateRoundTripTime for an example of
     * how to keep smoothed values. I think using a beta of 1/8 is probably
     * appropriate.  lws 93.04.21
     */
    skew = conn->lastSerial - np->header.serial;
    conn->lastSerial = np->header.serial;
    if (skew > 0) {
	struct rx_peer *peer;

	peer = conn->peer;
	if (skew > peer->inPacketSkew) {
	    dpf(("*** In skew changed from %d to %d\n",
		 peer->inPacketSkew, skew));
	    peer->inPacketSkew = skew;
	}
    }
    /* Now do packet type-specific processing */
    switch (np->header.type) {
    case RX_PACKET_TYPE_DATA:
	np = rxi_ReceiveDataPacket(call, np);
	break;
    case RX_PACKET_TYPE_ACK:
	/*
         * Respond immediately to ack packets requesting acknowledgement
         * (ping packets)
         */
	if (np->header.flags & RX_REQUEST_ACK) {
	    if (call->error)
		(void) rxi_SendCallAbort(call, 0);
	    else
		(void) rxi_SendAck(call, 0, 0, 0, 0, RX_ACK_PING_RESPONSE);
	}
	np = rxi_ReceiveAckPacket(call, np);
	break;
    case RX_PACKET_TYPE_ABORT:
	/*
         * An abort packet: reset the connection, passing the error up to
         * the user
         */
	/* XXX What if error is zero? and length of packet is 0 */
	rxi_CallError(call, ntohl(*(uint32_t *) rx_DataOf(np)));
	break;
    case RX_PACKET_TYPE_BUSY:
	/* XXXX */
	break;
    case RX_PACKET_TYPE_ACKALL:
	/*
         * All packets acknowledged, so we can drop all packets previously
         * readied for sending
         */
#ifdef	AFS_SUN5_ENV
	/*
         * XXX Hack. We because we can't release the global rx lock
         * when sending packets (osi_NetSend) we drop all ack pkts while
         * we're traversing the tq in rxi_Start sending packets out
         * because packets may move to the freePacketQueue as result of
         * being here! So we drop these packets until we're
         * safely out of the traversing. Really ugly!
         */
	if (call->flags & RX_CALL_TQ_BUSY) {
	    rx_tq_dropped++;
	    return np;		       /* xmitting; drop packet */
	}
#endif
	rxi_ClearTransmitQueue(call);
	break;
    default:
	/*
         * Should not reach here, unless the peer is broken: send an abort
         * packet
         */
	rxi_CallError(call, RX_PROTOCOL_ERROR);
	np = rxi_SendCallAbort(call, np);
	break;
    };
    /*
     * Note when this last legitimate packet was received, for keep-alive
     * processing.  Note, we delay getting the time until now in the hope that
     * the packet will be delivered to the user before any get time is required
     * (if not, then the time won't actually be re-evaluated here).
     */
    call->lastReceiveTime = clock_Sec();
#ifdef	RX_ENABLE_LOCKSX
    mutex_exit(&call->lock);
#endif
    conn->refCount--;
    return np;
}

/*
 * return true if this is an "interesting" connection from the point of view
 * of someone trying to debug the system
 */
int
rxi_IsConnInteresting(struct rx_connection * aconn)
{
    int i;
    struct rx_call *tcall;

    if (aconn->flags & (RX_CONN_MAKECALL_WAITING | RX_CONN_DESTROY_ME))
	return 1;
    for (i = 0; i < RX_MAXCALLS; i++) {
	tcall = aconn->call[i];
	if (tcall) {
	    if ((tcall->state == RX_STATE_PRECALL) ||
		(tcall->state == RX_STATE_ACTIVE))
		return 1;
	    if ((tcall->mode == RX_MODE_SENDING) ||
		(tcall->mode == RX_MODE_RECEIVING))
		return 1;
	}
    }
    return 0;
}

/*
 * if this is one of the last few packets AND it wouldn't be used by the
 * receiving call to immediately satisfy a read request, then drop it on
 * the floor, since accepting it might prevent a lock-holding thread from
 * making progress in its reading
 */

static int
TooLow(struct rx_packet * ap, struct rx_call * acall)
{
    if ((rx_nFreePackets < rxi_dataQuota + 2) &&
	!((ap->header.seq == acall->rnext) &&
	  (acall->flags & RX_CALL_READER_WAIT))) {
	return 1;
    } else
	return 0;
}

/* try to attach call, if authentication is complete */
static void
TryAttach(struct rx_call * acall)
{
    struct rx_connection *conn;

    conn = acall->conn;
    if ((conn->type == RX_SERVER_CONNECTION) &&
	(acall->state == RX_STATE_PRECALL)) {
	/* Don't attach until we have any req'd. authentication. */
	if (RXS_CheckAuthentication(conn->securityObject, conn) == 0) {
	    rxi_AttachServerProc(acall);
	    /*
	     * Note:  this does not necessarily succeed; there
	     * may not any proc available
	     */
	} else {
	    rxi_ChallengeOn(acall->conn);
	}
    }
}

/*
 * A data packet has been received off the interface.  This packet is
 * appropriate to the call (the call is in the right state, etc.).  This
 * routine can return a packet to the caller, for re-use
 */
struct rx_packet *
rxi_ReceiveDataPacket(struct rx_call * call,
		      struct rx_packet * np)
{
    u_long seq, serial, flags;
    int ack_done;

    ack_done = 0;

    seq = np->header.seq;
    serial = np->header.serial;
    flags = np->header.flags;

    rx_stats.dataPacketsRead++;

    /* If the call is in an error state, send an abort message */
    /* XXXX this will send too many aborts for multi-packet calls */
    if (call->error)
	return rxi_SendCallAbort(call, np);

    if (np->header.spare != 0)
	call->conn->flags |= RX_CONN_USING_PACKET_CKSUM;

    /*
     * If there are no packet buffers, drop this new packet, unless we can find
     * packet buffers from inactive calls
     */
    if (rxi_OverQuota(RX_PACKET_CLASS_RECEIVE) || TooLow(np, call)) {
	rx_stats.noPacketBuffersOnRead++;
	call->rprev = seq;
	TryAttach(call);
	rxi_calltrace(RX_TRACE_DROP, call);
	return np;
    }
    /* The usual case is that this is the expected next packet */
    if (seq == call->rnext) {

	/* Check to make sure it is not a duplicate of one already queued */
	if (queue_IsNotEmpty(&call->rq)
	    && queue_First(&call->rq, rx_packet)->header.seq == seq) {
	    rx_stats.dupPacketsRead++;
	    np = rxi_SendAck(call, np, seq, serial, flags, RX_ACK_DUPLICATE);
	    call->rprev = seq;
	    return np;
	}
	/*
	 * It's the next packet.  Stick it on the receive queue for this call
	 */
	queue_Prepend(&call->rq, np);
#ifdef SOFT_ACK
	call->nSoftAcks++;
#endif /* SOFT_ACK */

#ifndef	ADAPT_PERF
	np = 0;			       /* We can't use this any more */

	/*
	 * Provide asynchronous notification for those who want it
	 * (e.g. multi rx)
	 */
	if (call->arrivalProc) {
	    (*call->arrivalProc) (call, call->arrivalProcHandle,
				  call->arrivalProcArg);
	    call->arrivalProc = (void (*) ()) 0;
	}
	/* Wakeup the reader, if any */
	if (call->flags & RX_CALL_READER_WAIT) {
	    call->flags &= ~RX_CALL_READER_WAIT;
	    RX_MUTEX_ENTER(&call->lockq);

#ifdef	RX_ENABLE_LOCKS
	    cv_broadcast(&call->cv_rq);
#else
	    osi_rxWakeup(&call->rq);
#endif
	    RX_MUTEX_EXIT(&call->lockq);
	}
#endif

	/*
	 * ACK packet right away, in order to keep the window going, and to
	 * reduce variability in round-trip-time estimates.
	 */

	if (flags & RX_REQUEST_ACK) {

	    /*
	     * Acknowledge, if requested.  Also take this opportunity to
	     * revise MTU estimate.
	     */
#ifdef MISCMTU
	    /* Copy a lower estimate of the MTU from the other end.  (cfe) */
	    /*
	     * We can figure out what the other end is using by checking out
	     * the size of its packets, and then adding back the header size.
	     * We don't count the last packet, since it might be partly empty.
	     * We shouldn't do this check on every packet, it's overkill.
	     * Perhaps it would be better done in
	     * ComputeRate if I decide it's ever worth doing. (lws)
	     */
	    if (!(flags & RX_LAST_PACKET) && call->conn && call->conn->peer) {
		u_long length;
		struct rx_peer *peer = call->conn->peer;

		length = np->length + RX_HEADER_SIZE;
		if (length < peer->packetSize) {
		    dpf(("CONG peer %lx/%u: packetsize %lu=>%lu (rtt %u)",
		     ntohl(peer->host), ntohs(peer->port), peer->packetSize,
			 length, peer->srtt));
		    peer->packetSize = length;
		}
	    }
#endif				       /* MISCMTU */
	} else if (flags & RX_LAST_PACKET) {
	    struct clock when;

	    /* Or schedule an acknowledgement later on. */
	    rxevent_Cancel(call->delayedAckEvent);
	    clock_GetTime(&when);
	    clock_Add(&when, &rx_lastAckDelay);

	    call->delayedAckEvent = rxevent_Post(&when, rxi_SendDelayedAck,
						 call, NULL);

	    ack_done = 1;
	}
#ifdef	ADAPT_PERF
	/*
	 * Provide asynchronous notification for those who want it
	 * (e.g. multi rx)
	 */
	if (call->arrivalProc) {
	    (*call->arrivalProc) (call, call->arrivalProcHandle,
				  call->arrivalProcArg);
	    call->arrivalProc = (void (*) ()) 0;
	}
	/* Wakeup the reader, if any */
	RX_MUTEX_ENTER(&call->lockq);
	if (call->flags & RX_CALL_READER_WAIT) {
	    call->flags &= ~RX_CALL_READER_WAIT;
#ifdef	RX_ENABLE_LOCKS
/*	    cv_signal(&call->cv_rq);*/
	    cv_broadcast(&call->cv_rq);
#else
	    osi_rxWakeup(&call->rq);
#endif
	}
	RX_MUTEX_EXIT(&call->lockq);

	np = 0;			       /* We can't use this any more */
#endif				       /* ADAPT_PERF */

	/* Update last packet received */
	call->rprev = seq;

	/*
	 * If there is no server process serving this call, grab one, if
	 * available
	 */
	TryAttach(call);
    }
    /* This is not the expected next packet */
    else {
	/*
	 * Determine whether this is a new or old packet, and if it's
         * a new one, whether it fits into the current receive window.
         * It's also useful to know if the packet arrived out of
         * sequence, so that we can force an acknowledgement in that
         * case.  We have a slightly complex definition of
         * out-of-sequence: the previous packet number received off
         * the wire is remembered.  If the new arrival's sequence
         * number is less than previous, then previous is reset (to
         * 0).  MTUXXX  This should change slightly if skew is taken into
	 * consideration. lws 93.04.20
	 * The new packet is then declared out-of-sequence if
         * there are any packets missing between the "previous" packet
         * and the one which just arrived (because the missing packets
         * should have been filled in between the previous packet and
         * the new arrival).  This works regardless of whether the
         * peer's retransmission algorithm has been invoked, or not
         * (i.e. whether this is the first or subsequent pass over the
         * sequence of packets).  All this assumes that "most" of the
         * time, packets are delivered in the same *order* as they are
         * transmitted, with, possibly, some packets lost due to
         * transmission errors along the way.
	 */

	u_long prev;		       /* "Previous packet" sequence number */
	struct rx_packet *tp; /* Temporary packet pointer */
	struct rx_packet *nxp;/*
				        * Next packet pointer, for queue_Scan
				        */
	int nTwixt;		       /*
				        * Number of packets between previous
				        * and new one
				        */

	/*
	 * If the new packet's sequence number has been sent to the
         * application already, then this is a duplicate
	 */
	if (seq < call->rnext) {
	    rx_stats.dupPacketsRead++;
	    np = rxi_SendAck(call, np, seq, serial, flags, RX_ACK_DUPLICATE);
	    call->rprev = seq;
	    return np;
	}
	/*
	 * If the sequence number is greater than what can be
         * accommodated by the current window, then send a negative
         * acknowledge and drop the packet
	 */
	if ((call->rnext + call->rwind) <= seq) {
	    np = rxi_SendAck(call, np, seq, serial, flags,
			     RX_ACK_EXCEEDS_WINDOW);
	    call->rprev = seq;
	    return np;
	}
	/* Look for the packet in the queue of old received packets */
	prev = call->rprev;
	if (prev > seq)
	    prev = 0;
	for (nTwixt = 0, queue_Scan(&call->rq, tp, nxp, rx_packet)) {
	    /* Check for duplicate packet */
	    if (seq == tp->header.seq) {
		rx_stats.dupPacketsRead++;
		np = rxi_SendAck(call, np, seq, serial, flags,
				 RX_ACK_DUPLICATE);
		call->rprev = seq;
		return np;
	    }

	    /*
	     * Count the number of packets received 'twixt the previous
	     * packet and the new packet
	     */
	    if (tp->header.seq > prev && tp->header.seq < seq)
		nTwixt++;

	    /*
	     * If we find a higher sequence packet, break out and insert the
	     * new packet here.
	     */
	    if (seq < tp->header.seq)
		break;
	}

	/*
	 * It's within the window: add it to the the receive queue.
         * tp is left by the previous loop either pointing at the
         * packet before which to insert the new packet, or at the
         * queue head if the queue is empty or the packet should be
         * appended.
	 */
	queue_InsertBefore(tp, np);
#ifdef SOFT_ACK
	call->nSoftAcks++;
#endif /* SOFT_ACK */

	call->rprev = seq;
	np = 0;
    }

    /*
     * Acknowledge the packet if requested by peer, or we are doing
     * softack.
     *
     * Add a timed ack to make sure we send out a ack to before we get
     * a request from the client that they send a REQUEST-ACK packet.
     */
    if (ack_done) {
	/* ack is already taken care of */
    } else if (flags & RX_REQUEST_ACK) {
	rxi_SendAck(call, 0, seq, serial, flags, RX_ACK_REQUESTED);
	call->rprev = seq;
#ifdef SOFT_ACK
    } else if (call->nSoftAcks > rxi_SoftAckRate) {
	rxevent_Cancel(call->delayedAckEvent);
	rxi_SendAck(call, 0, seq, serial, flags, RX_ACK_IDLE);
    } else if (call->nSoftAcks) {
	struct clock when;

	rxevent_Cancel(call->delayedAckEvent);
	clock_GetTime(&when);
	clock_Add(&when, &rx_softAckDelay);
	call->delayedAckEvent = rxevent_Post(&when, rxi_SendDelayedAck,
					     call, NULL);
#endif /* SOFT_ACK */
    }
    
    return np;
}

#ifdef	ADAPT_WINDOW
static void rxi_ComputeRate();

#endif

/* Timeout is set to RTT + 4*MDEV. */
static
void
update_timeout(struct rx_peer *peer)
{
    u_long rtt_timeout;
    rtt_timeout = peer->srtt + 4*peer->mdev;
    /*
     * Add 100ms to hide the effects of unpredictable
     * scheduling. 100ms is *very* conservative and should probably be
     * much smaller. We don't want to generate any redundant
     * retransmits so for now, let's use 100ms.
     */
    rtt_timeout += 100*1000;
    if (rtt_timeout < 1000)	/* 1000 = 1ms */
        rtt_timeout = 1000;	/* Minimum timeout */
    peer->timeout.usec = rtt_timeout % 1000000;
    peer->timeout.sec  = rtt_timeout / 1000000;;
}

/* On a dubious timeout double MDEV but within reason.
 * Timeout is limited by 5*RTT.
 */
static
void
dubious_timeout(struct rx_peer *peer)
{
    if (peer->mdev >= peer->srtt)
        return;

    peer->mdev *= 2;
    if (peer->mdev > peer->srtt)
        peer->mdev = peer->srtt;
    update_timeout(peer);
}

/* The real smarts of the whole thing.  Right now somewhat short-changed. */
struct rx_packet *
rxi_ReceiveAckPacket(struct rx_call * call, struct rx_packet * np)
{
    struct rx_ackPacket *ap;
    int nAcks;
    struct rx_packet *tp;
    struct rx_packet *nxp;    /*
				        * Next packet pointer for queue_Scan
				        */
    struct rx_connection *conn = call->conn;
    struct rx_peer *peer = conn->peer;
    u_long first;
    u_long serial;

    /* because there are CM's that are bogus, sending weird values for this. */
    u_long skew = 0;
    int needRxStart = 0;
    int nbytes;

    rx_stats.ackPacketsRead++;
    ap = (struct rx_ackPacket *) rx_DataOf(np);
    nbytes = rx_Contiguous(np) - ((ap->acks) - (u_char *) ap);
    if (nbytes < 0)
	return np;		       /* truncated ack packet */

    nAcks = MIN(nbytes, ap->nAcks);    /* depends on ack packet struct */
    first = ntohl(ap->firstPacket);
    serial = ntohl(ap->serial);
#ifdef notdef
    skew = ntohs(ap->maxSkew);
#endif


#ifdef RXDEBUG
    if (Log) {
	fprintf(Log,
		"RACK: reason %x previous %lu seq %lu serial %lu skew %lu "
		"first %lu", ap->reason, 
		(unsigned long)ntohl(ap->previousPacket), 
		(unsigned long)np->header.seq, 
		(unsigned long)serial, 
		(unsigned long)skew, 
		(unsigned long)ntohl(ap->firstPacket));
	if (nAcks) {
	    int offset;

	    for (offset = 0; offset < nAcks; offset++)
		putc(ap->acks[offset] == RX_ACK_TYPE_NACK ? '-' : '*', Log);
	}
	putc('\n', Log);
    }
#endif

#if 0 /* need congestion avoidance stuff first */
    if (np->header.flags & RX_SLOW_START_OK)
	call->flags |= RX_CALL_SLOW_START_OK;
#endif
    

    /*
     * if a server connection has been re-created, it doesn't remember what
     * serial # it was up to.  An ack will tell us, since the serial field
     * contains the largest serial received by the other side
     */
    if ((conn->type == RX_SERVER_CONNECTION) && (conn->serial < serial)) {
	conn->serial = serial + 1;
    }

    /*
     * Update the outgoing packet skew value to the latest value of the
     * peer's incoming packet skew value.  The ack packet, of course, could
     * arrive out of order, but that won't affect things much
     */
    peer->outPacketSkew = skew;

#ifdef	AFS_SUN5_ENV
    /*
     * XXX Hack. Because we can't release the global rx lock when sending
     * packets (osi_NetSend) we drop all acks while we're traversing the tq in
     * rxi_Start sending packets out because packets
     * may move to the freePacketQueue as result of being here! So we drop
     * these packets until we're safely out of the traversing. Really ugly!
     */
    if (call->flags & RX_CALL_TQ_BUSY) {
	rx_tq_dropped++;
	return np;		       /* xmitting; drop packet */
    }
#endif
    /*
     * Check for packets that no longer need to be transmitted, and
     * discard them.  This only applies to packets positively
     * acknowledged as having been sent to the peer's upper level.
     * All other packets must be retained.  So only packets with
     * sequence numbers < ap->firstPacket are candidates.
     */
    while (queue_IsNotEmpty(&call->tq)) {
	tp = queue_First(&call->tq, rx_packet);
	if (tp->header.seq >= first)
	    break;
	call->tfirst = tp->header.seq + 1;
	if (tp->header.serial == serial) {
	    if (ap->reason != RX_ACK_DELAY) {
#ifdef	ADAPT_PERF
		rxi_ComputeRoundTripTime(tp, &tp->timeSent, peer);
#else
		rxi_ComputeRoundTripTime(tp, 0, 0);
#endif
	    }
#ifdef ADAPT_WINDOW
	    rxi_ComputeRate(peer, call, tp, np, ap->reason);
#endif
	}
#ifdef	ADAPT_PERF
	else if ((tp->firstSerial == serial)) {
	    if (ap->reason != RX_ACK_DELAY)
		rxi_ComputeRoundTripTime(tp, &tp->firstSent, peer);
#ifdef ADAPT_WINDOW
	    rxi_ComputeRate(peer, call, tp, np, ap->reason);
#endif
	}
#endif				       /* ADAPT_PERF */
	queue_Remove(tp);
	rxi_FreePacket(tp);	       /*
				        * rxi_FreePacket mustn't wake up anyone,
				        * preemptively.
				        */
    }

#ifdef ADAPT_WINDOW
    /* Give rate detector a chance to respond to ping requests */
    if (ap->reason == RX_ACK_PING_RESPONSE) {
	rxi_ComputeRate(peer, call, 0, np, ap->reason);
    }
#endif
    /* "Slow start" every call. */
    if (call->twind < rx_Window) call->twind += 1;

    /*
     * N.B. we don't turn off any timers here.  They'll go away by themselves,
     * anyway
     */

    /*
     * Now go through explicit acks/nacks and record the results in
     * the waiting packets.  These are packets that can't be released
     * yet, even with a positive acknowledge.  This positive
     * acknowledge only means the packet has been received by the
     * peer, not that it will be retained long enough to be sent to
     * the peer's upper level.  In addition, reset the transmit timers
     * of any missing packets (those packets that must be missing
     * because this packet was out of sequence)
     */

    for (queue_Scan(&call->tq, tp, nxp, rx_packet)) {

	/*
	 * Update round trip time if the ack was stimulated on receipt of
	 * this packet
	 */
	if (tp->header.serial == serial) {
	    if (ap->reason != RX_ACK_DELAY) {
#ifdef	ADAPT_PERF
		rxi_ComputeRoundTripTime(tp, &tp->timeSent, peer);
#else
		rxi_ComputeRoundTripTime(tp, 0, 0);
#endif
	    }
#ifdef ADAPT_WINDOW
	    rxi_ComputeRate(peer, call, tp, np, ap->reason);
#endif
	}
#ifdef	ADAPT_PERF
	else if ((tp->firstSerial == serial)) {
	    if (ap->reason != RX_ACK_DELAY)
		rxi_ComputeRoundTripTime(tp, &tp->firstSent, peer);
#ifdef ADAPT_WINDOW
	    rxi_ComputeRate(peer, call, tp, np, ap->reason);
#endif
	}
#endif				       /* ADAPT_PERF */

	/*
	 * Set the acknowledge flag per packet based on the
         * information in the ack packet.  It's possible for an
         * acknowledged packet to be downgraded
	 */
	if (tp->header.seq < first + nAcks) {
	    /* Explicit ack information:  set it in the packet appropriately */
	    tp->acked = (ap->acks[tp->header.seq - first] == RX_ACK_TYPE_ACK);
	} else {
	    /*
	     * No ack information: the packet may have been
             * acknowledged previously, but that is now rescinded (the
             * peer may have reduced the window size)
	     */
	    tp->acked = 0;
	}


#ifdef	ADAPT_PERF
	/*
	 * If packet isn't yet acked, and it has been transmitted at least
	 * once, reset retransmit time using latest timeout
	 * ie, this should readjust the retransmit timer for all outstanding
	 * packets...  So we don't just retransmit when we should know better
	 */

	if (!tp->acked && tp->header.serial) {
	    tp->retryTime = tp->timeSent;
	    clock_Add(&tp->retryTime, &peer->timeout);
	    /* shift by eight because one quarter-sec ~ 256 milliseconds */
	    clock_Addmsec(&(tp->retryTime), ((unsigned long) tp->backoff) << 8);
	}
#endif				       /* ADAPT_PERF */

	/*
	 * If the packet isn't yet acked, and its serial number
         * indicates that it was transmitted before the packet which
         * prompted the acknowledge (that packet's serial number is
         * supplied in the ack packet), then schedule the packet to be
         * transmitted *soon*.  This is done by resetting the
         * retransmit time in the packet to the current time.
         * Actually this is slightly more intelligent: to guard
         * against packets that have been transmitted out-of-order by
         * the network (this even happens on the local token ring with
         * our IBM RT's!), the degree of out-of-orderness (skew) of
         * the packet is compared against the maximum skew for this
         * peer.  If it is less, we don't retransmit yet.  Note that
         * we don't do this for packets with zero serial numbers: they
         * never have been transmitted.
	 */

	/*
	 * I don't know if we should add in the new retransmit backoff time
	 * here or not.  I think that we should also consider reducing
	 * the "congestion window" size as an alternative.  LWSXXX
	 */

	if (!tp->acked && tp->header.serial
	    && ((tp->header.serial + skew) <= serial)) {
	    rx_stats.dataPacketsPushed++;
	    clock_GetTime(&tp->retryTime);
	    needRxStart = 1;

	    dpf(("Pushed packet seq %d serial %d, new time %d.%d\n",
		 tp->header.seq, tp->header.serial, tp->retryTime.sec,
		 tp->retryTime.usec / 1000));
	}
    }

    if (ap->reason == RX_ACK_DUPLICATE) {
        /*
	 * Other end receives duplicates because either:
	 * A. acks where lost
	 * B. receiver gets scheduled in an unpredictable way
	 * C. we have a busted timer
	 *
	 * To fix B & C wait for new acks to update srtt and mdev. In
	 * the meantime, increase mdev to increase the retransmission
	 * timeout.
	 */
	dubious_timeout(peer);
    }

    /*
     * If the window has been extended by this acknowledge packet,
     * then wakeup a sender waiting in alloc for window space, or try
     * sending packets now, if he's been sitting on packets due to
     * lack of window space
     */
    if (call->tnext < (call->tfirst + call->twind)) {
#ifdef	RX_ENABLE_LOCKS
	RX_MUTEX_ENTER(&call->lockw);
	cv_signal(&call->cv_twind);
	RX_MUTEX_EXIT(&call->lockw);
#else
	if (call->flags & RX_CALL_WAIT_WINDOW_ALLOC) {
	    call->flags &= ~RX_CALL_WAIT_WINDOW_ALLOC;
	    osi_rxWakeup(&call->twind);
	}
#endif
	if (call->flags & RX_CALL_WAIT_WINDOW_SEND) {
	    call->flags &= ~RX_CALL_WAIT_WINDOW_SEND;
	    needRxStart = 1;
	}
    }
    /*
     * if the ack packet has a receivelen field hanging off it,
     * update our state
     */
    if (np->length >= rx_AckDataSize(ap->nAcks) + 4) {
	unsigned long maxPacketSize;

	rx_packetread(np, rx_AckDataSize(ap->nAcks), 4, &maxPacketSize);
	maxPacketSize = (unsigned long) ntohl(maxPacketSize);
	dpf(("maxPacketSize=%lu\n", maxPacketSize));

	/*
	 * sanity check - peer might have restarted with different params.
	 * If peer says "send less", dammit, send less...  Peer should never
	 * be unable to accept packets of the size that prior AFS versions
	 * would send without asking.
	 */
	if (OLD_MAX_PACKET_SIZE <= maxPacketSize)
	    conn->maxPacketSize = MIN(maxPacketSize, conn->peer->packetSize);
    }
    /* if (needRxStart) rxi_Start(0, call); */
    rxi_Start(0, call);		       /* Force rxi_Restart for now:  skew
				        * problems!!! */
    return np;
}

/* Post a new challenge-event, this is to resend lost packets. */
static void
rxi_resend_ChallengeEvent(struct rx_connection *conn)
{
    struct clock when;

    if (conn->challengeEvent)
	rxevent_Cancel(conn->challengeEvent);
    
    clock_GetTime(&when);
    when.sec += RX_CHALLENGE_TIMEOUT;
    conn->challengeEvent = rxevent_Post(&when, rxi_ChallengeEvent,
					conn, NULL);
}

/* Received a response to a challenge packet */
struct rx_packet *
rxi_ReceiveResponsePacket(struct rx_connection * conn,
			  struct rx_packet * np)
{
    int error;

    /* Ignore the packet if we're the client */
    if (conn->type == RX_CLIENT_CONNECTION)
	return np;

    /* If already authenticated, ignore the packet (it's probably a retry) */
    if (RXS_CheckAuthentication(conn->securityObject, conn) == 0)
	return np;

    /* Otherwise, have the security object evaluate the response packet */
    error = RXS_CheckResponse(conn->securityObject, conn, np);
    if (error == RX_AUTH_REPLY) {
	rxi_SendSpecial(NULL, conn, np, RX_PACKET_TYPE_CHALLENGE, 
			NULL, -1);
	rxi_resend_ChallengeEvent(conn);
    } else if (error) {
	/*
	 * If the response is invalid, reset the connection, sending an abort
	 * to the peer
	 */
#ifndef KERNEL
	IOMGR_Sleep(1);
#endif
	rxi_ConnectionError(conn, error);
	return rxi_SendConnectionAbort(conn, np);
    } else {
	/*
	 * If the response is valid, any calls waiting to attach servers can
	 * now do so
	 */
	int i;

	for (i = 0; i < RX_MAXCALLS; i++) {
	    struct rx_call *call = conn->call[i];

	    if (call && (call->state == RX_STATE_PRECALL))
		rxi_AttachServerProc(call);
	}
    }
    return np;
}

/*
 * A client has received an authentication challenge: the security
 * object is asked to cough up a respectable response packet to send
 * back to the server.  The server is responsible for retrying the
 * challenge if it fails to get a response.
 */

struct rx_packet *
rxi_ReceiveChallengePacket(struct rx_connection * conn,
			   struct rx_packet * np)
{
    int error;

    /* Ignore the challenge if we're the server */
    if (conn->type == RX_SERVER_CONNECTION)
	return np;

    /*
     * Ignore the challenge if the connection is otherwise idle; someone's
     * trying to use us as an oracle.
     */
    if (!rxi_HasActiveCalls(conn))
	return np;

    /*
     * Send the security object the challenge packet.  It is expected to fill
     * in the response.
     */
    error = RXS_GetResponse(conn->securityObject, conn, np);

    /*
     * If the security object is unable to return a valid response, reset the
     * connection and send an abort to the peer.  Otherwise send the response
     * packet to the peer connection.
     */
    if (error) {
	rxi_ConnectionError(conn, error);
	np = rxi_SendConnectionAbort(conn, np);
    } else {
	np = rxi_SendSpecial((struct rx_call *) 0, conn, np,
			     RX_PACKET_TYPE_RESPONSE, (char *) 0, -1);
    }
    return np;
}


/*
 * Find an available server process to service the current request in
 * the given call structure.  If one isn't available, queue up this
 * call so it eventually gets one
 */
void
rxi_AttachServerProc(struct rx_call * call)
{
    struct rx_serverQueueEntry *sq;
    struct rx_service *service = call->conn->service;

    /* May already be attached */
    if (call->state == RX_STATE_ACTIVE)
	return;

    if (!QuotaOK(service) || queue_IsEmpty(&rx_idleServerQueue)) {
	/*
	 * If there are no processes available to service this call,
         * put the call on the incoming call queue (unless it's
         * already on the queue).
         */
	if (!(call->flags & RX_CALL_WAIT_PROC)) {
	    call->flags |= RX_CALL_WAIT_PROC;
	    rx_nWaiting++;
	    rxi_calltrace(RX_CALL_ARRIVAL, call);
	    queue_Append(&rx_incomingCallQueue, call);
	}
    } else {
	sq = queue_First(&rx_idleServerQueue, rx_serverQueueEntry);

	RX_MUTEX_ENTER(&sq->lock);

	queue_Remove(sq);
	sq->newcall = call;
	if (call->flags & RX_CALL_WAIT_PROC) {
	    /* Conservative:  I don't think this should happen */
	    call->flags &= ~RX_CALL_WAIT_PROC;
	    rx_nWaiting--;
	    queue_Remove(call);
	}
	call->state = RX_STATE_ACTIVE;
	call->mode = RX_MODE_RECEIVING;
	if (call->flags & RX_CALL_CLEARED) {
	    /* send an ack now to start the packet flow up again */
	    call->flags &= ~RX_CALL_CLEARED;
	    rxi_SendAck(call, 0, 0, 0, 0, RX_ACK_DELAY);
	}
	service->nRequestsRunning++;
	if (service->nRequestsRunning <= service->minProcs)
	    rxi_minDeficit--;
	rxi_availProcs--;
#ifdef	RX_ENABLE_LOCKS
	cv_signal(&sq->cv);
#else
	osi_rxWakeup(sq);
#endif
	RX_MUTEX_EXIT(&sq->lock);
    }
}

/*
 * Delay the sending of an acknowledge event for a short while, while
 * a new call is being prepared (in the case of a client) or a reply
 * is being prepared (in the case of a server).  Rather than sending
 * an ack packet, an ACKALL packet is sent.
 */
void
rxi_AckAll(struct rxevent * event, struct rx_call * call, char *dummy)
{
    if (event)
	call->delayedAckEvent = (struct rxevent *) 0;
    rxi_SendSpecial(call, call->conn, (struct rx_packet *) 0,
		    RX_PACKET_TYPE_ACKALL, (char *) 0, 0);
}

void
rxi_SendDelayedAck(struct rxevent * event, struct rx_call * call,
		   char *dummy)
{
    if (event)
	call->delayedAckEvent = (struct rxevent *) 0;
    (void) rxi_SendAck(call, 0, 0, 0, 0, RX_ACK_DELAY);
}

/*
 * Clear out the transmit queue for the current call (all packets have
 * been received by peer)
 */
void
rxi_ClearTransmitQueue(struct rx_call * call)
{
    struct rx_packet *p, *tp;

    for (queue_Scan(&call->tq, p, tp, rx_packet)) {
	queue_Remove(p);
	rxi_FreePacket(p);
    }

    rxevent_Cancel(call->resendEvent);
    call->tfirst = call->tnext;	       /*
				        * implicitly acknowledge all data already sent
				        */
    RX_MUTEX_ENTER(&call->lockw);
#ifdef	RX_ENABLE_LOCKS
    cv_signal(&call->cv_twind);
#else
    osi_rxWakeup(&call->twind);
#endif
    RX_MUTEX_EXIT(&call->lockw);
}

void
rxi_ClearReceiveQueue(struct rx_call * call)
{
    struct rx_packet *p, *tp;

    for (queue_Scan(&call->rq, p, tp, rx_packet)) {
	queue_Remove(p);
	rxi_FreePacket(p);
    }
    if (call->state == RX_STATE_PRECALL)
	call->flags |= RX_CALL_CLEARED;
}

/* Send an abort packet for the specified call */
struct rx_packet *
rxi_SendCallAbort(struct rx_call * call, struct rx_packet * packet)
{
    if (call->error) {
	int32_t error;

	error = htonl(call->error);
	packet = rxi_SendSpecial(call, call->conn, packet, RX_PACKET_TYPE_ABORT,
				 (char *) &error, sizeof(error));
    }
    return packet;
}

/*
 * Send an abort packet for the specified connection.  Np is an
 * optional packet that can be used to send the abort.
 */
struct rx_packet *
rxi_SendConnectionAbort(struct rx_connection * conn,
			struct rx_packet * packet)
{
    if (conn->error) {
	int32_t error;

	error = htonl(conn->error);
	packet = rxi_SendSpecial((struct rx_call *) 0, conn, packet,
		      RX_PACKET_TYPE_ABORT, (char *) &error, sizeof(error));
    }
    return packet;
}

/*
 * Associate an error all of the calls owned by a connection.  Called
 * with error non-zero.  This is only for really fatal things, like
 * bad authentication responses.  The connection itself is set in
 * error at this point, so that future packets received will be
 * rejected.
 */
void
rxi_ConnectionError(struct rx_connection * conn, int32_t error)
{
    if (error) {
	int i;

	if (conn->challengeEvent)
	    rxevent_Cancel(conn->challengeEvent);
	for (i = 0; i < RX_MAXCALLS; i++) {
	    struct rx_call *call = conn->call[i];

	    if (call)
		rxi_CallError(call, error);
	}
	conn->error = error;
	rx_stats.fatalErrors++;
    }
}

/* Reset all of the calls associated with a connection. */
void
rxi_ResetConnection(struct rx_connection * conn)
{
    int i;

    for (i = 0; i < RX_MAXCALLS; i++) {
	struct rx_call *call = conn->call[i];

	if (call)
	    rxi_ResetCall(call);
    }

    /* get rid of pending events that could zap us later */
    if (conn->challengeEvent)
	rxevent_Cancel(conn->challengeEvent);
}

void
rxi_CallError(struct rx_call * call, int32_t error)
{
    if (call->error)
	error = call->error;
    rxi_ResetCall(call);
    call->error = error;
    call->mode = RX_MODE_ERROR;
}

/*
 * Reset various fields in a call structure, and wakeup waiting
 * processes.  Some fields aren't changed: state & mode are not
 * touched (these must be set by the caller), and bufptr, nLeft, and
 * nFree are not reset, since these fields are manipulated by
 * unprotected macros, and may only be reset by non-interrupting code.
 */
#ifdef ADAPT_WINDOW
/* this code requires that call->conn be set properly as a pre-condition. */
#endif				       /* ADAPT_WINDOW */

void
rxi_ResetCall(struct rx_call * call)
{
    int flags;

    /* Notify anyone who is waiting for asynchronous packet arrival */
    if (call->arrivalProc) {
	(*call->arrivalProc) (call, call->arrivalProcHandle,
			      call->arrivalProcArg);
	call->arrivalProc = (void (*) ()) 0;
    }
    flags = call->flags;
    rxi_ClearReceiveQueue(call);
    rxi_ClearTransmitQueue(call);
    call->error = 0;
    call->flags = 0;
    call->rwind = rx_Window;	       /* XXXX */
#ifdef ADAPT_WINDOW
    call->twind = call->conn->peer->maxWindow;	/* XXXX */
#else
    /* "Slow start" every call. */
    call->twind = rx_initialWindow;
#endif

    call->tfirst = call->rnext = call->tnext = 1;
    call->rprev = 0;
    call->lastAcked = 0;
    call->localStatus = call->remoteStatus = 0;

    RX_MUTEX_ENTER(&call->lockq);
    if (flags & RX_CALL_READER_WAIT) {
#ifdef	RX_ENABLE_LOCKS
/*	cv_signal(&call->cv_rq);*/
	cv_broadcast(&call->cv_rq);
#else
	osi_rxWakeup(&call->rq);
#endif
    }
    RX_MUTEX_EXIT(&call->lockq);
    if (flags & RX_CALL_WAIT_PACKETS)
	rxi_PacketsUnWait();	       /* XXX */
    RX_MUTEX_ENTER(&call->lockw);

#ifdef	RX_ENABLE_LOCKS
    cv_signal(&call->cv_twind);
#else
    if (flags & RX_CALL_WAIT_WINDOW_ALLOC)
	osi_rxWakeup(&call->twind);
#endif
    RX_MUTEX_EXIT(&call->lockw);

    if (queue_IsOnQueue(call)) {
	queue_Remove(call);
	if (flags & RX_CALL_WAIT_PROC)
	    rx_nWaiting--;
    }
    rxi_KeepAliveOff(call);
    rxevent_Cancel(call->delayedAckEvent);
}

/*
 * Send an acknowledge for the indicated packet (seq,serial) of the
 * indicated call, for the indicated reason (reason).  This
 * acknowledge will specifically acknowledge receiving the packet, and
 * will also specify which other packets for this call have been
 * received.  This routine returns the packet that was used to the
 * caller.  The caller is responsible for freeing it or re-using it.
 * This acknowledgement also returns the highest sequence number
 * actually read out by the higher level to the sender; the sender
 * promises to keep around packets that have not been read by the
 * higher level yet (unless, of course, the sender decides to abort
 * the call altogether).  Any of p, seq, serial, pflags, or reason may
 * be set to zero without ill effect.  That is, if they are zero, they
 * will not convey any information.
 * NOW there is a trailer field, after the ack where it will safely be
 * ignored by mundanes, which indicates the maximum size packet this
 * host can swallow.
 */
struct rx_packet *
rxi_SendAck(struct rx_call * call,
	    struct rx_packet * optionalPacket, int seq, int serial,
	    int pflags, int reason)
#if 0
    struct rx_call *call;
    struct rx_packet *optionalPacket;	/* use to send ack (or null) */
    int seq;			       /* Sequence number of the packet we
				        * are acking */
    int serial;			       /* Serial number of the packet */
    int pflags;			       /* Flags field from packet header */
    int reason;			       /* Reason an acknowledge was prompted */

#endif
{
    struct rx_ackPacket *ap;
    struct rx_packet *rqp;
    struct rx_packet *nxp;    /* For queue_Scan */
    struct rx_packet *p;
    u_char offset;
    long templ;

    if (call->rnext > call->lastAcked)
	call->lastAcked = call->rnext;

    p = optionalPacket;

    if (p) {
	rx_computelen(p, p->length);   /* reset length, you never know */
    }
     /* where that's been...         */ 
    else if (!(p = rxi_AllocPacket(RX_PACKET_CLASS_SPECIAL)))
	osi_Panic("rxi_SendAck");

#ifdef SOFT_ACK
    call->nSoftAcks = 0;
    call->nHardAcks = 0;
#endif /* SOFT_ACK */

    templ = rx_AckDataSize(call->rwind) + 4 - rx_GetDataSize(p);
    if (templ > 0) {
	if (rxi_AllocDataBuf(p, templ))
	    return optionalPacket;
	templ = rx_AckDataSize(call->rwind) + 4;
	if (rx_Contiguous(p) < templ)
	    return optionalPacket;
    }				       /* MTUXXX failing to send an ack is
				        * very serious.  We should */
    /* try as hard as possible to send even a partial ack; it's */
    /* better than nothing. */
    ap = (struct rx_ackPacket *) rx_DataOf(p);
    ap->bufferSpace = htonl(0);	       /* Something should go here, sometime */
    ap->reason = reason;

    /* The skew computation used to bullshit, I think it's better now. */
    /* We should start paying attention to skew.    XXX  */
    ap->serial = htonl(call->conn->maxSerial);
    ap->maxSkew = 0;		       /* used to be peer->inPacketSkew */

    ap->firstPacket = htonl(call->rnext);	/*
					         * First packet not yet forwarded
					         * to reader
					         */
    ap->previousPacket = htonl(call->rprev);	/* Previous packet received */

    /*
     * No fear of running out of ack packet here because there can only be
     * at most one window full of unacknowledged packets.  The window size
     * must be constrained to be less than the maximum ack size, of course.
     * Also, an ack should always  fit into a single packet -- it should not
     * ever be fragmented.
     */
    for (offset = 0, queue_Scan(&call->rq, rqp, nxp, rx_packet)) {
	while (rqp->header.seq > call->rnext + offset)
	    ap->acks[offset++] = RX_ACK_TYPE_NACK;
	ap->acks[offset++] = RX_ACK_TYPE_ACK;
    }
    ap->nAcks = offset;
    p->length = rx_AckDataSize(offset) + 4;
    templ = htonl(rx_maxReceiveSize);
    rx_packetwrite(p, rx_AckDataSize(offset), 4, &templ);
    p->header.serviceId = call->conn->serviceId;
    p->header.cid = (call->conn->cid | call->channel);
    p->header.callNumber = *call->callNumber;
    p->header.seq = seq;
    p->header.securityIndex = call->conn->securityIndex;
    p->header.epoch = call->conn->epoch;
    p->header.type = RX_PACKET_TYPE_ACK;
    p->header.flags = 0;
    if (reason == RX_ACK_PING) {
	p->header.flags |= RX_REQUEST_ACK;
#ifdef ADAPT_WINDOW
	clock_GetTime(&call->pingRequestTime);
#endif
    }
    if (call->conn->type == RX_CLIENT_CONNECTION)
	p->header.flags |= RX_CLIENT_INITIATED;


#ifdef RXDEBUG
    if (Log) {
	fprintf(Log, "SACK: reason %x previous %lu seq %lu first %lu",
		ap->reason, 
		(unsigned long)ntohl(ap->previousPacket), 
		(unsigned long)p->header.seq,
		(unsigned long)ntohl(ap->firstPacket));
	if (ap->nAcks) {
	    for (offset = 0; offset < ap->nAcks; offset++)
		putc(ap->acks[offset] == RX_ACK_TYPE_NACK ? '-' : '*', Log);
	}
	putc('\n', Log);
    }
#endif

    {
	int i, nbytes = p->length;

	for (i = 1; i < p->niovecs; i++) {	/* vec 0 is ALWAYS header */
	    if (nbytes <= p->wirevec[i].iov_len) {
		int savelen, saven;

		savelen = p->wirevec[i].iov_len;
		saven = p->niovecs;
		p->wirevec[i].iov_len = nbytes;
		p->niovecs = i + 1;
		rxi_Send(call, p);
		p->wirevec[i].iov_len = savelen;
		p->niovecs = saven;
		break;
	    } else
		nbytes -= p->wirevec[i].iov_len;
	}
    }
    rx_stats.ackPacketsSent++;
    if (!optionalPacket)
	rxi_FreePacket(p);
    return optionalPacket;	       /* Return packet for re-use by caller */
}


/*
 * This routine is called when new packets are readied for
 * transmission and when retransmission may be necessary, or when the
 * transmission window or burst count are favourable.  This should be
 * better optimized for new packets, the usual case, now that we've
 * got rid of queues of send packets. XXXXXXXXXXX
 */
void
rxi_Start(struct rxevent * event, struct rx_call * call)
{
    int nSent = 0;
    struct rx_packet *p;
    struct rx_packet *nxp;    /* Next pointer for queue_Scan */
    struct rx_packet *lastPacket;
    struct rx_peer *peer = call->conn->peer;
    struct clock now, retryTime;
    int haveEvent;

    /*
     * If rxi_Start is being called as a result of a resend event,
     * then make sure that the event pointer is removed from the call
     * structure, since there is no longer a per-call retransmission
     * event pending.
     */
    if (event && event == call->resendEvent)
	call->resendEvent = 0;

    if (queue_IsNotEmpty(&call->tq)) { /* If we have anything to send */
	/*
	 * Get clock to compute the re-transmit time for any packets
         * in this burst.  Note, if we back off, it's reasonable to
         * back off all of the packets in the same manner, even if
         * some of them have been retransmitted more times than more
         * recent additions
	 */
	clock_GetTime(&now);
	retryTime = now;	       /* initialize before use */
	clock_Add(&retryTime, &peer->timeout);

	/*
	 * Send (or resend) any packets that need it, subject to
         * window restrictions and congestion burst control
         * restrictions.  Ask for an ack on the last packet sent in
         * this burst.  For now, we're relying upon the window being
         * considerably bigger than the largest number of packets that
         * are typically sent at once by one initial call to
         * rxi_Start.  This is probably bogus (perhaps we should ask
         * for an ack when we're half way through the current
         * window?).  Also, for non file transfer applications, this
         * may end up asking for an ack for every packet.  Bogus. XXXX
         */
	call->flags |= RX_CALL_TQ_BUSY;
	for (lastPacket = (struct rx_packet *) 0,
	     queue_Scan(&call->tq, p, nxp, rx_packet)) {
	    if (p->acked) {
		rx_stats.ignoreAckedPacket++;
		continue;	       /* Ignore this packet if it has been
				        * acknowledged */
	    }
	    /*
	     * Turn off all flags except these ones, which are the same
             * on each transmission
	     */
	    p->header.flags &= RX_PRESET_FLAGS;

	    if (p->header.seq >= call->tfirst + call->twind) {
		call->flags |= RX_CALL_WAIT_WINDOW_SEND;	/*
							         * Wait for transmit
							         * window
							         */
		/*
	         * Note: if we're waiting for more window space, we can
                 * still send retransmits; hence we don't return here, but
                 * break out to schedule a retransmit event
	         */
		break;
	    }
	    /*
	     * If we're not allowed to send any more in the current
             * burst, make sure we get scheduled later.  Also schedule
             * an event to "give back" the packets we've used, when the
             * burst time has elapsed (if we used any packets at all).
             */
	    /* XXX this need to go away and replaced with congestion
	     * avoidance */
	    if (peer->burstSize && !peer->burst) {
		if (nSent) {
		    /* Send off the prior packet */
		    /*
	             * Don't request an ack if it's a short packet, because the
		     * peer will cut down its MTU as a result.
	             */
		    if ((lastPacket->header.flags & RX_LAST_PACKET) == 0) {
			if (/* call->cwind <= (u_short)call->ackRate || */
			    (!(call->flags & RX_CALL_SLOW_START_OK)
			     && (lastPacket->header.seq & 1))) {
			    
			    lastPacket->header.flags |= RX_REQUEST_ACK;
			}
		    }
		    rxi_Send(call, lastPacket);
		    rxi_ScheduleDecongestionEvent(call, nSent);
		}
		rxi_CongestionWait(call);
		/*
	         * Note: if we're waiting for congestion to ease, we can't
                 * send any packets, including retransmits.  Hence we do
                 * not schedule a new retransmit event right now
	         */
		call->flags &= ~RX_CALL_TQ_BUSY;
		return;
	    }
	    /*
	     * Transmit the packet if it has never been sent, or
             * retransmit it if the current timeout for this host for
             * this packet has elapsed
	     */
	    if (!clock_IsZero(&p->retryTime)) {
	        struct clock updatedRetryTime;

		if (clock_Lt(&now, &p->retryTime))
		    continue;

		/*
		 * If the RTT has gone up since the packet
		 * was sent, don't retransmit just yet!
		 */
		updatedRetryTime = p->timeSent;
		clock_Add(&updatedRetryTime, &peer->timeout);
		if (clock_Lt(&now, &updatedRetryTime)) {
		    p->retryTime = updatedRetryTime;
		    continue;
		}

		/*
		 * If we have to retransmit chances are that we have a
		 * busted timer. Increase MDEV to reflect this
		 * fact. If we are wrong, MDEV will come down quickly
		 * as new acks arrive.
		 */
		dubious_timeout(peer);

		/*
	         * Always request an ack on a retransmitted packet; this
                 * will help to get the data moving again, especially if
                 * the packet is near the beginning of the window.
                 * Actually, XXXX, we may want to do just that: only
                 * request the acks if the packet is in, say, the first
                 * half of the window
	         */
		p->header.flags |= RX_REQUEST_ACK;
		peer->reSends++, rx_stats.dataPacketsReSent++;
		p->retryTime = retryTime;

		/*
	         * if a packet gets dropped, don't keep hammering on it --
	         * back off exponentially, at least up to a point.  I had
	         * to trade off global congestion avoidance against individual
	         * performance.  Note that most connections will time out
	         * after 20 - 60 seconds.  In pathological cases, retransmits
	         * must still continue to disperse.  For instance, there is
	         * a condition where the server discards received packets, but
	         * still sends keep-alives on the call, so the call may live
	         * much longer than 60 seconds.
	         */
		if (p->backoff < MAXBACKOFF)
		    p->backoff = (p->backoff << 1) + 1;	/* so it can't stay == 0 */
		else
		    p->backoff++;
		clock_Addmsec(&(p->retryTime), ((unsigned long) p->backoff) << 8);
		/* consider shrinking the packet size? XXXX */
		/* no, shrink the burst size.  LWSXXX */
	    } else {
		peer->nSent++, rx_stats.dataPacketsSent++;
		p->firstSent = now;    /* improved RTO calculation- not Karn */
		p->retryTime = retryTime;
		/*
	         * Ask for an ack for the first packet on a new
                 * connection, since it may carry some interesting info
                 * like maxReceiveSize.  It will also help us train to a
                 * new estimate of RTT, for good or bad.  This has one
	         * neat side effect:  since the first call on a connection
	         * usually triggers a challenge/response exchange, the
	         * first packet was often retransmitted before the call
	         * could complete.  Getting this ack prevents those
	         * retransmissions.  Admittedly, this is straining at gnats.
	         */
		if ((p->header.callNumber == 1) && (p->header.seq == 1) &&
		    (p->length >= call->conn->maxPacketSize)) {
		    p->header.flags |= RX_REQUEST_ACK;
		}
	    }

	    /*
	     * Install the new retransmit time for the packet, and
             * record the time sent
	     */
	    p->timeSent = now;

	    /*
	     * Send the previous packet, and remember this one--we don't
             * send it immediately, so we can tag it as requiring an ack
             * later, if necessary
	     */
	    if (peer->burstSize)
		peer->burst--;
	    nSent++;
	    if (lastPacket) {
		/*
	         * Tag this packet as not being the last in this group,
                 * for the receiver's benefit
	         */
		lastPacket->header.flags |= RX_MORE_PACKETS;
		rxi_Send(call, lastPacket);
	    }
	    lastPacket = p;
	}
	call->flags &= ~RX_CALL_TQ_BUSY;

	/*
	 * If any packets are to be sent, send them and post a
         * decongestion event to bump the burst count that we used up
         * sending the packets
	 */
	if (nSent) {

	    /*
	     * we don't ask for an ack on the final packet, since the
	     * response from the peer implicitly acks it, but we do wait a
	     * little longer for the ack on the last packet on server conns.
	     */
	    if ((lastPacket->header.flags & RX_LAST_PACKET) == 0) {

		/* 
		 * to get the window up faster we ack every packet as
		 * long as we are below the fast ack window, or if the
		 * client doesn't support slow start, every second packet
		 */
		if (/* call->cwind <= (u_short)call->ackRate || */
		    (!(call->flags & RX_CALL_SLOW_START_OK)
		     && (lastPacket->header.seq & 1))) {

			lastPacket->header.flags |= RX_REQUEST_ACK;
		    }
	    } else if (!(lastPacket->header.flags & RX_CLIENT_INITIATED))
		clock_Addmsec(&(lastPacket->retryTime), 400);

	    rxi_Send(call, lastPacket);
	    if (peer->burstSize)
		rxi_ScheduleDecongestionEvent(call, nSent);
	}

	/*
	 * Always post a resend event, if there is anything in the queue, and
	 * resend is possible.  There should be at least one unacknowledged
	 * packet in the queue ... otherwise none of these packets should be
	 * on the queue in the first place.
	 */
	if (call->resendEvent) {

	    /*
	     * If there's an existing resend event, then if its expiry time
	     * is sooner than the new one, then it must be less than any
	     * possible expiry time (because it represents all previous
	     * packets sent that may still need retransmitting).  In this
	     * case, just leave that event as scheduled
	     */
	    if (clock_Le(&call->resendEvent->eventTime, &retryTime))
		return;
	    /* Otherwise, cancel the existing event and post a new one */
	    rxevent_Cancel(call->resendEvent);
	}

	/*
	 * Loop to find the earliest event.  I *know* XXXX that this can be
	 * coded more elegantly (perhaps rolled into the above code)
	 */
	for (haveEvent = 0, queue_Scan(&call->tq, p, nxp, rx_packet)) {
	    if (!p->acked && !clock_IsZero(&p->retryTime)) {
		haveEvent = 1;
		if (clock_Lt(&p->retryTime, &retryTime))
		    retryTime = p->retryTime;
	    }
	}

	/* Post a new event to re-run rxi_Start when retries may be needed */
	if (haveEvent) {
	    call->resendEvent = rxevent_Post(&retryTime, rxi_Start, (void *) call, NULL);
	}
    }
}

/*
 * Also adjusts the keep alive parameters for the call, to reflect
 * that we have just sent a packet (so keep alives aren't sent
 * immediately)
 */
void
rxi_Send(struct rx_call *call, struct rx_packet *p)
{
    struct rx_connection *conn = call->conn;

    /* Stamp each packet with the user supplied status */
    p->header.userStatus = call->localStatus;

    /*
     * Allow the security object controlling this call's security to make any
     * last-minute changes to the packet
     */
    RXS_SendPacket(conn->securityObject, call, p);

    /* Actually send the packet, filling in more connection-specific fields */
    rxi_SendPacket(conn, p);

    /*
     * Update last send time for this call (for keep-alive processing), and
     * for the connection (so that we can discover idle connections)
     */
    conn->lastSendTime = call->lastSendTime = clock_Sec();

    /*
     * Since we've just sent SOME sort of packet to the peer, it's safe to
     * nuke any scheduled end-of-packets ack
     */
    rxevent_Cancel(call->delayedAckEvent);
}


/*
 * Check if a call needs to be destroyed.  Called by keep-alive code to ensure
 * that things are fine.  Also called periodically to guarantee that nothing
 * falls through the cracks (e.g. (error + dally) connections have keepalive
 * turned off.  Returns 0 if conn is well, negativ otherwise.
 * -1 means that the call still exists, -2 means that the call is freed.
 */

static int 
rxi_CheckCall(struct rx_call *call)
{
    struct rx_connection *conn = call->conn;
    struct rx_service *tservice;
    u_long now;

    now = clock_Sec();

    /*
     * These are computed to the second (+- 1 second).  But that's good
     * enough for these values, which should be a significant number of
     * seconds.
     */
    if (now > (call->lastReceiveTime + conn->secondsUntilDead)) {

	if (call->state == RX_STATE_ACTIVE) {
	    rxi_CallError(call, RX_CALL_DEAD);
	    return -1;
	} else {
	    rxi_FreeCall(call);
	    return -2;
	}

	/*
	 * Non-active calls are destroyed if they are not responding to
	 * pings; active calls are simply flagged in error, so the attached
	 * process can die reasonably gracefully.
	 */
	
    }
    /* see if we have a non-activity timeout */
    tservice = conn->service;
    if ((conn->type == RX_SERVER_CONNECTION) && call->startWait
	&& tservice->idleDeadTime
	&& ((call->startWait + tservice->idleDeadTime) < now)) {
	if (call->state == RX_STATE_ACTIVE) {
	    rxi_CallError(call, RX_CALL_TIMEOUT);
	    return -1;
	}
    }
    /* see if we have a hard timeout */
    if (conn->hardDeadTime && (now > (conn->hardDeadTime + call->startTime))) {
	if (call->state == RX_STATE_ACTIVE)
	    rxi_CallError(call, RX_CALL_TIMEOUT);
	return -1;
    }
    return 0;
}


/*
 * When a call is in progress, this routine is called occasionally to
 * make sure that some traffic has arrived (or been sent to) the peer.
 * If nothing has arrived in a reasonable amount of time, the call is
 * declared dead; if nothing has been sent for a while, we send a
 * keep-alive packet (if we're actually trying to keep the call alive)
 */
void 
rxi_KeepAliveEvent(struct rxevent *event, struct rx_call *call,
		   char *dummy)
{
    struct rx_connection *conn = call->conn;
    u_long now;

    call->keepAliveEvent = (struct rxevent *) 0;
    now = clock_Sec();

    if (rxi_CheckCall(call))
	return;

    /* Don't try to keep alive dallying calls */
    if ((call->state != RX_STATE_DALLY)
	&& ((now - call->lastSendTime) > conn->secondsUntilPing)) {
	/* Don't try to send keepalives if there is unacknowledged data */

	/*
	 * the rexmit code should be good enough, this little hack doesn't
	 * quite work LWSXXX
	 */
	(void) rxi_SendAck(call, 0, 0, 0, 0, RX_ACK_PING);
    }
    rxi_ScheduleKeepAliveEvent(call);
}


void 
rxi_ScheduleKeepAliveEvent(struct rx_call *call)
{
    if (!call->keepAliveEvent) {
	struct clock when;

	clock_GetTime(&when);
	when.sec += call->conn->secondsUntilPing;
	call->keepAliveEvent = rxevent_Post(&when, rxi_KeepAliveEvent, call, NULL);
    }
}

/* N.B. rxi_KeepAliveOff:  is defined earlier as a macro */
void 
rxi_KeepAliveOn(struct rx_call *call)
{

    /*
     * Pretend last packet received was received now--i.e. if another packet
     * isn't received within the keep alive time, then the call will die;
     * Initialize last send time to the current time--even if a packet hasn't
     * been sent yet.  This will guarantee that a keep-alive is sent within
     * the ping time
     */
    call->lastReceiveTime = call->lastSendTime = clock_Sec();
    rxi_ScheduleKeepAliveEvent(call);
}

/*
 * This routine is called periodically (every RX_AUTH_REQUEST_TIMEOUT
 * seconds) to ask the client to authenticate itself.  The routine
 * issues a challenge to the client, which is obtained from the
 * security object associated with the connection
 */
void 
rxi_ChallengeEvent(struct rxevent *event, struct rx_connection *conn,
		   char *dummy)
{
    conn->challengeEvent = (struct rxevent *) 0;
    if (RXS_CheckAuthentication(conn->securityObject, conn) != 0) {
	struct rx_packet *packet;

	packet = rxi_AllocPacket(RX_PACKET_CLASS_SPECIAL);
	if (!packet)
	    osi_Panic("rxi_ChallengeEvent");
	RXS_GetChallenge(conn->securityObject, conn, packet);
	rxi_SendSpecial((struct rx_call *) 0, conn, packet,
			RX_PACKET_TYPE_CHALLENGE, (char *) 0, -1);
	rxi_FreePacket(packet);
	rxi_resend_ChallengeEvent(conn);
    }
}

/*
 * Call this routine to start requesting the client to authenticate
 * itself.  This will continue until authentication is established,
 * the call times out, or an invalid response is returned.  The
 * security object associated with the connection is asked to create
 * the challenge at this time.  N.B.  rxi_ChallengeOff is a macro,
 * defined earlier.
 */
void 
rxi_ChallengeOn(struct rx_connection *conn)
{
    if (!conn->challengeEvent) {
	RXS_CreateChallenge(conn->securityObject, conn);
	rxi_ChallengeEvent((struct rxevent *) 0, conn, 0);
    };
}

/*
 * Called by event.c when a decongestion event (setup by
 * rxi_CongestionWait) occurs.  This adds back in to the burst count
 * for the specified host the number of packets that were sent at the
 * time the event was scheduled.  It also calls rxi_Start on as many
 * waiting calls as possible before the burst count goes down to zero,
 * again.
 */
static void 
rxi_DecongestionEvent(struct rxevent *event, struct rx_peer *peer,
		      int nPackets)
{
    struct rx_call *call;
    struct rx_call *nxcall;   /* Next pointer for queue_Scan */

    peer->burst += nPackets;
    if (peer->burst > peer->burstSize)
	peer->burst = peer->burstSize;
    for (queue_Scan(&peer->congestionQueue, call, nxcall, rx_call)) {
	assert(queue_IsNotEmpty(&peer->congestionQueue));
	assert(queue_Prev(&peer->congestionQueue, rx_call));
	queue_Remove(call);

	/*
	 * The rxi_Start may put the call back on the congestion queue.  In
	 * that case, peer->burst should be 0 (otherwise no congestion was
	 * encountered).  It should go on the end of the queue, to allow
	 * other calls to proceed when the next burst is allowed
	 */
	rxi_Start((struct rxevent *) 0, call);
	if (!peer->burst)
	    goto done;
    }
 done:
    peer->refCount--;		       /* It was bumped by the callee */
    return;
}

/*
 * Schedule an event at a host-dependent time in the future which will
 * add back nPackets to the current allowed burst window.  Any number
 * of these events may be scheduled.
 */
void 
rxi_ScheduleDecongestionEvent(struct rx_call *call, int nPackets)
{
    struct rx_peer *peer = call->conn->peer;
    struct clock tmp;

    clock_GetTime(&tmp);
    clock_Add(&tmp, &peer->burstWait);
    peer->refCount++;		       /* So it won't disappear underneath
				        * us! */
    /* this is stupid  - sending an int as a pointer is begging for trouble */
    rxevent_Post(&tmp, rxi_DecongestionEvent, (void *) peer, (void *)nPackets);
}

/*
 * The caller wishes to have rxi_Start called when the burst count has
 * gone up, and more packets can therefore be sent.  Add the caller to
 * the end of the list of calls waiting for decongestion events to
 * happen.  It's important that it's added to the end so that the
 * rxi_DecongestionEvent procedure always terminates (aside from
 * matters of scheduling fairness).
 */
void 
rxi_CongestionWait(struct rx_call *call)
{
    if (queue_IsOnQueue(call))
	return;
    assert(queue_IsNotEmpty(&call->conn->peer->congestionQueue));
    assert(queue_Prev(&call->conn->peer->congestionQueue, rx_call));
    queue_Append(&call->conn->peer->congestionQueue, call);
}

/*
 * Compute round trip time of the packet provided, in *rttp.
 */
#ifdef ADAPT_PERF
#define ADAPT_RTO
#endif

void 
rxi_ComputeRoundTripTime(struct rx_packet *p, 
			 struct clock *sentp, 
			 struct rx_peer *peer)
{
    struct clock thisRtt, *rttp = &thisRtt;

#ifdef	ADAPT_RTO
    static char id[] = "@(#)adaptive RTO";
    *id = *id; /* so it won't complain about unsed variables */

    clock_GetTime(rttp);
    if (clock_Lt(rttp, sentp)) {
	clock_Zero(rttp);
	return;			       /* somebody set the clock back, don't
				        * count this time. */
    }
    clock_Sub(rttp, sentp);
#else
    clock_GetTime(rttp);
    clock_Sub(rttp, &p->timeSent);
#endif
    if (clock_Lt(rttp, &rx_stats.minRtt))
	rx_stats.minRtt = *rttp;
    if (clock_Gt(rttp, &rx_stats.maxRtt)) {
	if (rttp->sec > 110)
	    return;		       /* somebody set the clock ahead */
	rx_stats.maxRtt = *rttp;
    }
    clock_Add(&rx_stats.totalRtt, rttp);
    rx_stats.nRttSamples++;

#ifdef	ADAPT_RTO
    /* better rtt calculation courtesy of UMich crew (dave,larry,peter,???) */

    /* Apply VanJacobson round-trip estimations */
    if (peer->srtt) {
	u_long rtt;
	u_long err;

	rtt = rttp->usec + rttp->sec*1000000;
	if (rtt >= peer->srtt)
	    err = rtt - peer->srtt;
	else
	    err = peer->srtt - rtt;

	/*
	 * The following magic is equivalent to the smoothing
	 * algorithm in rfc793 with an alpha of .875
	 * (srtt = rtt/8 + srtt*7/8 in fixed point).
	 */

	peer->srtt = (peer->srtt*7 + rtt)/8;

	/*
	 * We accumulate a smoothed rtt variance (actually, a smoothed
	 * mean difference), then set the retransmit timer to smoothed
         * rtt + 4 times the smoothed variance (was 2x in van's
         * original paper, but 4x works better for me, and apparently
         * for him as well).
	 *
	 * The following is equivalent to rfc793 smoothing with an
	 * alpha of .75 (rttvar = rttvar*3/4 + |err| / 4). This
	 * replaces rfc793's wired-in beta.
	 */

	peer->mdev = (peer->mdev*3 + err)/4;
    } else {
        peer->srtt = rttp->usec + rttp->sec*1000000;
	peer->mdev = peer->srtt/2;
	/* One single measurement is a real poor estimate of RTT&MDEV */
	if (peer->mdev < 1000)
	  peer->mdev = 1000;	/* 1ms */
    }

    update_timeout(peer);

    dpf(("rtt=%.2f ms, srtt=%.2f ms, mdev=%.2f ms, timeout=%.2f ms\n",
	 rttp->usec/1000.0 + rttp->sec*1000.0,
	 peer->srtt/1000.0, peer->mdev/1000.0,
	 peer->timeout.sec*1000.0 + peer->timeout.usec/1000.0));
#endif				       /* ADAPT_RTO */
}


/*
 * Find all server connections that have not been active for a long time,
 * and toss them
 */
void 
rxi_ReapConnections(void)
{
    struct clock now;

    clock_GetTime(&now);

    /*
     * Find server connection structures that haven't been used for greater
     * than rx_idleConnectionTime
     */
    {
	struct rx_connection **conn_ptr, **conn_end;
	int i, havecalls = 0, ret;

	for (conn_ptr = &rx_connHashTable[0], 
		 conn_end = &rx_connHashTable[rx_hashTableSize];
	     conn_ptr < conn_end; 
	     conn_ptr++) {
	    struct rx_connection *conn, *next;

	rereap:
	    for (conn = *conn_ptr; conn; conn = next) {
		next = conn->next;
		/* once a minute look at everything to see what's up */
		havecalls = 0;
		for (i = 0; i < RX_MAXCALLS; i++) {
		    if (conn->call[i]) {
			havecalls = 1;
			ret = rxi_CheckCall(conn->call[i]);
			if (ret == -2) {
			    /* If CheckCall freed the call, it might
			     * have destroyed  the connection as well,
			     * which screws up the linked lists.
			     */
			    goto rereap;
			}
		    }
		}
		if (conn->type == RX_SERVER_CONNECTION) {

		    /*
		     * This only actually destroys the connection if there
		     * are no outstanding calls
		     */
		    if (!havecalls && !conn->refCount &&
			((conn->lastSendTime + rx_idleConnectionTime) < now.sec)) {
			conn->refCount++;	/* it will be decr in
						 * rx_DestroyConn */
			rx_DestroyConnection(conn);
		    }
		}
	    }
	}
    }

    /*
     * Find any peer structures that haven't been used (haven't had an
     * associated connection) for greater than rx_idlePeerTime
     */
    {
	struct rx_peer **peer_ptr, **peer_end;

	for (peer_ptr = &rx_peerHashTable[0],
	     peer_end = &rx_peerHashTable[rx_hashTableSize];
	     peer_ptr < peer_end; peer_ptr++) {
	    struct rx_peer *peer, *next;

	    for (peer = *peer_ptr; peer; peer = next) {
		next = peer->next;
		if (peer->refCount == 0
		    && ((peer->idleWhen + rx_idlePeerTime) < now.sec)) {
		    rxi_DestroyPeer(peer);
		}
	    }
	}
    }

    /*
     * THIS HACK IS A TEMPORARY HACK.  The idea is that the race condition in
     * rxi_AllocSendPacket, if it hits, will be handled at the next conn GC,
     * just below.  Really, we shouldn't have to keep moving packets from one
     * place to another, but instead ought to always know if we can afford to
     * hold onto a packet in its particular use.
     */
    RX_MUTEX_ENTER(&rx_waitingForPackets_lock);
    if (rx_waitingForPackets) {
	rx_waitingForPackets = 0;
#ifdef	RX_ENABLE_LOCKS
	cv_signal(&rx_waitingForPackets_cv);
#else
	osi_rxWakeup(&rx_waitingForPackets);
#endif
    }
    RX_MUTEX_EXIT(&rx_waitingForPackets_lock);

    now.sec += RX_REAP_TIME;	       /* Check every RX_REAP_TIME seconds */
    rxevent_Post(&now, rxi_ReapConnections, NULL, NULL);
}


/*
 * rxs_Release - This isn't strictly necessary but, since the macro name from
 * rx.h is sort of strange this is better.  This is called with a security
 * object before it is discarded.  Each connection using a security object has
 * its own refcount to the object so it won't actually be freed until the last
 * connection is destroyed.
 *
 * This is the only rxs module call.  A hold could also be written but no one
 * needs it.
 */

int 
rxs_Release(struct rx_securityClass *aobj)
{
    return RXS_Close(aobj);
}

#ifdef ADAPT_WINDOW
#define	RXRATE_PKT_OH	(RX_HEADER_SIZE + RX_IPUDP_SIZE)
#define	RXRATE_SMALL_PKT    (RXRATE_PKT_OH + sizeof(struct rx_ackPacket))
#define	RXRATE_AVG_SMALL_PKT	(RXRATE_PKT_OH + (sizeof(struct rx_ackPacket)/2))
#define	RXRATE_LARGE_PKT    (RXRATE_SMALL_PKT + 256)

/*
 * Adjust our estimate of the transmission rate to this peer, given
 * that the packet p was just acked. We can adjust peer->timeout and
 * call->twind (and peer->maxWindow). Pragmatically, this is called
 * only with packets of maximal length.
 */

static void 
rxi_ComputeRate(struct rx_peer *peer, struct rx_call *call,
		struct rx_packet *p, struct rx_packet *ackp, u_char ackReason)
{
    long xferSize, xferMs;
    long minTime;
    struct clock newTO;

    /* Count down packets */
    if (peer->rateFlag > 0)
	peer->rateFlag--;
    /* Do nothing until we're enabled */
    if (peer->rateFlag != 0)
	return;
    if (!call->conn)
	return;

    /* Count only when the ack seems legitimate */
    switch (ackReason) {
    case RX_ACK_REQUESTED:
	xferSize = p->length + RX_HEADER_SIZE +
	    call->conn->securityMaxTrailerSize;
	xferMs = peer->rtt;
	break;

    case RX_ACK_PING_RESPONSE:
	if (p)			       /* want the response to ping-request,
				        * not data send */
	    return;
	clock_GetTime(&newTO);
	if (clock_Gt(&newTO, &call->pingRequestTime)) {
	    clock_Sub(&newTO, &call->pingRequestTime);
	    xferMs = (newTO.sec * 1000) + (newTO.usec / 1000);
	} else {
	    return;
	}
	xferSize = rx_AckDataSize(rx_Window) + RX_HEADER_SIZE;
	break;

    default:
	return;
    }

    dpf(("CONG peer %lx/%u: sample (%s) size %ld, %ld ms (to %lu.%06lu, "
         "rtt %u, win %u, ps %u)",
	 ntohl(peer->host), ntohs(peer->port),
	 (ackReason == RX_ACK_REQUESTED ? "dataack" : "pingack"),
	 xferSize, xferMs, peer->timeout.sec, peer->timeout.usec, peer->smRtt,
	 peer->maxWindow, peer->packetSize));

    /* Track only packets that are big enough. */
    if ((p->length + RX_HEADER_SIZE + call->conn->securityMaxTrailerSize) <
	peer->packetSize)
	return;

    /* absorb RTT data (in milliseconds) for these big packets */
    if (peer->smRtt == 0) {
	peer->smRtt = xferMs;
    } else {
	peer->smRtt = ((peer->smRtt * 15) + xferMs + 4) >> 4;
	if (!peer->smRtt)
	    peer->smRtt = 1;
    }

    if (peer->countDown) {
	peer->countDown--;
	return;
    }
    peer->countDown = 10;	       /* recalculate only every so often */

#if 0

    /*
     * We here assume that we can approximate the total elapsed time for a
     * window-full of full packets as: time = RTT + ((winSize *
     * (packetSize+overhead)) - minPktSize) / byteRate
     */
    /* The RTT and byteRate numbers are what is measured above. */

    /*
     * In principle, we can change the other parameters: - winSize, the
     * number of packets in the transmission window; - packetSize, the max
     * size of a data packet; - the timeout, which must be larger than the
     * expected time.
     */

    /*
     * In practice, we do this in two steps: (a) ensure that the timeout is
     * large enough for a single packet to get through; (b) ensure that the
     * transmit-window is small enough to fit in the timeout.
     */

    /* First, an expression for the expected RTT for a full packet */
    minTime = peer->smRtt + ((1000 * (peer->packetSize +
			    RX_HEADER_SIZE + RX_IPUDP_SIZE)) / peer->smBps);

    /* Get a reasonable estimate for a timeout period */
    minTime += minTime;
    newTO.sec = minTime / 1000;
    newTO.usec = (minTime - (newTO.sec * 1000)) * 1000;

    /*
     * Increase the timeout period so that we can always do at least one
     * packet exchange
     */
    if (clock_Gt(&newTO, &peer->timeout)) {

	dpf(("CONG peer %lx/%u: timeout %lu.%06lu ==> %lu.%06lu "
	     "(rtt %u, win %u, ps %u, Bps %u)",
	     ntohl(peer->host), ntohs(peer->port), peer->timeout.sec,
	     peer->timeout.usec, newTO.sec, newTO.usec, peer->smRtt,
	     peer->maxWindow, peer->packetSize, peer->smBps));

	peer->timeout = newTO;
    }
    /* Now, get an estimate for the transmit window size. */
    minTime = peer->timeout.sec * 1000 + (peer->timeout.usec / 1000);

    /*
     * Now, convert to the number of full packets that could fit in that
     * interval
     */
    minTime = ((((minTime - peer->smRtt) * peer->smBps) / 1000) +
	       RXRATE_AVG_SMALL_PKT) /
	(peer->packetSize + RX_HEADER_SIZE + RX_IPUDP_SIZE);
    minTime >>= 1;		       /* Take half that many */
    xferSize = minTime;		       /* (make a copy) */

    /* Now clamp the size to reasonable bounds. */
    if (minTime <= 1)
	minTime = 1;
    else if (minTime > rx_Window)
	minTime = rx_Window;
    if (minTime != peer->maxWindow) {
	dpf(("CONG peer %lx/%u: windowsize %lu ==> %lu (to %lu.%06lu, "
	     "rtt %u, ps %u, Bps %u)",
	     ntohl(peer->host), ntohs(peer->port), peer->maxWindow, minTime,
	     peer->timeout.sec, peer->timeout.usec, peer->smRtt,
	     peer->packetSize, peer->smBps));

	peer->maxWindow = minTime;
	/* call->twind = minTime; */
    }

    /*
     * Cut back on the peer timeout if it has grown unreasonably. Discern
     * this by calculating the timeout necessary for rx_Window packets.
     */
    if ((xferSize > rx_Window) && (peer->timeout.sec >= 3)) {
	/* calculate estimate for transmission interval in milliseconds */
	minTime = (((1000 * rx_Window *
		     (peer->packetSize + RX_HEADER_SIZE + RX_IPUDP_SIZE))
		    - RXRATE_AVG_SMALL_PKT) / peer->smBps) + peer->smRtt;
	if (minTime < 1000) {

	    dpf(("CONG peer %lx/%u: cut TO %lu.%06lu by 0.5 (rtt %u, "
		 "win %u, ps %u, Bps %u)",
		 ntohl(peer->host), ntohs(peer->port), peer->timeout.sec,
		 peer->timeout.usec, peer->smRtt, peer->maxWindow,
		 peer->packetSize, peer->smBps));

	    newTO.sec = 0;	       /* cut back on timeout by half a
				        * second */
	    newTO.usec = 500000;
	    clock_Sub(&peer->timeout, &newTO);
	}
    }
#endif				       /* 0 */

    /*
     * In practice, we can measure only the RTT for full packets, because of
     * the way Rx acks the data that it receives.  (If it's smaller than a
     * full packet, it often gets implicitly acked either by the call
     * response (from a server) or by the next call (from a client), and
     * either case confuses transmission times with processing times.)
     * Therefore, replace the above more-sophisticated processing with a
     * simpler version, where the smoothed RTT is kept for full-size packets,
     * and the time to transmit a windowful of full-size packets is simply
     * RTT * windowSize. Again, we take two steps: - ensure the timeout is
     * large enough for a single packet's RTT; - ensure that the window is
     * small enough to fit in the desired timeout.
     */

    /* First, the timeout check. */
    minTime = peer->smRtt;
    /* Get a reasonable estimate for a timeout period */
    minTime += minTime;
    newTO.sec = minTime / 1000;
    newTO.usec = (minTime - (newTO.sec * 1000)) * 1000;

    /*
     * Increase the timeout period so that we can always do at least one
     * packet exchange
     */
    if (clock_Gt(&newTO, &peer->timeout)) {

	dpf(("CONG peer %lx/%u: timeout %lu.%06lu ==> %lu.%06lu (rtt %u, "
	     "win %u, ps %u)",
	     ntohl(peer->host), ntohs(peer->port), peer->timeout.sec,
	     peer->timeout.usec, newTO.sec, newTO.usec, peer->smRtt,
	     peer->maxWindow, peer->packetSize));

	peer->timeout = newTO;
    }
    /* Now, get an estimate for the transmit window size. */
    minTime = peer->timeout.sec * 1000 + (peer->timeout.usec / 1000);

    /*
     * Now, convert to the number of full packets that could fit in a
     * reasonable fraction of that interval
     */
    minTime /= (peer->smRtt << 1);
    xferSize = minTime;		       /* (make a copy) */

    /* Now clamp the size to reasonable bounds. */
    if (minTime <= 1)
	minTime = 1;
    else if (minTime > rx_Window)
	minTime = rx_Window;
    if (minTime != peer->maxWindow) {
	dpf(("CONG peer %lx/%u: windowsize %lu ==> %lu (to %lu.%06lu, "
	     "rtt %u, ps %u)",
	     ntohl(peer->host), ntohs(peer->port), peer->maxWindow, minTime,
	     peer->timeout.sec, peer->timeout.usec, peer->smRtt,
	     peer->packetSize));
	peer->maxWindow = minTime;
	/* call->twind = minTime; */
    }

    /*
     * Cut back on the peer timeout if it had earlier grown unreasonably.
     * Discern this by calculating the timeout necessary for rx_Window
     * packets.
     */
    if ((xferSize > rx_Window) && (peer->timeout.sec >= 3)) {
	/* calculate estimate for transmission interval in milliseconds */
	minTime = rx_Window * peer->smRtt;
	if (minTime < 1000) {
	    dpf(("CONG peer %lx/%u: cut TO %lu.%06lu by 0.5 (rtt %u, "
		 "win %u, ps %u)",
		 ntohl(peer->host), ntohs(peer->port), peer->timeout.sec,
		 peer->timeout.usec, peer->smRtt, peer->maxWindow,
		 peer->packetSize));

	    newTO.sec = 0;	       /* cut back on timeout by half a
				        * second */
	    newTO.usec = 500000;
	    clock_Sub(&peer->timeout, &newTO);
	}
    }
    return;
}				       /* end of rxi_ComputeRate */

#endif				       /* ADAPT_WINDOW */






#ifdef RXDEBUG
/* Don't call this debugging routine directly; use dpf */
void
rxi_DebugPrint(const char *fmt, ...)
{
    struct clock now;
    va_list ap;

    clock_GetTime(&now);
    
    fprintf(Log, " %lu.%.3lu:", now.sec, now.usec / 1000);
    va_start(ap, fmt);
    vfprintf(Log, fmt, ap);
    va_end(ap);
    putc('\n', Log);
}

#endif

#if defined(RXDEBUG)
void 
rx_PrintTheseStats(FILE *file, struct rx_stats *s, int size)
{
    int i;

    if (size != sizeof(struct rx_stats))
	fprintf(file, "Unexpected size of stats structure: was %d, "
		"expected %d\n", 
		size, 
		(int)sizeof(struct rx_stats));

    fprintf(file, 
	    "rx stats: free packets %d, allocs %d, "
	    "alloc-failures(rcv %d,send %d,ack %d)\n", rx_nFreePackets,
	    s->packetRequests, s->noPackets[0], s->noPackets[1],
	    s->noPackets[2]);
    fprintf(file, 
	    "   greedy %d, bogusReads %d (last from host %x), "
	    "noPackets %d, noBuffers %d, selects %d, sendSelects %d\n", 
	    s->socketGreedy, s->bogusPacketOnRead, s->bogusHost, 
	    s->noPacketOnRead, s->noPacketBuffersOnRead, s->selects, 
	    s->sendSelects);
    fprintf(file, "   packets read: ");
    for (i = 0; i < RX_N_PACKET_TYPES; i++)
	fprintf(file, "%s %d ", rx_packetTypes[i], s->packetsRead[i]);

    fprintf(file, "\n");
    fprintf(file,
	    "   other read counters: data %d, ack %d, dup %d "
	    "spurious %d\n", s->dataPacketsRead, s->ackPacketsRead,
	    s->dupPacketsRead, s->spuriousPacketsRead);
    fprintf(file, "   packets sent: ");
    for (i = 0; i < RX_N_PACKET_TYPES; i++)
	fprintf(file, "%s %d ", rx_packetTypes[i], s->packetsSent[i]);
    fprintf(file, "\n");
    fprintf(file,
	    "   other send counters: ack %d, data %d (not resends), "
	    "resends %d, pushed %d, acked&ignored %d\n", s->ackPacketsSent,
	    s->dataPacketsSent, s->dataPacketsReSent, s->dataPacketsPushed,
	    s->ignoreAckedPacket);
    fprintf(file,
	    "   \t(these should be small) sendFailed %lu, "
	    "fatalErrors %lu\n",
	    (unsigned long)s->netSendFailures,
	    (unsigned long)s->fatalErrors);
    if (s->nRttSamples) {
	fprintf(file, "   Average rtt is %0.3f, with %d samples\n",
		clock_Float(&s->totalRtt) / s->nRttSamples, s->nRttSamples);

	fprintf(file, "   Minimum rtt is %0.3f, maximum is %0.3f\n",
		clock_Float(&s->minRtt), clock_Float(&s->maxRtt));
    }
    fprintf(file,
	    "   %d server connections, %d client connections, %d "
	    "peer structs, %d call structs, %d free call structs\n",
	    s->nServerConns, s->nClientConns, s->nPeerStructs,
	    s->nCallStructs, s->nFreeCallStructs);
    fprintf(file, "   %d clock updates\n", clock_nUpdates);
}

/* for backward compatibility */
void 
rx_PrintStats(FILE *file)
{
    rx_PrintTheseStats(file, &rx_stats, sizeof(rx_stats));
}

void 
rx_PrintPeerStats(FILE *file, struct rx_peer *peer)
{
    fprintf(file, "Peer %lx.%d.  Burst size %d, burst wait %lu.%ld.\n",
	    (unsigned long)ntohl(peer->host), 
	    peer->port, 
	    peer->burstSize,
	    (unsigned long)peer->burstWait.sec, 
	    (unsigned long)peer->burstWait.usec);
    fprintf(file, "   Rtt %lu us, retry time %lu.%06ld, total sent %d, resent %d\n",
	    peer->srtt, 
	    (unsigned long)peer->timeout.sec, 
	    (long)peer->timeout.usec, 
	    peer->nSent,
	    peer->reSends);
    fprintf(file,
	    "   Packet size %d, max in packet skew %ld, max out packet "
	    "skew %ld\n", peer->packetSize, peer->inPacketSkew,
	    peer->outPacketSkew);
}

#endif				       /* RXDEBUG */

void
shutdown_rx(void)
{
    struct rx_serverQueueEntry *np;
    int i, j;

    rxinit_status = 0;
    {
	struct rx_peer **peer_ptr, **peer_end;

	for (peer_ptr = &rx_peerHashTable[0],
	     peer_end = &rx_peerHashTable[rx_hashTableSize];
	     peer_ptr < peer_end; peer_ptr++) {
	    struct rx_peer *peer, *next;

	    for (peer = *peer_ptr; peer; peer = next) {
		next = peer->next;
		rxi_DestroyPeer(peer);
	    }
	}
    }
    for (i = 0; i < RX_MAX_SERVICES; i++) {
	if (rx_services[i])
	    rxi_Free(rx_services[i], sizeof(*rx_services));
    }
    for (i = 0; i < rx_hashTableSize; i++) {
	struct rx_connection *tc, *ntc;

	for (tc = rx_connHashTable[i]; tc; tc = ntc) {
	    ntc = tc->next;
	    for (j = 0; j < RX_MAXCALLS; j++) {
		if (tc->call[j]) {
		    rxi_Free(tc->call[j], sizeof(*(tc->call)));
		}
	    }
	    rxi_Free(tc, sizeof(tc));
	}
    }

    RX_MUTEX_ENTER(&freeSQEList_lock);

    while ((np = rx_FreeSQEList) != NULL) {
	rx_FreeSQEList = *(struct rx_serverQueueEntry **) np;
	RX_MUTEX_DESTROY(&np->lock);
	rxi_Free(np, sizeof(np));
    }

    RX_MUTEX_EXIT(&freeSQEList_lock);
    RX_MUTEX_DESTROY(&freeSQEList_lock);
    RX_MUTEX_DESTROY(&rx_waitingForPackets_lock);
    RX_MUTEX_DESTROY(&rx_freeCallQueue_lock);

    osi_Free(rx_connHashTable, 
	     rx_hashTableSize * sizeof(struct rx_connection *));
    osi_Free(rx_peerHashTable, rx_hashTableSize * sizeof(struct rx_peer *));
    osi_Free(rx_allocedP, sizeof(struct rx_packet) * rx_nPackets);

    UNPIN(rx_connHashTable, rx_hashTableSize * sizeof(struct rx_connection *));
    UNPIN(rx_peerHashTable, rx_hashTableSize * sizeof(struct rx_peer *));
    UNPIN(rx_allocedP, sizeof(struct rx_packet) * rx_nPackets);

    rxi_FreeAllPackets();

    rxi_dataQuota = RX_MAX_QUOTA;
    rxi_availProcs = rxi_totalMin = rxi_minDeficit = 0;
}

void *
rx_getServiceRock(struct rx_service *service)
{
    return service->serviceRock;
}

void
rx_setServiceRock(struct rx_service *service, void *rock)
{
    service->serviceRock = rock;
}
