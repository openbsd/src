/*	$OpenBSD: bfd.c,v 1.28 2016/09/18 21:00:55 phessler Exp $	*/

/*
 * Copyright (c) 2016 Peter Hessler <phessler@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Support for Bi-directional Forwarding Detection (RFC 5880 / 5881)
 */

#include <sys/errno.h>
#include <sys/param.h>

#include <sys/task.h>
#include <sys/pool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stdint.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <net/bfd.h>

/*
 * RFC 5880 Page 7
 * The Mandatory Section of a BFD Control packet has the following
 * format:
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |Vers | Diag    |Sta|P|F|C|A|D|M|  Detect Mult  |    Length     |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                      My Discriminator                         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                     Your Discriminator                        |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                  Desired Min TX Interval                      |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                 Required Min RX Interval                      |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                Required Min Echo RX Interval                  |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *
 * An optional Authentication Section MAY be present:
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |   Auth Type   |   Auth Len    |     Authentication Data...    |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 */

/* BFD on-wire format */
struct bfd_header {
	uint8_t	bfd_ver_diag;
	uint8_t	bfd_sta_flags;

	uint8_t	bfd_detect_multi;		/* detection time multiplier */
	uint8_t	bfd_length;			/* in bytes */
	uint32_t	bfd_my_discriminator;		/* From this system */
	uint32_t	bfd_your_discriminator;		/* Received */
	uint32_t	bfd_desired_min_tx_interval;	/* in microseconds */
	uint32_t	bfd_required_min_rx_interval;	/* in microseconds */
	uint32_t	bfd_required_min_echo_interval;	/* in microseconds */
} __packed;

/* optional authentication on-wire format */
struct bfd_auth_header {
	uint8_t	bfd_auth_type;
	uint8_t	bfd_auth_len;
	uint16_t	bfd_auth_data;
} __packed;

#define BFD_VERSION		1	/* RFC 5880 Page 6 */
#define BFD_VER(x)		(((x) & 0xe0) >> 5)
#define BFD_DIAG(x)		((x) & 0x1f)
#define BFD_STATE(x)		(((x) & 0xf0) >> 6)
#define BFD_FLAGS(x)		((x) & 0x0f)
#define BFD_HDRLEN		24	/* RFC 5880 Page 37 */
#define BFD_AUTH_SIMPLE_LEN	16 + 3	/* RFC 5880 Page 10 */
#define BFD_AUTH_MD5_LEN	24	/* RFC 5880 Page 11 */
#define BFD_AUTH_SHA1_LEN	28	/* RFC 5880 Page 12 */

/* Diagnostic Code (RFC 5880 Page 8) */
#define BFD_DIAG_NONE			0
#define BFD_DIAG_EXPIRED		1
#define BFD_DIAG_ECHO_FAILED		2
#define BFD_DIAG_NEIGHBOR_SIGDOWN	3
#define BFD_DIAG_FIB_RESET		4
#define BFD_DIAG_PATH_DOWN		5
#define BFD_DIAG_CONCAT_PATH_DOWN	6
#define BFD_DIAG_ADMIN_DOWN		7
#define BFD_DIAG_CONCAT_REVERSE_DOWN	8

/* State (RFC 5880 Page 8) */
#define BFD_STATE_ADMINDOWN		0
#define BFD_STATE_DOWN			1
#define BFD_STATE_INIT			2
#define BFD_STATE_UP			3

/* Flags (RFC 5880 Page 8) */
#define BFD_FLAG_P			0x20
#define BFD_FLAG_F			0x10
#define BFD_FLAG_C			0x08
#define BFD_FLAG_A			0x04
#define BFD_FLAG_D			0x02
#define BFD_FLAG_M			0x01


/* Auth Type (RFC 5880 Page 10) */
#define BFD_AUTH_TYPE_RESERVED		0
#define BFD_AUTH_TYPE_SIMPLE		1
#define BFD_AUTH_KEYED_MD5		2
#define BFD_AUTH_METICULOUS_MD5		3
#define BFD_AUTH_KEYED_SHA1		4
#define BFD_AUTH_METICULOUS_SHA1	5

#define BFD_UDP_PORT_CONTROL		3784
#define BFD_UDP_PORT_ECHO		3785

