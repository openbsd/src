/*	$NetBSD: vm86.c,v 1.3 1996/01/08 22:23:35 mycroft Exp $	*/

/*
 *  Copyright (c) 1995 John T. Kohl
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/device.h>
#include <sys/sysctl.h>
#include <sys/syscallargs.h>
#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#include <sys/ktrace.h>
#include <machine/sysarch.h>
#include <machine/vm86.h>

static void return_to_32bit __P((struct proc *, int));
static void fast_intxx __P((struct proc *, int));

#define	SETDIRECT	((~(PSL_USERSTATIC|PSL_NT)) & 0xffff)
#define	GETDIRECT	(SETDIRECT|0x02a) /* add in two MBZ bits */

#define	IP(tf)		(*(u_short *)&tf->tf_eip)
#define	SP(tf)		(*(u_short *)&tf->tf_esp)


#define putword(base, ptr, val) \
__asm__ __volatile__( \
	"decw %w0\n\t" \
	"movb %h2,0(%1,%0)\n\t" \
	"decw %w0\n\t" \
	"movb %b2,0(%1,%0)" \
	: "=r" (ptr) \
	: "r" (base), "q" (val), "0" (ptr))

#define putdword(base, ptr, val) \
__asm__ __volatile__( \
	"rorl $16,%2\n\t" \
	"decw %w0\n\t" \
	"movb %h2,0(%1,%0)\n\t" \
	"decw %w0\n\t" \
	"movb %b2,0(%1,%0)\n\t" \
	"rorl $16,%2\n\t" \
	"decw %w0\n\t" \
	"movb %h2,0(%1,%0)\n\t" \
	"decw %w0\n\t" \
	"movb %b2,0(%1,%0)" \
	: "=r" (ptr) \
	: "r" (base), "q" (val), "0" (ptr))

#define getbyte(base, ptr) \
({ unsigned long __res; \
__asm__ __volatile__( \
	"movb 0(%1,%0),%b2\n\t" \
	"incw %w0" \
	: "=r" (ptr), "=r" (base), "=q" (__res) \
	: "0" (ptr), "1" (base), "2" (0)); \
__res; })

#define getword(base, ptr) \
({ unsigned long __res; \
__asm__ __volatile__( \
	"movb 0(%1,%0),%b2\n\t" \
	"incw %w0\n\t" \
	"movb 0(%1,%0),%h2\n\t" \
	"incw %w0" \
	: "=r" (ptr), "=r" (base), "=q" (__res) \
	: "0" (ptr), "1" (base), "2" (0)); \
__res; })

#define getdword(base, ptr) \
({ unsigned long __res; \
__asm__ __volatile__( \
	"movb 0(%1,%0),%b2\n\t" \
	"incw %w0\n\t" \
	"movb 0(%1,%0),%h2\n\t" \
	"incw %w0\n\t" \
	"rorl $16,%2\n\t" \
	"movb 0(%1,%0),%b2\n\t" \
	"incw %w0\n\t" \
	"movb 0(%1,%0),%h2\n\t" \
	"incw %w0\n\t" \
	"rorl $16,%2" \
	: "=r" (ptr), "=r" (base), "=q" (__res) \
	: "0" (ptr), "1" (base)); \
__res; })


static __inline__ int
is_bitset(nr, bitmap)
	int nr;
	caddr_t bitmap;
{
	u_int byte;		/* bt instruction doesn't do
					   bytes--it examines ints! */
	bitmap += nr / NBBY;
	nr = nr % NBBY;
	byte = fubyte(bitmap);

	__asm__ __volatile__("btl %2,%1\n\tsbbl %0,%0"
			     :"=r" (nr)
			     :"r" (byte),"r" (nr));
	return (nr);
}


static __inline__ void
set_vif(p)
	struct proc *p;
{

	VM86_EFLAGS(p) |= PSL_VIF;
	if (VM86_EFLAGS(p) & PSL_VIP)
		return_to_32bit(p, VM86_STI);
}

static __inline__ void
set_vflags(p, flags)
	struct proc *p;
	int flags;
{
	struct trapframe *tf = p->p_md.md_regs;

	SETFLAGS(VM86_EFLAGS(p), flags, VM86_FLAGMASK(p));
	SETFLAGS(tf->tf_eflags, flags, SETDIRECT);
	if (flags & PSL_I)
		set_vif(p);
}

static __inline__ void
set_vflags_short(p, flags)
	struct proc *p;
	int flags;
{
	struct trapframe *tf = p->p_md.md_regs;

	SETFLAGS(VM86_EFLAGS(p), flags, VM86_FLAGMASK(p) & 0xffff);
	SETFLAGS(tf->tf_eflags, flags, SETDIRECT);
	if (flags & PSL_I)
		set_vif(p);
}

