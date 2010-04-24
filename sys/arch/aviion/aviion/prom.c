/*	$OpenBSD: prom.c,v 1.4 2010/04/24 18:46:51 miod Exp $	*/

/*
 * Copyright (c) 2006, Miodrag Vallat.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
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
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/asm_macro.h>
#include <machine/prom.h>

register_t prom_vbr;					/* set in locore.S */

/*
 * No locking is necessary, since we will only use the SCM routines
 * during startup, before any secondary CPU is started.
 */

#define	SCM_DECL \
	register_t psr; \
	register_t ossr0, ossr1, ossr2, ossr3

#define	SCM_CALL(x) \
	__asm__ __volatile__ ("or r9, r0, " __STRING(x));		\
	__asm__ __volatile__ ("tb0 0, r0, 496" :::			\
	    "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8",		\
	    "r9", "r10", "r11", "r12", "r13")

#define	SCM_VBR() \
	__asm__ __volatile__ ("stcr %0, cr7" : : "r" (prom_vbr))

#define	SCM_CONTEXT() \
	__asm__ __volatile__ ("ldcr %0, cr17" : "=r" (ossr0)); \
	__asm__ __volatile__ ("ldcr %0, cr18" : "=r" (ossr1)); \
	__asm__ __volatile__ ("ldcr %0, cr19" : "=r" (ossr2)); \
	__asm__ __volatile__ ("ldcr %0, cr20" : "=r" (ossr3))

#define	OS_VBR() \
	__asm__ __volatile__ ("stcr r0, cr7")

#define	OS_CONTEXT() \
	__asm__ __volatile__ ("stcr %0, cr17" : : "r" (ossr0)); \
	__asm__ __volatile__ ("stcr %0, cr18" : : "r" (ossr1)); \
	__asm__ __volatile__ ("stcr %0, cr19" : : "r" (ossr2)); \
	__asm__ __volatile__ ("stcr %0, cr20" : : "r" (ossr3))

/* ==== */

int
scm_cpuconfig(struct scm_cpuconfig *scc)
{
	SCM_DECL;
	int ret;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	SCM_CONTEXT();
	SCM_VBR();
	__asm__ __volatile__ ("or r2, r0, %0" : : "r" (scc));
	SCM_CALL(SCM_CPUCONFIG);
	__asm__ __volatile__ ("or %0, r0, r2" : "=r" (ret));
	OS_CONTEXT();
	OS_VBR();
	set_psr(psr);

	return (ret);
}

u_int
scm_cpuid()
{
	SCM_DECL;
	u_int ret;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	SCM_CONTEXT();
	SCM_VBR();
	SCM_CALL(SCM_CPUID);
	__asm__ __volatile__ ("or %0, r0, r2" : "=r" (ret));
	OS_CONTEXT();
	OS_VBR();
	set_psr(psr);

	return (ret);
}

int
scm_getc(void)
{
	SCM_DECL;
	u_int ret;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	SCM_CONTEXT();
	SCM_VBR();
	SCM_CALL(SCM_CHAR);
	__asm__ __volatile__ ("or %0, r0, r2" : "=r" (ret));
	OS_CONTEXT();
	OS_VBR();
	set_psr(psr);

	return (ret & 0xff);
}

void
scm_getenaddr(u_char *ea)
{
	SCM_DECL;
	char *addr;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	SCM_CONTEXT();
	SCM_VBR();
	SCM_CALL(SCM_COMMID);
	__asm__ __volatile__ ("or %0, r0, r2" : "=r" (addr));
	OS_CONTEXT();
	OS_VBR();
	set_psr(psr);

	bcopy(addr, ea, 6);
}

__dead void
scm_halt()
{
	SCM_DECL;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	SCM_CONTEXT();
	SCM_VBR();
	SCM_CALL(SCM_HALT);
	OS_CONTEXT();
	OS_VBR();
	set_psr(psr);
	for (;;) ;
}

u_int
scm_memsize(int which)
{
	SCM_DECL;
	u_int msize;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	SCM_CONTEXT();
	SCM_VBR();
	__asm__ __volatile__ ("or r2, r0, %0" : : "r" (which));
	SCM_CALL(SCM_MSIZE);
	__asm__ __volatile__ ("or %0, r0, r2" : "=r" (msize));
	OS_CONTEXT();
	OS_VBR();
	set_psr(psr);

	return (msize);
}

/*
 * Does not accept parameters beyond a string because this would need extra
 * register constraints.
 */
void
scm_printf(const char *msg)
{
	SCM_DECL;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	SCM_CONTEXT();
	SCM_VBR();
	__asm__ __volatile__ ("or r2, r0, %0" : : "r" (msg));
	SCM_CALL(SCM_PTLINE);
	OS_CONTEXT();
	OS_VBR();
	set_psr(psr);
}

u_int
scm_promver()
{
	SCM_DECL;
	u_int ret;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	SCM_CONTEXT();
	SCM_VBR();
	SCM_CALL(SCM_REVNUM);
	__asm__ __volatile__ ("or %0, r0, r2" : "=r" (ret));
	OS_CONTEXT();
	OS_VBR();
	set_psr(psr);

	return (ret);
}

void
scm_putc(int c)
{
	SCM_DECL;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	SCM_CONTEXT();
	SCM_VBR();
	__asm__ __volatile__ ("or r2, r0, %0" : : "r" (c));
	SCM_CALL(SCM_OCHAR);
	OS_CONTEXT();
	OS_VBR();
	set_psr(psr);
}

void
scm_putcrlf()
{
	SCM_DECL;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	SCM_CONTEXT();
	SCM_VBR();
	SCM_CALL(SCM_OCRLF);
	OS_CONTEXT();
	OS_VBR();
	set_psr(psr);
}

__dead void
scm_reboot(const char *cmdline)
{
	SCM_DECL;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	SCM_CONTEXT();
	SCM_VBR();
	__asm__ __volatile__ ("or r2, r0, %0" : : "r" (cmdline));
	SCM_CALL(SCM_REBOOT);
	OS_CONTEXT();
	OS_VBR();
	set_psr(psr);
	for (;;) ;
}

u_int
scm_sysid()
{
	SCM_DECL;
	u_int ret;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	SCM_CONTEXT();
	SCM_VBR();
	SCM_CALL(SCM_SYSID);
	__asm__ __volatile__ ("or %0, r0, r2" : "=r" (ret));
	OS_CONTEXT();
	OS_VBR();
	set_psr(psr);

	return (ret);
}

#ifdef MULTIPROCESSOR
u_int
scm_jpstart(cpuid_t cpu, vaddr_t addr)
{
	SCM_DECL;
	u_int ret;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	SCM_CONTEXT();
	SCM_VBR();
	__asm__ __volatile__ ("or r2, r0, %0; or r3, r0, %1" : :
	    "r" (cpu), "r" (addr));
	SCM_CALL(SCM_JPSTART);
	__asm__ __volatile__ ("or %0, r0, r2" : "=r" (ret));
	OS_CONTEXT();
	OS_VBR();
	set_psr(psr);

	return (ret);
}
#endif
