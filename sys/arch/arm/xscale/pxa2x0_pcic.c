/* $OpenBSD: pxa2x0_pcic.c,v 1.7 2005/01/27 17:03:23 millert Exp $ */
/*
 * Copyright (c) Dale Rahn <drahn@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/timeout.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <uvm/uvm.h>

#include <machine/bus.h>
#include <machine/intr.h>
        
#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciachip.h>

#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0var.h>
#include <arm/xscale/pxa2x0_gpio.h>
#include <arm/xscale/pxapcicvar.h>

int	pxapcic_mem_alloc(pcmcia_chipset_handle_t, bus_size_t,
    struct pcmcia_mem_handle *);
void	pxapcic_mem_free(pcmcia_chipset_handle_t,
    struct pcmcia_mem_handle *);
int	pxapcic_mem_map(pcmcia_chipset_handle_t, int, bus_addr_t,
    bus_size_t, struct pcmcia_mem_handle *, bus_addr_t *, int *);
void	pxapcic_mem_unmap(pcmcia_chipset_handle_t, int);
int	pxapcic_io_alloc(pcmcia_chipset_handle_t, bus_addr_t,
    bus_size_t, bus_size_t, struct pcmcia_io_handle *);
void	pxapcic_io_free(pcmcia_chipset_handle_t,
    struct pcmcia_io_handle *);
int	pxapcic_io_map(pcmcia_chipset_handle_t, int,
    bus_addr_t, bus_size_t, struct pcmcia_io_handle *, int *);
void	pxapcic_io_unmap(pcmcia_chipset_handle_t, int);
void	*pxapcic_intr_establish(pcmcia_chipset_handle_t,
    struct pcmcia_function *, int, int (*)(void *), void *, char *);
void	pxapcic_intr_disestablish(pcmcia_chipset_handle_t, void *);
const char *pxapcic_intr_string(pcmcia_chipset_handle_t, void *);
void	pxapcic_socket_enable(pcmcia_chipset_handle_t);
void	pxapcic_socket_disable(pcmcia_chipset_handle_t);

void    pxapcic_event_thread(void *);
         
void    pxapcic_delay(int, const char *);

int     pxapcic_match(struct device *, void *, void *);
void    pxapcic_attach(struct device *, struct device *, void *);
void	pxapcic_create_event_thread(void *arg);
int	pxapcic_submatch(struct device *parent, void *, void *aux);
int	pxapcic_print(void *aux, const char *name);
int	pxapcic_intr(void *arg);

void	pxapcic_event_process(struct pxapcic_socket *);

void	pxapcic_attach_card(struct pxapcic_socket *h);
void	pxapcic_detach_card(struct pxapcic_socket *h, int flags);

int pxapcic_intr_detect(void *arg);

/* DONT CONFIGURE CF slot 1 for now */
#define NUM_CF_CARDS 2

struct cfattach pxapcic_ca = {
	sizeof(struct pxapcic_softc), pxapcic_match, pxapcic_attach
};

struct cfdriver pxapcic_cd = {
	NULL, "pxapcic", DV_DULL
};

struct pcmcia_chip_functions pxapcic_pcmcia_functions = {
	pxapcic_mem_alloc,
	pxapcic_mem_free,
	pxapcic_mem_map,
	pxapcic_mem_unmap,
  
	pxapcic_io_alloc,
	pxapcic_io_free,
	pxapcic_io_map,
	pxapcic_io_unmap,
 
	pxapcic_intr_establish,
	pxapcic_intr_disestablish,
	pxapcic_intr_string,

	pxapcic_socket_enable,
	pxapcic_socket_disable,
};



int
pxapcic_mem_alloc(pch, size, pmh)
	pcmcia_chipset_handle_t pch;
	bus_size_t size;
	struct pcmcia_mem_handle *pmh;
{
	struct pxapcic_socket *so = pch;

	/* All we need is bus space tag */
	memset(pmh, 0, sizeof(*pmh));
	pmh->memt = so->sc->sc_iot;
	return (0);
}

void
pxapcic_mem_free(pch, pmh)
	pcmcia_chipset_handle_t pch;
	struct pcmcia_mem_handle *pmh;
{
}

