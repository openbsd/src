/*	$OpenBSD: stp4020.c,v 1.3 2002/06/20 02:43:23 deraadt Exp $	*/
/*	$NetBSD: stp4020.c,v 1.23 2002/06/01 23:51:03 lukem Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * STP4020: SBus/PCMCIA bridge supporting two Type-3 PCMCIA cards.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/extent.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/device.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciachip.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/sbus/sbusvar.h>
#include <dev/sbus/stp4020reg.h>

/*
 * We use the three available windows per socket in a simple, fixed
 * arrangement. Each window maps (at full 1 MB size) one of the pcmcia
 * spaces into sbus space.
 */
#define STP_WIN_ATTR	0	/* index of the attribute memory space window */
#define	STP_WIN_MEM	1	/* index of the common memory space window */
#define	STP_WIN_IO	2	/* index of the io space window */


#if defined(STP4020_DEBUG)
int stp4020_debug = 0;
#define DPRINTF(x)	do { if (stp4020_debug) printf x; } while(0)
#else
#define DPRINTF(x)
#endif

/*
 * Event queue; events detected in an interrupt context go here
 * awaiting attention from our event handling thread.
 */
struct stp4020_event {
	SIMPLEQ_ENTRY(stp4020_event) se_q;
	int	se_type;
	int	se_sock;
};

/* Defined event types */
#define STP4020_EVENT_INSERTION	0
#define STP4020_EVENT_REMOVAL	1

/*
 * Per socket data.
 */
struct stp4020_socket {
	struct stp4020_softc	*sc;	/* Back link */
	int		flags;
#define STP4020_SOCKET_BUSY	0x0001
#define STP4020_SOCKET_SHUTDOWN	0x0002
	int		sock;		/* Socket number (0 or 1) */
	bus_space_tag_t	tag;		/* socket control space */
	bus_space_handle_t	regs;	/* 			*/
	struct device	*pcmcia;	/* Associated PCMCIA device */
	int		(*intrhandler)	/* Card driver interrupt handler */
			    (void *);
	void		*intrarg;	/* Card interrupt handler argument */
	int		ipl;		/* Interrupt level suggested by card */
	struct {
		bus_space_handle_t	winaddr;/* this window's address */
	} windows[STP4020_NWIN];

};

struct stp4020_softc {
	struct device	sc_dev;		/* Base device */
	struct sbusdev	sc_sd;		/* SBus device */
	bus_space_tag_t	sc_bustag;
	bus_dma_tag_t	sc_dmatag;
	pcmcia_chipset_tag_t	sc_pct;	/* Chipset methods */

	struct proc	*event_thread;		/* event handling thread */
	SIMPLEQ_HEAD(, stp4020_event)	events;	/* Pending events for thread */

	struct stp4020_socket sc_socks[STP4020_NSOCK];
};


int	stp4020print(void *, const char *);
int	stpmatch(struct device *, void *, void *);
void	stpattach(struct device *, struct device *, void *);
int	stp4020_iointr(void *);
int	stp4020_statintr(void *);
void	stp4020_map_window(struct stp4020_socket *, int, int);
void	stp4020_calc_speed(int, int, int *, int *);

struct cfattach stp_ca = {
	sizeof(struct stp4020_softc), stpmatch, stpattach
};

struct cfdriver stp_cd = {
	NULL, "stp", DV_DULL
};

#ifdef STP4020_DEBUG
static void	stp4020_dump_regs(struct stp4020_socket *);
#endif

static int	stp4020_rd_sockctl(struct stp4020_socket *, int);
static void	stp4020_wr_sockctl(struct stp4020_socket *, int, int);
static int	stp4020_rd_winctl(struct stp4020_socket *, int, int);
static void	stp4020_wr_winctl(struct stp4020_socket *, int, int, int);

void	stp4020_delay(unsigned int);
void	stp4020_attach_socket(struct stp4020_socket *, int);
void	stp4020_create_event_thread(void *);
void	stp4020_event_thread(void *);
void	stp4020_queue_event(struct stp4020_softc *, int, int);

