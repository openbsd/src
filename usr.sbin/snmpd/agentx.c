/*      $OpenBSD: agentx.c,v 1.13 2018/06/17 18:19:59 rob Exp $    */
/*
 * Copyright (c) 2013,2014 Bret Stephen Lambert <blambert@openbsd.org>
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/un.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "snmp.h"

int	snmp_agentx_octetstring(struct agentx_pdu *, char *, int);
int	snmp_agentx_buffercheck(struct agentx_pdu *, size_t);
int	snmp_agentx_oid(struct agentx_pdu *, struct snmp_oid *);
int	snmp_agentx_buffer_consume(struct agentx_pdu *, u_int);
int	snmp_agentx_int(struct agentx_pdu *, uint32_t *);
int	snmp_agentx_int64(struct agentx_pdu *, uint64_t *);
int	snmp_agentx_do_read_raw(struct agentx_pdu *, void *, int, int);
void	snmp_agentx_update_ids(struct agentx_handle *, struct agentx_pdu *);
struct agentx_pdu *
	agentx_find_inflight(struct agentx_handle *, uint32_t, uint32_t);
int	snmp_agentx_do_read_oid(struct agentx_pdu *, struct snmp_oid *, int *);

#ifdef DEBUG
static void	snmp_agentx_dump_hdr(struct agentx_hdr *);
#endif

#define PDU_BUFLEN 256

/* snmpTrapOid.0 */
struct snmp_oid trapoid_0 = {
	.o_id =	{ 1, 3, 6, 1, 6, 3, 1, 1, 4, 1, 0 },
	.o_n = 11
};

/*
 * AgentX handle allocation and management routines.
 */

struct agentx_handle *
snmp_agentx_alloc(int s)
{
	struct agentx_handle *h;

	if ((h = calloc(1, sizeof(*h))) == NULL)
		return (NULL);

	h->fd = s;
	h->timeout = AGENTX_DEFAULT_TIMEOUT;

	TAILQ_INIT(&h->w);
	TAILQ_INIT(&h->inflight);

	return (h);
}

/*
 * Synchronous open of unix socket path.
 */
struct agentx_handle *
snmp_agentx_open(const char *path, char *descr, struct snmp_oid *oid)
{
	struct sockaddr_un	 sun;
	struct agentx_handle	*h;
	int s;

	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		return (NULL);

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	if (strlcpy(sun.sun_path, path, sizeof(sun.sun_path)) >=
	    sizeof(sun.sun_path))
		goto fail;

	if (connect(s, (struct sockaddr *)&sun, sizeof(sun)) == -1)
		goto fail;

	if ((h = snmp_agentx_fdopen(s, descr, oid)) == NULL)
		goto fail;

	return (h);
 fail:
	close(s);
	return (NULL);
}

/*
 * Synchronous AgentX open operation over previously-opened socket.
 */
struct agentx_handle *
snmp_agentx_fdopen(int s, char *descr, struct snmp_oid *oid)
{
	struct agentx_handle	*h;
	struct agentx_pdu	*pdu = NULL;

	if ((h = snmp_agentx_alloc(s)) == NULL)
		return (NULL);

	if ((pdu = snmp_agentx_open_pdu(h, descr, oid)) == NULL ||
	    (pdu = snmp_agentx_request(h, pdu)) == NULL ||
	    snmp_agentx_open_response(h, pdu) == -1) {
		if (pdu)
			snmp_agentx_pdu_free(pdu);
		snmp_agentx_free(h);
		return (NULL);
	}

	return (h);
}

/*
 * Synchronous close of agentx handle.
 */
int
snmp_agentx_close(struct agentx_handle *h, uint8_t reason)
{
	struct agentx_pdu			*pdu;
	int					 error = 0;

	if ((pdu = snmp_agentx_close_pdu(h, reason)) == NULL)
		return (-1);
	if ((pdu = snmp_agentx_request(h, pdu)) == NULL)
		return (-1);
	if (snmp_agentx_response(h, pdu) == -1)
		error = -1;

	snmp_agentx_pdu_free(pdu);

	return (error);
}

