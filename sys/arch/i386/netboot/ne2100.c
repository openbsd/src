/*	$NetBSD: ne2100.c,v 1.3 1994/10/27 04:21:20 cgd Exp $	*/

/*
 * source in this file came from
 * the Mach ethernet boot written by Leendert van Doorn.
 *
 * A very simple network driver for NE2100 boards that polls.
 *
 * Copyright (c) 1992 by Leendert van Doorn
 */

#include "assert.h"
#include "nbtypes.h"
#include "packet.h"
#include "ether.h"
#include "lance.h"
#include "proto.h"

/* configurable parameters */
#define	NE_BASEREG	0x300		/* base register */
#define	NE_DMACHANNEL	5		/* DMA channel */

/* Lance register offsets */
#define LA_CSR          (NE_BASEREG+0x10)
#define LA_CSR1         (NE_BASEREG+0x10)
#define LA_CSR2         (NE_BASEREG+0x10)
#define LA_CSR3         (NE_BASEREG+0x10)
#define LA_RAP          (NE_BASEREG+0x12)

/*
 * Some driver specific constants.
 * Take care when tuning, this program only has 32 Kb
 */
#define	LANCEBUFSIZE	1518		/* plus 4 CRC bytes */
#define	MAXLOOP		1000000L	/* arbitrary retry limit */
#define	LOG2NRCVRING	2		/* log2(NRCVRING) */
#define	NRCVRING	(1 << LOG2NRCVRING)

u_char eth_myaddr[ETH_ADDRSIZE];

static int next_rmd;			/* next receive element */
static initblock_t *initblock;		/* initialization block */
static tmde_t *tmd;			/* transmit ring */
static rmde_t *rmd;			/* receive ring */
static char rbuffer[NRCVRING][LANCEBUFSIZE]; /* receive buffers */

static char *top = (char *)RAMSIZE;
static char last;

static char *
aalloc(size, align)
     int size, align;
{
  register char *p;
  register int mask;

  if (align == 0)
    align = sizeof(int);
  mask = align - 1;
  assert((align & mask) == 0);
  top = top - (size + align);
  p = (char *)((int)top & ~mask);
  assert(p > &last);
  assert(p <= (char *) RAMSIZE);
  return top = p;
}

/*
 * Program DMA channel 'chan' for cascade mode
 */
static void
dma_cascade(chan)
    int chan;
{
    assert(chan >= 0);
    assert(chan <= 7);
    if (chan >= 0 && chan <= 3) {
	outb(0x0B, 0xC0 | (chan & 03));
	outb(0x0A, chan & 03);
    } else {
	outb(0xD6, 0xC0 | ((chan - 4) & 03));
	outb(0xD4, (chan - 4) & 03);
    }
}

/*
 * Reset ethernet board (i.e. after a timeout)
 */
void
EtherReset(void) {
    long l;
    u_long addr;
    int i;

    /* program DMA chip */
    dma_cascade(NE_DMACHANNEL);

    /* stop the chip, and make sure it did */
    outw(LA_RAP, RDP_CSR0);
    outw(LA_CSR, CSR_STOP);
    for (l = 0; (inw(LA_CSR) & CSR_STOP) == 0; l++) {
	if (l >= MAXLOOP) {
	    printf("Lance failed to stop\n");
	    return;
	}
    }

    /* fill lance initialization block */
    bzero(initblock, sizeof(initblock_t));

    /* set my ethernet address */
    initblock->ib_padr[0] = eth_myaddr[0];
    initblock->ib_padr[1] = eth_myaddr[1];
    initblock->ib_padr[2] = eth_myaddr[2];
    initblock->ib_padr[3] = eth_myaddr[3];
    initblock->ib_padr[4] = eth_myaddr[4];
    initblock->ib_padr[5] = eth_myaddr[5];

    /* receive ring pointer */
    addr = LA(rmd);
    initblock->ib_rdralow = (u_short)addr;
    initblock->ib_rdrahigh = (u_char)(addr >> 16);
    initblock->ib_rlen = LOG2NRCVRING << 5;

    /* transmit ring with one element */
    addr = LA(tmd);
    initblock->ib_tdralow = (u_short)addr;
    initblock->ib_tdrahigh = (u_char)(addr >> 16);
    initblock->ib_tlen = 0 << 5;

    /* setup the receive ring entries */
    for (next_rmd = 0, i = 0; i < NRCVRING; i++) {
	addr = LA(&rbuffer[i]);
	rmd[i].rmd_ladr = (u_short)addr;
	rmd[i].rmd_hadr = (u_char)(addr >> 16);
	rmd[i].rmd_mcnt = 0;
	rmd[i].rmd_bcnt = -LANCEBUFSIZE;
	rmd[i].rmd_flags = RMD_OWN;
    }

    /* zero transmit ring */
    bzero(tmd, sizeof(tmde_t));

    /* give lance the init block */
    addr = LA(initblock);
    outw(LA_RAP, RDP_CSR1);
    outw(LA_CSR1, (u_short)addr);
    outw(LA_RAP, RDP_CSR2);
    outw(LA_CSR2, (char)(addr >> 16));
    outw(LA_RAP, RDP_CSR3);
    outw(LA_CSR3, 0);

    /* and initialize it */
    outw(LA_RAP, RDP_CSR0);
    outw(LA_CSR, CSR_INIT|CSR_STRT);

    /* wait for the lance to complete initialization and fire it up */
    for (l = 0; (inw(LA_CSR) & CSR_IDON) == 0; l++) {
	if (l >= MAXLOOP) {
	    printf("Lance failed to initialize\n");
	    break;
	}
    }
    for (l=0; (inw(LA_CSR)&(CSR_TXON|CSR_RXON))!=(CSR_TXON|CSR_RXON); l++) {
	if (l >= MAXLOOP) {
	    printf("Lance not started\n");
	    break;
	}
    }
}

