/*	$Id: cbc_cksm.c,v 1.1.1.1 1995/12/14 06:52:44 tholo Exp $	*/

/* Copyright (C) 1993 Eric Young - see README for more details */
#include "des_locl.h"

u_int32_t des_cbc_cksum(des_cblock (*input), des_cblock (*output), long int length, struct des_ks_struct *schedule, des_cblock (*ivec))
{
  register u_int32_t tout0,tout1,tin0,tin1;
  register long l=length;
  u_int32_t tin[2],tout[2];
  unsigned char *in,*out,*iv;

  in=(unsigned char *)input;
  out=(unsigned char *)output;
  iv=(unsigned char *)ivec;

  c2l(iv,tout0);
  c2l(iv,tout1);
  for (; l>0; l-=8)
    {
      if (l >= 8)
	{
	  c2l(in,tin0);
	  c2l(in,tin1);
	}
      else
	c2ln(in,tin0,tin1,l);
			
      tin0^=tout0;
      tin1^=tout1;
      tin[0]=tin0;
      tin[1]=tin1;
      des_encrypt(tin,tout,
		  schedule,DES_ENCRYPT);
      /* fix 15/10/91 eay - thanks to keithr@sco.COM */
      tout0=tout[0];
      tout1=tout[1];
    }
  if (out != NULL)
    {
      l2c(tout0,out);
      l2c(tout1,out);
    }
  tout0=tin0=tin1=tin[0]=tin[1]=tout[0]=tout[1]=0;
  return(tout1);
}
