/* Run an ELF binary on a OpenBSD system.

   Copyright (C) 1993, Eric Youngdale.
   Copyright (C) 1995, Andreas Schwab.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */


/* Program to load an ELF binary on a OpenBSD system, and run it.
   References to symbols in sharable libraries can be resolved by either
   an ELF sharable library or a linux style of shared library. */

/* Disclaimer:  I have never seen any AT&T source code for SVr4, nor have
   I ever taken any courses on internals.  This program was developed using
   information available through the book "UNIX SYSTEM V RELEASE 4,
   Programmers guide: Ansi C and Programming Support Tools", which did
   a more than adequate job of explaining everything required to get this
   working. */

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <elf.h>

#include "hash.h"
#include "syscall.h"
#include "sysdep.h"
#include "string.h"

extern char *_dl_progname;

/* Search loaded objects' symbol tables for a definition of
   the symbol UNDEF_NAME.  If NOSELF is nonzero, then *REF
   cannot satisfy the reference itself; some different binding
   must be found.  */

Elf32_Addr
_dl_lookup_symbol (const char *undef_name, const Elf32_Sym **ref,
		   struct elf_resolve *symbol_scope,
		   const char *reference_name,
		   int noself)
{
  unsigned long int hash = _dl_elf_hash (undef_name);
  struct elf_resolve *map;
  struct
    {
      Elf32_Addr a;
      const Elf32_Sym *s;
    } weak_value = { 0, NULL };

  /* Search the relevant loaded objects for a definition.  */
  for (map = symbol_scope; map; map = map->next)
    {
      const Elf32_Sym *symtab;
      const char *strtab;
      Elf32_Word symidx;

      symtab = ((void *) map->loadoffs + map->dynamic_info[DT_SYMTAB]);
      strtab = ((void *) map->loadoffs + map->dynamic_info[DT_STRTAB]);

      /* Search the appropriate hash bucket in this object's symbol table
	 for a definition for the same symbol name.  */
      for (symidx = map->elf_buckets[hash % map->nbucket];
	   symidx != STN_UNDEF;
	   symidx = map->chains[symidx])
	{
	  const Elf32_Sym *sym = &symtab[symidx];

	  if (sym->st_value == 0 || /* No value.  */
	      sym->st_shndx == SHN_UNDEF || /* PLT entry.  */
	      (noself && sym == *ref))	/* The reference can't define it.  */
	    continue;

	  switch (ELF32_ST_TYPE (sym->st_info))
	    {
	    case STT_NOTYPE:
	    case STT_FUNC:
	    case STT_OBJECT:
	      break;
	    default:
	      /* Not a code/data definition.  */
	      continue;
	    }

	  if (sym != *ref && _dl_strcmp (strtab + sym->st_name, undef_name))
	    /* Not the symbol we are looking for.  */
	    continue;

	  switch (ELF32_ST_BIND (sym->st_info))
	    {
	    case STB_GLOBAL:
	      /* Global definition.  Just what we need.  */
	      *ref = sym;
	      return (Elf32_Word)map->loadoffs;
	    case STB_WEAK:
	      /* Weak definition.  Use this value if we don't find another.  */
	      if (! weak_value.s)
		{
		  weak_value.s = sym;
		  weak_value.a = (Elf32_Word)map->loadoffs;
		}
	      break;
	    default:
	      /* Local symbols are ignored.  */
	      break;
	    }
	}
    }

  if (weak_value.s == NULL && ELF32_ST_BIND ((*ref)->st_info) != STB_WEAK) {
      _dl_fdprintf(2,"%s: undefined symbol: '%s' %x\n", _dl_progname, undef_name,(*ref)->st_info);
  }

  *ref = weak_value.s;
  return weak_value.a;
}


/* Get link_map for this object.  */
static struct elf_resolve *
elf_machine_runtime_link_map (Elf32_Addr gpreg)
{
  struct elf_resolve *l = 0;
  Elf32_Addr *got = (Elf32_Addr *) (gpreg - 0x7ff0);
  Elf32_Word g1;

  g1 = ((Elf32_Word *) got)[1];

  if (g1 & 0x80000000)
    l = (void *) (g1 & ~0x80000000);
#if 0
  else
    l = LOOK_UP_A_TABLE (got);
#endif
  return l;
}

/* This function is called from assembler function _dl_runtime_resolve
   which converts special argument registers t7 ($15) and t8 ($24):
     t7  address to return to the caller of the function
     t8  index for this function symbol in .dynsym
   to usual c arguments.  */

