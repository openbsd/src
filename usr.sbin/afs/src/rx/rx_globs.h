/*	$OpenBSD: rx_globs.h,v 1.1.1.1 1998/09/14 21:53:15 art Exp $	*/
/* $KTH: rx_globs.h,v 1.4 1998/02/22 19:46:16 joda Exp $ */

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

/* RX:  Globals for internal use, basically */

#ifdef	KERNEL
#include "../rx/rx.h"
#else				       /* KERNEL */
#include "rx.h"
#endif				       /* KERNEL */

#ifndef INIT
#define INIT(x)
#define	EXT extern
#endif				       /* INIT */

/* The array of installed services.  Null terminated. */
EXT struct rx_service *rx_services[RX_MAX_SERVICES + 1];

/* Incoming calls wait on this queue when there are no available server processes */
EXT struct rx_queue rx_incomingCallQueue;

/* Server processes wait on this queue when there are no appropriate calls to process */
EXT struct rx_queue rx_idleServerQueue;

/* Constant delay time before sending an acknowledge of the last packet received.  This is to avoid sending an extra acknowledge when the client is about to make another call, anyway, or the server is about to respond. */
EXT struct clock rx_lastAckDelay;

/* Variable to allow introduction of network unreliability */
#ifdef RXDEBUG
EXT int rx_intentionallyDroppedPacketsPer100 INIT(0);	/* Dropped on Send */

#endif

EXT int rx_extraQuota INIT(0);	       /* extra packets to add to the quota */
EXT int rx_extraPackets INIT(32);      /* extra packets to alloc (2 windows
				        * by deflt) */

EXT int rx_stackSize INIT(RX_DEFAULT_STACK_SIZE);

EXT int rx_connDeadTime INIT(12);      /* Time until an unresponsive
				        * connection is declared dead */
EXT int rx_idleConnectionTime INIT(700);	/* Time until we toss an idle
						 * connection */
EXT int rx_idlePeerTime INIT(60);      /* Time until we toss a peer
				        * structure, after all connections
				        * using it have disappeared */

/* These definitions should be in one place */
#ifdef	AFS_SUN5_ENV
#define	RX_CBUF_TIME	180	       /* Check for cbuf deficit */
#define	RX_REAP_TIME	90	       /* Check for tossable connections
				        * every 90 seconds */
#else
#define	RX_CBUF_TIME	120	       /* Check for cbuf deficit */
#define	RX_REAP_TIME	60	       /* Check for tossable connections
				        * every 60 seconds */
#endif

EXT int rx_Window INIT(15);	       /* Temporary HACK:  transmit/receive
				        * window */
EXT int rx_ACKHACK INIT(4);	       /* Temporary HACK:  how often to send
				        * request for acknowledge */

#define	ACKHACK(p)  ((((p)->header.seq & 3)==0) && ((p)->header.flags |= RX_REQUEST_ACK))

EXT int rx_nPackets INIT(100);	       /* obsolete; use rx_extraPackets now */

/* List of free packets */
EXT struct rx_queue rx_freePacketQueue;
EXT struct rx_queue rx_freeCbufQueue;

#ifdef RX_ENABLE_LOCKS
EXT afs_lock_t rx_freePktQ_lock;

#endif

/* Number of free packets */
EXT int rx_nFreePackets INIT(0);
EXT int rx_nFreeCbufs INIT(0);
EXT int rx_nCbufs INIT(0);
EXT int rxi_NeedMoreCbufs INIT(0);
EXT int rx_nWaiting INIT(0);

/* largest packet which we can safely receive, initialized to AFS 3.2 value
 * This is provided for backward compatibility with peers which may be unable
 * to swallow anything larger. THIS MUST NEVER DECREASE WHILE AN APPLICATION
 * IS RUNNING! */
EXT u_long rx_maxReceiveSize INIT(OLD_MAX_PACKET_SIZE);

#if (defined(AFS_SUN5_ENV) || defined(AFS_AOS_ENV)) && defined(KERNEL)
EXT u_long rx_MyMaxSendSize INIT(OLD_MAX_PACKET_SIZE - RX_HEADER_SIZE);

#else
EXT u_long rx_MyMaxSendSize INIT(RX_MAX_PACKET_DATA_SIZE);

#endif


