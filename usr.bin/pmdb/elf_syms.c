/*	$OpenBSD: elf_syms.c,v 1.9 2007/07/24 21:08:49 deraadt Exp $	*/
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <err.h>

#include <sys/param.h>
#include <sys/ptrace.h>
#include <sys/mman.h>

#include <nlist.h>
#ifdef __NetBSD__
#include <machine/elf_machdep.h>
#define ELFSIZE ARCH_ELFSIZE
#include <sys/exec_elf.h>
#else
#include <elf_abi.h>
#endif
#include <link.h>

#include "pmdb.h"
#include "symbol.h"
#include "break.h"

struct elf_symbol_handle {
	struct sym_table esh_st;
	int		esh_fd;
	char 		*esh_strtab;
	Elf_Word	esh_strsize;
	Elf_Sym 	*esh_symtab;
	Elf_Word	esh_symsize;
};

#define ESH_TO_ST(esh) (&(esh)->esh_st)
#define ST_TO_ESH(st) ((struct elf_symbol_handle *)(st))

struct sym_table *elf_open(const char *);
void elf_close(struct sym_table *);
char *elf_name_and_off(struct sym_table *, reg, reg *);
int elf_lookup(struct pstate *, const char *, reg *);
void elf_update(struct pstate *);

struct sym_ops elf_sops = {
	elf_open,
	elf_close,
	elf_name_and_off,
	elf_lookup,
	elf_update
};	

int
sym_check_elf(const char *name, struct pstate *ps)
{
	Elf_Ehdr ehdr;
	int error = 0;
	int fd;

	if ((fd = open(name, O_RDONLY)) < 0)
		return (1);

	if (pread(fd, &ehdr, sizeof(Elf_Ehdr), 0) != sizeof(Elf_Ehdr))
		error = 1;

#ifndef __NetBSD__
	if (!error && (!IS_ELF(ehdr) ||
	    ehdr.e_ident[EI_CLASS] != ELF_TARG_CLASS ||
	    ehdr.e_ident[EI_DATA] != ELF_TARG_DATA ||
	    ehdr.e_ident[EI_VERSION] != ELF_TARG_VER ||
	    ehdr.e_machine != ELF_TARG_MACH ||
	    ehdr.e_version != ELF_TARG_VER))
		error = 1;
#endif

	close(fd);

	if (!error)
		ps->ps_sops = &elf_sops;

	return (error);
}

struct sym_table *
elf_open(const char *name)
{
	struct elf_symbol_handle *esh;
	Elf_Off symoff, stroff;
	Elf_Ehdr ehdr;
	Elf_Shdr *shdr;
	int i, fd;

	/* Just a sanity check */
	if (sizeof(reg) != sizeof(Elf_Addr))
		errx(1, "sym_open: sizeof(reg) != sizeof(Elf_Addr)");

	if ((esh = malloc(sizeof(*esh))) == NULL) {
		return (NULL);
	}

	memset(esh, 0, sizeof(*esh));
	esh->esh_fd = -1;

	if ((fd = esh->esh_fd = open(name, O_RDONLY)) < 0) {
		goto fail;
	}

	if (pread(fd, &ehdr, sizeof(Elf_Ehdr), 0) != sizeof(Elf_Ehdr)) {
		goto fail;
	}
#ifndef __NetBSD__
	if (!IS_ELF(ehdr) ||
	    ehdr.e_ident[EI_CLASS] != ELF_TARG_CLASS ||
	    ehdr.e_ident[EI_DATA] != ELF_TARG_DATA ||
	    ehdr.e_ident[EI_VERSION] != ELF_TARG_VER ||
	    ehdr.e_machine != ELF_TARG_MACH ||
	    ehdr.e_version != ELF_TARG_VER) {
		goto fail;
	}
#endif

	if ((shdr = (Elf_Shdr *)mmap(NULL, ehdr.e_shentsize * ehdr.e_shnum,
	    PROT_READ, MAP_SHARED, fd, ehdr.e_shoff)) == MAP_FAILED) {
		goto fail;
	}

	for (i = 0; i < ehdr.e_shnum; i++) {
		if (shdr[i].sh_type == SHT_SYMTAB) {
			symoff = shdr[i].sh_offset;
			esh->esh_symsize = shdr[i].sh_size;
			stroff = shdr[shdr[i].sh_link].sh_offset;
			esh->esh_strsize = shdr[shdr[i].sh_link].sh_size;
			break;
		}
	}

	munmap(shdr, ehdr.e_shentsize * ehdr.e_shnum);

	if (i == ehdr.e_shnum) {
		goto fail;
	}

	if (esh->esh_symsize) {

		if ((esh->esh_strtab = mmap(NULL, esh->esh_strsize, PROT_READ,
		    MAP_SHARED, fd, stroff)) == MAP_FAILED) {
			goto fail;
		}

		if ((esh->esh_symtab = mmap(NULL, esh->esh_symsize, PROT_READ,
		    MAP_SHARED, fd, symoff)) == MAP_FAILED) {
			goto fail;
		}
	}

	return (ESH_TO_ST(esh));
fail:

	elf_close(ESH_TO_ST(esh));
	return (NULL);
}

