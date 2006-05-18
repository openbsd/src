/* $OpenBSD: debug.c,v 1.4 2006/05/18 17:00:06 deraadt Exp $ */
/*
 * Copyright (c) 2006 Dale Rahn <drahn@dalerahn.com>
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
#include <sys/stat.h>
#include <sys/syslimits.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <nlist.h>
#include <elf_abi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include "resolve.h"
#include "link.h"
#include "sod.h"
#ifndef __mips64__
#include "machine/reloc.h"
#endif
#include "prebind.h"
#include "prebind_struct.h"

#ifdef DEBUG1
void
dump_info(struct elf_object *object)
{
	int numrel, numrela, i;
	const Elf_Sym	*symt;
	const char	*strt;
	Elf_Word *needed_list;

	symt = object->dyn.symtab;
	strt = object->dyn.strtab;

	for (i = 0; i < object->nchains; i++) {
		const Elf_Sym *sym = symt + i;
		char *type;

		switch (ELF_ST_TYPE(sym->st_info)) {
		case STT_FUNC:
			type = "func";
			break;
		case STT_OBJECT:
			type = "object";
			break;
		case STT_NOTYPE:
			type = "notype";
			break;
		default:
			type = "UNKNOWN";
		}
		printf("symbol %d [%s] type %s value %x\n", i,
		    strt + sym->st_name,
		    type, sym->st_value);
	}

	numrel = object->dyn.relsz / sizeof(Elf_Rel);
	numrela = object->dyn.relasz / sizeof(Elf_RelA);
	printf("numrel %d numrela %d\n", numrel, numrela);

	printf("rel relocations:\n");
	for (i = 0; i < numrel ; i++) {
		Elf_Rel *rel = object->dyn.rel;

		printf("%d: %x sym %x type %d\n", i, rel[i].r_offset,
		    ELF_R_SYM(rel[i].r_info), ELF_R_TYPE(rel[i].r_info));
	}
	printf("rela relocations:\n");
	for (i = 0; i < numrela ; i++) {
		Elf_RelA *rela = object->dyn.rela;

		printf("%d: %x sym %x type %d\n", i, rela[i].r_offset,
		    ELF_R_SYM(rela[i].r_info), ELF_R_TYPE(rela[i].r_info));
	}
	needed_list = (Elf_Addr *)object->dyn.needed;
	for (i = 0; needed_list[i] != NULL; i++)
		printf("NEEDED %s\n", needed_list[i] + strt);

}
#endif


void
elf_dump_footer(struct prebind_footer *footer)
{
	printf("\nbase %llx\n", (long long)footer->prebind_base);
	printf("nameidx_idx %d\n", footer->nameidx_idx);
	printf("symcache_idx %d\n", footer->symcache_idx);
	printf("pltsymcache_idx %d\n", footer->pltsymcache_idx);
	printf("fixupcnt_idx %d\n", footer->fixupcnt_idx);
	printf("fixup_cnt %d\n", footer->fixup_cnt);
	printf("nametab_idx %d\n", footer->nametab_idx);
	printf("symcache_cnt %d\n", footer->symcache_cnt);
	printf("pltsymcache_cnt %d\n", footer->pltsymcache_cnt);
	printf("fixup_cnt %d\n", footer->fixup_cnt);
	printf("numlibs %d\n", footer->numlibs);
	printf("id0 %x\n", footer->id0);
	printf("id1 %x\n", footer->id1);
	printf("orig_size %lld\n", (long long)footer->orig_size);
	printf("version %d\n", footer->prebind_version);
	printf("bind_id %c%c%c%c\n", footer->bind_id[0],
	    footer->bind_id[1], footer->bind_id[2], footer->bind_id[3]);
}


void
dump_symcachetab(struct symcachetab *symcachetab, int symcache_cnt,
    struct elf_object *object, int id)
{
	int i;

	printf("symcache for %s\n", object->load_name);
	for (i = 0; i < symcache_cnt; i++) {
		printf("symidx %d: obj %d sym %d\n",
		    symcachetab[i].idx,
		    symcachetab[i].obj_idx,
		    symcachetab[i].sym_idx);
	}
}

void
elf_print_prog_list(prog_list_ty *prog_list)
{
	struct elf_object *object;
	struct proglist *pl;

	TAILQ_FOREACH(pl, prog_list, list) {
		object = TAILQ_FIRST(&(pl->curbin_list))->object;
		printf("bin: %s\n", object->load_name);
		elf_print_curbin_list(pl);
	}
}

void
elf_print_curbin_list(struct proglist *bin)
{
	struct objlist *ol;

	TAILQ_FOREACH(ol, &(bin->curbin_list), list) {
		printf("\t%s\n", ol->object->load_name);
	}
}

