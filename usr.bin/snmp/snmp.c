/*	$OpenBSD: snmp.c,v 1.10 2020/03/24 14:09:14 martijn Exp $	*/

/*
 * Copyright (c) 2019 Martijn van Duren <martijn@openbsd.org>
 * Copyright (c) 2013 Reyk Floeter <reyk@openbsd.org>
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

#include <sys/socket.h>

#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "ber.h"
#include "smi.h"
#include "snmp.h"

#define UDP_MAXPACKET 65535

static struct ber_element *
    snmp_resolve(struct snmp_agent *, struct ber_element *, int);
static char *
    snmp_package(struct snmp_agent *, struct ber_element *, size_t *);
static struct ber_element *
    snmp_unpackage(struct snmp_agent *, char *, size_t);
static void snmp_v3_free(struct snmp_v3 *);
static void snmp_v3_secparamsoffset(void *, size_t);

struct snmp_v3 *
snmp_v3_init(int level, const char *ctxname, size_t ctxnamelen,
    struct snmp_sec *sec)
{
	struct snmp_v3 *v3;

	if ((level & (SNMP_MSGFLAG_SECMASK | SNMP_MSGFLAG_REPORT)) != level ||
	    sec == NULL) {
		errno = EINVAL;
		return NULL;
	}
	if ((v3 = calloc(1, sizeof(*v3))) == NULL)
		return NULL;

	v3->level = level | SNMP_MSGFLAG_REPORT;
	v3->ctxnamelen = ctxnamelen;
	if (ctxnamelen != 0) {
		if ((v3->ctxname = malloc(ctxnamelen)) == NULL) {
			free(v3);
			return NULL;
		}
		memcpy(v3->ctxname, ctxname, ctxnamelen);
	}
	v3->sec = sec;
	return v3;
}

int
snmp_v3_setengineid(struct snmp_v3 *v3, char *engineid, size_t engineidlen)
{
	if (v3->engineidset)
		free(v3->engineid);
	if ((v3->engineid = malloc(engineidlen)) == NULL)
		return -1;
	memcpy(v3->engineid, engineid, engineidlen);
	v3->engineidlen = engineidlen;
	v3->engineidset = 1;
	return 0;
}

struct snmp_agent *
snmp_connect_v12(int fd, enum snmp_version version, const char *community)
{
	struct snmp_agent *agent;

	if (version != SNMP_V1 && version != SNMP_V2C) {
		errno = EINVAL;
		return NULL;
	}
	if ((agent = malloc(sizeof(*agent))) == NULL)
		return NULL;
	agent->fd = fd;
	agent->version = version;
	if ((agent->community = strdup(community)) == NULL)
		goto fail;
	agent->timeout = 1;
	agent->retries = 5;
	agent->v3 = NULL;
	return agent;

fail:
	free(agent);
	return NULL;
}

struct snmp_agent *
snmp_connect_v3(int fd, struct snmp_v3 *v3)
{
	struct snmp_agent *agent;

	if ((agent = malloc(sizeof(*agent))) == NULL)
		return NULL;
	agent->fd = fd;
	agent->version = SNMP_V3;
	agent->v3 = v3;
	agent->timeout = 1;
	agent->retries = 5;
	agent->community = NULL;

	if (v3->sec->init(agent) == -1) {
		snmp_free_agent(agent);
		return NULL;
	}
	return agent;
}

void
snmp_free_agent(struct snmp_agent *agent)
{
	free(agent->community);
	if (agent->v3 != NULL)
		snmp_v3_free(agent->v3);
	free(agent);
}

static void
snmp_v3_free(struct snmp_v3 *v3)
{
	v3->sec->free(v3->sec->data);
	free(v3->sec);
	free(v3->ctxname);
	free(v3->engineid);
	free(v3);
}

struct ber_element *
snmp_get(struct snmp_agent *agent, struct ber_oid *oid, size_t len)
{
	struct ber_element *pdu, *varbind;
	size_t i;

	if ((pdu = ober_add_sequence(NULL)) == NULL)
		return NULL;
	if ((varbind = ober_printf_elements(pdu, "tddd{", BER_CLASS_CONTEXT,
	    SNMP_C_GETREQ, arc4random() & 0x7fffffff, 0, 0)) == NULL)
		goto fail;
	for (i = 0; i < len; i++)
		varbind = ober_printf_elements(varbind, "{O0}", &oid[i]);
		if (varbind == NULL)
			goto fail;

	return snmp_resolve(agent, pdu, 1);
fail:
	ober_free_elements(pdu);
	return NULL;
}

struct ber_element *
snmp_getnext(struct snmp_agent *agent, struct ber_oid *oid, size_t len)
{
	struct ber_element *pdu, *varbind;
	size_t i;

	if ((pdu = ober_add_sequence(NULL)) == NULL)
		return NULL;
	if ((varbind = ober_printf_elements(pdu, "tddd{", BER_CLASS_CONTEXT,
	    SNMP_C_GETNEXTREQ, arc4random() & 0x7fffffff, 0, 0)) == NULL)
		goto fail;
	for (i = 0; i < len; i++)
		varbind = ober_printf_elements(varbind, "{O0}", &oid[i]);
		if (varbind == NULL)
			goto fail;

	return snmp_resolve(agent, pdu, 1);
fail:
	ober_free_elements(pdu);
	return NULL;
}

int
snmp_trap(struct snmp_agent *agent, struct timespec *uptime,
    struct ber_oid *oid, struct ber_element *custvarbind)
{
	struct ber_element *pdu, *varbind;
	struct ber_oid sysuptime, trap;
	long long ticks;

	if ((pdu = ober_add_sequence(NULL)) == NULL)
		return -1;
	if ((varbind = ober_printf_elements(pdu, "tddd{", BER_CLASS_CONTEXT,
	    SNMP_C_TRAPV2, arc4random() & 0x7fffffff, 0, 0)) == NULL)
		goto fail;

	ticks = uptime->tv_sec * 100;
	ticks += uptime->tv_nsec / 10000000;
	if (smi_string2oid("sysUpTime.0", &sysuptime) == -1)
		goto fail;
	if ((varbind = ober_printf_elements(varbind, "{Oit}", &sysuptime, ticks,
	    BER_CLASS_APPLICATION, SNMP_T_TIMETICKS)) == NULL)
		goto fail;
	if (smi_string2oid("snmpTrapOID.0", &trap) == -1)
		goto fail;
	if ((varbind = ober_printf_elements(varbind, "{OO}", &trap, oid)) == NULL)
		goto fail;
	if (custvarbind != NULL)
		ober_link_elements(varbind, custvarbind);

	snmp_resolve(agent, pdu, 0);
	return 0;
fail:
	ober_free_elements(pdu);
	return -1;
}

struct ber_element *
snmp_getbulk(struct snmp_agent *agent, struct ber_oid *oid, size_t len,
    int non_repeaters, int max_repetitions)
{
	struct ber_element *pdu, *varbind;
	size_t i;

	if ((pdu = ober_add_sequence(NULL)) == NULL)
		return NULL;
	if ((varbind = ober_printf_elements(pdu, "tddd{", BER_CLASS_CONTEXT,
	    SNMP_C_GETBULKREQ, arc4random() & 0x7fffffff, non_repeaters,
	    max_repetitions)) == NULL)
		goto fail;
	for (i = 0; i < len; i++)
		varbind = ober_printf_elements(varbind, "{O0}", &oid[i]);
		if (varbind == NULL)
			goto fail;

	return snmp_resolve(agent, pdu, 1);
fail:
	ober_free_elements(pdu);
	return NULL;
}

struct ber_element *
snmp_set(struct snmp_agent *agent, struct ber_element *vblist)
{
	struct ber_element *pdu;

	if ((pdu = ober_add_sequence(NULL)) == NULL)
		return NULL;
	if (ober_printf_elements(pdu, "tddd{e", BER_CLASS_CONTEXT,
	    SNMP_C_SETREQ, arc4random() & 0x7fffffff, 0, 0, vblist) == NULL) {
		ober_free_elements(pdu);
		ober_free_elements(vblist);
		return NULL;
	}

	return snmp_resolve(agent, pdu, 1);
}

static struct ber_element *
snmp_resolve(struct snmp_agent *agent, struct ber_element *pdu, int reply)
{
	struct ber_element *varbind;
	struct ber_oid oid;
	struct timespec start, now;
	struct pollfd pfd;
	char *message;
	ssize_t len;
	long long reqid, rreqid;
	short direction;
	int to, nfds, ret;
	int tries;
	char buf[READ_BUF_SIZE];

	if (ober_scanf_elements(pdu, "{i", &reqid) != 0) {
		errno = EINVAL;
		ober_free_elements(pdu);
		return NULL;
	}

	if ((message = snmp_package(agent, pdu, &len)) == NULL)
		return NULL;

	clock_gettime(CLOCK_MONOTONIC, &start);
	memcpy(&now, &start, sizeof(now));
	direction = POLLOUT;
	tries = agent->retries + 1;
	while (tries) {
		pfd.fd = agent->fd;
		pfd.events = direction;
		if (agent->timeout > 0) {
			to = (agent->timeout - (now.tv_sec - start.tv_sec)) * 1000;
			to -= (now.tv_nsec - start.tv_nsec) / 1000000;
		} else
			to = INFTIM;
		nfds = poll(&pfd, 1, to);
		if (nfds == 0) {
			errno = ETIMEDOUT;
			direction = POLLOUT;
			tries--;
			continue;
		}
		if (nfds == -1) {
			if (errno == EINTR)
				continue;
			else
				goto fail;
		}
		if (direction == POLLOUT) {
			ret = send(agent->fd, message, len, MSG_DONTWAIT);
			if (ret == -1)
				goto fail;
			if (ret < len) {
				errno = EBADMSG;
				goto fail;
			}
			if (!reply)
				return NULL;
			direction = POLLIN;
			continue;
		}
		ret = recv(agent->fd, buf, sizeof(buf), MSG_DONTWAIT);
		if (ret == 0)
			errno = ECONNRESET;
		if (ret <= 0)
			goto fail;
		if ((pdu = snmp_unpackage(agent, buf, ret)) == NULL) {
			tries--;
			direction = POLLOUT;
			errno = EPROTO;
			continue;
		}
		/* Validate pdu format and check request id */
		if (ober_scanf_elements(pdu, "{iSSe", &rreqid, &varbind) != 0 ||
		    varbind->be_encoding != BER_TYPE_SEQUENCE) {
			errno = EPROTO;
			direction = POLLOUT;
			tries--;
			continue;
		}
		if (rreqid != reqid && rreqid != 0) {
			errno = EPROTO;
			direction = POLLOUT;
			tries--;
			continue;
		}
		for (varbind = varbind->be_sub; varbind != NULL;
		    varbind = varbind->be_next) {
			if (ober_scanf_elements(varbind, "{oS}", &oid) != 0) {
				errno = EPROTO;
				direction = POLLOUT;
				tries--;
				break;
			}
		}
		if (varbind != NULL)
			continue;

		free(message);
		return pdu;
	}

