/* global.c -- Implementation File (module.c template V1.0)
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

   Description:
      Manages information kept across individual program units within a single
      source file.  This includes reporting errors when a name is defined
      multiple times (for example, two program units named FOO) and when a
      COMMON block is given initial data in more than one program unit.

   Modifications:
*/

/* Include files. */

#include "proj.h"
#include "global.h"
#include "lex.h"
#include "malloc.h"
#include "name.h"
#include "symbol.h"
#include "top.h"

/* Externals defined here. */


/* Simple definitions and enumerations. */


/* Internal typedefs. */


/* Private include files. */


/* Internal structure definitions. */


/* Static objects accessed by functions in this module. */

#if FFEGLOBAL_ENABLED
static ffenameSpace ffeglobal_filewide_ = NULL;
#endif

/* Static functions (internal). */


/* Internal macros. */


/* Call given fn with all globals

   ffeglobal (*fn)(ffeglobal g);
   ffeglobal_drive(fn);	 */

#if FFEGLOBAL_ENABLED
void
ffeglobal_drive (ffeglobal (*fn) ())
{
  if (ffeglobal_filewide_ != NULL)
    ffename_space_drive_global (ffeglobal_filewide_, fn);
}

#endif
/* ffeglobal_new_ -- Make new global

   ffename n;
   ffeglobal g;
   g = ffeglobal_new_(n);  */

#if FFEGLOBAL_ENABLED
static ffeglobal
ffeglobal_new_ (ffename n)
{
  ffeglobal g;

  assert (n != NULL);

  g = (ffeglobal) malloc_new_ks (malloc_pool_image (), "FFEGLOBAL",
				 sizeof (*g));
  g->n = n;
#ifdef FFECOM_globalHOOK
  g->hook = FFECOM_globalNULL;
#endif

  ffename_set_global (n, g);

  return g;
}

#endif
/* ffeglobal_init_1 -- Initialize per file

   ffeglobal_init_1();	*/

void
ffeglobal_init_1 ()
{
#if FFEGLOBAL_ENABLED
  if (ffeglobal_filewide_ != NULL)
    ffename_space_kill (ffeglobal_filewide_);
  ffeglobal_filewide_ = ffename_space_new (malloc_pool_image ());
#endif
}

/* ffeglobal_init_common -- Initial value specified for common block

   ffesymbol s;	 // the ffesymbol for the common block
   ffelexToken t;  // the token with the point of initialization
   ffeglobal_init_common(s,t);

   For back ends where file-wide global symbols are not maintained, does
   nothing.  Otherwise, makes sure this common block hasn't already been
   initialized in a previous program unit, and flag that it's been
   initialized in this one.  */

void
ffeglobal_init_common (ffesymbol s, ffelexToken t)
{
#if FFEGLOBAL_ENABLED
  ffeglobal g;

  g = ffesymbol_global (s);
  if ((g == NULL) || (g->type != FFEGLOBAL_typeCOMMON))
    return;

  if (g->init == ffe_count_2)
    return;

  if (g->init != 0)
    {
      if (g->initt != NULL)
	{
	  ffebad_start (FFEBAD_COMMON_ALREADY_INIT);
	  ffebad_string (ffesymbol_text (s));
	  ffebad_here (0, ffelex_token_where_line (t), ffelex_token_where_column (t));
	  ffebad_here (1, ffelex_token_where_line (g->initt),
		       ffelex_token_where_column (g->initt));
	  ffebad_finish ();
	}

      /* Complain about just one attempt to reinit per program unit, but
	 continue referring back to the first such successful attempt.  */
    }
  else
    {
      if (g->blank)
	{
	  ffebad_start (FFEBAD_COMMON_BLANK_INIT);
	  ffebad_here (0, ffelex_token_where_line (t), ffelex_token_where_column (t));
	  ffebad_finish ();
	}

      g->initt = ffelex_token_use (t);
    }

  g->init = ffe_count_2;
#endif
}

/* ffeglobal_new_common -- New common block

   ffesymbol s;	 // the ffesymbol for the new common block
   ffelexToken t;  // the token with the name of the common block
   bool blank;	// TRUE if blank common
   ffeglobal_new_common(s,t,blank);

   For back ends where file-wide global symbols are not maintained, does
   nothing.  Otherwise, makes sure this symbol hasn't been seen before or
   is known as a common block.	*/

