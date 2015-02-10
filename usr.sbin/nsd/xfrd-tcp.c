/*
 * xfrd-tcp.c - XFR (transfer) Daemon TCP system source file. Manages tcp conn.
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#include "config.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "xfrd-tcp.h"
#include "buffer.h"
#include "packet.h"
#include "dname.h"
#include "options.h"
#include "namedb.h"
#include "xfrd.h"
#include "xfrd-disk.h"
#include "util.h"

/* sort tcppipe, first on IP address, for an IPadresss, sort on num_unused */
static int
xfrd_pipe_cmp(const void* a, const void* b)
{
	const struct xfrd_tcp_pipeline* x = (struct xfrd_tcp_pipeline*)a;
	const struct xfrd_tcp_pipeline* y = (struct xfrd_tcp_pipeline*)b;
	int r;
	if(x == y)
		return 0;
	if(y->ip_len != x->ip_len)
		/* subtraction works because nonnegative and small numbers */
		return (int)y->ip_len - (int)x->ip_len;
	r = memcmp(&x->ip, &y->ip, x->ip_len);
	if(r != 0)
		return r;
	/* sort that num_unused is sorted ascending, */
	if(x->num_unused != y->num_unused) {
		return (x->num_unused < y->num_unused) ? -1 : 1;
	}
	/* different pipelines are different still, even with same numunused*/
	return (uintptr_t)x < (uintptr_t)y ? -1 : 1;
}

xfrd_tcp_set_t* xfrd_tcp_set_create(struct region* region)
{
	int i;
	xfrd_tcp_set_t* tcp_set = region_alloc(region, sizeof(xfrd_tcp_set_t));
	memset(tcp_set, 0, sizeof(xfrd_tcp_set_t));
	tcp_set->tcp_count = 0;
	tcp_set->tcp_waiting_first = 0;
	tcp_set->tcp_waiting_last = 0;
	for(i=0; i<XFRD_MAX_TCP; i++)
		tcp_set->tcp_state[i] = xfrd_tcp_pipeline_create(region);
	tcp_set->pipetree = rbtree_create(region, &xfrd_pipe_cmp);
	return tcp_set;
}

struct xfrd_tcp_pipeline*
xfrd_tcp_pipeline_create(region_type* region)
{
	int i;
	struct xfrd_tcp_pipeline* tp = (struct xfrd_tcp_pipeline*)
		region_alloc_zero(region, sizeof(*tp));
	tp->num_unused = ID_PIPE_NUM;
	assert(sizeof(tp->unused)/sizeof(tp->unused[0]) == ID_PIPE_NUM);
	for(i=0; i<ID_PIPE_NUM; i++)
		tp->unused[i] = (uint16_t)i;
	tp->tcp_r = xfrd_tcp_create(region, QIOBUFSZ);
	tp->tcp_w = xfrd_tcp_create(region, 512);
	return tp;
}

void
xfrd_setup_packet(buffer_type* packet,
	uint16_t type, uint16_t klass, const dname_type* dname, uint16_t qid)
{
	/* Set up the header */
	buffer_clear(packet);
	ID_SET(packet, qid);
	FLAGS_SET(packet, 0);
	OPCODE_SET(packet, OPCODE_QUERY);
	QDCOUNT_SET(packet, 1);
	ANCOUNT_SET(packet, 0);
	NSCOUNT_SET(packet, 0);
	ARCOUNT_SET(packet, 0);
	buffer_skip(packet, QHEADERSZ);

	/* The question record. */
	buffer_write(packet, dname_name(dname), dname->name_size);
	buffer_write_u16(packet, type);
	buffer_write_u16(packet, klass);
}

static socklen_t
#ifdef INET6
xfrd_acl_sockaddr(acl_options_t* acl, unsigned int port,
	struct sockaddr_storage *sck)
#else
xfrd_acl_sockaddr(acl_options_t* acl, unsigned int port,
	struct sockaddr_in *sck, const char* fromto)
#endif /* INET6 */
{
	/* setup address structure */
#ifdef INET6
	memset(sck, 0, sizeof(struct sockaddr_storage));
#else
	memset(sck, 0, sizeof(struct sockaddr_in));
#endif
	if(acl->is_ipv6) {
#ifdef INET6
		struct sockaddr_in6* sa = (struct sockaddr_in6*)sck;
		sa->sin6_family = AF_INET6;
		sa->sin6_port = htons(port);
		sa->sin6_addr = acl->addr.addr6;
		return sizeof(struct sockaddr_in6);
#else
		log_msg(LOG_ERR, "xfrd: IPv6 connection %s %s attempted but no \
INET6.", fromto, acl->ip_address_spec);
		return 0;
#endif
	} else {
		struct sockaddr_in* sa = (struct sockaddr_in*)sck;
		sa->sin_family = AF_INET;
		sa->sin_port = htons(port);
		sa->sin_addr = acl->addr.addr;
		return sizeof(struct sockaddr_in);
	}
}

