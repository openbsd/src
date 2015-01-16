/* $OpenBSD: dl_prebind.c,v 1.3 2015/01/16 16:18:07 deraadt Exp $ */

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
#include <sys/mman.h>
#include <sys/exec.h>
#include <sys/sysctl.h>
#include <nlist.h>
#include <string.h>
#include <link.h>
#include <dlfcn.h>
#include <unistd.h>

#include "syscall.h"
#include "archdep.h"
#include "resolve.h"
#include "sod.h"
#include "stdlib.h"
#include "dl_prebind.h"

void elf_dump_footer(struct prebind_footer *footer);
void dump_prelink(Elf_Addr base, u_long size);
void prebind_dump_footer(struct prebind_footer *footer, char *file);
void prebind_dump_symcache(struct symcachetab *symcachetab, u_int32_t cnt);
void prebind_dump_nameidx(struct nameidx *nameidx, u_int32_t numblibs,
    char *nametab);
void prebind_dump_fixup(struct fixup *fixup, u_int32_t numfixups);
void prebind_dump_libmap(u_int32_t *libmap, u_int32_t numlibs);
struct prebind_footer *_dl_prebind_data_to_footer(void *data);

void *_dl_prog_prebind_map;
struct prebind_footer *prog_footer;
extern char *_dl_noprebind;
extern char *_dl_prebind_validate;

int _dl_prebind_match_failed; /* = 0 */

char *prebind_bind_now = "prebind";

struct prebind_footer *
_dl_prebind_data_to_footer(void *prebind_data)
{
	u_int32_t *poffset, offset;
	struct prebind_footer *footer;
	char *c;

	poffset = prebind_data;
	c = prebind_data;
	offset = *poffset;
	c += offset;
	footer = (void *)c;
	return footer;
}

void
prebind_load_exe(Elf_Phdr *phdp, elf_object_t *exe_obj)
{
	struct prebind_footer *footer;

	exe_obj->prebind_data = (void *)phdp->p_vaddr;
	_dl_prog_prebind_map = exe_obj->prebind_data;

	footer = _dl_prebind_data_to_footer(_dl_objects->prebind_data);

	if (footer->bind_id[0] == BIND_ID0 &&
	    footer->bind_id[1] == BIND_ID1 &&
	    footer->bind_id[2] == BIND_ID2 &&
	    footer->bind_id[3] == BIND_ID3 &&
	    footer->prebind_version == PREBIND_VERSION) {
		prog_footer = footer;
		if (_dl_bindnow == NULL)
			_dl_bindnow = prebind_bind_now;
	} else {
		DL_DEB(("prebind data missing\n"));
		_dl_prog_prebind_map = NULL;
	}
	if (_dl_noprebind != NULL) {
		/*prog_footer is valid, we should free it */
		_dl_prog_prebind_map = NULL;
		prog_footer = NULL;
		exe_obj->prebind_data = NULL;
		if (_dl_bindnow == prebind_bind_now)
			_dl_bindnow = NULL;
	}
#if 0
	else if (_dl_debug)
		dump_prelink((long)_dl_prog_prebind_map,
		    prog_footer->prebind_size);
#endif
}

