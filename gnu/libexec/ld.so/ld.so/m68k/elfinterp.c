/* Run an ELF binary on a linux system.

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

/* Adapted to ELF/68k by Andreas Schwab.  */

#ifndef VERBOSE_DLINKER
#define VERBOSE_DLINKER
#endif
#ifdef VERBOSE_DLINKER
static char *_dl_reltypes[] =
{
  "R_68K_NONE",
  "R_68K_32", "R_68K_16", "R_68K_8",
  "R_68K_PC32", "R_68K_PC16", "R_68K_PC8",
  "R_68K_GOT32", "R_68K_GOT16", "R_68K_GOT8",
  "R_68K_GOT32O", "R_68K_GOT16O", "R_68K_GOT8O",
  "R_68K_PLT32", "R_68K_PLT16", "R_68K_PLT8",
  "R_68K_PLT32O", "R_68K_PLT16O", "R_68K_PLT8O",
  "R_68K_COPY", "R_68K_GLOB_DAT", "R_68K_JMP_SLOT", "R_68K_RELATIVE",
  "R_68K_NUM"
};
#endif

/* Program to load an ELF binary on a linux system, and run it.
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
#ifdef IBCS_COMPATIBLE
#include <ibcs/unistd.h>
#else
#include <linux/unistd.h>
#endif
#include <fcntl.h>
#include <linux/elf.h>

#include "hash.h"
#include "linuxelf.h"
#include "syscall.h"
#include "sysdep.h"
#include "string.h"

extern char *_dl_progname;

unsigned int 
_dl_linux_resolver (int dummy1, int dummy2,
		    struct elf_resolve *tpnt, int reloc_entry)
{
  int reloc_type;
  struct elf32_rela *this_reloc;
  char *strtab;
  struct elf32_sym *symtab;
  char *rel_addr;
  int symtab_index;
  char *new_addr;
  char **got_addr;
  unsigned int instr_addr;

  rel_addr = tpnt->loadaddr + tpnt->dynamic_info[DT_JMPREL];
  this_reloc = (struct elf32_rela *) (rel_addr + reloc_entry);
  reloc_type = ELF32_R_TYPE (this_reloc->r_info);
  symtab_index = ELF32_R_SYM (this_reloc->r_info);

  symtab = (struct elf32_sym *) (tpnt->dynamic_info[DT_SYMTAB]
				 + tpnt->loadaddr);
  strtab = (char *) (tpnt->dynamic_info[DT_STRTAB] + tpnt->loadaddr);


  if (reloc_type != R_68K_JMP_SLOT)
    {
      _dl_fdprintf (2, "%s: incorrect relocation type in jump relocations\n",
		    _dl_progname);
      _dl_exit (1);
    }

  /* Address of jump instruction to fix up.  */
  instr_addr = (int) this_reloc->r_offset + (int) tpnt->loadaddr;
  got_addr = (char **) instr_addr;

#ifdef DEBUG
  _dl_fdprintf (2, "Resolving symbol %s\n",
		strtab + symtab[symtab_index].st_name);
#endif

  /* Get the address of the GOT entry.  */
  new_addr = _dl_find_hash (strtab + symtab[symtab_index].st_name,
			    tpnt->symbol_scope, (int) got_addr, tpnt, 0);
  if (!new_addr)
    {
      _dl_fdprintf (2, "%s: can't resolve symbol '%s'\n",
		    _dl_progname, strtab + symtab[symtab_index].st_name);
      _dl_exit (1);
    }
/* #define DEBUG_LIBRARY */
#ifdef DEBUG_LIBRARY
  if ((unsigned int) got_addr < 0x40000000)
    _dl_fdprintf (2, "Calling library function: %s\n",
		  strtab + symtab[symtab_index].st_name);
  else
#endif
    *got_addr = new_addr;
  return (unsigned int) new_addr;
}

