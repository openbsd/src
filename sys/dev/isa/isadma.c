/*	$OpenBSD: isadma.c,v 1.6 1996/05/07 07:37:10 deraadt Exp $	*/
/*	$NetBSD: isadma.c,v 1.19 1996/04/29 20:03:26 christos Exp $	*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <sys/syslog.h>
#include <sys/malloc.h>
#include <sys/uio.h>

#include <vm/vm.h>

#include <machine/pio.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isadmavar.h>
#include <dev/isa/isadmareg.h>

struct dma_info {
	int flags;
	int active;
	caddr_t addr;
	vm_size_t nbytes;
	struct isadma_seg phys[1];
};

static struct dma_info dma_info[8];
static u_int8_t dma_finished;

/* high byte of address is stored in this port for i-th dma channel */
static int dmapageport[2][4] = {
	{0x87, 0x83, 0x81, 0x82},
	{0x8f, 0x8b, 0x89, 0x8a}
};

static u_int8_t dmamode[4] = {
	DMA37MD_READ | DMA37MD_SINGLE,
	DMA37MD_WRITE | DMA37MD_SINGLE,
	DMA37MD_READ | DMA37MD_LOOP,
	DMA37MD_WRITE | DMA37MD_LOOP
};

/*
 * isadma_cascade(): program 8237 DMA controller channel to accept
 * external dma control by a board.
 */
void
isadma_cascade(chan)
	int chan;
{

#ifdef ISADMA_DEBUG
	if (chan < 0 || chan > 7)
		panic("isadma_cascade: impossible request"); 
#endif

	/* set dma channel mode, and set dma channel mode */
	if ((chan & 4) == 0) {
		outb(DMA1_MODE, chan | DMA37MD_CASCADE);
		outb(DMA1_SMSK, chan);
	} else {
		chan &= 3;

		outb(DMA2_MODE, chan | DMA37MD_CASCADE);
		outb(DMA2_SMSK, chan);
	}
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
	struct dma_info *di;
	int waport;
	int mflags;
	vm_size_t size;

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
		/* set dma channel mode, and reset address ff */
		outb(DMA1_MODE, chan | dmamode[flags]);
		outb(DMA1_FFC, 0);

		/* send start address */
		waport = DMA1_CHN(chan);
		outb(dmapageport[0][chan], di->phys[0].addr>>16);
		outb(waport, di->phys[0].addr);
		outb(waport, di->phys[0].addr>>8);

		/* send count */
		outb(waport + 1, --nbytes);
		outb(waport + 1, nbytes>>8);

		/* unmask channel */
		outb(DMA1_SMSK, chan | DMA37SM_CLEAR);
	} else {
		/*
		 * Program one of DMA channels 4..7.  These are
		 * word mode channels.
		 */
		/* set dma channel mode, and reset address ff */
		outb(DMA2_MODE, (chan & 3) | dmamode[flags]);
		outb(DMA2_FFC, 0);

		/* send start address */
		waport = DMA2_CHN(chan & 3);
		outb(dmapageport[1][chan], di->phys[0].addr>>16);
		outb(waport, di->phys[0].addr>>1);
		outb(waport, di->phys[0].addr>>9);

		/* send count */
		nbytes >>= 1;
		outb(waport + 2, --nbytes);
		outb(waport + 2, nbytes>>8);

		/* unmask channel */
		outb(DMA2_SMSK, (chan & 3) | DMA37SM_CLEAR);
	}
}

void
isadma_abort(chan)
	int chan;
{
	struct dma_info *di;

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
	if ((chan & 4) == 0)
		outb(DMA1_SMSK, DMA37SM_SET | chan);
	else
		outb(DMA2_SMSK, DMA37SM_SET | (chan & 3));

	isadma_unmap(di->addr, di->nbytes, 1, di->phys);
	di->active = 0;
}

int
isadma_finished(chan)
	int chan;
{

#ifdef ISADMA_DEBUG
	if (chan < 0 || chan > 7)
		panic("isadma_finished: impossible request");
#endif

	/* check that the terminal count was reached */
	if ((chan & 4) == 0)
		dma_finished |= inb(DMA1_SR) & 0x0f;
	else
		dma_finished |= (inb(DMA2_SR) & 0x0f) << 4;

	return ((dma_finished & (1 << chan)) != 0);
}

void
isadma_done(chan)
	int chan;
{
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
	if ((chan & 4) == 0)
		tc = inb(DMA1_SR) & (1 << chan);
	else
		tc = inb(DMA2_SR) & (1 << (chan & 3));
	if (tc == 0)
		/* XXX probably should panic or something */
		log(LOG_ERR, "dma channel %d not finished\n", chan);

	/* mask channel */
	if ((chan & 4) == 0)
		outb(DMA1_SMSK, DMA37SM_SET | chan);
	else
		outb(DMA2_SMSK, DMA37SM_SET | (chan & 3));

	/* XXX Will this do what we want with DMAMODE_LOOP?  */
	if (di->flags & DMAMODE_READ)
		isadma_copyfrombuf(di->addr, di->nbytes, 1, di->phys);

	isadma_unmap(di->addr, di->nbytes, 1, di->phys);
	di->active = 0;
}
