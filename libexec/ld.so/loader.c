/*	$OpenBSD: loader.c,v 1.1.1.1 2000/06/13 03:34:06 rahnds Exp $ */

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
#include <link.h>

#include "syscall.h"
#include "archdep.h"
#include "resolve.h"

/*
 *  Local decls.
 */
static char *_dl_getenv(const char *var, const char **env);

/*
 *   Static vars usable after bootsrapping.
 */
static void *_dl_malloc_base;
static void *_dl_malloc_pool = 0;
static int  *_dl_malloc_free = 0;

const char *_dl_progname;
int  _dl_pagesz;
int  _dl_trusted;

char *_dl_libpath;
char *_dl_preload;
char *_dl_bindnow;
char *_dl_traceld;
char *_dl_debug;
char *_dl_showmap;

struct r_debug *_dl_debug_map;

void
_dl_debug_state(void)
{
	/* Debugger stub */
}

/*
 *  This is the dynamic loader entrypoint. When entering here, depending
 *  on architecture type, the stack and registers are set up according
 *  to the architectures ABI specification. The first thing requiered
 *  to do is to dig out all information we need to accomplish out task.
 */

int
_dl_boot(const int sp, const int loff)
{
	int		argc;
	int		n;
	int		brk_addr;
	const char	**argv, **envp;
	int		*stack, execstack = sp;
	AuxInfo		*auxstack;
	int		dl_data[AUX_entry + 1];
	Elf32_Dyn	*dynp;
	Elf32_Phdr	*phdp;
	elf_object_t	*dynobj;
	char		*us = "";

	struct elf_object  dynld;	/* Resolver data for the loader */
	struct elf_object  *exe_obj;	/* Pointer to executable object */
	struct elf_object  *dyn_obj;	/* Pointer to executable object */
	struct r_debug	   *debug_map;	/* Dynamic objects map for gdb */
	struct r_debug	   **map_link;	/* Where to put pointer for gdb */

	/*
	 * Scan argument and environment vectors. Find dynamic
	 * data vector put after them.
	 */
	stack = (int *)execstack;
	argc = *stack++;
	argv = (const char **)stack;
	envp = &argv[argc + 1];
	stack = (int *)envp;
	while(*stack++ != NULL) {};

	/*
	 * Dig out auxilary data set up by exec call. Move all known
	 * tags to an indexed local table for easy access.
	 */

	auxstack = (AuxInfo *)stack;
	while(auxstack->au_id != AUX_null) {
		if(auxstack->au_id <= AUX_entry) {
			dl_data[auxstack->au_id] = auxstack->au_v;
		}
		auxstack++;
	}

	/*
	 *  We need to do 'selfreloc' in case the code were'nt
	 *  loaded at the address it was linked to.
	 *
	 *  Scan the DYNAMIC section for the loader.
	 *  Cache the data for easier access.
	 */

	dynp = (Elf32_Dyn *)((int)_DYNAMIC + loff);
	while(dynp->d_tag != DT_NULL) {
		if(dynp->d_tag < DT_PROCNUM) {
			dynld.Dyn.info[dynp->d_tag] = dynp->d_un.d_val;
		}
		else if(dynp->d_tag >= DT_LOPROC && dynp->d_tag < DT_LOPROC + DT_PROCNUM) {
			dynld.Dyn.info[dynp->d_tag + DT_NUM - DT_LOPROC] = dynp->d_un.d_val;
		}
		if(dynp->d_tag == DT_TEXTREL)
			dynld.dyn.textrel = 1;
		dynp++;
	}

	/*
	 *  Do the 'bootstrap relocation'. This is really only needed if
	 *  the code was loaded at another location than it was linked to.
	 *  We don't do undefined symbols resolving (to difficult..)
	 */
	for(n = 0; n < 2; n++) {
		int	  i;
		u_int32_t rs;
		RELTYPE	  *rp;

		if(n == 0) {
			rp = (RELTYPE *)(dynld.Dyn.info[DT_REL] + loff);
			rs = dynld.dyn.relsz;
		}
		else {
			rp = (RELTYPE *)(dynld.Dyn.info[DT_JMPREL] + loff);
			rs = dynld.dyn.pltrelsz;
		}

		for(i = 0; i < rs; i += RELSIZE) {
			Elf32_Addr *ra;
			const Elf32_Sym  *sp;

			sp = dynld.dyn.symtab + loff;
			sp += ELF32_R_SYM(rp->r_info);
			if(ELF32_R_SYM(rp->r_info) && sp->st_value == 0) {
				_dl_wrstderr("Dynamic loader failure: self bootstrapping impossible.\n");
				_dl_wrstderr("Undefined symbol: ");
				_dl_wrstderr((char *)dynld.dyn.strtab + loff + sp->st_name);
				_dl_exit(4);
			}

			ra = (Elf32_Addr *)(rp->r_offset + loff);
			SIMPLE_RELOC(rp, sp, ra, loff);
		}

	}

	/*
	 *  Get paths to various things we are going to use.
	 */
	_dl_libpath = _dl_getenv("LD_LIBRARY_PATH", envp);
	_dl_preload = _dl_getenv("LD_PRELOAD", envp);
	_dl_bindnow = _dl_getenv("LD_BIND_NOW", envp);
	_dl_traceld = _dl_getenv("LD_TRACE_LOADED_OBJECTS", envp);
	_dl_debug   = _dl_getenv("LD_DEBUG", envp);

	_dl_progname = argv[0];
	if(dl_data[AUX_pagesz] != 0) {
		_dl_pagesz = dl_data[AUX_pagesz];
	}
	else {
		_dl_pagesz = 4096;
	}
	if(_dl_debug)
		_dl_printf("rtld loading: '%s'\n", _dl_progname);

	/*
	 *  Don't allow someone to change the search paths if he runs
	 *  a suid program without credentials high enough.
	 */
	if((_dl_trusted = !_dl_suid_ok())) {	/* Zap paths if s[ug]id... */
		if(_dl_preload) {
			*_dl_preload = '\0';
		}
		if(_dl_libpath) {
			*_dl_libpath = '\0';
		}
	}

	/*
	 *  Examine the user application and set up object information.
	 */
	phdp = (Elf32_Phdr *) dl_data[AUX_phdr];
	for(n = 0; n < dl_data[AUX_phnum]; n++) {
		if(phdp->p_type == PT_LOAD) {				/*XXX*/
			if(phdp->p_vaddr + phdp->p_memsz > brk_addr)	/*XXX*/
				brk_addr = phdp->p_vaddr + phdp->p_memsz;
		}							/*XXX*/
		if(phdp->p_type == PT_DYNAMIC) {
			exe_obj = _dl_add_object("", (Elf32_Dyn *)phdp->p_vaddr,
						   dl_data, OBJTYPE_EXE, 0, 0);
		}
		if(phdp->p_type == PT_INTERP) {
			us = (char *)_dl_malloc(_dl_strlen((char *)phdp->p_vaddr));
			_dl_strcpy(us, (char *)phdp->p_vaddr);
		}
		phdp++;
	}

	/*
	 *  Now, pick up and 'load' all libraries requierd. Start
	 *  With the first on the list and then do whatever gets
	 *  added along the tour.
	 */

	dynobj = _dl_objects;
	while(dynobj) {
		if(_dl_debug)
			_dl_printf("examining: '%s'\n", dynobj->load_name);
		for(dynp = dynobj->load_dyn; dynp->d_tag; dynp++) {
			if(dynp->d_tag == DT_NEEDED) {
				const char *libname;
				libname = dynobj->dyn.strtab;
				libname += dynp->d_un.d_val;
				if(_dl_debug) 
					_dl_printf("needs: '%s'\n", libname);
				if(_dl_load_shlib(libname, dynobj, OBJTYPE_LIB) == 0) {
					_dl_printf("%s: can't load library '%s'\n",
						_dl_progname, libname);
					_dl_exit(4);
				}
			}
		}
		dynobj = dynobj->next;
	}

	/*
	 *  Now add the dynamic loader itself last in the object list
	 *  so we can use the _dl_ code when serving dl.... calls.
	 */

	dynp = (Elf32_Dyn *)((int)_DYNAMIC + loff);
	dyn_obj = _dl_add_object(us, dynp, 0, OBJTYPE_LDR, dl_data[AUX_base], loff);
	dyn_obj->status |= STAT_RELOC_DONE;

	/*
	 *  Everything should be in place now for doing the relocation
	 *  and binding. Call _dl_rtld to do the job. Fingers crossed.
	 */

	_dl_rtld(_dl_objects);
	_dl_call_init(_dl_objects);

	/*
	 *  Finally make something to help gdb when poking around in the code.
	 */

#ifdef __mips__
	map_link = (struct r_debug **)(exe_obj->Dyn.info[DT_MIPS_RLD_MAP - DT_LOPROC + DT_NUM]);
	if(map_link) {
		debug_map = (struct r_debug *)_dl_malloc(sizeof(*debug_map));
		debug_map->r_version = 1;
		debug_map->r_map = (struct link_map *)_dl_objects;
		debug_map->r_brk = (Elf32_Addr)_dl_debug_state;
		debug_map->r_state = RT_CONSISTENT;
		debug_map->r_ldbase = loff;
		_dl_debug_map = debug_map;
		*map_link = _dl_debug_map;
	}
#endif

	_dl_debug_state();

	if(_dl_debug) {
		_dl_show_objects();
		_dl_printf("dynamic loading done.\n");
	}

	return(dl_data[AUX_entry]);
}


