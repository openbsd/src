/*	$OpenBSD: resolve.c,v 1.87 2018/11/28 03:18:00 guenther Exp $ */

/*
 * Copyright (c) 1998 Per Fogelstrom, Opsycon AB
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#define _DYN_LOADER

#include <sys/types.h>

#include <limits.h>
#include <nlist.h>
#include <link.h>
#include "syscall.h"
#include "archdep.h"
#include "path.h"
#include "resolve.h"

/* substitution types */
typedef enum {
	SUBST_UNKNOWN, SUBST_ORIGIN, SUBST_OSNAME, SUBST_OSREL, SUBST_PLATFORM
} SUBST_TYPES;

struct symlookup {
	const char		*sl_name;
	const elf_object_t	*sl_obj_out;
	const Elf_Sym		*sl_sym_out;
	const elf_object_t	*sl_weak_obj_out;
	const Elf_Sym		*sl_weak_sym_out;
	unsigned long		sl_elf_hash;
	uint32_t		sl_gnu_hash;
	int			sl_flags;
};

elf_object_t *_dl_objects;
elf_object_t *_dl_last_object;
elf_object_t *_dl_loading_object;

/*
 * Add a new dynamic object to the object list.
 */
void
_dl_add_object(elf_object_t *object)
{
	/*
	 * If a .so is marked nodelete, then the entire load group that it's
	 * in needs to be kept around forever, so add a reference there.
	 * XXX It would be better if we tracked inter-object dependencies
	 * from relocations and didn't leave dangling pointers when a load
	 * group was partially unloaded.  That would render this unnecessary.
	 */
	if (object->obj_flags & DF_1_NODELETE &&
	    (object->load_object->status & STAT_NODELETE) == 0) {
		DL_DEB(("objname %s is nodelete\n", object->load_name));
		object->load_object->opencount++;
		object->load_object->status |= STAT_NODELETE;
	}

	/*
	 * if this is a new object, prev will be NULL
	 * != NULL if an object already in the list
	 * prev == NULL for the first item in the list, but that will
	 * be the executable.
	 */
	if (object->prev != NULL)
		return;

	if (_dl_objects == NULL) {			/* First object ? */
		_dl_last_object = _dl_objects = object;
	} else {
		_dl_last_object->next = object;
		object->prev = _dl_last_object;
		_dl_last_object = object;
	}
}

/*
 * Identify substitution sequence name.
 */
static int
_dl_subst_name(const char *name, size_t siz) {
	switch (siz) {
	case 5:
		if (_dl_strncmp(name, "OSREL", 5) == 0)
			return SUBST_OSREL;
		break;
	case 6:
		if (_dl_strncmp(name, "ORIGIN", 6) == 0)
			return SUBST_ORIGIN;
		if (_dl_strncmp(name, "OSNAME", 6) == 0)
			return SUBST_OSNAME;
		break;
	case 8:
		if (_dl_strncmp(name, "PLATFORM", 8) == 0)
			return SUBST_PLATFORM;
		break;
	}

	return (SUBST_UNKNOWN);
}

/*
 * Perform $ORIGIN substitutions on path
 */
static void
_dl_origin_subst_path(elf_object_t *object, const char *origin_path,
    char **path)
{
	char tmp_path[PATH_MAX];
	char *new_path, *tp;
	const char *pp, *name, *value;
	static struct utsname uts;
	size_t value_len;
	int skip_brace;

	if (uts.sysname[0] == '\0') {
		if (_dl_uname(&uts) != 0)
			return;
	}

	tp = tmp_path;
	pp = *path;

	while (*pp != '\0' && (tp - tmp_path) < sizeof(tmp_path)) {

		/* copy over chars up to but not including $ */
		while (*pp != '\0' && *pp != '$' &&
		    (tp - tmp_path) < sizeof(tmp_path))
			*tp++ = *pp++;

		/* substitution sequence detected */
		if (*pp == '$' && (tp - tmp_path) < sizeof(tmp_path)) {
			pp++;

			if ((skip_brace = (*pp == '{')))
				pp++;

			/* skip over name */
			name = pp;
			while (_dl_isalnum((unsigned char)*pp) || *pp == '_')
				pp++;

			switch (_dl_subst_name(name, pp - name)) {
			case SUBST_ORIGIN:
				value = origin_path;
				break;
			case SUBST_OSNAME:
				value = uts.sysname;
				break;
			case SUBST_OSREL:
				value = uts.release;
				break;
			case SUBST_PLATFORM:
				value = uts.machine;
				break;
			default:
				value = "";
			}

			value_len = _dl_strlen(value);
			if (value_len >= sizeof(tmp_path) - (tp - tmp_path))
				return;

			_dl_bcopy(value, tp, value_len);
			tp += value_len;

			if (skip_brace && *pp == '}')
				pp++;
		}
	}

	/* no substitution made if result exceeds sizeof(tmp_path) */
	if (tp - tmp_path >= sizeof(tmp_path))
		return;

	/* NULL terminate tmp_path */
	*tp = '\0';

	if (_dl_strcmp(tmp_path, *path) == 0)
		return;

	new_path = _dl_strdup(tmp_path);
	if (new_path == NULL)
		return;

	DL_DEB(("orig_path %s\n", *path));
	DL_DEB(("new_path  %s\n", new_path));

	_dl_free(*path);
	*path = new_path;
}

