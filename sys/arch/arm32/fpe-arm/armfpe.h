/*
 * $NetBSD: armfpe.h,v 1.3 1996/03/18 19:54:53 mark Exp $
 *
 * Copyright (c) 1995 Neil A Carson.
 * All rights reserved.
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
 * This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
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
 * armfpe.h
 *
 * Details of functions and structures needed for ARM FP cor
 * support in RiscBSD
 *
 * Created      : 04/01/96
 */

#ifndef __ARM32_ARM_FPE_H
#define __ARM32_ARM_FPE_H

/************************************ Types ***********************************/

/*
 * An extended precision floating point number
 */
 
typedef struct {
	u_int32_t exponent;
	u_int32_t mantissa_hi;
	u_int32_t mantissa_lo;
} fp_extended_precision_t;

/*
 * Type for a saved FP context, if we want to translate the context to a
 * user-readable form
 */
 
typedef struct fp_context_frame {
	u_int32_t fpsr;
	fp_extended_precision_t regs[8];
} fp_context_frame_t;

/************************************* Module *********************************/

typedef struct {
	/*
	 * Addresses of procedures/functions
	 */

	u_int32_t core_abort_addr;
	u_int32_t core_initws_addr;
	u_int32_t core_initcontext_addr;
	u_int32_t core_changecontext_addr;
	u_int32_t core_shutdown_addr;
	u_int32_t core_activatecontext_addr;
	u_int32_t core_deactivatecontext_addr;
	u_int32_t core_savecontext_addr;
	u_int32_t core_loadcontext_addr;
	u_int32_t core_disable_addr;
	u_int32_t core_enable_addr;

	/*
	 * Addresses of things that need to be filled in by the kernel on startup
	 */

	u_int32_t *main_ws_ptr_addr;
	u_int32_t *local_handler_ptr_addr;
	u_int32_t *old_handler_ptr_addr;
	u_int32_t *exc_handler_ptr_addr;
	u_int32_t *fp_post_proc_addr;

	/*
	 * Numbers that the kernel needs - these are not pointers so do not need to
	 * be relocated!
	 */

	u_int32_t WorkspaceLength;
	u_int32_t ContextLength;
} arm_fpe_mod_hdr_t;

/**************************** Procedures in arm_fpe.c *************************/

#ifdef _KERNEL

/* macro to return the FP context for a process */

#define FP_CONTEXT(p) ((u_int)(((u_char *)(p)->p_addr) + sizeof(struct user)))

/* Prototypes */

void arm_fpe_mod_reloc	__P((void));
int arm_fpe_boot	__P((void));
int initialise_arm_fpe	__P((cpu_t *cpu));
void arm_fpe_postproc	__P((u_int fpframe, struct trapframe *frame));
void arm_fpe_exception	__P((int exception, u_int pc, u_int fpframe));

void arm_fpe_core_disable	__P((void));
void arm_fpe_core_enable	__P((void));
u_int arm_fpe_core_initws	__P((u_int workspace, int handler1, int handler2));
u_int arm_fpe_core_abort	__P((u_int context, int r12, int pc));
void arm_fpe_core_initcontext	__P((u_int context));
u_int arm_fpe_core_changecontext	__P((u_int context));
void arm_fpe_core_shutdown		__P((void));
void arm_fpe_core_activatecontext	__P((u_int context));
u_int arm_fpe_core_deactivatecontext	__P((void));
u_int arm_fpe_core_savecontext	__P((u_int context, int *savearea, int pc));
void arm_fpe_core_loadcontext	__P((u_int context, int *loadarea));
void arm_fpe_copycontext	__P((u_int c1, u_int c2));
#endif

#endif
