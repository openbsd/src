/*	$NetBSD: if_aevar.h,v 1.5 1997/02/28 08:56:07 scottr Exp $	*/

/*
 * Device driver for National Semiconductor DS8390/WD83C690 based ethernet
 * adapters.
 *
 * Copyright (c) 1994, 1995 Charles M. Hannum.  All rights reserved.
 *
 * Copyright (C) 1993, David Greenman.  This software may be used, modified,
 * copied, distributed, and sold, in both source and binary form provided that
 * the above copyright and these terms are retained.  Under no circumstances is
 * the author responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with its use.
 *
 * Adapted for MacBSD by Brad Parker <brad@fcr.com>.
 */

#define INTERFACE_NAME_LEN	32

/*
 * ae_softc: per line info and status
 */
struct ae_softc {
	struct device	sc_dev;
	bus_space_tag_t	sc_regt;	/* NIC register space tag */
	bus_space_handle_t sc_regh;	/* NIC register space handle */
	bus_space_tag_t	sc_buft;	/* Buffer space tag */
	bus_space_handle_t sc_bufh;	/* Buffer space handle */

/*	struct	intrhand sc_ih;	*/

	struct arpcom sc_arpcom;/* ethernet common */
	int	sc_flags;	/* interface flags, from config */

	char	type_str[INTERFACE_NAME_LEN];	/* type string */
	u_short	type;		/* interface type code */
	u_char	vendor;		/* interface vendor */
	u_char	regs_rev;	/* registers are reversed */
	u_char	use16bit;	/* use word-width transfers */

	u_char  cr_proto;	/* values always set in CR */

	int	mem_size;	/* total shared memory size */
	int	mem_ring;	/* start of RX ring-buffer (in smem) */

	u_char  txb_cnt;	/* Number of transmit buffers */
	u_char  txb_inuse;	/* number of transmit buffers active */

	u_char  txb_new;	/* pointer to where new buffer will be added */
	u_char  txb_next_tx;	/* pointer to next buffer ready to xmit */
	u_short txb_len[8];	/* buffered xmit buffer lengths */
	u_char  tx_page_start;	/* first page of TX buffer area */
	u_char  rec_page_start;	/* first page of RX ring-buffer */
	u_char  rec_page_stop;	/* last page of RX ring-buffer */
	u_char  next_packet;	/* pointer to next unread RX packet */
};

int	ae_size_card_memory __P((
	    bus_space_tag_t, bus_space_handle_t, int));

int	aesetup __P((struct ae_softc *));
void	aeintr __P((void *, int));
int	aeioctl __P((struct ifnet *, u_long, caddr_t));
void	aestart __P((struct ifnet *));
void	aewatchdog __P((struct ifnet *));
void	aereset __P((struct ae_softc *));
void	aeinit __P((struct ae_softc *));
void	aestop __P((struct ae_softc *));

void	aeread __P((struct ae_softc *, int, int));
struct mbuf *aeget __P((struct ae_softc *, int, int));

int	ae_put __P((struct ae_softc *, struct mbuf *, int));
void	ae_getmcaf __P((struct arpcom *, u_char *));
