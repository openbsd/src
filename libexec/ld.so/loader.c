/*	$OpenBSD: loader.c,v 1.50 2002/10/21 16:01:55 drahn Exp $ */

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom, Opsycon AB, Sweden.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#include <nlist.h>
#include <string.h>
#include <link.h>

#include "syscall.h"
#include "archdep.h"
#include "resolve.h"
#include "sod.h"

/*
 * Local decls.
 */
static char *_dl_getenv(const char *var, char **env);
static void _dl_unsetenv(const char *var, char **env);

const char *_dl_progname;
int  _dl_pagesz;

char *_dl_libpath;
char *_dl_preload;
char *_dl_bindnow;
char *_dl_traceld;
char *_dl_debug;
char *_dl_showmap;

struct r_debug *_dl_debug_map;

void _dl_dopreload(char *paths);

void
_dl_debug_state(void)
{
	/* Debugger stub */
}

/*
 * Routine to walk through all of the objects except the first
 * (main executable).
 */
void
_dl_run_dtors(elf_object_t *object)
{
	DL_DEB(("doing dtors: [%s]\n", object->load_name));
	if (object->dyn.fini)
		(*object->dyn.fini)();
	if (object->next)
		_dl_run_dtors(object->next);
}

void
_dl_dtors(void)
{
	DL_DEB(("doing dtors\n"));
	if (_dl_objects->next)
		_dl_run_dtors(_dl_objects->next);
}

void
_dl_dopreload(paths)
	char		*paths;
{
	char		*cp, *dp;

	dp = paths = _dl_strdup(paths);
	if (dp == NULL) {
		_dl_printf("preload: out of memory");
		_dl_exit(1);
	}

	while ((cp = _dl_strsep(&dp, ":")) != NULL) {
		if (_dl_load_shlib(cp, _dl_objects, OBJTYPE_LIB) == 0) {
			_dl_printf("%s: can't load library '%s'\n",
			    _dl_progname, cp);
			_dl_exit(4);
		}
	}
	_dl_free(paths);
	return;
}

/*
 * This is the dynamic loader entrypoint. When entering here, depending
 * on architecture type, the stack and registers are set up according
 * to the architectures ABI specification. The first thing required
 * to do is to dig out all information we need to accomplish our task.
 */
