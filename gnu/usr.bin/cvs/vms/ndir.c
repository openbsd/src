/*
 * Copyright © 1994 the Free Software Foundation, Inc.
 *
 * Author: Richard Levitte (levitte@e.kth.se)
 *
 * This file is a part of GNU VMSLIB, the GNU library for porting GNU
 * software to VMS.
 *
 * GNU VMSLIB is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GNU VMSLIB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __VMS_VER
#define __VMS_VER 0
#endif
#ifndef __DECC_VER
#define __DECC_VER 0
#endif

#include <varargs.h>
#include <rms.h>
#include <descrip.h>
#include <string.h>
#include <errno.h>

#ifdef __GNUC__
#include <sys/stat.h>
#else
#include <stat.h>
#endif
#include <lib$routines.h>

#include "ndir.h"
#include "filutils.h"

/* The following was snarfed from lib-src/alloca.c in GNU Emacs,
   the hacked.  */

#if __STDC__
typedef void procedure;
typedef void *pointer;
#else
typedef int procedure;
typedef char *pointer;
#endif

/* Different portions of Emacs need to call different versions of
   malloc.  The Emacs executable needs alloca to call xmalloc, because
   ordinary malloc isn't protected from input signals.  On the other
   hand, the utilities in lib-src need alloca to call malloc; some of
   them are very simple, and don't have an xmalloc routine.

   Non-Emacs programs expect this to call use xmalloc.

   Callers below should use malloc.

   There is some need for BLOCK_INPUT and UNBLOCK_INPUT, but it is really
   only used in Emacs, so that's the only time it's used.  Otherwise,
   they are just empty statements.  */

#ifndef emacs
#include "misc.h"
#define malloc xmalloc
#define free xfree
#endif

#if 0
extern pointer malloc ();
extern procedure free ();
#endif

/* end of snarf.  */

#ifndef BLOCK_INPUT
#define BLOCK_INPUT
#endif
#ifndef UNBLOCK_INPUT
#define UNBLOCK_INPUT
#endif

static struct direct *vms_low_readdir ();

typedef struct
{
  DIR s_dir;
  unsigned long context;
  unsigned long uflags;
  struct dsc$descriptor_s dir_spec;
  struct dsc$descriptor_s file_spec;
  int version_flag;
  unsigned long status;
} VMS_DIR;

DIR *
vms_opendir (infilename, filepattern)
     char *infilename;	/* name of directory */
     char *filepattern;
{
  register VMS_DIR *dirp;	/* -> malloc'ed storage */
  register unsigned int length = 1024;
  register int fd;		/* file descriptor for read */
  char *filename;
  struct stat sbuf;		/* result of fstat */

  filename = (char *) malloc(length+1);
  strcpy(filename, infilename);

  strip_trailing_slashes (filename);
  if(strcmp(filename, ".") == 0)
     {
     getcwd(filename, length+1, 1); /* Get a VMS filespec */
     length = strlen(filename);
     }

  BLOCK_INPUT;
  if ((filename[length-1] != ']'
       && filename[length-1] != '>'
       && filename[length-1] != ':'
       && (stat (filename, &sbuf) < 0
	   || (sbuf.st_mode & S_IFMT) != S_IFDIR)))
    {
      errno = ENOTDIR;
      UNBLOCK_INPUT;
      free(filename);
      return 0;		/* bad luck today */
    }

  if ((dirp = (VMS_DIR *) xmalloc (sizeof (VMS_DIR))) == 0)
    {
      errno = ENOMEM;
      UNBLOCK_INPUT;
      free(filename);
      return 0;		/* bad luck today */
    }

  {
    int count;
    va_count(count);
    if (count == 2)
      {
	dirp->file_spec.dsc$a_pointer = 
	  (char *) xmalloc (strlen (filepattern) + 1);
	strcpy (dirp->file_spec.dsc$a_pointer, filepattern);
      }
    else
      {
	dirp->file_spec.dsc$a_pointer = 
	  (char *) xmalloc (4);
	strcpy (dirp->file_spec.dsc$a_pointer, "*.*");
      }
    dirp->file_spec.dsc$w_length = strlen (dirp->file_spec.dsc$a_pointer);
    dirp->file_spec.dsc$b_dtype = DSC$K_DTYPE_T;
    dirp->file_spec.dsc$b_class = DSC$K_CLASS_S;
    dirp->version_flag = strchr (dirp->file_spec.dsc$a_pointer, ';') != 0;
  }
  dirp->dir_spec.dsc$a_pointer = (char *) xmalloc (strlen (filename) + 10);
  UNBLOCK_INPUT;
  file_name_as_directory (dirp->dir_spec.dsc$a_pointer, filename);
  dirp->dir_spec.dsc$w_length = strlen (dirp->dir_spec.dsc$a_pointer);
  dirp->dir_spec.dsc$b_dtype = DSC$K_DTYPE_T;
  dirp->dir_spec.dsc$b_class = DSC$K_CLASS_S;
  dirp->context = 0;
  dirp->uflags = 2;
  dirp->s_dir.dd_fd = 0;
  dirp->s_dir.dd_loc = dirp->s_dir.dd_size = 0;	/* refill needed */

  free(filename);

  /* In the cases where the filename ended with `]', `>' or `:',
     we never checked if it really was a directory, so let's do that
     now, by trying to read the first entry.  */
  if (vms_low_readdir ((DIR *) dirp) == (struct direct *) -1)
    {
      vms_closedir (dirp);		/* was: xfree (dirp);  */
      errno = ENOENT;
      return 0;
    }
  dirp->s_dir.dd_loc = 0;	/* Make sure the entry just read is
				   reused at the next call to readdir.  */

  return (DIR *) dirp;		/* I had to cast, for VMS sake.  */
}

