/*	$NetBSD: kgdb_stub.c,v 1.7 1996/12/17 21:11:30 gwr Exp $	*/

/*
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratories.
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
 *	@(#)kgdb_stub.c	8.4 (Berkeley) 1/12/94
 */

/*
 * "Stub" to allow remote cpu to debug over a serial line using gdb.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>

#include <machine/control.h>
#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/psl.h>
#include <machine/pte.h>
#include <machine/reg.h>
#include <machine/trap.h>

#include <sun3/sun3/kgdb_proto.h>
#include <machine/db_machdep.h>
#include <machine/remote-sl.h>

#ifndef KGDBDEV
#define KGDBDEV -1
#endif
#ifndef KGDBRATE
#define KGDBRATE 19200
#endif

int kgdb_dev = KGDBDEV;		/* remote debugging device (-1 if none) */
int kgdb_rate = KGDBRATE;	/* remote debugging baud rate */
int kgdb_active = 0;		/* remote debugging active if != 0 */
int kgdb_debug_init = 0;	/* != 0 waits for remote at system init */
int kgdb_debug_panic = 0;	/* != 0 waits for remote on panic */

static void kgdb_send __P((u_int, u_char *, int));
static int kgdb_recv __P((u_char *, int *));
static int computeSignal __P((int));
int kgdb_trap __P((int, struct trapframe *));
int kgdb_acc __P((caddr_t, int));

static int (*kgdb_getc) __P((void *));
static void (*kgdb_putc) __P((void *, int));
static void *kgdb_ioarg;

#define GETC()	((*kgdb_getc)(kgdb_ioarg))
#define PUTC(c)	((*kgdb_putc)(kgdb_ioarg, c))
#define PUTESC(c) do { \
	if (c == FRAME_END) { \
		PUTC(FRAME_ESCAPE); \
		c = TRANS_FRAME_END; \
	} else if (c == FRAME_ESCAPE) { \
		PUTC(FRAME_ESCAPE); \
		c = TRANS_FRAME_ESCAPE; \
	} else if (c == FRAME_START) { \
		PUTC(FRAME_ESCAPE); \
		c = TRANS_FRAME_START; \
	} \
	PUTC(c); \
} while (0)

/*
 * This is called by the approprite tty driver.
 * In our case, by dev/zs_kgdb.c:zs_kgdb_init()
 */
void
kgdb_attach(getfn, putfn, ioarg)
	int (*getfn) __P((void *));
	void (*putfn) __P((void *, int));
	void *ioarg;
{

	kgdb_getc = getfn;
	kgdb_putc = putfn;
	kgdb_ioarg = ioarg;
}

/*
 * Send a message.  The host gets one chance to read it.
 */
static void
kgdb_send(type, bp, len)
	register u_int type;
	register u_char *bp;
	register int len;
{
	register u_char csum;
	register u_char *ep = bp + len;

	PUTC(FRAME_START);
	PUTESC(type);

	csum = type;
	while (bp < ep) {
		type = *bp++;
		csum += type;
		PUTESC(type);
	}
	csum = -csum;
	PUTESC(csum);
	PUTC(FRAME_END);
}

static int
kgdb_recv(bp, lenp)
	u_char *bp;
	int *lenp;
{
	register u_char c, csum;
	register int escape, len;
	register int type;

restart:
	csum = len = escape = 0;
	type = -1;
	while (1) {
		c = GETC();
		switch (c) {

		case FRAME_ESCAPE:
			escape = 1;
			continue;

		case TRANS_FRAME_ESCAPE:
			if (escape)
				c = FRAME_ESCAPE;
			break;

		case TRANS_FRAME_END:
			if (escape)
				c = FRAME_END;
			break;

		case TRANS_FRAME_START:
			if (escape)
				c = FRAME_START;
			break;

		case FRAME_START:
			goto restart;

		case FRAME_END:
			if (type < 0 || --len < 0) {
				csum = len = escape = 0;
				type = -1;
				continue;
			}
			if (csum != 0)
				return (0);
			*lenp = len;
			return (type);
		}
		csum += c;
		if (type < 0) {
			type = c;
			escape = 0;
			continue;
		}
		if (++len > SL_RPCSIZE) {
			while (GETC() != FRAME_END)
				continue;
			return (0);
		}
		*bp++ = c;
		escape = 0;
	}
}

