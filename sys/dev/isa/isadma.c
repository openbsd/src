/*	$OpenBSD: isadma.c,v 1.12 1996/12/11 22:38:06 niklas Exp $	*/
/*	$NetBSD: isadma.c,v 1.19 1996/04/29 20:03:26 christos Exp $	*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <sys/syslog.h>
#include <sys/malloc.h>
#include <sys/uio.h>

#include <vm/vm.h>

#include <machine/bus.h>
#include <machine/pio.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>
#include <dev/isa/isadmareg.h>

struct dma_info {
	int flags;
	int active;
	caddr_t addr;
	vm_size_t nbytes;
	struct isadma_seg phys[1];
};

static struct isadma_softc *isadma_sc;	/*XXX ugly */
static struct dma_info dma_info[8];
static u_int8_t dma_finished;

/*
 * high byte of address is stored in this port (offset IO_DMAPG) for i-th
 * dma channel
 */
static short dmapageport[2][4] = {
	{0x7, 0x3, 0x1, 0x2},
	{0xf, 0xb, 0x9, 0xa}
};

static u_int8_t dmamode[4] = {
	DMA37MD_READ | DMA37MD_SINGLE,
	DMA37MD_WRITE | DMA37MD_SINGLE,
	DMA37MD_READ | DMA37MD_LOOP,
	DMA37MD_WRITE | DMA37MD_LOOP
};

int isadmamatch __P((struct device *, void *, void *));
void isadmaattach __P((struct device *, struct device *, void *));
int isadmaprint __P((void *, const char *));

struct isadma_softc {
	struct device sc_dev;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh1;
	bus_space_handle_t sc_ioh2;
	bus_space_handle_t sc_dmapageioh[2][4];
};

struct cfattach isadma_ca = {
	sizeof(struct device), isadmamatch, isadmaattach
};

struct cfdriver isadma_cd = {
	NULL, "isadma", DV_DULL, 1
};

int
isadmamatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct isa_attach_args *ia = aux;

	/* Sure we exist */
	ia->ia_iosize = 0;
	return (1);
}

void
isadmaattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct isadma_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot;
	int i;

	printf("\n");
	iot = sc->sc_iot = ia->ia_iot;
	if (bus_space_map(iot, IO_DMA1, DMA_NREGS(1), 0, &sc->sc_ioh1))
	    	panic("isadmaattach: couldn't map I/O ports at IO_DMA1");
	if (bus_space_map(iot, IO_DMA2, DMA_NREGS(2), 0, &sc->sc_ioh2))
		panic("isadmaattach: couldn't map I/O ports at IO_DMA2");

	/* XXX the constants below is a bit ugly, I know... */
	for (i = 0; i < 8; i++) {
		if (bus_space_map(iot, IO_DMAPG + dmapageport[i % 2][i / 2], 1,
		    0, &sc->sc_dmapageioh[i % 2][i / 2]))
			panic("isadmaattach: couldn't map DMA page I/O port");
	}
	isadma_sc = sc;
}

/*
 * isadma_cascade(): program 8237 DMA controller channel to accept
 * external dma control by a board.
 */
void
isadma_cascade(chan)
	int chan;
{
	struct isadma_softc *sc = isadma_sc;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh;
	int dma_unit;

#ifdef ISADMA_DEBUG
	if (chan < 0 || chan > 7)
		panic("isadma_cascade: impossible request"); 
#endif

	/* set dma channel mode, and set dma channel mode */
	if ((chan & 4) == 0) {
		ioh = sc->sc_ioh1;
		dma_unit = 1;
	} else {
		ioh = sc->sc_ioh2;
		chan &= 3;
		dma_unit = 2;
	}
	bus_space_write_1(iot, ioh, DMA_MODE(dma_unit),
	    chan | DMA37MD_CASCADE);
	bus_space_write_1(iot, ioh, DMA_SMSK(dma_unit), chan);
}

/*
 * isadma_start(): program 8237 DMA controller channel, avoid page alignment
 * problems by using a bounce buffer.
 */
