/*	$NetBSD: db_command.c,v 1.15 1995/10/26 14:52:33 gwr Exp $	*/

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
 */

/*
 * Command dispatcher.
 */
#include <sys/param.h>
#include <sys/proc.h>

#include <vm/vm.h>

#include <machine/db_machdep.h>		/* type definitions */

#include <ddb/db_lex.h>
#include <ddb/db_output.h>
#include <ddb/db_command.h>

#include <setjmp.h>

/*
 * Exported global variables
 */
boolean_t	db_cmd_loop_done;
jmp_buf		*db_recover;

/*
 * if 'ed' style: 'dot' is set at start of last item printed,
 * and '+' points to next line.
 * Otherwise: 'dot' points to next item, '..' points to last.
 */
boolean_t	db_ed_style = TRUE;

/*
 * Utility routine - discard tokens through end-of-line.
 */
void
db_skip_to_eol()
{
	int	t;
	do {
	    t = db_read_token();
	} while (t != tEOL);
}

/*
 * Results of command search.
 */
#define	CMD_UNIQUE	0
#define	CMD_FOUND	1
#define	CMD_NONE	2
#define	CMD_AMBIGUOUS	3
#define	CMD_HELP	4

/*
 * Search for command prefix.
 */
int
db_cmd_search(name, table, cmdp)
	char			*name;
	struct db_command	*table;
	struct db_command	**cmdp;	/* out */
{
	struct db_command	*cmd;
	int			result = CMD_NONE;

	for (cmd = table; cmd->name != 0; cmd++) {
	    register char *lp;
	    register char *rp;
	    register int  c;

	    lp = name;
	    rp = cmd->name;
	    while ((c = *lp) == *rp) {
		if (c == 0) {
		    /* complete match */
		    *cmdp = cmd;
		    return (CMD_UNIQUE);
		}
		lp++;
		rp++;
	    }
	    if (c == 0) {
		/* end of name, not end of command -
		   partial match */
		if (result == CMD_FOUND) {
		    result = CMD_AMBIGUOUS;
		    /* but keep looking for a full match -
		       this lets us match single letters */
		}
		else {
		    *cmdp = cmd;
		    result = CMD_FOUND;
		}
	    }
	}
	if (result == CMD_NONE) {
	    /* check for 'help' */
		if (name[0] == 'h' && name[1] == 'e'
		    && name[2] == 'l' && name[3] == 'p')
			result = CMD_HELP;
	}
	return (result);
}

void
db_cmd_list(table)
	struct db_command *table;
{
	register struct db_command *cmd;

	for (cmd = table; cmd->name != 0; cmd++) {
	    db_printf("%-12s", cmd->name);
	    db_end_line();
	}
}

void
db_command(last_cmdp, cmd_table)
	struct db_command	**last_cmdp;	/* IN_OUT */
	struct db_command	*cmd_table;
{
	struct db_command	*cmd;
	int		t;
	char		modif[TOK_STRING_SIZE];
	db_expr_t	addr, count;
	boolean_t	have_addr;
	int		result;

