/* $NetBSD: if_ea.c,v 1.4 1996/03/27 21:49:26 mark Exp $ */

/*
 * Copyright (c) 1995 Mark Brinicombe
 * All rights reserved.
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
 *	This product includes software developed by Mark Brinicombe.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * if_ea.c
 *
 * Ether3 device driver
 *
 * Created      : 08/07/95
 */

/*
 * SEEQ 8005 device driver
 */

/*
 * Bugs/possible improvements:
 *	- Does not currently support DMA
 *	- Does not currently support multicasts
 *	- Does not transmit multiple packets in one go
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/cpu.h>
#include <machine/katelib.h>
#include <machine/io.h>
#include <machine/irqhandler.h>

#include <arm32/podulebus/if_eareg.h>
#include <arm32/podulebus/podulebus.h>

#define ETHER_MIN_LEN	64
#define ETHER_MAX_LEN	1514
#define ETHER_ADDR_LEN	6

#ifndef EA_TIMEOUT
#define EA_TIMEOUT	60
#endif

/*#define EA_TX_DEBUG*/
/*#define EA_RX_DEBUG*/
/*#define EA_DEBUG*/
/*#define EA_PACKET_DEBUG*/

/* for debugging convenience */
#ifdef EA_DEBUG
#define dprintf(x) printf x
#else
#define dprintf(x)
#endif

#define MY_MANUFACTURER 0x11
#define MY_PODULE       0xa4

/*
 * per-line info and status
 */

struct ea_softc {
	struct device sc_dev;
	irqhandler_t sc_ih;
	int sc_irq;			/* IRQ number */
	podule_t *sc_podule;		/* Our podule */
	int sc_podule_number;		/* Our podule number */
	u_int sc_iobase;		/* base I/O addr */
	struct arpcom sc_arpcom;	/* ethernet common */
	char sc_pktbuf[EA_BUFSIZ]; 	/* frame buffer */
	int sc_config1;			/* Current config1 bits */
	int sc_config2;			/* Current config2 bits */
	int sc_command;			/* Current command bits */
	int sc_irqclaimed;		/* Whether we have an IRQ claimed */
	int sc_rx_ptr;			/* Receive buffer pointer */
	int sc_tx_ptr;			/* Transmit buffer pointer */
};

/*
 * prototypes
 */

static int eaintr __P((void *));
static int ea_init __P((struct ea_softc *));
static int ea_ioctl __P((struct ifnet *, u_long, caddr_t));
static void ea_start __P((struct ifnet *));
static void ea_watchdog __P((int));
static void ea_reinit __P((struct ea_softc *));
static void ea_chipreset __P((struct ea_softc *));
static void ea_ramtest __P((struct ea_softc *));
static int ea_stoptx __P((struct ea_softc *));
static int ea_stoprx __P((struct ea_softc *));
static void ea_stop __P((struct ea_softc *));
static void ea_writebuf __P((struct ea_softc *, u_char *, int, int));
static void ea_readbuf __P((struct ea_softc *, u_char *, int, int));
static void earead __P((struct ea_softc *, caddr_t, int));
static struct mbuf *eaget __P((caddr_t, int, struct ifnet *));
static void ea_hardreset __P((struct ea_softc *));
static void eagetpackets __P((struct ea_softc *));
static void eatxpacket __P((struct ea_softc *));

int eaprobe __P((struct device *, void *, void *));
void eaattach __P((struct device *, struct device *, void *));

/* driver structure for autoconf */

struct cfattach ea_ca = {
	sizeof(struct ea_softc), eaprobe, eaattach
};

struct cfdriver ea_cd = {
	NULL, "ea", DV_IFNET
};

#if 0

/*
 * Dump the chip registers
 */

void
eadump(iobase)
	u_int iobase;
{
	dprintf(("%08x: %04x %04x %04x %04x %04x %04x %04x %04x\n", iobase,
	    ReadShort(iobase + 0x00), ReadShort(iobase + 0x40),
	    ReadShort(iobase + 0x80), ReadShort(iobase + 0xc0),
	    ReadShort(iobase + 0x100), ReadShort(iobase + 0x140),
	    ReadShort(iobase + 0x180), ReadShort(iobase + 0x1c0)));
}
#endif

/*
 * Dump the interface buffer
 */

void
ea_dump_buffer(sc, offset)
	struct ea_softc *sc;
	int offset;
{
#ifdef EA_PACKET_DEBUG
	u_int iobase = sc->sc_iobase;
	int addr;
	int loop;
	int size;
	int ctrl;
	int ptr;
	
	addr = offset;

	do {
		WriteShort(sc->sc_iobase + EA_8005_COMMAND, sc->sc_command | EA_CMD_FIFO_READ);
		WriteShort(iobase + EA_8005_CONFIG1, sc->sc_config1 | EA_BUFCODE_LOCAL_MEM);
		WriteShort(iobase + EA_8005_DMA_ADDR, addr);

		ptr = ReadShort(iobase + EA_8005_BUFWIN);
		ctrl = ReadShort(iobase + EA_8005_BUFWIN);
		ptr = ((ptr & 0xff) << 8) | ((ptr >> 8) & 0xff);

		if (ptr == 0) break;
		size = ptr - addr;

		printf("addr=%04x size=%04x ", addr, size);
		printf("cmd=%02x st=%02x\n", ctrl & 0xff, ctrl >> 8);

		for (loop = 0; loop < size - 4; loop += 2)
			printf("%04x ", ReadShort(iobase + EA_8005_BUFWIN));
		printf("\n");
		addr = ptr;
	} while (size != 0);
#endif
}

