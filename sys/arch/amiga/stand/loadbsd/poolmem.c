/* $OpenBSD: poolmem.c,v 1.1 1998/08/13 21:08:16 espie Exp $ */

/* This piece of code ensures that poolmem no longer runs. It is necessary
 * to kill poolmem before assessing the memory configuration, as this patch
 * tampers with exec memory lists

 From: Thomas Richter <thor@math.TU-Berlin.DE>

The following is the "official" port structure of the PoolMem port. That's
what you get as result of a FindPort("PoolMem.rendezvous"):

struct PoolMemPort {
	struct MsgPort		pm_Port;
	UWORD			pm_Flags;          DO NOT CARE 
	ULONG			pm_DoNotTouch;
	void		      (*pm_RemoveProc)();  The important stuff 
};

Calling syntax is as follows:

	The remove procedure must be called with Exec in "Forbid" state,
	using register a0 as a pointer to the routine. Register a6 MUST be
	a pointer to the DosLibrary (purely for historical reasons).
	Register a5 MUST be a pointer to this port structure.
 */
 
#include <proto/exec.h>
#include <stdio.h>

extern struct DosLibrary *DOSBase;
void remove_poolmem(struct DosLibrary *db, struct MsgPort *mp);


void ensure_no_poolmem()
   {
   Forbid();
      {
      struct MsgPort *p = FindPort("PoolMem.rendezvous");
      if (p)
	 remove_poolmem(DOSBase, p);
		/* this will actually break the Forbid() */
	 puts("Poolmem detected (and removed)");
      }
   Permit();
   }

asm("
	.text
	.globl _remove_poolmem

_remove_poolmem:
        movem.l a0-a6/d0-d7,sp@-		| save all regs
	move.l sp@(64),a6			| DosBase
	move.l sp@(68),a5			| PoolMemPort
	move.l  a5@(40),a0		| Routine to call
	jsr  a0@
	movem.l sp@+,a0-a6/d0-d7		| restore all regs
	rts
");
