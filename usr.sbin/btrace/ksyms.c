/*	$OpenBSD: ksyms.c,v 1.1 2020/01/21 16:24:55 mpi Exp $ */

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

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <gelf.h>
#include <paths.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "btrace.h"

int		 kfd = -1;
Elf		*kelf;
Elf_Scn		*ksymtab;
size_t		 kstrtabndx, knsymb;

int		 kelf_parse(void);

int
kelf_open(void)
{
	int error;

	assert(kfd == -1);

	if (elf_version(EV_CURRENT) == EV_NONE)
		errx(1, "elf_version: %s", elf_errmsg(-1));

	kfd = open(_PATH_KSYMS, O_RDONLY);
	if (kfd == -1) {
		warn("open");
		return 1;
	}

	if ((kelf = elf_begin(kfd, ELF_C_READ, NULL)) == NULL) {
		warnx("elf_begin: %s", elf_errmsg(-1));
		error = 1;
		goto bad;
	}

	if (elf_kind(kelf) != ELF_K_ELF) {
		error = 1;
		goto bad;
	}

	error = kelf_parse();
	if (error)
		goto bad;

	return 0;

bad:
	kelf_close();
	return error;
}

void
kelf_close(void)
{
	elf_end(kelf);
	kelf = NULL;
	close(kfd);
	kfd = -1;
}

int
kelf_snprintsym(char *str, size_t size, unsigned long pc)
{
	GElf_Sym	 sym;
	Elf_Data	*data = NULL;
	Elf_Addr	 offset, bestoff = 0;
	size_t		 i, bestidx = 0;
	char		*name;
	int		 cnt;

	data = elf_rawdata(ksymtab, data);
	if (data == NULL)
		goto fallback;

	for (i = 0; i < knsymb; i++) {
		if (gelf_getsym(data, i, &sym) == NULL)
			continue;
		if (GELF_ST_TYPE(sym.st_info) != STT_FUNC)
			continue;
		if (pc >= sym.st_value) {
			if (pc < (sym.st_value + sym.st_size))
				break;
			/* Workaround for symbols w/o size, usually asm ones. */
			if (sym.st_size == 0 && sym.st_value > bestoff) {
				bestidx = i;
				bestoff = sym.st_value;
			}
		}
	}

	if (i == knsymb) {
		if (bestidx == 0 || gelf_getsym(data, bestidx, &sym) == NULL)
			goto fallback;
	}

	name = elf_strptr(kelf, kstrtabndx, sym.st_name);
	if (name != NULL)
		cnt = snprintf(str, size, "%s", name);
	else
		cnt = snprintf(str, size, "0x%llx", sym.st_value);

	offset = pc - sym.st_value;
	if (offset != 0)
		cnt += snprintf(str + cnt, size - cnt, "+0x%llx\n", offset);
	else
		cnt += snprintf(str + cnt, size - cnt, "\n");

	return cnt;

fallback:
	return snprintf(str, size, "0x%lx\n", pc);
}

int
kelf_parse(void)
{
	GElf_Shdr	 shdr;
	Elf_Scn		*scn, *scnctf;
	char		*name;
	size_t		 shstrndx;

	if (elf_getshdrstrndx(kelf, &shstrndx) != 0) {
		warnx("elf_getshdrstrndx: %s", elf_errmsg(-1));
		return 1;
	}

	scn = scnctf = NULL;
	while ((scn = elf_nextscn(kelf, scn)) != NULL) {
		if (gelf_getshdr(scn, &shdr) != &shdr) {
			warnx("elf_getshdr: %s", elf_errmsg(-1));
			return 1;
		}

		if ((name = elf_strptr(kelf, shstrndx, shdr.sh_name)) == NULL) {
			warnx("elf_strptr: %s", elf_errmsg(-1));
			return 1;
		}

		if (strcmp(name, ELF_SYMTAB) == 0 &&
		    shdr.sh_type == SHT_SYMTAB && shdr.sh_entsize != 0) {
			ksymtab = scn;
			knsymb = shdr.sh_size / shdr.sh_entsize;
		}

		if (strcmp(name, ELF_STRTAB) == 0 &&
		    shdr.sh_type == SHT_STRTAB) {
			kstrtabndx = elf_ndxscn(scn);
		}
	}

	if (ksymtab == NULL)
		warnx("symbol table not found");

	return 0;
}
