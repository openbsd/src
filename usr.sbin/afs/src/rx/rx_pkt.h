#ifndef _RX_PACKET_
#define _RX_PACKET_
#include "sys/uio.h"
/*
 * this file includes the macros and decls which depend on packet
 * format, and related packet manipulation macros.  Note that code
 * which runs at NETPRI should not sleep, or AIX will panic
 */
/*
 * There are some assumptions that various code makes -- I'll try to
 * express them all here:
 * 1.  rx_ReceiveAckPacket assumes that it can get an entire ack
 * contiguous in the first iovec.  As a result, the iovec buffers must
 * be >= sizeof (struct rx_ackpacket)
 * 2. All callers of rx_Pullup besides rx_ReceiveAckPacket try to pull
 * up less data than rx_ReceiveAckPacket does.
 * 3. rx_GetLong and rx_PutLong (and the slow versions of same) assume
 * that the iovec buffers are all integral multiples of the word size,
 * and that the offsets are as well.
 */


#ifndef MIN
#define MIN(a,b)  ((a)<(b)?(a):(b))
#endif

#define	RX_IPUDP_SIZE		28

/*
 * REMOTE_PACKET_SIZE is currently the same as local.  This is because REMOTE
 * is defined much too generally for my tastes, and includes the case of
 * multiple class C nets connected with a router within one campus or MAN.
 * I don't want to make local performance suffer just because of some
 * out-dated protocol that used to be in use on the NSFANET that's
 * practically unused anymore.  Any modern IP implementation will be
 * using MTU discovery, and even old routers shouldn't frag packets
 * when sending from one connected network directly to another.  Maybe
 * the next release of RX will do MTU discovery.
 */

/*
 * MTUXXX the various "MAX" params here must be rationalized.  From now on,
 * the MAX packet size will be the maximum receive size, but the maximum send
 * size will be larger than that.
 */

#ifdef notdef
/*  some sample MTUs
           4352   what FDDI(RFC1188) uses... Larger?
           4096   VJ's recommendation for FDDI
          17914   what IBM 16MB TR  uses
           8166   IEEE 802.4
           4464   IEEE 802.5 MAX
           2002   IEEE 802.5 Recommended
	   1500   what Ethernet uses
	   1492   what 802.3 uses ( 8 bytes for 802.2 SAP )
*/

/* * * * these are the old defines
*/
#define	RX_MAX_PACKET_SIZE	(RX_MAX_DL_MTU -RX_IPUDP_SIZE)

#define	RX_MAX_PACKET_DATA_SIZE	(RX_MAX_PACKET_SIZE-RX_HEADER_SIZE)
#ifdef AFS_HPUX_ENV
/* HPUX by default uses an 802.3 size, and it's not evident from SIOCGIFCONF */
#define	RX_LOCAL_PACKET_SIZE	(1492 - RX_IPUDP_SIZE)
#define	RX_REMOTE_PACKET_SIZE	(1492 - RX_IPUDP_SIZE)
#else
#define	RX_LOCAL_PACKET_SIZE	RX_MAX_PACKET_SIZE	/* For hosts on same net */
#define	RX_REMOTE_PACKET_SIZE	RX_MAX_PACKET_SIZE	/* see note above */
#endif

#endif				       /* notdef */

/* These are the new, streamlined ones.
 */
#define	RX_HEADER_SIZE		28

#define	OLD_MAX_PACKET_SIZE	(1500 -RX_IPUDP_SIZE)
#define	RX_PP_PACKET_SIZE	(576 +RX_HEADER_SIZE)

/* if the other guy is not on the local net, use this size */
#define	RX_REMOTE_PACKET_SIZE	(1500 - RX_IPUDP_SIZE)

/* for now, never send more data than this */
#define	RX_MAX_PACKET_SIZE	(16384 + RX_HEADER_SIZE)
#define	RX_MAX_PACKET_DATA_SIZE	 16384


/* Packet types, for rx_packet.type */
#define	RX_PACKET_TYPE_DATA	    1  /* A vanilla data packet */
#define	RX_PACKET_TYPE_ACK	    2  /* Acknowledge packet */
#define	RX_PACKET_TYPE_BUSY	    3  /* Busy: can't accept call
				        * immediately; try later */
#define	RX_PACKET_TYPE_ABORT	    4  /* Abort packet.  No response needed. */
#define	RX_PACKET_TYPE_ACKALL	    5  /* Acknowledges receipt of all packets */
#define	RX_PACKET_TYPE_CHALLENGE    6  /* Challenge client's identity:
				        * request credentials */
#define	RX_PACKET_TYPE_RESPONSE	    7  /* Respond to challenge packet */
#define	RX_PACKET_TYPE_DEBUG	    8  /* Get debug information */

