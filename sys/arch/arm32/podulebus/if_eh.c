/* $NetBSD: if_eh.c,v 1.6 1996/03/27 21:49:34 mark Exp $ */

/*
 * Copyright (c) 1995 Melvin Tang-Richardson.
 * All rights reserved.
 *
 * Essential things to do
 *  - BPF and multicast support
 *  - Testing
 *  - 16-bit dma's
 *  - Buffer overflow handling (This must be done a certain way) [Nuts]
 *  - Enabling card interrupts (Need i cubed info)
 *
 * Other things to do
 *  - Pipelined transmitts
 *  - Dynamic buffer allocation
 *  - Shared RAM if I cubed give me info
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
 *	This product includes software developed by RiscBSD.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY RISCBSD ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL RISCBSD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * if_eh.c
 *
 * Ether H driver.
 */

/* Some system includes *****************************************************/

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <machine/bootconfig.h>

/* Hardware related include chip/card/bus ***********************************/

#include <machine/io.h>
#include <machine/irqhandler.h>
#include <machine/katelib.h>
#include <arm32/podulebus/podulebus.h>
#include <arm32/podulebus/if_ehreg.h>

/* Includes for interfacing with the network subsystem **********************/

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/netisr.h>
#include <net/route.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

/****************************************************************************/
/* Some useful definitions **************************************************/
/****************************************************************************/

#define MY_MANUFACTURER	(0x46)
#define MY_PODULE	(0xec)

#define TXBUF_SIZE	(1522)
#define NTXBUF		(1)

#define RXBUF_SIZE	(256)
#define NRXBUF		(26)

#define ETHER_MIN_LEN   (64)
#define ETHER_MAX_LEN   (1522)

#define PRINTF	printf

/****************************************************************************/
/* Prototype internal data structures ***************************************/
/****************************************************************************/

struct eh_softc {
	/* Kernel interface variables */
	struct device	sc_dev;
	int		sc_podule_number;
	podule_t	*sc_podule;
	irqhandler_t	sc_ih;

	/* Network subsystem interface */
	struct arpcom	sc_arpcom;
	int 		promisc;

	/* Driver defined state variables */
	int		sc_flags;

	/* Hardware defined state */
	int		sc_base;
	int		sc_reg;
	int		sc_nxtpkt;

	/* Card memory map */
	int		sc_physstart;
	int		sc_physend;
	int		sc_tbs[NTXBUF];  /* Transmit buffer start */
	int		sc_rbs;
	int		sc_rbd;

	/* Txrx parameters */
	int		sc_txcur;	 /* Fill this buffer next */
	int		sc_txlst;	 /* Buffer in use         */
};

/* sc_flags */

#define MODE_MASK	(3)
#define MODE_DEAD	(0)
#define	MODE_PIO	(1)
#define MODE_SHRAM	(2)

/****************************************************************************/
/* Function and data prototypes *********************************************/
/****************************************************************************/

int  ehprobe		__P((struct device *parent, void *match, void *aux));
void ehattach		__P((struct device *parent, struct device *self, void *aux));
void ehstart		__P((struct ifnet *ifp));
int  ehioctl		__P((struct ifnet *ifp, u_long cmd, caddr_t data));
void ehwatchdog		__P((int unit));
void eh_stop_controller  __P((struct eh_softc *sc));
void eh_start_controller __P((struct eh_softc *sc));
void eh_transmit_command __P((struct eh_softc *sc));
int  eh_copyin		__P((struct eh_softc *sc, int src, char *dest, int len));
int  eh_copyout		__P((struct eh_softc *sc, char *src, int dest, int len));
void ehinit		__P((struct eh_softc *sc));
int  ehstop		__P((struct eh_softc *sc));
int  ehintr		__P((void *arg));
void eh_shutdown	__P((void *arg));

struct cfattach eh_ca = {
	sizeof(struct eh_softc), ehprobe, ehattach
};

struct cfdriver eh_cd = {
	NULL, "eh", DV_DULL, NULL, 0
};

/****************************************************************************/
/* Some useful macros *******************************************************/
/****************************************************************************/

#define SetReg(r,v)	WriteWord ( sc->sc_reg+(r), (v)|((v)<<16) )

#define GetReg(r)	ReadByte ( sc->sc_reg+(r) )

#define	PAGE(n)	SetReg ( EH_COMMAND, 	\
		((GetReg(EH_COMMAND)&COM_MASK_PAGE)|(n<<6)) )

