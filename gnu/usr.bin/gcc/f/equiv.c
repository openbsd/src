/* equiv.c -- Implementation File (module.c template V1.0)
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
      Handles the EQUIVALENCE relationships in a program unit.

   Modifications:
*/

#define FFEEQUIV_DEBUG 0

/* Include files. */

#include "proj.h"
#include "equiv.h"
#include "bad.h"
#include "bld.h"
#include "com.h"
#include "data.h"
#include "global.h"
#include "lex.h"
#include "malloc.h"
#include "symbol.h"

/* Externals defined here. */


/* Simple definitions and enumerations. */


/* Internal typedefs. */


/* Private include files. */


/* Internal structure definitions. */

struct _ffeequiv_list_
  {
    ffeequiv first;
    ffeequiv last;
  };

/* Static objects accessed by functions in this module. */

static struct _ffeequiv_list_ ffeequiv_list_;

/* Static functions (internal). */

static void ffeequiv_layout_local_ (ffeequiv eq);
static bool ffeequiv_offset_ (ffetargetOffset *offset, ffesymbol s,
			      ffebld expr, bool subtract,
			      ffetargetOffset adjust);

/* Internal macros. */


/* ffeequiv_layout_local_ -- Lay out storage for local equivalenced vars

   ffeequiv eq;
   ffeequiv_layout_local_(eq);

   Makes a single master ffestorag object that contains all the vars
   in the equivalence, and makes subordinate ffestorag objects for the
   vars with the correct offsets.  */

