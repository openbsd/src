/*	$OpenBSD: db_output.h,v 1.5 1996/04/21 22:19:07 deraadt Exp $ */
/*	$NetBSD: db_output.h,v 1.9 1996/04/04 05:13:50 cgd Exp $	*/

/* 
 * Mach Operating System
 * Copyright (c) 1993,1992,1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 *
 * 	Author: David B. Golub, Carnegie Mellon University
 *	Date:	8/90
 */

/*
 * Printing routines for kernel debugger.
 */
void db_force_whitespace __P((void));
void db_putchar __P((int));
int db_print_position __P((void));
void db_printf __P((const char *, ...))
    __kprintf_attribute__((__format__(__kprintf__,1,2)));
void kdbprintf __P((const char *, ...))
    __kprintf_attribute__((__format__(__kprintf__,1,2)));
void db_end_line __P((void));
