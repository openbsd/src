/*
 * rx.h,v 1.7 1995/04/11 06:07:07 assar Exp
 */

#ifndef _RX_
#define _RX_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <list.h>
#include <pthread.h>
#include <thrpool.h>

#include <rx_pkt.h>

/* XXX - This should be moved somewhere else and replaced with XDR */

typedef u_long unsigned32;
typedef u_short unsigned16;

/* Security garbage */

typedef struct rx_securityClass {
} rx_securityClass;

typedef struct rx_connection rx_connection;

typedef struct rx_call rx_call;

typedef struct rx_service rx_service;

#define RX_MAXDATA 1444		/* XXX - ??? */


typedef struct rx_wirepacket {
     rx_header header;
     char data[RX_MAXDATA];
} rx_wirepacket;

/*
 * There are two different types of acks.
 * soft ack means that the packet has been receieved at the other end,
 * but the sender should not throw away the packet yet. The receiver
 * can still drop the packet or not give it to the application.
 * The point of the soft acks is mainly flow-control.
 *
 * A hard ack means that the packet has been acknowledged by the
 * application. Then the packet can be thrown away.
 *
 */

typedef enum {
     RX_PKT_SOFT_ACK = 1
} rx_packet_flags;

typedef struct rx_packet {
     rx_wirepacket wire;
     unsigned datalen;
     rx_call *call;
     rx_packet_flags flags;
} rx_packet;


/* every call - i.e. RPC transaction */

struct rx_call {
     enum { SENDING, RECEIVING } mode;
     u_long callno;		/* Call # of this connection */
     u_long seqno;		/* Seq # for packets */
     int channelid;		/* What channel are we using? */
     rx_connection *conn;
     List *recvpackets;		/* List of received packets */
#if 0
     List *ooopackets;		/* Packets rec'd out-of-order */
#endif
     rx_packet *thispacket;	/* This packet, sending or receiving */
     char *ptr;			/* Here we should write data */
     unsigned nleft;		/* How much data there is left */
     pthread_mutex_t mutex;	/* Synchronisation */
     pthread_cond_t cond;
};

/* This represents the on-going communication, i.e. a connection */

typedef enum {
     RX_CONN_SERVER,		/* This is a server */
     RX_CONN_CLIENT		/* The other side is a server */
} rx_connection_type;

#define RX_WINDOW 15

struct rx_connection {
     time_t epoch;		/* Time when this connection started */
     u_long connid;		/* Connection ID. How? */
     struct sockaddr_in peer;	/* The one we're talking to */
     u_long serialno;		/* Next serial number to use */
     u_long callno[MAXCALLS];	/* Next call number to use */
     rx_call *calls[MAXCALLS];	/* The on-going calls */
     u_char secindex;		/* Security index */
     u_short serviceid;		/* Service ID */
     rx_connection_type type;	/* Type of connection C-S or S-C */
     u_long maxnoacked;		/* Max packet sent and soft-acked */
     List *packets;		/* Not yet acked sent packets */
     u_long window;		/* Size of the window */
     pthread_cond_t condsend;	/* Conditional variable for sending */
     pthread_mutex_t mutexsend;	/* Mutex for above */
     pthread_cond_t condrecv;
     pthread_mutex_t mutexrecv;
     rx_service *service;	/* Service if server, else NULL */
};

/*
 * About packets:
 *
 * Here we keep the packets that have been sent but not yet
 * hard-acked. When a packet has been soft-acked we set a flag in the
 * packet_flags and stop the resend-timer.
 */

struct rx_service {
     u_short port;
     u_short serviceid;
     char *servicename;
     int (*serviceproc)(rx_call *);
     Thrpool *thrpool;
};

/* functions */

int rx_Init (int port);

rx_connection *rx_NewConnection (struct in_addr host, u_short port,
				 u_short service, 
				 rx_securityClass *sec,
				 int secindex);

void rx_DestroyConnection (rx_connection *);

rx_call *rx_NewCall (rx_connection *);

int rx_EndCall (rx_call *, int);

rx_service *rx_NewService(u_short port, u_short serviceid,
			  char *servicename, rx_securityClass **so,
			  int nso, int (*serviceproc)(rx_call *));

int rx_Write (rx_call *, void *, int);
int rx_Read (rx_call *, void *, int);
int rx_FlushWrite (rx_call *call);


#endif /* _RX_ */




/* Old garbage */

#if 0

/* header of a RPC packet */
/* We should use XDR on this too. */

typedef enum {
     HT_DATA = 1,
     HT_ACK = 2,
     HT_BUSY = 3,
     HT_ABORT = 4,
     HT_ACKALL = 5,
     HT_CHAL = 6,
     HT_RESP = 7,
     HT_DEBUG = 8
} rx_header_type;

/* For flags in header */

enum {
     HF_CLIENT_INITIATED = 1,
     HF_REQ_ACK = 2,
     HF_LAST = 4,
     HF_MORE = 8};

#define MAXCALLS 4

#define CALL_MASK (MAXCALLS-1)
#define CONNID_MASK (~(MAXCALLS-1))

typedef struct rx_header {
     unsigned32 epoch;
     unsigned32 connid;		/* And channel ID */
     unsigned32 callid;
     unsigned32 seqno;		/* Call-based number */
     unsigned32 serialno;	/* Unique number */
     u_char type;
     u_char flags;
     u_char status;
     u_char secindex;
     unsigned16 reserved;	/* ??? verifier? */
     unsigned16 serviceid;
/* This should be the other way around according to everything but */
/* tcpdump */
} rx_header;

#endif

#if 0

typedef enum {
     RX_ACK_REQUESTED = 1, 
     RX_ACK_DUPLICATE = 2,
     RX_ACK_OUT_OF_SEQUENCE = 3,
     RX_ACK_EXEEDS_WINDOW = 4,
     RX_ACK_NOSPACE = 5,
     RX_ACK_PING = 6,
     RX_ACK_PING_RESPONSE = 7,
     RX_ACK_DELAY = 8
} rx_ack_reason;

typedef enum {
     RX_ACK_TYPE_NACK = 0,
     RX_ACK_TYPE_ACK = 1
} rx_ack_type;

#define RXMAXACKS 255

typedef struct rx_ack_packet {
     unsigned16 bufferspace;
     unsigned16 maxskew;
     unsigned32 firstpacket;	/* First packet in acks below */
     unsigned32 prevpacket;
     unsigned32 serial;		/* Packet that prompted this one */
     u_char reason;		/* rx_ack_reason */
     u_char nacks;		/* # of acks */
     u_char acks[RXMAXACKS];	/* acks (rx_ack_type) */
} rx_ack_packet;

#endif
