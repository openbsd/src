/*	$OpenBSD: trap.c,v 1.23 2014/11/19 10:19:00 blambert Exp $	*/

/*
 * Copyright (c) 2008 Reyk Floeter <reyk@openbsd.org>
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

void
trap_init(void)
{
	struct ber_oid	 trapoid = OID(MIB_coldStart);

	/*
	 * Send a coldStart to notify that the daemon has been
	 * started and re-initialized.
	 */
	trap_send(&trapoid, NULL);
}

int
trap_agentx(struct agentx_handle *h, struct agentx_pdu *pdu, int *idx,
    char **varcpy, int *vcpylen)
{
	struct agentx_varbind_hdr	 vbhdr;
	u_int32_t			 d;
	struct ber_oid			 o, oid;
	struct ber_oid			 uptime = OID(MIB_sysUpTime);
	struct ber_oid			 trapoid = OID(MIB_snmpTrapOID);
	struct ber_element		*varbind, *iter;
	int				 x = 0, state = 0;
	int				 ret = AGENTX_ERR_NONE;
	int				 seensysuptime, seentrapoid;
	size_t				 len = 0;
	pid_t				 pid = -1;
	char				*v = NULL;

	*varcpy = NULL;
	varbind = NULL;
	seensysuptime = seentrapoid = 0;

	if (pdu->hdr->flags & AGENTX_NON_DEFAULT_CONTEXT) {
		ret = AGENTX_ERR_UNSUPPORTED_CONTEXT;
		goto done;
	}

	if ((v = malloc(pdu->hdr->length)) == NULL ||
	    snmp_agentx_copy_raw(pdu, v, pdu->hdr->length) == -1) {
		ret = AGENTX_ERR_PROCESSING_ERROR;
		goto done;
	}

	while (pdu->datalen > sizeof(struct agentx_hdr)) {
		x++;

		if (snmp_agentx_read_vbhdr(pdu, &vbhdr) == -1) {
			ret = AGENTX_ERR_PARSE_ERROR;
			goto done;
		}

		if (state < 2) {
			if (snmp_agentx_read_oid(pdu, (struct snmp_oid *)&oid) == -1) {
				ret = AGENTX_ERR_PARSE_ERROR;
				goto done;
			}
			if (state == 0 && ber_oid_cmp(&oid, &uptime) == 0) {
				if (snmp_agentx_read_int(pdu, &d) == -1) {
					ret = AGENTX_ERR_PARSE_ERROR;
					goto done;
				}
				state = 1;
				continue;
			} else if (ber_oid_cmp(&oid, &trapoid) == 0) {
				if (snmp_agentx_read_oid(pdu,
				    (struct snmp_oid *)&o) == -1) {
					ret = AGENTX_ERR_PARSE_ERROR;
					goto done;
				}
				state = 2;
				continue;
			} else {
				ret = AGENTX_ERR_PROCESSING_ERROR;
				goto done;
			}
		}

		ret = varbind_convert(pdu, &vbhdr, &varbind, &iter);
		if (ret != AGENTX_ERR_NONE)
			goto done;
	}

	if (varbind != NULL)
		len = ber_calc_len(varbind);
	log_debug("trap_agentx: from pid %u len %d elements %d",
	    pid, len, x);

	trap_send(&o, varbind);

	*varcpy = v;
	*vcpylen = pdu->hdr->length;

	return (AGENTX_ERR_NONE);
 done:
	if (varbind != NULL)
		ber_free_elements(varbind);
	if (v)
		free(v);
	*idx = x;
	return (ret);
}

int
trap_send(struct ber_oid *oid, struct ber_element *elm)
{
	int			 ret = 0, s;
	struct address		*tr;
	struct ber_element	*root, *b, *c, *trap;
	struct ber		 ber;
	char			*cmn;
	ssize_t			 len;
	u_int8_t		*ptr;
	struct			 ber_oid uptime = OID(MIB_sysUpTime);
	struct			 ber_oid trapoid = OID(MIB_snmpTrapOID);
	char			 ostr[SNMP_MAX_OID_STRLEN];
	struct oid		 oa, ob;

	if (TAILQ_EMPTY(&env->sc_trapreceivers))
		return (0);

	smi_scalar_oidlen(&uptime);
	smi_scalar_oidlen(&trapoid);
	smi_scalar_oidlen(oid);

	smi_oid2string(oid, ostr, sizeof(ostr), 0);
	log_debug("trap_send: oid %s", ostr);

	/* Setup OIDs to compare against the trap receiver MIB */
	bzero(&oa, sizeof(oa));
	bcopy(oid->bo_id, &oa.o_oid, sizeof(oa.o_oid));
	oa.o_oidlen = oid->bo_n;
	bzero(&ob, sizeof(ob));
	ob.o_flags = OID_TABLE;

	/* Add mandatory varbind elements */
	trap = ber_add_sequence(NULL);
	c = ber_printf_elements(trap, "{Odt}{OO}",
	    &uptime, smi_getticks(),
	    BER_CLASS_APPLICATION, SNMP_T_TIMETICKS,
	    &trapoid, oid);
	if (elm != NULL)
		ber_link_elements(c, elm);

	bzero(&ber, sizeof(ber));
	ber.fd = -1;

	TAILQ_FOREACH(tr, &env->sc_trapreceivers, entry) {
		if (tr->sa_oid != NULL && tr->sa_oid->bo_n) {
			/* The trap receiver may want only a specified MIB */
			bcopy(&tr->sa_oid->bo_id, &ob.o_oid,
			    sizeof(ob.o_oid));
			ob.o_oidlen = tr->sa_oid->bo_n;
			if (smi_oid_cmp(&oa, &ob) != 0)
				continue;
		}

		if ((s = snmpd_socket_af(&tr->ss, htons(tr->port))) == -1) {
			ret = -1;
			goto done;
		}

		cmn = tr->sa_community != NULL ?
		    tr->sa_community : env->sc_trcommunity;

		/* SNMP header */
		root = ber_add_sequence(NULL);
		b = ber_printf_elements(root, "ds{tddd",
		    SNMP_V2, cmn, BER_CLASS_CONTEXT, SNMP_C_TRAPV2,
		    arc4random(), 0, 0);
		ber_link_elements(b, trap);

#ifdef DEBUG
		smi_debug_elements(root);
#endif
		len = ber_write_elements(&ber, root);
		if (ber_get_writebuf(&ber, (void *)&ptr) > 0 &&
		    sendto(s, ptr, len, 0, (struct sockaddr *)&tr->ss,
		    tr->ss.ss_len) != -1) {
			env->sc_stats.snmp_outpkts++;
			ret++;
		}

		close(s);
		ber_unlink_elements(b);
		ber_free_elements(root);
	}

 done:
	ber_free_elements(trap);
	ber_free(&ber);

	return (ret);
}
