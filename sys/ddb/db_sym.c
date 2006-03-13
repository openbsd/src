/*	$OpenBSD: db_sym.c,v 1.32 2006/03/13 06:23:20 jsg Exp $	*/
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
#include <sys/proc.h>
#include <sys/systm.h>

#include <machine/db_machdep.h>

#include <ddb/db_lex.h>
#include <ddb/db_sym.h>
#include <ddb/db_output.h>
#include <ddb/db_extern.h>
#include <ddb/db_command.h>

/*
 * Multiple symbol tables
 */
#ifndef MAXLKMS
#define MAXLKMS 20
#endif

#ifndef MAXNOSYMTABS
#define	MAXNOSYMTABS	MAXLKMS+1	/* Room for kernel + LKM's */
#endif

db_symtab_t	db_symtabs[MAXNOSYMTABS] = {{0,},};

db_symtab_t	*db_last_symtab;

static db_forall_func_t db_sift;

extern char end[];

/*
 * Put the most picky symbol table formats at the top!
 */
const db_symformat_t *db_symformats[] = {
#ifdef DB_ELF_SYMBOLS
	&db_symformat_elf,
#endif
#ifdef DB_AOUT_SYMBOLS
	&db_symformat_aout,
#endif
	NULL,
};

const db_symformat_t *db_symformat;

boolean_t	X_db_sym_init(int, void *, void *, const char *);
db_sym_t	X_db_lookup(db_symtab_t *, char *);
db_sym_t	X_db_search_symbol(db_symtab_t *, db_addr_t,
		    db_strategy_t, db_expr_t *);
void		X_db_symbol_values(db_symtab_t *, db_sym_t, char **,
		    db_expr_t *);
boolean_t	X_db_line_at_pc(db_symtab_t *, db_sym_t, char **,
		    int *, db_expr_t);
int		X_db_sym_numargs(db_symtab_t *, db_sym_t, int *,
		    char **);

/*
 * Initialize the kernel debugger by initializing the master symbol
 * table.  Note that if initializing the master symbol table fails,
 * no other symbol tables can be loaded.
 */
#if 0
void
ddb_init(int symsize, void *vss, void *vse)
{
	const db_symformat_t **symf;
	const char *name = "bsd";

	if (symsize <= 0) {
		printf(" [ no symbols available ]\n");
		return;
	}

	/*
	 * Do this check now for the master symbol table to avoid printing
	 * the message N times.
	 */
	if (ALIGNED_POINTER(vss, long) == 0) {
		printf("[ %s symbol table has bad start address %p ]\n",
		    name, vss);
		return;
	}

	for (symf = db_symformats; *symf != NULL; symf++) {
		db_symformat = *symf;
		if (X_db_sym_init(symsize, vss, vse, name) == TRUE)
			return;
	}

	db_symformat = NULL;
	printf("[ no symbol table formats found ]\n");
}
#else
void
ddb_init(void)
{
	const db_symformat_t **symf;
	const char *name = "bsd";
	extern char *esym;
#if defined(__sparc64__) || defined(__mips__)
	extern char *ssym;
#endif
	char *xssym, *xesym;

	xesym = esym;
#if defined(__sparc64__) || defined(__mips__)
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

	if (xesym != NULL && xesym != xssym)
		for (symf = db_symformats; *symf != NULL; symf++) {
			db_symformat = *symf;
			if (X_db_sym_init((vaddr_t)xesym - (vaddr_t)xssym,
			    xssym, xesym, name) == TRUE)
			return;
		}

	db_symformat = NULL;
	printf("[ no symbol table formats found ]\n");
}
#endif

/*
 * Add symbol table, with given name, to list of symbol tables.
 */
int
db_add_symbol_table(char *start, char *end, const char *name, char *ref)
{
	int slot;

	for (slot = 0; slot < MAXNOSYMTABS; slot++) {
		if (db_symtabs[slot].name == NULL)
			break;
	}
	if (slot >= MAXNOSYMTABS) {
		db_printf("No slots left for %s symbol table", name);
		return(-1);
	}

	db_symtabs[slot].start = start;
	db_symtabs[slot].end = end;
	db_symtabs[slot].name = name;
	db_symtabs[slot].private = ref;

	return(slot);
}

/*
 * Delete a symbol table. Caller is responsible for freeing storage.
 */