void *
prebind_load_fd(int fd, const char *name)
{
	struct prebind_footer footer;
	struct nameidx *nameidx;
	void *prebind_data;
	char *nametab;
	ssize_t len;
	int idx;

	if (_dl_prog_prebind_map == NULL || _dl_prebind_match_failed)
		return 0;

	_dl_lseek(fd, -(off_t)sizeof(struct prebind_footer), SEEK_END);
	len = _dl_read(fd, (void *)&footer, sizeof(struct prebind_footer));

	if (len != sizeof(struct prebind_footer) ||
	    footer.bind_id[0] != BIND_ID0 ||
	    footer.bind_id[1] != BIND_ID1 ||
	    footer.bind_id[2] != BIND_ID2 ||
	    footer.bind_id[3] != BIND_ID3 ||
	    footer.prebind_version != PREBIND_VERSION) {
		_dl_prebind_match_failed = 1;
		DL_DEB(("prebind match failed %s\n", name));
		return (NULL);
	}

	prebind_data = _dl_mmap(0, footer.prebind_size, PROT_READ,
	    MAP_FILE, fd, footer.prebind_base);
	DL_DEB(("prebind_load_fd for lib %s\n", name));

	nameidx = _dl_prog_prebind_map + prog_footer->nameidx_idx;
	nametab = _dl_prog_prebind_map + prog_footer->nametab_idx;

	/* libraries are loaded in random order, so we just have
	 * to look thru the list to find ourselves
	 */
	for (idx = 0; idx < prog_footer->numlibs; idx++) {
		if (_dl_strcmp(nametab + nameidx[idx].name, name) == 0)
			break;
	}

	if (idx == prog_footer->numlibs) {
		_dl_prebind_match_failed = 1; /* not found */
	} else if (footer.id0 != nameidx[idx].id0 ||
	    footer.id1 != nameidx[idx].id1) {
		_dl_prebind_match_failed = 1;
		DL_DEB(("prebind match id0 %d pid0 %d id1 %d pid1 %d\n",
		    footer.id0, nameidx[idx].id0,
		    footer.id1, nameidx[idx].id1));
	}

	if (_dl_prebind_match_failed == 1) {
		DL_DEB(("prebind match failed for %s\n", name));
	}

	return prebind_data;
}
#define NUM_STATIC_OBJS 10
elf_object_t *objarray_static[NUM_STATIC_OBJS];
elf_object_t **objarray;

