/*	$OpenBSD: snmpclient.c,v 1.1 2013/10/01 12:41:49 reyk Exp $	*/

/*
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

#include <sys/queue.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/tree.h>

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <poll.h>
#include <err.h>

#include "snmpd.h"
#include "mib.h"
#include "ber.h"
#include "parser.h"

struct snmpc {
	char			 sc_root[BUFSIZ];
	size_t			 sc_root_len;
	struct ber_oid		 sc_root_oid;
	struct ber_oid		 sc_last_oid;
	struct ber_oid		 sc_oid;
	struct sockaddr_storage	 sc_addr;
	socklen_t		 sc_addr_len;
	u_int32_t		 sc_msgid;
	int			 sc_fd;
	int			 sc_retry;
	int			 sc_retry_max;
	const char		*sc_community;
	int			 sc_version;
	int			 sc_nresp;
};

#define	SNMPC_RETRY_MAX		3
#define SNMPC_COMMUNITY		"public"
#define SNMPC_OID		"system"

void	 snmpc_req(int, int, const char *, struct ber_oid *,
	    struct sockaddr *, socklen_t, u_int32_t *, u_long);
void	 snmpc_getreq(struct snmpc *, u_long);
struct ber_element
	*snmpc_getresp(int, int, u_int32_t);

void
snmpclient(struct parse_result *res)
{
	struct snmpc		 sc;
	struct addrinfo		 hints, *ai, *ai0;
	int			 s;
	int			 error;

	if (res->oid == NULL)
		res->oid = strdup(SNMPC_OID);
	if (res->community == NULL)
		res->community = strdup(SNMPC_COMMUNITY);
	if (res->version == -1 || res->version > SNMP_V2)
		res->version = SNMP_V2;
	if (res->oid == NULL || res->community == NULL)
		err(1, "strdup");

	bzero(&hints, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	error = getaddrinfo(res->host, "snmp", &hints, &ai0);
	if (error)
		errx(1, "%s", gai_strerror(error));
	s = -1;
	for (ai = ai0; ai; ai = ai->ai_next) {
		if ((s = socket(ai->ai_family, ai->ai_socktype,
                    ai->ai_protocol)) == -1) {
			continue;
		}
		/* use first available address */
 		break;
	}
	if (s == -1)
		errx(1, "invalid host");

	bzero(&sc, sizeof(sc));
	bcopy(ai->ai_addr, &sc.sc_addr, ai->ai_addrlen);
	sc.sc_addr_len = ai->ai_addrlen;
	sc.sc_fd = s;
	sc.sc_retry_max = SNMPC_RETRY_MAX;
	sc.sc_community = res->community;
	sc.sc_version = res->version;

	if (smi_string2oid(res->oid, &sc.sc_oid) == -1)
		errx(1, "oid");

	/*
	 * Get the size of the requested root OID to shorten the output
	 * of its children.
	 */
	smi_oid2string(&sc.sc_oid, sc.sc_root, sizeof(sc.sc_root), 0);
	if (sc.sc_oid.bo_n > 2)
		sc.sc_root_len = sc.sc_oid.bo_n - 1;

	bcopy(&sc.sc_oid, &sc.sc_root_oid, sizeof(sc.sc_root_oid));
	bcopy(&sc.sc_oid, &sc.sc_last_oid, sizeof(sc.sc_last_oid));

	if (res->action == GET)
		snmpc_getreq(&sc, SNMP_C_GETREQ);
	else
		snmpc_getreq(&sc, SNMP_C_GETNEXTREQ);

	close(s);
	freeaddrinfo(ai0);
}