static __inline__ int
get_vflags(p)
	struct proc *p;
{
	struct trapframe *tf = p->p_md.md_regs;
	int flags = 0;

	SETFLAGS(flags, VM86_EFLAGS(p), VM86_FLAGMASK(p));
	SETFLAGS(flags, tf->tf_eflags, GETDIRECT);
	if (VM86_EFLAGS(p) & PSL_VIF)
		flags |= PSL_I;
	return (flags);
}


#define V86_AH(regs)	(((u_char *)&((regs)->tf_eax))[1])
#define V86_AL(regs)	(((u_char *)&((regs)->tf_eax))[0])

static void
fast_intxx(p, intrno)
	struct proc *p;
	int intrno;
{
	struct trapframe *tf = p->p_md.md_regs;
	/*
	 * handle certain interrupts directly by pushing the interrupt
	 * frame and resetting registers, but only if user said that's ok
	 * (i.e. not revectored.)  Otherwise bump to 32-bit user handler.
	 */
	struct vm86_struct *u_vm86p;
	struct { u_short ip, cs; } ihand;

	u_short cs;
	u_long ss, sp;

	/* 
	 * Note: u_vm86p points to user-space, we only compute offsets
	 * and don't deref it. is_revectored() above does fubyte() to
	 * get stuff from it
	 */
	u_vm86p = (struct vm86_struct *)p->p_addr->u_pcb.vm86_userp;

	/* 
	 * If coming from BIOS segment, or going to BIOS segment, or user
	 * requested special handling, return to user space with indication
	 * of which INT was requested.
	 */
	cs = tf->tf_cs;
	if (cs == BIOSSEG || is_bitset(intrno, &u_vm86p->int_byuser[0]))
		goto vector;

	/*
	 * If it's interrupt 0x21 (special in the DOS world) and the
	 * sub-command (in AH) was requested for special handling,
	 * return to user mode.
	 */
	if (intrno == 0x21 && is_bitset(V86_AH(tf), &u_vm86p->int21_byuser[0]))
		goto vector;

	/*
	 * Fetch intr handler info from "real-mode" IDT based at addr 0 in
	 * the user address space.
	 */
	if (copyin((caddr_t)(intrno * sizeof(ihand)), &ihand, sizeof(ihand)))
		goto bad;

	if (ihand.cs == BIOSSEG)
		goto vector;

	/*
	 * Otherwise, push flags, cs, eip, and jump to handler to
	 * simulate direct INT call.
	 */
	ss = tf->tf_ss << 4;
	sp = SP(tf);

	putword(ss, sp, get_vflags(p));
	putword(ss, sp, tf->tf_cs);
	putword(ss, sp, IP(tf));
	SP(tf) = sp;

	IP(tf) = ihand.ip;
	tf->tf_cs = ihand.cs;

	/* disable further "hardware" interrupts, turn off any tracing. */
	VM86_EFLAGS(p) &= ~PSL_VIF;
	tf->tf_eflags &= ~PSL_VIF|PSL_T;
	return;

vector:
	return_to_32bit(p, VM86_MAKEVAL(VM86_INTx, intrno));
	return;

bad:
	return_to_32bit(p, VM86_UNKNOWN);
	return;
}

static void
return_to_32bit(p, retval)
	struct proc *p;
	int retval;
{

	/*
	 * We can't set the virtual flags in our real trap frame,
	 * since it's used to jump to the signal handler.  Instead we
	 * let sendsig() pull in the VM86_EFLAGS bits.
	 */
	if (p->p_sigmask & sigmask(SIGURG)) {
#ifdef DIAGNOSTIC
		printf("pid %d killed on VM86 protocol screwup (SIGURG blocked)\n",
		       p->p_pid);
#endif
		sigexit(p, SIGILL);
		/* NOTREACHED */
	}
	trapsignal(p, SIGURG, retval);
}

#define	CLI	0xFA
#define	STI	0xFB
#define	INTxx	0xCD
#define	IRET	0xCF
#define	OPSIZ	0x66
#define	INT3	0xCC	/* Actually the process gets 32-bit IDT to handle it */
#define	LOCK	0xF0
#define	PUSHF	0x9C
#define	POPF	0x9D

/*
 * Handle a GP fault that occurred while in VM86 mode.  Things that are easy
 * to handle here are done here (much more efficient than trapping to 32-bit
 * handler code and then having it restart VM86 mode).
 */
