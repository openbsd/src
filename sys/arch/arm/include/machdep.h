/*	$OpenBSD: machdep.h,v 1.1 2004/02/01 05:09:49 drahn Exp $	*/
/* $NetBSD: machdep.h,v 1.7 2002/02/21 02:52:21 thorpej Exp $ */

#ifndef _ARM32_BOOT_MACHDEP_H_
#define _ARM32_BOOT_MACHDEP_H_

/* misc prototypes used by the many arm machdeps */
void halt __P((void));
void parse_mi_bootargs __P((char *));
void data_abort_handler __P((trapframe_t *));
void prefetch_abort_handler __P((trapframe_t *));
void undefinedinstruction_bounce __P((trapframe_t *));
void dumpsys	__P((void));

/* 
 * note that we use void * as all the platforms have different ideas on what
 * the structure is
 */
u_int initarm __P((void *));

/* from arm/arm/intr.c */
void dosoftints __P((void));
void set_spl_masks __P((void));
#ifdef DIAGNOSTIC
void dump_spl_masks __P((void));
#endif
#endif
