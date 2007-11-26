/*	$OpenBSD: if_slvar.h,v 1.13 2007/11/26 09:28:33 martynas Exp $	*/
/*	$NetBSD: if_slvar.h,v 1.16 1996/05/07 02:40:46 thorpej Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)if_slvar.h	8.3 (Berkeley) 2/1/94
 */

#ifndef _NET_IF_SLVAR_H_
#define _NET_IF_SLVAR_H_

/*
 * Definitions for SLIP interface data structures
 * 
 * (This exists so programs like slstats can get at the definition
 *  of sl_softc.)
 */
struct sl_softc {
	struct	ifnet sc_if;		/* network-visible interface */
	int	sc_unit;		/* XXX unit number */
	struct	ifqueue sc_fastq;	/* interactive output queue */
	struct	tty *sc_ttyp;		/* pointer to tty structure */
	u_char	*sc_mp;			/* pointer to next available buf char */
	u_char	*sc_ep;			/* pointer to last available buf char */
	u_char	*sc_pktstart;		/* pointer to beginning of packet */
	struct mbuf *sc_mbuf;		/* input buffer */
	u_int	sc_flags;		/* see below */
	u_int	sc_escape;	/* =1 if last char input was FRAME_ESCAPE */
	long	sc_lasttime;		/* last time a char arrived */
	long	sc_abortcount;		/* number of abort esacpe chars */
	long	sc_starttime;		/* time of first abort in window */
	long	sc_oqlen;		/* previous output queue size */
	long	sc_otimeout;		/* number of times output's stalled */
#if defined(__NetBSD__) || defined(__OpenBSD__)
	int	sc_oldbufsize;		/* previous output buffer size */
	int	sc_oldbufquot;		/* previous output buffer quoting */
#endif
#ifdef INET				/* XXX */
	struct	slcompress sc_comp;	/* tcp compression data */
#endif
	caddr_t	sc_bpf;			/* BPF data */
	struct timeval sc_lastpacket;	/* for watchdog */
	LIST_ENTRY(sl_softc) sc_list;	/* all slip interfaces */
};

/*
 * Statistics.
 */
struct slstat	{
	u_int	sl_ibytes;	/* bytes received */
	u_int	sl_ipackets;	/* packets received */
	u_int	sl_obytes;	/* bytes sent */
	u_int	sl_opackets;	/* packets sent */
};

struct vjstat {
	u_int	vjs_packets;	/* outbound packets */
	u_int	vjs_compressed;	/* outbound compressed packets */
	u_int	vjs_searches;	/* searches for connection state */
	u_int	vjs_misses;	/* times couldn't find conn. state */
	u_int	vjs_uncompressedin; /* inbound uncompressed packets */
	u_int	vjs_compressedin;   /* inbound compressed packets */
	u_int	vjs_errorin;	/* inbound unknown type packets */
	u_int	vjs_tossed;	/* inbound packets tossed because of error */
};

struct sl_stats {
	struct slstat	sl;	/* basic PPP statistics */
	struct vjstat	vj;	/* VJ header compression statistics */
};

struct ifslstatsreq {
	char ifr_name[IFNAMSIZ];
	struct sl_stats stats;
};

/* internal flags */
#define	SC_ERROR	0x0001		/* had an input error */

/* visible flags */
#define	SC_COMPRESS	IFF_LINK0	/* compress TCP traffic */
#define	SC_NOICMP	IFF_LINK1	/* suppress ICMP traffic */
#define	SC_AUTOCOMP	IFF_LINK2	/* auto-enable TCP compression */

/*
 * These two are interface ioctls so that pppstats can do them on
 * a socket without having to open the serial device.
 */
#define SIOCGSLSTATS	_IOWR('i', 123, struct ifslstatsreq)

#ifdef _KERNEL
void	slattach(int);
void	slclose(struct tty *);
void	slinput(int, struct tty *);
int	slioctl(struct ifnet *, u_long, caddr_t);
int	slopen(dev_t, struct tty *);
int	sloutput(struct ifnet *,
	    struct mbuf *, struct sockaddr *, struct rtentry *);
void	slstart(struct tty *);
int	sltioctl(struct tty *, u_long, caddr_t, int);
#endif /* _KERNEL */
#endif /* _NET_IF_SLVAR_H_ */