socklen_t
#ifdef INET6
xfrd_acl_sockaddr_to(acl_options_t* acl, struct sockaddr_storage *to)
#else
xfrd_acl_sockaddr_to(acl_options_t* acl, struct sockaddr_in *to)
#endif /* INET6 */
{
	unsigned int port = acl->port?acl->port:(unsigned)atoi(TCP_PORT);
#ifdef INET6
	return xfrd_acl_sockaddr(acl, port, to);
#else
	return xfrd_acl_sockaddr(acl, port, to, "to");
#endif /* INET6 */
}

socklen_t
#ifdef INET6
xfrd_acl_sockaddr_frm(acl_options_t* acl, struct sockaddr_storage *frm)
#else
xfrd_acl_sockaddr_frm(acl_options_t* acl, struct sockaddr_in *frm)
#endif /* INET6 */
{
	unsigned int port = acl->port?acl->port:0;
#ifdef INET6
	return xfrd_acl_sockaddr(acl, port, frm);
#else
	return xfrd_acl_sockaddr(acl, port, frm, "from");
#endif /* INET6 */
}

void
xfrd_write_soa_buffer(struct buffer* packet,
	const dname_type* apex, struct xfrd_soa* soa)
{
	size_t rdlength_pos;
	uint16_t rdlength;
	buffer_write(packet, dname_name(apex), apex->name_size);

	/* already in network order */
	buffer_write(packet, &soa->type, sizeof(soa->type));
	buffer_write(packet, &soa->klass, sizeof(soa->klass));
	buffer_write(packet, &soa->ttl, sizeof(soa->ttl));
	rdlength_pos = buffer_position(packet);
	buffer_skip(packet, sizeof(rdlength));

	/* uncompressed dnames */
	buffer_write(packet, soa->prim_ns+1, soa->prim_ns[0]);
	buffer_write(packet, soa->email+1, soa->email[0]);

	buffer_write(packet, &soa->serial, sizeof(uint32_t));
	buffer_write(packet, &soa->refresh, sizeof(uint32_t));
	buffer_write(packet, &soa->retry, sizeof(uint32_t));
	buffer_write(packet, &soa->expire, sizeof(uint32_t));
	buffer_write(packet, &soa->minimum, sizeof(uint32_t));

	/* write length of RR */
	rdlength = buffer_position(packet) - rdlength_pos - sizeof(rdlength);
	buffer_write_u16_at(packet, rdlength_pos, rdlength);
}

xfrd_tcp_t*
xfrd_tcp_create(region_type* region, size_t bufsize)
{
	xfrd_tcp_t* tcp_state = (xfrd_tcp_t*)region_alloc(
		region, sizeof(xfrd_tcp_t));
	memset(tcp_state, 0, sizeof(xfrd_tcp_t));
	tcp_state->packet = buffer_create(region, bufsize);
	tcp_state->fd = -1;

	return tcp_state;
}

static struct xfrd_tcp_pipeline*
pipeline_find(xfrd_tcp_set_t* set, xfrd_zone_t* zone)
{
	rbnode_t* sme = NULL;
	struct xfrd_tcp_pipeline* r;
	/* smaller buf than a full pipeline with 64kb ID array, only need
	 * the front part with the key info, this front part contains the
	 * members that the compare function uses. */
	const size_t keysize = sizeof(struct xfrd_tcp_pipeline) -
		ID_PIPE_NUM*(sizeof(struct xfrd_zone*) + sizeof(uint16_t));
	/* void* type for alignment of the struct,
	 * divide the keysize by ptr-size and then add one to round up */
	void* buf[ (keysize / sizeof(void*)) + 1 ];
	struct xfrd_tcp_pipeline* key = (struct xfrd_tcp_pipeline*)buf;
	key->node.key = key;
	key->ip_len = xfrd_acl_sockaddr_to(zone->master, &key->ip);
	key->num_unused = ID_PIPE_NUM;
	/* lookup existing tcp transfer to the master with highest unused */
	if(rbtree_find_less_equal(set->pipetree, key, &sme)) {
		/* exact match, strange, fully unused tcp cannot be open */
		assert(0);
	} 
	if(!sme)
		return NULL;
	r = (struct xfrd_tcp_pipeline*)sme->key;
	/* <= key pointed at, is the master correct ? */
	if(r->ip_len != key->ip_len)
		return NULL;
	if(memcmp(&r->ip, &key->ip, key->ip_len) != 0)
		return NULL;
	/* correct master, is there a slot free for this transfer? */
	if(r->num_unused == 0)
		return NULL;
	return r;
}