static void
ffeequiv_layout_local_ (ffeequiv eq)
{
  ffesymbol s;			/* Symbol. */
  ffestorag st;			/* Equivalence storage area. */
  ffebld list;			/* List of list of equivalences. */
  ffebld item;			/* List of equivalences. */
  ffebld eqv;			/* Equivalence item. */
  ffebld root;			/* Expression for (1st) root sym (offset=0). */
  ffestorag rst;		/* Storage for root. */
  ffetargetOffset root_offset;	/* Negative offset for root. */
  ffesymbol sr;			/* Root itself. */
  ffebld var;			/* Expression for equivalence. */
  ffestorag vst;		/* Storage for var. */
  ffetargetOffset var_offset;	/* Offset for var into equiv area (from
				   root). */
  ffesymbol sv;			/* Var itself. */
  ffetargetAlign alignment;
  ffetargetAlign modulo;
  ffetargetAlign pad;
  ffetargetOffset size;
  ffetargetOffset num_elements;
  bool new_storage;		/* Established new storage info. */
  bool need_storage;		/* Have need for more storage info. */
  bool ok;
  bool init;

  assert (eq != NULL);

  if (ffeequiv_common (eq) != NULL)
    {				/* Put in common due to programmer error. */
      ffeequiv_kill (eq);
      return;
    }

  /* First find the symbol which, in the list of lists, has the reference
     with the greatest offset, which means that symbol is the root symbol (it
     will end up with an offset of zero in the equivalence area). */

  root_offset = 0;		/* Lowest possible value, to find max value. */
  sr = NULL;			/* No sym found yet. */
  ok = TRUE;

#if FFEEQUIV_DEBUG
  fprintf (stderr, "Equiv1:\n");
#endif

  for (list = ffeequiv_list (eq);
       list != NULL;
       list = ffebld_trail (list))
    {				/* For every equivalence list in the list of
				   equivs */
#if FFEEQUIV_DEBUG
      fprintf (stderr, "(");
#endif

      for (item = ffebld_head (list);
	   item != NULL;
	   item = ffebld_trail (item))
	{			/* For every equivalence item in the list */
	  eqv = ffebld_head (item);
	  s = ffeequiv_symbol (eqv);
	  if (s == NULL)
	    continue;		/* Ignore me. */

#if FFEEQUIV_DEBUG
	  fprintf (stderr, "%s,", ffesymbol_text (s));
#endif

	  assert (ffesymbol_storage (s) == NULL);	/* No storage yet. */
	  ffesymbol_set_equiv (s, NULL);	/* Equiv area slated for
						   death. */
	  if (!ffeequiv_offset_ (&var_offset, s, eqv, FALSE, 0))
	    ok = FALSE;		/* Can't calc shape of equivalence area. */
	  if ((var_offset > root_offset) || (sr == NULL))
	    {
	      root_offset = var_offset;
	      sr = s;
	    }
	}
#if FFEEQUIV_DEBUG
      fprintf (stderr, ")\n");
#endif

    }

  if (!ok || (sr == NULL))
    {
      ffeequiv_kill (eq);
      return;
    }

  /* We've got work to do, so make the LOCAL storage object that'll hold all
     the equivalenced vars inside it. */

  st = ffestorag_new (ffestorag_list_master ());
  ffestorag_set_parent (st, NULL);	/* Initializations happen here. */
  ffestorag_set_init (st, NULL);
  ffestorag_set_accretion (st, NULL);
  ffestorag_set_symbol (st, NULL);	/* LOCAL equiv collection has no
					   single sym. */
  ffestorag_set_offset (st, 0);
  ffestorag_set_alignment (st, 1);
  ffestorag_set_modulo (st, 0);
  ffestorag_set_type (st, FFESTORAG_typeLOCAL);
  ffestorag_set_basictype (st, ffesymbol_basictype (sr));
  ffestorag_set_kindtype (st, ffesymbol_kindtype (sr));
  ffestorag_set_typesymbol (st, sr);
  ffestorag_set_is_save (st, ffeequiv_is_save (eq));
  if (ffesymbol_is_save (sr))
    ffestorag_update_save (st);
  ffestorag_set_is_init (st, ffeequiv_is_init (eq));
  if (ffesymbol_is_init (sr))
    ffestorag_update_init (st);

  /* Make the EQUIV storage object for the root symbol. */

  if (ffesymbol_rank (sr) == 0)
    num_elements = 1;
  else
    num_elements = ffebld_constant_integerdefault (ffebld_conter
						(ffesymbol_arraysize (sr)));
  ffetarget_layout (ffesymbol_text (sr), &alignment, &modulo, &size,
		    ffesymbol_basictype (sr), ffesymbol_kindtype (sr),
		    ffesymbol_size (sr), num_elements);
  pad = ffetarget_align (ffestorag_ptr_to_alignment (st),
			 ffestorag_ptr_to_modulo (st), 0, alignment,
			 modulo);
  assert (pad == 0);

  rst = ffestorag_new (ffestorag_list_equivs (st));
  ffestorag_set_parent (rst, st);	/* Initializations happen there. */
  ffestorag_set_init (rst, NULL);
  ffestorag_set_accretion (rst, NULL);
  ffestorag_set_symbol (rst, sr);
  ffestorag_set_size (rst, size);
  ffestorag_set_offset (rst, 0);
  ffestorag_set_alignment (rst, alignment);
  ffestorag_set_modulo (rst, modulo);
  ffestorag_set_type (rst, FFESTORAG_typeEQUIV);
  ffestorag_set_basictype (rst, ffesymbol_basictype (sr));
  ffestorag_set_kindtype (rst, ffesymbol_kindtype (sr));
  ffestorag_set_typesymbol (rst, sr);
  ffestorag_set_is_save (rst, FALSE);	/* Assume FALSE, then... */
  if (ffestorag_is_save (st))	/* ...update to TRUE if needed. */
    ffestorag_update_save (rst);
  ffestorag_set_is_init (rst, FALSE);	/* Assume FALSE, then... */
  if (ffestorag_is_init (st))	/* ...update to TRUE if needed. */
    ffestorag_update_init (rst);
  ffestorag_set_size (st, size);
  ffesymbol_set_storage (sr, rst);
  ffesymbol_signal_unreported (sr);
  init = ffesymbol_is_init (sr);

  /* Now that we know the root (offset=0) symbol, revisit all the lists and
     do the actual storage allocation.	Keep doing this until we've gone
     through them all without making any new storage objects. */

#if FFEEQUIV_DEBUG
  fprintf (stderr, "Equiv2:\n");
#endif

  do
    {
#if FFEEQUIV_DEBUG
      fprintf (stderr, "  Equiv3:\n");
#endif

      new_storage = FALSE;
      need_storage = FALSE;
      for (list = ffeequiv_list (eq);
	   list != NULL;
	   list = ffebld_trail (list))
	{			/* For every equivalence list in the list of
				   equivs */
#if FFEEQUIV_DEBUG
	  fprintf (stderr, "  (");
#endif

	  root_offset = 0;
	  sr = NULL;
	  root = NULL;
	  for (item = ffebld_head (list);
	       item != NULL;
	       item = ffebld_trail (item))
	    {			/* For every equivalence item in the list */
	      var = ffebld_head (item);
	      sv = ffeequiv_symbol (var);
	      if (sv == NULL)
		continue;	/* Ignore me. */

#if FFEEQUIV_DEBUG
	      fprintf (stderr, "%s,", ffesymbol_text (sv));
#endif

	      need_storage = TRUE;	/* Somebody is likely to need
					   storage. */
	      if ((vst = ffesymbol_storage (sv)) == NULL)
		continue;	/* No storage for this guy, try another. */

	      ffeequiv_offset_ (&var_offset, sv, var, FALSE,
				ffestorag_offset (vst));
	      if ((var_offset > root_offset) || (sr == NULL))
		{
		  root = var;
		  root_offset = var_offset;
		  sr = sv;
		  rst = vst;
		}
	    }
	  if (sr == NULL)	/* No storage to go on, try later. */
	    {
#if FFEEQUIV_DEBUG
	      fprintf (stderr, ")\n");
#endif
	      continue;
	    }

#if FFEEQUIV_DEBUG
	  fprintf (stderr, ") %s:\n  (", ffesymbol_text (sr));
#endif

	  /* We now know the root symbol/expr and the operating offset of
	     that root into the equivalence area.  The other expressions in
	     the list all identify an initial storage unit that must have the
	     same offset. */

	  for (item = ffebld_head (list);
	       item != NULL;
	       item = ffebld_trail (item))
	    {			/* For every equivalence item in the list */
	      var = ffebld_head (item);
	      sv = ffeequiv_symbol (var);
	      if (sv == NULL)
		continue;	/* Except erroneous stuff (opANY). */
	      if (var == root)
		{
		  /* The last root symbol we see must therefore
		     (by static deduction) be the first-listed "rooted" item
		     in the EQUIVALENCE statements pertaining to this area.  */
		  ffestorag_set_symbol (st, sv);
		  continue;	/* Root sym already set up. */
		}

	      if (!ffeequiv_offset_ (&var_offset, sv, var, TRUE, root_offset))
		continue;	/* Attempt to start sym prior to equiv area! */

#if FFEEQUIV_DEBUG
	      fprintf (stderr, "%s:%ld,", ffesymbol_text (sv),
		       (long) var_offset);
#endif

	      if (ffesymbol_rank (sv) == 0)
		num_elements = 1;
	      else
		num_elements = ffebld_constant_integerdefault (ffebld_conter
						(ffesymbol_arraysize (sv)));
	      ffetarget_layout (ffesymbol_text (sv), &alignment, &modulo,
				&size, ffesymbol_basictype (sv),
				ffesymbol_kindtype (sv), ffesymbol_size (sv),
				num_elements);
	      pad = ffetarget_align (ffestorag_ptr_to_alignment (st),
				     ffestorag_ptr_to_modulo (st),
				     var_offset, alignment, modulo);
	      if (pad != 0)
		{
		  ffebad_start (FFEBAD_EQUIV_ALIGN);
		  ffebad_string (ffesymbol_text (sv));
		  ffebad_finish ();
		  continue;
		}

	      /* The last symbol we see with a zero offset must therefore
		 (by static deduction) be the first-listed "rooted" item
		 in the EQUIVALENCE statements pertaining to this area.  */
	      if (var_offset == 0)
		ffestorag_set_symbol (st, sv);

	      if ((vst = ffesymbol_storage (sv)) == NULL)
		{		/* Create new ffestorag object, extend equiv
				   area. */
		  new_storage = TRUE;
		  vst = ffestorag_new (ffestorag_list_equivs (st));
		  ffestorag_set_parent (vst, st);	/* Initializations
							   happen there. */
		  ffestorag_set_init (vst, NULL);
		  ffestorag_set_accretion (vst, NULL);
		  ffestorag_set_symbol (vst, sv);
		  ffestorag_set_size (vst, size);
		  ffestorag_set_offset (vst, var_offset);
		  ffestorag_set_alignment (vst, alignment);
		  ffestorag_set_modulo (vst, modulo);
		  ffestorag_set_type (vst, FFESTORAG_typeEQUIV);
		  ffestorag_set_basictype (vst, ffesymbol_basictype (sv));
		  ffestorag_set_kindtype (vst, ffesymbol_kindtype (sv));
		  ffestorag_set_typesymbol (vst, sv);
		  ffestorag_set_is_save (vst, FALSE);	/* Assume FALSE... */
		  if (ffestorag_is_save (st))	/* ...update TRUE */
		    ffestorag_update_save (vst);	/* if needed. */
		  ffestorag_set_is_init (vst, FALSE);	/* Assume FALSE... */
		  if (ffestorag_is_init (st))	/* ...update TRUE */
		    ffestorag_update_init (vst);	/* if needed. */
		  if (!ffetarget_offset_add (&size, var_offset, size))
		    /* Find one size of equiv area, complain if overflow. */
		    ffetarget_offset_overflow (ffesymbol_text (s));
		  else if (size > ffestorag_size (st))
		    /* Extend equiv area if necessary. */
		    ffestorag_set_size (st, size);
		  ffesymbol_set_storage (sv, vst);
		  ffesymbol_signal_unreported (sv);
		  ffestorag_update (st, sv, ffesymbol_basictype (sv),
				    ffesymbol_kindtype (sv));
		  if (ffesymbol_is_init (sv))
		    init = TRUE;
		}
	      else
		{
		  /* Make sure offset agrees with known offset. */
		  if (var_offset != ffestorag_offset (vst))
		    {
		      ffebad_start (FFEBAD_EQUIV_MISMATCH);
		      ffebad_string (ffesymbol_text (sv));
		      ffebad_finish ();
		    }
		}
	    }			/* (For every equivalence item in the list) */
#if FFEEQUIV_DEBUG
	  fprintf (stderr, ")\n");
#endif
	  ffebld_set_head (list, NULL);	/* Don't do this list again. */
	}			/* (For every equivalence list in the list of
				   equivs) */
    } while (new_storage && need_storage);

  ffeequiv_kill (eq);		/* Fully processed, no longer needed. */

  if (init)
    ffedata_gather (st);	/* Gather subordinate inits into one init. */
}

