/* Copyright (C) 1991, 1992 Free Software Foundation, Inc.
This file is part of the GNU C Library.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include "mmprivate.h"

PTR
mmemalign (md, alignment, size)
  PTR md;
  size_t alignment;
  size_t size;
{
  PTR result;
  unsigned long int adj;
  struct alignlist *l;
  struct mdesc *mdp;

  if ((result = mmalloc (md, size + alignment - 1)) != NULL)
    {
      adj = RESIDUAL (result, alignment);
      if (adj != 0)
	{
	  mdp = MD_TO_MDP (md);
	  for (l = mdp -> aligned_blocks; l != NULL; l = l -> next)
	    {
	      if (l -> aligned == NULL)
		{
		  /* This slot is free.  Use it.  */
		  break;
		}
	    }
	  if (l == NULL)
	    {
	      l = (struct alignlist *) mmalloc (md, sizeof (struct alignlist));
	      if (l == NULL)
		{
		  mfree (md, result);
		  return (NULL);
		}
	      l -> next = mdp -> aligned_blocks;
	      mdp -> aligned_blocks = l;
	    }
	  l -> exact = result;
	  result = l -> aligned = (char *) result + alignment - adj;
	}
    }
  return (result);
}