/*
 * Determine origin_path from object load_name. The origin_path argument
 * must refer to a buffer capable of storing at least PATH_MAX characters.
 * Returns 0 on success.
 */
static int
_dl_origin_path(elf_object_t *object, char *origin_path)
{
	const char *dirname_path = _dl_dirname(object->load_name);

	if (dirname_path == NULL)
		return -1;

	if (_dl_realpath(dirname_path, origin_path) == NULL)
		return -1;

	return 0;
}

/*
 * Perform $ORIGIN substitutions on runpath and rpath
 */
static void
_dl_origin_subst(elf_object_t *object)
{
	char origin_path[PATH_MAX];
	char **pp;

	if (_dl_origin_path(object, origin_path) != 0)
		return;

	/* perform path substitutions on each segment of runpath and rpath */
	if (object->runpath != NULL) {
		for (pp = object->runpath; *pp != NULL; pp++)
			_dl_origin_subst_path(object, origin_path, pp);
	}
	if (object->rpath != NULL) {
		for (pp = object->rpath; *pp != NULL; pp++)
			_dl_origin_subst_path(object, origin_path, pp);
	}
}

/*
 * Initialize a new dynamic object.
 */
elf_object_t *
_dl_finalize_object(const char *objname, Elf_Dyn *dynp, Elf_Phdr *phdrp,
    int phdrc, const int objtype, const long lbase, const long obase)
{
	elf_object_t *object;
	Elf_Addr gnu_hash = 0;

#if 0
	_dl_printf("objname [%s], dynp %p, objtype %x lbase %lx, obase %lx\n",
	    objname, dynp, objtype, lbase, obase);
#endif
	object = _dl_calloc(1, sizeof(elf_object_t));
	if (object == NULL)
		_dl_oom();
	object->prev = object->next = NULL;

	object->load_dyn = dynp;
	while (dynp->d_tag != DT_NULL) {
		if (dynp->d_tag < DT_NUM)
			object->Dyn.info[dynp->d_tag] = dynp->d_un.d_val;
		else if (dynp->d_tag >= DT_LOPROC &&
		    dynp->d_tag < DT_LOPROC + DT_PROCNUM)
			object->Dyn.info[dynp->d_tag + DT_NUM - DT_LOPROC] =
			    dynp->d_un.d_val;
		if (dynp->d_tag == DT_TEXTREL)
			object->dyn.textrel = 1;
		if (dynp->d_tag == DT_SYMBOLIC)
			object->dyn.symbolic = 1;
		if (dynp->d_tag == DT_BIND_NOW)
			object->obj_flags |= DF_1_NOW;
		if (dynp->d_tag == DT_FLAGS_1)
			object->obj_flags |= dynp->d_un.d_val;
		if (dynp->d_tag == DT_FLAGS) {
			object->dyn.flags |= dynp->d_un.d_val;
			if (dynp->d_un.d_val & DF_SYMBOLIC)
				object->dyn.symbolic = 1;
			if (dynp->d_un.d_val & DF_TEXTREL)
				object->dyn.textrel = 1;
			if (dynp->d_un.d_val & DF_ORIGIN)
				object->obj_flags |= DF_1_ORIGIN;
			if (dynp->d_un.d_val & DF_BIND_NOW)
				object->obj_flags |= DF_1_NOW;
		}
		if (dynp->d_tag == DT_RELACOUNT)
			object->relacount = dynp->d_un.d_val;
		if (dynp->d_tag == DT_RELCOUNT)
			object->relcount = dynp->d_un.d_val;
		if (dynp->d_tag == DT_GNU_HASH)
			gnu_hash = dynp->d_un.d_val;
		dynp++;
	}
	DL_DEB((" flags %s = 0x%x\n", objname, object->obj_flags ));
	object->obj_type = objtype;

	if (_dl_loading_object == NULL) {
		/*
		 * no loading object, object is the loading object,
		 * as it is either executable, or dlopened()
		 */
		_dl_loading_object = object;
	}

	if ((object->obj_flags & DF_1_NOOPEN) != 0 &&
	    _dl_loading_object->obj_type == OBJTYPE_DLO &&
	    !_dl_traceld) {
		_dl_free(object);
		_dl_errno = DL_CANT_LOAD_OBJ;
		return(NULL);
	}

	/*
	 *  Now relocate all pointer to dynamic info, but only
	 *  the ones which have pointer values.
	 */
	if (object->Dyn.info[DT_PLTGOT])
		object->Dyn.info[DT_PLTGOT] += obase;
	if (object->Dyn.info[DT_STRTAB])
		object->Dyn.info[DT_STRTAB] += obase;
	if (object->Dyn.info[DT_SYMTAB])
		object->Dyn.info[DT_SYMTAB] += obase;
	if (object->Dyn.info[DT_RELA])
		object->Dyn.info[DT_RELA] += obase;
	if (object->Dyn.info[DT_SONAME])
		object->Dyn.info[DT_SONAME] += object->Dyn.info[DT_STRTAB];
	if (object->Dyn.info[DT_RPATH])
		object->Dyn.info[DT_RPATH] += object->Dyn.info[DT_STRTAB];
	if (object->Dyn.info[DT_RUNPATH])
		object->Dyn.info[DT_RUNPATH] += object->Dyn.info[DT_STRTAB];
	if (object->Dyn.info[DT_REL])
		object->Dyn.info[DT_REL] += obase;
	if (object->Dyn.info[DT_INIT])
		object->Dyn.info[DT_INIT] += obase;
	if (object->Dyn.info[DT_FINI])
		object->Dyn.info[DT_FINI] += obase;
	if (object->Dyn.info[DT_JMPREL])
		object->Dyn.info[DT_JMPREL] += obase;
	if (object->Dyn.info[DT_INIT_ARRAY])
		object->Dyn.info[DT_INIT_ARRAY] += obase;
	if (object->Dyn.info[DT_FINI_ARRAY])
		object->Dyn.info[DT_FINI_ARRAY] += obase;
	if (object->Dyn.info[DT_PREINIT_ARRAY])
		object->Dyn.info[DT_PREINIT_ARRAY] += obase;

	if (gnu_hash) {
		Elf32_Word *hashtab = (Elf32_Word *)(gnu_hash + obase);
		Elf32_Word nbuckets = hashtab[0];
		Elf32_Word nmaskwords = hashtab[2];

		/* validity check */
		if (nbuckets > 0 && (nmaskwords & (nmaskwords - 1)) == 0) {
			Elf32_Word symndx = hashtab[1];
			int bloom_size32 = (ELFSIZE / 32) * nmaskwords;

			object->nbuckets = nbuckets;
			object->symndx_gnu = symndx;
			object->mask_bm_gnu = nmaskwords - 1;
			object->shift2_gnu = hashtab[3];
			object->bloom_gnu = (Elf_Addr *)(hashtab + 4);
			object->buckets_gnu = hashtab + 4 + bloom_size32;
			object->chains_gnu = object->buckets_gnu + nbuckets
			    - symndx;

			/*
			 * If the ELF hash is present, get the total symbol
			 * count ("nchains") from there.  Otherwise, count
			 * the entries in the GNU hash chain.
			 */
			if (object->Dyn.info[DT_HASH] == 0) {
				Elf32_Word n;

				for (n = 0; n < nbuckets; n++) {
					Elf_Word bkt = object->buckets_gnu[n];
					const Elf32_Word *hashval;
					if (bkt == 0)
						continue;
					hashval = &object->chains_gnu[bkt];
					do {
						symndx++;
					} while ((*hashval++ & 1U) == 0);
				}
				object->nchains = symndx;
			}
			object->status |= STAT_GNU_HASH;
		}
	}
	if (object->Dyn.info[DT_HASH] != 0) {
		Elf_Word *hashtab = (Elf_Word *)(object->Dyn.info[DT_HASH]
		    + obase);

		object->nchains = hashtab[1];
		if (object->nbuckets == 0) {
			object->nbuckets = hashtab[0];
			object->buckets_elf = hashtab + 2;
			object->chains_elf = object->buckets_elf +
			    object->nbuckets;
		}
	}

	object->phdrp = phdrp;
	object->phdrc = phdrc;
	object->load_base = lbase;
	object->obj_base = obase;
	object->load_name = _dl_strdup(objname);
	if (object->load_name == NULL)
		_dl_oom();
	object->load_object = _dl_loading_object;
	if (object->load_object == object)
		DL_DEB(("head %s\n", object->load_name));
	DL_DEB(("obj %s has %s as head\n", object->load_name,
	    _dl_loading_object->load_name ));
	object->refcount = 0;
	TAILQ_INIT(&object->child_list);
	object->opencount = 0;	/* # dlopen() & exe */
	object->grprefcount = 0;
	/* default dev, inode for dlopen-able objects. */
	object->dev = 0;
	object->inode = 0;
	object->grpsym_gen = 0;
	TAILQ_INIT(&object->grpsym_list);
	TAILQ_INIT(&object->grpref_list);

	if (object->dyn.runpath)
		object->runpath = _dl_split_path(object->dyn.runpath);
	/*
	 * DT_RPATH is ignored if DT_RUNPATH is present...except in
	 * the exe, whose DT_RPATH is a fallback for libs that don't
	 * use DT_RUNPATH
	 */
	if (object->dyn.rpath && (object->runpath == NULL ||
	    objtype == OBJTYPE_EXE))
		object->rpath = _dl_split_path(object->dyn.rpath);
	if ((object->obj_flags & DF_1_ORIGIN) && _dl_trust)
		_dl_origin_subst(object);

	_dl_trace_object_setup(object);

	return (object);
}

