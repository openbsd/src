/* IBM RS/6000 "XCOFF" back-end for BFD.
   Copyright 1990, 91, 92, 93, 94, 95, 96, 97, 98, 2000
   Free Software Foundation, Inc.
   FIXME: Can someone provide a transliteration of this name into ASCII?
   Using the following chars caused a compiler warning on HIUX (so I replaced
   them with octal escapes), and isn't useful without an understanding of what
   character set it is.
   Written by Metin G. Ozisik, Mimi Ph\373\364ng-Th\345o V\365, 
     and John Gilmore.
   Archive support from Damon A. Permezel.
   Contributed by IBM Corporation and Cygnus Support.

This file is part of BFD, the Binary File Descriptor library.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* This port currently only handles reading object files, except when
   compiled on an RS/6000 host.  -- no archive support, no core files.
   In all cases, it does not support writing.

   FIXMEmgo comments are left from Metin Ozisik's original port.

   This is in a separate file from coff-rs6000.c, because it includes
   system include files that conflict with coff/rs6000.h.
  */

/* Internalcoff.h and coffcode.h modify themselves based on this flag.  */
#define RS6000COFF_C 1

/* The AIX 4.1 kernel is obviously compiled with -D_LONG_LONG, so
   we have to define _LONG_LONG for older versions of gcc to get the
   proper alignments in the user structure.  */
#if defined(_AIX41) && !defined(_LONG_LONG)
#define _LONG_LONG
#endif

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

#ifdef AIX_CORE

/* AOUTHDR is defined by the above.  We need another defn of it, from the
   system include files.  Punt the old one and get us a new name for the
   typedef in the system include files.  */
#ifdef AOUTHDR
#undef AOUTHDR
#endif
#define	AOUTHDR	second_AOUTHDR

#undef	SCNHDR


/* ------------------------------------------------------------------------ */
/*	Support for core file stuff.. 					    */
/* ------------------------------------------------------------------------ */

#include <sys/user.h>
#include <sys/ldr.h>
#include <sys/core.h>


/* Number of special purpose registers supported by gdb.  This value
   should match `tm.h' in gdb directory.  Clean this mess up and use
   the macros in sys/reg.h.  FIXMEmgo. */

#define	NUM_OF_SPEC_REGS  7

#define	core_hdr(bfd)		(((Rs6kCorData*)(bfd->tdata.any))->hdr)

/* AIX 4.1 Changed the names and locations of a few items in the core file,
   this seems to be the quickest/easiest way to deal with it. 

   Note however that encoding magic addresses (STACK_END_ADDR) is going
   to be _very_ fragile.  But I don't see any easy way to get that info
   right now.
   
   AIX 4.3 defines an entirely new structure (core_dumpx).  Yet the
   basic logic stays the same and we can still use our macro
   redefinition mechanism to effect the necessary changes.  */

#ifdef AIX_CORE_DUMPX_CORE
#define CORE_DATA_SIZE_FIELD c_dataorg
#define CORE_COMM_FIELD c_u.U_proc.pi_comm
#define SAVE_FIELD c_flt.hctx.r32
#define STACK_END_ADDR coredata.c_stackorg + coredata.c_size
#define LOADER_OFFSET_FIELD c_loader
#define LOADER_REGION_SIZE coredata.c_lsize
#define CORE_DUMP core_dumpx
#else
#ifdef CORE_VERSION_1
#define CORE_DATA_SIZE_FIELD c_u.U_dsize
#define CORE_COMM_FIELD c_u.U_comm
#define SAVE_FIELD c_mst
#define	STACK_END_ADDR 0x2ff23000
#define LOADER_OFFSET_FIELD c_tab
#define LOADER_REGION_SIZE 0x7ffffff
#define CORE_DUMP core_dump
#else
#define CORE_DATA_SIZE_FIELD c_u.u_dsize
#define CORE_COMM_FIELD c_u.u_comm
#define SAVE_FIELD c_u.u_save
#define	STACK_END_ADDR 0x2ff80000
#define LOADER_OFFSET_FIELD c_tab
#define LOADER_REGION_SIZE 0x7ffffff
#define CORE_DUMP core_dump
#endif
#endif

/* These are stored in the bfd's tdata */
typedef struct {
  struct CORE_DUMP hdr;		/* core file header */
} Rs6kCorData;

