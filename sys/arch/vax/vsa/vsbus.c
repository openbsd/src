/*	$NetBSD: vsbus.c,v 1.4 1996/10/13 03:36:17 christos Exp $ */
/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
 * All rights reserved.
 *
 * This code is derived from software contributed to Ludd by Bertram Barth.
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
 *	This product includes software developed at Ludd, University of 
 *	Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/map.h>
#include <sys/device.h>
#include <sys/dkstat.h>
#include <sys/disklabel.h>
#include <sys/syslog.h>
#include <sys/stat.h>

#include <machine/pte.h>
#include <machine/sid.h>
#include <machine/scb.h>
#include <machine/cpu.h>
#include <machine/trap.h>
#include <machine/nexus.h>

#include <machine/uvax.h>
#include <machine/ka410.h>
#include <machine/ka43.h>

#include <machine/vsbus.h>

#define trace(x)
#define debug(x)

int	vsbus_match	__P((struct device *, void *, void *));
void	vsbus_attach	__P((struct device *, struct device *, void *));
int	vsbus_print	__P((void *, const char *));

void	ka410_attach	__P((struct device *, struct device *, void *));
void	ka43_attach	__P((struct device *, struct device *, void *));

struct	cfdriver vsbus_cd = { 
	NULL, "vsbus", DV_DULL 
};
struct	cfattach vsbus_ca = { 
	sizeof(struct device), vsbus_match, vsbus_attach
};

/*
void	vsbus_intr_register __P((struct confargs *ca, int (*)(void*), void*));
void	vsbus_intr_unregister __P((struct confargs *));
*/

void	vsbus_intr_dispatch __P((int i));

#define VSBUS_MAXDEVS	8
#define VSBUS_MAXINTR	8

struct confargs *vsbus_devs = NULL;

#ifdef VAX410	/* also: KA420 */
struct confargs ka410_devs[] = {
	/* name		intslot intpri intvec	intbit	ioaddr	*/
	{ "dc",		7,	7,	0x2C0,	(1<<7), KA410_SER_BASE, 
			6,	6,	0x2C4,	(1<<6), 0x01,		},
	{ "dc (xmit)",	6,	6,	0x2C4,	(1<<6), KA410_SER_BASE, },
	{ "le",		5,	5,	0x250,	(1<<5), KA410_LAN_BASE, 
			KA410_NWA_BASE, 0x00,				},
	{ "ncr",	1,	1,	0x3F8,	(1<<1), KA410_SCS_BASE,
			KA410_SCS_DADR, KA410_SCS_DCNT, KA410_SCS_DDIR,
			KA410_DMA_BASE, KA410_DMA_SIZE, 0x00,	0x07,	},
	{ "hdc",	0,	0,	0x3FC,	(1<<0), KA410_DKC_BASE, 
			0, 0, 0, 
			KA410_DMA_BASE, KA410_DMA_SIZE, 0x00,		},
#if 0
	{ "dc (recv)",	7,	7,	0x2C0,	(1<<7), KA410_SER_BASE, },
	{ "dc (xmit)",	6,	6,	0x2C4,	(1<<6), KA410_SER_BASE, },
	{ "hdc9224",	0,	0,	0x3FC,	(1<<0), KA410_DKC_BASE, },
	{ "ncr5380",	1,	1,	0x3F8,	(1<<1), KA410_SCS_BASE, },
	{ "am7990",	5,	5,	0x250,	(1<<5), KA410_LAN_BASE, },
	{ "NETOPT",	4,	4,	0x254,	(1<<4), KA410_LAN_BASE, },
#endif
	{ "" },
};
#endif

