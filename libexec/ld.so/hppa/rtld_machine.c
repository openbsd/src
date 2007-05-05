/*	$OpenBSD: rtld_machine.c,v 1.13 2007/05/05 15:21:21 drahn Exp $	*/

/*
 * Copyright (c) 2004 Michael Shalayeff
 * Copyright (c) 2001 Niklas Hallqvist
 * Copyright (c) 2001 Artur Grabowski
 * Copyright (c) 1999 Dale Rahn
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#define _DYN_LOADER

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/mman.h>
#include <sys/tree.h>

#include <nlist.h>
#include <link.h>
#include <signal.h>
#include <string.h>

#include "syscall.h"
#include "archdep.h"
#include "resolve.h"

typedef
struct hppa_plabel {
	Elf_Addr	pc;
	Elf_Addr	*sl;
	SPLAY_ENTRY(hppa_plabel) node;
} hppa_plabel_t;
SPLAY_HEAD(_dl_md_plabels, hppa_plabel) _dl_md_plabel_root;

void	_hppa_dl_set_dp(Elf_Addr *dp);	/* from ldasm.S */

static __inline int
_dl_md_plcmp(hppa_plabel_t *a, hppa_plabel_t *b)
{
	if (a->sl < b->sl)
		return (-1);
	else if (a->sl > b->sl)
		return (1);
	else if (a->pc < b->pc)
		return (-1);
	else if (a->pc > b->pc)
		return (1);
	else
		return (0);
}

SPLAY_PROTOTYPE(_dl_md_plabels, hppa_plabel, node, _dl_md_plcmp);
SPLAY_GENERATE(_dl_md_plabels, hppa_plabel, node, _dl_md_plcmp);

Elf_Addr
_dl_md_plabel(Elf_Addr pc, Elf_Addr *sl)
{
	hppa_plabel_t key, *p;

	key.pc = pc;
	key.sl = sl;
	p = SPLAY_FIND(_dl_md_plabels, &_dl_md_plabel_root, &key);
	if (p == NULL) {
		p = _dl_malloc(sizeof(*p));
		if (p == NULL)
			_dl_exit(5);
		p->pc = pc;
		p->sl = sl;
		SPLAY_INSERT(_dl_md_plabels, &_dl_md_plabel_root, p);
	}

	return (Elf_Addr)p | 2;
}

