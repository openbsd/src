/* $OpenBSD: prebind_struct.h,v 1.3 2006/06/15 22:09:32 drahn Exp $ */
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

struct symcache_noflag {
	const elf_object_t *obj;
	const Elf_Sym	*sym;
};

struct objlist {
	TAILQ_ENTRY(objlist) list;
	TAILQ_ENTRY(objlist) inst_list;
	struct elf_object *load_prog;
	struct elf_object *object;
};

struct proglist {
	TAILQ_ENTRY(proglist) list;
	TAILQ_HEAD(, objlist) curbin_list;
	struct fixup **fixup;
	int *fixupcnt;
	int *fixupcntalloc;
	int nobj;
	u_int32_t **libmap;
	u_int32_t *libmapcnt;
	char	*interp;
};
extern struct proglist *curbin;
extern struct elf_object *load_object;

typedef TAILQ_HEAD(, proglist) prog_list_ty;
typedef TAILQ_HEAD(, objlist) obj_list_ty;

extern obj_list_ty library_list;
extern prog_list_ty prog_list;

/* debug */
void	elf_print_curbin_list(struct proglist *bin);
void	elf_print_prog_list (prog_list_ty *prog_list);

void	elf_add_object_curbin_list(struct elf_object *object);

void	elf_copy_syms(struct symcache_noflag *tcache,
	    struct symcache_noflag *scache, struct elf_object *obj,
	    struct elf_object *prog, int nsyms);
int	elf_prep_lib_prebind(struct elf_object *object);
int	elf_prep_bin_prebind(struct proglist *pl);
void	elf_calc_fixups(struct proglist *pl, struct objlist *ol, int libidx);
int	elf_write_lib(struct elf_object *object, struct nameidx *nameidx,
	    char *nametab, int nametablen, int numlibs,
	    int nfixup, struct fixup **fixup, int *fixupcnt,
	    u_int32_t **libmap, int *libmapcnt,
	    struct symcachetab *symcachetab, int symcache_cnt,
	    struct symcachetab *pltsymcachetab, int pltsymcache_cnt);

void	dump_symcachetab(struct symcachetab *symcachetab, int symcache_cnt,
	    struct elf_object *object, int id);
void	dump_info(struct elf_object *object);
void	elf_clear_prog_load(int fd, struct elf_object *object);
void	elf_fixup_prog_load(int fd, struct prebind_footer *footer,
	    struct elf_object *object);
void	elf_dump_footer(struct prebind_footer *footer);

extern	int verbose;
extern	int merge;
extern	int safe;

extern int64_t prebind_blocks;
extern struct elf_object *load_object;
struct elf_object *elf_lookup_object(const char *name);
struct elf_object *load_file(const char *filename, int objtype);

void	elf_load_existing_prebind(struct elf_object *object, int fd);