/* ffeequiv_offset_ -- Determine offset from start of symbol

   ffetargetOffset offset;
   ffesymbol s;	 // Symbol for error reporting.
   ffebld expr;	 // opSUBSTR, opARRAYREF, opSYMTER, opANY.
   bool subtract;  // FALSE means add to adjust, TRUE means subtract from it.
   ffetargetOffset adjust;  // Helps keep answer in pos range (unsigned).
   if (!ffeequiv_offset_(&offset,s,expr,subtract,adjust))
       // error doing the calculation, message already printed

   Returns the offset represented by the SUBSTR, ARRAYREF, or SUBSTR/ARRAYREF
   combination added-to/subtracted-from the adjustment specified.  If there
   is an error of some kind, returns FALSE, else returns TRUE.	Note that
   only the first storage unit specified is considered; A(1:1) and A(1:2000)
   have the same first storage unit and so return the same offset.  */

static bool
ffeequiv_offset_ (ffetargetOffset *offset, ffesymbol s UNUSED,
		  ffebld expr, bool subtract, ffetargetOffset adjust)
{
  ffetargetIntegerDefault value = 0;
  ffetargetOffset cval;		/* Converted value. */
  ffesymbol sym;

  if (expr == NULL)
    return FALSE;

again:				/* :::::::::::::::::::: */

  switch (ffebld_op (expr))
    {
    case FFEBLD_opANY:
      return FALSE;

    case FFEBLD_opSYMTER:
      {
	ffetargetOffset size;	/* Size of a single unit. */
	ffetargetAlign a;	/* Ignored. */
	ffetargetAlign m;	/* Ignored. */

	sym = ffebld_symter (expr);
	if (ffesymbol_basictype (sym) == FFEINFO_basictypeANY)
	  return FALSE;

	if (value < 0)
	  {			/* Really invalid, as in A(-2:5), but in case
				   it's wanted.... */
	    if (!ffetarget_offset (&cval, -value))
	      return FALSE;
	    if (subtract)
	      return ffetarget_offset_add (offset, cval, adjust);

	    if (cval > adjust)
	      {
	      neg:		/* :::::::::::::::::::: */
		ffebad_start (FFEBAD_COMMON_NEG);
		ffebad_string (ffesymbol_text (sym));
		ffebad_finish ();
		return FALSE;
	      }
	    *offset = adjust - cval;
	    return TRUE;
	  }

	if (!ffetarget_offset (&cval, value))
	  return FALSE;

	ffetarget_layout (ffesymbol_text (sym), &a, &m, &size,
			  ffesymbol_basictype (sym),
			  ffesymbol_kindtype (sym), 1, 1);

	if (!ffetarget_offset_multiply (&cval, cval, size))
	  return FALSE;

	if (subtract)
	  if (cval > adjust)
	    goto neg;		/* :::::::::::::::::::: */
	  else
	    *offset = adjust - cval;
	else if (!ffetarget_offset_add (offset, cval, adjust))
	  return FALSE;
	return TRUE;
      }

    case FFEBLD_opARRAYREF:
      {
	ffebld symexp = ffebld_left (expr);
	ffebld subscripts = ffebld_right (expr);
	ffebld dims;
	ffetargetIntegerDefault width;
	ffetargetIntegerDefault arrayval;
	ffetargetIntegerDefault lowbound;
	ffetargetIntegerDefault highbound;
	ffebld subscript;
	ffebld dim;
	ffebld low;
	ffebld high;
	int rank = 0;

	if (ffebld_op (symexp) != FFEBLD_opSYMTER)
	  return FALSE;

	sym = ffebld_symter (symexp);
	if (ffesymbol_basictype (sym) == FFEINFO_basictypeANY)
	  return FALSE;

	if (ffesymbol_size (sym) == FFETARGET_charactersizeNONE)
	  width = 1;
	else
	  width = ffesymbol_size (sym);
	dims = ffesymbol_dims (sym);

	while (subscripts != NULL)
	  {
	    ++rank;
	    if (dims == NULL)
	      {
		ffebad_start (FFEBAD_EQUIV_MANY);
		ffebad_string (ffesymbol_text (sym));
		ffebad_finish ();
		return FALSE;
	      }

	    subscript = ffebld_head (subscripts);
	    dim = ffebld_head (dims);

	    assert (ffebld_op (subscript) == FFEBLD_opCONTER);
	    assert (ffeinfo_basictype (ffebld_info (subscript))
		    == FFEINFO_basictypeINTEGER);
	    assert (ffeinfo_kindtype (ffebld_info (subscript))
		    == FFEINFO_kindtypeINTEGERDEFAULT);
	    arrayval = ffebld_constant_integerdefault (ffebld_conter
						       (subscript));

	    assert (ffebld_op (dim) == FFEBLD_opBOUNDS);
	    low = ffebld_left (dim);
	    high = ffebld_right (dim);

	    if (low == NULL)
	      lowbound = 1;
	    else
	      {
		assert (ffeinfo_basictype (ffebld_info (low))
			== FFEINFO_basictypeINTEGER);
		assert (ffeinfo_kindtype (ffebld_info (low))
			== FFEINFO_kindtypeINTEGERDEFAULT);
		lowbound
		  = ffebld_constant_integerdefault (ffebld_conter (low));
	      }

	    assert (ffebld_op (high) == FFEBLD_opCONTER);
	    assert (ffeinfo_basictype (ffebld_info (high))
		    == FFEINFO_basictypeINTEGER);
	    assert (ffeinfo_kindtype (ffebld_info (high))
		    == FFEINFO_kindtypeINTEGER1);
	    highbound
	      = ffebld_constant_integerdefault (ffebld_conter (high));

	    if ((arrayval < lowbound) || (arrayval > highbound))
	      {
		char rankstr[10];

		sprintf (rankstr, "%d", rank);
		ffebad_start (FFEBAD_EQUIV_SUBSCRIPT);
		ffebad_string (ffesymbol_text (sym));
		ffebad_string (rankstr);
		ffebad_finish ();
	      }

	    subscripts = ffebld_trail (subscripts);
	    dims = ffebld_trail (dims);

	    value += width * (arrayval - lowbound);
	    if (subscripts != NULL)
	      width *= highbound - lowbound + 1;
	  }

	if (dims != NULL)
	  {
	    ffebad_start (FFEBAD_EQUIV_FEW);
	    ffebad_string (ffesymbol_text (sym));
	    ffebad_finish ();
	    return FALSE;
	  }

	expr = symexp;
      }
      goto again;		/* :::::::::::::::::::: */

    case FFEBLD_opSUBSTR:
      {
	ffebld begin = ffebld_head (ffebld_right (expr));

	expr = ffebld_left (expr);
	if (ffebld_op (expr) == FFEBLD_opARRAYREF)
	  sym = ffebld_symter (ffebld_left (expr));
	else if (ffebld_op (expr) == FFEBLD_opSYMTER)
	  sym = ffebld_symter (expr);
	else
	  sym = NULL;

	if ((sym != NULL)
	    && (ffesymbol_basictype (sym) == FFEINFO_basictypeANY))
	  return FALSE;

	if (begin == NULL)
	  value = 0;
	else
	  {
	    assert (ffebld_op (begin) == FFEBLD_opCONTER);
	    assert (ffeinfo_basictype (ffebld_info (begin))
		    == FFEINFO_basictypeINTEGER);
	    assert (ffeinfo_kindtype (ffebld_info (begin))
		    == FFEINFO_kindtypeINTEGERDEFAULT);

	    value = ffebld_constant_integerdefault (ffebld_conter (begin));

	    if ((value < 1)
		|| ((sym != NULL)
		    && (value > ffesymbol_size (sym))))
	      {
		ffebad_start (FFEBAD_EQUIV_RANGE);
		ffebad_string (ffesymbol_text (sym));
		ffebad_finish ();
	      }

	    --value;
	  }
	if ((sym != NULL)
	    && (ffesymbol_basictype (sym) != FFEINFO_basictypeCHARACTER))
	  {
	    ffebad_start (FFEBAD_EQUIV_SUBSTR);
	    ffebad_string (ffesymbol_text (sym));
	    ffebad_finish ();
	    value = 0;
	  }
      }
      goto again;		/* :::::::::::::::::::: */

    default:
      assert ("bad op" == NULL);
      return FALSE;
    }

}

