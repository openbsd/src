/*	$OpenBSD: grouptest.c,v 1.2 1998/11/15 00:44:09 niklas Exp $	*/

/*
 * Copyright (c) 1998 Niels Provos.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Ericsson Radio Systems.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This code was written under funding by Ericsson Radio Systems.
 */

/*
 * This module exercises the operations supplied by the group abstraction.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "math_2n.h"
#include "math_ec2n.h"
#include "math_group.h"

#define DUMP_X(_x_) point = (_x_); b2n_print (point->x);

int
main (void)
{
  int i;
  char buf[100];
  char buf2[100];
  struct group *group, *group2;
  ec2np_ptr point;

  group_init ();
  group = group_get (3);
  group2 = group_get (3);

  printf ("Testing: setraw, getraw: ");
  for (i = 0; i < 20; i++)
    buf[i] = i;

  group->setraw (group, group->c, buf, 20);
  if (group->getlen (group) != 20)
    printf ("FAILED ");
  else
    printf ("OKAY ");

  group->getraw (group, group->c, buf2);
  for (i = 0; i < 20; i++)
    if (buf2[i] != i)
      break;
  if (i < 20)
    printf ("FAILED ");
  else
    printf ("OKAY ");

  printf ("\nTesting: setrandom: ");
  group->setrandom (group, group->c);
  DUMP_X (group->c);
  group2->setrandom (group2, group2->c);
  DUMP_X (group2->c);

  printf ("\nTesting: operation:\n");
  group->operation (group, group->a, group->gen, group->c);
  point = group->a;
  printf ("\tX (%d): ", point->x->chunks); b2n_print (point->x);
  printf ("\tY (%d): ", point->y->chunks); b2n_print (point->y);

  group2->operation (group2, group2->a, group2->gen, group2->c);
  point = group2->a;
  printf ("\tX (%d): ", point->x->chunks); b2n_print (point->x);
  printf ("\tY (%d): ", point->y->chunks); b2n_print (point->y);

  printf ("Exchange Value 1: "); b2n_print (group->d);
  printf ("Exchange Value 2: "); b2n_print (group2->d);

  printf ("Testing: operation ...:\n");
  group->getraw (group, group->a, buf);
  group2->setraw (group2, group2->b, buf, 20);

  group2->getraw (group2, group2->a, buf);
  group->setraw (group, group->b, buf, 20);

  group2->operation (group2, group2->a, group2->b, group2->c);
  printf ("Exchange Value 21: "); DUMP_X (group2->a);

  group->operation (group, group->a, group->b, group->c);
  printf ("Exchange Value 12: "); DUMP_X (group->a);

  group->getraw (group, group->a, buf);
  group2->getraw (group2, group2->a, buf2);
  printf ("Testing: operation ...: ");
  if (memcmp(buf, buf2, 20))
    printf ("FAILED ");
  else
    printf ("OKAY ");

  printf ("\n");
  return 1;
}
