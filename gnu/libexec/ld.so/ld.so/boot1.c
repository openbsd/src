/* Run an ELF binary on a linux system.

   Copyright (C) 1993-1995, Eric Youngdale.

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



/* Program to load an ELF binary on a linux system, and run it.
 * References to symbols in sharable libraries can be resolved by
 * an ELF sharable library. */

/* Disclaimer:  I have never seen any AT&T source code for SVr4, nor have
   I ever taken any courses on internals.  This program was developed using
   information available through the book "UNIX SYSTEM V RELEASE 4,
   Programmers guide: Ansi C and Programming Support Tools", which did
   a more than adequate job of explaining everything required to get this
   working. */

/*
 * The main trick with this program is that initially, we ourselves are not
 * dynamicly linked.  This means that we cannot access any global variables
 * since the GOT is initialized by the linker assuming a virtual address of 0,
 * and we cannot call any functions since the PLT is not initialized at all
 * (it will tend to want to call the dynamic linker
 *
 * There are further restrictions - we cannot use large switch statements,
 * since the compiler generates tables of addresses and jumps through them.
 * We can use inline functions, because these do not transfer control to
 * a new address, but they must be static so that they are not exported
 * from the modules.  We cannot use normal syscall stubs, because these
 * all reference the errno global variable which is not yet initialized.
 * We can use all of the local stack variables that we want, since these
 * are all referenced to %ebp or %esp.
 *
 * Life is further complicated by the fact that initially we do not want
 * to do a complete dynamic linking.  We want to allow the user to supply
 * new functions replacing some of the library versions, and until we have
 * the list of modules that we should search set up, we do not want to do
 * any of this.  Thus I have chosen to only perform the relocations for
 * variables that start with "_dl_" since ANSI specifies that the user is
 * not supposed to redefine any of these variables.
 *
 * Fortunately, the linker itself leaves a few clues lying around, and
 * when the kernel starts the image, there are a few further clues.
 * First of all, there is information buried on the stack that the kernel
 * leaves, which includes information about the load address that the
 * program interpreter was loaded at, the number of sections, the address
 * the application was loaded at and so forth.  Here this information
 * is stored in the array dl_info, and the indicies are taken from the
 * file /usr/include/sys/auxv.h on any SVr4 system.
 *
 * The linker itself leaves a pointer to the .dynamic section in the first
 * slot of the GOT, and as it turns out, %ebx points to ghe GOT when
 * you are using PIC code, so we just dereference this to get the address
 * of the dynamic sections.
 *
 * Typically you must load all text pages as writable so that dynamic linking
 * can succeed.  The kernel under SVr4 loads these as R/O, so we must call
 * mprotect to change the protections.  Once we are done, we should set these
 * back again, so the desired behavior is achieved.  Under linux there is
 * currently no mprotect function in the distribution kernel (although
 * someone has alpha patches), so for now everything is loaded writable.
 *
 * We do not have access to malloc and friends at the initial stages of dynamic
 * linking, and it would be handy to have some scratchpad memory available
 * for use as we set things up.  It is a bit of a kluge, but we mmap /dev/zero
 * to get one page of scratchpad.  A simpleminded _dl_malloc is provided so
 * that we have some memory that can be used for this purpose.  Typically
 * we would not want to use the same memory pool as malloc anyway - the user
 * might want to redefine malloc for example.
 *
 * Our first task is to perform a minimal linking so that we can call other
 * portions of the dynamic linker.  Once we have done this, we then build
 * the list of modules that the application requires, using LD_LIBRARY_PATH
 * if this is not a suid program (/usr/lib otherwise).  Once this is done,
 * we can do the dynamic linking as required (and we must omit the things
 * we did to get the dynamic linker up and running in the first place.
 * After we have done this, we just have a few housekeeping chores and we
 * can transfer control to the user's application.
 */

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/mman.h>
#include "hash.h"
#include <link.h>

#include "sysdep.h"
#include "syscall.h"
#include "string.h"