/* List of free queue entries */
EXT struct rx_serverQueueEntry *rx_FreeSQEList INIT(0);

#ifdef	RX_ENABLE_LOCKS
EXT kmutex_t freeSQEList_lock;

#endif

/* List of free call structures */
EXT struct rx_queue rx_freeCallQueue;

#ifdef	RX_ENABLE_LOCKS
EXT kmutex_t rx_freeCallQueue_lock;

#endif
EXT long rxi_nCalls INIT(0);

/* Basic socket for client requests; other sockets (for receiving server requests) are in the service structures */
EXT osi_socket rx_socket;

/* Port requested at rx_Init.  If this is zero, the actual port used will be different--but it will only be used for client operations.  If non-zero, server provided services may use the same port. */
EXT u_short rx_port;

/* 32-bit select Mask for rx_Listener.  We use 32 bits because IOMGR_Select only supports 32 */
EXT fd_set rx_selectMask;
EXT int rx_maxSocketNumber;	       /* Maximum socket number represented
				        * in the select mask */

/* This is actually the minimum number of packets that must remain free,
    overall, immediately after a packet of the requested class has been
    allocated.  *WARNING* These must be assigned with a great deal of care.
    In order, these are receive quota, send quota and special quota */
#define	RX_PACKET_QUOTAS {1, 10, 0}
/* value large enough to guarantee that no allocation fails due to RX_PACKET_QUOTAS.
   Make it a little bigger, just for fun */
#define	RX_MAX_QUOTA	15	       /* part of min packet computation */
EXT int rx_packetQuota[RX_N_PACKET_CLASSES] INIT(RX_PACKET_QUOTAS);

EXT int rx_nextCid;		       /* Next connection call id */
EXT int rx_epoch;		       /* Initialization time of rx */

#ifdef	RX_ENABLE_LOCKS
EXT kmutex_t rx_waitingForPackets_lock;
EXT kcondvar_t rx_waitingForPackets_cv;

#endif
EXT char rx_waitingForPackets;	       /* Processes set and wait on this
				        * variable when waiting for packet
				        * buffers */

EXT struct rx_stats rx_stats;

EXT struct rx_peer **rx_peerHashTable;
EXT struct rx_connection **rx_connHashTable;
EXT u_long rx_hashTableSize INIT(256); /* Power of 2 */
EXT u_long rx_hashTableMask INIT(255); /* One less than rx_hashTableSize */

#define CONN_HASH(host, port, cid, epoch, type) ((((cid)>>RX_CIDSHIFT)&rx_hashTableMask))

#define PEER_HASH(host, port)  ((host ^ port) & rx_hashTableMask)

#ifdef	notdef			       /* Use a func for now to measure
				        * allocated structs */
#define	rxi_Free(addr, size)	osi_Free(addr, size)
#endif				       /* notdef */

#define rxi_AllocSecurityObject() (struct rx_securityClass *) rxi_Alloc(sizeof(struct rx_securityClass))
#define	rxi_FreeSecurityObject(obj) rxi_Free(obj, sizeof(struct rx_securityClass))
#define	rxi_AllocService()	(struct rx_service *) rxi_Alloc(sizeof(struct rx_service))
#define	rxi_FreeService(obj)	rxi_Free(obj, sizeof(struct rx_service))
#define	rxi_AllocPeer()		(struct rx_peer *) rxi_Alloc(sizeof(struct rx_peer))
#define	rxi_FreePeer(peer)	rxi_Free(peer, sizeof(struct rx_peer))
#define	rxi_AllocConnection()	(struct rx_connection *) rxi_Alloc(sizeof(struct rx_connection))
#define rxi_FreeConnection(conn) (rxi_Free(conn, sizeof(struct rx_connection)))

/* Forward definitions of internal procedures */
struct rx_packet *rxi_AllocPacket(int);
struct rx_packet *rxi_AllocSendPacket(struct rx_call *, int);
char *rxi_Alloc(int);
struct rx_peer *rxi_FindPeer(u_long, u_short);
struct rx_call *rxi_NewCall(struct rx_connection *, int);
void rxi_FreeCall(struct rx_call *);
void rxi_Listener(void);
int rxi_ReadPacket(int, struct rx_packet *, u_long *, u_short *);
struct rx_packet *rxi_ReceivePacket(struct rx_packet *, osi_socket,
				    u_long, u_short);
