/*	$OpenBSD: if_ie.h,v 1.2 1997/08/08 08:25:11 downsj Exp $	*/
/*	$NetBSD: if_ie.h,v 1.4 1994/12/16 22:01:11 deraadt Exp $ */

/*
 * if_sunie.h
 *
 * sun's ie interface
 */

/*
 * programming notes:
 *
 * the ie chip operates in a 24 bit address space.
 *
 * most ie interfaces appear to be divided into two parts:
 *	 - generic 586 stuff
 *	 - board specific
 *
 * generic:
 *	the generic stuff of the ie chip is all done with data structures
 * 	that live in the chip's memory address space.   the chip expects
 * 	its main data structure (the sys conf ptr -- SCP) to be at a fixed
 * 	address in its 24 bit space: 0xfffff4
 *
 *      the SCP points to another structure called the ISCP.
 *      the ISCP points to another structure called the SCB.
 * 	the SCB has a status field, a linked list of "commands", and
 * 	a linked list of "receive buffers".   these are data structures that
 * 	live in memory, not registers.
 *
 * board:
 * 	to get the chip to do anything, you first put a command in the
 * 	command data structure list.   then you have to signal "attention"
 * 	to the chip to get it to look at the command.   how you
 * 	signal attention depends on what board you have... on PC's
 * 	there is an i/o port number to do this, on sun's there is a
 * 	register bit you toggle.
 *
 * 	to get data from the chip you program it to interrupt...
 *
 *
 * sun issues:
 *
 *      there are 3 kinds of sun "ie" interfaces:
 *        1 - a VME/multibus card
 *        2 - an on-board interface (sun3's, sun-4/100's, and sun-4/200's)
 *        3 - another VME board called the 3E
 *
 * 	the VME boards lives in vme16 space.   only 16 and 8 bit accesses
 * 	are allowed, so functions that copy data must be aware of this.
 *
 * 	the chip is an intel chip.  this means that the byte order
 * 	on all the "short"s in the chip's data structures is wrong.
 * 	so, constants described in the intel docs are swapped for the sun.
 * 	that means that any buffer pointers you give the chip must be
 * 	swapped to intel format.   yuck.
 *
 *   VME/multibus interface:
 * 	for the multibus interface the board ignores the top 4 bits
 * 	of the chip address.   the multibus interface seems to have its
 * 	own MMU like page map (without protections or valid bits, etc).
 * 	there are 256 pages of physical memory on the board (each page
 * 	is 1024 bytes).   there are 1024 slots in the page map.  so,
 * 	a 1024 byte page takes up 10 bits of address for the offset,
 * 	and if there are 1024 slots in the page that is another 10 bits
 * 	of the address.   that makes a 20 bit address, and as stated
 * 	earlier the board ignores the top 4 bits, so that accounts
 * 	for all 24 bits of address.
 *
 * 	note that the last entry of the page map maps the top of the
 * 	24 bit address space and that the SCP is supposed to be at
 * 	0xfffff4 (taking into account allignment).   so,
 *	for multibus, that entry in the page map has to be used for the SCP.
 *
 * 	the page map effects BOTH how the ie chip sees the
 * 	memory, and how the host sees it.
 *
 * 	the page map is part of the "register" area of the board
 *
 *   on-board interface:
 *
 *	<fill in useful info later>
 *
 *
 *   VME3E interface:
 *
 *	<fill in useful info later>
 *
 */

/*
 * PART 1: VME/multibus defs
 */
#define IEVME_PAGESIZE 1024	/* bytes */
#define IEVME_PAGSHIFT 10	/* bits */
#define IEVME_NPAGES   256	/* number of pages on chip */
#define IEVME_MAPSZ    1024	/* number of entries in the map */

/*
 * PTE for the page map
 */
#define IEVME_SBORDR 0x8000	/* sun byte order */
#define IEVME_IBORDR 0x0000	/* intel byte ordr */

#define IEVME_P2MEM  0x2000	/* memory is on P2 */
#define IEVME_OBMEM  0x0000	/* memory is on board */

#define IEVME_PGMASK 0x0fff	/* gives the physical page frame number */

struct ievme {
	u_short pgmap[IEVME_MAPSZ];
	u_short xxx[32];	/* prom */
	u_short status;		/* see below for bits */
	u_short xxx2;		/* filler */
	u_short pectrl;		/* parity control (see below) */
	u_short peaddr;		/* low 16 bits of address */
};

/*
 * status bits
 */
#define IEVME_RESET 0x8000	/* reset board */
#define IEVME_ONAIR 0x4000	/* go out of loopback 'on-air' */
#define IEVME_ATTEN 0x2000	/* attention */
#define IEVME_IENAB 0x1000	/* interrupt enable */
#define IEVME_PEINT 0x0800	/* parity error interrupt enable */
#define IEVME_PERR  0x0200	/* parity error flag */
#define IEVME_INT   0x0100	/* interrupt flag */
#define IEVME_P2EN  0x0020	/* enable p2 bus */
#define IEVME_256K  0x0010	/* 256kb rams */
#define IEVME_HADDR 0x000f	/* mask for bits 17-20 of address */

/*
 * parity control
 */
#define IEVME_PARACK 0x0100	/* parity error ack */
#define IEVME_PARSRC 0x0080	/* parity error source */
#define IEVME_PAREND 0x0040	/* which end of the data got the error */
#define IEVME_PARADR 0x000f	/* mask to get bits 17-20 of parity address */


/*
 * PART 2: the on-board interface
 */
struct ieob {
	u_char  obctrl;
};
#define IEOB_NORSET 0x80	/* don't reset the board */
#define IEOB_ONAIR  0x40	/* put us on the air */
#define IEOB_ATTEN  0x20	/* attention! */
#define IEOB_IENAB  0x10	/* interrupt enable */
#define IEOB_XXXXX  0x08	/* free bit */
#define IEOB_XCVRL2 0x04	/* level 2 transceiver? */
#define IEOB_BUSERR 0x02	/* bus error */
#define IEOB_INT    0x01	/* interrupt */

#define IEOB_ADBASE 0xff000000  /* KVA base addr of 24 bit address space */

/*
 * PART 3: the 3E board
 */

/*
 * not supported (yet?)
 */