static char * _dl_malloc_addr, *_dl_mmap_zero;
char * _dl_library_path = 0; /* Where we look for libraries */
char *_dl_preload = 0; /* Things to be loaded before the libs. */
char *_dl_progname = "/lib/ld.so.1";
static char * _dl_not_lazy = 0;
static char * _dl_warn = 0; /* Used by ldd */
static char * _dl_trace_loaded_objects = 0;
static int (*_dl_elf_main)(int, char **, char**);

static int (*_dl_elf_init)(void);
extern void _dl_printhex(int);

void * (*_dl_malloc_function)(int size) = NULL;

struct r_debug * _dl_debug_addr = NULL;

unsigned int * _dl_brkp; 

unsigned int * _dl_envp;

#define DL_MALLOC(SIZE) ((void *) (malloc_buffer += SIZE, malloc_buffer - SIZE))
/*
 * Make sure that the malloc buffer is aligned on 4 byte boundary.  For 64 bit
 * platforms we may need to increase this to 8, but this is good enough for
 * now.  This is typically called after DL_MALLOC.
 */
#define REALIGN() malloc_buffer = (char *) (((unsigned int) malloc_buffer + 3) & ~(3))



#define ELF_HASH(RESULT,NAME) { \
  unsigned long hash = 0; \
    unsigned long tmp;  \
  char * name = NAME; \
  while (*name){  \
    hash = (hash << 4) + *name++; \
    if((tmp = hash & 0xf0000000)) hash ^= tmp >> 24; \
    hash &= ~tmp; \
  } \
  RESULT = hash; \
}
extern _dl_runtime_resolve(void);
extern int _dl_interpreter_exit(int);
extern char * _dl_strdup(const char *);
extern char * _dl_getenv(char * symbol, char ** envp);
extern int _dl_fixup(struct elf_resolve * tpnt);

extern Elf32_Dyn _DYNAMIC[];
/*
 * Datatype of a relocation on this platform
 */
#ifdef ELF_USES_RELOCA
typedef Elf32_Rela ELF_RELOC;
#else
typedef Elf32_Rel ELF_RELOC;
#endif

/*
 * This stub function is used by some debuggers.  The idea is that they
 * can set an internal breakpoint on it, so that we are notified when the
 * address mapping is changed in some way.
 */
static void _dl_debug_state()
{
  return;
}

/*
 *  XXX The following function declaration is a HACK to make this work
 * without asm code for the MIPS processors. Don't ask why... :-)
 */