unsigned long
_dl_boot(const char **argv, char **envp, const long loff, long *dl_data)
{
	struct elf_object *exe_obj;	/* Pointer to executable object */
	struct elf_object *dyn_obj;	/* Pointer to executable object */
	struct r_debug **map_link;	/* Where to put pointer for gdb */
	struct r_debug *debug_map;
	Elf_Dyn *dynp;
	elf_object_t *dynobj;
	Elf_Phdr *phdp;
	char *us = "";
	int n;

	/*
	 * Get paths to various things we are going to use.
	 */
	_dl_libpath = _dl_getenv("LD_LIBRARY_PATH", envp);
	_dl_preload = _dl_getenv("LD_PRELOAD", envp);
	_dl_bindnow = _dl_getenv("LD_BIND_NOW", envp);
	_dl_traceld = _dl_getenv("LD_TRACE_LOADED_OBJECTS", envp);
	_dl_debug = _dl_getenv("LD_DEBUG", envp);

	/*
	 * Don't allow someone to change the search paths if he runs
	 * a suid program without credentials high enough.
	 */
	if (_dl_issetugid()) {	/* Zap paths if s[ug]id... */
		if (_dl_libpath) {
			_dl_libpath = NULL;
			_dl_unsetenv("LD_LIBRARY_PATH", envp);
		}
		if (_dl_preload) {
			_dl_preload = NULL;
			_dl_unsetenv("LD_PRELOAD", envp);
		}
		if (_dl_bindnow) {
			_dl_bindnow = NULL;
			_dl_unsetenv("LD_BIND_NOW", envp);
		}
		if (_dl_debug) {
			_dl_debug = NULL;
			_dl_unsetenv("LD_DEBUG", envp);
		}
	}

	_dl_progname = argv[0];
	if (dl_data[AUX_pagesz] != 0)
		_dl_pagesz = dl_data[AUX_pagesz];
	else
		_dl_pagesz = 4096;

	DL_DEB(("rtld loading: '%s'\n", _dl_progname));

	exe_obj = NULL;
	/*
	 * Examine the user application and set up object information.
	 */
	phdp = (Elf_Phdr *)dl_data[AUX_phdr];
	for (n = 0; n < dl_data[AUX_phnum]; n++) {
		if (phdp->p_type == PT_DYNAMIC) {
			exe_obj = _dl_add_object(argv[0],
			    (Elf_Dyn *)phdp->p_vaddr, dl_data, OBJTYPE_EXE,
			    0, 0);
		}
		if (phdp->p_type == PT_INTERP)
			us = _dl_strdup((char *)phdp->p_vaddr);
		phdp++;
	}

	if (_dl_preload != NULL)
		_dl_dopreload(_dl_preload);

	/*
	 * Now, pick up and 'load' all libraries requierd. Start
	 * with the first on the list and then do whatever gets
	 * added along the tour.
	 */
	dynobj = _dl_objects;
	while (dynobj) {
		DL_DEB(("examining: '%s'\n", dynobj->load_name));
		for (dynp = dynobj->load_dyn; dynp->d_tag; dynp++) {
			const char *libname;

			if (dynp->d_tag != DT_NEEDED)
				continue;
			libname = dynobj->dyn.strtab;
			libname += dynp->d_un.d_val;
			DL_DEB(("needs: '%s'\n", libname));
			if (_dl_load_shlib(libname, dynobj, OBJTYPE_LIB) == 0) {
				_dl_printf("%s: can't load library '%s'\n",
				    _dl_progname, libname);
				_dl_exit(4);
			}
		}
		dynobj = dynobj->next;
	}

	/*
	 * Now add the dynamic loader itself last in the object list
	 * so we can use the _dl_ code when serving dl.... calls.
	 */
	dynp = (Elf_Dyn *)((void *)_DYNAMIC);
	dyn_obj = _dl_add_object(us, dynp, 0, OBJTYPE_LDR, dl_data[AUX_base], loff);
	dyn_obj->status |= STAT_RELOC_DONE;

	/*
	 * Everything should be in place now for doing the relocation
	 * and binding. Call _dl_rtld to do the job. Fingers crossed.
	 */
	if (_dl_traceld == NULL)
		_dl_rtld(_dl_objects);

	/*
	 * The first object is the executable itself,
	 * it is responsible for running it's own ctors/dtors
	 * thus do NOT run the ctors for the executable, all of
	 * the shared libraries which follow.
	 * Do not run init code if run from ldd.
	 */
	if ((_dl_traceld == NULL) && (_dl_objects->next != NULL))
		_dl_call_init(_dl_objects->next);

	/*
	 * Schedule a routine to be run at shutdown, by using atexit.
	 * Cannot call atexit directly from ld.so?
	 * Do not schedule destructors if run from ldd.
	 */
	if (_dl_traceld == NULL) {
		const Elf_Sym *sym;
		Elf_Addr ooff;

		sym = NULL;
		ooff = _dl_find_symbol("atexit", _dl_objects, &sym,
		    SYM_SEARCH_ALL|SYM_NOWARNNOTFOUND|SYM_PLT, 0);
		if (sym == NULL) {
			_dl_printf("cannot find atexit, destructors will not be run!\n");
		} else {
			(*(void (*)(Elf_Addr))(sym->st_value + ooff))((Elf_Addr)_dl_dtors);
		}
	}

	/*
	 * Finally make something to help gdb when poking around in the code.
	 */
#ifdef __mips__
	map_link = (struct r_debug **)(exe_obj->Dyn.info[DT_MIPS_RLD_MAP - DT_LOPROC + DT_NUM]);
#else
	map_link = NULL;
	for (dynp = exe_obj->load_dyn; dynp->d_tag; dynp++) {
		if (dynp->d_tag == DT_DEBUG) {
			map_link = (struct r_debug **)&dynp->d_un.d_ptr;
			break;
		}
	}
	if (dynp->d_tag != DT_DEBUG) {
		DL_DEB(("failed to mark DTDEBUG\n"));
	}
#endif
	if (map_link) {
		debug_map = (struct r_debug *)_dl_malloc(sizeof(*debug_map));
		debug_map->r_version = 1;
		debug_map->r_map = (struct link_map *)_dl_objects;
		debug_map->r_brk = (Elf_Addr)_dl_debug_state;
		debug_map->r_state = RT_CONSISTENT;
		debug_map->r_ldbase = loff;
		_dl_debug_map = debug_map;
		*map_link = _dl_debug_map;
	}

	_dl_debug_state();

	if (_dl_debug || _dl_traceld) {
		_dl_show_objects();
		DL_DEB(("dynamic loading done.\n"));
	}
	if (_dl_traceld)
		_dl_exit(0);

	DL_DEB(("entry point: 0x%lx\n", dl_data[AUX_entry]));
	/*
	 * Return the entry point.
	 */
	return(dl_data[AUX_entry]);
}

