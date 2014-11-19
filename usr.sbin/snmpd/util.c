/*	$OpenBSD	*/
/*
 * Copyright (c) 2014 Bret Stephen Lambert <blambert@openbsd.org>
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>

#include <net/if.h>

#include <event.h>

#include "ber.h"
#include "snmp.h"
#include "snmpd.h"

/*
 * Convert variable bindings from AgentX to SNMP dialect.
 */
int
varbind_convert(struct agentx_pdu *pdu, struct agentx_varbind_hdr *vbhdr,
    struct ber_element **varbind, struct ber_element **iter)
{
	struct ber_oid			 oid;
	u_int32_t			 d;
	u_int64_t			 l;
	int				 slen;
	char				*str;
	struct ber_element		*a;
	int				 ret = AGENTX_ERR_NONE;

	if (snmp_agentx_read_oid(pdu, (struct snmp_oid *)&oid) == -1) {
		ret = AGENTX_ERR_PARSE_ERROR;
		goto done;
	}

	*iter = ber_add_sequence(*iter);
	if (*varbind == NULL)
		*varbind = *iter;

	a = ber_add_oid(*iter, &oid);

	switch (vbhdr->type) {
	case AGENTX_NO_SUCH_OBJECT:
	case AGENTX_NO_SUCH_INSTANCE:
	case AGENTX_END_OF_MIB_VIEW:
	case AGENTX_NULL:
		a = ber_add_null(a);
		break;

	case AGENTX_IP_ADDRESS:
	case AGENTX_OPAQUE:
	case AGENTX_OCTET_STRING:
		str = snmp_agentx_read_octetstr(pdu, &slen);
		if (str == NULL) {
			ret = AGENTX_ERR_PARSE_ERROR;
			goto done;
		}
		a = ber_add_nstring(a, str, slen);
		break;

	case AGENTX_OBJECT_IDENTIFIER:
		if (snmp_agentx_read_oid(pdu,
		    (struct snmp_oid *)&oid) == -1) {
			ret = AGENTX_ERR_PARSE_ERROR;
			goto done;
		}
		a = ber_add_oid(a, &oid);
		break;

	case AGENTX_INTEGER:
	case AGENTX_COUNTER32:
	case AGENTX_GAUGE32:
	case AGENTX_TIME_TICKS:
		if (snmp_agentx_read_int(pdu, &d) == -1) {
			ret = AGENTX_ERR_PARSE_ERROR;
			goto done;
		}
		a = ber_add_integer(a, d);
		break;

	case AGENTX_COUNTER64:
		if (snmp_agentx_read_int64(pdu, &l) == -1) {
			ret = AGENTX_ERR_PARSE_ERROR;
			goto done;
		}
		a = ber_add_integer(a, l);
		break;

	default:
		log_debug("unknown data type '%i'", vbhdr->type);
		ret = AGENTX_ERR_PARSE_ERROR;
		goto done;
	}

	/* AgentX types correspond to BER types */
	switch (vbhdr->type) {
	case BER_TYPE_INTEGER:
	case BER_TYPE_BITSTRING:
	case BER_TYPE_OCTETSTRING:
	case BER_TYPE_NULL:
	case BER_TYPE_OBJECT:
		/* universal types */
		break;

	/* Convert AgentX error types to SNMP error types */
	case AGENTX_NO_SUCH_OBJECT:
		ber_set_header(a, BER_CLASS_CONTEXT, 0);
		break;
	case AGENTX_NO_SUCH_INSTANCE:
		ber_set_header(a, BER_CLASS_CONTEXT, 1);
		break;

	case AGENTX_COUNTER64:
		ber_set_header(a, BER_CLASS_APPLICATION, SNMP_COUNTER64);
		break;

	default:
		/* application-specific types */
		ber_set_header(a, BER_CLASS_APPLICATION, vbhdr->type);
		break;
	}
 done:
	return (ret);
}