/* When initiating a remote dma, reset the TXP bit.  This Does Not abort */
/* the transmit, but allows us overlap this remote dma			 */

#define	REMOTE_DMA(n)	SetReg ( EH_COMMAND, 	\
		((GetReg(EH_COMMAND)&(COM_MASK_DMA))|n) )

extern char *boot_args;
char *strstr		__P((char */*s1*/, char */*s2*/));

/****************************************************************************/
/* Bus attachment code ******************************************************/
/****************************************************************************/

int
ehprobe(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct eh_softc *sc = (void *) match;
	struct podule_attach_args *pa = (void *) aux;
	int counter;
	int temp;
	int podule;

	podule = findpodule(MY_MANUFACTURER, MY_PODULE, pa->pa_podule_number);

	if (podule == -1)
		return 0;

	/* Start to fill in the softc with the trivial variables */

	sc->sc_flags = MODE_PIO;	/* Select pio mode until I get info from i^3 */
	sc->sc_base = podules[podule].fast_base;
	sc->sc_reg = sc->sc_base + 0x800;

	/* The probe could be tidied up a little */

	/* Reset the card */
	SetReg(EH_RESET, 0xff);
	delay(10000);

	/* Explicitly abort remote DMA, mainly to set a legal state */
	PAGE(0);
	eh_stop_controller(sc);
	SetReg(EH_COMMAND, 0x20);	     /* This is wrong */

	SetReg(EH_ISR, 0xf );	             /* Clear any previous work */

	temp = GetReg(EH_CONFIGA);
	temp = GetReg(EH_CONFIGA);
	temp &= ~0x38;
	SetReg(EH_CONFIGA, temp | 3<<3);

	temp = GetReg(EH_CONFIGA);
	temp = GetReg(EH_CONFIGA);

	/* Make the thing interrupt and test it */

	eh_start_controller(sc);

	/* Temporary initialisation, to allow a transmit test */

	SetReg(EH_PSTART, 16384 >> 8);
	SetReg(EH_PSTOP,  16896 >> 8);
	SetReg(EH_BNRY,   16384 >> 8);

	SetReg(EH_TCR, TCR_NORMALLOOP);	/* Erk!! No, put us in loopback */
	SetReg(EH_DCR, DCR_WTS|DCR_LS|DCR_LAS);
	SetReg(EH_RCR, 0);

	/* Transmit some garbage into the loopback */

	SetReg(EH_TPSR, 0x5000);
	SetReg(EH_TBCR0, 0);
	SetReg(EH_TBCR1, 3);

	eh_transmit_command(sc);

	/* The card shouldn't be this quick, so use it as a test */

	if ((GetReg(EH_TSR)&TSR_DONE) != 0)
		PRINTF("eh: Card responded rather quickly %02x\n", GetReg(EH_TSR));

	for (counter=500; counter>0; counter--) {
		if ((GetReg(EH_TSR)&TSR_DONE) != 0)
			break;

		delay(500);
	}

	if (counter == 0) {
		PRINTF("eh: card did not respond\n");
		return 0;
	}

	/* Let it sleep since we're not doing anything with it for now */

	SetReg(EH_ISR, 0);
	SetReg(EH_DCR, DCR_WTS|DCR_LS|DCR_LAS);


	/* Test the board ram.  This code will change */

	/*
	 * Due to problems with some ether H cards the boot option ehbug
	 * can be used to skip this test.
	 * If you system hangs during the eh probe try this option.
	 */

	#define NLEN (0x2000)
	#define NBASE (0x4000)

        if (boot_args) {
        	char *ptr;
       
		ptr = strstr(boot_args, "ehbug");
		if (!ptr) {
			char *test_data;
			char *read_buffer;
			MALLOC(test_data, char *, NLEN, M_TEMP, M_NOWAIT);
			if (test_data == NULL)
				panic("Cannot allocate temporary memory for buffer test (1)");
			MALLOC(read_buffer, char *, NLEN, M_TEMP, M_NOWAIT);
			if (read_buffer == NULL)
				panic("Cannot allocate temporary memory for buffer test (1)");

			printf("1.");

		/* Fill the test_data block */

			for (counter=0; counter<NLEN; counter++) {
				test_data[counter] = (char)(counter&0xff);
				if (test_data[counter] == 0)
					test_data[counter]=0x23;
			}

			printf("2.");
	
			eh_copyout(sc, test_data, NBASE, NLEN);
			printf("3.");
			delay(10000);
			eh_copyin(sc, NBASE, read_buffer, NLEN);
			printf("4.");
	
		        if (bcmp(test_data, read_buffer, NLEN))
				PRINTF("Block test failed\n");
			printf("5.");
			FREE(test_data, M_TEMP);
			FREE(read_buffer, M_TEMP);
		}
	}
	/* This is always true for our ether h */

	sc->sc_physstart = 0x4000;
	sc->sc_physend   = 0x6000;

	/* Get out ethernet address */
	sc->sc_arpcom.ac_enaddr[0] = 0x00;
	sc->sc_arpcom.ac_enaddr[1] = 0x00;
	sc->sc_arpcom.ac_enaddr[2] = 0xc0;
	sc->sc_arpcom.ac_enaddr[3] = 0x41;
	sc->sc_arpcom.ac_enaddr[4] = bootconfig.machine_id[1];
	sc->sc_arpcom.ac_enaddr[5] = bootconfig.machine_id[0];

	/* Copy the ether addr cards filter */

	PAGE(1);
	for (counter=0; counter<6; counter++)
		SetReg(((counter+1)<<2), sc->sc_arpcom.ac_enaddr[counter]);
	PAGE(0);

	/* Tell the podule system about our successful probe */
	pa->pa_podule_number = podule;
	pa->pa_podule = &podules[podule];

	return 1;
}

