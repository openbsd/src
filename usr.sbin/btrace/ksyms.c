/*	$OpenBSD: ksyms.c,v 1.6 2023/11/10 18:56:21 jasper Exp $ */

/*
 * Copyright (c) 2016 Martin Pieuchot <mpi@openbsd.org>
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

#define _DYN_LOADER	/* needed for AuxInfo */

#include <sys/types.h>

#include <err.h>
#include <fcntl.h>
#include <gelf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "btrace.h"

struct syms {
	int		 fd;
	Elf		*elf;
	Elf_Scn		*symtab;
	size_t		 strtabndx, nsymb;
};

int		 kelf_parse(struct syms *);

struct syms *
kelf_open(const char *path)
{
	struct syms *syms;
	int error;

	if (elf_version(EV_CURRENT) == EV_NONE)
		errx(1, "elf_version: %s", elf_errmsg(-1));

	if ((syms = calloc(1, sizeof(*syms))) == NULL)
		err(1, NULL);

	syms->fd = open(path, O_RDONLY);
	if (syms->fd == -1) {
		warn("open: %s", path);
		free(syms);
		return NULL;
	}

	if ((syms->elf = elf_begin(syms->fd, ELF_C_READ, NULL)) == NULL) {
		warnx("elf_begin: %s", elf_errmsg(-1));
		goto bad;
	}

	if (elf_kind(syms->elf) != ELF_K_ELF)
		goto bad;

	error = kelf_parse(syms);
	if (error)
		goto bad;

	return syms;

bad:
	kelf_close(syms);
	return NULL;
}

void
kelf_close(struct syms *syms)
{
	if (syms == NULL)
		return;
	elf_end(syms->elf);
	close(syms->fd);
	free(syms);
}

int
kelf_snprintsym(struct syms *syms, char *str, size_t size, unsigned long pc,
    unsigned long off)
{
	GElf_Sym	 sym;
	Elf_Data	*data = NULL;
	Elf_Addr	 offset, bestoff = 0;
	size_t		 i, bestidx = 0;
	char		*name;
	int		 cnt;

	if (syms == NULL)
		goto fallback;

	data = elf_rawdata(syms->symtab, data);
	if (data == NULL)
		goto fallback;

	for (i = 0; i < syms->nsymb; i++) {
		if (gelf_getsym(data, i, &sym) == NULL)
			continue;
		if (GELF_ST_TYPE(sym.st_info) != STT_FUNC)
			continue;
		if (pc >= sym.st_value + off) {
			if (pc < (sym.st_value + off + sym.st_size))
				break;
			/* Workaround for symbols w/o size, usually asm ones. */
			if (sym.st_size == 0 && sym.st_value + off > bestoff) {
				bestidx = i;
				bestoff = sym.st_value + off;
			}
		}
	}

	if (i == syms->nsymb) {
		if (bestidx == 0 || gelf_getsym(data, bestidx, &sym) == NULL)
			goto fallback;
	}

	name = elf_strptr(syms->elf, syms->strtabndx, sym.st_name);
	if (name != NULL)
		cnt = snprintf(str, size, "\n%s", name);
	else
		cnt = snprintf(str, size, "\n0x%llx", sym.st_value);
	if (cnt < 0)
		return cnt;

	offset = pc - (sym.st_value + off);
	if (offset != 0) {
		int l;

		l = snprintf(str + cnt, size > (size_t)cnt ? size - cnt : 0,
		    "+0x%llx", (unsigned long long)offset);
		if (l < 0)
			return l;
		cnt += l;
	}

	return cnt;

fallback:
	return snprintf(str, size, "\n0x%lx", pc);
}

int
kelf_parse(struct syms *syms)
{
	GElf_Shdr	 shdr;
	Elf_Scn		*scn, *scnctf;
	char		*name;
	size_t		 shstrndx;

	if (elf_getshdrstrndx(syms->elf, &shstrndx) != 0) {
		warnx("elf_getshdrstrndx: %s", elf_errmsg(-1));
		return 1;
	}

	scn = scnctf = NULL;
	while ((scn = elf_nextscn(syms->elf, scn)) != NULL) {
		if (gelf_getshdr(scn, &shdr) != &shdr) {
			warnx("elf_getshdr: %s", elf_errmsg(-1));
			return 1;
		}

		if ((name = elf_strptr(syms->elf, shstrndx,
		    shdr.sh_name)) == NULL) {
			warnx("elf_strptr: %s", elf_errmsg(-1));
			return 1;
		}

		if (strcmp(name, ELF_SYMTAB) == 0 &&
		    shdr.sh_type == SHT_SYMTAB && shdr.sh_entsize != 0) {
			syms->symtab = scn;
			syms->nsymb = shdr.sh_size / shdr.sh_entsize;
		}

		if (strcmp(name, ELF_STRTAB) == 0 &&
		    shdr.sh_type == SHT_STRTAB) {
			syms->strtabndx = elf_ndxscn(scn);
		}
	}

	if (syms->symtab == NULL)
		warnx("symbol table not found");

	return 0;
}
