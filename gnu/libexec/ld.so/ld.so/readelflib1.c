/* Load an ELF sharable library into memory.

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



/* This file contains the helper routines to load an ELF sharable
   library into memory and add the symbol table info to the chain. */

#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "hash.h"
#include "sysdep.h"
#include <unistd.h>
#include "syscall.h"
#include "string.h"
#ifdef USE_CACHE
#include "../config.h"
#endif

extern char *_dl_progname;
extern _dl_runtime_resolve(void);

#ifdef USE_CACHE

static caddr_t _dl_cache_addr = NULL;
static size_t _dl_cache_size = 0;

int _dl_map_cache(void)
{
  int fd;
  struct stat st;
  header_t *header;

  if (_dl_cache_addr != NULL)
    return -1;

  if (_dl_stat(LDSO_CACHE, &st) || (fd = _dl_open(LDSO_CACHE, O_RDONLY)) < 0)
  {
    _dl_fdprintf(2, "%s: can't open cache '%s'\n", _dl_progname, LDSO_CACHE);
    _dl_cache_addr = (caddr_t)-1; /* so we won't try again */
    return -1;
  }

  _dl_cache_size = st.st_size;
  _dl_cache_addr = (caddr_t)_dl_mmap(0, _dl_cache_size, PROT_READ,
				     MAP_SHARED, fd, 0);
  _dl_close (fd);
  if (_dl_cache_addr == (caddr_t)-1)
  {
    _dl_fdprintf(2, "%s: can't map cache '%s'\n", _dl_progname, LDSO_CACHE);
    return -1;
  }

  header = (header_t *)_dl_cache_addr;

  if (_dl_memcmp(header->magic, LDSO_CACHE_MAGIC, LDSO_CACHE_MAGIC_LEN))
  {
    _dl_fdprintf(2, "%s: cache '%s' is corrupt\n", _dl_progname, LDSO_CACHE);
    goto fail;
  }

  if (_dl_memcmp (header->version, LDSO_CACHE_VER, LDSO_CACHE_VER_LEN))
  {
    _dl_fdprintf(2, "%s: cache '%s' has wrong version\n", _dl_progname, LDSO_CACHE);
    goto fail;
  }

  return 0;

fail:
  _dl_munmap(_dl_cache_addr, _dl_cache_size);
  _dl_cache_addr = (caddr_t)-1;
  return -1;
}

int _dl_unmap_cache(void)
{
  if (_dl_cache_addr == NULL || _dl_cache_addr == (caddr_t)-1)
    return -1;

  _dl_munmap (_dl_cache_addr, _dl_cache_size);
  _dl_cache_addr = NULL;

  return 0;
}

#endif

/*
 * Used to return error codes back to dlopen et. al.
 */

unsigned int _dl_error_number;
unsigned int _dl_internal_error_number;

