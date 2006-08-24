/*	$OpenBSD: if_ze.c,v 1.5 2006/08/24 22:10:36 miod Exp $ */
/*	$NetBSD: if_ze.c,v 1.12 2002/05/27 16:54:18 ragge Exp $	*/
/*
 * Copyright (c) 1998 James R. Maynard III.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by James R. Maynard III.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	Standalone routine for the SGEC Ethernet controller.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/if_ether.h>

#include <lib/libsa/netif.h>
#include <lib/libsa/stand.h>
#include <lib/libsa/net.h>

#include <arch/vax/if/sgecreg.h>

#include "arch/vax/include/sid.h"
#include "arch/vax/include/rpb.h"

#include "vaxstand.h"

static int ze_get(struct iodesc *, void *, size_t, time_t);
static int ze_put(struct iodesc *, void *, size_t);

#define ETHER_MIN_LEN 64
#define ETHER_MAX_LEN 1518

struct netif_driver ze_driver = {
	0, 0, 0, 0, ze_get, ze_put,
};

#define NRCV 8				/* allocate 8 receive descriptors */
#define NXMT 4				/* and 4 transmit - must be >1 */
#define SETUP_FRAME_LEN 128		/* length of the setup frame */

/* allocate a buffer on an octaword boundary */
#define OW_ALLOC(x) ((void *)((int)((int)alloc((x) + 15) + 15) & ~15))

static	volatile struct zedevice *addr;

struct ze_tdes *ze_tdes_list;	/* transmit desc list */
struct ze_rdes *ze_rdes_list;	/* and receive desc list */
u_char ze_myaddr[ETHER_ADDR_LEN];	/* my Ethernet address */