void
snmpc_getreq(struct snmpc *sc, u_long cmd)
{
	struct ber_element	*resp = NULL, *e;
	char			*value = NULL, *p;
	char			 buf[BUFSIZ];
	struct pollfd		 pfd[1];
	int			 nfds;

	snmpc_req(sc->sc_fd, sc->sc_version, sc->sc_community, &sc->sc_oid,
	    (struct sockaddr *)&sc->sc_addr, sc->sc_addr_len,
	    &sc->sc_msgid, cmd);

	/* Wait for input */
	pfd[0].fd = sc->sc_fd;
	pfd[0].events = POLLIN;
	nfds = poll(pfd, 1, 3 * 1000);
	if (nfds == -1 || (pfd[0].revents & (POLLERR|POLLHUP|POLLNVAL)))
		errx(1, "poll error");
	if (nfds == 0) {
		if (sc->sc_retry++ < sc->sc_retry_max) {
			warnx("time out, retry %d/%d",
			    sc->sc_retry, sc->sc_retry_max);
			snmpc_getreq(sc, cmd);
			return;
		}
		errx(1, "time out");
	}
	sc->sc_retry = 0;

	if ((resp = snmpc_getresp(sc->sc_fd, sc->sc_version,
	    sc->sc_msgid)) == NULL)
		errx(1, "invalid response");

	if (ber_scanf_elements(resp, "{SS{SSSS{{oe}}}",
	    &sc->sc_oid, &e) != 0)
		err(1, "response");

	if (sc->sc_nresp++ &&
	    (ber_oid_cmp(&sc->sc_root_oid, &sc->sc_oid) != 2 ||
	    e->be_type == BER_TYPE_NULL))
		return;

	if ((value = smi_print_element(e)) != NULL) {
		smi_oid2string(&sc->sc_oid, buf, sizeof(buf), sc->sc_root_len);
		p = buf;
		if (*p != '\0')
			printf("%s=%s\n", p, value);
		else
			printf("%s\n", value);
		free(value);
	}
	ber_free_elements(resp);

	bcopy(&sc->sc_oid, &sc->sc_last_oid, sizeof(sc->sc_last_oid));

	if (cmd != SNMP_C_GETNEXTREQ)
		return;

	snmpc_getreq(sc, cmd);
}

void
snmpc_req(int s, int version, const char *community, struct ber_oid *oid,
    struct sockaddr *addr, socklen_t addrlen, u_int32_t *msgid, u_long type)
{
	struct ber_element	*root, *b;
	struct ber		 ber;
	ssize_t			 len;
	u_int8_t		*ptr;

	/* SNMP header */
	*msgid = arc4random();
	if ((root = ber_add_sequence(NULL)) == NULL ||
	    (b = ber_printf_elements(root, "ds{tddd{{O0}}",
	    version, community, BER_CLASS_CONTEXT, type,
	    *msgid, 0, 0, oid)) == NULL)
		err(1, "invalid elements");

#ifdef DEBUG
	fprintf(stderr, "REQUEST(%lu):\n", type);
	smi_debug_elements(root);
#endif

	bzero(&ber, sizeof(ber));
	ber.fd = -1;

	len = ber_write_elements(&ber, root);
	if (ber_get_writebuf(&ber, (void *)&ptr) < 1)
		err(1, "sendto");

	if (sendto(s, ptr, len, 0, addr, addrlen) == -1)
		err(1, "sendto");
}

struct ber_element *
snmpc_getresp(int s, int msgver, u_int32_t msgid)
{
	char			 buf[READ_BUF_SIZE];
	ssize_t			 rlen;
	struct ber		 ber;
	struct ber_element	*resp;
	char			*comn;
	long long		 ver, id;

	if ((rlen = recv(s, buf, sizeof(buf), MSG_WAITALL)) == -1)
		err(1, "recv");

	bzero(&ber, sizeof(ber));
	ber.fd = -1;
	ber_set_application(&ber, smi_application);
	ber_set_readbuf(&ber, buf, rlen);

#ifdef DEBUG
	fprintf(stderr, "RESPONSE (%ld bytes):\n", rlen);
#endif

	resp = ber_read_elements(&ber, NULL);
	if (resp == NULL)
		err(1, "ber_read_elements");

#ifdef DEBUG
	smi_debug_elements(resp);
#endif

	if (ber_scanf_elements(resp, "{is{i", &ver, &comn, &id) != 0)
		err(1, "response");
	if (!(msgver == (int)ver && msgid == (u_int32_t)id)) {
		ber_free_elements(resp);
		return (NULL);
	}

	return (resp);
}