int
vms_closedir (dirp)
     register DIR *dirp;		/* stream from vms_opendir */
{
  {
    VMS_DIR *vms_dirp = (VMS_DIR *) dirp;

    if (vms_dirp->context != 0)
      lib$find_file_end (&(vms_dirp->context));
    xfree (vms_dirp->dir_spec.dsc$a_pointer);
    xfree (vms_dirp->file_spec.dsc$a_pointer);
  }

  xfree ((char *) dirp);
  return 0;
}

struct direct dir_static;	/* simulated directory contents */

static struct direct *
vms_low_readdir (dirp)
     register DIR *dirp;
{
  static char rbuf[257];
  static struct dsc$descriptor_s rdsc =
    { sizeof (rbuf), DSC$K_DTYPE_T, DSC$K_CLASS_S, rbuf };
  VMS_DIR * vms_dirp = (VMS_DIR *) dirp;

  if (dirp->dd_size == 0)
    {
      char *cp, *cp2;
      unsigned long status;

      status = lib$find_file (&vms_dirp->file_spec, &rdsc, &vms_dirp->context,
			      &vms_dirp->dir_spec, 0, 0, &vms_dirp->uflags);
      vms_dirp->status = status;
      if (status == RMS$_NMF || status == RMS$_FNF)
	return 0;
      if (status != RMS$_NORMAL)
	return (struct direct *) -1;

      rbuf [256] = '\0';
      if (cp = strchr (rbuf, ' '))
	*cp = '\0';
      if ((cp = strchr (rbuf, ';')) != 0
	  && !vms_dirp->version_flag)
	*cp = '\0';

      for (cp2 = rbuf - 1; cp2 != 0;)
	{
	  char *cp2tmp = 0;
	  cp = cp2 + 1;
	  cp2 = strchr (cp, ']');
	  if (cp2 != 0)
	    cp2tmp = strchr (cp2 + 1, '>');
	  if (cp2tmp != 0)
	    cp2 = cp2tmp;
	}

      /* Propagate names as lower case only,
         directories have ".dir" truncated,
         do not propagate null extensions "makefile." */
      {
      char *p, *q;

      if(strcmp(cp, "CVS.DIR") == 0)
        strcpy(dirp->dd_buf, "CVS");
      else
        {
        for(p = cp, q = dirp->dd_buf; *p;)
           {
           if(strcmp(p, ".DIR") == 0)
              break;
           else
             *q++ = tolower(*p++);
           }
        *q = '\0';
        if(*(q-1) == '.')
          *(q-1) = '\0';
        }
     }
#if 0
      strcpy (dirp->dd_buf, cp);
#endif

      dirp->dd_size = strlen (dirp->dd_buf);
      dirp->dd_loc = 0;
    }

  if (vms_dirp->status != RMS$_NORMAL)
    return 0;

  dir_static.d_ino = -1;	/* Couldn't care less...  */
  dir_static.d_namlen = strlen (dirp->dd_buf);
  dir_static.d_reclen = sizeof (struct direct)
    - MAXNAMLEN + 3
      + dir_static.d_namlen - dir_static.d_namlen % 4;
  strcpy (dir_static.d_name, dirp->dd_buf);
  dir_static.d_name[dir_static.d_namlen] = '\0';
  dirp->dd_loc = dirp->dd_size; /* only one record at a time */

  return &dir_static;
}

/* ARGUSED */
struct direct *
vms_readdir (dirp)
     register DIR *dirp;	/* stream from vms_opendir */
{
  register struct direct *dp;

  for (; ;)
    {
      if (dirp->dd_loc >= dirp->dd_size)
	dirp->dd_loc = dirp->dd_size = 0;

      dp = vms_low_readdir (dirp);
      if (dp == 0 || dp == (struct direct *) -1)
	return 0;
      return dp;
    }
}