/* ffeequiv_add -- Add list of equivalences to list of lists for eq object

   ffeequiv eq;
   ffebld list;
   ffelexToken t;  // points to first item in equivalence list
   ffeequiv_add(eq,list,t);

   Check the list to make sure only one common symbol is involved (even
   if multiple times) and agrees with the common symbol for the equivalence
   object (or it has no common symbol until now).  Prepend (aka append, it
   doesn't matter) the list to the list of lists for the equivalence object.
   Otherwise report an error and return.  */

void
ffeequiv_add (ffeequiv eq, ffebld list, ffelexToken t)
{
  ffebld item;
  ffesymbol symbol;

  for (item = list; item != NULL; item = ffebld_trail (item))
    {
      symbol = ffeequiv_symbol (ffebld_head (item));

      if (ffesymbol_equiv (symbol) == NULL)
	ffesymbol_set_equiv (symbol, eq);
      else
	assert (ffesymbol_equiv (symbol) == eq);

      if (ffesymbol_common (symbol) == NULL)	/* Is symbol in a COMMON
						   area? */
	{			/* No (at least not yet). */
	  if (ffesymbol_is_save (symbol))
	    ffeequiv_update_save (eq);	/* EQUIVALENCE has >=1 SAVEd entity. */
	  if (ffesymbol_is_init (symbol))
	    ffeequiv_update_init (eq);	/* EQUIVALENCE has >=1 init'd entity. */
	  continue;		/* Nothing more to do here. */
	}

#if FFEGLOBAL_ENABLED
      if (ffesymbol_is_init (symbol))
	ffeglobal_init_common (ffesymbol_common (symbol), t);
#endif

      if (ffesymbol_is_save (ffesymbol_common (symbol)))
	ffeequiv_update_save (eq);	/* EQUIVALENCE is in a SAVEd COMMON
					   block. */
      if (ffesymbol_is_init (ffesymbol_common (symbol)))
	ffeequiv_update_init (eq);	/* EQUIVALENCE is in a init'd COMMON
					   block. */

      if (ffeequiv_common (eq) == NULL)	/* Is COMMON involved already? */
	/* No, but there is now. */
	ffeequiv_set_common (eq, ffesymbol_common (symbol));
      else if (ffeequiv_common (eq) != ffesymbol_common (symbol))
	{
	  /* Yes, and it isn't the same as our new COMMON area. */
	  ffebad_start (FFEBAD_EQUIV_COMMON);
	  ffebad_here (0, ffelex_token_where_line (t), ffelex_token_where_column (t));
	  ffebad_string (ffesymbol_text (ffeequiv_common (eq)));
	  ffebad_string (ffesymbol_text (ffesymbol_common (symbol)));
	  ffebad_finish ();
	  return;
	}
    }

  ffeequiv_set_list (eq, ffebld_new_item (list, ffeequiv_list (eq)));
}