int	stp4020_chip_mem_alloc(pcmcia_chipset_handle_t, bus_size_t,
	    struct pcmcia_mem_handle *);
void	stp4020_chip_mem_free(pcmcia_chipset_handle_t,
	    struct pcmcia_mem_handle *);
int	stp4020_chip_mem_map(pcmcia_chipset_handle_t, int, bus_addr_t,
	    bus_size_t, struct pcmcia_mem_handle *, bus_addr_t *, int *);
void	stp4020_chip_mem_unmap(pcmcia_chipset_handle_t, int);

int	stp4020_chip_io_alloc(pcmcia_chipset_handle_t,
	    bus_addr_t, bus_size_t, bus_size_t, struct pcmcia_io_handle *);
void	stp4020_chip_io_free(pcmcia_chipset_handle_t,
	    struct pcmcia_io_handle *);
int	stp4020_chip_io_map(pcmcia_chipset_handle_t, int, bus_addr_t,
	    bus_size_t, struct pcmcia_io_handle *, int *);
void	stp4020_chip_io_unmap(pcmcia_chipset_handle_t, int);

void	stp4020_chip_socket_enable(pcmcia_chipset_handle_t);
void	stp4020_chip_socket_disable(pcmcia_chipset_handle_t);
void	*stp4020_chip_intr_establish(pcmcia_chipset_handle_t,
	    struct pcmcia_function *, int, int (*) (void *), void *, char *);
void	stp4020_chip_intr_disestablish(pcmcia_chipset_handle_t, void *);

/* Our PCMCIA chipset methods */
static struct pcmcia_chip_functions stp4020_functions = {
	stp4020_chip_mem_alloc,
	stp4020_chip_mem_free,
	stp4020_chip_mem_map,
	stp4020_chip_mem_unmap,

	stp4020_chip_io_alloc,
	stp4020_chip_io_free,
	stp4020_chip_io_map,
	stp4020_chip_io_unmap,

	stp4020_chip_intr_establish,
	stp4020_chip_intr_disestablish,

	stp4020_chip_socket_enable,
	stp4020_chip_socket_disable
};


static __inline__ int
stp4020_rd_sockctl(h, idx)
	struct stp4020_socket *h;
	int idx;
{
	int o = ((STP4020_SOCKREGS_SIZE * (h->sock)) + idx);
	return (bus_space_read_2(h->tag, h->regs, o));
}

static __inline__ void
stp4020_wr_sockctl(h, idx, v)
	struct stp4020_socket *h;
	int idx;
	int v;
{
	int o = (STP4020_SOCKREGS_SIZE * (h->sock)) + idx;
	bus_space_write_2(h->tag, h->regs, o, v);
}

static __inline__ int
stp4020_rd_winctl(h, win, idx)
	struct stp4020_socket *h;
	int win;
	int idx;
{
	int o = (STP4020_SOCKREGS_SIZE * (h->sock)) +
	    (STP4020_WINREGS_SIZE * win) + idx;
	return (bus_space_read_2(h->tag, h->regs, o));
}

static __inline__ void
stp4020_wr_winctl(h, win, idx, v)
	struct stp4020_socket *h;
	int win;
	int idx;
	int v;
{
	int o = (STP4020_SOCKREGS_SIZE * (h->sock)) +
	    (STP4020_WINREGS_SIZE * win) + idx;
	bus_space_write_2(h->tag, h->regs, o, v);
}


int
stp4020print(aux, busname)
	void *aux;
	const char *busname;
{
	struct pcmciabus_attach_args *paa = aux;
	struct stp4020_socket *h = paa->pch;

	printf(" socket %d", h->sock);
	return (UNCONF);
}

int
stpmatch(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct sbus_attach_args *sa = aux;

	return (strcmp("SUNW,pcmcia", sa->sa_name) == 0);
}

/*
 * Attach all the sub-devices we can find
 */
