/* $NetBSD: irqhandler.c,v 1.4 1996/03/28 21:43:52 mark Exp $ */

/*
 * Copyright (c) 1994-1996 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * irqhandler.c
 *
 * IRQ/FIQ initialisation, claim, release and handler routines
 *
 * NOTE: Although the irqhandlers support chaining and the claim
 * and release routines install handlers at the top of the chain
 * The low level IRQ handler will only call the top handler in a
 * chain.
 *
 * Created      : 30/09/94
 */

/* Note: Need to remove IRQ_FLAG_ACTIVE as it is not used */

#include <sys/param.h>
#include <sys/systm.h>
#include <vm/vm.h>
#include <sys/syslog.h>

#include <machine/irqhandler.h>
#include <machine/cpu.h>
#include <machine/iomd.h>
#include <machine/katelib.h>
#include <machine/pte.h>

irqhandler_t *irqhandlers[NIRQS];
fiqhandler_t *fiqhandlers;

u_int irqmasks[IRQ_LEVELS];
u_int current_mask;
u_int actual_mask;
u_int disabled_mask;
u_int spl_mask;
u_int soft_interrupts;
extern u_int intrcnt[];

typedef struct {
    vm_offset_t physical;
    vm_offset_t virtual;
} pv_addr_t;
             
extern pv_addr_t systempage;

/* Prototypes */

int podule_irqhandler __P((void));
int irq_claim __P((int /*irq*/, irqhandler_t */*handler*/));
void zero_page_readonly __P((void));
void zero_page_readwrite __P((void));

int fiq_setregs __P((fiqhandler_t *));

/*
 * void irq_init(void)
 *
 * Initialise the IRQ/FIQ sub system
 */

void
irq_init()
{
	int loop;

/* Clear all the IRQ handlers */

	for (loop = 0; loop < NIRQS; ++loop)
		irqhandlers[loop] = NULL;

/* Clear the FIQ handler */

	fiqhandlers = NULL;

/* Clear the IRQ/FIQ masks in the IOMD */

	WriteByte(IOMD_IRQMSKA, 0x00);
	WriteByte(IOMD_IRQMSKB, 0x00);
	WriteByte(IOMD_FIQMSK, 0x00);
	WriteByte(IOMD_DMAMSK, 0x00);

/*
 * Setup the irqmasks for the different Interrupt Priority Levels
 * We will start with no bits set and these will be updated as handlers
 * are installed at different IPL's.
 */

	irqmasks[IPL_BIO]   = 0x00000000;
	irqmasks[IPL_NET]   = 0x00000000;
	irqmasks[IPL_TTY]   = 0x00000000;
	irqmasks[IPL_CLOCK] = 0x00000000;
	irqmasks[IPL_IMP]   = 0x00000000;

	current_mask = 0x00000000;
	actual_mask = 0x00000000;
	spl_mask = 0x00000000;
	soft_interrupts = 0x00000000;

	set_spl_masks();

/* Enable IRQ's and FIQ's */

	enable_interrupts(I32_bit | F32_bit); 
}


/*
 * int irq_claim(int irq, irqhandler_t *handler)
 *
 * Enable an IRQ and install a handler for it.
 */

