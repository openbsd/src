/*	$OpenBSD: alloca.c,v 1.8 2007/02/26 19:32:54 miod Exp $	*/

/* alloca.c -- allocate automatically reclaimed memory
   (Mostly) portable public-domain implementation -- D A Gwyn

   This implementation of the PWB library alloca function,
   which is used to allocate space off the run-time stack so
   that it is automatically reclaimed upon procedure exit,
   was inspired by discussions with J. Q. Johnson of Cornell.

   There are some preprocessor constants that can
   be defined when compiling for your specific system, for
   improved efficiency; however, the defaults should be okay.

   The general concept of this implementation is to keep
   track of all alloca-allocated blocks, and reclaim any
   that are found to be deeper in the stack than the current
   invocation.  This heuristic does not reclaim storage as
   soon as it becomes invalid, but it will do so eventually.

   As a special case, alloca(0) reclaims storage without
   allocating any.  It is a good idea to use alloca(0) in
   your main control loop, etc. to force garbage collection.  */

/* If someone has defined alloca as a macro,
   there must be some other way alloca is supposed to work.  */
#ifndef alloca

#include <string.h>
#include <stdlib.h>

/* If your stack is a linked list of frames, you have to
   provide an "address metric" ADDRESS_FUNCTION macro.  */

#define ADDRESS_FUNCTION(arg) &(arg)

typedef void *pointer;

/* Define STACK_DIRECTION for your cpu:
   STACK_DIRECTION > 0 => grows toward higher addresses
   STACK_DIRECTION < 0 => grows toward lower addresses */

#if defined(__alpha__) || defined(__m68k__)    || defined(__i386__) || \
    defined(__m88k__)  || defined(__mips__)    || defined(__powerpc__) || \
    defined(__sparc__) || defined(__sparc64__) || defined(__vax__) || \
    defined(__amd64__) || defined(__mips64__) || defined(__arm__) || \
    defined(__sh__)
# define	STACK_DIRECTION	-1
#elif defined(__hppa__) || defined(__hppa64__)
# define	STACK_DIRECTION	1
#else
# error must specify stack direction
#endif

/* An "alloca header" is used to:
   (a) chain together all alloca'ed blocks;
   (b) keep track of stack depth.

   It is very important that sizeof(header) agree with malloc
   alignment chunk size.  The following default should work okay.  */

#ifndef	ALIGN_SIZE
#define	ALIGN_SIZE	sizeof(double)
#endif

typedef union hdr
{
  char align[ALIGN_SIZE];	/* To force sizeof(header).  */
  struct
    {
      union hdr *next;		/* For chaining headers.  */
      char *deep;		/* For stack depth measure.  */
    } h;
} header;

static header *last_alloca_header = NULL;	/* -> last alloca header.  */

/* Return a pointer to at least SIZE bytes of storage,
   which will be automatically reclaimed upon exit from
   the procedure that called alloca.  Originally, this space
   was supposed to be taken from the current stack frame of the
   caller, but that method cannot be made to work for some
   implementations of C, for example under Gould's UTX/32.  */

pointer
alloca (size_t size)
{
  char probe;		/* Probes stack depth: */
  char *depth = ADDRESS_FUNCTION (probe);

  /* Reclaim garbage, defined as all alloca'd storage that
     was allocated from deeper in the stack than currently.  */

  {
    header *hp;	/* Traverses linked list.  */

    for (hp = last_alloca_header; hp != NULL;)
      if ((STACK_DIRECTION > 0 && hp->h.deep > depth)
	  || (STACK_DIRECTION < 0 && hp->h.deep < depth))
	{
	  header *np = hp->h.next;

	  free ((pointer) hp);	/* Collect garbage.  */

	  hp = np;		/* -> next header.  */
	}
      else
	break;			/* Rest are not deeper.  */

    last_alloca_header = hp;	/* -> last valid storage.  */
  }

  if (size == 0)
    return NULL;		/* No allocation required.  */

  /* Allocate combined header + user data storage.  */

  {
    pointer new = malloc (sizeof (header) + size);
    /* Address of header.  */

    if (new == 0)
      abort();

    ((header *) new)->h.next = last_alloca_header;
    ((header *) new)->h.deep = depth;

    last_alloca_header = (header *) new;

    /* User storage begins just after header.  */

    return (pointer) ((char *) new + sizeof (header));
  }
}

#endif /* no alloca */