int
pxapcic_mem_map(pch, kind, card_addr, size, pmh, offsetp, windowp)
	pcmcia_chipset_handle_t pch;
	int kind;
	bus_addr_t card_addr;
	bus_size_t size;
	struct pcmcia_mem_handle *pmh;
	bus_addr_t *offsetp;
	int *windowp;
{
	struct pxapcic_socket *so = pch;
	int error;
	bus_addr_t pa;
 
	pa = trunc_page(card_addr);
	*offsetp = card_addr - pa;
	size = round_page(card_addr + size) - pa;
	pmh->realsize = size;

#define PXA2X0_SOCKET_OFFSET	(PXA2X0_PCMCIA_SLOT1-PXA2X0_PCMCIA_SLOT0)
#define PXAPCIC_ATTR_OFFSET	0x08000000
#define PXAPCIC_COMMON_OFFSET	0x0C000000

	pa += PXA2X0_PCMCIA_SLOT0;
	pa += PXA2X0_SOCKET_OFFSET * so->socket;

	switch (kind & ~PCMCIA_WIDTH_MEM_MASK) {
	case PCMCIA_MEM_ATTR:   
		pa += PXAPCIC_ATTR_OFFSET;
		break;
	case PCMCIA_MEM_COMMON:
		pa += PXAPCIC_COMMON_OFFSET;
		break;
	default:
		panic("pxapcic_mem_map: bogus kind");
	}

	error = bus_space_map(so->sc->sc_iot, pa, size, 0, &pmh->memh);
	if (! error)
		*windowp = (int)pmh->memh;
	return (error);
}

void
pxapcic_mem_unmap(pch, window)
        pcmcia_chipset_handle_t pch;
        int window;
{
        struct pxapcic_socket *so = pch;

        bus_space_unmap(so->sc->sc_iot, (bus_addr_t)window, 4096); /* XXX */
}

int
pxapcic_io_alloc(pch, start, size, align, pih)
        pcmcia_chipset_handle_t pch;
        bus_addr_t start;
        bus_size_t size;
        bus_size_t align;
        struct pcmcia_io_handle *pih;
{
        struct pxapcic_socket *so = pch;
        int error;
        bus_addr_t pa;
        
        memset(pih, 0, sizeof(*pih));
        pih->iot = so->sc->sc_iot;
        pih->addr = start;
        pih->size = size;
        
        pa = pih->addr;
        pa += PXA2X0_PCMCIA_SLOT0;
        pa += PXA2X0_SOCKET_OFFSET * so->socket;

#if 0
        printf("pxapcic_io_alloc: %x %x\n", (unsigned int)pa,
                 (unsigned int)size);
#endif
        /* XXX Are we ignoring alignment constraints? */
        error = bus_space_map(so->sc->sc_iot, pa, size, 0, &pih->ioh);
                
        return (error);
}
                
void
pxapcic_io_free(pch, pih)
        pcmcia_chipset_handle_t pch;
        struct pcmcia_io_handle *pih;
{
        struct pxapcic_socket *so = pch;
        
        bus_space_unmap(so->sc->sc_iot, pih->ioh, pih->size);
}
 
int
pxapcic_io_map(pch, width, offset, size, pih, windowp)
        pcmcia_chipset_handle_t pch;
        int width;
        bus_addr_t offset;
        bus_size_t size;
        struct pcmcia_io_handle *pih;
        int *windowp;
{
        return (0);
}

void pxapcic_io_unmap(pch, window)
        pcmcia_chipset_handle_t pch;
        int window;
{
}

void *
pxapcic_intr_establish(pch, pf, ipl, fct, arg, name)
        pcmcia_chipset_handle_t pch;
        struct pcmcia_function *pf;
        int ipl;
        int (*fct)(void *);
        void *arg;
	char *name;
{
        struct pxapcic_socket *so = pch;
        /* XXX need to check if something should be done here */

	return pxa2x0_gpio_intr_establish(so->irqpin, IST_EDGE_FALLING,
	    ipl, fct, arg, name);
}

void
pxapcic_intr_disestablish(pch, ih)
        pcmcia_chipset_handle_t pch;
        void *ih;
{
	pxa2x0_gpio_intr_disestablish(ih);
}

const char *
pxapcic_intr_string(pch, ih)
        pcmcia_chipset_handle_t pch;
        void *ih;
{
	struct pxapcic_socket *so = pch;
	static char irqstr[64];

	if (ih == NULL)
		snprintf(irqstr, sizeof(irqstr), "couldn't establish interrupt");
	else
		snprintf(irqstr, sizeof(irqstr), "irq %d", so->irqpin);
	return (irqstr);
}

void
pxapcic_socket_enable(pch)
        pcmcia_chipset_handle_t pch;
{
	/* XXX */
}

void
pxapcic_socket_disable(pch)
        pcmcia_chipset_handle_t pch;
{
        /* XXX */
}

#if 0
void
pxapcic_socket_settype(pch, type)
        pcmcia_chipset_handle_t pch;
        int type;
{

        /* XXX */
}
#endif

#if 0
void
pxapcic_socket_setup(struct pxapcic_socket *sp)
{
	/*
        sp->power_capability = PXAPCIC_POWER_3V;
	*/
        sp->pcictag = &pxapcic_sacpcic_functions;
}
#endif