void
stpattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sbus_attach_args *sa = aux;
	struct stp4020_softc *sc = (void *)self;
	int node, rev;
	int i;
	bus_space_handle_t bh;

	node = sa->sa_node;

	/* Transfer bus tags */
	sc->sc_bustag = sa->sa_bustag;
	sc->sc_dmatag = sa->sa_dmatag;

	/* Set up per-socket static initialization */
	sc->sc_socks[0].sc = sc->sc_socks[1].sc = sc;
	sc->sc_socks[0].tag = sc->sc_socks[1].tag = sa->sa_bustag;

	if (sa->sa_nreg < 8) {
		printf(": only %d register sets\n", sa->sa_nreg);
		return;
	}

	if (sa->sa_nintr != 2) {
		printf(": expect 2 interrupt Sbus levels; got %d\n",
		    sa->sa_nintr);
		return;
	}

#define STP4020_BANK_PROM	0
#define STP4020_BANK_CTRL	4
	for (i = 0; i < 8; i++) {

		/*
		 * STP4020 Register address map:
		 *	bank  0:   Forth PROM
		 *	banks 1-3: socket 0, windows 0-2
		 *	bank  4:   control registers
		 *	banks 5-7: socket 1, windows 0-2
		 */

		if (i == STP4020_BANK_PROM)
			/* Skip the PROM */
			continue;

		if (sbus_bus_map(sa->sa_bustag, sa->sa_reg[i].sbr_slot,
		    sa->sa_reg[i].sbr_offset, sa->sa_reg[i].sbr_size,
		    BUS_SPACE_MAP_LINEAR, 0, &bh) != 0) {
			printf(": attach: cannot map registers\n");
			return;
		}

		if (i == STP4020_BANK_CTRL) {
			/*
			 * Copy tag and handle to both socket structures
			 * for easy access in control/status IO functions.
			 */
			sc->sc_socks[0].regs = sc->sc_socks[1].regs = bh;
		} else if (i < STP4020_BANK_CTRL) {
			/* banks 1-3 */
			sc->sc_socks[0].windows[i-1].winaddr = bh;
		} else {
			/* banks 5-7 */
			sc->sc_socks[1].windows[i-5].winaddr = bh;
		}
	}

	sbus_establish(&sc->sc_sd, &sc->sc_dev);

	/*
	 * We get to use two SBus interrupt levels.
	 * The higher level we use for status change interrupts;
	 * the lower level for PC card I/O.
	 */
	if (sa->sa_nintr != 0) {
		bus_intr_establish(sa->sa_bustag, sa->sa_intr[1].sbi_pri,
		    IPL_NONE, 0, stp4020_statintr, sc);

		bus_intr_establish(sa->sa_bustag, sa->sa_intr[0].sbi_pri,
		    IPL_NONE, 0, stp4020_iointr, sc);
	}

	rev = stp4020_rd_sockctl(&sc->sc_socks[0], STP4020_ISR1_IDX) &
	    STP4020_ISR1_REV_M;
	printf(": rev %x\n", rev);

	sc->sc_pct = (pcmcia_chipset_tag_t)&stp4020_functions;

	/*
	 * Arrange that a kernel thread be created to handle
	 * insert/removal events.
	 */
	SIMPLEQ_INIT(&sc->events);
	kthread_create_deferred(stp4020_create_event_thread, sc);

	for (i = 0; i < STP4020_NSOCK; i++) {
		struct stp4020_socket *h = &sc->sc_socks[i];
		h->sock = i;
		h->sc = sc;
#ifdef STP4020_DEBUG
		if (stp4020_debug)
			stp4020_dump_regs(h);
#endif
		stp4020_attach_socket(h, sa->sa_frequency);
	}
}

