/* This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

#include <unixlib.h>

#include "cvs.h"

/* Return the current directory, newly allocated, arbitrarily long.
   Return NULL and set errno on error. */
char *
xgetwd ()
{
    char pathname[256];

    return xstrdup (getcwd (pathname, sizeof (pathname) - 2, 0));
}
