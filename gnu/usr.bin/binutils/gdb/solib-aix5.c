/* Handle AIX5 shared libraries for GDB, the GNU Debugger.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1998, 1999, 2000,
   2001
   Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"

#include <sys/types.h>
#include <signal.h>
#include "gdb_string.h"
#include <sys/param.h>
#include <fcntl.h>
#include <sys/procfs.h>

#include "elf/external.h"

#include "symtab.h"
#include "bfd.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdbcore.h"
#include "command.h"
#include "target.h"
#include "frame.h"
#include "gdb_regex.h"
#include "inferior.h"
#include "environ.h"
#include "language.h"
#include "gdbcmd.h"

#include "solist.h"

/* Link map info to include in an allocated so_list entry */

struct lm_info
  {
    int nmappings;		/* number of mappings */
    struct lm_mapping
      {
	CORE_ADDR addr;		/* base address */
	CORE_ADDR size;		/* size of mapped object */
	CORE_ADDR offset;	/* offset into mapped object */
	long flags;		/* MA_ protection and attribute flags */
	CORE_ADDR gp;		/* global pointer value */
      } *mapping;
      char *mapname;		/* name in /proc/pid/object */
      char *pathname;		/* full pathname to object */
      char *membername;		/* member name in archive file */
  };

/* List of symbols in the dynamic linker where GDB can try to place
   a breakpoint to monitor shared library events. */

static char *solib_break_names[] =
{
  "_r_debug_state",
  NULL
};

static void aix5_relocate_main_executable (void);

/*

   LOCAL FUNCTION

   bfd_lookup_symbol -- lookup the value for a specific symbol

   SYNOPSIS

   CORE_ADDR bfd_lookup_symbol (bfd *abfd, char *symname)

   DESCRIPTION

   An expensive way to lookup the value of a single symbol for
   bfd's that are only temporary anyway.  This is used by the
   shared library support to find the address of the debugger
   interface structures in the shared library.

   Note that 0 is specifically allowed as an error return (no
   such symbol).
 */

static CORE_ADDR
bfd_lookup_symbol (bfd *abfd, char *symname)
{
  long storage_needed;
  asymbol *sym;
  asymbol **symbol_table;
  unsigned int number_of_symbols;
  unsigned int i;
  struct cleanup *back_to;
  CORE_ADDR symaddr = 0;

  storage_needed = bfd_get_symtab_upper_bound (abfd);

  if (storage_needed > 0)
    {
      symbol_table = (asymbol **) xmalloc (storage_needed);
      back_to = make_cleanup (xfree, symbol_table);
      number_of_symbols = bfd_canonicalize_symtab (abfd, symbol_table);

      for (i = 0; i < number_of_symbols; i++)
	{
	  sym = *symbol_table++;
	  if (strcmp (sym->name, symname) == 0)
	    {
	      /* Bfd symbols are section relative. */
	      symaddr = sym->value + sym->section->vma;
	      break;
	    }
	}
      do_cleanups (back_to);
    }

  if (symaddr)
    return symaddr;

  /* Look for the symbol in the dynamic string table too.  */

  storage_needed = bfd_get_dynamic_symtab_upper_bound (abfd);

  if (storage_needed > 0)
    {
      symbol_table = (asymbol **) xmalloc (storage_needed);
      back_to = make_cleanup (xfree, symbol_table);
      number_of_symbols = bfd_canonicalize_dynamic_symtab (abfd, symbol_table);

      for (i = 0; i < number_of_symbols; i++)
	{
	  sym = *symbol_table++;
	  if (strcmp (sym->name, symname) == 0)
	    {
	      /* Bfd symbols are section relative. */
	      symaddr = sym->value + sym->section->vma;
	      break;
	    }
	}
      do_cleanups (back_to);
    }

  return symaddr;
}


/* Read /proc/PID/map and build a list of shared objects such that
   the pr_mflags value AND'd with MATCH_MASK is equal to MATCH_VAL.
   This gives us a convenient way to find all of the mappings that
   don't belong to the main executable or vice versa.  Here are
   some of the possibilities:

    - Fetch all mappings:
        MATCH_MASK: 0
        MATCH_VAL: 0
    - Fetch all mappings except for main executable:
        MATCH_MASK: MA_MAINEXEC
	MATCH_VAL: 0
    - Fetch only main executable:
        MATCH_MASK: MA_MAINEXEC
	MATCH_VAL: MA_MAINEXEC
	
   A cleanup chain for the list allocations done by this function should
   be established prior to calling build_so_list_from_mapfile().  */