void
prebind_symcache(elf_object_t *object, int plt)
{
	u_int32_t *fixupidx, *fixupcnt, *libmap, *idxtolib;
	u_int32_t *poffset, offset, symcache_cnt;
	struct symcachetab *symcachetab;
	struct prebind_footer *footer;
	int i = 0, cur_obj = -1, idx;
	void *prebind_map;
	struct nameidx *nameidx;
	char *nametab, *c;
	struct fixup *fixup;
	elf_object_t *obj;

	struct symcachetab *s;

	if (object->prebind_data == NULL)
		return;
//	DL_DEB(("prebind symcache %s\n", object->load_name));

	obj = _dl_objects;
	while (obj != NULL) {
		if (obj == object)
			cur_obj = i;
		i++;
		obj = obj->next;
	}

	if (cur_obj == -1)
		return;	/* unable to find object ? */

	if (objarray == NULL) {
		if (i <= NUM_STATIC_OBJS) {
			objarray = &objarray_static[0];
		} else {
			objarray = _dl_malloc(sizeof(elf_object_t *) * i);
		}

		obj = _dl_objects;
		i = 0;
		while (obj != NULL) {
			objarray[i] = obj;
			i++;
			obj = obj->next;
		}
	}

	poffset = (u_int32_t *)object->prebind_data;
	c = object->prebind_data;
	offset = *poffset;
	c += offset;

	footer = (void *)c;
	prebind_map = (void *)object->prebind_data;
	nameidx = prebind_map + footer->nameidx_idx;;
	if (plt) {
		symcachetab = prebind_map + footer->pltsymcache_idx;
		symcache_cnt = footer->pltsymcache_cnt;
//		DL_DEB(("loading plt %d\n", symcache_cnt));
	} else {
		symcachetab = prebind_map + footer->symcache_idx;
		symcache_cnt = footer->symcache_cnt;
//		DL_DEB(("loading got %d\n", symcache_cnt));
	}
	nametab = prebind_map + footer->nametab_idx;

	libmap = _dl_prog_prebind_map + prog_footer->libmap_idx;
	idxtolib = _dl_prog_prebind_map + libmap[cur_obj];

	for (i = 0; i < symcache_cnt; i++) {
		struct elf_object *tobj;
		const Elf_Sym *sym;
		const char *str;

		s = &(symcachetab[i]);
		if (cur_obj == 0)
			idx = s->obj_idx;
		else
			idx = idxtolib[s->obj_idx];

		if (idx == -1) /* somehow an invalid object ref happend */
			continue;
#if 0
		DL_DEB(("%s:", object->load_name));
		DL_DEB(("symidx %d: obj %d %d sym %d flags %x\n",
		    s->idx, s->obj_idx, idx, s->sym_idx,
		    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|plt));
#endif
		tobj = objarray[idx];
		sym = tobj->dyn.symtab + s->sym_idx;
		str = tobj->dyn.strtab + sym->st_name;
#ifdef DEBUG2
		DL_DEB(("symidx %d: obj %d %s sym %d %s flags %d %x\n",
		    s->idx, s->obj_idx, tobj->load_name,
		    s->sym_idx, str, SYM_SEARCH_ALL|SYM_WARNNOTFOUND|plt,
		    object->obj_base + sym->st_value));
#endif
		_dl_symcache[s->idx].obj = tobj;
		_dl_symcache[s->idx].sym = sym;
		_dl_symcache[s->idx].flags =
		    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|plt;
	}

	if (!plt) {
		fixupidx = _dl_prog_prebind_map + prog_footer->fixup_idx;
		fixup = _dl_prog_prebind_map + fixupidx[2*cur_obj];
		fixupcnt = _dl_prog_prebind_map + prog_footer->fixupcnt_idx;

		for (i = 0; i < fixupcnt[2*cur_obj]; i++) {
			struct fixup *f;
			struct elf_object *tobj;
			const Elf_Sym *sym;
			const char *str;

			f = &(fixup[i]);
#if 0
			DL_DEB(("symidx %d: obj %d sym %d flags %x\n",
			    f->sym, f->obj_idx, f->sym_idx, f->flags));
#endif
			tobj = objarray[f->obj_idx];
			sym = tobj->dyn.symtab + f->sym_idx;
			str = tobj->dyn.strtab + sym->st_name;
#ifdef DEBUG2
			DL_DEB(("symidx %d: obj %d %s sym %d %s flags %d %x\n",
			    f->sym, f->obj_idx, tobj->load_name,
			    f->sym_idx, str, SYM_SEARCH_ALL|SYM_WARNNOTFOUND|plt,
			    object->obj_base + sym->st_value));
#endif
			_dl_symcache[f->sym].obj = tobj;
			_dl_symcache[f->sym].sym = sym;
			_dl_symcache[f->sym].flags =
			    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|plt;
		}
	} else {

		fixupidx = _dl_prog_prebind_map + prog_footer->fixup_idx;
		fixup = _dl_prog_prebind_map + fixupidx[2*cur_obj+1];
		fixupcnt = _dl_prog_prebind_map + prog_footer->fixupcnt_idx;

#if 0
		DL_DEB(("prebind loading symbols fixup plt %s\n",
		    object->load_name));
#endif
		for (i = 0; i < fixupcnt[2*cur_obj+1]; i++) {
			struct fixup *f;
			struct elf_object *tobj;
			const Elf_Sym *sym;
			const char *str;

			f = &(fixup[i]);
#if 0
			DL_DEB(("symidx %d: obj %d sym %d flags %x\n",
			    f->sym, f->obj_idx, f->sym_idx,
			    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|plt));
#endif
			tobj = objarray[f->obj_idx];
			sym = tobj->dyn.symtab + f->sym_idx;
			str = tobj->dyn.strtab + sym->st_name;
#ifdef DEBUG2
			DL_DEB(("symidx %d: obj %d %s sym %d %s flags %d %x\n",
			    f->sym, f->obj_idx, tobj->load_name,
			    f->sym_idx, str, SYM_SEARCH_ALL|SYM_WARNNOTFOUND|plt,
			    object->obj_base + sym->st_value));
#endif
			_dl_symcache[f->sym].obj = tobj;
			_dl_symcache[f->sym].sym = sym;
			_dl_symcache[f->sym].flags =
			    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|plt;
		}
	}
//	DL_DEB(("prebind_data loaded\n"));
}

void
prebind_free(elf_object_t *object)
{
	struct prebind_footer *footer;

	if (object->prebind_data == NULL)
		return;
#ifdef DEBUG1
	DL_DEB(("prebind_free for %s %p\n", object->load_name,
	    object->prebind_data));
#endif
	if (object->prebind_data != 0) {
		footer = _dl_prebind_data_to_footer(object->prebind_data);

#ifdef DEBUG1
		DL_DEB(("freeing prebind data sz %x\n", footer->prebind_size));
#endif

		_dl_munmap((void *)ELF_TRUNC((long)object->prebind_data, _dl_pagesz),
		    ELF_ROUND((long)object->prebind_data+footer->prebind_size,
		    _dl_pagesz) - ELF_TRUNC((long)object->prebind_data, _dl_pagesz));

		object->prebind_data = NULL;
		_dl_prog_prebind_map = NULL;

		if (_dl_bindnow == prebind_bind_now)
			_dl_bindnow = NULL;
	}
}

int validate_errs;

struct timeval beforetp;

