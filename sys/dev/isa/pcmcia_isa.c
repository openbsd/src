/*
 * Copyright (c) 1994 Stefan Grefen.  All rights reserved.
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
 *	This product includes software developed by Charles Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *      $Id: pcmcia_isa.c,v 1.1 1996/01/16 20:13:01 hvozda Exp $
 */

/* TODO add modload support and loadable lists of devices */
/* How to do cards with more than one function (modem/ethernet ..) */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <dev/pcmcia/pcmcia.h>
#include <dev/pcmcia/pcmciabus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/ic/i8042reg.h>
#include <i386/isa/isa_machdep.h>       /* XXX USES ISA HOLE DIRECTLY */

#ifdef IBM_WD
#define PCMCIA_ISA_DEBUG
#endif

static int pcmcia_isa_init __P((struct device *, struct cfdata *,
				void *, struct pcmcia_adapter *, int));
static int pcmcia_isa_search __P((struct device *, void *, cfprint_t));
static int pcmcia_isa_probe __P((struct device *, void *,
				 void *, struct pcmcia_link *));
static int pcmcia_isa_config __P((struct pcmcia_link *, struct device *,
				  struct pcmcia_conf *, struct cfdata *));
static int pcmcia_isa_unconfig __P((struct pcmcia_link *));

struct pcmciabus_link pcmcia_isa_link = {
	pcmcia_isa_config,
	pcmcia_isa_unconfig,
	pcmcia_isa_probe,
	pcmcia_isa_search,
	pcmcia_isa_init
};

/* copy out the addr and length from machine specific attach struct */
static int
pcmcia_isa_init(parent, cf, aux, pca, flag)
	struct device  *parent;
	struct cfdata  *cf;
	void           *aux;
	struct pcmcia_adapter *pca;
	int             flag;
{
	struct isa_attach_args *ia = aux;

#ifdef PCMCIA_ISA_DEBUG
	if (parent != NULL)
		printf("PARENT %s\n", parent->dv_xname);
#endif
	if (flag) {		/* attach */
		pca->scratch_mem = (caddr_t) ISA_HOLE_VADDR(ia->ia_maddr);
		pca->scratch_memsiz = ia->ia_msize;
	}
	ia->ia_iosize = 0;
	return 1;
}

/* probe and attach a device, the has to be configured already */
static int
pcmcia_isa_probe(parent, match, aux, pc_link)
	struct device  *parent;
	void	       *match;
	void           *aux;
	struct pcmcia_link *pc_link;
{
	struct device *dev = match;
	struct cfdata  *cf = aux;
	struct isa_attach_args ia;
	struct pcmciadevs *pcs = pc_link->device;
	int (*probe) () = (pcs != NULL) ? pcs->dev->pcmcia_probe : NULL;

	ia.ia_iobase = cf->cf_loc[0];
	ia.ia_iosize = 0x666;
	ia.ia_maddr = cf->cf_loc[2];
	ia.ia_msize = cf->cf_loc[3];
	ia.ia_irq = cf->cf_loc[4];
	ia.ia_drq = cf->cf_loc[5];
	if (probe == NULL)
		probe = cf->cf_driver->cd_match;

#ifdef PCMCIA_ISA_DEBUG
	printf("pcmcia probe %x %x %x\n", ia.ia_iobase, ia.ia_irq, probe);
	printf("parentname = %s\n", parent->dv_xname);
	printf("devname = %s\n", dev->dv_xname);
	printf("driver name = %s\n", cf->cf_driver->cd_name);
#endif
	if ((*probe) (parent, dev, &ia, pc_link) > 0) {
		extern          isaprint();
		config_attach(parent, dev, &ia, isaprint);
#ifdef PCMCIA_ISA_DEBUG
		printf("biomask %x netmask %x ttymask %x\n",
		       (u_short) imask[IPL_BIO], (u_short) imask[IPL_NET],
		       (u_short) imask[IPL_TTY]);
#endif
		return 1;
	}
	else
		free(dev, M_DEVBUF);
	return 0;
}

/*
 * Modify a pcmcia_conf struct to match the config entry. Pc_cf was filled
 * with config data from the card and may be modified before and after the
 * call to pcmcia_isa_config. Unless the FIXED_WIN flag is set we assume
 * contiguous windows and shift according to the offset for the first not
 * fixed window
 */
static int
pcmcia_isa_config(pc_link, self, pc_cf, cf)
	struct pcmcia_link *pc_link;
	struct device  *self;
	struct pcmcia_conf *pc_cf;
	struct cfdata  *cf;
{
	struct isa_attach_args ia;

