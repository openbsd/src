/*	$NetBSD: locore.h,v 1.2 1996/05/20 23:38:26 jonathan Exp $	*/

/*
 * Copyright 1996 The Board of Trustees of The Leland Stanford
 * Junior University. All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  Stanford University
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

/*
 * Jump table for MIPS cpu locore functions that are implemented
 * differently on different generations, or instruction-level
 * archtecture (ISA) level, the Mips family.
 * The following functions must be provided for each mips ISA level:
 *
 * 
 *	MachConfigCache
 *	MachFlushCache
 *	MachFlushDCache
 *	MachFlushICache
 *	MachForceCacheUpdate
 *	MachSetPID
 *	MachTLBFlush
 *	MachTLBFlushAddr __P()
 *	MachTLBUpdate (u_int, (pt_entry_t?) u_int);
 *	MachTLBWriteIndexed
 *
 * We currently provide support for:
 *
 *	r2000 and r3000 (mips ISA-I)
 *	r4000 and r4400 in 32-bit mode (mips ISA-III?)
 */

#ifndef _MIPS_LOCORE_H
#define  _MIPS_LOCORE_H

/*
 * locore functions used by vm_machdep.c.
 * These are not yet CPU-model specific.
 */

struct user;
extern int  copykstack __P((struct user *up));
extern void MachSaveCurFPState __P((struct proc *p));
extern int switch_exit __P((void)); /* XXX never really returns? */

/* MIPS-generic locore functions used by trap.c */
 extern void MachFPTrap __P((u_int statusReg, u_int CauseReg, u_int pc));

/*
 * locore service routine for exeception vectors. Used outside locore
 * only to print them by name in stack tracebacks
 */

extern void mips_r2000_KernIntr __P(());

extern void mips_r2000_ConfigCache  __P((void));
extern void mips_r2000_FlushCache  __P((void));
extern void mips_r2000_FlushDCache  __P((vm_offset_t addr, vm_offset_t len));
extern void mips_r2000_FlushICache  __P((vm_offset_t addr, vm_offset_t len));
extern void mips_r2000_ForceCacheUpdate __P((void));
extern void mips_r2000_SetPID   __P((int pid));
extern void mips_r2000_TLBFlush __P((void));
extern void mips_r2000_TLBFlushAddr   __P( /* XXX Really pte highpart ? */
					  (vm_offset_t addr));
extern void mips_r2000_TLBUpdate __P((u_int, /*pt_entry_t*/ u_int));
extern void mips_r2000_TLBWriteIndexed  __P((u_int index, u_int high,
					    u_int low));

extern void mips_r4000_ConfigCache __P((void));
extern void mips_r4000_FlushCache  __P((void));
extern void mips_r4000_FlushDCache __P((vm_offset_t addr, vm_offset_t len));
extern void mips_r4000_FlushICache __P((vm_offset_t addr, vm_offset_t len));
extern void mips_r4000_ForceCacheUpdate __P((void));
extern void mips_r4000_SetPID  __P((int pid));
extern void mips_r4000_TLBFlush __P((void));
extern void mips_r4000_TLBFlushAddr __P( /* XXX Really pte highpart ? */
					  (vm_offset_t addr));
extern void mips_r4000_TLBUpdate __P((u_int, /*pt_entry_t*/ u_int));
extern void mips_r4000_TLBWriteIndexed __P((u_int index, u_int high,
					    u_int low));

/*
 *  A vector with an entry for each mips-ISA-level dependent
 * locore function, and macros which jump through it.
 * XXX the macro names are chosen to be compatible with the old
 * Sprite  coding-convention names used in 4.4bsd/pmax.
 */
typedef struct  {
	void (*configCache) __P((void));
	void (*flushCache)  __P((void));
	void (*flushDCache) __P((vm_offset_t addr, vm_offset_t len));
	void (*flushICache) __P((vm_offset_t addr, vm_offset_t len));
	void (*forceCacheUpdate)  __P((void));
	void (*setTLBpid)  __P((int pid));
	void (*tlbFlush)  __P((void));
	void (*tlbFlushAddr)  __P((vm_offset_t)); /* XXX Really pte highpart ? */
	void (*tlbUpdate)  __P((u_int highreg, u_int lowreg));
	void (*tlbWriteIndexed)  __P((u_int, u_int, u_int));
} mips_locore_jumpvec_t;


/*
 * The "active" locore-fuction vector, and

 */
extern mips_locore_jumpvec_t mips_locore_jumpvec;
extern mips_locore_jumpvec_t r2000_locore_vec;
extern mips_locore_jumpvec_t r4000_locore_vec;

#define MachConfigCache		(*(mips_locore_jumpvec.configCache))
#define MachFlushCache		(*(mips_locore_jumpvec.flushCache))
#define MachFlushDCache		(*(mips_locore_jumpvec.flushDCache))
#define MachFlushICache		(*(mips_locore_jumpvec.flushICache))
#define MachForceCacheUpdate	(*(mips_locore_jumpvec.forceCacheUpdate))
#define MachSetPID		(*(mips_locore_jumpvec.setTLBpid))
#define MachTLBFlush		(*(mips_locore_jumpvec.tlbFlush))
#define MachTLBFlushAddr	(*(mips_locore_jumpvec.tlbFlushAddr))
#define MachTLBUpdate		(*(mips_locore_jumpvec.tlbUpdate))
#define MachTLBWriteIndexed	(*(mips_locore_jumpvec.tlbWriteIndexed))

#endif	/* _MIPS_LOCORE_H */