void
stp4020_attach_socket(h, speed)
	struct stp4020_socket *h;
	int speed;
{
	struct pcmciabus_attach_args paa;
	int v;

	/* Map all three windows */
	stp4020_map_window(h, STP_WIN_ATTR, speed);
	stp4020_map_window(h, STP_WIN_MEM, speed);
	stp4020_map_window(h, STP_WIN_IO, speed);

	/* Configure one pcmcia device per socket */
	paa.paa_busname = "pcmcia";
	paa.pct = (pcmcia_chipset_tag_t)h->sc->sc_pct;
	paa.pch = (pcmcia_chipset_handle_t)h;
	paa.iobase = 0;
	paa.iosize = STP4020_WINDOW_SIZE;

	h->pcmcia = config_found(&h->sc->sc_dev, &paa, stp4020print);

	if (h->pcmcia == NULL)
		return;

	/*
	 * There's actually a pcmcia bus attached; initialize the slot.
	 */

	/*
	 * Clear things up before we enable status change interrupts.
	 * This seems to not be fully initialized by the PROM.
	 */
	stp4020_wr_sockctl(h, STP4020_ICR1_IDX, 0);
	stp4020_wr_sockctl(h, STP4020_ICR0_IDX, 0);
	stp4020_wr_sockctl(h, STP4020_ISR1_IDX, 0x3fff);
	stp4020_wr_sockctl(h, STP4020_ISR0_IDX, 0x3fff);

	/*
	 * Enable socket status change interrupts.
	 * We use SB_INT[1] for status change interrupts.
	 */
	v = STP4020_ICR0_ALL_STATUS_IE | STP4020_ICR0_SCILVL_SB1;
	stp4020_wr_sockctl(h, STP4020_ICR0_IDX, v);

	/* Get live status bits from ISR0 */
	v = stp4020_rd_sockctl(h, STP4020_ISR0_IDX);
	if ((v & (STP4020_ISR0_CD1ST | STP4020_ISR0_CD2ST)) == 0)
		return;

	pcmcia_card_attach(h->pcmcia);
	h->flags |= STP4020_SOCKET_BUSY;
}


/*
 * Deferred thread creation callback.
 */
void
stp4020_create_event_thread(arg)
	void *arg;
{
	struct stp4020_softc *sc = arg;
	const char *name = sc->sc_dev.dv_xname;

	if (kthread_create(stp4020_event_thread, sc, &sc->event_thread,
	    "%s", name)) {
		panic("%s: unable to create event thread", name);
	}
}

/*
 * The actual event handling thread.
 */
void
stp4020_event_thread(arg)
	void *arg;
{
	struct stp4020_softc *sc = arg;
	struct stp4020_event *e;
	int s;

	while (1) {
		struct stp4020_socket *h;
		int n;

		s = splhigh();
		if ((e = SIMPLEQ_FIRST(&sc->events)) == NULL) {
			splx(s);
			(void)tsleep(&sc->events, PWAIT, "pcicev", 0);
			continue;
		}
		SIMPLEQ_REMOVE_HEAD(&sc->events, e, se_q);
		splx(s);

		n = e->se_sock;
		if (n < 0 || n >= STP4020_NSOCK)
			panic("stp4020_event_thread: wayward socket number %d",
			    n);

		h = &sc->sc_socks[n];
		switch (e->se_type) {
		case STP4020_EVENT_INSERTION:
			pcmcia_card_attach(h->pcmcia);
			break;
		case STP4020_EVENT_REMOVAL:
			pcmcia_card_detach(h->pcmcia, DETACH_FORCE);
			break;
		default:
			panic("stp4020_event_thread: unknown event type %d",
			    e->se_type);
		}
		free(e, M_TEMP);
	}
}

void
stp4020_queue_event(sc, sock, event)
	struct stp4020_softc *sc;
	int sock, event;
{
	struct stp4020_event *e;
	int s;

	e = malloc(sizeof(*e), M_TEMP, M_NOWAIT);
	if (e == NULL)
		panic("stp4020_queue_event: can't allocate event");

	e->se_type = event;
	e->se_sock = sock;
	s = splhigh();
	SIMPLEQ_INSERT_TAIL(&sc->events, e, se_q);
	splx(s);
	wakeup(&sc->events);
}