int
zeopen(struct open_file *f, int adapt, int ctlr, int unit, int part)
{
	u_long nicsr0_work, *nisa_rom;
	struct ze_tdes *ze_setup_tdes_list;
	int i;

	/* point to the device in memory */
	if (askname == 0) /* Override if autoboot */
		addr = (struct zedevice *)bootrpb.csrphy;
	else
		addr = (struct zedevice *)0x20008000;

	/* reset the device and wait for completion */
	addr->ze_nicsr6 = ZE_NICSR6_MBO | ZE_NICSR6_RE;
	while ((addr->ze_nicsr5 & ZE_NICSR5_ID) == 0)
		;
	if (addr->ze_nicsr5 & ZE_NICSR5_SF) {
		printf("SGEC self-test failed...\n");
		return 1;
	}

	/* Get our Ethernet address */
	if (vax_boardtype == VAX_BTYP_49) {
		nisa_rom = (u_long *)0x27800000;
		for (i=0; i<ETHER_ADDR_LEN; i++)
			ze_myaddr[i] = nisa_rom[i] & 0377;
	} else if (vax_boardtype == VAX_BTYP_VXT) {
		nisa_rom = (u_long *)0x200c4000;
		for (i=0; i<ETHER_ADDR_LEN; i++)
			ze_myaddr[i] = nisa_rom[i] & 0xff;
	} else {
		nisa_rom = (u_long *)0x20084000;
		for (i=0; i<ETHER_ADDR_LEN; i++)
			if (vax_boardtype == VAX_BTYP_660)
				ze_myaddr[i] = (nisa_rom[i] & 0xff000000) >> 24;
			else
				ze_myaddr[i] = (nisa_rom[i] & 0x0000ff00) >> 8;
	}
	printf("SGEC: Ethernet address %s", ether_sprintf(ze_myaddr));

	/* initialize SGEC operating mode */
	/* disable interrupts here */
	nicsr0_work = ZE_NICSR0_IPL14 | ZE_NICSR0_SA | ZE_NICSR0_MBO |
		(ZE_NICSR0_IV_MASK & 0x0108);
	do {
		addr->ze_nicsr0 = nicsr0_work;
	} while (addr->ze_nicsr0 != nicsr0_work);
	if (addr->ze_nicsr5 & ZE_NICSR5_ME)
		addr->ze_nicsr5 |= ZE_NICSR5_ME;
	/* reenable interrupts here */

	/* Allocate space for descriptor lists and buffers, 
		then initialize them. Set up both lists as a ring. */
	ze_rdes_list = OW_ALLOC((NRCV+1) * sizeof(struct ze_rdes));
	ze_tdes_list = OW_ALLOC((NXMT+1) * sizeof(struct ze_tdes));
	for (i=0; i < NRCV; i++) {
		bzero(ze_rdes_list+i,sizeof(struct ze_rdes));
		ze_rdes_list[i].ze_framelen = ZE_FRAMELEN_OW;
		ze_rdes_list[i].ze_bufsize = ETHER_MAX_LEN;
		ze_rdes_list[i].ze_bufaddr = alloc(ETHER_MAX_LEN);
	}
	bzero(ze_rdes_list+NRCV,sizeof(struct ze_rdes));
	ze_rdes_list[NRCV].ze_framelen = ZE_FRAMELEN_OW;
	ze_rdes_list[NRCV].ze_rdes1 = ZE_RDES1_CA;
	ze_rdes_list[NRCV].ze_bufaddr = (u_char *)ze_rdes_list;
	for (i=0; i < NXMT; i++) {
		bzero(ze_tdes_list+i,sizeof(struct ze_tdes));
		ze_tdes_list[i].ze_tdes1 = ZE_TDES1_FS | ZE_TDES1_LS;
		ze_tdes_list[i].ze_bufsize = ETHER_MAX_LEN;
		ze_tdes_list[i].ze_bufaddr = alloc(ETHER_MAX_LEN);
	}
	bzero(ze_tdes_list+NXMT,sizeof(struct ze_tdes));
	ze_tdes_list[NXMT].ze_tdes1 = ZE_TDES1_CA;
	ze_tdes_list[NXMT].ze_tdr = ZE_TDR_OW;
	ze_tdes_list[NXMT].ze_bufaddr = (u_char *)ze_tdes_list;

	printf(".");	/* XXX VXT */

	/* Build setup frame. We set the SGEC to do a
		perfect filter on our own address. */
	ze_setup_tdes_list = OW_ALLOC(2*sizeof(struct ze_tdes));
	bzero(ze_setup_tdes_list+0,2*sizeof(struct ze_tdes));
	ze_setup_tdes_list[0].ze_tdr = ZE_TDR_OW;
	ze_setup_tdes_list[0].ze_tdes1 = ZE_TDES1_DT_SETUP;
	ze_setup_tdes_list[0].ze_bufsize = SETUP_FRAME_LEN;
	ze_setup_tdes_list[0].ze_bufaddr = alloc(SETUP_FRAME_LEN);
	bzero(ze_setup_tdes_list[0].ze_bufaddr,SETUP_FRAME_LEN);
	for (i=0; i < 16; i++)
		bcopy(ze_myaddr,ze_setup_tdes_list[0].ze_bufaddr+(8*i),
			ETHER_ADDR_LEN);
	ze_setup_tdes_list[1].ze_tdes1 = ZE_TDES1_CA;
	ze_setup_tdes_list[1].ze_bufaddr = (u_char *)ze_setup_tdes_list;

	printf(".");	/* XXX VXT */

	/* Start the transmitter and initialize almost everything else. */
	addr->ze_nicsr4 = ze_setup_tdes_list;
	addr->ze_nicsr6 = ZE_NICSR6_MBO | ZE_NICSR6_SE | ZE_NICSR6_ST |
		ZE_NICSR6_DC | ZE_NICSR6_BL_4;
	while ((addr->ze_nicsr5 & ZE_NICSR5_TS) != ZE_NICSR5_TS_SUSP)
		;	/* wait for the frame to be processed */

	printf(".");	/* XXX VXT */

	/* Setup frame is done processing, initialize the receiver and
		point the transmitter to the real tdes list. */
	addr->ze_nicsr4 = ze_tdes_list;
	addr->ze_nicsr3 = ze_rdes_list;
	addr->ze_nicsr6 |= ZE_NICSR6_SR;

	/* And away-y-y we go! */

	printf("\n");
	net_devinit(f, &ze_driver, ze_myaddr);
	return 0;
}

