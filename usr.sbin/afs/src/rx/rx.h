/* $arla: rx.h,v 1.28 2003/01/19 08:49:53 lha Exp $ */

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

#ifndef	_RX_
#define _RX_

#ifdef	KERNEL
#include "../rx/rx_machdep.h"
#include "../rx/rx_kernel.h"
#include "../rx/rx_clock.h"
#include "../rx/rx_event.h"
#include "../rx/rx_queue.h"
#include "../rx/rx_packet.h"
#include "../rx/rxgencon.h"
#else				       /* KERNEL */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <atypes.h>
#include <stdio.h>
#include <sys/param.h>
#include "rx_mach.h"
#include "rx_user.h"
#include "rx_clock.h"
#include "rx_event.h"
#include "rx_pkt.h"
#include "rxgencon.h"
#endif				       /* KERNEL */


/* Configurable parameters */
#define	RX_IDLE_DEAD_TIME	60     /* default idle dead time */
#define	RX_MAX_SERVICES		20     /* Maximum number of services that may
				        * be installed */
#define	RX_DEFAULT_STACK_SIZE	16000  /* Default process stack size;
				        * overriden by rx_SetStackSize */

/* This parameter should not normally be changed */
#define	RX_PROCESS_PRIORITY	LWP_NORMAL_PRIORITY

/* backoff is fixed point binary.  Ie, units of 1/4 seconds */
#define MAXBACKOFF 0x1F

struct rx_securityClass;
struct rx_peer;

/* Exported interfaces XXXX clean this up:  not all of these are exported */
int rx_Init(u_short);
struct rx_service *rx_NewService(u_short, u_short, char *, 
				 struct rx_securityClass **, int, int32_t (*)());
struct rx_connection *rx_NewConnection(uint32_t, u_short, u_short,
				       struct rx_securityClass *, int);
struct rx_call *
		rx_NewCall(struct rx_connection *);
struct rx_call *
		rx_GetCall(void); /* Not normally used, but not obsolete */
int32_t		rx_EndCall(struct rx_call *, int32_t);
int		rx_AllocPackets(void);
void		rx_FreePackets(void);
int 		rx_Write(struct rx_call *, const void *, int);
int 		rx_Read(struct rx_call *, void *, int);
void		rx_FlushWrite(struct rx_call *);

#ifdef RXDEBUG
void rx_PrintStats(FILE *);
#else
#define rx_PrintStats(a)
#endif
void rx_PrintPeerStats(FILE *file, struct rx_peer *peer);
void rx_SetArrivalProc(struct rx_call * call, void (*proc) (),
		       void *handle, void *arg);
void rx_Finalize(void);
void rx_StartServer(int);
void rx_DestroyConnection(struct rx_connection *);
void rxi_Free(void *, int);
int rxi_GetCallNumberVector(const struct rx_connection *, int32_t *);
int rxi_SetCallNumberVector(struct rx_connection *, int32_t *);
void rx_SetEpoch(uint32_t);

void shutdown_rx(void);



#define	RX_WAIT	    1
#define	RX_DONTWAIT 0

#define	rx_ConnectionOf(call)		((call)->conn)
#define	rx_PeerOf(conn)			((conn)->peer)
#define	rx_HostOf(peer)			((peer)->host)
#define	rx_PortOf(peer)			((peer)->port)
#define	rx_SetLocalStatus(call, status)	((call)->localStatus = (status))
#define rx_GetLocalStatus(call, status) ((call)->localStatus)
#define	rx_GetRemoteStatus(call)	((call)->remoteStatus)
#define	rx_SetCallError(call,status)	((call)->error = (status))
#define	rx_GetCallError(call)		((call)->error)
#define	rx_Error(call)			((call)->error)
#define	rx_ConnError(conn)		((conn)->error)
#define	rx_IsServerConn(conn)		((conn)->type == RX_SERVER_CONNECTION)
#define	rx_IsClientConn(conn)		((conn)->type == RX_CLIENT_CONNECTION)
/* Don't use these; use the IsServerConn style */
#define	rx_ServerConn(conn)		((conn)->type == RX_SERVER_CONNECTION)
#define	rx_ClientConn(conn)		((conn)->type == RX_CLIENT_CONNECTION)
#define rx_IsUsingPktCksum(conn)	((conn)->flags & \
					 RX_CONN_USING_PACKET_CKSUM)

/*
 * Set and get rock is applicable to both connections and calls.
 * It's used by multi rx macros for calls.
 */
#define	rx_SetRock(obj, newrock)	((obj)->rock = (void *)(newrock))
#define	rx_GetRock(obj,	type)		((type)(obj)->rock)
#define rx_ServiceIdOf(conn)		((conn)->serviceId)
#define	rx_SecurityClassOf(conn)	((conn)->securityIndex)
#define rx_SecurityObjectOf(conn)	((conn)->securityObject)

/*
 * Macros callable by the user to further define attributes of a
 * service.  Must be called before rx_StartServer
 */

/*
 *  Set the service stack size.  This currently just sets the stack
 * size for all processes to be the maximum seen, so far
 */
