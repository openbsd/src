/*	$OpenBSD: rtld_machine.c,v 1.1 2001/05/14 22:18:21 niklas Exp $ */

/*
 * Copyright (c) 1999 Dale Rahn
 * Copyright (c) 2001 Niklas Hallqvist
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
 *	Dale Rahn.
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

#define _DYN_LOADER

#include <sys/types.h>
#include <sys/cdefs.h>

#include <machine/elf_machdep.h>

#include <nlist.h>
#include <link.h>

#include "syscall.h"
#include "archdep.h"
#include "resolve.h"

void
_dl_bcopy(void *src, void *dest, int size)
{
	unsigned char *psrc, *pdest;
	int i;
	psrc = src;
	pdest = dest;
	for (i = 0; i < size; i++) {
		pdest[i] = psrc[i];
	}
}

int
_dl_md_reloc(elf_object_t *object, int rel, int relasz)
{
	int	i;
	int	numrela;
	int	fails = 0;
	Elf64_Addr loff;
	Elf64_Rela  *relas;
	/* for jmp table relocations */
	Elf64_Addr *pltcall;
	Elf64_Addr *plttable;

	Elf64_Addr * first_rela;

	loff   = object->load_offs;
	numrela = object->Dyn.info[relasz] / sizeof(Elf64_Rela);
	relas = (Elf64_Rela *)(object->Dyn.info[rel]);

#ifdef DL_PRINTF_DEBUG
_dl_printf("loff 0x%lx object relocation size %x, numrela %x\n", loff,
	object->Dyn.info[relasz], numrela);