	ia.ia_iobase = cf->cf_loc[0];
	ia.ia_iosize = 0x666;
	ia.ia_maddr = cf->cf_loc[2];
	ia.ia_msize = cf->cf_loc[3];
	ia.ia_irq = cf->cf_loc[4];
	ia.ia_drq = cf->cf_loc[5];
#ifdef PCMCIA_ISA_DEBUG
	printf("pcmcia_isa_config iobase=%x maddr=%x msize=%x irq=%x drq=%x\n",
	       ia.ia_iobase, ISA_HOLE_VADDR(ia.ia_maddr), ia.ia_msize,
	       ia.ia_irq, ia.ia_drq);
#endif

	if (ia.ia_irq != IRQUNK) {
		int             irq = 1 << ia.ia_irq;
		/*
		 * This is tricky irq 9 must match irq 2 in a device mask and
		 * configured  irq 9 must match irq 2
		 */
#ifdef PCMCIA_ISA_DEBUG
		printf("pcmcia_isa_config irq=%x num=%x mask=%x and=%x\n", irq,
		       1 << pc_cf->irq_num, pc_cf->irq_mask,
		       irq & pc_cf->irq_mask);
#endif
		if (irq != (1 << pc_cf->irq_num) &&
		    !(irq == (1 << 9) && pc_cf->irq_num == 2)) {
			if (irq == (1 << 9) || irq == (1 << 2))
				irq = (1 << 9) | (1 << 2);
			if ((irq & pc_cf->irq_mask) == 0) {
				printf("irq %d mask %x\n", ia.ia_irq,
				       pc_cf->irq_mask);
				return ENODEV;
			}
			/* 2 is 9 is 2 ... */
			irq&=~(1 << 2);
			cf->cf_loc[4] = pc_cf->irq_num = ffs(irq) - 1;
#ifdef PCMCIA_ISA_DEBUG
			printf("pcmcia_isa_config modify num=%x\n",
			       pc_cf->irq_num);
#endif
		}
	}
	if (ia.ia_iobase != IOBASEUNK) {
		int             i;
		int             offs = 0;
		if (pc_cf->iowin == 0)
			return 0;
		for (i = 0; i < pc_cf->iowin; i++) {
			if (pc_cf->io[i].flags & PCMCIA_FIXED_WIN)
				continue;
			if (offs == 0) {
				if (pc_cf->io[i].start != ia.ia_iobase) {
					offs = ia.ia_iobase - 
					       pc_cf->io[i].start;
				} else
					break;
			}
			pc_cf->io[i].start += offs;
		}
	} else
		pc_cf->iowin = 0;
	if (ia.ia_maddr != MADDRUNK && ia.ia_msize) {
		int             i;
		unsigned long   offs = 0;
		int             mlen = ia.ia_msize;
		int		maddr = (int) ISA_HOLE_VADDR(ia.ia_maddr);

		if (pc_cf->memwin == 0)
			return ENODEV;

#ifdef PCMCIA_ISA_DEBUG
		printf("Doing ia=%x ma=%x ms=%d\n", ia.ia_maddr,
		       maddr, ia.ia_msize);
#endif
		for (i = 0; i < pc_cf->memwin && mlen > 0; i++) {
#ifdef PCMCIA_ISA_DEBUG
		printf("Doing i=%d st=%x len=%d, flags=%x offs=%d mlen=%d\n",
		       i, pc_cf->mem[i].start, pc_cf->mem[i].len,
		       pc_cf->mem[i].flags, offs, mlen);
#endif
			if (pc_cf->mem[i].flags & PCMCIA_FIXED_WIN)
				continue;
			if (offs == 0) {
				if (pc_cf->mem[i].start != maddr) {
					offs = maddr - pc_cf->mem[i].start;
				} else
					break;
			}
			mlen -= pc_cf->mem[i].len;
			if (mlen < 0)
				pc_cf->mem[i].len += mlen;
			pc_cf->mem[i].start += offs;
		}
	} else
		pc_cf->memwin = 0;
	return 0;
}


static int
pcmcia_isa_unconfig(pc_link)
	struct pcmcia_link *pc_link;
{
	if (pc_link && pc_link->intr > 0) {
		/* THIS IS A GUESS ... TODO check all possible drivers */
		struct softc {
			struct device   sc_dev;
			void *sc_ih;
		} *sc = pc_link->devp;
		if (sc)
			isa_intr_disestablish(sc->sc_ih);
	}
	return 0;
}

/* Searches for for configured devices on the pcmciabus */
static int
pcmcia_isa_search(parent, aux, print)
	struct device  *parent;
	void           *aux;
	cfprint_t       print;
{
	static char    *msgs[3] = {"", " not configured\n", " unsupported\n"};

#ifdef PCMCIA_ISA_DEBUG
	printf("pcmcia_isa_search\n");
#endif
	if (config_search(pcmcia_configure, parent, aux) != NULL)
		return 1;

	if (print) {
		int i;
		i = (*print) (aux, parent->dv_xname);
		printf(msgs[i]);
	}
	return 0;
}