int
ze_get(desc, pkt, maxlen, timeout)
	struct iodesc *desc;
	void *pkt;
	size_t maxlen;
	time_t timeout;
{
	int timeout_ctr=100000*timeout, len, rdes;

	while (timeout_ctr-- > 0) {

	/* If there's not a packet waiting for us, just decrement the
		timeout counter. */
		if (!(addr->ze_nicsr5 & ZE_NICSR5_RI))
			continue;

	/* Look through the receive descriptor list for the packet. */
		for (rdes=0; rdes<NRCV; rdes++) {
			if (ze_rdes_list[rdes].ze_framelen & ZE_FRAMELEN_OW)
				continue;

	/* If the packet has an error, ignore it. */
			if (ze_rdes_list[rdes].ze_rdes0 & ZE_RDES0_ES)
				len = 0;

	/* Copy the packet, up to the length supplied by the caller, to
		the caller's buffer. */
			else {
				if ((len = (ze_rdes_list[rdes].ze_framelen &
					(~ ZE_FRAMELEN_OW))) > maxlen)
					len = maxlen;
				bcopy((void *)ze_rdes_list[rdes].ze_bufaddr,
					pkt,len);
			}

	/* Give ownership of this descriptor back to the SGEC. */
			ze_rdes_list[rdes].ze_framelen = ZE_FRAMELEN_OW;

	/* If we actually got a good packet, reset the error flags and
		tell the SGEC to look for more before returning. */
			if (len > 0) {
				addr->ze_nicsr5=ZE_NICSR5_RU | ZE_NICSR5_RI |
					ZE_NICSR5_IS;
				addr->ze_nicsr2=ZE_NICSR2_RXPD;
				return len;
			}
		}
	}

	/* If we're going to return an error indication, at least reset the
		error flags and tell the SGEC to keep receiving first. */
	addr->ze_nicsr5=ZE_NICSR5_RU | ZE_NICSR5_RI | ZE_NICSR5_IS;
	addr->ze_nicsr2=ZE_NICSR2_RXPD;
	return 0;
}

int
ze_put(desc, pkt, len)
	struct iodesc *desc;
	void *pkt;
	size_t len;
{
	int timeout=100000;

	/* The SGEC maintains its position in the transmit descriptor list
	for the next frame to transmit. Unfortunately, there's no way to tell
	from software just where that is. We're forced to reset the position
	whenever we send a frame, which requires waiting for the previous
	frame to be sent. Argh. */
	while ((addr->ze_nicsr5 & ZE_NICSR5_TS) == ZE_NICSR5_TS_RUN)
		;

	/* Copy the packet to the buffer we allocated. */
	bcopy(pkt, (void *)ze_tdes_list[0].ze_bufaddr, len);

	/* Set the packet length in the descriptor, increasing it to the
		minimum size if needed. */
	ze_tdes_list[0].ze_bufsize = len;
	if (len < ETHER_MIN_LEN)
		ze_tdes_list[0].ze_bufsize = ETHER_MIN_LEN;

	/* Give ownership of the descriptor to the SGEC and tell it to start
		transmitting. */
	ze_tdes_list[0].ze_tdr = ZE_TDR_OW;
	addr->ze_nicsr4 = ze_tdes_list;
	addr->ze_nicsr1 = ZE_NICSR1_TXPD;

	/* Wait for the frame to be sent, but not too long. */
	timeout = 100000;
	while (((addr->ze_nicsr5 & ZE_NICSR5_TI) == 0) && (--timeout>0))
		;

	/* Reset the transmitter interrupt pending flag. */
	addr->ze_nicsr5 |= ZE_NICSR5_TI;

	/* Return good if we didn't timeout, or error if we did. */
	if (timeout>0) return len;
	return -1;
}

int
zeclose(struct open_file *f)
{
	addr->ze_nicsr6 = ZE_NICSR6_RE;

	return 0;
}
