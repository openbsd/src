/* Run-time dynamic linker data structures for loaded ELF shared objects.
Copyright (C) 1995 Free Software Foundation, Inc.
This file is part of the GNU C Library.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

#ifndef	_LINK_H
#define	_LINK_H	1

#include <elf.h>
#include <elf-machine.h>


/* Rendezvous structure used by the run-time dynamic linker to communicate
   details of shared object loading to the debugger.  If the executable's
   dynamic section has a DT_MIPS_RLD_MAP element, the run-time linker sets
   that element's value to the address where this structure can be found.  */

struct r_debug
  {
    int r_version;		/* Version number for this protocol.  */

    struct link_map *r_map;	/* Head of the chain of loaded objects.  */

    /* This is the address of a function internal to the run-time linker,
       that will always be called when the linker begins to map in a
       library or unmap it, and again when the mapping change is complete.
       The debugger can set a breakpoint at this address if it wants to
       notice shared object mapping changes.  */
    Elf32_Addr r_brk;
    enum
      {
	/* This state value describes the mapping change taking place when
	   the `r_brk' address is called.  */
	RT_CONSISTENT,		/* Mapping change is complete.  */
	RT_ADD,			/* Beginning to add a new object.  */
	RT_DELETE,		/* Beginning to remove an object mapping.  */
      } r_state;

    Elf32_Addr r_ldbase;	/* Base address the linker is loaded at.  */
  };

/* This symbol refers to the "dynamic structure" in the `.dynamic' section
   of whatever module refers to `_DYNAMIC'.  So, to find its own
   `struct r_debug', a program could do:
     for (dyn = _DYNAMIC; dyn->d_tag != DT_NULL)
       if (dyn->d_tag == DT_MIPS_RLD_MAP) r_debug = (struct r_debug) dyn->d_un.d_ptr;
   */

extern Elf32_Dyn _DYNAMIC[];


/* Structure describing a loaded shared object.  The `l_next' and `l_prev'
   members form a chain of all the shared objects loaded at startup.

   These data structures exist in space used by the run-time dynamic linker;
   modifying them may have disastrous results.  */

struct link_map
  {
    /* These first few members are part of the protocol with the debugger.
       This is the same format used in SVR4.  */

    Elf32_Addr l_addr;		/* Base address shared object is loaded at.  */
    Elf32_Addr l_offs;		/* Offset to something XXX Added for gdb */
    char *l_name;		/* Absolute file name object was found in.  */
    Elf32_Dyn *l_ld;		/* Dynamic section of the shared object.  */
    struct link_map *l_next, *l_prev; /* Chain of loaded objects.  */

    /* All following members are internal to the dynamic linker.
       They may change without notice.  */

    const char *l_libname;	/* Name requested (before search).  */

    /* Indexed pointers to dynamic section.  */
    Elf32_Dyn *l_info[DT_NUM + DT_PROCNUM];

    const Elf32_Phdr *l_phdr;	/* Pointer to program header table in core.  */
    Elf32_Word l_phnum;		/* Number of program header entries.  */
    Elf32_Addr l_entry;		/* Entry point location.  */

    /* Symbol hash table.  */
    Elf32_Word l_nbuckets;
    const Elf32_Word *l_buckets, *l_chain;

    unsigned int l_opencount;	/* Reference count for dlopen/dlclose.  */
    enum			/* Where this object came from.  */
      {
	lt_executable,		/* The main executable program.  */
	lt_interpreter,		/* The interpreter: the dynamic linker.  */
	lt_library,		/* Library needed by main executable.  */
	lt_loaded,		/* Extra run-time loaded shared object.  */
      } l_type:2;
    unsigned int l_deps_loaded:1; /* Nonzero if DT_NEEDED items loaded.  */
    unsigned int l_relocated:1;	/* Nonzero if object's relocations done.  */
    unsigned int l_init_called:1; /* Nonzero if DT_INIT function called.  */
    unsigned int l_init_running:1; /* Nonzero while DT_INIT function runs.  */
  };

/* Internal functions of the run-time dynamic linker.
   These can be accessed if you link again the dynamic linker
   as a shared library, as in `-lld' or `/lib/ld.so' explicitly;
   but are not normally of interest to user programs.

   The `-ldl' library functions in <dlfcn.h> provide a simple
   user interface to run-time dynamic linking.  */


/* File descriptor referring to the zero-fill device.  */
extern int _dl_zerofd;

/* OS-dependent function to open the zero-fill device.  */
extern int _dl_sysdep_open_zero_fill (void); /* dl-sysdep.c */

