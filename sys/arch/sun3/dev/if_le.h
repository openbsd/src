/*	$NetBSD: if_le.h,v 1.6 1995/01/03 15:43:38 gwr Exp $	*/

/*
 * Ethernet software status per interface.
 *
 * Each interface is referenced by a network interface structure,
 * arpcom, which the routing code uses to locate the interface.
 * This structure contains the output queue for the interface,
 * its address, ...
 */
struct	le_softc {
	struct	device sc_dev;		/* base device */
	struct	arpcom sc_ac;		/* common Ethernet structures */
#define	sc_if   	sc_ac.ac_if		/* network-visible interface */
#define	sc_enaddr	sc_ac.ac_enaddr		/* hardware Ethernet address */

	volatile struct	le_regs *sc_regs;	/* LANCE registers */
	void *sc_mem;		/* Shared RAM */

	volatile struct	init_block *sc_init;	/* Lance init. block */
	volatile struct	mds *sc_rd, *sc_td;
	u_char	*sc_rbuf, *sc_tbuf;
	int	sc_last_rd, sc_last_td;
	int	sc_no_td;
#ifdef LEDEBUG
	int	sc_debug;
#endif
};
