/*	$OpenBSD: machdep.h,v 1.4 2011/10/19 20:18:31 drahn Exp $	*/
/* $NetBSD: machdep.h,v 1.7 2002/02/21 02:52:21 thorpej Exp $ */

#ifndef _ARM_MACHDEP_H_
#define _ARM_MACHDEP_H_

/* misc prototypes used by the many arm machdeps */
void halt (void);
void parse_mi_bootargs (char *);
void data_abort_handler (trapframe_t *);
void prefetch_abort_handler (trapframe_t *);
void undefinedinstruction_bounce (trapframe_t *);
void dumpsys	(void);

/* 
 * note that we use void * as all the platforms have different ideas on what
 * the structure is
 */
u_int initarm (void *, void *, void *);

/* from arm/arm/intr.c */
void dosoftints (void);
void set_spl_masks (void);
#ifdef DIAGNOSTIC
void dump_spl_masks (void);
#endif
#endif