#ifdef VAX43
struct confargs ka43_devs[] = {
	/* name		intslot intpri intvec	intbit	ioaddr	*/
	{ "dc",		7,	7,	0x2C0,	(1<<7), KA43_SER_BASE,	
			6,	6,	0x2C4,	(1<<6), 0x01,		},
	{ "dc (xmit)",	6,	6,	0x2C4,	(1<<6), KA43_SER_BASE,	},
	{ "le",		5,	5,	0x250,	(1<<5), KA43_LAN_BASE,	
			KA43_NWA_BASE,	0x00,				},
	{ "ncr",	1,	1,	0x3F8,	(1<<1), KA43_SC1_BASE,
			KA43_SC1_DADR,	KA43_SC1_DCNT,	KA43_SC1_DDIR,	
			KA43_DMA_BASE,	KA43_DMA_SIZE,	0x01,	0x06,	},
	{ "ncr",	0,	0,	0x3FC,	(1<<0), KA43_SC2_BASE,
			KA43_SC2_DADR,	KA43_SC2_DCNT,	KA43_SC2_DDIR,
			KA43_DMA_BASE,	KA43_DMA_SIZE,	0x01,	0x06,	},
#if 0
	{ "le (2nd)",	4,	4,	0x254,	(1<<4), 0x???,		},
	{ "NETOPT",	4,	4,	0x254,	(1<<4), 0x???,		},
#endif
	{ "" },
};
#endif

int
vsbus_print(aux, name)
	void *aux;
	const char *name;
{
	struct confargs *ca = aux;

	trace(("vsbus_print(%x, %s)\n", ca->ca_name, name));

	if (name) {
		printf ("device %s at %s", ca->ca_name, name);
		return (UNSUPP);
	}
	return (UNCONF); 
}

int
vsbus_match(parent, cf, aux)
	struct	device	*parent;
	void	*cf;
	void	*aux;
{
	struct bp_conf *bp = aux;
	
	trace(("vsbus_match: bp->type = \"%s\"\n", bp->type));

	if (strcmp(bp->type, "vsbus"))
		return 0;
	/*
	 * on machines which can have it, the vsbus is always there
	 */
	if ((vax_bustype & VAX_VSBUS) == 0)
		return (0);

	return (1);
}

#if 1	/*------------------------------------------------------------*/
#if 1
#define REG(name)	short name; short X##name##X;
#else
#define REG(name)	int name;
#endif
static volatile struct {/* base address of DZ-controller: 0x200A0000 */
  REG(csr);		/* 00 Csr: control/status register */
  REG(rbuf);		/* 04 Rbuf/Lpr: receive buffer/line param reg. */
  REG(tcr);		/* 08 Tcr: transmit console register */
  REG(tdr);		/* 0C Msr/Tdr: modem status reg/transmit data reg */
  REG(lpr0);		/* 10 Lpr0: */
  REG(lpr1);		/* 14 Lpr0: */
  REG(lpr2);		/* 18 Lpr0: */
  REG(lpr3);		/* 1C Lpr0: */
} *dz = (void*)0x200A0000; 
extern int dzcnrint();
extern int dzcntint();
int hardclock_count = 0;
int
ka410_consintr_enable()
{
	vsbus_intr_enable(&ka410_devs[0]);
	vsbus_intr_enable(&ka410_devs[1]);
}

int
ka410_consRecv_intr(p)
	void *p;
{
  /* printf("ka410_consRecv_intr: hc-count=%d\n", hardclock_count); */
  dzcnrint();
  /* printf("gencnrint() returned.\n"); */
  return(0);
}

int
ka410_consXmit_intr(p)
	void *p;
{
  /* printf("ka410_consXmit_intr: hc-count=%d\n", hardclock_count); */
  dzcntint();
  /* printf("gencntint() returned.\n"); */
  return(0);
}
#endif	/*------------------------------------------------------------*/

void
vsbus_attach(parent, self, aux)
	struct	device	*parent, *self;
	void	*aux;
{
	struct confargs *ca;
	int i;

	printf("\n");
	trace (("vsbus_attach()\n"));

	printf("vsbus_attach: boardtype = %x\n", vax_boardtype);

	switch (vax_boardtype) {
	case VAX_BTYP_410:
	case VAX_BTYP_420:
		vsbus_devs = ka410_devs;
		break;

	case VAX_BTYP_43:
	case VAX_BTYP_46:
	case VAX_BTYP_49:
		vsbus_devs = ka43_devs;
		break;

	default:
		printf ("unsupported boardtype 0x%x in vsbus_attach()\n",
			vax_boardtype);
		return;
	}

	/*
	 * first setup interrupt-table, so that devices can register
	 * their interrupt-routines...
	 */
	vsbus_intr_setup();	

	/*
	 * now check for all possible devices on this "bus"
	 */
	for (i=0; i<VSBUS_MAXDEVS; i++) {
		ca = &vsbus_devs[i];
		if (*ca->ca_name == '\0')
			break;
		config_found(self, (void*)ca, vsbus_print);
	}

	/*
	 * as long as there's no working DZ-driver, we use this dummy
	 */
	vsbus_intr_register(&ka410_devs[0], ka410_consRecv_intr, NULL);
	vsbus_intr_register(&ka410_devs[1], ka410_consXmit_intr, NULL);
}

