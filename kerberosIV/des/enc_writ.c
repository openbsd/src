/*	$Id: enc_writ.c,v 1.1.1.1 1995/12/14 06:52:44 tholo Exp $	*/

/* Copyright (C) 1993 Eric Young - see README for more details */

#include "des_locl.h"
#include <unistd.h>
#include <errno.h>

int des_enc_write(int fd, char *buf, int len, struct des_ks_struct *sched, des_cblock (*iv))
{
  long rnum;
  int i,j,k,outnum;
  char outbuf[BSIZE+HDRSIZE];
  char shortbuf[8];
  char *p;
  static int start=1;

  /* If we are sending less than 8 bytes, the same char will look
   * the same if we don't pad it out with random bytes */
  if (start)
    {
      start=0;
      srand(time(NULL));
    }

  /* lets recurse if we want to send the data in small chunks */
  if (len > MAXWRITE)
    {
      j=0;
      for (i=0; i<len; i+=k)
	{
	  k=des_enc_write(fd,&(buf[i]),
			  ((len-i) > MAXWRITE)?MAXWRITE:(len-i),sched,iv);
	  if (k < 0)
	    return(k);
	  else
	    j+=k;
	}
      return(j);
    }

  /* write length first */
  p=outbuf;
  l2n(len,p);

  /* pad short strings */
  if (len < 8)
    {
#ifdef LEFT_JUSTIFIED
      p=shortbuf;
      memcpy(shortbuf,buf,len);
      for (i=len; i<8; i++)
	shortbuf[i]=rand();
      rnum=8;
#else
      p=shortbuf;
      for (i=0; i<8-len; i++)
	shortbuf[i]=rand();
      memcpy(shortbuf + 8 - len, buf, len);
      rnum=8;
#endif
    }
  else
    {
      p=buf;
      rnum=((len+7)/8*8);	/* round up to nearest eight */
    }

  if (des_rw_mode & DES_PCBC_MODE)
    des_pcbc_encrypt((des_cblock *)p,(des_cblock *)&(outbuf[HDRSIZE]),
		 (long)((len<8)?8:len),sched,iv,DES_ENCRYPT); 
  else
    des_cbc_encrypt((des_cblock *)p,(des_cblock *)&(outbuf[HDRSIZE]),
		(long)((len<8)?8:len),sched,iv,DES_ENCRYPT); 

  /* output */
  outnum=rnum+HDRSIZE;

  for (j=0; j<outnum; j+=i)
    {
      /* eay 26/08/92 I was not doing writing from where we
       * got upto. */
      i=write(fd,&(outbuf[j]),(int)(outnum-j));
      if (i == -1)
	{
	  if (errno == EINTR)
	    i=0;
	  else			/* This is really a bad error - very bad
				 * It will stuff-up both ends. */
	    return(-1);
	}
    }

  return(len);
}