#endif

	if((object->status & STAT_RELOC_DONE) || !relas) {
		return(0);
	}
	/* for plt relocation usage */
	if (object->Dyn.info[DT_JMPREL] != 0) {
		/* resolver stub not set up */
		Elf64_Addr val;

		first_rela = (Elf64_Addr *)
		(((Elf64_Rela *)(object->Dyn.info[DT_JMPREL]))->r_offset
			+ loff);
		/* Need to construct table to do jumps */
		pltcall = (Elf64_Addr *)(first_rela) - 12;
#ifdef DL_PRINTF_DEBUG
_dl_printf("creating pltcall at %p\n", pltcall);
_dl_printf("md_reloc( jumprel %p\n", first_rela );
#endif
		plttable = (Elf64_Addr *)
			((Elf64_Addr)first_rela) + (2 *
			(object->Dyn.info[DT_PLTRELSZ]/sizeof(Elf64_Rela))
			);

#ifdef DL_PRINTF_DEBUG
_dl_printf("md_reloc:  plttbl size %x\n", 
			(object->Dyn.info[DT_PLTRELSZ]/sizeof(Elf64_Rela))
);
_dl_printf("md_reloc: plttable %p\n", plttable);
#endif
	} else {
		first_rela = NULL;
	}

	for(i = 0; i < numrela; i++, relas++) {
		Elf64_Addr *r_addr = (Elf64_Addr *)(relas->r_offset + loff);
		Elf64_Addr ooff;
		const Elf64_Sym *sym, *this;
		const char *symn;

#if 0
_dl_printf("%d offset 0x%lx info 0x%lx addend 0x%lx\n", i, relas->r_offset, relas->r_info, relas->r_addend);
#endif

		if(ELF64_R_SYM(relas->r_info) == 0xffffff) {
			continue;
		}

		sym = object->dyn.symtab;
		sym += ELF64_R_SYM(relas->r_info);
		this = sym;
		symn = object->dyn.strtab + sym->st_name;

		if(ELF64_R_SYM(relas->r_info) &&
		   !(ELF64_ST_BIND(sym->st_info) == STB_LOCAL &&
		     ELF64_ST_TYPE (sym->st_info) == STT_NOTYPE)) {
			
			ooff = _dl_find_symbol(symn, _dl_objects, &this, 0, 1);
			if(!this && ELF64_ST_BIND(sym->st_info) == STB_GLOBAL) {
				_dl_printf("%s:"
					" %s :can't resolve reference '%s'\n",
					_dl_progname, object->load_name,
					symn);
				fails++;
			}

		}

#if 0
_dl_printf("reloc %d\n", ELF64_R_TYPE(relas->r_info));
#endif
		switch(ELF64_R_TYPE(relas->r_info)) {
#if 1
		case R_TYPE(REFQUAD):
			if(ELF64_ST_BIND(sym->st_info) == STB_LOCAL &&
			   (ELF64_ST_TYPE(sym->st_info) == STT_SECTION ||
			    ELF64_ST_TYPE(sym->st_info) == STT_NOTYPE) ) {
				*r_addr = ooff + relas->r_addend;
			} else {
				*r_addr = ooff + this->st_value +
					relas->r_addend;
			}
			break;
#endif
		case R_TYPE(RELATIVE):
#if 0
_dl_printf("sym info %d r_addr %p relas %p\n", sym->st_info, r_addr, relas);
#endif
			if(ELF64_ST_BIND(sym->st_info) == STB_LOCAL &&
			   (ELF64_ST_TYPE(sym->st_info) == STT_SECTION ||
			    ELF64_ST_TYPE(sym->st_info) == STT_NOTYPE) ) {
#if 0
_dl_printf("addend 0x%lx\n", relas->r_addend);
_dl_printf("*r_addr 0x%lx\n", *r_addr);
#endif
				*r_addr = loff + relas->r_addend;
#if 0
_dl_printf("*r_addr 0x%lx\n", *r_addr);
#endif

#ifdef DL_PRINTF_DEBUG
_dl_printf("rel1 r_addr %p val %lx loff %lx ooff %lx addend %lx\n", r_addr,
loff + relas->r_addend, loff, ooff, relas->r_addend);
#endif

			} else {
#if 0
_dl_printf("this %p\n", this);
#endif
				*r_addr = loff + this->st_value +
					relas->r_addend;
			}
			break;
		case R_TYPE(JMP_SLOT):
		   {
			Elf64_Addr val = ooff + this->st_value +
				relas->r_addend - (Elf64_Addr)r_addr;
			if (!(((val & 0xfe000000) == 0x00000000) || 
				((val &  0xfe000000) == 0xfe000000)))
			{
				int index;
#ifdef DL_PRINTF_DEBUG
_dl_printf(" ooff %lx, sym val %lx, addend %lx"
	" r_addr %lx symn [%s] -> %x\n",
	ooff, this->st_value, relas->r_addend,
	r_addr, symn, val);
#endif
				/* if offset is > RELOC_24 deal with it */
				index = (r_addr - first_rela) >> 1;

				if (index > (2 << 14)) {

					/* addis r11,r11,.PLTtable@ha*/
					val = (index*4 >> 16) +
						((index*4 & 0x00008000) >> 15);
					r_addr[0] = 0x3d600000 | val;
					val = (Elf64_Addr)pltcall -
						(Elf64_Addr)&r_addr[2];
					r_addr[1] = 0x396b0000 | val; 
					val &= ~0xfc000000;
					val |=  0x48000000;
					r_addr[2] = val;

				} else {
#ifdef DL_PRINTF_DEBUG
	_dl_printf("  index %d, pltcall %x r_addr %lx\n",
		index, pltcall, r_addr);
#endif

					r_addr[0] = 0x39600000 | (index * 4);
					val = (Elf64_Addr)pltcall -
						(Elf64_Addr)&r_addr[1];
					val &= ~0xfc000000;
					val |=  0x48000000;
					r_addr[1] = val;

				}
				_dl_dcbf(r_addr);
				_dl_dcbf(&r_addr[2]);
				val= ooff + this->st_value +
					relas->r_addend;
#ifdef DL_PRINTF_DEBUG
		_dl_printf(" symn [%s] val 0x%x\n", symn, val);
#endif
				plttable[index] = val;
			} else {
				/* if the offset is small enough, 
				 * branch directy to the dest
				 */
				val &= ~0xfc000000;
				val |=  0x48000000;
				*r_addr = val;	
				_dl_dcbf(r_addr);
			}
		   }

			break;
		case R_TYPE(GLOB_DAT):
			*r_addr = ooff + this->st_value + relas->r_addend;
			break;
#if 0
#ifdef DL_PRINTF_DEBUG
		/* should not be supported ??? */
		case RELOC_REL24:
			{
			Elf64_Addr val = ooff + this->st_value +
				relas->r_addend - (Elf64_Addr)r_addr;
			if ((val & 0xfe000000 != 0) &&
				(val & 0xfe000000 != 0xfe000000))
			{
				/* invalid offset */
				_dl_exit(20);
			}
			val &= ~0xfc000003;
			val |=  (*r_addr & 0xfc000003);
			*r_addr = val;	
				
			_dl_dcbf(r_addr);
			}
#endif
			break;
		case RELOC_REL14_TAKEN:
			/* val |= 1 << (31-10) XXX? */
		case RELOC_REL14:
		case RELOC_REL14_NTAKEN:
			{
			Elf64_Addr val = ooff + this->st_value +
				relas->r_addend - (Elf64_Addr)r_addr;
			if (((val & 0xffff8000) != 0) &&
				((val & 0xffff8000) != 0xffff8000))
			{
				/* invalid offset */
				_dl_exit(20);
			}
			val &= ~0xffff0003;
			val |=  (*r_addr & 0xffff0003);
			*r_addr = val;	
#ifdef DL_PRINTF_DEBUG
			_dl_printf("rel 14 %lx val %lx\n", 
				r_addr, val);
#endif
				
			_dl_dcbf(r_addr);
			}
			break;
#endif
		case R_TYPE(COPY):
#ifdef DL_PRINTF_DEBUG
			_dl_printf("copy r_addr %lx, sym %x [%s] size %d val %lx\n",
				r_addr, sym, symn, sym->st_size,
				(ooff + this->st_value+
				relas->r_addend)

				);
#endif
{
	/* we need to find a symbol, that is not in the current object,
	 * start looking at the beginning of the list, searching all objects
	 * but _not_ the current object, first one found wins.
	 */
	elf_object_t *cobj;
	const Elf64_Sym *cpysrc = NULL;
	Elf64_Addr src_loff;
	int size;
	for (cobj = _dl_objects;
		cobj != NULL && cpysrc == NULL;
		cobj = cobj->next)
	{
		if (object != cobj) {

			/* only look in this object */
			src_loff = _dl_find_symbol(symn, cobj,
				&cpysrc, 1, 1);
		}
	}
	if (cpysrc == NULL) {
		_dl_printf("symbol not found [%s] \n", symn);
	} else {
		size  = sym->st_size;
		if (sym->st_size != cpysrc->st_size) {
			_dl_printf("symbols size differ [%s] \n", symn);
			size = sym->st_size < cpysrc->st_size ?
				sym->st_size : cpysrc->st_size;
		}
#ifdef DL_PRINTF_DEBUG
_dl_printf(" found other symbol at %x size %d\n", 
		src_loff + cpysrc->st_value,  cpysrc->st_size);
#endif
		_dl_bcopy((void *)(src_loff + cpysrc->st_value),
			(void *)(ooff + this->st_value+ relas->r_addend),
			size);
	}
}
			break;
		case R_TYPE(NONE):
			break;

		default:
			_dl_printf("%s:"
				" %s: unsupported relocation '%s' %d at %lx\n",
					_dl_progname, object->load_name, symn,
					ELF64_R_TYPE(relas->r_info), r_addr );
			_dl_exit(1);
		}
	}
	object->status |= STAT_RELOC_DONE;
#if 0
_dl_printf("<\n");
#endif
	return(fails);
}

/*
 *	Relocate the Global Offset Table (GOT). Currently we don't
 *	do lazy evaluation here because the GNU linker doesn't
 *	follow the ABI spec which says that if an external symbol
 *	is referenced by other relocations than CALL16 and 26 it
 *	should not be given a stub and have a zero value in the
 *	symbol table. By not doing so, we can't use pointers to
 *	external functions and use them in comparitions...
 */
void
_dl_md_reloc_got(elf_object_t *object, int lazy)
{
	/* relocations all done via rela relocations above */
}

/* should not be defined here, but is is 32 for all powerpc 603-G4 */
#define CACHELINESIZE 32
void
_dl_syncicache(char *from, size_t len)
{
        int l = len;
	unsigned int off = 0;

	while (off < len) {
                off += CACHELINESIZE;
        }
}