struct elf_resolve * _dl_load_shared_library(struct dyn_elf * dpnt,
	char * full_libname){
  char * pnt, *pnt1, *pnt2;
  struct elf_resolve * tpnt, *tpnt1;
  char mylibname[1024];
  char * libname;

  _dl_internal_error_number = 0;
  pnt = libname = full_libname;
  while (*pnt) {
    if(*pnt == '/') libname = pnt+1;
    pnt++;
  }

 /* If the filename has any '/', try it straight and leave it at that. */

  if (libname != full_libname) {
    tpnt1 = _dl_load_elf_shared_library(full_libname, 0);
    if (tpnt1)
      return tpnt1;
    goto goof;
  }

  /* Check in LD_{ELF_}LIBRARY_PATH, if specified and allowed */
  pnt1 = _dl_library_path;
  if (pnt1 && *pnt1) {
    while (*pnt1) {
      pnt2 = mylibname;
      while(*pnt1 && *pnt1 != ':' && *pnt1 != ';') *pnt2++ = *pnt1++;
      if(pnt2[-1] != '/') *pnt2++ = '/';
      pnt = libname;
      while(*pnt) *pnt2++  = *pnt++;
      *pnt2++ = 0;
      tpnt1 = _dl_load_elf_shared_library(mylibname, 0);
      if(tpnt1) return tpnt1;
      if(*pnt1 == ':' || *pnt1 == ';') pnt1++;
    }
  }

#ifdef USE_CACHE
  if (_dl_cache_addr != NULL && _dl_cache_addr != (caddr_t)-1)
  {
    int i;
    header_t *header = (header_t *)_dl_cache_addr;
    libentry_t *libent = (libentry_t *)&header[1];
    char *strs = (char *)&libent[header->nlibs];

    for (i = 0; i < header->nlibs; i++)
    {
      if (libent[i].flags == LIB_ELF &&
	  _dl_strcmp(libname, strs+libent[i].sooffset) == 0 &&
	  (tpnt1 = _dl_load_elf_shared_library(strs+libent[i].liboffset, 0)))
	return tpnt1;
    }
  }
#endif

  /* Check in rpath directories */
  for(; dpnt; dpnt = dpnt->next) {
    tpnt = dpnt->dyn;
    pnt1 = (char *) tpnt->dynamic_info[DT_RPATH];
    if(pnt1) {
      pnt1 += (unsigned int) tpnt->loadoffs + tpnt->dynamic_info[DT_STRTAB];
      while(*pnt1){
	pnt2 = mylibname;
	while(*pnt1 && *pnt1 != ':') *pnt2++ = *pnt1++;
	if(pnt2[-1] != '/') *pnt2++ = '/';
	pnt = libname;
	while(*pnt) *pnt2++  = *pnt++;
	*pnt2++ = 0;
	tpnt1 = _dl_load_elf_shared_library(mylibname, 0);
	if(tpnt1) return tpnt1;
	if(*pnt1 == ':') pnt1++;
      }
    }
  }

  /* Check in /usr/lib */
  pnt1 = "/usr/lib/";
  pnt = mylibname;
  while(*pnt1) *pnt++ = *pnt1++;
  pnt1 = libname;
  while(*pnt1) *pnt++ = *pnt1++;
  *pnt++ = 0;
  tpnt1 = _dl_load_elf_shared_library(mylibname, 0);
  if (tpnt1) return tpnt1;
  
  /* Check in /lib */
  /* try "/lib/". */
  pnt1 = "/lib/";
  pnt = mylibname;
  while(*pnt1) *pnt++ = *pnt1++;
  pnt1 = libname;
  while(*pnt1) *pnt++ = *pnt1++;
  *pnt++ = 0;
  tpnt1 = _dl_load_elf_shared_library(mylibname, 0);
  if (tpnt1) return tpnt1;

goof:
  /* Well, we shot our wad on that one.  All we can do now is punt */
  if (_dl_internal_error_number) _dl_error_number = _dl_internal_error_number;
	else _dl_error_number = DL_ERROR_NOFILE;
  return NULL;
}

/*
 * Read one ELF library into memory, mmap it into the correct locations and
 * add the symbol info to the symbol chain.  Perform any relocations that
 * are required.
 */

struct elf_resolve * _dl_load_elf_shared_library(char * libname, int flag){
  Elf32_Ehdr * epnt;
  unsigned int dynamic_addr = 0;
  unsigned int dynamic_size = 0;
  Elf32_Dyn * dpnt;
  struct elf_resolve * tpnt;
  Elf32_Phdr * ppnt;
  int piclib;
  char * status;
  int flags;
  char header[4096];
  int dynamic_info[DT_NUM + DT_MDEP];
  int * lpnt;
  unsigned int libaddr;
  unsigned int liboffs;
  unsigned int minvma=0xffffffff, maxvma=0;
  
  int i;
  int infile;

  /* If this file is already loaded, skip this step */
  tpnt = _dl_check_hashed_files(libname);
  if(tpnt) return tpnt;

  libaddr = 0;
  liboffs = 0;
  infile = _dl_open(libname, O_RDONLY);
  if(infile < 0)
  {
#if 0
    /*
     * NO!  When we open shared libraries we may search several paths.
     * it is inappropriate to generate an error here.
     */
    _dl_fdprintf(2, "%s: can't open '%s'\n", _dl_progname, libname);
#endif
    _dl_internal_error_number = DL_ERROR_NOFILE;
    return NULL;
  }
 