#define BFD_SECOND			1000000 /* 1,000,000 us == 1 second */
/* We cannot handle more often than 10ms, so force a minimum */
#define BFD_MINIMUM			10000	/* 10,000 us == 10 ms */


struct pool	 bfd_pool, bfd_pool_neigh, bfd_pool_time;
struct taskq	*bfdtq;


struct bfd_config *bfd_lookup(struct rtentry *);
void		 bfddestroy(void);

struct socket	*bfd_listener(struct bfd_config *, unsigned int);
struct socket	*bfd_sender(struct bfd_config *, unsigned int);
void		 bfd_input(struct bfd_config *, struct mbuf *);
void		 bfd_set_state(struct bfd_config *, int);

int	 bfd_send(struct bfd_config *, struct mbuf *);
void	 bfd_send_control(void *);

void	 bfd_start_task(void *);
void	 bfd_send_task(void *);
void	 bfd_timeout_rx(void *);
void	 bfd_timeout_tx(void *);

void	 bfd_upcall(struct socket *, caddr_t, int);
void	 bfd_senddown(struct bfd_config *);
void	 bfd_reset(struct bfd_config *);
void	 bfd_set_uptime(struct bfd_config *);

void	 bfd_debug(struct bfd_config *);

TAILQ_HEAD(bfd_queue, bfd_config)  bfd_queue;

/*
 * allocate a new bfd session
 */
int
bfdset(struct rtentry *rt)
{
	struct bfd_config	*bfd;

	/* at the moment it is not allowed to run BFD on indirect routes */
	if (ISSET(rt->rt_flags, RTF_GATEWAY) || !ISSET(rt->rt_flags, RTF_HOST))
		return (EINVAL);

	/* make sure we don't already have this setup */
	if (bfd_lookup(rt) != NULL)
		return (EADDRINUSE);

	/* Do our necessary memory allocations upfront */
	bfd = pool_get(&bfd_pool, PR_WAITOK | PR_ZERO);
	bfd->bc_neighbor = pool_get(&bfd_pool_neigh, PR_WAITOK | PR_ZERO);
	bfd->bc_time = pool_get(&bfd_pool_time, PR_WAITOK | PR_ZERO);

	bfd->bc_rt = rt;
	rtref(bfd->bc_rt);	/* we depend on this route not going away */

	microtime(bfd->bc_time);
	bfd_reset(bfd);
	bfd->bc_neighbor->bn_ldiscr = arc4random();	/* XXX - MUST be globally unique */

	if (!timeout_initialized(&bfd->bc_timo_rx))
		timeout_set(&bfd->bc_timo_rx, bfd_timeout_rx, bfd);
	if (!timeout_initialized(&bfd->bc_timo_tx))
		timeout_set(&bfd->bc_timo_tx, bfd_timeout_tx, bfd);

	task_set(&bfd->bc_bfd_task, bfd_start_task, bfd);
	task_add(bfdtq, &bfd->bc_bfd_task);

	TAILQ_INSERT_TAIL(&bfd_queue, bfd, bc_entry);

	return (0);
}

/*
 * remove and free a bfd session
 */
void
bfdclear(struct rtentry *rt)
{
	struct bfd_config *bfd;

	if ((bfd = bfd_lookup(rt)) == NULL)
		return;

	timeout_del(&bfd->bc_timo_rx);
	timeout_del(&bfd->bc_timo_tx);
	task_del(bfdtq, &bfd->bc_bfd_send_task);

/* XXX - punt this off to a task */
	TAILQ_REMOVE(&bfd_queue, bfd, bc_entry);

	/* send suicide packets immediately */
	if (rtisvalid(bfd->bc_rt))
		bfd_senddown(bfd);

	if (bfd->bc_so) {
		/* remove upcall before calling soclose or it will be called */
		bfd->bc_so->so_upcall = NULL;
		soclose(bfd->bc_so);
	}
	if (bfd->bc_soecho) {
		bfd->bc_soecho->so_upcall = NULL;
		soclose(bfd->bc_soecho);
	}
	if (bfd->bc_sosend)
		soclose(bfd->bc_sosend);

	rtfree(bfd->bc_rt);

	pool_put(&bfd_pool_time, bfd->bc_time);
	pool_put(&bfd_pool_neigh, bfd->bc_neighbor);
	pool_put(&bfd_pool, bfd);
}

