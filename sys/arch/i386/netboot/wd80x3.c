/*	$NetBSD: wd80x3.c,v 1.4 1994/10/27 04:21:28 cgd Exp $	*/

/*
 * source in this file came from
 * the Mach ethernet boot written by Leendert van Doorn.
 *
 * A very simple network driver for WD80x3 boards that polls.
 *
 * Copyright (c) 1992 by Leendert van Doorn
 */

#include "proto.h"
#include "assert.h"
#include "packet.h"
#include "ether.h"
#include "dp8390.h"

/* configurable parameters */
#define	WD_BASEREG	0x280		/* base register */
/* the base address doesn't have to be particularly accurate - the
   board seems to pick up on addresses in the range a0000..effff.
   */
#define	WD_BASEMEM	0xd0000		/* base ram */

/* bit definitions for board features */
#define	INTERFACE_CHIP	01		/* has an WD83C583 interface chip */
#define	BOARD_16BIT	02		/* 16 bit board */
#define	SLOT_16BIT	04		/* 16 bit slot */

/* register offset definitions */
#define	WD_MSR		0x00		/* control (w) and status (r) */
#define WD_REG0		0x00		/* generic register definitions */
#define WD_REG1		0x01
#define WD_REG2		0x02
#define WD_REG3		0x03
#define WD_REG4		0x04
#define WD_REG5		0x05
#define WD_REG6		0x06
#define WD_REG7		0x07
#define	WD_EA0		0x08		/* most significant addr byte */
#define	WD_EA1		0x09
#define	WD_EA2		0x0A
#define	WD_EA3		0x0B
#define	WD_EA4		0x0C
#define	WD_EA5		0x0D		/* least significant addr byte */
#define	WD_LTB		0x0E		/* LAN type byte */
#define	WD_CHKSUM	0x0F		/* sum from WD_EA0 upto here is 0xFF */
#define	WD_DP8390	0x10		/* natsemi chip */

/* bits in control register */
#define	WD_MSR_MEMMASK	0x3F		/* memory enable bits mask */
#define	WD_MSR_MENABLE	0x40		/* memory enable */
#define	WD_MSR_RESET	0x80		/* software reset */

/* bits in bus size register */
#define	WD_BSR_16BIT	0x01		/* 16 bit bus */

/* bits in LA address register */
#define	WD_LAAR_A19	0x01		/* address lines for above 1Mb ram */
#define	WD_LAAR_LAN16E	0x40		/* enables 16bit shrd RAM for LAN */
#define	WD_LAAR_MEM16E	0x80		/* enables 16bit shrd RAM for host */

u_char eth_myaddr[ETH_ADDRSIZE];

static int boardid;
static dpconf_t dpc;

/*
 * Determine whether wd8003 hardware performs register aliasing
 * (i.e. whether it is an old WD8003E board).
 */
static int
Aliasing(void) {
  if (inb(WD_BASEREG + WD_REG1) != inb(WD_BASEREG + WD_EA1))
    return 0;
  if (inb(WD_BASEREG + WD_REG2) != inb(WD_BASEREG + WD_EA2))
    return 0;
  if (inb(WD_BASEREG + WD_REG3) != inb(WD_BASEREG + WD_EA3))
    return 0;
  if (inb(WD_BASEREG + WD_REG4) != inb(WD_BASEREG + WD_EA4))
    return 0;
  if (inb(WD_BASEREG + WD_REG7) != inb(WD_BASEREG + WD_CHKSUM))
    return 0;
  return 1;
}

/*
 * This trick is stolen from the clarkson packet driver
TBD - this is _ugly_ bogus! should use system timer
 */
static void
LongPause(void) {
  short i;
  for (i = 1600; i > 0; i++)
    (void) inb(0x61);
}

/*
 * Determine whether this board has 16-bit capabilities
 */
static int
BoardIs16Bit(void) {
  u_char bsreg = inb(WD_BASEREG + WD_REG1);

  outb(WD_BASEREG + WD_REG1, bsreg ^ WD_BSR_16BIT);
  LongPause();
  if (inb(WD_BASEREG + WD_REG1) == bsreg) {
    /*
     * Pure magic: LTB is 0x05 indicates that this is a WD8013EB board,
     * 0x27 indicates that this is an WD8013 Elite board, and 0x29
     * indicates an SMC Elite 16 board.
     */
    u_char tlb = inb(WD_BASEREG + WD_LTB);
    return tlb == 0x05 || tlb == 0x27 || tlb == 0x29;
  }
  outb(WD_BASEREG + WD_REG1, bsreg);
  return 1;
}