void
snmp_agentx_free(struct agentx_handle *h)
{
	struct agentx_pdu *pdu;

	if (h->fd != -1)
		close(h->fd);

	while ((pdu = TAILQ_FIRST(&h->w))) {
		TAILQ_REMOVE(&h->w, pdu, entry);
		snmp_agentx_pdu_free(pdu);
	}
	while ((pdu = TAILQ_FIRST(&h->inflight))) {
		TAILQ_REMOVE(&h->w, pdu, entry);
		snmp_agentx_pdu_free(pdu);
	}
	if (h->r)
		snmp_agentx_pdu_free(h->r);

	free(h);
}

/*
 * AgentX pdu allocation routines.
 */

/*
 * Allocate an AgentX PDU.
 */
struct agentx_pdu *
snmp_agentx_pdu_alloc(void)
{
	struct agentx_pdu	*pdu;

	if ((pdu = calloc(1, sizeof(*pdu))) == NULL)
		return (NULL);
	if ((pdu->buffer = calloc(PDU_BUFLEN, sizeof(uint8_t))) == NULL) {
		free(pdu);
		return (NULL);
	}

	pdu->buflen = PDU_BUFLEN;

	bzero(pdu->buffer, pdu->buflen);
	pdu->ptr = pdu->buffer + sizeof(struct agentx_hdr);
	pdu->ioptr = pdu->buffer;
	pdu->hdr = (struct agentx_hdr *)pdu->buffer;
	pdu->hdr->version = AGENTX_VERSION;
	pdu->hdr->flags = AGENTX_LOCAL_BYTE_ORDER_FLAG;
	pdu->hdr->reserved = 0;
	pdu->hdr->length = 0;
	pdu->datalen = sizeof(struct agentx_hdr);

	return (pdu);
}

/*
 * Read the response PDU for a generic operation.
 */
int
snmp_agentx_response(struct agentx_handle *h, struct agentx_pdu *pdu)
{
	struct agentx_response_data resp;

	if (snmp_agentx_read_raw(pdu, &resp, sizeof(resp)) == -1)
		return (-1);

	if (!snmp_agentx_byteorder_native(pdu->hdr)) {
		resp.error = snmp_agentx_int16_byteswap(resp.error);
		resp.index = snmp_agentx_int16_byteswap(resp.index);
	}

	h->error = resp.error;
	if (resp.error != AGENTX_ERR_NONE)
		return (-1);

	return (0);
}

/*
 * Read the response PDU for an open operation.
 */
int
snmp_agentx_open_response(struct agentx_handle *h, struct agentx_pdu *pdu)
{

	if (snmp_agentx_response(h, pdu) == -1)
		return (-1);

	h->sessionid = pdu->hdr->sessionid;
	return (0);
}

void
snmp_agentx_pdu_free(struct agentx_pdu *pdu)
{
	free(pdu->buffer);
	free(pdu->request);
	free(pdu);
}

int
snmp_agentx_buffer_consume(struct agentx_pdu *b, u_int len)
{
	int padding;

	padding = ((len + 3) & ~0x03) - len;

	if (b->datalen < (len + padding))
		return (-1);

	b->datalen -= len + padding;
	b->ptr += len + padding;

	return (0);
}

/*
 * Send an AgentX PDU. Flushes any already-enqueued PDUs.
 */
int
snmp_agentx_send(struct agentx_handle *h, struct agentx_pdu *pdu)
{
	ssize_t n;

	/* set the appropriate IDs in the protocol header */
	if (pdu != NULL &&
	    (pdu->datalen == pdu->hdr->length + sizeof(struct agentx_hdr))) {
		pdu->hdr->sessionid = h->sessionid;

		if (pdu->hdr->type != AGENTX_RESPONSE) {
			++h->transactid;
			++h->packetid;
		}

		pdu->hdr->transactid = h->transactid;
		pdu->hdr->packetid = h->packetid;
		TAILQ_INSERT_TAIL(&h->w, pdu, entry);
	}

 again:
	if ((pdu = TAILQ_FIRST(&h->w)) == NULL)
		return (0);

	if ((n = send(h->fd, pdu->ioptr, pdu->datalen, 0)) == -1)
		return (-1);

	pdu->ioptr += n;
	pdu->datalen -= n;

	if (pdu->datalen > 0) {
		errno = EAGAIN;
		return (-1);
	}

#ifdef DEBUG
	snmp_agentx_dump_hdr(pdu->hdr);
#endif

	TAILQ_REMOVE(&h->w, pdu, entry);
	TAILQ_INSERT_TAIL(&h->inflight, pdu, entry);

	goto again;
}