void
ehattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct eh_softc *sc = (void *) self;
	struct podule_attach_args *pa = (void *)aux;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int irq;
	int temp;

	/* Check a few things about the attach args */
	sc->sc_podule_number = pa->pa_podule_number;
	if (sc->sc_podule_number == -1)
		panic("Podule has disappeared !");

	sc->sc_podule = &podules[sc->sc_podule_number];
	podules[sc->sc_podule_number].attached = 1;

	/* Fill in my application form to attach to the networking system */

	ifp->if_unit = sc->sc_dev.dv_unit;
	ifp->if_name = eh_cd.cd_name;
	ifp->if_start = ehstart;
	ifp->if_ioctl = ehioctl;
	ifp->if_watchdog = ehwatchdog;
	ifp->if_flags = IFF_BROADCAST | IFF_NOTRAILERS;

	/* Signed, dated then sent */

	if_attach(ifp);
	ether_ifattach(ifp);

	/* Not print some stuff for the attach line, starting with the address */

	PRINTF(" address %s", ether_sprintf(sc->sc_arpcom.ac_enaddr));

	/* Now the transfer mode we're operating in */

	switch (sc->sc_flags & MODE_MASK) {
		case MODE_PIO:
			PRINTF(" pio mode");
 			break;
		case MODE_SHRAM:
			PRINTF(" ram mode");
			break;
		default:
			panic("Ether H probe set an unknown mode");
	}

	/* Get an irq handler installed */

	SetReg(EH_IMR, 0);
	SetReg(EH_ISR, 0xff);

	sc->sc_ih.ih_func  = ehintr;
	sc->sc_ih.ih_arg   = sc;
	sc->sc_ih.ih_level = IPL_NET;
	sc->sc_ih.ih_name = "net: eh";
	
	irq = (sc->sc_podule_number>=MAX_PODULES) ? IRQ_NETSLOT : IRQ_PODULE;

	if (irq_claim(irq, &sc->sc_ih))
		panic("Cannot install IRQ handler");

    /* This loopbacks the card on reset, and stops riscos locking */

	if (shutdownhook_establish(eh_shutdown, (void *)sc)==NULL)
    		panic("Cannot install shutdown handler");

    /* Reprogram CONFIGA.  I dont think we should do this any more */

	temp = GetReg(EH_CONFIGA);
	temp = GetReg(EH_CONFIGA);
	temp &= ~0x38;
	SetReg(EH_CONFIGA, temp | (3<<3));

	/* And that's it */

	PRINTF("\n");
}

/****************************************************************************/
/* Net subsystem interface **************************************************/
/****************************************************************************/

#define NEXT_TXBUF (((sc->sc_txcur+1)>=NTXBUF) ? 0 : (sc->sc_txcur+1))

void
ehstart(ifp)
	struct ifnet *ifp;
{
	struct eh_softc *sc = eh_cd.cd_devs[ifp->if_unit];
	int cur;
	struct mbuf *m0, *m;
	int nextbuf;
	char *buffer;
	char txbuf[TXBUF_SIZE];
	int len = 0;