/* remove zone from tcp waiting list */
static void
tcp_zone_waiting_list_popfirst(xfrd_tcp_set_t* set, xfrd_zone_t* zone)
{
	assert(zone->tcp_waiting);
	set->tcp_waiting_first = zone->tcp_waiting_next;
	if(zone->tcp_waiting_next)
		zone->tcp_waiting_next->tcp_waiting_prev = NULL;
	else	set->tcp_waiting_last = 0;
	zone->tcp_waiting_next = 0;
	zone->tcp_waiting = 0;
}

/* remove zone from tcp pipe write-wait list */
static void
tcp_pipe_sendlist_remove(struct xfrd_tcp_pipeline* tp, xfrd_zone_t* zone)
{
	if(zone->in_tcp_send) {
		if(zone->tcp_send_prev)
			zone->tcp_send_prev->tcp_send_next=zone->tcp_send_next;
		else	tp->tcp_send_first=zone->tcp_send_next;
		if(zone->tcp_send_next)
			zone->tcp_send_next->tcp_send_prev=zone->tcp_send_prev;
		else	tp->tcp_send_last=zone->tcp_send_prev;
		zone->in_tcp_send = 0;
	}
}

/* remove first from write-wait list */
static void
tcp_pipe_sendlist_popfirst(struct xfrd_tcp_pipeline* tp, xfrd_zone_t* zone)
{
	tp->tcp_send_first = zone->tcp_send_next;
	if(tp->tcp_send_first)
		tp->tcp_send_first->tcp_send_prev = NULL;
	else	tp->tcp_send_last = NULL;
	zone->in_tcp_send = 0;
}

/* remove zone from tcp pipe ID map */
static void
tcp_pipe_id_remove(struct xfrd_tcp_pipeline* tp, xfrd_zone_t* zone)
{
	assert(tp->num_unused < ID_PIPE_NUM && tp->num_unused >= 0);
	assert(tp->id[zone->query_id] == zone);
	tp->id[zone->query_id] = NULL;
	tp->unused[tp->num_unused] = zone->query_id;
	/* must remove and re-add for sort order in tree */
	(void)rbtree_delete(xfrd->tcp_set->pipetree, &tp->node);
	tp->num_unused++;
	(void)rbtree_insert(xfrd->tcp_set->pipetree, &tp->node);
}

/* stop the tcp pipe (and all its zones need to retry) */
static void
xfrd_tcp_pipe_stop(struct xfrd_tcp_pipeline* tp)
{
	int i, conn = -1;
	assert(tp->num_unused < ID_PIPE_NUM); /* at least one 'in-use' */
	assert(ID_PIPE_NUM - tp->num_unused > tp->num_skip); /* at least one 'nonskip' */
	/* need to retry for all the zones connected to it */
	/* these could use different lists and go to a different nextmaster*/
	for(i=0; i<ID_PIPE_NUM; i++) {
		if(tp->id[i] && tp->id[i] != TCP_NULL_SKIP) {
			xfrd_zone_t* zone = tp->id[i];
			conn = zone->tcp_conn;
			zone->tcp_conn = -1;
			zone->tcp_waiting = 0;
			tcp_pipe_sendlist_remove(tp, zone);
			tcp_pipe_id_remove(tp, zone);
			xfrd_set_refresh_now(zone);
		}
	}
	assert(conn != -1);
	/* now release the entire tcp pipe */
	xfrd_tcp_pipe_release(xfrd->tcp_set, tp, conn);
}

static void
tcp_pipe_reset_timeout(struct xfrd_tcp_pipeline* tp)
{
	int fd = tp->handler.ev_fd;
	struct timeval tv;
	tv.tv_sec = xfrd->tcp_set->tcp_timeout;
	tv.tv_usec = 0;
	if(tp->handler_added)
		event_del(&tp->handler);
	event_set(&tp->handler, fd, EV_PERSIST|EV_TIMEOUT|EV_READ|
		(tp->tcp_send_first?EV_WRITE:0), xfrd_handle_tcp_pipe, tp);
	if(event_base_set(xfrd->event_base, &tp->handler) != 0)
		log_msg(LOG_ERR, "xfrd tcp: event_base_set failed");
	if(event_add(&tp->handler, &tv) != 0)
		log_msg(LOG_ERR, "xfrd tcp: event_add failed");
	tp->handler_added = 1;
}