/*
 * Probe routine.
 */

/*
 * int eaprobe(struct device *parent, void *match, void *aux)
 *
 * Probe for the ether3 podule.
 */

int
eaprobe(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct podule_attach_args *pa = (void *)aux;
	int podule;
	u_int iobase;
	
/*	dprintf(("Probing for SEEQ 8005... \n"));*/

/* Look for a network slot interface */

	podule = findpodule(MY_MANUFACTURER, MY_PODULE, pa->pa_podule_number);

/* Fail if we did not find it */

	if (podule == -1)
		return(0);

	iobase = podules[podule].mod_base + EA_8005_BASE;

/* Reset it  - Why here ? */

	WriteShort(iobase + EA_8005_CONFIG2, EA_CFG2_RESET);
	delay(100);

/* We found it */

	pa->pa_podule_number = podule;
	pa->pa_podule = &podules[podule];

	return(1);
}


/*
 * void eaattach(struct device *parent, struct device *dev, void *aux)
 *
 * Attach podule.
 */

void
eaattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct ea_softc *sc = (void *)self;
	struct podule_attach_args *pa = (void *)aux;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int loop;
	int sum;

/*	dprintf(("Attaching %s...\n", sc->sc_dev.dv_xname));*/

/* Note the podule number and validate */

	sc->sc_podule_number = pa->pa_podule_number;
	if (sc->sc_podule_number == -1)
		panic("Podule has disappeared !");

	sc->sc_podule = &podules[sc->sc_podule_number];
	podules[sc->sc_podule_number].attached = 1;

/* Set the address of the controller for easy access */
	
	sc->sc_iobase = sc->sc_podule->mod_base + EA_8005_BASE;

	sc->sc_irqclaimed = 0;

/* Set up the interrupt structure */

	sc->sc_ih.ih_func = eaintr;
	sc->sc_ih.ih_arg = sc;
	sc->sc_ih.ih_level = IPL_NET;
	sc->sc_ih.ih_name = "net: ea";

/* Claim either a network slot interrupt or a podule interrupt */

	if (sc->sc_podule_number >= MAX_PODULES)
		sc->sc_irq = IRQ_NETSLOT;
	else
		sc->sc_irq = IRQ_PODULE /*+ sc->sc_podule_number*/;

	/* Stop the board. */

	ea_chipreset(sc);
	ea_stoptx(sc);
	ea_stoprx(sc);

	/* Initialise ifnet structure. */

	ifp->if_unit = sc->sc_dev.dv_unit;
	ifp->if_name = ea_cd.cd_name;
	ifp->if_start = ea_start;
	ifp->if_ioctl = ea_ioctl;
	ifp->if_watchdog = ea_watchdog;
	ifp->if_flags = IFF_BROADCAST | IFF_NOTRAILERS;

	/* Now we can attach the interface. */

/*	dprintf(("Attaching interface...\n"));*/
	if_attach(ifp);
	ether_ifattach(ifp);

/* Read the station address - the receiver must be off */

	WriteShort(sc->sc_iobase + EA_8005_CONFIG1, EA_BUFCODE_STATION_ADDR0);
	
	for (sum = 0, loop = 0; loop < ETHER_ADDR_LEN; ++loop) {
		sc->sc_arpcom.ac_enaddr[loop] =
		    ReadByte(sc->sc_iobase + EA_8005_BUFWIN);
		sum += sc->sc_arpcom.ac_enaddr[loop];
	}

/*
 * Hard code the ether address if we don't have one.
 * Need to work out how I get the real address
 */

	if (sum == 0) {
		sc->sc_arpcom.ac_enaddr[0] = 0x00;
		sc->sc_arpcom.ac_enaddr[1] = 0x00;
		sc->sc_arpcom.ac_enaddr[2] = 0xa4;
		sc->sc_arpcom.ac_enaddr[3] = 0x10;
		sc->sc_arpcom.ac_enaddr[4] = 0x02;
		sc->sc_arpcom.ac_enaddr[5] = 0x87;
	}

	/* Print out some information for the user. */

	printf(" SEEQ8005 address %s", ether_sprintf(sc->sc_arpcom.ac_enaddr));

	/* Finally, attach to bpf filter if it is present. */

#if NBPFILTER > 0
/*	dprintf(("Attaching to BPF...\n"));*/
	bpfattach(&ifp->if_bpf, ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif

	/* Should test the RAM */

	ea_ramtest(sc);
	
/*	dprintf(("eaattach() finished.\n"));*/
}


/*
 * Test the RAM on the ethernet card. This does not work yet
 */

void
ea_ramtest(sc)
	struct ea_softc *sc;
{
	register u_int iobase = sc->sc_iobase;
	register int loop;
	register u_int sum = 0;

/*	dprintf(("ea_ramtest()\n"));*/

	/*
	 * Test the buffer memory on the board.
	 * Write simple pattens to it and read them back.
	 */

	/* Set up the whole buffer RAM for writing */

	WriteShort(iobase + EA_8005_CONFIG1, EA_BUFCODE_TX_EAP);
	WriteShort(iobase + EA_8005_BUFWIN, ((EA_BUFFER_SIZE >> 8) - 1));
	WriteShort(iobase + EA_8005_TX_PTR, 0x0000);
	WriteShort(iobase + EA_8005_RX_PTR, EA_BUFFER_SIZE - 2);

	/* Set the write start address and write a pattern */

	ea_writebuf(sc, NULL, 0x0000, 0);

	for (loop = 0; loop < EA_BUFFER_SIZE; loop += 2)
		WriteShort(iobase + EA_8005_BUFWIN, loop);

