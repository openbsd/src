/*  msd_dir.c - portable directory routines
    Copyright (C) 1990 by Thorsten Ohl, td12@ddagsi3.bitnet

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 1, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.  */

/* Everything non trivial in this code is from: @(#)msd_dir.c 1.4
   87/11/06.  A public domain implementation of BSD directory routines
   for MS-DOS.  Written by Michael Rendell ({uunet,utai}michael@garfield),
   August 1897 */


#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <dos.h>

#include <ndir.h>

static void free_dircontents (struct _dircontents *);

/* find ALL files! */
#define ATTRIBUTES	(_A_RDONLY | _A_HIDDEN | _A_SYSTEM | _A_SUBDIR)



DIR *
opendir (const char *name)
{
  struct _finddata_t find_buf;
  DIR *dirp;
  struct _dircontents *dp;
  char name_buf[_MAX_PATH + 1];
  char *slash = "";
  long hFile;

  if (!name)
    name = "";
  else if (*name)
    {
      const char *s;
      int l = strlen (name);

      s = name + l - 1;
      if ( !(l == 2 && *s == ':') && *s != '\\' && *s != '/')
	slash = "/";	/* save to insert slash between path and "*.*" */
    }

  strcat (strcat (strcpy (name_buf, name), slash), "*.*");

  dirp = (DIR *) malloc (sizeof (DIR));
  if (dirp == (DIR *)0)
    return (DIR *)0;

  dirp->dd_loc = 0;
  dirp->dd_contents = dirp->dd_cp = (struct _dircontents *) 0;

  if ((hFile = _findfirst (name_buf, &find_buf)) < 0)
    {
      free (dirp);
      return (DIR *)0;
    }

  do
    {
      dp = (struct _dircontents *) malloc (sizeof (struct _dircontents));
      if (dp == (struct _dircontents *)0)
	{
	  free_dircontents (dirp->dd_contents);
	  return (DIR *)0;
	}

      dp->_d_entry = malloc (strlen (find_buf.name) + 1);
      if (dp->_d_entry == (char *)0)
	{
	  free (dp);
	  free_dircontents (dirp->dd_contents);
	  return (DIR *)0;
	}

      if (dirp->dd_contents)
	dirp->dd_cp = dirp->dd_cp->_d_next = dp;
      else
	dirp->dd_contents = dirp->dd_cp = dp;

      strcpy (dp->_d_entry, find_buf.name);

      dp->_d_next = (struct _dircontents *)0;

    } while (!_findnext (hFile, &find_buf));

  dirp->dd_cp = dirp->dd_contents;

  _findclose(hFile);

  return dirp;
}


void
closedir (DIR *dirp)
{
  free_dircontents (dirp->dd_contents);
  free ((char *) dirp);
}


struct direct *
readdir (DIR *dirp)
{
  static struct direct dp;

  if (dirp->dd_cp == (struct _dircontents *)0)
    return (struct direct *)0;
  dp.d_namlen = dp.d_reclen =
    strlen (strcpy (dp.d_name, dirp->dd_cp->_d_entry));
#if 0 /* JB */
  strlwr (dp.d_name);		/* JF */
#endif
  dp.d_ino = 0;
  dirp->dd_cp = dirp->dd_cp->_d_next;
  dirp->dd_loc++;

  return &dp;
}


void
seekdir (DIR *dirp, long off)
{
  long i = off;
  struct _dircontents *dp;

  if (off < 0)
    return;
  for (dp = dirp->dd_contents; --i >= 0 && dp; dp = dp->_d_next)
    ;
  dirp->dd_loc = off - (i + 1);
  dirp->dd_cp = dp;
}


long
telldir (DIR *dirp)
{
  return dirp->dd_loc;
}


/* Garbage collection */

static void
free_dircontents (struct _dircontents *dp)
{
  struct _dircontents *odp;

  while (dp)
    {
      if (dp->_d_entry)
	free (dp->_d_entry);
      dp = (odp = dp)->_d_next;
      free (odp);
    }
}


#ifdef TEST

void main (int argc, char *argv[]);

void
main (int argc, char *argv[])
{
  static DIR *directory;
  struct direct *entry = (struct direct *)0;

  char *name = "";

  if (argc > 1)
    name = argv[1];

  directory = opendir (name);

  if (!directory)
    {
      fprintf (stderr, "can't open directory `%s'.\n", name);
      exit (2);
    }

  while (entry = readdir (directory))
    printf ("> %s\n", entry->d_name);

  printf ("done.\n");
}

#endif /* TEST */

/* 
 * Local Variables:
 * mode:C
 * ChangeLog:ChangeLog
 * compile-command:make
 * End:
 */