static asection *make_bfd_asection PARAMS ((bfd *, CONST char *, flagword,
					    bfd_size_type, bfd_vma, file_ptr));

static asection *
make_bfd_asection (abfd, name, flags, _raw_size, vma, filepos)
     bfd *abfd;
     CONST char *name;
     flagword flags;
     bfd_size_type _raw_size;
     bfd_vma vma;
     file_ptr filepos;
{
  asection *asect;

  asect = bfd_make_section_anyway (abfd, name);
  if (!asect)
    return NULL;

  asect->flags = flags;
  asect->_raw_size = _raw_size;
  asect->vma = vma;
  asect->filepos = filepos;
  asect->alignment_power = 8;

  return asect;
}

/* Decide if a given bfd represents a `core' file or not. There really is no
   magic number or anything like, in rs6000coff. */

const bfd_target *
rs6000coff_core_p (abfd)
     bfd *abfd;
{
  struct CORE_DUMP coredata;
  struct stat statbuf;
  bfd_size_type nread;
  char *tmpptr;

  if (bfd_seek (abfd, 0, SEEK_SET) != 0)
    return NULL;

  nread = bfd_read (&coredata, 1, sizeof (struct CORE_DUMP), abfd);
  if (nread != sizeof (struct CORE_DUMP))
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  if (bfd_stat (abfd, &statbuf) < 0)
    {
      bfd_set_error (bfd_error_system_call);
      return NULL;
    }

  /* If the core file ulimit is too small, the system will first
     omit the data segment, then omit the stack, then decline to
     dump core altogether (as far as I know UBLOCK_VALID and LE_VALID
     are always set) (this is based on experimentation on AIX 3.2).
     Now, the thing is that GDB users will be surprised
     if segments just silently don't appear (well, maybe they would
     think to check "info files", I don't know).

     For the data segment, we have no choice but to keep going if it's
     not there, since the default behavior is not to dump it (regardless
     of the ulimit, it's based on SA_FULLDUMP).  But for the stack segment,
     if it's not there, we refuse to have anything to do with this core
     file.  The usefulness of a core dump without a stack segment is pretty
     limited anyway.  */
     
  if (!(coredata.c_flag & UBLOCK_VALID)
      || !(coredata.c_flag & LE_VALID))
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  if (!(coredata.c_flag & USTACK_VALID))
    {
      bfd_set_error (bfd_error_file_truncated);
      return NULL;
    }

  /* Don't check the core file size for a full core, AIX 4.1 includes
     additional shared library sections in a full core.  */
  if (!(coredata.c_flag & (FULL_CORE | CORE_TRUNC))
      && ((bfd_vma)coredata.c_stack + coredata.c_size) != statbuf.st_size)
    {
      /* If the size is wrong, it means we're misinterpreting something.  */
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

#ifdef AIX_CORE_DUMPX_CORE
  /* For the core_dumpx format, make sure c_entries == 0  If it does
     not, the core file uses the old format */
  if (coredata.c_entries != 0)
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }
#else
  /* Sanity check on the c_tab field.  */
  if ((u_long) coredata.c_tab < sizeof coredata ||
      (u_long) coredata.c_tab >= statbuf.st_size ||
      (long) coredata.c_tab >= (long)coredata.c_stack)
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }
#endif

  /* Issue warning if the core file was truncated during writing.  */
  if (coredata.c_flag & CORE_TRUNC)
    (*_bfd_error_handler) (_("%s: warning core file truncated"),
			   bfd_get_filename (abfd));

  /* Allocate core file header.  */
  tmpptr = (char*) bfd_zalloc (abfd, sizeof (Rs6kCorData));
  if (!tmpptr)
    return NULL;
      
  set_tdata (abfd, tmpptr);

  /* Copy core file header.  */
  core_hdr (abfd) = coredata;

  /* .stack section. */
  if (!make_bfd_asection (abfd, ".stack",
  			  SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS,
			  (bfd_size_type) coredata.c_size,
			  (bfd_vma) (STACK_END_ADDR - coredata.c_size),
			  (file_ptr) coredata.c_stack))
    return NULL;

  /* .reg section for GPRs and special registers. */
  if (!make_bfd_asection (abfd, ".reg",
  			  SEC_HAS_CONTENTS,
			  (bfd_size_type) ((32 + NUM_OF_SPEC_REGS) * 4),
			  (bfd_vma) 0,
			  (file_ptr) ((char *) &coredata.SAVE_FIELD
				      - (char *) &coredata)))
    return NULL;

  /* .reg2 section for FPRs (floating point registers). */
  if (!make_bfd_asection (abfd, ".reg2",
  			  SEC_HAS_CONTENTS,
			  (bfd_size_type) 8 * 32,	/* 32 FPRs. */
			  (bfd_vma) 0,
			  (file_ptr) ((char *) &coredata.SAVE_FIELD.fpr[0]
				      - (char *) &coredata)))
    return NULL;

  /* .ldinfo section.
     To actually find out how long this section is in this particular
     core dump would require going down the whole list of struct ld_info's.
     See if we can just fake it.  */
  if (!make_bfd_asection (abfd, ".ldinfo",
  			  SEC_HAS_CONTENTS,
			  (bfd_size_type) LOADER_REGION_SIZE,
			  (bfd_vma) 0,
			  (file_ptr) coredata.LOADER_OFFSET_FIELD))
    return NULL;