	/* Set the read start address and verify the pattern */
	
	ea_readbuf(sc, NULL, 0x0000, 0);

	for (loop = 0; loop < EA_BUFFER_SIZE; loop += 2)
		if (ReadShort(iobase + EA_8005_BUFWIN) != loop)
			++sum;

	if (sum != 0)
		dprintf(("sum=%d\n", sum));

	/* Set the write start address and write a pattern */

	ea_writebuf(sc, NULL, 0x0000, 0);

	for (loop = 0; loop < EA_BUFFER_SIZE; loop += 2)
		WriteShort(iobase + EA_8005_BUFWIN, loop ^ (EA_BUFFER_SIZE - 1));

	/* Set the read start address and verify the pattern */

	ea_readbuf(sc, NULL, 0x0000, 0);

	for (loop = 0; loop < EA_BUFFER_SIZE; loop += 2)
		if (ReadShort(iobase + EA_8005_BUFWIN) != (loop ^ (EA_BUFFER_SIZE - 1)))
			++sum;

	if (sum != 0)
		dprintf(("sum=%d\n", sum));

	/* Set the write start address and write a pattern */

	ea_writebuf(sc, NULL, 0x0000, 0);

	for (loop = 0; loop < EA_BUFFER_SIZE; loop += 2)
		WriteShort(iobase + EA_8005_BUFWIN, 0xaa55);

	/* Set the read start address and verify the pattern */

	ea_readbuf(sc, NULL, 0x0000, 0);

	for (loop = 0; loop < EA_BUFFER_SIZE; loop += 2)
		if (ReadShort(iobase + EA_8005_BUFWIN) != 0xaa55)
			++sum;

	if (sum != 0)
		dprintf(("sum=%d\n", sum));

	/* Set the write start address and write a pattern */

	ea_writebuf(sc, NULL, 0x0000, 0);

	for (loop = 0; loop < EA_BUFFER_SIZE; loop += 2)
		WriteShort(iobase + EA_8005_BUFWIN, 0x55aa);

	/* Set the read start address and verify the pattern */

	ea_readbuf(sc, NULL, 0x0000, 0);

	for (loop = 0; loop < EA_BUFFER_SIZE; loop += 2)
		if (ReadShort(iobase + EA_8005_BUFWIN) != 0x55aa)
			++sum;

	if (sum != 0)
		dprintf(("sum=%d\n", sum));

	/* Report */

	if (sum == 0)
		printf(" %dK buffer RAM\n", EA_BUFFER_SIZE / 1024);
	else
		printf(" buffer RAM failed self test, %d faults\n", sum);
}


/* Claim an irq for the board */

void
ea_claimirq(sc)
	struct ea_softc *sc;
{
/* Have we claimed one already ? */

	if (sc->sc_irqclaimed) return;

/* Claim it */

	dprintf(("ea_claimirq(%d)\n", sc->sc_irq));
	if (irq_claim(sc->sc_irq, &sc->sc_ih))
		panic("Cannot install IRQ handler for IRQ %d", sc->sc_irq);

	sc->sc_irqclaimed = 1;
}


/* Release an irq */

void
ea_releaseirq(sc)
	struct ea_softc *sc;
{
/* Have we claimed one ? */

	if (!sc->sc_irqclaimed) return;

	dprintf(("ea_releaseirq(%d)\n", sc->sc_irq));
	if (irq_release(sc->sc_irq, &sc->sc_ih))
		panic("Cannot release IRQ handler for IRQ %d", sc->sc_irq);

	sc->sc_irqclaimed = 0;
}


/*
 * Stop and reinitialise the interface.
 */

static void
ea_reinit(sc)
	struct ea_softc *sc;
{
	int s;

	dprintf(("eareinit()\n"));

/* Stop and reinitialise the interface */

	s = splimp();
	ea_stop(sc);
	ea_init(sc);
	(void)splx(s);
}


/*
 * Stop the tx interface.
 *
 * Returns 0 if the tx was already stopped or 1 if it was active
 */

static int
ea_stoptx(sc)
	struct ea_softc *sc;
{
	u_int iobase = sc->sc_iobase;
	int timeout;
	int status;

	dprintf(("ea_stoptx()\n"));

	status = ReadShort(iobase + EA_8005_STATUS);
	if (!(status & EA_STATUS_TX_ON))
		return(0);

/* Stop any tx and wait for confirmation */

	WriteShort(iobase + EA_8005_COMMAND, sc->sc_command | EA_CMD_TX_OFF);

	timeout = 20000;
	do {
		status = ReadShort(iobase + EA_8005_STATUS);
	} while ((status & EA_STATUS_TX_ON) && --timeout > 0);
	if (timeout == 0)
		dprintf(("ea_stoptx: timeout waiting for tx termination\n"));

/* Clear any pending tx interrupt */

	WriteShort(iobase + EA_8005_COMMAND, sc->sc_command | EA_CMD_TX_INTACK);
	return(1);
}


/*
 * Stop the rx interface.
 *
 * Returns 0 if the tx was already stopped or 1 if it was active
 */

static int
ea_stoprx(sc)
	struct ea_softc *sc;
{
	u_int iobase = sc->sc_iobase;
	int timeout;
	int status;

	dprintf(("ea_stoprx()\n"));

	status = ReadShort(iobase + EA_8005_STATUS);
	if (!(status & EA_STATUS_RX_ON))
		return(0);

/* Stop any rx and wait for confirmation */

	WriteShort(iobase + EA_8005_COMMAND, sc->sc_command | EA_CMD_RX_OFF);

