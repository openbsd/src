/*	$Id: ofb_enc.c,v 1.1.1.1 1995/12/14 06:52:44 tholo Exp $	*/

/* Copyright (C) 1993 Eric Young - see README for more details */
#include "des_locl.h"

/* The input and output are loaded in multiples of 8 bits.
 * What this means is that if you hame numbits=12 and length=2
 * the first 12 bits will be retrieved from the first byte and half
 * the second.  The second 12 bits will come from the 3rd and half the 4th
 * byte.
 */
int des_ofb_encrypt(unsigned char *in, unsigned char *out, int numbits, long int length, struct des_ks_struct *schedule, des_cblock (*ivec))
{
  register u_int32_t d0,d1,v0,v1,n=(numbits+7)/8;
  register u_int32_t mask0,mask1;
  register long l=length;
  register int num=numbits;
  u_int32_t ti[2];
  unsigned char *iv;

  if (num > 64) return(0);
  if (num > 32)
    {
      mask0=0xffffffff;
      if (num >= 64)
	mask1=mask0;
      else
	mask1=(1L<<(num-32))-1;
    }
  else
    {
      if (num == 32)
	mask0=0xffffffff;
      else
	mask0=(1L<<num)-1;
      mask1=0x00000000;
    }

  iv=(unsigned char *)ivec;
  c2l(iv,v0);
  c2l(iv,v1);
  ti[0]=v0;
  ti[1]=v1;
  while (l-- > 0)
    {
      des_encrypt(ti,ti,
		  schedule,DES_ENCRYPT);
      c2ln(in,d0,d1,n);
      in+=n;
      d0=(d0^ti[0])&mask0;
      d1=(d1^ti[1])&mask1;
      l2cn(d0,d1,out,n);
      out+=n;
    }
  v0=ti[0];
  v1=ti[1];
  iv=(unsigned char *)ivec;
  l2c(v0,iv);
  l2c(v1,iv);
  v0=v1=d0=d1=ti[0]=ti[1]=0;
  return(0);
}

