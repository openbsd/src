/*	$OpenBSD: vectors_88100.S,v 1.1.1.1 2004/04/21 15:24:14 aoyama Exp $	*/
/*
 * Mach Operating System
 * Copyright (c) 1991, 1992 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON AND OMRON ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND OMRON DISCLAIM ANY LIABILITY OF ANY KIND
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

#include <machine/asm.h>

#define	UNDEFINED16	\
	word UNKNOWN_HANDLER; word UNKNOWN_HANDLER; \
	word UNKNOWN_HANDLER; word UNKNOWN_HANDLER; \
	word UNKNOWN_HANDLER; word UNKNOWN_HANDLER; \
	word UNKNOWN_HANDLER; word UNKNOWN_HANDLER; \
	word UNKNOWN_HANDLER; word UNKNOWN_HANDLER; \
	word UNKNOWN_HANDLER; word UNKNOWN_HANDLER; \
	word UNKNOWN_HANDLER; word UNKNOWN_HANDLER; \
	word UNKNOWN_HANDLER; word UNKNOWN_HANDLER;

	data
	.align 4096			  /* VBR points to page aligned list */
GLOBAL(vector_list)
	VECTOR(reset_handler)			/* 00 */
	VECTOR(interrupt_handler)		/* 01 */
	VECTOR(instruction_access_handler)	/* 02 */
	VECTOR(data_exception_handler)		/* 03 */
	VECTOR(misaligned_handler)		/* 04 */
	VECTOR(unimplemented_handler)		/* 05 */
	VECTOR(privilege_handler)		/* 06 */
	VECTOR(bounds_handler)			/* 07 */
	VECTOR(divide_handler)			/* 08 */
	VECTOR(overflow_handler)		/* 09 */
	VECTOR(error_handler)			/* 0a */
	word UNKNOWN_HANDLER			/* 0b */
	word UNKNOWN_HANDLER			/* 0c */
	word UNKNOWN_HANDLER			/* 0d */
	word UNKNOWN_HANDLER			/* 0e */
	word UNKNOWN_HANDLER			/* 0f */
	UNDEFINED16				/* 1x */
	UNDEFINED16				/* 2x */
	UNDEFINED16				/* 3x */
	UNDEFINED16				/* 4x */
	UNDEFINED16				/* 5x */
	UNDEFINED16				/* 6x */
	word UNKNOWN_HANDLER			/* 70 */
	word UNKNOWN_HANDLER			/* 71 */
	VECTOR(fp_precise_handler)		/* 72 */
	VECTOR(fp_imprecise_handler)		/* 73 */
	VECTOR(unimplemented_handler)		/* 74 */
	word UNKNOWN_HANDLER			/* 75 */
	VECTOR(unimplemented_handler)		/* 76 */
	word UNKNOWN_HANDLER			/* 77 */
	VECTOR(unimplemented_handler)		/* 78 */
	word UNKNOWN_HANDLER			/* 79 */
	VECTOR(unimplemented_handler)		/* 7a */
	word UNKNOWN_HANDLER			/* 7b */
	VECTOR(unimplemented_handler)		/* 7c */
	word UNKNOWN_HANDLER			/* 7d */
	VECTOR(unimplemented_handler)		/* 7e */
	word UNKNOWN_HANDLER			/* 7f */
	VECTOR(syscall_handler)			/* 80 */
	VECTOR(syscall_handler)			/* 81 */
	VECTOR(break)				/* 82 */
	VECTOR(trace)				/* 83 */
	VECTOR(entry)				/* 84 */
GLOBAL(vector_list_end)
	word END_OF_VECTOR_LIST

