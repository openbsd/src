/*	$NetBSD: if_stripvar.h,v 1.2 1996/05/19 22:09:45 jonathan Exp $	*/

/*
 * Definitions for SLIP interface data structures
 * 
 * (This exists so programs like slstats can get at the definition
 *  of sl_softc.)
 */
struct st_softc {
	struct	ifnet sc_if;		/* network-visible interface */
	int	sc_unit;		/* XXX unit number */
	struct	ifqueue sc_fastq;	/* interactive output queue */
	struct	tty *sc_ttyp;		/* pointer to tty structure */
	u_char	*sc_mp;			/* pointer to next available buf char */
	u_char	*sc_ep;			/* pointer to last available buf char */
	u_char	*sc_buf;		/* input buffer */
	u_char	*sc_rxbuf;		/* input destuffing buffer */
	u_char	*sc_txbuf;		/* output stuffing buffer */
	u_int	sc_flags;		/* see below */
	u_int	sc_escape;	/* =1 if last char input was FRAME_ESCAPE */
	long	sc_lasttime;		/* last time a char arrived */
	long	sc_abortcount;		/* number of abort esacpe chars */
	long	sc_starttime;		/* time of first abort in window */
	long	sc_oqlen;		/* previous output queue size */
	long	sc_otimeout;		/* number of times output's stalled */
#ifdef NetBSD
	int	sc_oldbufsize;		/* previous output buffer size */
	int	sc_oldbufquot;		/* previous output buffer quoting */
#endif
#ifdef INET				/* XXX */
	struct	slcompress sc_comp;	/* tcp compression data */
#endif
	caddr_t	sc_bpf;			/* BPF data */
};

/* internal flags */
#define	SC_ERROR	0x0001		/* had an input error */

/* visible flags */
#define	SC_COMPRESS	IFF_LINK0	/* compress TCP traffic */
#define	SC_NOICMP	IFF_LINK1	/* supress ICMP traffic */
#define	SC_AUTOCOMP	IFF_LINK2	/* auto-enable TCP compression */

#ifdef _KERNEL
void	stripattach __P((int n));
void	stripclose __P((struct tty *));
void	stripinput __P((int, struct tty *));
int	stripioctl __P((struct ifnet *, u_long, caddr_t));
int	stripopen __P((dev_t, struct tty *));
int	stripoutput __P((struct ifnet *,
	    struct mbuf *, struct sockaddr *, struct rtentry *));
void	stripstart __P((struct tty *));
int	striptioctl __P((struct tty *, u_long, caddr_t, int));
#endif /* _KERNEL */