#define rx_SetStackSize(service, stackSize) \
  rx_stackSize = (((stackSize) > rx_stackSize)? stackSize: rx_stackSize)

/*
 * Set minimum number of processes guaranteed to be available for this
 * service at all times
 */
#define rx_SetMinProcs(service, min) ((service)->minProcs = (min))

/*
 * Set maximum number of processes that will be made available to this
 * service (also a guarantee that this number will be made available
 * if there is no competition)
 */
#define rx_SetMaxProcs(service, max) ((service)->maxProcs = (max))

/*
 * Define a procedure to be called just before a server connection is
 * destroyed
 */
#define rx_SetDestroyConnProc(service,proc) ((service)->destroyConnProc = (proc))

/* Define procedure to set service dead time */
#define rx_SetIdleDeadTime(service,time) ((service)->idleDeadTime = (time))

/*
 * Define procedures for getting and setting before and after execute-request
 * procs
 */
#define rx_SetAfterProc(service,proc) ((service)->afterProc = (proc))
#define rx_SetBeforeProc(service,proc) ((service)->beforeProc = (proc))
#define rx_GetAfterProc(service) ((service)->afterProc)
#define rx_GetBeforeProc(service) ((service)->beforeProc)

/* Define a procedure to be called when a server connection is created */
#define rx_SetNewConnProc(service, proc) ((service)->newConnProc = (proc))

/*
 * NOTE:  We'll probably redefine the following three routines, again,
 * sometime.
 */

/*
 * Set the connection dead time for any connections created for this service
 * (server only)
 */
#define rx_SetServiceDeadTime(service, seconds) ((service)->secondsUntilDead = (seconds))

/* Set connection dead time, for a specific client or server connection */
#define rx_SetConnDeadTime(conn, seconds) (rxi_SetConnDeadTime(conn, seconds))
extern void rxi_SetConnDeadTime(struct rx_connection * conn, int seconds);

/* Set connection hard timeout for a connection */
#define rx_SetConnHardDeadTime(conn, seconds) ((conn)->hardDeadTime = (seconds))

/*
 * Set rx default connection dead time; set on both services and
 * connections at creation time
 */
extern int rx_connDeadTime;

#define	rx_SetRxDeadTime(seconds)   (rx_connDeadTime = (seconds))

extern int rx_nPackets;

#define cpspace(call) \
  ((call)->currentPacket->wirevec[(call)->curvec].iov_len - (call)->curpos)
#define cppos(call) \
  ((call)->currentPacket->wirevec[(call)->curvec].iov_base + (call)->curpos)

/*
 * This is the maximum size data packet that can be sent on this connection,
 * accounting for security module-specific overheads.
 */
#define	rx_MaxUserDataSize(conn) ((conn)->maxPacketSize - \
				  RX_HEADER_SIZE - \
				  (conn)->securityHeaderSize - \
				  (conn)->securityMaxTrailerSize)

struct rx_securityObjectStats {
    char type;			       /* 0:unk 1:null,2:vab 3:kad */
    char level;
    char sparec[10];		       /* force correct alignment */
    uint32_t flags;		       /* 1=>unalloc, 2=>auth, 4=>expired */
    uint32_t expires;
    uint32_t packetsReceived;
    uint32_t packetsSent;
    uint32_t bytesReceived;
    uint32_t bytesSent;
    uint16_t spares[4];
    uint32_t sparel[8];
};

/*
 * XXXX (rewrite this description) A security class object contains a set of
 * procedures and some private data to implement a security model for rx
 * connections.  These routines are called by rx as appropriate.  Rx knows
 * nothing about the internal details of any particular security model, or
 * about security state.  Rx does maintain state per connection on behalf of
 * the security class.  Each security class implementation is also expected to
 * provide routines to create these objects.  Rx provides a basic routine to
 * allocate one of these objects; this routine must be called by the class.
 */
struct rx_securityClass {
    struct rx_securityOps {
	int (*op_Close) ( /* obj */ );
	int (*op_NewConnection) ( /* obj, conn */ );
	int (*op_PreparePacket) ( /* obj, call, packet */ );
	int (*op_SendPacket) ( /* obj, call, packet */ );
	int (*op_CheckAuthentication) ( /* obj,conn */ );
	int (*op_CreateChallenge) ( /* obj,conn */ );
	int (*op_GetChallenge) ( /* obj,conn,packet */ );
	int (*op_GetResponse) ( /* obj,conn,packet */ );
	int (*op_CheckResponse) ( /* obj,conn,packet */ );
	int (*op_CheckPacket) ( /* obj,call,packet */ );
	int (*op_DestroyConnection) ( /* obj, conn */ );
	int (*op_GetStats) ( /* obj, conn, stats */ );
	int (*op_NewService) ( /* obj, service */ );
	int (*op_Spare2) ();
	int (*op_Spare3) ();
    } *ops;
    void *privateData;
    int refCount;
};

