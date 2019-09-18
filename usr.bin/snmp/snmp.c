/*	$OpenBSD: snmp.c,v 1.3 2019/09/18 09:44:38 martijn Exp $	*/

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

static struct ber_element *
    snmp_resolve(struct snmp_agent *, struct ber_element *, int);
static char *
    snmp_package(struct snmp_agent *, struct ber_element *, size_t *);
static struct ber_element *
    snmp_unpackage(struct snmp_agent *, char *, size_t);

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
	return agent;

fail:
	free(agent->community);
	free(agent);
	return NULL;
}

void
snmp_free_agent(struct snmp_agent *agent)
{
	free(agent->community);
	free(agent);
}

struct ber_element *
snmp_get(struct snmp_agent *agent, struct ber_oid *oid, size_t len)
{
	struct ber_element *pdu, *varbind;
	size_t i;

	if ((pdu = ber_add_sequence(NULL)) == NULL)
		return NULL;
	if ((varbind = ber_printf_elements(pdu, "tddd{", BER_CLASS_CONTEXT,
	    SNMP_C_GETREQ, arc4random() & 0x7fffffff, 0, 0)) == NULL)
		goto fail;
	for (i = 0; i < len; i++)
		varbind = ber_printf_elements(varbind, "{O0}", &oid[i]);
		if (varbind == NULL)
			goto fail;

	return snmp_resolve(agent, pdu, 1);
fail:
	ber_free_elements(pdu);
	return NULL;
}

struct ber_element *
snmp_getnext(struct snmp_agent *agent, struct ber_oid *oid, size_t len)
{
	struct ber_element *pdu, *varbind;
	size_t i;

	if ((pdu = ber_add_sequence(NULL)) == NULL)
		return NULL;
	if ((varbind = ber_printf_elements(pdu, "tddd{", BER_CLASS_CONTEXT,
	    SNMP_C_GETNEXTREQ, arc4random() & 0x7fffffff, 0, 0)) == NULL)
		goto fail;
	for (i = 0; i < len; i++)
		varbind = ber_printf_elements(varbind, "{O0}", &oid[i]);
		if (varbind == NULL)
			goto fail;

	return snmp_resolve(agent, pdu, 1);
fail:
	ber_free_elements(pdu);
	return NULL;
}

int
snmp_trap(struct snmp_agent *agent, struct timespec *uptime,
    struct ber_oid *oid, struct ber_element *custvarbind)
{
	struct ber_element *pdu, *varbind;
	struct ber_oid sysuptime, trap;
	long long ticks;

	if ((pdu = ber_add_sequence(NULL)) == NULL)
		return -1;
	if ((varbind = ber_printf_elements(pdu, "tddd{", BER_CLASS_CONTEXT,
	    SNMP_C_TRAPV2, arc4random() & 0x7fffffff, 0, 0)) == NULL)
		goto fail;

	ticks = uptime->tv_sec * 100;
	ticks += uptime->tv_nsec / 10000000;
	if (smi_string2oid("sysUpTime.0", &sysuptime) == -1)
		goto fail;
	if ((varbind = ber_printf_elements(varbind, "{Oit}", &sysuptime, ticks,
	    BER_CLASS_APPLICATION, SNMP_T_TIMETICKS)) == NULL)
		goto fail;
	if (smi_string2oid("snmpTrapOID.0", &trap) == -1)
		goto fail;
	if ((varbind = ber_printf_elements(varbind, "{OO}", &trap, oid)) == NULL)
		goto fail;
	if (custvarbind != NULL)
		ber_link_elements(varbind, custvarbind);

	snmp_resolve(agent, pdu, 0);
	return 0;
fail:
	ber_free_elements(pdu);
	return -1;
}

