/* Copyright (C) 1993 Eric Young - see README for more details */
/* From "Message Authentication"  R.R. Jueneman, S.M. Matyas, C.H. Meyer
 * IEEE Communications Magazine Sept 1985 Vol. 23 No. 9 p 29-40
 * This module in only based on the code in this paper and is
 * almost definitely not the same as the MIT implementation.
 *
 *	$Id: qud_cksm.c,v 1.1.1.1 1995/12/14 06:52:43 tholo Exp $
 */
#include "des_locl.h"

/* bug fix for dos - 7/6/91 - Larry hughes@logos.ucs.indiana.edu */
#define B0(a)	(((u_int32_t)(a)))
#define B1(a)	(((u_int32_t)(a))<<8)
#define B2(a)	(((u_int32_t)(a))<<16)
#define B3(a)	(((u_int32_t)(a))<<24)

/* used to scramble things a bit */
/* Got the value MIT uses via brute force :-) 2/10/90 eay */
#define NOISE	((u_int32_t)83653421)

u_int32_t des_quad_cksum(des_cblock (*input), des_cblock (*output), long int length, int out_count, des_cblock (*seed))
{
  u_int32_t z0,z1,t0,t1;
  int i;
  long l=0;
  unsigned char *cp;
  unsigned char *lp;

  if (out_count < 1) out_count=1;
  lp=(unsigned char *)output;

  z0=B0((*seed)[0])|B1((*seed)[1])|B2((*seed)[2])|B3((*seed)[3]);
  z1=B0((*seed)[4])|B1((*seed)[5])|B2((*seed)[6])|B3((*seed)[7]);

  for (i=0; ((i<4)&&(i<out_count)); i++)
    {
      cp=(unsigned char *)input;
      l=length;
      while (l > 0)
	{
	  if (l > 1)
	    {
	      t0= (u_int32_t)(*(cp++));
	      t0|=(u_int32_t)B1(*(cp++));
	      l--;
	    }
	  else
	    t0= (u_int32_t)(*(cp++));
	  l--;
	  /* add */
	  t0+=z0;
	  t0&=0xffffffff;
	  t1=z1;
	  /* square, well sort of square */
	  z0=((((t0*t0)&0xffffffff)+((t1*t1)&0xffffffff))
	      &0xffffffff)%0x7fffffff; 
	  z1=((t0*((t1+NOISE)&0xffffffff))&0xffffffff)%0x7fffffff;
	}
      if (lp != NULL)
	{
	  /* I believe I finally have things worked out.
	   * The MIT library assumes that the checksum
	   * is one huge number and it is returned in a
	   * host dependant byte order.
	   */
	  static u_int32_t l=1;
	  static unsigned char *c=(unsigned char *)&l;

	  if (c[0])
	    {
	      l2c(z0,lp);
	      l2c(z1,lp);
	    }
	  else
	    {
	      lp=output[out_count-i-1];
	      l2n(z1,lp);
	      l2n(z0,lp);
	    }
	}
    }
  return(z0);
}