/* handle event from fd of tcp pipe */
void
xfrd_handle_tcp_pipe(int ATTR_UNUSED(fd), short event, void* arg)
{
	struct xfrd_tcp_pipeline* tp = (struct xfrd_tcp_pipeline*)arg;
	if((event & EV_WRITE)) {
		tcp_pipe_reset_timeout(tp);
		if(tp->tcp_send_first) {
			DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: event tcp write, zone %s",
				tp->tcp_send_first->apex_str));
			xfrd_tcp_write(tp, tp->tcp_send_first);
		}
	}
	if((event & EV_READ) && tp->handler_added) {
		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: event tcp read"));
		tcp_pipe_reset_timeout(tp);
		xfrd_tcp_read(tp);
	}
	if((event & EV_TIMEOUT) && tp->handler_added) {
		/* tcp connection timed out */
		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: event tcp timeout"));
		xfrd_tcp_pipe_stop(tp);
	}
}

/* add a zone to the pipeline, it starts to want to write its query */
static void
pipeline_setup_new_zone(xfrd_tcp_set_t* set, struct xfrd_tcp_pipeline* tp,
	xfrd_zone_t* zone)
{
	/* assign the ID */
	int idx;
	assert(tp->num_unused > 0);
	/* we pick a random ID, even though it is TCP anyway */
	idx = random_generate(tp->num_unused);
	zone->query_id = tp->unused[idx];
	tp->unused[idx] = tp->unused[tp->num_unused-1];
	tp->id[zone->query_id] = zone;
	/* decrement unused counter, and fixup tree */
	(void)rbtree_delete(set->pipetree, &tp->node);
	tp->num_unused--;
	(void)rbtree_insert(set->pipetree, &tp->node);

	/* add to sendlist, at end */
	zone->tcp_send_next = NULL;
	zone->tcp_send_prev = tp->tcp_send_last;
	zone->in_tcp_send = 1;
	if(tp->tcp_send_last)
		tp->tcp_send_last->tcp_send_next = zone;
	else	tp->tcp_send_first = zone;
	tp->tcp_send_last = zone;

	/* is it first in line? */
	if(tp->tcp_send_first == zone) {
		xfrd_tcp_setup_write_packet(tp, zone);
		/* add write to event handler */
		tcp_pipe_reset_timeout(tp);
	}
}

void
xfrd_tcp_obtain(xfrd_tcp_set_t* set, xfrd_zone_t* zone)
{
	struct xfrd_tcp_pipeline* tp;
	assert(zone->tcp_conn == -1);
	assert(zone->tcp_waiting == 0);

	if(set->tcp_count < XFRD_MAX_TCP) {
		int i;
		assert(!set->tcp_waiting_first);
		set->tcp_count ++;
		/* find a free tcp_buffer */
		for(i=0; i<XFRD_MAX_TCP; i++) {
			if(set->tcp_state[i]->tcp_r->fd == -1) {
				zone->tcp_conn = i;
				break;
			}
		}
		/** What if there is no free tcp_buffer? return; */
		if (zone->tcp_conn < 0) {
			return;
		}

		tp = set->tcp_state[zone->tcp_conn];
		zone->tcp_waiting = 0;

		/* stop udp use (if any) */
		if(zone->zone_handler.ev_fd != -1)
			xfrd_udp_release(zone);

		if(!xfrd_tcp_open(set, tp, zone)) {
			zone->tcp_conn = -1;
			set->tcp_count --;
			xfrd_set_refresh_now(zone);
			return;
		}
		/* ip and ip_len set by tcp_open */
		tp->node.key = tp;
		tp->num_unused = ID_PIPE_NUM;
		tp->num_skip = 0;
		tp->tcp_send_first = NULL;
		tp->tcp_send_last = NULL;
		memset(tp->id, 0, sizeof(tp->id));
		for(i=0; i<ID_PIPE_NUM; i++) {
			tp->unused[i] = i;
		}

		/* insert into tree */
		(void)rbtree_insert(set->pipetree, &tp->node);
		xfrd_deactivate_zone(zone);
		xfrd_unset_timer(zone);
		pipeline_setup_new_zone(set, tp, zone);
		return;
	}
	/* check for a pipeline to the same master with unused ID */
	if((tp = pipeline_find(set, zone))!= NULL) {
		int i;
		if(zone->zone_handler.ev_fd != -1)
			xfrd_udp_release(zone);
		for(i=0; i<XFRD_MAX_TCP; i++) {
			if(set->tcp_state[i] == tp)
				zone->tcp_conn = i;
		}
		xfrd_deactivate_zone(zone);
		xfrd_unset_timer(zone);
		pipeline_setup_new_zone(set, tp, zone);
		return;
	}

	/* wait, at end of line */
	DEBUG(DEBUG_XFRD,2, (LOG_INFO, "xfrd: max number of tcp "
		"connections (%d) reached.", XFRD_MAX_TCP));
	zone->tcp_waiting_next = 0;
	zone->tcp_waiting_prev = set->tcp_waiting_last;
	zone->tcp_waiting = 1;
	if(!set->tcp_waiting_last) {
		set->tcp_waiting_first = zone;
		set->tcp_waiting_last = zone;
	} else {
		set->tcp_waiting_last->tcp_waiting_next = zone;
		set->tcp_waiting_last = zone;
	}
	xfrd_deactivate_zone(zone);
	xfrd_unset_timer(zone);
}

