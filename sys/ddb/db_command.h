/*	$OpenBSD: db_command.h,v 1.30 2010/11/05 15:17:50 claudio Exp $	*/
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
void db_skip_to_eol(void);
struct db_command;
int db_cmd_search(char *, struct db_command *, struct db_command **);
void db_cmd_list(struct db_command *);
void db_command(struct db_command **, struct db_command *);
void db_buf_print_cmd(db_expr_t, int, db_expr_t, char *);
void db_map_print_cmd(db_expr_t, int, db_expr_t, char *);
void db_malloc_print_cmd(db_expr_t, int, db_expr_t, char *);
void db_mbuf_print_cmd(db_expr_t, int, db_expr_t, char *);
void db_mount_print_cmd(db_expr_t, int, db_expr_t, char *);
void db_show_all_mounts(db_expr_t, int, db_expr_t, char *);
void db_show_all_vnodes(db_expr_t, int, db_expr_t, char *);
void db_show_all_bufs(db_expr_t, int, db_expr_t, char *);
void db_object_print_cmd(db_expr_t, int, db_expr_t, char *);
void db_page_print_cmd(db_expr_t, int, db_expr_t, char *);
void db_extent_print_cmd(db_expr_t, int, db_expr_t, char *);
void db_pool_print_cmd(db_expr_t, int, db_expr_t, char *);
void db_proc_print_cmd(db_expr_t, int, db_expr_t, char *);
void db_uvmexp_print_cmd(db_expr_t, int, db_expr_t, char *);
void db_vnode_print_cmd(db_expr_t, int, db_expr_t, char *);
void db_nfsreq_print_cmd(db_expr_t, int, db_expr_t, char *);
void db_nfsnode_print_cmd(db_expr_t, int, db_expr_t, char *);
void db_machine_commands_install(struct db_command *);
void db_help_cmd(db_expr_t, int, db_expr_t, char *);
void db_command_loop(void);
void db_error(char *);
void db_fncall(db_expr_t, int, db_expr_t, char *);
void db_boot_sync_cmd(db_expr_t, int, db_expr_t, char *);
void db_boot_crash_cmd(db_expr_t, int, db_expr_t, char *);
void db_boot_dump_cmd(db_expr_t, int, db_expr_t, char *);
void db_boot_halt_cmd(db_expr_t, int, db_expr_t, char *);
void db_boot_reboot_cmd(db_expr_t, int, db_expr_t, char *);
void db_boot_poweroff_cmd(db_expr_t, int, db_expr_t, char *);
void db_stack_trace_cmd(db_expr_t, int, db_expr_t, char *);
void db_dmesg_cmd(db_expr_t, int, db_expr_t, char *);
void db_show_panic_cmd(db_expr_t, int, db_expr_t, char *);
void db_bcstats_print_cmd(db_expr_t, int, db_expr_t, char *);
void db_struct_offset_cmd(db_expr_t, int, db_expr_t, char *);
void db_struct_layout_cmd(db_expr_t, int, db_expr_t, char *);

extern	db_addr_t db_dot, db_last_addr, db_prev, db_next;

/*
 * Command table
 */
struct db_command {
	char		*name;		/* command name */
	/* function to call */
	void		(*fcn)(db_expr_t, int, db_expr_t, char *);
	int		flag;		/* extra info: */
#define	CS_OWN		0x1		/* non-standard syntax */
#define	CS_MORE		0x2		/* standard syntax, but may have other
					   words at end */
#define	CS_SET_DOT	0x100		/* set dot after command */
	struct db_command *more;	/* another level of command */
};