	timeout = 20000;
	do {
		status = ReadShort(iobase + EA_8005_STATUS);
	} while ((status & EA_STATUS_RX_ON) && --timeout > 0);
	if (timeout == 0)
		dprintf(("ea_stoprx: timeout waiting for rx termination\n"));

/* Clear any pending rx interrupt */

	WriteShort(iobase + EA_8005_COMMAND, sc->sc_command | EA_CMD_RX_INTACK);
	return(1);
}


/*
 * Stop interface.
 * Stop all IO and shut the interface down
 */

static void
ea_stop(sc)
	struct ea_softc *sc;
{
	u_int iobase = sc->sc_iobase;

	dprintf(("ea_stop()\n"));

/* Stop all IO */

	ea_stoptx(sc);
	ea_stoprx(sc);

/* Disable rx and tx interrupts */

	sc->sc_command &= (EA_CMD_RX_INTEN | EA_CMD_TX_INTEN);

/* Clear any pending interrupts */

	WriteShort(iobase + EA_8005_COMMAND, sc->sc_command
	    | EA_CMD_RX_INTACK | EA_CMD_TX_INTACK | EA_CMD_DMA_INTACK
	    | EA_CMD_BW_INTACK);
	dprintf(("st=%08x", ReadShort(iobase + EA_8005_STATUS)));

/* Release the irq */

	ea_releaseirq(sc);

	/* Cancel any watchdog timer */
	
	sc->sc_arpcom.ac_if.if_timer = 0;
}


/*
 * Reset the chip
 * Following this the software registers are reset
 */

static void
ea_chipreset(sc)
	struct ea_softc *sc;
{
	u_int iobase = sc->sc_iobase;

	dprintf(("ea_chipreset()\n"));

/* Reset the controller. Min of 4us delay here */

	WriteShort(iobase + EA_8005_CONFIG2, EA_CFG2_RESET);
	delay(100);

	sc->sc_command = 0;
	sc->sc_config1 = 0;
	sc->sc_config2 = 0;
}


/*
 * Do a hardware reset of the board, and upload the ethernet address again in
 * case the board forgets.
 */

static void
ea_hardreset(sc)
	struct ea_softc *sc;
{
	u_int iobase = sc->sc_iobase;
	int loop;

	dprintf(("ea_hardreset()\n"));

/* Stop any activity */

	ea_stoptx(sc);
	ea_stoprx(sc);

	ea_chipreset(sc);

/* Set up defaults for the registers */

	sc->sc_config2 = 0;
	WriteShort(iobase + EA_8005_CONFIG2, sc->sc_config2);
	sc->sc_command = 0x00;
	sc->sc_config1 = EA_CFG1_STATION_ADDR0 | EA_CFG1_DMA_BSIZE_1 | EA_CFG1_DMA_BURST_CONT;
	WriteShort(iobase + EA_8005_CONFIG1, sc->sc_config1);
	WriteShort(iobase + EA_8005_COMMAND, sc->sc_command);

	WriteShort(iobase + EA_8005_CONFIG1, EA_BUFCODE_TX_EAP);
	WriteShort(iobase + EA_8005_BUFWIN, ((EA_TX_BUFFER_SIZE >> 8) - 1));

/* Write the station address - the receiver must be off */

	WriteShort(sc->sc_iobase + EA_8005_CONFIG1,
	    sc->sc_config1 | EA_BUFCODE_STATION_ADDR0);
	for (loop = 0; loop < ETHER_ADDR_LEN; ++loop) {
		WriteByte(sc->sc_iobase + EA_8005_BUFWIN, sc->sc_arpcom.ac_enaddr[loop]);
	}
}


/*
 * write to the buffer memory on the interface
 *
 * If addr is within range for the interface buffer then the buffer
 * address is set to addr.
 * If len != 0 then data is copied from the address starting at buf
 * to the interface buffer.
 */

static void
ea_writebuf(sc, buf, addr, len)
	struct ea_softc *sc;
	u_char *buf;
	int addr;
	int len;
{
	u_int iobase = sc->sc_iobase;
	int loop;
	int timeout;

	dprintf(("writebuf: st=%04x\n", ReadShort(iobase + EA_8005_STATUS)));

/* If we have a valid buffer address set the buffer pointer and direction */

	if (addr >= 0 && addr < EA_BUFFER_SIZE) {
		WriteShort(iobase + EA_8005_CONFIG1, sc->sc_config1 | EA_BUFCODE_LOCAL_MEM);
		WriteShort(iobase + EA_8005_COMMAND, sc->sc_command | EA_CMD_FIFO_WRITE);

		/* Should wait here of FIFO empty flag */

		timeout = 20000;
		while ((ReadShort(iobase + EA_8005_STATUS) & EA_STATUS_FIFO_EMPTY) == 0 && --timeout > 0);


		WriteShort(iobase + EA_8005_DMA_ADDR, addr);
	}

	for (loop = 0; loop < len; loop += 2)
		WriteShort(iobase + EA_8005_BUFWIN, buf[loop] | buf[loop + 1] << 8);

/*	if (len > 0)
		outsw(iobase + EA_8005_BUFWIN, buf, len / 2);*/
}


/*
 * read from the buffer memory on the interface
 *
 * If addr is within range for the interface buffer then the buffer
 * address is set to addr.
 * If len != 0 then data is copied from the interface buffer to the
 * address starting at buf.
 */