/*
 * Create and initialize the global bfd framework
 */
void
bfdinit(void)
{
	pool_init(&bfd_pool, sizeof(struct bfd_config), 0,
	    IPL_SOFTNET, 0, "bfd_config", NULL);
	pool_init(&bfd_pool_neigh, sizeof(struct bfd_neighbor), 0,
	    IPL_SOFTNET, 0, "bfd_config_peer", NULL);
	pool_init(&bfd_pool_time, sizeof(struct timeval), 0,
	    IPL_SOFTNET, 0, "bfd_config_time", NULL);

	bfdtq = taskq_create("bfd", 1, IPL_SOFTNET, 0);
	if (bfdtq == NULL)
		panic("unable to create BFD taskq");

	TAILQ_INIT(&bfd_queue);
}

/* 
 * Destroy all bfd sessions and remove the tasks
 *
 */
void
bfddestroy(void)
{
	struct bfd_config	*bfd;

	/* send suicide packets immediately */
	while ((bfd = TAILQ_FIRST(&bfd_queue))) {
		bfdclear(bfd->bc_rt);
	}

	taskq_destroy(bfdtq);
	pool_destroy(&bfd_pool_time);
	pool_destroy(&bfd_pool_neigh);
	pool_destroy(&bfd_pool);
}

/*
 * Return the matching bfd
 */
struct bfd_config *
bfd_lookup(struct rtentry *rt)
{
	struct bfd_config *bfd;

	TAILQ_FOREACH(bfd, &bfd_queue, bc_entry) {
		if (bfd->bc_rt == rt)
			return (bfd);
	}
	return (NULL);
}

/*
 * End of public interfaces.
 *
 * Everything below this line should not be used outside of this file.
 */

/*
 * Task to listen and kick off the bfd process
 */
void
bfd_start_task(void *arg)
{
	struct bfd_config	*bfd = (struct bfd_config *)arg;

	/* start listeners */
	bfd->bc_so = bfd_listener(bfd, BFD_UDP_PORT_CONTROL);
	if (!bfd->bc_so)
		printf("bfd_listener(%d) failed\n",
		    BFD_UDP_PORT_CONTROL);
	bfd->bc_soecho = bfd_listener(bfd, BFD_UDP_PORT_ECHO);
	if (!bfd->bc_soecho)
		printf("bfd_listener(%d) failed\n",
		    BFD_UDP_PORT_ECHO);

	/* start sending */
	bfd->bc_sosend = bfd_sender(bfd, BFD_UDP_PORT_CONTROL);
	if (bfd->bc_sosend) {
		task_set(&bfd->bc_bfd_send_task, bfd_send_task, bfd);
		task_add(bfdtq, &bfd->bc_bfd_send_task);	
	}

	return;
}

void
bfd_send_task(void *arg)
{
	struct bfd_config	*bfd = (struct bfd_config *)arg;
	struct rtentry		*rt = bfd->bc_rt;

	if (ISSET(rt->rt_flags, RTF_UP)) {
		bfd_send_control(bfd);
	} else {
		bfd->bc_error++;
		bfd->bc_neighbor->bn_ldiag = BFD_DIAG_ADMIN_DOWN;
		if (bfd->bc_neighbor->bn_lstate > BFD_STATE_DOWN) {
			bfd_reset(bfd);
			bfd_set_state(bfd, BFD_STATE_DOWN);
		}
	}

	/* re-add 70%-90% jitter to our transmits, rfc 5880 6.8.7 */
	timeout_add_usec(&bfd->bc_timo_tx,
	    bfd->bc_mintx * (arc4random_uniform(20) + 70) / 100);
}

/*
 * Setup a bfd listener socket
 */
