/* ldcref.c -- output a cross reference table
   Copyright (C) 1996 Free Software Foundation, Inc.
   Written by Ian Lance Taylor <ian@cygnus.com>

This file is part of GLD, the Gnu Linker.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* This file holds routines that manage the cross reference table.  */

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "libiberty.h"

#include "ld.h"
#include "ldmain.h"
#include "ldmisc.h"

/* We keep an instance of this structure for each reference to a
   symbol from a given object.  */

struct cref_ref
{
  /* The next reference.  */
  struct cref_ref *next;
  /* The object.  */
  bfd *abfd;
  /* True if the symbol is defined.  */
  unsigned int def : 1;
  /* True if the symbol is common.  */
  unsigned int common : 1;
  /* True if the symbol is undefined.  */
  unsigned int undef : 1;
};

/* We keep a hash table of symbols.  Each entry looks like this.  */

struct cref_hash_entry
{
  struct bfd_hash_entry root;
  /* The demangled name.  */
  char *demangled;
  /* References to and definitions of this symbol.  */
  struct cref_ref *refs;
};

/* This is what the hash table looks like.  */

struct cref_hash_table
{
  struct bfd_hash_table root;
};

/* Local functions.  */

static struct bfd_hash_entry *cref_hash_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));
static boolean cref_fill_array PARAMS ((struct cref_hash_entry *, PTR));
static int cref_sort_array PARAMS ((const PTR, const PTR));
static void output_one_cref PARAMS ((FILE *, struct cref_hash_entry *));

/* Look up an entry in the cref hash table.  */

#define cref_hash_lookup(table, string, create, copy)		\
  ((struct cref_hash_entry *)					\
   bfd_hash_lookup (&(table)->root, (string), (create), (copy)))

/* Traverse the cref hash table.  */

#define cref_hash_traverse(table, func, info)				\
  (bfd_hash_traverse							\
   (&(table)->root,							\
    (boolean (*) PARAMS ((struct bfd_hash_entry *, PTR))) (func),	\
    (info)))

/* The cref hash table.  */

static struct cref_hash_table cref_table;

/* Whether the cref hash table has been initialized.  */

static boolean cref_initialized;

/* The number of symbols seen so far.  */

static size_t cref_symcount;

/* Create an entry in a cref hash table.  */

static struct bfd_hash_entry *
cref_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct cref_hash_entry *ret = (struct cref_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == NULL)
    ret = ((struct cref_hash_entry *)
	   bfd_hash_allocate (table, sizeof (struct cref_hash_entry)));
  if (ret == NULL)
    return (struct bfd_hash_entry *) ret;

  /* Call the allocation method of the superclass.  */
  ret = ((struct cref_hash_entry *)
	 bfd_hash_newfunc ((struct bfd_hash_entry *) ret, table, string));
  if (ret != NULL)
    {
      /* Set local fields.  */
      ret->demangled = NULL;
      ret->refs = NULL;

      /* Keep a count of the number of entries created in the hash
         table.  */
      ++cref_symcount;
    }

  return (struct bfd_hash_entry *) ret;
}

/* Add a symbol to the cref hash table.  This is called for every
   symbol that is seen during the link.  */

/*ARGSUSED*/
void
add_cref (name, abfd, section, value)
     const char *name;
     bfd *abfd;
     asection *section;
     bfd_vma value;
{
  struct cref_hash_entry *h;
  struct cref_ref *r;

  if (! cref_initialized)
    {
      if (! bfd_hash_table_init (&cref_table.root, cref_hash_newfunc))
	einfo ("%X%P: bfd_hash_table_init of cref table failed: %E\n");
      cref_initialized = true;
    }

  h = cref_hash_lookup (&cref_table, name, true, false);
  if (h == NULL)
    einfo ("%X%P: cref_hash_lookup failed: %E\n");

  for (r = h->refs; r != NULL; r = r->next)
    if (r->abfd == abfd)
      break;

  if (r == NULL)
    {
      r = (struct cref_ref *) xmalloc (sizeof *r);
      r->next = h->refs;
      h->refs = r;
      r->abfd = abfd;
      r->def = false;
      r->common = false;
      r->undef = false;
    }

  if (bfd_is_und_section (section))
    r->undef = true;
  else if (bfd_is_com_section (section))
    r->common = true;
  else
    r->def = true;
}

