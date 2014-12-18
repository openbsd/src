/* $OpenBSD: prom.c,v 1.16 2014/12/18 10:50:02 dlg Exp $ */
/* $NetBSD: prom.c,v 1.39 2000/03/06 21:36:05 thorpej Exp $ */

/* 
 * Copyright (c) 1992, 1994, 1995, 1996 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <uvm/uvm_extern.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/mutex.h>

#include <machine/cpu.h>
#include <machine/rpb.h>
#define	ENABLEPROM
#include <machine/prom.h>

#include <dev/cons.h>

/* XXX this is to fake out the console routines, while booting. */
struct consdev promcons = { NULL, NULL, promcngetc, promcnputc,
			    nullcnpollc, NULL, makedev(23,0), 1 };

struct rpb	*hwrpb;
int		alpha_console;

extern struct prom_vec prom_dispatch_v;

struct mutex prom_lock = MUTEX_INITIALIZER(IPL_HIGH);

void	prom_enter(void);
void	prom_leave(void);

#ifdef _PMAP_MAY_USE_PROM_CONSOLE
int		prom_mapped = 1;	/* Is PROM still mapped? */

pt_entry_t	prom_pte, saved_pte[1];	/* XXX */
static pt_entry_t *prom_lev1map(void);

static pt_entry_t *
prom_lev1map()
{
	struct alpha_pcb *apcb;

	/*
	 * Find the level 1 map that we're currently running on.
	 */
	apcb = (struct alpha_pcb *)ALPHA_PHYS_TO_K0SEG(curpcb);

	return ((pt_entry_t *)ALPHA_PHYS_TO_K0SEG(apcb->apcb_ptbr << PGSHIFT));
}
#endif /* _PMAP_MAY_USE_PROM_CONSOLE */

void
init_prom_interface(struct rpb *rpb)
{
	struct crb *c;

	c = (struct crb *)((char *)rpb + rpb->rpb_crb_off);

	prom_dispatch_v.routine_arg = c->crb_v_dispatch;
	prom_dispatch_v.routine = c->crb_v_dispatch->entry_va;
}

void
init_bootstrap_console()
{
	char buf[4];

	init_prom_interface(hwrpb);

	prom_getenv(PROM_E_TTY_DEV, buf, 4);
	alpha_console = buf[0] - '0';

	/* XXX fake out the console routines, for now */
	cn_tab = &promcons;
}

#ifdef _PMAP_MAY_USE_PROM_CONSOLE
static void prom_cache_sync(void);
#endif

void
prom_enter(void)
{
	mtx_enter(&prom_lock);

#ifdef _PMAP_MAY_USE_PROM_CONSOLE
	/*
	 * If we have not yet switched out of the PROM's context
	 * (i.e. the first one after alpha_init()), then the PROM
	 * is still mapped, regardless of the `prom_mapped' setting.
	 */
	if (prom_mapped == 0 && curpcb != 0) {
		if (!pmap_uses_prom_console())
			panic("prom_enter");
		{
			pt_entry_t *lev1map;

			lev1map = prom_lev1map();	/* XXX */
			saved_pte[0] = lev1map[0];	/* XXX */
			lev1map[0] = prom_pte;		/* XXX */
		}
		prom_cache_sync();			/* XXX */
	}
#endif
}

void
prom_leave(void)
{
#ifdef _PMAP_MAY_USE_PROM_CONSOLE
	/*
	 * See comment above.
	 */
	if (prom_mapped == 0 && curpcb != 0) {
		if (!pmap_uses_prom_console())
			panic("prom_leave");
		{
			pt_entry_t *lev1map;

			lev1map = prom_lev1map();	/* XXX */
			lev1map[0] = saved_pte[0];	/* XXX */
		}
		prom_cache_sync();			/* XXX */
	}
#endif

	mtx_leave(&prom_lock);
}

#ifdef _PMAP_MAY_USE_PROM_CONSOLE
static void
prom_cache_sync(void)
{
	ALPHA_TBIA();
	alpha_pal_imb();
}
#endif

/*
 * promcnputc:
 *
 * Remap char before passing off to prom.
 *
 * Prom only takes 32 bit addresses. Copy char somewhere prom can
 * find it. This routine will stop working after pmap_rid_of_console 
 * is called in alpha_init. This is due to the hard coded address
 * of the console area.
 */