void
ffeglobal_new_common (ffesymbol s, ffelexToken t, bool blank)
{
#if FFEGLOBAL_ENABLED
  ffename n;
  ffeglobal g;

  if (ffesymbol_global (s) == NULL)
    {
      n = ffename_find (ffeglobal_filewide_, t);
      g = ffename_global (n);
    }
  else
    {
      g = ffesymbol_global (s);
      n = NULL;
    }

  if (g != NULL)
    {
      if (g->type == FFEGLOBAL_typeCOMMON)
	{
	  assert (g->blank == blank);
	}
      else
	{
	  ffebad_start (FFEBAD_FILEWIDE_ALREADY_SEEN);
	  ffebad_string (ffelex_token_text (t));
	  ffebad_here (0, ffelex_token_where_line (t), ffelex_token_where_column (t));
	  ffebad_here (1, ffelex_token_where_line (g->t),
		       ffelex_token_where_column (g->t));
	  ffebad_finish ();
	  g->type = FFEGLOBAL_typeANY;
	}
    }
  else
    {
      g = ffeglobal_new_ (n);
      g->t = ffelex_token_use (t);
      g->init = 0;
      g->type = FFEGLOBAL_typeCOMMON;
      g->have_pad = FALSE;
      g->have_save = FALSE;
      g->have_size = FALSE;
      g->blank = blank;
    }

  ffesymbol_set_global (s, g);
#endif
}

/* ffeglobal_new_progunit_ -- New program unit

   ffesymbol s;	 // the ffesymbol for the new unit
   ffelexToken t;  // the token with the name of the unit
   ffeglobalType type;	// the type of the new unit
   ffeglobal_new_progunit_(s,t,type);

   For back ends where file-wide global symbols are not maintained, does
   nothing.  Otherwise, makes sure this symbol hasn't been seen before.	 */

void
ffeglobal_new_progunit_ (ffesymbol s, ffelexToken t, ffeglobalType type)
{
#if FFEGLOBAL_ENABLED
  ffename n;
  ffeglobal g;

  n = ffename_find (ffeglobal_filewide_, t);
  g = ffename_global (n);
  if (g != NULL)
    {
      ffebad_start (FFEBAD_FILEWIDE_ALREADY_SEEN);
      ffebad_string (ffelex_token_text (t));
      ffebad_here (0, ffelex_token_where_line (t),
		   ffelex_token_where_column (t));
      ffebad_here (1, ffelex_token_where_line (g->t),
		   ffelex_token_where_column (g->t));
      ffebad_finish ();
      g->type = FFEGLOBAL_typeANY;
    }
  else
    {
      g = ffeglobal_new_ (n);
      g->t = ffelex_token_use (t);
      g->type = type;
    }

  ffesymbol_set_global (s, g);
#endif
}

/* ffeglobal_pad_common -- Check initial padding of common area

   ffesymbol s;	 // the common area
   ffetargetAlign pad;	// the initial padding
   ffeglobal_pad_common(s,pad,ffesymbol_where_line(s),
	 ffesymbol_where_column(s));

   In global-enabled mode, make sure the padding agrees with any existing
   padding established for the common area, otherwise complain.
   In global-disabled mode, warn about nonzero padding.	 */

void
ffeglobal_pad_common (ffesymbol s, ffetargetAlign pad, ffewhereLine wl,
		      ffewhereColumn wc)
{
#if FFEGLOBAL_ENABLED
  ffeglobal g;

  g = ffesymbol_global (s);
  if ((g == NULL) || (g->type != FFEGLOBAL_typeCOMMON))
    return;			/* Let someone else catch this! */

  if (!g->have_pad)
    {
      g->have_pad = TRUE;
      g->pad = pad;
      g->pad_where_line = ffewhere_line_use (wl);
      g->pad_where_col = ffewhere_column_use (wc);
    }
  else
    {
      if (g->pad != pad)
	{
	  char padding_1[20];
	  char padding_2[20];

	  sprintf (&padding_1[0], "%" ffetargetAlign_f "u", pad);
	  sprintf (&padding_2[0], "%" ffetargetAlign_f "u", g->pad);
	  ffebad_start (FFEBAD_COMMON_DIFF_PAD);
	  ffebad_string (ffesymbol_text (s));
	  ffebad_string (padding_1);
	  ffebad_here (0, wl, wc);
	  ffebad_string (padding_2);
	  ffebad_string ((pad == 1)
			 ? FFECOM_SIZE_UNIT : FFECOM_SIZE_UNITS);
	  ffebad_string ((g->pad == 1)
			 ? FFECOM_SIZE_UNIT : FFECOM_SIZE_UNITS);
	  ffebad_here (1, g->pad_where_line, g->pad_where_col);
	  ffebad_finish ();
	}
    }
#endif

  if (pad != 0)
    {				/* Warn about initial padding in common area. */
      char padding[20];

      sprintf (&padding[0], "%" ffetargetAlign_f "u", pad);
      ffebad_start (FFEBAD_COMMON_INIT_PAD);
      ffebad_string (ffesymbol_text (s));
      ffebad_string (padding);
      ffebad_string ((pad == 1)
		     ? FFECOM_SIZE_UNIT : FFECOM_SIZE_UNITS);
      ffebad_here (0, wl, wc);
      ffebad_finish ();
    }
}

/* Return a global for a promoted symbol (one that has heretofore
   been assumed to be local, but since discovered to be global).  */

ffeglobal
ffeglobal_promoted (ffesymbol s)
{
#if FFEGLOBAL_ENABLED
  ffename n;
  ffeglobal g;

  assert (ffesymbol_global (s) == NULL);

  n = ffename_find (ffeglobal_filewide_, ffename_token (ffesymbol_name (s)));
  g = ffename_global (n);

  return g;
#else
  return NULL;
#endif
}