static struct so_list *
build_so_list_from_mapfile (int pid, long match_mask, long match_val)
{
  char *mapbuf = NULL;
  struct prmap *prmap;
  int mapbuf_size;
  struct so_list *sos = NULL;

  {
    int mapbuf_allocation_size = 8192;
    char *map_pathname;
    int map_fd;

    /* Open the map file */

    map_pathname = xstrprintf ("/proc/%d/map", pid);
    map_fd = open (map_pathname, O_RDONLY);
    xfree (map_pathname);
    if (map_fd < 0)
      return 0;

    /* Read the entire map file in */
    do
      {
	if (mapbuf)
	  {
	    xfree (mapbuf);
	    mapbuf_allocation_size *= 2;
	    lseek (map_fd, 0, SEEK_SET);
	  }
	mapbuf = xmalloc (mapbuf_allocation_size);
	mapbuf_size = read (map_fd, mapbuf, mapbuf_allocation_size);
	if (mapbuf_size < 0)
	  {
	    xfree (mapbuf);
	    /* FIXME: This warrants an error or a warning of some sort */
	    return 0;
	  }
      } while (mapbuf_size == mapbuf_allocation_size);

    close (map_fd);
  }

  for (prmap = (struct prmap *) mapbuf;
       (char *) prmap < mapbuf + mapbuf_size;
       prmap++)
    {
      char *mapname, *pathname, *membername;
      struct so_list *sop;
      int mapidx;

      if (prmap->pr_size == 0)
	break;

      /* Skip to the next entry if there's no path associated with the
         map, unless we're looking for the kernel text region, in which
	 case it's okay if there's no path.  */
      if ((prmap->pr_pathoff == 0 || prmap->pr_pathoff >= mapbuf_size)
	  && ((match_mask & MA_KERNTEXT) == 0))
	continue;

      /* Skip to the next entry if our match conditions don't hold.  */
      if ((prmap->pr_mflags & match_mask) != match_val)
	continue;

      mapname = prmap->pr_mapname;
      if (prmap->pr_pathoff == 0)
        {
	  pathname = "";
	  membername = "";
	}
      else
        {
	  pathname = mapbuf + prmap->pr_pathoff;
	  membername = pathname + strlen (pathname) + 1;
        }

      for (sop = sos; sop != NULL; sop = sop->next)
	if (strcmp (pathname, sop->lm_info->pathname) == 0
	    && strcmp (membername, sop->lm_info->membername) == 0)
	  break;

      if (sop == NULL)
	{
	  sop = xcalloc (1, sizeof (struct so_list));
	  make_cleanup (xfree, sop);
	  sop->lm_info = xcalloc (1, sizeof (struct lm_info));
	  make_cleanup (xfree, sop->lm_info);
	  sop->lm_info->mapname = xstrdup (mapname);
	  make_cleanup (xfree, sop->lm_info->mapname);
	  /* FIXME: Eliminate the pathname field once length restriction
	     is lifted on so_name and so_original_name.  */
	  sop->lm_info->pathname = xstrdup (pathname);
	  make_cleanup (xfree, sop->lm_info->pathname);
	  sop->lm_info->membername = xstrdup (membername);
	  make_cleanup (xfree, sop->lm_info->membername);

	  strncpy (sop->so_name, pathname, SO_NAME_MAX_PATH_SIZE - 1);
	  sop->so_name[SO_NAME_MAX_PATH_SIZE - 1] = '\0';
	  strcpy (sop->so_original_name, sop->so_name);

	  sop->next = sos;
	  sos = sop;
	}

      mapidx = sop->lm_info->nmappings;
      sop->lm_info->nmappings += 1;
      sop->lm_info->mapping 
	= xrealloc (sop->lm_info->mapping,
	            sop->lm_info->nmappings * sizeof (struct lm_mapping));
      sop->lm_info->mapping[mapidx].addr = (CORE_ADDR) prmap->pr_vaddr;
      sop->lm_info->mapping[mapidx].size = prmap->pr_size;
      sop->lm_info->mapping[mapidx].offset = prmap->pr_off;
      sop->lm_info->mapping[mapidx].flags = prmap->pr_mflags;
      sop->lm_info->mapping[mapidx].gp = (CORE_ADDR) prmap->pr_gp;
    }

  xfree (mapbuf);
  return sos;
}

