/*	$OpenBSD: math_2n.c,v 1.4 1999/02/26 03:46:18 niklas Exp $	*/
/*	$EOM: math_2n.c,v 1.11 1999/02/25 11:39:12 niklas Exp $	*/

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
 * B2N is a module for doing arithmetic on the Field GF(2**n) which is
 * isomorph to ring of polynomials GF(2)[x]/p(x) where p(x) is an
 * irreduciable polynomial over GF(2)[x] with grade n.
 * 
 * First we need functions which operate on GF(2)[x], operation
 * on GF(2)[x]/p(x) can be done as for Z_p then.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "sysdep.h"

#include "util.h"
#include "math_2n.h"

u_int8_t hex2int (char);

static char int2hex[] = "0123456789abcdef";
CHUNK_TYPE b2n_mask[CHUNK_BITS] = {
  0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,
#if CHUNK_BITS > 8
  0x0100,0x0200,0x0400,0x0800,0x1000,0x2000,0x4000,0x8000,
#if CHUNK_BITS > 16
  0x00010000,0x00020000,0x00040000,0x00080000,
  0x00100000,0x00200000,0x00400000,0x00800000,
  0x01000000,0x02000000,0x04000000,0x08000000,
  0x10000000,0x20000000,0x40000000,0x80000000,
#endif
#endif
};

/* Misc */
u_int8_t
hex2int (char c)
{
  if (c <= '9')
    return c - '0';
  if (c <= 'f')
    return 10 + c - 'a';

  return 0;
}


void
b2n_random (b2n_ptr n, u_int32_t bits)
{
  b2n_resize (n, (CHUNK_MASK + bits) >> CHUNK_SHIFTS);

  getrandom ((u_int8_t *)n->limp, CHUNK_BYTES * n->chunks);

  /* Get the number of significant bits right */
  if (bits & CHUNK_MASK)
    {
      CHUNK_TYPE m = (((1 << ((bits & CHUNK_MASK)-1)) - 1) << 1) | 1;
      n->limp[n->chunks-1] &= m;
    }

  n->dirty = 1;
}

/* b2n management functions */

void
b2n_init (b2n_ptr n)
{
  n->chunks = 0;
  n->limp = NULL;
}

void
b2n_clear (b2n_ptr n)
{
  /* XXX Does all systems deal with free (NULL) nicely?  */
  free (n->limp);
}

void
b2n_resize (b2n_ptr n, unsigned int chunks)
{
  int old = n->chunks;
  int size;
  CHUNK_TYPE *new;

  if (chunks == 0)
    chunks = 1;

  if (chunks == old)
    return;
  
  size = CHUNK_BYTES * chunks;

  /* XXX - is there anything I can do here? */
  new = realloc (n->limp, size);
  if (new == NULL)
    return ;

  n->limp = new;
  n->chunks = chunks;
  n->bits = chunks << CHUNK_SHIFTS;
  n->dirty = 1;

  if (chunks > old)
      memset (n->limp + old, 0, size - CHUNK_BYTES*old);
}

/* Simple assignment functions */

void
b2n_set (b2n_ptr d, b2n_ptr s)
{
  if (d == s)
    return;

  b2n_sigbit (s);
  b2n_resize (d, (CHUNK_MASK + s->bits) >> CHUNK_SHIFTS);
  memcpy (d->limp, s->limp, CHUNK_BYTES*d->chunks);
  d->bits = s->bits;
  d->dirty = s->dirty;
}

void
b2n_set_null (b2n_ptr n)
{
  b2n_resize (n , 1);
  n->limp[0] = n->bits = n->dirty = 0;
}

void
b2n_set_ui (b2n_ptr n, unsigned int val)
{
#if CHUNK_BITS < 32
  int i, chunks;

  chunks = (CHUNK_BYTES - 1 + sizeof (val))/CHUNK_BYTES;

  b2n_resize (n, chunks );

  for (i = 0; i < chunks; i++)
    {
      n->limp[i] = val & CHUNK_BMASK;
      val >>= CHUNK_BITS;
    }
#else
  b2n_resize (n, 1);
  n->limp[0] = val;
#endif
  n->dirty = 1;
}

/* Only takes hex at the moment */