	PAGE(0);

	if ((ifp->if_flags & IFF_OACTIVE) != 0)
		return;

/* This is for pipelined transmit.  At present just use 1 buffer */

	for (;;) {

/* The #ifdef PIPELINE_TRANSMIT code is total screwed */

#ifdef PIPELINE_TRANSMIT
		nextbuf = NEXT_TXBUF;
		if ( nextbuf == sc->sc_txlst )
			ifp->if_flags |= IFF_OACTIVE;
		break;
 	}
#else
		nextbuf = 0;
#endif

		/*** Speed up - In future, copy the bufs direct to the card ***/

		/* Copy a frame out of the mbufs */

		IF_DEQUEUE(&sc->sc_arpcom.ac_if.if_snd, m);
		if ( !m )
			break;

	        buffer = txbuf;

	        len = 0;
		for ( m0=m; m && (len + m->m_len) < TXBUF_SIZE; m=m->m_next ) {
			bcopy(mtod(m,caddr_t), buffer, m->m_len);
			buffer+=m->m_len;
			len+=m->m_len;
		}
	    
		m_freem(m0);
		len = max ( len, ETHER_MIN_LEN );

	        /* And blat it to the card */

		eh_copyout ( sc, txbuf, sc->sc_tbs[nextbuf], len );

#ifdef PIPELINE_TRANSMIT
		sc->sc_txlst = nextbuf;
#endif
 		PAGE(0);

	        SetReg ( EH_TPSR, sc->sc_tbs[nextbuf]>>8 );
	        SetReg ( EH_TBCR0, len&0xff );
	        SetReg ( EH_TBCR1, (len>>8) & 0xff );

	        cur = GetReg(EH_COMMAND);

		eh_transmit_command (sc );

		ifp->if_flags |= IFF_OACTIVE;
		ifp->if_timer = 30;

#ifndef PIPELINE_TRANSMIT
		break;
#endif
	}
}

#define IZSET(a,b) ((a->if_flags&b)!=0)
#define IZCLR(a,b) ((a->if_flags&b)==0)
#define DOSET(a,b) (a->if_flags|=b)
#define DOCLR(a,b) (a->if_flags&=~b)

int
ehioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct eh_softc *sc = eh_cd.cd_devs[ifp->if_unit];
	struct ifaddr *ifa = (struct ifaddr *)data;
	int s = splimp ();
	int error = 0;

	if ((error = ether_ioctl(ifp, &sc->sc_arpcom, cmd, data)) > 0) {
		splx(s);
		return error;
	}

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch ( ifa->ifa_addr->sa_family ) {
#ifdef INET
		case AF_INET:
			ehinit(sc);
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif
		default:
			ehinit(sc);
			break;
		}
		break;

	case SIOCSIFFLAGS:
		sc->promisc = ifp->if_flags & ( IFF_PROMISC | IFF_ALLMULTI );

		if ( IZCLR(ifp,IFF_UP) && IZSET(ifp,IFF_RUNNING) ) {
		    	/* Interface marked down, but still running */
			ehstop(sc);
			DOCLR(ifp,IFF_RUNNING);	
		} else if ( IZSET(ifp,IFF_UP) && IZCLR(ifp,IFF_RUNNING) ) {
			/* Just marked up, not running yet */
			ehinit(sc);
		} else {
			/* Reset to invoke changes in other registers */
			ehstop(sc);
			ehinit(sc);
		}

	default:
		error = EINVAL;
	}

	splx (s);
	return error;
}

void
ehwatchdog(unit)
	int unit;
{
	struct eh_softc *sc = eh_cd.cd_devs[unit];

	log (LOG_ERR, "%s: device timeout\n", sc->sc_dev.dv_xname );
	sc->sc_arpcom.ac_if.if_oerrors++;

	ehinit (sc);
}

/*
 * void eh_stop_controller ( struct eh_softc *sc )
 *
 * Software reset command, takes the controller offline, no packets will be
 * received or transmitted.  Any reception or transmission inprogres will
 * continue to completion before entering the reset state.
 */

void
eh_stop_controller(sc)
	struct eh_softc *sc;
{
	SetReg ( EH_COMMAND, ((GetReg(EH_COMMAND)&0xfc)|1) );
	delay ( 10000 );	/* Change this to wait on the ISR bit */
}