int
stp4020_statintr(arg)
	void *arg;
{
	struct stp4020_softc *sc = arg;
	int i, r = 0;

	/*
	 * Check each socket for pending requests.
	 */
	for (i = 0 ; i < STP4020_NSOCK; i++) {
		struct stp4020_socket *h;
		int v, cd_change = 0;

		h = &sc->sc_socks[i];

		/* Read socket's ISR0 for the interrupt status bits */
		v = stp4020_rd_sockctl(h, STP4020_ISR0_IDX);

#ifdef STP4020_DEBUG
		if (stp4020_debug != 0) {
			char bits[64];
			bitmask_snprintf(v, STP4020_ISR0_IOBITS,
			    bits, sizeof(bits));
			printf("stp4020_statintr: ISR0=%s\n", bits);
		}
#endif

		/* Ack all interrupts at once */
		stp4020_wr_sockctl(h, STP4020_ISR0_IDX,
		    STP4020_ISR0_ALL_STATUS_IRQ);

		if ((v & STP4020_ISR0_CDCHG) != 0) {
			/*
			 * Card status change detect
			 */
			cd_change = 1;
			r = 1;
			if ((v & (STP4020_ISR0_CD1ST|STP4020_ISR0_CD2ST)) ==
			    (STP4020_ISR0_CD1ST|STP4020_ISR0_CD2ST)) {
				if ((h->flags & STP4020_SOCKET_BUSY) == 0) {
					stp4020_queue_event(sc, i,
					    STP4020_EVENT_INSERTION);
					h->flags |= STP4020_SOCKET_BUSY;
				}
			}
			if ((v & (STP4020_ISR0_CD1ST|STP4020_ISR0_CD2ST)) ==
			    0) {
				if ((h->flags & STP4020_SOCKET_BUSY) != 0) {
					stp4020_queue_event(sc, i,
					    STP4020_EVENT_REMOVAL);
					h->flags &= ~STP4020_SOCKET_BUSY;
				}
			}
		}

		/* informational messages */
		if ((v & STP4020_ISR0_BVD1CHG) != 0) {
			/* ignore if this is caused by insert or removal */
			if (!cd_change)
				printf("stp4020[%d]: Battery change 1\n",
				    h->sock);
			r = 1;
		}

		if ((v & STP4020_ISR0_BVD2CHG) != 0) {
			/* ignore if this is caused by insert or removal */
			if (!cd_change)
				printf("stp4020[%d]: Battery change 2\n",
				    h->sock);
			r = 1;
		}

		if ((v & STP4020_ISR0_RDYCHG) != 0) {
			DPRINTF(("stp4020[%d]: Ready/Busy change\n",
			    h->sock));
			r = 1;
		}

		if ((v & STP4020_ISR0_WPCHG) != 0) {
			DPRINTF(("stp4020[%d]: Write protect change\n",
			    h->sock));
			r = 1;
		}

		if ((v & STP4020_ISR0_PCTO) != 0) {
			DPRINTF(("stp4020[%d]: Card access timeout\n",
			    h->sock));
			r = 1;
		}
	}

	return (r);
}

int
stp4020_iointr(arg)
	void *arg;
{
	struct stp4020_softc *sc = arg;
	int i, r = 0;

	/*
	 * Check each socket for pending requests.
	 */
	for (i = 0 ; i < STP4020_NSOCK; i++) {
		struct stp4020_socket *h;
		int v;

		h = &sc->sc_socks[i];
		v = stp4020_rd_sockctl(h, STP4020_ISR0_IDX);

		if ((v & STP4020_ISR0_IOINT) != 0) {
			/* we can not deny this is ours, no matter what the
			   card driver says. */
			r = 1;

			/* ack interrupt */
			stp4020_wr_sockctl(h, STP4020_ISR0_IDX, v);

			/* It's a card interrupt */
			if ((h->flags & STP4020_SOCKET_BUSY) == 0) {
				printf("stp4020[%d]: spurious interrupt?\n",
				    h->sock);
				continue;
			}
			/* Call card handler, if any */
			if (h->intrhandler != NULL) {
				/*
				 * Called without handling of it's requested
				 * protection level (h->ipl), since we have
				 * no general queuing mechanism available
				 * right now and we know for sure we are
				 * running at a higher protection level
				 * right now.
				 */
				(*h->intrhandler)(h->intrarg);
			}
		}
	}

	return (r);
}