#if 0
void
pxapcic_set_power(struct pxapcic_socket *so, int arg)
{
	/* 3 volt only, not supported - XXX */
}

#endif
int
pxapcic_match(struct device *parent, void *v, void *aux)
{
        return (1);
}     

void   
pxapcic_event_thread(void *arg)
{
	struct pxapcic_socket *sock = arg;
	u_int16_t csr;
	int present;

	while (sock->sc->sc_shutdown == 0) {
		/* sleep .25s to avoid chatterling interrupts */

		/* poll every 5 seconds in case we miss an interrupt */
		(void) tsleep(sock, PWAIT, "pxapcicev", hz*5);

		(void) tsleep((caddr_t)sock, PWAIT,
		    "pxapcicss", hz/4);

		csr = bus_space_read_2(sock->sc->sc_iot, sock->scooph,
		    SCOOP_REG_CSR);

		present = sock->flags & PXAPCIC_FLAG_CARDP;

		if (((csr & SCP_CSR_MISSING) == 0) == (present == 1))
			continue; /* state unchanged */

		/* XXX Do both? */
		pxapcic_event_process(sock);
	}
	
	sock->event_thread = NULL;
		
	/* In case parent is waiting for us to exit. */
	wakeup(sock->sc);
	 
	kthread_exit(0);
}

void
pxapcic_attach_card(struct pxapcic_socket *h)
{
	if (h->flags & PXAPCIC_FLAG_CARDP)
		panic("pcic_attach_card: already attached"); 
	h->flags |= PXAPCIC_FLAG_CARDP;
 
	/* call the MI attach function */
	pcmcia_card_attach(h->pcmcia);
}

void
pxapcic_detach_card(struct pxapcic_socket *h, int flags)
{
	if (h->flags & PXAPCIC_FLAG_CARDP) {
		h->flags &= ~PXAPCIC_FLAG_CARDP;
	 
		/* call the MI detach function */
		pcmcia_card_detach(h->pcmcia, flags);
	} else {
		//DPRINTF(("pcic_detach_card: already detached"));
	}
}


void pxapcic_event_process_st(void *h);
void
pxapcic_event_process_st(void *v)
{
	struct pxapcic_socket *h = v;
	pxapcic_event_process(h);
}
void
pxapcic_event_process(sock)
	struct pxapcic_socket *sock;
{
	struct pxapcic_softc *sc = sock->sc;
	u_int16_t csr;

	csr = bus_space_read_2(sc->sc_iot, sock->scooph, SCOOP_REG_CSR);

	switch (csr & SCP_CSR_MISSING) {
	case 0: /* PRESENT */
		//DPRINTF(("%s: insertion event\n", sock->sc->dv_xname));
		if (!(sock->flags & PXAPCIC_FLAG_CARDP))
			pxapcic_attach_card(sock);
		break;

	case SCP_CSR_MISSING:
		//DPRINTF(("%s: removal event\n", sock->sc->dv_xname));
		if ((sock->flags & PXAPCIC_FLAG_CARDP))
			pxapcic_detach_card(sock, DETACH_FORCE);
		break;
	}
}

void
pxapcic_create_event_thread(void *arg)
{
	struct pxapcic_socket *sock = arg;
	struct pxapcic_softc *sc = sock->sc;
	u_int16_t csr;

	csr = bus_space_read_2(sc->sc_iot, sock->scooph, SCOOP_REG_CSR);

	/* if there's a card there, then attach it  */

	switch (csr & SCP_CSR_MISSING) {
	case 0: /* PRESENT */
		pxapcic_attach_card(sock);
		break;
	default:
		;
	}

	if (kthread_create(pxapcic_event_thread, sock, &sock->event_thread,
	     sc->sc_dev.dv_xname, sock->socket ? "1" : "0")) {
		printf("%s: unable to create event thread for %s\n",
		     sc->sc_dev.dv_xname,  sock->socket ? "1" : "0");
	}
#ifdef DO_CONFIG_PENDING
	config_pending_decr();
#endif
}

int
pxapcic_submatch(struct device *parent, void *match, void *aux)
{
	struct cfdata *cf = match;

	return ((*cf->cf_attach->ca_match)(parent, cf, aux));

}

int
pxapcic_print(void *aux, const char *name)
{
	return (UNCONF);
}