/*
 * Translate a trap number into a unix compatible signal value.
 * (gdb only understands unix signal numbers).
 */
static int
computeSignal(type)
	int type;
{
	int sigval;

	switch (type) {
	case T_BUSERR:
		sigval = SIGBUS;
		break;
	case T_ADDRERR:
		sigval = SIGBUS;
		break;
	case T_ILLINST:
		sigval = SIGILL;
		break;
	case T_ZERODIV:
		sigval = SIGFPE;
		break;
	case T_CHKINST:
		sigval = SIGFPE;
		break;
	case T_TRAPVINST:
		sigval = SIGFPE;
		break;
	case T_PRIVINST:
		sigval = SIGILL;
		break;
	case T_TRACE:
		sigval = SIGTRAP;
		break;
	case T_MMUFLT:
		sigval = SIGSEGV;
		break;
	case T_SSIR:
		sigval = SIGSEGV;
		break;
	case T_FMTERR:
		sigval = SIGILL;
		break;
	case T_FPERR:
		sigval = SIGFPE;
		break;
	case T_COPERR:
		sigval = SIGFPE;
		break;
	case T_ASTFLT:
		sigval = SIGINT;
		break;
	case T_TRAP15:
		sigval = SIGTRAP;
		break;
	default:
		sigval = SIGEMT;
		break;
	}
	return (sigval);
}

/*
 * Trap into kgdb to wait for debugger to connect,
 * noting on the console why nothing else is going on.
 */
void
kgdb_connect(verbose)
	int verbose;
{

	if (kgdb_dev < 0 || kgdb_getc == NULL)
		return;
	fb_unblank();
	if (verbose)
		printf("kgdb waiting...");
	Debugger(); 	/* XXX: trap into kgdb */
	if (verbose)
		printf("connected.\n");
}

/*
 * Decide what to do on panic.
 * (This is called by panic, like Debugger())
 */
void
kgdb_panic()
{

	if (kgdb_dev >= 0 && kgdb_getc != NULL &&
	    kgdb_active == 0 && kgdb_debug_panic)
	{
		/* XXX: Just call Debugger() instead? */
		kgdb_connect(1);
	}
}

/*
 * Definitions exported from gdb.
 */
#define NUM_REGS 18
#define REGISTER_BYTES		(NUM_REGS * 4)
#define REGISTER_BYTE(n)	((n) * 4)

#define GDB_SR 16
#define GDB_PC 17

/*
 * This little routine exists simply so that bcopy() can be debugged.
 */
static void
kgdb_copy(vsrc, vdst, len)
	void *vsrc, *vdst;
	register int len;
{
	register char *src = vsrc;
	register char *dst = vdst;

	while (--len >= 0)
		*dst++ = *src++;
}

/* ditto for bzero */
static void
kgdb_zero(vptr, len)
	void *vptr;
	register int len;
{
	register char *ptr = vptr;

	while (--len >= 0)
		*ptr++ = (char) 0;
}

/*
 * Translate the values stored in the kernel regs struct to the format
 * understood by gdb.
 *
 * There is a short pad word between SP (A7) and SR which keeps the
 * kernel stack long word aligned (note that this is in addition to
 * the stack adjust short that we treat as the upper half of a longword
 * SR).  We must skip this when copying into and out of gdb.
 */
