/*	$OpenBSD: db_command.h,v 1.8 1998/08/30 15:38:26 art Exp $	*/
/*	$NetBSD: db_command.h,v 1.8 1996/02/05 01:56:55 christos Exp $	*/

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
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */

/*
 * Command loop declarations.
 */
void db_skip_to_eol __P((void));
struct db_command;
int db_cmd_search __P((char *, struct db_command *, struct db_command **));
void db_cmd_list __P((struct db_command *));
void db_command __P((struct db_command **, struct db_command *));
void db_map_print_cmd __P((db_expr_t, int, db_expr_t, char *));
void db_object_print_cmd __P((db_expr_t, int, db_expr_t, char *));
void db_extent_print_cmd __P((db_expr_t, int, db_expr_t, char *));
void db_machine_commands_install __P((struct db_command *));
void db_help_cmd __P((db_expr_t, int, db_expr_t, char *));
void db_command_loop __P((void));
void db_error __P((char *));
void db_fncall __P((db_expr_t, int, db_expr_t, char *));
void db_boot_sync_cmd __P((db_expr_t, int, db_expr_t, char *));
void db_boot_crash_cmd __P((db_expr_t, int, db_expr_t, char *));
void db_boot_dump_cmd __P((db_expr_t, int, db_expr_t, char *));
void db_sync_cmd __P((db_expr_t, int, db_expr_t, char *));

db_addr_t	db_dot;		/* current location */
db_addr_t	db_last_addr;	/* last explicit address typed */
db_addr_t	db_prev;	/* last address examined
				   or written */
db_addr_t	db_next;	/* next address to be examined
				   or written */

/*
 * Command table
 */
struct db_command {
	char		*name;		/* command name */
	/* function to call */
	void		(*fcn) __P((db_expr_t, int, db_expr_t, char *));
	int		flag;		/* extra info: */
#define	CS_OWN		0x1		/* non-standard syntax */
#define	CS_MORE		0x2		/* standard syntax, but may have other
					   words at end */
#define	CS_SET_DOT	0x100		/* set dot after command */
	struct db_command *more;	/* another level of command */
};