void
_dl_parse_lazy_relocation_information (struct elf_resolve *tpnt, int rel_addr,
				       int rel_size, int type)
{
  int i;
  char *strtab;
  int reloc_type;
  int symtab_index;
  struct elf32_sym *symtab;
  struct elf32_rela *rpnt;
  unsigned int *reloc_addr;

  /* Now parse the relocation information.  */
  rpnt = (struct elf32_rela *) (rel_addr + tpnt->loadaddr);
  rel_size = rel_size / sizeof (struct elf32_rela);

  symtab = (struct elf32_sym *) (tpnt->dynamic_info[DT_SYMTAB]
				 + tpnt->loadaddr);
  strtab = (char *) (tpnt->dynamic_info[DT_STRTAB] + tpnt->loadaddr);

  for (i = 0; i < rel_size; i++, rpnt++)
    {
      reloc_addr = (int *) (tpnt->loadaddr + (int) rpnt->r_offset);
      reloc_type = ELF32_R_TYPE (rpnt->r_info);
      symtab_index = ELF32_R_SYM (rpnt->r_info);

      /* When the dynamic linker bootstrapped itself, it resolved some symbols.
         Make sure we do not do them again.  */
      if (tpnt->libtype == program_interpreter
	  && (!symtab_index
	      || _dl_symbol (strtab + symtab[symtab_index].st_name)))
	continue;

      switch (reloc_type)
	{
	case R_68K_NONE:
	  break;
	case R_68K_JMP_SLOT:
	  *reloc_addr += (unsigned int) tpnt->loadaddr;
	  break;
	default:
	  _dl_fdprintf (2, "%s: (LAZY) can't handle reloc type ", _dl_progname);
#ifdef VERBOSE_DLINKER
	  _dl_fdprintf (2, "%s ", _dl_reltypes[reloc_type]);
#endif
	  if (symtab_index)
	    _dl_fdprintf (2, "'%s'", strtab + symtab[symtab_index].st_name);
	  _dl_fdprintf (2, "\n");
	  _dl_exit (1);
	}
    }
}

int 
_dl_parse_relocation_information (struct elf_resolve *tpnt, int rel_addr,
				  int rel_size, int type)
{
  int i;
  char *strtab;
  int reloc_type;
  int goof = 0;
  struct elf32_sym *symtab;
  struct elf32_rela *rpnt;
  unsigned int *reloc_addr;
  unsigned int symbol_addr;
  int symtab_index;
  /* Now parse the relocation information */

  rpnt = (struct elf32_rela *) (rel_addr + tpnt->loadaddr);
  rel_size = rel_size / sizeof (struct elf32_rela);

  symtab = (struct elf32_sym *) (tpnt->dynamic_info[DT_SYMTAB]
				 + tpnt->loadaddr);
  strtab = (char *) (tpnt->dynamic_info[DT_STRTAB] + tpnt->loadaddr);

  for (i = 0; i < rel_size; i++, rpnt++)
    {
      reloc_addr = (int *) (tpnt->loadaddr + (int) rpnt->r_offset);
      reloc_type = ELF32_R_TYPE (rpnt->r_info);
      symtab_index = ELF32_R_SYM (rpnt->r_info);
      symbol_addr = 0;

      if (tpnt->libtype == program_interpreter
	  && (!symtab_index
	      || _dl_symbol (strtab + symtab[symtab_index].st_name)))
	continue;

      if (symtab_index)
	{
	  symbol_addr = (unsigned int)
	    _dl_find_hash (strtab + symtab[symtab_index].st_name,
			   tpnt->symbol_scope, (int) reloc_addr,
			   reloc_type == R_68K_JMP_SLOT ? tpnt : NULL, 0);

	  /* We want to allow undefined references to weak symbols -
	     this might have been intentional.  We should not be
	     linking local symbols here, so all bases should be
	     covered.  */
	  if (!symbol_addr
	      && ELF32_ST_BIND (symtab[symtab_index].st_info) == STB_GLOBAL)
	    {
	      _dl_fdprintf (2, "%s: can't resolve symbol '%s'\n",
			    _dl_progname, strtab + symtab[symtab_index].st_name);
	      goof++;
	    }
	}
      switch (reloc_type)
	{
	case R_68K_NONE:
	  break;
	case R_68K_8:
	  *(char *) reloc_addr = symbol_addr + rpnt->r_addend;
	  break;
	case R_68K_16:
	  *(short *) reloc_addr = symbol_addr + rpnt->r_addend;
	  break;
	case R_68K_32:
	  *reloc_addr = symbol_addr + rpnt->r_addend;
	  break;
	case R_68K_PC8:
	  *(char *) reloc_addr = (symbol_addr + rpnt->r_addend
				  - (unsigned int) reloc_addr);
	  break;
	case R_68K_PC16:
	  *(short *) reloc_addr = (symbol_addr + rpnt->r_addend
				   - (unsigned int) reloc_addr);
	  break;
	case R_68K_PC32:
	  *reloc_addr = (symbol_addr + rpnt->r_addend
			 - (unsigned int) reloc_addr);
	  break;
	case R_68K_GLOB_DAT:
	case R_68K_JMP_SLOT:
	  *reloc_addr = symbol_addr + rpnt->r_addend;
	  break;
	case R_68K_RELATIVE:
	  *reloc_addr += (unsigned int) tpnt->loadaddr + rpnt->r_addend;
	  break;
	case R_68K_COPY:
#if 0 /* Do this later.  */
	  _dl_fdprintf (2, "Doing copy");
	  if (symtab_index)
	    _dl_fdprintf (2, " for symbol %s",
			  strtab + symtab[symtab_index].st_name);
	  _dl_fdprintf (2, "\n");
	  _dl_memcpy ((void *) symtab[symtab_index].st_value,
		      (void *) symbol_addr,
		      symtab[symtab_index].st_size);
#endif
	  break;
	default:
	  _dl_fdprintf (2, "%s: can't handle reloc type ", _dl_progname);
#ifdef VERBOSE_DLINKER
	  _dl_fdprintf (2, "%s ", _dl_reltypes[reloc_type]);
#endif
	  if (symtab_index)
	    _dl_fdprintf (2, "'%s'", strtab + symtab[symtab_index].st_name);
	  _dl_fdprintf (2, "\n");
	  _dl_exit (1);
	}

    }
  return goof;
}

