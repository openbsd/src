/*	$OpenBSD: server.c,v 1.14 2011/10/11 23:41:51 yasuoka Exp $	*/

/*
 * Copyright (c) 2000 Network Security Technologies, Inc. http://www.netsec.net
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <net/bpf.h>
#include <net/ppp_defs.h>
#include <errno.h>
#include <string.h>
#include <err.h>
#include <fcntl.h>
#include <unistd.h>
#include <sysexits.h>
#include <stdlib.h>
#include <md5.h>

#include "pppoe.h"

#define	COOKIE_LEN	4	/* bytes/cookie, must be <= 16 */
#define	COOKIE_MAX	16

static u_int8_t ac_cookie_key[8];

static void getpackets(int, u_int8_t *, struct ether_addr *);

static void recv_padi(int, struct ether_addr *,
    struct ether_header *, struct pppoe_header *, u_long, u_int8_t *);
static void recv_padr(int, u_int8_t *, struct ether_addr *,
    struct ether_header *, struct pppoe_header *, u_long, u_int8_t *);
static void recv_padt(int, struct ether_addr *,
    struct ether_header *, struct pppoe_header *, u_long, u_int8_t *);

static void send_pado(int, struct ether_addr *,
    struct ether_header *, struct pppoe_header *, u_long, u_int8_t *);
static void send_pads(int, u_int8_t *, struct ether_addr *,
    struct ether_header *, struct pppoe_header *, u_long, u_int8_t *);
static void key_gen(void);
static u_int8_t *key_make(u_int8_t *, int, u_int8_t *, int);
static int key_cmp(u_int8_t *, int, u_int8_t *, int, u_int8_t *, int);

void
server_mode(int bpffd, u_int8_t *sysname, u_int8_t *srvname,
    struct ether_addr *ea)
{
	struct pppoe_session *ses;
	fd_set *fdsp = NULL;
	int n, oldmax = 0;

	key_gen();

	while (1) {
		n = bpffd;
		LIST_FOREACH(ses, &session_master.sm_sessions, s_next) {
			if (ses->s_fd != -1 && ses->s_fd > n)
				n = ses->s_fd;
		}
		n++;

		if (n > oldmax) {
			if (fdsp != NULL)
				free(fdsp);
			fdsp = (fd_set *)calloc(howmany(n, NFDBITS),
			    sizeof(fd_mask));
			if (fdsp == NULL)
				break;
			oldmax = n;
		}
		bzero(fdsp, howmany(n, NFDBITS) * sizeof(fd_mask));

		FD_SET(bpffd, fdsp);
		LIST_FOREACH(ses, &session_master.sm_sessions, s_next) {
			if (ses->s_fd != -1)
				FD_SET(ses->s_fd, fdsp);
		}

		n = select(n, fdsp, NULL, NULL, NULL);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			err(EX_IOERR, "select");
			/* NOTREACHED */
		}
		if (n == 0)
			continue;
		if (FD_ISSET(bpffd, fdsp)) {
			n--;
			getpackets(bpffd, sysname, ea);
		}
		if (n == 0)
			continue;

		LIST_FOREACH(ses, &session_master.sm_sessions, s_next) {
			if (ses->s_fd != -1 && FD_ISSET(ses->s_fd, fdsp)) {
				if (ppp_to_bpf(bpffd, ses->s_fd, ea,
					&ses->s_ea, ses->s_id) < 0) {
					send_padt(bpffd, ea,
					    &ses->s_ea, ses->s_id);
					session_destroy(ses);
				}
				n--;
				if (n == 0)
					break;
			}
		}
	}

	if (fdsp != NULL)
		free(fdsp);
}

void
key_gen(void)
{
	u_int32_t r;

	r = arc4random();
	memcpy(ac_cookie_key, &r, sizeof(r));
	r = arc4random();
	memcpy(ac_cookie_key + sizeof(r), &r, sizeof(r));
}

u_int8_t *
key_make(u_int8_t *in1, int in1len, u_int8_t *in2, int in2len)
{
	u_int8_t *p;
	MD5_CTX ctx;

	p = (u_int8_t *)malloc(COOKIE_MAX);
	if (p == NULL)
		return (p);

	MD5Init(&ctx);
	MD5Update(&ctx, in1, in1len);
	MD5Update(&ctx, in2, in2len);
	MD5Final(p, &ctx);
	return (p);
}

int
key_cmp(u_int8_t *k, int klen, u_int8_t *in1, int in1len,
    u_int8_t *in2, int in2len)
{
	u_int8_t *p;
	int r;

	if (klen != COOKIE_LEN)
		return (-1);

	p = key_make(in1, in1len, in2, in2len);
	if (p == NULL)
		return (-1);

	r = memcmp(k, p, COOKIE_LEN);
	free(p);
	return (r);
}

