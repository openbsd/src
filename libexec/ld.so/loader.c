/*	$OpenBSD: loader.c,v 1.177 2018/12/03 05:29:56 guenther Exp $ */

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

#define	_DYN_LOADER

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/exec.h>
#include <sys/sysctl.h>
#include <nlist.h>
#include <string.h>
#include <link.h>
#include <limits.h>			/* NAME_MAX */
#include <dlfcn.h>

#include "syscall.h"
#include "archdep.h"
#include "path.h"
#include "resolve.h"
#include "sod.h"
#include "stdlib.h"

/*
 * Local decls.
 */
unsigned long _dl_boot(const char **, char **, const long, long *);
void _dl_debug_state(void);
void _dl_setup_env(const char *_argv0, char **_envp);
void _dl_dtors(void);
void _dl_fixup_user_env(void);
void _dl_call_preinit(elf_object_t *);
void _dl_call_init_recurse(elf_object_t *object, int initfirst);

int _dl_pagesz __relro = 4096;
int _dl_bindnow __relro = 0;
int _dl_debug __relro = 0;
int _dl_trust __relro = 0;
char **_dl_libpath __relro = NULL;

/* XXX variables which are only used during boot */
char *_dl_preload __relro = NULL;
char *_dl_tracefmt1 __relro = NULL;
char *_dl_tracefmt2 __relro = NULL;
char *_dl_traceprog __relro = NULL;

int _dl_traceld;
struct r_debug *_dl_debug_map;

void _dl_dopreload(char *paths);

/*
 * Run dtors for a single object.
 */
void
_dl_run_dtors(elf_object_t *obj)
{
	if (obj->dyn.fini_array) {
		int num = obj->dyn.fini_arraysz / sizeof(Elf_Addr);
		int i;

		DL_DEB(("doing finiarray obj %p @%p: [%s]\n",
		    obj, obj->dyn.fini_array, obj->load_name));
		for (i = num; i > 0; i--)
			(*obj->dyn.fini_array[i-1])();
	}

	if (obj->dyn.fini) {
		DL_DEB(("doing dtors obj %p @%p: [%s]\n",
		    obj, obj->dyn.fini, obj->load_name));
		(*obj->dyn.fini)();
	}
}

/*
 * Run dtors for all objects that are eligible.
 */
void
_dl_run_all_dtors(void)
{
	elf_object_t *node;
	struct dep_node *dnode;
	int fini_complete;
	int skip_initfirst;
	int initfirst_skipped;

	fini_complete = 0;
	skip_initfirst = 1;
	initfirst_skipped = 0;

	while (fini_complete == 0) {
		fini_complete = 1;
		for (node = _dl_objects;
		    node != NULL;
		    node = node->next) {
			if ((node->dyn.fini || node->dyn.fini_array) &&
			    (OBJECT_REF_CNT(node) == 0) &&
			    (node->status & STAT_INIT_DONE) &&
			    ((node->status & STAT_FINI_DONE) == 0)) {
				if (skip_initfirst &&
				    (node->obj_flags & DF_1_INITFIRST))
					initfirst_skipped = 1;
				else
					node->status |= STAT_FINI_READY;
			    }
		}
		for (node = _dl_objects;
		    node != NULL;
		    node = node->next ) {
			if ((node->dyn.fini || node->dyn.fini_array) &&
			    (OBJECT_REF_CNT(node) == 0) &&
			    (node->status & STAT_INIT_DONE) &&
			    ((node->status & STAT_FINI_DONE) == 0) &&
			    (!skip_initfirst ||
			    (node->obj_flags & DF_1_INITFIRST) == 0))
				TAILQ_FOREACH(dnode, &node->child_list,
				    next_sib)
					dnode->data->status &= ~STAT_FINI_READY;
		}


		for (node = _dl_objects;
		    node != NULL;
		    node = node->next ) {
			if (node->status & STAT_FINI_READY) {
				fini_complete = 0;
				node->status |= STAT_FINI_DONE;
				node->status &= ~STAT_FINI_READY;
				_dl_run_dtors(node);
			}
		}

		if (fini_complete && initfirst_skipped)
			fini_complete = initfirst_skipped = skip_initfirst = 0;
	}
}

