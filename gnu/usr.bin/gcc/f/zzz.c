/* zzz.c -- Implementation File (module.c template V1.0)
   Copyright (C) 1995 Free Software Foundation, Inc.
   Contributed by James Craig Burley (burley@gnu.ai.mit.edu).

This file is part of GNU Fortran.

GNU Fortran is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Fortran is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Fortran; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.

   Related Modules:
      None

   Description:
      Has the version number for the front end.	 Makes it easier to
      tell how consistently patches have been applied, etc.

   Modifications:
*/

#include "proj.h"
#include "zzz.h"

/* If you want to override the version date/time info with your own
   macros, e.g. for a consistent distribution when bootstrapping,
   go ahead!  */

#ifndef FFEZZZ_DATE
#define FFEZZZ_DATE __DATE__
#endif
#ifndef FFEZZZ_TIME
#define FFEZZZ_TIME __TIME__
#endif

char *ffezzz_version_string = "0.5.18";
char *ffezzz_date = FFEZZZ_DATE;
char *ffezzz_time = FFEZZZ_TIME;
