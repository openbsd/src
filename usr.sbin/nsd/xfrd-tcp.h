/*
 * xfrd-tcp.h - XFR (transfer) Daemon TCP system header file. Manages tcp conn.
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#ifndef XFRD_TCP_H
#define XFRD_TCP_H

#include "xfrd.h"

struct buffer;
struct xfrd_zone;
struct xfrd_soa;
struct xfrd_state;
struct region;
struct dname;
struct acl_options;

struct xfrd_tcp_pipeline;
typedef struct xfrd_tcp xfrd_tcp_t;
typedef struct xfrd_tcp_set xfrd_tcp_set_t;
/*
 * A set of xfrd tcp connections.
 */
struct xfrd_tcp_set {
	/* tcp connections, each has packet and read/wr state */
	struct xfrd_tcp_pipeline *tcp_state[XFRD_MAX_TCP];
	/* number of TCP connections in use. */
	int tcp_count;
	/* TCP timeout. */
	int tcp_timeout;
	/* rbtree with pipelines sorted by master */
	rbtree_t* pipetree;
	/* double linked list of zones waiting for a TCP connection */
	struct xfrd_zone *tcp_waiting_first, *tcp_waiting_last;
};

/*
 * Structure to keep track of an open tcp connection
 * The xfrd tcp connection is used to first make a request
 * Then to receive the answer packet(s).
 */
struct xfrd_tcp {
	/* tcp connection state */
	/* state: reading or writing */
	uint8_t is_reading;

	/* how many bytes have been read/written - total,
	   incl. tcp length bytes */
	uint32_t total_bytes;

	/* msg len bytes */
	uint16_t msglen;

	/* fd of connection. -1 means unconnected */
	int fd;

	/* packet buffer of connection */
	struct buffer* packet;
};

/* use illegal pointer value to denote skipped ID number.
 * if this does not work, we can allocate with malloc */
#define TCP_NULL_SKIP ((struct xfrd_zone*)-1)
/* the number of ID values (16 bits) for a pipeline */
#define ID_PIPE_NUM 65536

/**
 * Structure to keep track of a pipelined set of queries on
 * an open tcp connection.  The queries may be answered with
 * interleaved answer packets, the ID number disambiguates.
 * Sorted by the master IP address so you can use lookup with
 * smaller-or-equal to find the tcp connection most suitable.
 */
struct xfrd_tcp_pipeline {
	/* the rbtree node, sorted by IP and nr of unused queries */
	rbnode_t node;
	/* destination IP address */
#ifdef INET6
	struct sockaddr_storage ip;
#else
	struct sockaddr_in ip;
#endif /* INET6 */
	socklen_t ip_len;
	/* number of unused IDs.  used IDs are waiting to send their query,
	 * or have been sent but not not all answer packets have been received.
	 * Sorted by num_unused, so a lookup smaller-equal for 65536 finds the
	 * connection to that master that has the most free IDs. */
	int num_unused;
	/* number of skip-set IDs (these are 'in-use') */
	int num_skip;

	int handler_added;
	/* the event handler for this pipe (it'll disambiguate by ID) */
	struct event handler;

	/* the tcp connection to use for reading */
	xfrd_tcp_t* tcp_r;
	/* the tcp connection to use for writing, if it is done successfully,
	 * then the first zone from the sendlist can be removed. */
	xfrd_tcp_t* tcp_w;
	/* once a byte has been written, handshake complete */
	int connection_established;

	/* list of queries that want to send, first to get write event,
	 * if NULL, no write event interest */
	struct xfrd_zone* tcp_send_first, *tcp_send_last;
	/* the unused and id arrays must be last in the structure */
	/* per-ID number the queries that have this ID number, every
	 * query owns one ID numbers (until it is done). NULL: unused
	 * When a query is done but not all answer-packets have been
	 * consumed for that ID number, the rest is skipped, this
	 * is denoted with the pointer-value TCP_NULL_SKIP, the ids that
	 * are skipped are not on the unused list.  They may be
	 * removed once the last answer packet is skipped.
	 * ID_PIPE_NUM-num_unused values in the id array are nonNULL (either
	 * a zone pointer or SKIP) */
	struct xfrd_zone* id[ID_PIPE_NUM];
	/* unused ID numbers; the first part of the array contains the IDs */
	uint16_t unused[ID_PIPE_NUM];
};

/* create set of tcp connections */
xfrd_tcp_set_t* xfrd_tcp_set_create(struct region* region);

/* init tcp state */
xfrd_tcp_t* xfrd_tcp_create(struct region* region, size_t bufsize);
/* obtain tcp connection for a zone (or wait) */
void xfrd_tcp_obtain(xfrd_tcp_set_t* set, struct xfrd_zone* zone);
/* release tcp connection for a zone (starts waiting) */
void xfrd_tcp_release(xfrd_tcp_set_t* set, struct xfrd_zone* zone);
/* release tcp pipe entirely (does not stop the zones inside it) */
void xfrd_tcp_pipe_release(xfrd_tcp_set_t* set, struct xfrd_tcp_pipeline* tp,
	int conn);
/* use tcp connection to start xfr */
void xfrd_tcp_setup_write_packet(struct xfrd_tcp_pipeline* tp,
	struct xfrd_zone* zone);
/* initialize tcp_state for a zone. Opens the connection. true on success.*/
int xfrd_tcp_open(xfrd_tcp_set_t* set, struct xfrd_tcp_pipeline* tp, struct xfrd_zone* zone);
/* read data from tcp, maybe partial read */
void xfrd_tcp_read(struct xfrd_tcp_pipeline* tp);
/* write data to tcp, maybe a partial write */
void xfrd_tcp_write(struct xfrd_tcp_pipeline* tp, struct xfrd_zone* zone);
/* handle tcp pipe events */
void xfrd_handle_tcp_pipe(int fd, short event, void* arg);

/*
 * Read from a stream connection (size16)+packet into buffer.
 * returns value is
 *	-1 on error.
 *	0 on short read, call back later.
 *	1 on completed read.
 * On first call, make sure total_bytes = 0, msglen=0, buffer_clear().
 * and the packet and fd need to be set.
 */
int conn_read(xfrd_tcp_t* conn);
/*
 * Write to a stream connection (size16)+packet.
 * return value is
 * -1 on error. 0 on short write, call back later. 1 completed write.
 * On first call, make sure total_bytes=0, msglen=buffer_limit(),
 * buffer_flipped(). packet and fd need to be set.
 */
int conn_write(xfrd_tcp_t* conn);

/* setup DNS packet for a query of this type */
void xfrd_setup_packet(struct buffer* packet,
        uint16_t type, uint16_t klass, const struct dname* dname, uint16_t qid);
/* write soa in network format to the packet buffer */
void xfrd_write_soa_buffer(struct buffer* packet,
        const struct dname* apex, struct xfrd_soa* soa);
/* use acl address to setup sockaddr struct, returns length of addr. */
socklen_t xfrd_acl_sockaddr_to(struct acl_options* acl,
#ifdef INET6
	struct sockaddr_storage *to);
#else
	struct sockaddr_in *to);
#endif /* INET6 */

socklen_t xfrd_acl_sockaddr_frm(struct acl_options* acl,
#ifdef INET6
	struct sockaddr_storage *frm);
#else
	struct sockaddr_in *frm);
#endif /* INET6 */

/* create pipeline tcp structure */
struct xfrd_tcp_pipeline* xfrd_tcp_pipeline_create(region_type* region);

#endif /* XFRD_TCP_H */