	t = db_read_token();
	if (t == tEOL) {
	    /* empty line repeats last command, at 'next' */
	    cmd = *last_cmdp;
	    addr = (db_expr_t)db_next;
	    have_addr = FALSE;
	    count = 1;
	    modif[0] = '\0';
	}
	else if (t == tEXCL) {
	    void db_fncall();
	    db_fncall();
	    return;
	}
	else if (t != tIDENT) {
	    db_printf("?\n");
	    db_flush_lex();
	    return;
	}
	else {
	    /*
	     * Search for command
	     */
	    while (cmd_table) {
		result = db_cmd_search(db_tok_string,
				       cmd_table,
				       &cmd);
		switch (result) {
		    case CMD_NONE:
			db_printf("No such command\n");
			db_flush_lex();
			return;
		    case CMD_AMBIGUOUS:
			db_printf("Ambiguous\n");
			db_flush_lex();
			return;
		    case CMD_HELP:
			db_cmd_list(cmd_table);
			db_flush_lex();
			return;
		    default:
			break;
		}
		if ((cmd_table = cmd->more) != 0) {
		    t = db_read_token();
		    if (t != tIDENT) {
			db_cmd_list(cmd_table);
			db_flush_lex();
			return;
		    }
		}
	    }

	    if ((cmd->flag & CS_OWN) == 0) {
		/*
		 * Standard syntax:
		 * command [/modifier] [addr] [,count]
		 */
		t = db_read_token();
		if (t == tSLASH) {
		    t = db_read_token();
		    if (t != tIDENT) {
			db_printf("Bad modifier\n");
			db_flush_lex();
			return;
		    }
		    db_strcpy(modif, db_tok_string);
		}
		else {
		    db_unread_token(t);
		    modif[0] = '\0';
		}

		if (db_expression(&addr)) {
		    db_dot = (db_addr_t) addr;
		    db_last_addr = db_dot;
		    have_addr = TRUE;
		}
		else {
		    addr = (db_expr_t) db_dot;
		    have_addr = FALSE;
		}
		t = db_read_token();
		if (t == tCOMMA) {
		    if (!db_expression(&count)) {
			db_printf("Count missing\n");
			db_flush_lex();
			return;
		    }
		}
		else {
		    db_unread_token(t);
		    count = -1;
		}
		if ((cmd->flag & CS_MORE) == 0) {
		    db_skip_to_eol();
		}
	    }
	}
	*last_cmdp = cmd;
	if (cmd != 0) {
	    /*
	     * Execute the command.
	     */
	    (*cmd->fcn)(addr, have_addr, count, modif);

	    if (cmd->flag & CS_SET_DOT) {
		/*
		 * If command changes dot, set dot to
		 * previous address displayed (if 'ed' style).
		 */
		if (db_ed_style) {
		    db_dot = db_prev;
		}
		else {
		    db_dot = db_next;
		}
	    }
	    else {
		/*
		 * If command does not change dot,
		 * set 'next' location to be the same.
		 */
		db_next = db_dot;
	    }
	}
}