/*
 * void eh_start_controller ( struct eh_softc *sc )
 *
 * Activte the NIC core after either power up or when the NIC Core has
 * been placed in a reset mode by eh_stop_controller or an error.
 */

void
eh_start_controller(sc)
	struct eh_softc *sc;
{
	int temp = GetReg(EH_COMMAND);
	SetReg ( EH_COMMAND, ( (temp&0xfc)|2) );
}

void
eh_transmit_command(sc)
	struct eh_softc *sc;
{
	/* Explicit set here.  There is no other reasonable combination */
	SetReg ( EH_COMMAND, COM_ABORT|COM_STA|COM_TXP);
}

/****************************************************************************/
/* Remote DMA handling ******************************************************/
/****************************************************************************/

/* Remote DMA handling is going to undergo a major overhaul real soon now */

/* We musn't fail */

inline void
eh_ensure_dma_ok(sc)
	struct eh_softc *sc;
{
	register int isr = GetReg ( EH_ISR );
	register int status = GetReg ( EH_COMMAND );
	int dummy;

	if ((status&COM_ABORT)==0) {
		SetReg ( EH_COMMAND, status|COM_ABORT );
		while ((isr&ISR_RDC)==0) {
			dummy = GetReg ( EH_DATA_PORT );
			delay(50);
		}
	}

	/* Reset the COM_TXP bit.  This does not abort transmission */

	SetReg(EH_COMMAND, 0x22);
}

inline int
eh_ensure_dma_competed(sc, type)
	struct eh_softc *sc;
	int type;
{
	register int status = GetReg ( EH_COMMAND );

	if ( (status&COM_ABORT)==0 ) {
	        int dummy=0;
		printf ( "EHDEBUG: Remote %d wasn't finished\n", type );
		SetReg ( EH_COMMAND, status|COM_ABORT );
		switch ( type ) {
		case COM_READ:
			dummy = GetReg ( EH_DATA_PORT );
			break;
		case COM_WRITE:
			SetReg ( EH_DATA_PORT, dummy );
			break;
		}
		ehinit (sc);
		return 1;
	}
	return 0;
}

/* Do an eh_copyin, but take into consideration ring wrap */

int
eh_copyring(sc, src, dest, len)
	struct eh_softc *sc;
	int src;
	char *dest;
	int len;
{
	if ( (src+len)>sc->sc_rbd ) {
		register int in = sc->sc_rbd - src;
		if ( eh_copyin ( sc, src, dest, in )==0 ) {
			printf ( "copyring failed pass 1\n" );
			return 0;
		}
		if ( eh_copyin ( sc, sc->sc_rbs, dest+in, len-in )==0 ) {
			printf ( "copyring failed pass 2\n" );
			return 0;
		}
	} else {
		if ( eh_copyin ( sc, src, dest, len )==0 ) {
			printf ( "copyring faild on 1 pass copy\n" );
			return 0;
		}
	}
	return len;
}

int
eh_copyin(sc, src, dest, len)
	struct eh_softc *sc;
	int src;
	char *dest;
	int len;
{
	int counter;
	int s;
	short *dst = (short *)dest;

	s = splhigh();		/* Erm, maybe not needed now */

	if (len & 1)
		len++;
	if (src & 1) {
		printf ( "EHDEBUG: Copying from odd address\n" );
		src++;
	}

	/* Remote DMA to the DP83905 must be done with care.  If we dont */
	/* do it exactly right, it punishes us by locking the bus. 	     */

	eh_ensure_dma_ok (sc);

	/* Set up the DMA */

	SetReg ( EH_RSAR0, src & 0xff );
	SetReg ( EH_RSAR1, (src>>8) & 0xff );
	SetReg ( EH_RBCR0, len & 0xff );
	SetReg ( EH_RBCR1, (len>>8) & 0xff );

	/* Set the DMA running */
	/* REMOTE_DMA ( COM_READ ); */ /* SetReg ( EH_COMMAND, 0x0a ); */
	SetReg ( EH_COMMAND, COM_READ|COM_STA );

	for ( counter=0; counter<len; counter+=2 )
		*(dst++)=ReadShort ( sc->sc_reg + EH_DATA_PORT );

	splx(s);

	if ( eh_ensure_dma_competed ( sc, COM_READ ) )
		return 0;

	return len;
}

