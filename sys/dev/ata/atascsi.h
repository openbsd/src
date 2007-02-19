/*	$OpenBSD: atascsi.h,v 1.2 2007/02/19 11:48:34 dlg Exp $ */

/*
 * Copyright (c) 2007 David Gwynne <dlg@openbsd.org>
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

struct atascsi;
struct ata_xfer;

struct atascsi_methods {
	int			(*probe)(void *, int);
	int			(*ata_cmd)(void *, struct ata_xfer *);
};

struct atascsi_attach_args {
	void			*aaa_cookie;

	struct atascsi_methods	*aaa_methods;
	void			(*aaa_minphys)(struct buf *);
	int			aaa_nports;
	int			aaa_ncmds;
};

struct atascsi_port {
	int			ap_type;
#define ATABUS_PORT_T_NONE		0
#define ATABUS_PORT_T_DISK		1
#define ATABUS_PORT_T_ATAPI		2
};

struct ata_xfer {
	int			port;

	int			flags;
#define ATA_F_POLL		(1<<0)
#define ATA_F_NOSLEEP		(1<<1)

	u_int8_t		*cmd;

	u_int8_t		*data;
	size_t			datalen;
};

struct atascsi	*atascsi_attach(struct device *, struct atascsi_attach_args *);
int		atascsi_detach(struct atascsi *);

int		atascsi_probe_dev(struct atascsi *, int);
int		atascsi_detach_dev(struct atascsi *, int);
