/*	$OpenBSD: vme.c,v 1.2 1998/12/15 05:52:31 smurph Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
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
 *      This product includes software developed by Theo de Raadt
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
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/callout.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/fcntl.h>
#include <sys/device.h>
#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/frame.h>

#include "pcctwo.h"

#include <mvme88k/dev/vme.h>

int  vmematch __P((struct device *, void *, void *));
void vmeattach __P((struct device *, struct device *, void *));

int vme1chip_init __P((struct vmesoftc *sc));
int vme2chip_init __P((struct vmesoftc *sc));
u_long vme2chip_map __P((u_long base, int len, int dwidth));
int vme2abort __P((struct frame *frame));

static int vmebustype;

struct cfattach vme_ca = {
        sizeof(struct vmesoftc), vmematch, vmeattach
}; 
 
struct cfdriver vme_cd = {
        NULL, "vme", DV_DULL, 0
}; 

int
vmematch(parent, cf, args)
	struct device *parent;
	void *cf;
	void *args;
{
	struct confargs *ca = args;

#if NMC > 0
	if (ca->ca_bustype == BUS_MC) {
		struct mcreg *mc = (struct mcreg *)ca->ca_master;

		if (mc->mc_ver & MC_VER_NOVME)
			return (0);
	}
#endif
	return (1);
	}

#if defined(MVME162) || defined(MVME167) || defined(MVME177) || defined (MVME187)
/*
 * make local addresses 1G-2G correspond to VME addresses 3G-4G,
 * as D32
 */
#define VME2_D32STARTPHYS	(1*1024*1024*1024UL)
#define VME2_D32ENDPHYS		(2*1024*1024*1024UL)
#define VME2_D32STARTVME	(3*1024*1024*1024UL)
#define VME2_D32BITSVME		(3*1024*1024*1024UL)

/*
 * make local addresses 3G-3.75G correspond to VME addresses 3G-3.75G,
 * as D16
 */
#define VME2_D16STARTPHYS	(3*1024*1024*1024UL)
#define VME2_D16ENDPHYS		(3*1024*1024*1024UL + 768*1024*1024UL)
#endif

/*
 * Returns a physical address mapping for a VME address & length.
 * Note: on some hardware it is not possible to create certain
 * mappings, ie. the MVME147 cannot do 32 bit accesses to VME bus
 * addresses from 0 to physmem.
 */
void *
vmepmap(sc, vmeaddr, len, bustype)
	struct vmesoftc *sc;
	void *vmeaddr;
	int len;
	int bustype;
{
	u_int32_t base = (u_int32_t)vmeaddr;

	len = roundup(len, NBPG);
	switch (vmebustype) {
#if NPCC > 0
	case BUS_PCC:
		switch (bustype) {
		case BUS_VMES:
			printf("base 0x%8x/0x%8x len 0x%x\n",
				vmeaddr, base, len);
			if (base > VME1_A16BASE &&
			    (base+len - VME1_A16BASE) < VME1_A16D16LEN) {
				base = base - VME1_A16BASE + VME1_A16D16BASE;
				printf("vmes1: base = 0x%8x\n", base); /* 1:1 */
			} else if (base > VME1_A32D16BASE &&
			    base+len < VME1_A16BASE) {
				/* 1:1 mapped */
				printf("vmes2: base = 0x%8x\n", base);
			} else {
				printf("%s: cannot map pa 0x%x len 0x%x\n",
				    sc->sc_dev.dv_xname, base, len);
				return (NULL);
			}
			break;
		case BUS_VMEL:
			if (base >= physmem && (base+len) < VME1_A32D32LEN)
				base = base + VME1_A32D32BASE;
			else if (base+len < VME1_A32D16LEN)		/* HACK! */
				base = base + VME1_A32D16BASE;
			else {
				printf("%s: cannot map pa 0x%x len 0x%x\n",
				    sc->sc_dev.dv_xname, base, len);
				return (NULL);
			}
			break;
		}
		break;
#endif
#if NMC > 0 || NPCCTWO > 0
	case BUS_MC:
	case BUS_PCCTWO:
		switch (bustype) {
		case BUS_VMES:
			/*printf("base %x len %d\n", base, len);*/
			if (base > VME2_A16BASE &&
			    (base+len-VME2_A16BASE) < VME2_A16D16LEN) {
				/* XXX busted? */
				base = base - VME2_A16BASE + VME2_A16D16BASE;
			} else if (base > VME2_A24BASE &&
			    (base+len-VME2_A24BASE) < VME2_A24D16LEN) {
				base = base - VME2_A24BASE + VME2_D16STARTPHYS;
			} else if ((base+len) < VME2_A32D16LEN) {
				/* XXX busted? */
				base = base + VME2_A32D16BASE;
			} else {
				printf("vme2chip_map\n");
				base = vme2chip_map(base, len, 16);
				if (base == NULL)
					return (NULL);
			}
			break;
		case BUS_VMEL:
#if 0
			if (base > VME2_A16BASE &&
			    (base+len-VME2_A16BASE) < VME2_A16D32LEN)
				base = base - VME2_A16BASE + VME2_A16D32BASE;
#endif
			base = vme2chip_map(base, len, 32);
			if (base == NULL)
				return (NULL);
			break;
		}
		break;
#endif
	}
	return ((void *)base);
}