void _dl_boot(void *a0, void *a1, void *a2, void *a3, void *a4)
{
  unsigned int argc;
  char ** argv, ** envp;

  unsigned int load_addr;
  unsigned int *aux_dat;
  int goof = 0;
  unsigned int *stack;
  struct elf_resolve * tpnt;
  struct dyn_elf * rpnt;
  struct elf_resolve * app_tpnt;
  unsigned int brk_addr;
  unsigned int dl_data[10];
  unsigned char * malloc_buffer, *mmap_zero;
  int (*_dl_atexit)(void *);
  int * lpnt;
  Elf32_Dyn * dpnt;
  unsigned int *hash_addr;
  struct r_debug * debug_addr;
  unsigned int *chains;
  int indx;

  /* First obtain the information on the stack that tells us more about
     what binary is loaded, where it is loaded, etc, etc */


  GET_ARGV(aux_dat, a0);

  stack = aux_dat;	/* Remember this for start later */
  argc = *(aux_dat);
  argv = (char **)(aux_dat + 1);
  aux_dat += argc + 1;  /* Skip over the argv pointers */
  aux_dat++;  /* Skip over NULL at end of argv */
  envp = (char **) aux_dat;
  while(*aux_dat) aux_dat++;  /* Skip over the envp pointers */
  aux_dat++;  /* Skip over NULL at end of envp */
  while(*aux_dat)
    {
      unsigned int * ad1;
      ad1 = aux_dat + 1;
      if( *aux_dat <= AT_ENTRY ) dl_data[*aux_dat] = *ad1;
      aux_dat += 2;
    }

  /* Next, locate the GOT */

  load_addr = elf_machine_load_offset();

  GET_GOT(got);
  dpnt = (Elf32_Dyn *) ((int)_DYNAMIC + load_addr);
 
  /* OK, time for another hack.  Now call mmap to get a page of writable
     memory that can be used for a temporary malloc.  We do not know brk
     yet, so we cannot use real malloc. */

  mmap_zero = malloc_buffer = (unsigned char *) _dl_mmap((void*) 0, 4096,
			     PROT_READ | PROT_WRITE, 
			     MAP_ANON | MAP_COPY, -1, 0);
  if(_dl_mmap_check_error(mmap_zero)) {
	SEND_STDERR("dl_boot: mmap of zeros failed!\n");
	_dl_exit(13);
  }

  tpnt = DL_MALLOC(sizeof(struct elf_resolve));
  REALIGN();
  _dl_memset (tpnt, 0, sizeof (*tpnt));
  app_tpnt = DL_MALLOC(sizeof(struct elf_resolve));
  REALIGN();
  _dl_memset (app_tpnt, 0, sizeof (*app_tpnt));

  /*
   * This is used by gdb to locate the chain of shared libraries that are currently loaded.
   */
  debug_addr = DL_MALLOC(sizeof(struct r_debug));
  REALIGN();
  _dl_memset (debug_addr, 0, sizeof (*debug_addr));

  /* OK, that was easy.  Next scan the DYNAMIC section of the image.
     We are only doing ourself right now - we will have to do the rest later */

  while(dpnt->d_tag != DT_NULL) {
    if(dpnt->d_tag < DT_NUM) {
      tpnt->dynamic_info[dpnt->d_tag] = dpnt->d_un.d_val;
    }
    else if(dpnt->d_tag  >= DT_LOPROC && dpnt->d_tag < DT_LOPROC + DT_MDEP) {
      tpnt->dynamic_info[dpnt->d_tag - DT_LOPROC + DT_NUM] = dpnt->d_un.d_val;
    }
    if(dpnt->d_tag == DT_TEXTREL)
      tpnt->dynamic_info[DT_TEXTREL] = 1;
    dpnt++;
  }

  {
    Elf32_Phdr * ppnt;
    int i;
    
    ppnt = (Elf32_Phdr *) dl_data[AT_PHDR];
    for(i=0; i<dl_data[AT_PHNUM]; i++, ppnt++)
      if(ppnt->p_type == PT_DYNAMIC) {
	dpnt = (Elf32_Dyn *) ppnt->p_vaddr;
	while(dpnt->d_tag != DT_NULL)
	  {
            if(dpnt->d_tag < DT_NUM) {
              app_tpnt->dynamic_info[dpnt->d_tag] = dpnt->d_un.d_val;
            }
            else if(dpnt->d_tag  >= DT_LOPROC && dpnt->d_tag < DT_LOPROC + DT_MDEP) {
              app_tpnt->dynamic_info[dpnt->d_tag - DT_LOPROC + DT_NUM] = dpnt->d_un.d_val;
            }
	/*
	 * We need to store the pointer to the debug data for the
	 * debugger in DYNAMIC if we find a DT_MIPS_RLD_MAP tag. The DYNAMIC
         * area is write protected so we have to temporary remove
         * the write protection.
         */
	    if(dpnt->d_tag == DT_MIPS_RLD_MAP) {
              caddr_t addr;
              addr = (caddr_t)((int)dpnt & ~(dl_data[AT_PAGESZ] - 1));
#if 0
              _dl_mprotect(addr,dl_data[AT_PAGESZ],PROT_EXEC|PROT_READ|PROT_WRITE);
#endif
              *(struct r_debug **)(dpnt->d_un.d_val) = debug_addr;
#if 0
              _dl_mprotect(addr,dl_data[AT_PAGESZ],PROT_EXEC|PROT_READ);
#endif
            }
	    if(dpnt->d_tag == DT_TEXTREL)
              app_tpnt->dynamic_info[DT_TEXTREL] = 1;
	    dpnt++;
	  }
      }
  }

  /* Get some more of the information that we will need to dynamicly link
     this module to itself */

  hash_addr = (unsigned int *) (tpnt->dynamic_info[DT_HASH]+load_addr);
  tpnt->nbucket = *hash_addr++;
  tpnt->nchain = *hash_addr++;
  tpnt->elf_buckets = hash_addr;
  hash_addr += tpnt->nbucket;
  chains = hash_addr;

  /* OK, now do the relocations.  We do not do a lazy binding here, so
   that once we are done, we have considerably more flexibility. */

  goof = 0;
  for(indx=0; indx < 2; indx++)
    {
      int i;
      ELF_RELOC * rpnt;
      unsigned int * reloc_addr;
      unsigned int symbol_addr;
      int symtab_index;
      unsigned int rel_addr, rel_size;

  
      rel_addr = (indx ? tpnt->dynamic_info[DT_JMPREL] : tpnt->dynamic_info[DT_REL]);
      rel_size = (indx ? tpnt->dynamic_info[DT_PLTRELSZ] : tpnt->dynamic_info[DT_RELSZ]);


      if(!rel_addr) continue;

      /* Now parse the relocation information */
      rpnt = (ELF_RELOC *) (rel_addr + load_addr);
      for(i=0; i< rel_size; i+=sizeof(ELF_RELOC), rpnt++){
	Elf32_Sym * symtab;
	char * strtab;
	reloc_addr = (int *) (load_addr + (int)rpnt->r_offset);
	symtab_index = ELF32_R_SYM(rpnt->r_info);
	symbol_addr = 0;
	symtab = (Elf32_Sym *) (tpnt->dynamic_info[DT_SYMTAB]+load_addr);
	strtab = (char *) (tpnt->dynamic_info[DT_STRTAB]+load_addr);
	if(symtab_index) {

	  /* We only do a partial dynamic linking right now.  The user
	     is not supposed to redefine any symbols that start with
	     a '_', so we can do this with confidence. */

	  symbol_addr = load_addr + symtab[symtab_index].st_value;

	  if(!symbol_addr) {
	    /*
	     * This will segfault - you cannot call a function until
	     * we have finished the relocations.
	     */
	    SEND_STDERR("ELF dynamic loader - unable to self-bootstrap - symbol ");
	    SEND_STDERR(strtab + symtab[symtab_index].st_name);
	    SEND_STDERR(" undefined.\n");
	    goof++;
	  }
	}
	/*
	 * Use this machine-specific macro to perform the actual relocation.
	 */
	PERFORM_BOOTSTRAP_RELOC(rpnt, reloc_addr, &symtab[symtab_index], load_addr);
      }
    }

  if (goof)    _dl_exit(14);

  /* OK, at this point we have a crude malloc capability.  Start to build
     the tables of the modules that are required for this beast to run.
     We start with the basic executable, and then go from there.  Eventually
     we will run across ourself, and we will need to properly deal with that
     as well. */

  _dl_malloc_addr = malloc_buffer;

  _dl_mmap_zero = mmap_zero;
/*  tpnt = _dl_malloc(sizeof(struct elf_resolve)); */

/* Now we have done the mandatory linking of some things.  We are now
   free to start using global variables, since these things have all been
   fixed up by now.  Still no function calls outside of this library ,
   since the dynamic resolver is not yet ready. */

  lpnt = (int *) (tpnt->dynamic_info[DT_PLTGOT] + load_addr);
  lpnt[0] = (int)_dl_runtime_resolve;
  if(lpnt[1] & 0x80000000)
    lpnt[1] = (Elf32_Addr) ((unsigned) tpnt | 0x80000000);

  /* OK, this was a big step, now we need to scan all of the user images
     and load them properly. */

  tpnt->next = 0;
  tpnt->libname = 0;
  tpnt->libtype = program_interpreter;

  { Elf32_Ehdr * epnt;
    Elf32_Phdr * ppnt;
    int i;

    epnt = (Elf32_Ehdr *) dl_data[AT_BASE];
    tpnt->n_phent = epnt->e_phnum;
    tpnt->ppnt = ppnt = (Elf32_Phdr *) ((int)epnt + epnt->e_phoff);
    for(i=0;i < epnt->e_phnum; i++, ppnt++){
      if(ppnt->p_type == PT_DYNAMIC) {
	tpnt->dynamic_addr = ppnt->p_vaddr + load_addr;
	tpnt->dynamic_size = ppnt->p_filesz;
      }
    }
  }

  tpnt->chains = chains;
  tpnt->loadaddr = (char *) load_addr;
  tpnt->loadoffs = (char *) 0;

  brk_addr = 0;
  rpnt = NULL;

  /* At this point we are now free to examine the user application,
     and figure out which libraries are supposed to be called.  Until
     we have this list, we will not be completely ready for dynamic linking */

  {
    Elf32_Phdr * ppnt;
    int i;

    ppnt = (Elf32_Phdr *) dl_data[AT_PHDR];
    for(i=0; i<dl_data[AT_PHNUM]; i++, ppnt++) {
      if(ppnt->p_type == PT_LOAD) {
	if(ppnt->p_vaddr + ppnt->p_memsz > brk_addr) 
	  brk_addr = ppnt->p_vaddr + ppnt->p_memsz;
      }
      if(ppnt->p_type == PT_DYNAMIC) {
	/* make sure it's really there. */
	if (app_tpnt->dynamic_info[DT_PLTGOT] == 0) continue;
	/* OK, we have what we need - slip this one into the list. */
	app_tpnt = _dl_add_elf_hash_table("", 0, 0,
			    app_tpnt->dynamic_info, ppnt->p_vaddr, ppnt->p_filesz);
	_dl_loaded_modules->libtype = elf_executable;
	_dl_loaded_modules->ppnt = (Elf32_Phdr *) dl_data[AT_PHDR];
	_dl_loaded_modules->n_phent = dl_data[AT_PHNUM];
	_dl_symbol_tables = rpnt = (struct dyn_elf *) _dl_malloc(sizeof(struct dyn_elf));
	_dl_memset (rpnt, 0, sizeof (*rpnt));
	rpnt->dyn = _dl_loaded_modules;
	app_tpnt->usage_count++;
	app_tpnt->symbol_scope = _dl_symbol_tables;
	lpnt = (int *) (app_tpnt->dynamic_info[DT_PLTGOT]);
        lpnt[0] = (int)_dl_runtime_resolve;
        if(lpnt[1] & 0x80000000)
          lpnt[1] = (Elf32_Addr) ((unsigned) app_tpnt | 0x80000000);
      }
      if(ppnt->p_type == PT_INTERP) { /* OK, fill this in - we did not have
					 this before */
	tpnt->libname =  _dl_strdup((char *) ppnt->p_vaddr);
      }
    }
  }

  if (argv[0])
    _dl_progname = argv[0];

  /* Now we need to figure out what kind of options are selected.
   Note that for SUID programs we ignore the settings in LD_LIBRARY_PATH */
  {
    _dl_not_lazy = _dl_getenv("LD_BIND_NOW",envp);

#ifndef IBCS_COMPATIBLE
    _dl_preload = _dl_getenv("LD_ELF_PRELOAD", envp);
    if (!_dl_preload)
#endif
      _dl_preload = _dl_getenv("LD_PRELOAD", envp);
#ifndef IBCS_COMPATIBLE
    _dl_library_path = _dl_getenv("LD_ELF_LIBRARY_PATH", envp);
    if (!_dl_library_path)
#endif
      _dl_library_path = _dl_getenv("LD_LIBRARY_PATH",envp);

    if(!_dl_suid_ok() ) {
      if (_dl_preload) {
	*_dl_preload = '\0';
	_dl_preload = 0;
      }
      if (_dl_library_path) {
	*_dl_library_path = '\0';
	_dl_library_path = 0;
      }
    }
  }

  _dl_trace_loaded_objects = _dl_getenv("LD_TRACE_LOADED_OBJECTS", envp);


  /* OK, we now have the application in the list, and we have some
     basic stuff in place.  Now search through the list for other shared
     libraries that should be loaded, and insert them on the list in the
     correct order. */

#ifdef USE_CACHE
  _dl_map_cache();
#endif

  {
    struct elf_resolve *tcurr;
    struct elf_resolve * tpnt1;
    char * lpnt;

    tcurr = _dl_loaded_modules;
    if (_dl_preload)  {
      int i;
      char *str;
      char c=1;
      str = _dl_preload; 
      for (i = 0; c != 0; i++) {
	if (_dl_preload[i] == ':' || _dl_preload[i] == '\0') {
	  c= _dl_preload[i];
	  _dl_preload[i]=0;
	  tpnt1 = _dl_load_shared_library(_dl_symbol_tables, str);
	  if (!tpnt1) {
	    if (_dl_trace_loaded_objects)
	      _dl_fdprintf(1, "\t%s => not found\n", str);
	    else
	    {
	      _dl_fdprintf(2, "%s: can't load library '%s'\n",
			   _dl_progname,str);
	      _dl_exit(15);
	    }
	  }
	  else
	  {
	    if (_dl_trace_loaded_objects && !tpnt1->usage_count)
	      _dl_fdprintf(1, "\t%s => %s\n", str, tpnt1->libname);
	    rpnt->next = 
	      (struct dyn_elf *) _dl_malloc(sizeof(struct dyn_elf));
	    _dl_memset (rpnt->next, 0, sizeof (*(rpnt->next)));
	    rpnt = rpnt->next;
	    tpnt1->usage_count++;
	    tpnt1->symbol_scope = _dl_symbol_tables;
	    tpnt1->libtype = elf_lib;
	    rpnt->dyn = tpnt1;
	  }
	  str=_dl_preload+i+1;
	  _dl_preload[i]=c;
	}
      }
    }
    do{
      for(dpnt = (Elf32_Dyn *) tcurr->dynamic_addr; dpnt->d_tag; dpnt++)
	{
	  if(dpnt->d_tag == DT_NEEDED)
	    {
	      lpnt = tcurr->loadoffs + tcurr->dynamic_info[DT_STRTAB] + 
		dpnt->d_un.d_val;
	      if(tpnt && _dl_strcmp(lpnt, tpnt->libname) == 0) {
		struct elf_resolve * ttmp;
		ttmp = _dl_loaded_modules;
		while(ttmp->next) ttmp = ttmp->next;
		ttmp->next = tpnt;
		tpnt->prev = ttmp;
		tpnt->next = NULL;
		rpnt->next = 
		   (struct dyn_elf *) _dl_malloc(sizeof(struct dyn_elf));
		_dl_memset (rpnt->next, 0, sizeof (*(rpnt->next)));
		rpnt = rpnt->next;
		rpnt->dyn = tpnt;
		tpnt->usage_count++;
		tpnt->symbol_scope = _dl_symbol_tables;
		tpnt = NULL;
		continue;
	      }
	      if(!(tpnt1 = _dl_load_shared_library(_dl_symbol_tables, lpnt))) {
		if (_dl_trace_loaded_objects)
		  _dl_fdprintf(1, "\t%s => not found\n", lpnt);
		else
		{
		  _dl_fdprintf(2, "%s: can't load library '%s'\n",
			       _dl_progname, lpnt);
		  _dl_exit(16);
		}
	      }
	      else
	      {
		if (_dl_trace_loaded_objects && !tpnt1->usage_count)
		  _dl_fdprintf(1, "\t%s => %s\n", lpnt, tpnt1->libname);
		rpnt->next = 
		  (struct dyn_elf *) _dl_malloc(sizeof(struct dyn_elf));
		_dl_memset (rpnt->next, 0, sizeof (*(rpnt->next)));
		rpnt = rpnt->next;
		tpnt1->usage_count++;
		tpnt1->symbol_scope = _dl_symbol_tables;
		tpnt1->libtype = elf_lib;
		rpnt->dyn = tpnt1;
	      }
	    }
	}
      
      tcurr = tcurr->next;
    } while(tcurr);
  }

#ifdef USE_CACHE
  _dl_unmap_cache();
#endif

    /* ldd uses uses this.  I am not sure how you pick up the other flags */ 
  if(_dl_trace_loaded_objects)
    {
      _dl_warn = _dl_getenv("LD_WARN", envp);
      if(!_dl_warn) _dl_exit(0);
    }

  /*
   * If the program interpreter is not in the module chain, add it.  This will
   * be required for dlopen to be able to access the internal functions in the 
   * dynamic linker.
   */
  if(tpnt) {
    struct elf_resolve * tcurr;

    tcurr = _dl_loaded_modules;
    if (tcurr)
      while(tcurr->next) tcurr = tcurr->next;
    tpnt->next = NULL;
    tpnt->usage_count++;

    if (tcurr) {
      tcurr->next = tpnt;
      tpnt->prev = tcurr;
    }
    else {
      _dl_loaded_modules = tpnt;
      tpnt->prev = NULL;
    }
    if (rpnt) {
      rpnt->next = 
	(struct dyn_elf *) _dl_malloc(sizeof(struct dyn_elf));
      _dl_memset (rpnt->next, 0, sizeof (*(rpnt->next)));
      rpnt = rpnt->next;
    } else {
      rpnt = 	(struct dyn_elf *) _dl_malloc(sizeof(struct dyn_elf));
      _dl_memset (rpnt, 0, sizeof (*(rpnt->next)));
    }
    rpnt->dyn = tpnt;
    tpnt = NULL;
  }

  /*
   * OK, now all of the kids are tucked into bed in their proper addresses.
   * Now we go through and look for REL and RELA records that indicate fixups
   * to the GOT tables.  We need to do this in reverse order so that COPY
   * directives work correctly */


  goof = _dl_fixup(_dl_loaded_modules);

  if(goof || _dl_trace_loaded_objects) _dl_exit(0);

  /* OK, at this point things are pretty much ready to run.  Now we
     need to touch up a few items that are required, and then
     we can let the user application have at it.  Note that
     the dynamic linker itself is not guaranteed to be fully
     dynamicly linked if we are using ld.so.1, so we have to look
     up each symbol individually. */


  _dl_brkp = (unsigned int *) _dl_find_hash("___brk_addr", NULL, 1, NULL, 0);
  if (_dl_brkp) *_dl_brkp = brk_addr;

  _dl_envp = (unsigned int *) _dl_find_hash("__environ", NULL, 1, NULL, 0);

  if (_dl_envp) *_dl_envp = (unsigned int) envp;

  _dl_atexit = (int (*)(void *)) _dl_find_hash("atexit", NULL, 1, NULL, 0);

  for(tpnt = _dl_loaded_modules; tpnt; tpnt = tpnt->next)
    {
      /* Apparently crt1 for the application is responsible for handling this.
       * We only need to run the init/fini for shared libraries
       */
      if (tpnt->libtype == program_interpreter ||
	tpnt->libtype == elf_executable) continue;
      if (tpnt->init_flag & INIT_FUNCS_CALLED) continue;
      tpnt->init_flag |= INIT_FUNCS_CALLED;
      
      if(tpnt->dynamic_info[DT_INIT]) {
	_dl_elf_init = (int (*)(void)) (tpnt->loadoffs +
				    tpnt->dynamic_info[DT_INIT]);
	(*_dl_elf_init)();
      }
      if(_dl_atexit && tpnt->dynamic_info[DT_FINI])
      {
        (*_dl_atexit)(tpnt->loadoffs + tpnt->dynamic_info[DT_FINI]);
      }
#undef DL_DEBUG
#ifdef DL_DEBUG
      else
      {
	_dl_fdprintf(2, tpnt->libname);
	_dl_fdprintf(2, ": ");
	if (!_dl_atexit)
	  _dl_fdprintf(2, "The address is atexit () is 0x0.");
	if (!tpnt->dynamic_info[DT_FINI])
	  _dl_fdprintf(2, "Invalid .fini section.");
	_dl_fdprintf(2, "\n");
      }
#endif
#undef DL_DEBUG
   }

  /*
   * OK, fix one more thing - set up the debug_addr structure to point
   * to our chain.  Later we may need to fill in more fields, but this
   * should be enough for now. 
   */
  debug_addr->r_map = (struct link_map *) _dl_loaded_modules;
  debug_addr->r_version = 1;
  debug_addr->r_ldbase = load_addr;
  debug_addr->r_brk = (unsigned long) &_dl_debug_state;
  _dl_debug_addr = debug_addr;

  /* OK we are done here.  Turn out the lights, and lock up. */
  _dl_elf_main = (int (*)(int, char**, char**)) dl_data[AT_ENTRY];

  /*
   * Transfer control to the application.
   */
  START();
}