void
vm86_gpfault(p, type)
	struct proc *p;
	int type;
{
	struct trapframe *tf = p->p_md.md_regs;
	/*
	 * we want to fetch some stuff from the current user virtual
	 * address space for checking.  remember that the frame's
	 * segment selectors are real-mode style selectors.
	 */
	u_char tmpbyte;
	u_long cs, ip, ss, sp;

	cs = tf->tf_cs << 4;
	ip = IP(tf);
	ss = tf->tf_ss << 4;
	sp = SP(tf);

	/*
	 * For most of these, we must set all the registers before calling
	 * macros/functions which might do a return_to_32bit.
	 */
	tmpbyte = getbyte(cs, ip);
	IP(tf) = ip;
	switch (tmpbyte) {
	case CLI:
		/* simulate handling of IF */
		VM86_EFLAGS(p) &= ~PSL_VIF;
		tf->tf_eflags &= ~PSL_VIF;
		break;

	case STI:
		/* simulate handling of IF.
		 * XXX the i386 enables interrupts one instruction later.
		 * code here is wrong, but much simpler than doing it Right.
		 */
		set_vif(p);
		break;

	case INTxx:
		/* try fast intxx, or return to 32bit mode to handle it. */
		tmpbyte = getbyte(cs, ip);
		IP(tf) = ip;
		fast_intxx(p, tmpbyte);
		break;

	case PUSHF:
		putword(ss, sp, get_vflags(p));
		SP(tf) = sp;
		break;

	case IRET:
		IP(tf) = getword(ss, sp);
		tf->tf_cs = getword(ss, sp);
	case POPF:
		set_vflags_short(p, getword(ss, sp));
		SP(tf) = sp;
		break;

	case OPSIZ:
		tmpbyte = getbyte(cs, ip);
		IP(tf) = ip;
		switch (tmpbyte) {
		case PUSHF:
			putdword(ss, sp, get_vflags(p));
			SP(tf) = sp;
			break;

		case IRET:
			IP(tf) = getdword(ss, sp);
			tf->tf_cs = getdword(ss, sp);
		case POPF:
			set_vflags(p, getdword(ss, sp));
			SP(tf) = sp;
			break;

		default:
			IP(tf) -= 2;
			goto bad;
		}
		break;

	case LOCK:
	default:
		IP(tf) -= 1;
		goto bad;
	}
	return;

bad:
	return_to_32bit(p, VM86_UNKNOWN);
	return;
}

int
i386_vm86(p, args, retval)
	struct proc *p;
	char *args;
	register_t *retval;
{
	struct trapframe *tf = p->p_md.md_regs;
	struct vm86_kern vm86s;
	int err;

	if (err = copyin(args, &vm86s, sizeof(vm86s)))
		return err;

	p->p_addr->u_pcb.vm86_userp = (void *)args;

#define DOVREG(reg) tf->tf_vm86_##reg = (u_short) vm86s.regs.vmsc.sc_##reg
#define DOREG(reg) tf->tf_##reg = (u_short) vm86s.regs.vmsc.sc_##reg

	DOVREG(ds);
	DOVREG(es);
	DOVREG(fs);
	DOVREG(gs);
	DOREG(edi);
	DOREG(esi);
	DOREG(ebp);
	DOREG(eax);
	DOREG(ebx);
	DOREG(ecx);
	DOREG(edx);
	DOREG(eip);
	DOREG(cs);
	DOREG(esp);
	DOREG(ss);

#undef	DOVREG
#undef	DOREG

	SETFLAGS(VM86_EFLAGS(p), vm86s.regs.vmsc.sc_eflags, VM86_FLAGMASK(p)|PSL_VIF);
	SETFLAGS(tf->tf_eflags, vm86s.regs.vmsc.sc_eflags, SETDIRECT);
	tf->tf_eflags |= PSL_VM;

	/*
	 * Keep mask of flags we simulate to simulate a particular type of
	 * processor.
	 */
	switch (vm86s.ss_cpu_type) {
	case VCPU_086:
	case VCPU_186:
	case VCPU_286:
		VM86_FLAGMASK(p) = 0;
		break;
	case VCPU_386:
		VM86_FLAGMASK(p) = PSL_NT|PSL_IOPL;
		break;
	case VCPU_486:
		VM86_FLAGMASK(p) = PSL_AC|PSL_NT|PSL_IOPL;
		break;
	case VCPU_586:
	default:
		VM86_FLAGMASK(p) = PSL_ID|PSL_AC|PSL_NT|PSL_IOPL;
		break;
	}

	/* Going into vm86 mode jumps off the signal stack. */
	p->p_sigacts->ps_sigstk.ss_flags &= ~SA_ONSTACK;

	return (EJUSTRETURN);
}