/* ffeglobal_save_common -- Check SAVE status of common area

   ffesymbol s;	 // the common area
   bool save;  // TRUE if SAVEd, FALSE otherwise
   ffeglobal_save_common(s,save,ffesymbol_where_line(s),
	 ffesymbol_where_column(s));

   In global-enabled mode, make sure the save info agrees with any existing
   info established for the common area, otherwise complain.
   In global-disabled mode, do nothing.	 */

void
ffeglobal_save_common (ffesymbol s, bool save, ffewhereLine wl,
		       ffewhereColumn wc)
{
#if FFEGLOBAL_ENABLED
  ffeglobal g;

  g = ffesymbol_global (s);
  if ((g == NULL) || (g->type != FFEGLOBAL_typeCOMMON))
    return;			/* Let someone else catch this! */

  if (!g->have_save)
    {
      g->have_save = TRUE;
      g->save = save;
      g->save_where_line = ffewhere_line_use (wl);
      g->save_where_col = ffewhere_column_use (wc);
    }
  else
    {
      if ((g->save != save) && ffe_is_pedantic ())
	{
	  ffebad_start (FFEBAD_COMMON_DIFF_SAVE);
	  ffebad_string (ffesymbol_text (s));
	  ffebad_here (save ? 0 : 1, wl, wc);
	  ffebad_here (save ? 1 : 0, g->pad_where_line, g->pad_where_col);
	  ffebad_finish ();
	}
    }
#endif
}

/* ffeglobal_size_common -- Establish size of COMMON area

   ffesymbol s;	 // the common area
   long size;  // size in units
   if (ffeglobal_size_common(s,size))  // new size is largest seen

   In global-enabled mode, set the size if it current size isn't known or is
   smaller than new size, and for non-blank common, complain if old size
   is different from new.  Return TRUE if the new size is the largest seen
   for this COMMON area (or if no size was known for it previously).
   In global-disabled mode, do nothing.	 */

#if FFEGLOBAL_ENABLED
bool
ffeglobal_size_common (ffesymbol s, long size)
{
  ffeglobal g;

  g = ffesymbol_global (s);
  if ((g == NULL) || (g->type != FFEGLOBAL_typeCOMMON))
    return FALSE;

  if (!g->have_size)
    {
      g->have_size = TRUE;
      g->size = size;
      return TRUE;
    }

  if ((g->size < size) && (g->init > 0) && (g->init < ffe_count_2))
    {
      char oldsize[40];
      char newsize[40];

      sprintf (&oldsize[0], "%ld", g->size);
      sprintf (&newsize[0], "%ld", size);

      ffebad_start (FFEBAD_COMMON_ENLARGED);
      ffebad_string (ffesymbol_text (s));
      ffebad_string (oldsize);
      ffebad_string (newsize);
      ffebad_string ((g->size == 1)
		     ? FFECOM_SIZE_UNIT : FFECOM_SIZE_UNITS);
      ffebad_string ((size == 1)
		     ? FFECOM_SIZE_UNIT : FFECOM_SIZE_UNITS);
      ffebad_here (0, ffelex_token_where_line (g->initt),
		   ffelex_token_where_column (g->initt));
      ffebad_here (1, ffesymbol_where_line (s),
		   ffesymbol_where_column (s));
      ffebad_finish ();
    }
  else if ((g->size != size) && !g->blank)
    {
      char oldsize[40];
      char newsize[40];

      /* Warn about this even if not -pedantic, because putting all
	 program units in a single source file is the only way to
	 detect this.  Apparently UNIX-model linkers neither handle
	 nor report when they make a common unit smaller than
	 requested, such as when the smaller-declared version is
	 initialized and the larger-declared version is not.  So
	 if people complain about strange overwriting, we can tell
	 them to put all their code in a single file and compile
	 that way.  Warnings about differing sizes must therefore
	 always be issued.  */

      sprintf (&oldsize[0], "%ld", g->size);
      sprintf (&newsize[0], "%ld", size);

      ffebad_start (FFEBAD_COMMON_DIFF_SIZE);
      ffebad_string (ffesymbol_text (s));
      ffebad_string (oldsize);
      ffebad_string (newsize);
      ffebad_string ((g->size == 1)
		     ? FFECOM_SIZE_UNIT : FFECOM_SIZE_UNITS);
      ffebad_string ((size == 1)
		     ? FFECOM_SIZE_UNIT : FFECOM_SIZE_UNITS);
      ffebad_here (0, ffelex_token_where_line (g->t),
		   ffelex_token_where_column (g->t));
      ffebad_here (1, ffesymbol_where_line (s),
		   ffesymbol_where_column (s));
      ffebad_finish ();
    }

  if (size > g->size)
    {
      g->size = size;
      return TRUE;
    }
  return FALSE;
}

#endif
void
ffeglobal_terminate_1 ()
{
}