#if defined(__STDC__) && !defined(__HIGHC__)
#define RXS_OP(obj,op,args) ((obj->ops->op_ ## op) ? \
			     (*(obj)->ops->op_ ## op)args : 0)
#else
#define RXS_OP(obj,op,args) ((obj->ops->op_/**/op) ? \
			     (*(obj)->ops->op_/**/op)args : 0)
#endif

#define RXS_Close(obj) RXS_OP(obj,Close,(obj))
#define RXS_NewConnection(obj,conn) RXS_OP(obj,NewConnection,(obj,conn))
#define RXS_PreparePacket(obj,call,packet) RXS_OP(obj,PreparePacket,\
						  (obj,call,packet))
#define RXS_SendPacket(obj,call,packet) RXS_OP(obj,SendPacket,\
					       (obj,call,packet))
#define RXS_CheckAuthentication(obj,conn) RXS_OP(obj,CheckAuthentication,\
						 (obj,conn))
#define RXS_CreateChallenge(obj,conn) RXS_OP(obj,CreateChallenge,\
					     (obj,conn))
#define RXS_GetChallenge(obj,conn,packet) RXS_OP(obj,GetChallenge,\
						 (obj,conn,packet))
#define RXS_GetResponse(obj,conn,packet) RXS_OP(obj,GetResponse,\
						(obj,conn,packet))
#define RXS_CheckResponse(obj,conn,packet) RXS_OP(obj,CheckResponse,\
						  (obj,conn,packet))
#define RXS_CheckPacket(obj,call,packet) RXS_OP(obj,CheckPacket,\
						(obj,call,packet))
#define RXS_DestroyConnection(obj,conn) RXS_OP(obj,DestroyConnection,\
					       (obj,conn))
#define RXS_GetStats(obj,conn,stats) RXS_OP(obj,GetStats,\
					    (obj,conn,stats))
#define RXS_NewService(obj,service,reuse) RXS_OP(obj,NewService,\
					    (obj,service,reuse))

int 
rxs_Release(struct rx_securityClass *aobj);

/*
 * A service is installed by rx_NewService, and specifies a service type that
 * is exported by this process.  Incoming calls are stamped with the service
 * type, and must match an installed service for the call to be accepted.
 * Each service exported has a (port,serviceId) pair to uniquely identify it.
 * It is also named:  this is intended to allow a remote statistics gathering
 * program to retrieve per service statistics without having to know the local
 * service id's.  Each service has a number of security objects (instances of
 * security classes) which implement various types of end-to-end security
 * protocols for connections made to this service.  Finally, there are two
 * parameters controlling the number of requests which may be executed in
 * parallel by this service: minProcs is the number of requests to this
 * service which are guaranteed to be able to run in parallel at any time;
 * maxProcs has two meanings: it limits the total number of requests which may
 * execute in parallel and it also guarantees that that many requests
 * may be handled in parallel if no other service is handling any
 * requests.
 */

struct rx_service {
    uint16_t serviceId;		       /* Service number */
    uint16_t servicePort;	       /* UDP port for this service */
    char *serviceName;		       /* Name of the service */
    osi_socket socket;		       /* socket struct or file descriptor */
    u_short nRequestsRunning;	       /* 
					* Number of requests currently in
				        * progress 
					*/
    u_short nSecurityObjects;	       /* Number of entries in security
				        * objects array */
    struct rx_securityClass **securityObjects;	/*
						 * Array of security class
						 * objects
						 */
    int32_t (*executeRequestProc) (struct rx_call *);
				       /* Routine to call when an rpc request
				        * is received */
    void (*destroyConnProc) ();	       /* Routine to call when a server
				        * connection is destroyed */
    void (*newConnProc) ();	       /*
				        * Routine to call when a server
					* connection is created
				        */
    void (*beforeProc) ();	       /* routine to call before a call is
				        * executed */
    void (*afterProc) ();	       /* routine to call after a call is
				        * executed */
    u_short maxProcs;		       /* Maximum procs to be used for this
				        * service */
    u_short minProcs;		       /* Minimum # of requests guaranteed
				        * executable simultaneously */
    u_short connDeadTime;	       /*
			                * Secs until a client of this service
				        * will be declared dead, if it is not
				        * responding
				        */
    u_short idleDeadTime;	       /*
				        * Time a server will wait for I/O to
				        * start up again
				        */
    void *serviceRock;			/* Rock for service */
};

void *	rx_getServiceRock(struct rx_service *);
void	rx_setServiceRock(struct rx_service *, void *);

/*
 * A server puts itself on an idle queue for a service using an
 * instance of the following structure.  When a call arrives, the call
 * structure pointer is placed in "newcall", the routine to execute to
 * service the request is placed in executeRequestProc, and the
 * process is woken up.  The queue entry's address is used for the
 * sleep/wakeup.
 */
struct rx_serverQueueEntry {
    struct rx_queue queueItemHeader;
    struct rx_call *newcall;
#ifdef	RX_ENABLE_LOCKS
    kmutex_t lock;
    kcondvar_t cv;
#endif
};

/* Bottom n-bits of the Call Identifier give the call number */
#define	RX_MAXCALLS 4		       /* Power of 2; max async calls per
				        * connection */