/*
 * Routine to walk through all of the objects except the first
 * (main executable).
 *
 * Big question, should dlopen()ed objects be unloaded before or after
 * the destructor for the main application runs?
 */
void
_dl_dtors(void)
{
	_dl_thread_kern_stop();

	/* ORDER? */
	_dl_unload_dlopen();

	DL_DEB(("doing dtors\n"));

	_dl_objects->opencount--;
	_dl_notify_unload_shlib(_dl_objects);

	_dl_run_all_dtors();
}

void
_dl_dopreload(char *paths)
{
	char		*cp, *dp;
	elf_object_t	*shlib;

	dp = paths = _dl_strdup(paths);
	if (dp == NULL)
		_dl_oom();

	while ((cp = _dl_strsep(&dp, ":")) != NULL) {
		shlib = _dl_load_shlib(cp, _dl_objects, OBJTYPE_LIB,
		_dl_objects->obj_flags);
		if (shlib == NULL)
			_dl_die("can't preload library '%s'", cp);
		_dl_add_object(shlib);
		_dl_link_child(shlib, _dl_objects);
	}
	_dl_free(paths);
	return;
}

/*
 * grab interesting environment variables, zap bad env vars if
 * issetugid, and set the exported environ and __progname variables
 */
char **environ = NULL;
char *__progname = NULL;
void
_dl_setup_env(const char *argv0, char **envp)
{
	static char progname_storage[NAME_MAX+1] = "";

	/*
	 * Get paths to various things we are going to use.
	 */
	_dl_debug = _dl_getenv("LD_DEBUG", envp) != NULL;
	_dl_libpath = _dl_split_path(_dl_getenv("LD_LIBRARY_PATH", envp));
	_dl_preload = _dl_getenv("LD_PRELOAD", envp);
	_dl_bindnow = _dl_getenv("LD_BIND_NOW", envp) != NULL;
	_dl_traceld = _dl_getenv("LD_TRACE_LOADED_OBJECTS", envp) != NULL;
	_dl_tracefmt1 = _dl_getenv("LD_TRACE_LOADED_OBJECTS_FMT1", envp);
	_dl_tracefmt2 = _dl_getenv("LD_TRACE_LOADED_OBJECTS_FMT2", envp);
	_dl_traceprog = _dl_getenv("LD_TRACE_LOADED_OBJECTS_PROGNAME", envp);

	/*
	 * Don't allow someone to change the search paths if he runs
	 * a suid program without credentials high enough.
	 */
	_dl_trust = !_dl_issetugid();
	if (!_dl_trust) {	/* Zap paths if s[ug]id... */
		if (_dl_libpath) {
			_dl_free_path(_dl_libpath);
			_dl_libpath = NULL;
			_dl_unsetenv("LD_LIBRARY_PATH", envp);
		}
		if (_dl_preload) {
			_dl_preload = NULL;
			_dl_unsetenv("LD_PRELOAD", envp);
		}
		if (_dl_bindnow) {
			_dl_bindnow = 0;
			_dl_unsetenv("LD_BIND_NOW", envp);
		}
		if (_dl_debug) {
			_dl_debug = 0;
			_dl_unsetenv("LD_DEBUG", envp);
		}
	}
	environ = envp;

	_dl_trace_setup(envp);

	if (argv0 != NULL) {		/* NULL ptr if argc = 0 */
		const char *p = _dl_strrchr(argv0, '/');

		if (p == NULL)
			p = argv0;
		else
			p++;
		_dl_strlcpy(progname_storage, p, sizeof(progname_storage));
	}
	__progname = progname_storage;
}

