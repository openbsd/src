/* Copyright (C) 1995, 1996 Free Software Foundation, Inc.
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

#if 0	/* Don't include these unless necessary -- jcb. */
#include "f2c.h"
#include <math.h>

doublereal besj0_ (const real *x) {
    return j0 (*x);
}

doublereal besj1_ (const real *x) {
    return j1 (*x);
}

doublereal besjn_ (const integer *n, real *x) {
     return jn (*n, *x);
 }

doublereal besy0_ (const real *x) {
    return y0 (*x);
}

doublereal besy1_ (const real *x) {
    return y1 (*x);
}

doublereal besyn_ (const integer *n, real *x) {
    return yn (*n, *x);
}
#endif