#define	RX_CIDSHIFT 2		       /* Log2(RX_MAXCALLS) */
#define	RX_CHANNELMASK (RX_MAXCALLS-1)
#define	RX_CIDMASK  (~RX_CHANNELMASK)

/*
 * A peer refers to a peer process, specified by a (host,port) pair.
 * There may be more than one peer on a given host.
 */
struct rx_peer {
    struct rx_peer *next;	       /* Next in hash conflict or free list */
    struct rx_queue connQueue;	       /* a list of all conn use this peer */
    uint32_t host;		       /* Remote IP address, in net byte
				        * order */
    uint16_t port;		       /* Remote UDP port, in net byte order */
    u_short packetSize;		       /*
				        * Max packet size, if known, for this
				        * host
				        */

    /* For garbage collection */
    u_long idleWhen;		       /* When the refcountwent to zero */
    short refCount;		       /* Reference count for this structure */

    /* Congestion control parameters */
    u_char burstSize;		       /*
				        * Reinitialization size for the burst
				        * parameter
				        */
    u_char burst;		       /* Number of packets that can be
				        * transmitted right now, without
				        * pausing */
    struct clock burstWait;	       /* Delay until new burst is allowed */
    struct rx_queue congestionQueue;   /*
				        * Calls that are waiting for non-zero
				        * burst value
				        */
    u_long srtt;		       /* Smoothed RTT in us. */
    u_long mdev;		       /* Smoothed mean deviation of RTT (us)*/

    struct clock timeout;	       /* Current retransmission delay */
    int nSent;			       /*
				        * Total number of distinct data packets
				        * sent, not including retransmissions
				        */
    int reSends;		       /*
				        * Total number of retransmissions for
				        * this peer, since this structure was
				        * created
				        */

/*
 * Skew: if a packet is received N packets later than expected (based
 * on packet serial numbers), then we define it to have a skew of N.
 * The maximum skew values allow us to decide when a packet hasn't
 * been received yet because it is out-of-order, as opposed to when it
 * is likely to have been dropped.
 */
    u_long inPacketSkew;	       /* Maximum skew on incoming packets */
    u_long outPacketSkew;	       /* Peer-reported max skew on our sent
				        * packets */
    int rateFlag;		       /* Flag for rate testing (-no 0yes
				        * +decrement) */
    u_short maxWindow;		       /* Maximum window size (number of
				        * packets) */
    u_short spare;		       /*
				        * we have to manually align things
					* b/c 220s crash
				        */
};

/*
 * A connection is an authenticated communication path, allowing
 * limited multiple asynchronous conversations.
 */
struct rx_connection {
    struct rx_queue queue_item;        /* conns on same peer */
    struct rx_connection *next;	       /* on hash chain _or_ free list */
    struct rx_peer *peer;
#ifdef	RX_ENABLE_LOCKS
    kmutex_t lock;
    kcondvar_t cv;
#endif
    uint32_t epoch;		       /* Process start time of client side
				        * of connection */
    uint32_t cid;			       /* Connection id (call channel is
				        * bottom bits) */
    uint32_t error;		       /* If this connection is in error,
				        * this is it */
    void *rock;			       /* User definable */
    struct rx_call *call[RX_MAXCALLS];
    uint32_t callNumber[RX_MAXCALLS];    /* Current call numbers */
    uint32_t serial;		       /* Next outgoing packet serial number */
    uint32_t lastSerial;		       /* # of last packet received, for
				        * computing skew */
    uint32_t maxSerial;		       /* largest serial number seen on
				        * incoming packets */
    uint32_t maxPacketSize;	       /*
				        * max packet size should be
					* per-connection since peer process
					* could be restarted on us.
				        */
    struct rxevent *challengeEvent;    /* Scheduled when the server is
				        * challenging a client-- to
				        * retransmit the challenge */
    struct rx_service *service;	       /* used by servers only */
    u_short serviceId;		       /* To stamp on requests, clients only */
    short refCount;		       /* Reference count */
    u_char flags;		       /* Defined below */
    u_char type;		       /* Type of connection, defined below */
    u_char secondsUntilPing;	       /* how often to ping for each active
				        * call */
    u_char securityIndex;	       /* corresponds to the security class
				        * of the */
    /* securityObject for this conn */
    struct rx_securityClass *securityObject;	/*
					         * Security object for this
					         * connection
					         */
    void *securityData;		       /* Private data for this conn's
				        * security class */
    u_short securityHeaderSize;	       /*
				        * Length of security module's packet
				        * header data
				        */
    u_short securityMaxTrailerSize;    /*
				        * Length of security module's packet
				        * trailer data
				        */
    int timeout;		       /*
				        * Overall timeout per call (seconds)
				        * for this conn
				        */
    int lastSendTime;		       /* Last send time for this connection */
    u_short secondsUntilDead;	       /*
				        * Maximum silence from peer before
				        * RX_CALL_DEAD
				        */
    u_short hardDeadTime;	       /* hard max for call execution */
};

