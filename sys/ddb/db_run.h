/*	$NetBSD: db_run.h,v 1.2 1994/10/09 08:30:12 mycroft Exp $	*/

/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
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
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 *
 * 	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */

#ifndef	_DDB_DB_RUN_
#define	_DDB_DB_RUN_

/*
 * Commands to run process.
 */
int		db_inst_count;
int		db_load_count;
int		db_store_count;

#ifndef db_set_single_step
void db_set_single_step __P((db_regs_t *));
#endif
#ifndef db_clear_single_step
void db_clear_single_step __P((db_regs_t *));
#endif
void db_restart_at_pc __P((db_regs_t *, boolean_t));
boolean_t db_stop_at_pc __P((db_regs_t *, boolean_t *));

#endif	_DDB_DB_RUN_