/*

  LOCAL FUNCTION

  open_symbol_file_object

  SYNOPSIS

  void open_symbol_file_object (void *from_tty)

  DESCRIPTION

  If no open symbol file, attempt to locate and open the main symbol
  file.

  If FROM_TTYP dereferences to a non-zero integer, allow messages to
  be printed.  This parameter is a pointer rather than an int because
  open_symbol_file_object() is called via catch_errors() and
  catch_errors() requires a pointer argument. */

static int
open_symbol_file_object (void *from_ttyp)
{
  CORE_ADDR lm, l_name;
  char *filename;
  int errcode;
  int from_tty = *(int *)from_ttyp;
  struct cleanup *old_chain = make_cleanup (null_cleanup, 0);
  struct so_list *sos;

  sos = build_so_list_from_mapfile (PIDGET (inferior_ptid),
                                    MA_MAINEXEC, MA_MAINEXEC);


  if (sos == NULL)
    {
      warning ("Could not find name of main executable in map file");
      return 0;
    }

  symbol_file_command (sos->lm_info->pathname, from_tty);

  do_cleanups (old_chain);

  aix5_relocate_main_executable ();

  return 1;
}

/* LOCAL FUNCTION

   aix5_current_sos -- build a list of currently loaded shared objects

   SYNOPSIS

   struct so_list *aix5_current_sos ()

   DESCRIPTION

   Build a list of `struct so_list' objects describing the shared
   objects currently loaded in the inferior.  This list does not
   include an entry for the main executable file.

   Note that we only gather information directly available from the
   inferior --- we don't examine any of the shared library files
   themselves.  The declaration of `struct so_list' says which fields
   we provide values for.  */

static struct so_list *
aix5_current_sos (void)
{
  struct cleanup *old_chain = make_cleanup (null_cleanup, 0);
  struct so_list *sos;

  /* Fetch the list of mappings, excluding the main executable. */
  sos = build_so_list_from_mapfile (PIDGET (inferior_ptid), MA_MAINEXEC, 0);

  /* Reverse the list; it looks nicer when we print it if the mappings
     are in the same order as in the map file.  */
  if (sos)
    {
      struct so_list *next = sos->next;

      sos->next = 0;
      while (next)
	{
	  struct so_list *prev = sos;

	  sos = next;
	  next = next->next;
	  sos->next = prev;
	}
    }
  discard_cleanups (old_chain);
  return sos;
}


/* Return 1 if PC lies in the dynamic symbol resolution code of the
   run time loader.  */

static CORE_ADDR interp_text_sect_low;
static CORE_ADDR interp_text_sect_high;
static CORE_ADDR interp_plt_sect_low;
static CORE_ADDR interp_plt_sect_high;

static int
aix5_in_dynsym_resolve_code (CORE_ADDR pc)
{
  return ((pc >= interp_text_sect_low && pc < interp_text_sect_high)
	  || (pc >= interp_plt_sect_low && pc < interp_plt_sect_high)
	  || in_plt_section (pc, NULL));
}

/*

   LOCAL FUNCTION

   enable_break -- arrange for dynamic linker to hit breakpoint

   SYNOPSIS

   int enable_break (void)

   DESCRIPTION

   The dynamic linkers has, as part of its debugger interface, support
   for arranging for the inferior to hit a breakpoint after mapping in
   the shared libraries.  This function enables that breakpoint.

 */