/* if successful, returns the va of a vme bus mapping */
void *
vmemap(sc, vmeaddr, len, bustype)
	struct vmesoftc *sc;
	void *vmeaddr;
	int len;
	int bustype;
{
	void *pa, *va;

	pa = vmepmap(sc, vmeaddr, len, bustype);
	if (pa == NULL)
		return (NULL);
	va = mapiodev(pa, len);
	return (va);
}

void
vmeunmap(va, len)
	void *va;
	int len;
{
	unmapiodev(va, len);
}

int
vmerw(sc, uio, flags, bus)
	struct vmesoftc *sc;
	struct uio *uio;
	int flags;
	int bus;
{
	register vm_offset_t o, v;
	register int c;
	register struct iovec *iov;
	void *vme;
	int error = 0;

	while (uio->uio_resid > 0 && error == 0) {
		iov = uio->uio_iov;
		if (iov->iov_len == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			if (uio->uio_iovcnt < 0)
				panic("vmerw");
			continue;
		}

		v = uio->uio_offset;
		c = min(iov->iov_len, MAXPHYS);
		if ((v & PGOFSET) + c > NBPG)	/* max NBPG at a time */
			c = NBPG - (v & PGOFSET);
		if (c == 0)
			return (0);
		vme = vmemap(sc, (void *)(v & ~PGOFSET),
		    NBPG, BUS_VMES);
		if (vme == NULL) {
			error = EFAULT;	/* XXX? */
			continue;
		}
		error = uiomove((void *)vme + (v & PGOFSET), c, uio);
		vmeunmap(vme, NBPG);
	}
	return (error);
}

int
vmeprint(args, bus)
	void *args;
	const char *bus;
{
	struct confargs *ca = args;

	printf(" addr 0x%x", ca->ca_offset);
	if (ca->ca_vec > 0)
		printf(" vec 0x%x", ca->ca_vec);
	if (ca->ca_ipl > 0)
		printf(" ipl %d", ca->ca_ipl);
	return (UNCONF);
}