void
isadma_start(addr, nbytes, chan, flags)
	caddr_t addr;
	vm_size_t nbytes;
	int chan;
	int flags;
{
	struct isadma_softc *sc = isadma_sc;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh;
	struct dma_info *di;
	int waport;
	int mflags;
	int dma_unit;

#ifdef ISADMA_DEBUG
	if (chan < 0 || chan > 7 ||
	    (((flags & DMAMODE_READ) != 0) + ((flags & DMAMODE_WRITE) != 0) +
	    ((flags & DMAMODE_LOOP) != 0) != 1) ||
	    ((chan & 4) ? (nbytes >= (1<<17) || nbytes & 1 || (u_int)addr & 1) :
	    (nbytes >= (1<<16))))
		panic("isadma_start: impossible request"); 
#endif

	di = dma_info+chan;
	if (di->active) {
		log(LOG_ERR,"isadma_start: old request active on %d\n",chan);
		isadma_abort(chan);
	}

	di->flags = flags;
	di->active = 1;
	di->addr = addr;
	di->nbytes = nbytes;

	mflags = ISADMA_MAP_WAITOK | ISADMA_MAP_BOUNCE | ISADMA_MAP_CONTIG;
	mflags |= (chan & 4) ? ISADMA_MAP_16BIT : ISADMA_MAP_8BIT;

	if (isadma_map(addr, nbytes, di->phys, mflags) != 1)
		panic("isadma_start: cannot map");

	/* XXX Will this do what we want with DMAMODE_LOOP?  */
	if ((flags & DMAMODE_READ) == 0)
		isadma_copytobuf(addr, nbytes, 1, di->phys);

	dma_finished &= ~(1 << chan);

	if ((chan & 4) == 0) {
		/*
		 * Program one of DMA channels 0..3.  These are
		 * byte mode channels.
		 */
		ioh = sc->sc_ioh1;
		dma_unit = 1;
	} else {
		/*
		 * Program one of DMA channels 4..7.  These are
		 * word mode channels.
		 */
		ioh = sc->sc_ioh2;
		dma_unit = 2;
		chan &= 3;
		nbytes >>= 1;
	}
       	/* set dma channel mode, and reset address ff */
	bus_space_write_1(iot, ioh, DMA_MODE(dma_unit), chan | dmamode[flags]);
	bus_space_write_1(iot, ioh, DMA_FFC(dma_unit), 0);

	/* send start address */
	waport = DMA_CHN(dma_unit, chan);
	bus_space_write_1(iot, sc->sc_dmapageioh[dma_unit - 1][chan], 0,
	    di->phys[0].addr>>16);
	bus_space_write_1(iot, ioh, waport, di->phys[0].addr);
	bus_space_write_1(iot, ioh, waport, di->phys[0].addr>>8);

	/* send count */
	bus_space_write_1(iot, ioh, waport + dma_unit, --nbytes & 0xff);
	bus_space_write_1(iot, ioh, waport + dma_unit, nbytes >> 8);

	/* unmask channel */
	bus_space_write_1(iot, ioh, DMA_SMSK(dma_unit), chan | DMA37SM_CLEAR);
}

void
isadma_abort(chan)
	int chan;
{
	struct isadma_softc *sc = isadma_sc;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh;
	struct dma_info *di;
	int dma_unit;

#ifdef ISADMA_DEBUG
	if (chan < 0 || chan > 7)
		panic("isadma_abort: impossible request");
#endif

	di = dma_info+chan;
	if (! di->active) {
		log(LOG_ERR,"isadma_abort: no request active on %d\n",chan);
		return;
	}

	/* mask channel */
	if ((chan & 4) == 0) {
		ioh = sc->sc_ioh1;
		dma_unit = 1;
	} else {
		ioh = sc->sc_ioh2;
		chan &= 3;
		dma_unit = 2;
	}
	bus_space_write_1(iot, ioh, DMA_SMSK(dma_unit), DMA37SM_SET | chan);
	isadma_unmap(di->addr, di->nbytes, 1, di->phys);
	di->active = 0;
}

int
isadma_finished(chan)
	int chan;
{
	struct isadma_softc *sc = isadma_sc;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh;
	int dma_unit;

#ifdef ISADMA_DEBUG
	if (chan < 0 || chan > 7)
		panic("isadma_finished: impossible request");
#endif

	/* check that the terminal count was reached */
	if ((chan & 4) == 0) {
		ioh = sc->sc_ioh1;
		dma_unit = 1;
	} else {
		ioh = sc->sc_ioh2;
		chan &= 3;
		dma_unit = 2;
	}
	dma_finished |= bus_space_read_1(iot, ioh, DMA_SR(dma_unit)) & 0x0f;
	return ((dma_finished & (1 << chan)) != 0);
}

void
isadma_done(chan)
	int chan;
{
	struct isadma_softc *sc = isadma_sc;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh;
	int dma_unit;
	struct dma_info *di;
	u_char tc;

#ifdef DIAGNOSTIC
	if (chan < 0 || chan > 7)
		panic("isadma_done: impossible request");
#endif

	di = dma_info+chan;
	if (! di->active) {
		log(LOG_ERR,"isadma_done: no request active on %d\n",chan);
		return;
	}

	/* check that the terminal count was reached */
	if ((chan & 4) == 0) {
		ioh = sc->sc_ioh1;
		dma_unit = 1;
	} else {
		ioh = sc->sc_ioh2;
		chan &= 3;
		dma_unit = 2;
	}
	tc = bus_space_read_1(iot, ioh, DMA_SR(dma_unit)) & (1 << chan);

	if (tc == 0)
		/* XXX probably should panic or something */
		log(LOG_ERR, "dma channel %d not finished\n",
		    dma_unit == 1 ? chan : (chan | 4));

	/* mask channel */
	bus_space_write_1(iot, ioh, DMA_SMSK(dma_unit), DMA37SM_SET | chan);

	/* XXX Will this do what we want with DMAMODE_LOOP?  */
	if (di->flags & DMAMODE_READ)
		isadma_copyfrombuf(di->addr, di->nbytes, 1, di->phys);

	isadma_unmap(di->addr, di->nbytes, 1, di->phys);
	di->active = 0;
}
