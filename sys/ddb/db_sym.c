/*	$OpenBSD: db_sym.c,v 1.11 1996/05/31 10:37:25 niklas Exp $	*/
/*	$NetBSD: db_sym.c,v 1.12 1996/02/05 01:57:15 christos Exp $	*/

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
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <machine/db_machdep.h>

#include <ddb/db_sym.h>
#include <ddb/db_output.h>
#include <ddb/db_extern.h>
#include <ddb/db_command.h>

/*
 * Multiple symbol tables
 */
static TAILQ_HEAD(, db_symtab)	db_symtabs;
static struct db_symtab	db_sym_kernel;
size_t			db_nsymtabs = 0;

db_symtab_t	db_last_symtab;

/*
 * Init sym module
 */
void
db_sym_init(void)
{
	TAILQ_INIT(&db_symtabs);
}

/*
 * Add symbol table, with given name, to list of symbol tables.
 */
int
db_add_symbol_table(start, end, name, ref)
	char *start;
	char *end;
	char *name;
	char *ref;
{
	db_symtab_t	new = db_nsymtabs? NULL : &db_sym_kernel;

	if (new == NULL &&
	    (new = malloc(sizeof(*new), M_DEVBUF, M_WAITOK)) == NULL)
		return -1;

	new->start = start;
	new->end = end;
	new->name = name;
	new->private = ref;
	TAILQ_INSERT_TAIL(&db_symtabs, new, list);

	return ++db_nsymtabs;
}

/*
 * Delete a symbol table. Caller is responsible for freeing storage.
 */
void
db_del_symbol_table(name)
	char *name;
{
	db_symtab_t	p;

	for (p = db_symtabs.tqh_first; p != NULL; p = p->list.tqe_next)
		if (!strcmp(name, p->name))
			break;

	if (p == NULL)
		db_printf("ddb: %s symbol table was not allocated", name);

	if (--db_nsymtabs == 0)
		panic("ddb: kernel symtab delete");

	TAILQ_REMOVE(&db_symtabs, p, list);
	free(p, M_DEVBUF);
}

db_symtab_t
db_istab(i)
	size_t	i;
{
	register db_symtab_t	p;

	for (p = db_symtabs.tqh_first; i && p != NULL ; p = p->list.tqe_next)
		i--;

	return i? NULL : p;
}

/*
 *  db_qualify("vm_map", "bsd") returns "bsd:vm_map".
 *
 *  Note: return value points to static data whose content is
 *  overwritten by each call... but in practice this seems okay.
 */
char *
db_qualify(sym, symtabname)
	db_sym_t	sym;
	register char	*symtabname;
{
	char		*symname;
	static char     tmp[256];
	register char	*s;

	db_symbol_values(sym, &symname, 0);
	s = tmp;
	while ((*s++ = *symtabname++) != '\0')
		;
	s[-1] = ':';
	while ((*s++ = *symname++) != '\0')
		;
	return tmp;
}


boolean_t
db_eqname(src, dst, c)
	char *src;
	char *dst;
	int c;
{
	if (!strcmp(src, dst))
	    return (TRUE);
	if (src[0] == c)
	    return (!strcmp(src+1,dst));
	return (FALSE);
}

boolean_t
db_value_of_name(name, valuep)
	char		*name;
	db_expr_t	*valuep;
{
	db_sym_t	sym;

	sym = db_lookup(name);
	if (sym == DB_SYM_NULL)
	    return (FALSE);
	db_symbol_values(sym, &name, valuep);
	return (TRUE);
}


/*
 * Lookup a symbol.
 * If the symbol has a qualifier (e.g., ux:vm_map),
 * then only the specified symbol table will be searched;
 * otherwise, all symbol tables will be searched.
 */