fail:
	free(message);
	return NULL;
}

static char *
snmp_package(struct snmp_agent *agent, struct ber_element *pdu, size_t *len)
{
	struct ber ber;
	struct ber_element *message, *scopedpdu = NULL, *secparams, *encpdu;
	ssize_t securitysize, ret;
	size_t secparamsoffset;
	char *securityparams = NULL, *packet = NULL;
	long long msgid;
	void *cookie = NULL;

	bzero(&ber, sizeof(ber));
	ober_set_application(&ber, smi_application);

	if ((message = ober_add_sequence(NULL)) == NULL) {
		ober_free_elements(pdu);
		goto fail;
	}

	switch (agent->version) {
	case SNMP_V1:
	case SNMP_V2C:
		if (ober_printf_elements(message, "dse", agent->version,
		    agent->community, pdu) == NULL) {
			ober_free_elements(pdu);
			goto fail;
		}
		break;
	case SNMP_V3:
		msgid = arc4random_uniform(2147483647);
		if ((scopedpdu = ober_add_sequence(NULL)) == NULL) {
			ober_free_elements(pdu);
			goto fail;
		}
		if (ober_printf_elements(scopedpdu, "xxe",
		    agent->v3->engineid, agent->v3->engineidlen,
		    agent->v3->ctxname, agent->v3->ctxnamelen, pdu) == NULL) {
			ober_free_elements(pdu);
			ober_free_elements(scopedpdu);
			goto fail;
		}
		pdu = NULL;
		if ((securityparams = agent->v3->sec->genparams(agent,
		    &securitysize, &cookie)) == NULL) {
			ober_free_elements(scopedpdu);
			goto fail;
		}
		if (agent->v3->level & SNMP_MSGFLAG_PRIV) {
			if ((encpdu = agent->v3->sec->encpdu(agent, scopedpdu,
			    cookie)) == NULL)
				goto fail;
			ober_free_elements(scopedpdu);
			scopedpdu = encpdu;
		}
		if (ober_printf_elements(message, "d{idxd}xe",
		    agent->version, msgid, UDP_MAXPACKET, &(agent->v3->level),
		    (size_t) 1, agent->v3->sec->model, securityparams,
		    securitysize, scopedpdu) == NULL) {
			ober_free_elements(scopedpdu);
			goto fail;
		}
		if (ober_scanf_elements(message, "{SSe", &secparams) == -1)
			goto fail;
		ober_set_writecallback(secparams, snmp_v3_secparamsoffset,
		    &secparamsoffset);
		break;
	}

	if (ober_write_elements(&ber, message) == -1)
		goto fail;
	ret = ber_copy_writebuf(&ber, (void **)&packet);

	*len = (size_t) ret;
	ober_free(&ber);

	if (agent->version == SNMP_V3 && packet != NULL) {
		if (agent->v3->sec->finalparams(agent, packet,
		    ret, secparamsoffset, cookie) == -1) {
			free(packet);
			packet = NULL;
		}
	}

fail:
	if (agent->version == SNMP_V3)
		agent->v3->sec->freecookie(cookie);
	ober_free_elements(message);
	free(securityparams);
	return packet;
}