static int
enable_break (void)
{
  int success = 0;

  struct minimal_symbol *msymbol;
  char **bkpt_namep;
  asection *interp_sect;

  /* First, remove all the solib event breakpoints.  Their addresses
     may have changed since the last time we ran the program.  */
  remove_solib_event_breakpoints ();

  interp_text_sect_low = interp_text_sect_high = 0;
  interp_plt_sect_low = interp_plt_sect_high = 0;

  /* Find the .interp section; if not found, warn the user and drop
     into the old breakpoint at symbol code.  */
  interp_sect = bfd_get_section_by_name (exec_bfd, ".interp");
  if (interp_sect)
    {
      unsigned int interp_sect_size;
      char *buf;
      CORE_ADDR load_addr;
      bfd *tmp_bfd;
      CORE_ADDR sym_addr = 0;

      /* Read the contents of the .interp section into a local buffer;
         the contents specify the dynamic linker this program uses.  */
      interp_sect_size = bfd_section_size (exec_bfd, interp_sect);
      buf = alloca (interp_sect_size);
      bfd_get_section_contents (exec_bfd, interp_sect,
				buf, 0, interp_sect_size);

      /* Now we need to figure out where the dynamic linker was
         loaded so that we can load its symbols and place a breakpoint
         in the dynamic linker itself.

         This address is stored on the stack.  However, I've been unable
         to find any magic formula to find it for Solaris (appears to
         be trivial on GNU/Linux).  Therefore, we have to try an alternate
         mechanism to find the dynamic linker's base address.  */
      tmp_bfd = bfd_openr (buf, gnutarget);
      if (tmp_bfd == NULL)
	goto bkpt_at_symbol;

      /* Make sure the dynamic linker's really a useful object.  */
      if (!bfd_check_format (tmp_bfd, bfd_object))
	{
	  warning ("Unable to grok dynamic linker %s as an object file", buf);
	  bfd_close (tmp_bfd);
	  goto bkpt_at_symbol;
	}

      /* We find the dynamic linker's base address by examining the
         current pc (which point at the entry point for the dynamic
         linker) and subtracting the offset of the entry point.  */
      load_addr = read_pc () - tmp_bfd->start_address;

      /* Record the relocated start and end address of the dynamic linker
         text and plt section for aix5_in_dynsym_resolve_code.  */
      interp_sect = bfd_get_section_by_name (tmp_bfd, ".text");
      if (interp_sect)
	{
	  interp_text_sect_low =
	    bfd_section_vma (tmp_bfd, interp_sect) + load_addr;
	  interp_text_sect_high =
	    interp_text_sect_low + bfd_section_size (tmp_bfd, interp_sect);
	}
      interp_sect = bfd_get_section_by_name (tmp_bfd, ".plt");
      if (interp_sect)
	{
	  interp_plt_sect_low =
	    bfd_section_vma (tmp_bfd, interp_sect) + load_addr;
	  interp_plt_sect_high =
	    interp_plt_sect_low + bfd_section_size (tmp_bfd, interp_sect);
	}

      /* Now try to set a breakpoint in the dynamic linker.  */
      for (bkpt_namep = solib_break_names; *bkpt_namep != NULL; bkpt_namep++)
	{
	  sym_addr = bfd_lookup_symbol (tmp_bfd, *bkpt_namep);
	  if (sym_addr != 0)
	    break;
	}

      /* We're done with the temporary bfd.  */
      bfd_close (tmp_bfd);

      if (sym_addr != 0)
	{
	  create_solib_event_breakpoint (load_addr + sym_addr);
	  return 1;
	}

      /* For whatever reason we couldn't set a breakpoint in the dynamic
         linker.  Warn and drop into the old code.  */
    bkpt_at_symbol:
      warning ("Unable to find dynamic linker breakpoint function.\nGDB will be unable to debug shared library initializers\nand track explicitly loaded dynamic code.");
    }

  /* Nothing good happened.  */
  success = 0;

  return (success);
}

/*

   LOCAL FUNCTION

   special_symbol_handling -- additional shared library symbol handling

   SYNOPSIS

   void special_symbol_handling ()

   DESCRIPTION

   Once the symbols from a shared object have been loaded in the usual
   way, we are called to do any system specific symbol handling that 
   is needed.

 */

static void
aix5_special_symbol_handling (void)
{
  /* Nothing needed (yet) for AIX5. */
}