int
xfrd_tcp_open(xfrd_tcp_set_t* set, struct xfrd_tcp_pipeline* tp,
	xfrd_zone_t* zone)
{
	int fd, family, conn;
	struct timeval tv;
	assert(zone->tcp_conn != -1);
	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s open tcp conn to %s",
		zone->apex_str, zone->master->ip_address_spec));
	tp->tcp_r->is_reading = 1;
	tp->tcp_r->total_bytes = 0;
	tp->tcp_r->msglen = 0;
	buffer_clear(tp->tcp_r->packet);
	tp->tcp_w->is_reading = 0;
	tp->tcp_w->total_bytes = 0;
	tp->tcp_w->msglen = 0;
	tp->connection_established = 0;

	if(zone->master->is_ipv6) {
#ifdef INET6
		family = PF_INET6;
#else
		xfrd_set_refresh_now(zone);
		return 0;
#endif
	} else {
		family = PF_INET;
	}
	fd = socket(family, SOCK_STREAM, IPPROTO_TCP);
	if(fd == -1) {
		log_msg(LOG_ERR, "xfrd: %s cannot create tcp socket: %s",
			zone->master->ip_address_spec, strerror(errno));
		xfrd_set_refresh_now(zone);
		return 0;
	}
	if(fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
		log_msg(LOG_ERR, "xfrd: fcntl failed: %s", strerror(errno));
		close(fd);
		xfrd_set_refresh_now(zone);
		return 0;
	}

	tp->ip_len = xfrd_acl_sockaddr_to(zone->master, &tp->ip);

	/* bind it */
	if (!xfrd_bind_local_interface(fd, zone->zone_options->pattern->
		outgoing_interface, zone->master, 1)) {
		close(fd);
		xfrd_set_refresh_now(zone);
		return 0;
        }

	conn = connect(fd, (struct sockaddr*)&tp->ip, tp->ip_len);
	if (conn == -1 && errno != EINPROGRESS) {
		log_msg(LOG_ERR, "xfrd: connect %s failed: %s",
			zone->master->ip_address_spec, strerror(errno));
		close(fd);
		xfrd_set_refresh_now(zone);
		return 0;
	}
	tp->tcp_r->fd = fd;
	tp->tcp_w->fd = fd;

	/* set the tcp pipe event */
	if(tp->handler_added)
		event_del(&tp->handler);
	event_set(&tp->handler, fd, EV_PERSIST|EV_TIMEOUT|EV_READ|EV_WRITE,
		xfrd_handle_tcp_pipe, tp);
	if(event_base_set(xfrd->event_base, &tp->handler) != 0)
		log_msg(LOG_ERR, "xfrd tcp: event_base_set failed");
	tv.tv_sec = set->tcp_timeout;
	tv.tv_usec = 0;
	if(event_add(&tp->handler, &tv) != 0)
		log_msg(LOG_ERR, "xfrd tcp: event_add failed");
	tp->handler_added = 1;
	return 1;
}

void
xfrd_tcp_setup_write_packet(struct xfrd_tcp_pipeline* tp, xfrd_zone_t* zone)
{
	xfrd_tcp_t* tcp = tp->tcp_w;
	assert(zone->tcp_conn != -1);
	assert(zone->tcp_waiting == 0);
	/* start AXFR or IXFR for the zone */
	if(zone->soa_disk_acquired == 0 || zone->master->use_axfr_only ||
						zone->master->ixfr_disabled) {
		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "request full zone transfer "
						"(AXFR) for %s to %s",
			zone->apex_str, zone->master->ip_address_spec));

		xfrd_setup_packet(tcp->packet, TYPE_AXFR, CLASS_IN, zone->apex,
			zone->query_id);
	} else {
		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "request incremental zone "
						"transfer (IXFR) for %s to %s",
			zone->apex_str, zone->master->ip_address_spec));

		xfrd_setup_packet(tcp->packet, TYPE_IXFR, CLASS_IN, zone->apex,
			zone->query_id);
        	NSCOUNT_SET(tcp->packet, 1);
		xfrd_write_soa_buffer(tcp->packet, zone->apex, &zone->soa_disk);
	}
	/* old transfer needs to be removed still? */
	if(zone->msg_seq_nr)
		xfrd_unlink_xfrfile(xfrd->nsd, zone->xfrfilenumber);
	zone->msg_seq_nr = 0;
	zone->msg_rr_count = 0;
	if(zone->master->key_options && zone->master->key_options->tsig_key) {
		xfrd_tsig_sign_request(tcp->packet, &zone->tsig, zone->master);
	}
	buffer_flip(tcp->packet);
	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "sent tcp query with ID %d", zone->query_id));
	tcp->msglen = buffer_limit(tcp->packet);
	tcp->total_bytes = 0;
}