/*
 * The function gets the sbus speed and a access time and calculates
 * values for the CMDLNG and CMDDLAY registers.
 */
void
stp4020_calc_speed(int bus_speed, int ns, int *length, int *delay)
{
	int result;

	if (ns < STP4020_MEM_SPEED_MIN)
		ns = STP4020_MEM_SPEED_MIN;
	else if (ns > STP4020_MEM_SPEED_MAX)
		ns = STP4020_MEM_SPEED_MAX;
	result = ns * (bus_speed / 1000);
	if (result % 1000000)
		result = result / 1000000 + 1;
	else
		result /= 1000000;
	*length = result;

	/* the sbus frequency range is limited, so we can keep this simple */
	*delay = ns <= STP4020_MEM_SPEED_MIN ? 1 : 2;
}

void
stp4020_map_window(struct stp4020_socket *h, int win, int speed)
{
	int v, length, delay;

	/*
	 * According to the PC Card standard 300ns access timing should be
	 * used for attribute memory access. Our pcmcia framework does not
	 * seem to propagate timing information, so we use that
	 * everywhere.
	 */
	stp4020_calc_speed(speed, 300, &length, &delay);

	/*
	 * Fill in the Address Space Select and Base Address
	 * fields of this windows control register 0.
	 */
	v = ((delay << STP4020_WCR0_CMDDLY_S) & STP4020_WCR0_CMDDLY_M) |
	    ((length << STP4020_WCR0_CMDLNG_S) & STP4020_WCR0_CMDLNG_M);
	switch (win) {
	case STP_WIN_ATTR:
		v |= STP4020_WCR0_ASPSEL_AM;
		break;
	case STP_WIN_MEM:
		v |= STP4020_WCR0_ASPSEL_CM;
		break;
	case STP_WIN_IO:
		v |= STP4020_WCR0_ASPSEL_IO;
		break;
	}
	v |= (STP4020_ADDR2PAGE(0) & STP4020_WCR0_BASE_M);
	stp4020_wr_winctl(h, win, STP4020_WCR0_IDX, v);
	stp4020_wr_winctl(h, win, STP4020_WCR1_IDX,
	    1 << STP4020_WCR1_WAITREQ_S);
}

int
stp4020_chip_mem_alloc(pch, size, pcmhp)
	pcmcia_chipset_handle_t pch;
	bus_size_t size;
	struct pcmcia_mem_handle *pcmhp;
{
	struct stp4020_socket *h = (struct stp4020_socket *)pch;

	/* we can not do much here, defere work to _mem_map */
	pcmhp->memt = h->tag;
	pcmhp->size = size;
	pcmhp->addr = 0;
	pcmhp->mhandle = 0;
	pcmhp->realsize = size;

	return (0);
}

void
stp4020_chip_mem_free(pch, pcmhp)
	pcmcia_chipset_handle_t pch;
	struct pcmcia_mem_handle *pcmhp;
{
}

int
stp4020_chip_mem_map(pch, kind, card_addr, size, pcmhp, offsetp, windowp)
	pcmcia_chipset_handle_t pch;
	int kind;
	bus_addr_t card_addr;
	bus_size_t size;
	struct pcmcia_mem_handle *pcmhp;
	bus_addr_t *offsetp;
	int *windowp;
{
	struct stp4020_socket *h = (struct stp4020_socket *)pch;
	int win = (kind & PCMCIA_MEM_ATTR) ? STP_WIN_ATTR : STP_WIN_MEM;

	pcmhp->memt = h->tag;
	bus_space_subregion(h->tag, h->windows[win].winaddr,
	    card_addr, size, &pcmhp->memh);
	pcmhp->size = size;
	pcmhp->realsize = STP4020_WINDOW_SIZE - card_addr;
	*offsetp = 0;
	*windowp = 0;

	return (0);
}

void
stp4020_chip_mem_unmap(pch, win)
	pcmcia_chipset_handle_t pch;
	int win;
{
}

