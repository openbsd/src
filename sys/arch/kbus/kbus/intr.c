/*	$NetBSD: intr.c,v 1.13 1996/03/31 23:35:20 pk Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)intr.c	8.3 (Berkeley) 11/11/93
 */

#include "ppp.h"
#include "bridge.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <vm/vm.h>

#include <dev/cons.h>

#include <net/netisr.h>
#include <net/if.h>

#include <machine/cpu.h>
#include <machine/asi.h>
#include <machine/sic.h>
#include <machine/trap.h>
#include <machine/asm.h>
#include <machine/autoconf.h>
#include <machine/kbus.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip_var.h>
#endif
#ifdef NS
#include <netns/ns_var.h>
#endif
#ifdef ISO
#include <netiso/iso.h>
#include <netiso/clnp.h>
#endif
#include "ppp.h"
#if NPPP > 0
#include <net/ppp_defs.h>
#include <net/if_ppp.h>
#endif

void	strayintr __P((struct clockframe *));
int	soft01intr __P((void *));
void	sic_init __P((void));
void	do_sir __P((void));
static void netintr __P((void));
void	zssoft __P((void));

unsigned int ssir;
volatile int hir;

volatile unsigned char intrdelay[256];
static int delay_low, delay_high;
int intrcnt[256];

/*
 * Stray interrupt handler.  Clear it if possible.
 * If not, and if we get 10 interrupts in 10 seconds, panic.
 */
void
strayintr(fp)
	struct clockframe *fp;
{
	static int straytime, nstray;
	int timesince;

	printf("stray interrupt ipv=%x ipr=%x pc=%x npc=%x psr=%b\n",
	       fp->ipv, fp->ipr, fp->pc, fp->npc, fp->psr, PSR_BITS);

	printf ("DIR: %x, IPR: %x, IRC: %x, IXR: %x, IXC: %x\n",
		lduba (ASI_DIR, 0) & SIC_DIR_MASK,
		lda (ASI_IPR, 0) & SIC_IPR_MASK,
		lduba (ASI_IRXC, 0) & 0x3,
		lda (ASI_IXR, 0) & 0xffff,
		lduba (ASI_ITXC, 0) & 0x3);
	timesince = time.tv_sec - straytime;
	if (timesince <= 10) {
		if (++nstray > 9)
			panic("crazy interrupts");
	} else {
		straytime = time.tv_sec;
		nstray = 1;
	}
}

static void
netintr()
{
	int n, s;

	s = splhigh();
	n = netisr;
	netisr = 0;
	splx(s);

#define	DONETISR(bit, fn)						\
	do {								\
		if (n & (1 << (bit)))					\
			fn;						\
	} while (0)

#ifdef INET
	DONETISR(NETISR_ARP, arpintr());
	DONETISR(NETISR_IP, ipintr());
#endif
#ifdef INET6
	DONETISR(NETISR_IPV6, ip6intr());
#endif
#ifdef NS
	DONETISR(NETISR_NS, nsintr());
#endif
#ifdef ISO
	DONETISR(NETISR_ISO, clnlintr());
#endif
#ifdef CCITT
	DONETISR(NETISR_CCITT, ccittintr());
#endif
#ifdef NATM
	DONETISR(NETISR_NATM, natmintr());
#endif
#if NPPP > 1
	DONETISR(NETISR_PPP, pppintr());
#endif
#if NBRIDGE > 1
	DONETISR(NETISR_BRIDGE, bridgeintr());
#endif

#undef DONETISR
}

static void
am7990_lesoft (void)
{
  if (intrhand[135])
    {
      int s;
      s = splnet ();
      (*intrhand[135]->ih_fun)(intrhand[135]->ih_arg);
      splx (s);
    }
}

