/*	$OpenBSD: trap.c,v 1.3 2008/01/16 19:36:06 reyk Exp $	*/

/*
 * Copyright (c) 2008 Reyk Floeter <reyk@vantronix.net>
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
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

#include "snmpd.h"
#include "mib.h"

extern struct snmpd	*env;

int
trap_imsg(struct imsgbuf *ibuf, pid_t pid)
{
	struct imsg		 imsg;
	int			 ret = -1, n, x = 0;
	int			 done = 0;
	struct snmp_imsg	*sm;
	u_int32_t		 d;
	u_int64_t		 l;
	u_int8_t		*c;
	char			 ostr[SNMP_MAX_OID_LEN];
	struct ber_element	*ber, *trap = NULL, *oid = NULL, *a;
	size_t			 len;
	struct			 ber_oid o;
	struct			 ber_oid uptime = OID(MIB_sysUpTime);
	struct			 ber_oid trapoid = OID(MIB_snmpTrapOID);

	bzero(&o, sizeof(o));
	smi_oidlen(&uptime);
	ber = trap = ber_printf_elements(NULL, "{{Oit}",
	    &uptime, smi_getticks(),
	    BER_CLASS_APPLICATION, SNMP_T_TIMETICKS);

	while (!done) {
		while (!done) {
			if ((n = imsg_get(ibuf, &imsg)) == -1)
				goto done;
			if (n == 0)
				break;
			switch (imsg.hdr.type) {
			case IMSG_SNMP_ELEMENT:
				if (imsg.hdr.len < (IMSG_HEADER_SIZE +
				    sizeof(struct snmp_imsg)))
					goto imsgdone;

				sm = (struct snmp_imsg *)imsg.data;

				if (oid == NULL) {
					/* First element must be the trap OID */
					if (sm->snmp_type != SNMP_NULL)
						goto imsgdone;
					ber_string2oid(sm->snmp_oid, &o);
					smi_oidlen(&trapoid);
					ber = oid = ber_printf_elements(ber,
					    "{OO}", &trapoid, &o);
					break;
				}

				ber = a = ber_add_sequence(ber);
				a = ber_add_oidstring(a, sm->snmp_oid);

				switch (sm->snmp_type) {
				case SNMP_OBJECT:
					if (sm->snmp_len != sizeof(ostr))
						goto imsgdone;
					bcopy(sm + 1, &ostr, sm->snmp_len);
					a = ber_add_oidstring(a, ostr);
					break;
				case SNMP_BITSTRING:
				case SNMP_OCTETSTRING:
					if ((sm->snmp_len < 1) ||
					    (sm->snmp_len >= SNMPD_MAXSTRLEN))
						goto imsgdone;
					if ((c =
					    calloc(1, sm->snmp_len)) == NULL)
						goto imsgdone;
					bcopy(sm + 1, c, sm->snmp_len);
					if (sm->snmp_type == SNMP_BITSTRING)
						a = ber_add_bitstring(a, c,
						    sm->snmp_len);
					else
						a = ber_add_nstring(a, c,
						    sm->snmp_len);
					a->be_free = 1;
					break;
				case SNMP_NULL:
					a = ber_add_null(a);
					break;
				case SNMP_INTEGER32:
				case SNMP_IPADDR:
				case SNMP_COUNTER32:
				case SNMP_GAUGE32:
				case SNMP_TIMETICKS:
				case SNMP_OPAQUE:
				case SNMP_UINTEGER32:
					if (sm->snmp_len != sizeof(d))
						goto imsgdone;
					bcopy(sm + 1, &d, sm->snmp_len);
					a = ber_add_integer(a, d);
					break;
				case SNMP_COUNTER64:
					if (sm->snmp_len != sizeof(l))
						goto imsgdone;
					bcopy(sm + 1, &l, sm->snmp_len);
					a = ber_add_integer(a, d);
					break;
				default:
					log_debug("snmpe_trap: illegal type %d",
					    sm->snmp_type);
					imsg_free(&imsg);
					goto imsgdone;
				}
				switch (sm->snmp_type) {
				case SNMP_INTEGER32:
				case SNMP_BITSTRING:
				case SNMP_OCTETSTRING:
				case SNMP_NULL:
				case SNMP_OBJECT:
					/* universal types */
					break;
				default:
					/* application-specific types */
					ber_set_header(a, BER_CLASS_APPLICATION,
					    sm->snmp_type);
					break;
				}
				x++;
				break;
			case IMSG_SNMP_END:
				done = 1;
				break;
			default:
				log_debug("snmpe_trap: illegal imsg %d",
				    imsg.hdr.type);
				goto imsgdone;
			}
			imsg_free(&imsg);
		}
		if (done)
			break;
		if ((n = imsg_read(ibuf)) == -1)
			goto done;
		if (n == 0)
			goto done;
	}

	len = ber_calc_len(trap);
	smi_oidstring(&o, ostr, sizeof(ostr));
	log_debug("trap_imsg: %s, len %d, pid %d", ostr, len, pid);

	trap_send(trap, &o);

	ret = 0;
 imsgdone:
	if (ret != 0)
		imsg_free(&imsg);
 done:
	ber_free_elements(trap);
	return (ret);
}

int
trap_send(struct ber_element *trap, struct ber_oid *oid)
{
	int				 ok = 0, s;
	struct address			*tr;
	struct ber_element		*root, *b;
	struct ber			 ber;
	char				*c;
	ssize_t				 len;
	u_int8_t			*ptr;

	if (TAILQ_EMPTY(&env->sc_trapreceivers))
		return (0);

	bzero(&ber, sizeof(ber));
	ber.fd = -1;

	TAILQ_FOREACH(tr, &env->sc_trapreceivers, entry) {
		if (tr->sa_oid != NULL && tr->sa_oid->bo_n)
			/* XXX only send if the OID is specified */;

		if ((s = snmpd_socket_af(&tr->ss, htons(tr->port))) == -1)
			return (-1);

		c = tr->sa_community != NULL ?
		    tr->sa_community : env->sc_trcommunity;

		/* SNMP header */
		root = ber_add_sequence(NULL);
		b = ber_printf_elements(root, "ds{tiii",
		    SNMP_V2, c, BER_CLASS_CONTEXT, SNMP_C_TRAPV2,
		    arc4random(), 0, 0);
		ber_link_elements(b, trap);

#ifdef DEBUG
		snmpe_debug_elements(root);
#endif

		len = ber_write_elements(&ber, root);
		if (ber_get_writebuf(&ber, (void *)&ptr) > 0 &&
		    sendto(s, ptr, len, 0, (struct sockaddr *)&tr->ss,
		    tr->ss.ss_len) != -1)
			env->sc_stats.snmp_outpkts++;

		close(s);
		ber_unlink_elements(b);
		ber_free_elements(root);
	}

	return (ok);
}