static void
ea_readbuf(sc, buf, addr, len)
	struct ea_softc *sc;
	u_char *buf;
	int addr;
	int len;
{
	u_int iobase = sc->sc_iobase;
	int loop;
	int word;
	int timeout;

	dprintf(("readbuf: st=%04x addr=%04x len=%d\n", ReadShort(iobase + EA_8005_STATUS), addr, len));

/* If we have a valid buffer address set the buffer pointer and direction */

	if (addr >= 0 && addr < EA_BUFFER_SIZE) {
		if ((ReadShort(iobase + EA_8005_STATUS) & EA_STATUS_FIFO_DIR) == 0) {
			/* Should wait here of FIFO empty flag */

			timeout = 20000;
			while ((ReadShort(iobase + EA_8005_STATUS) & EA_STATUS_FIFO_EMPTY) == 0 && --timeout > 0);
		}
		WriteShort(iobase + EA_8005_CONFIG1, sc->sc_config1 | EA_BUFCODE_LOCAL_MEM);
		WriteShort(iobase + EA_8005_COMMAND, sc->sc_command | EA_CMD_FIFO_WRITE);

		/* Should wait here of FIFO empty flag */

		timeout = 20000;
		while ((ReadShort(iobase + EA_8005_STATUS) & EA_STATUS_FIFO_EMPTY) == 0 && --timeout > 0);

		WriteShort(iobase + EA_8005_DMA_ADDR, addr);
		WriteShort(iobase + EA_8005_COMMAND, sc->sc_command | EA_CMD_FIFO_READ);

		/* Should wait here of FIFO full flag */

		timeout = 20000;
		while ((ReadShort(iobase + EA_8005_STATUS) & EA_STATUS_FIFO_FULL) == 0 && --timeout > 0);


	}

	for (loop = 0; loop < len; loop += 2) {
		word = ReadShort(iobase + EA_8005_BUFWIN);
		buf[loop] = word & 0xff;
		buf[loop + 1] = word >> 8;
	}
}


/*
 * Initialize interface.
 *
 * This should leave the interface in a state for packet reception and transmission
 */

static int
ea_init(sc)
	struct ea_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	u_int iobase = sc->sc_iobase;
	int s;

	dprintf(("ea_init()\n"));

	s = splimp();

/* Grab an irq */

	ea_claimirq(sc);

	/* First, reset the board. */

	ea_hardreset(sc);

/* Configure rx. */

	dprintf(("Configuring rx...\n"));
	if (ifp->if_flags & IFF_PROMISC)
		sc->sc_config1 = EA_CFG1_PROMISCUOUS;
	else
		sc->sc_config1 = EA_CFG1_BROADCAST;

	sc->sc_config1 |= EA_CFG1_DMA_BSIZE_8 | EA_CFG1_STATION_ADDR0 | EA_CFG1_DMA_BURST_CONT;
	WriteShort(iobase + EA_8005_CONFIG1, sc->sc_config1);

/* Configure TX. */

	dprintf(("Configuring tx...\n"));

	WriteShort(iobase + EA_8005_CONFIG1, sc->sc_config1 | EA_BUFCODE_TX_EAP);
	WriteShort(iobase + EA_8005_BUFWIN, ((EA_TX_BUFFER_SIZE >> 8) - 1));
	WriteShort(iobase + EA_8005_TX_PTR, 0x0000);

	sc->sc_config2 |= EA_CFG2_OUTPUT;
	WriteShort(iobase + EA_8005_CONFIG2, sc->sc_config2);

/* Place a NULL header at the beginning of the transmit area */

	ea_writebuf(sc, NULL, 0x0000, 0);
		
	WriteShort(iobase + EA_8005_BUFWIN, 0x0000);
	WriteShort(iobase + EA_8005_BUFWIN, 0x0000);

	sc->sc_command |= EA_CMD_TX_INTEN;
	WriteShort(iobase + EA_8005_COMMAND, sc->sc_command);

/* Setup the Rx pointers */

	sc->sc_rx_ptr = EA_TX_BUFFER_SIZE;

	WriteShort(iobase + EA_8005_RX_PTR, sc->sc_rx_ptr);
	WriteShort(iobase + EA_8005_RX_END, (sc->sc_rx_ptr >> 8));

/* Place a NULL header at the beginning of the receive area */

	ea_writebuf(sc, NULL, sc->sc_rx_ptr, 0);
		
	WriteShort(iobase + EA_8005_BUFWIN, 0x0000);
	WriteShort(iobase + EA_8005_BUFWIN, 0x0000);

/* Turn on Rx */

	sc->sc_command |= EA_CMD_RX_INTEN;
	WriteShort(iobase + EA_8005_COMMAND, sc->sc_command | EA_CMD_RX_ON);

	/* Set flags appropriately. */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	dprintf(("init: st=%04x\n", ReadShort(iobase + EA_8005_STATUS)));

	/* And start output. */
	ea_start(ifp);

	(void)splx(s);
	return(0);
}


/*
 * Start output on interface. Get datagrams from the queue and output them,
 * giving the receiver a chance between datagrams. Call only from splimp or
 * interrupt level!
 */

static void
ea_start(ifp)
	struct ifnet *ifp;
{
	struct ea_softc *sc = ea_cd.cd_devs[ifp->if_unit];
	int s;

	s = splimp();
#ifdef EA_TX_DEBUG
	dprintf(("ea_start()...\n"));
#endif

	/* Don't do anything if output is active. */

	if (sc->sc_arpcom.ac_if.if_flags & IFF_OACTIVE)
		return;

	/* Mark interface as output active */
	
	sc->sc_arpcom.ac_if.if_flags |= IFF_OACTIVE;

	/* tx packets */

	eatxpacket(sc);
	(void)splx(s);
}


/*
 * Transfer a packet to the interface buffer and start transmission
 *
 * Called at splimp()
 */
 
