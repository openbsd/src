/*	$OpenBSD: privsep.c,v 1.17 2012/10/30 18:39:44 krw Exp $ */

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

struct buf *
buf_open(size_t len)
{
	struct buf	*buf;

	if ((buf = calloc(1, sizeof(struct buf))) == NULL)
		error("buf_open: %m");
	if ((buf->buf = malloc(len)) == NULL) {
		free(buf);
		error("buf_open: %m");
	}
	buf->size = len;

	return (buf);
}

void
buf_add(struct buf *buf, void *data, size_t len)
{
	if (len == 0)
		return;

	if (buf->wpos + len > buf->size)
		error("buf_add: %m");

	memcpy(buf->buf + buf->wpos, data, len);
	buf->wpos += len;
}

void
buf_close(int sock, struct buf *buf)
{
	ssize_t	n;

	do {
		n = write(sock, buf->buf + buf->rpos, buf->size - buf->rpos);
		if (n == 0)			/* connection closed */
			error("buf_close (connection closed)");
		if (n != -1 && n < buf->size - buf->rpos)
			error("buf_close (short write): %m");

	} while (n == -1 && (errno == EAGAIN || errno == EINTR));

	if (n == -1)
		error("buf_close: %m");

	free(buf->buf);
	free(buf);
}

void
buf_read(int sock, void *buf, size_t nbytes)
{
	ssize_t	n;

	do {
		n = read(sock, buf, nbytes);
		if (n == 0) {			/* connection closed */
#ifdef DEBUG
			debug("buf_read (connection closed)");
#endif
			exit(1);
		}
		if (n != -1 && n < nbytes)
			error("buf_read (short read): %m");
	} while (n == -1 && (errno == EINTR || errno == EAGAIN));

	if (n == -1)
		error("buf_read: %m");
}

