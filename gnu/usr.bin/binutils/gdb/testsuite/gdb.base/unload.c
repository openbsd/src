/* This testcase is part of GDB, the GNU debugger.

   Copyright 2004 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

int k = 0;

#define SHLIB_NAME SHLIB_DIR "/unloadshr.sl"

int main()
{
  void *handle;
  int (*unloadshr) (int);
  int y;
  const char *msg;

  handle = dlopen (SHLIB_NAME, RTLD_LAZY);
  msg = dlerror ();
  
  if (!handle)
    {
      fprintf (stderr, msg);
      exit (1);
    }

  unloadshr = (int (*)(int))dlsym (handle, "shrfunc1");

  if (!unloadshr)
    {
      fprintf (stderr, dlerror ());
      exit (1);
    }

  y = (*unloadshr)(3);

  printf ("y is %d\n", y);

  dlclose (handle);

  return 0;
}