#define VSBUS_MAX_INTR	8	/* 64? */
/*
 * interrupt service routines are given an int as argument, which is
 * pushed onto stack as LITERAL. Thus the value is between 0-63.
 * This array of 64 might be oversized for now, but it's all which 
 * ever will be possible.
 */
struct vsbus_ivec {
	struct ivec_dsp intr_vec;		/* this is referenced in SCB */
	int		intr_count;		/* keep track of interrupts */
	int		intr_flags;		/* valid, etc. */
	void		(*enab)(int);		/* enable interrupt */
	void		(*disab)(int);		/* disable interrupt */
	void		(*prep)(int);		/* need pre-processing? */
	int		(*handler)(void*);	/* isr-routine to call */
	void		*hndlarg;		/* args to this routine */
	void		(*postp)(int);		/* need post-processing? */
} vsbus_ivtab[VSBUS_MAX_INTR];

/*
 * 
 */
int
vsbus_intr_setup()
{
	int i;
	struct vsbus_ivec *ip;
	extern struct ivec_dsp idsptch;		/* subr.s */

	for (i=0; i<VSBUS_MAX_INTR; i++) {
		ip = &vsbus_ivtab[i];
		bcopy(&idsptch, &ip->intr_vec, sizeof(struct ivec_dsp));
		ip->intr_vec.pushlarg = i;
		ip->intr_vec.hoppaddr = vsbus_intr_dispatch;
		ip->intr_count = 0;
		ip->intr_flags = 0;
		ip->enab = NULL;
		ip->disab = NULL;
		ip->postp = NULL;
	}
	switch (vax_boardtype) {
	case VAX_BTYP_410:
	case VAX_BTYP_420:
	case VAX_BTYP_43:
	case VAX_BTYP_46:
	case VAX_BTYP_49:
		ka410_intr_setup();
		return(0);
	default:
		printf("unsupported board-type 0x%x in vsbus_intr_setup()\n",
			vax_boardtype);
		return(1);
	}
}

int
vsbus_intr_register(ca, handler, arg)
	struct confargs *ca;
	int (*handler)(void*);
	void *arg;
{
	/* struct device *dev = arg; */
	int i = ca->ca_intslot;
	struct vsbus_ivec *ip = &vsbus_ivtab[i];

	trace (("vsbus_intr_register(%s/%d)\n", ca->ca_name, ca->ca_intslot));

	ip->handler = handler;
	ip->hndlarg = arg;
}

int
vsbus_intr_enable(ca)
	struct confargs *ca;
{
	int i = ca->ca_intslot;
	struct vsbus_ivec *ip = &vsbus_ivtab[i];

	trace (("vsbus_intr_enable(%s/%d)\n", ca->ca_name, ca->ca_intslot));

	/* XXX check for valid handler etc. !!! */
	if (ip->handler == NULL) {
		printf("interrupts for \"%s\"(%d) not enabled: null-handler\n",
		      ca->ca_name, ca->ca_intslot);
		return;
	}

	ip->enab(i);
}

int
vsbus_intr_disable(ca)
	struct confargs *ca;
{
	int i = ca->ca_intslot;
	struct vsbus_ivec *ip = &vsbus_ivtab[i];

	trace (("vsbus_intr_disable(%s/%d)\n", ca->ca_name, i));

	ip->disab(i);
}

int 
vsbus_intr_unregister(ca)
	struct confargs *ca;
{
	int i = ca->ca_intslot;
	struct vsbus_ivec *ip = &vsbus_ivtab[i];

	trace (("vsbus_intr_unregister(%s/%d)\n", ca->ca_name, i));

	ip->handler = NULL;
	ip->hndlarg = NULL;
}