/* OS-dependent function to write a message on the standard output.
   All arguments are `const char *'; args until a null pointer
   are concatenated to form the message to print.  */
extern void _dl_sysdep_message (const char *string, ...);

/* OS-dependent function to give a fatal error message and exit
   when the dynamic linker fails before the program is fully linked.
   All arguments are `const char *'; args until a null pointer
   are concatenated to form the message to print.  */
extern void _dl_sysdep_fatal (const char *string, ...)
     __attribute__ ((__noreturn__));

/* Nonzero if the program should be "secure" (i.e. it's setuid or somesuch).
   This tells the dynamic linker to ignore environment variables.  */
extern int _dl_secure;

/* This function is called by all the internal dynamic linker functions
   when they encounter an error.  ERRCODE is either an `errno' code or
   zero; OBJECT is the name of the problematical shared object, or null if
   it is a general problem; ERRSTRING is a string describing the specific
   problem.  */
   
extern void _dl_signal_error (int errcode,
			      const char *object,
			      const char *errstring)
     __attribute__ ((__noreturn__));

/* Call OPERATE, catching errors from `dl_signal_error'.  If there is no
   error, *ERRSTRING is set to null.  If there is an error, *ERRSTRING and
   *OBJECT are set to the strings passed to _dl_signal_error, and the error
   code passed is the return value.  */
extern int _dl_catch_error (const char **errstring,
			    const char **object,
			    void (*operate) (void));


/* Helper function for <dlfcn.h> functions.  Runs the OPERATE function via
   _dl_catch_error.  Returns zero for success, nonzero for failure; and
   arranges for `dlerror' to return the error details.  */
extern int _dlerror_run (void (*operate) (void));


/* Open the shared object NAME and map in its segments.
   LOADER's DT_RPATH is used in searching for NAME.
   If the object is already opened, returns its existing map.  */
extern struct link_map *_dl_map_object (struct link_map *loader,
					const char *name);

/* Similar, but file found at REALNAME and opened on FD.
   REALNAME must malloc'd storage and is used in internal data structures.  */
extern struct link_map *_dl_map_object_from_fd (const char *name,
						int fd, char *realname);

/* Cache the locations of MAP's hash table.  */
extern void _dl_setup_hash (struct link_map *map);


/* Search loaded objects' symbol tables for a definition of the symbol
   referred to by UNDEF.  *SYM is the symbol table entry containing the
   reference; it is replaced with the defining symbol, and the base load
   address of the defining object is returned.  SYMBOL_SCOPE is the head of
   the chain used for searching.  REFERENCE_NAME should name the object
   containing the reference; it is used in error messages.  If NOSELF is
   nonzero, them *SYM itself cannot define the value; another binding must
   be found.  */
extern Elf32_Addr _dl_lookup_symbol (const char *undef_name,
				     const Elf32_Sym **ref,
				     struct elf_resolve *symbol_scope,
				     const char *reference_name,
				     int noself);


/* List of objects currently loaded.  */
extern struct link_map *_dl_loaded;

/* Tail of that list which were loaded at startup.  */
extern struct link_map *_dl_startup_loaded;

/* Allocate a `struct link_map' for a new object being loaded,
   and enter it into the _dl_loaded list.  */
extern struct link_map *_dl_new_object (char *realname, const char *libname,
					int type);

/* Relocate the given object (if it hasn't already been).
   If LAZY is nonzero, don't relocate its PLT.  */
extern void _dl_relocate_object (struct link_map *map, int lazy);

/* Return the address of the next initializer function not yet run.
   When there are no more initializers to be run, this returns zero.
   The functions are returned in the order they should be called.  */
extern Elf32_Addr _dl_init_next (void);

/* Call the finalizer functions of all shared objects whose
   initializer functions have completed.  */
extern void _dl_fini (void);

/* The dynamic linker calls this function before and having changing
   any shared object mappings.  The `r_state' member of `struct r_debug'
   says what change is taking place.  This function's address is
   the value of the `r_brk' member.  */
extern void _dl_r_debug_state (void);

extern void * _dl_malloc(int size);
extern int _dl_map_cache(void);
extern int _dl_unmap_cache(void);

extern struct elf_resolve * _dl_load_elf_shared_library(char * libname, int);

extern int linux_run(int argc, char * argv[]);

extern void _dl_parse_lazy_relocation_information(struct elf_resolve * tpnt,
	int rel_addr, int rel_size, int type);
  
extern int _dl_parse_relocation_information(struct elf_resolve * tpnt,
	int rel_addr, int rel_size, int type);
extern int _dl_parse_copy_information(struct dyn_elf * rpnt, int rel_addr,
       int rel_size, int type);



#endif /* link.h */