struct socket *
bfd_listener(struct bfd_config *bfd, unsigned int port)
{
	struct proc		*p = curproc;
	struct rtentry		*rt = bfd->bc_rt;
	struct sockaddr		*src = rt->rt_ifa->ifa_addr;
	struct sockaddr		*dst = rt_key(rt);
	struct sockaddr		*sa;
	struct sockaddr_in	*sin;
	struct sockaddr_in6	*sin6;
	struct socket		*so;
	struct mbuf		*m = NULL, *mopt = NULL;
	int			*ip, error;

	/* sa_family and sa_len must be equal */
	if (src->sa_family != dst->sa_family || src->sa_len != dst->sa_len)
		return (NULL);

	error = socreate(dst->sa_family, &so, SOCK_DGRAM, 0);
	if (error) {
		printf("%s: socreate error %d\n",
		    __func__, error);
		return (NULL);
	}

	MGET(mopt, M_WAIT, MT_SOOPTS);
	mopt->m_len = sizeof(int);
	ip = mtod(mopt, int *);
	*ip = MAXTTL;
	error = sosetopt(so, IPPROTO_IP, IP_MINTTL, mopt);
	if (error) {
		printf("%s: sosetopt error %d\n",
		    __func__, error);
		goto close;
	}

	MGET(m, M_WAIT, MT_SONAME);
	m->m_len = src->sa_len;
	sa = mtod(m, struct sockaddr *);
	memcpy(sa, src, src->sa_len);
	switch(sa->sa_family) {
	case AF_INET:
		sin = (struct sockaddr_in *)sa;
		sin->sin_port = htons(port);
		break;
	case AF_INET6:
		sin6 = (struct sockaddr_in6 *)sa;
		sin6->sin6_port = htons(port);
		break;
	default:
		break;
	}

	error = sobind(so, m, p);
	if (error) {
		printf("%s: sobind error %d\n",
		    __func__, error);
		goto close;
	}
	so->so_upcallarg = (caddr_t)bfd;
	so->so_upcall = bfd_upcall;

	return (so);

 close:
	m_free(m);
	soclose(so);

	return (NULL);
}

/*
 * Setup the bfd sending process
 */
struct socket *
bfd_sender(struct bfd_config *bfd, unsigned int port)
{
	struct socket 		*so;
	struct rtentry		*rt = bfd->bc_rt;
	struct proc		*p = curproc;
	struct mbuf		*m = NULL, *mopt = NULL;
	struct sockaddr		*src = rt->rt_ifa->ifa_addr;
	struct sockaddr		*dst = rt_key(rt);
	struct sockaddr		*sa;
	struct sockaddr_in6	*sin6;
	struct sockaddr_in	*sin;
	int		 error, *ip;

	/* sa_family and sa_len must be equal */
	if (src->sa_family != dst->sa_family || src->sa_len != dst->sa_len)
		return (NULL);

	error = socreate(dst->sa_family, &so, SOCK_DGRAM, 0);

	if (error)
		return (NULL);

	MGET(mopt, M_WAIT, MT_SOOPTS);
	mopt->m_len = sizeof(int);
	ip = mtod(mopt, int *);
	*ip = IP_PORTRANGE_HIGH;
	error = sosetopt(so, IPPROTO_IP, IP_PORTRANGE, mopt);
	if (error) {
		printf("%s: sosetopt error %d\n",
		    __func__, error);
		goto close;
	}

	MGET(mopt, M_WAIT, MT_SOOPTS);
	mopt->m_len = sizeof(int);
	ip = mtod(mopt, int *);
	*ip = MAXTTL;
	error = sosetopt(so, IPPROTO_IP, IP_TTL, mopt);
	if (error) {
		printf("%s: sosetopt error %d\n",
		    __func__, error);
		goto close;
	}

	MGET(m, M_WAIT, MT_SONAME);
	m->m_len = src->sa_len;
	sa = mtod(m, struct sockaddr *);
	memcpy(sa, src, src->sa_len);
	switch(sa->sa_family) {
	case AF_INET:
		sin = (struct sockaddr_in *)sa;
		sin->sin_port = 0;
		break;
	case AF_INET6:
		sin6 = (struct sockaddr_in6 *)sa;
		sin6->sin6_port = 0;
		break;
	default:
		break;
	}

	error = sobind(so, m, p);
	if (error) {
		printf("%s: sobind error %d\n",
		    __func__, error);
		goto close;
	}

	memcpy(sa, dst, dst->sa_len);
	switch(sa->sa_family) {
	case AF_INET:
		sin = (struct sockaddr_in *)sa;
		sin->sin_port = ntohs(port);
		break;
	case AF_INET6:
		sin6 = (struct sockaddr_in6 *)sa;
		sin6->sin6_port = ntohs(port);
		break;
	default:
		break;
	}

	error = soconnect(so, m);
	if (error && error != ECONNREFUSED) {
		printf("%s: soconnect error %d\n",
		    __func__, error);
		goto close;
	}

	m_free(m);

	return (so);

 close:
	m_free(m);
	soclose(so);

	return (NULL);
}

