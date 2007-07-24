/*	$OpenBSD: aout_syms.c,v 1.11 2007/07/24 21:11:02 deraadt Exp $	*/
/*
 * Copyright (c) 2002 Federico Schwindt <fgsch@openbsd.org>
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

#include <sys/param.h>
#include <sys/ptrace.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <err.h>

#include <a.out.h>
#include <link.h>

#include "pmdb.h"
#include "symbol.h"

struct aout_symbol_handle {
	struct sym_table	ash_st;
	int		ash_fd;
	char	       *ash_strtab;
	u_int32_t	ash_strsize;
	struct nlist   *ash_symtab;
	int		ash_symsize;
};

#define ASH_TO_ST(ash) (&(ash)->ash_st)
#define ST_TO_ASH(st) ((struct aout_symbol_handle *)(st))

struct sym_table *aout_open(const char *);
void aout_close(struct sym_table *);
char *aout_name_and_off(struct sym_table *, reg, reg *);
int aout_lookup(struct pstate *, const char *, reg *);
void aout_update(struct pstate *);

struct sym_ops aout_sops = {
	aout_open,
	aout_close,
	aout_name_and_off,
	aout_lookup,
	aout_update
};	

int
sym_check_aout(const char *name, struct pstate *ps)
{
	struct exec ahdr;
	int error = 0;
	int fd;

	if ((fd = open(name, O_RDONLY)) < 0)
		return (1);

	if (pread(fd, &ahdr, sizeof(ahdr), 0) != sizeof(ahdr)) {
		error = 1;
	}

	if (!error && N_BADMAG(ahdr)) {
		error = 1;
	}

	close(fd);

	if (!error)
		ps->ps_sops = &aout_sops;

	return (error);
}

struct sym_table *
aout_open(const char *name)
{
	struct aout_symbol_handle *ash;
	struct stat sb;
	u_int32_t symoff, stroff;
	struct exec ahdr;

	if ((ash = malloc(sizeof(*ash))) == NULL) {
		return NULL;
	}

	memset(ash, 0, sizeof(*ash));
	ash->ash_fd = -1;

	if ((ash->ash_fd = open(name, O_RDONLY)) < 0) {
		warn("open(%s)", name);
		goto fail;
	}

	if (pread(ash->ash_fd, &ahdr, sizeof(ahdr), 0) != sizeof(ahdr)) {
		warn("pread(header)");
		goto fail;
	}

	if (N_BADMAG(ahdr)) {
		warnx("Bad magic.");
		goto fail;
	}

	/* Don't go further for stripped files. */
	if (fstat(ash->ash_fd, &sb) < 0 || N_SYMOFF(ahdr) == sb.st_size ||
	    N_STROFF(ahdr) == sb.st_size)
		goto fail;

	symoff = N_SYMOFF(ahdr);
	ash->ash_symsize = ahdr.a_syms;
	stroff = N_STROFF(ahdr);

	if (ahdr.a_syms == 0) {
		warnx("No symbol table");
		goto fail;
	}

	if (pread(ash->ash_fd, &ash->ash_strsize, sizeof(u_int32_t),
	    stroff) != sizeof(u_int32_t)) {
		warn("pread(strsize)");
		goto fail;
	}

	if ((ash->ash_strtab = mmap(NULL, ash->ash_strsize, PROT_READ,
	    MAP_SHARED, ash->ash_fd, stroff)) == MAP_FAILED) {
		warn("mmap(strtab)");
		goto fail;
	}

	if ((ash->ash_symtab = mmap(NULL, ash->ash_symsize, PROT_READ,
	    MAP_SHARED, ash->ash_fd, symoff)) == MAP_FAILED) {
		warn("mmap(symtab)");
		goto fail;
	}

	return (ASH_TO_ST(ash));
fail:

	aout_close(ASH_TO_ST(ash));
	return (NULL);
}

void
aout_close(struct sym_table *st)
{
	struct aout_symbol_handle *ash = ST_TO_ASH(st);

	if (ash->ash_fd != -1)
		close(ash->ash_fd);

	munmap(ash->ash_strtab, ash->ash_strsize);
	munmap(ash->ash_symtab, ash->ash_symsize);
	free(ash);
}

