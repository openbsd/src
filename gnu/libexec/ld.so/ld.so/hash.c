/* Run an ELF binary on a linux system.

   Copyright (C) 1993, Eric Youngdale.

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



/* Various symbol table handling functions, including symbol lookup */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <elf.h>

#include "hash.h"
#include "syscall.h"
#include "string.h"
#include "sysdep.h"

/*
 * This is the start of the linked list that describes all of the files present
 * in the system with pointers to all of the symbol, string, and hash tables, 
 * as well as all of the other good stuff in the binary.
 */

struct elf_resolve * _dl_loaded_modules = NULL;

/*
 * This is the list of modules that are loaded when the image is first
 * started.  As we add more via dlopen, they get added into other
 * chains.
 */
struct dyn_elf * _dl_symbol_tables = NULL;

/*
 * This is the hash function that is used by the ELF linker to generate
 * the hash table that each executable and library is required to
 * have.  We need it to decode the hash table.
 */

unsigned long _dl_elf_hash(const char * name){
  unsigned long hash = 0;
  unsigned long tmp;

  while (*name){
    hash = (hash << 4) + *name++;
    if((tmp = hash & 0xf0000000)) hash ^= tmp >> 24;
    hash &= ~tmp;
  };
  return hash;
}

/*
 * Check to see if a library has already been added to the hash chain.
 */
struct elf_resolve * _dl_check_hashed_files(char * libname){
  struct elf_resolve * tpnt;
  tpnt = _dl_loaded_modules;
  while(tpnt){
    if(_dl_strcmp(tpnt->libname, libname) == 0) return tpnt;
    tpnt = tpnt->next;
  };
  return NULL;
}

/*
 * We call this function when we have just read an ELF library or executable.
 * We add the relevant info to the symbol chain, so that we can resolve all
 * externals properly.
 */

struct elf_resolve * _dl_add_elf_hash_table(char * libname,
					char * loadaddr,
					char * loadoffs,
					unsigned int * dynamic_info,
					unsigned int dynamic_addr,
					unsigned int dynamic_size){
  unsigned int *  hash_addr;
  struct elf_resolve * tpnt;
  int i;

  if (!_dl_loaded_modules) {
    tpnt = _dl_loaded_modules = 
      (struct elf_resolve *) _dl_malloc(sizeof(struct elf_resolve));
    _dl_memset (tpnt, 0, sizeof (*tpnt));
  }
  else {
    tpnt = _dl_loaded_modules;
    while(tpnt->next) tpnt = tpnt->next;
    tpnt->next = (struct elf_resolve *) _dl_malloc(sizeof(struct elf_resolve));
    _dl_memset (tpnt->next, 0, sizeof (*(tpnt->next)));
    tpnt->next->prev = tpnt;
    tpnt = tpnt->next;
  };
  
  tpnt->next = NULL;
  tpnt->init_flag = 0;
  tpnt->libname = _dl_strdup(libname);
  tpnt->dynamic_addr = dynamic_addr;
  tpnt->dynamic_size = dynamic_size;
  tpnt->libtype = loaded_file;

  if( dynamic_info[DT_HASH] != 0 )
  {
    hash_addr = (unsigned int *) (dynamic_info[DT_HASH] + loadoffs);
    tpnt->nbucket = *hash_addr++;
    tpnt->nchain = *hash_addr++;
    tpnt->elf_buckets = hash_addr;
    hash_addr += tpnt->nbucket;
    tpnt->chains = hash_addr;
  }
  tpnt->loadaddr = loadaddr;
  tpnt->loadoffs = loadoffs;
  for(i=0; i<(DT_NUM + DT_MDEP); i++) tpnt->dynamic_info[i] = dynamic_info[i];
  return tpnt;
}


/*
 * This function resolves externals, and this is either called when we process
 * relocations or when we call an entry in the PLT table for the first time.
 */

char * _dl_find_hash(char * name, struct dyn_elf * rpnt1, 
		     unsigned int instr_addr, struct elf_resolve * f_tpnt, int copyrel){
  struct elf_resolve * tpnt;
  int si;
  char * pnt;
  char * strtab;
  Elf32_Sym * symtab;
  unsigned int elf_hash_number, hn;
  char * weak_result;
  struct elf_resolve * first_def;
  struct dyn_elf * rpnt, first;

  weak_result = 0;
  elf_hash_number = _dl_elf_hash(name);

  /* A quick little hack to make sure that any symbol in the executable
  will be preferred to one in a shared library.  This is necessary so
  that any shared library data symbols referenced in the executable
  will be seen at the same address by the executable, shared libraries
  and dynamically loaded code. -Rob Ryan (robr@cmu.edu) */
  if(!copyrel && rpnt1) {
      first=(*_dl_symbol_tables);
      first.next=rpnt1;
      rpnt1=(&first);
  }
  
  for(rpnt = (rpnt1 ? rpnt1 : _dl_symbol_tables); rpnt; rpnt = rpnt->next) {
    tpnt = rpnt->dyn;

    /*
     * The idea here is that if we are using dlsym, we want to first search
     * the entire chain loaded from dlopen, and return a result from that
     * if we found anything.  If this fails, then we continue the search
     * into the stuff loaded when the image was activated.  For normal
     * lookups, we start with rpnt == NULL, so we should never hit this.
     */
    if( tpnt->libtype == elf_executable
       && weak_result != 0 )
      {
       return weak_result;
      }

    /*
     * Avoid calling .urem here.
     */
    do_rem(hn, elf_hash_number, tpnt->nbucket);
    symtab = (Elf32_Sym *) (tpnt->dynamic_info[DT_SYMTAB] + tpnt->loadoffs);
    strtab = (char *) (tpnt->dynamic_info[DT_STRTAB] + tpnt->loadoffs);
    /*
     * This crap is required because the first instance of a symbol on the
     * chain will be used for all symbol references.  Thus this instance
     * must be resolved to an address that contains the actual function,
     */

    first_def = NULL;

    for(si = tpnt->elf_buckets[hn]; si; si = tpnt->chains[si]){
      pnt = strtab + symtab[si].st_name;

      if(_dl_strcmp(pnt, name) == 0 && 
	 (ELF32_ST_TYPE(symtab[si].st_info) == STT_FUNC ||
	  ELF32_ST_TYPE(symtab[si].st_info) == STT_NOTYPE ||
	  ELF32_ST_TYPE(symtab[si].st_info) == STT_OBJECT) &&
	 symtab[si].st_value != 0) {

	/* Here we make sure that we find a module where the symbol is
	 * actually defined.
	 */

	if(!first_def) first_def = tpnt;
	if(f_tpnt && first_def == f_tpnt && symtab[si].st_shndx == 0)
	  continue;

	switch(ELF32_ST_BIND(symtab[si].st_info)){
	case STB_GLOBAL:
	  return tpnt->loadoffs + symtab[si].st_value;
	case STB_WEAK:
	  if (!weak_result) weak_result = tpnt->loadoffs + symtab[si].st_value;
	    break;
	default:  /* Do local symbols need to be examined? */
	  break;
	}
      }
    }
  }
  return weak_result;
}


