/*	$OpenBSD: db_variables.h,v 1.2 1996/02/20 13:35:46 mickey Exp $	*/

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

#ifndef	_DB_VARIABLES_H_
#define	_DB_VARIABLES_H_

/*
 * Debugger variables.
 */
struct db_variable {
	char	*name;		/* Name of variable */
	int	*valuep;	/* value of variable */
				/* function to call when reading/writing */
	int	(*fcn) __P((struct db_variable *, db_expr_t *, int));
#define DB_VAR_GET	0
#define DB_VAR_SET	1
};
#define	FCN_NULL ((int (*) __P((struct db_variable *, db_expr_t *, int)))0)

extern struct db_variable	db_vars[];	/* debugger variables */
extern struct db_variable	*db_evars;
extern struct db_variable	db_regs[];	/* machine registers */
extern struct db_variable	*db_eregs;

int db_find_variable __P((struct db_variable **));
int db_get_variable __P((db_expr_t *));
int db_set_variable __P((db_expr_t));
void db_read_variable __P((struct db_variable *, db_expr_t *));
void db_write_variable __P((struct db_variable *, db_expr_t *));
void db_set_cmd __P((db_expr_t, int, db_expr_t, char *));

#endif	/* _DB_VARIABLES_H_ */