int
stp4020_chip_io_alloc(pch, start, size, align, pcihp)
	pcmcia_chipset_handle_t pch;
	bus_addr_t start;
	bus_size_t size;
	bus_size_t align;
	struct pcmcia_io_handle *pcihp;
{
	struct stp4020_socket *h = (struct stp4020_socket *)pch;

	pcihp->iot = h->tag;
	pcihp->ioh = h->windows[STP_WIN_IO].winaddr;
	pcihp->size = size;
	return (0);
}

void
stp4020_chip_io_free(pch, pcihp)
	pcmcia_chipset_handle_t pch;
	struct pcmcia_io_handle *pcihp;
{
}

int
stp4020_chip_io_map(pch, width, offset, size, pcihp, windowp)
	pcmcia_chipset_handle_t pch;
	int width;
	bus_addr_t offset;
	bus_size_t size;
	struct pcmcia_io_handle *pcihp;
	int *windowp;
{
	struct stp4020_socket *h = (struct stp4020_socket *)pch;

	pcihp->iot = h->tag;
	bus_space_subregion(h->tag, h->windows[STP_WIN_IO].winaddr,
	    offset, size, &pcihp->ioh);
	*windowp = 0;
	return (0);
}

void
stp4020_chip_io_unmap(pch, win)
	pcmcia_chipset_handle_t pch;
	int win;
{
}

void
stp4020_chip_socket_enable(pch)
	pcmcia_chipset_handle_t pch;
{
	struct stp4020_socket *h = (struct stp4020_socket *)pch;
	int i, v;

	/* this bit is mostly stolen from pcic_attach_card */

	/* Power down the socket to reset it, clear the card reset pin */
	stp4020_wr_sockctl(h, STP4020_ICR1_IDX, 0);

	/*
	 * wait 300ms until power fails (Tpf).  Then, wait 100ms since
	 * we are changing Vcc (Toff).
	 */
	stp4020_delay((300 + 100) * 1000);

	/* Power up the socket */
	v = STP4020_ICR1_MSTPWR;
	stp4020_wr_sockctl(h, STP4020_ICR1_IDX, v);

	/*
	 * wait 100ms until power raise (Tpr) and 20ms to become
	 * stable (Tsu(Vcc)).
	 */
	stp4020_delay((100 + 20) * 1000);

	v |= STP4020_ICR1_PCIFOE|STP4020_ICR1_VPP1_VCC;
	stp4020_wr_sockctl(h, STP4020_ICR1_IDX, v);

	/*
	 * hold RESET at least 10us.
	 */
	delay(10);

	/* Clear reset flag */
	v = stp4020_rd_sockctl(h, STP4020_ICR0_IDX);
	v &= ~STP4020_ICR0_RESET;
	stp4020_wr_sockctl(h, STP4020_ICR0_IDX, v);

	/* wait 20ms as per pc card standard (r2.01) section 4.3.6 */
	stp4020_delay(20000);

	/* Wait for the chip to finish initializing (5 seconds max) */
	for (i = 10000; i > 0; i--) {
		v = stp4020_rd_sockctl(h, STP4020_ISR0_IDX);
		if ((v & STP4020_ISR0_RDYST) != 0)
			break;
		delay(500);
	}
	if (i <= 0) {
#if STP4020_DEBUG
		char bits[64];
		bitmask_snprintf(stp4020_rd_sockctl(h, STP4020_ISR0_IDX),
		    STP4020_ISR0_IOBITS, bits, sizeof(bits));
		printf("stp4020_chip_socket_enable: not ready: status %s\n",
		    bits);
#endif
		return;
	}

	v = stp4020_rd_sockctl(h, STP4020_ICR0_IDX);

	/*
	 * Check the card type.
	 * Enable socket I/O interrupts for IO cards.
	 * We use level SB_INT[0] for I/O interrupts.
	 */
	if (pcmcia_card_gettype(h->pcmcia) == PCMCIA_IFTYPE_IO) {
		v &= ~(STP4020_ICR0_IOILVL | STP4020_ICR0_IFTYPE);
		v |= STP4020_ICR0_IFTYPE_IO | STP4020_ICR0_IOIE |
		    STP4020_ICR0_IOILVL_SB0 | STP4020_ICR0_SPKREN;
		DPRINTF(("%s: configuring card for IO usage\n",
		    h->sc->sc_dev.dv_xname));
	} else {
		v &= ~(STP4020_ICR0_IOILVL | STP4020_ICR0_IFTYPE |
		    STP4020_ICR0_SPKREN | STP4020_ICR0_IOILVL_SB0 |
		    STP4020_ICR0_IOILVL_SB1 | STP4020_ICR0_SPKREN);
		v |= STP4020_ICR0_IFTYPE_MEM;
		DPRINTF(("%s: configuring card for MEM ONLY usage\n",
		    h->sc->sc_dev.dv_xname));
	}
	stp4020_wr_sockctl(h, STP4020_ICR0_IDX, v);
}

