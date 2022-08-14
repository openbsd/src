/*	$OpenBSD: elf.c,v 1.10 2022/08/14 14:54:13 millert Exp $ */

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

#include <sys/types.h>

#include <machine/reloc.h>

#include <assert.h>
#include <elf.h>
#include <err.h>
#include <string.h>

static int	elf_reloc_size(unsigned long);
static void	elf_reloc_apply(const char *, size_t, const char *, size_t,
		    ssize_t, char *, size_t);

int
iself(const char *p, size_t filesize)
{
	Elf_Ehdr		*eh = (Elf_Ehdr *)p;

	if (filesize < sizeof(Elf_Ehdr)) {
		warnx("file too small to be ELF");
		return 0;
	}

	if (eh->e_ehsize < sizeof(Elf_Ehdr) || !IS_ELF(*eh))
		return 0;

	if (eh->e_ident[EI_CLASS] != ELFCLASS) {
		warnx("unexpected word size %u", eh->e_ident[EI_CLASS]);
		return 0;
	}
	if (eh->e_ident[EI_VERSION] != ELF_TARG_VER) {
		warnx("unexpected version %u", eh->e_ident[EI_VERSION]);
		return 0;
	}
	if (eh->e_ident[EI_DATA] >= ELFDATANUM) {
		warnx("unexpected data format %u", eh->e_ident[EI_DATA]);
		return 0;
	}
	if (eh->e_shoff > filesize) {
		warnx("bogus section table offset 0x%llx",
		    (unsigned long long)eh->e_shoff);
		return 0;
	}
	if (eh->e_shentsize < sizeof(Elf_Shdr)) {
		warnx("bogus section header size %u", eh->e_shentsize);
		return 0;
	}
	if (eh->e_shnum > (filesize - eh->e_shoff) / eh->e_shentsize) {
		warnx("bogus section header count %u", eh->e_shnum);
		return 0;
	}
	if (eh->e_shstrndx >= eh->e_shnum) {
		warnx("bogus string table index %u", eh->e_shstrndx);
		return 0;
	}

	return 1;
}

int
elf_getshstab(const char *p, size_t filesize, const char **shstab,
    size_t *shstabsize)
{
	Elf_Ehdr		*eh = (Elf_Ehdr *)p;
	Elf_Shdr		*sh;
	size_t			 shoff;

	shoff = eh->e_shoff + eh->e_shstrndx * eh->e_shentsize;
	if (shoff > (filesize - sizeof(*sh))) {
		warnx("unexpected string table size");
		return -1;
	}

	sh = (Elf_Shdr *)(p + shoff);
	if (sh->sh_type != SHT_STRTAB) {
		warnx("unexpected string table type");
		return -1;
	}
	if (sh->sh_offset > filesize) {
		warnx("bogus string table offset");
		return -1;
	}
	if (sh->sh_size > filesize - sh->sh_offset) {
		warnx("bogus string table size");
		return -1;
	}
	if (shstab != NULL)
		*shstab = p + sh->sh_offset;
	if (shstabsize != NULL)
		*shstabsize = sh->sh_size;

	return 0;
}

ssize_t
elf_getsymtab(const char *p, size_t filesize, const char *shstab,
    size_t shstabsz, const Elf_Sym **symtab, size_t *nsymb, const char **strtab,
    size_t *strtabsz)
{
	Elf_Ehdr	*eh = (Elf_Ehdr *)p;
	Elf_Shdr	*sh, *symsh;
	size_t		 snlen, shoff;
	ssize_t		 i;

	snlen = strlen(ELF_SYMTAB);
	symsh = NULL;

	for (i = 0; i < eh->e_shnum; i++) {
		shoff = eh->e_shoff + i * eh->e_shentsize;
		if (shoff > (filesize - sizeof(*sh)))
			continue;

		sh = (Elf_Shdr *)(p + shoff);
		if (sh->sh_type != SHT_SYMTAB)
			continue;

		if ((sh->sh_link >= eh->e_shnum) || (sh->sh_name >= shstabsz))
			continue;

		if (sh->sh_offset > filesize)
			continue;

		if (sh->sh_size > (filesize - sh->sh_offset))
			continue;

		if (sh->sh_entsize == 0)
			continue;

		if (strncmp(shstab + sh->sh_name, ELF_SYMTAB, snlen) == 0) {
			if (symtab != NULL)
				*symtab = (Elf_Sym *)(p + sh->sh_offset);
			if (nsymb != NULL)
				*nsymb = (sh->sh_size / sh->sh_entsize);
			symsh = sh;

			break;
		}
	}

	if (symsh == NULL || (symsh->sh_link >= eh->e_shnum))
		return -1;

	shoff = eh->e_shoff + symsh->sh_link * eh->e_shentsize;
	if (shoff > (filesize - sizeof(*sh)))
		return -1;

	sh = (Elf_Shdr *)(p + shoff);
	if ((sh->sh_offset + sh->sh_size) > filesize)
		return -1;

	if (strtab != NULL)
		*strtab = p + sh->sh_offset;
	if (strtabsz != NULL)
		*strtabsz = sh->sh_size;

	return i;
}