int
_dl_md_reloc(elf_object_t *object, int rel, int relasz)
{
	Elf_RelA	*rela;
	Elf_Addr	loff;
	int	i, numrela, fails = 0;
	size_t	size;

	loff = object->load_offs;
	numrela = object->Dyn.info[relasz] / sizeof(Elf_RelA);
	rela = (Elf_RelA *)(object->Dyn.info[rel]);

#ifdef DEBUG
	DL_DEB(("object %s relasz %x, numrela %x loff %x\n",
	    object->load_name, object->Dyn.info[relasz], numrela, loff));
#endif

	if (rela == NULL)
		return (0);

	/* either it's an ld bug or a wacky hpux abi */
	if (!object->dyn.pltgot)
		object->Dyn.info[DT_PLTGOT] += loff;

	if (object->dyn.init && !((Elf_Addr)object->dyn.init & 2)) {
		Elf_Addr addr = _dl_md_plabel((Elf_Addr)object->dyn.init,
		    object->dyn.pltgot);
#ifdef DEBUG
		DL_DEB(("PLABEL32: %p:%p(_init) -> 0x%x in %s\n",
		    object->dyn.init, object->dyn.pltgot,
		    addr, object->load_name));
#endif
		object->dyn.init = (void *)addr;
	}

	if (object->dyn.fini && !((Elf_Addr)object->dyn.fini & 2)) {
		Elf_Addr addr = _dl_md_plabel((Elf_Addr)object->dyn.fini,
		    object->dyn.pltgot);
#ifdef DEBUG
		DL_DEB(("PLABEL32: %p:%p(_fini) -> 0x%x in %s\n",
		    object->dyn.fini, object->dyn.pltgot,
		    addr, object->load_name));
#endif
		object->dyn.fini = (void *)addr;
	}

	/*
	 * this is normally done by the crt0 code but we have to make
	 * sure it's set here to allow constructors to call functions
	 * that are overriden in the user binary (that are un-pic)
	 */
	if (object->obj_type == OBJTYPE_EXE)
		_hppa_dl_set_dp(object->dyn.pltgot);

	for (i = 0; i < numrela; i++, rela++) {
		const elf_object_t *sobj;
		const Elf_Sym *sym, *this;
		Elf_Addr *pt, ooff;
		const char *symn;
		int type;

		type = ELF_R_TYPE(rela->r_info);
		if (type == RELOC_NONE)
			continue;

		sym = object->dyn.symtab + ELF_R_SYM(rela->r_info);
		sobj = object;
		symn = object->dyn.strtab + sym->st_name;
		pt = (Elf_Addr *)(rela->r_offset + loff);

		ooff = 0;
		this = NULL;
		if (ELF_R_SYM(rela->r_info) && sym->st_name) {
			ooff = _dl_find_symbol_bysym(object,
			    ELF_R_SYM(rela->r_info), &this,
			    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|
			    ((type == RELOC_IPLT) ? SYM_PLT: SYM_NOTPLT),
			    sym, &sobj);
			if (this == NULL) {
				if (ELF_ST_BIND(sym->st_info) != STB_WEAK)
					fails++;
				continue;
			}
		}

#ifdef DEBUG
		DL_DEB(("*pt=%x r_addend=%x r_sym=%x\n",
		    *pt, rela->r_addend, ELF_R_SYM(rela->r_info)));
#endif

		switch (type) {
		case RELOC_DIR32:
			if (ELF_R_SYM(rela->r_info) && sym->st_name) {
				*pt = ooff + this->st_value + rela->r_addend;
#ifdef DEBUG
				DL_DEB(("[%x]DIR32: %s:%s -> 0x%x in %s\n",
				    i, symn, object->load_name,
				    *pt, sobj->load_name));
#endif
			} else {
				/*
				 * XXX should objects ever get their
				 * sections loaded insequential this
				 * would have to get a section number
				 * (ELF_R_SYM(rela->r_info))-1 and then:
				 *    *pt = sect->addr + rela->r_addend;
				 */
				if (ELF_R_SYM(rela->r_info))
					*pt += loff;
				else
					*pt += loff + rela->r_addend;
#ifdef DEBUG
				DL_DEB(("[%x]DIR32: %s @ 0x%x\n", i,
				    object->load_name, *pt));
#endif
			}
			break;

		case RELOC_PLABEL32:
			if (ELF_R_SYM(rela->r_info)) {
				if (ELF_ST_TYPE(this->st_info) != STT_FUNC) {
					DL_DEB(("[%x]PLABEL32: bad\n", i));
					break;
				}
				*pt = _dl_md_plabel(sobj->load_offs +
				    this->st_value + rela->r_addend,
				    sobj->dyn.pltgot);
#ifdef DEBUG
				DL_DEB(("[%x]PLABEL32: %s:%s -> 0x%x in %s\n",
				    i, symn, object->load_name,
				    *pt, sobj->load_name));
#endif
			} else {
				*pt = loff + rela->r_addend;
#ifdef DEBUG
				DL_DEB(("[%x]PLABEL32: %s @ 0x%x\n", i,
				    object->load_name, *pt));
#endif
			}
			break;

		case RELOC_IPLT:
			if (ELF_R_SYM(rela->r_info)) {
				pt[0] = ooff + this->st_value + rela->r_addend;
				pt[1] = (Elf_Addr)sobj->dyn.pltgot;
#ifdef DEBUG
				DL_DEB(("[%x]IPLT: %s:%s -> 0x%x:0x%x in %s\n",
				    i, symn, object->load_name,
				    pt[0], pt[1], sobj->load_name));
#endif
			} else {
				pt[0] = loff + rela->r_addend;
				pt[1] = (Elf_Addr)object->dyn.pltgot;
#ifdef DEBUG
				DL_DEB(("[%x]IPLT: %s @ 0x%x:0x%x\n", i,
				    object->load_name, pt[0], pt[1]));
#endif
			}
			break;

		case RELOC_COPY:
		{
			const Elf32_Sym *cpysrc = NULL;
			size = sym->st_size;
			ooff = _dl_find_symbol(symn, &cpysrc,
			    SYM_SEARCH_OTHER|SYM_WARNNOTFOUND|SYM_NOTPLT,
			    sym, object, NULL);
			if (cpysrc) {
				_dl_bcopy((void *)(ooff + cpysrc->st_value),
				    pt, sym->st_size);
#ifdef DEBUG
				DL_DEB(("[%x]COPY: %s[%x]:%s -> %p[%x] in %s\n",
				    i, symn, ooff + cpysrc->st_value,
				    object->load_name, pt, sym->st_size,
				    sobj->load_name));
#endif
			} else
				DL_DEB(("[%x]COPY: no sym\n", i));
			break;
		}
		default:
			DL_DEB(("[%x]UNKNOWN(%d): type=%d off=0x%lx "
			    "addend=0x%lx rel=0x%x\n", i, type,
			    ELF_R_TYPE(rela->r_info), rela->r_offset,
			    rela->r_addend, *pt));
			break;
		}
	}

	return (fails);
}