  _dl_read(infile, header, sizeof(header));
  epnt = (Elf32_Ehdr *) header;
  if (epnt->e_ident[0] != 0x7f ||
      epnt->e_ident[1] != 'E' ||
      epnt->e_ident[2] != 'L' ||
      epnt->e_ident[3] != 'F') {
    _dl_fdprintf(2, "%s: '%s' is not an ELF file\n", _dl_progname, libname);
    _dl_internal_error_number = DL_ERROR_NOTELF;
    _dl_close(infile);
    return NULL;
  };
  
  if((epnt->e_type != ET_DYN) || 
     (epnt->e_machine != MAGIC1 
#ifdef MAGIC2
      && epnt->e_machine != MAGIC2
#endif
      )){
    _dl_internal_error_number = (epnt->e_type != ET_DYN ? DL_ERROR_NOTDYN : DL_ERROR_NOTMAGIC);
    _dl_fdprintf(2, "%s: '%s' is not an ELF executable for " ELF_TARGET "\n",
		 _dl_progname, libname);
    _dl_close(infile);
    return NULL;
  };

  ppnt = (Elf32_Phdr *) &header[epnt->e_phoff];

  piclib = 1;
  for(i=0;i < epnt->e_phnum; i++){

    if(ppnt->p_type == PT_DYNAMIC) {
      if (dynamic_addr)
	_dl_fdprintf(2, "%s: '%s' has more than one dynamic section\n",
		     _dl_progname, libname);
      dynamic_addr = ppnt->p_vaddr;
      dynamic_size = ppnt->p_filesz;
    };

    if(ppnt->p_type == PT_LOAD) {
	/* See if this is a PIC library. */
	if(i == 0 && ppnt->p_vaddr > 0x1000000) {
	    piclib = 0;
	    minvma=ppnt->p_vaddr;
	}
	if(piclib && ppnt->p_vaddr < minvma) {
	    minvma = ppnt->p_vaddr;
	}
	if(((unsigned int)ppnt->p_vaddr + ppnt->p_memsz) > maxvma) {
	    maxvma = ppnt->p_vaddr + ppnt->p_memsz;
	}
     }
    ppnt++;
  };

  maxvma=(maxvma+0xfffU)&~0xfffU;
  minvma=minvma&~0xffffU;
  
  flags = MAP_COPY;
  if(!piclib) flags|=MAP_FIXED;
  
  status = (char *) _dl_mmap((char *) (piclib?0:minvma),
			  maxvma-minvma, 
			  PROT_NONE, 
			  flags | MAP_ANON, -1,
			  0);
  if(_dl_mmap_check_error(status)) {
    _dl_fdprintf(2, "%s: can't map 'zero'\n", _dl_progname);
    _dl_internal_error_number = DL_ERROR_MMAP_FAILED;
    _dl_close(infile);
     return NULL;
  };
  libaddr=(unsigned int)status;
  liboffs = libaddr - minvma;
  flags|=MAP_FIXED;
  
  /* Get the memory to store the library */
  ppnt = (Elf32_Phdr *) &header[epnt->e_phoff];
  