/*ARGSUSED*/
void
db_map_print_cmd(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{
        extern void	_vm_map_print();
        boolean_t full = FALSE;
        
        if (modif[0] == 'f')
                full = TRUE;

        _vm_map_print((vm_map_t)addr, full, db_printf);
}

/*ARGSUSED*/
void
db_object_print_cmd(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{
        extern void	_vm_object_print();
        boolean_t full = FALSE;
        
        if (modif[0] == 'f')
                full = TRUE;

        _vm_object_print((vm_object_t)addr, full, db_printf);
}

/*
 * 'show' commands
 */
extern void	db_show_all_procs();
extern void	db_show_callout();
extern void	db_listbreak_cmd();
extern void	db_listwatch_cmd();
extern void	db_show_regs();
void		db_show_help();

struct db_command db_show_all_cmds[] = {
	{ "procs",	db_show_all_procs,0,	0 },
	{ "callout",	db_show_callout,0,	0 },
	{ (char *)0 }
};

struct db_command db_show_cmds[] = {
	{ "all",	0,			0,	db_show_all_cmds },
	{ "registers",	db_show_regs,		0,	0 },
	{ "breaks",	db_listbreak_cmd, 	0,	0 },
	{ "watches",	db_listwatch_cmd, 	0,	0 },
	{ "map",	db_map_print_cmd,	0,	0 },
	{ "object",	db_object_print_cmd,	0,	0 },
	{ (char *)0, }
};

extern void	db_print_cmd(), db_examine_cmd(), db_set_cmd();
extern void	db_search_cmd();
extern void	db_write_cmd();
extern void	db_delete_cmd(), db_breakpoint_cmd();
extern void	db_deletewatch_cmd(), db_watchpoint_cmd();
extern void	db_single_step_cmd(), db_trace_until_call_cmd(),
		db_trace_until_matching_cmd(), db_continue_cmd();
extern void	db_stack_trace_cmd();
void		db_help_cmd();
void		db_fncall();

struct db_command db_command_table[] = {
#ifdef DB_MACHINE_COMMANDS
  /* this must be the first entry, if it exists */
	{ "machine",    0,                      0,     		0},
#endif
	{ "print",	db_print_cmd,		0,		0 },
	{ "examine",	db_examine_cmd,		CS_SET_DOT, 	0 },
	{ "x",		db_examine_cmd,		CS_SET_DOT, 	0 },
	{ "search",	db_search_cmd,		CS_OWN|CS_SET_DOT, 0 },
	{ "set",	db_set_cmd,		CS_OWN,		0 },
	{ "write",	db_write_cmd,		CS_MORE|CS_SET_DOT, 0 },
	{ "w",		db_write_cmd,		CS_MORE|CS_SET_DOT, 0 },
	{ "delete",	db_delete_cmd,		0,		0 },
	{ "d",		db_delete_cmd,		0,		0 },
	{ "break",	db_breakpoint_cmd,	0,		0 },
	{ "dwatch",	db_deletewatch_cmd,	0,		0 },
	{ "watch",	db_watchpoint_cmd,	CS_MORE,	0 },
	{ "step",	db_single_step_cmd,	0,		0 },
	{ "s",		db_single_step_cmd,	0,		0 },
	{ "continue",	db_continue_cmd,	0,		0 },
	{ "c",		db_continue_cmd,	0,		0 },
	{ "until",	db_trace_until_call_cmd,0,		0 },
	{ "next",	db_trace_until_matching_cmd,0,		0 },
	{ "match",	db_trace_until_matching_cmd,0,		0 },
	{ "trace",	db_stack_trace_cmd,	0,		0 },
	{ "call",	db_fncall,		CS_OWN,		0 },
	{ "ps",		db_show_all_procs,	0,		0 },
	{ "callout",	db_show_callout,	0,		0 },
	{ "show",	0,			0,		db_show_cmds },
	{ (char *)0, }
};

#ifdef DB_MACHINE_COMMANDS

/* this function should be called to install the machine dependent
   commands. It should be called before the debugger is enabled  */
void db_machine_commands_install(ptr)
struct db_command *ptr;
{
  db_command_table[0].more = ptr;
  return;
}

#endif

struct db_command	*db_last_command = 0;

void
db_help_cmd()
{
	struct db_command *cmd = db_command_table;

	while (cmd->name != 0) {
	    db_printf("%-12s", cmd->name);
	    db_end_line();
	    cmd++;
	}
}

void
db_command_loop()
{
	jmp_buf		db_jmpbuf;
	jmp_buf		*savejmp = db_recover;
	extern int	db_output_line;

	/*
	 * Initialize 'prev' and 'next' to dot.
	 */
	db_prev = db_dot;
	db_next = db_dot;

	db_cmd_loop_done = 0;
	(void) setjmp(*(db_recover = &db_jmpbuf));

	while (!db_cmd_loop_done) {
		if (db_print_position() != 0)
			db_printf("\n");
		db_output_line = 0;

		db_printf("db> ");
		(void) db_read_line();

		db_command(&db_last_command, db_command_table);
	}

	db_recover = savejmp;
}

void
db_error(s)
	char *s;
{
	if (s)
	    db_printf(s);
	db_flush_lex();
	longjmp(*db_recover, 1);
}


/*
 * Call random function:
 * !expr(arg,arg,arg)
 */
void
db_fncall()
{
	db_expr_t	fn_addr;
#define	MAXARGS		11
	db_expr_t	args[MAXARGS];
	int		nargs = 0;
	db_expr_t	retval;
	db_expr_t	(*func)();
	int		t;

	if (!db_expression(&fn_addr)) {
	    db_printf("Bad function\n");
	    db_flush_lex();
	    return;
	}
	func = (db_expr_t (*) ()) fn_addr;

	t = db_read_token();
	if (t == tLPAREN) {
	    if (db_expression(&args[0])) {
		nargs++;
		while ((t = db_read_token()) == tCOMMA) {
		    if (nargs == MAXARGS) {
			db_printf("Too many arguments\n");
			db_flush_lex();
			return;
		    }
		    if (!db_expression(&args[nargs])) {
			db_printf("Argument missing\n");
			db_flush_lex();
			return;
		    }
		    nargs++;
		}
		db_unread_token(t);
	    }
	    if (db_read_token() != tRPAREN) {
		db_printf("?\n");
		db_flush_lex();
		return;
	    }
	}
	db_skip_to_eol();

	while (nargs < MAXARGS) {
	    args[nargs++] = 0;
	}

	retval = (*func)(args[0], args[1], args[2], args[3], args[4],
			 args[5], args[6], args[7], args[8], args[9] );
	db_printf("%#n\n", retval);
}
