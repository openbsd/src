/* global.h -- Public #include File (module.h template V1.0)
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

   Owning Modules:
      global.c

   Modifications:
*/

/* Allow multiple inclusion to work. */

#ifndef _H_f_global
#define _H_f_global

/* Simple definitions and enumerations. */

typedef enum
  {
    FFEGLOBAL_typeMAIN,
    FFEGLOBAL_typeSUBR,
    FFEGLOBAL_typeFUNC,
    FFEGLOBAL_typeBDATA,
    FFEGLOBAL_typeCOMMON,
    FFEGLOBAL_typeANY,		/* Confusion reigns, so just ignore. */
    FFEGLOBAL_type
  } ffeglobalType;

/* Typedefs. */

typedef struct _ffeglobal_ *ffeglobal;

/* Include files needed by this one. */

#include "lex.h"
#include "name.h"
#include "symbol.h"
#include "target.h"
#include "top.h"

/* Structure definitions. */

struct _ffeglobal_
  {
    ffelexToken t;
    ffename n;
#ifdef FFECOM_globalHOOK
    ffecomGlobal hook;
#endif
    ffeCounter init;		/* COMMON block given initial value(s) in
				   this progunit. */
    ffelexToken initt;		/* First initial value. */
    ffeglobalType type;
    bool have_pad;		/* Padding info avail for COMMON? */
    ffetargetAlign pad;		/* Initial padding for COMMON. */
    ffewhereLine pad_where_line;
    ffewhereColumn pad_where_col;
    bool have_save;		/* Save info avail for COMMON? */
    bool save;			/* Save info for COMMON. */
    ffewhereLine save_where_line;
    ffewhereColumn save_where_col;
    bool have_size;		/* Size info avail for COMMON? */
    long size;			/* Size info for COMMON. */
    bool blank;			/* TRUE if blank COMMON. */
  };

/* Global objects accessed by users of this module. */


/* Declare functions with prototypes. */

void ffeglobal_drive (ffeglobal (*fn) ());
void ffeglobal_init_1 (void);
void ffeglobal_init_common (ffesymbol s, ffelexToken t);
void ffeglobal_new_progunit_ (ffesymbol s, ffelexToken t, ffeglobalType type);
void ffeglobal_new_common (ffesymbol s, ffelexToken t, bool blank);
void ffeglobal_pad_common (ffesymbol s, ffetargetAlign pad, ffewhereLine wl,
			   ffewhereColumn wc);
ffeglobal ffeglobal_promoted (ffesymbol s);
void ffeglobal_save_common (ffesymbol s, bool save, ffewhereLine wl,
			    ffewhereColumn wc);
bool ffeglobal_size_common (ffesymbol s, long size);
void ffeglobal_terminate_1 (void);

/* Define macros. */

#if FFECOM_targetCURRENT == FFECOM_targetFFE
#define FFEGLOBAL_ENABLED 0
#elif FFECOM_targetCURRENT == FFECOM_targetGCC
#define FFEGLOBAL_ENABLED 1
#else
#error
#endif

#define ffeglobal_common_init(g) ((g)->init != 0)
#define ffeglobal_have_pad(g) ((g)->have_pad)
#define ffeglobal_have_size(g) ((g)->have_size)
#define ffeglobal_hook(g) ((g)->hook)
#define ffeglobal_init_0()
#define ffeglobal_init_2()
#define ffeglobal_init_3()
#define ffeglobal_init_4()
#define ffeglobal_new_blockdata(s,t) \
      ffeglobal_new_progunit_(s,t,FFEGLOBAL_typeBDATA)
#define ffeglobal_new_function(s,t) \
      ffeglobal_new_progunit_(s,t,FFEGLOBAL_typeFUNC)
#define ffeglobal_new_program(s,t) \
      ffeglobal_new_progunit_(s,t,FFEGLOBAL_typeMAIN)
#define ffeglobal_new_subroutine(s,t) \
      ffeglobal_new_progunit_(s,t,FFEGLOBAL_typeSUBR)
#define ffeglobal_pad(g) ((g)->pad)
#define ffeglobal_set_hook(g,h) ((g)->hook = (h))
#define ffeglobal_size(g) ((g)->size)
#define ffeglobal_terminate_0()
#define ffeglobal_terminate_2()
#define ffeglobal_terminate_3()
#define ffeglobal_terminate_4()
#define ffeglobal_text(g) ffename_text((g)->n)
#define ffeglobal_type(g) ((g)->type)

/* End of #include file. */

#endif
