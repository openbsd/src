/*	$Id: pcbc_enc.c,v 1.1.1.1 1995/12/14 06:52:44 tholo Exp $	*/

/* Copyright (C) 1993 Eric Young - see README for more details */
#include "des_locl.h"

int des_pcbc_encrypt(des_cblock (*input), des_cblock (*output), long int length, struct des_ks_struct *schedule, des_cblock (*ivec), int encrypt)
{
  register u_int32_t sin0,sin1,xor0,xor1,tout0,tout1;
  u_int32_t tin[2],tout[2];
  unsigned char *in,*out,*iv;

  in=(unsigned char *)input;
  out=(unsigned char *)output;
  iv=(unsigned char *)ivec;

  if (encrypt)
    {
      c2l(iv,xor0);
      c2l(iv,xor1);
      for (; length>0; length-=8)
	{
	  if (length >= 8)
	    {
	      c2l(in,sin0);
	      c2l(in,sin1);
	    }
	  else
	    c2ln(in,sin0,sin1,length);
	  tin[0]=sin0^xor0;
	  tin[1]=sin1^xor1;
	  des_encrypt(tin,tout,
		      schedule,encrypt);
	  tout0=tout[0];
	  tout1=tout[1];
	  xor0=sin0^tout[0];
	  xor1=sin1^tout[1];
	  l2c(tout0,out);
	  l2c(tout1,out);
	}
    }
  else
    {
      c2l(iv,xor0); c2l(iv,xor1);
      for (; length>0; length-=8)
	{
	  c2l(in,sin0);
	  c2l(in,sin1);
	  tin[0]=sin0;
	  tin[1]=sin1;
	  des_encrypt(tin,tout,
		      schedule,encrypt);
	  tout0=tout[0]^xor0;
	  tout1=tout[1]^xor1;
	  if (length >= 8)
	    {
	      l2c(tout0,out);
	      l2c(tout1,out);
	    }
	  else
	    l2cn(tout0,tout1,out,length);
	  xor0=tout0^sin0;
	  xor1=tout1^sin1;
	}
    }
  tin[0]=tin[1]=tout[0]=tout[1]=0;
  sin0=sin1=xor0=xor1=tout0=tout1=0;
  return(0);
}