void
db_del_symbol_table(char *name)
{
	int slot;

	for (slot = 0; slot < MAXNOSYMTABS; slot++) {
		if (db_symtabs[slot].name &&
		    ! strcmp(db_symtabs[slot].name, name))
			break;
	}
	if (slot >= MAXNOSYMTABS) {
		db_printf("Unable to find symbol table slot for %s.", name);
		return;
	}

	db_symtabs[slot].start = 0;
	db_symtabs[slot].end = 0;
	db_symtabs[slot].name = 0;
	db_symtabs[slot].private = 0;
}

/*
 *  db_qualify("vm_map", "bsd") returns "bsd:vm_map".
 *
 *  Note: return value points to static data whose content is
 *  overwritten by each call... but in practice this seems okay.
 */
char *
db_qualify(db_sym_t sym, const char *symtabname)
{
	char		*symname;
	static char     tmp[256];
	char	*s;

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
db_lookup(char *symstr)
{
	db_sym_t sp;
	int i;
	int symtab_start = 0;
	int symtab_end = MAXNOSYMTABS;
	char *cp;

	/*
	 * Look for, remove, and remember any symbol table specifier.
	 */
	for (cp = symstr; *cp; cp++) {
		if (*cp == ':') {
			*cp = '\0';
			for (i = 0; i < MAXNOSYMTABS; i++) {
				if (db_symtabs[i].name &&
				    ! strcmp(symstr, db_symtabs[i].name)) {
					symtab_start = i;
					symtab_end = i + 1;
					break;
				}
			}
			*cp = ':';
			if (i == MAXNOSYMTABS) {
				db_error("invalid symbol table name");
				/*NOTREACHED*/
			}
			symstr = cp+1;
		}
	}

	/*
	 * Look in the specified set of symbol tables.
	 * Return on first match.
	 */
	for (i = symtab_start; i < symtab_end; i++) {
		if (db_symtabs[i].name && 
		    (sp = X_db_lookup(&db_symtabs[i], symstr))) {
			db_last_symtab = &db_symtabs[i];
			return sp;
		}
	}
	return 0;
}

/* Private structure for passing args to db_sift() from db_sifting(). */
struct db_sift_args {
	char	*symstr;
	int	mode;
};

/*
 * Does the work of db_sifting(), called once for each
 * symbol via X_db_forall(), prints out symbols matching
 * criteria.
 */
static void
db_sift(db_symtab_t *stab, db_sym_t sym, char *name, char *suffix, int prefix,
    void *arg)
{
	char c, sc;
	char *find, *p;
	size_t len;
	struct db_sift_args *dsa;

	dsa = (struct db_sift_args*)arg;

	find = dsa->symstr;	/* String we're looking for. */
	p = name;		/* String we're searching within. */
	
	/* Matching algorithm cribbed from strstr(), which is not
	   in the kernel. */
	if ((c = *find++) != 0) {
		len = strlen(find);
		do {
			do {
				if ((sc = *p++) == 0)
					return;
			} while (sc != c);
		} while (strncmp(p, find, len) != 0);
	}
	if (dsa->mode=='F')	/* ala ls -F */
		db_printf("%s%s ", name, suffix);
	else
		db_printf("%s ", name);
}

/*
 * "Sift" for a partial symbol.
 * Named for the Sun OpenPROM command ("sifting").
 * If the symbol has a qualifier (e.g., ux:vm_map),
 * then only the specified symbol table will be searched;
 * otherwise, all symbol tables will be searched..
 *
 * "mode" is how-to-display, set from modifiers.
 */
void
db_sifting(char *symstr, int mode)
{
	char *cp;
	int i;
	int symtab_start = 0;
	int symtab_end = MAXNOSYMTABS;
	struct db_sift_args dsa;

	/*
	 * Look for, remove, and remember any symbol table specifier.
	 */
	for (cp = symstr; *cp; cp++) {
		if (*cp == ':') {
			*cp = '\0';
			for (i = 0; i < MAXNOSYMTABS; i++) {
				if (db_symtabs[i].name &&
				    ! strcmp(symstr, db_symtabs[i].name)) {
					symtab_start = i;
					symtab_end = i + 1;
					break;
				}
			}
			*cp = ':';
			if (i == MAXNOSYMTABS) {
				db_error("invalid symbol table name");
				/*NOTREACHED*/
			}
			symstr = cp+1;
		}
	}

	/* Pass args to db_sift(). */
	dsa.symstr = symstr;
	dsa.mode = mode;

	/*
	 * Look in the specified set of symbol tables.
	 */
	for (i = symtab_start; i < symtab_end; i++)
		if (db_symtabs[i].name) {
			db_printf("Sifting table %s:\n", db_symtabs[i].name);
			X_db_forall(&db_symtabs[i], db_sift, &dsa);
		}

	return;
}


/*
 * Does this symbol name appear in more than one symbol table?
 * Used by db_symbol_values to decide whether to qualify a symbol.
 */
boolean_t db_qualify_ambiguous_names = FALSE;

boolean_t
db_symbol_is_ambiguous(db_sym_t sym)
{
	char		*sym_name;
	int	i;
	boolean_t	found_once = FALSE;

	if (!db_qualify_ambiguous_names)
		return FALSE;

	db_symbol_values(sym, &sym_name, 0);
	for (i = 0; i < MAXNOSYMTABS; i++) {
		if (db_symtabs[i].name &&
		    X_db_lookup(&db_symtabs[i], sym_name)) {
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
db_search_symbol(db_addr_t val, db_strategy_t strategy, db_expr_t *offp)
{
	unsigned int	diff;
	db_expr_t	newdiff;
	int		i;
	db_sym_t	ret = DB_SYM_NULL, sym;

	newdiff = diff = ~0;
	db_last_symtab = 0;
	for (i = 0; i < MAXNOSYMTABS; i++) {
	    if (!db_symtabs[i].name)
	        continue;
	    sym = X_db_search_symbol(&db_symtabs[i], val, strategy, &newdiff);
	    if (newdiff < diff) {
		db_last_symtab = &db_symtabs[i];
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
db_symbol_values(db_sym_t sym, char **namep, db_expr_t *valuep)
{
	db_expr_t	value;

	if (sym == DB_SYM_NULL) {
		*namep = 0;
		return;
	}

	X_db_symbol_values(db_last_symtab, sym, namep, &value);

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
			if (strategy == DB_STGY_PROC) {
				if (db_line_at_pc(cursym, &filename, &linenum, off))
					(*pr)(" [%s:%d]", filename, linenum);
			}
			return;
		}
	}

	(*pr)("%s", db_format(buf, sizeof(buf), off, DB_FORMAT_N, 1, 0));
	return;
}


boolean_t
db_line_at_pc(db_sym_t sym, char **filename, int *linenum, db_expr_t pc)
{
	return X_db_line_at_pc(db_last_symtab, sym, filename, linenum, pc);
}

int
db_sym_numargs(db_sym_t sym, int *nargp, char **argnames)
{
	return X_db_sym_numargs(db_last_symtab, sym, nargp, argnames);
}

boolean_t
X_db_sym_init(int symsize, void *vss, void *vse, const char *name)
{

	if (db_symformat != NULL)
		return ((*db_symformat->sym_init)(symsize, vss, vse, name));
	return (FALSE);
}

db_sym_t
X_db_lookup(db_symtab_t *stab, char *symstr)
{

	if (db_symformat != NULL)
		return ((*db_symformat->sym_lookup)(stab, symstr));
	return ((db_sym_t)0);
}

db_sym_t
X_db_search_symbol(db_symtab_t *stab, db_addr_t off, db_strategy_t strategy,
    db_expr_t *diffp)
{

	if (db_symformat != NULL)
		return ((*db_symformat->sym_search)(stab, off, strategy,
		    diffp));
	return ((db_sym_t)0);
}

void
X_db_symbol_values(db_symtab_t *stab, db_sym_t sym, char **namep,
    db_expr_t *valuep)
{

	if (db_symformat != NULL) 
		(*db_symformat->sym_value)(stab, sym, namep, valuep);
}

boolean_t
X_db_line_at_pc(db_symtab_t *stab, db_sym_t cursym, char **filename,
    int *linenum, db_expr_t off)
{

	if (db_symformat != NULL)
		return ((*db_symformat->sym_line_at_pc)(stab, cursym,
		    filename, linenum, off));
	return (FALSE);
}

boolean_t
X_db_sym_numargs(db_symtab_t *stab, db_sym_t cursym, int *nargp,
    char **argnamep)
{

	if (db_symformat != NULL)
		return ((*db_symformat->sym_numargs)(stab, cursym, nargp,
		    argnamep));
	return (FALSE);
}

void
X_db_forall(db_symtab_t *stab, db_forall_func_t db_forall_func, void *arg)
{
	if (db_symformat != NULL)
		(*db_symformat->sym_forall)(stab, db_forall_func, arg);
}