/*
 * Attempt to read a single AgentX PDU.
 */
struct agentx_pdu *
snmp_agentx_recv(struct agentx_handle *h)
{
	struct agentx_pdu *pdu, *match;
	ssize_t n;

	h->error = AGENTX_ERR_NONE;

	if (h->r == NULL) {
		if ((h->r = snmp_agentx_pdu_alloc()) == NULL)
			return (NULL);
		h->r->datalen = 0;	/* XXX force this for receive buffers */
	}
	pdu = h->r;

	if (snmp_agentx_buffercheck(pdu, sizeof(struct agentx_hdr)) == -1)
		return (NULL);

	/* read header */
	if (pdu->datalen < sizeof(struct agentx_hdr)) {
		n = recv(h->fd, pdu->ioptr, sizeof(struct agentx_hdr), 0);

		if (n == 0 || n == -1)
			return (NULL);

		pdu->datalen += n;
		pdu->ioptr += n;

		if (pdu->datalen < sizeof(struct agentx_hdr)) {
			errno = EAGAIN;
			return (NULL);
		}

		if (pdu->hdr->version != AGENTX_VERSION) {
			h->error = AGENTX_ERR_PARSE_ERROR;
			return (NULL);
		}

		if (snmp_agentx_buffercheck(pdu, pdu->hdr->length) == -1)
			return (NULL);
	}

	/* read body */
	if (pdu->hdr->length > 0) {
		n = recv(h->fd, pdu->ioptr, pdu->hdr->length, 0);

		if (n == 0 || n == -1)
			return (NULL);

		pdu->datalen += n;
		pdu->ioptr += n;
	}

	if (pdu->datalen < pdu->hdr->length + sizeof(struct agentx_hdr)) {
		errno = EAGAIN;
		return (NULL);
	}

	if (pdu->hdr->version != AGENTX_VERSION) {
		h->error = AGENTX_ERR_PARSE_ERROR;
		goto fail;
	}

	/* If this is an open on a new connection, fix it up */
	if (pdu->hdr->type == AGENTX_OPEN && h->sessionid == 0) {
		pdu->hdr->sessionid = 0;		/* ignored, per RFC */
		h->transactid = pdu->hdr->transactid;
		h->packetid = pdu->hdr->packetid;
	}

	if (pdu->hdr->type == AGENTX_RESPONSE) {

		match = agentx_find_inflight(h, pdu->hdr->transactid,
		    pdu->hdr->packetid);
		if (match == NULL) {
			errno = ESRCH;		/* XXX */
			goto fail;
		}

		TAILQ_REMOVE(&h->inflight, match, entry);
		pdu->request = match;
		h->r = NULL;

	} else {
		if (pdu->hdr->sessionid != h->sessionid) {
			h->error = AGENTX_ERR_NOT_OPEN;
			goto fail;
		}

		snmp_agentx_update_ids(h, pdu);              /* XXX */

		if (pdu->datalen != pdu->hdr->length + sizeof(*pdu->hdr)) {
			h->error = AGENTX_ERR_PARSE_ERROR;
			goto fail;
		}

		if (pdu->hdr->flags & AGENTX_NON_DEFAULT_CONTEXT) {
			pdu->context = snmp_agentx_read_octetstr(pdu,
			    &pdu->contextlen);
			if (pdu->context == NULL) {
				h->error = AGENTX_ERR_PARSE_ERROR;
				goto fail;
			}
		}
	}

#ifdef DEBUG
	snmp_agentx_dump_hdr(pdu->hdr);
#endif
	h->r = NULL;

	return (pdu);

 fail:
#ifdef DEBUG
	snmp_agentx_dump_hdr(pdu->hdr);
#endif
	snmp_agentx_pdu_free(pdu);
	h->r = NULL;

	return (NULL);
}

