/*	$OpenBSD: rf_dagfuncs.h,v 1.1 1999/01/11 14:29:11 niklas Exp $	*/
/*	$NetBSD: rf_dagfuncs.h,v 1.1 1998/11/13 04:20:28 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland, William V. Courtright II, Jim Zelenka
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*****************************************************************************************
 *
 * dagfuncs.h -- header file for DAG node execution routines
 * 
 ****************************************************************************************/

/*
 * :  
 * Log: rf_dagfuncs.h,v 
 * Revision 1.17  1996/07/22 19:52:16  jimz
 * switched node params to RF_DagParam_t, a union of
 * a 64-bit int and a void *, for better portability
 * attempted hpux port, but failed partway through for
 * lack of a single C compiler capable of compiling all
 * source files
 *
 * Revision 1.16  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.15  1996/06/06  17:27:20  jimz
 * added another read mirror func (partitioning), changed names so dag
 * creation routines can use the appropriate one
 *
 * Revision 1.14  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.13  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.12  1996/05/24  04:28:55  jimz
 * release cleanup ckpt
 *
 * Revision 1.11  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.10  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.9  1995/12/01  15:56:46  root
 * added copyright info
 *
 * Revision 1.8  1995/11/07  16:25:23  wvcii
 * added DiskUnlockFuncForThreads
 *
 */

#ifndef _RF__RF_DAGFUNCS_H_
#define _RF__RF_DAGFUNCS_H_

int rf_ConfigureDAGFuncs(RF_ShutdownList_t **listp);
int rf_TerminateFunc(RF_DagNode_t *node);
int rf_TerminateUndoFunc(RF_DagNode_t *node);
int rf_DiskReadMirrorIdleFunc(RF_DagNode_t *node);
int rf_DiskReadMirrorPartitionFunc(RF_DagNode_t *node);
int rf_DiskReadMirrorUndoFunc(RF_DagNode_t *node);
int rf_ParityLogUpdateFunc(RF_DagNode_t *node);
int rf_ParityLogOverwriteFunc(RF_DagNode_t *node);
int rf_ParityLogUpdateUndoFunc(RF_DagNode_t *node);
int rf_ParityLogOverwriteUndoFunc(RF_DagNode_t *node);
int rf_NullNodeFunc(RF_DagNode_t *node);
int rf_NullNodeUndoFunc(RF_DagNode_t *node);
int rf_DiskReadFuncForThreads(RF_DagNode_t *node);
int rf_DiskWriteFuncForThreads(RF_DagNode_t *node);
int rf_DiskUndoFunc(RF_DagNode_t *node);
int rf_DiskUnlockFuncForThreads(RF_DagNode_t *node);
int rf_GenericWakeupFunc(RF_DagNode_t *node, int status);
int rf_RegularXorFunc(RF_DagNode_t *node);
int rf_SimpleXorFunc(RF_DagNode_t *node);
int rf_RecoveryXorFunc(RF_DagNode_t *node);
int rf_XorIntoBuffer(RF_Raid_t *raidPtr, RF_PhysDiskAddr_t *pda, char *srcbuf,
	char *targbuf, void *bp);
int rf_bxor(char *src, char *dest, int len, void *bp);
int rf_longword_bxor(register unsigned long *src, register unsigned long *dest,
	int len, void *bp);
int rf_longword_bxor3(register unsigned long *dest, register unsigned long *a,
	register unsigned long *b, register unsigned long *c, int len, void *bp);
int rf_bxor3(unsigned char *dst, unsigned char *a, unsigned char *b,
	unsigned char *c, unsigned long len, void *bp);

/* function ptrs defined in ConfigureDAGFuncs() */
extern int (*rf_DiskReadFunc)(RF_DagNode_t *);
extern int (*rf_DiskWriteFunc)(RF_DagNode_t *);
extern int (*rf_DiskReadUndoFunc)(RF_DagNode_t *);
extern int (*rf_DiskWriteUndoFunc)(RF_DagNode_t *);
extern int (*rf_DiskUnlockFunc)(RF_DagNode_t *);
extern int (*rf_DiskUnlockUndoFunc)(RF_DagNode_t *);
extern int (*rf_SimpleXorUndoFunc)(RF_DagNode_t *);
extern int (*rf_RegularXorUndoFunc)(RF_DagNode_t *);
extern int (*rf_RecoveryXorUndoFunc)(RF_DagNode_t *);

/* macros for manipulating the param[3] in a read or write node */
#define RF_CREATE_PARAM3(pri, lk, unlk, wru) (((RF_uint64)(((wru&0xFFFFFF)<<8)|((lk)?0x10:0)|((unlk)?0x20:0)|((pri)&0xF)) ))
#define RF_EXTRACT_PRIORITY(_x_)     ((((unsigned) ((unsigned long)(_x_))) >> 0) & 0x0F)
#define RF_EXTRACT_LOCK_FLAG(_x_)    ((((unsigned) ((unsigned long)(_x_))) >> 4) & 0x1)
#define RF_EXTRACT_UNLOCK_FLAG(_x_)  ((((unsigned) ((unsigned long)(_x_))) >> 5) & 0x1)
#define RF_EXTRACT_RU(_x_)           ((((unsigned) ((unsigned long)(_x_))) >> 8) & 0xFFFFFF)

#endif /* !_RF__RF_DAGFUNCS_H_ */