void
pxapcic_attach(struct device *parent, struct device *self, void *aux)
{
	struct pcmciabus_attach_args paa;
	struct pxapcic_softc *sc = (struct pxapcic_softc *)self;
	struct pxaip_attach_args *pxa = aux;
	struct pxapcic_socket *so;
	bus_space_tag_t iot;
	bus_space_handle_t scooph;
	int i;
	int error;
	bus_addr_t pa;
	bus_size_t size = 0x100;

	iot = sc->sc_iot = pxa->pxa_iot;

	sc->sc_shutdown = 0;

	printf("\n");

	/*
	 * should be model based not processor based, and in zaurus/...
	 * not arm/xscale
	 */
	if ((cputype & ~CPU_ID_XSCALE_COREREV_MASK) == CPU_ID_PXA27X) {
#define C3000_CF0_IRQ 105
#define C3000_CF1_IRQ 106
#define C3000_CF0_CD 94
#define C3000_CF1_CD 93
		sc->sc_nslots = 2;
		sc->sc_irqpin[0] = C3000_CF0_IRQ;
		sc->sc_irqcfpin[0] = C3000_CF0_CD;
		sc->sc_irqpin[1] = C3000_CF1_IRQ;
		sc->sc_irqcfpin[1] = C3000_CF1_CD;
	} else {
#define C860_CF0_IRQ 17
#define C860_CF0_CD 14
		/* C860 */
		sc->sc_nslots = 1;
		sc->sc_irqpin[0] = C860_CF0_IRQ;
		sc->sc_irqcfpin[0] = C860_CF0_CD;
	}

	for(i = 0; i < NUM_CF_CARDS; i++) {
		so = &sc->sc_socket[i];
		so->sc = sc;
		so->socket = i;
		so->flags = 0;

		/* scoop? */
		if (so->socket == 0)
			pa = 0x10800000;
		else if (so->socket == 1)
			pa = 0x14800000;
		else {
			printf ("%s: invalid CF slot %d\n", sc->sc_dev.dv_xname,
			    i);
			continue;
		}
		error = bus_space_map(iot, pa, size, 0, &scooph);
		if (error) {
			printf ("%s%d:failed to map memory %x for scoop\n",
			    sc->sc_dev.dv_xname, i, pa);
			    continue;
		}
		so->scooph = scooph;

		bus_space_write_2(iot, scooph, SCOOP_REG_IMR, 0x00c0);

		/* setup */
		bus_space_write_2(iot, scooph, SCOOP_REG_MCR, 0x0100);
		bus_space_write_2(iot, scooph, SCOOP_REG_CDR, 0x0000);
		bus_space_write_2(iot, scooph, SCOOP_REG_CPR, 0x0000);
		bus_space_write_2(iot, scooph, SCOOP_REG_IMR, 0x0000);
		bus_space_write_2(iot, scooph, SCOOP_REG_IRM, 0x00ff);
		bus_space_write_2(iot, scooph, SCOOP_REG_ISR, 0x0000);
		bus_space_write_2(iot, scooph, SCOOP_REG_IMR, 0x0000);

		bus_space_write_2(iot, scooph, SCOOP_REG_CPR,
		    SCP_CPR_PWR|SCP_CPR_5V); /* 5 V for 3000? */
	

		paa.paa_busname = "pcmcia";
		paa.pct = (pcmcia_chipset_tag_t) &pxapcic_pcmcia_functions;
		paa.pch = (pcmcia_chipset_handle_t)&sc->sc_socket[i];
		paa.iobase = 0;
		paa.iosize = 0x4000000;

		so->pcmcia = config_found_sm(&sc->sc_dev, &paa, pxapcic_print,
		     pxapcic_submatch);
	}

	/* configure slot 1 first to make internal drive be wd0 */
	for (i = sc->sc_nslots-1; i >= 0; i--) {
		so = &sc->sc_socket[i];
		pxa2x0_gpio_set_function(sc->sc_irqpin[i], GPIO_IN);
		pxa2x0_gpio_set_function(sc->sc_irqcfpin[i], GPIO_IN);

		sc->sc_socket[i].irq = pxa2x0_gpio_intr_establish(
		    sc->sc_irqcfpin[i], 
		    IST_EDGE_BOTH, IPL_BIO /* XXX */, pxapcic_intr_detect,
		    so, sc->sc_dev.dv_xname);

		/* GPIO pin for interrupt */
		sc->sc_socket[i].irqpin = sc->sc_irqpin[i];

		bus_space_write_2(iot, so->scooph, SCOOP_REG_IMR, 0x00ce);
		bus_space_write_2(iot, so->scooph, SCOOP_REG_MCR, 0x0111);

#ifdef DO_CONFIG_PENDING
		config_pending_incr();
#endif
		kthread_create_deferred(pxapcic_create_event_thread, so);
	}
}

int
pxapcic_intr_detect(void *arg)
{
        struct pxapcic_socket *so = arg;
	printf("pxapcic_intr_detect %x\n", so->socket);

	/*
        (so->pcictag->clear_intr)(so->socket);
	*/
        wakeup(so);
        return 1;
}