void
b2n_set_str (b2n_ptr n, char *str)
{
  int i, j, w, len, chunks;
  CHUNK_TYPE tmp;

  if (strncasecmp (str, "0x", 2))
    return;

  /* Make the hex string even lengthed */
  len = strlen (str) - 2;
  if (len & 1)
    {
      len ++;
      str ++;
    }
  else
    str += 2;

  len /= 2;

  chunks = (CHUNK_BYTES - 1 + len)/CHUNK_BYTES;
  b2n_resize (n, chunks);
  memset (n->limp, 0, CHUNK_BYTES * n->chunks);

  for (w = 0, i = 0; i < chunks; i++)
    {
      tmp = 0;
      for (j = (i == 0 ? ((len-1) % CHUNK_BYTES)+1 : CHUNK_BYTES); j > 0; j--)
	{
	  tmp <<= 8;
	  tmp |= (hex2int(str[w]) << 4) | hex2int(str[w+1]);
	  w += 2;
	}
      n->limp[chunks-1-i] = tmp;
    }

  n->dirty = 1;
}

/* Output function, mainly for debugging perpurses */

void
b2n_print (b2n_ptr n)
{
  int i, j, w, flag = 0;
  int left;
  char buffer[2*CHUNK_BYTES];
  CHUNK_TYPE tmp;

  left = ((((7 + b2n_sigbit (n)) >> 3) - 1) % CHUNK_BYTES) + 1;
  printf("0x");
  for (i = 0; i < n->chunks; i++)
    {
      tmp = n->limp[n->chunks-1-i];
      memset (buffer, '0', sizeof (buffer));
      for (w = 0, j = (i == 0 ? left : CHUNK_BYTES); j > 0; j--)
	{
	  buffer[w++] = int2hex[(tmp >> 4) & 0xF];
	  buffer[w++] = int2hex[tmp & 0xF];
	  tmp >>= 8;
	}

      for (j = (i == 0 ? left - 1: CHUNK_BYTES - 1); j >= 0; j--)
	if (flag || (i == n->chunks - 1 && j == 0) ||
	    buffer[2*j] != '0' || buffer[2*j+1] != '0')
	  {
	    putchar (buffer[2*j]);
	    putchar (buffer[2*j+1]);
	    flag = 1;
	  }
    }
  printf("\n");
}

int
b2n_sprint (char *buf, b2n_ptr n)
{
  int i, k, j, w, flag = 0;
  int left;
  char buffer[2*CHUNK_BYTES];
  CHUNK_TYPE tmp;

  left = ((((7 + b2n_sigbit (n)) >> 3) - 1) % CHUNK_BYTES) + 1;

  strcpy (buf, "0x"); k = 2;
  for (i = 0; i < n->chunks; i++)
    {
      tmp = n->limp[n->chunks-1-i];
      memset (buffer, '0', sizeof (buffer));
      for (w = 0, j = (i == 0 ? left : CHUNK_BYTES); j > 0; j--)
	{
	  buffer[w++] = int2hex[(tmp >> 4) & 0xF];
	  buffer[w++] = int2hex[tmp & 0xF];
	  tmp >>= 8;
	}

      for (j = (i == 0 ? left - 1: CHUNK_BYTES - 1); j >= 0; j--)
	if (flag || (i == n->chunks - 1 && j == 0) ||
	    buffer[2*j] != '0' || buffer[2*j+1] != '0')
	  {
	    buf[k++] = buffer[2*j];
	    buf[k++] = buffer[2*j+1];
	    flag = 1;
	  }
    }

  buf [k++] = 0;
  return k;
}

/* Arithmetic functions */

u_int32_t
b2n_sigbit (b2n_ptr n)
{
  int i, j;
  
  if (!n->dirty)
    return n->bits;

  for (i = n->chunks-1; i > 0; i--)
    if (n->limp[i])
      break;

  if (!n->limp[i])
    return 0;

  for (j = CHUNK_MASK; j > 0; j--)
    if (n->limp[i] & b2n_mask[j])
      break;
  
  n->bits = (i << CHUNK_SHIFTS) + j + 1;
  n->dirty = 0;
  return n->bits;
}


/*
 * Addition on GF(2)[x] is nice, its just an XOR.
 */