/*
 * Will be called per-received packet
 */
void
bfd_upcall(struct socket *so, caddr_t arg, int waitflag)
{
	struct bfd_config *bfd = (struct bfd_config *)arg;
	struct mbuf *m;
	struct uio uio;
	int flags, error;

	uio.uio_procp = NULL;
	do {
		uio.uio_resid = 1000000000;
		flags = MSG_DONTWAIT;
		error = soreceive(so, NULL, &uio, &m, NULL, &flags, 0);
		if (error && error != EAGAIN) {
			bfd->bc_error++;
			return;
		}
		if (m != NULL)
			bfd_input(bfd, m);
	} while (m != NULL);

	return;
}


void
bfd_timeout_tx(void *v)
{
	struct bfd_config *bfd = v;
	task_add(bfdtq, &bfd->bc_bfd_send_task);	
}

/*
 * Triggers when we do not receive a valid packet in time
 */
void
bfd_timeout_rx(void *v)
{
	struct bfd_config *bfd = v;


	if ((bfd->bc_neighbor->bn_lstate > BFD_STATE_DOWN) &&
	    (++bfd->bc_error >= bfd->bc_neighbor->bn_mult)) {
		bfd->bc_neighbor->bn_ldiag = BFD_DIAG_EXPIRED;
printf("%s: failed, bfd->bc_error %u\n", __func__, bfd->bc_error);
		bfd_reset(bfd);
		bfd_set_state(bfd, BFD_STATE_DOWN);

		return;
	}

	timeout_add_usec(&bfd->bc_timo_rx, bfd->bc_minrx);
}

/*
 * Tell our neighbor that we are going down
 */
void
bfd_senddown(struct bfd_config *bfd)
{
	/* If we are down, return early */
	if (bfd->bc_state < BFD_STATE_INIT)
		return;

	bfd->bc_neighbor->bn_lstate = BFD_STATE_ADMINDOWN;
	if (bfd->bc_neighbor->bn_ldiag == 0)
		bfd->bc_neighbor->bn_ldiag = BFD_DIAG_ADMIN_DOWN;

	bfd_send_control(bfd);

	return;
}

/*
 * Clean a BFD peer to defaults
 */
void
bfd_reset(struct bfd_config *bfd)
{
if (bfd->bc_error)
printf("%s: error=%u\n", __func__, bfd->bc_error);

	/* Clean */
	bfd->bc_neighbor->bn_rdiscr = 0;
	bfd->bc_neighbor->bn_demand = 0;
	bfd->bc_neighbor->bn_rdemand = 0;
	bfd->bc_neighbor->bn_authtype = 0;
	bfd->bc_neighbor->bn_rauthseq = 0;
	bfd->bc_neighbor->bn_lauthseq = 0;
	bfd->bc_neighbor->bn_authseqknown = 0;
	bfd->bc_neighbor->bn_ldiag = 0;

	bfd->bc_mode = BFD_MODE_ASYNC;
	bfd->bc_state = BFD_STATE_DOWN;

	/* Set RFC mandated values */
	bfd->bc_neighbor->bn_lstate = BFD_STATE_DOWN;
	bfd->bc_neighbor->bn_rstate = BFD_STATE_DOWN;
	bfd->bc_neighbor->bn_mintx = BFD_SECOND;
	bfd->bc_neighbor->bn_req_minrx = BFD_SECOND; /* rfc5880 6.8.18 */
	bfd->bc_neighbor->bn_rminrx = 1;
	bfd->bc_neighbor->bn_mult = 3; /* XXX - MUST be nonzero */

	bfd->bc_mintx = bfd->bc_neighbor->bn_mintx;
	bfd->bc_minrx = bfd->bc_neighbor->bn_rminrx;
	bfd->bc_multiplier = bfd->bc_neighbor->bn_mult;

	bfd_set_uptime(bfd);
printf("%s: localdiscr: %u\n", __func__, bfd->bc_neighbor->bn_ldiscr);

	return;
}