static void
_dl_tailq_free(struct dep_node *n)
{
	struct dep_node *next;

	while (n != NULL) {
		next = TAILQ_NEXT(n, next_sib);
		_dl_free(n);
		n = next;
	}
}

static elf_object_t *free_objects;

void
_dl_cleanup_objects()
{
	elf_object_t *nobj, *head;
	struct dep_node *n, *next;

	n = TAILQ_FIRST(&_dlopened_child_list);
	while (n != NULL) {
		next = TAILQ_NEXT(n, next_sib);
		if (OBJECT_DLREF_CNT(n->data) == 0) {
			TAILQ_REMOVE(&_dlopened_child_list, n, next_sib);
			_dl_free(n);
		}
		n = next;
	}

	head = free_objects;
	free_objects = NULL;
	while (head != NULL) {
		_dl_free(head->load_name);
		_dl_free((char *)head->sod.sod_name);
		_dl_free_path(head->runpath);
		_dl_free_path(head->rpath);
		_dl_tailq_free(TAILQ_FIRST(&head->grpsym_list));
		_dl_tailq_free(TAILQ_FIRST(&head->child_list));
		_dl_tailq_free(TAILQ_FIRST(&head->grpref_list));
		nobj = head->next;
		_dl_free(head);
		head = nobj;
	}
}