/* Flag bits for connection structure */
#define	RX_CONN_MAKECALL_WAITING   1   /* rx_NewCall is waiting for a
				        * channel */
#define	RX_CONN_DESTROY_ME	   2   /*
					* Destroy *client* connection after
				        * last call
			                */
#define RX_CONN_USING_PACKET_CKSUM 4   /* non-zero header.spare field seen */
#define RX_CONN_BIG_ONES           8   /*
					* may use packets > 1500 bytes
				        * (compatibility)
					*/


/* Type of connection, client or server */
#define	RX_CLIENT_CONNECTION	0
#define	RX_SERVER_CONNECTION	1

/*
 * Call structure:  only instantiated for active calls and dallying server
 * calls. The permanent call state (i.e. the call number as well as state
 * shared with other calls associated with this connection) is maintained
 * in the connection structure.
 */
struct rx_call {
    struct rx_queue queue_item_header; /*
					* Call can be on various queues
					* (one-at-a-time)
					*/
    struct rx_queue tq;		       /* Transmit packet queue */
    struct rx_queue rq;		       /* Receive packet queue */
#ifdef	RX_ENABLE_LOCKS
    kmutex_t lock;
    kmutex_t lockw;
    kcondvar_t cv_twind;
    kmutex_t lockq;
    kcondvar_t cv_rq;
#endif
    struct rx_connection *conn;	       /* Parent connection for this call */
    uint32_t *callNumber;		       /*
					* Pointer to call number field
					* within connection
					*/
#if 0
    char *bufPtr;		       /*
					* Next byte to fill or read in current
					* send/read packet
					*/
#endif
    u_short nLeft;		       /*
				        * Number of bytes left in first receive
				        * queue packet
				        */
    struct rx_packet *currentPacket;   /*
					* Current packet being assembled or
					* being read
					*/
    u_short curvec;		       /* current iovec in currentPacket */
    u_short curpos;		       /* current position within curvec */
    u_short nFree;		       /* Number of bytes free in last send
				        * packet */
    u_char channel;		       /* Index of call, within connection */
    u_char state;		       /* Current call state as defined below */
    u_char mode;		       /* Current mode of a call in ACTIVE
				        * state */
    u_char flags;		       /* Some random flags */
    u_char localStatus;		       /* Local user status sent out of band */
    u_char remoteStatus;	       /* Remote user status received out of
				        * band */
    int32_t error;		       /* Error condition for this call */
    u_long timeout;		       /* High level timeout for this call */
    uint32_t rnext;		       /*
			                * Next sequence number expected to be
			                * read by rx_ReadData
				        */
    uint32_t rprev;		       /*
				        * Previous packet received; used for
				        * deciding what the next packet to be
				        * received should be in order to decide
				        * whether a negative acknowledge should
				        * be sent
				        */
    u_long rwind;		       /*
				        * The receive window:  the peer must
				        * not send packets with sequence
			                * numbers >= rnext+rwind
			                */
    uint32_t tfirst;		       /*
				        * First unacknowledged transmit packet
				        * number
				        */
    uint32_t tnext;		       /* Next transmit sequence number to
				        * use */
    u_long twind;		       /*
				        * The transmit window:  we cannot
					* assign a sequence number to a
				        * packet >= tfirst + twind
				        */
#if SOFT_ACK
    u_short nSoftAcks;		       /* The number of delayed soft acks */
    u_short nHardAcks;		       /* The number of delayed hard acks */
#endif
#if 0
    u_short cwind;		       /* The congestion window */
    u_short nextCwind;		       /* The congestion window after recovery */
    u_short nCwindAcks;		       /* Number acks received at current cwind */
    u_short ssthresh;		       /* The slow start threshold */
    u_short nAcks;		       /* The number of consecttive acks */
    u_short nNacks;		       /* Number packets acked that follow the
					* first negatively acked packet */
    u_short congestSeq;		       /* Peer's congestion sequence counter */
#endif
    struct rxevent *resendEvent;       /*
				        * If this is non-Null, there is a
				        * retransmission event pending
				        */
    struct rxevent *timeoutEvent;      /*
				        * If this is non-Null, then there is an
				        * overall timeout for this call
				        */
    struct rxevent *keepAliveEvent;    /*
				        * Scheduled periodically in active
				        * calls to keep call alive
				        */
    struct rxevent *delayedAckEvent;   /*
				        * Scheduled after all packets are
				        * received to send an ack if a reply
				        * or new call is not generated soon
				        */
    int lastSendTime;		       /* Last time a packet was sent on this
				        * call */
    int lastReceiveTime;	       /* Last time a packet was received for
				        * this call */
    void (*arrivalProc) ();	       /* Procedure to call when reply is
				        * received */
    void *arrivalProcHandle;	       /* Handle to pass to replyFunc */
    void *arrivalProcArg;	       /* Additional arg to pass to reply
				        * Proc */
    u_long lastAcked;		       /* last packet "hard" acked by
				        * receiver */
    u_long startTime;		       /* time the call started running */
    u_long startWait;		       /*
				        * time server began waiting for
					* input data/send quota
				        */
    struct clock traceWait;	       /*
				        * time server began waiting for input
				        * data/send quota
				        */
    struct clock traceStart;	       /* time the call started running */
};