int
_dl_load_dep_libs(elf_object_t *object, int flags, int booting)
{
	elf_object_t *dynobj;
	Elf_Dyn *dynp;
	unsigned int loop;
	int libcount;
	int depflags;

	dynobj = object;
	while (dynobj) {
		DL_DEB(("examining: '%s'\n", dynobj->load_name));
		libcount = 0;

		/* propagate DF_1_NOW to deplibs (can be set by dynamic tags) */
		depflags = flags | (dynobj->obj_flags & DF_1_NOW);

		for (dynp = dynobj->load_dyn; dynp->d_tag; dynp++) {
			if (dynp->d_tag == DT_NEEDED) {
				libcount++;
			}
		}

		if ( libcount != 0) {
			struct listent {
				Elf_Dyn *dynp;
				elf_object_t *depobj;
			} *liblist;
			int *randomlist;

			liblist = _dl_reallocarray(NULL, libcount,
			    sizeof(struct listent));
			randomlist =  _dl_reallocarray(NULL, libcount,
			    sizeof(int));

			if (liblist == NULL || randomlist == NULL)
				_dl_oom();

			for (dynp = dynobj->load_dyn, loop = 0; dynp->d_tag;
			    dynp++)
				if (dynp->d_tag == DT_NEEDED)
					liblist[loop++].dynp = dynp;

			/* Randomize these */
			for (loop = 0; loop < libcount; loop++)
				randomlist[loop] = loop;

			for (loop = 1; loop < libcount; loop++) {
				unsigned int rnd;
				int cur;
				rnd = _dl_arc4random();
				rnd = rnd % (loop+1);
				cur = randomlist[rnd];
				randomlist[rnd] = randomlist[loop];
				randomlist[loop] = cur;
			}

			for (loop = 0; loop < libcount; loop++) {
				elf_object_t *depobj;
				const char *libname;
				libname = dynobj->dyn.strtab;
				libname +=
				    liblist[randomlist[loop]].dynp->d_un.d_val;
				DL_DEB(("loading: %s required by %s\n", libname,
				    dynobj->load_name));
				depobj = _dl_load_shlib(libname, dynobj,
				    OBJTYPE_LIB, depflags);
				if (depobj == 0) {
					if (booting) {
						_dl_die(
						    "can't load library '%s'",
						    libname);
					}
					DL_DEB(("dlopen: failed to open %s\n",
					    libname));
					_dl_free(liblist);
					_dl_free(randomlist);
					return (1);
				}
				liblist[randomlist[loop]].depobj = depobj;
			}

			for (loop = 0; loop < libcount; loop++) {
				_dl_add_object(liblist[loop].depobj);
				_dl_link_child(liblist[loop].depobj, dynobj);
			}
			_dl_free(liblist);
			_dl_free(randomlist);
		}
		dynobj = dynobj->next;
	}

	/* add first object manually */
	_dl_link_grpsym(object, 1);
	_dl_cache_grpsym_list_setup(object);

	return(0);
}


/* do any RWX -> RX fixups for executable PLTs and apply GNU_RELRO */
static inline void
_dl_self_relro(long loff)
{
	Elf_Ehdr *ehdp;
	Elf_Phdr *phdp;
	int i;

	ehdp = (Elf_Ehdr *)loff;
	phdp = (Elf_Phdr *)(loff + ehdp->e_phoff);
	for (i = 0; i < ehdp->e_phnum; i++, phdp++) {
		switch (phdp->p_type) {
#if defined(__alpha__) || defined(__hppa__) || defined(__powerpc__) || \
    defined(__sparc64__)
		case PT_LOAD:
			if ((phdp->p_flags & (PF_X | PF_W)) != (PF_X | PF_W))
				break;
			_dl_mprotect((void *)(phdp->p_vaddr + loff),
			    phdp->p_memsz, PROT_READ);
			break;
#endif
		case PT_GNU_RELRO:
			_dl_mprotect((void *)(phdp->p_vaddr + loff),
			    phdp->p_memsz, PROT_READ);
			break;
		}
	}
}


#define PFLAGS(X) ((((X) & PF_R) ? PROT_READ : 0) | \
		   (((X) & PF_W) ? PROT_WRITE : 0) | \
		   (((X) & PF_X) ? PROT_EXEC : 0))