Elf32_Addr
__dl_runtime_resolve (Elf32_Word sym_index,
		      Elf32_Word return_address,
		      Elf32_Addr old_gpreg,
		      Elf32_Addr stub_return_address)
{
  struct elf_resolve *l = elf_machine_runtime_link_map (old_gpreg);

  const Elf32_Sym *const symtab
    = (const Elf32_Sym *) (l->loadoffs + l->dynamic_info[DT_SYMTAB]);
  char *strtab
    = (void *) (l->loadoffs + l->dynamic_info[DT_STRTAB]);
  Elf32_Addr *got
    = (Elf32_Addr *) (l->loadoffs + l->dynamic_info[DT_PLTGOT]);

  const Elf32_Word local_gotno
    = (const Elf32_Word) l->dynamic_info[DT_MIPS_LOCAL_GOTNO - DT_LOPROC + DT_NUM];
  const Elf32_Word gotsym
    = (const Elf32_Word) l->dynamic_info[DT_MIPS_GOTSYM - DT_LOPROC + DT_NUM];
  
  const Elf32_Sym *definer;
  Elf32_Addr loadbase;
  Elf32_Addr funcaddr;
  struct elf_resolve *scope, *real_next;

  /* Look up the symbol's run-time value.  */

  real_next = l->next;
  if (l->dynamic_info[DT_SYMBOLIC])
    {
      l->next = _dl_loaded_modules;
      if (l->prev)
	l->prev->next = real_next;
      scope = l;
    }
  else
    scope = _dl_loaded_modules;

  definer = &symtab[sym_index];
  loadbase = _dl_lookup_symbol (strtab + definer->st_name, &definer,
                                scope, l->libname, 0);

  
  /* Restore list frobnication done above for DT_SYMBOLIC.  */
  l->next = real_next;
  if (l->prev)
    l->prev->next = l;

  /* Apply the relocation with that value.  */
  funcaddr = loadbase + definer->st_value;
  *(got + local_gotno + sym_index - gotsym) = funcaddr;

  return funcaddr;
}

asm ("\n\
	.text\n\
	.align	2\n\
	.globl	_dl_runtime_resolve\n\
	.type	_dl_runtime_resolve,@function\n\
	.ent	_dl_runtime_resolve\n\
_dl_runtime_resolve:\n\
	.set noreorder\n\
	# Save old GP to $3.\n\
	move	$3,$28\n\
	# Modify t9 ($25) so as to point .cpload instruction.\n\
	addu	$25,8\n\
	# Compute GP.\n\
	.cpload $25\n\
	.set reorder\n\
	# Save arguments and sp value in stack.\n\
	subu	$29, 40\n\
	.cprestore 32\n\
	sw	$15, 36($29)\n\
	sw	$4, 12($29)\n\
	sw	$5, 16($29)\n\
	sw	$6, 20($29)\n\
	sw	$7, 24($29)\n\
	sw	$16, 28($29)\n\
	move	$16, $29\n\
	move	$4, $24\n\
	move	$5, $15\n\
	move	$6, $3\n\
	move	$7, $31\n\
	jal	__dl_runtime_resolve\n\
	move	$29, $16\n\
	lw	$31, 36($29)\n\
	lw	$4, 12($29)\n\
	lw	$5, 16($29)\n\
	lw	$6, 20($29)\n\
	lw	$7, 24($29)\n\
	lw	$16, 28($29)\n\
	addu	$29, 40\n\
	move	$25, $2\n\
	jr	$25\n\
	.end	_dl_runtime_resolve\n\
");


int 
_dl_parse_relocation_information (struct elf_resolve *tpnt, int rel_addr,
				  int rel_size, int lazy)
{
  int i;
  char *strtab;
  int reloc_type;
  int goof = 0;
  Elf32_Sym *symtab;
  const Elf32_Sym *symbol;
  const Elf32_Sym *ref;
  Elf32_Rel *rpnt;
  unsigned int *reloc_addr;
  unsigned int symbol_addr;
  int symtab_index;
  struct elf_resolve *real_next, *scope;

  /* Now parse the relocation information */

  rpnt = (Elf32_Rel *) (rel_addr + tpnt->loadoffs);
  rel_size = rel_size / sizeof (Elf32_Rel);

  symtab = (Elf32_Sym *) (tpnt->dynamic_info[DT_SYMTAB] + tpnt->loadoffs);
  strtab = (char *) (tpnt->dynamic_info[DT_STRTAB] + tpnt->loadoffs);

  /* Set scope.  */
  real_next = tpnt->next;
  if (tpnt->dynamic_info[DT_SYMBOLIC]) {
      if (tpnt->prev)
          tpnt->prev->next = real_next;
      tpnt->next = _dl_loaded_modules;
          scope = tpnt;
  }
  else
      scope = _dl_loaded_modules;

  for (i = 0; i < rel_size; i++, rpnt++) {
      reloc_addr = (int *) (tpnt->loadoffs + (int) rpnt->r_offset);
      reloc_type = ELF32_R_TYPE (rpnt->r_info);
      symtab_index = ELF32_R_SYM (rpnt->r_info);
      symbol = symtab + symtab_index;
      symbol_addr = (unsigned int) tpnt->loadoffs;
      ref = symbol;

      if (tpnt->libtype == program_interpreter
	  && (!symtab_index || _dl_symbol (strtab + symbol->st_name)))
	continue;

      if (symtab_index == 0xffffff)
	continue;

      if (symtab_index &&
          !(ELF32_ST_BIND (ref->st_info) == STB_LOCAL &&
            ELF32_ST_TYPE (ref->st_info) == STT_NOTYPE)) {

          symbol_addr = (unsigned int)
            _dl_lookup_symbol (strtab + symbol->st_name, &ref,
                                scope, tpnt->libname, 0);
 
	  /* We want to allow undefined references to weak symbols -
	     this might have been intentional.  We should not be
	     linking local symbols here, so all bases should be
	     covered.  */
	  if (!ref && ELF32_ST_BIND (symbol->st_info) == STB_GLOBAL) {
	      _dl_fdprintf (2, "%s: can't resolve symbol '%s'\n",
			    _dl_progname, strtab + symbol->st_name);
	      goof++;
	  }
      }
      switch (reloc_type)
      {
	case R_MIPS_NONE:
	  break;

	case R_MIPS_REL32:
	  if (ELF32_ST_BIND (symbol->st_info) == STB_LOCAL
               && (ELF32_ST_TYPE (symbol->st_info) == STT_SECTION      
                || ELF32_ST_TYPE (symbol->st_info) == STT_NOTYPE)) {    
              *reloc_addr += (unsigned int) tpnt->loadoffs;
          }
          else {
              *reloc_addr += ref ? symbol_addr + ref->st_value : 0;
          }
	  break;

	default:
	  _dl_fdprintf (2, "%s: can't handle reloc type ", _dl_progname);
	  if (symtab_index)
              _dl_fdprintf (2, "'%s'", strtab + symbol->st_name);
	  _dl_fdprintf (2, "\n");
	  _dl_exit (1);
      }

  }
  /* Restore list frobnication done above for DT_SYMBOLIC.  */
  tpnt->next = real_next;
  if (tpnt->prev)
    tpnt->prev->next = tpnt;

  return goof;
}