void
bfd_input(struct bfd_config *bfd, struct mbuf *m)
{
	struct bfd_header	*peer;
	struct bfd_auth_header	*auth;
	struct mbuf		*mp, *mp0;
	unsigned int		 ver, diag, state, flags;
	int			 offp;

	mp = m_pulldown(m, 0, sizeof(*peer), &offp);

	if (mp == NULL)
		return;
	peer = (struct bfd_header *)(mp->m_data + offp);

	/* We only support BFD Version 1 */
	if (( ver = BFD_VER(peer->bfd_ver_diag)) != 1)
		goto discard;

	diag = BFD_DIAG(peer->bfd_ver_diag);
	state = BFD_STATE(peer->bfd_sta_flags);
	flags = BFD_FLAGS(peer->bfd_sta_flags);

	if (peer->bfd_length + offp != mp->m_len) {
		printf("%s: bad len %d != %d\n", __func__, peer->bfd_length + offp, mp->m_len);
		goto discard;
	}

	if (peer->bfd_detect_multi == 0)
		goto discard;
	if  (ntohl(peer->bfd_my_discriminator) == 0)
		goto discard;
	if (ntohl(peer->bfd_your_discriminator) == 0 &&
	    BFD_STATE(peer->bfd_sta_flags) > BFD_STATE_DOWN)
		goto discard;
	if ((ntohl(peer->bfd_your_discriminator) != 0) &&
	    (ntohl(peer->bfd_your_discriminator) != bfd->bc_neighbor->bn_ldiscr)) {
		bfd->bc_error++;
printf("%s: peer your discr %u != local %u\n",
    __func__, ntohl(peer->bfd_your_discriminator), bfd->bc_neighbor->bn_ldiscr);
		bfd->bc_neighbor->bn_ldiag = BFD_DIAG_EXPIRED;
		bfd_senddown(bfd);
		goto discard;
	}

	if ((flags & BFD_FLAG_A) && bfd->bc_neighbor->bn_authtype == 0)
		goto discard;
	if (!(flags & BFD_FLAG_A) && bfd->bc_neighbor->bn_authtype != 0)
		goto discard;
	if (flags & BFD_FLAG_A) {
		mp0 = m_pulldown(mp, 0, sizeof(*auth), &offp);
		if (mp0 == NULL)
			goto discard;
		auth = (struct bfd_auth_header *)(mp0->m_data + offp);
#if 0
		if (bfd_process_auth(bfd, auth) != 0) {
			m_free(mp0);
			goto discard;
		}
#endif
	}

	if ((bfd->bc_neighbor->bn_rdiscr == 0) &&
	    (ntohl(peer->bfd_my_discriminator) != 0))
		bfd->bc_neighbor->bn_rdiscr = ntohl(peer->bfd_my_discriminator);

	if (bfd->bc_neighbor->bn_rdiscr != ntohl(peer->bfd_my_discriminator))
		goto discard;

	bfd->bc_neighbor->bn_rstate = state;

	bfd->bc_neighbor->bn_rminrx =
	    ntohl(peer->bfd_required_min_rx_interval);
	/* Local change to the algorithm, we don't accept below 10ms */
	if (bfd->bc_neighbor->bn_req_minrx < BFD_MINIMUM)
		goto discard;
	/*
	 * Local change to the algorithm, we can't use larger than signed
	 * 32bits for a timeout.
	 * That is Too Long(tm) anyways.
	 */
	if (bfd->bc_neighbor->bn_req_minrx > INT32_MAX)
		goto discard;
	bfd->bc_minrx = bfd->bc_neighbor->bn_req_minrx;

	bfd->bc_neighbor->bn_mintx =
	    htonl(peer->bfd_desired_min_tx_interval);
	if (bfd->bc_neighbor->bn_lstate != BFD_STATE_UP)
		bfd->bc_neighbor->bn_mintx = BFD_SECOND;

	bfd->bc_neighbor->bn_req_minrx =
	    ntohl(peer->bfd_required_min_rx_interval);

	/* rfc5880 6.8.7 */
	bfd->bc_mintx = max(bfd->bc_neighbor->bn_rminrx,
	    bfd->bc_neighbor->bn_mintx);

	if (bfd->bc_neighbor->bn_rstate == BFD_STATE_ADMINDOWN) {
		if (bfd->bc_neighbor->bn_lstate != BFD_STATE_DOWN) {
			bfd->bc_neighbor->bn_ldiag = BFD_DIAG_NEIGHBOR_SIGDOWN;
			bfd->bc_neighbor->bn_lstate = BFD_STATE_DOWN;
			bfd_set_state(bfd, BFD_STATE_DOWN);
		}
		goto discard;
	}

	/* According the to pseudo-code RFC 5880 page 34 */
	if (bfd->bc_neighbor->bn_lstate == BFD_STATE_DOWN) {
printf("%s: BFD_STATE_DOWN remote 0x%x  ", __func__, ntohl(peer->bfd_my_discriminator));
printf("local 0x%x\n", ntohl(peer->bfd_your_discriminator));
bfd_debug(bfd);
		if (bfd->bc_neighbor->bn_rstate == BFD_STATE_DOWN)
			bfd->bc_neighbor->bn_lstate = BFD_STATE_INIT;
		else if (bfd->bc_neighbor->bn_rstate == BFD_STATE_INIT) {
printf("%s: set BFD_STATE_UP\n", __func__);
			bfd->bc_neighbor->bn_ldiag = 0;
			bfd_set_state(bfd, BFD_STATE_UP);
		}
	} else if (bfd->bc_neighbor->bn_lstate == BFD_STATE_INIT) {
printf("%s: BFD_STATE_INIT remote 0x%x  ", __func__, ntohl(peer->bfd_my_discriminator));
printf("local 0x%x\n", ntohl(peer->bfd_your_discriminator));

		if (bfd->bc_neighbor->bn_rstate >= BFD_STATE_INIT) {
printf("%s: set BFD_STATE_UP\n", __func__);
			bfd->bc_neighbor->bn_ldiag = 0;
			bfd_set_state(bfd, BFD_STATE_UP);
		} else {
			goto discard;
		}
	} else {
		if (bfd->bc_neighbor->bn_rstate == BFD_STATE_DOWN) {
printf("%s: set BFD_STATE_DOWN\n", __func__);
			bfd->bc_neighbor->bn_ldiag = BFD_DIAG_NEIGHBOR_SIGDOWN;
			bfd_set_state(bfd, BFD_STATE_DOWN);
			goto discard;
		}
	}

	if (bfd->bc_neighbor->bn_lstate == BFD_STATE_UP) {
		bfd->bc_neighbor->bn_ldiag = 0;
		bfd->bc_neighbor->bn_demand = 1;
		bfd->bc_neighbor->bn_rdemand = (flags & BFD_FLAG_D);
	}

	bfd->bc_error = 0;

 discard:
	m_free(m);

	//XXX task_add(bfdtq, &bfd->bc_bfd_send_task);	
	timeout_add_usec(&bfd->bc_timo_rx, bfd->bc_minrx);

	return;
}