void
b2n_add (b2n_ptr d, b2n_ptr a, b2n_ptr b)
{
  int i;
  b2n_ptr bmin, bmax;

  if (!b2n_cmp_null (a))
    {
      b2n_set (d, b);
      return;
    }

  if (!b2n_cmp_null (b))
    {
      b2n_set (d, a);
      return;
    }

  bmin = B2N_MIN (a,b);
  bmax = B2N_MAX (a,b);
    
  b2n_resize (d, bmax->chunks);

  for (i = 0; i < bmin->chunks; i++)
      d->limp[i] = bmax->limp[i] ^ bmin->limp[i];

  /* 
   * If d is not bmax, we have to copy the rest of the bytes, and also
   * need to adjust to number of relevant bits.
   */
  if (d != bmax)
    {
      for ( ; i < bmax->chunks; i++) 
	  d->limp[i] = bmax->limp[i];
      
      d->bits = bmax->bits;
    }

  /*
   * Help to converse memory. When the result of the addition is zero
   * truncate the used amount of memory.
   */
  if (d != bmax && !b2n_cmp_null (d)) 
      b2n_set_null (d);
  else
    d->dirty = 1;
}


/*
 * Compare two polynomials.
 */

int
b2n_cmp (b2n_ptr n, b2n_ptr m)
{
  int sn, sm;
  int i;

  sn = b2n_sigbit (n);
  sm = b2n_sigbit (m);

  if (sn > sm)
    return 1;
  if (sn < sm)
    return -1;

  for (i = n->chunks-1; i >= 0; i--)
    if (n->limp[i] > m->limp[i])
      return 1;
    else if (n->limp[i] < m->limp[i])
      return -1;
  
  return 0;
}

int
b2n_cmp_null (b2n_ptr a)
{
  int i = 0;

  do
    {
      if (a->limp[i])
	return 1;
    } while (++i < a->chunks);

  return 0;
}

/*
 * Left shift, needed for polynomial multiplication.
 */

void
b2n_lshift (b2n_ptr d, b2n_ptr n, unsigned int s)
{
  int i, maj, min, chunks;
  u_int16_t bits = b2n_sigbit (n), add;
  CHUNK_TYPE *p, *op;

  if (!s)
    {
      b2n_set (d, n);
      return;
    }

  maj = s >> CHUNK_SHIFTS;
  min = s & CHUNK_MASK;

  add = (!(bits&CHUNK_MASK) || ((bits&CHUNK_MASK) + min) > CHUNK_MASK) ? 1 : 0;
  chunks = n->chunks;
  b2n_resize (d, chunks + maj + add);
  memmove (d->limp + maj, n->limp, CHUNK_BYTES * chunks);

  if (maj)
    memset (d->limp, 0, CHUNK_BYTES * maj);
  if (add) 
    d->limp[d->chunks-1] = 0;

  /* If !min there are no bit shifts, we are done */
  if (!min)
    return;

  op = p = &d->limp[d->chunks-1];
  for (i = d->chunks-2; i >= maj; i--)
    {
      op--;
      *p-- = (*p << min) | (*op >> (CHUNK_BITS - min));
    }
  *p <<= min;

  d->dirty = 0;
  d->bits = bits + (maj << CHUNK_SHIFTS) + min;
}

/*
 * Right shift, needed for polynomial division.
 */

void
b2n_rshift (b2n_ptr d, b2n_ptr n, unsigned int s)
{
  int maj, min, size = n->chunks, newsize;
  b2n_ptr tmp;

  if (!s)
    {
      b2n_set (d, n);
      return;
    }

  maj = s >> CHUNK_SHIFTS;

  newsize = size - maj;

  if (size < maj)
    {
      b2n_set_null (d);
      return;
    }

  min = (CHUNK_BITS - (s & CHUNK_MASK)) & CHUNK_MASK;
  if (min)
    {
      if ((b2n_sigbit (n) & CHUNK_MASK) > min)
	newsize++;

      b2n_lshift (d, n, min);
      tmp = d;
    }
  else
    tmp = n;

  memmove (d->limp, tmp->limp+maj+(min ? 1 : 0), CHUNK_BYTES * newsize);
  b2n_resize (d, newsize);

  d->bits = tmp->bits - ((maj + (min ? 1 : 0)) << CHUNK_SHIFTS);
}

/*
 * Normal polynomial multiplication.
 */

