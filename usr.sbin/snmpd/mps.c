/*	$OpenBSD: mps.c,v 1.20 2015/01/16 00:05:13 deraadt Exp $	*/

/*
 * Copyright (c) 2007, 2008, 2012 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_arp.h>
#include <net/if_media.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
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

extern struct snmpd *env;

struct ber_oid *
	 mps_table(struct oid *, struct ber_oid *, struct ber_oid *);

extern void control_event_add(struct ctl_conn *, int, int, struct timeval *); /* XXX */

int
mps_getstr(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	char			*s = oid->o_data;

	if (s == NULL)
		return (-1);
	*elm = ber_add_string(*elm, s);
	return (0);
}

int
mps_setstr(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct ber_element	*ber = *elm;
	char			*s, *v;

	if ((oid->o_flags & OID_WR) == 0)
		return (-1);

	if (ber->be_class != BER_CLASS_UNIVERSAL ||
	    ber->be_type != BER_TYPE_OCTETSTRING)
		return (-1);
	if (ber_get_string(ber, &s) == -1)
		return (-1);
	if ((v = (void *)strdup(s)) == NULL)
		return (-1);
	if (oid->o_data != NULL)
		free(oid->o_data);
	oid->o_data = v;
	oid->o_val = strlen(v);

	return (0);
}

int
mps_getint(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	*elm = ber_add_integer(*elm, oid->o_val);
	return (0);
}

int
mps_setint(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	long long	 i;

	if (ber_get_integer(*elm, &i) == -1)
		return (-1);
	oid->o_val = i;

	return (0);
}

int
mps_getts(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	*elm = ber_add_integer(*elm, oid->o_val);
	ber_set_header(*elm, BER_CLASS_APPLICATION, SNMP_T_TIMETICKS);
	return (0);
}

int
mps_getreq(struct snmp_message *msg, struct ber_element *root,
    struct ber_oid *o, u_int sm_version)
{
	struct ber_element	*elm = root;
	struct oid		 key, *value;
	unsigned long		 error_type = 0;	/* noSuchObject */

	if (o->bo_n > BER_MAX_OID_LEN)
		return (-1);
	bzero(&key, sizeof(key));
	bcopy(o, &key.o_id, sizeof(struct ber_oid));
	smi_oidlen(&key.o_id);	/* Strip off any trailing .0. */
	value = smi_find(&key);
	if (value == NULL)
		goto fail;

	if (OID_NOTSET(value))
		return (-1);

	if (value->o_flags & OID_REGISTERED) {
		struct agentx_pdu	*pdu;

		if ((pdu = snmp_agentx_get_pdu((struct snmp_oid *)o, 1)) == NULL)
			return (-1);
		pdu->cookie = msg;
		if (snmp_agentx_send(value->o_session->handle, pdu) == -1)
			return (-1);

		control_event_add(value->o_session,
		    value->o_session->handle->fd, EV_WRITE, NULL);
		return (1);
	}

	if (value->o_get == NULL)
		goto fail;

	if (value->o_oidlen == o->bo_n) {
		/* No instance identifier specified. */
		error_type = 1;	/* noSuchInstance */
		goto fail;
	}

	if ((value->o_flags & OID_TABLE) == 0)
		elm = ber_add_oid(elm, o);
	if (value->o_get(value, o, &elm) != 0)
		return (-1);

	return (0);

fail:
	if (sm_version == 0)
		return (-1);

	/* Set SNMPv2 extended error response. */
	elm = ber_add_oid(elm, o);
	elm = ber_add_null(elm);
	ber_set_header(elm, BER_CLASS_CONTEXT, error_type);
	return (0);
}

int
mps_setreq(struct snmp_message *msg, struct ber_element *ber,
    struct ber_oid *o)
{
	struct oid		 key, *value;

	smi_oidlen(o);
	if (o->bo_n > BER_MAX_OID_LEN)
		return (-1);
	bzero(&key, sizeof(key));
	bcopy(o, &key.o_id, sizeof(struct ber_oid));
	value = smi_find(&key);
	if (value == NULL)
		return (-1);
	if ((value->o_flags & OID_WR) == 0 ||
	    value->o_set == NULL)
		return (-1);
	return (value->o_set(value, o, &ber));
}

int
mps_getnextreq(struct snmp_message *msg, struct ber_element *root,
    struct ber_oid *o)
{
	struct oid		*next = NULL;
	struct ber_element	*ber = root;
	struct oid		 key, *value;
	int			 ret;
	struct ber_oid		 no;

	if (o->bo_n > BER_MAX_OID_LEN)
		return (-1);
	bzero(&key, sizeof(key));
	bcopy(o, &key.o_id, sizeof(struct ber_oid));
	smi_oidlen(&key.o_id);	/* Strip off any trailing .0. */
	value = smi_find(&key);
	if (value == NULL)
		return (-1);

	if (value->o_flags & OID_REGISTERED) {
		struct agentx_pdu	*pdu;

		if ((pdu = snmp_agentx_getnext_pdu((struct snmp_oid *)o, 1)) == NULL)
			return (-1);
		pdu->cookie = msg;
		if (snmp_agentx_send(value->o_session->handle, pdu) == -1)
			return (-1);

		control_event_add(value->o_session,
		    value->o_session->handle->fd, EV_WRITE, NULL);
		return (1);
	}

	if (value->o_flags & OID_TABLE) {
		/* Get the next table row for this column */
		if (mps_table(value, o, &no) != NULL) {
			bcopy(&no, o, sizeof(*o));
			ret = value->o_get(value, o, &ber);
		} else
			ret = 1;
		switch (ret) {
		case 0:
			return (0);
		case -1:
			return (-1);
		case 1:	/* end-of-rows */
			break;
		}
	} else if (o->bo_n == value->o_oidlen && value->o_get != NULL) {
		next = value;
		goto appendzero;
	}