ssize_t
elf_getsection(char *p, size_t filesize, const char *sname, const char *shstab,
    size_t shstabsz, const char **psdata, size_t *pssz)
{
	Elf_Ehdr	*eh = (Elf_Ehdr *)p;
	Elf_Shdr	*sh;
	char		*sdata = NULL;
	size_t		 snlen, shoff, ssz = 0;
	ssize_t		 sidx, i;

	snlen = strlen(sname);
	if (snlen == 0)
		return -1;

	/* Find the given section. */
	for (i = 0; i < eh->e_shnum; i++) {
		shoff = eh->e_shoff + i * eh->e_shentsize;
		if (shoff > (filesize - sizeof(*sh)))
			continue;

		sh = (Elf_Shdr *)(p + shoff);
		if ((sh->sh_link >= eh->e_shnum) || (sh->sh_name >= shstabsz))
			continue;

		if (sh->sh_offset > filesize)
			continue;

		if (sh->sh_size > (filesize - sh->sh_offset))
			continue;

		if (strncmp(shstab + sh->sh_name, sname, snlen) == 0) {
			sidx = i;
			sdata = p + sh->sh_offset;
			ssz = sh->sh_size;
			elf_reloc_apply(p, filesize, shstab, shstabsz, sidx,
			    sdata, ssz);
			break;
		}
	}

	if (sdata == NULL)
		return -1;

	if (psdata != NULL)
		*psdata = sdata;
	if (pssz != NULL)
		*pssz = ssz;

	return sidx;
}

static int
elf_reloc_size(unsigned long type)
{
	switch (type) {
#ifdef R_X86_64_64
	case R_X86_64_64:
		return sizeof(uint64_t);
#endif
#ifdef R_X86_64_32
	case R_X86_64_32:
		return sizeof(uint32_t);
#endif
#ifdef RELOC_32
	case RELOC_32:
		return sizeof(uint32_t);
#endif
	default:
		break;
	}

	return -1;
}

#define ELF_WRITE_RELOC(buf, val, rsize)				\
do {									\
	if (rsize == 4) {						\
		uint32_t v32 = val;					\
		memcpy(buf, &v32, sizeof(v32));				\
	} else {							\
		uint64_t v64 = val;					\
		memcpy(buf, &v64, sizeof(v64));				\
	}								\
} while (0)

static void
elf_reloc_apply(const char *p, size_t filesize, const char *shstab,
    size_t shstabsz, ssize_t sidx, char *sdata, size_t ssz)
{
	Elf_Ehdr	*eh = (Elf_Ehdr *)p;
	Elf_Shdr	*sh;
	Elf_Rel		*rel = NULL;
	Elf_RelA	*rela = NULL;
	const Elf_Sym	*symtab, *sym;
	ssize_t		 symtabidx;
	size_t		 nsymb, rsym, rtyp, roff;
	size_t		 shoff, i, j;
	uint64_t	 value;
	int		 rsize;

	/* Find symbol table location and number of symbols. */
	symtabidx = elf_getsymtab(p, filesize, shstab, shstabsz, &symtab,
	    &nsymb, NULL, NULL);
	if (symtabidx == -1) {
		warnx("symbol table not found");
		return;
	}

	/* Apply possible relocation. */
	for (i = 0; i < eh->e_shnum; i++) {
		shoff = eh->e_shoff + i * eh->e_shentsize;
		if (shoff > (filesize - sizeof(*sh)))
			continue;

		sh = (Elf_Shdr *)(p + shoff);
		if (sh->sh_size == 0)
			continue;

		if ((sh->sh_info != sidx) || (sh->sh_link != symtabidx))
			continue;

		if (sh->sh_offset > filesize)
			continue;

		if (sh->sh_size > (filesize - sh->sh_offset))
			continue;

		switch (sh->sh_type) {
		case SHT_RELA:
			rela = (Elf_RelA *)(p + sh->sh_offset);
			for (j = 0; j < (sh->sh_size / sizeof(Elf_RelA)); j++) {
				rsym = ELF_R_SYM(rela[j].r_info);
				rtyp = ELF_R_TYPE(rela[j].r_info);
				roff = rela[j].r_offset;
				if (rsym >= nsymb)
					continue;
				if (roff >= filesize)
					continue;
				sym = &symtab[rsym];
				value = sym->st_value + rela[j].r_addend;

				rsize = elf_reloc_size(rtyp);
				if (rsize == -1 || roff + rsize >= ssz)
					continue;

				ELF_WRITE_RELOC(sdata + roff, value, rsize);
			}
			break;
		case SHT_REL:
			rel = (Elf_Rel *)(p + sh->sh_offset);
			for (j = 0; j < (sh->sh_size / sizeof(Elf_Rel)); j++) {
				rsym = ELF_R_SYM(rel[j].r_info);
				rtyp = ELF_R_TYPE(rel[j].r_info);
				roff = rel[j].r_offset;
				if (rsym >= nsymb)
					continue;
				if (roff >= filesize)
					continue;
				sym = &symtab[rsym];
				value = sym->st_value;

				rsize = elf_reloc_size(rtyp);
				if (rsize == -1 || roff + rsize >= ssz)
					continue;

				ELF_WRITE_RELOC(sdata + roff, value, rsize);
			}
			break;
		default:
			continue;
		}
	}
}