void
b2n_mul (b2n_ptr d, b2n_ptr n, b2n_ptr m)
{
  int i, j;
  b2n_t tmp, tmp2;

  if (!b2n_cmp_null (m) || !b2n_cmp_null (n))
    {
      b2n_set_null (d);
      return;
    }

  if (b2n_sigbit (m) == 1)
    {
      b2n_set (d, n);
      return;
    }

  if (b2n_sigbit (n) == 1)
    {
      b2n_set (d, m);
      return;
    }
  
  b2n_init (tmp);
  b2n_init (tmp2);

  b2n_set (tmp, B2N_MAX (n, m));
  b2n_set (tmp2, B2N_MIN (n, m));

  b2n_set_null (d);

  for (i = 0; i < tmp2->chunks; i++)
    if (tmp2->limp[i])
      for (j = 0; j < CHUNK_BITS; j++)
	{
	  if (tmp2->limp[i] & b2n_mask[j]) 
	    b2n_add (d, d, tmp);
	  
	  b2n_lshift (tmp, tmp, 1);
	}
    else
      b2n_lshift (tmp, tmp, CHUNK_BITS);

  b2n_clear (tmp);
  b2n_clear (tmp2);
}

/*
 * Squaring in this polynomial ring is more efficient than normal
 * multiplication.
 */

void
b2n_square (b2n_ptr d, b2n_ptr n)
{
  int i, j, maj, min, bits, chunk;
  b2n_t t;

  maj = b2n_sigbit (n);
  min = maj & CHUNK_MASK;
  maj = (maj + CHUNK_MASK) >> CHUNK_SHIFTS;

  b2n_init (t);
  b2n_resize (t, 2*maj + ((CHUNK_MASK + 2*min) >> CHUNK_SHIFTS));

  chunk = 0;
  bits = 0;

  for (i = 0; i < maj; i++)
    if (n->limp[i])
      for (j = 0; j < CHUNK_BITS; j++)
	{
	  if (n->limp[i] & b2n_mask[j])
	    t->limp[chunk] ^= b2n_mask[bits];

	  bits += 2;
	  if (bits >= CHUNK_BITS)
	    {
	      chunk++;
	      bits &= CHUNK_MASK;
	    }
	}
    else
	chunk += 2;
 
  t->dirty = 1;
  B2N_SWAP (d, t);
  b2n_clear (t);
}

/*
 * Normal polynomial division.
 * These functions are far from optimal in speed.
 */

void
b2n_div_q (b2n_ptr d, b2n_ptr n, b2n_ptr m)
{
  b2n_t r;

  b2n_init (r);
  b2n_div (d, r, n, m);
  b2n_clear (r);
}

void
b2n_div_r (b2n_ptr r, b2n_ptr n, b2n_ptr m)
{
  b2n_t q;

  b2n_init (q);
  b2n_div (q, r, n, m);
  b2n_clear (q);
}

void
b2n_div (b2n_ptr q, b2n_ptr r, b2n_ptr n, b2n_ptr m)
{
  int sn, sm, i, j, len, bits;
  b2n_t nenn, div, shift, mask;

  /* If Teiler > Zaehler, the result is 0 */
  if ((sm = b2n_sigbit (m)) > (sn = b2n_sigbit (n)))
    {
      b2n_set_null (q);
      b2n_set (r, n);
      return;
    }

  if (sm == 0)
    /* Division by Zero */
    return;
  else if (sm == 1)
    {
      /* Division by the One-Element */
      b2n_set (q, n);
      b2n_set_null (r);
      return;
    }

  b2n_init (nenn);
  b2n_init (div);
  b2n_init (shift);
  b2n_init (mask);

  b2n_set (nenn, n);
  b2n_set (div, m);
  b2n_set (shift, m);
  b2n_set_ui (mask, 1);

  b2n_resize (q, (sn - sm + CHUNK_MASK) >> CHUNK_SHIFTS);
  memset (q->limp, 0, CHUNK_BYTES * q->chunks);

  b2n_lshift (shift, shift, sn - sm);
  b2n_lshift (mask, mask, sn - sm);
  
  /* Number of significant octets */
  len = (sn - 1) >> CHUNK_SHIFTS;
  /* The first iteration is done over the relevant bits */
  bits = (CHUNK_MASK + sn) & CHUNK_MASK;
  for (i = len; i >= 0 && b2n_sigbit (nenn) >= sm; i--)
    for (j = (i == len ? bits : CHUNK_MASK); j >= 0 && b2n_sigbit (nenn) >= sm; j--)
      {
	if (nenn->limp[i] & b2n_mask[j])
	  {
	    b2n_sub (nenn, nenn, shift);
	    b2n_add (q, q, mask);
	  }
	b2n_rshift (shift, shift, 1);
	b2n_rshift (mask, mask, 1);
      }


  B2N_SWAP (r, nenn);

  b2n_clear (nenn);
  b2n_clear (div);
  b2n_clear (shift);
}


