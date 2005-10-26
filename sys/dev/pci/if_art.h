/*	$OpenBSD: if_art.h,v 1.5 2005/10/26 09:26:56 claudio Exp $ */

/*
 * Copyright (c) 2005  Internet Business Solutions AG, Zurich, Switzerland
 * Written by: Claudio Jeker <jeker@accoom.net>
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

#ifndef __IF_ART_H__
#define __IF_ART_H__

#define	MUSYCC_FRAMER_BT8370	0x8370

enum art_sbi_type {
	ART_SBI_SINGLE,
	ART_SBI_MASTER,
	ART_SBI_SLAVE
};

struct art_softc {
	struct device		 art_dev;	/* generic device structures */
	struct ebus_dev		 art_ebus;	/* ebus attachement */
	struct ifmedia		 art_ifm;	/* interface media descriptor */
	struct timeout		 art_onesec;	/* onesec timeout */
	struct musycc_softc	*art_parent;	/* parent hdlc controller */
	struct channel_softc	*art_channel;	/* channel config */
	void			*art_linkstatehook;

	u_int			 art_media;	/* if_media media */
	enum art_sbi_type	 art_type;	/* System Bus Type */
	u_int8_t		 art_gnum;	/* group number */
	u_int8_t		 art_port;	/* port number */
	char			 art_slot;	/* TDM slot */
};

enum art_sbi_mode {
	SBI_MODE_1536 = 1,	/* 24TS */
	SBI_MODE_1544,		/* 24TS + F bit */
	SBI_MODE_2048,		/* 32TS */
	SBI_MODE_4096_A,	/* lower 32TS */
	SBI_MODE_4096_B,	/* upper 32TS */
	SBI_MODE_8192_A,	/* first 32TS */
	SBI_MODE_8192_B,	/* second 32TS */
	SBI_MODE_8192_C,	/* third 32TS */
	SBI_MODE_8192_D		/* last 32TS */
};

enum art_linecode {
	ART_LIU_AMI,		/* Alternate Mark Inversion */
	ART_LIU_B8ZS,		/* Bipolar 8-zero Substitution */
	ART_LIU_HDB3		/* High Density Bipolar 3 */
};

enum art_loopback {
	ART_NOLOOP,		/* All Loopback disabled */
	ART_RLOOP_PAYLOAD,	/* Remote Payload Loopback */
	ART_RLOOP_LINE,		/* Remote Line Loopback */
	ART_LLOOP_PAYLOAD,	/* Local Payload Loopback */
	ART_LLOOP_LINE		/* Local Line Loopback */
};

#define ART_DL1_BOP	1
#define ART_BOP_ESF	1

int	bt8370_reset(struct art_softc *);
int	bt8370_set_frame_mode(struct art_softc *, enum art_sbi_type, u_int,
	    u_int);
void	bt8370_intr_enable(struct art_softc *, int);
void	bt8370_intr(struct art_softc *);
int	bt8370_link_status(struct art_softc *);

#endif
