/*	$OpenBSD: link_elf.h,v 1.3 2002/09/08 01:28:58 deraadt Exp $	*/

/*
 * Copyright (c) 1996 Per Fogelstrom
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
 *	Per Fogelstrom.
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

#ifndef _LINK_ELF_H

#include <elf_abi.h>

#ifndef DT_PROCNUM
#define DT_PROCNUM 0
#endif

/* Structure describing a loaded shared object.  The `l_next' and `l_prev'
   members form a chain of all the shared objects loaded at startup.

   These data structures exist in space used by the run-time dynamic linker;
   modifying them may have disastrous results.  */

struct link_map
  {
    /* These first few members are part of the protocol with the debugger.
       This is the same format used in SVR4.  */

    Elf_Addr l_addr;		/* Base address shared object is loaded at.  */
    Elf_Addr l_offs;		/* Offset */
    char *l_name;		/* Absolute file name object was found in.  */
    Elf_Dyn *l_ld;		/* Dynamic section of the shared object.  */
    struct link_map *l_next, *l_prev; /* Chain of loaded objects.  */

    /* All following members are internal to the dynamic linker.
       They may change without notice.  */

    const char *l_libname;	/* Name requested (before search).  */

    /* Indexed pointers to dynamic section.  */
    Elf_Dyn *l_info[DT_NUM + DT_PROCNUM];

    const Elf_Phdr *l_phdr;	/* Pointer to program header table in core.  */
    Elf_Word l_phnum;		/* Number of program header entries.  */
    Elf_Addr l_entry;		/* Entry point location.  */

    /* Symbol hash table.  */
    Elf_Word l_nbuckets;
    const Elf_Word *l_buckets, *l_chain;

    unsigned int l_opencount;	/* Reference count for dlopen/dlclose.  */
    enum			/* Where this object came from.  */
      {
	lt_executable,		/* The main executable program.  */
	lt_interpreter,		/* The interpreter: the dynamic linker.  */
	lt_library,		/* Library needed by main executable.  */
	lt_loaded		/* Extra run-time loaded shared object.  */
      } l_type:2;
    unsigned int l_deps_loaded:1; /* Nonzero if DT_NEEDED items loaded.  */
    unsigned int l_relocated:1;	/* Nonzero if object's relocations done.  */
    unsigned int l_init_called:1; /* Nonzero if DT_INIT function called.  */
    unsigned int l_init_running:1; /* Nonzero while DT_INIT function runs.  */
  };

#endif /* !_LINK_ELF_H */
