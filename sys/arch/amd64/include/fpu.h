/*	$OpenBSD: fpu.h,v 1.10 2015/03/21 20:42:38 kettenis Exp $	*/
/*	$NetBSD: fpu.h,v 1.1 2003/04/26 18:39:40 fvdl Exp $	*/

#ifndef	_MACHINE_FPU_H_
#define	_MACHINE_FPU_H_

#include <sys/types.h>

/*
 * amd64 only uses the extended save/restore format used
 * by fxsave/fsrestore, to always deal with the SSE registers,
 * which are part of the ABI to pass floating point values.
 * Must be stored in memory on a 16-byte boundary.
 */

struct fxsave64 {
	u_int16_t  fx_fcw;
	u_int16_t  fx_fsw;
	u_int8_t   fx_ftw;
	u_int8_t   fx_unused1;
	u_int16_t  fx_fop;
	u_int64_t  fx_rip;
	u_int64_t  fx_rdp;
	u_int32_t  fx_mxcsr;
	u_int32_t  fx_mxcsr_mask;
	u_int64_t  fx_st[8][2];   /* 8 normal FP regs */
	u_int64_t  fx_xmm[16][2]; /* 16 SSE2 registers */
	u_int8_t   fx_unused3[96];
} __packed;

struct xstate_hdr {
	uint64_t	xstate_bv;
	uint64_t	xstate_xcomp_bv;
	uint8_t		xstate_rsrv0[0];
	uint8_t		xstate_rsrv[40];
} __packed;

struct savefpu {
	struct fxsave64 fp_fxsave;	/* see above */
	struct xstate_hdr fp_xstate;
	u_int64_t fp_ymm[16][2];
	u_int16_t fp_ex_sw;		/* saved status from last exception */
	u_int16_t fp_ex_tw;		/* saved tag from last exception */
};

/*
 * The i387 defaults to Intel extended precision mode and round to nearest,
 * with all exceptions masked.
 */
#define	__INITIAL_NPXCW__	0x037f
#define __INITIAL_MXCSR__ 	0x1f80
#define __INITIAL_MXCSR_MASK__	0xffbf

#ifdef _KERNEL
/*
 * XXX
 */
struct trapframe;
struct cpu_info;

extern uint32_t	fpu_mxcsr_mask;

void fpuinit(struct cpu_info *);
void fpudrop(void);
void fpudiscard(struct proc *);
void fputrap(struct trapframe *);
void fpusave_proc(struct proc *, int);
void fpusave_cpu(struct cpu_info *, int);
void fpu_kernel_enter(void);
void fpu_kernel_exit(void);

#endif

#endif /* _MACHINE_FPU_H_ */
