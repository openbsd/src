/*	$Id: 3ecb_enc.c,v 1.1.1.1 1995/12/14 06:52:45 tholo Exp $	*/

/* Copyright (C) 1993 Eric Young - see README for more details */
#include "des_locl.h"

int des_3ecb_encrypt(des_cblock (*input), des_cblock (*output), struct des_ks_struct *ks1, struct des_ks_struct *ks2, int encrypt)
{
  register u_int32_t l0,l1;
  register unsigned char *in,*out;
  u_int32_t ll[2];

  in=(unsigned char *)input;
  out=(unsigned char *)output;
  c2l(in,l0);
  c2l(in,l1);
  ll[0]=l0;
  ll[1]=l1;
  des_encrypt(ll,ll,ks1,encrypt);
  des_encrypt(ll,ll,ks2,!encrypt);
  des_encrypt(ll,ll,ks1,encrypt);
  l0=ll[0];
  l1=ll[1];
  l2c(l0,out);
  l2c(l1,out);
  return(0);
}