/* ffeequiv_dump -- Dump info on equivalence object

   ffeequiv eq;
   ffeequiv_dump(eq);  */

void
ffeequiv_dump (ffeequiv eq)
{
  if (ffeequiv_common (eq) != NULL)
    fprintf (dmpout, "(common %s) ", ffesymbol_text (ffeequiv_common (eq)));
  ffebld_dump (ffeequiv_list (eq));
}

/* ffeequiv_exec_transition -- Do the hard work on all the equivalence objects

   ffeequiv_exec_transition();	*/

void
ffeequiv_exec_transition ()
{
  while (ffeequiv_list_.first != (ffeequiv) &ffeequiv_list_.first)
    ffeequiv_layout_local_ (ffeequiv_list_.first);
}

/* ffeequiv_init_2 -- Initialize for new program unit

   ffeequiv_init_2();

   Initializes the list of equivalences.  */

void
ffeequiv_init_2 ()
{
  ffeequiv_list_.first = (ffeequiv) &ffeequiv_list_.first;
  ffeequiv_list_.last = (ffeequiv) &ffeequiv_list_.first;
}

/* ffeequiv_kill -- Kill equivalence object after removing from list

   ffeequiv eq;
   ffeequiv_kill(eq);

   Removes equivalence object from master list, then kills it.	*/

void
ffeequiv_kill (ffeequiv victim)
{
  victim->next->previous = victim->previous;
  victim->previous->next = victim->next;
  malloc_kill_ks (ffe_pool_program_unit (), victim, sizeof (*victim));
}

/* ffeequiv_layout_cblock -- Lay out storage for common area

   ffestorag st;
   if (ffeequiv_layout_cblock(st))
       // at least one equiv'd symbol has init/accretion expr.

   Now that the explicitly COMMONed variables in the common area (whose
   ffestorag object is passed) have been laid out, lay out the storage
   for all variables equivalenced into the area by making subordinate
   ffestorag objects for them.	*/

