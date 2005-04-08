/*	$OpenBSD: ec2ntest.c,v 1.1 2005/04/08 17:12:49 cloder Exp $	*/
/*	$EOM: ec2ntest.c,v 1.3 1998/07/16 09:21:59 niklas Exp $	*/

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
 * B2N is a module for doing arithmetic on the Field GF(2**n) which is
 * isomorph to ring of polynomials GF(2)[x]/p(x) where p(x) is an
 * irreduciable polynomial over GF(2)[x] with grade n.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "math_2n.h"
#include "math_ec2n.h"

#define BUFSIZE 200

#define CMP_FAIL(n,x) b2n_snprint (buf, BUFSIZE, n); if (strcmp (buf, (x))) \
    printf ("FAILED: %s != %s ", buf, x); else printf ("OKAY ");

int
main (void)
{
  b2n_t k;
  ec2np_t p, q, r;
  ec2ng_t g;
  char buf[BUFSIZE];

  b2n_init (k);
  ec2np_init (p);
  ec2np_init (q);
  ec2np_init (r);
  ec2ng_init (g);

  printf ("Testing: ec2ng_set* :");
  /* Init Group */
  ec2ng_set_p_str (g, "0x0800000000000000000000004000000000000001");
  CMP_FAIL (g->p, "0x0800000000000000000000004000000000000001");
  ec2ng_set_a_ui (g, 0);
  CMP_FAIL (g->a, "0x00");
  ec2ng_set_b_str (g, "0x07338f");
  CMP_FAIL (g->b, "0x07338f");

  printf ("\nTesting: ec2np_find_y: ");
  /* Init Point */
  ec2np_set_x_ui (p, 0x7b);
  ec2np_find_y (p, g);

  CMP_FAIL (p->y, "0x01c8");

  printf ("\nTesting: ec2np_ison: ");
  if (ec2np_ison (p, g))
    printf ("OKAY ");
  else
    printf ("FAILED ");

  ec2np_set_x_ui (q, 0x4);
  ec2np_find_y (q, g);
  if (ec2np_ison (q, g))
    printf ("OKAY ");
  else
    printf ("FAILED ");

  printf ("\nTesting: ec2np_add: ");
  ec2np_set (r, p);
  b2n_add (r->y, r->y, r->x);
  ec2np_add (r, r, p, g);
  if (!r->inf)
    printf ("FAILED ");
  else
    printf ("OKAY ");

  ec2np_add (q, p, q, g);
  CMP_FAIL (q->x, "0x06f32d7cc82cec8612a87a86e026350fb7595469");
  CMP_FAIL (q->y, "0x4ab92e21e51358ca8deab3fbbc9f7d8a7d1575");
  if (ec2np_ison (q, g))
    printf ("OKAY ");
  else
    printf ("FAILED ");

  ec2np_add (p, q, q, g);
  CMP_FAIL (p->x, "0x0390001461385559a22ac9b6181c1e1889b38451");
  CMP_FAIL (p->y, "0x0188e61f38d747d7813c6a8b33d14dfb7418b04c");
  if (ec2np_ison (p, g))
    printf ("OKAY ");
  else
    printf ("FAILED ");

  printf ("\nTesting: ec2np_mul: ");
  b2n_set_ui (k, 57);
  ec2np_set (q, p);
  ec2np_mul (q, q, k, g);
  if (ec2np_ison (q, g))
    printf ("OKAY ");
  else
    printf ("FAILED ");
  CMP_FAIL (q->x, "0x06bcf88caab88f99399350c46559da3b91afbf9d");

  b2n_set_str (k, "0x0800000000000000000057db5698537193aef943");
  ec2np_set (q, p);
  ec2np_mul (q, q, k, g);
  if (ec2np_ison (q, g))
    printf ("OKAY ");
  else
    printf ("FAILED ");
  CMP_FAIL (q->x, "0x0390001461385559a22ac9b6181c1e1889b38451");

  printf ("\n");
  ec2np_clear (p);
  ec2np_clear (q);
  ec2np_clear (r);
  ec2ng_clear (g);
  b2n_clear (k);
  return 0;
}