struct rx_packet *rxi_ReceiveDataPacket(struct rx_call *,
					struct rx_packet *);
struct rx_packet *rxi_ReceiveAckPacket(struct rx_call *,
				       struct rx_packet *);
struct rx_packet *rxi_ReceiveResponsePacket(struct rx_connection *, 
					    struct rx_packet *);
struct rx_packet *rxi_ReceiveChallengePacket(struct rx_connection *,
					     struct rx_packet *);
void rx_ServerProc(void);
void rxi_AttachServerProc(struct rx_call *);
void rxi_ChallengeOn(struct rx_connection *);
void rxi_InitPeerParams(struct rx_peer *);


#define	rxi_ChallengeOff(conn)	rxevent_Cancel((conn)->challengeEvent);
void rxi_ChallengeEvent(struct rxevent*, struct rx_connection *, 
			char *);
struct rx_packet *rxi_SendAck(struct rx_call *, 
			      struct rx_packet *, int, int, int, int);
void rxi_ClearTransmitQueue(struct rx_call *);
void rxi_ClearReceiveQueue(struct rx_call *);
void rxi_ResetConnection(struct rx_connection *);
void rxi_InitCall(void); /* obsolete ? */
void rxi_ResetCall(struct rx_call *);
void rxi_CallError(struct rx_call *, long);
void rxi_ConnectionError(struct rx_connection *, long);
void rxi_QueuePackets(void); /* obsolete ? */
void rxi_Start(struct rxevent *, struct rx_call *);
void rxi_CallIsIdle(void); /* obsolete ? */
void rxi_CallTimedOut(void); /* obsolete ? */
void rxi_ComputeRoundTripTime(struct rx_packet *, 
			      struct clock *, 
			      struct rx_peer *);
void rxi_ScheduleKeepAliveEvent(struct rx_call *);
void rxi_KeepAliveEvent(struct rxevent *, struct rx_call *, char *);
void rxi_KeepAliveOn(struct rx_call *);

#define rxi_KeepAliveOff(call) rxevent_Cancel((call)->keepAliveEvent)
void rxi_AckAll(struct rxevent *, struct rx_call *, char *);
void rxi_SendDelayedAck(struct rxevent *, struct rx_call *, char *);
struct rx_packet *rxi_SendSpecial(struct rx_call *,
				  struct rx_connection *,
				  struct rx_packet *, int, char *, int);
struct rx_packet *rxi_SendCallAbort(struct rx_call *, 
				    struct rx_packet *);
struct rx_packet *rxi_SendConnectionAbort(struct rx_connection *,
					  struct rx_packet *);
void rxi_ScheduleDecongestionEvent(struct rx_call *, int);
void rxi_CongestionWait(struct rx_call *);
void rxi_ReapConnections(void);
void rxi_EncodePacketHeader(struct rx_packet *);
void rxi_DecodePacketHeader(struct rx_packet *);
void rxi_DebugPrint(const char *, ...);
void rxi_PrepareSendPacket(struct rx_call *, 
			   struct rx_packet *, int);
void rxi_MoreCbufs(int);
long rx_SlowGetLong(struct rx_packet *, int);

void rxi_Send(struct rx_call *, struct rx_packet *);
void rxi_FreeAllPackets(void);
void rxi_SendPacket(struct rx_connection *,
		    struct rx_packet *);
int rxi_IsConnInteresting(struct rx_connection *);
struct rx_packet *rxi_ReceiveDebugPacket(struct rx_packet *, osi_socket,
					 long, short);
struct rx_packet *rxi_ReceiveVersionPacket(struct rx_packet *, osi_socket,
					   long, short);
void rxi_SendDebugPacket(struct rx_packet *, osi_socket, long, short);


#ifdef RXDEBUG
/* Some debugging stuff */
EXT FILE *rx_debugFile;		       /* Set by the user to a stdio file for
				        * debugging output */

#define Log rx_debugFile
#define dpf(args) if (rx_debugFile) rxi_DebugPrint args; else

EXT char *rx_packetTypes[RX_N_PACKET_TYPES] INIT(RX_PACKET_TYPES);	/* Strings defined in
									 * rx.h */

#else
#define dpf(args)
#endif				       /* RXDEBUG */