void
eatxpacket(sc)
	struct ea_softc *sc;
{
	u_int iobase = sc->sc_iobase;
	struct mbuf *m, *m0;
	int len;

/* Dequeue the next datagram. */

	IF_DEQUEUE(&sc->sc_arpcom.ac_if.if_snd, m0);

/* If there's nothing to send, return. */

	if (!m0) {
		sc->sc_arpcom.ac_if.if_flags &= ~IFF_OACTIVE;
		sc->sc_config2 |= EA_CFG2_OUTPUT;
		WriteShort(iobase + EA_8005_CONFIG2, sc->sc_config2);
#ifdef EA_TX_DEBUG
		dprintf(("tx finished\n"));
#endif
		return;
	}

	/* Give the packet to the bpf, if any. */
#if NBPFILTER > 0
	if (sc->sc_arpcom.ac_if.if_bpf)
		bpf_mtap(sc->sc_arpcom.ac_if.if_bpf, m0);
#endif

#ifdef EA_TX_DEBUG
	dprintf(("Tx new packet\n"));
#endif

/*
 * Copy the datagram to the temporary buffer.
 *
 * Eventually we may as well just copy straight into the interface buffer
 */

	len = 0;
	for (m = m0; m; m = m->m_next) {
		if (m->m_len == 0)
			continue;
		bcopy(mtod(m, caddr_t), sc->sc_pktbuf + len, m->m_len);
		len += m->m_len;
	}
	m_freem(m0);

/* If packet size is odd round up to the next 16 bit boundry */

	if (len % 2)
		++len;

	len = max(len, ETHER_MIN_LEN);
	
	if (len > ETHER_MAX_LEN)
		log(LOG_WARNING, "ea: oversize packet = %d bytes\n", len);

/* Ok we now have a packet len bytes long in our packet buffer */

/* Transfer datagram to board. */

#ifdef EA_TX_DEBUG
	dprintf(("ea: xfr pkt length=%d...\n", len));

	dprintf(("%s-->", ether_sprintf(sc->sc_pktbuf+6)));
	dprintf(("%s\n", ether_sprintf(sc->sc_pktbuf)));
#endif

	sc->sc_config2 &= ~EA_CFG2_OUTPUT;
	WriteShort(iobase + EA_8005_CONFIG2, sc->sc_config2);

/*	dprintf(("st=%04x\n", ReadShort(iobase + EA_8005_STATUS)));*/

/* Write the packet to the interface buffer, skipping the packet header */

	ea_writebuf(sc, sc->sc_pktbuf, 0x0004, len);

/* Follow it with a NULL packet header */

	WriteShort(iobase + EA_8005_BUFWIN, 0x00);
	WriteShort(iobase + EA_8005_BUFWIN, 0x00);

/* Write the packet header */

	ea_writebuf(sc, NULL, 0x0000, 0);

	WriteShort(iobase + EA_8005_BUFWIN, (((len+4) & 0xff00) >> 8) | (((len+4) & 0xff) << 8));
	WriteShort(iobase + EA_8005_BUFWIN, 0x00aa);

	WriteShort(iobase + EA_8005_TX_PTR, 0x0000);

/*	dprintf(("st=%04x\n", ReadShort(iobase + EA_8005_STATUS)));*/

#ifdef EA_DEBUG
	ea_dump_buffer(sc, 0);
#endif

/* Now transmit the datagram. */

/*	dprintf(("st=%04x\n", ReadShort(iobase + EA_8005_STATUS)));*/
	WriteShort(iobase + EA_8005_COMMAND, sc->sc_command | EA_CMD_TX_ON);
#ifdef EA_TX_DEBUG
	dprintf(("st=%04x\n", ReadShort(iobase + EA_8005_STATUS)));
	dprintf(("tx: queued\n"));
#endif
}


/*
 * Ethernet controller interrupt.
 */

int
eaintr(arg)
	void *arg;
{
	register struct ea_softc *sc = arg;
	u_int iobase = sc->sc_iobase;
	int status, s;
	u_int txstatus;

	dprintf(("eaintr: "));

/* Get the controller status */

	status = ReadShort(iobase + EA_8005_STATUS);
        dprintf(("st=%04x ", status));	

/* Tx interrupt ? */

	if (status & EA_STATUS_TX_INT) {
		dprintf(("txint "));

/* Acknowledge the interrupt */

		WriteShort(iobase + EA_8005_COMMAND, sc->sc_command
		    | EA_CMD_TX_INTACK);

		ea_readbuf(sc, (u_char *)&txstatus, 0x0000, 4);

#ifdef EA_TX_DEBUG		
		dprintf(("txstatus=%08x\n", txstatus));
#endif
		txstatus = (txstatus >> 24) & 0xff;

/*
 * Did it succeed ? Did we collide ?
 *
 * The exact proceedure here is not clear. We should get
 * an interrupt on a sucessfull tx or on a collision.
 * The done flag is set after successfull tx or 16 collisions
 * We should thus get a interrupt for each of collision
 * and the done bit should not be set. However it does appear
 * to be set at the same time as the collision bit ...
 *
 * So we will count collisions and output errors and will assume
 * that if the done bit is set the packet was transmitted.
 * Stats may be wrong if 16 collisions occur on a packet
 * as the done flag should be set but the packet may not have been
 * transmitted. so the output count might not require incrementing
 * if the 16 collisions flags is set. I don;t know abou this until
 * it happens.
 */

		if (txstatus & EA_TXHDR_COLLISION) {
			sc->sc_arpcom.ac_if.if_collisions++;
		} else if (txstatus & EA_TXHDR_ERROR_MASK) {
			sc->sc_arpcom.ac_if.if_oerrors++;
		}

/*		if (txstatus & EA_TXHDR_ERROR_MASK) {
			log(LOG_WARNING, "tx packet error =%02x\n", txstatus);
		}*/

		if (txstatus & EA_PKTHDR_DONE) {
			sc->sc_arpcom.ac_if.if_opackets++;

			/* Tx next packet */

			s = splimp();
			eatxpacket(sc);
			(void)splx(s);
		}
	}

/* Rx interrupt ? */

	if (status & EA_STATUS_RX_INT) {
		dprintf(("rxint "));

/* Acknowledge the interrupt */

		WriteShort(iobase + EA_8005_COMMAND, sc->sc_command
		    | EA_CMD_RX_INTACK);

/* Install a watchdog timer needed atm to fixed rx lockups */

		sc->sc_arpcom.ac_if.if_timer = EA_TIMEOUT;

/* Processes the received packets */
		eagetpackets(sc);

/* Make sure the receiver is on */

/*		if ((status & EA_STATUS_RX_ON) == 0) {
			WriteShort(iobase + EA_8005_COMMAND, sc->sc_command
			    | EA_CMD_RX_ON);
			printf("rxintr: rx is off st=%04x\n",status);
		}*/
	}

#ifdef EA_DEBUG
	status = ReadShort(iobase + EA_8005_STATUS);
        dprintf(("st=%04x\n", status));
#endif
	return(0);
}