void
_dl_remove_object(elf_object_t *object)
{
	object->prev->next = object->next;
	if (object->next)
		object->next->prev = object->prev;

	if (_dl_last_object == object)
		_dl_last_object = object->prev;

	object->next = free_objects;
	free_objects = object;
}


sym_cache *_dl_symcache;
int _dl_symcachestat_hits;
int _dl_symcachestat_lookups;


Elf_Addr
_dl_find_symbol_bysym(elf_object_t *req_obj, unsigned int symidx,
    const Elf_Sym **this, int flags, const Elf_Sym *ref_sym, const elf_object_t **pobj)
{
	Elf_Addr ret;
	const Elf_Sym *sym;
	const char *symn;
	const elf_object_t *sobj;

	_dl_symcachestat_lookups ++;
	if (_dl_symcache != NULL &&
	    symidx < req_obj->nchains &&
	    _dl_symcache[symidx].obj != NULL &&
	    _dl_symcache[symidx].sym != NULL &&
	    _dl_symcache[symidx].flags == flags) {

		_dl_symcachestat_hits++;
		sobj = _dl_symcache[symidx].obj;
		*this = _dl_symcache[symidx].sym;
		if (pobj)
			*pobj = sobj;
		return sobj->obj_base;
	}

	sym = req_obj->dyn.symtab;
	sym += symidx;
	symn = req_obj->dyn.strtab + sym->st_name;

	ret = _dl_find_symbol(symn, this, flags, ref_sym, req_obj, &sobj);

	if (pobj)
		*pobj = sobj;

	if (_dl_symcache != NULL && symidx < req_obj->nchains) {
#if 0
		DL_DEB(("cache miss %d %p %p, %p %p %s %s %d %d %s\n",
		    symidx,
		    _dl_symcache[symidx].sym, *this,
		    _dl_symcache[symidx].obj, sobj, sobj->load_name,
		    sobj->dyn.strtab + (*this)->st_name,
		    _dl_symcache[symidx].flags, flags, req_obj->load_name));
#endif

		_dl_symcache[symidx].sym = *this;
		_dl_symcache[symidx].obj = sobj;
		_dl_symcache[symidx].flags = flags;
	}

	return ret;
}