void
stp4020_chip_socket_disable(pch)
	pcmcia_chipset_handle_t pch;
{
	struct stp4020_socket *h = (struct stp4020_socket *)pch;
	int v;

	/*
	 * Disable socket I/O interrupts.
	 */
	v = stp4020_rd_sockctl(h, STP4020_ICR0_IDX);
	v &= ~(STP4020_ICR0_IOIE | STP4020_ICR0_IOILVL);
	stp4020_wr_sockctl(h, STP4020_ICR0_IDX, v);

	/* Power down the socket */
	stp4020_wr_sockctl(h, STP4020_ICR1_IDX, 0);

	/*
	 * wait 300ms until power fails (Tpf).
	 */
	stp4020_delay(300 * 1000);
}

void *
stp4020_chip_intr_establish(pch, pf, ipl, handler, arg, xname)
	pcmcia_chipset_handle_t pch;
	struct pcmcia_function *pf;
	int ipl;
	int (*handler) (void *);
	void *arg;
	char *xname;
{
	struct stp4020_socket *h = (struct stp4020_socket *)pch;

	h->intrhandler = handler;
	h->intrarg = arg;
	h->ipl = ipl;
	return (h);
}

void
stp4020_chip_intr_disestablish(pch, ih)
	pcmcia_chipset_handle_t pch;
	void *ih;
{
	struct stp4020_socket *h = (struct stp4020_socket *)pch;

	h->intrhandler = NULL;
	h->intrarg = NULL;
}

/*
 * Delay and possibly yield CPU.
 * XXX - assumes a context
 */
void
stp4020_delay(ms)
	unsigned int ms;
{
	unsigned int ticks;

	/* Convert to ticks */
	ticks = (ms * hz ) / 1000000;

	if (cold || ticks == 0) {
		delay(ms);
		return;
	}

#ifdef DIAGNOSTIC
	if (ticks > 60 * hz)
		panic("stp4020: preposterous delay: %u", ticks);
#endif
	tsleep(&ticks, 0, "stp4020_delay", ticks);
}

#ifdef STP4020_DEBUG
void
stp4020_dump_regs(h)
	struct stp4020_socket *h;
{
	char bits[64];
	/*
	 * Dump control and status registers.
	 */
	printf("socket[%d] registers:\n", h->sock);
	bitmask_snprintf(stp4020_rd_sockctl(h, STP4020_ICR0_IDX),
	    STP4020_ICR0_BITS, bits, sizeof(bits));
	printf("\tICR0=%s\n", bits);

	bitmask_snprintf(stp4020_rd_sockctl(h, STP4020_ICR1_IDX),
	    STP4020_ICR1_BITS, bits, sizeof(bits));
	printf("\tICR1=%s\n", bits);

	bitmask_snprintf(stp4020_rd_sockctl(h, STP4020_ISR0_IDX),
	    STP4020_ISR0_IOBITS, bits, sizeof(bits));
	printf("\tISR0=%s\n", bits);

	bitmask_snprintf(stp4020_rd_sockctl(h, STP4020_ISR1_IDX),
	    STP4020_ISR1_BITS, bits, sizeof(bits));
	printf("\tISR1=%s\n", bits);
}
#endif /* STP4020_DEBUG */
