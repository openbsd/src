/*	$OpenBSD: db_sym.c,v 1.48 2016/03/01 11:56:00 mpi Exp $	*/
/*	$NetBSD: db_sym.c,v 1.24 2000/08/11 22:50:47 tv Exp $	*/

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
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/db_machdep.h>

#include <ddb/db_lex.h>
#include <ddb/db_sym.h>
#include <ddb/db_output.h>
#include <ddb/db_command.h>

extern char end[];

/*
 * Initialize the kernel debugger by initializing the master symbol
 * table.  Note that if initializing the master symbol table fails,
 * no other symbol tables can be loaded.
 */
void
ddb_init(void)
{
	const char *name = "bsd";
	extern char *esym;
#if defined(__sparc64__) || defined(__mips__) || defined(__amd64__) || \
    defined(__i386__)
	extern char *ssym;
#endif
	char *xssym, *xesym;

	xesym = esym;
#if defined(__sparc64__) || defined(__mips__) || defined(__amd64__) || \
    defined(__i386__)
	xssym = ssym;
#else
	xssym = (char *)&end;
#endif
	/*
	 * Do this check now for the master symbol table to avoid printing
	 * the message N times.
	 */
	if ((((vaddr_t)xssym) & (sizeof(long) - 1)) != 0) {
		printf("[ %s symbol table has bad start address %p ]\n",
		    name, xssym);
		return;
	}

	if (xesym != NULL && xesym != xssym) {
		if (db_elf_sym_init((vaddr_t)xesym - (vaddr_t)xssym, xssym,
		    xesym, name) == TRUE)
			return;
	}

	printf("[ no symbol table formats found ]\n");
}

boolean_t
db_eqname(char *src, char *dst, int c)
{
	if (!strcmp(src, dst))
	    return (TRUE);
	if (src[0] == c)
	    return (!strcmp(src+1,dst));
	return (FALSE);
}

boolean_t
db_value_of_name(char *name, db_expr_t *valuep)
{
	db_sym_t	sym;

	sym = db_lookup(name);
	if (sym == NULL)
	    return (FALSE);
	db_symbol_values(sym, &name, valuep);
	return (TRUE);
}


/*
 * Lookup a symbol.
 */
db_sym_t
db_lookup(char *symstr)
{
	return db_elf_sym_lookup(symstr);
}

/*
 * Find the closest symbol to val, and return its name
 * and the difference between val and the symbol found.
 */
db_sym_t
db_search_symbol(db_addr_t val, db_strategy_t strategy, db_expr_t *offp)
{
	unsigned int	diff;
	db_expr_t	newdiff;
	db_sym_t	ret = NULL, sym;

	newdiff = diff = ~0;
	sym = db_elf_sym_search(val, strategy, &newdiff);
	if (newdiff < diff) {
		diff = newdiff;
		ret = sym;
	}
	*offp = diff;
	return ret;
}

/*
 * Return name and value of a symbol
 */
void
db_symbol_values(db_sym_t sym, char **namep, db_expr_t *valuep)
{
	db_expr_t	value;

	if (sym == NULL) {
		*namep = NULL;
		return;
	}

	db_elf_sym_values(sym, namep, &value);

	if (valuep)
		*valuep = value;
}


/*
 * Print a the closest symbol to value
 *
 * After matching the symbol according to the given strategy
 * we print it in the name+offset format, provided the symbol's
 * value is close enough (eg smaller than db_maxoff).
 * We also attempt to print [filename:linenum] when applicable
 * (eg for procedure names).
 *
 * If we could not find a reasonable name+offset representation,
 * then we just print the value in hex.  Small values might get
 * bogus symbol associations, e.g. 3 might get some absolute
 * value like _INCLUDE_VERSION or something, therefore we do
 * not accept symbols whose value is zero (and use plain hex).
 * Also, avoid printing as "end+0x????" which is useless.
 * The variable db_lastsym is used instead of "end" in case we
 * add support for symbols in loadable driver modules.
 */
unsigned long	db_lastsym = (unsigned long)end;
unsigned int	db_maxoff = 0x10000000;


void
db_printsym(db_expr_t off, db_strategy_t strategy,
    int (*pr)(const char *, ...))
{
	db_expr_t	d;
	char 		*filename;
	char		*name;
	db_expr_t	value;
	int 		linenum;
	db_sym_t	cursym;
	char		buf[DB_FORMAT_BUF_SIZE];

	if (off <= db_lastsym) {
		cursym = db_search_symbol(off, strategy, &d);
		db_symbol_values(cursym, &name, &value);
		if (name && (d < db_maxoff) && value) {
			(*pr)("%s", name);
			if (d) {
				(*pr)("+%s", db_format(buf, sizeof(buf),
				    d, DB_FORMAT_R, 1, 0));
			}
			if (strategy != DB_STGY_PROC) {
				if (db_elf_line_at_pc(cursym, &filename,
				    &linenum, off))
					(*pr)(" [%s:%d]", filename, linenum);
			}
			return;
		}
	}

	(*pr)("%s", db_format(buf, sizeof(buf), off, DB_FORMAT_N, 1, 0));
	return;
}