bool
ffeequiv_layout_cblock (ffestorag st)
{
  ffesymbol s = ffestorag_symbol (st);	/* CBLOCK symbol. */
  ffebld list;			/* List of explicit common vars, in order, in
				   s. */
  ffebld item;			/* List of list of equivalences in a given
				   explicit common var. */
  ffebld root;			/* Expression for (1st) explicit common var
				   in list of eqs. */
  ffestorag rst;		/* Storage for root. */
  ffetargetOffset root_offset;	/* Offset for root into common area. */
  ffesymbol sr;			/* Root itself. */
  ffeequiv seq;			/* Its equivalence object, if any. */
  ffebld var;			/* Expression for equivalence. */
  ffestorag vst;		/* Storage for var. */
  ffetargetOffset var_offset;	/* Offset for var into common area. */
  ffesymbol sv;			/* Var itself. */
  ffebld altroot;		/* Alternate root. */
  ffesymbol altrootsym;		/* Alternate root symbol. */
  ffetargetAlign alignment;
  ffetargetAlign modulo;
  ffetargetAlign pad;
  ffetargetOffset size;
  ffetargetOffset num_elements;
  bool new_storage;		/* Established new storage info. */
  bool need_storage;		/* Have need for more storage info. */
  bool ok;
  bool init = FALSE;

  assert (st != NULL);
  assert (ffestorag_type (st) == FFESTORAG_typeCBLOCK);
  assert (ffesymbol_kind (ffestorag_symbol (st)) == FFEINFO_kindCOMMON);

  for (list = ffesymbol_commonlist (ffestorag_symbol (st));
       list != NULL;
       list = ffebld_trail (list))
    {				/* For every variable in the common area */
      assert (ffebld_op (ffebld_head (list)) == FFEBLD_opSYMTER);
      sr = ffebld_symter (ffebld_head (list));
      if ((seq = ffesymbol_equiv (sr)) == NULL)
	continue;		/* No equivalences to process. */
      rst = ffesymbol_storage (sr);
      if (rst == NULL)
	{
	  assert (ffesymbol_kind (sr) == FFEINFO_kindANY);
	  continue;
	}
      ffesymbol_set_equiv (sr, NULL);	/* Cancel ref to equiv obj. */
      do
	{
	  new_storage = FALSE;
	  need_storage = FALSE;
	  for (item = ffeequiv_list (seq);	/* Get list of equivs. */
	       item != NULL;
	       item = ffebld_trail (item))
	    {			/* For every eqv list in the list of equivs
				   for the variable */
	      altroot = NULL;
	      altrootsym = NULL;
	      for (root = ffebld_head (item);
		   root != NULL;
		   root = ffebld_trail (root))
		{		/* For every equivalence item in the list */
		  sv = ffeequiv_symbol (ffebld_head (root));
		  if (sv == sr)
		    break;	/* Found first mention of "rooted" symbol. */
		  if (ffesymbol_storage (sv) != NULL)
		    {
		      altroot = root;	/* If no mention, use this guy
					   instead. */
		      altrootsym = sv;
		    }
		}
	      if (root != NULL)
		{
		  root = ffebld_head (root);	/* Lose its opITEM. */
		  ok = ffeequiv_offset_ (&root_offset, sr, root, FALSE,
					 ffestorag_offset (rst));
		  /* Equiv point prior to start of common area? */
		}
	      else if (altroot != NULL)
		{
		  /* Equiv point prior to start of common area? */
		  root = ffebld_head (altroot);
		  ok = ffeequiv_offset_ (&root_offset, altrootsym, root,
					 FALSE,
			 ffestorag_offset (ffesymbol_storage (altrootsym)));
		  ffesymbol_set_equiv (altrootsym, NULL);
		}
	      else
		/* No rooted symbol in list of equivalences! */
		{		/* Assume this was due to opANY and ignore
				   this list for now. */
		  need_storage = TRUE;
		  continue;
		}

	      /* We now know the root symbol and the operating offset of that
		 root into the common area.  The other expressions in the
		 list all identify an initial storage unit that must have the
		 same offset. */

	      for (var = ffebld_head (item);
		   var != NULL;
		   var = ffebld_trail (var))
		{		/* For every equivalence item in the list */
		  if (ffebld_head (var) == root)
		    continue;	/* Except root, of course. */
		  sv = ffeequiv_symbol (ffebld_head (var));
		  if (sv == NULL)
		    continue;	/* Except erroneous stuff (opANY). */
		  ffesymbol_set_equiv (sv, NULL);	/* Don't need this ref
							   anymore. */
		  if (!ok
		      || !ffeequiv_offset_ (&var_offset, sv,
					    ffebld_head (var), TRUE,
					    root_offset))
		    continue;	/* Can't do negative offset wrt COMMON. */

		  if (ffesymbol_rank (sv) == 0)
		    num_elements = 1;
		  else
		    num_elements = ffebld_constant_integerdefault
		      (ffebld_conter (ffesymbol_arraysize (sv)));
		  ffetarget_layout (ffesymbol_text (sv), &alignment,
				    &modulo, &size,
				    ffesymbol_basictype (sv),
				    ffesymbol_kindtype (sv),
				    ffesymbol_size (sv), num_elements);
		  pad = ffetarget_align (ffestorag_ptr_to_alignment (st),
					 ffestorag_ptr_to_modulo (st),
					 var_offset, alignment, modulo);
		  if (pad != 0)
		    {
		      ffebad_start (FFEBAD_EQUIV_ALIGN);
		      ffebad_string (ffesymbol_text (sv));
		      ffebad_finish ();
		      continue;
		    }

		  if ((vst = ffesymbol_storage (sv)) == NULL)
		    {		/* Create new ffestorag object, extend
				   cblock. */
		      new_storage = TRUE;
		      vst = ffestorag_new (ffestorag_list_equivs (st));
		      ffestorag_set_parent (vst, st);	/* Initializations
							   happen there. */
		      ffestorag_set_init (vst, NULL);
		      ffestorag_set_accretion (vst, NULL);
		      ffestorag_set_symbol (vst, sv);
		      ffestorag_set_size (vst, size);
		      ffestorag_set_offset (vst, var_offset);
		      ffestorag_set_alignment (vst, alignment);
		      ffestorag_set_modulo (vst, modulo);
		      ffestorag_set_type (vst, FFESTORAG_typeEQUIV);
		      ffestorag_set_basictype (vst, ffesymbol_basictype (sv));
		      ffestorag_set_kindtype (vst, ffesymbol_kindtype (sv));
		      ffestorag_set_typesymbol (vst, sv);
		      ffestorag_set_is_save (vst, FALSE);	/* Assume FALSE... */
		      if (ffestorag_is_save (st))	/* ...update TRUE */
			ffestorag_update_save (vst);	/* if needed. */
		      ffestorag_set_is_init (vst, FALSE);	/* Assume FALSE... */
		      if (ffestorag_is_init (st))	/* ...update TRUE */
			ffestorag_update_init (vst);	/* if needed. */
		      if (!ffetarget_offset_add (&size, var_offset, size))
			/* Find one size of common block, complain if
			   overflow. */
			ffetarget_offset_overflow (ffesymbol_text (s));
		      else if (size > ffestorag_size (st))
			/* Extend common. */
			ffestorag_set_size (st, size);
		      ffesymbol_set_storage (sv, vst);
		      ffesymbol_set_common (sv, s);
		      ffesymbol_signal_unreported (sv);
		      ffestorag_update (st, sv, ffesymbol_basictype (sv),
					ffesymbol_kindtype (sv));
		      if (ffesymbol_is_init (sv))
			init = TRUE;
		    }
		  else
		    {
		      /* Make sure offset agrees with known offset. */
		      if (var_offset != ffestorag_offset (vst))
			{
			  ffebad_start (FFEBAD_EQUIV_MISMATCH);
			  ffebad_string (ffesymbol_text (sv));
			  ffebad_finish ();
			}
		    }
		}		/* (For every equivalence item in the list) */
	    }			/* (For every eqv list in the list of equivs
				   for the variable) */
	}
      while (new_storage && need_storage);

      ffeequiv_kill (seq);	/* Kill equiv obj. */
    }				/* (For every variable in the common area) */

  return init;
}

