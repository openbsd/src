/*	$Id: 3cbc_enc.c,v 1.1.1.1 1995/12/14 06:52:43 tholo Exp $	*/

/* Copyright (C) 1993 Eric Young - see README for more details */
#include "des_locl.h"

static void 
xp(des_cblock *arg)
{
  unsigned char *a = (unsigned char *) arg;
  int i;
  for(i=0; i<8; i++) printf("%02X",a[i]);printf("\n");
}

int des_3cbc_encrypt(des_cblock (*input), des_cblock (*output), long int length, struct des_ks_struct *ks1, struct des_ks_struct *ks2, des_cblock (*iv1), des_cblock (*iv2), int encrypt)
{
  int off=length/8-1;
  des_cblock niv1,niv2;

  printf("3cbc\n");
  xp(iv1);
  xp(iv1);
  xp(iv2);
  xp(input);
  if (encrypt == DES_ENCRYPT)
    {
      des_cbc_encrypt(input,output,length,ks1,iv1,encrypt);
      if (length >= sizeof(des_cblock))
	memcpy(niv1,output[off],sizeof(des_cblock));
      des_cbc_encrypt(output,output,length,ks2,iv1,!encrypt);
      des_cbc_encrypt(output,output,length,ks1,iv2, encrypt);
      if (length >= sizeof(des_cblock))
	memcpy(niv2,output[off],sizeof(des_cblock));
      memcpy(*iv1,niv1,sizeof(des_cblock));
    }
  else
    {
      if (length >= sizeof(des_cblock))
	memcpy(niv1,input[off],sizeof(des_cblock));
      des_cbc_encrypt(input,output,length,ks1,iv1,encrypt);
      des_cbc_encrypt(output,output,length,ks2,iv2,!encrypt);
      if (length >= sizeof(des_cblock))
	memcpy(niv2,output[off],sizeof(des_cblock));
      des_cbc_encrypt(output,output,length,ks1,iv2, encrypt);
    }
  memcpy(iv1,niv1,sizeof(des_cblock));
  memcpy(iv2,niv2,sizeof(des_cblock));
  xp(iv1);
  xp(iv1);
  xp(iv2);
  xp(output);
  return(0);
}
