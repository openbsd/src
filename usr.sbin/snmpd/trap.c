/*	$OpenBSD: trap.c,v 1.36 2020/09/06 15:51:28 martijn Exp $	*/

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
trap_send(struct ber_oid *oid, struct ber_element *elm)
{
	int			 ret = 0, s;
	struct trap_address	*tr;
	struct ber_element	*root, *b, *c, *trap;
	struct ber		 ber;
	char			*cmn;
	ssize_t			 len;
	u_int8_t		*ptr;
	struct			 ber_oid uptime = OID(MIB_sysUpTime);
	struct			 ber_oid trapoid = OID(MIB_snmpTrapOID);
	char			 ostr[SNMP_MAX_OID_STRLEN];
	struct oid		 oa, ob;

	if (TAILQ_EMPTY(&snmpd_env->sc_trapreceivers))
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
	trap = ober_add_sequence(NULL);
	c = ober_printf_elements(trap, "{Odt}{OO}",
	    &uptime, smi_getticks(),
	    BER_CLASS_APPLICATION, SNMP_T_TIMETICKS,
	    &trapoid, oid);
	if (elm != NULL)
		ober_link_elements(c, elm);

	bzero(&ber, sizeof(ber));

	TAILQ_FOREACH(tr, &snmpd_env->sc_trapreceivers, entry) {
		if (tr->sa_oid != NULL && tr->sa_oid->bo_n) {
			/* The trap receiver may want only a specified MIB */
			bcopy(&tr->sa_oid->bo_id, &ob.o_oid,
			    sizeof(ob.o_oid));
			ob.o_oidlen = tr->sa_oid->bo_n;
			if (smi_oid_cmp(&oa, &ob) != 0)
				continue;
		}

		if ((s = snmpd_socket_af(&tr->ss, SOCK_DGRAM)) == -1) {
			ret = -1;
			goto done;
		}
		if (tr->ss_local.ss_family != 0) {
			if (bind(s, (struct sockaddr *)&(tr->ss_local),
			    tr->ss_local.ss_len) == -1) {
				ret = -1;
				goto done;
			}
		}

		cmn = tr->sa_community != NULL ?
		    tr->sa_community : snmpd_env->sc_trcommunity;

		/* SNMP header */
		root = ober_add_sequence(NULL);
		b = ober_printf_elements(root, "ds{tddd",
		    SNMP_V2, cmn, BER_CLASS_CONTEXT, SNMP_C_TRAPV2,
		    arc4random(), 0, 0);
		ober_link_elements(b, trap);

#ifdef DEBUG
		smi_debug_elements(root);
#endif
		len = ober_write_elements(&ber, root);
		if (ober_get_writebuf(&ber, (void *)&ptr) > 0 &&
		    sendto(s, ptr, len, 0, (struct sockaddr *)&tr->ss,
		    tr->ss.ss_len) != -1) {
			snmpd_env->sc_stats.snmp_outpkts++;
			ret++;
		}

		close(s);
		ober_unlink_elements(b);
		ober_free_elements(root);
	}

 done:
	ober_free_elements(trap);
	ober_free(&ber);

	return (ret);
}