void
elf_close(struct sym_table *st)
{
	struct elf_symbol_handle *esh = ST_TO_ESH(st);

	if (esh->esh_fd != -1)
		close(esh->esh_fd);

	munmap(esh->esh_strtab, esh->esh_strsize);
	munmap(esh->esh_symtab, esh->esh_symsize);
	free(esh);
}

char *
elf_name_and_off(struct sym_table *st, reg pc, reg *offs)
{
	struct elf_symbol_handle *esh = ST_TO_ESH(st);
	Elf_Sym *s, *bests = NULL;
	Elf_Addr bestoff = 0;
	int nsyms, i;
	char *symn;

	if (esh == NULL)
		return (NULL);

#define SYMVAL(S) (unsigned long)((S)->st_value + st->st_offs)

	nsyms = esh->esh_symsize / sizeof(Elf_Sym);

	bests = NULL;
	for (i = 0; i < nsyms; i++) {
		s = &esh->esh_symtab[i];

		if (s->st_value == 0 ||
		    s->st_shndx == 0 ||
		    (ELF_ST_BIND(s->st_info) != STB_GLOBAL &&
		     ELF_ST_BIND(s->st_info) != STB_WEAK &&
		     ELF_ST_BIND(s->st_info) != STB_LOCAL))
			continue;
		symn = &esh->esh_strtab[s->st_name];
		if (SYMVAL(s) <= pc && SYMVAL(s) > bestoff &&
		    symn[0] != '\0' && strcmp(symn, "gcc2_compiled.")) {
			bests = s;
			bestoff = SYMVAL(s);
		}
	}

	if ((s = bests) == NULL)
		return (NULL);

	*offs = pc - SYMVAL(s);

	return &esh->esh_strtab[s->st_name];
}

static Elf_Sym *
elf_lookup_table(struct elf_symbol_handle *esh, const char *name)
{
	int nsyms, i;
	char *symn;
	Elf_Sym *s = NULL;

	if (esh == NULL)
		return (NULL);

	/* XXX - dumb, doesn't follow the rules (weak, hash, etc.). */
	nsyms = esh->esh_symsize / sizeof(Elf_Sym);
	for (i = 0; i < nsyms; i++) {
		s = &esh->esh_symtab[i];
		symn = &esh->esh_strtab[s->st_name];
		if (strcmp(name, symn) == 0)
			break;
	}
	if (i == nsyms)
		return (NULL);

	return (s);
}

int
elf_lookup(struct pstate *ps, const char *name, reg *res)
{
	struct sym_table *st;
	Elf_Sym *s = NULL;

	TAILQ_FOREACH(st, &ps->ps_syms, st_list) {
		if ((s = elf_lookup_table(ST_TO_ESH(st), name)) != NULL)
			break;
	}

	if (s != NULL) {
		*res = s->st_value + st->st_offs;
		return (0);
	}

	return (-1);
}