#ifndef CORE_VERSION_1
  /* .data section if present.
     AIX 3 dumps the complete data section and sets FULL_CORE if the
     ulimit is large enough, otherwise the data section is omitted.
     AIX 4 sets FULL_CORE even if the core file is truncated, we have
     to examine coredata.c_datasize below to find out the actual size of
     the .data section.  */
  if (coredata.c_flag & FULL_CORE)
    {
      if (!make_bfd_asection (abfd, ".data",
			      SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS,
			      (bfd_size_type) coredata.CORE_DATA_SIZE_FIELD,
			      (bfd_vma)
				CDATA_ADDR (coredata.CORE_DATA_SIZE_FIELD),
			      (file_ptr) coredata.c_stack + coredata.c_size))
	return NULL;
    }
#endif

#ifdef CORE_VERSION_1
  /* AIX 4 adds data sections from loaded objects to the core file,
     which can be found by examining ldinfo, and anonymously mmapped
     regions.  */
  {
    struct ld_info ldinfo;
    bfd_size_type ldinfo_size;
    file_ptr ldinfo_offset = (file_ptr) coredata.LOADER_OFFSET_FIELD;

    /* .data section from executable.  */
    if (coredata.c_datasize)
      {
	if (!make_bfd_asection (abfd, ".data",
				SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS,
				(bfd_size_type) coredata.c_datasize,
				(bfd_vma)
				  CDATA_ADDR (coredata.CORE_DATA_SIZE_FIELD),
				(file_ptr) coredata.c_data))
	  return NULL;
      }

    /* .data sections from loaded objects.  */
    ldinfo_size = (char *) &ldinfo.ldinfo_filename[0]
		  - (char *) &ldinfo.ldinfo_next;
    while (1)
      {
	if (bfd_seek (abfd, ldinfo_offset, SEEK_SET) != 0)
	  return NULL;
	if (bfd_read (&ldinfo, ldinfo_size, 1, abfd) != ldinfo_size)
	  return NULL;
	if (ldinfo.ldinfo_core)
	  {
	    if (!make_bfd_asection (abfd, ".data",
				    SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS,
				    (bfd_size_type) ldinfo.ldinfo_datasize,
				    (bfd_vma) ldinfo.ldinfo_dataorg,
				    (file_ptr) ldinfo.ldinfo_core))
	      return NULL;
	  }
	if (ldinfo.ldinfo_next == 0)
	  break;
	ldinfo_offset += ldinfo.ldinfo_next;
      }

    /* .vmdata sections from anonymously mmapped regions.  */
    if (coredata.c_vmregions)
      {
	int i;

	if (bfd_seek (abfd, (file_ptr) coredata.c_vmm, SEEK_SET) != 0)
	  return NULL;

	for (i = 0; i < coredata.c_vmregions; i++)
	  {
	    struct vm_info vminfo;

	    if (bfd_read (&vminfo, sizeof (vminfo), 1, abfd) != sizeof (vminfo))
	      return NULL;
	    if (vminfo.vminfo_offset)
	      {
		if (!make_bfd_asection (abfd, ".vmdata",
					SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS,
					(bfd_size_type) vminfo.vminfo_size,
					(bfd_vma) vminfo.vminfo_addr,
					(file_ptr) vminfo.vminfo_offset))
		  return NULL;
	      }
	  }
      }
  }
#endif

  return abfd->xvec;				/* this is garbage for now. */
}



