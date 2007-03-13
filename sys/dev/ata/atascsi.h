/*	$OpenBSD: atascsi.h,v 1.7 2007/03/13 11:20:57 dlg Exp $ */

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

struct ata_port {
	struct atascsi		*ap_as;
	int			ap_port;
	int			ap_type;
#define ATA_PORT_T_NONE			0
#define ATA_PORT_T_DISK			1
#define ATA_PORT_T_ATAPI		2
};

struct ata_cmd {
	u_int8_t		command;
	u_int8_t		head;
	u_int16_t		cyl;
	u_int8_t		sector;
	u_int8_t		count;
	u_int8_t		features;
	u_int8_t		st_bmask;
	u_int8_t		st_pmask;
	u_int8_t		error;
};

struct ata_xfer {
	struct ata_cmd		cmd;
	u_int8_t		*data;
	size_t			datalen;

	void			(*complete)(struct ata_xfer *);
	struct timeout		stimeout;
	u_int			timeout;

	struct ata_port		*port;
	int			flags;
#define ATA_F_READ			(1<<0)
#define ATA_F_WRITE			(1<<1)
#define ATA_F_NOWAIT			(1<<2)
#define ATA_F_POLL			(1<<3)
	volatile int		state;
#define ATA_S_SETUP			0
#define ATA_S_PENDING			1
#define ATA_S_COMPLETE			2
#define ATA_S_ERROR			3

	void			*atascsi_private;
};

#define ATA_QUEUED		0
#define ATA_COMPLETE		1
#define ATA_ERROR		2

struct atascsi	*atascsi_attach(struct device *, struct atascsi_attach_args *);
int		atascsi_detach(struct atascsi *);

int		atascsi_probe_dev(struct atascsi *, int);
int		atascsi_detach_dev(struct atascsi *, int);
