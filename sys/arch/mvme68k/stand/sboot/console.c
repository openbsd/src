/*
 *
 * Copyright (c) 1995 Charles D. Cranor and Seth Widoff
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
 *      This product includes software developed by Charles D. Cranor
 *	and Seth Widoff.
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
 * console i/o
 */

#include "sboot.h"

/*
 * hardware
 */

struct zs_hw {
  volatile u_char ctl;
  volatile u_char data;
};

struct zs_hw *zs =  (struct zs_hw *)CONS_ZS_ADDR;

/*
 * consinit: init console
 */

consinit()

{
  register int mark = time();
  register int rr1;
  while (1) {
    if (time() > mark + 5) break;
    zs->ctl = 1; rr1 = zs->ctl;
    zs->ctl = 0;
    if ((rr1 & 0x1) == 1 && (zs->ctl & 0x4) == 4) break; /* zs_drain! */
  }
  zs->ctl =  9; zs->ctl = 0x00; /* clear interrupt */
  zs->ctl =  4; zs->ctl = 0x44; /* 16x clk, 1 stop bit */
  zs->ctl =  5; zs->ctl = 0xea; /* DTR on, 8 bit xmit, xmit on, RTS on */
  zs->ctl =  3; zs->ctl = 0xc1; /* 8 bit recv, auto cd_cts, recv on */
  zs->ctl =  1; zs->ctl = 0x00; /* no intrs */
  zs->ctl =  2; zs->ctl = 0x00; /* no vector */
  zs->ctl = 10; zs->ctl = 0x00; /* */
  zs->ctl = 11; zs->ctl = 0x50; /* clocking options */
  zs->ctl = 12; zs->ctl = 0x0e; /* 9600 baud, part 1 */
  zs->ctl = 13; zs->ctl = 0x00; /* 9600 baud, part 2 */
  zs->ctl = 14; zs->ctl = 0x03; /* more clocking options */
  zs->ctl = 15; zs->ctl = 0x00; /* clear intrs */
}

/*
 * putchar: put char to console
 */

void putchar(char c) 
{
  if (c == '\n') putchar('\r');  /* avoid the need for \r\n in printf */
  zs->ctl = 0;
  while ((zs->ctl & 0x04) == 0) {
    zs->ctl = 0;
  }
  zs->ctl = 8;
  zs->ctl = c;
}

/*
 * cngetc: get 1 char from console
 */

char cngetc () 
{
  zs->ctl = 0;
  while ((zs->ctl & 0x1) == 0) {
    zs->ctl = 0;
  }
  zs->ctl = 8;
  return zs->ctl;
}

/*
 * puts: put string to console
 */

void puts ( char * str )
{
  while ( *str != '\0' ) {
    putchar(*str);
    str++;
  }
}

/*
 * ngets: get string from console 
 */

void ngets ( char * str, int size )
{
  int i = 0;
  while ( (i < size - 1) && (str[i] = cngetc()) != '\r') {
    if ( str[i] == '\b' || str[i] == 0x7F ) {
      if ( i == 0) continue;
      i--;
      puts("\b \b");
      continue;
    }
    putchar(str[i]);
    i++;
  }
  puts("\n");
  str[i] = '\0';
}