void
_dl_prebind_pre_resolve()
{
	struct prebind_footer *footer;
	elf_object_t *object;
	struct nameidx *nameidx;
	char *nametab, *name;
	int idx;

	if (_dl_prog_prebind_map != NULL) {
		nameidx = _dl_prog_prebind_map + prog_footer->nameidx_idx;
		nametab = _dl_prog_prebind_map + prog_footer->nametab_idx;
		for (idx = 1, object = _dl_objects->next; object != NULL;
		    object = object->next, idx++) {
			if (object->prebind_data == NULL) {
				/* ld.so doesn't have prebind data */
				if (object->next == NULL)
					continue;
				DL_DEB(("missing prebind data %s\n",
				    object->load_name));
				_dl_prebind_match_failed = 1;
				break;
			}
			footer = _dl_prebind_data_to_footer(
			    object->prebind_data);
			if (footer == NULL ||
			    nameidx[idx].id0 != footer->id0 ||
			    nameidx[idx].id1 != footer->id1) {
				DL_DEB(("invalid prebind data %s\n",
				    object->load_name));
				_dl_prebind_match_failed = 1;
				break;
			}
			name = object->load_name;
			if (_dl_strcmp(nametab + nameidx[idx].name, name)
			    != 0) {
				DL_DEB(("invalid prebind name %s\n",
				    object->load_name));
				_dl_prebind_match_failed = 1;
				break;
			}
		}
	}

	if (_dl_prebind_match_failed) {
		for (object = _dl_objects; object != NULL;
		    object = object->next)
			prebind_free(object);
		if (_dl_bindnow == prebind_bind_now)
			_dl_bindnow = NULL;
	}

	if (_dl_debug)
		_dl_gettimeofday(&beforetp, NULL);
}

void
_dl_prebind_post_resolve()
{
	char buf[7];
	int i;
	struct timeval after_tp;
	struct timeval diff_tp;
	elf_object_t *object;

	if (_dl_debug) {
		_dl_gettimeofday(&after_tp, NULL);

		timersub(&after_tp, &beforetp, &diff_tp);

		for (i = 0; i < 6; i++) {
			buf[5-i] = (diff_tp.tv_usec % 10) + '0';
			diff_tp.tv_usec /= 10;
		}
		buf[6] = '\0';

		_dl_printf("relocation took %d.%s\n", diff_tp.tv_sec, buf);
	}

	for (object = _dl_objects; object != NULL; object = object->next)
		prebind_free(object);

	if (_dl_prebind_validate) {
		if (validate_errs) {
			_dl_printf("validate_errs %d\n", validate_errs);
			_dl_exit(20);
		} else {
			_dl_exit(0);
		}
	}
}

void
prebind_validate(elf_object_t *req_obj, unsigned int symidx, int flags,
    const Elf_Sym *ref_sym)
{
	const Elf_Sym *sym, **this;
	const elf_object_t *sobj;
	const char *symn;
	Elf_Addr ret;

	/* Don't verify non-matching flags*/

	sym = req_obj->dyn.symtab;
	sym += symidx;
	symn = req_obj->dyn.strtab + sym->st_name;
	this = &sym;

	//_dl_printf("checking %s\n", symn);
	ret = _dl_find_symbol(symn, this, flags, ref_sym, req_obj, &sobj);

	if (_dl_symcache[symidx].sym != *this ||
	    _dl_symcache[symidx].obj != sobj) {
		_dl_printf("symbol %d mismatch on sym %s req_obj %s,\n"
		    "should be obj %s is obj %s\n",
		    symidx, symn, req_obj->load_name, sobj->load_name,
		    _dl_symcache[symidx].obj->load_name);
		if (req_obj == sobj)
			_dl_printf("obj %p %p\n", _dl_symcache[symidx].obj, sobj);
		sym = _dl_symcache[symidx].obj->dyn.symtab;
		sym += symidx;
		symn = _dl_symcache[symidx].obj->dyn.strtab + sym->st_name;
		_dl_printf("obj %s name %s\n",
		    _dl_symcache[symidx].obj->load_name,
		    symn);
	}
}

#ifdef DEBUG1
void
prebind_dump_symcache(struct symcachetab *symcachetab, u_int32_t cnt)
{
	struct symcachetab *s;
	int i;

	_dl_printf("cache: cnt %d\n", cnt);
	for (i = 0; i < cnt; i++) {
		s = &(symcachetab[i]);
		_dl_printf("symidx %d: obj %d sym %d\n",
		    s->idx, s->obj_idx, s->sym_idx);
	}
}