db_sym_t
db_lookup(symstr)
	char *symstr;
{
	db_symtab_t	st = NULL;
	db_sym_t	sp = NULL;
	register char	*cp;

	/*
	 * Look for, remove, and remember any symbol table specifier.
	 */
	for (cp = symstr; *cp; cp++)
		if (*cp == ':') {
			*cp = '\0';
			for (st = db_symtabs.tqh_first;
			     st != NULL;
			     st = st->list.tqe_next)
				if (!strcmp(symstr, st->name))
					break;
			*cp = ':';
			if (st == NULL) {
				db_error("invalid symbol table name");
				/* NOTREACHED */
			}
			symstr = cp+1;
		}

	/*
	 * Look in the specified set of symbol tables.
	 * Return on first match.
	 */
	if (st != NULL)
		sp = X_db_lookup(st, symstr);
	else
		for (st = db_symtabs.tqh_first;
		     st != NULL;
		     st = st->list.tqe_next)
			if ((sp = X_db_lookup(st, symstr)) != NULL)
				break;

	if (sp != NULL && st != NULL)
		db_last_symtab = st;

	return sp;
}

/*
 * Does this symbol name appear in more than one symbol table?
 * Used by db_symbol_values to decide whether to qualify a symbol.
 */
boolean_t db_qualify_ambiguous_names = FALSE;

boolean_t
db_symbol_is_ambiguous(sym)
	db_sym_t	sym;
{
	char		*sym_name;
	register db_symtab_t	st;
	register
	boolean_t	found_once = FALSE;

	if (!db_qualify_ambiguous_names)
		return FALSE;

	db_symbol_values(sym, &sym_name, 0);
	for (st = db_symtabs.tqh_first; st != NULL; st = st->list.tqe_next) {
		if (X_db_lookup(st, sym_name) != NULL) {
			if (found_once)
				return TRUE;
			found_once = TRUE;
		}
	}
	return FALSE;
}

/*
 * Find the closest symbol to val, and return its name
 * and the difference between val and the symbol found.
 */
db_sym_t
db_search_symbol( val, strategy, offp)
	register db_addr_t	val;
	db_strategy_t		strategy;
	db_expr_t		*offp;
{
	register
	unsigned int	diff;
	unsigned int	newdiff;
	db_symtab_t	st;
	db_sym_t	ret = DB_SYM_NULL, sym;

	newdiff = diff = ~0;
	db_last_symtab = 0;
	for (st = db_symtabs.tqh_first; st != NULL; st = st->list.tqe_next) {
	    sym = X_db_search_symbol(st, val, strategy, &newdiff);
	    if (newdiff < diff) {
		db_last_symtab = st;
		diff = newdiff;
		ret = sym;
	    }
	}
	*offp = diff;
	return ret;
}

/*
 * Return name and value of a symbol
 */
void
db_symbol_values(sym, namep, valuep)
	db_sym_t	sym;
	char		**namep;
	db_expr_t	*valuep;
{
	db_expr_t	value;

	if (sym == DB_SYM_NULL) {
		*namep = 0;
		return;
	}

	X_db_symbol_values(sym, namep, &value);

	if (db_symbol_is_ambiguous(sym))
		*namep = db_qualify(sym, db_last_symtab->name);
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
extern char end[];
unsigned int	db_lastsym = (unsigned long)end;
db_expr_t db_maxoff = 0x10000000;


void
db_printsym(off, strategy)
	db_expr_t	off;
	db_strategy_t	strategy;
{
	db_expr_t	d;
	char 		*filename;
	char		*name;
	db_expr_t	value;
	int 		linenum;
	db_sym_t	cursym;

	if (off <= db_lastsym) {
		cursym = db_search_symbol(off, strategy, &d);
		db_symbol_values(cursym, &name, &value);
		if (name && (d < db_maxoff) && value) {
			db_printf("%s", name);
			if (d)
				db_printf("+%#r", d);
			if (strategy == DB_STGY_PROC) {
				if (db_line_at_pc(cursym, &filename, &linenum, off))
					db_printf(" [%s:%d]", filename, linenum);
			}
			return;
		}
	}
	db_printf("%#n", off);
	return;
}


boolean_t
db_line_at_pc( sym, filename, linenum, pc)
	db_sym_t	sym;
	char		**filename;
	int		*linenum;
	db_expr_t	pc;
{
	return X_db_line_at_pc( db_last_symtab, sym, filename, linenum, pc);
}

int
db_sym_numargs(sym, nargp, argnames)
	db_sym_t	sym;
	int		*nargp;
	char		**argnames;
{
	return X_db_sym_numargs(db_last_symtab, sym, nargp, argnames);
}