  for(i=0;i < epnt->e_phnum; i++){
    if(ppnt->p_type == PT_LOAD) {

      /* See if this is a PIC library. */
      if(i == 0 && ppnt->p_vaddr > 0x1000000) {
	piclib = 0;
	/* flags |= MAP_FIXED; */
      }


      
      if(ppnt->p_flags & PF_W) {
	unsigned int map_size;
	char * cpnt;
	
	status = (char *) _dl_mmap((char *) (libaddr - minvma +
					     (ppnt->p_vaddr & 0xfffff000)),
			  (ppnt->p_vaddr & 0xfff) + ppnt->p_filesz, 
			  LXFLAGS(ppnt->p_flags), 
			  flags, infile,
			  ppnt->p_offset & 0x7ffff000);
	
	if(_dl_mmap_check_error(status)) {
	    _dl_fdprintf(2, "%s: can't map '%s'\n", _dl_progname, libname);
	    _dl_internal_error_number = DL_ERROR_MMAP_FAILED;
	    _dl_munmap((char *)libaddr, maxvma-minvma);
	    _dl_close(infile);
	    return NULL;
	};
	
	if(!piclib) status = 0;
	
	/* Pad the last page with zeros. */
	cpnt =(char *) (status + (ppnt->p_vaddr & 0xfff) + ppnt->p_filesz);
	while(((unsigned int) cpnt) & 0xfff) *cpnt++ = 0;
	
/* I am not quite sure if this is completely correct to do or not, but
   the basic way that we handle bss segments is that we mmap zerofill if
   there are any pages left over that are not mapped as part of the file */

	map_size = (ppnt->p_vaddr - minvma + ppnt->p_filesz + 0xfff) & 0xfffff000;
	if(map_size < ppnt->p_vaddr - minvma + ppnt->p_memsz)
	  status = (char *) _dl_mmap((char *) map_size + libaddr, 
			    ppnt->p_vaddr - minvma + ppnt->p_memsz - map_size,
			    LXFLAGS(ppnt->p_flags),
			    flags | MAP_ANON, -1, 0);
      } else
	status = (char *) _dl_mmap((char *) (ppnt->p_vaddr & 0xfffff000) + 
				   libaddr - minvma, 
			  ((ppnt->p_vaddr - minvma) & 0xfff) + ppnt->p_filesz, 
			  LXFLAGS(ppnt->p_flags), 
			  flags, infile, 
			  ppnt->p_offset & 0x7ffff000);
      if(_dl_mmap_check_error(status)) {
	_dl_fdprintf(2, "%s: can't map '%s'\n", _dl_progname, libname);
	_dl_internal_error_number = DL_ERROR_MMAP_FAILED;
	_dl_munmap((char *)libaddr, maxvma-minvma);
	_dl_close(infile);
	return NULL;
      };
      /* if(libaddr == 0 && piclib) {
	libaddr = (unsigned int) status;
	flags |= MAP_FIXED;
      }; */
    };
    ppnt++;
  };
  _dl_close(infile);
  
  /* For a non-PIC library, the addresses are all absolute */
  if(!piclib) libaddr = 0;

  dynamic_addr -= (unsigned int) minvma;
  dynamic_addr += (unsigned int) libaddr;

 /* 
  * OK, the ELF library is now loaded into VM in the correct locations
  * The next step is to go through and do the dynamic linking (if needed).
  */
  
  /* Start by scanning the dynamic section to get all of the pointers */
  
  if(!dynamic_addr) {
    _dl_internal_error_number = DL_ERROR_NODYNAMIC;
    _dl_fdprintf(2, "%s: '%s' is missing a dynamic section\n", _dl_progname, libname);
    return NULL;
  }

  dpnt = (Elf32_Dyn *) dynamic_addr;

  _dl_memset(dynamic_info, 0, sizeof(dynamic_info));
  while(dpnt->d_tag != DT_NULL) { 
    if(dpnt->d_tag < DT_NUM) { 
      dynamic_info[dpnt->d_tag] = dpnt->d_un.d_val;
    }
    else if(dpnt->d_tag  >= DT_LOPROC && dpnt->d_tag < DT_LOPROC + DT_MDEP) {
      dynamic_info[dpnt->d_tag - DT_LOPROC + DT_NUM] = dpnt->d_un.d_val;
    }
    if(dpnt->d_tag == DT_TEXTREL)
      dynamic_info[DT_TEXTREL] = 1;
    dpnt++;
  }
  dynamic_size = dpnt - (Elf32_Dyn *) dynamic_addr;

  tpnt = _dl_add_elf_hash_table(libname, (char *)libaddr, (char *)liboffs,
				dynamic_info, dynamic_addr, dynamic_size);

  tpnt->ppnt = (Elf32_Phdr *) (tpnt->loadaddr + epnt->e_phoff);
  tpnt->n_phent = epnt->e_phnum;

  /*
   * OK, the next thing we need to do is to insert the dynamic linker into
   * the proper entry in the GOT so that the PLT symbols can be properly
   * resolved. 
   */
  
  lpnt = (int *) dynamic_info[DT_PLTGOT];
  
  if(lpnt) {
    lpnt = (int *) (dynamic_info[DT_PLTGOT] + ((int)liboffs));
    lpnt[0] = (int)_dl_runtime_resolve;
    if(lpnt[1] & 0x80000000)
      lpnt[1] = (Elf32_Addr) ((unsigned) tpnt | 0x80000000);
  };
  
  return tpnt;
}