/* Major call states */
#define	RX_STATE_NOTINIT  0	       /* Call structure has never been
				        * initialized */
#define	RX_STATE_PRECALL  1	       /*
					* Server-only:  call is not in
					* progress, but packets have arrived
					*/
#define	RX_STATE_ACTIVE	  2	       /*
	       		                * An active call; a process is dealing
					* with this call
					*/
#define	RX_STATE_DALLY	  3	       /* Dallying after process is done with
				        * call */

/*
 * Call modes:  the modes of a call in RX_STATE_ACTIVE state (process attached)
 */
#define	RX_MODE_SENDING	  1	       /* Sending or ready to send */
#define	RX_MODE_RECEIVING 2	       /* Receiving or ready to receive */
#define	RX_MODE_ERROR	  3	       /* Something in error for current
				        * conversation */
#define	RX_MODE_EOF	  4	       /*
				        * Server has flushed (or client has
					* read) last reply packet
					*/

/* Flags */
#define	RX_CALL_READER_WAIT	   1   /* Reader is waiting for next packet */
#define	RX_CALL_WAIT_WINDOW_ALLOC  2   /*
	       			        * Sender is waiting for window to
				        * allocate buffers
	     			        */
#define	RX_CALL_WAIT_WINDOW_SEND   4   /*
		       		        * Sender is waiting for window to
				        * send buffers
					*/
#define	RX_CALL_WAIT_PACKETS	   8   /*
					* Sender is waiting for packet buffers
					*/
#define	RX_CALL_WAIT_PROC	  16   /*
					* Waiting for a process to be assigned
					*/
#define	RX_CALL_RECEIVE_DONE	  32   /* All packets received on this call */
#define	RX_CALL_CLEARED		  64   /*
					* Receive queue cleared in precall
					* state
					*/
#define	RX_CALL_TQ_BUSY		  128  /*
					* Call's Xmit Queue is busy;
					* don't modify
					*/

#define RX_CALL_SLOW_START_OK	0x2000	/* receiver support slow start */

/* Maximum number of acknowledgements in an acknowledge packet */
#define	RX_MAXACKS	    255

/*
 * The structure of the data portion of an acknowledge packet: An acknowledge
 * packet is in network byte order at all times.  An acknowledgement is always
 * prompted for a specific reason by a specific incoming packet.  This reason
 * is reported in "reason" and the packet's sequence number in the packet
 * header.seq.  In addition to this information, all of the current
 * acknowledgement information about this call is placed in the packet.
 * "FirstPacket" is the sequence number of the first packet represented in an
 * array of bytes, "acks", containing acknowledgement information for a number
 * of consecutive packets.  All packets prior to FirstPacket are implicitly
 * acknowledged: the sender need no longer be concerned about them.  Packets
 * from firstPacket+nAcks and on are not acknowledged.  Packets in the range
 * [firstPacket,firstPacket+nAcks) are each acknowledged explicitly.  The
 * acknowledgement may be RX_NACK if the packet is not (currently) at the
 * receiver (it may have never been received, or received and then later
 * dropped), or it may be RX_ACK if the packet is queued up waiting to be read
 * by the upper level software.  RX_ACK does not imply that the packet may not
 * be dropped before it is read; it does imply that the sender should stop
 * retransmitting the packet until notified otherwise.  The field
 * previousPacket identifies the previous packet received by the peer.  This
 * was used in a previous version of this software, and could be used in the
 * future.  The serial number in the data part of the ack packet corresponds to
 * the serial number oof the packet which prompted the acknowledge.  Any
 * packets which are explicitly not acknowledged, and which were last
 * transmitted with a serial number less than the provided serial number,
 * should be retransmitted immediately.  Actually, this is slightly inaccurate:
 * packets are not necessarily received in order.  When packets are habitually
 * transmitted out of order, this is allowed for in the retransmission
 * algorithm by introducing the notion of maximum packet skew: the degree of
 * out-of-orderness of the packets received on the wire.  This number is
 * communicated from the receiver to the sender in ack packets.
 */

struct rx_ackPacket {
    uint16_t bufferSpace;	       /*
				        * Number of packet buffers available.
					* That is: the number of buffers that
					* the sender of the ack packet is
					* willing to provide for data,
				        * on this or subsequent calls. Lying is
				        * permissable.
				        */
    uint16_t maxSkew;		       /*
				        * Maximum difference between serial# of
				        * packet acknowledged and highest
					* packet yet received
				        */
    uint32_t firstPacket;	       /*
				        * The first packet in the list of
				        * acknowledged packets
				        */
    uint32_t previousPacket;	       /*
				        * The previous packet number received
					* (obsolete?)
				        */
    uint32_t serial;		       /*
				        * Serial number of the packet which
				        * prompted the acknowledge
				        */
    u_char reason;		       /*
				        * Reason for the acknowledge of
					* ackPacket,  defined below
				        */
    u_char nAcks;		       /* Number of acknowledgements */
    u_char acks[RX_MAXACKS];	       /*
				        * Up to RX_MAXACKS packet ack's,
				        * defined below
				        */
    /*
     * Packets <firstPacket are implicitly acknowledged and may be discarded
     * by the sender.  Packets >= firstPacket+nAcks are implicitly NOT
     * acknowledged.  No packets with sequence numbers >= firstPacket should
     * be discarded by the sender (they may thrown out at any time by the
     * receiver)
     */
};