void
bfd_set_state(struct bfd_config *bfd, int state)
{
	struct ifnet	*ifp;
	struct rtentry	*rt = bfd->bc_rt;
	int		 new_state;

	ifp = if_get(rt->rt_ifidx);
	if (ifp == NULL) {
		printf("%s: cannot find interface index %u\n",
		    __func__, rt->rt_ifidx);
		bfd->bc_error++;
		bfd_reset(bfd);
		return;
	}

	bfd_set_uptime(bfd);

	bfd->bc_state = bfd->bc_neighbor->bn_lstate = state;

	if (!rtisvalid(rt))
		bfd->bc_neighbor->bn_lstate = BFD_STATE_ADMINDOWN;

	switch (bfd->bc_neighbor->bn_lstate) {
	case BFD_STATE_ADMINDOWN:
		new_state = RTM_BFD;
//		rt->rt_flags &= ~RTF_BFDUP;
//		rt->rt_flags |= RTF_BFDDOWN;
		break;
	case BFD_STATE_DOWN:
		new_state = RTM_BFD;
//		rt->rt_flags &= ~RTF_BFDUP;
//		rt->rt_flags |= RTF_BFDDOWN;
		break;
	case BFD_STATE_UP:
		new_state = RTM_BFD;
//		rt->rt_flags &= ~RTF_BFDDOWN;
//		rt->rt_flags |= RTF_BFDUP;
		break;
	}

printf("%s: BFD set linkstate %u (oldstate: %u)\n", ifp->if_xname, new_state, state);
	rt_sendmsg(rt, new_state, ifp->if_rdomain);

	if_put(ifp);

	return;
}

