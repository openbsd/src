
/* Win32-Unix compatibility library.
   Copyright (C) 1995 Free Software Foundation, Inc.

This file is part of the libiberty library.
Libiberty is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

Libiberty is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with libiberty; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* sac@cygnus.com */

/* This should only be compiled and linked under Win32. */

#include <stdio.h>
#include <stdlib.h>

/*

NAME

	basename -- return pointer to last component of a pathname

SYNOPSIS

	char *basename (const char *name)

DESCRIPTION

	Given a pointer to a string containing a typical pathname
	(/usr/src/cmd/ls/ls.c for example), returns a pointer to the
	last component of the pathname ("ls.c" in this case).


*/


char *
basename (name)
     const char *name;
{
  const char *base = name;

  while (*name)
    {
      if (*name == '/'
	  || *name == '\\')
	{
	  base = name+1;
	}
      name++;
    }
  return (char *) base;
}