static void
tcp_conn_ready_for_reading(xfrd_tcp_t* tcp)
{
	tcp->total_bytes = 0;
	tcp->msglen = 0;
	buffer_clear(tcp->packet);
}

int conn_write(xfrd_tcp_t* tcp)
{
	ssize_t sent;

	if(tcp->total_bytes < sizeof(tcp->msglen)) {
		uint16_t sendlen = htons(tcp->msglen);
		sent = write(tcp->fd,
			(const char*)&sendlen + tcp->total_bytes,
			sizeof(tcp->msglen) - tcp->total_bytes);

		if(sent == -1) {
			if(errno == EAGAIN || errno == EINTR) {
				/* write would block, try later */
				return 0;
			} else {
				return -1;
			}
		}

		tcp->total_bytes += sent;
		if(tcp->total_bytes < sizeof(tcp->msglen)) {
			/* incomplete write, resume later */
			return 0;
		}
		assert(tcp->total_bytes == sizeof(tcp->msglen));
	}

	assert(tcp->total_bytes < tcp->msglen + sizeof(tcp->msglen));

	sent = write(tcp->fd,
		buffer_current(tcp->packet),
		buffer_remaining(tcp->packet));
	if(sent == -1) {
		if(errno == EAGAIN || errno == EINTR) {
			/* write would block, try later */
			return 0;
		} else {
			return -1;
		}
	}

	buffer_skip(tcp->packet, sent);
	tcp->total_bytes += sent;

	if(tcp->total_bytes < tcp->msglen + sizeof(tcp->msglen)) {
		/* more to write when socket becomes writable again */
		return 0;
	}

	assert(tcp->total_bytes == tcp->msglen + sizeof(tcp->msglen));
	return 1;
}