void
do_sir ()
{
	unsigned int n;

	do {
		(void)splhigh();
		n = ssir;
		ssir = 0;
		splsoftint();		/* don't recurse through spl0() */
	
#define	DO_SIR(bit, fn)							\
		do {							\
			if (n & (bit)) {				\
				cnt.v_soft++;				\
				fn;					\
			}						\
		} while (0)

		DO_SIR(SIR_NET, netintr());
		DO_SIR(SIR_CLOCK, softclock());
		DO_SIR(SIR_ZS, zssoft());
		DO_SIR(SIR_LE, am7990_lesoft());

#undef DO_SIR
	} while (ssir != 0);
}

/* Handle delayed interruption from higher level to LEVEL.  */
static void
do_hir (void)
{
  int i;
  int done;
  struct intrhand *ih;
  
  setpil15 ();
  hir = 0;
  setpil0 ();

#if 0
  if (intrdelay[135] || intrdelay[137] != 0)
    printf ("do_hir: net is set\n");
#endif

  for (i = delay_high; i >= delay_low; i--)
    while (intrdelay[i])
      {
	done = 0;
	setpil15();
	intrdelay[i] = 0;
	setpil0();

	/* Run at the correct spl.  */
	stba (ASI_DGRAM, 0, i);

	/* Call the handlers.  */
	for (ih = intrhand[i]; ih; ih = ih->ih_next)
	  done += (*ih->ih_fun)(ih->ih_arg) ? 1 : 0;
	if (!done)
	  printf ("stray delayed interruption %d not acked\n", i);
      }
}
	

/*
 * Level 15 interrupts are special, and not vectored here.
 * Only `prewired' interrupts appear here; boot-time configured devices
 * are attached via intr_establish() below.
 */
struct intrhand *intrhand[256];

/*
 * Attach an interrupt handler to the vector chain for the given level.
 * This is not possible if it has been taken away as a fast vector.
 */
void
intr_establish(level, flags, ih)
	int level;
	unsigned int flags;
	struct intrhand *ih;
{
	register struct intrhand **p, *q;
	int s;

	s = splhigh();

	ih->ih_flags = flags;
#if 1
	printf ("intr establish level %d%s\n",
		level,
		(flags & IH_CAN_DELAY) ? " - can delay" : "");
#endif

	/*
	 * This is O(N^2) for long chains, but chains are never long
	 * and we do want to preserve order.
	 */
	for (p = &intrhand[level]; (q = *p) != NULL; p = &q->ih_next)
		continue;
	*p = ih;
	if (flags & IH_CAN_DELAY)
	  {
	    if (delay_low == 0 && delay_high == 0)
	      delay_low = delay_high = level;
	    else
	      {
		if (level < delay_low)
		  delay_low = level;
		if (level > delay_high)
		  delay_high = level;
	      }
	  }
	ih->ih_next = NULL;
	splx(s);
}

#if 0
void
sendint (int boardid, int level)
{
  if (lduba (ASI_ITXC, 0) & SIC_ITXC_E)
    return;
  sta (ASI_IXR, 0, ((level & 0xff) << 8) | SIC_IXR_DIR | (boardid & 0x0f));
  stba (ASI_ITXC, 0, SIC_ITXC_E);
}
#endif

/* System board interrupt register.  */
static volatile char *sbir;

void
sic_init ()
{
  unsigned char irc_status;
  unsigned char id;
  unsigned char v;

  /* Map the system board interrupt register (SBIR).  */
  sbir = (volatile char *)bus_mapin (BUS_KBUS, SBIR_BASE, SBIR_SIZE);
  if (!sbir)
    panic ("Can't map SBIR");

  setpil15 ();

  /* Disable the receiver.  */
  irc_status = lduba (ASI_IRXC, 0);
  while (irc_status & SIC_IRC_E)
    {
      stba (ASI_IRXC, 0, irc_status & ~SIC_IRC_E);
      irc_status = lduba (ASI_IRXC, 0);
    }

  /* Enable all interruptions.  */
  sta (ASI_IPR, 0, 0);
  
  /* Set device id.  */
  id = lda (ASI_BID, 0) & 0x0f;
  stba (ASI_DIR, 0, id);

  /* Ack int.  */
  lda (ASI_ACK_IPV, 0);

  /* Enable the receiver.  */
  stba (ASI_IRXC, 0, irc_status | SIC_IRC_E);

  spl0();
  setpil0();

  *sbir = id | SBIR_DI;

  /* Enable interruptions from the system board.  */
  v = sbir[SBI_EN_OFF];

  printf ("Interrupts enabled.\n");
}