/* On AIX5, the /proc/PID/map information is used to determine
   the relocation offsets needed for relocating the main executable.
   There is no problem determining which map entries correspond
   to the main executable, because these will have the MA_MAINEXEC
   flag set.  The tricky part is determining which sections correspond
   to which map entries.  To date, the following approaches have
   been tried:

    - Use the MA_WRITE attribute of pr_mflags to distinguish the read-only
      mapping from the read/write mapping.  (This assumes that there are
      only two mappings for the main executable.)  All writable sections
      are associated with the read/write mapping and all non-writable
      sections are associated with the read-only mapping.

      This approach worked quite well until we came across executables
      which didn't have a read-only mapping.  Both mappings had the
      same attributes represented in pr_mflags and it was impossible
      to tell them apart.

    - Use the pr_off field (which represents the offset into the
      executable) to determine the section-to-mapping relationship.
      Unfortunately, this approach doesn't work either, because the
      offset value contained in the mapping is rounded down by some
      moderately large power-of-2 value (4096 is a typical value).
      A small (e.g. "Hello World") program will appear to have all
      of its sections belonging to both mappings.

   Also, the following approach has been considered, but dismissed:

    - The section vma values typically look (something) like
      0x00000001xxxxxxxx or 0x00000002xxxxxxxx.  Furthermore, the
      0x00000001xxxxxxxx values always belong to one mapping and
      the 0x00000002xxxxxxxx values always belong to the other.
      Thus it seems conceivable that GDB could use the bit patterns
      in the upper portion (for some definition of "upper") in a
      section's vma to help determine the section-to-mapping
      relationship.

      This approach was dismissed because there is nothing to prevent
      the linker from lumping the section vmas together in one large
      contiguous space and still expecting the dynamic linker to
      separate them and relocate them independently.  Also, different
      linkers have been observed to use different patterns for the
      upper portions of the vma addresses and it isn't clear what the
      mask ought to be for distinguishing these patterns.

   The current (admittedly inelegant) approach uses a lookup 
   table which associates section names with the map index that
   they're permitted to be in.  This is inelegant because we are
   making the following assumptions:

    1) There will only be two mappings.
    2) The relevant (i.e. main executable) mappings will always appear
       in the same order in the map file.
    3) The sections named in the table will always belong to the
       indicated mapping.
    4) The table completely enumerates all possible section names.

   IMO, any of these deficiencies alone will normally be sufficient
   to disqualify this approach, but I haven't been able to think of
   a better way to do it.
   
   map_index_vs_section_name_okay() is a predicate which returns
   true iff the section name NAME is associated with the map index
   IDX in its builtin table.  Of course, there's no guarantee that
   this association is actually valid...  */

static int
map_index_vs_section_name_okay (int idx, const char *name)
{
  static struct
    {
      char *name;
      int idx;
    } okay[] =
    {
      { ".interp", 0 },
      { ".hash", 0 },
      { ".dynsym", 0 },
      { ".dynstr", 0 },
      { ".rela.text", 0 },
      { ".rela.rodata", 0 },
      { ".rela.data", 0 },
      { ".rela.ctors", 0 },
      { ".rela.dtors", 0 },
      { ".rela.got", 0 },
      { ".rela.sdata", 0 },
      { ".rela.IA_64.pltoff", 0 },
      { ".rel.data", 0 },
      { ".rel.sdata", 0 },
      { ".rel.got", 0 },
      { ".rel.AIX.pfdesc", 0 },
      { ".rel.IA_64.pltoff", 0 },
      { ".dynamic", 0 },
      { ".init", 0 },
      { ".plt", 0 },
      { ".text", 0 },
      { ".fini", 0 },
      { ".rodata", 0 },
      { ".IA_64.unwind_info", 0 },
      { ".IA_64.unwind", 0 },
      { ".AIX.mustrel", 0 },

      { ".data", 1 },
      { ".ctors", 1 },
      { ".dtors", 1 },
      { ".got", 1 },
      { ".dynamic", 1},
      { ".sdata", 1 },
      { ".IA_64.pltoff", 1 },
      { ".sbss", 1 },
      { ".bss", 1 },
      { ".AIX.pfdesc", 1 }
    };
  int i;

  for (i = 0; i < sizeof (okay) / sizeof (okay[0]); i++)
    {
      if (strcmp (name, okay[i].name) == 0)
	return idx == okay[i].idx;
    }

  warning ("solib-aix5.c: Ignoring section %s when relocating the executable\n",
           name);
  return 0;
}

#define SECTMAPMASK (~ (CORE_ADDR) 0x03ffffff)