void
xfrd_tcp_write(struct xfrd_tcp_pipeline* tp, xfrd_zone_t* zone)
{
	int ret;
	xfrd_tcp_t* tcp = tp->tcp_w;
	assert(zone->tcp_conn != -1);
	assert(zone == tp->tcp_send_first);
	/* see if for non-established connection, there is a connect error */
	if(!tp->connection_established) {
		/* check for pending error from nonblocking connect */
		/* from Stevens, unix network programming, vol1, 3rd ed, p450 */
		int error = 0;
		socklen_t len = sizeof(error);
		if(getsockopt(tcp->fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0){
			error = errno; /* on solaris errno is error */
		}
		if(error == EINPROGRESS || error == EWOULDBLOCK)
			return; /* try again later */
		if(error != 0) {
			log_msg(LOG_ERR, "%s: Could not tcp connect to %s: %s",
				zone->apex_str, zone->master->ip_address_spec,
				strerror(error));
			xfrd_tcp_pipe_stop(tp);
			return;
		}
	}
	ret = conn_write(tcp);
	if(ret == -1) {
		log_msg(LOG_ERR, "xfrd: failed writing tcp %s", strerror(errno));
		xfrd_tcp_pipe_stop(tp);
		return;
	}
	if(tcp->total_bytes != 0 && !tp->connection_established)
		tp->connection_established = 1;
	if(ret == 0) {
		return; /* write again later */
	}
	/* done writing this message */

	/* remove first zone from sendlist */
	tcp_pipe_sendlist_popfirst(tp, zone);

	/* see if other zone wants to write; init; let it write (now) */
	/* and use a loop, because 64k stack calls is a too much */
	while(tp->tcp_send_first) {
		/* setup to write for this zone */
		xfrd_tcp_setup_write_packet(tp, tp->tcp_send_first);
		/* attempt to write for this zone (if success, continue loop)*/
		ret = conn_write(tcp);
		if(ret == -1) {
			log_msg(LOG_ERR, "xfrd: failed writing tcp %s", strerror(errno));
			xfrd_tcp_pipe_stop(tp);
			return;
		}
		if(ret == 0)
			return; /* write again later */
		tcp_pipe_sendlist_popfirst(tp, tp->tcp_send_first);
	}

	/* if sendlist empty, remove WRITE from event */

	/* listen to READ, and not WRITE events */
	assert(tp->tcp_send_first == NULL);
	tcp_pipe_reset_timeout(tp);
}

int
conn_read(xfrd_tcp_t* tcp)
{
	ssize_t received;
	/* receive leading packet length bytes */
	if(tcp->total_bytes < sizeof(tcp->msglen)) {
		received = read(tcp->fd,
			(char*) &tcp->msglen + tcp->total_bytes,
			sizeof(tcp->msglen) - tcp->total_bytes);
		if(received == -1) {
			if(errno == EAGAIN || errno == EINTR) {
				/* read would block, try later */
				return 0;
			} else {
#ifdef ECONNRESET
				if (verbosity >= 2 || errno != ECONNRESET)
#endif /* ECONNRESET */
				log_msg(LOG_ERR, "tcp read sz: %s", strerror(errno));
				return -1;
			}
		} else if(received == 0) {
			/* EOF */
			return -1;
		}
		tcp->total_bytes += received;
		if(tcp->total_bytes < sizeof(tcp->msglen)) {
			/* not complete yet, try later */
			return 0;
		}

		assert(tcp->total_bytes == sizeof(tcp->msglen));
		tcp->msglen = ntohs(tcp->msglen);

		if(tcp->msglen == 0) {
			buffer_set_limit(tcp->packet, tcp->msglen);
			return 1;
		}
		if(tcp->msglen > buffer_capacity(tcp->packet)) {
			log_msg(LOG_ERR, "buffer too small, dropping connection");
			return 0;
		}
		buffer_set_limit(tcp->packet, tcp->msglen);
	}

	assert(buffer_remaining(tcp->packet) > 0);

	received = read(tcp->fd, buffer_current(tcp->packet),
		buffer_remaining(tcp->packet));
	if(received == -1) {
		if(errno == EAGAIN || errno == EINTR) {
			/* read would block, try later */
			return 0;
		} else {
#ifdef ECONNRESET
			if (verbosity >= 2 || errno != ECONNRESET)
#endif /* ECONNRESET */
			log_msg(LOG_ERR, "tcp read %s", strerror(errno));
			return -1;
		}
	} else if(received == 0) {
		/* EOF */
		return -1;
	}

	tcp->total_bytes += received;
	buffer_skip(tcp->packet, received);

	if(buffer_remaining(tcp->packet) > 0) {
		/* not complete yet, wait for more */
		return 0;
	}

	/* completed */
	assert(buffer_position(tcp->packet) == tcp->msglen);
	return 1;
}

void
xfrd_tcp_read(struct xfrd_tcp_pipeline* tp)
{
	xfrd_zone_t* zone;
	xfrd_tcp_t* tcp = tp->tcp_r;
	int ret;
	enum xfrd_packet_result pkt_result;

	ret = conn_read(tcp);
	if(ret == -1) {
		xfrd_tcp_pipe_stop(tp);
		return;
	}
	if(ret == 0)
		return;
	/* completed msg */
	buffer_flip(tcp->packet);
	/* see which ID number it is, if skip, handle skip, NULL: warn */
	if(tcp->msglen < QHEADERSZ) {
		/* too short for DNS header, skip it */
		DEBUG(DEBUG_XFRD,1, (LOG_INFO,
			"xfrd: tcp skip response that is too short"));
		tcp_conn_ready_for_reading(tcp);
		return;
	}
	zone = tp->id[ID(tcp->packet)];
	if(!zone || zone == TCP_NULL_SKIP) {
		/* no zone for this id? skip it */
		DEBUG(DEBUG_XFRD,1, (LOG_INFO,
			"xfrd: tcp skip response with %s ID",
			zone?"set-to-skip":"unknown"));
		tcp_conn_ready_for_reading(tcp);
		return;
	}
	assert(zone->tcp_conn != -1);

	/* handle message for zone */
	pkt_result = xfrd_handle_received_xfr_packet(zone, tcp->packet);
	/* setup for reading the next packet on this connection */
	tcp_conn_ready_for_reading(tcp);
	switch(pkt_result) {
		case xfrd_packet_more:
			/* wait for next packet */
			break;
		case xfrd_packet_newlease:
			/* set to skip if more packets with this ID */
			tp->id[zone->query_id] = TCP_NULL_SKIP;
			tp->num_skip++;
			/* fall through to remove zone from tp */
		case xfrd_packet_transfer:
			xfrd_tcp_release(xfrd->tcp_set, zone);
			assert(zone->round_num == -1);
			break;
		case xfrd_packet_notimpl:
			zone->master->ixfr_disabled = time(NULL);
			xfrd_tcp_release(xfrd->tcp_set, zone);
			/* query next server */
			xfrd_make_request(zone);
			break;
		case xfrd_packet_bad:
		case xfrd_packet_tcp:
		default:
			/* set to skip if more packets with this ID */
			tp->id[zone->query_id] = TCP_NULL_SKIP;
			tp->num_skip++;
			xfrd_tcp_release(xfrd->tcp_set, zone);
			/* query next server */
			xfrd_make_request(zone);
			break;
	}
}

void
xfrd_tcp_release(xfrd_tcp_set_t* set, xfrd_zone_t* zone)
{
	int conn = zone->tcp_conn;
	struct xfrd_tcp_pipeline* tp = set->tcp_state[conn];
	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s released tcp conn to %s",
		zone->apex_str, zone->master->ip_address_spec));
	assert(zone->tcp_conn != -1);
	assert(zone->tcp_waiting == 0);
	zone->tcp_conn = -1;
	zone->tcp_waiting = 0;

	/* remove from tcp_send list */
	tcp_pipe_sendlist_remove(tp, zone);
	/* remove it from the ID list */
	if(tp->id[zone->query_id] != TCP_NULL_SKIP)
		tcp_pipe_id_remove(tp, zone);
	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: released tcp pipe now %d unused",
		tp->num_unused));
	/* if pipe was full, but no more, then see if waiting element is
	 * for the same master, and can fill the unused ID */
	if(tp->num_unused == 1 && set->tcp_waiting_first) {
#ifdef INET6
		struct sockaddr_storage to;
#else
		struct sockaddr_in to;
#endif
		socklen_t to_len = xfrd_acl_sockaddr_to(
			set->tcp_waiting_first->master, &to);
		if(to_len == tp->ip_len && memcmp(&to, &tp->ip, to_len) == 0) {
			/* use this connnection for the waiting zone */
			zone = set->tcp_waiting_first;
			assert(zone->tcp_conn == -1);
			zone->tcp_conn = conn;
			tcp_zone_waiting_list_popfirst(set, zone);
			if(zone->zone_handler.ev_fd != -1)
				xfrd_udp_release(zone);
			xfrd_unset_timer(zone);
			pipeline_setup_new_zone(set, tp, zone);
			return;
		}
		/* waiting zone did not go to same server */
	}

	/* if all unused, or only skipped leftover, close the pipeline */
	if(tp->num_unused >= ID_PIPE_NUM || tp->num_skip >= ID_PIPE_NUM - tp->num_unused)
		xfrd_tcp_pipe_release(set, tp, conn);
}

