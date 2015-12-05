/*	$OpenBSD: snmpctl.c,v 1.21 2015/12/05 13:14:40 claudio Exp $	*/

/*
 * Copyright (c) 2007, 2008 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2005 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2004, 2005 Esben Norby <norby@openbsd.org>
 * Copyright (c) 2003 Henning Brauer <henning@openbsd.org>
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
#include <sys/tree.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <event.h>

#include "snmpd.h"
#include "snmp.h"
#include "parser.h"
#include "mib.h"

__dead void	 usage(void);

struct imsgname {
	int type;
	char *name;
	void (*func)(struct imsg *);
};

void		 show_mib(void);
struct imsgname *monitor_lookup(u_int8_t);
void		 monitor_id(struct imsg *);
int		 monitor(struct imsg *);

struct imsgname imsgs[] = {
	{ 0,				NULL,			NULL }
};
struct imsgname imsgunknown = {
	-1,				"<unknown>",		NULL
};

void snmpctl_trap(int, struct parse_result *);

struct imsgbuf	 ibuf;
struct snmpd	*env;
struct oid	 mib_tree[] = MIB_TREE;

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-n] [-s socket] command [arg ...]\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct sockaddr_un	 sun;
	struct parse_result	*res;
	struct imsg		 imsg;
	int			 ctl_sock;
	int			 done = 0;
	int			 n;
	int			 ch;
	const char		*sock = SNMPD_SOCKET;

	if ((env = calloc(1, sizeof(struct snmpd))) == NULL)
		err(1, "calloc");
	gettimeofday(&env->sc_starttime, NULL);

	while ((ch = getopt(argc, argv, "ns:")) != -1) {
		switch (ch) {
		case 'n':
			env->sc_flags |= SNMPD_F_NONAMES;
			break;
		case 's':
			sock = optarg;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	smi_init();

	/* parse options */
	if ((res = parse(argc, argv)) == NULL)
		exit(1);

	switch (res->action) {
	case NONE:
		usage();
		break;
	case SHOW_MIB:
		show_mib();
		break;
	case WALK:
	case GET:
	case BULKWALK:
		snmpclient(res);
		break;
	default:
		goto connect;
	}

	free(env);
	return (0);

 connect:
	/* connect to snmpd control socket */
	if ((ctl_sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		err(1, "socket");

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, sock, sizeof(sun.sun_path));
 reconnect:
	if (connect(ctl_sock, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		/* Keep retrying if running in monitor mode */
		if (res->action == MONITOR &&
		    (errno == ENOENT || errno == ECONNREFUSED)) {
			usleep(100);
			goto reconnect;
		}
		err(1, "connect: %s", sock);
	}

	imsg_init(&ibuf, ctl_sock);
	done = 0;

	/* process user request */
	switch (res->action) {
	case MONITOR:
		imsg_compose(&ibuf, IMSG_CTL_NOTIFY, 0, 0, -1, NULL, 0);
		break;
	case NONE:
	case SHOW_MIB:
	case WALK:
	case GET:
	case BULKWALK:
		break;
	case TRAP:
		/* explicitly downgrade the socket */
		imsg_compose(&ibuf, IMSG_SNMP_AGENTX, 0, 0, -1, NULL, 0);
		break;
	}

	while (ibuf.w.queued)
		if (msgbuf_write(&ibuf.w) <= 0 && errno != EAGAIN)
			err(1, "write error");

	while (!done) {
		if ((n = imsg_read(&ibuf)) == -1 && errno != EAGAIN)
			errx(1, "imsg_read error");
		if (n == 0)
			errx(1, "pipe closed");

		while (!done) {
			if ((n = imsg_get(&ibuf, &imsg)) == -1)
				errx(1, "imsg_get error");
			if (n == 0)
				break;
			switch (res->action) {
			case MONITOR:
				done = monitor(&imsg);
				break;
			case TRAP:
				if (imsg.hdr.type == IMSG_CTL_OK) {
					snmpctl_trap(ctl_sock, res);
					done = 1;
				} else
					errx(1, "snmpd refused connection");
				break;
			case NONE:
			case SHOW_MIB:
			case WALK:
			case GET:
			case BULKWALK:
				break;
			}
			imsg_free(&imsg);
		}
	}
	close(ctl_sock);

	return (0);
}

void
mib_init(void)
{
	/*
	 * MIB declarations (to register the OID names)
	 */
	smi_mibtree(mib_tree);
}

void
show_mib(void)
{
	struct oid	*oid;

	for (oid = NULL; (oid = smi_foreach(oid, 0)) != NULL;) {
		char	 buf[BUFSIZ];
		smi_oid2string(&oid->o_id, buf, sizeof(buf), 0);
		printf("%s\n", buf);
	}
}

struct imsgname *
monitor_lookup(u_int8_t type)
{
	int i;

	for (i = 0; imsgs[i].name != NULL; i++)
		if (imsgs[i].type == type)
			return (&imsgs[i]);
	return (&imsgunknown);
}

int
monitor(struct imsg *imsg)
{
	time_t			 now;
	int			 done = 0;
	struct imsgname		*imn;

	now = time(NULL);

	imn = monitor_lookup(imsg->hdr.type);
	printf("%s: imsg type %u len %u peerid %u pid %d\n", imn->name,
	    imsg->hdr.type, imsg->hdr.len, imsg->hdr.peerid, imsg->hdr.pid);
	printf("\ttimestamp: %lld, %s", (long long)now, ctime(&now));
	if (imn->type == -1)
		done = 1;
	if (imn->func != NULL)
		(*imn->func)(imsg);

	return (done);
}

void
snmpctl_trap(int ctl_sock, struct parse_result *res)
{
	struct snmp_oid		 oid, oid1;
	struct parse_varbind	*vb;
	struct agentx_handle	*h;
	struct agentx_pdu	*pdu;
	void			*ptr;

	if ((h = snmp_agentx_fdopen(ctl_sock, "snmpctl", NULL)) == NULL)
		err(1, "agentx socket");

	if (ber_string2oid(res->trapoid, (struct ber_oid *)&oid) == -1)
		errx(1, "bad trap oid %s", res->trapoid);

	if ((pdu = snmp_agentx_notify_pdu(&oid)) == NULL)
		errx(1, "notify start");

	while ((vb = TAILQ_FIRST(&res->varbinds)) != NULL) {

		if (ber_string2oid(vb->sm.snmp_oid,
		    (struct ber_oid *)&oid) == -1)
			errx(1, "bad oid %s", vb->sm.snmp_oid);

		switch (vb->sm.snmp_type) {

		/* SNMP_GAUGE32 is handled here */
		case SNMP_INTEGER32:
		case SNMP_NSAPADDR:
			if (snmp_agentx_varbind(pdu, &oid, AGENTX_INTEGER,
			    &vb->u.d, vb->sm.snmp_len) == -1)
				errx(1, "varbind");
			break;

		case SNMP_OPAQUE:
			if (snmp_agentx_varbind(pdu, &oid, AGENTX_OPAQUE,
			    &vb->u.d, vb->sm.snmp_len) == -1)
				errx(1, "varbind");
			break;

		case SNMP_COUNTER32:
			if (snmp_agentx_varbind(pdu, &oid, AGENTX_COUNTER32,
			    &vb->u.d, vb->sm.snmp_len) == -1)
				errx(1, "varbind");
			break;

		case SNMP_TIMETICKS:
			if (snmp_agentx_varbind(pdu, &oid, AGENTX_TIME_TICKS,
			    &vb->u.d, vb->sm.snmp_len) == -1)
				errx(1, "varbind");
			break;

		case SNMP_UINTEGER32:
		case SNMP_UNSIGNED32:
			if (snmp_agentx_varbind(pdu, &oid, AGENTX_INTEGER,
			    &vb->u.u, vb->sm.snmp_len) == -1)
				errx(1, "varbind");
			break;

		case SNMP_COUNTER64:
			if (snmp_agentx_varbind(pdu, &oid, AGENTX_COUNTER64,
			    &vb->u.l, vb->sm.snmp_len) == -1)
				errx(1, "varbind");
			break;

		case SNMP_IPADDR:
			if (vb->sm.snmp_len == sizeof(struct in6_addr))
				ptr = &vb->u.in6;
			else
				ptr = &vb->u.in4;

			if (snmp_agentx_varbind(pdu, &oid, AGENTX_OPAQUE,
			    ptr, vb->sm.snmp_len) == -1)
				errx(1, "varbind");
			break;

		case SNMP_OBJECT:
			if (ber_string2oid(vb->u.str,
			    (struct ber_oid *)&oid1) == -1)
				errx(1, "invalid OID %s", vb->u.str);

			if (snmp_agentx_varbind(pdu, &oid,
			    AGENTX_OBJECT_IDENTIFIER, &oid1,
			    sizeof(oid1)) == -1)
				errx(1, "varbind");
			break;

		case SNMP_BITSTRING:
		case SNMP_OCTETSTRING:
			if (snmp_agentx_varbind(pdu, &oid, AGENTX_OCTET_STRING,
			    vb->u.str, vb->sm.snmp_len) == -1)
				errx(1, "varbind");
			break;

		case SNMP_NULL:
			/* no data beyond the OID itself */
			if (snmp_agentx_varbind(pdu, &oid, AGENTX_NULL,
			    NULL, 0) == -1)
				errx(1, "varbind");
			break;
		}

		TAILQ_REMOVE(&res->varbinds, vb, vb_entry);
		free(vb);
	}

	if ((pdu = snmp_agentx_request(h, pdu)) == NULL)
		err(1, "request: %i", h->error);
	if (snmp_agentx_response(h, pdu) == -1)
		errx(1, "response: %i", h->error);
	snmp_agentx_pdu_free(pdu);
	if (snmp_agentx_close(h, AGENTX_CLOSE_SHUTDOWN) == -1)
		err(1, "close");
}
