/*	$OpenBSD: db_trace.c,v 1.2 2004/08/02 20:45:42 andreas Exp $	*/

/*
 * Copyright (c) 2004 Andreas Gunnarsson <andreas@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <machine/db_machdep.h>
#include <ddb/db_sym.h>
#include <ddb/db_access.h>
#include <ddb/db_interface.h>

extern char end[];
extern unsigned int db_maxoff;

db_addr_t db_findcaller(db_addr_t, db_addr_t, char **name, db_expr_t *);
int db_endtrace(db_addr_t);

db_addr_t
db_findcaller(db_addr_t sp, db_addr_t ip, char **name, db_expr_t *offset)
{
	db_addr_t previp;
	db_addr_t called_addr = 0;
	db_expr_t value;
	db_sym_t sym;
	unsigned char callinstr;
	int calloffs;
	short scalloffs;

	if (*(char **)sp < (char *)VM_MIN_KERNEL_ADDRESS || *(char **)sp >= end)
		return 0;

	sym = db_search_symbol((db_addr_t)*(char **)sp, DB_STGY_PROC, offset);
	db_symbol_values(sym, name, &value);
	if (!*name || *offset > db_maxoff || !value)
		return 0;

	/* Check for call with 32 bit relative displacement */
	/* e8 xx xx xx xx */
	previp = (db_addr_t)*(char **)sp - 5;
	callinstr = db_get_value(previp, 1, 0);
	if (callinstr == 0xe8) {
		calloffs = db_get_value(previp + 1, 4, 1);
		called_addr = previp + 5 + calloffs;
		goto found;
	}

	/* Check for call with 16 bit relative displacement */
	/* 66 e8 xx xx */
	previp = (db_addr_t)*(char **)sp - 4;
	callinstr = db_get_value(previp, 1, 0);
	if (callinstr == 0x66) {
		callinstr = db_get_value(previp + 1, 1, 0);
		if (callinstr == 0xe8) {
			scalloffs = db_get_value(previp + 2, 2, 1);
			called_addr = previp + 4 + scalloffs;
			goto found;
		}
	}

	/* Check for call with reg/mem */
	previp = (db_addr_t)*(char **)sp - 2;
	callinstr = db_get_value(previp, 1, 0);
	if (callinstr == 0xff) {
		callinstr = db_get_value(previp + 1, 1, 0);
		if (((callinstr & 0xf8) == 0x10 || (callinstr & 0xf8) == 0xd0) &&
		    callinstr != 0x15) {
			goto found;
		}
	}

	previp = (db_addr_t)*(char **)sp - 3;
	callinstr = db_get_value(previp, 1, 0);
	if (callinstr == 0xff) {
		callinstr = db_get_value(previp + 1, 1, 0);
		if ((callinstr & 0xf8) == 0x50) {
			goto found;
		}
	}

	previp = (db_addr_t)*(char **)sp - 6;
	callinstr = db_get_value(previp, 1, 0);
	if (callinstr == 0xff) {
		callinstr = db_get_value(previp + 1, 1, 0);
		if (callinstr == 0x15 || (callinstr & 0xf8) == 0x90) {
			goto found;
		}
	}

	/* Value on stack doesn't seem to be a return address */
	return 0;

found:
	if (called_addr) {
		sym = db_search_symbol(called_addr, DB_STGY_PROC, offset);
	} else {
		sym = db_search_symbol(ip, DB_STGY_PROC, offset);
		*offset = 0;
	}
	db_symbol_values(sym, name, &value);
	if (!*name || *offset > db_maxoff || !value)
		return 0;
	return previp;
}

int
db_endtrace(db_addr_t ip)
{
	db_expr_t offset, value;
	db_sym_t sym;
	char *name;

	sym = db_search_symbol(ip, DB_STGY_PROC, &offset);
	db_symbol_values(sym, &name, &value);
	if (!name || offset > db_maxoff || !value)
		return 1;
	if (!strcmp(name, "trap") ||
	    !strcmp(name, "syscall") ||
	    !strncmp(name, "Xintr", 5) ||
	    !strncmp(name, "Xresume", 7) ||
	    !strncmp(name, "Xstray", 6) ||
	    !strncmp(name, "Xhold", 5) ||
	    !strncmp(name, "Xrecurse", 8) ||
	    !strcmp(name, "Xdoreti") ||
	    !strncmp(name, "Xsoft", 5)) {
		return 1;
	}
	return 0;
}

void
db_stack_trace_print(db_expr_t addr, boolean_t have_addr, db_expr_t count,
    char *modif, int (*pr)(const char *, ...))
{
	db_addr_t ip, previp, sp;
	db_expr_t offset, value;
	db_sym_t sym;
	char *name;

	ip = ddb_regs.tf_rip;

	for (sp = ddb_regs.tf_rsp; !db_endtrace(ip); sp += 4) {
		if (*(char **)sp >= end)
			continue;
		previp = db_findcaller(sp, ip, &name, &offset);
		if (!previp || !name)
			continue;
		if (offset) {
			pr("[%s+%p", name, offset);
			pr(" called from ");
			db_printsym(previp, DB_STGY_PROC, pr);
			pr("]");
			continue;
		}
		pr("%s() at ", name);
		/* pr("%s(%%rsp=%p) at ", name, sp); */
		db_printsym(ip, DB_STGY_PROC, pr);
		pr("\n");
		ip = previp;
	}
	sym = db_search_symbol(ip, DB_STGY_PROC, &offset);
	db_symbol_values(sym, &name, &value);
	if (name)
		pr("%s() at ", name);
	db_printsym(ip, DB_STGY_PROC, pr);
	pr("\n");
}