void
xfrd_tcp_pipe_release(xfrd_tcp_set_t* set, struct xfrd_tcp_pipeline* tp,
	int conn)
{
	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: tcp pipe released"));
	/* one handler per tcp pipe */
	if(tp->handler_added)
		event_del(&tp->handler);
	tp->handler_added = 0;

	/* fd in tcp_r and tcp_w is the same, close once */
	if(tp->tcp_r->fd != -1)
		close(tp->tcp_r->fd);
	tp->tcp_r->fd = -1;
	tp->tcp_w->fd = -1;

	/* remove from pipetree */
	(void)rbtree_delete(xfrd->tcp_set->pipetree, &tp->node);

	/* a waiting zone can use the free tcp slot (to another server) */
	/* if that zone fails to set-up or connect, we try to start the next
	 * waiting zone in the list */
	while(set->tcp_count == XFRD_MAX_TCP && set->tcp_waiting_first) {
		int i;

		/* pop first waiting process */
		xfrd_zone_t* zone = set->tcp_waiting_first;
		/* start it */
		assert(zone->tcp_conn == -1);
		zone->tcp_conn = conn;
		tcp_zone_waiting_list_popfirst(set, zone);

		/* stop udp (if any) */
		if(zone->zone_handler.ev_fd != -1)
			xfrd_udp_release(zone);
		if(!xfrd_tcp_open(set, tp, zone)) {
			zone->tcp_conn = -1;
			xfrd_set_refresh_now(zone);
			/* try to start the next zone (if any) */
			continue;
		}
		/* re-init this tcppipe */
		/* ip and ip_len set by tcp_open */
		tp->node.key = tp;
		tp->num_unused = ID_PIPE_NUM;
		tp->num_skip = 0;
		tp->tcp_send_first = NULL;
		tp->tcp_send_last = NULL;
		memset(tp->id, 0, sizeof(tp->id));
		for(i=0; i<ID_PIPE_NUM; i++) {
			tp->unused[i] = i;
		}

		/* insert into tree */
		(void)rbtree_insert(set->pipetree, &tp->node);
		/* setup write */
		xfrd_unset_timer(zone);
		pipeline_setup_new_zone(set, tp, zone);
		/* started a task, no need for cleanups, so return */
		return;
	}
	/* no task to start, cleanup */
	assert(!set->tcp_waiting_first);
	set->tcp_count --;
	assert(set->tcp_count >= 0);
}

