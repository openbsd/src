/* Yield time_t from struct partime yielded by partime.  */

/* Copyright 1993 Paul Eggert
   Distributed under license by the Free Software Foundation, Inc.

This file is part of RCS.

RCS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

RCS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RCS; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

Report problems and direct all questions to:

    rcs-bugs@cs.purdue.edu

*/

#if defined(__STDC__) || has_prototypes
#	define __MAKETIME_P(x) x
#else
#	define __MAKETIME_P(x) ()
#endif

#ifndef __STDC__
#	define const
#endif

struct tm *time2tm __MAKETIME_P((time_t,int));
time_t difftm __MAKETIME_P((struct tm const*, struct tm const*));
time_t str2time __MAKETIME_P((char const *, time_t, int));
time_t tm2time __MAKETIME_P((struct tm*, int));
void adjzone __MAKETIME_P((struct tm*, int));