/*
 * Synchonous request and receipt of response.
 */
struct agentx_pdu *
snmp_agentx_request(struct agentx_handle *h, struct agentx_pdu *pdu)
{

	if (snmp_agentx_send(h, pdu) == -1) {
		if (errno != EAGAIN)
			return (NULL);
	}
	while (snmp_agentx_send(h, NULL) == -1) {
		if (errno != EAGAIN)
			return (NULL);
	}
	while ((pdu = snmp_agentx_recv(h)) == NULL) {
		if (errno != EAGAIN)
			return (NULL);
	}
	h->error = AGENTX_ERR_NONE;

	return (pdu);
}

struct agentx_pdu *
agentx_find_inflight(struct agentx_handle *h, uint32_t tid, uint32_t pid)
{
	struct agentx_pdu *pdu;

	TAILQ_FOREACH(pdu, &h->inflight, entry)
		if (pdu->hdr->transactid == tid && pdu->hdr->packetid == pid)
			break;
	return (pdu);
}

int
snmp_agentx_buffercheck(struct agentx_pdu *pdu, size_t len)
{
	uint8_t *newptr;
	size_t newlen;

	if (pdu->buflen - pdu->datalen >= len)
		return (0);

	newlen = pdu->buflen + len;
	if (newlen < pdu->buflen || newlen < len)
		return (-1);

	if ((newptr = realloc(pdu->buffer, newlen)) == NULL)
		return (-1);

	pdu->buflen = newlen;
	pdu->ioptr = &newptr[pdu->ioptr - pdu->buffer];
	pdu->buffer = newptr;
	pdu->hdr = (struct agentx_hdr *)pdu->buffer;
	pdu->ptr = &pdu->buffer[pdu->datalen];

	return (0);
}

/*
 * Utility routines for initializing common AgentX PDUs.
 */

struct agentx_pdu *
snmp_agentx_open_pdu(struct agentx_handle *h, char *descr,
    struct snmp_oid *oid)
{
	struct agentx_open_timeout to;
	struct snmp_oid nulloid;
	struct agentx_pdu *pdu;

	if ((pdu = snmp_agentx_pdu_alloc()) == NULL)
		return (NULL);

	pdu->hdr->type = AGENTX_OPEN;

	if (oid == NULL) {
		bzero(&nulloid, sizeof(nulloid));
		oid = &nulloid;
	}

	bzero(&to, sizeof(to));
	to.timeout = AGENTX_DEFAULT_TIMEOUT;

	if (snmp_agentx_raw(pdu, &to, sizeof(to)) == -1 ||
	    snmp_agentx_oid(pdu, oid) == -1 ||
	    snmp_agentx_octetstring(pdu, descr, strlen(descr)) == -1)
		goto fail;

	return (pdu);
 fail:
	snmp_agentx_pdu_free(pdu);
	return (NULL);
}

struct agentx_pdu *
snmp_agentx_close_pdu(struct agentx_handle *h, uint8_t reason)
{
	struct agentx_close_request_data	 req;
	struct agentx_pdu			*pdu;

	if ((pdu = snmp_agentx_pdu_alloc()) == NULL)
		return (NULL);
	pdu->hdr->type = AGENTX_CLOSE;

	bzero(&req, sizeof(req));
	req.reason = reason;

	if (snmp_agentx_raw(pdu, &req, sizeof(req)) == -1) {
		snmp_agentx_pdu_free(pdu);
		return (NULL);
	}

	return (pdu);
}

struct agentx_pdu *
snmp_agentx_notify_pdu(struct snmp_oid *oid)
{
	struct agentx_pdu	*pdu;

	if ((pdu = snmp_agentx_pdu_alloc()) == NULL)
		return (NULL);
	pdu->hdr->type = AGENTX_NOTIFY;

	if (snmp_agentx_varbind(pdu, &trapoid_0,
	    AGENTX_OBJECT_IDENTIFIER, oid, sizeof(*oid)) == -1) {
		snmp_agentx_pdu_free(pdu);
		return (NULL);
	}

	return (pdu);
}