void
vsbus_intr_dispatch(i)
	register int i;
{
	register struct vsbus_ivec *ip = &vsbus_ivtab[i];

	trace (("vsbus_intr_dispatch(%d)", i));
	
	if (i < VSBUS_MAX_INTR && ip->handler != NULL) {
		ip->intr_count++;
		debug (("intr-count[%d] = %d\n", i, ip->intr_count));
		(ip->handler)(ip->hndlarg);
		if (ip->postp)
			(ip->postp)(i);
		return;
	}

	if (i < 0 || i >= VSBUS_MAX_INTR) {
		printf ("stray interrupt %d on vsbus.\n", i);
		return;
	}

	if (!ip->handler) {
		printf ("unhandled interrupt %d on vsbus.\n", i);
		return;
	}
}

/*
 * These addresses are invalid and will be updated/corrected by
 * ka410_intr_setup(), but having them this way helps debugging
 */
static volatile u_char *ka410_intmsk = (void*)KA410_INTMSK;
static volatile u_char *ka410_intreq = (void*)KA410_INTREQ;
static volatile u_char *ka410_intclr = (void*)KA410_INTCLR;

static void
ka410_intr_enable(i)
	int i;
{
	trace (("ka410_intr_enable(%d)\n", i));
	*ka410_intmsk |= (1<<i);
}

static void
ka410_intr_disable(i)
	int i;
{
	trace (("ka410_intr_disable(%d)\n", i));
	*ka410_intmsk &= ~(1<<i);
}

static void
ka410_intr_clear(i)
	int i;
{
	trace (("ka410_intr_clear(%d)\n", i));
	*ka410_intclr = (1<<i);
}

ka410_intr_setup()
{
	int i;
	struct vsbus_ivec *ip;
	void **scbP = (void*)scb;

	trace (("ka410_intr_setup()\n"));

	ka410_intmsk = (void*)uvax_phys2virt(KA410_INTMSK);
	ka410_intreq = (void*)uvax_phys2virt(KA410_INTREQ);
	ka410_intclr = (void*)uvax_phys2virt(KA410_INTCLR);

	*ka410_intmsk = 0;		/* disable all interrupts */
	*ka410_intclr = 0xFF;		/* clear all old interrupts */

	/*
	 * insert the VS2000-specific routines into ivec-table...
	 */
	for (i=0; i<8; i++) {
		ip = &vsbus_ivtab[i];
		ip->enab  = ka410_intr_enable;
		ip->disab = ka410_intr_disable;
		/* ip->postp = ka410_intr_clear; bertram XXX */
	}
	/*
	 * ...and register the interrupt-vectors in SCB
	 */
	scbP[IVEC_DC/4] = &vsbus_ivtab[0].intr_vec;
	scbP[IVEC_SC/4] = &vsbus_ivtab[1].intr_vec;
	scbP[IVEC_VS/4] = &vsbus_ivtab[2].intr_vec;
	scbP[IVEC_VF/4] = &vsbus_ivtab[3].intr_vec;
	scbP[IVEC_NS/4] = &vsbus_ivtab[4].intr_vec;
	scbP[IVEC_NP/4] = &vsbus_ivtab[5].intr_vec;
	scbP[IVEC_ST/4] = &vsbus_ivtab[6].intr_vec;
	scbP[IVEC_SR/4] = &vsbus_ivtab[7].intr_vec;
}

/*
 *
 *
 */

static volatile struct dma_lock {
	int	dl_locked;
	int	dl_wanted;
	void	*dl_owner;
	int	dl_count;
} dmalock = { 0, 0, NULL, 0 };

int
vsbus_lockDMA(ca)
	struct confargs *ca;
{
	while (dmalock.dl_locked) {
		dmalock.dl_wanted++;
		sleep((caddr_t)&dmalock, PRIBIO);	/* PLOCK or PRIBIO ? */
		dmalock.dl_wanted--;
	}
	dmalock.dl_locked++;
	dmalock.dl_owner = ca;

	/*
	 * no checks yet, no timeouts, nothing...
	 */

#ifdef DEBUG
	if ((++dmalock.dl_count % 1000) == 0)
		printf("%d locks, owner: %s\n", dmalock.dl_count, ca->ca_name);
#endif
	return (0);
}