int
vmescan(parent, child, args, bustype)
	struct device *parent;
	void *child, *args;
	int bustype;
{
	struct cfdata *cf = child;
	struct vmesoftc *sc = (struct vmesoftc *)parent;
	struct confargs *ca = args;
	struct confargs oca;

	if (parent->dv_cfdata->cf_driver->cd_indirect) {
		printf(" indirect devices not supported\n");
		return 0;
	}

	bzero(&oca, sizeof oca);
	oca.ca_bustype = bustype;
	oca.ca_paddr = (void *)cf->cf_loc[0];
	oca.ca_len = cf->cf_loc[1];
	oca.ca_vec = cf->cf_loc[2];
	oca.ca_ipl = cf->cf_loc[3];
	if (oca.ca_ipl > 0 && oca.ca_vec == -1)
		oca.ca_vec = intr_findvec(255, 0);
	if (oca.ca_len == -1)
		oca.ca_len = 4096;

	oca.ca_offset = (u_int)oca.ca_paddr;
	oca.ca_vaddr = vmemap(sc, oca.ca_paddr, oca.ca_len, oca.ca_bustype);
	if (!oca.ca_vaddr)
		oca.ca_vaddr = (void *)-1;
	oca.ca_master = (void *)sc;
	oca.ca_name = cf->cf_driver->cd_name;
	if ((*cf->cf_attach->ca_match)(parent, cf, &oca) == 0) {
		if (oca.ca_vaddr != (void *)-1)
			vmeunmap(oca.ca_vaddr, oca.ca_len);
		return (0);
	}
	/*
	 * If match works, the driver is responsible for
	 * vmunmap()ing if it does not need the mapping. 
	 */
	config_attach(parent, cf, &oca, vmeprint);
	return (1);
}

void
vmeattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct vmesoftc *sc = (struct vmesoftc *)self;
	struct confargs *ca = args;
	struct vme1reg *vme1;
	struct vme2reg *vme2;
	int scon;

	/* XXX any initialization to do? */

	sc->sc_vaddr = ca->ca_vaddr;

	vmebustype = ca->ca_bustype;
	switch (ca->ca_bustype) {
#if NPCC > 0
	case BUS_PCC:
		vme1 = (struct vme1reg *)sc->sc_vaddr;
		scon = (vme1->vme1_scon & VME1_SCON_SWITCH);
		printf(": %sscon\n", scon ? "" : "not ");
		vme1chip_init(sc);
		break;
#endif
#if (NMC > 0) || (NPCCTWO > 0)
	case BUS_MC:
	case BUS_PCCTWO:
		vme2 = (struct vme2reg *)sc->sc_vaddr;
		scon = (vme2->vme2_tctl & VME2_TCTL_SCON);
		printf(": %sscon\n", scon ? "" : "not ");
		vme2chip_init(sc);
		break;
#endif
	}

	while (config_found(self, NULL, NULL))
		;
}

/*
 * On the VMEbus, only one cpu may be configured to respond to any
 * particular vme ipl. Therefore, it wouldn't make sense to globally
 * enable all the interrupts all the time -- it would not be possible
 * to put two cpu's and one vme card into a single cage. Rather, we
 * enable each vme interrupt only when we are attaching a device that
 * uses it. This makes it easier (though not trivial) to put two cpu
 * cards in one VME cage, and both can have some limited access to vme
 * interrupts (just can't share the same irq).
 * Obviously no check is made to see if another cpu is using that
 * interrupt. If you share you will lose.
 */
int
vmeintr_establish(vec, ih)
	int vec;
	struct intrhand *ih;
{
	struct vmesoftc *sc = (struct vmesoftc *) vme_cd.cd_devs[0];
#if NPCC > 0
	struct vme1reg *vme1;
#endif
#if NMC > 0 || NPCCTWO > 0
	struct vme2reg *vme2;
#endif
	int x;

	x = (intr_establish(vec, ih));

	switch (vmebustype) {
#if NPCC > 0
	case BUS_PCC:
		vme1 = (struct vme1reg *)sc->sc_vaddr;
		vme1->vme1_irqen = vme1->vme1_irqen |
		    VME1_IRQ_VME(ih->ih_ipl);
		break;
#endif
#if NMC > 0 || NPCCTWO > 0
	case BUS_MC:
	case BUS_PCCTWO:
		vme2 = (struct vme2reg *)sc->sc_vaddr;
		vme2->vme2_irqen = vme2->vme2_irqen |
		    VME2_IRQ_VME(ih->ih_ipl);
		break;
#endif
	}
	return (x);
}