/*
 * This is the dynamic loader entrypoint. When entering here, depending
 * on architecture type, the stack and registers are set up according
 * to the architectures ABI specification. The first thing required
 * to do is to dig out all information we need to accomplish our task.
 */
unsigned long
_dl_boot(const char **argv, char **envp, const long dyn_loff, long *dl_data)
{
	struct elf_object *exe_obj;	/* Pointer to executable object */
	struct elf_object *dyn_obj;	/* Pointer to ld.so object */
	struct r_debug **map_link;	/* Where to put pointer for gdb */
	struct r_debug *debug_map;
	struct load_list *next_load, *load_list = NULL;
	Elf_Dyn *dynp;
	Elf_Phdr *phdp;
	Elf_Ehdr *ehdr;
	char *us = NULL;
	unsigned int loop;
	int failed;
	struct dep_node *n;
	Elf_Addr minva, maxva, exe_loff;
	Elf_Phdr *ptls = NULL;
	int align;

	if (dl_data[AUX_pagesz] != 0)
		_dl_pagesz = dl_data[AUX_pagesz];
	_dl_malloc_init();
	_dl_setup_env(argv[0], envp);

	/*
	 * Make read-only the GOT and PLT and variables initialized
	 * during the ld.so setup above.
	 */
	_dl_self_relro(dyn_loff);

	align = _dl_pagesz - 1;

#define ROUND_PG(x) (((x) + align) & ~(align))
#define TRUNC_PG(x) ((x) & ~(align))

	if (_dl_bindnow) {
		/* Lazy binding disabled, so disable kbind */
		_dl___syscall(SYS_kbind, (void *)NULL, (size_t)0, (long long)0);
	}

	DL_DEB(("ld.so loading: '%s'\n", __progname));

	/* init this in runtime, not statically */
	TAILQ_INIT(&_dlopened_child_list);

	exe_obj = NULL;
	_dl_loading_object = NULL;

	minva = ELF_NO_ADDR;
	maxva = exe_loff = 0;

	/*
	 * Examine the user application and set up object information.
	 */
	phdp = (Elf_Phdr *)dl_data[AUX_phdr];
	for (loop = 0; loop < dl_data[AUX_phnum]; loop++) {
		switch (phdp->p_type) {
		case PT_PHDR:
			exe_loff = (Elf_Addr)dl_data[AUX_phdr] - phdp->p_vaddr;
			us += exe_loff;
			DL_DEB(("exe load offset:  0x%lx\n", exe_loff));
			break;
		case PT_DYNAMIC:
			minva = TRUNC_PG(minva);
			maxva = ROUND_PG(maxva);
			exe_obj = _dl_finalize_object(argv[0] ? argv[0] : "",
			    (Elf_Dyn *)(phdp->p_vaddr + exe_loff),
			    (Elf_Phdr *)dl_data[AUX_phdr],
			    dl_data[AUX_phnum], OBJTYPE_EXE, minva + exe_loff,
			    exe_loff);
			_dl_add_object(exe_obj);
			break;
		case PT_INTERP:
			us += phdp->p_vaddr;
			break;
		case PT_LOAD:
			if (phdp->p_vaddr < minva)
				minva = phdp->p_vaddr;
			if (phdp->p_vaddr > maxva)
				maxva = phdp->p_vaddr + phdp->p_memsz;

			next_load = _dl_calloc(1, sizeof(struct load_list));
			if (next_load == NULL)
				_dl_oom();
			next_load->next = load_list;
			load_list = next_load;
			next_load->start = (char *)TRUNC_PG(phdp->p_vaddr) + exe_loff;
			next_load->size = (phdp->p_vaddr & align) + phdp->p_filesz;
			next_load->prot = PFLAGS(phdp->p_flags);
			break;
		case PT_TLS:
			if (phdp->p_filesz > phdp->p_memsz)
				_dl_die("invalid tls data");
			ptls = phdp;
			break;
		case PT_GNU_RELRO:
			exe_obj->relro_addr = phdp->p_vaddr + exe_loff;
			exe_obj->relro_size = phdp->p_memsz;
			break;
		}
		phdp++;
	}
	exe_obj->load_list = load_list;
	exe_obj->obj_flags |= DF_1_GLOBAL;
	exe_obj->load_size = maxva - minva;
	_dl_set_sod(exe_obj->load_name, &exe_obj->sod);

	/* TLS bits in the base executable */
	if (ptls != NULL && ptls->p_memsz)
		_dl_set_tls(exe_obj, ptls, exe_loff, NULL);

	n = _dl_malloc(sizeof *n);
	if (n == NULL)
		_dl_oom();
	n->data = exe_obj;
	TAILQ_INSERT_TAIL(&_dlopened_child_list, n, next_sib);
	exe_obj->opencount++;

	if (_dl_preload != NULL)
		_dl_dopreload(_dl_preload);

	_dl_load_dep_libs(exe_obj, exe_obj->obj_flags, 1);

	/*
	 * Now add the dynamic loader itself last in the object list
	 * so we can use the _dl_ code when serving dl.... calls.
	 * Intentionally left off the exe child_list.
	 */
	dynp = (Elf_Dyn *)((void *)_DYNAMIC);
	ehdr = (Elf_Ehdr *)dl_data[AUX_base];
	dyn_obj = _dl_finalize_object(us, dynp,
	    (Elf_Phdr *)((char *)dl_data[AUX_base] + ehdr->e_phoff),
	    ehdr->e_phnum, OBJTYPE_LDR, dl_data[AUX_base], dyn_loff);
	_dl_add_object(dyn_obj);

	dyn_obj->refcount++;
	_dl_link_grpsym(dyn_obj, 1);

	dyn_obj->status |= STAT_RELOC_DONE;
	_dl_set_sod(dyn_obj->load_name, &dyn_obj->sod);

	/* calculate the offsets for static TLS allocations */
	_dl_allocate_tls_offsets();

	/*
	 * Make something to help gdb when poking around in the code.
	 * Do this poking at the .dynamic section now, before relocation
	 * renders it read-only
	 */
	map_link = NULL;
#ifdef __mips__
	if (exe_obj->Dyn.info[DT_MIPS_RLD_MAP - DT_LOPROC + DT_NUM] != 0)
		map_link = (struct r_debug **)(exe_obj->Dyn.info[
		    DT_MIPS_RLD_MAP - DT_LOPROC + DT_NUM] + exe_loff);
#endif
	if (map_link == NULL) {
		for (dynp = exe_obj->load_dyn; dynp->d_tag; dynp++) {
			if (dynp->d_tag == DT_DEBUG) {
				map_link = (struct r_debug **)&dynp->d_un.d_ptr;
				break;
			}
		}
		if (dynp->d_tag != DT_DEBUG)
			DL_DEB(("failed to mark DTDEBUG\n"));
	}
	if (map_link) {
		debug_map = _dl_malloc(sizeof(*debug_map));
		if (debug_map == NULL)
			_dl_oom();
		debug_map->r_version = 1;
		debug_map->r_map = (struct link_map *)_dl_objects;
		debug_map->r_brk = (Elf_Addr)_dl_debug_state;
		debug_map->r_state = RT_CONSISTENT;
		debug_map->r_ldbase = dyn_loff;
		_dl_debug_map = debug_map;
#ifdef __mips__
		Elf_Addr relro_addr = exe_obj->relro_addr;
		if (dynp->d_tag == DT_DEBUG &&
		    ((Elf_Addr)map_link + sizeof(*map_link) <= relro_addr ||
		     (Elf_Addr)map_link >= relro_addr + exe_obj->relro_size)) {
			_dl_mprotect(map_link, sizeof(*map_link),
			    PROT_READ|PROT_WRITE);
			*map_link = _dl_debug_map;
			_dl_mprotect(map_link, sizeof(*map_link),
			    PROT_READ|PROT_EXEC);
		} else
#endif
			*map_link = _dl_debug_map;
	}


	/*
	 * Everything should be in place now for doing the relocation
	 * and binding. Call _dl_rtld to do the job. Fingers crossed.
	 */

	failed = 0;
	if (!_dl_traceld)
		failed = _dl_rtld(_dl_objects);

	if (_dl_debug || _dl_traceld) {
		if (_dl_traceld)
			_dl_pledge("stdio rpath", NULL);
		_dl_show_objects();
	}

	DL_DEB(("dynamic loading done, %s.\n",
	    (failed == 0) ? "success":"failed"));

	if (failed != 0)
		_dl_die("relocation failed");

	if (_dl_traceld)
		_dl_exit(0);

	_dl_loading_object = NULL;

	/* set up the TIB for the initial thread */
	_dl_allocate_first_tib();

	_dl_fixup_user_env();

	_dl_debug_state();

	/*
	 * Do not run init code if run from ldd.
	 */
	if (_dl_objects->next != NULL) {
		_dl_call_preinit(_dl_objects);
		_dl_call_init(_dl_objects);
	}

	DL_DEB(("entry point: 0x%lx\n", dl_data[AUX_entry]));

	/*
	 * Return the entry point.
	 */
	return(dl_data[AUX_entry]);
}

