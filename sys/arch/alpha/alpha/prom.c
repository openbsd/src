/*	$NetBSD: prom.c,v 1.4 1995/08/03 00:58:33 cgd Exp $	*/

/* 
 * Copyright (c) 1992, 1994, 1995 Carnegie Mellon University
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

#include <machine/rpb.h>
#include <machine/prom.h>
#include <machine/pte.h>

#include <dev/cons.h>

/* XXX this is to fake out the console routines, while booting. */
void promcnputc __P((dev_t, int));
int promcngetc __P((dev_t));
struct consdev promcons = { NULL, NULL, promcngetc, promcnputc,
			    nullcnpollc, makedev(23,0), 1 };

struct rpb	*hwrpb;
int		alpha_console;
int		prom_mapped = 1;	/* Is PROM still mapped? */

extern struct prom_vec prom_dispatch_v;

pt_entry_t	*rom_ptep, rom_pte, saved_pte;	/* XXX */

void
init_prom_interface()
{
	struct crb *c;
	char buf[4];

	c = (struct crb*)((char*)hwrpb + hwrpb->rpb_crb_off);

        prom_dispatch_v.routine_arg = c->crb_v_dispatch;
        prom_dispatch_v.routine = c->crb_v_dispatch->code;

	prom_getenv(PROM_E_TTY_DEV, buf, 4);
	alpha_console = buf[0] - '0';

	/* XXX fake out the console routines, for now */
	cn_tab = &promcons;
}

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
promcnputc(dev, c)
	dev_t dev;
	int c;
{
        prom_return_t ret;
	unsigned char *to = (unsigned char *)0x20000000;
	int s;

#ifdef notdef /* XXX */
	if (!prom_mapped)
		return;
#endif

	s = splhigh();
	if (!prom_mapped) {					/* XXX */
		saved_pte = *rom_ptep;				/* XXX */
		*rom_ptep = rom_pte;				/* XXX */
		TBIA();						/* XXX */
	}							/* XXX */
	*to = c;

	do {
		ret.bits = prom_dispatch(PROM_R_PUTS, alpha_console, to, 1);
	} while ((ret.u.retval & 1) == 0);

	if (!prom_mapped) {					/* XXX */
		*rom_ptep = saved_pte;				/* XXX */
		TBIA();						/* XXX */
	}							/* XXX */
	splx(s);
}

/*
 * promcngetc:
 *
 * Wait for the prom to get a real char and pass it back.
 */
int
promcngetc(dev)
	dev_t dev;
{
        prom_return_t ret;
	int s;

#ifdef notdef /* XXX */
	if (!prom_mapped)
		return (-1);
#endif

        for (;;) {
		s = splhigh();
		if (!prom_mapped) {				/* XXX */
			saved_pte = *rom_ptep;			/* XXX */
			*rom_ptep = rom_pte;			/* XXX */
			TBIA();					/* XXX */
		}						/* XXX */
                ret.bits = prom_dispatch(PROM_R_GETC, alpha_console);
		if (!prom_mapped) {				/* XXX */
			*rom_ptep = saved_pte;			/* XXX */
			TBIA();					/* XXX */
		}						/* XXX */
		splx(s);
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
promcnlookc(dev, cp)
	dev_t dev;
	char *cp;
{
        prom_return_t ret;
	int s;

#ifdef notdef /* XXX */
	if (!prom_mapped)
		return (-1);
#endif

	s = splhigh();
	if (!prom_mapped) {					/* XXX */
		saved_pte = *rom_ptep;				/* XXX */
		*rom_ptep = rom_pte;				/* XXX */
		TBIA();						/* XXX */
	}							/* XXX */
	ret.bits = prom_dispatch(PROM_R_GETC, alpha_console);
	if (!prom_mapped) {					/* XXX */
		*rom_ptep = saved_pte;				/* XXX */
		TBIA();						/* XXX */
	}
	splx(s);
	if (ret.u.status == 0 || ret.u.status == 1) {
		*cp = ret.u.retval;
		return 1;
	} else
		return 0;
}

int
prom_getenv(id, buf, len)
	int id, len;
	char *buf;
{
	unsigned char *to = (unsigned char *)0x20000000;
	prom_return_t ret;
	int s;

#ifdef notdef /* XXX */
	if (!prom_mapped)
		return (-1);
#endif

	s = splhigh();
	if (!prom_mapped) {					/* XXX */
		saved_pte = *rom_ptep;				/* XXX */
		*rom_ptep = rom_pte;				/* XXX */
		TBIA();						/* XXX */
	}							/* XXX */
	ret.bits = prom_dispatch(PROM_R_GETENV, id, to, len);
	bcopy(to, buf, len);
	if (!prom_mapped) {					/* XXX */
		*rom_ptep = saved_pte;				/* XXX */
		TBIA();						/* XXX */
	}							/* XXX */
	splx(s);
	return (ret.bits);
}

void
prom_halt(halt)
	int halt;
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
	p = (struct pcs *)((char *)hwrpb + hwrpb->rpb_pcs_off);
	p->pcs_flags &= ~(PCS_RC | PCS_HALT_REQ);
	if (halt)
		p->pcs_flags |= PCS_HALT_STAY_HALTED;
	else
		p->pcs_flags |= PCS_HALT_WARM_BOOT;

	/*
	 * Halt the machine.
	 */
	pal_halt();
}
