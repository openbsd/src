/* This testcase is part of GDB, the GNU debugger.

   Copyright 2004 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Please email any bugs, comments, and/or additions to this file to:
   bug-gdb@prep.ai.mit.edu  */

#include <unistd.h>
#include <stdlib.h>
#include <sys/resource.h>

/* Print routines:

   The following are so that printf et.al. can be avoided.  Those
   might try to use malloc() and that, for this code, would be a
   disaster.  */

#define printf do not use

const char digit[] = "0123456789abcdefghijklmnopqrstuvwxyz";

static void
print_char (char c)
{
  write (1, &c, sizeof (c));
}

static void
print_unsigned (unsigned long u)
{
  if (u >= 10)
    print_unsigned (u / 10);
  print_char (digit[u % 10]);
}

static void
print_hex (unsigned long u)
{
  if (u >= 16)
    print_hex (u / 16);
  print_char (digit[u % 16]);
}

static void
print_string (const char *s)
{
  for (; (*s) != '\0'; s++)
    print_char ((*s));
}

static void
print_address (const void *a)
{
  print_string ("0x");
  print_hex ((unsigned long) a);
}

/* Print the current values of RESOURCE.  */

static void
print_rlimit (int resource)
{
  struct rlimit rl;
  getrlimit (resource, &rl);
  print_string ("cur=0x");
  print_hex (rl.rlim_cur);
  print_string (" max=0x");
  print_hex (rl.rlim_max);
}

static void
maximize_rlimit (int resource, const char *prefix)
{
  struct rlimit rl;
  print_string ("  ");
  print_string (prefix);
  print_string (": ");
  print_rlimit (resource);
  getrlimit (resource, &rl);
  rl.rlim_cur = rl.rlim_max;
  setrlimit (resource, &rl);
  print_string (" -> ");
  print_rlimit (resource);
  print_string ("\n");
}

/* Maintain a doublely linked list.  */
struct list
{
  struct list *next;
  struct list *prev;
  size_t size;
};

/* Put the "heap" in the DATA section.  That way it is more likely
   that the variable will occur early in the core file (an address
   before the heap) and hence more likely that GDB will at least get
   its value right.

   To simplify the list append logic, start the heap out with one
   entry (that lives in the BSS section).  */

static struct list dummy;
static struct list heap = { &dummy, &dummy };

int
main ()
{
  size_t max_chunk_size;

  /* Try to expand all the resource limits beyond the point of sanity
     - we're after the biggest possible core file.  */

  print_string ("Maximize resource limits ...\n");
#ifdef RLIMIT_CORE
  maximize_rlimit (RLIMIT_CORE, "core");
#endif
#ifdef RLIMIT_DATA
  maximize_rlimit (RLIMIT_DATA, "data");
#endif
#ifdef RLIMIT_STACK
  maximize_rlimit (RLIMIT_STACK, "stack");
#endif
#ifdef RLIMIT_AS
  maximize_rlimit (RLIMIT_AS, "stack");
#endif

  /* Compute an initial chunk size.  The math is dodgy but it works
     for the moment.  Perhaphs there's a constant around somewhere.  */
  {
    size_t tmp;
    for (tmp = 1; tmp > 0; tmp <<= 1)
      max_chunk_size = tmp;
  }

  /* Allocate as much memory as possible creating a linked list of
     each section.  The linking ensures that some, but not all, the
     memory is allocated.  NB: Some kernels handle this efficiently -
     only allocating and writing out referenced pages leaving holes in
     the file for unreferend pages - while others handle this poorly -
     writing out all pages including those that wern't referenced.  */

  print_string ("Alocating the entire heap ...\n");
  {
    size_t chunk_size;
    long bytes_allocated = 0;
    long chunks_allocated = 0;
    /* Create a linked list of memory chunks.  Start with
       MAX_CHUNK_SIZE blocks of memory and then try allocating smaller
       and smaller amounts until all (well at least most) memory has
       been allocated.  */
    for (chunk_size = max_chunk_size;
	 chunk_size >= sizeof (struct list);
	 chunk_size >>= 1)
      {
	unsigned long count = 0;
	print_string ("  ");
	print_unsigned (chunk_size);
	print_string (" bytes ... ");
	while (1)
	  {
	    struct list *chunk = malloc (chunk_size);
	    if (chunk == NULL)
	      break;
	    chunk->size = chunk_size;
	    /* Link it in.  */
	    chunk->next = NULL;
	    chunk->prev = heap.prev;
	    heap.prev->next = chunk;
	    heap.prev = chunk;
	    count++;
	  }
	print_unsigned (count);
	print_string (" chunks\n");
	chunks_allocated += count;
	bytes_allocated += chunk_size * count;
      }
    print_string ("Total of ");
    print_unsigned (bytes_allocated);
    print_string (" bytes ");
    print_unsigned (chunks_allocated);
    print_string (" chunks\n");
  }

  /* Push everything out to disk.  */

  print_string ("Dump core ....\n");
  *(char*)0 = 0;
}