#define DL_SM_SYMBUF_CNT 512
sym_cache _dl_sm_symcache_buffer[DL_SM_SYMBUF_CNT];

int
_dl_rtld(elf_object_t *object)
{
	size_t sz;
	struct load_list *llist;
	int fails = 0;

	if (object->next)
		fails += _dl_rtld(object->next);

	if (object->status & STAT_RELOC_DONE)
		return 0;

	sz = 0;
	if (object->nchains < DL_SM_SYMBUF_CNT) {
		_dl_symcache = _dl_sm_symcache_buffer;
//		DL_DEB(("using static buffer for %d entries\n",
//		    object->nchains));
		_dl_memset(_dl_symcache, 0,
		    sizeof (sym_cache) * object->nchains);
	} else {
		sz = ELF_ROUND(sizeof (sym_cache) * object->nchains,
		    _dl_pagesz);
//		DL_DEB(("allocating symcache sz %x with mmap\n", sz));

		_dl_symcache = (void *)_dl_mmap(0, sz, PROT_READ|PROT_WRITE,
		    MAP_PRIVATE|MAP_ANON, -1, 0);
		if (_dl_mmap_error(_dl_symcache)) {
			sz = 0;
			_dl_symcache = NULL;
		}
	}

	/*
	 * Do relocation information first, then GOT.
	 */
	fails =_dl_md_reloc(object, DT_REL, DT_RELSZ);
	fails += _dl_md_reloc(object, DT_RELA, DT_RELASZ);
	fails += _dl_md_reloc_got(object, !(_dl_bindnow ||
	    object->obj_flags & DF_1_NOW));

	/*
	 * Handle GNU_RELRO
	 */
	if (object->relro_addr != 0 && object->relro_size != 0) {
		Elf_Addr addr = object->relro_addr;

		DL_DEB(("protect RELRO [0x%lx,0x%lx) in %s\n",
		    addr, addr + object->relro_size, object->load_name));
		_dl_mprotect((void *)addr, object->relro_size, PROT_READ);
	}

	/*
	 * Look for W&X segments and make them read-only.
	 */
	for (llist = object->load_list; llist != NULL; llist = llist->next) {
		if ((llist->prot & PROT_WRITE) && (llist->prot & PROT_EXEC)) {
			_dl_mprotect(llist->start, llist->size,
			    llist->prot & ~PROT_WRITE);
		}
	}

	if (_dl_symcache != NULL) {
		if (sz != 0)
			_dl_munmap( _dl_symcache, sz);
		_dl_symcache = NULL;
	}
	if (fails == 0)
		object->status |= STAT_RELOC_DONE;

	return (fails);
}