struct agentx_pdu *
snmp_agentx_response_pdu(int uptime, int error, int idx)
{
	struct agentx_response_data	 resp;
	struct agentx_pdu		*pdu;

	if ((pdu = snmp_agentx_pdu_alloc()) == NULL)
		return (NULL);
	pdu->hdr->type = AGENTX_RESPONSE;

	resp.sysuptime = uptime;
	resp.error = error;
	resp.index = idx;

	if (snmp_agentx_raw(pdu, &resp, sizeof(resp)) == -1) {
		snmp_agentx_pdu_free(pdu);
		return (NULL);
	}

	return (pdu);
}

struct agentx_pdu *
snmp_agentx_ping_pdu(void)
{
	struct agentx_pdu *pdu;

	if ((pdu = snmp_agentx_pdu_alloc()) == NULL)
		return (NULL);
	pdu->hdr->version = AGENTX_VERSION;
	pdu->hdr->type = AGENTX_PING;

	return (pdu);
}

struct agentx_pdu *
snmp_agentx_register_pdu(struct snmp_oid *oid, int timeout, int range_index,
    int range_bound)
{
	struct agentx_register_hdr	 rhdr;
	struct agentx_pdu		*pdu;

	if ((pdu = snmp_agentx_pdu_alloc()) == NULL)
		return (NULL);

	pdu->hdr->version = AGENTX_VERSION;
	pdu->hdr->type = AGENTX_REGISTER;

	rhdr.timeout = timeout;
	rhdr.priority = AGENTX_REGISTER_PRIO_DEFAULT;
	rhdr.subrange = range_index;
	rhdr.reserved = 0;

	if (snmp_agentx_raw(pdu, &rhdr, sizeof(rhdr)) == -1 ||
	    snmp_agentx_oid(pdu, oid) == -1 ||
	    (range_index && snmp_agentx_int(pdu, &range_bound) == -1)) {
		snmp_agentx_pdu_free(pdu);
		return (NULL);
	}

	return (pdu);
}

struct agentx_pdu *
snmp_agentx_unregister_pdu(struct snmp_oid *oid, int range_index,
    int range_bound)
{
	struct agentx_unregister_hdr	 uhdr;
	struct agentx_pdu		*pdu;

	if ((pdu = snmp_agentx_pdu_alloc()) == NULL)
		return (NULL);

	pdu->hdr->version = AGENTX_VERSION;
	pdu->hdr->type = AGENTX_UNREGISTER;

	uhdr.reserved1 = 0;
	uhdr.priority = AGENTX_REGISTER_PRIO_DEFAULT;
	uhdr.subrange = range_index;
	uhdr.reserved2 = 0;

	if (snmp_agentx_raw(pdu, &uhdr, sizeof(uhdr)) == -1 ||
	    snmp_agentx_oid(pdu, oid) == -1 ||
	    snmp_agentx_oid(pdu, oid) == -1 ||
	    (range_index && snmp_agentx_int(pdu, &range_bound) == -1)) {
		snmp_agentx_pdu_free(pdu);
		return (NULL);
	}

	return (pdu);
}

struct agentx_pdu *
snmp_agentx_get_pdu(struct snmp_oid oid[], int noid)
{
	struct snmp_oid		 nulloid;
	struct agentx_pdu	*pdu;
	int			 i;

	if ((pdu = snmp_agentx_pdu_alloc()) == NULL)
		return (NULL);

	pdu->hdr->version = AGENTX_VERSION;
	pdu->hdr->type = AGENTX_GET;

	bzero(&nulloid, sizeof(nulloid));

	for (i = 0; i < noid; i++) {
		if (snmp_agentx_oid(pdu, &oid[i]) == -1 ||
		    snmp_agentx_oid(pdu, &nulloid) == -1) {
			snmp_agentx_pdu_free(pdu);
			return (NULL);
		}
	}

	return (pdu);
}

