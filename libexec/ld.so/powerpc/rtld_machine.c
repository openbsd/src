/*	$OpenBSD: rtld_machine.c,v 1.16 2002/11/05 16:53:19 drahn Exp $ */

/*
 * Copyright (c) 1999 Dale Rahn
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
#include <sys/mman.h>

#include <nlist.h>
#include <link.h>

#include "syscall.h"
#include "archdep.h"
#include "resolve.h"


/* relocation bits */
#define HA(x) (((Elf_Addr)(x) >> 16) + (((Elf_Addr)(x) & 0x00008000) >> 15))
#define L(x) (((Elf_Addr)x) & 0x0000ffff)
#define ADDIS_R11_R11	0x3d6b0000
#define ADDIS_R11_R0	0x3d600000
#define ADDI_R11_R11	0x396b0000
#define LWZ_R11_R11	0x816b0000
#define LI_R11		0x39600000

#define ADDIS_R12_R0	0x3d800000
#define ADDI_R12_R12	0x398c0000
#define MCTR_R11	0x7d6903a6
#define MCTR_R12	0x7d8903a6
#define BCTR		0x4e800420
#define BR(from, to)	do { \
	int lval = (Elf32_Addr)(to) - (Elf32_Addr)(&(from)); \
	lval &= ~0xfc000000; \
	lval |= 0x48000000; \
	(from) = lval; \
}while(0)

/* these are structures/functions offset from PLT region */
#define PLT_CALL_OFFSET		6
#define PLT_INFO_OFFSET		10
#define PLT_1STRELA_OFFSET	18
#define B24_VALID_RANGE(x) \
    ((((x) & 0xfe000000) == 0x00000000) || (((x) &  0xfe000000) == 0xfe000000))

void _dl_bind_start(void); /* XXX */

void
_dl_bcopy(void *src, void *dest, int size)
{
	unsigned char *psrc = src, *pdest = dest;
	int i;

	for (i = 0; i < size; i++)
		pdest[i] = psrc[i];
}