#define RX_PACKET_TYPE_PARAMS       9  /* exchange size params (showUmine) */
#define RX_PACKET_TYPE_VERSION	   13  /* get AFS version */


#define	RX_PACKET_TYPES	    {"data", "ack", "busy", "abort", "ackall", "challenge", "response", "debug", "params", "unused", "unused", "unused", "version"}
#define	RX_N_PACKET_TYPES	    13 /* Must agree with above list; counts
				        * 0 */

/* Packet classes, for rx_AllocPacket */
#define	RX_PACKET_CLASS_RECEIVE	    0
#define	RX_PACKET_CLASS_SEND	    1
#define	RX_PACKET_CLASS_SPECIAL	    2

#define	RX_N_PACKET_CLASSES	    3  /* Must agree with above list */

/* Flags for rx_header flags field */
#define	RX_CLIENT_INITIATED	1      /* Packet is sent/received from client
				        * side of call */
#define	RX_REQUEST_ACK		2      /* Peer requests acknowledgement */
#define	RX_LAST_PACKET		4      /* This is the last packet from this
				        * side of the call */
#define	RX_MORE_PACKETS		8      /* There are more packets following
				        * this, i.e. the next sequence number
				        * seen by the receiver should be
				        * greater than this one, rather than
				        * a resend of an earlier sequence
				        * number */
#define RX_SLOW_START_OK	32     /* Set this flag in an ack
					* packet to inform the sender
					* that slow start is supported
					* by the receiver. */

/* The following flags are preset per packet, i.e. they don't change
 * on retransmission of the packet */
#define	RX_PRESET_FLAGS		(RX_CLIENT_INITIATED | RX_LAST_PACKET)


/* The rx part of the header of a packet, in host form */
struct rx_header {
    uint32_t epoch;		       /* Start time of client process */
    uint32_t cid;		       /* Connection id (defined by client) */
    uint32_t callNumber;	       /* Current call number */
    uint32_t seq;		       /* Sequence number of this packet,
				        * within this call */
    uint32_t serial;		       /* Serial number of this packet: a new
				        * serial number is stamped on each
				        * packet sent out */
    u_char type;		       /* RX packet type */
    u_char flags;		       /* Flags, defined below */
    u_char userStatus;		       /* User defined status information,
				        * returned/set by macros
				        * rx_Get/SetLocal/RemoteStatus */
    u_char securityIndex;	       /* Which service-defined security
				        * method to use */
    u_short serviceId;		       /* service this packet is directed
				        * _to_ */

    /*
     * This spare is now used for packet header checkksum.  see
     * rxi_ReceiveDataPacket and packet cksum macros above for details.
     */
    u_short spare;
};

#define RX_MAXWVECS 10		       /* most Unixes max is 16, so never let
				        * this > 15 */
/*
 * RX_FIRSTBUFFERSIZE must be larger than the largest ack packet,
 * the largest possible challenge or response packet.
 * Both Firstbuffersize and cbuffersize must be integral multiples of 8,
 * so the security header and trailer stuff works for rxkad_crypt.  yuck.
 */

#define RX_FIRSTBUFFERSIZE (OLD_MAX_PACKET_SIZE - RX_HEADER_SIZE - 4)
#define RX_CBUFFERSIZE 1024

#if 0
#if	defined(AFS_SUN5_ENV) || defined(AFS_AOS_ENV)
#define RX_FIRSTBUFFERSIZE (OLD_MAX_PACKET_SIZE - RX_HEADER_SIZE)
#define RX_CBUFFERSIZE 1012
#else
#define RX_FIRSTBUFFERSIZE 480	       /* MTUXXX should be 1444 */
#define RX_CBUFFERSIZE 504	       /* MTUXXX change this to 1024 or 1012  */
#endif				       /* AFS_SUN5_ENV */
#endif

struct rx_packet {
    struct rx_queue queueItemHeader;   /* Packets are chained using the
				        * queue.h package */
    struct clock retryTime;	       /* When this packet should NEXT be
				        * re-transmitted */
    struct clock timeSent;	       /* When this packet was transmitted
				        * last */
    uint32_t firstSerial;		       /* Original serial number of this
				        * packet */
    struct clock firstSent;	       /* When this packet was transmitted
				        * first */
    struct rx_header header;	       /* The internal packet header */
    int niovecs;
    struct iovec wirevec[RX_MAXWVECS + 1];	/* the new form of the packet */
    u_long wirehead[RX_HEADER_SIZE / sizeof(u_long)+1 ];
    u_long localdata[RX_FIRSTBUFFERSIZE / sizeof(u_long)+1];
    uint32_t dummy;
    u_char acked;		       /* This packet has been *tentatively*
				        * acknowledged */
    u_char backoff;		       /* for multiple re-sends */
    u_short length;		       /* Data length */
};