static void
aix5_relocate_main_executable (void)
{
  struct so_list *so;
  struct section_offsets *new_offsets;
  int i;
  int changed = 0;
  struct cleanup *old_chain = make_cleanup (null_cleanup, 0);

  /* Fetch the mappings for the main executable from the map file.  */
  so = build_so_list_from_mapfile (PIDGET (inferior_ptid),
                                   MA_MAINEXEC, MA_MAINEXEC);

  /* Make sure we actually have some mappings to work with.  */
  if (so == NULL)
    {
      warning ("Could not find main executable in map file");
      do_cleanups (old_chain);
      return;
    }

  /* Allocate the data structure which'll contain the new offsets to
     relocate by.  Initialize it so it contains the current offsets.  */
  new_offsets = xcalloc (symfile_objfile->num_sections,
			 sizeof (struct section_offsets));
  make_cleanup (xfree, new_offsets);
  for (i = 0; i < symfile_objfile->num_sections; i++)
    new_offsets->offsets[i] = ANOFFSET (symfile_objfile->section_offsets, i);

  /* Iterate over the mappings in the main executable and compute
     the new offset value as appropriate.  */
  for (i = 0; i < so->lm_info->nmappings; i++)
    {
      CORE_ADDR increment = 0;
      struct obj_section *sect;
      bfd *obfd = symfile_objfile->obfd;
      struct lm_mapping *mapping = &so->lm_info->mapping[i];

      ALL_OBJFILE_OSECTIONS (symfile_objfile, sect)
	{
	  int flags = bfd_get_section_flags (obfd, sect->the_bfd_section);
	  if (flags & SEC_ALLOC)
	    {
	      file_ptr filepos = sect->the_bfd_section->filepos;
	      if (map_index_vs_section_name_okay (i,
		    bfd_get_section_name (obfd, sect->the_bfd_section)))
		{
		  int idx = sect->the_bfd_section->index;

		  if (increment == 0)
		    increment = mapping->addr
		      - (bfd_section_vma (obfd, sect->the_bfd_section) 
		         & SECTMAPMASK);

		  if (increment != ANOFFSET (new_offsets, idx))
		    {
		      new_offsets->offsets[idx] = increment;
		      changed = 1;
		    }
		}
	    }
	}
    }

  /* If any of the offsets have changed, then relocate the objfile.  */
  if (changed)
    objfile_relocate (symfile_objfile, new_offsets);

  /* Free up all the space we've allocated.  */
  do_cleanups (old_chain);
}

/*

   GLOBAL FUNCTION

   aix5_solib_create_inferior_hook -- shared library startup support

   SYNOPSIS

   void aix5_solib_create_inferior_hook()

   DESCRIPTION

   When gdb starts up the inferior, it nurses it along (through the
   shell) until it is ready to execute it's first instruction.  At this
   point, this function gets called via expansion of the macro
   SOLIB_CREATE_INFERIOR_HOOK.

   For AIX5 executables, this first instruction is the first
   instruction in the dynamic linker (for dynamically linked
   executables) or the instruction at "start" for statically linked
   executables.  For dynamically linked executables, the system
   first exec's libc.so.N, which contains the dynamic linker,
   and starts it running.  The dynamic linker maps in any needed
   shared libraries, maps in the actual user executable, and then
   jumps to "start" in the user executable.

 */

static void
aix5_solib_create_inferior_hook (void)
{
  aix5_relocate_main_executable ();

  if (!enable_break ())
    {
      warning ("shared library handler failed to enable breakpoint");
      return;
    }
}

static void
aix5_clear_solib (void)
{
}

static void
aix5_free_so (struct so_list *so)
{
  xfree (so->lm_info->mapname);
  xfree (so->lm_info->pathname);
  xfree (so->lm_info->membername);
  xfree (so->lm_info);
}

static void
aix5_relocate_section_addresses (struct so_list *so,
                                 struct section_table *sec)
{
  int flags = bfd_get_section_flags (sec->bfd, sec->the_bfd_section);
  file_ptr filepos = sec->the_bfd_section->filepos;

  if (flags & SEC_ALLOC)
    {
      int idx;
      CORE_ADDR addr;

      for (idx = 0; idx < so->lm_info->nmappings; idx++)
	{
	  struct lm_mapping *mapping = &so->lm_info->mapping[idx];
	  if (mapping->offset <= filepos
	      && filepos <= mapping->offset + mapping->size)
	    break;
	}

      if (idx >= so->lm_info->nmappings)
	internal_error (__FILE__, __LINE__,
	  "aix_relocate_section_addresses: Can't find mapping for section %s",
	  bfd_get_section_name (sec->bfd, sec->the_bfd_section));
      
      addr = so->lm_info->mapping[idx].addr;

      sec->addr += addr;
      sec->endaddr += addr;
    }
}