/*
 * Determine whether the 16 bit capable board is plugged
 * into a 16 bit slot.
 */
static int
SlotIs16Bit(void) {
  return inb(WD_BASEREG + WD_REG1) & WD_BSR_16BIT;
}

/*
 * Reset ethernet board after a timeout
 */
void
EtherReset(void) {
  int dpreg = dpc.dc_reg;
  /* initialize the board */
  outb(WD_BASEREG + WD_MSR,
       WD_MSR_MENABLE | (((u_long)WD_BASEMEM >> 13) & WD_MSR_MEMMASK));

  /* reset dp8390 ethernet chip */
  outb(dpreg + DP_CR, CR_STP|CR_DM_ABORT);

  /* initialize first register set */
  outb(dpreg + DP_IMR, 0);
  outb(dpreg + DP_CR, CR_PS_P0|CR_STP|CR_DM_ABORT);
  outb(dpreg + DP_TPSR, dpc.dc_tpsr);
  outb(dpreg + DP_PSTART, dpc.dc_pstart);
  outb(dpreg + DP_PSTOP, dpc.dc_pstop);
  outb(dpreg + DP_BNRY, dpc.dc_pstart);
  outb(dpreg + DP_RCR, RCR_MON);
  outb(dpreg + DP_TCR, TCR_NORMAL|TCR_OFST);
  if (boardid & SLOT_16BIT)
    outb(dpreg + DP_DCR, DCR_WORDWIDE|DCR_8BYTES);
  else
    outb(dpreg + DP_DCR, DCR_BYTEWIDE|DCR_8BYTES);
  outb(dpreg + DP_RBCR0, 0);
  outb(dpreg + DP_RBCR1, 0);
  outb(dpreg + DP_ISR, 0xFF);

  /* initialize second register set */
  outb(dpreg + DP_CR, CR_PS_P1|CR_DM_ABORT);
  outb(dpreg + DP_PAR0, eth_myaddr[0]);
  outb(dpreg + DP_PAR1, eth_myaddr[1]);
  outb(dpreg + DP_PAR2, eth_myaddr[2]);
  outb(dpreg + DP_PAR3, eth_myaddr[3]);
  outb(dpreg + DP_PAR4, eth_myaddr[4]);
  outb(dpreg + DP_PAR5, eth_myaddr[5]);
  outb(dpreg + DP_CURR, dpc.dc_pstart+1);

  /* and back to first register set */
  outb(dpreg + DP_CR, CR_PS_P0|CR_DM_ABORT);
  outb(dpreg + DP_RCR, RCR_AB);

  /* flush counters */
  (void) inb(dpreg + DP_CNTR0);
  (void) inb(dpreg + DP_CNTR1);
  (void) inb(dpreg + DP_CNTR2);

  /* and go ... */
  outb(dpreg + DP_CR, CR_STA|CR_DM_ABORT);
}

/*
 * Initialize the WD80X3 board
 */
int
EtherInit(void) {
  unsigned sum;
  int memsize;
  /* reset the ethernet card */
  outb(WD_BASEREG + WD_MSR, WD_MSR_RESET);
  LongPause();
  outb(WD_BASEREG + WD_MSR, 0);

  /* determine whether the controller is there */
  sum = inb(WD_BASEREG + WD_EA0) + inb(WD_BASEREG + WD_EA1) +
    inb(WD_BASEREG + WD_EA2) + inb(WD_BASEREG + WD_EA3) +
      inb(WD_BASEREG + WD_EA4) + inb(WD_BASEREG + WD_EA5) +
	inb(WD_BASEREG + WD_LTB) + inb(WD_BASEREG + WD_CHKSUM);
  if ((sum & 0xFF) != 0xFF)
    return 0;

  /*
   * Determine the type of board
   */
  boardid = 0;
  if (!Aliasing()) {
    if (BoardIs16Bit()) {
      boardid |= BOARD_16BIT;
      if (SlotIs16Bit())
	boardid |= SLOT_16BIT;
    }
  }
  memsize = (boardid & BOARD_16BIT) ? 0x4000 : 0x2000; /* 16 or 8 Kb */

  /* special setup needed for WD8013 boards */
  if (boardid & SLOT_16BIT)
    outb(WD_BASEREG + WD_REG5, WD_LAAR_A19|WD_LAAR_LAN16E);

  /* get ethernet address */
  eth_myaddr[0] = inb(WD_BASEREG + WD_EA0);
  eth_myaddr[1] = inb(WD_BASEREG + WD_EA1);
  eth_myaddr[2] = inb(WD_BASEREG + WD_EA2);
  eth_myaddr[3] = inb(WD_BASEREG + WD_EA3);
  eth_myaddr[4] = inb(WD_BASEREG + WD_EA4);
  eth_myaddr[5] = inb(WD_BASEREG + WD_EA5);

  /* save settings for future use */
  dpc.dc_reg = WD_BASEREG + WD_DP8390;
  dpc.dc_mem = WD_BASEMEM;
  dpc.dc_tpsr = 0;
  dpc.dc_pstart = 6;
  dpc.dc_pstop = (memsize >> 8) & 0xFF;

  printf("Using wd80x3 board, port 0x%x, iomem 0x%x, iosiz %d\n", WD_BASEREG, WD_BASEMEM, memsize);

  EtherReset();
  return 1;
}

