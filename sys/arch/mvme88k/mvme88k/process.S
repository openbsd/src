#ifndef ASSEMBLER /* predefined by ascpp, at least */
#define ASSEMBLER
#endif

#include "machine/locore.h"
#include "machine/asm.h"
#include "assym.s"

#ifndef NBPG
#define	NBPG 4096
#endif /* NBPG */

	data
	align 4
Lsw0:
	string	"cpu_switch\n"
	align 4
swchanpanic:
	string  "switch wchan\n"
	align 4
swsrunpanic:
	string	"switch SRUN\n"

	text
	align 8
Lswchanpanic:
	or.u r2, r0, hi16(swchanpanic)
	or   r2, r2, lo16(swchanpanic)
	bsr  _panic

Lswsrunpanic:
	or.u r2, r0, hi16(swsrunpanic)
	or   r2, r2, lo16(swsrunpanic)
	bsr  _panic
/*
 * At exit of a process, do a cpu_switch for the last time.
 * The mapping of the pcb at p->p_addr has already been deleted,
 * and the memory for the pcb+stack has been freed.
 * The ipl is high enough to prevent the memory from being reallocated.
 */
ENTRY(switch_exit)
	/*
	 * Change pcb to idle u. area, i.e., set r31 to top of stack
	 * and set curpcb to point to _idle_u.
	 */
	or.u	r31, r0, hi16(_idle_u)
	or	r31, r31,lo16(_idle_u)
	or.u	r10, r10,hi16(_curpcb)
	or	r10, r10,lo16(_curpcb)
	st	r31, r0, r10		/* curpcb = &idle_u */
	addu	r31, r31, UPAGES * NBPG	/* now on idle_u stack */
	or.u	r10, r0, hi16(_curproc)
	st	r0, r10, lo16(_curproc)	/* curproc = NULL */
	bsr.n	_cpu_switch
	or	r2, r0, r10

/*
 * When no processes are on the runq, switch
 * idles here watiing for something to come ready.
 */
LABEL(idle)
	or.u	r10, r0, hi16(_curproc)
	st	r0, r10, lo16(_curproc)	/* curproc = NULL */

	or	r2,r0,0
	bsr	_spln			/*(void) spl0(); */
					; spin reading _whichqs until nonzero
1:
	or.u	r10, r0, hi16(_whichqs)
	ld	r11, r10,lo16(_whichqs) 
	bcnd	eq0, r11, 1b
	bsr.n	_spln
	or	r2,r0,6
	br	Lsw1
/*
 * cpu_switch()
 * XXX - Arg 1 is a proc pointer (curproc) but this doesn't use it.
 * XXX - how about using stack for saving spl and last proc?
 */
ENTRY(cpu_switch)
	or.u	r10, r0, hi16(_curpcb)
	ld	r10,r10, lo16(_curpcb)
	st	r1, r10, 0		; save r1 in pcb
	bsr	_spl
	or.u	r10, r0, hi16(_curpcb)	; a call can clobber
	ld	r10,r10, lo16(_curpcb)  ; r10 - so reload it
	st	r2, r10, 19 * 4		; save ipl in pcb
	or.u	r11, r0, hi16(_curproc)
	ld	r11,r11, lo16(_curproc)
	or.u	r12, r0, hi16(_lastproc)
	or	r12, r12, lo16(_lastproc)
	st	r11, r12, 0		; lastproc = curproc
	or.u	r11, r0, hi16(_curproc)
	st	r0, r11, lo16(_curproc)	; curproc = NULL
	bsr.n	_spln
	or	r2,r0,6
Lsw1:
	/*
	 * Find the highest-priority queue that isn't empty,
	 * then take the first proc from that queue.
	 */
	or	r6, r0, r0
	or.u	r7, r0, hi16(_whichqs)
	ld	r7, r7, lo16(_whichqs)
Lswchk:
	bcnd	eq0, r7, idle
	ff1	r6, r7		; 0 <= r6 <= 31
	
	or.u	r7, r0, hi16(_qs)
	or	r7, r7, lo16(_qs)
	mak	r6, r6, 0<3>
	lda	r8, r7[r6]	; r8 = qs[ff1(whichqs)]
				; r8 is q, r9 is p
	ld	r9,  r8, P_FORW	; p = q->p_forw
	ld	r12, r9, P_FORW	; r12 is p->p_forw
	st	r12, r8, P_FORW ; q->p_forw = p->p_forw
	st	r12, r8, 0	; q = p->p_forw
	ld	r12, r9, P_BACK	; r12 is p->p_back
	st	r12, r8, P_BACK	; q->p_back = p->p_back
	lda	r8,  r7[r6]	; reload r8 with qs[ff1(whichqs)]
	ld	r12,  r8, P_FORW; q->p_forw
	cmp	r12,  r12, r8	; q == q->p_forw; anyone left on queue?
	bb1	ne,  r12, Lsw2	; no, skip
	ext	r6,  r6, 0<3>	
	add	r6,  r6, 1	; turn off the bit we looked at
	or.u	r7,  r0, hi16(_whichqs)
	ld	r8,  r7, lo16(_whichqs)
	and.c	r8,  r8, r6	; whichqs &= ~the bit
	st	r8,  r7, lo16(_whichqs)	; reset bit in whichqs
