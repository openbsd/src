/*

rfc-pg.c

Author: Tatu Ylonen <ylo@cs.hut.fi>

Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
                   All rights reserved

Created: Fri Jul  7 02:14:16 1995 ylo

*/

/* RCSID("$Id: rfc-pg.c,v 1.1 1999/09/26 20:53:37 deraadt Exp $"); */

#include <stdio.h>

int main()
{
  int add_formfeed = 0;
  int skipping = 0;
  int ch;

  while ((ch = getc(stdin)) != EOF)
    {
      if (ch == '\n')
	{
	  if (add_formfeed)
	    {
	      putc('\n', stdout);
	      putc('\014', stdout);
	      putc('\n', stdout);
	      add_formfeed = 0;
	      skipping = 1;
	      continue;
	    }
	  if (skipping)
	    continue;
	}
      skipping = 0;
      if (ch == '\014')
	{
	  add_formfeed = 1;
	  continue;
	}
      putc(ch, stdout);
    }
  exit(0);
}
