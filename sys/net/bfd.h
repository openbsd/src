/*	$OpenBSD: bfd.h,v 1.6 2016/09/15 13:09:44 phessler Exp $	*/

/*
 * Copyright (c) 2016 Peter Hessler <phessler@openbsd.org>
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

/*
 * Support for Bi-directional Forwarding Detection (RFC 5880 / 5881)
 */

#ifndef _NET_BFD_H_
#define _NET_BFD_H_

/* Public Interface */

#define BFD_MODE_ASYNC			1
#define BFD_MODE_DEMAND			2

/* Diagnostic Code (RFC 5880 Page 8) */
#define BFD_DIAG_NONE			0
#define BFD_DIAG_EXPIRED		1
#define BFD_DIAG_ECHO_FAILED		2
#define BFD_DIAG_NEIGHBOR_SIGDOWN	3
#define BFD_DIAG_FIB_RESET		4
#define BFD_DIAG_PATH_DOWN		5
#define BFD_DIAG_CONCAT_PATH_DOWN	6
#define BFD_DIAG_ADMIN_DOWN		7
#define BFD_DIAG_CONCAT_REVERSE_DOWN	8

/* State (RFC 5880 Page 8) */
#define BFD_STATE_ADMINDOWN		0
#define BFD_STATE_DOWN			1
#define BFD_STATE_INIT			2
#define BFD_STATE_UP			3

/* Flags (RFC 5880 Page 8) */
#define BFD_FLAG_P			0x20
#define BFD_FLAG_F			0x10
#define BFD_FLAG_C			0x08
#define BFD_FLAG_A			0x04
#define BFD_FLAG_D			0x02
#define BFD_FLAG_M			0x01

struct bfd_msghdr {
	unsigned short rtm_msglen;     /* to skip over non-understood msgs */
	unsigned char  rtm_version;    /* future binary compatibility */
	unsigned char  rtm_type;       /* message type */
	unsigned short rtm_hdrlen;     /* to skip over the header */

	uint16_t	mode;		/* */
	uint32_t	mintx;		/* minimum time (us) to send */
	uint32_t	minrx;		/* minimum window (us) to receive */
	uint16_t	multiplier;	/* Retry backoff multiplier */

	time_t		uptime;
	time_t		lastuptime;
	int		state;
	int		laststate;
	int		error;

	uint32_t	localdiscr;
	uint32_t	localdiag;
	uint32_t	remotediscr;
	uint32_t	remotediag;
};

#ifdef _KERNEL
struct bfd_softc {
	TAILQ_ENTRY(bfd_softc)	 bfd_next;
	struct socket		*sc_so;
	struct socket		*sc_soecho;
	struct socket		*sc_sosend;
	struct rtentry		*sc_rt;
	struct bfd_state	*sc_peer;
	struct timeval		*sc_time;
	struct task		 sc_bfd_task;
	struct task		 sc_bfd_send_task;
	struct timeout		 sc_timo_rx;
	struct timeout		 sc_timo_tx;
	time_t			 lastuptime;
	int			 laststate;
	int			 state;
	int			 mode;
	int			 error;
	int			 minrx;
	int			 mintx;
	int			 multiplier;
};
#endif /* _KERNEL */

int		 bfd_rtalloc(struct rtentry *);
void		 bfd_rtfree(struct rtentry *);
void		 bfdinit(void);
void		 bfddestroy(void);

struct bfd_softc *bfd_lookup(struct rtentry *);

#endif /* _NET_BFD_H_ */
