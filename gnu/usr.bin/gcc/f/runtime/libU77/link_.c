/* Copyright (C) 1995 Free Software Foundation, Inc.
This file is part of GNU Fortran libU77 library.

This library is free software; you can redistribute it and/or modify it
under the terms of the GNU Library General Public License as published
by the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GNU Fortran is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with GNU Fortran; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if HAVE_STDLIB_H
#  include <stdlib.h>
#else
#  include <stdio.h>
#endif
#if HAVE_UNISTD_H
#  include <unistd.h>
#endif
#include <errno.h>
#include <sys/param.h>
#include "f2c.h"

#ifdef KR_headers
integer link_ (path1, path2, Lpath1, Lpath2)
     char *path1, *path2; ftnlen Lpath1, Lpath2;
#else
integer link_ (const char *path1, const char *path2, const ftnlen  Lpath1,
	       const ftnlen  Lpath2)
#endif
{
  char *buff1, *buff2;
  char *bp, *blast;
  int i;

  buff1 = malloc (Lpath1+1);
  if (buff1 == NULL) return -1;
  blast = buff1 + Lpath1;
  for (bp = buff1 ; bp<blast ; )
    *bp++ = *path1++;
  *bp = '\0';
  buff2 = malloc (Lpath2+1);
  if (buff2 == NULL) return -1;
  blast = buff2 + Lpath2;
  for (bp = buff2 ; bp<blast ; )
    *bp++ = *path2++;
  *bp = '\0';
  i = link (buff1, buff2);
  free (buff1); free (buff2);
  return i ? errno : 0;
}