static void
getpackets(int bpffd, u_int8_t *sysname, struct ether_addr *ea)
{
	static u_int8_t *pktbuf;
	u_int8_t *mpkt, *pkt, *epkt;
	struct ether_header eh;
	struct pppoe_header ph;
	struct bpf_hdr *bh;
	u_long len;
	int rlen;

	if (pktbuf == NULL) {
		pktbuf = (u_int8_t *)malloc(PPPOE_BPF_BUFSIZ);
		if (pktbuf == NULL)
			return;
	}

	rlen = read(bpffd, pktbuf, PPPOE_BPF_BUFSIZ);
	if (rlen < 0)
		return;

	pkt = pktbuf;
	epkt = pkt + rlen;
	while (pkt < epkt) {
		bh = (struct bpf_hdr *)pkt;
		len = bh->bh_caplen;
		mpkt = pkt + bh->bh_hdrlen;

		/* Pull out ethernet header */
		if (len < sizeof(struct ether_header))
			goto next;
		bcopy(mpkt, &eh, sizeof(struct ether_header));
		eh.ether_type = ntohs(eh.ether_type);
		len -= sizeof(struct ether_header);
		mpkt += sizeof(struct ether_header);

		/* Pull out pppoe header */
		if (len < sizeof(struct pppoe_header))
			goto next;
		bcopy(mpkt, &ph, sizeof(struct pppoe_header));
		mpkt += sizeof(struct pppoe_header);
		len -= sizeof(struct pppoe_header);
		ph.len = ntohs(ph.len);
		ph.sessionid = ntohs(ph.sessionid);

		if (PPPOE_VER(ph.vertype) != 1 ||
		    PPPOE_TYPE(ph.vertype) != 1)
			goto next;

		if (len > ph.len)
			len = ph.len;

		if (eh.ether_type == ETHERTYPE_PPPOEDISC) {
			/* Discovery Stage */
			switch (ph.code) {
			case PPPOE_CODE_PADI:
				recv_padi(bpffd, ea, &eh, &ph, len, mpkt);
				break;
			case PPPOE_CODE_PADR:
				recv_padr(bpffd, sysname, ea, &eh, &ph,
				    len, mpkt);
				break;
			case PPPOE_CODE_PADT:
				recv_padt(bpffd, ea, &eh, &ph, len, mpkt);
				break;
			default:
				break;
			}
		}
		else if (eh.ether_type == ETHERTYPE_PPPOE) {
			/* Session Stage */
			struct pppoe_session *s;

			s = session_find_eaid(
			    (struct ether_addr *)&eh.ether_shost[0],
			    ph.sessionid);
			if (s != NULL && bpf_to_ppp(s->s_fd, len, mpkt) <= 0)
				session_destroy(s);
		}
next:
		pkt += BPF_WORDALIGN(bh->bh_hdrlen + bh->bh_caplen);
	}
}

static void
recv_padi(int bpffd, struct ether_addr *ea, struct ether_header *eh,
    struct pppoe_header *ph, u_long pktlen, u_int8_t *pktbuf)
{
	struct tag_list tl;

	if (ph->sessionid != 0)
		return;
	if (bcmp(&eh->ether_dhost[0], etherbroadcastaddr, ETHER_ADDR_LEN))
		return;

	tag_init(&tl);
	if (tag_pkt(&tl, pktlen, pktbuf) < 0)
		goto out;

	if (tag_lookup(&tl, PPPOE_TAG_SERVICE_NAME, 1) != NULL)
		goto out;

	send_pado(bpffd, ea, eh, ph, pktlen, pktbuf);

out:
	tag_destroy(&tl);
}

static void
send_pado(int bpffd, struct ether_addr *ea, struct ether_header *eh,
    struct pppoe_header *ph, u_long pktlen, u_int8_t *pktbuf)
{
	struct pppoe_tag ktag, htag;
	u_int8_t hn[MAXHOSTNAMELEN];
	u_int8_t *k = NULL;
	struct iovec v[7];
	int idx = 0;

	memcpy(&eh->ether_dhost[0], &eh->ether_shost[0], ETHER_ADDR_LEN);
	memcpy(&eh->ether_shost[0], ea, ETHER_ADDR_LEN);
	eh->ether_type = htons(eh->ether_type);
	v[idx].iov_base = eh; v[idx].iov_len = sizeof(*eh); idx++;

	ph->code = PPPOE_CODE_PADO;
	v[idx].iov_base = ph; v[idx].iov_len = sizeof(*ph); idx++;

	v[idx].iov_base = pktbuf; v[idx].iov_len = pktlen; idx++;