void
_dl_rtld(elf_object_t *object)
{
	if(object->next) {
		_dl_rtld(object->next);
	}

	/*
	 *  Do relocation information first, then GOT.
	 */
	_dl_md_reloc(object, DT_REL, DT_RELSZ);
	_dl_md_reloc(object, DT_JMPREL, DT_PLTRELSZ);
	if(_dl_bindnow) {	/* XXX Perhaps more checking ? */
		_dl_md_reloc_got(object, 1);
	}
	else {
		_dl_md_reloc_got(object, 0);
	}
}

void
_dl_call_init(elf_object_t *object)
{
	Elf32_Addr ooff;
	Elf32_Sym  *sym;

	if(object->next) {
		_dl_call_init(object->next);
	}

	if(object->status & STAT_INIT_DONE) {
		return;
	}

/* XXX We perform relocation of DTOR/CTOR. This is a ld bug problem
 * XXX that should be fixed.
 */
	sym = 0;
	ooff = _dl_find_symbol("__CTOR_LIST__", object, &sym, 1);
	if(sym) {
		int i = *(int *)(sym->st_value + ooff);
		while(i--) {
			*(int *)(sym->st_value + ooff + 4 + 4 * i) += ooff;
		}
	}
	sym = 0;
	ooff = _dl_find_symbol("__DTOR_LIST__", object, &sym, 1);
	if(sym) {
		int i = *(int *)(sym->st_value + ooff);
		while(i--) {
			*(int *)(sym->st_value + ooff + 4 + 4 * i) += ooff;
		}
	}

/* XXX We should really call any code which resides in the .init segment
 * XXX but at the moment this functionality is not provided by the toolchain.
 * XXX Instead we rely on a symbol named '.init' and call it if it exists.
 */
	sym = 0;
	ooff = _dl_find_symbol(".init", object, &sym, 1);
	if(sym) {
		if(_dl_debug)
			_dl_printf("calling .init in '%s'\n",object->load_name);
		(*(void(*)(void))(sym->st_value + ooff))();
	}
#if 0 /*XXX*/
	if(object->dyn.init) {
		(*object->dyn.init)();
	}
#endif
	object->status |= STAT_INIT_DONE;
}