/* Copy the addresses of the hash table entries into an array.  This
   is called via cref_hash_traverse.  We also fill in the demangled
   name.  */

static boolean
cref_fill_array (h, data)
     struct cref_hash_entry *h;
     PTR data;
{
  struct cref_hash_entry ***pph = (struct cref_hash_entry ***) data;

  ASSERT (h->demangled == NULL);
  h->demangled = demangle (h->root.string);

  **pph = h;

  ++*pph;

  return true;
}

/* Sort an array of cref hash table entries by name.  */

static int
cref_sort_array (a1, a2)
     const PTR a1;
     const PTR a2;
{
  const struct cref_hash_entry **p1 = (const struct cref_hash_entry **) a1;
  const struct cref_hash_entry **p2 = (const struct cref_hash_entry **) a2;

  return strcmp ((*p1)->demangled, (*p2)->demangled);
}

/* Write out the cref table.  */

#define FILECOL (50)

void
output_cref (fp)
     FILE *fp;
{
  int len;
  struct cref_hash_entry **csyms, **csym_fill, **csym, **csym_end;

  fprintf (fp, "\nCross Reference Table\n\n");
  fprintf (fp, "Symbol");
  len = sizeof "Symbol" - 1;
  while (len < FILECOL)
    {
      putc (' ' , fp);
      ++len;
    }
  fprintf (fp, "File\n");

  if (! cref_initialized)
    {
      fprintf (fp, "No symbols\n");
      return;
    }

  csyms = ((struct cref_hash_entry **)
	   xmalloc (cref_symcount * sizeof (*csyms)));

  csym_fill = csyms;
  cref_hash_traverse (&cref_table, cref_fill_array, &csym_fill);
  ASSERT (csym_fill - csyms == cref_symcount);

  qsort (csyms, cref_symcount, sizeof (*csyms), cref_sort_array);

  csym_end = csyms + cref_symcount;
  for (csym = csyms; csym < csym_end; csym++)
    output_one_cref (fp, *csym);
}

/* Output one entry in the cross reference table.  */

static void
output_one_cref (fp, h)
     FILE *fp;
     struct cref_hash_entry *h;
{
  int len;
  struct bfd_link_hash_entry *hl;
  struct cref_ref *r;

  hl = bfd_link_hash_lookup (link_info.hash, h->root.string, false,
			     false, true);
  if (hl == NULL)
    einfo ("%P: symbol `%T' missing from main hash table\n",
	   h->root.string);
  else
    {
      /* If this symbol is defined in a dynamic object but never
	 referenced by a normal object, then don't print it.  */
      if (hl->type == bfd_link_hash_defined)
	{
	  if (hl->u.def.section->output_section == NULL)
	    return;
	  if ((hl->u.def.section->owner->flags & DYNAMIC) != 0)
	    {
	      for (r = h->refs; r != NULL; r = r->next)
		if ((r->abfd->flags & DYNAMIC) == 0)
		  break;
	      if (r == NULL)
		return;
	    }
	}
    }

  fprintf (fp, "%s ", h->demangled);
  len = strlen (h->demangled) + 1;

  for (r = h->refs; r != NULL; r = r->next)
    {
      if (r->def)
	{
	  while (len < FILECOL)
	    {
	      putc (' ', fp);
	      ++len;
	    }
	  finfo (fp, "%B\n", r->abfd);
	  len = 0;
	}
    }

  for (r = h->refs; r != NULL; r = r->next)
    {
      if (! r->def)
	{
	  while (len < FILECOL)
	    {
	      putc (' ', fp);
	      ++len;
	    }
	  finfo (fp, "%B\n", r->abfd);
	  len = 0;
	}
    }

  ASSERT (len == 0);
}