struct ber_element *
snmp_getbulk(struct snmp_agent *agent, struct ber_oid *oid, size_t len,
    int non_repeaters, int max_repetitions)
{
	struct ber_element *pdu, *varbind;
	size_t i;

	if ((pdu = ber_add_sequence(NULL)) == NULL)
		return NULL;
	if ((varbind = ber_printf_elements(pdu, "tddd{", BER_CLASS_CONTEXT,
	    SNMP_C_GETBULKREQ, arc4random() & 0x7fffffff, non_repeaters,
	    max_repetitions)) == NULL)
		goto fail;
	for (i = 0; i < len; i++)
		varbind = ber_printf_elements(varbind, "{O0}", &oid[i]);
		if (varbind == NULL)
			goto fail;

	return snmp_resolve(agent, pdu, 1);
fail:
	ber_free_elements(pdu);
	return NULL;
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

	if (ber_scanf_elements(pdu, "{i", &reqid) != 0) {
		errno = EINVAL;
		ber_free_elements(pdu);
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
		if (ber_scanf_elements(pdu, "{iSSe", &rreqid, &varbind) != 0 ||
		    varbind->be_encoding != BER_TYPE_SEQUENCE) {
			errno = EPROTO;
			direction = POLLOUT;
			tries--;
			continue;
		}
		if (rreqid != reqid) {
			errno = EPROTO;
			direction = POLLOUT;
			tries--;
			continue;
		}
		for (varbind = varbind->be_sub; varbind != NULL;
		    varbind = varbind->be_next) {
			if (ber_scanf_elements(varbind, "{oS}", &oid) != 0) {
				errno = EPROTO;
				direction = POLLOUT;
				tries--;
				break;
			}
		}

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
	struct ber_element *message;
	ssize_t ret;
	char *packet = NULL;

	bzero(&ber, sizeof(ber));
	ber_set_application(&ber, smi_application);

	if ((message = ber_add_sequence(NULL)) == NULL) {
		ber_free_elements(pdu);
		goto fail;
	}

	switch (agent->version) {
	case SNMP_V1:
	case SNMP_V2C:
		if (ber_printf_elements(message, "dse", agent->version,
		    agent->community, pdu) == NULL) {
			ber_free_elements(pdu);
			goto fail;
		}
		break;
	case SNMP_V3:
		break;
	}

	if (ber_write_elements(&ber, message) == -1)
		goto fail;
	ret = ber_copy_writebuf(&ber, (void **)&packet);

	*len = (size_t) ret;
	ber_free(&ber);

fail:
	ber_free_elements(message);
	return packet;
}

static struct ber_element *
snmp_unpackage(struct snmp_agent *agent, char *buf, size_t buflen)
{
	struct ber ber;
	enum snmp_version version;
	char *community;
	struct ber_element *pdu;
	struct ber_element *message = NULL, *payload;

	bzero(&ber, sizeof(ber));
	ber_set_application(&ber, smi_application);

	ber_set_readbuf(&ber, buf, buflen);
	if ((message = ber_read_elements(&ber, NULL)) == NULL)
		return NULL;
	ber_free(&ber);

	if (ber_scanf_elements(message, "{de", &version, &payload) != 0)
		goto fail;

	if (version != agent->version)
		goto fail;

	switch (version)
	{
	case SNMP_V1:
	case SNMP_V2C:
		if (ber_scanf_elements(payload, "se", &community, &pdu) == -1)
			goto fail;
		if (strcmp(community, agent->community) != 0)
			goto fail;
		ber_unlink_elements(payload);
		ber_free_elements(message);
		return pdu;
	case SNMP_V3:
		break;
	}
	/* NOTREACHED */

fail:
	ber_free_elements(message);
	return NULL;
}

ssize_t
ber_copy_writebuf(struct ber *ber, void **buf)
{
	char *bbuf;
	ssize_t ret;

	*buf = NULL;
	if ((ret = ber_get_writebuf(ber, (void **)&bbuf)) == -1)
		return -1;
	if ((*buf = malloc(ret)) == NULL)
		return -1;
	memcpy(*buf, bbuf, ret);
	return  ret;
}