int 
vsbus_unlockDMA(ca)
	struct confargs *ca;
{
	if (dmalock.dl_locked != 1 || dmalock.dl_owner != ca) {
		printf("locking-problem: %d, %s\n", dmalock.dl_locked,
		       (dmalock.dl_owner ? dmalock.dl_owner : "null"));
		dmalock.dl_locked = 0;
		return (-1);
	}
	dmalock.dl_owner = NULL;
	dmalock.dl_locked = 0;
	if (dmalock.dl_wanted) {
		wakeup((caddr_t)&dmalock);
	}
	return (0);
}

/*----------------------------------------------------------------------*/
#if 0
/*
 * small set of routines needed for mapping when doing pseudo-DMA,
 * quasi-DMA or virtual-DMA (choose whatever name you like).
 *
 * Once I know how VS3100 is doing real DMA (I hope it does), this
 * should be rewritten to present a general interface...
 *
 */

extern u_long uVAX_physmap;

u_long
vsdma_mapin(bp, len)
	struct buf *bp;
	int len;
{
	pt_entry_t *pte;	/* pointer to Page-Table-Entry */
	struct pcb *pcb;	/* pointer to Process-Controll-Block */
	pt_entry_t *xpte;
	caddr_t addr;
	int pgoff;		/* offset into 1st page */
	int pgcnt;		/* number of pages needed */
	int pfnum;
	int i;

	trace(("mapin(bp=%x, bp->data=%x)\n", bp, bp->b_data));

	addr = bp->b_data;
	pgoff = (int)bp->b_data & PGOFSET;	/* get starting offset */
	pgcnt = btoc(bp->b_bcount + pgoff) + 1; /* one more than needed */
	
	/*
	 * Get a pointer to the pte pointing out the first virtual address.
	 * Use different ways in kernel and user space.
	 */
	if ((bp->b_flags & B_PHYS) == 0) {
		pte = kvtopte(addr);
	} else {
		pcb = bp->b_proc->p_vmspace->vm_pmap.pm_pcb;
		pte = uvtopte(addr, pcb);
	}

	/*
	 * When we are doing DMA to user space, be sure that all pages
	 * we want to transfer to are mapped. WHY DO WE NEED THIS???
	 * SHOULDN'T THEY ALWAYS BE MAPPED WHEN DOING THIS???
	 */
	for (i=0; i<(pgcnt-1); i++) {
		if ((pte + i)->pg_pfn == 0) {
			int rv;
			rv = vm_fault(&bp->b_proc->p_vmspace->vm_map,
				      (unsigned)addr + i * NBPG,
				      VM_PROT_READ|VM_PROT_WRITE, FALSE);
			if (rv)
				panic("vs-DMA to nonexistent page, %d", rv);
		}
	}

	/*
	 * now insert new mappings for this memory area into kernel's
	 * mapping-table
	 */
	xpte = kvtopte(uVAX_physmap);
	while (--pgcnt > 0) {
		pfnum = pte->pg_pfn;
		if (pfnum == 0)
			panic("vsbus: zero entry");
		*(int *)xpte++ = *(int *)pte++;
	}
	*(int *)xpte = 0;	/* mark last mapped page as invalid! */

	debug(("uVAX: 0x%x\n", uVAX_physmap + pgoff));

	return (uVAX_physmap + pgoff);	/* ??? */
}
#endif
/*----------------------------------------------------------------------*/
/*
 * Here follows some currently(?) unused stuff. Someday this should be removed
 */

#if 0
/*
 * Configure devices on VS2000/KA410 directly attached to vsbus
 */
void
ka410_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct confargs *ca;
	int i;

	for (i=0; i<KA410_MAXDEVS; i++) {
		ca = &ka410_devs[i];
		if (*ca->ca_name == '\0')
			break;
		config_found(self, (void*)ca, vsbus_print);
	}
	/*
	 * as long as there's no real DZ-driver, we used this dummy
	 */
	vsbus_intr_register(&ka410_devs[0], ka410_consRecv_intr, NULL);
	vsbus_intr_register(&ka410_devs[1], ka410_consXmit_intr, NULL);
}

#endif