#define FIRSTACKOFFSET 4

/* Reason for acknowledge message */
#define	RX_ACK_REQUESTED	1      /* Peer requested an ack on this
				        * packet */
#define	RX_ACK_DUPLICATE	2      /* Duplicate packet */
#define	RX_ACK_OUT_OF_SEQUENCE	3      /* Packet out of sequence */
#define	RX_ACK_EXCEEDS_WINDOW	4      /*
					* Packet sequence number higher than
					* window; discarded
					*/
#define	RX_ACK_NOSPACE		5      /* No buffer space at all */
#define	RX_ACK_PING		6      /* This is a keep-alive ack */
#define	RX_ACK_PING_RESPONSE	7      /* Ack'ing because we were pinged */
#define	RX_ACK_DELAY		8      /*
					* Ack generated since nothing has
					* happened since receiving packet
					*/
#define RX_ACK_IDLE             9	/* */

/* Packet acknowledgement type */
#define	RX_ACK_TYPE_NACK	0      /* I Don't have this packet */
#define	RX_ACK_TYPE_ACK		1      /*
					* I have this packet, although I may
					* discard it later
					*/

/*
 * The packet size transmitted for an acknowledge is adjusted to reflect the
 * actual size of the acks array.  This macro defines the size
 */
#define rx_AckDataSize(nAcks) (18 + (nAcks))

#define	RX_CHALLENGE_TIMEOUT	2      /*
					* Number of seconds before another
					* authentication request packet is
					* generated
					*/

/*
 * RX error codes.  RX uses error codes from -1 to -64.  Rxgen may use other
 * error codes < -64; user programs are expected to return positive error
 * codes
 */

/* Min rx error */
#define RX_MIN_ERROR		    (-1)

/* Something bad happened to the connection; temporary loss of communication */
#define	RX_CALL_DEAD		    (-1)

/*
 * An invalid operation, such as a client attempting to send data after
 * having received the beginning of a reply from the server
 */
#define	RX_INVALID_OPERATION	    (-2)

/* An optional timeout per call may be specified */
#define	RX_CALL_TIMEOUT		    (-3)

/* End of data on a read */
#define	RX_EOF			    (-4)

/* Some sort of low-level protocol error */
#define	RX_PROTOCOL_ERROR	    (-5)

/*
 * Generic user abort code; used when no more specific error code needs to
 * be communicated.  For example, multi rx clients use this code to abort a
 * multi rx call
 */
#define	RX_USER_ABORT		    (-6)

/* Port already in use (from rx_Init) */
#define RX_ADDRINUSE		    (-7)

/* EMSGSIZE returned from network.  Packet too big, must fragment */
#define RX_MSGSIZE		    (-8)

/*
 *Not on wire, when CheckResponse/GetResponse return this,
 * packet should be sent.
 */
#define RX_AUTH_REPLY		    (-63)

/* Max rx error */
#define RX_MAX_ERROR		    (-64)

/*
 * Structure for keeping rx statistics.  Note that this structure is returned
 * by rxdebug, so, for compatibility reasons, new fields should be appended (or
 * spares used), the rxdebug protocol checked, if necessary, and the PrintStats
 * code should be updated as well.
 *
 * Clearly we assume that ntohl will work on these structures so sizeof(int)
 * must equal sizeof(long).
 */