void
_dl_md_reloc_got(elf_object_t *object, int lazy)
{
	Elf_RelA *rela;
	Elf_Addr  ooff;
	int	i, numrela;
	const Elf_Sym *this;

	if (object->dyn.pltrel != DT_RELA)
		return;

	lazy = 0;	/* force busy binding */

	object->got_addr = NULL;
	object->got_size = 0;
	this = NULL;
	ooff = _dl_find_symbol("__got_start", &this,
	    SYM_SEARCH_OBJ|SYM_NOWARNNOTFOUND|SYM_PLT, NULL, object, NULL );
	if (this != NULL)
		object->got_addr = ooff + this->st_value;

	this = NULL;
	ooff = _dl_find_symbol("__got_end", &this,
	    SYM_SEARCH_OBJ|SYM_NOWARNNOTFOUND|SYM_PLT, NULL, object, NULL);
	if (this != NULL)
		object->got_size = ooff + this->st_value  - object->got_addr;

	if (object->got_addr == NULL)
		object->got_start = NULL;
	else {
		object->got_start = ELF_TRUNC(object->got_addr, _dl_pagesz);
		object->got_size += object->got_addr - object->got_start;
		object->got_size = ELF_ROUND(object->got_size, _dl_pagesz);
	}

	if (!lazy) {
		_dl_md_reloc(object, DT_JMPREL, DT_PLTRELSZ);
	} else {
		rela = (Elf_RelA *)(object->dyn.jmprel);
		numrela = object->dyn.pltrelsz / sizeof(Elf_RelA);
		ooff = object->load_offs;

		for (i = 0; i < numrela; i++, rela++) {
			Elf_Addr *r_addr = (Elf_Addr *)(ooff + rela->r_offset);

			if (ELF_R_SYM(rela->r_info)) {
				r_addr[0] = 0;	/* TODO */
				r_addr[1] = (Elf_Addr) ((void *)rela -
				    (void *)object->dyn.jmprel);
			} else {
				r_addr[0] = ooff + rela->r_addend;
				r_addr[1] = (Elf_Addr)object->dyn.pltgot;
			}
		}
	}
	if (object->got_size != 0)
		_dl_mprotect((void *)object->got_start, object->got_size,
		    GOT_PERMS);
}

/*
 * Resolve a symbol at run-time.
 */
Elf_Addr
_dl_bind(elf_object_t *object, int reloff)
{
	const elf_object_t *sobj;
	const Elf_Sym *sym, *this;
	Elf_Addr *addr, ooff;
	const char *symn;
	Elf_Addr value;
	Elf_RelA *rela;
	sigset_t omask, nmask;

	rela = (Elf_RelA *)(object->dyn.jmprel + reloff);

	sym = object->dyn.symtab;
	sym += ELF_R_SYM(rela->r_info);
	symn = object->dyn.strtab + sym->st_name;

	addr = (Elf_Addr *)(object->load_offs + rela->r_offset);
	this = NULL;
	ooff = _dl_find_symbol(symn, &this,
	    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|SYM_PLT, sym, object, &sobj);
	if (this == NULL) {
		_dl_printf("lazy binding failed!\n");
		*((int *)0) = 0;	/* XXX */
	}

	value = ooff + this->st_value;

	/* if PLT+GOT is protected, allow the write */
	if (object->got_size != 0) {
		sigfillset(&nmask);
		_dl_sigprocmask(SIG_BLOCK, &nmask, &omask);
		_dl_thread_bind_lock(0);
		/* mprotect the actual modified region, not the whole plt */
		_dl_mprotect((void*)addr, sizeof (Elf_Addr) * 2,
		    PROT_READ|PROT_WRITE);
	}

	addr[0] = value;
	addr[1] = (Elf_Addr)sobj->dyn.pltgot;

	/* if PLT is (to be protected, change back to RO */
	if (object->got_size != 0) {
		/* mprotect the actual modified region, not the whole plt */
		_dl_mprotect((void*)addr, sizeof (Elf_Addr) * 3,
		    PROT_READ);
		_dl_thread_bind_lock(1);
		_dl_sigprocmask(SIG_SETMASK, &omask, NULL);
	}

	return (value);
}
