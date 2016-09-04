/*	$OpenBSD: bfd.h,v 1.2 2016/09/04 09:39:01 claudio Exp $	*/

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

struct bfd_msghdr {
	u_short rtm_msglen;     /* to skip over non-understood messages */ 
	u_char  rtm_version;    /* future binary compatibility */
	u_char  rtm_type;       /* message type */
	u_short rtm_hdrlen;     /* sizeof(rt_msghdr) to skip over the header */

	u_int16_t	mode;		/* */
	u_int32_t	minimum;	/* minimum time (us) to send */
	u_int32_t	rx;		/* minimum window (us) to receive */
	u_int16_t	multiplier;	/* Retry backoff multiplier */

};


struct bfd_softc {
	TAILQ_ENTRY(bfd_softc)	 bfd_next;
	struct socket		*sc_so;
	struct socket		*sc_soecho;
	struct socket		*sc_sosend;
	struct rtentry		*sc_rt;
	struct bfd_state	*sc_peer;
	struct task		 sc_bfd_task;
	struct task		 sc_bfd_send_task;
	struct timeout		 sc_timo_rx;
	struct timeout		 sc_timo_tx;
	int			 state;
	int			 mode;
	int			 error;
	int			 minrx;
	int			 mintx;
	int			 multiplier;
};

struct bfd_flags {
	int		 version;
};

int		 bfd_rtalloc(struct rtentry *);
void		 bfd_rtfree(struct rtentry *);
void		 bfdinit(void);
void		 bfddestroy(void);

#endif /* _NET_BFD_H_ */