int _dl_fixup(struct elf_resolve * tpnt)
{
  int goof = 0;
  if(tpnt->next) goof += _dl_fixup(tpnt->next);

  if(tpnt->dynamic_info[DT_REL]) {
    if (tpnt->init_flag & RELOCS_DONE) return goof;
    tpnt->init_flag |= RELOCS_DONE;
   
    goof += _dl_parse_relocation_information(tpnt, tpnt->dynamic_info[DT_REL],
					     tpnt->dynamic_info[DT_RELSZ], 0);
  }
  if(tpnt->dynamic_info[DT_RELA]) {
    _dl_fdprintf(2, "%s: can't handle RELA relocation records\n", _dl_progname);
    _dl_exit(18);
  }
  if(tpnt->dynamic_info[DT_JMPREL])
    {
      if (tpnt->init_flag & JMP_RELOCS_DONE) return goof;
      tpnt->init_flag |= JMP_RELOCS_DONE;
      
      if(! _dl_not_lazy || *_dl_not_lazy == 0)
	goof += _dl_parse_relocation_information(tpnt,
					  tpnt->dynamic_info[DT_JMPREL],
					  tpnt->dynamic_info[DT_PLTRELSZ], 0);
      else
	goof += _dl_parse_relocation_information(tpnt,
					  tpnt->dynamic_info[DT_JMPREL],
					  tpnt->dynamic_info[DT_PLTRELSZ], 1);
    }

  elf_machine_got_rel(tpnt);

  return goof;
}

