/*	$NetBSD: if_aevar.h,v 1.7 1997/03/19 08:04:40 scottr Exp $	*/
/*	$OpenBSD: if_aevar.h,v 1.4 2004/11/26 21:21:24 miod Exp $	*/

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

	bus_size_t	sc_reg_map[16];	/* register map (offsets) */

/*	struct	intrhand sc_ih;	*/

	struct arpcom sc_arpcom;/* ethernet common */
	int	sc_flags;	/* interface flags, from config */

	char	type_str[INTERFACE_NAME_LEN];	/* type string */
	u_int	type;		/* interface type code */
	u_int	vendor;		/* interface vendor */
	u_int	use16bit;	/* use word-width transfers */
	u_int8_t cr_proto;	/* values always set in CR */

	int	mem_size;	/* total shared memory size */
	int	mem_ring;	/* start of RX ring-buffer (in smem) */

	u_short	txb_cnt;	/* Number of transmit buffers */
	u_short	txb_inuse;	/* Number of transmit buffers active */
	u_short	txb_new;	/* pointer to where new buffer will be added */
	u_short	txb_next_tx;	/* pointer to next buffer ready to xmit */
	u_short	txb_len[8];	/* buffered xmit buffer lengths */
	u_short	tx_page_start;	/* first page of TX buffer area */
	u_short	rec_page_start;	/* first page of RX ring-buffer */
	u_short	rec_page_stop;	/* last page of RX ring-buffer */
	u_short	next_packet;	/* pointer to next unread RX packet */
};

int	ae_size_card_memory(
	    bus_space_tag_t, bus_space_handle_t, int);

int	aesetup(struct ae_softc *);
int	aeintr(void *);
int	aeioctl(struct ifnet *, u_long, caddr_t);
void	aestart(struct ifnet *);
void	aewatchdog(struct ifnet *);
void	aereset(struct ae_softc *);
void	aeinit(struct ae_softc *);
void	aestop(struct ae_softc *);

void	aeread(struct ae_softc *, int, int);
struct mbuf *aeget(struct ae_softc *, int, int);

int	ae_put(struct ae_softc *, struct mbuf *, int);
void	ae_getmcaf(struct arpcom *, u_char *);