/*
 * Get ethernet address and compute checksum to be sure
 * that there is a board at this address.
 */
int
EtherInit()
{
    u_short checksum, sum;
    int i;

    for (i = 0; i < 6; i++)
	eth_myaddr[i] = inb(NE_BASEREG + i);

    sum = 0;
    for (i = 0x00; i <= 0x0B; i++)
	sum += inb(NE_BASEREG + i);
    for (i = 0x0E; i <= 0xF; i++)
	sum += inb(NE_BASEREG + i);
    checksum = inb(NE_BASEREG + 0x0C) | (inb(NE_BASEREG + 0x0D) << 8);
    if (sum != checksum)
	return 0;

    /* initblock, tmd, and rmd should be 8 byte aligned ! */
    initblock = (initblock_t *) aalloc(sizeof(initblock_t), 8);
    tmd = (tmde_t *) aalloc(sizeof(tmde_t), 8);
    rmd = (rmde_t *) aalloc(NRCVRING * sizeof(rmde_t), 8);
    EtherReset();
    return 1;
}

/*
 * Disable DMA for channel 'chan'
 */
static void
dma_done(chan)
    int chan;
{
    assert(chan >= 0);
    assert(chan <= 7);
    if (chan >= 0 && chan <= 3)
	outb(0x0A, 0x04 | (chan & 03));
    else
	outb(0xD4, 0x04 | ((chan - 4 ) & 03));
}

/*
 * Stop ethernet board
 */
void
EtherStop()
{
    long l;

    /* stop chip and disable DMA access */
    outw(LA_RAP, RDP_CSR0);
    outw(LA_CSR, CSR_STOP);
    for (l = 0; (inw(LA_CSR) & CSR_STOP) == 0; l++) {
	if (l >= MAXLOOP) {
	    printf("Lance failed to stop\n");
	    break;
	}
    }
    dma_done(NE_DMACHANNEL);
}

/*
 * Send an ethernet packet to destination 'dest'
 */
void
EtherSend(pkt, proto, dest)
    packet_t *pkt;
    u_short proto;
    u_char *dest;
{
    ethhdr_t *ep;
    long l;
    u_long addr;
    u_short csr;

    /* add ethernet header and fill in source & destination */
    pkt->pkt_len += sizeof(ethhdr_t);
    pkt->pkt_offset -= sizeof(ethhdr_t);
    ep = (ethhdr_t *) pkt->pkt_offset;
    ep->eth_proto = htons(proto);
    bcopy(dest, ep->eth_dst, ETH_ADDRSIZE);
    bcopy(eth_myaddr, ep->eth_src, ETH_ADDRSIZE);
    if (pkt->pkt_len < 60)
	pkt->pkt_len = 60;
    assert(pkt->pkt_len <= 1514);

    /* set up transmit ring element */
    assert((tmd->tmd_flags & TMD_OWN) == 0);
    addr = LA(pkt->pkt_offset);
    assert((addr & 1) == 0);
    tmd->tmd_ladr = (u_short)addr;
    tmd->tmd_hadr = (u_char)(addr >> 16);
    tmd->tmd_bcnt = -pkt->pkt_len;
    tmd->tmd_err = 0;
    tmd->tmd_flags = TMD_OWN|TMD_STP|TMD_ENP;

    /* start transmission */
    outw(LA_CSR, CSR_TDMD);

    /* wait for interrupt and acknowledge it */
    for (l = 0; l < MAXLOOP; l++) {
	if ((csr = inw(LA_CSR)) & CSR_TINT) {
	    outw(LA_CSR, CSR_TINT);
	    break;
	}
    }
}

/*
 * Poll the LANCE just see if there's an Ethernet packet
 * available. If there is, its contents is returned in a
 * pkt structure, otherwise a nil pointer is returned.
 */
packet_t *
EtherReceive(void) {
    packet_t *pkt;
    rmde_t *rp;
    u_long addr;
    u_short csr;

    pkt = (packet_t *)0;
    if ((csr = inw(LA_CSR)) & CSR_RINT) {
	outw(LA_CSR, CSR_RINT);
	assert(next_rmd >= 0);
	assert(next_rmd <= NRCVRING);
	rp = &rmd[next_rmd];
	if ((rp->rmd_flags & ~RMD_OFLO) == (RMD_STP|RMD_ENP)) {
	    pkt = PktAlloc(0);
	    pkt->pkt_len = rp->rmd_mcnt - 4;
	    assert(pkt->pkt_len >= 0);
	    assert(pkt->pkt_len < PKT_DATASIZE);
	    bcopy(rbuffer[next_rmd], pkt->pkt_offset, pkt->pkt_len);
	    /* give packet back to the lance */
	    rp->rmd_bcnt = -LANCEBUFSIZE;
	    rp->rmd_mcnt = 0;
	    rp->rmd_flags = RMD_OWN;
	}
	next_rmd = (next_rmd + 1) & (NRCVRING - 1);
    }
    return pkt;
}

/*
 * Print an ethernet address in human readable form
 */
void
EtherPrintAddr(addr)
    u_char *addr;
{
    printf("%x:%x:%x:%x:%x:%x",
	addr[0] & 0xFF, addr[1] & 0xFF, addr[2] & 0xFF,
	addr[3] & 0xFF, addr[4] & 0xFF, addr[5] & 0xFF);
}