void * _dl_malloc(int size) {
  void * retval;

  if(_dl_malloc_function)
  	return (*_dl_malloc_function)(size);

  if(_dl_malloc_addr-_dl_mmap_zero+size>4096) {
	_dl_mmap_zero = _dl_malloc_addr = (unsigned char *) _dl_mmap((void*) 0, size,
  				PROT_READ | PROT_WRITE, 
  				MAP_ANON | MAP_COPY, -1, 0);
  	if(_dl_mmap_check_error(_dl_mmap_zero)) {
  	    _dl_fdprintf(2, "%s: can't map '/dev/zero'\n", _dl_progname);
  	    _dl_exit(20);
  	}
  }
  retval = _dl_malloc_addr;
  _dl_malloc_addr += size;

  /*
   * Align memory to 4 byte boundary.  Some platforms require this, others
   * simply get better performance.
   */
  _dl_malloc_addr = (char *) (((unsigned int) _dl_malloc_addr + 3) & ~(3));
  return retval;
}

char * _dl_getenv(char * symbol, char ** envp)
{
  char * pnt;
  char * pnt1;
  while ((pnt = *envp++)) {
    pnt1 = symbol;
    while(*pnt && *pnt1 && *pnt == *pnt1) {pnt1++; pnt++;}
    if(!*pnt || *pnt != '=' || *pnt1) continue;
    return pnt+1;
  }
  return 0;
}

char * _dl_strdup(const char * string){
  void * retval;
  char * pnt;

  pnt = retval = _dl_malloc_addr;
  while(*string)
    *pnt++ = *string++;
  *pnt++ = 0;
  _dl_malloc_addr = pnt;
  /*
   * Align memory to 4 byte boundary.  Some platforms require this, others
   * simply get better performance.
   */
  _dl_malloc_addr = (char *) (((unsigned int) _dl_malloc_addr + 3) & ~(3));
  return retval;
}

/* In principle we could do the .fini stuff here, but we already
   registered this stuff with atexit */
int _dl_interpreter_exit(int exitcode){
/*  _dl_fdprintf(2, "Hey, look where I am!\n"); */
  return 0;
}

void _dl_printhex(no)
{
static char xc[]="0123456789abcdef";

        int i;  

        _dl_write(1, " 0x", 3);
        for(i=0; i < 8; i++) {
                _dl_write(1, &xc[((no >> 28) & 0xf)], 1);
                no <<= 4;
        }       
}             

