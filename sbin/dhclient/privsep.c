/*	$OpenBSD: privsep.c,v 1.37 2014/10/27 13:36:21 krw Exp $ */

/*
 * Copyright (c) 2004 Henning Brauer <henning@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE, ABUSE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "dhcpd.h"
#include "privsep.h"

#include <sys/queue.h>
#include <sys/uio.h>

void
dispatch_imsg(struct imsgbuf *ibuf)
{
	struct imsg			 imsg;
	struct imsg_write_file		*wfimsg;
	ssize_t				 n;
	size_t				 len;

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			error("dispatch_imsg: imsg_get failure: %s",
			    strerror(errno));

		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_DELETE_ADDRESS:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct imsg_delete_address))
				warning("bad IMSG_DELETE_ADDRESS");
			else
				priv_delete_address(imsg.data);
			break;

		case IMSG_ADD_ADDRESS:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct imsg_add_address))
				warning("bad IMSG_ADD_ADDRESS");
			else
				priv_add_address(imsg.data);
			break;

		case IMSG_FLUSH_ROUTES:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct imsg_flush_routes))
				warning("bad IMSG_FLUSH_ROUTES");
			else
				priv_flush_routes(imsg.data);
			break;

		case IMSG_ADD_ROUTE:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct imsg_add_route))
				warning("bad IMSG_ADD_ROUTE");
			else
				priv_add_route(imsg.data);
			break;

		case IMSG_HUP:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct imsg_hup))
				warning("bad IMSG_HUP");
			else {
				ifi->flags |= IFI_HUP;
				quit = SIGHUP;
			}
			break;

		case IMSG_WRITE_FILE:
			if (imsg.hdr.len < IMSG_HEADER_SIZE +
			    sizeof(struct imsg_write_file))
				warning("short IMSG_WRITE_FILE");
			else {
				wfimsg = (struct imsg_write_file *)imsg.data;
				len = imsg.hdr.len;
				len -= IMSG_HEADER_SIZE;
				len -= sizeof(struct imsg_write_file);
				len -= wfimsg->len;
				if (len == 0)
					priv_write_file(wfimsg);
				else
					warning("bad IMSG_WRITE_FILE (%zu)",
					    len);
			}
			break;

		default:
			warning("received unknown message, code %u",
			    imsg.hdr.type);
		}

		imsg_free(&imsg);
	}
}
