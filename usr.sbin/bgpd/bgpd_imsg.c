/*	$OpenBSD: bgpd_imsg.c,v 1.2 2026/02/04 13:49:23 claudio Exp $	*/
/*
 * Copyright (c) 2026 Claudio Jeker <claudio@openbsd.org>
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "bgpd.h"
#include "rde.h"
#include "log.h"

int
imsg_send_filterset(struct imsgbuf *imsgbuf, struct filter_set_head *set)
{
	struct filter_set	*s;
	struct ibuf		*msg;
	int			 nsets = 0;

	msg = imsg_create(imsgbuf, IMSG_FILTER_SET, 0, 0, 0);
	if (msg == NULL)
		return -1;

	TAILQ_FOREACH(s, set, entry)
		nsets++;
	if (ibuf_add_n16(msg, nsets) == -1)
		goto fail;

	TAILQ_FOREACH(s, set, entry) {
		if (ibuf_add_n32(msg, s->type) == -1)
			goto fail;

		switch (s->type) {
		case ACTION_SET_PREPEND_SELF:
		case ACTION_SET_PREPEND_PEER:
			if (ibuf_add_n8(msg, s->action.prepend) == -1)
				goto fail;
			break;
		case ACTION_SET_AS_OVERRIDE:
			break;
		case ACTION_SET_LOCALPREF:
		case ACTION_SET_MED:
		case ACTION_SET_WEIGHT:
			if (ibuf_add_n32(msg, s->action.metric) == -1)
				goto fail;
			break;
		case ACTION_SET_RELATIVE_LOCALPREF:
		case ACTION_SET_RELATIVE_MED:
		case ACTION_SET_RELATIVE_WEIGHT:
			if (ibuf_add_n32(msg, s->action.relative) == -1)
				goto fail;
			break;
		case ACTION_SET_NEXTHOP:
			if (ibuf_add(msg, &s->action.nexthop,
			    sizeof(s->action.nexthop)) == -1)
				goto fail;
			break;
		case ACTION_SET_NEXTHOP_BLACKHOLE:
		case ACTION_SET_NEXTHOP_REJECT:
		case ACTION_SET_NEXTHOP_NOMODIFY:
		case ACTION_SET_NEXTHOP_SELF:
			break;
		case ACTION_DEL_COMMUNITY:
		case ACTION_SET_COMMUNITY:
			if (ibuf_add(msg, &s->action.community,
			    sizeof(s->action.community)) == -1)
				goto fail;
			break;
		case ACTION_PFTABLE:
			if (ibuf_add_strbuf(msg, s->action.pftable,
			    sizeof(s->action.pftable)) == -1)
				goto fail;
			break;
		case ACTION_RTLABEL:
			if (ibuf_add_strbuf(msg, s->action.rtlabel,
			    sizeof(s->action.rtlabel)) == -1)
				goto fail;
			break;
		case ACTION_SET_ORIGIN:
			if (ibuf_add_n8(msg, s->action.origin) == -1)
				goto fail;
			break;
		}
	}

	imsg_close(imsgbuf, msg);
	return 0;

 fail:
	ibuf_free(msg);
	return -1;
}

int
ibuf_recv_filterset_count(struct ibuf *ibuf, uint16_t *count)
{
	return ibuf_get_n16(ibuf, count);
}

int
ibuf_recv_one_filterset(struct ibuf *ibuf, struct filter_set *set)
{
	uint32_t type, num;

	memset(set, 0, sizeof(*set));

	if (ibuf_get_n32(ibuf, &type) == -1)
		return -1;
	set->type = type;

	switch (set->type) {
	case ACTION_SET_PREPEND_SELF:
	case ACTION_SET_PREPEND_PEER:
		if (ibuf_get_n8(ibuf, &set->action.prepend) == -1)
			return -1;
		break;
	case ACTION_SET_AS_OVERRIDE:
		break;
	case ACTION_SET_LOCALPREF:
	case ACTION_SET_MED:
	case ACTION_SET_WEIGHT:
		if (ibuf_get_n32(ibuf, &set->action.metric) == -1)
			return -1;
		break;
	case ACTION_SET_RELATIVE_LOCALPREF:
	case ACTION_SET_RELATIVE_MED:
	case ACTION_SET_RELATIVE_WEIGHT:
		if (ibuf_get_n32(ibuf, &num) == -1)
			return -1;
		set->action.relative = num;
		break;
	case ACTION_SET_NEXTHOP:
		if (ibuf_get(ibuf, &set->action.nexthop,
		    sizeof(set->action.nexthop)) == -1)
			return -1;
		break;
	case ACTION_SET_NEXTHOP_BLACKHOLE:
	case ACTION_SET_NEXTHOP_REJECT:
	case ACTION_SET_NEXTHOP_NOMODIFY:
	case ACTION_SET_NEXTHOP_SELF:
		break;
	case ACTION_DEL_COMMUNITY:
	case ACTION_SET_COMMUNITY:
		if (ibuf_get(ibuf, &set->action.community,
		    sizeof(set->action.community)) == -1)
			return -1;
		break;
	case ACTION_PFTABLE:
		if (ibuf_get_strbuf(ibuf, set->action.pftable,
		    sizeof(set->action.pftable)) == -1)
			return -1;
		break;
	case ACTION_RTLABEL:
		if (ibuf_get_strbuf(ibuf, set->action.rtlabel,
		    sizeof(set->action.rtlabel)) == -1)
			return -1;
		break;
	case ACTION_SET_ORIGIN:
		if (ibuf_get_n8(ibuf, &set->action.origin) == -1)
			return -1;
		break;
	}
	return 0;
}

int
imsg_check_filterset(struct imsg *imsg)
{
	struct ibuf ibuf;
	uint16_t count, i;

	if (imsg_get_ibuf(imsg, &ibuf) == -1)
		return -1;
	if (ibuf_recv_filterset_count(&ibuf, &count) == -1)
		return -1;
	for (i = 0; i < count; i++) {
		struct filter_set set;
		if (ibuf_recv_one_filterset(&ibuf, &set) == -1)
			return -1;
	}
	if (ibuf_size(&ibuf) != 0) {
		errno = EBADMSG;
		return -1;
	}
	return 0;
}