#if 1
/* Set the new spl and return the old one.  */
#define SPL_BODY(new_spl, can_lower)			\
{							\
   int oldspl;						\
							\
   /* Get old spl.  */					\
   oldspl = lduba (ASI_DGRAM, 0);			\
							\
   /* If CAN_LOWER is false, prevent from lowering.  */	\
   if (!can_lower && new_spl < oldspl)			\
     return oldspl;					\
							\
   /* Do hard int.  */					\
   if (new_spl <= SPL_SOFTCLOCK && hir)			\
     do_hir ();						\
							\
   /* Do ast.  */					\
   if (new_spl == 0 && ssir)				\
     do_sir ();						\
							\
   stba (ASI_DGRAM, 0, new_spl);			\
							\
   return oldspl;					\
}
#else
/* Set the new spl and return the old one.  */
#define SPL_BODY(new_spl)			\
{						\
   int oldspl, oldpsr, i;			\
   						\
   /* Get old spl.  */				\
   oldspl = lda (ASI_IPR, 0) & 0xff;		\
						\
   /* Do ast.  */				\
   if (new_spl == 0 && ssir)			\
     do_sir ();					\
						\
   /* Mask all interrupts.  */			\
   oldpsr = getpsr ();				\
   setpsr (oldpsr | PSR_PIL);			\
        					\
   /* Disable IRC.  */				\
   stba (ASI_IRXC, 0, 0);			\
						\
   /* Wait for being disabled.  */		\
   for (i = 0xfff; i; i--)			\
     {						\
       if (!(lduba (ASI_IRXC, 0) & SIC_IRC_E))	\
	 goto done;				\
     }						\
   panic ("Cannot disable IRXC");		\
						\
 done:						\
   sta (ASI_IPR, 0, new_spl);			\
   if (!(lduba (ASI_IRXC, 0) & SIC_IRC_P))	\
     {						\
       /* Enable IRXC. */			\
       stba (ASI_IRXC, 0, 1);			\
     }						\
   setpsr (oldpsr);				\
						\
   return oldspl;				\
}
#endif

/*
 * PIL 1 through 14 can use this macro.
 * (spl0 and splhigh are special since they put all 0s or all 1s
 * into the ipl field.)
 */
#undef SPL
#define	SPL(name, newspl) \
int name __P((void)); \
int name() \
SPL_BODY(newspl, 0)

SPL(splsoftint, SPL_NET)

/* Block devices */
SPL(splbio, SPL_BIO)

/* network hardware interrupts are at level 6 */
SPL(splnet, SPL_NET)

/* tty input runs at software level 6 */
SPL(spltty, SPL_TTY)

/*
 * Memory allocation (must be as high as highest network, tty, or disk device)
 */
SPL(splimp, SPL_IMP)

SPL(splclock, SPL_CLOCK)
SPL(splstatclock, SPL_CLOCK)

/* zs hardware interrupts are at level 12 */
SPL(splzs, SPL_ZS)

SPL(splhigh, SPL_HIGH)

int
spl0 (void)
{
  SPL_BODY(0, 1)
}

int splx (int newspl)
{
  SPL_BODY(newspl, 1)
}

#if 0
void
disp_spl (void)
{
  printf ("ipr = %d, hir = %d, intrdelay[135] = %d\n",
	  lduba (ASI_DGRAM, 0), hir, intrdelay[135]);
/*  printf ("spl = %d\n", (getpsr() & PSR_PIL) >> 8); */
}
#endif