#ifndef __NetBSD__
struct elf_object_v1 {
	Elf_Addr load_addr;
	Elf_Addr load_offs;
	char *load_name;
	Elf_Dyn *load_dyn;
	struct elf_object_v1 *next;
	struct elf_object_v1 *prev;
	void *load_list;
	u_int32_t load_size;
	u_long info[DT_NUM + DT_PROCNUM];
	struct elf_object_v1 *dep_next;
	int status;
	Elf_Phdr *phdrp;
	int phdrc;
	int refcount;
	int obj_type;
#define EOBJ1_LDR 1
#define EOBJ1_EXE 2
#define EOBJ1_LIB 3
#define EOBJ1_DLO 4
};
#endif

/*
 * dlopen breakpoint (XXX make this generic?)
 */
int
sym_bkpt(struct pstate *ps, void *arg)
{
	fprintf(stderr, "pmdb: shared lib changed\n");

	sym_update(ps);

	return BKPT_KEEP_CONT;
}

/* Is the ABI really so daft that it doesn't include the linking offset? */
struct xlink_map {
	struct link_map lm;
	Elf_Addr a;
};

/*
 * Called after execution started so that we can load any dynamic symbols.
 */
void
elf_update(struct pstate *ps)
{
#ifndef __NetBSD__
	struct xlink_map xlm;
#define lm xlm.lm
	struct r_debug rdeb;
	reg addr;
	Elf_Dyn dyn;
	static int bkpt_set;
	Elf_Sym *s;

	if ((s = elf_lookup_table(ST_TO_ESH(ps->ps_sym_exe), "_DYNAMIC")) == NULL) {
		warnx("Can't find _DYNAMIC");
		return;
	}
	addr = s->st_value + ps->ps_sym_exe->st_offs;

	do {
		if (process_read(ps, addr, &dyn, sizeof(dyn)) < 0) {
			warnx("Can't read _DYNAMIC");
			return;
		}
		addr += sizeof(dyn);
	} while (dyn.d_tag != 0 && dyn.d_tag != DT_DEBUG);

	if (dyn.d_tag == 0) {
		warnx("Can't find DT_DEBUG");
		return;
	}

	if (process_read(ps, dyn.d_un.d_ptr, &rdeb, sizeof(rdeb)) < 0) {
		warnx("Can't read DT_DEBUG");
		return;
	}

	if (rdeb.r_version != 1) {
		warn("Can't handle debug map version %d", rdeb.r_version);
		return;
	}
	if (rdeb.r_state != RT_CONSISTENT) {
		warn("debug map not consistent: %d", rdeb.r_state);
		return;
	}

	if (!bkpt_set) {
		if (bkpt_add_cb(ps, rdeb.r_brk, sym_bkpt, NULL))
			warn("sym_exec: can't set bkpt");
		bkpt_set = 1;
	}

	addr = (Elf_Addr)rdeb.r_map;
	while (addr != 0 && addr != -1) {
		char fname[MAXPATHLEN];
		int i;

		if (process_read(ps, addr, &xlm, sizeof(xlm)) < 0) {
			warnx("Can't read symbols...");
			return;
		}

		addr = (Elf_Addr)lm.l_next;

		if (lm.l_name == NULL || lm.l_name == (char *)-1)
			continue;
		if (process_read(ps, (Elf_Addr)lm.l_name, fname,
		    sizeof(fname)) < 0) {
			warnx("Can't read symbols...");
			return;
		}

		/* sanity check the file name */
		for (i = 0; i < MAXPATHLEN; i++)
			if (fname[i] == '\0')
				break;
		if (i == MAXPATHLEN)
			continue;

		if (st_open(ps, fname, xlm.a) == NULL)
			warn("symbol loading failed");
	}
#endif
}