int
eh_copyout(sc, src, dest, len)
	struct eh_softc *sc;
	char *src;
	int dest;
	int len;
{
	int counter;
	int s;
	short *sr = (short *)src;

	if (len & 1)
		len++;
	if (dest & 1) {
		printf ( "EHDEBUG: Copying to odd address\n" );
		dest++;
	}

	s = splhigh();		/* Erm, maybe not needed now */

	/* Remote DMA to the DP83905 must be done with care.  If we dont */
	/* do it exactly right, it punishes us by locking the bus. 	     */

	eh_ensure_dma_ok (sc);

	/* Set up the DMA */

	SetReg ( EH_RSAR0, dest & 0xff );
	SetReg ( EH_RSAR1, (dest>>8) & 0xff );
	SetReg ( EH_RBCR0, len & 0xff );
	SetReg ( EH_RBCR1, (len>>8) & 0xff );

	/* Set the DMA running */
	/*REMOTE_DMA ( COM_WRITE );*/ /* SetReg ( EH_COMMAND, 0x0a ); */
	SetReg ( EH_COMMAND, COM_WRITE|COM_STA );

	for ( counter=0; counter<len; counter+=2 ) {
		WriteShort ( sc->sc_reg + EH_DATA_PORT, *(sr) );
		sr++;
	}

	splx(s);

	if ( eh_ensure_dma_competed ( sc, COM_WRITE ) )
		return 0;

	return len;
}

#define ALLOCBLK(v,s)	(v)=card_freestart;	\
			card_freestart+=((s)+0xff)&(~0xff)

void
ehinit(sc)
	struct eh_softc *sc;
{
	int card_freestart;
	int counter;

	PAGE(0);

	/* Lay out the cards ram */
	card_freestart = sc->sc_physstart;

	/* Allocate receive buffers */

	ALLOCBLK ( sc->sc_rbs, (RXBUF_SIZE*NRXBUF) );
	sc->sc_rbd = card_freestart;
	sc->sc_nxtpkt = sc->sc_rbs + 0x100;

	/* Allocate transmit buffers */
	for ( counter=0; counter<NTXBUF; counter++ ) {
		ALLOCBLK ( sc->sc_tbs[counter], TXBUF_SIZE );
	}

	if ( card_freestart > sc->sc_physend ) {
		printf ( "eh: card short of ram %02x required %02x present\n",
		    card_freestart, sc->sc_physstart );
	        sc->sc_arpcom.ac_if.if_flags &= ~IFF_RUNNING;
		sc->sc_arpcom.ac_if.if_flags |= IFF_OACTIVE;
		return;
	}

	/*
	 * Bring the controller up
	 *
	 * The ordering here is critical.  Dont change unless you know what
	 * you're doing
	 */

	/* Reset the card */

	SetReg ( EH_RESET, 0xff );
	delay (50000);
	SetReg ( EH_RESET, 0x00 );
	delay (50000);

	/* 1. Program the command register for page 0 */

	SetReg ( EH_COMMAND, 0x21 );

	/* 2. Initialize the Data configuration register */

	SetReg ( EH_DCR, DCR_WTS|DCR_LS|(0<<5) );

	/* 3. Clear RBCR if using remote DMA */

	SetReg ( EH_RBCR0, 0 );
	SetReg ( EH_RBCR1, 0 );

	/* 4. Initialise RCR */

	SetReg ( EH_RCR, 0x04 );

	/* 5. Place the controller in loopback mode 1 or 2  TCR=02H or 04H */

	SetReg ( EH_TCR, 0x02 );

	/* 6. Initiliase Receive ring buffer BNDRY PSTART PSTOP */

	SetReg ( EH_BNRY,   sc->sc_rbs >> 8 );
	SetReg ( EH_PSTART, sc->sc_rbs >> 8 );
	SetReg ( EH_PSTOP,  sc->sc_rbd >> 8 );

	/* 7. Clear ISR */

	SetReg ( EH_ISR, 0xff );

	/* 8. Initialise IMR */

	SetReg ( EH_IMR, ISR_PRX|ISR_PTX|ISR_RXE|ISR_TXE|ISR_OVW|ISR_CNT );

	/* 9. Program command register for page 1 COM=0x61 	*/
	/*    Initialise PAR MAR and CURR 			*/

	PAGE(1);

	SetReg ( EH_CURR,   sc->sc_nxtpkt >> 8 );

