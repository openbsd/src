/* $NetBSD: cpus.h,v 1.3 1996/03/14 23:11:08 mark Exp $ */

/*
 * Copyright (c) 1995 Mark Brinicombe.
 * Copyright (c) 1995 Brini.
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
 * cpus.h
 *
 * cpu device header file
 *
 * Created      : 26/12/95
 */

#ifndef _LOCORE
#include <sys/param.h>
#endif

/* If hydra is defined then we take into consideration the slave CPU's available */

#ifdef HYDRA
#define MAX_CPUS		6
#define MAX_SLAVE_CPUS		4
#define MAX_FOREIGN_CPUS	1
#else
#define MAX_CPUS		2
#define MAX_SLAVE_CPUS		0
#define MAX_FOREIGN_CPUS	1
#endif

#define CPU_MASTER	0
#define CPU_486		1
#define CPU_SLAVE	2

#define CPU_CLASS_NONE	0	/* No CPU */
#define CPU_CLASS_ARM	1	/* ARM 6/7/8 */
#define CPU_CLASS_SARM	2	/* Guess */
#define CPU_CLASS_I486	3	/* 486/586 */

#define CPU_HOST_NONE		0	/* No host */
#define CPU_HOST_MAINBUS	1	/* Hosted via motherboard */
#define CPU_HOST_HYDRA		2	/* Hosted via hydra multiprocessor board */

#define CPU_FLAG_PRESENT	0x01
#define CPU_FLAG_HALTED		0x02

#define FPU_CLASS_NONE		0	/* no Floating point support */
#define FPU_CLASS_FPE		1	/* Floating point emulator installed */
#define FPU_CLASS_FPA		2	/* Floating point accelerator installed */
#define FPU_CLASS_FPU		3	/* Floating point unit installed */

#define FPU_TYPE_SP_FPE		1	/* Single precision FPE */
#define FPU_TYPE_ARMLTD_FPE	2	/* ARM Ltd FPE */
#define FPU_TYPE_FPA11		0x81	/* ID of FPA11 */

#ifndef _LOCORE

/* Define the structure used to describe a cpu */

typedef struct cpu_arm {
	u_int cpu_id;		/* The CPU id */
	u_int cpu_ctrl;		/* The CPU control register */

	u_int cpu_svc_r13;	/* local data */
	u_int cpu_und_r13;	/* local data */
	u_int cpu_abt_r13;	/* local data */
	u_int cpu_irq_r13;	/* local data */
} cpu_arm_t;

typedef struct cpu_sarm {
	u_int cpu_id;		/* The CPU id */
	u_int cpu_ctrl;		/* The CPU control register */
} cpu_sarm_t;

typedef struct cpu_i486 {
	u_int cpu_id;		/* The CPU id */
	u_int cpu_ctrl;		/* The CPU control register */
} cpu_i486_t;


typedef struct _cpu {
/* These are generic CPU variables */

	u_int	cpu_class;	/* The CPU class */
	u_int	cpu_type;	/* The CPU type */
	u_int	cpu_host;	/* The CPU host interface */
	u_int	cpu_flags;	/* The CPU flags */
	char	cpu_model[256];	/* Text description of CPU */

/* These are generic FPU variables */

	u_int	fpu_class;	/* The FPU class */
	u_int	fpu_type;	/* The FPU type */
	u_int	fpu_flags;	/* The FPU flags */
	char	fpu_model[256];	/* Text description of FPU */

/* These are ARM specific variables */

	u_int cpu_id;		/* The CPU id */
	u_int cpu_ctrl;		/* The CPU control register */

	u_int cpu_svc_r13;	/* local data */
	u_int cpu_und_r13;	/* local data */
	u_int cpu_abt_r13;	/* local data */
	u_int cpu_irq_r13;	/* local data */

/* Not used yet */

	union {
		cpu_arm_t	cpu_arm;
		cpu_sarm_t	cpu_sarm;
		cpu_i486_t	cpu_i486;
	} cpu_local;
} cpu_t;


struct cpu_softc {
	struct	device sc_device;
	int	sc_open;
};

#ifdef _KERNEL

/* Array of cpu structures, one per possible cpu */

extern cpu_t cpus[MAX_CPUS];

#endif	/* _KERNEL */
#endif	/* _LOCORE */

/* End of hydra.h */
