/* Code to look inside gdb's structures to provide
   information suitable for a GUI or an ASCII representation */

#include <stdio.h>
#include <varargs.h>
#include <stdarg.h>
#include "../defs.h"
#include "../symtab.h"
#include "../../include/obstack.h"


/* This code builds all its stuff in one obstack
   so we never bother with freeing */


static int
build_info_block_frame_locals (b, frame)
     struct block *b;
     register FRAME frame;
{
  int nsyms;
  register int i;
  register struct symbol *sym;

  nsyms = BLOCK_NSYMS (b);

  for (i = 0; i < nsyms; i++)
    {
      sym = BLOCK_SYM (b, i);
      switch (SYMBOL_CLASS (sym))
	{
	case LOC_LOCAL:
	case LOC_REGISTER:
	case LOC_STATIC:
	case LOC_BASEREG:
	  add_record (sym, frame);
	  break;

	default:
	  /* Ignore symbols which are not locals.  */
	  break;
	}
    }
}

build_block_info (frame)
FRAME frame;
{
  register struct block *block = get_frame_block (frame);
  register int values_printed = 0;

  while (block != 0)
    {
      build_info_block_frame_locals (block, frame);

      /* After handling the function's top-level block, stop.
	 Don't continue to its superblock, the block of
	 per-file symbols.  */
      if (BLOCK_FUNCTION (block))
	break;
      block = BLOCK_SUPERBLOCK (block);
    }
}