static void
regs_to_gdb(tf, gdb_regs)
	struct trapframe *tf;
	u_long *gdb_regs;
{
	kgdb_copy(tf->tf_regs, gdb_regs, 16*4);
	gdb_regs[GDB_SR] = tf->tf_sr;
	gdb_regs[GDB_PC] = tf->tf_pc;
}

/*
 * Reverse the above.
 */
static void
gdb_to_regs(tf, gdb_regs)
	struct trapframe *tf;
	u_long *gdb_regs;
{
	kgdb_copy(gdb_regs, tf->tf_regs, 16*4);
	tf->tf_sr = gdb_regs[GDB_SR];
	tf->tf_pc = gdb_regs[GDB_PC];
}

static u_long reg_cache[NUM_REGS];
static u_char inbuffer[SL_RPCSIZE];
static u_char outbuffer[SL_RPCSIZE];

/*
 * This function does all command procesing for interfacing to
 * a remote gdb.
 */
int
kgdb_trap(type, tf)
	int type;
	struct trapframe *tf;
{
	register u_long len;
	caddr_t addr;
	register u_char *cp;
	register u_char out, in;
	register int outlen;
	int inlen;
	u_long gdb_regs[NUM_REGS];

	if (kgdb_dev < 0 || kgdb_getc == NULL) {
		/* not debugging */
		return (0);
	}
	if (kgdb_active == 0) {
		if (type != T_BREAKPOINT) {
			/* No debugger active -- let trap handle this. */
			return (0);
		}

		/*
		 * If the packet that woke us up isn't an exec packet,
		 * ignore it since there is no active debugger.  Also,
		 * we check that it's not an ack to be sure that the
		 * remote side doesn't send back a response after the
		 * local gdb has exited.  Otherwise, the local host
		 * could trap into gdb if it's running a gdb kernel too.
		 */
		in = GETC();
		/*
		 * If we came in asynchronously through the serial line,
		 * the framing character is eaten by the receive interrupt,
		 * but if we come in through a synchronous trap (i.e., via
		 * kgdb_connect()), we will see the extra character.
		 */
		if (in == FRAME_START)
			in = GETC();

		/*
		 * Check that this is a debugger exec message.  If so,
		 * slurp up the entire message then ack it, and fall
		 * through to the recv loop.
		 */
		if (KGDB_CMD(in) != KGDB_EXEC || (in & KGDB_ACK) != 0)
			return (0);
		while (GETC() != FRAME_END)
			continue;
		/*
		 * Do the printf *before* we ack the message.  This way
		 * we won't drop any inbound characters while we're
		 * doing the polling printf.
		 */
		printf("kgdb started from device %x\n", kgdb_dev);
		kgdb_send(in | KGDB_ACK, (u_char *)0, 0);
		kgdb_active = 1;
	}
	/*
	 * Stick frame regs into our reg cache then tell remote host
	 * that an exception has occured.
	 */
	regs_to_gdb(tf, gdb_regs);
	if (type != T_BREAKPOINT) {
		/*
		 * XXX - Which is right?  The code or the comment? -gwr
		 * Only send an asynchronous SIGNAL message when we hit
		 * a breakpoint.  Otherwise, we will drop the incoming
		 * packet while we output this one (and on entry the other
		 * side isn't interested in the SIGNAL type -- if it is,
		 * it will have used a signal packet.)
		 */
		outbuffer[0] = computeSignal(type);
		kgdb_send(KGDB_SIGNAL, outbuffer, 1);
	}