void
promcnputc(dev_t dev, int c)
{
	prom_return_t ret;
	unsigned char *to = (unsigned char *)0x20000000;

	prom_enter();	/* lock and map prom */
	*to = c;

	do {
		ret.bits = prom_putstr(alpha_console, to, 1);
	} while ((ret.u.retval & 1) == 0);

	prom_leave();		/* unmap prom and unlock */
}

/*
 * promcngetc:
 *
 * Wait for the prom to get a real char and pass it back.
 */
int
promcngetc(dev_t dev)
{
	prom_return_t ret;

	for (;;) {
		prom_enter();
		ret.bits = prom_getc(alpha_console);
		prom_leave();
		if (ret.u.status == 0 || ret.u.status == 1)
			return (ret.u.retval);
	}
}

/*
 * promcnlookc:
 *
 * See if prom has a real char and pass it back.
 */
int
promcnlookc(dev_t dev, char *cp)
{
	prom_return_t ret;

	prom_enter();
	ret.bits = prom_getc(alpha_console);
	prom_leave();
	if (ret.u.status == 0 || ret.u.status == 1) {
		*cp = ret.u.retval;
		return 1;
	} else
		return 0;
}

int
prom_getenv(int id, char *buf, int len)
{
	unsigned char *to = (unsigned char *)0x20000000;
	prom_return_t ret;

	prom_enter();
	ret.bits = prom_getenv_disp(id, to, len);
	bcopy(to, buf, len);
	prom_leave();

	if (ret.u.status & 0x4)
		ret.u.retval = 0;
	buf[ret.u.retval] = '\0';

	return (ret.bits);
}

void
prom_halt(int halt)
{
	struct pcs *p;

	/*
	 * Turn off interrupts, for sanity.
	 */
	(void) splhigh();

	/*
	 * Set "boot request" part of the CPU state depending on what
	 * we want to happen when we halt.
	 */
	p = LOCATE_PCS(hwrpb, hwrpb->rpb_primary_cpu_id);
	p->pcs_flags &= ~(PCS_RC | PCS_HALT_REQ);
	if (halt)
		p->pcs_flags |= PCS_HALT_STAY_HALTED;
	else
		p->pcs_flags |= PCS_HALT_WARM_BOOT;

	/*
	 * Halt the machine.
	 */
	alpha_pal_halt();
}

u_int64_t
hwrpb_checksum()
{
	u_int64_t *p, sum;
	int i;

	for (i = 0, p = (u_int64_t *)hwrpb, sum = 0;
	    i < (offsetof(struct rpb, rpb_checksum) / sizeof (u_int64_t));
	    i++, p++)
		sum += *p;

	return (sum);
}

void
hwrpb_primary_init()
{
	struct pcs *p;

	p = LOCATE_PCS(hwrpb, hwrpb->rpb_primary_cpu_id);

	/* Initialize the primary's HWPCB and the Virtual Page Table Base. */
	bcopy(&proc0.p_addr->u_pcb.pcb_hw, p->pcs_hwpcb,
	    sizeof proc0.p_addr->u_pcb.pcb_hw);
	hwrpb->rpb_vptb = VPTBASE;

	hwrpb->rpb_checksum = hwrpb_checksum();
}

void
hwrpb_restart_setup()
{
	struct pcs *p;

	/* Clear bootstrap-in-progress flag since we're done bootstrapping */
	p = LOCATE_PCS(hwrpb, hwrpb->rpb_primary_cpu_id);
	p->pcs_flags &= ~PCS_BIP;

	/* when 'c'ontinuing from console halt, do a dump */
	hwrpb->rpb_rest_term = (u_int64_t)&XentRestart;
	hwrpb->rpb_rest_term_val = 0x1;

	hwrpb->rpb_checksum = hwrpb_checksum();

	p->pcs_flags |= (PCS_RC | PCS_CV);
}

u_int64_t
console_restart(struct trapframe *framep)
{
	struct pcs *p;

	/* Clear restart-capable flag, since we can no longer restart. */
	p = LOCATE_PCS(hwrpb, hwrpb->rpb_primary_cpu_id);
	p->pcs_flags &= ~PCS_RC;

	/* Fill in the missing frame slots */

	framep->tf_regs[FRAME_PS] = p->pcs_halt_ps;
	framep->tf_regs[FRAME_PC] = p->pcs_halt_pc;
	framep->tf_regs[FRAME_T11] = p->pcs_halt_r25;
	framep->tf_regs[FRAME_RA] = p->pcs_halt_r26;
	framep->tf_regs[FRAME_T12] = p->pcs_halt_r27;

	panic("user requested console halt");

	return (1);
}