struct agentx_pdu *
snmp_agentx_getnext_pdu(struct snmp_oid oid[], int noid)
{
	struct snmp_oid		 nulloid;
	struct agentx_pdu	*pdu;
	int			 i;

	if ((pdu = snmp_agentx_pdu_alloc()) == NULL)
		return (NULL);

	pdu->hdr->version = AGENTX_VERSION;
	pdu->hdr->type = AGENTX_GET_NEXT;

	bzero(&nulloid, sizeof(nulloid));

	for (i = 0; i < noid; i++) {
		if (snmp_agentx_oid(pdu, &oid[i]) == -1 ||
		    snmp_agentx_oid(pdu, &nulloid) == -1) {
			snmp_agentx_pdu_free(pdu);
			return (NULL);
		}
	}

	return (pdu);
}

/*
 * AgentX PDU write routines.
 */

int
snmp_agentx_raw(struct agentx_pdu *pdu, void *data, int len)
{

	if (snmp_agentx_buffercheck(pdu, len) == -1)
		return (-1);

	memcpy(pdu->ptr, data, len);

	pdu->hdr->length += len;
	pdu->ptr += len;
	pdu->datalen += len;

	return (0);
}

int
snmp_agentx_int(struct agentx_pdu *pdu, uint32_t *i)
{
	return (snmp_agentx_raw(pdu, i, sizeof(*i)));
}

int
snmp_agentx_int64(struct agentx_pdu *pdu, uint64_t *i)
{
	return (snmp_agentx_raw(pdu, i, sizeof(*i)));
}

int
snmp_agentx_octetstring(struct agentx_pdu *pdu, char *str, int len)
{
	static uint8_t pad[4] = { 0, 0, 0, 0 };
	int padding;
	uint32_t l;

	padding = ((len + 3) & ~0x03) - len;

	l = len;
	if (snmp_agentx_int(pdu, &l) == -1 ||
	    snmp_agentx_raw(pdu, str, len) == -1 ||
	    snmp_agentx_raw(pdu, pad, padding) == -1)
		return (-1);

	return (0);
}

int
snmp_agentx_oid(struct agentx_pdu *pdu, struct snmp_oid *oid)
{
	struct agentx_oid_hdr ohdr;
	u_int i, prefix;

	i = prefix = 0;

	if (oid->o_id[0] == 1 && oid->o_id[1] == 3 &&
	    oid->o_id[2] == 6 && oid->o_id[3] == 1 &&
	    oid->o_id[4] < 256) {
		prefix = oid->o_id[4];
		i = 5;
	}

	if (prefix)
		ohdr.n_subid = oid->o_n - 5;
	else
		ohdr.n_subid = oid->o_n;
	ohdr.prefix = prefix;
	ohdr.include = 0;
	ohdr.reserved = 0;

	if (snmp_agentx_raw(pdu, &ohdr, sizeof(ohdr)) == -1)
		return (-1);

	for (; i < oid->o_n; i++)
		if (snmp_agentx_int(pdu, &oid->o_id[i]) == -1)
			return (-1);

	return (0);
}

int
snmp_agentx_varbind(struct agentx_pdu *pdu, struct snmp_oid *oid, int type,
    void *data, int len)
{
	struct agentx_varbind_hdr vbhdr;

	vbhdr.type = type;
	vbhdr.reserved = 0;
	if (snmp_agentx_raw(pdu, &vbhdr, sizeof(vbhdr)) == -1)
		return (-1);

	if (snmp_agentx_oid(pdu, oid) == -1)
		return (-1);

	switch (type) {

	case AGENTX_NO_SUCH_OBJECT:
	case AGENTX_NO_SUCH_INSTANCE:
	case AGENTX_END_OF_MIB_VIEW:
	case AGENTX_NULL:
		/* no data follows the OID */
		return (0);

	case AGENTX_IP_ADDRESS:
	case AGENTX_OPAQUE:
	case AGENTX_OCTET_STRING:
		return (snmp_agentx_octetstring(pdu, data, len));

	case AGENTX_OBJECT_IDENTIFIER:
		return (snmp_agentx_oid(pdu, (struct snmp_oid *)data));

	case AGENTX_INTEGER:
	case AGENTX_COUNTER32:
	case AGENTX_GAUGE32:
	case AGENTX_TIME_TICKS:
		return (snmp_agentx_int(pdu, (uint32_t *)data));

	case AGENTX_COUNTER64:
		return (snmp_agentx_int64(pdu, (uint64_t *)data));

	default:
		return (-1);
	}
	/* NOTREACHED */
}