	for (;;) {
		in = kgdb_recv(inbuffer, &inlen);
		if (in == 0 || (in & KGDB_ACK))
			/* Ignore inbound acks and error conditions. */
			continue;

		out = in | KGDB_ACK;
		switch (KGDB_CMD(in)) {

		case KGDB_SIGNAL:
			/*
			 * if this command came from a running gdb,
			 * answer it -- the other guy has no way of
			 * knowing if we're in or out of this loop
			 * when he issues a "remote-signal".  (Note
			 * that without the length check, we could
			 * loop here forever if the output line is
			 * looped back or the remote host is echoing.)
			 */
			if (inlen == 0) {
				outbuffer[0] = computeSignal(type);
				kgdb_send(KGDB_SIGNAL, outbuffer, 1);
			}
			continue;

		case KGDB_REG_R:
		case KGDB_REG_R | KGDB_DELTA:
			cp = outbuffer;
			outlen = 0;
			for (len = inbuffer[0]; len < NUM_REGS; ++len) {
				if (reg_cache[len] != gdb_regs[len] ||
				    (in & KGDB_DELTA) == 0) {
					if (outlen + 5 > SL_MAXDATA) {
						out |= KGDB_MORE;
						break;
					}
					cp[outlen] = len;
					kgdb_copy(&gdb_regs[len],
					          &cp[outlen + 1], 4);
					reg_cache[len] = gdb_regs[len];
					outlen += 5;
				}
			}
			break;

		case KGDB_REG_W:
		case KGDB_REG_W | KGDB_DELTA:
			cp = inbuffer;
			for (len = 0; len < inlen; len += 5) {
				register int j = cp[len];

				kgdb_copy(&cp[len + 1],
				          &gdb_regs[j], 4);
				reg_cache[j] = gdb_regs[j];
			}
			gdb_to_regs(tf, gdb_regs);
			outlen = 0;
			break;

		case KGDB_MEM_R:
			len = inbuffer[0];
			kgdb_copy(&inbuffer[1], &addr, 4);
			if (len > SL_MAXDATA) {
				outlen = 1;
				outbuffer[0] = E2BIG;
			} else if (!kgdb_acc(addr, len)) {
				outlen = 1;
				outbuffer[0] = EFAULT;
			} else {
				outlen = len + 1;
				outbuffer[0] = 0;
				db_read_bytes((vm_offset_t)addr, (size_t)len,
				              (char *)&outbuffer[1]);
			}
			break;

		case KGDB_MEM_W:
			len = inlen - 4;
			kgdb_copy(inbuffer, &addr, 4);
			outlen = 1;
			if (!kgdb_acc(addr, len))
				outbuffer[0] = EFAULT;
			else {
				outbuffer[0] = 0;
				db_write_bytes((vm_offset_t)addr, (size_t)len,
				               (char *)&inbuffer[4]);
			}
			break;

		case KGDB_KILL:
			kgdb_active = 0;
			printf("kgdb detached\n");
			/* FALLTHROUGH */

		case KGDB_CONT:
			kgdb_send(out, 0, 0);
			tf->tf_sr &=~ PSL_T;
			goto out;

		case KGDB_STEP:
			kgdb_send(out, 0, 0);
			tf->tf_sr |= PSL_T;
			goto out;

		case KGDB_HALT:
			kgdb_send(out, 0, 0);
			sun3_mon_halt();
			/* NOTREACHED */

		case KGDB_BOOT:
			kgdb_send(out, 0, 0);
			sun3_mon_reboot("");
			/* NOTREACHED */

		case KGDB_EXEC:
		default:
			/* Unknown command.  Ack with a null message. */
			outlen = 0;
			break;
		}
		/* Send the reply */
		kgdb_send(out, outbuffer, outlen);
	}
out:
	return (1);
}

/*
 * XXX: Determine if the memory at addr..(addr+len) is valid.
 * XXX: Should we just use the PTE bits?  Why not?
 * XXX: Better yet, setup a fault handler?
 */
kgdb_acc(addr, len)
	caddr_t addr;
	int len;
{
	vm_offset_t va;
	int pte, pgoff;

	va = (vm_offset_t) addr;
	pgoff = va & PGOFSET;
	va  -= pgoff;
	len += pgoff;

	do {
		pte = get_pte(va);
		if ((pte & PG_VALID) == 0)
			return (0);
		va  += NBPG;
		len -= NBPG;
	} while (len > 0);

	return (1);
}