/* ffeequiv_merge -- Merge two equivalence objects, return the merged result

   ffeequiv eq1;
   ffeequiv eq2;
   ffelexToken t;  // points to current equivalence item forcing the merge.
   eq1 = ffeequiv_merge(eq1,eq2,t);

   If the two equivalence objects can be merged, they are, all the
   ffesymbols in their lists of lists are adjusted to point to the merged
   equivalence object, and the merged object is returned.

   Otherwise, the two equivalence objects have different non-NULL common
   symbols, so the merge cannot take place.  An error message is issued and
   NULL is returned.  */

ffeequiv
ffeequiv_merge (ffeequiv eq1, ffeequiv eq2, ffelexToken t)
{
  ffebld list;
  ffebld eqs;
  ffesymbol symbol;
  ffebld last = NULL;

  /* If both equivalence objects point to different common-based symbols,
     complain.	Of course, one or both might have NULL common symbols now,
     and get COMMONed later, but the COMMON statement handler checks for
     this. */

  if ((ffeequiv_common (eq1) != NULL) && (ffeequiv_common (eq2) != NULL)
      && (ffeequiv_common (eq1) != ffeequiv_common (eq2)))
    {
      ffebad_start (FFEBAD_EQUIV_COMMON);
      ffebad_here (0, ffelex_token_where_line (t), ffelex_token_where_column (t));
      ffebad_string (ffesymbol_text (ffeequiv_common (eq1)));
      ffebad_string (ffesymbol_text (ffeequiv_common (eq2)));
      ffebad_finish ();
      return NULL;
    }

  /* Make eq1 the new, merged object (arbitrarily). */

  if (ffeequiv_common (eq1) == NULL)
    ffeequiv_set_common (eq1, ffeequiv_common (eq2));

  /* If the victim object has any init'ed entities, so does the new object. */

  if (eq2->is_init)
    eq1->is_init = TRUE;

#if FFEGLOBAL_ENABLED
  if (eq1->is_init && (ffeequiv_common (eq1) != NULL))
    ffeglobal_init_common (ffeequiv_common (eq1), t);
#endif

  /* If the victim object has any SAVEd entities, then the new object has
     some. */

  if (ffeequiv_is_save (eq2))
    ffeequiv_update_save (eq1);

  /* If the victim object has any init'd entities, then the new object has
     some. */

  if (ffeequiv_is_init (eq2))
    ffeequiv_update_init (eq1);

  /* Adjust all the symbols in the list of lists of equivalences for the
     victim equivalence object so they point to the new merged object
     instead. */

  for (list = ffeequiv_list (eq2); list != NULL; list = ffebld_trail (list))
    {
      for (eqs = ffebld_head (list); eqs != NULL; eqs = ffebld_trail (eqs))
	{
	  symbol = ffeequiv_symbol (ffebld_head (eqs));
	  if (ffesymbol_equiv (symbol) == eq2)
	    ffesymbol_set_equiv (symbol, eq1);
	  else
	    assert (ffesymbol_equiv (symbol) == eq1);	/* Can see a sym > once. */
	}

      /* For convenience, remember where the last ITEM in the outer list is. */

      if (ffebld_trail (list) == NULL)
	{
	  last = list;
	  break;
	}
    }

  /* Append the list of lists in the new, merged object to the list of lists
     in the victim object, then use the new combined list in the new merged
     object. */

  ffebld_set_trail (last, ffeequiv_list (eq1));
  ffeequiv_set_list (eq1, ffeequiv_list (eq2));

  /* Unlink and kill the victim object. */

  ffeequiv_kill (eq2);

  return eq1;			/* Return the new merged object. */
}