#if defined(MVME147)
int
vme1chip_init(sc)
	struct vmesoftc *sc;
{
	struct vme1reg *vme1 = (struct vme1reg *)sc->sc_vaddr;

	vme1->vme1_scon &= ~VME1_SCON_SYSFAIL;	/* XXX doesn't work */
}
#endif


#if defined(MVME162) || defined(MVME167) || defined(MVME177) || defined (MVME187)

/*
 * XXX what AM bits should be used for the D32/D16 mappings?
 */
int
vme2chip_init(sc)
	struct vmesoftc *sc;
{
	struct vme2reg *vme2 = (struct vme2reg *)sc->sc_vaddr;
	u_long ctl;

	/* turn off SYSFAIL LED */
	vme2->vme2_tctl &= ~VME2_TCTL_SYSFAIL;

	ctl = vme2->vme2_masterctl;

	/* unused decoders 1 */
	vme2->vme2_master1 = 0;
	ctl &= ~(VME2_MASTERCTL_ALL << VME2_MASTERCTL_1SHIFT);
	printf("%s: 1phys 0x%08x-0x%08x to VMExxx 0x%08x-0x%08x\n",
	    sc->sc_dev.dv_xname,
	    vme2->vme2_master1 << 16, vme2->vme2_master1 & 0xffff0000,
	    vme2->vme2_master1 << 16, vme2->vme2_master1 & 0xffff0000);

	/* unused decoders 2 */
	vme2->vme2_master2 = 0;
	ctl &= ~(VME2_MASTERCTL_ALL << VME2_MASTERCTL_2SHIFT);
	printf("%s: 2phys 0x%08x-0x%08x to VMExxx 0x%08x-0x%08x\n",
	    sc->sc_dev.dv_xname,
	    vme2->vme2_master2 << 16, vme2->vme2_master2 & 0xffff0000,
	    vme2->vme2_master2 << 16, vme2->vme2_master2 & 0xffff0000);

	/* setup a A24D16 space */
	vme2->vme2_master3 = ((VME2_D16ENDPHYS-1) & 0xffff0000) |
	    (VME2_D16STARTPHYS >> 16);
	ctl &= ~(VME2_MASTERCTL_ALL << VME2_MASTERCTL_3SHIFT);
	ctl |= (VME2_MASTERCTL_D16 | VME2_MASTERCTL_AM24UD) <<
	    VME2_MASTERCTL_3SHIFT;
	printf("%s: 3phys 0x%08x-0x%08x to VMED16 0x%08x-0x%08x\n",
	    sc->sc_dev.dv_xname,
	    vme2->vme2_master3 << 16, vme2->vme2_master3 & 0xffff0000,
	    vme2->vme2_master3 << 16, vme2->vme2_master3 & 0xffff0000);

	/* setup a A32D32 space */
	vme2->vme2_master4 = ((VME2_D32ENDPHYS-1) & 0xffff0000) |
	    (VME2_D32STARTPHYS >> 16);
	vme2->vme2_master4mod = (VME2_D32STARTVME & 0xffff0000) |
	    (VME2_D32BITSVME >> 16);
	ctl &= ~(VME2_MASTERCTL_ALL << VME2_MASTERCTL_4SHIFT);
	ctl |= (VME2_MASTERCTL_AM32UD) <<
	    VME2_MASTERCTL_4SHIFT;
	printf("%s: 4phys 0x%08x-0x%08x to VMED32 0x%08x-0x%08x\n",
	    sc->sc_dev.dv_xname,
	    vme2->vme2_master4 << 16, vme2->vme2_master4 & 0xffff0000,
	    vme2->vme2_master4 << 16, vme2->vme2_master4 & 0xffff0000);

	vme2->vme2_masterctl = ctl;
	ctl = vme2->vme2_gcsrctl;

