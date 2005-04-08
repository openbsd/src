/*	$OpenBSD: b2ntest.c,v 1.1 2005/04/08 17:12:48 cloder Exp $	*/
/*	$EOM: b2ntest.c,v 1.4 1998/07/16 19:31:55 provos Exp $	*/

/*
 * Copyright (c) 1998 Niels Provos.  All rights reserved.
 * Copyright (c) 2001 Niklas Hallqvist.  All rights reserved.
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

#define BUFSIZE 200

#define CMP_FAIL(n,x) b2n_snprint (buf, BUFSIZE, n); if (strcmp (buf, (x))) \
    printf ("FAILED: %s != %s ", buf, x); else printf ("OKAY ");

int
main (void)
{
  int i;
  b2n_t n, m, d, r;
  char buf[BUFSIZE];

  b2n_init (n);
  b2n_init (m);
  b2n_init (d);
  b2n_init (r);

  printf ("Arithimetic Tests for GF(2)[x]:\n");
  printf ("Testing: b2n_set*: ");
  b2n_set_ui (n, 0xffc0);
  CMP_FAIL (n, "0xffc0");

  b2n_set_str (m, "0x180c0");
  CMP_FAIL (m, "0x0180c0");
  b2n_set_str (m, "0x808b8080c0");
  CMP_FAIL (m, "0x808b8080c0");

  printf ("\nTesting: b2n_add: ");
  b2n_add (d, n, m);
  CMP_FAIL (d, "0x808b807f00");
  b2n_add (n, n, m);
  CMP_FAIL (n, "0x808b807f00");
  b2n_add (n, n, n);
  CMP_FAIL (n, "0x00");
  b2n_set_str (n, "0x9090900000000000000000");
  b2n_set_ui (m, 0);
  b2n_add (n, n, m);
  CMP_FAIL (n, "0x9090900000000000000000");

  printf ("\nTesting: b2n_lshift: ");
  b2n_set_str (m, "0x808b8080c0");
  b2n_lshift (n, m, 3);
  CMP_FAIL (n, "0x04045c040600");
  b2n_lshift (n, m, 11);
  CMP_FAIL (n, "0x04045c04060000");
  b2n_set (n, m);
  for (i = 0; i < 11; i++)
    b2n_lshift (n, n, 1);
  CMP_FAIL (n, "0x04045c04060000");
  b2n_lshift (d, m, 12);
  CMP_FAIL (d, "0x0808b8080c0000");
  b2n_set_str (m, "0xdeadbeef");
  b2n_lshift (d, m, 103);
  CMP_FAIL (d, "0x6f56df7780000000000000000000000000");

  printf ("\nTesting: b2n_rshift: ");
  b2n_rshift (m, n, 3);
  CMP_FAIL (m, "0x808b8080c000");
  b2n_rshift (m, m, 11);
  CMP_FAIL (m, "0x1011701018");
  b2n_set_str (m, "0x12381998713258186712365");
  b2n_rshift (m, m, 23);
  CMP_FAIL (m, "0x024703330e264b030c");
  b2n_set_str (m, "0x12381998713258186712365");
  for (i=0; i<23; i++)
    b2n_rshift (m, m, 1);
  CMP_FAIL (m, "0x024703330e264b030c");

  printf ("\nTesting: b2n_mul: 0x9 o 0x5: ");
  b2n_set_ui (n, 9);
  b2n_set_ui (m, 5);
  b2n_mul (d, n, m);
  CMP_FAIL (d, "0x2d");
  b2n_mul (n, n, m);
  CMP_FAIL (d, "0x2d");

  printf ("\nTesting: b2n_mul: 0x9 o 0x0: ");
  b2n_set_ui (n, 9);
  b2n_set_ui (m, 0);
  b2n_mul (d, n, m);
  CMP_FAIL (d, "0x00");
  b2n_set_ui (n, 0);
  b2n_set_ui (m, 9);
  b2n_mul (d, n, m);
  CMP_FAIL (d, "0x00");

  printf ("\nTesting: b2n_mul: 0x9 o 0x1: ");
  b2n_set_ui (n, 9);
  b2n_set_ui (m, 1);
  b2n_mul (d, n, m);
  CMP_FAIL (d, "0x09");

  printf ("\nTesting: b2n_mul: 0x12329 o 0x1235: ");
  b2n_set_str (n, "0x12329");
  b2n_set_str (m, "0x1235");
  b2n_mul (d, n, m);
  CMP_FAIL (d, "0x10473a3d");
  b2n_mul (n, n, m);
  CMP_FAIL (d, "0x10473a3d");

  printf ("\nTesting: b2n_square: 0x1235 o 0x1235: ");
  b2n_set_str (m, "0x1235");
  b2n_square (n, m);
  CMP_FAIL (n, "0x01040511");

  printf ("\nTesting: b2n_square: 0x80c1235 o 0x80c1235: ");
  b2n_set_str (m, "0x80c1235");
  b2n_square (n, m);
  CMP_FAIL (n, "0x40005001040511");

  b2n_set_str (m, "0x12329");
  printf ("\nTesting: sigbit: 0x12329: %d, %s",
	  b2n_sigbit(m), b2n_sigbit(m) == 17 ? "OKAY" : "FAILED");
  b2n_set_ui (m, 0);
  printf ("\nTesting: sigbit: 0x0: %d, %s",
	  b2n_sigbit(m), b2n_sigbit(m) == 0 ? "OKAY" : "FAILED");
  b2n_set_str (m, "0x7f3290000");
  printf ("\nTesting: sigbit: 0x7f3290000: %d, %s",
	  b2n_sigbit(m), b2n_sigbit(m) == 35 ? "OKAY" : "FAILED");

  printf ("\nTesting: b2n_cmp: ");
  b2n_set_str (m, "0x2234");
  b2n_set_str (n, "0x1234");
  printf ("%d <-> %d, ", b2n_sigbit (m), b2n_sigbit(n));
  printf ("%d, %d ,%d: ", b2n_cmp (m,m), b2n_cmp (m,n), b2n_cmp (n,m));
  if (b2n_cmp (m,m) || b2n_cmp (m,n) != 1 || b2n_cmp (n,m) != -1)
    printf ("FAILED");
  else
    printf ("OKAY");
  printf ("\nTesting: b2n_cmp_null: ");
  b2n_set_str (m, "0x2234");
  b2n_set_ui (n, 0);
  printf ("%d, %d: ", b2n_cmp_null (m), b2n_cmp_null (n));
  if (b2n_cmp_null (m) != 1 || b2n_cmp_null (n))
    printf ("FAILED");
  else
    printf ("OKAY");

  printf ("\nTesting: b2n_div: 0x2d / 0x5: ");
  b2n_set_str (n, "0x2d");
  b2n_set_ui (m, 5);
  b2n_div (n, m, n, m);
  CMP_FAIL (n, "0x09");
  CMP_FAIL (m, "0x00");
  printf ("\nTesting: b2n_div: 0x2d / 0x1: ");
  b2n_set_str (n, "0x2d");
  b2n_set_ui (m, 1);
  b2n_div (n, m, n, m);
  CMP_FAIL (n, "0x2d");
  CMP_FAIL (m, "0x00");

  printf ("\nTesting: b2n_div: 0x10473a3d / 0x1235: ");
  b2n_set_str (n, "0x10473a3d");
  b2n_set_str (m, "0x1235");
  b2n_div (n, m, n, m);
  CMP_FAIL (n, "0x012329");
  CMP_FAIL (m, "0x00");

  printf ("\nTesting: b2n_div: 0x10473a3d / 0x1536: ");
  b2n_set_str (n, "0x10473a3d");
  b2n_set_str (m, "0x1536");
  b2n_div (n, m, n, m);
  CMP_FAIL (n, "0x014331");
  CMP_FAIL (m, "0xab");
  b2n_set_str (n, "0x10473a3d");
  b2n_set_str (m, "0x1536");
  b2n_div_q (d, n, m);
  CMP_FAIL (d, "0x014331");
  b2n_div_r (d, n, m);
  CMP_FAIL (d, "0xab");

  printf ("\nTesting: b2n_div: "
	  "0x0800000000000000000000004000000000000001 / 0xffab09909a00: ");
  b2n_set_str (n, "0x0800000000000000000000004000000000000001");
  b2n_set_str (m, "0xffab09909a00");
  b2n_div_q (d, n, m);
  CMP_FAIL (d, "0x18083e83a98647cedae0b3e69a5e");
  b2n_div_r (d, n, m);
  CMP_FAIL (d, "0x5b8bf98cac01");
  b2n_set (d, m);
  b2n_div (n, m, n, m);
  CMP_FAIL (n, "0x18083e83a98647cedae0b3e69a5e");
  CMP_FAIL (m, "0x5b8bf98cac01");

  printf ("\nTesting: b2n_div: "
	  "0x0800000000000000000000004000000000000001 / 0x7b: ");
  b2n_set_str (n, "0x0800000000000000000000004000000000000001");
  b2n_set_str (m, "0x7b");
  b2n_div (n, m, n, m);
  CMP_FAIL (n, "0x32dea27065bd44e0cb7a89c000000000000000");
  CMP_FAIL (m, "0x01");

  printf ("\n\nArithimetic Tests for GF(2**m) ~= GF(2)[x]/p(x):\n");
  printf ("Testing: b2n_gcd: ");
  b2n_set_str (d, "0x771");
  b2n_set_str (m, "0x26d");
  b2n_gcd (n, m, d);
  CMP_FAIL (n, "0x0b");
  b2n_set_str (d, "0x0800000000000000000000004000000000000001");
  b2n_set_str (m, "0xffab09909a00");
  b2n_gcd (n, m, d);
  CMP_FAIL (n, "0x01");
  b2n_set_str (d, "0x0800000000000000000000004000000000000001");
  b2n_set_str (m, "0x7b");
  b2n_gcd (n, m, d);
  CMP_FAIL (n, "0x01");

  printf ("\nTesting: b2n_mul_inv: ");
  b2n_set_str (d, "0x0800000000000000000000004000000000000001");
  b2n_set_str (m, "0xffab09909a00");
  b2n_mul_inv (n, m, d);
  CMP_FAIL (n, "0x074029149f69304174d28858ae5c60df208a22a8");
  b2n_set_str (n, "0xffab09909a00");
  b2n_mul_inv (n, n, d);
  CMP_FAIL (n, "0x074029149f69304174d28858ae5c60df208a22a8");
  b2n_mul (n, n, m);
  b2n_mod (n, n, d);
  CMP_FAIL (n, "0x01");
  b2n_set_str (d, "0x0800000000000000000000004000000000000001");
  b2n_set_str (m, "0x7b");
  b2n_mul_inv (n, m, d);
  CMP_FAIL (n, "0x32dea27065bd44e0cb7a89c000000000000000");
  b2n_mul (n, n, m);
  b2n_mod (n, n, d);
  CMP_FAIL (n, "0x01");

  printf ("\nTesting: b2n_random: ");
  b2n_random (m, 155);
  b2n_snprint (buf, BUFSIZE, m);
  printf ("%s, %d", buf, b2n_sigbit(m));

  printf ("\nTesting: b2n_sqrt: ");
  b2n_set_str (n, "0x0800000000000000000000004000000000000001");
  b2n_set_ui (d, 2);
  b2n_sqrt (m, d, n);
  b2n_square (d, m);
  b2n_add (d, d, m);
  b2n_mod (d, d, n);
  CMP_FAIL (d, "0x02");

  /* x**3 + b */
  b2n_set_ui (n, 0x7b);
  b2n_square (d, n);
  b2n_mul (d, d, n);
  b2n_set_str (n, "0x07338f");
  b2n_add (d, d, n);
  b2n_set_str (n, "0x0800000000000000000000004000000000000001");
  b2n_mod (d, d, n);
  /* \alpha = x**3 + b - end */

  /* \beta = x**(-2)*\alpha */
  b2n_set_ui (m, 0x7b);
  b2n_mul_inv (m, m, n);
  b2n_square (m, m);
  b2n_mod (m, m, n);
  b2n_mul (d, d, m);
  b2n_mod (d, d, n);
  b2n_set (r, d);
  /* \beta = x**(-2)*\alpha - end */

  b2n_sqrt (m, d, n);
  CMP_FAIL (m, "0x0690aec7cd215d8f9a42bb1f0000000000000004");
  b2n_square (d, m);
  b2n_mod (d, d, n);
  b2n_add (d, d, m);
  b2n_mod (d, d, n);
  printf ("Squaring Check: ");
  CMP_FAIL (d, "0x03d5af92c8311d9e8f56be4b3e690aec7cd215cc");

  printf ("\nTesting: b2n_trace: ");
  b2n_set_ui (m, 2);
  b2n_trace (d, m, n);
  CMP_FAIL (d, "0x00");
  b2n_set_ui (m, 0x11223);
  b2n_trace (d, m, n);
  CMP_FAIL (d, "0x01");

  printf ("\nTesting: b2n_exp_mod: ");
  b2n_set_ui (m, 0x7b);
  b2n_exp_mod (d, m, 5, n);
  CMP_FAIL (d, "0x7cccb7cb");
  b2n_set_str (m, "0x123456789abcdef");
  b2n_exp_mod (d, m, 13, n);
  CMP_FAIL (d, "0x043f0a8550cb69b3c50d0340d1c6d5c97ecd60d4");

  printf ("\nTesting: b2n_3mul: ");
  b2n_set_ui (m, 0x7b);
  b2n_3mul (m, m);
  CMP_FAIL (m, "0x0171");

  b2n_set_ui (m, 0x7fffffff);
  b2n_3mul (m, m);
  CMP_FAIL (m, "0x017ffffffd");

  printf ("\nTesting: b2n_nadd: ");
  b2n_set_str (m, "0x7fffffff");
  b2n_set_str (n, "0x10203045");
  b2n_nadd (d, n, m);
  CMP_FAIL (d, "0x90203044");

  b2n_set_str (m, "0x9a4a54d8b8dfa566112849991214329a233d");
  b2n_set_str (n, "0x70ee40dd60c8657e58eda9a17ad9176e28b4b457e5a34a0948e335");
  b2n_nadd (d, n, m);
  CMP_FAIL (d, "0x70ee40dd60c8657e5987f3f65391f7138ec5dca17eb55e3be30672");

  printf ("\nTesting: b2n_nsub: ");
  b2n_set_str (n, "0x90203044");
  b2n_set_str (m, "0x10203045");
  b2n_nsub (d, n, m);
  CMP_FAIL (d, "0x7fffffff");

  b2n_set_str (n, "0x70ee40dd60c8657e5987f3f65391f7138ec5dca17eb55e3be30672");
  b2n_set_str (m, "0x70ee40dd60c8657e58eda9a17ad9176e28b4b457e5a34a0948e335");
  b2n_nsub (d, n, m);
  CMP_FAIL (d, "0x9a4a54d8b8dfa566112849991214329a233d");

  b2n_clear (n);
  b2n_clear (m);
  b2n_clear (d);
  b2n_clear (r);

  printf ("\n");
  return 0;
}