/* ffeequiv_new -- Create new equivalence object, put in list

   ffeequiv eq;
   eq = ffeequiv_new();

   Creates a new equivalence object and adds it to the list of equivalence
   objects.  */

ffeequiv
ffeequiv_new ()
{
  ffeequiv eq;

  eq = malloc_new_ks (ffe_pool_program_unit (), "ffeequiv", sizeof (*eq));
  eq->next = (ffeequiv) &ffeequiv_list_.first;
  eq->previous = ffeequiv_list_.last;
  ffeequiv_set_common (eq, NULL);	/* No COMMON area yet. */
  ffeequiv_set_list (eq, NULL);	/* No list of lists of equivalences yet. */
  ffeequiv_set_is_save (eq, FALSE);
  ffeequiv_set_is_init (eq, FALSE);
  eq->next->previous = eq;
  eq->previous->next = eq;

  return eq;
}

/* ffeequiv_symbol -- Return symbol for equivalence expression

   ffesymbol symbol;
   ffebld expr;
   symbol = ffeequiv_symbol(expr);

   Finds the terminal SYMTER in an equivalence expression and returns the
   ffesymbol for it.  */

ffesymbol
ffeequiv_symbol (ffebld expr)
{
  assert (expr != NULL);

again:				/* :::::::::::::::::::: */

  switch (ffebld_op (expr))
    {
    case FFEBLD_opARRAYREF:
    case FFEBLD_opSUBSTR:
      expr = ffebld_left (expr);
      goto again;		/* :::::::::::::::::::: */

    case FFEBLD_opSYMTER:
      return ffebld_symter (expr);

    case FFEBLD_opANY:
      return NULL;

    default:
      assert ("bad eq expr" == NULL);
      return NULL;
    }
}

/* ffeequiv_update_init -- Update the INIT flag for the area to TRUE

   ffeequiv eq;
   ffeequiv_update_init(eq);

   If the INIT flag for the <eq> object is already set, return.	 Else,
   set it TRUE and call ffe*_update_init for all objects contained in
   this one.  */

void
ffeequiv_update_init (ffeequiv eq)
{
  ffebld list;			/* Current list in list of lists. */
  ffebld item;			/* Current item in current list. */
  ffebld expr;			/* Expression in head of current item. */

  if (eq->is_init)
    return;

  eq->is_init = TRUE;

  if ((eq->common != NULL)
      && !ffesymbol_is_init (eq->common))
    ffesymbol_update_init (eq->common);	/* Shouldn't be needed. */

  for (list = eq->list; list != NULL; list = ffebld_trail (list))
    {
      for (item = ffebld_head (list); item != NULL; item = ffebld_trail (item))
	{
	  expr = ffebld_head (item);

	again:			/* :::::::::::::::::::: */

	  switch (ffebld_op (expr))
	    {
	    case FFEBLD_opANY:
	      break;

	    case FFEBLD_opSYMTER:
	      if (!ffesymbol_is_init (ffebld_symter (expr)))
		ffesymbol_update_init (ffebld_symter (expr));
	      break;

	    case FFEBLD_opARRAYREF:
	      expr = ffebld_left (expr);
	      goto again;	/* :::::::::::::::::::: */

	    case FFEBLD_opSUBSTR:
	      expr = ffebld_left (expr);
	      goto again;	/* :::::::::::::::::::: */

	    default:
	      assert ("bad op for ffeequiv_update_init" == NULL);
	      break;
	    }
	}
    }
}

/* ffeequiv_update_save -- Update the SAVE flag for the area to TRUE

   ffeequiv eq;
   ffeequiv_update_save(eq);

   If the SAVE flag for the <eq> object is already set, return.	 Else,
   set it TRUE and call ffe*_update_save for all objects contained in
   this one.  */

void
ffeequiv_update_save (ffeequiv eq)
{
  ffebld list;			/* Current list in list of lists. */
  ffebld item;			/* Current item in current list. */
  ffebld expr;			/* Expression in head of current item. */

  if (eq->is_save)
    return;

  eq->is_save = TRUE;

  if ((eq->common != NULL)
      && !ffesymbol_is_save (eq->common))
    ffesymbol_update_save (eq->common);	/* Shouldn't be needed. */

  for (list = eq->list; list != NULL; list = ffebld_trail (list))
    {
      for (item = ffebld_head (list); item != NULL; item = ffebld_trail (item))
	{
	  expr = ffebld_head (item);

	again:			/* :::::::::::::::::::: */

	  switch (ffebld_op (expr))
	    {
	    case FFEBLD_opANY:
	      break;

	    case FFEBLD_opSYMTER:
	      if (!ffesymbol_is_save (ffebld_symter (expr)))
		ffesymbol_update_save (ffebld_symter (expr));
	      break;

	    case FFEBLD_opARRAYREF:
	      expr = ffebld_left (expr);
	      goto again;	/* :::::::::::::::::::: */

	    case FFEBLD_opSUBSTR:
	      expr = ffebld_left (expr);
	      goto again;	/* :::::::::::::::::::: */

	    default:
	      assert ("bad op for ffeequiv_update_save" == NULL);
	      break;
	    }
	}
    }
}