void
_dl_boot_bind(const long sp, long *dl_data)
{
	AuxInfo		*auxstack;
	long		*stack;
	Elf_Dyn		*dynp;
	int		n;
	int argc;
	char **argv;
	char **envp;
	long loff;
	struct elf_object  dynld;	/* Resolver data for the loader */

	/*
	 * Scan argument and environment vectors. Find dynamic
	 * data vector put after them.
	 */
	stack = (long *)sp;
	argc = *stack++;
	argv = (char **)stack;
	envp = &argv[argc + 1];
	stack = (long *)envp;
	while (*stack++ != NULL)
		;

	/*
	 * Zero out dl_data.
	 */
	for (n = 0; n < AUX_entry; n++)
		dl_data[n] = 0;

	/*
	 * Dig out auxiliary data set up by exec call. Move all known
	 * tags to an indexed local table for easy access.
	 */
	for (auxstack = (AuxInfo *)stack; auxstack->au_id != AUX_null;
	    auxstack++) {
		if (auxstack->au_id > AUX_entry)
			continue;
		dl_data[auxstack->au_id] = auxstack->au_v;
	}
	loff = dl_data[AUX_base];

	/*
	 * We need to do 'selfreloc' in case the code weren't
	 * loaded at the address it was linked to.
	 *
	 * Scan the DYNAMIC section for the loader.
	 * Cache the data for easier access.
	 */

#if defined(__alpha__)
	dynp = (Elf_Dyn *)((long)_DYNAMIC);
#else
	dynp = (Elf_Dyn *)((long)_DYNAMIC + loff);
#endif
	while (dynp != NULL && dynp->d_tag != DT_NULL) {
		if (dynp->d_tag < DT_LOPROC)
			dynld.Dyn.info[dynp->d_tag] = dynp->d_un.d_val;
		else if (dynp->d_tag >= DT_LOPROC && dynp->d_tag < DT_LOPROC + DT_NUM)
			dynld.Dyn.info[dynp->d_tag + DT_NUM - DT_LOPROC] =
			    dynp->d_un.d_val;
		if (dynp->d_tag == DT_TEXTREL)
			dynld.dyn.textrel = 1;
		dynp++;
	}

	/*
	 * Do the 'bootstrap relocation'. This is really only needed if
	 * the code was loaded at another location than it was linked to.
	 * We don't do undefined symbols resolving (to difficult..)
	 */

	/* "relocate" dyn.X values if they represent addresses */
	{
		int i, val;
		/* must be code, not pic data */
		int table[20];

		i = 0;
		table[i++] = DT_PLTGOT;
		table[i++] = DT_HASH;
		table[i++] = DT_STRTAB;
		table[i++] = DT_SYMTAB;
		table[i++] = DT_RELA;
		table[i++] = DT_INIT;
		table[i++] = DT_FINI;
		table[i++] = DT_REL;
		table[i++] = DT_JMPREL;
		/* other processors insert their extras here */
		table[i++] = DT_NULL;
		for (i = 0; table[i] != DT_NULL; i++) {
			val = table[i];
			if (val > DT_HIPROC) /* ??? */
				continue;
			if (val > DT_LOPROC)
				val -= DT_LOPROC + DT_NUM;
			if (dynld.Dyn.info[val] != 0)
				dynld.Dyn.info[val] += loff;
		}

	}

	{
		u_int32_t rs;
		Elf_Rel *rp;
		int	i;

		rp = (Elf_Rel *)(dynld.Dyn.info[DT_REL]);
		rs = dynld.dyn.relsz;

		for (i = 0; i < rs; i += sizeof (Elf_Rel)) {
			Elf_Addr *ra;
			const Elf_Sym *sp;

			sp = dynld.dyn.symtab;
			sp += ELF_R_SYM(rp->r_info);

			if (ELF_R_SYM(rp->r_info) && sp->st_value == 0) {
#if 0
/* cannot printf in this function */
				_dl_wrstderr("Dynamic loader failure: self bootstrapping impossible.\n");
				_dl_wrstderr("Undefined symbol: ");
				_dl_wrstderr((char *)dynld.dyn.strtab +
				    sp->st_name);
#endif
				_dl_exit(5);
			}

			ra = (Elf_Addr *)(rp->r_offset + loff);
			RELOC_REL(rp, sp, ra, loff);
			rp++;
		}

	}

	for (n = 0; n < 2; n++) {
		unsigned long rs;
		Elf_RelA *rp;
		int	i;

		switch (n) {
		case 0:
			rp = (Elf_RelA *)(dynld.Dyn.info[DT_JMPREL]);
			rs = dynld.dyn.pltrelsz;
			break;
		case 1:
			rp = (Elf_RelA *)(dynld.Dyn.info[DT_RELA]);
			rs = dynld.dyn.relasz;
			break;
		default:
			rp = NULL;
			rs = 0;
		}
		for (i = 0; i < rs; i += sizeof (Elf_RelA)) {
			Elf_Addr *ra;
			const Elf_Sym *sp;

			sp = dynld.dyn.symtab;
			sp += ELF_R_SYM(rp->r_info);
			if (ELF_R_SYM(rp->r_info) && sp->st_value == 0) {
#if 0
				_dl_wrstderr("Dynamic loader failure: self bootstrapping impossible.\n");
				_dl_wrstderr("Undefined symbol: ");
				_dl_wrstderr((char *)dynld.dyn.strtab +
				    sp->st_name);
#endif
				_dl_exit(6);
			}

			ra = (Elf_Addr *)(rp->r_offset + loff);
			RELOC_RELA(rp, sp, ra, loff);
			rp++;
		}
	}
	/*
	 * we have been fully relocated here, so most things no longer
	 * need the loff adjustment
	 */
}