/*
 * Stop ethernet board
 */
void
EtherStop(void) {
  /* stop dp8390, followed by a board reset */
  outb(dpc.dc_reg + DP_CR, CR_STP|CR_DM_ABORT);
  outb(WD_BASEREG + WD_MSR, WD_MSR_RESET);
  outb(WD_BASEREG + WD_MSR, 0);
}

/* TBD - all users must take care to use the current "data seg" value
when moving data from/to the controller */
static void
WdCopy(u_long src, u_long dst, u_long count) {
#if TRACE > 0
printf("WdCopy from %x to %x for %d\n", src, dst, count);
#endif
  assert(count <= 1514);
  if (boardid & SLOT_16BIT)
    outb(WD_BASEREG + WD_REG5,
	 WD_LAAR_MEM16E|WD_LAAR_LAN16E|WD_LAAR_A19);
  PhysBcopy(src, dst, count);
  if (boardid & SLOT_16BIT)
    outb(WD_BASEREG + WD_REG5, WD_LAAR_LAN16E|WD_LAAR_A19);
}

/*
 * Send an ethernet packet to destination 'dest'
 */
void
EtherSend(packet_t *pkt, u_short proto, u_char *dest) {
  ethhdr_t *ep;

  pkt->pkt_len += sizeof(ethhdr_t);
  pkt->pkt_offset -= sizeof(ethhdr_t);
  ep = (ethhdr_t *) pkt->pkt_offset;
  ep->eth_proto = htons(proto);
  bcopy((char *)dest, (char *)ep->eth_dst, ETH_ADDRSIZE);
  bcopy((char *)eth_myaddr, (char *)ep->eth_src, ETH_ADDRSIZE);
#if 0
DUMP_STRUCT("ethhdr_t", ep, sizeof(ethhdr_t));
#endif
  if (pkt->pkt_len < 60)
    pkt->pkt_len = 60;

#if TRACE > 0
  {
    int i;
DUMP_STRUCT("EtherSend: pkt", pkt->pkt_offset, pkt->pkt_len);
#if 0
    for(i=0; i<(pkt->pkt_len<MDUMP?pkt->pkt_len:MDUMP); i++) printe("%x ", *((u_char*)(pkt->pkt_offset)+i));
    printe("\n");
#endif
  }
#endif

#if 0
printe("EtherSend: WdCopy from %x to %x for %d\n", LA(pkt->pkt_offset), dpc.dc_mem + (dpc.dc_tpsr << 8), (u_long)pkt->pkt_len);
#endif
  WdCopy(LA(pkt->pkt_offset),
	 dpc.dc_mem + (dpc.dc_tpsr << 8), (u_long)pkt->pkt_len);
  outb(dpc.dc_reg + DP_TPSR, dpc.dc_tpsr);
  outb(dpc.dc_reg + DP_TBCR0, (pkt->pkt_len & 0xFF));
  outb(dpc.dc_reg + DP_TBCR1, (pkt->pkt_len >> 8) & 0xFF);
  outb(dpc.dc_reg + DP_CR, CR_TXP);

#if 0
  printe("Ethersend: outb(%x, %x)\n", dpc.dc_reg + DP_TPSR, dpc.dc_tpsr);
  printe("Ethersend: outb(%x, %x)\n", dpc.dc_reg + DP_TBCR0, (pkt->pkt_len & 0xFF));
  printe("Ethersend: outb(%x, %x)\n", dpc.dc_reg + DP_TBCR1, (pkt->pkt_len >> 8) & 0xFF);
  printe("Ethersend: outb(%x, %x)\n", dpc.dc_reg + DP_CR, CR_TXP);
#endif
}

/*
 * Copy dp8390 packet header for observation
 */