/* Relocate GOT. */
void
elf_machine_got_rel (struct elf_resolve *map)
{
  Elf32_Addr *got;
  Elf32_Sym *sym = 0;
  int i, n;
  struct elf_resolve *real_next, *scope;
  char *strtab = ((void *) map->loadoffs + map->dynamic_info[DT_STRTAB]);

  Elf32_Addr resolve (const Elf32_Sym *sym)
  {
      const Elf32_Sym *ref;
      Elf32_Addr sym_addr;

      sym_addr = _dl_lookup_symbol (strtab + sym->st_name, &ref, scope,
                                        map->libname, 1);
      if(ref)
          return sym_addr + ref->st_value;
      else
          return 0;
   }

  got = (Elf32_Addr *) ((void *) map->loadoffs + map->dynamic_info[DT_PLTGOT]);

  /* Add the run-time display to all local got entries. */
  i = (got[1] & 0x80000000)? 2: 1;
  n = map->dynamic_info[DT_MIPS_LOCAL_GOTNO - DT_LOPROC + DT_NUM];
  while (i < n) {
    got[i++] += (Elf32_Addr)map->loadoffs;
  }

  /* Set scope.  */
  real_next = map->next;
  if (map->dynamic_info[DT_SYMBOLIC]) {
      if (map->prev)
	map->prev->next = real_next;
      map->next = _dl_loaded_modules;
      scope = map;
  }
  else
    scope = _dl_loaded_modules;

  /* Handle global got entries. */
  got += n;
  sym = (Elf32_Sym *) ((void *) map->loadoffs + map->dynamic_info[DT_SYMTAB]);
  sym += map->dynamic_info[DT_MIPS_GOTSYM - DT_LOPROC + DT_NUM];
  i = (map->dynamic_info[DT_MIPS_SYMTABNO - DT_LOPROC + DT_NUM]
       - map->dynamic_info[DT_MIPS_GOTSYM - DT_LOPROC + DT_NUM]);

  while (i--) {
      if (sym->st_shndx == SHN_UNDEF) {
	  if (ELF32_ST_TYPE (sym->st_info) == STT_FUNC) {
#if 0 /*XXX*/
	      if (sym->st_value /* && maybe_stub (sym->st_value) */) {
		*got = sym->st_value + (Elf32_Word)map->loadoffs;
              }
	      else {
		*got = resolve (sym);
              }
#else
	      *got = resolve (sym);
#endif
	  }
	  else /* if (*got == 0 || *got == QS) */ {
	    *got = resolve (sym);
          }
      }
      else if (sym->st_shndx == SHN_COMMON) {
	*got = resolve (sym);
      }
      else if (ELF32_ST_TYPE (sym->st_info) == STT_FUNC
	       /* && maybe_stub (*got) */) {
	*got += (Elf32_Word)map->loadoffs;
      }
      else {
	*got = sym->st_value + (Elf32_Word)map->loadoffs;
      }

      got++;
      sym++;
    }

  /* Restore list frobnication done above for DT_SYMBOLIC.  */
  map->next = real_next;
  if (map->prev)
    map->prev->next = map;

  return;
}
