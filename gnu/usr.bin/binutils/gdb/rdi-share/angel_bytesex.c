/* 
 * Copyright (C) 1995 Advanced RISC Machines Limited. All rights reserved.
 * 
 * This software may be freely used, copied, modified, and distributed
 * provided that the above copyright notice is preserved in all copies of the
 * software.
 */

/*
 * angel_bytesex.c - Code to support byte-sex independence
 * Copyright: (C) 1991, Advanced RISC Machines Ltd., Cambridge, England.
 */

/*
 * RCS $Revision: 1.3 $
 * Checkin $Date: 2004/12/27 14:00:53 $
 */

#include "angel_bytesex.h"

static int reversing_bytes = 0;

void bytesex_reverse(yes_or_no)
int yes_or_no;
{ reversing_bytes = yes_or_no;
}

int bytesex_reversing()
{
  return reversing_bytes;
}

int32 bytesex_hostval(v)
int32 v;
{ /* Return v with the same endian-ness as the host */
  /* This mess generates better ARM code than the more obvious mess */
  /* and may eventually peephole to optimal code...                 */
  if (reversing_bytes)
  { unsigned32 t;
    /* t = v ^ (v ror 16) */
    t = v ^ ((v << 16) | (((unsigned32)v) >> 16));
    t &= ~0xff0000;
    /* v = v ror 8 */
    v = (v << 24) | (((unsigned32)v) >> 8);
    v = v ^ (t >> 8);
  }
  return v;
}

int32 bytesex_hostval_16(v)
int32 v;
{
  if (reversing_bytes) {
    v = ((v >> 8) & 0xff) | ((v << 8) & 0xff00);
  }
  return v;
}