static char *
_dl_getenv(const char *var, const char **env)
{
	const char *ep;

	while((ep = *env++)) {
		const char *vp = var;
		while(*vp && *vp == *ep) {
			vp++;
			ep++;
		}
		if(*vp == '\0' && *ep++ == '=') {
			return((char *)ep);
		}
	}
	return(0);
}


/*
 *  The following malloc/free code is a very simplified implementation
 *  of a malloc function. However, we do not need to be very complex here
 *  because we only free memory when 'dlclose()' is called and we can
 *  reuse at least the memory allocated for the object descriptor. We have
 *  one dynamic string allocated, the library name and it is likely that
 *  we can reuse that one to without a lot of complex colapsing code.
 */

void *
_dl_malloc(int size)
{
	int *p;
	int *t, *n;

	size = (size + 8 + DL_MALLOC_ALIGN - 1) & ~(DL_MALLOC_ALIGN - 1);

	if((t = _dl_malloc_free) != 0) {	/* Try free list first */
		n = (int *)&_dl_malloc_free;
		while(t && t[-1] < size) {
			n = t;
			t = (int *)*t;
		}
		if(t) {
			*n = *t;
			_dl_memset(t, 0, t[-1] - 4);
			return((void *)t);
		}
	}
	if((_dl_malloc_pool == 0) ||
	   (_dl_malloc_pool + size > _dl_malloc_base + 4096)) {
		_dl_malloc_pool = (void *)_dl_mmap((void *)0, 4096,
						PROT_READ|PROT_WRITE,
						MAP_ANON|MAP_COPY, -1, 0);
		if(_dl_malloc_pool == 0) {
			_dl_printf("Dynamic loader failure: malloc.\n");
			_dl_exit(4);
		}
		_dl_malloc_base = _dl_malloc_pool;
	}
	p = _dl_malloc_pool;
	_dl_malloc_pool += size;
	_dl_memset(p, 0, size);
	*p = size;
	return((void *)(p + 1));
}

void
_dl_free(void *p)
{
	int *t = (int *)p;

	*t = (int)_dl_malloc_free;
	_dl_malloc_free = p;
}