/*
 * Functions for Operation on GF(2**n) ~= GF(2)[x]/p(x).
 */

void
b2n_mod (b2n_ptr m, b2n_ptr n, b2n_ptr p)
{
  int bits, size;
  b2n_div_r (m, n, p);

  bits = b2n_sigbit (m);
  size = ((CHUNK_MASK + bits) >> CHUNK_SHIFTS);
  if (size == 0)
    size = 1;
  if (m->chunks > size)
      b2n_resize (m, size);

  m->bits = bits;
  m->dirty = 0;
}

void
b2n_gcd (b2n_ptr e, b2n_ptr go, b2n_ptr ho)
{
  b2n_t g, h;

  b2n_init (g); b2n_set (g, go);
  b2n_init (h); b2n_set (h, ho);
  
  while (b2n_cmp_null (h))
    {
      b2n_mod (g, g, h);
      B2N_SWAP (g,h);
    }

  B2N_SWAP (e, g);

  b2n_clear (g);
  b2n_clear (h);
}

void
b2n_mul_inv (b2n_ptr ga, b2n_ptr be, b2n_ptr p)
{
  b2n_t a;

  b2n_init (a);
  b2n_set_ui (a,1);

  b2n_div_mod (ga, a, be, p);

  b2n_clear (a);
}

void
b2n_div_mod (b2n_ptr ga, b2n_ptr a, b2n_ptr be, b2n_ptr p)
{
  b2n_t s0, s1, s2, q, r0, r1;

  /* There is no multiplicative inverse to Null */
  if (!b2n_cmp_null(be))
    {
      b2n_set_null (ga);
      return;
    }

  b2n_init (s0); b2n_init (s1); b2n_init (s2);
  b2n_init (r0); b2n_init (r1);
  b2n_init (q);

  b2n_set (r0, p);
  b2n_set (r1, be);

  b2n_set_null (s0);
  b2n_set (s1, a);

  while (b2n_cmp_null (r1))
    {
      b2n_div(q, r0, r0, r1);
      B2N_SWAP (r0, r1);

      b2n_mul (s2, q, s1);
      b2n_mod (s2, s2, p);
      b2n_sub (s2, s0, s2);

      B2N_SWAP (s0, s1);
      B2N_SWAP (s1, s2);
    }
  B2N_SWAP (ga, s0);

  b2n_clear (s0); b2n_clear (s1); b2n_clear (s2);
  b2n_clear (r0); b2n_clear (r1);
  b2n_clear (q);
}

/*
 * The trace tells us if there do exist any square roots 
 * for 'a' in GF(2)[x]/p(x). The number of square roots is
 * 2 - 2*Trace. 
 * If z is a square root, z + 1 is the other.
 */

void
b2n_trace (b2n_ptr ho, b2n_ptr a, b2n_ptr p)
{
  int i, m = b2n_sigbit (p) - 1;
  b2n_t h;

  b2n_init (h);
  b2n_set (h, a);
  
  for (i = 0; i < m - 1; i++)
    {
      b2n_square (h, h);
      b2n_mod (h, h, p);

      b2n_add (h, h, a);
    }
  B2N_SWAP (ho, h);

  b2n_clear (h);
}

/*
 * The halftrace yields the square root if the degree of the
 * irreduceable polynomial is odd.
 */

void
b2n_halftrace (b2n_ptr ho, b2n_ptr a, b2n_ptr p)
{
  int i, m = b2n_sigbit (p) - 1;
  b2n_t h;

  b2n_init (h);
  b2n_set (h, a);
  
  for (i = 0; i < (m - 1)/2; i++)
    {
      b2n_square (h, h);
      b2n_mod (h, h, p);
      b2n_square (h, h);
      b2n_mod (h, h, p);

      b2n_add (h, h, a);
    }

  B2N_SWAP (ho, h);

  b2n_clear (h);
}

/*
 * Solving the equation: y**2 + y = b in GF(2**m) where ip is the 
 * irreduceable polynomial. If m is odd, use the half trace.
 */