static struct ber_element *
snmp_unpackage(struct snmp_agent *agent, char *buf, size_t buflen)
{
	struct ber ber;
	enum snmp_version version;
	char *community;
	struct ber_element *pdu;
	long long msgid, model;
	int msgsz;
	char *msgflags, *secparams;
	size_t msgflagslen, secparamslen;
	struct ber_element *message = NULL, *payload, *scopedpdu, *ctxname;
	off_t secparamsoffset;
	char *encpdu, *engineid;
	size_t encpdulen, engineidlen;
	void *cookie = NULL;

	bzero(&ber, sizeof(ber));
	ober_set_application(&ber, smi_application);

	ober_set_readbuf(&ber, buf, buflen);
	if ((message = ober_read_elements(&ber, NULL)) == NULL)
		return NULL;
	ober_free(&ber);

	if (ober_scanf_elements(message, "{de", &version, &payload) != 0)
		goto fail;

	if (version != agent->version)
		goto fail;

	switch (version) {
	case SNMP_V1:
	case SNMP_V2C:
		if (ober_scanf_elements(payload, "se", &community, &pdu) == -1)
			goto fail;
		if (strcmp(community, agent->community) != 0)
			goto fail;
		ober_unlink_elements(payload);
		ober_free_elements(message);
		return pdu;
	case SNMP_V3:
		if (ober_scanf_elements(payload, "{idxi}pxe", &msgid, &msgsz,
		    &msgflags, &msgflagslen, &model, &secparamsoffset,
		    &secparams, &secparamslen, &scopedpdu) == -1)
			goto fail;
		if (msgflagslen != 1)
			goto fail;
		if (agent->v3->sec->parseparams(agent, buf, buflen,
		    secparamsoffset, secparams, secparamslen, msgflags[0],
		    &cookie) == -1) {
			cookie = NULL;
			goto fail;
		}
		if (msgflags[0] & SNMP_MSGFLAG_PRIV) {
			if (ober_scanf_elements(scopedpdu, "x", &encpdu,
			    &encpdulen) == -1)
				goto fail;
			if ((scopedpdu = agent->v3->sec->decpdu(agent, encpdu,
			    encpdulen, cookie)) == NULL)
				goto fail;
		}
		if (ober_scanf_elements(scopedpdu, "{xeS{", &engineid,
		    &engineidlen, &ctxname) == -1)
			goto fail;
		if (!agent->v3->engineidset) {
			if (snmp_v3_setengineid(agent->v3, engineid,
			    engineidlen) == -1)
				goto fail;
		}
		pdu = ober_unlink_elements(ctxname);
		/* Accept reports, so we can continue if possible */
		if (pdu->be_type != SNMP_C_REPORT) {
			if ((msgflags[0] & SNMP_MSGFLAG_SECMASK) !=
			    (agent->v3->level & SNMP_MSGFLAG_SECMASK))
				goto fail;
		}

		ober_free_elements(message);
		agent->v3->sec->freecookie(cookie);
		return pdu;
	}
	/* NOTREACHED */

fail:
	if (version == SNMP_V3)
		agent->v3->sec->freecookie(cookie);
	ober_free_elements(message);
	return NULL;
}

static void
snmp_v3_secparamsoffset(void *cookie, size_t offset)
{
	size_t *spoffset = cookie;

	*spoffset = offset;
}

ssize_t
ber_copy_writebuf(struct ber *ber, void **buf)
{
	char *bbuf;
	ssize_t ret;

	*buf = NULL;
	if ((ret = ober_get_writebuf(ber, (void **)&bbuf)) == -1)
		return -1;
	if ((*buf = malloc(ret)) == NULL)
		return -1;
	memcpy(*buf, bbuf, ret);
	return  ret;
}