void
_dl_call_preinit(elf_object_t *object)
{
	if (object->dyn.preinit_array) {
		int num = object->dyn.preinit_arraysz / sizeof(Elf_Addr);
		int i;

		DL_DEB(("doing preinitarray obj %p @%p: [%s]\n",
		    object, object->dyn.preinit_array, object->load_name));
		for (i = 0; i < num; i++)
			(*object->dyn.preinit_array[i])();
	}
}

void
_dl_call_init(elf_object_t *object)
{
	_dl_call_init_recurse(object, 1);
	_dl_call_init_recurse(object, 0);
}

void
_dl_call_init_recurse(elf_object_t *object, int initfirst)
{
	struct dep_node *n;

	object->status |= STAT_VISITED;

	TAILQ_FOREACH(n, &object->child_list, next_sib) {
		if (n->data->status & STAT_VISITED)
			continue;
		_dl_call_init_recurse(n->data, initfirst);
	}

	object->status &= ~STAT_VISITED;

	if (object->status & STAT_INIT_DONE)
		return;

	if (initfirst && (object->obj_flags & DF_1_INITFIRST) == 0)
		return;

	if (object->dyn.init) {
		DL_DEB(("doing ctors obj %p @%p: [%s]\n",
		    object, object->dyn.init, object->load_name));
		(*object->dyn.init)();
	}

	if (object->dyn.init_array) {
		int num = object->dyn.init_arraysz / sizeof(Elf_Addr);
		int i;

		DL_DEB(("doing initarray obj %p @%p: [%s]\n",
		    object, object->dyn.init_array, object->load_name));
		for (i = 0; i < num; i++)
			(*object->dyn.init_array[i])();
	}

	object->status |= STAT_INIT_DONE;
}