/*
 * AgentX PDU read routines.
 */

int
snmp_agentx_read_vbhdr(struct agentx_pdu *pdu,
    struct agentx_varbind_hdr *vbhdr)
{
	if (snmp_agentx_read_raw(pdu, vbhdr, sizeof(*vbhdr)) == -1)
		return (-1);
	if (!snmp_agentx_byteorder_native(pdu->hdr))
		vbhdr->type = snmp_agentx_int16_byteswap(vbhdr->type);
	return (0);
}

int
snmp_agentx_copy_raw(struct agentx_pdu *pdu, void *v, int len)
{
	return (snmp_agentx_do_read_raw(pdu, v, len, 0));
}

int
snmp_agentx_read_raw(struct agentx_pdu *pdu, void *v, int len)
{
	return (snmp_agentx_do_read_raw(pdu, v, len, 1));
}

int
snmp_agentx_do_read_raw(struct agentx_pdu *pdu, void *v, int len, int consume)
{
	void *ptr = pdu->ptr;

	if (consume)
		if (snmp_agentx_buffer_consume(pdu, len) == -1)
			return (-1);

	memcpy(v, ptr, len);

	return (0);
}

int
snmp_agentx_read_int(struct agentx_pdu *pdu, uint32_t *i)
{
	if (snmp_agentx_read_raw(pdu, i, sizeof(*i)) == -1)
		return (-1);
	if (!snmp_agentx_byteorder_native(pdu->hdr))
		*i = snmp_agentx_int_byteswap(*i);
	return (0);
}

int
snmp_agentx_read_int64(struct agentx_pdu *pdu, uint64_t *i)
{
	if (snmp_agentx_read_raw(pdu, i, sizeof(*i)) == -1)
		return (-1);
	if (!snmp_agentx_byteorder_native(pdu->hdr))
		*i = snmp_agentx_int64_byteswap(*i);
	return (0);
}

int
snmp_agentx_read_oid(struct agentx_pdu *pdu, struct snmp_oid *oid)
{
	int dummy;

	return (snmp_agentx_do_read_oid(pdu, oid, &dummy));
}

int
snmp_agentx_do_read_oid(struct agentx_pdu *pdu, struct snmp_oid *oid,
    int *include)
{
	struct agentx_oid_hdr ohdr;
	int i = 0;

	if (snmp_agentx_read_raw(pdu, &ohdr, sizeof(ohdr)) == -1)
		return (-1);

	bzero(oid, sizeof(*oid));

	if (ohdr.prefix != 0) {
		oid->o_id[0] = 1;
		oid->o_id[1] = 3;
		oid->o_id[2] = 6;
		oid->o_id[3] = 1;
		oid->o_id[4] = ohdr.prefix;
		i = 5;
	}

	while (ohdr.n_subid--)
		if (snmp_agentx_read_int(pdu, &oid->o_id[i++]) == -1)
			return (-1);

	oid->o_n = i;
	*include = ohdr.include;

	return (0);
}

int
snmp_agentx_read_searchrange(struct agentx_pdu *pdu,
    struct agentx_search_range *sr)
{
	if (snmp_agentx_do_read_oid(pdu, &sr->start, &sr->include) == -1 ||
	    snmp_agentx_read_oid(pdu, &sr->end) == -1)
		return (-1);

	return (0);
}

char *
snmp_agentx_read_octetstr(struct agentx_pdu *pdu, int *len)
{
	char *str;
	uint32_t l;

	if (snmp_agentx_read_int(pdu, &l) == -1)
		return (NULL);

	if ((str = malloc(l)) == NULL)
		return (NULL);

	if (snmp_agentx_read_raw(pdu, str, l) == -1) {
		free(str);
		return (NULL);
	}
	*len = l;

	return (str);
}