int
irq_claim(irq, handler)
	int irq;
	irqhandler_t *handler;
{
	int level;

/* IRQ_INSTRUCT indicates that we should get the irq number from the irq structure */

	if (irq == IRQ_INSTRUCT)
		irq = handler->ih_num;
    
/* Make sure the irq number is valid */

	if (irq < 0 || irq >= NIRQS)
		return(-1);

/* Install the handler at the top of the chain */

	handler->ih_next = irqhandlers[irq];
	irqhandlers[irq] = handler;

/*	if (irq == IRQ_VSYNC)
	  {
	    irqhandler_t *x;
	    x = irqhandlers[irq];
	    while (x) {
	    	printf("handler = %08x %08x\n", x, handler);
	    	x = x->ih_next;
	    }
	  }*/

/*
 * Reset the flags for this handler. As it is at the top of the list it
 * must be the active handler.
 */
/* amb - no needed these days */ 
	handler->ih_flags = 0 | IRQ_FLAG_ACTIVE;

/*
 * Record the interrupt number for accounting.
 * Done here as the accounting number may not be the same as the IRQ number
 * though for the moment they are
 */
 
	handler->ih_num = irq;

/* If this is the first interrupt to be attached make a not of any name */

#ifdef IRQSTATS
	if (handler->ih_next == NULL && handler->ih_name) {
		extern char *_intrnames;
		char *ptr = _intrnames + (irq * 14);
/*		printf("intrnames=%08x ptr=%08x irq=%d\n", (u_int)_intrnames, (u_int)ptr, irq);*/
		strcpy(ptr, "             ");
		strncpy(ptr, handler->ih_name, min(strlen(handler->ih_name), 13));
	}
#endif

/*
 * Update the irq masks.
 * This IRQ is allowable at all lower Interrupt Priority Levels.
 */
	if (handler->ih_level >= 0 && handler->ih_level < IRQ_LEVELS) {
		level = handler->ih_level - 1;
		while (level >= 0) {
			irqmasks[level] |= (1 << irq);
			--level;
		}

#include "sl.h"
#include "ppp.h"
#if NSL > 0 || NPPP > 0
/* In the presence of SLIP or PPP, splimp > spltty. */
		irqmasks[IPL_NET] &= irqmasks[IPL_TTY];
#endif
	}
                 
/*
	for (level = 0; level < IRQ_LEVELS; ++level)
		printf("irqmask[%d] = %08x\n", level, irqmasks[level]);
*/

/*
 * Is this an expansion card IRQ and is there a PODULE IRQ handler
 * installed ?
 * If not panic as the podulebus irq handler should have been installed
 * when the podulebus was attached.
 */

	if (irq >= IRQ_EXPCARD0 && irqhandlers[IRQ_PODULE] == NULL)
		panic("Podule IRQ %d claimed but no podulebus handler installed",
		    irq);

	enable_irq(irq);
	set_spl_masks();
    
	return(0);
}


/*
 * int irq_release(int irq, irqhandler_t *handler)
 *
 * Disable an IRQ and remove a handler for it.
 */

int
irq_release(irq, handler)
	int irq;
	irqhandler_t *handler;
{
	int level;

	irqhandler_t *irqhand;
	irqhandler_t **prehand;

/* IRQ_INSTRUCT indicates that we should get the irq number from the irq structure */

	if (irq == IRQ_INSTRUCT)
		irq = handler->ih_num;

/* Make sure the irq number is valid */

	if (irq < 0 || irq >= NIRQS)
		return(-1);

/*
 * Update the irq masks.
 * Remove the IRQ from all the approriate IPL's
 */
  
	if (handler->ih_level >= 0 && handler->ih_level < IRQ_LEVELS) {
		level = handler->ih_level - 1;
		while (level >= 0) {
			irqmasks[level] &= ~(1 << irq);
			--level;
		}
	}

/* Locate the handler */

	irqhand = irqhandlers[irq];
	prehand = &irqhandlers[irq];
    
	while (irqhand && handler != irqhand) {
		prehand = &irqhand;
		irqhand = irqhand->ih_next;
	}

/* Remove the handler if located */
      
	if (irqhand)
		*prehand = irqhand->ih_next;
	else
		return(-1);

/* Flag the handler being removed as non active (in case it was) */ 
/* Not needed these days - AMB */
	irqhand->ih_flags &= ~IRQ_FLAG_ACTIVE;

/* Make sure the head of the handler list is active */

	if (irqhandlers[irq])
		irqhandlers[irq]->ih_flags |= IRQ_FLAG_ACTIVE;

/*
 * Disable the appropriate mask bit if there are no handlers left for
 * this IRQ.
 */

	if (irqhandlers[irq] == NULL)
		disable_irq(irq);

	set_spl_masks();
      
	return(0);
}


u_int
disable_interrupts(mask)
	u_int mask;
{
	register u_int cpsr;

	cpsr = SetCPSR(mask, mask);
	if ((GetCPSR() & I32_bit) == 0)
		printf("Alert ! disable_interrupts has failed\n");

	return(cpsr);
}


u_int
restore_interrupts(old_cpsr)
	u_int old_cpsr;
{
	register int mask = I32_bit | F32_bit;
	return(SetCPSR(mask, old_cpsr & mask));
}


u_int
enable_interrupts(mask)
	u_int mask;
{
	return(SetCPSR(mask, 0));
}


/*
 * void disable_irq(int irq)
 *
 * Disables a specific irq. The irq is removed from the master irq mask
 */

void
disable_irq(irq)
	int irq;
{
	register int oldirqstate; 

	oldirqstate = disable_interrupts(I32_bit);
	current_mask &= ~(1 << irq);
	irq_setmasks();
	restore_interrupts(oldirqstate);
}  


/*
 * void enable_irq(int irq)
 *
 * Enables a specific irq. The irq is added to the master irq mask
 * This routine should be used with caution. A handler should already
 * be installed.
 */