	/* enable A16 short IO map decoder (0xffffxxxx) */
	ctl &= ~(VME2_GCSRCTL_I1EN | VME2_GCSRCTL_I1D16 | VME2_GCSRCTL_I1WP |
	    VME2_GCSRCTL_I1SU);
	ctl |= VME2_GCSRCTL_I1EN | VME2_GCSRCTL_I1D16 | VME2_GCSRCTL_I1SU;

	/* enable A24D16 (0xf0xxxxxx) and A32D16 (0xf[1-e]xxxxxx) decoders */
	ctl &= ~(VME2_GCSRCTL_I2EN | VME2_GCSRCTL_I2WP | VME2_GCSRCTL_I2SU |
	    VME2_GCSRCTL_I2PD);
	ctl |= VME2_GCSRCTL_I2EN | VME2_GCSRCTL_I2SU | VME2_GCSRCTL_I2PD;

	/* map decoders 3 & 4 which were just configured */
	ctl &= ~(VME2_GCSRCTL_MDEN4 | VME2_GCSRCTL_MDEN3 | VME2_GCSRCTL_MDEN1 |
	    VME2_GCSRCTL_MDEN2);
	ctl |= VME2_GCSRCTL_MDEN4 | VME2_GCSRCTL_MDEN3;

	vme2->vme2_gcsrctl = ctl;

	/*
	 * Map the VME irq levels to the cpu levels 1:1.
	 * This is rather inflexible, but much easier.
	 */
	vme2->vme2_irql4 = (7 << VME2_IRQL4_VME7SHIFT) |
	    (6 << VME2_IRQL4_VME6SHIFT) | (5 << VME2_IRQL4_VME5SHIFT) |
	    (4 << VME2_IRQL4_VME4SHIFT) | (3 << VME2_IRQL4_VME3SHIFT) |
	    (2 << VME2_IRQL4_VME2SHIFT) | (1 << VME2_IRQL4_VME1SHIFT);
	printf("%s: vme2_irql4 = 0x%08x\n",	sc->sc_dev.dv_xname,
	    vme2->vme2_irql4);

#if NPCCTWO > 0
	if (vmebustype == BUS_PCCTWO){
		/* 
		 * pseudo driver, abort interrupt handler
		 */
		sc->sc_abih.ih_fn = vme2abort;
		sc->sc_abih.ih_arg = 0;
		sc->sc_abih.ih_ipl = IPL_NMI;
		sc->sc_abih.ih_wantframe = 1;
#if 0
		printf("inserting vme_ab handler\n");
#endif
		intr_establish(110, &sc->sc_abih);
		vme2->vme2_irqen |= VME2_IRQ_AB;
	}
#endif
	vme2->vme2_irqen = vme2->vme2_irqen | VME2_IRQ_ACF;
}

/*
 * A32 accesses on the MVME1[678]x require setting up mappings in
 * the VME2 chip.
 * XXX VME address must be between 2G and 4G
 * XXX We only support D32 at the moment..
 */
u_long
vme2chip_map(base, len, dwidth)
	u_long base;
	int len, dwidth;
{
	switch (dwidth) {
	case 16:
		if (base < VME2_D16STARTPHYS ||
		    base + (u_long)len > VME2_D16ENDPHYS)
			return (NULL);
		return (base);
	case 32:
		if (base < VME2_D32STARTVME)
			return (NULL);
		return (base - VME2_D32STARTVME + VME2_D32STARTPHYS);
	}
}

#if NPCCTWO > 0
int
vme2abort(frame)
	struct frame *frame;
{
	struct vmesoftc *sc = (struct vmesoftc *) vme_cd.cd_devs[0];
	struct vme2reg *vme2 = (struct vme2reg *)sc->sc_vaddr;

	if (vme2->vme2_irqstat & VME2_IRQ_AB == 0) {
		printf("%s: vme2chip irq not set\n", sc->sc_dev.dv_xname);
		return (0);
	}
	vme2->vme2_irqclr = VME2_IRQ_AB;
	nmihand(frame);
	return (1);
}
#endif
#endif /* MVME1[678]x */