char *
_dl_getenv(const char *var, char **env)
{
	const char *ep;

	while ((ep = *env++)) {
		const char *vp = var;

		while (*vp && *vp == *ep) {
			vp++;
			ep++;
		}
		if (*vp == '\0' && *ep++ == '=')
			return((char *)ep);
	}
	return(NULL);
}

void
_dl_unsetenv(const char *var, char **env)
{
	char *ep;

	while ((ep = *env)) {
		const char *vp = var;

		while (*vp && *vp == *ep) {
			vp++;
			ep++;
		}
		if (*vp == '\0' && *ep++ == '=') {
			char **P;

			for (P = env;; ++P)
				if (!(*P = *(P + 1)))
					break;
		} else
			env++;
	}
}

/*
 * _dl_fixup_user_env()
 *
 * Set the user environment so that programs can use the environment
 * while running constructors. Specifically, MALLOC_OPTIONS= for malloc()
 */
void
_dl_fixup_user_env(void)
{
	const struct elf_object *obj;
	const Elf_Sym *sym;
	Elf_Addr ooff;
	struct elf_object dummy_obj;

	dummy_obj.dyn.symbolic = 0;
	dummy_obj.load_name = "ld.so";

	sym = NULL;
	ooff = _dl_find_symbol("environ", &sym,
	    SYM_SEARCH_ALL|SYM_NOWARNNOTFOUND|SYM_PLT, NULL, &dummy_obj, &obj);
	if (sym != NULL) {
		DL_DEB(("setting environ %p@%s[%p] from %p\n",
		    (void *)(sym->st_value + ooff), obj->load_name,
		    (void *)obj, (void *)&environ));
		if ((char ***)(sym->st_value + ooff) != &environ)
			*((char ***)(sym->st_value + ooff)) = environ;
	}

	sym = NULL;
	ooff = _dl_find_symbol("__progname", &sym,
	    SYM_SEARCH_ALL|SYM_NOWARNNOTFOUND|SYM_PLT, NULL, &dummy_obj, &obj);
	if (sym != NULL) {
		DL_DEB(("setting __progname %p@%s[%p] from %p\n",
		    (void *)(sym->st_value + ooff), obj->load_name,
		    (void *)obj, (void *)&__progname));
		if ((char **)(sym->st_value + ooff) != &__progname)
			*((char **)(sym->st_value + ooff)) = __progname;
	}
}