void
bfd_set_uptime(struct bfd_config *bfd)
{
	struct timeval tv;

	microtime(&tv);
	bfd->bc_lastuptime = tv.tv_sec - bfd->bc_time->tv_sec;
	bfd->bc_laststate = bfd->bc_state;
	memcpy(bfd->bc_time, &tv, sizeof(tv));
}

void
bfd_send_control(void *x)
{
	struct bfd_config	*bfd = x;
	struct mbuf		*m;
	struct bfd_header	*h;
	int error;

	MGETHDR(m, M_WAIT, MT_DATA);
	MCLGET(m, M_WAIT);

	m->m_len = m->m_pkthdr.len = sizeof(*bfd);
	h = mtod(m, struct bfd_header *);

	memset(h, 0xff, sizeof(*h));	/* canary */

	h->bfd_ver_diag = ((BFD_VERSION << 5) | (bfd->bc_neighbor->bn_ldiag));
	h->bfd_sta_flags = (bfd->bc_neighbor->bn_lstate << 6);
	h->bfd_detect_multi = bfd->bc_neighbor->bn_mult;
	h->bfd_length = BFD_HDRLEN;
	h->bfd_my_discriminator = htonl(bfd->bc_neighbor->bn_ldiscr);
	h->bfd_your_discriminator = htonl(bfd->bc_neighbor->bn_rdiscr);

	h->bfd_desired_min_tx_interval =
	    htonl(bfd->bc_neighbor->bn_mintx);
	h->bfd_required_min_rx_interval =
	    htonl(bfd->bc_neighbor->bn_req_minrx);
	h->bfd_required_min_echo_interval =
	    htonl(bfd->bc_neighbor->bn_rminrx);

	error = bfd_send(bfd, m);

	if (error) {
		if (!(error == EHOSTDOWN || error == ECONNREFUSED)) {
			printf("%s: %u\n", __func__, error);
		}
	}
}

int
bfd_send(struct bfd_config *bfd, struct mbuf *m)
{
	struct rtentry *rt = bfd->bc_rt;

	if (!ISSET(rt->rt_flags, RTF_UP)) {
		m_freem(m);
		return (EHOSTDOWN);
	}

	return (sosend(bfd->bc_sosend, NULL, NULL, m, NULL, MSG_DONTWAIT));
}

/*
 * Print debug information about this bfd instance
 */
void
bfd_debug(struct bfd_config *bfd)
{
	struct rtentry	*rt = bfd->bc_rt;
	struct timeval	 tv;
	char buf[64];

	printf("dest: %s ", sockaddr_ntop(rt_key(rt), buf, sizeof(buf)));
	printf("src: %s ", sockaddr_ntop(rt->rt_ifa->ifa_addr, buf,
	    sizeof(buf)));
	printf("\n");
	printf("\t");
	printf("session state: %u ", bfd->bc_state);
	printf("mode: %u ", bfd->bc_mode);
	printf("error: %u ", bfd->bc_error);
	printf("minrx: %u ", bfd->bc_minrx);
	printf("mintx: %u ", bfd->bc_mintx);
	printf("multiplier: %u ", bfd->bc_multiplier);
	printf("\n");
	printf("\t");
	printf("local session state: %u ", bfd->bc_neighbor->bn_lstate);
	printf("local diag: %u ", bfd->bc_neighbor->bn_ldiag);
	printf("\n");
	printf("\t");
	printf("remote discriminator: %u ", bfd->bc_neighbor->bn_rdiscr);
	printf("local discriminator: %u ", bfd->bc_neighbor->bn_ldiscr);
	printf("\n");
	printf("\t");
	printf("remote session state: %u ", bfd->bc_neighbor->bn_rstate);
	printf("remote diag: %u ", bfd->bc_neighbor->bn_rdiag);
	printf("remote min rx: %u ", bfd->bc_neighbor->bn_rminrx);
	printf("\n");
	printf("\t");
	printf("last state: %u ", bfd->bc_laststate);
	getmicrotime(&tv);
	printf("uptime %llds ", tv.tv_sec - bfd->bc_time->tv_sec);
	printf("time started %lld.%06ld ", bfd->bc_time->tv_sec,
	    bfd->bc_time->tv_usec);
	printf("last uptime %llds ", bfd->bc_lastuptime);
	printf("\n");
}