void
eagetpackets(sc)
	struct ea_softc *sc;
{
	u_int iobase = sc->sc_iobase;
	int addr;
	int len;
	int ctrl;
	int ptr;
	int pack;
	int status;
	u_int rxstatus;

/* We start from the last rx pointer position */

	addr = sc->sc_rx_ptr;
	sc->sc_config2 &= ~EA_CFG2_OUTPUT;
	WriteShort(iobase + EA_8005_CONFIG2, sc->sc_config2);

	do {
/* Read rx header */

		ea_readbuf(sc, (u_char *)&rxstatus, addr, 4);
		
/* Split the packet header */

		ptr = ((rxstatus & 0xff) << 8) | ((rxstatus >> 8) & 0xff);
		ctrl = (rxstatus >> 16) & 0xff;
		status = (rxstatus >> 24) & 0xff;

#ifdef EA_RX_DEBUG
		dprintf(("addr=%04x ptr=%04x ctrl=%02x status=%02x\n", addr, ptr, ctrl, status));
#endif

/* Zero packet ptr ? then must be null header so exit */

		if (ptr == 0) break;

/* Get packet length */
	
		len = (ptr - addr) - 4;

		if (len < 0) {
			len += EA_RX_BUFFER_SIZE;
		}

#ifdef EA_RX_DEBUG
		dprintf(("len=%04x\n", len));
#endif

/* Has the packet rx completed ? if not then exit */

		if ((status & EA_PKTHDR_DONE) == 0)
			break;

/* Did we have any errors ? then note error and go to next packet */

		if (status & 0x0f) {
			++sc->sc_arpcom.ac_if.if_ierrors;
			printf("rx packet error (%02x) - dropping packet\n", status & 0x0f);
/*			sc->sc_config2 |= EA_CFG2_OUTPUT;
			WriteShort(iobase + EA_8005_CONFIG2, sc->sc_config2);
			ea_reinit(sc);
			return; */
			addr = ptr;
			continue;
		}

/* Is the packet too big ? - this will probably be trapped above as a receive error */

		if (len > ETHER_MAX_LEN) {
			++sc->sc_arpcom.ac_if.if_ierrors;
			printf("rx packet size error len=%d\n", len);
/*			sc->sc_config2 |= EA_CFG2_OUTPUT;
			WriteShort(iobase + EA_8005_CONFIG2, sc->sc_config2);
			ea_reinit(sc);
			return;*/
			addr = ptr;
			continue;
		}

		ea_readbuf(sc, sc->sc_pktbuf, addr + 4, len);

#ifdef EA_RX_DEBUG
		dprintf(("%s-->", ether_sprintf(sc->sc_pktbuf+6)));
		dprintf(("%s\n", ether_sprintf(sc->sc_pktbuf)));
#endif
		sc->sc_arpcom.ac_if.if_ipackets++;
		/* Pass data up to upper levels. */
		earead(sc, (caddr_t)sc->sc_pktbuf, len);

		addr = ptr;
		++pack;
	} while (len != 0);

	sc->sc_config2 |= EA_CFG2_OUTPUT;
	WriteShort(iobase + EA_8005_CONFIG2, sc->sc_config2);

#ifdef EA_RX_DEBUG
	dprintf(("new rx ptr=%04x\n", addr));
#endif

/* Store new rx pointer */

	sc->sc_rx_ptr = addr;
	WriteShort(iobase + EA_8005_RX_END, (sc->sc_rx_ptr >> 8));

/* Make sure the receiver is on */

	WriteShort(iobase + EA_8005_COMMAND, sc->sc_command
	    | EA_CMD_RX_ON);

}


/*
 * Pass a packet up to the higher levels.
 */