/* This is done as a separate step, because there are cases where
   information is first copied and later initialized.  This results in
   the wrong information being copied.  Someone at Sun was complaining about
   a bug in the handling of _COPY by SVr4, and this may in fact be what he
   was talking about.  Sigh.  */

/* No, there are cases where the SVr4 linker fails to emit COPY relocs
   at all.  */

#ifndef BROKEN_LINKER
int 
_dl_parse_copy_information (struct dyn_elf *xpnt, int rel_addr,
			    int rel_size, int type)
{
  int i;
  char *strtab;
  int reloc_type;
  int goof = 0;
  struct elf32_sym *symtab;
  struct elf32_rela *rpnt;
  unsigned int *reloc_addr;
  unsigned int symbol_addr;
  struct elf_resolve *tpnt;
  int symtab_index;
  /* Now parse the relocation information */

  tpnt = xpnt->dyn;

  rpnt = (struct elf32_rela *) (rel_addr + tpnt->loadaddr);
  rel_size = rel_size / sizeof (struct elf32_rela);

  symtab = (struct elf32_sym *) (tpnt->dynamic_info[DT_SYMTAB]
				 + tpnt->loadaddr);
  strtab = (char *) (tpnt->dynamic_info[DT_STRTAB] + tpnt->loadaddr);

  for (i = 0; i < rel_size; i++, rpnt++)
    {
      reloc_addr = (int *) (tpnt->loadaddr + (int) rpnt->r_offset);
      reloc_type = ELF32_R_TYPE (rpnt->r_info);
      if (reloc_type != R_68K_COPY)
	continue;
      symtab_index = ELF32_R_SYM (rpnt->r_info);
      symbol_addr = 0;
      if (tpnt->libtype == program_interpreter
	  && (!symtab_index
	      || _dl_symbol (strtab + symtab[symtab_index].st_name)))
	continue;
      if (symtab_index)
	{
	  symbol_addr = (unsigned int)
	    _dl_find_hash (strtab + symtab[symtab_index].st_name,
			   xpnt->next, (int) reloc_addr, NULL, 1);
	  if (!symbol_addr)
	    {
	      _dl_fdprintf (2, "%s: can't resolve symbol '%s'\n",
			    _dl_progname, strtab + symtab[symtab_index].st_name);
	      goof++;
	    }
	}
      _dl_memcpy ((void *) symtab[symtab_index].st_value, (void *) symbol_addr,
		  symtab[symtab_index].st_size);
    }
  return goof;
}
#endif