struct rx_cbuf {
    struct rx_queue queueItemHeader;
    u_long data[(RX_CBUFFERSIZE / sizeof(u_long)) + 1];
};

/* Macros callable by security modules, to set header/trailer lengths,
 * set actual packet size, and find the beginning of the security
 * header (or data) */
#define rx_SetSecurityHeaderSize(conn, length) ((conn)->securityHeaderSize = (length))
#define rx_SetSecurityMaxTrailerSize(conn, length) ((conn)->securityMaxTrailerSize = (length))
#define rx_GetSecurityHeaderSize(conn) ((conn)->securityHeaderSize)
#define rx_GetSecurityMaxTrailerSize(conn) ((conn)->securityMaxTrailerSize)

/* This is the address of the data portion of the packet.  Any encryption
 * headers will be at this address, the actual data, for a data packet, will
 * start at this address + the connection's security header size. */
#define	rx_DataOf(packet)		((char *) (packet)->wirevec[1].iov_base)
#define	rx_GetDataSize(packet)		((packet)->length)
#define	rx_SetDataSize(packet, size)	((packet)->length = (size))

/* These macros used in conjunction with reuse of packet header spare as a
 * packet cksum for rxkad security module. */
#define rx_GetPacketCksum(packet)	 ((packet)->header.spare)
#define rx_SetPacketCksum(packet, cksum) ((packet)->header.spare = (cksum))

#define rxi_OverQuota(packetclass) (rx_nFreePackets - 1 < rx_packetQuota[packetclass])

/* compat stuff */
#define rx_GetLong(p,off) rx_SlowGetLong((p), (off))
#define rx_PutLong(p,off,b) rx_SlowPutLong((p), (off), (b))

#define rx_data(p, o, l) ((l=((struct rx_packet*)(p))->wirevec[(o+1)].iov_len),\
  (((struct rx_packet*)(p))->wirevec[(o+1)].iov_base))


struct rx_packet *rx_AllocPacket(void);
void rxi_MorePackets(int);
void rx_CheckCbufs(unsigned long);
void rxi_FreePacket(struct rx_packet *);
int rxi_AllocDataBuf(struct rx_packet *, int);
size_t rx_SlowReadPacket(struct rx_packet*, int, int, void*);
size_t rx_SlowWritePacket(struct rx_packet*, int, int, void*);
int rxi_RoundUpPacket(struct rx_packet *, unsigned int);

uint32_t rx_SlowGetLong(struct rx_packet *packet, int offset);
int rx_SlowPutLong(struct rx_packet *packet, int offset, uint32_t data);
int  rxi_FreeDataBufs(struct rx_packet *p, int first);

int osi_NetSend(osi_socket socket, char *addr, struct iovec *dvec,
		int nvecs, int length);

/* copy data into an RX packet */
#define rx_packetwrite(p, off, len, in)               \
  ( (off) + (len) > (p)->wirevec[1].iov_len ?         \
    rx_SlowWritePacket(p, off, len, in) :             \
    ((memcpy((char *)((p)->wirevec[1].iov_base)+(off), (in), (len))),0))

/* copy data from an RX packet */
#define rx_packetread(p, off, len, out)               \
  ( (off) + (len) > (p)->wirevec[1].iov_len ?         \
    rx_SlowReadPacket(p, off, len, out) :             \
    ((memcpy((out), (char *)((p)->wirevec[1].iov_base)+(off), len)),0))

#define rx_computelen(p,l) { int i; \
   for (l=0, i=1; i < p->niovecs; i++ ) l += p->wirevec[i].iov_len; }

/* return what the actual contiguous space is: should be min(length,size) */
/* The things that call this really want something like ...pullup MTUXXX  */
#define rx_Contiguous(p) MIN((p)->length,((p)->wirevec[1].iov_len))

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

/* === packet-ized down to here, the following macros work temporarily */
/* Unfortunately, they know that the cbuf stuff isn't there. */

/* try to ensure that rx_DataOf will return a contiguous space at
 * least size bytes uint32_t */
/* return what the actual contiguous space is: should be min(length,size) */
#define rx_Pullup(p,size)	       /* this idea here is that this will
				        * make a guarantee */


/* The offset of the actual user's data in the packet, skipping any
 * security header */
/* DEPRECATED: DON'T USE THIS!  [ 93.05.03  lws ] */
#define	rx_UserDataOf(conn, packet)	(((char *) (packet)->wirevec[1].iov_base) + (conn)->securityHeaderSize)

#endif				       /* _RX_PACKET_ */
