static char *rcs_id = "$Id: fixup_fujitsu_grayscale.c,v 1.1 1997/03/11 03:23:11 kstailey Exp $";
/*
 * Copyright (c) 1995 Kenneth Stailey
 * All rights reserved.
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
 *	This product includes software developed for the NetBSD Project
 *	by Kenneth Stailey
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * fixup_fujitsu_grayscale.c: convert scanner output to PGM
 *
 * This is faster than using "pgminvert | pnmgamma" but does the same
 * thing basically.
 */

#include <stdio.h>
#include <math.h>

static int gamma_table[256];

void buildgamma(double gamma)
{
  int i, v;
  double one_over_gamma, index;

  one_over_gamma = 1.0 / gamma;

  for (i = 0; i <= 255; ++i) {
    index = (double)i / 255.0;
    v = (255.0 * pow(index, one_over_gamma)) + 0.5;
    if (v > 255)
      v = 255;
    gamma_table[i] = 255 - v;
  }
}

main()
{
  int ch;

  buildgamma(0.75);

  while ((ch = getchar()) != EOF)
    putchar((char)(gamma_table[ch]));
}
