/*  ndir.c - portable directory routines
    Copyright (C) 1990 by Thorsten Ohl, td12@ddagsi3.bitnet

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 1, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Header: /home/cvs/src/gnu/usr.bin/cvs/windows-NT/ndir.h,v 1.1.1.1 1995/12/19 09:21:43 deraadt Exp $
 */

/* Everything non trivial in this code is taken from: @(#)msd_dir.c 1.4
   87/11/06.  A public domain implementation of BSD directory routines
   for MS-DOS.  Written by Michael Rendell ({uunet,utai}michael@garfield),
   August 1897 */

#include <sys/types.h>	/* ino_t definition */

#define	rewinddir(dirp)	seekdir(dirp, 0L)

#define	MAXNAMLEN	12

struct direct
{
  ino_t d_ino;			/* a bit of a farce */
  int d_reclen;			/* more farce */
  int d_namlen;			/* length of d_name */
  char d_name[MAXNAMLEN + 1];	/* garentee null termination */
};

struct _dircontents
{
  char *_d_entry;
  struct _dircontents *_d_next;
};

typedef struct _dirdesc
{
  int dd_id;			/* uniquely identify each open directory */
  long dd_loc;			/* where we are in directory entry is this */
  struct _dircontents *dd_contents;	/* pointer to contents of dir */
  struct _dircontents *dd_cp;	/* pointer to current position */
} DIR;

extern void seekdir (DIR *, long);
extern long telldir (DIR *);
extern DIR *opendir (const char *);
extern void closedir (DIR *);
extern struct direct *readdir (DIR *);

/* 
 * Local Variables:
 * mode:C
 * ChangeLog:ChangeLog
 * compile-command:make
 * End:
 */