	for ( counter=0; counter<6; counter++ )
		SetReg ( ((counter+1)<<2), sc->sc_arpcom.ac_enaddr[counter] );

	/* Put controller into start mode COM = 0x22 */

	SetReg ( EH_COMMAND, 0x22 );

	/* Initialise the TCR */

	SetReg ( EH_TCR, TCR_NORMALLOOP );

	/* Interface is up */

	sc->sc_arpcom.ac_if.if_flags |= IFF_RUNNING;
	sc->sc_arpcom.ac_if.if_flags &= ~IFF_OACTIVE;

	ehstart(&sc->sc_arpcom.ac_if);
}

int
ehstop(sc)
	struct eh_softc *sc;
{
	int s = splimp();
	eh_stop_controller(sc);
	SetReg(EH_DCR, DCR_LS|DCR_LAS);
	splx(s);
	return 0;
}

#define INTR_ACK(s) SetReg ( EH_ISR, s )

/* In need of tidying */

#undef MYTEST

void
eh_rint(sc)
	struct eh_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct mbuf *top, **mp, *m;
	struct ether_header eh;
	struct eh_rxhdr hdr;
	int status = GetReg(EH_RSR);
	int rxstatus=0;
	int totlen, len;
	int ptr;
	int rbend;
	int thispacket;
	int bnry;

#ifdef MYTEST
	int old;
#endif

	/* Get the end of the ring buffer */

	PAGE(1);
	rbend = GetReg ( EH_CURR ) << 8;
	PAGE(0);

	/* If we have something to receive then receive it */

	if ( status&RSR_PRX ) {
		/* Remove all the packets from the ring buffer */
		for ( ptr=sc->sc_nxtpkt; ptr!=rbend ; ptr = sc->sc_nxtpkt ) {

			thispacket = ptr;
			/* Copy the ether h receive header */

			if ( eh_copyring ( sc, ptr, (char *)&hdr, 4 ) != 4 ) {
				printf ( "eh: Failed copying in header\n" );
				printf ( "eh: thispacket %02x\n", thispacket );
				ehinit (sc);
				return;
			}

			rxstatus = hdr.rx_status;
			totlen = (hdr.rx_rbc1<<8) + hdr.rx_rbc0;

        		ptr+=4;

			/* Check the packet's status */
			if ( hdr.rx_status&(RSR_CRC|RSR_FAE|RSR_FO|RSR_MPA) ) {
				printf ( "eh: intact packet is corrupt %02x %04x\n",
				    hdr.rx_status, ptr );
			}

			if (((hdr.rx_nxtpkt<<8)>sc->sc_rbd) || ((hdr.rx_nxtpkt<<8)<sc->sc_rbs)) {
				printf ( "eh: ring buffer empty %04x %04x\n",
				    hdr.rx_nxtpkt, ptr );
				ehinit (sc);
				return;
			}

			if (0)
				printf ( "pulling packet at %08x, next at %08x\n", thispacket, sc->sc_nxtpkt );

			if ( totlen>ETHER_MAX_LEN ) {
				printf ( "eh: giant packet received %04x\n", totlen );
				totlen = ETHER_MAX_LEN;
			}

			/* Copy the ether header */

			if ((eh_copyring ( sc, ptr, (char *)&eh, sizeof (eh)))!=sizeof(eh))  {
				printf ( "eh: Failed copying in ethenet header\n" );
				return;
			}
			ptr+=sizeof(eh);
			totlen-=sizeof(eh);

			/* Copy the packet into mbufs */

			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if ( m==0 )
				return;
			m->m_pkthdr.rcvif = ifp;
			m->m_pkthdr.len   = totlen;
			len = MHLEN;
			top = 0;
			mp = &top;

			while ( totlen > 0 ) {
				if (top) {
					MGET(m, M_DONTWAIT, MT_DATA);
					if ( m==0 ) {
						m_freem(top);
						goto skip;
					}
					len = MLEN;
				}
				if ( totlen >= MINCLSIZE ) {
					MCLGET(m, M_DONTWAIT);
					if (m->m_flags & M_EXT)
					len = MCLBYTES;
				}
				m->m_len = len = min ( totlen, len );

				if ((eh_copyring ( sc, ptr, mtod(m, caddr_t), len))!=len)
					printf ( "eh: failed copying in buffer %d\n", len );

				ptr += len;
				totlen -= len;
				*mp = m;
				mp = &m->m_next;
			}

			m = top;
			ifp->if_ipackets++;
			ether_input(ifp, &eh, m );
skip:

			/* Ok, I'm done with this packet */

			sc->sc_nxtpkt = hdr.rx_nxtpkt << 8;

			/* Set the boundary pointer on the ring buffer */

			bnry = (sc->sc_nxtpkt>>8)-1;
			if ( bnry < (sc->sc_rbs>>8) )
				bnry = (sc->sc_rbd>>8)-1;
			SetReg ( EH_BNRY, bnry );
	
			PAGE(1);
			rbend = GetReg ( EH_CURR ) << 8;
			PAGE(0);
		}
	}
}