void
prebind_dump_nameidx(struct nameidx *nameidx, u_int32_t numlibs, char *nametab)
{
	struct nameidx *n;
	int i;

	_dl_printf("libs:\n");
	for (i = 0; i < numlibs; i++) {
		_dl_printf("lib %d offset %d id0 %d, id1 %d\n", i,
		    nameidx[i].name, nameidx[i].id0, nameidx[i].id1);
	}
	for (i = 0; i < numlibs; i++) {
		n = &(nameidx[i]);
		_dl_printf("nametab %p n %d\n", nametab, n->name);
		_dl_printf("lib %s %x %x\n", nametab + n->name, n->id0, n->id1);
	}
}

void
prebind_dump_fixup(struct fixup *fixup, u_int32_t numfixups)
{
	struct fixup *f;
	int i;

	_dl_printf("fixup: %d\n", numfixups);
	for (i = 0; i < numfixups; i++) {
		f = &(fixup[i]);

		_dl_printf("idx %d obj %d sym idx %d\n",
		    f->sym, f->obj_idx, f->sym_idx);

	}
}

void
prebind_dump_libmap(u_int32_t *libmap, u_int32_t numlibs)
{
	int i;

	for (i = 0; i < numlibs; i++) {
		//_dl_printf("lib%d off %d %s\n", i, libmap[i], strtab+libmap[i]);
		_dl_printf("lib%d off %d\n", i, libmap[i]);
	}
}

void
dl_dump_footer(struct prebind_footer *footer)
{
//	_dl_printf("base %qd\n", (long long)footer->prebind_base);
	_dl_printf("nameidx_idx %d\n", footer->nameidx_idx);
	_dl_printf("symcache_idx %d\n", footer->symcache_idx);
	_dl_printf("fixupcnt_idx %d\n", footer->fixupcnt_idx);
	_dl_printf("fixup_cnt %d\n", footer->fixup_cnt);
	_dl_printf("nametab_idx %d\n", footer->nametab_idx);
	_dl_printf("symcache_cnt %d\n", footer->symcache_cnt);
	_dl_printf("fixup_cnt %d\n", footer->fixup_cnt);
	_dl_printf("numlibs %d\n", footer->numlibs);
	_dl_printf("id0 %d\n", footer->id0);
	_dl_printf("id1 %d\n", footer->id1);
//	_dl_printf("orig_size %lld\n", (long long)footer->orig_size);
	_dl_printf("version %d\n", footer->prebind_version);
	_dl_printf("bind_id %c%c%c%c\n", footer->bind_id[0],
	    footer->bind_id[1], footer->bind_id[2], footer->bind_id[3]);
}
void
dump_prelink(Elf_Addr base, u_long size)
{
	u_int32_t *fixupidx, *fixupcnt, *libmap;
	struct symcachetab *symcachetab;
	struct prebind_footer *footer;
	struct nameidx *nameidx;
	struct fixup *fixup;
	char *nametab, *id;
	void *prebind_map;
	int i;

	id = (char *) (base+size);
	id -= 4;
	DL_DEB(("id %c %c %c %c\n", id[0], id[1], id[2], id[3]));
	footer = (void *) (base+size - sizeof (struct prebind_footer));
	dl_dump_footer(footer);

	prebind_map = (void *)base;
	nameidx = prebind_map + footer->nameidx_idx;;
	symcachetab = prebind_map + footer->symcache_idx;
	fixupidx = prebind_map + footer->fixup_idx;
	nametab = prebind_map + footer->nametab_idx;
	fixupcnt = prebind_map + footer->fixupcnt_idx;
	libmap = prebind_map + footer->libmap_idx;

	prebind_dump_symcache(symcachetab, footer->symcache_cnt);
	prebind_dump_nameidx(nameidx, footer->numlibs, nametab);
	for (i = 0; i < footer->fixup_cnt; i++) {
		_dl_printf("fixup %d cnt %d idx %d\n", i, fixupcnt[i], fixupidx[i]);
		fixup = prebind_map + fixupidx[i];
		prebind_dump_fixup(fixup, fixupcnt[i]);
	}
	prebind_dump_libmap(libmap, footer->numlibs);
}
#endif /* DEBUG1 */