/* Find the global pointer for the given function address ADDR.  */

static CORE_ADDR
aix5_find_global_pointer (CORE_ADDR addr)
{
  struct so_list *sos, *so;
  CORE_ADDR global_pointer = 0;
  struct cleanup *old_chain = make_cleanup (null_cleanup, 0);

  sos = build_so_list_from_mapfile (PIDGET (inferior_ptid), 0, 0);

  for (so = sos; so != NULL; so = so->next)
    {
      int idx;
      for (idx = 0; idx < so->lm_info->nmappings; idx++)
	if (so->lm_info->mapping[idx].addr <= addr
	    && addr <= so->lm_info->mapping[idx].addr
			 + so->lm_info->mapping[idx].size)
	  {
	    break;
	  }

      if (idx < so->lm_info->nmappings)
	{
	  /* Look for a non-zero global pointer in the current set of
	     mappings.  */
	  for (idx = 0; idx < so->lm_info->nmappings; idx++)
	    if (so->lm_info->mapping[idx].gp != 0)
	      {
		global_pointer = so->lm_info->mapping[idx].gp;
		break;
	      }
	  /* Get out regardless of whether we found one or not.  Mappings
	     don't overlap, so it would be pointless to continue.  */
	  break;
	}
    }

  do_cleanups (old_chain);

  return global_pointer;
}

/* Find the execute-only kernel region known as the gate page.  This
   page is where the signal trampoline lives.  It may be found by
   querying the map file and looking for the MA_KERNTEXT flag.  */
static void
aix5_find_gate_addresses (CORE_ADDR *start, CORE_ADDR *end)
{
  struct so_list *so;
  struct cleanup *old_chain = make_cleanup (null_cleanup, 0);

  /* Fetch the mappings for the main executable from the map file.  */
  so = build_so_list_from_mapfile (PIDGET (inferior_ptid),
                                   MA_KERNTEXT, MA_KERNTEXT);

  /* Make sure we actually have some mappings to work with.  */
  if (so == NULL)
    {
      warning ("Could not find gate page in map file");
      *start = 0;
      *end = 0;
      do_cleanups (old_chain);
      return;
    }

  /* There should only be on kernel mapping for the gate page and
     it'll be in the read-only (even though it's execute-only)
     mapping in the lm_info struct.  */

  *start = so->lm_info->mapping[0].addr;
  *end = *start + so->lm_info->mapping[0].size;

  /* Free up all the space we've allocated.  */
  do_cleanups (old_chain);
}

/* From ia64-tdep.c.  FIXME:  If we end up using this for rs6000 too,
   we'll need to make the names match.  */
extern CORE_ADDR (*native_find_global_pointer) (CORE_ADDR);

/* From ia64-aix-tdep.c.  Hook for finding the starting and
   ending gate page addresses.  The only reason that this hook
   is in this file is because this is where the map file reading
   code is located.  */
extern void (*aix5_find_gate_addresses_hook) (CORE_ADDR *, CORE_ADDR *);

static struct target_so_ops aix5_so_ops;

void
_initialize_aix5_solib (void)
{
  aix5_so_ops.relocate_section_addresses = aix5_relocate_section_addresses;
  aix5_so_ops.free_so = aix5_free_so;
  aix5_so_ops.clear_solib = aix5_clear_solib;
  aix5_so_ops.solib_create_inferior_hook = aix5_solib_create_inferior_hook;
  aix5_so_ops.special_symbol_handling = aix5_special_symbol_handling;
  aix5_so_ops.current_sos = aix5_current_sos;
  aix5_so_ops.open_symbol_file_object = open_symbol_file_object;
  aix5_so_ops.in_dynsym_resolve_code = aix5_in_dynsym_resolve_code;

  native_find_global_pointer = aix5_find_global_pointer;
  aix5_find_gate_addresses_hook = aix5_find_gate_addresses;

  /* FIXME: Don't do this here.  *_gdbarch_init() should set so_ops. */
  current_target_so_ops = &aix5_so_ops;
}