static void
earead(sc, buf, len)
	struct ea_softc *sc;
	caddr_t buf;
	int len;
{
	register struct ether_header *eh;
	struct mbuf *m;

	eh = (struct ether_header *)buf;
	len -= sizeof(struct ether_header);
	if (len <= 0)
		return;

	/* Pull packet off interface. */
	m = eaget(buf, len, &sc->sc_arpcom.ac_if);
	if (m == 0)
		return;

#if NBPFILTER > 0
	/*
	 * Check if there's a BPF listener on this interface.
	 * If so, hand off the raw packet to bpf.
	 */
	if (sc->sc_arpcom.ac_if.if_bpf) {
		bpf_tap(sc->sc_arpcom.ac_if.if_bpf, buf, len + sizeof(struct ether_header));
/*		bpf_mtap(sc->sc_arpcom.ac_if.if_bpf, m);*/

		/*
		 * Note that the interface cannot be in promiscuous mode if
		 * there are no BPF listeners.  And if we are in promiscuous
		 * mode, we have to check if this packet is really ours.
		 */
		if ((sc->sc_arpcom.ac_if.if_flags & IFF_PROMISC) &&
		    (eh->ether_dhost[0] & 1) == 0 && /* !mcast and !bcast */
		    bcmp(eh->ether_dhost, sc->sc_arpcom.ac_enaddr,
			    sizeof(eh->ether_dhost)) != 0) {
			m_freem(m);
			return;
		}
	}
#endif

	ether_input(&sc->sc_arpcom.ac_if, eh, m);
}

/*
 * Pull read data off a interface.  Len is length of data, with local net
 * header stripped.  We copy the data into mbufs.  When full cluster sized
 * units are present we copy into clusters.
 */

struct mbuf *
eaget(buf, totlen, ifp)
        caddr_t buf;
        int totlen;
        struct ifnet *ifp;
{
        struct mbuf *top, **mp, *m;
        int len;
        register caddr_t cp = buf;
        char *epkt;

        buf += sizeof(struct ether_header);
        cp = buf;
        epkt = cp + totlen;

        MGETHDR(m, M_DONTWAIT, MT_DATA);
        if (m == 0)
                return 0;
        m->m_pkthdr.rcvif = ifp;
        m->m_pkthdr.len = totlen;
        m->m_len = MHLEN;
        top = 0;
        mp = &top;

        while (totlen > 0) {
                if (top) {
                        MGET(m, M_DONTWAIT, MT_DATA);
                        if (m == 0) {
                                m_freem(top);
                                return 0;
                        }
                        m->m_len = MLEN;
                }
                len = min(totlen, epkt - cp);
                if (len >= MINCLSIZE) {
                        MCLGET(m, M_DONTWAIT);
                        if (m->m_flags & M_EXT)
                                m->m_len = len = min(len, MCLBYTES);
                        else
                                len = m->m_len;
                } else {
                        /*
                         * Place initial small packet/header at end of mbuf.
                         */
                        if (len < m->m_len) {
                                if (top == 0 && len + max_linkhdr <= m->m_len)
                                        m->m_data += max_linkhdr;
                                m->m_len = len;
                        } else
                                len = m->m_len;
                }
                bcopy(cp, mtod(m, caddr_t), (unsigned)len);
                cp += len;
                *mp = m;
                mp = &m->m_next;
                totlen -= len;
                if (cp == epkt)
                        cp = buf;
        }

        return top;
}

/*
 * Process an ioctl request. This code needs some work - it looks pretty ugly.
 */
static int
ea_ioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct ea_softc *sc = ea_cd.cd_devs[ifp->if_unit];
	struct ifaddr *ifa = (struct ifaddr *)data;
/*	struct ifreq *ifr = (struct ifreq *)data;*/
	int s, error = 0;

	s = splimp();

	if ((error = ether_ioctl(ifp, &sc->sc_arpcom, cmd, data)) > 0) {
		splx(s);
		return error;
	}

	switch (cmd) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		dprintf(("if_flags=%08x\n", ifp->if_flags));

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			arp_ifinit(&sc->sc_arpcom, ifa);
			dprintf(("Interface ea is coming up (AF_INET)\n"));
			ea_init(sc);
			break;
#endif
		default:
			dprintf(("Interface ea is coming up (default)\n"));
			ea_init(sc);
			break;
		}
		break;

	case SIOCSIFFLAGS:
		dprintf(("if_flags=%08x\n", ifp->if_flags));
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (ifp->if_flags & IFF_RUNNING) != 0) {
			/*
			 * If interface is marked down and it is running, then
			 * stop it.
			 */
			dprintf(("Interface ea is stopping\n"));
			ea_stop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
		    	   (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			dprintf(("Interface ea is restarting(1)\n"));
			ea_init(sc);
		} else {
			/*
			 * Some other important flag might have changed, so
			 * reset.
			 */
			dprintf(("Interface ea is reinitialising\n"));
			ea_reinit(sc);
		}
		break;

	default:
		error = EINVAL;
		break;
	}

	(void)splx(s);
	return error;
}

/*
 * Device timeout routine.
 *
 * Ok I am not sure exactly how the device timeout should work....
 * Currently what will happens is that that the device timeout is only
 * set when a packet it received. This indicates we are on an active
 * network and thus we should expect more packets. If non arrive in
 * in the timeout period then we reinitialise as we may have jammed.
 * We zero the timeout at this point so that we don't end up with
 * an endless stream of timeouts if the network goes down.
 */

static void
ea_watchdog(unit)
	int unit;
{
	struct ea_softc *sc = ea_cd.cd_devs[unit];

	log(LOG_ERR, "%s: device timeout\n", sc->sc_dev.dv_xname);
	sc->sc_arpcom.ac_if.if_oerrors++;
	dprintf(("ea_watchdog: "));
	dprintf(("st=%04x\n", ReadShort(sc->sc_iobase + EA_8005_STATUS)));

	/* Kick the interface */

	ea_reinit(sc);

/*	sc->sc_arpcom.ac_if.if_timer = EA_TIMEOUT;*/
	sc->sc_arpcom.ac_if.if_timer = 0;
}

/* End of if_ea.c */