/*
 * Synchronous AgentX calls.
 */

int
snmp_agentx_ping(struct agentx_handle *h)
{
	struct agentx_pdu	*pdu;
	int			 error = 0;

	if ((pdu = snmp_agentx_ping_pdu()) == NULL)
		return (-1);

	/* Attaches the pdu to the handle; will be released later */
	if ((pdu = snmp_agentx_request(h, pdu)) == NULL)
		return (-1);

	if (snmp_agentx_response(h, pdu) == -1)
		error = -1;
	snmp_agentx_pdu_free(pdu);

	return (error);
}

/*
 * Internal utility functions.
 */

void
snmp_agentx_update_ids(struct agentx_handle *h, struct agentx_pdu *pdu)
{
	/* XXX -- update to reflect the new queueing semantics */
	h->transactid = pdu->hdr->transactid;
	h->packetid = pdu->hdr->packetid;
}

char *
snmp_oid2string(struct snmp_oid *o, char *buf, size_t len)
{
	char		 str[256];
	size_t		 i;

	bzero(buf, len);

	for (i = 0; i < o->o_n; i++) {
		snprintf(str, sizeof(str), "%u", o->o_id[i]);
		strlcat(buf, str, len);
		if (i < (o->o_n - 1))
			strlcat(buf, ".", len);
	}

	return (buf);
}

int
snmp_oid_cmp(struct snmp_oid *a, struct snmp_oid *b)
{
	size_t		i;

	for (i = 0; i < SNMP_MAX_OID_LEN; i++) {
		if (a->o_id[i] != 0) {
			if (a->o_id[i] == b->o_id[i])
				continue;
			else if (a->o_id[i] < b->o_id[i]) {
				/* b is a successor of a */
				return (1);
			} else {
				/* b is a predecessor of a */
				return (-1);
			}
		} else if (b->o_id[i] != 0) {
			/* b is larger, but a child of a */
			return (2);
		} else
			break;
	}

	/* b and a are identical */
	return (0);
}

void
snmp_oid_increment(struct snmp_oid *o)
{
	u_int		i;

	for (i = o->o_n; i > 0; i--) {
		o->o_id[i - 1]++;
		if (o->o_id[i - 1] != 0)
			break;
	}
}

char *
snmp_agentx_type2name(int type)
{
	static char *names[] = {
		"AGENTX_OPEN",
		"AGENTX_CLOSE",
		"AGENTX_REGISTER",
		"AGENTX_UNREGISTER",
		"AGENTX_GET",
		"AGENTX_GET_NEXT",
		"AGENTX_GET_BULK",
		"AGENTX_TEST_SET",
		"AGENTX_COMMIT_SET",
		"AGENTX_UNDO_SET",
		"AGENTX_CLEANUP_SET",
		"AGENTX_NOTIFY",
		"AGENTX_PING",
		"AGENTX_INDEX_ALLOCATE",
		"AGENTX_INDEX_DEALLOCATE",
		"AGENTX_ADD_AGENT_CAPS",
		"AGENTX_REMOVE_AGENT_CAPS",
		"AGENTX_RESPONSE"
	};

	if (type > 18)
		return ("unknown");

	return (names[type - 1]);
}

#ifdef DEBUG
static void
snmp_agentx_dump_hdr(struct agentx_hdr *hdr)
{
	if (hdr == NULL) {
		printf("NULL\n");
		return;
	}

	fprintf(stderr,
	    "agentx: version %d type %s flags %d reserved %d"
	    " sessionid %d transactid %d packetid %d length %d",
	    hdr->version, snmp_agentx_type2name(hdr->type), hdr->flags,
	    hdr->reserved, hdr->sessionid, hdr->transactid,
	    hdr->packetid, hdr->length);

	if (hdr->type == AGENTX_RESPONSE) {
		struct agentx_response *r = (struct agentx_response *)hdr;

		fprintf(stderr, " sysuptime %d error %d index %d",
		    r->data.sysuptime, r->data.error, r->data.index);
	}

	fprintf(stderr, "\n");
}
#endif
