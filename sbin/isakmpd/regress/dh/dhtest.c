/*	$Id: dhtest.c,v 1.1.1.1 1998/11/15 00:03:50 niklas Exp $	*/

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
 * This module does a Diffie-Hellman Exchange
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "math_group.h"
#include "dh.h"

#define DUMP_X(_x_) point = (_x_); b2n_print (point->x);

int
main (void)
{
  int len;
  char buf[100], buf2[100];
  char sec[100], sec2[100];
  struct group *group, *group2;

  group_init ();
  group = group_get (4);
  group2 = group_get (4);

  printf ("Testing DH (elliptic curve): \n");

  printf ("dh_getlen\n");
  len = dh_getlen (group);
  printf ("dh_create_exchange\n");
  dh_create_exchange (group, buf);
  dh_create_exchange (group2, buf2);

  printf ("dh_create_shared\n");
  dh_create_shared (group, sec, buf2);
  dh_create_shared (group2, sec2, buf);

  printf ("Result: ");
  if (memcmp (sec, sec2, len))
    printf ("FAILED ");
  else
    printf ("OKAY ");

  group_free (group);
  group_free (group2);

  printf ("\nTesting DH (MODP): \n");

  group = group_get (1);
  group2 = group_get (1);

  printf ("dh_getlen\n");
  len = dh_getlen (group);
  printf ("dh_create_exchange\n");
  dh_create_exchange (group, buf);
  dh_create_exchange (group2, buf2);

  printf ("dh_create_shared\n");
  dh_create_shared (group, sec, buf2);
  dh_create_shared (group2, sec2, buf);

  printf ("Result: ");
  if (memcmp (sec, sec2, len))
    printf ("FAILED ");
  else
    printf ("OKAY ");
  

  printf ("\n");
  return 1;
}