void
b2n_sqrt (b2n_ptr zo, b2n_ptr b, b2n_ptr ip)
{
  int i, m = b2n_sigbit (ip) - 1;
  b2n_t w, p, temp, z;

  if (!b2n_cmp_null (b))
    {
      b2n_set_null (z);
      return;
    }

  if (m & 1)
    {
      b2n_halftrace (zo, b, ip);
      return;
    }

  b2n_init (z);
  b2n_init (w);
  b2n_init (p);
  b2n_init (temp);
  do {
    b2n_random (p, m);
    b2n_set_null (z);
    b2n_set (w, p);
    for (i = 1; i < m; i++)
      {
	b2n_square (z, z);	/* z**2 */
	b2n_mod (z, z, ip);

	b2n_square (w, w);	/* w**2 */
	b2n_mod (w, w, ip);

	b2n_mul (temp, w, b);   /* w**2 * b */
	b2n_mod (temp, temp, ip);
	b2n_add (z, z, temp);   /* z**2 + w**2 + b */

	b2n_add (w, w, p);	/* w**2 + p */
      }
  } while (!b2n_cmp_null (w));

  B2N_SWAP (zo, z);

  b2n_clear (w);
  b2n_clear (p);
  b2n_clear (temp);
  b2n_clear (z);
}


/*
 * Exponentiation modulo a polynomial.
 */

void
b2n_exp_mod (b2n_ptr d, b2n_ptr b0, u_int32_t e, b2n_ptr p)
{
  b2n_t u, b;

  b2n_init (u);
  b2n_set_ui (u, 1);
  b2n_init (b);
  b2n_mod (b, b0, p);
  
  while (e)
    {
      if (e & 1)
	{
	  b2n_mul (u, u, b);
	  b2n_mod (u, u, p);
	}
      b2n_square (b, b);
      b2n_mod (b, b, p);
      e >>= 1;
    }

  B2N_SWAP (d, u);
  b2n_clear (u);
  b2n_clear (b);
}

/*
 * Low-level function to speed up scalar multiplication with
 * elliptic curves.
 * Multiplies a normal number by 3.
 */ 

/*
 * Normal addition behaves as Z_{2**n} and not F_{2**n}.
 */

void
b2n_nadd (b2n_ptr d0, b2n_ptr a0, b2n_ptr b0)
{
  int i, carry;

  b2n_ptr a, b;
  b2n_t d;

  if (!b2n_cmp_null (a0))
    {
      b2n_set (d0, b0);
      return;
    }

  if (!b2n_cmp_null (b0))
    {
      b2n_set (d0, a0);
      return;
    }

  b2n_init (d);
  a = B2N_MAX (a0, b0);
  b = B2N_MIN (a0, b0);

  b2n_resize (d, a->chunks + 1);

  for (carry = i = 0; i < b->chunks; i++)
    {
      d->limp[i] = a->limp[i] + b->limp[i] + carry;
      carry = (d->limp[i] < a->limp[i] ? 1 : 0);
    }
  
  for ( ; i < a->chunks && carry; i++)
    {
      d->limp[i] = a->limp[i] + carry;
      carry = (d->limp[i] < a->limp[i] ? 1 : 0);
    }

  if (i < a->chunks)
      memcpy (d->limp + i, a->limp + i, CHUNK_BYTES*(a->chunks - i));

  d->dirty = 1;
  B2N_SWAP (d0, d);

  b2n_clear (d);
}

/*
 * Very special sub, a > b.
 */

void
b2n_nsub (b2n_ptr d0, b2n_ptr a, b2n_ptr b)
{
  int i, carry;
  b2n_t d;

  if (b2n_cmp (a, b) <= 0)
    {
      b2n_set_null (d0);
      return;
    }
  
  b2n_init (d);
  b2n_resize (d, a->chunks);

  for (carry = i = 0; i < b->chunks; i++)
    {
      d->limp[i] = a->limp[i] - b->limp[i] - carry;
      carry = (d->limp[i] > a->limp[i] ? 1 : 0);
    }
  
  for ( ; i < a->chunks && carry; i++)
    {
      d->limp[i] = a->limp[i] - carry;
      carry = (d->limp[i] > a->limp[i] ? 1 : 0);
    }

  if (i < a->chunks)
      memcpy (d->limp + i, a->limp + i, CHUNK_BYTES*(a->chunks - i));

  d->dirty = 1;

  B2N_SWAP (d0, d);

  b2n_clear (d);
}

void
b2n_3mul (b2n_ptr d0, b2n_ptr e)
{
  b2n_t d;

  b2n_init (d);
  b2n_lshift (d, e, 1);

  b2n_nadd (d0, d, e);

  b2n_clear (d);
}