void
dispatch_imsg(int fd)
{
	struct imsg_hdr		 hdr;
	in_addr_t		*mask;
	char			*ifname, *contents;
	size_t			 totlen, len;
	struct iaddr		*addr, *gateway;
	int			 rdomain;

	buf_read(fd, &hdr, sizeof(hdr));

	switch (hdr.code) {
	case IMSG_DELETE_ADDRESS:
		totlen = sizeof(hdr);
		ifname = NULL;
		addr = NULL;
		if (hdr.len < totlen + sizeof(len))
			error("IMSG_DELETE_ADDRESS missing ifname length");
		buf_read(fd, &len, sizeof(len));
		totlen += sizeof(len);
		if (len == SIZE_T_MAX) {
			error("IMSG_DELETE_ADDRESS invalid ifname length");
		} else if (len > 0) {
			if (hdr.len < totlen + len)
				error("IMSG_DELETE_ADDRESS short ifname");
			if ((ifname = calloc(1, len + 1)) == NULL)
				error("%m");
			buf_read(fd, ifname, len);
			totlen += len;
		} else
			error("IMSG_DELETE_ADDRESS ifname missing");

		if (hdr.len < totlen + sizeof(len))
			error("IMSG_DELETE_ADDRESS missing rdomain length");
		buf_read(fd, &len, sizeof(len));
		totlen += sizeof(len);
		if (len == SIZE_T_MAX) {
			error("IMSG_DELETE_ADDRESS invalid rdomain length");
		} else if (len > 0) {
			if (hdr.len < totlen + len)
				error("IMSG_DELETE_ADDRESS short rdomain");
			buf_read(fd, &rdomain, len);
			totlen += len;
		} else
			error("IMSG_DELETE_ADDRESS rdomain missing");

		if (hdr.len < totlen + sizeof(len))
			error("IMSG_DELETE_ADDRESS missing addr length");
		buf_read(fd, &len, sizeof(len));
		totlen += sizeof(len);
		if (len == SIZE_T_MAX) {
			error("IMSG_DELETE_ADDRESS invalid addr");
		} else if (len == sizeof(*addr)) {
			if ((addr = calloc(1, len)) == NULL)
				error("%m");
			buf_read(fd, addr, len);
			totlen += len;
		} else {
			error("IMSG_DELETE_ADDRESS addr missing %zu", len);
		}

		priv_delete_old_address(ifname, rdomain, *addr);
		free(ifname);
		free(addr);
		break;

	case IMSG_ADD_ADDRESS:
		totlen = sizeof(hdr);
		ifname = NULL;
		addr = NULL;
		mask = NULL;
		if (hdr.len < totlen + sizeof(len))
			error("IMSG_ADD_ADDRESS missing ifname length");
		buf_read(fd, &len, sizeof(len));
		totlen += sizeof(len);
		if (len == SIZE_T_MAX) {
			error("IMSG_ADD_ADDRESS invalid ifname length");
		} else if (len > 0) {
			if (hdr.len < totlen + len)
				error("IMSG_ADD_ADDRESS short ifname");
			if ((ifname = calloc(1, len + 1)) == NULL)
				error("%m");
			buf_read(fd, ifname, len);
			totlen += len;
		} else
			error("IMSG_ADD_ADDRESS ifname missing");

		if (hdr.len < totlen + sizeof(len))
			error("IMSG_ADD_ADDRESS missing rdomain length");
		buf_read(fd, &len, sizeof(len));
		totlen += sizeof(len);
		if (len == SIZE_T_MAX) {
			error("IMSG_ADD_ADDRESS invalid rdomain length");
		} else if (len > 0) {
			if (hdr.len < totlen + len)
				error("IMSG_ADD_ADDRESS short rdomain");
			buf_read(fd, &rdomain, len);
			totlen += len;
		} else
			error("IMSG_ADD_ADDRESS rdomain missing");

		if (hdr.len < totlen + sizeof(len))
			error("IMSG_ADD_ADDRESS missing addr length");
		buf_read(fd, &len, sizeof(len));
		totlen += sizeof(len);
		if (len == SIZE_T_MAX) {
			error("IMSG_ADD_ADDRESS invalid addr");
		} else if (len == sizeof(*addr)) {
			if ((addr = calloc(1, len)) == NULL)
				error("%m");
			buf_read(fd, addr, len);
			totlen += len;
		} else {
			error("IMSG_ADD_ADDRESS addr missing %zu", len);
		}

		if (hdr.len < totlen + sizeof(len))
			error("IMSG_ADD_ADDRESS missing mask length");
		buf_read(fd, &len, sizeof(len));
		totlen += sizeof(len);
		mask = NULL;
		if (len == SIZE_T_MAX) {
			error("IMSG_ADD_ADDRESS invalid mask");
		} else if (len == sizeof(*mask)) {
			if ((mask = calloc(1, len)) == NULL)
				error("%m");
			buf_read(fd, mask, len);
			totlen += len;
		} else {
			error("IMSG_ADD_ADDRESS mask missing %zu", len);
		}

		priv_add_new_address(ifname, rdomain, *addr, *mask);
		free(ifname);
		free(addr);
		free(mask);
		break;

	case IMSG_FLUSH_ROUTES:
		totlen = sizeof(hdr);
		ifname = NULL;
		addr = NULL;
		if (hdr.len < totlen + sizeof(len))
			error("IMSG_FLUSH_ROUTES missing ifname length");
		buf_read(fd, &len, sizeof(len));
		totlen += sizeof(len);
		if (len == SIZE_T_MAX) {
			error("IMSG_FLUSH_ROUTES invalid ifname length");
		} else if (len > 0) {
			if (hdr.len < totlen + len)
				error("IMSG_FLUSH_ROUTES short ifname");
			if ((ifname = calloc(1, len + 1)) == NULL)
				error("%m");
			buf_read(fd, ifname, len);
			totlen += len;
		} else
			error("IMSG_FLUSH_ROUTES ifname missing");

		if (hdr.len < totlen + sizeof(len))
			error("IMSG_FLUSH_ROUTES missing rdomain length");
		buf_read(fd, &len, sizeof(len));
		totlen += sizeof(len);
		if (len == SIZE_T_MAX) {
			error("IMSG_FLUSH_ROUTES invalid rdomain length");
		} else if (len > 0) {
			if (hdr.len < totlen + len)
				error("IMSG_FLUSH_ROUTES short rdomain");
			buf_read(fd, &rdomain, len);
			totlen += len;
		} else
			error("IMSG_FLUSH_ROUTES rdomain missing");

		priv_flush_routes_and_arp_cache(ifname, rdomain);
		free(ifname);
		break;

	case IMSG_ADD_DEFAULT_ROUTE:
		totlen = sizeof(hdr);
		ifname = NULL;
		addr = NULL;
		if (hdr.len < totlen + sizeof(len))
			error("IMSG_ADD_DEFAULT_ROUTE missing ifname length");
		buf_read(fd, &len, sizeof(len));
		totlen += sizeof(len);
		if (len == SIZE_T_MAX) {
			error("IMSG_ADD_DEFAULT_ROUTE invalid ifname length");
		} else if (len > 0) {
			if (hdr.len < totlen + len)
				error("IMSG_ADD_DEFAULT_ROUTE short ifname");
			if ((ifname = calloc(1, len + 1)) == NULL)
				error("%m");
			buf_read(fd, ifname, len);
			totlen += len;
		} else
			error("IMSG_ADD_DEFAULT_ROUTE ifname missing");

		if (hdr.len < totlen + sizeof(len))
			error("IMSG_ADD_DEFAULT_ROUTE missing rdomain length");
		buf_read(fd, &len, sizeof(len));
		totlen += sizeof(len);
		if (len == SIZE_T_MAX) {
			error("IMSG_ADD_DEFAULT_ROUTE invalid rdomain length");
		} else if (len > 0) {
			if (hdr.len < totlen + len)
				error("IMSG_FLUSH_ROUTES short rdomain");
			buf_read(fd, &rdomain, len);
			totlen += len;
		} else
			error("IMSG_ADD_DEFAULT_ROUTE rdomain missing");

		if (hdr.len < totlen + sizeof(len))
			error("IMSG_ADD_DEFAULT_ROUTE missing addr length");
		buf_read(fd, &len, sizeof(len));
		totlen += sizeof(len);
		if (len == SIZE_T_MAX) {
			error("IMSG_ADD_DEFAULT_ROUTE invalid addr");
		} else if (len == sizeof(*addr)) {
			if ((addr = calloc(1, len)) == NULL)
				error("%m");
			buf_read(fd, addr, len);
			totlen += len;
		} else {
			error("IMSG_ADD_DEFAULT_ROUTE addr missing %zu",
			    len);
		}

		if (hdr.len < totlen + sizeof(len))
			error("IMSG_ADD_DEFAULT_ROUTE missing gateway length");
		buf_read(fd, &len, sizeof(len));
		totlen += sizeof(len);
		gateway = NULL;
		if (len == SIZE_T_MAX) {
			error("IMSG_ADD_DEFAULT_ROUTE invalid gateway");
		} else if (len == sizeof(*gateway)) {
			if ((gateway = calloc(1, len)) == NULL)
				error("%m");
			buf_read(fd, gateway, len);
			totlen += len;
		} else {
			error("IMSG_ADD_DEFAULT_ROUTE gateway missing %zu",
			    len);
		}

		priv_add_default_route(ifname, rdomain, *addr, *gateway);
		free(ifname);
		free(addr);
		free(gateway);
		break;
	case IMSG_NEW_RESOLV_CONF:
		totlen = sizeof(hdr);
		ifname = NULL;
		contents = NULL;
		if (hdr.len < totlen + sizeof(len))
			error("IMSG_NEW_RESOLV_CONF missing ifname length");
		buf_read(fd, &len, sizeof(len));
		totlen += sizeof(len);
		if (len == SIZE_T_MAX) {
			error("IMSG_NEW_RESOLV_CONF invalid ifname length");
		} else if (len > 0) {
			if (hdr.len < totlen + len)
				error("IMSG_NEW_RESOLV_CONF short ifname");
			if ((ifname = calloc(1, len + 1)) == NULL)
				error("%m");
			buf_read(fd, ifname, len);
			totlen += len;
		} else
			error("IMSG_NEW_RESOLV_CONF ifname missing");

		if (hdr.len < totlen + sizeof(len))
			error("IMSG_NEW_RESOLV_CONF missing contents length");
		buf_read(fd, &len, sizeof(len));
		totlen += sizeof(len);
		if (len == SIZE_T_MAX) {
			error("IMSG_NEW_RESOLV_CONF invalid contents length");
		} else if (len > 0) {
			if (hdr.len < totlen + len)
				error("IMSG_NEW_RESOLV_CONF short contents");
			if ((contents = calloc(1, len + 1)) == NULL)
				error("%m");
			buf_read(fd, contents, len);
			totlen += len;
		} else
			error("IMSG_NEW_RESOLV_CONF contents missing");

		priv_new_resolv_conf(ifname, contents);
		free(ifname);
		free(contents);
		break;
	default:
		error("received unknown message, code %d", hdr.code);
	}
}