/* return `true' if given core is from the given executable.. */
boolean
rs6000coff_core_file_matches_executable_p (core_bfd, exec_bfd)
     bfd *core_bfd;
     bfd *exec_bfd;
{
  struct CORE_DUMP coredata;
  struct ld_info ldinfo;
  bfd_size_type size;
  char *path, *s;
  size_t alloc;
  const char *str1, *str2;
  boolean ret;

  if (bfd_seek (core_bfd, 0, SEEK_SET) != 0
      || bfd_read (&coredata, sizeof coredata, 1, core_bfd) != sizeof coredata)
    return false;

  if (bfd_seek (core_bfd, (long) coredata.LOADER_OFFSET_FIELD, SEEK_SET) != 0)
    return false;

  size = (char *) &ldinfo.ldinfo_filename[0] - (char *) &ldinfo.ldinfo_next;
  if (bfd_read (&ldinfo, size, 1, core_bfd) != size)
    return false;

  alloc = 100;
  path = bfd_malloc (alloc);
  if (path == NULL)
    return false;
  s = path;

  while (1)
    {
      if (bfd_read (s, 1, 1, core_bfd) != 1)
	{
	  free (path);
	  return false;
	}
      if (*s == '\0')
	break;
      ++s;
      if (s == path + alloc)
	{
	  char *n;

	  alloc *= 2;
	  n = bfd_realloc (path, alloc);
	  if (n == NULL)
	    {
	      free (path);
	      return false;
	    }
	  s = n + (path - s);
	  path = n;
	}
    }
  
  str1 = strrchr (path, '/');
  str2 = strrchr (exec_bfd->filename, '/');

  /* step over character '/' */
  str1 = str1 != NULL ? str1 + 1 : path;
  str2 = str2 != NULL ? str2 + 1 : exec_bfd->filename;

  if (strcmp (str1, str2) == 0)
    ret = true;
  else
    ret = false;

  free (path);

  return ret;
}

char *
rs6000coff_core_file_failing_command (abfd)
     bfd *abfd;
{
  char *com = core_hdr (abfd).CORE_COMM_FIELD;
  if (*com)
    return com;
  else
    return 0;
}

int
rs6000coff_core_file_failing_signal (abfd)
     bfd *abfd;
{
  return core_hdr (abfd).c_signo;
}


boolean
rs6000coff_get_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     sec_ptr section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
    if (count == 0)
	return true;

    /* Reading a core file's sections will be slightly different. For the
       rest of them we can use bfd_generic_get_section_contents () I suppose. */
    /* Make sure this routine works for any bfd and any section. FIXMEmgo. */

    if (abfd->format == bfd_core && strcmp (section->name, ".reg") == 0) {

      struct mstsave mstatus;
      int    regoffset = (char*)&mstatus.gpr[0] - (char*)&mstatus;

      /* Assert that the only way this code will be executed is reading the
         whole section. */
      if (offset || count != (sizeof(mstatus.gpr) + (4 * NUM_OF_SPEC_REGS)))
        (*_bfd_error_handler)
	  (_("ERROR! in rs6000coff_get_section_contents()\n"));

      /* for `.reg' section, `filepos' is a pointer to the `mstsave' structure
         in the core file. */

      /* read GPR's into the location. */
      if ( bfd_seek(abfd, section->filepos + regoffset, SEEK_SET) == -1
	|| bfd_read(location, sizeof (mstatus.gpr), 1, abfd) != sizeof (mstatus.gpr))
	return (false); /* on error */

      /* increment location to the beginning of special registers in the section,
         reset register offset value to the beginning of first special register
	 in mstsave structure, and read special registers. */

      location = (PTR) ((char*)location + sizeof (mstatus.gpr));
      regoffset = (char*)&mstatus.iar - (char*)&mstatus;

      if ( bfd_seek(abfd, section->filepos + regoffset, SEEK_SET) == -1
	|| bfd_read(location, 4 * NUM_OF_SPEC_REGS, 1, abfd) != 
							4 * NUM_OF_SPEC_REGS)
	return (false); /* on error */
      
      /* increment location address, and read the special registers.. */
      /* FIXMEmgo */
      return (true);
    }

    /* else, use default bfd section content transfer. */
    else
      return _bfd_generic_get_section_contents 
      			(abfd, section, location, offset, count);
}

#endif /* AIX_CORE */
