/* Native-dependent code for FreeBSD.

   Copyright 2002, 2003, 2004 Free Software Foundation, Inc.

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
#include "gdbcore.h"
#include "inferior.h"
#include "regcache.h"
#include "regset.h"

#include "gdb_assert.h"
#include "gdb_string.h"
#include <sys/procfs.h>
#include <sys/types.h>

#include "elf-bfd.h"
#include "fbsd-nat.h"

/* Return a the name of file that can be opened to get the symbols for
   the child process identified by PID.  */

char *
fbsd_pid_to_exec_file (int pid)
{
  char *path;
  char *buf;

  path = xstrprintf ("/proc/%d/file", pid);
  buf = xcalloc (MAXPATHLEN, sizeof (char));
  make_cleanup (xfree, path);
  make_cleanup (xfree, buf);

  if (readlink (path, buf, MAXPATHLEN) > 0)
    return buf;

  return NULL;
}

static int
fbsd_read_mapping (FILE *mapfile, unsigned long *start, unsigned long *end,
		   char *protection)
{
  /* FreeBSD 5.1-RELEASE uses a 256-byte buffer.  */
  char buf[256];
  int resident, privateresident;
  unsigned long obj;
  int ret = EOF;

  /* As of FreeBSD 5.0-RELEASE, the layout is described in
     /usr/src/sys/fs/procfs/procfs_map.c.  Somewhere in 5.1-CURRENT a
     new column was added to the procfs map.  Therefore we can't use
     fscanf since we need to support older releases too.  */
  if (fgets (buf, sizeof buf, mapfile) != NULL)
    ret = sscanf (buf, "%lx %lx %d %d %lx %s", start, end,
		  &resident, &privateresident, &obj, protection);

  return (ret != 0 && ret != EOF);
}

/* Iterate over all the memory regions in the current inferior,
   calling FUNC for each memory region.  OBFD is passed as the last
   argument to FUNC.  */

int
fbsd_find_memory_regions (int (*func) (CORE_ADDR, unsigned long,
				       int, int, int, void *),
			  void *obfd)
{
  pid_t pid = ptid_get_pid (inferior_ptid);
  char *mapfilename;
  FILE *mapfile;
  unsigned long start, end, size;
  char protection[4];
  int read, write, exec;

  mapfilename = xstrprintf ("/proc/%ld/map", (long) pid);
  mapfile = fopen (mapfilename, "r");
  if (mapfile == NULL)
    error ("Couldn't open %s\n", mapfilename);

  if (info_verbose)
    fprintf_filtered (gdb_stdout, 
		      "Reading memory regions from %s\n", mapfilename);

  /* Now iterate until end-of-file.  */
  while (fbsd_read_mapping (mapfile, &start, &end, &protection[0]))
    {
      size = end - start;

      read = (strchr (protection, 'r') != 0);
      write = (strchr (protection, 'w') != 0);
      exec = (strchr (protection, 'x') != 0);

      if (info_verbose)
	{
	  fprintf_filtered (gdb_stdout, 
			    "Save segment, %ld bytes at 0x%s (%c%c%c)\n", 
			    size, paddr_nz (start),
			    read ? 'r' : '-',
			    write ? 'w' : '-',
			    exec ? 'x' : '-');
	}

      /* Invoke the callback function to create the corefile segment. */
      func (start, size, read, write, exec, obfd);
    }

  fclose (mapfile);
  return 0;
}

/* Create appropriate note sections for a corefile, returning them in
   allocated memory.  */

char *
fbsd_make_corefile_notes (bfd *obfd, int *note_size)
{
  struct gdbarch *gdbarch = current_gdbarch;
  const struct regcache *regcache = current_regcache;
  gregset_t gregs;
  fpregset_t fpregs;
  char *note_data = NULL;
  Elf_Internal_Ehdr *i_ehdrp;
  const struct regset *regset;
  size_t size;

  /* Put a "FreeBSD" label in the ELF header.  */
  i_ehdrp = elf_elfheader (obfd);
  i_ehdrp->e_ident[EI_OSABI] = ELFOSABI_FREEBSD;

  gdb_assert (gdbarch_regset_from_core_section_p (gdbarch));

  size = sizeof gregs;
  regset = gdbarch_regset_from_core_section (gdbarch, ".reg", size);
  gdb_assert (regset && regset->collect_regset);
  regset->collect_regset (regset, regcache, -1, &gregs, size);

  note_data = elfcore_write_prstatus (obfd, note_data, note_size,
				      ptid_get_pid (inferior_ptid),
				      stop_signal, &gregs);

  size = sizeof fpregs;
  regset = gdbarch_regset_from_core_section (gdbarch, ".reg2", size);
  gdb_assert (regset && regset->collect_regset);
  regset->collect_regset (regset, regcache, -1, &fpregs, size);

  note_data = elfcore_write_prfpreg (obfd, note_data, note_size,
				     &fpregs, sizeof (fpregs));

  if (get_exec_file (0))
    {
      char *fname = strrchr (get_exec_file (0), '/') + 1;
      char *psargs = xstrdup (fname);

      if (get_inferior_args ())
	psargs = reconcat (psargs, psargs, " ", get_inferior_args (), NULL);

      note_data = elfcore_write_prpsinfo (obfd, note_data, note_size,
					  fname, psargs);
    }

  make_cleanup (xfree, note_data);
  return note_data;
}