	if (gethostname((char *)hn, sizeof(hn)) < 0)
		return;
	htag.len = strlen((char *)hn);
	htag.type = htons(PPPOE_TAG_AC_NAME);
	htag.val = hn;
	v[idx].iov_base = &htag;
	v[idx].iov_len = sizeof(htag.len) + sizeof(htag.type);
	idx++;
	v[idx].iov_base = hn; v[idx].iov_len = htag.len; idx++;
	ph->len += sizeof(htag.len) + sizeof(htag.type) + htag.len;
	htag.len = htons(htag.len);

	k = key_make(&eh->ether_dhost[0], ETHER_ADDR_LEN, ac_cookie_key,
	    sizeof(ac_cookie_key));
	if (k == NULL)
		return;
	ktag.type = htons(PPPOE_TAG_AC_COOKIE);
	ktag.len = COOKIE_LEN;
	ktag.val = k;
	v[idx].iov_base = &ktag;
	v[idx].iov_len = sizeof(ktag.len) + sizeof(ktag.type);
	idx++;
	v[idx].iov_base = k; v[idx].iov_len = COOKIE_LEN; idx++;
	ph->len += sizeof(ktag.len) + sizeof(ktag.type) + COOKIE_LEN;
	ktag.len = htons(COOKIE_LEN);

	ph->len = htons(ph->len);

	writev(bpffd, v, idx);

	if (k)
		free(k);
}

static void
recv_padr(int bpffd, u_int8_t *sysname, struct ether_addr *ea,
    struct ether_header *eh, struct pppoe_header *ph,
    u_long pktlen, u_int8_t *pktbuf)
{
	struct tag_list tl;
	struct tag_node *n;

	if (ph->sessionid != 0)
		return;

	tag_init(&tl);
	if (tag_pkt(&tl, pktlen, pktbuf) < 0)
		return;

	n = tag_lookup(&tl, PPPOE_TAG_AC_COOKIE, 0);
	if (n == NULL)
		return;
	if (key_cmp(n->val, n->len, &eh->ether_shost[0], ETHER_ADDR_LEN,
	    ac_cookie_key, sizeof(ac_cookie_key)))
		return;

	send_pads(bpffd, sysname, ea, eh, ph, pktlen, pktbuf);

	tag_destroy(&tl);
}

static void
send_pads(int bpffd, u_int8_t *sysname, struct ether_addr *ea,
    struct ether_header *eh, struct pppoe_header *ph,
    u_long pktlen, u_int8_t *pktbuf)
{
	u_int8_t hn[MAXHOSTNAMELEN];
	struct iovec v[16];
	struct pppoe_session *s;
	struct pppoe_tag htag;
	int idx = 0;

	s = session_new((struct ether_addr *)&eh->ether_shost[0]);
	if (s == NULL)
		return;

	memcpy(&eh->ether_dhost[0], &eh->ether_shost[0], ETHER_ADDR_LEN);
	memcpy(&eh->ether_shost[0], ea, ETHER_ADDR_LEN);
	eh->ether_type = htons(eh->ether_type);
	v[idx].iov_base = eh; v[idx].iov_len = sizeof(*eh); idx++;

	ph->code = PPPOE_CODE_PADS;
	ph->sessionid = htons(s->s_id);
	if (gethostname((char *)hn, sizeof(hn)) < 0)
		return;
	v[idx].iov_base = ph; v[idx].iov_len = sizeof(*ph); idx++;

	v[idx].iov_base = pktbuf; v[idx].iov_len = pktlen; idx++;

	htag.len = strlen((char *)hn);
	htag.type = htons(PPPOE_TAG_AC_NAME);
	htag.val = hn;
	v[idx].iov_base = &htag;
	v[idx].iov_len = sizeof(htag.len) + sizeof(htag.type);
	idx++;
	v[idx].iov_base = hn; v[idx].iov_len = htag.len; idx++;
	ph->len += sizeof(htag.len) + sizeof(htag.type) + htag.len;
	htag.len = htons(htag.len);

	ph->len = htons(ph->len);

	writev(bpffd, v, idx);

	s->s_fd = runppp(bpffd, sysname);
	if (s->s_fd < 0) {
		/* XXX Send PADT with Generic-Error */
		s->s_fd = -1;
	}
}

static void
recv_padt(int bpffd, struct ether_addr *ea, struct ether_header *eh,
    struct pppoe_header *ph, u_long pktlen, u_int8_t *pktbuf)
{
	struct pppoe_session *s;
	struct tag_list tl;

	tag_init(&tl);
	if (tag_pkt(&tl, pktlen, pktbuf) < 0)
		goto out;

	s = session_find_eaid((struct ether_addr *)&eh->ether_shost[0],
	    ph->sessionid);
	if (s == NULL)
		goto out;
	session_destroy(s);

out:
	tag_destroy(&tl);
}
