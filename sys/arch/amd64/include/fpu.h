/*	$OpenBSD: fpu.h,v 1.2 2004/02/28 22:26:05 deraadt Exp $	*/
/*	$NetBSD: fpu.h,v 1.1 2003/04/26 18:39:40 fvdl Exp $	*/

#ifndef	_AMD64_FPU_H_
#define	_AMD64_FPU_H_

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
} __attribute__((packed));

struct savefpu {
	struct fxsave64 fp_fxsave;	/* see above */
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

/* OpenBSD uses IEEE double precision. */
#define	__OpenBSD_NPXCW__	0x127f
/* Linux just uses the default control word. */
#define	__Linux_NPXCW__		0x037f

/*
 * The standard control word from finit is 0x37F, giving:
 *	round to nearest
 *	64-bit precision
 *	all exceptions masked.
 *
 * Now we want:
 *	affine mode (if we decide to support 287's)
 *	round to nearest
 *	53-bit precision
 *	all exceptions masked.
 *
 * 64-bit precision often gives bad results with high level languages
 * because it makes the results of calculations depend on whether
 * intermediate values are stored in memory or in FPU registers.
 */

#ifdef _KERNEL
/*
 * XXX
 */
struct trapframe;
struct cpu_info;

void fpuinit(struct cpu_info *);
void fpudrop(void);
void fpusave(struct proc *);
void fpudiscard(struct proc *);
void fputrap(struct trapframe *);
void fpusave_proc(struct proc *, int);
void fpusave_cpu(struct cpu_info *, int);

#endif

#endif /* _AMD64_FPU_H_ */
