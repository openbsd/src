/*	$OpenBSD: bfd.h,v 1.8 2016/09/19 07:28:40 phessler Exp $	*/

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
	unsigned short	bm_msglen;
	unsigned char	bm_version;
	unsigned char	bm_type;
	unsigned short	bm_hdrlen;
	int		bm_addrs;
	/* above matches rtm_msghdr */

	uint16_t	bm_mode;
	uint32_t	bm_mintx;
	uint32_t	bm_minrx;
	uint16_t	bm_multiplier;

	time_t		bm_uptime;
	time_t		bm_lastuptime;
	int		bm_state;
	int		bm_remotestate;
	int		bm_laststate;
	int		bm_error;

	uint32_t	bm_localdiscr;
	uint32_t	bm_localdiag;
	uint32_t	bm_remotediscr;
	uint32_t	bm_remotediag;
};

#ifdef _KERNEL
/* state machine from RFC 5880 6.8.1*/
struct bfd_neighbor {
	uint32_t	bn_lstate;		/* SessionState */
	uint32_t	bn_rstate;		/* RemoteSessionState */
	uint32_t	bn_ldiscr;		/* LocalDiscr */
	uint32_t	bn_rdiscr;		/* RemoteDiscr */
	uint32_t	bn_ldiag;		/* LocalDiag */
	uint32_t	bn_rdiag;		/* RemoteDiag */
	uint32_t	bn_mintx;		/* DesiredMinTxInterval */
	uint32_t	bn_req_minrx;		/* RequiredMinRxInterval */
	uint32_t	bn_rminrx;		/* RemoteMinRxInterval */
	uint32_t	bn_demand;		/* DemandMode */
	uint32_t	bn_rdemand;		/* RemoteDemandMode */
	uint32_t	bn_mult;		/* DetectMult */
	uint32_t	bn_authtype;		/* AuthType */
	uint32_t	bn_rauthseq;		/* RcvAuthSeq */
	uint32_t	bn_lauthseq;		/* XmitAuthSeq */
	uint32_t	bn_authseqknown;	/* AuthSeqKnown */
};

struct bfd_config {
	TAILQ_ENTRY(bfd_config)	 bc_entry;
	struct socket		*bc_so;
	struct socket		*bc_soecho;
	struct socket		*bc_sosend;
	struct rtentry		*bc_rt;
	struct bfd_neighbor	*bc_neighbor;
	struct timeval		*bc_time;
	struct task		 bc_bfd_task;
	struct task		 bc_bfd_send_task;
	struct timeout		 bc_timo_rx;
	struct timeout		 bc_timo_tx;
	time_t			 bc_lastuptime;
	int			 bc_laststate;
	int			 bc_state;
	int			 bc_mode;
	int			 bc_poll;
	int			 bc_error;
	int			 bc_minrx;
	int			 bc_mintx;
	int			 bc_minecho;
	int			 bc_multiplier;
};

int		 bfdset(struct rtentry *);
void		 bfdclear(struct rtentry *);
void		 bfdinit(void);

#endif /* _KERNEL */

#endif /* _NET_BFD_H_ */