static void
GetHeader(u_long haddr, dphdr_t *dph) {
#if TRACE > 0
printe("GetHeader: WdCopy from %x to %x for %d\n", haddr, LA(dph), sizeof(dphdr_t));
#endif
  WdCopy(haddr, LA(dph), sizeof(dphdr_t));
#if 0
DUMP_STRUCT("GetHeader: dphdr_t", dph, sizeof(dphdr_t));
#endif
}

/*
 * Poll the dp8390 just see if there's an Ethernet packet
 * available. If there is, its contents is returned in a
 * pkt structure, otherwise a nil pointer is returned.
 */
packet_t *
EtherReceive(void) {
  u_char pageno, curpage, nextpage;
  int dpreg = dpc.dc_reg;
  packet_t *pkt;
  dphdr_t dph;
  u_long addr;

  pkt = (packet_t *)0;
  if (inb(dpreg + DP_RSR) & RSR_PRX) {
    /* get current page numbers */
    pageno = inb(dpreg + DP_BNRY) + 1;
    if (pageno == dpc.dc_pstop)
      pageno = dpc.dc_pstart;
    outb(dpreg + DP_CR, CR_PS_P1);
    curpage = inb(dpreg + DP_CURR);
    outb(dpreg + DP_CR, CR_PS_P0);
    if (pageno == curpage)
      return (packet_t *) 0;

    /* get packet header */
    addr = dpc.dc_mem + (pageno << 8);
    GetHeader(addr, &dph);
    nextpage = dph.dh_next;

    /* allocate packet */
    pkt = PktAlloc(0);
#if 0
printe("EtherReceive: allocated pkt %x\n", pkt);
#endif
    pkt->pkt_len = ((dph.dh_rbch & 0xFF) << 8) | (dph.dh_rbcl & 0xFF);
    pkt->pkt_len -= sizeof(dphdr_t);
    if (pkt->pkt_len > 1514) /* bug in dp8390 */
      pkt->pkt_len = 1514;

#if TRACE > 0
    {
      int i;
      printe("EtherReceive %d bytes: ", pkt->pkt_len);
#if 0
      for(i=0; i<(pkt->pkt_len<MDUMP?pkt->pkt_len:MDUMP); i++) printe("%x ", *((u_char*)pkt+i));
#else
      DUMP_STRUCT("", pkt, pkt->pkt_len);
#endif
      printe("\n");
    }
#endif

    /*
     * The dp8390 maintains a circular buffer of pages (256 bytes)
     * in which incomming ethernet packets are stored. The following
     * if detects wrap arounds, and copies the ethernet packet to
     * our local buffer in two chunks if necesarry.
     */
    assert(pkt->pkt_offset);
    assert(pkt->pkt_len <= (6 << 8));
    if (nextpage < pageno && nextpage > dpc.dc_pstart) {
      u_long nbytes = ((dpc.dc_pstop - pageno) << 8) - sizeof(dphdr_t);

      assert(nbytes <= (6 << 8));
#if TRACE > 0
printe("EtherReceive1: WdCopy from %x to %x for %x\n", addr + sizeof(dphdr_t), LA(pkt->pkt_offset), nbytes);
#endif
      WdCopy(addr + sizeof(dphdr_t),
	     LA(pkt->pkt_offset), nbytes);
      if ((pkt->pkt_len - nbytes) > 0)
	/* TBD - this OK? */
#if TRACE > 0
printe("EtherReceive2: WdCopy from %x to %x for %x\n",dpc.dc_mem + (dpc.dc_pstart << 8), LA(pkt->pkt_offset) + nbytes, pkt->pkt_len - nbytes);
#endif
	WdCopy(dpc.dc_mem + (dpc.dc_pstart << 8),
	       LA(pkt->pkt_offset) + nbytes,
	       pkt->pkt_len - nbytes); 
    } else {
#if TRACE > 0
printe("EtherReceive3: WdCopy from %x to %x for %x\n", addr + sizeof(dphdr_t), LA(pkt->pkt_offset), (u_long)pkt->pkt_len);
#endif
      WdCopy(addr + sizeof(dphdr_t),
	     LA(pkt->pkt_offset), (u_long)pkt->pkt_len);
    }

    /* release occupied pages */
    if (nextpage == dpc.dc_pstart)
      nextpage = dpc.dc_pstop;
    outb(dpreg + DP_BNRY, nextpage - 1);
  }

  return pkt;
}

/*
 * Print an ethernet address in human readable form
 */
void
EtherPrintAddr(u_char *addr) {
  printf("%x:%x:%x:%x:%x:%x",
	 addr[0] & 0xFF, addr[1] & 0xFF, addr[2] & 0xFF,
	 addr[3] & 0xFF, addr[4] & 0xFF, addr[5] & 0xFF);
}