int
_dl_md_reloc(elf_object_t *object, int rel, int relasz)
{
	int	i;
	int	numrela;
	int	fails = 0;
	struct load_list *load_list;
	Elf32_Addr loff;
	Elf32_Rela  *relas;
	/* for jmp table relocations */
	Elf32_Addr *pltresolve;
	Elf32_Addr *pltcall;
	Elf32_Addr *plttable;
	Elf32_Addr *pltinfo;

	Elf32_Addr *first_rela;

	loff = object->load_offs;
	numrela = object->Dyn.info[relasz] / sizeof(Elf32_Rela);
	relas = (Elf32_Rela *)(object->Dyn.info[rel]);

#ifdef DL_PRINTF_DEBUG
_dl_printf("object relocation size %x, numrela %x\n",
	object->Dyn.info[relasz], numrela);
#endif

	if (relas == NULL)
		return(0);

	pltresolve = NULL;
	pltcall = NULL;
	plttable = NULL;

	/* for plt relocation usage */
	if (object->Dyn.info[DT_JMPREL] != 0) {
		/* resolver stub not set up */

		/* Need to construct table to do jumps */
		pltresolve = (Elf32_Addr *)(object->Dyn.info[DT_PLTGOT]);
		pltcall = (Elf32_Addr *)(pltresolve) + PLT_CALL_OFFSET;
		pltinfo = (Elf32_Addr *)(pltresolve) + PLT_INFO_OFFSET;
		first_rela =  (Elf32_Addr *)(pltresolve) + PLT_1STRELA_OFFSET;

		plttable = (Elf32_Addr *)
		    ((Elf32_Addr)first_rela) + (2 *
		    (object->Dyn.info[DT_PLTRELSZ]/sizeof(Elf32_Rela)));


		pltinfo[0] = (Elf32_Addr)plttable;

#ifdef DL_PRINTF_DEBUG
		_dl_printf("md_reloc:  plttbl size %x\n",
		    (object->Dyn.info[DT_PLTRELSZ]/sizeof(Elf32_Rela)));
		_dl_printf("md_reloc: plttable %x\n", plttable);
#endif
		pltresolve[0] = ADDIS_R12_R0 | HA(_dl_bind_start);
		pltresolve[1] = ADDI_R12_R12 | L(_dl_bind_start);
		pltresolve[2] = MCTR_R12;
		pltresolve[3] = ADDIS_R12_R0 | HA(object);
		pltresolve[4] = ADDI_R12_R12 | L(object);
		pltresolve[5] = BCTR;
		_dl_dcbf(&pltresolve[0]);
		_dl_dcbf(&pltresolve[5]);

		/* addis r11,r11,.PLTtable@ha*/
		pltcall[0] = ADDIS_R11_R11 | HA(plttable);
		/* lwz r11,plttable@l(r11) */
		pltcall[1] = LWZ_R11_R11 | L(plttable);
		pltcall[2] = MCTR_R11;	/* mtctr r11 */
		pltcall[3] = BCTR;	/* bctr */
		_dl_dcbf(&pltcall[0]);
		_dl_dcbf(&pltcall[3]);
	} else {
		first_rela = NULL;
	}

	/*
	 * Change protection of all write protected segments in the object
	 * so we can do relocations such as REL24, REL16 etc. After
	 * relocation restore protection.
	 */
	load_list = object->load_list;
	while (load_list != NULL) {
		_dl_mprotect(load_list->start, load_list->size,
		    load_list->prot|PROT_WRITE);
		load_list = load_list->next;
	}


	for (i = 0; i < numrela; i++, relas++) {
		Elf32_Addr *r_addr = (Elf32_Addr *)(relas->r_offset + loff);
		Elf32_Addr ooff;
		const Elf32_Sym *sym, *this;
		const char *symn;
		int type;

		if (ELF32_R_SYM(relas->r_info) == 0xffffff)
			continue;

		type = ELF32_R_TYPE(relas->r_info);

		if (type == RELOC_JMP_SLOT && rel != DT_JMPREL)
			continue;

		sym = object->dyn.symtab;
		sym += ELF32_R_SYM(relas->r_info);
		this = sym;
		symn = object->dyn.strtab + sym->st_name;

		ooff = 0;

		if (ELF32_R_SYM(relas->r_info) &&
		    !(ELF32_ST_BIND(sym->st_info) == STB_LOCAL &&
		    ELF32_ST_TYPE (sym->st_info) == STT_NOTYPE)) {
			ooff = _dl_find_symbol(symn, _dl_objects, &this,
			    SYM_SEARCH_ALL|SYM_NOWARNNOTFOUND|
			    ((type == RELOC_JMP_SLOT) ? SYM_PLT:SYM_NOTPLT),
			    sym->st_size);

			if (!this && ELF32_ST_BIND(sym->st_info) == STB_GLOBAL) {
				_dl_printf("%s: %s :can't resolve reference '%s'\n",
				    _dl_progname, object->load_name, symn);
				fails++;
			}
		}

		switch (type) {
#if 1
		case RELOC_32:
			if (ELF32_ST_BIND(sym->st_info) == STB_LOCAL &&
			    (ELF32_ST_TYPE(sym->st_info) == STT_SECTION ||
			    ELF32_ST_TYPE(sym->st_info) == STT_NOTYPE) ) {
				*r_addr = ooff + relas->r_addend;
			} else {
				*r_addr = ooff + this->st_value +
				    relas->r_addend;
			}
			break;
#endif
		case RELOC_RELATIVE:
			if (ELF32_ST_BIND(sym->st_info) == STB_LOCAL &&
			    (ELF32_ST_TYPE(sym->st_info) == STT_SECTION ||
			    ELF32_ST_TYPE(sym->st_info) == STT_NOTYPE) ) {
				*r_addr = loff + relas->r_addend;

#ifdef DL_PRINTF_DEBUG
_dl_printf("rel1 r_addr %x val %x loff %x ooff %x addend %x\n", r_addr,
    loff + relas->r_addend, loff, ooff, relas->r_addend);
#endif

			} else {
				*r_addr = loff + this->st_value +
				    relas->r_addend;
			}
			break;
		case RELOC_JMP_SLOT:
		    {
			Elf32_Addr target = ooff + this->st_value +
			    relas->r_addend;
			Elf32_Addr val = target - (Elf32_Addr)r_addr;

			if(!B24_VALID_RANGE(val)){
				int index;
#ifdef DL_PRINTF_DEBUG
_dl_printf(" ooff %x, sym val %x, addend %x"
	" r_addr %x symn [%s] -> %x\n",
	ooff, this->st_value, relas->r_addend,
	r_addr, symn, val);
#endif
				/* if offset is > RELOC_24 deal with it */
				index = (r_addr - first_rela) >> 1;

				if (index > (2 << 14)) {
					/* addis r11,r11,.PLTtable@ha*/
					r_addr[0] = ADDIS_R11_R0 | HA(index*4);
					r_addr[1] = ADDI_R11_R11 | L(index*4);
					BR(r_addr[2], pltcall);
				} else {
					r_addr[0] = LI_R11 | (index * 4);
					BR(r_addr[1], pltcall);

				}
				_dl_dcbf(&r_addr[0]);
				_dl_dcbf(&r_addr[2]);
				val= ooff + this->st_value +
				    relas->r_addend;
#ifdef DL_PRINTF_DEBUG
_dl_printf(" symn [%s] val 0x%x\n", symn, val);
#endif
				plttable[index] = val;
			} else {
				/* if the offset is small enough,
				 * branch directly to the dest
				 */
				BR(r_addr[0], target);
				_dl_dcbf(&r_addr[0]);
			}
		    }

			break;
		case RELOC_GLOB_DAT:
			*r_addr = ooff + this->st_value + relas->r_addend;
			break;
#if 1
		/* should not be supported ??? */
		case RELOC_REL24:
		    {
			Elf32_Addr val = ooff + this->st_value +
			    relas->r_addend - (Elf32_Addr)r_addr;
			if(!B24_VALID_RANGE(val)){
				/* invalid offset */
				_dl_exit(20);
			}
			val &= ~0xfc000003;
			val |= (*r_addr & 0xfc000003);
			*r_addr = val;

			_dl_dcbf(r_addr);
		    }
		break;
#endif
#if 1
		case RELOC_16_LO:
		    {
			Elf32_Addr val;

			val = loff + relas->r_addend;
			*(Elf32_Half *)r_addr = val;

			_dl_dcbf(r_addr);
		    }
		break;
#endif
#if 1
		case RELOC_16_HI:
		    {
			Elf32_Addr val;

			val = loff + relas->r_addend;
			*(Elf32_Half *)r_addr = (val >> 16);

			_dl_dcbf(r_addr);
		    }
		break;
#endif
#if 1
		case RELOC_16_HA:
		    {
			Elf32_Addr val;

			val = loff + relas->r_addend;
			*(Elf32_Half *)r_addr = ((val + 0x8000) >> 16);

			_dl_dcbf(r_addr);
		    }
		break;
#endif
		case RELOC_REL14_TAKEN:
			/* val |= 1 << (31-10) XXX? */
		case RELOC_REL14:
		case RELOC_REL14_NTAKEN:
		    {
			Elf32_Addr val = ooff + this->st_value +
			    relas->r_addend - (Elf32_Addr)r_addr;
			if (((val & 0xffff8000) != 0) &&
			    ((val & 0xffff8000) != 0xffff8000)) {
				/* invalid offset */
				_dl_exit(20);
			}
			val &= ~0xffff0003;
			val |= (*r_addr & 0xffff0003);
			*r_addr = val;
#ifdef DL_PRINTF_DEBUG
			_dl_printf("rel 14 %x val %x\n", r_addr, val);
#endif

			_dl_dcbf(r_addr);
		    }
			break;
		case RELOC_COPY:
#ifdef DL_PRINTF_DEBUG
			_dl_printf("copy r_addr %x, sym %x [%s] size %d val %x\n",
			    r_addr, sym, symn, sym->st_size,
			    (ooff + this->st_value+
			    relas->r_addend));
#endif
		    {
			/*
			 * we need to find a symbol, that is not in the current
			 * object, start looking at the beginning of the list,
			 * searching all objects but _not_ the current object,
			 * first one found wins.
			 */
			elf_object_t *cobj;
			const Elf32_Sym *cpysrc = NULL;
			Elf32_Addr src_loff;
			int size;

			src_loff = 0;
			for (cobj = _dl_objects; cobj != NULL && cpysrc == NULL;
			    cobj = cobj->next) {
				if (object != cobj) {
					/* only look in this object */
					src_loff = _dl_find_symbol(symn, cobj,
					    &cpysrc,
					    SYM_SEARCH_SELF|SYM_NOWARNNOTFOUND|
					    ((type == RELOC_JMP_SLOT) ?
					        SYM_PLT : SYM_NOTPLT),
					    sym->st_size);
				}
			}
			if (cpysrc == NULL) {
				_dl_printf("symbol not found [%s] \n", symn);
			} else {
				size = sym->st_size;
				if (sym->st_size != cpysrc->st_size) {
					_dl_printf("symbols size differ [%s] \n",
					    symn);
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
		case RELOC_NONE:
			break;

		default:
			_dl_printf("%s:"
			    " %s: unsupported relocation '%s' %d at %x\n",
			    _dl_progname, object->load_name, symn,
			    ELF32_R_TYPE(relas->r_info), r_addr );
			_dl_exit(1);
		}
	}
	load_list = object->load_list;
	while (load_list != NULL) {
		_dl_mprotect(load_list->start, load_list->size, load_list->prot);
		load_list = load_list->next;
	}
	return(fails);
}

/*
 *	Relocate the Global Offset Table (GOT).
 *	This is done by calling _dl_md_reloc on DT_JMPREL for DL_BIND_NOW,
 *	otherwise the lazy binding plt initialization is performed.
 */
void
_dl_md_reloc_got(elf_object_t *object, int lazy)
{
	Elf32_Addr *pltresolve;
	Elf32_Addr *first_rela;
	Elf32_Rela  *relas;
	int	numrela;
	int i;
	int index;
	Elf32_Addr *r_addr;

	if (object->Dyn.info[DT_PLTREL] != DT_RELA)
		return;

	if (!lazy) {
		_dl_md_reloc(object, DT_JMPREL, DT_PLTRELSZ);
		return;
	}
	first_rela = (Elf32_Addr *)
	    (((Elf32_Rela *)(object->Dyn.info[DT_JMPREL]))->r_offset +
	    object->load_offs);
	pltresolve = (Elf32_Addr *)(first_rela) - 18;

	relas = (Elf32_Rela *)(object->Dyn.info[DT_JMPREL]);
	numrela = object->Dyn.info[DT_PLTRELSZ] / sizeof(Elf32_Rela);
	r_addr = (Elf32_Addr *)(relas->r_offset + object->load_offs);

	for (i = 0, index = 0; i < numrela; i++, r_addr+=2, index++) {
		if (index > (2 << 14)) {
			/* addis r11,r11,.PLTtable@ha*/
			r_addr[0] = ADDIS_R11_R0 | HA(index*4);
			r_addr[1] = ADDI_R11_R11 | L(index*4);
			BR(r_addr[2], pltresolve);
			/* only every other slot is used after 2^14 entries */
			r_addr += 2;
			index++;
		} else {
			r_addr[0] = LI_R11 | (index * 4);
			BR(r_addr[1], pltresolve);
		}
		_dl_dcbf(&r_addr[0]);
		_dl_dcbf(&r_addr[2]);
	}
}

Elf_Addr
_dl_bind(elf_object_t *object, int reloff)
{
	const Elf_Sym *sym, *this;
	Elf_Addr *r_addr, ooff;
	const char *symn;
	Elf_Addr value;
	Elf_RelA *relas;

	relas = ((Elf_RelA *)object->Dyn.info[DT_JMPREL]) + (reloff>>2);

	sym = object->dyn.symtab;
	sym += ELF_R_SYM(relas->r_info);
	symn = object->dyn.strtab + sym->st_name;

	r_addr = (Elf_Addr *)(object->load_offs + relas->r_offset);
	this = NULL;
	ooff = _dl_find_symbol(symn, _dl_objects, &this,
	    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|SYM_PLT, SYM_NOTPLT);
	if (this == NULL) {
		_dl_printf("lazy binding failed!\n");
		*((int *)0) = 0;	/* XXX */
	}

	value = ooff + this->st_value;

	{
		Elf32_Addr val = value - (Elf32_Addr)r_addr;

		Elf32_Addr *pltresolve;
		Elf32_Addr *pltcall;
		Elf32_Addr *pltinfo;
		Elf32_Addr *plttable;

		pltresolve = (Elf32_Addr *)
		    (Elf32_Rela *)(object->Dyn.info[DT_PLTGOT]);
		pltcall = (Elf32_Addr *)(pltresolve) + PLT_CALL_OFFSET;

		if (!B24_VALID_RANGE(val)) {
			int index;
			/* if offset is > RELOC_24 deal with it */
			index = reloff >> 2;

			/* update plttable before pltcall branch, to make
			 * this a safe race for threads
			 */
			val = ooff + this->st_value + relas->r_addend;

			pltinfo = (Elf32_Addr *)(pltresolve) + PLT_INFO_OFFSET;
			plttable = (Elf32_Addr *)pltinfo[0];
			plttable[index] = val;

			if (index > (2 << 14)) {
				/* r_addr[0,1] is initialized to correct
				 * value in reloc_got.
				 */
				BR(r_addr[2], pltcall);
				_dl_dcbf(&r_addr[2]);
			} else {
				/* r_addr[0] is initialized to correct
				 * value in reloc_got.
				 */
				BR(r_addr[1], pltcall);
				_dl_dcbf(&r_addr[1]);
			}
		} else {
			/* if the offset is small enough,
			 * branch directly to the dest
			 */
			BR(r_addr[0], value);
			_dl_dcbf(&r_addr[0]);
		}
	}

	return (value);
}

/* should not be defined here, but is is 32 for all powerpc 603-G4 */
#define CACHELINESIZE 32
void
_dl_syncicache(char *from, size_t len)
{
	unsigned int off = 0;
	int l = len + ((int)from & (CACHELINESIZE-1));

	while (off < l) {
		asm volatile ("dcbst %1,%0" :: "r"(from), "r"(off));
		asm volatile ("sync");
		asm volatile ("icbi %1, %0" :: "r"(from), "r"(off));
		asm volatile ("sync");
		asm volatile ("isync");

		off += CACHELINESIZE;
	}
}