void
_dl_rtld(elf_object_t *object)
{
	if (object->next)
		_dl_rtld(object->next);

	if (object->status & STAT_RELOC_DONE)
		return;

	/*
	 * Do relocation information first, then GOT.
	 */
	_dl_md_reloc(object, DT_REL, DT_RELSZ);
	_dl_md_reloc(object, DT_RELA, DT_RELASZ);
	_dl_md_reloc_got(object, !(_dl_bindnow || object->dyn.bind_now));
	object->status |= STAT_RELOC_DONE;
}

void
_dl_call_init(elf_object_t *object)
{
	Elf_Addr ooff;
	const Elf_Sym *sym;

	if (object->next)
		_dl_call_init(object->next);

	if (object->status & STAT_INIT_DONE)
		return;

#ifndef __mips__
	if (object->dyn.init)
		(*object->dyn.init)();
/*
 * XXX We perform relocation of DTOR/CTOR. This is a ld bug problem
 * XXX that should be fixed.
 */
	sym = NULL;
	ooff = _dl_find_symbol("__CTOR_LIST__", object, &sym,
	    SYM_SEARCH_SELF|SYM_WARNNOTFOUND|SYM_PLT, 0);
	if (sym != NULL) {
		int i = *(int *)(sym->st_value + ooff);
		while (i--)
			*(int *)(sym->st_value + ooff + 4 + 4 * i) += ooff;
	}
	sym = NULL;
	ooff = _dl_find_symbol("__DTOR_LIST__", object, &sym,
	    SYM_SEARCH_SELF|SYM_WARNNOTFOUND|SYM_PLT, 0);
	if (sym != NULL) {
		int i = *(int *)(sym->st_value + ooff);
		while (i--)
			*(int *)(sym->st_value + ooff + 4 + 4 * i) += ooff;
	}

/*
 * XXX We should really call any code which resides in the .init segment
 * XXX but at the moment this functionality is not provided by the toolchain.
 * XXX Instead we rely on a symbol named '.init' and call it if it exists.
 */
	sym = NULL;
	ooff = _dl_find_symbol(".init", object, &sym,
	    SYM_SEARCH_SELF|SYM_WARNNOTFOUND|SYM_PLT, 0);
	if (sym != NULL) {
		DL_DEB(("calling .init in '%s'\n",object->load_name));
		(*(void(*)(void))(sym->st_value + ooff))();
	}
#if 0 /*XXX*/
	if (object->dyn.init)
		(*object->dyn.init)();
#endif
#endif /* __mips__ */
	object->status |= STAT_INIT_DONE;
}

static char *
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

static void
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
		}
		env++;
	}

}
