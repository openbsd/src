/*	$OpenBSD: symbol.c,v 1.7 2003/03/28 23:33:27 mickey Exp $	*/
/*
 * Copyright (c) 2002 Artur Grabowski <art@openbsd.org>
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <errno.h>

#include "pmdb.h"
#include "symbol.h"

/*
 * Initialize the executable and the symbol table.
 */
void
sym_init_exec(struct pstate *ps, const char *name)
{
	ps->ps_sops = NULL;
	ps->ps_sym_exe = NULL;
	TAILQ_INIT(&ps->ps_syms);

#ifdef PMDB_ELF
	if (sym_check_elf(name, ps))
#endif
#ifdef PMDB_AOUT
	if (sym_check_aout(name, ps))
#endif
		warnx("sym_init_exec: %s is not a supported file format", name);

	if (ps->ps_sops) {
		/* XXX - this 0 doesn't have to be correct.. */
		ps->ps_sym_exe = st_open(ps, name, 0);
		if (ps->ps_sym_exe)
			ps->ps_sym_exe->st_flags |= ST_EXEC;
	}
}

/*
 * Destroy all symbol tables.
 */
void
sym_destroy(struct pstate *ps)
{
	struct sym_table *st;

	if (!(ps->ps_flags & PSF_SYMBOLS))
		return;
	while ((st = TAILQ_FIRST(&ps->ps_syms)) != NULL) {
		TAILQ_REMOVE(&ps->ps_syms, st, st_list);
		(*ps->ps_sops->sop_close)(st);
	}
	ps->ps_sym_exe = NULL;
}

/*
 * We have reasons to believe that the symbol tables we have are not consistent
 * with the running binary. Update.
 */
void
sym_update(struct pstate *ps)
{
	(*ps->ps_sops->sop_update)(ps);
}

char *
sym_name_and_offset(struct pstate *ps, reg pc, char *nam, size_t len, reg *off)
{
	struct sym_table *st;
	int bestoffisset = 0;
	reg bestoff, noff;
	char *res;

	TAILQ_FOREACH(st, &ps->ps_syms, st_list) {
		res = (*ps->ps_sops->sop_name_and_off)(st, pc, &noff);
		if (res == NULL)
			continue;
		if (noff < bestoff || !bestoffisset) {
			bestoffisset = 1;
			bestoff = noff;
			strlcpy(nam, res, len);
		}
	}

	if (!bestoffisset || !strcmp(nam, "_end"))
		return (NULL);

	*off = bestoff;
	return (nam);
}

int
sym_lookup(struct pstate *ps, const char *name, reg *res)
{
	/*
	 * We let the sop do the table walking itself since it might have
	 * preferences about what symbols to pick (weak and stuff).
	 */
	return ((*ps->ps_sops->sop_lookup)(ps, name, res));
}

char *
sym_print(struct pstate *ps, reg pc, char *buf, size_t buflen)
{
	char namebuf[1024], *name;
	reg offs;

	name = sym_name_and_offset(ps, pc, namebuf, sizeof(namebuf), &offs);
	if (name == NULL) {
		snprintf(buf, buflen, "0x%lx", pc);
	} else {
		if (offs)
			snprintf(buf, buflen, "%s+0x%lx(0x%lx)",
			    name, offs, pc);
		else
			snprintf(buf, buflen, "%s(0x%lx)", name, pc);
	}

	return (buf);
}

/*
 * Open a symbol table and install it in the list. Don't do anything if
 * it's already there.
 */
struct sym_table *
st_open(struct pstate *ps, const char *name, reg offs)
{
	struct sym_table *st;

	TAILQ_FOREACH(st, &ps->ps_syms, st_list) {
		if (!strcmp(name, st->st_fname) && (st->st_offs == offs))
			return (st);
	}

	warnx("Loading symbols from %s at 0x%lx", name, offs);

	if ((st = (*ps->ps_sops->sop_open)(name)) != NULL) {
		TAILQ_INSERT_TAIL(&ps->ps_syms, st, st_list);
		strlcpy(st->st_fname, name, sizeof(st->st_fname));
		st->st_offs = offs;
	}

	return (st);
}

/*
 * Load a symbol table from file argv[1] at offset argv[2].
 */
int
cmd_sym_load(int argc, char **argv, void *arg)
{
	struct pstate *ps = arg;
	char *fname, *ep;
	reg offs;

	fname = argv[1];
	errno = 0;
	offs = strtol(argv[2], &ep, 0);
	if (argv[2][0] == '\0' || *ep != '\0' || errno == ERANGE) {
		fprintf(stderr, "%s is not a valid offset\n", argv[2]);
		return (0);
	}

	if (st_open(ps, fname, offs) == NULL) {
		warnx("symbol loading failed");
	}

	return (0);
}