char *
aout_name_and_off(struct sym_table *st, reg pc, reg *offs)
{
	struct aout_symbol_handle *ash = ST_TO_ASH(st);
	struct nlist *s, *bests = NULL;
	int bestoff = 0;
	int nsyms, i;
	char *symn;

#define SYMVAL(S) (unsigned long)((S)->n_value + st->st_offs)

	nsyms = ash->ash_symsize / sizeof(struct nlist);

	bests = NULL;
	for (i = 0; i < nsyms; i++) {
		s = &ash->ash_symtab[i];

		if (s->n_value == 0 ||
		    s->n_un.n_strx == 0)
			continue;

		symn = &ash->ash_strtab[s->n_un.n_strx];
		if (SYMVAL(s) <= pc && SYMVAL(s) > bestoff &&
		    symn[0] != '\0' && strcmp(symn, "gcc2_compiled.")) {
			bests = s;
			bestoff = SYMVAL(s);
		}
	}

	if ((s = bests) == NULL)
		return (NULL);

	*offs = pc - SYMVAL(s);

	return &ash->ash_strtab[s->n_un.n_strx];
}

static struct nlist *
aout_lookup_table(struct aout_symbol_handle *ash, const char *name)
{
	int nsyms, i;
	char *symn;
	struct nlist *s = NULL;

	nsyms = ash->ash_symsize / sizeof(struct nlist);
	for (i = 0; i < nsyms; i++) {
		s = &ash->ash_symtab[i];
		symn = &ash->ash_strtab[s->n_un.n_strx];
		if (strcmp(name, symn) == 0)
			break;
	}

	if (i == nsyms)
		return (NULL);

	return (s);
}

int
aout_lookup(struct pstate *ps, const char *name, reg *res)
{
	struct sym_table *st;
	struct nlist *s = NULL;
	int first = 1;
	char *sname = (char *)name;

restart:
	TAILQ_FOREACH(st, &ps->ps_syms, st_list) {
		if ((s = aout_lookup_table(ST_TO_ASH(st), sname)) != NULL)
			break;
	}

	if (!first)
		free(sname);

	if (s == NULL) {
		if (first) {
			if (asprintf(&sname, "_%s", sname) != -1) {
				first = 0;
				goto restart;
			}
		}

		return (-1);
	}

	*res = s->n_value + st->st_offs;
	return (0);
}

/*
 * Called after execution started so that we can load any dynamic symbols.
 */
void
aout_update(struct pstate *ps)
{
	struct _dynamic dyn;
	struct so_debug sdeb;
	struct section_dispatch_table sdt;
	struct so_map som;
	off_t somp;
	reg addr;
	struct nlist *s;

	if ((s = aout_lookup_table(ST_TO_ASH(ps->ps_sym_exe), "__DYNAMIC")) == NULL) {
		warnx("Can't find __DYNAMIC");
		return;
	}
	addr = s->n_value + ps->ps_sym_exe->st_offs;

	if (process_read(ps, addr, &dyn, sizeof(dyn)) < 0) {
		warn("Can't read __DYNAMIC");
		return;
	}

	if (dyn.d_version != LD_VERSION_BSD) {
		warn("Can't handle __DYNAMIC version %d", dyn.d_version);
		return;
	}

	if (process_read(ps, (off_t)(reg)dyn.d_debug, &sdeb, sizeof(sdeb)) < 0) {
		warn("Can't read __DYNAMIC.d_debug");
		return;
	}

	if (sdeb.dd_version != 0) {
		warn("Can't handle so_debug version %d", sdeb.dd_version);
		return;
	}

	if (process_read(ps, (off_t)(reg)dyn.d_un.d_sdt, &sdt, sizeof(sdt)) < 0) {
		warn("Can't read section dispatch table");
		return;
	}

	somp = (off_t)(reg)sdt.sdt_loaded;
	while (somp) {
		char fname[MAXPATHLEN];
		int i;

		if (process_read(ps, somp, &som, sizeof(som)) < 0) {
			warn("Can't read so_map");
			break;
		}
		somp = (off_t)(reg)som.som_next;
		if (process_read(ps, (off_t)(reg)som.som_path, fname,
		    sizeof(fname)) < 0) {
			warn("Can't read so filename");
			continue;
		}
		
		/* sanity check the file name */
		for (i = 0; i < MAXPATHLEN; i++)
			if (fname[i] == '\0')
				break;
		if (i == MAXPATHLEN) {
			warnx("so filename invalid");
			continue;
		}

		if (st_open(ps, fname, (reg)som.som_addr) == NULL)
			warn("symbol loading failed");
	}
}