int
ehintr(arg)
	void *arg;
{
	struct eh_softc *sc = arg;
	register int isr = GetReg ( EH_ISR );	/* Is char of int faster ? */
	register int times = 0;

	PAGE(0);

	if ( isr & ISR_RXE ) {
		int status = GetReg ( EH_RSR );
		PRINTF ( "eh: Receive Error:" );
		if ( status&RSR_PRX )
			PRINTF ( " packet received intact (this should happen)" );
		if ( status&RSR_CRC )
			PRINTF ( " crc error" );
		if ( status&RSR_FAE )
			PRINTF ( " frame alignment error" );
		if ( status&RSR_FO )
			PRINTF ( " fifo overrun" );
		if ( status&RSR_MPA )
			PRINTF ( " missed packet" );
		if ( status&RSR_DIS )
			PRINTF ( " receiver disabled" );
		printf ( "\n" );
		INTR_ACK ( ISR_RXE );
		ehinit (sc);
		return 0;
	}

	if ( isr & ISR_PRX ) {
		eh_rint (sc);
		INTR_ACK ( ISR_PRX );
	}

	if ( isr & ISR_PTX ) {
		struct ifnet *ifp = &sc->sc_arpcom.ac_if;
		int status;
		INTR_ACK ( ISR_PTX );
		ifp->if_timer=0;
		ifp->if_flags &= ~IFF_OACTIVE;

		status = GetReg ( EH_TSR );

		if ( status&TSR_PTX )
			ifp->if_opackets++;

		if ( status&TSR_COL )
			ifp->if_collisions+=GetReg(EH_NCR);

		if ( status&TSR_ABT )
			PRINTF ( "eh: too many collisions\n" );

		if ( status&TSR_CRS )
			PRINTF ( "eh: no carrier\n" );

		if ( status&TSR_CDH )
			PRINTF ( "eh: tranceiver failure, no heartbeat \n" );

		if ( status&TSR_OWC )
			PRINTF ( "eh: out of window \n" );

		if ( status&(TSR_ABT|TSR_CRS|TSR_FU|TSR_CDH|TSR_OWC) )
			ifp->if_oerrors++;

		ehstart(ifp);	
	}

	if ( isr & ISR_TXE ) {
		INTR_ACK ( ISR_TXE );
		PRINTF ( "ehintr: Transmit error\n" );
	}

	if ( isr & ISR_OVW ) {
		INTR_ACK ( ISR_OVW );
		PRINTF ( "ehintr: Counter overflow\n" );
	}

	if ( isr & ISR_RST ) {
		INTR_ACK ( ISR_RST );
		PRINTF ( "ehintr: Reset status\n" );
	}

	if ((times++)>16) {
		PRINTF ( "eh: possible interrupt jammed on." );
		SetReg ( EH_IMR, 0x0 );
	}

	/* Dont do this for the mo until I'm sure.

	isr = GetReg ( EH_ISR );

	if ( (isr&GetReg(EH_IMR))!=0 )
		goto more;
	*/

	if ( (isr&GetReg(EH_IMR))!=0 )
		printf ( "eh: Interrupt not serviced\n" );

	return 0;
}

/****************************************************************************/
/* Auxilary routines ********************************************************/
/****************************************************************************/

void
eh_shutdown(arg)
	void *arg;
{
	struct eh_softc *sc = (struct eh_softc *)arg;

	/* On shutdown put us in loopback */

	SetReg(EH_DCR, DCR_LAS);
	SetReg(EH_TCR, TCR_NICMOD);

	/* and program remote dma so riscos doesnt lock */

	SetReg(EH_RSAR0, 0);
	SetReg(EH_RSAR1, 1);
	SetReg(EH_RBCR0, 0);
	SetReg(EH_RBCR1, 1);
}