static int
matched_symbol(elf_object_t *obj, const Elf_Sym *sym, struct symlookup *sl)
{
	switch (ELF_ST_TYPE(sym->st_info)) {
	case STT_FUNC:
		/*
		 * Allow this symbol if we are referring to a function which
		 * has a value, even if section is UNDEF.  This allows &func
		 * to refer to PLT as per the ELF spec.  If flags has SYM_PLT
		 * set, we must have actual symbol, so this symbol is skipped.
		 */
		if ((sl->sl_flags & SYM_PLT) && sym->st_shndx == SHN_UNDEF)
			return 0;
		if (sym->st_value == 0)
			return 0;
		break;
	case STT_NOTYPE:
	case STT_OBJECT:
		if (sym->st_value == 0)
			return 0;
#if 0
		/* FALLTHROUGH */
	case STT_TLS:
#endif
		if (sym->st_shndx == SHN_UNDEF)
			return 0;
		break;
	default:
		return 0;
	}

	if (sym != sl->sl_sym_out &&
	    _dl_strcmp(sl->sl_name, obj->dyn.strtab + sym->st_name))
		return 0;

	if (ELF_ST_BIND(sym->st_info) == STB_GLOBAL) {
		sl->sl_sym_out = sym;
		sl->sl_obj_out = obj;
		return 1;
	} else if (ELF_ST_BIND(sym->st_info) == STB_WEAK) {
		if (sl->sl_weak_sym_out == NULL) {
			sl->sl_weak_sym_out = sym;
			sl->sl_weak_obj_out = obj;
		}
		/* done with this object, but need to check other objects */
		return -1;
	}
	return 0;
}

static int
_dl_find_symbol_obj(elf_object_t *obj, struct symlookup *sl)
{
	const Elf_Sym	*symt = obj->dyn.symtab;

	if (obj->status & STAT_GNU_HASH) {
		uint32_t hash = sl->sl_gnu_hash;
		Elf_Addr bloom_word;
		unsigned int h1;
		unsigned int h2;
		Elf32_Word bucket;
		const Elf32_Word *hashval;

		/* pick right bitmask word from Bloom filter array */
		bloom_word = obj->bloom_gnu[(hash / ELFSIZE) &
		    obj->mask_bm_gnu];

		/* calculate modulus ELFSIZE of gnu hash and its derivative */
		h1 = hash & (ELFSIZE - 1);
		h2 = (hash >> obj->shift2_gnu) & (ELFSIZE - 1);

		/* Filter out the "definitely not in set" queries */
		if (((bloom_word >> h1) & (bloom_word >> h2) & 1) == 0)
			return 0;

		/* Locate hash chain and corresponding value element */
		bucket = obj->buckets_gnu[hash % obj->nbuckets];
		if (bucket == 0)
			return 0;
		hashval = &obj->chains_gnu[bucket];
		do {
			if (((*hashval ^ hash) >> 1) == 0) {
				const Elf_Sym *sym = symt +
				    (hashval - obj->chains_gnu);
				
				int r = matched_symbol(obj, sym, sl);
				if (r)
					return r > 0;
			}
		} while ((*hashval++ & 1U) == 0);
	} else {
		Elf_Word si;

		for (si = obj->buckets_elf[sl->sl_elf_hash % obj->nbuckets];
		    si != STN_UNDEF; si = obj->chains_elf[si]) {
			const Elf_Sym *sym = symt + si;

			int r = matched_symbol(obj, sym, sl);
			if (r)
				return r > 0;
		}
	}
	return 0;
}