struct rx_stats {		       /* General rx statistics */
    uint32_t packetRequests;		       /* Number of packet allocation
				        * requests */
    uint32_t noPackets[RX_N_PACKET_CLASSES];/*
				        * Number of failed packet requests,
				        * per allocation class
				        */
    uint32_t socketGreedy;		       /* Whether SO_GREEDY succeeded */
    uint32_t bogusPacketOnRead;	       /*
				        * Number of inappropriately short
					* packets  received
				        */
    uint32_t bogusHost;		       /* Host address from bogus packets */
    uint32_t noPacketOnRead;		       /*
				        * Number of read packets attempted
					* when there was actually no packet
				        * to read off the wire
				        */
    uint32_t noPacketBuffersOnRead;	       /*
					* Number of dropped data packets due
					* to lack of packet buffers
					*/
    uint32_t selects;		       /*
				        * Number of selects waiting for packet
					* or timeout
					*/
    uint32_t sendSelects;		       /*
					* Number of selects forced when
					* sending packet
					*/
    uint32_t packetsRead[RX_N_PACKET_TYPES];/*
				        * Total number of packets read, per
				        * type
				        */
    uint32_t dataPacketsRead;	       /*
				        * Number of unique data packets read
					* off the wire
				        */
    uint32_t ackPacketsRead;		       /* Number of ack packets read */
    uint32_t dupPacketsRead;		       /* Number of duplicate data packets
				        * read */
    uint32_t spuriousPacketsRead;	       /* Number of inappropriate data
				        * packets */
    uint32_t packetsSent[RX_N_PACKET_TYPES];/*
				        * Number of rxi_Sends: packets sent
				        * over the wire, per type
				        */
    uint32_t ackPacketsSent;		       /* Number of acks sent */
    uint32_t pingPacketsSent;	       /* Total number of ping packets sent */
    uint32_t abortPacketsSent;	       /* Total number of aborts */
    uint32_t busyPacketsSent;	       /* Total number of busies sent
				        * received */
    uint32_t dataPacketsSent;	       /* Number of unique data packets sent */
    uint32_t dataPacketsReSent;	       /* Number of retransmissions */
    uint32_t dataPacketsPushed;	       /*
				        * Number of retransmissions pushed early by
				        * a NACK
				        */
    uint32_t ignoreAckedPacket;	       /*
				        * Number of packets with acked flag,
					* on rxi_Start
				        */
    struct clock totalRtt;	       /*
				        * Total round trip time measured
					* (use to compute average)
				        */
    struct clock minRtt;	       /* Minimum round trip time measured */
    struct clock maxRtt;	       /* Maximum round trip time measured */
    uint32_t nRttSamples;		       /* Total number of round trip samples */
    uint32_t nServerConns;		       /* Total number of server connections */
    uint32_t nClientConns;		       /* Total number of client connections */
    uint32_t nPeerStructs;		       /* Total number of peer structures */
    uint32_t nCallStructs;		       /* Total number of call structures
				        * allocated */
    uint32_t nFreeCallStructs;	       /*
				        * Total number of previously allocated
					* free call structures
				        */
    uint32_t netSendFailures;
    uint32_t fatalErrors;
    uint32_t spares[8];
};

void rx_PrintTheseStats(FILE *file, struct rx_stats *s, int size);

/* structures for debug input and output packets */

/* debug input types */
struct rx_debugIn {
    uint32_t type;
    uint32_t index;
};

/* Invalid rx debug package type */
#define RX_DEBUGI_BADTYPE     (-8)

#define RX_DEBUGI_VERSION_MINIMUM ('L')/* earliest real version */
#define RX_DEBUGI_VERSION     ('N')    /* Latest version */
 /* first version w/ secStats */
#define RX_DEBUGI_VERSION_W_SECSTATS ('L')
 /* version M is first supporting GETALLCONN and RXSTATS type */
#define RX_DEBUGI_VERSION_W_GETALLCONN ('M')
#define RX_DEBUGI_VERSION_W_RXSTATS ('M')
 /* last version with unaligned debugConn */
#define RX_DEBUGI_VERSION_W_UNALIGNED_CONN ('L')
#define RX_DEBUGI_VERSION_W_WAITERS ('N')

#define	RX_DEBUGI_GETSTATS	1      /* get basic rx stats */
#define	RX_DEBUGI_GETCONN	2      /* get connection info */
#define	RX_DEBUGI_GETALLCONN	3      /* get even uninteresting conns */
#define	RX_DEBUGI_RXSTATS	4      /* get all rx stats */

struct rx_debugStats {
    uint32_t nFreePackets;
    uint32_t packetReclaims;
    uint32_t callsExecuted;
    uint8_t waitingForPackets;
    uint8_t usedFDs;
    uint8_t version;
    uint8_t spare1;
    uint32_t nWaiting;
    uint32_t spare2[9];
};

struct rx_debugConn_vL {
    uint32_t host;
    uint32_t cid;
    uint32_t serial;
    uint32_t callNumber[RX_MAXCALLS];
    uint32_t error;
    uint16_t port;
    char flags;
    char type;
    char securityIndex;
    char callState[RX_MAXCALLS];
    char callMode[RX_MAXCALLS];
    char callFlags[RX_MAXCALLS];
    char callOther[RX_MAXCALLS];
    /* old style getconn stops here */
    struct rx_securityObjectStats secStats;
    uint32_t sparel[10];
};

struct rx_debugConn {
    uint32_t host;
    uint32_t cid;
    uint32_t serial;
    uint32_t callNumber[RX_MAXCALLS];
    uint32_t error;
    uint16_t port;
    char flags;
    char type;
    char securityIndex;
    char sparec[3];		       /* force correct alignment */
    char callState[RX_MAXCALLS];
    char callMode[RX_MAXCALLS];
    char callFlags[RX_MAXCALLS];
    char callOther[RX_MAXCALLS];
    /* old style getconn stops here */
    struct rx_securityObjectStats secStats;
    uint32_t epoch;
    uint32_t maxPacketSize;
    uint32_t sparel[9];
};

#define	RX_OTHER_IN	1	       /* packets avail in in queue */
#define	RX_OTHER_OUT	2	       /* packets avail in out queue */

#endif				       /* _RX_	 End of rx.h */