void
enable_irq(irq)
	int irq;
{
	register u_int oldirqstate; 

	oldirqstate = disable_interrupts(I32_bit);
	current_mask |= (1 << irq);
	irq_setmasks();
	restore_interrupts(oldirqstate);
}  


/*
 * void stray_irqhandler(u_int mask)
 *
 * Handler for stray interrupts. This gets called if a handler cannot be
 * found for an interrupt.
 */

void
stray_irqhandler(mask)
	u_int mask;
{
/*	panic("Stray IRQ received (%08x)", mask);*/
	static u_int stray_irqs = 0;

	if (++stray_irqs <= 8)
		log(LOG_ERR, "Stray interrupt %08x%s\n", mask,
		    stray_irqs >= 8 ? ": stopped logging" : "");
}


void
dosoftints()
{
	register u_int softints;
    
	softints = soft_interrupts & spl_mask;
	if (softints & IRQMASK_SOFTCLOCK) {
		int s;
		
		++cnt.v_soft;
		++intrcnt[IRQ_SOFTCLOCK];
		soft_interrupts &= ~IRQMASK_SOFTCLOCK;
		s = lowerspl(SPL_SOFT);
		softclock();
		(void)splx(s);
	}
	if (softints & IRQMASK_SOFTNET) {
		++cnt.v_soft;
		++intrcnt[IRQ_SOFTNET];
		soft_interrupts &= ~IRQMASK_SOFTNET;
#ifdef INET
#include "ether.h"
#if NETHER > 0
		arpintr();
#endif
		ipintr();
#endif
#ifdef IMP
		impintr();
#endif
#ifdef NS
		nsintr();
#endif
#ifdef ISO
		clnlintr();
#endif
#ifdef CCITT
		ccittintr();
#endif                                                         
#include "ppp.h"
#if NPPP > 0
		pppintr();
#endif
	}
}

extern vgone();
extern vfinddev();
extern idle();
extern cpu_switch();
extern switch_exit();

void
validate_irq_address(irqf, mask)
	irqframe_t *irqf;
	u_int mask;
{
	return;
	if (irqf->if_pc > (int)idle && irqf->if_pc < (int)switch_exit)
		return;
	if (irqf->if_pc > (int)SetCPSR && irqf->if_pc < (int)GetCPSR)
		return;
	if ((irqf->if_spsr & PSR_MODE) != PSR_USR32_MODE) {
		printf("Alert! IRQ while in non USR mode (%08x) pc=%08x\n",
		    irqf->if_spsr, irqf->if_pc);
	}
	if ((GetCPSR() & I32_bit) == 0) {
		printf("Alert! IRQ's enabled during IRQ handler\n");
	}
	if (irqf->if_pc >= (int)vgone && irqf->if_pc < (int)vfinddev)
		printf("Alert! IRQ between vgone & vfinddev : pc=%08x\n",
		    irqf->if_pc);
}


/*
 * int fiq_claim(fiqhandler_t *handler)
 *
 * Claim FIQ's and install a handler for them.
 */

int
fiq_claim(handler)
	fiqhandler_t *handler;
{
/* Fail if the FIQ's are already claimed */

	if (fiqhandlers)
		return(-1);

	if (handler->fh_size > 0xc0)
		return(-1);

/* Install the handler */

	fiqhandlers = handler;

/* Now we have to actually install the FIQ handler */

/* Eventually we will copy this down but for the moment ... */

	zero_page_readwrite();

	WriteWord(0x0000003c, (u_int) handler->fh_func);
    
	zero_page_readonly();
    
/*	bcopy(handler->fh_func, 0x0000001c, handler->fh_size);*/

/* We must now set up the FIQ registers */

	fiq_setregs(handler);

/* Set up the FIQ mask */

	WriteWord(IOMD_FIQMSK, handler->fh_mask);
    
/* Make sure that the FIQ's are enabled */
    
	enable_interrupts(F32_bit);
	return(0);
}


/*
 * int fiq_release(fiqhandler_t *handler)
 *
 * Release FIQ's and remove a handler for them.
 */

int
fiq_release(handler)
	fiqhandler_t *handler;
{
/* Fail if the handler is wrong */

	if (fiqhandlers != handler)
		return(-1);

/* Disable FIQ interrupts */
      
	disable_interrupts(F32_bit);

/* Clear up the FIQ mask */

	WriteWord(IOMD_FIQMSK, 0x00);

/* Remove the handler */

	fiqhandlers = NULL;
	return(0);
}

/* End of irqhandler.c */