Elf_Addr
_dl_find_symbol(const char *name, const Elf_Sym **this,
    int flags, const Elf_Sym *ref_sym, elf_object_t *req_obj,
    const elf_object_t **pobj)
{
	const unsigned char *p;
	unsigned char c;
	struct dep_node *n, *m;
	struct symlookup sl = {
		.sl_name = name,
		.sl_obj_out = NULL,
		.sl_weak_obj_out = NULL,
		.sl_weak_sym_out = NULL,
		.sl_elf_hash = 0,
		.sl_gnu_hash = 5381,
		.sl_flags = flags,
	};

	/* Calculate both hashes in one pass */
	for (p = (const unsigned char *)name; (c = *p) != '\0'; p++) {
		unsigned long g;
		sl.sl_elf_hash = (sl.sl_elf_hash << 4) + c;
		if ((g = sl.sl_elf_hash & 0xf0000000))
			sl.sl_elf_hash ^= g >> 24;
		sl.sl_elf_hash &= ~g;
		sl.sl_gnu_hash = sl.sl_gnu_hash * 33 + c;
	}

	if (req_obj->dyn.symbolic)
		if (_dl_find_symbol_obj(req_obj, &sl))
			goto found;

	if (flags & SYM_DLSYM) {
		if (_dl_find_symbol_obj(req_obj, &sl))
			goto found;

		/* weak definition in the specified object is good enough */
		if (sl.sl_weak_obj_out != NULL)
			goto found;

		/* search dlopened obj and all children */
		TAILQ_FOREACH(n, &req_obj->load_object->grpsym_list, next_sib) {
			if (_dl_find_symbol_obj(n->data, &sl))
				goto found;
		}
	} else {
		int skip = 0;

		if ((flags & SYM_SEARCH_SELF) || (flags & SYM_SEARCH_NEXT))
			skip = 1;

		/*
		 * search dlopened objects: global or req_obj == dlopened_obj
		 * and and it's children
		 */
		TAILQ_FOREACH(n, &_dlopened_child_list, next_sib) {
			if (((n->data->obj_flags & DF_1_GLOBAL) == 0) &&
			    (n->data != req_obj->load_object))
				continue;

			TAILQ_FOREACH(m, &n->data->grpsym_list, next_sib) {
				if (skip == 1) {
					if (m->data == req_obj) {
						skip = 0;
						if (flags & SYM_SEARCH_NEXT)
							continue;
					} else
						continue;
				}
				if ((flags & SYM_SEARCH_OTHER) &&
				    (m->data == req_obj))
					continue;
				if (_dl_find_symbol_obj(m->data, &sl))
					goto found;
			}
		}
	}

found:
	if (sl.sl_sym_out != NULL) {
		*this = sl.sl_sym_out;
	} else if (sl.sl_weak_obj_out != NULL) {
		sl.sl_obj_out = sl.sl_weak_obj_out;
		*this = sl.sl_weak_sym_out;
	} else {
		if ((ref_sym == NULL ||
		    (ELF_ST_BIND(ref_sym->st_info) != STB_WEAK)) &&
		    (flags & SYM_WARNNOTFOUND))
			_dl_printf("%s:%s: undefined symbol '%s'\n",
			    __progname, req_obj->load_name, name);
		return (0);
	}

	if (ref_sym != NULL && ref_sym->st_size != 0 &&
	    (ref_sym->st_size != (*this)->st_size)  &&
	    (ELF_ST_TYPE((*this)->st_info) != STT_FUNC) ) {
		_dl_printf("%s:%s: %s : WARNING: "
		    "symbol(%s) size mismatch, relink your program\n",
		    __progname, req_obj->load_name, sl.sl_obj_out->load_name,
		    name);
	}

	if (pobj != NULL)
		*pobj = sl.sl_obj_out;

	return sl.sl_obj_out->obj_base;
}

void
_dl_debug_state(void)
{
	/* Debugger stub */
}