 getnext:
	for (next = value; next != NULL;) {
		next = smi_next(next);
		if (next == NULL)
			break;
		if (!OID_NOTSET(next) && next->o_get != NULL)
			break;
	}
	if (next == NULL || next->o_get == NULL)
		return (-1);

	if (next->o_flags & OID_TABLE) {
		/* Get the next table row for this column */
		if (mps_table(next, o, &no) == NULL) {
			value = next;
			goto getnext;
		}
		bcopy(&no, o, sizeof(*o));
		if ((ret = next->o_get(next, o, &ber)) != 0) {
			if (ret == 1) {
				value = next;
				goto getnext;
			}
			return (-1);
		}
	} else {
		bcopy(&next->o_id, o, sizeof(*o));
 appendzero:
		/* No instance identifier specified. Append .0. */
		if (o->bo_n + 1 > BER_MAX_OID_LEN)
			return (-1);
		ber = ber_add_noid(ber, o, ++o->bo_n);
		if ((ret = next->o_get(next, o, &ber)) != 0)
			return (-1);
	}

	return (0);
}

int
mps_getbulkreq(struct snmp_message *msg, struct ber_element **root,
    struct ber_oid *o, int max)
{
	struct ber_element *c, *d, *e;
	size_t len;
	int j, ret;

	j = max;
	c = *root;

	for (d = NULL, len = 0; j > 0; j--) {
		e = ber_add_sequence(NULL);
		if (c == NULL)
			c = e;
		ret = mps_getnextreq(msg, e, o);
		if (ret == 1)
			return (1);
		if (ret == -1) {
			ber_free_elements(e);
			if (d == NULL)
				return (-1);
			break;
		}
		len += ber_calc_len(e);
		if (len > SNMPD_MAXVARBINDLEN) {
			ber_free_elements(e);
			break;
		}
		if (d != NULL)
			ber_link_elements(d, e);
		d = e;
	}

	*root = c;
	return (0);
}

int
mps_set(struct ber_oid *o, void *p, long long len)
{
	struct oid		 key, *value;

	smi_oidlen(o);
	if (o->bo_n > BER_MAX_OID_LEN)
		return (-1);
	bzero(&key, sizeof(key));
	bcopy(o, &key.o_id, sizeof(struct ber_oid));
	value = smi_find(&key);
	if (value == NULL)
		return (-1);
	if (value->o_data != NULL)
		free(value->o_data);
	value->o_data = p;
	value->o_val = len;

	return (0);
}

struct ber_oid *
mps_table(struct oid *oid, struct ber_oid *o, struct ber_oid *no)
{
	u_int32_t		 col, idx = 1, id, subid;
	struct oid		 a, b;

	/*
	 * This function is being used to iterate through elements
	 * in a SMI "table". It is called by the mps_getnext() handler.
	 * For example, if the input is sysORIndex, it will return
	 * sysORIndex.1. If the input is sysORIndex.2, it will return
	 * sysORIndex.3 etc.. The MIB code has to verify the index,
	 * see mib_sysor() as an example.
	 */

	if (oid->o_table != NULL)
		return (oid->o_table(oid, o, no));

	bcopy(&oid->o_id, no, sizeof(*no));
	id = oid->o_oidlen - 1;
	subid = oid->o_oidlen;

	if (o->bo_n >= oid->o_oidlen) {
		/*
		 * Compare the requested and the matched OID to see
		 * if we have to iterate to the next element.
		 */
		bzero(&a, sizeof(a));
		bcopy(o, &a.o_id, sizeof(struct ber_oid));
		bzero(&b, sizeof(b));
		bcopy(&oid->o_id, &b.o_id, sizeof(struct ber_oid));
		b.o_oidlen--;
		b.o_flags |= OID_TABLE;
		if (smi_oid_cmp(&a, &b) == 0) {
			col = oid->o_oid[id];
			if (col > o->bo_id[id])
				idx = 1;
			else
				idx = o->bo_id[subid] + 1;
			o->bo_id[subid] = idx;
			o->bo_id[id] = col;
			bcopy(o, no, sizeof(*no));
		}
	}

	/* The root element ends with a 0, iterate to the first element */
	if (!no->bo_id[subid])
		no->bo_id[subid]++;

	smi_oidlen(no);

	return (no);
}

void
mps_encodeinaddr(struct ber_oid *o, struct in_addr *addr, int offset)
{
	u_int32_t	 a, i;

	o->bo_n = offset;
	if (addr != NULL) {
		a = htole32(addr->s_addr);
		o->bo_id[o->bo_n++] = a & 0xff;
		o->bo_id[o->bo_n++] = (a >> 8) & 0xff;
		o->bo_id[o->bo_n++] = (a >> 16) & 0xff;
		o->bo_id[o->bo_n++] = (a >> 24) & 0xff;
	} else {
		/* Create an invalid "last address" marker (5 bytes) */
		for (i = 0; i < 5; i++)
			o->bo_id[o->bo_n++] = 0xff;
	}
}

int
mps_decodeinaddr(struct ber_oid *o, struct in_addr *addr, int offset)
{
	u_int32_t	 a;

	a = ((o->bo_id[offset] & 0xff)) |
	    ((o->bo_id[offset + 1] & 0xff) << 8) |
	    ((o->bo_id[offset + 2] & 0xff) << 16) |
	    ((o->bo_id[offset + 3] & 0xff) << 24);
	addr->s_addr = letoh32(a);

	/* Detect invalid address */
	if ((o->bo_n - offset) > 4)
		return (-1);

	return (0);
}