Lsw2:
	ld	r2, r9, P_WCHAN
	bcnd	ne0, r2, Lswchanpanic
	ld.b	r2, r9, P_STAT
	cmp	r2, r2, SRUN
	bb1	ne, r2, Lswsrunpanic

	or.u	r11, r0, hi16(_want_resched)
	st	r0, r11, lo16(_want_resched)

	or.u	r11, r0, hi16(_curproc)
	st	r9,  r11,lo16(_curproc)	; curproc = p
	
	or.u	r2, r0, hi16(_lastproc)
	ld	r2, r2, lo16(_lastproc)

	or.u	r10, r0, hi16(_curpcb)
	ld	r10,r10, lo16(_curpcb)

	cmp	r2, r2, r9
	bb1	eq, r2, Lswsameproc

	/*
	 * Save state of previous process in its pcb.
	 */

	; r1 and ipl already saved above
	st	r14,r10,4
	st	r15,r10,2*4
	st	r16,r10,3*4
	st	r17,r10,4*4
	st	r18,r10,5*4
	st	r19,r10,6*4
	st	r20,r10,7*4
	st	r21,r10,8*4
	st	r22,r10,9*4
	st	r23,r10,10*4
	st	r24,r10,11*4
	st	r25,r10,12*4
	st	r26,r10,13*4
	st	r27,r10,14*4
	st	r28,r10,15*4
	st	r29,r10,16*4
	st	r30,r10,17*4		/* save frame pointer */
	st	r31,r10,18*4		/* save stack pointer */
					/* ipl already saved  */
	; r9 is curproc
	or.u	r10, r0, hi16(_curpcb)
	or	r10,r10, lo16(_curpcb)
	st	r0, r9, P_BACK		; p->p_back = 0
	ld	r3, r9, P_ADDR
	st	r3, r10, 0		; curpcb = p->p_addr
	/* see if pmap_activate needs to be called */
	ld	r2, r9, P_VMSPACE	; vmspace = p->p_vmspace
	addu	r2, r2, VM_PMAP		; pmap = &vmspace.vm_pmap
#if 0
	ld	r5, r2, PM_STCHG	; pmap->st_changed?
	bcnd	eq0, r5, Lswnochg	; no, skip
#endif
	or	r14, r0, r9		; save p in r14
	bsr	_pmap_activate		; pmap_activate(pmap, pcb)
	or	r9, r0, r14		; restore p saved in r14

Lswnochg:
	or.u	r31, r0, hi16(_intstack_end)
	or	r31,r31, lo16(_intstack_end); now goto a tmp stack for NMI
	bsr.n    _load_u_area		; load_u_area(p)
	or	r2, r0, r9
	or.u	r10, r0,  hi16(_curpcb)
	ld	r10, r10,  lo16(_curpcb)
	; XXX Is this correct/necessary?
	st	r10, r14, P_ADDR	; p->p_addr = curpcb; restore p_addr
	; flush some data cache here
	
	; restore from the current context

	ld	r1,r10,0	
	ld	r14,r10,4
	ld	r15,r10,2*4
	ld	r16,r10,3*4
	ld	r17,r10,4*4
	ld	r18,r10,5*4
	ld	r19,r10,6*4
	ld	r20,r10,7*4
	ld	r21,r10,8*4
	ld	r22,r10,9*4
	ld	r23,r10,10*4
	ld	r24,r10,11*4
	ld	r25,r10,12*4
	ld	r26,r10,13*4
	ld	r27,r10,14*4
	ld	r28,r10,15*4
	ld	r29,r10,16*4
	ld	r30,r10,17*4		/* restore frame pointer */
	ld	r31,r10,18*4		/* restore stack pointer */
Lswsameproc:
	ld	r2, r10,19*4		/* restore interrupt mask */
	subu	r31,r31,40
	st	r1, r31,32		; save r1 on stack
	bsr	_spln
Lcxswdone:
	ld	r1, r31,32		; restore r1 from stack
	addu	r31,r31,40
	jmp.n	r1
	or	r2, r0, 1		 ; return 1 (for alternate returns)

/*
 * savectx(pcb)
 * Update pcb, saving current processor state.
 */
ENTRY(savectx)
	/* get the spl mask */
	subu	r31,r31,40		/* allocate stack for r1 and args */
	st	r1,r31,36		/* save return address */
	st	r2,r31,32		/* save r2 */
	bsr	_spl			/* get the current interrupt mask */
	ld	r1,r31,36		/* recover return address */
	ld	r10,r31,32		/* recover r2 into r10 */
	addu	r31,r31,40		/* put stack pointer back */
	st	r1,r10,0	/* do setjmp */ /* save return address */
	st	r14,r10,4
	st	r15,r10,2*4
	st	r16,r10,3*4
	st	r17,r10,4*4
	st	r18,r10,5*4
	st	r19,r10,6*4
	st	r20,r10,7*4
	st	r21,r10,8*4
	st	r22,r10,9*4
	st	r23,r10,10*4
	st	r24,r10,11*4
	st	r25,r10,12*4
	st	r26,r10,13*4
	st	r27,r10,14*4
	st	r28,r10,15*4
	st	r29,r10,16*4
	st	r30,r10,17*4		/* save frame pointer */
	st	r31,r10,18*4		/* save stack pointer */
	st	r2, r10,19*4		/* save interrupt mask */
	jmp.n	r1
	or	r2,r0,r0
