/*	$OpenBSD: exception.h,v 1.4 2004/09/16 07:25:24 miod Exp $ */

/*
 * Copyright (c) 1998-2003 Opsycon AB (www.opsycon.se)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 *  Definitions for exeption processing.
 */

#ifndef _MIPS_EXCEPTION_H_
#define _MIPS_EXCEPTION_H_

/*
 *  Exception codes.
 */

#define	EX_INT		0	/* Interrupt */
#define	EX_MOD		1	/* TLB Modification */
#define	EX_TLBL		2	/* TLB exception, load or i-fetch */
#define	EX_TLBS		3	/* TLB exception, store */
#define	EX_ADEL		4	/* Address error exception, load or i-fetch */
#define	EX_ADES		5	/* Address error exception, store */
#define	EX_IBE		6	/* Bus error exception, i-fetch */
#define	EX_DBE		7	/* Bus error exception, data reference */
#define	EX_SYS		8	/* Syscall exception */
#define	EX_BP		9	/* Breakpoint exception */
#define	EX_RI		10	/* Reserved instruction exception */
#define	EX_CPU		11	/* Coprocessor unusable exception */
#define	EX_OV		12	/* Arithmetic overflow exception */
#define	EX_TR		13	/* Trap exception */
#define	EX_VCEI		14	/* Virtual coherency exception instruction */
#define	EX_FPE		15	/* Floating point exception */
#define	EX_WATCH	23	/* Reference to watch/hi/watch/lo address */
#define	EX_VCED		31	/* Virtual coherency exception data */

#define	EX_U		32	/* Exception from user mode (SW flag) */

#if defined(DDB) || defined(DEBUG)
#define EX_SIZE	10
struct ex_debug {
	u_int	ex_status;
	u_int	ex_cause;
	u_int	ex_badaddr;
	u_int	ex_pc;
	u_int	ex_ra;
	u_int	ex_sp;
	u_int	ex_code;
} ex_debug[EX_SIZE], *exp = ex_debug;

#endif
#endif /* !_MIPS_EXCEPTION_H_ */
