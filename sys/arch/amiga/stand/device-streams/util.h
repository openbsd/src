/* -------------------------------------------------- 
 |  NAME
 |    util
 |  PURPOSE
 |    header for util.c
 |  NOTES
 | 
 |  COPYRIGHT
 |    Copyright (C) 1993  Christian E. Hopps
 |
 |    This program is free software; you can redistribute it and/or modify
 |    it under the terms of the GNU General Public License as published by
 |    the Free Software Foundation; either version 2 of the License, or
 |    (at your option) any later version.
 |
 |    This program is distributed in the hope that it will be useful,
 |    but WITHOUT ANY WARRANTY; without even the implied warranty of
 |    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 |    GNU General Public License for more details.
 |
 |    You should have received a copy of the GNU General Public License
 |    along with this program; if not, write to the Free Software
 |    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 |    
 |  HISTORY
 |    chopps - Oct 9, 1993: Created.
 +--------------------------------------------------- */

#if ! defined (_UTIL_H)
#define _UTIL_H

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/nodes.h>
#include <exec/lists.h>
#include <dos/dos.h>
#include <dos/dosextens.h>

#include <clib/exec_protos.h>
#include <pragmas/exec_pragmas.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

typedef ULONG ulong;

#include "protos.h"
extern ulong opt_debug;
extern ulong opt_verbose;

/* Macros */
#define copy_string(x) alloc_string (x)
#define ptrfrom(st,fl,p) ((st *)(((char *)p) - (offsetof (st,fl))))
#define valid_mem(m) (TypeOfMem (m))
#define megs(x) ((x)/(1024*1024))
#define tenths_of_a_meg(x) ((10*(((x)/1024) % 1024))/1024)

extern FILE *mout;
extern FILE *min;

#if defined (DEBUG_ENABLED_VERSION)
#define D(x) x
#else
#define D(x)
#endif				   

#endif /* _UTIL_H */
