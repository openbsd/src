/*	$Id: enc_read.c,v 1.1.1.1 1995/12/14 06:52:44 tholo Exp $	*/

/* Copyright (C) 1993 Eric Young - see README for more details */

#include "des_locl.h"
#include <unistd.h>
#include <errno.h>

/* This has some uglies in it but it works - even over sockets. */
extern int errno;
int des_rw_mode=DES_PCBC_MODE;

int des_enc_read(int fd, char *buf, int len, struct des_ks_struct *sched, des_cblock (*iv))
{
  /* data to be unencrypted */
  int net_num=0;
  unsigned char net[BSIZE];
  /* extra unencrypted data 
   * for when a block of 100 comes in but is des_read one byte at
   * a time. */
  static char unnet[BSIZE];
  static int unnet_start=0;
  static int unnet_left=0;
  int i;
  long num=0,rnum;
  unsigned char *p;

  /* left over data from last decrypt */
  if (unnet_left != 0)
    {
      if (unnet_left < len)
	{
	  /* we still still need more data but will return
	   * with the number of bytes we have - should always
	   * check the return value */
	  memcpy(buf,&(unnet[unnet_start]),unnet_left);
	  /* eay 26/08/92 I had the next 2 lines
	   * reversed :-( */
	  i=unnet_left;
	  unnet_start=unnet_left=0;
	}
      else
	{
	  memcpy(buf,&(unnet[unnet_start]),len);
	  unnet_start+=len;
	  unnet_left-=len;
	  i=len;
	}
      return(i);
    }

  /* We need to get more data. */
  if (len > MAXWRITE) len=MAXWRITE;

  /* first - get the length */
  net_num=0;
  while (net_num < HDRSIZE) 
    {
      i=read(fd,&(net[net_num]),HDRSIZE-net_num);
      if ((i == -1) && (errno == EINTR)) continue;
      if (i <= 0) return(0);
      net_num+=i;
    }

  /* we now have at net_num bytes in net */
  p=net;
  num=0;
  n2l(p,num);
  /* num should be rounded up to the next group of eight
   * we make sure that we have read a multiple of 8 bytes from the net.
   */
  if ((num > MAXWRITE) || (num < 0)) /* error */
    return(-1);
  rnum=(num < 8)?8:((num+7)/8*8);

  net_num=0;
  while (net_num < rnum)
    {
      i=read(fd,&(net[net_num]),rnum-net_num);
      if ((i == -1) && (errno == EINTR)) continue;
      if (i <= 0) return(0);
      net_num+=i;
    }

  /* Check if there will be data left over. */
  if (len < num)
    {
      if (des_rw_mode & DES_PCBC_MODE)
	des_pcbc_encrypt((des_cblock *)net,(des_cblock *)unnet,
		     num,sched,iv,DES_DECRYPT);
      else
	des_cbc_encrypt((des_cblock *)net,(des_cblock *)unnet,
		    num,sched,iv,DES_DECRYPT);
      memcpy(buf,unnet,len);
      unnet_start=len;
      unnet_left=num-len;

      /* The following line is done because we return num
       * as the number of bytes read. */
      num=len;
    }
  else
    {
      /* >output is a multiple of 8 byes, if len < rnum
       * >we must be careful.  The user must be aware that this
       * >routine will write more bytes than he asked for.
       * >The length of the buffer must be correct.
       * FIXED - Should be ok now 18-9-90 - eay */
      if (len < rnum)
	{
	  char tmpbuf[BSIZE];

	  if (des_rw_mode & DES_PCBC_MODE)
	    des_pcbc_encrypt((des_cblock *)net,
			 (des_cblock *)tmpbuf,
			 num,sched,iv,DES_DECRYPT);
	  else
	    des_cbc_encrypt((des_cblock *)net,
			(des_cblock *)tmpbuf,
			num,sched,iv,DES_DECRYPT);

	  /* eay 26/08/92 fix a bug that returned more
	   * bytes than you asked for (returned len bytes :-( */
	  memcpy(buf,tmpbuf,num);
	}
      else if (num >= 8)
	{
	  if (des_rw_mode & DES_PCBC_MODE)
	    des_pcbc_encrypt((des_cblock *)net,
			 (des_cblock *)buf,num,sched,iv,
			 DES_DECRYPT);
	  else
	    des_cbc_encrypt((des_cblock *)net,
			(des_cblock *)buf,num,sched,iv,
			DES_DECRYPT);
	}
      else
	{
	  if (des_rw_mode & DES_PCBC_MODE)
	    des_pcbc_encrypt((des_cblock *)net,
			 (des_cblock *)buf,8,sched,iv,
			 DES_DECRYPT);
	  else
	    des_cbc_encrypt((des_cblock *)net,
			(des_cblock *)buf,8,sched,iv,
			DES_DECRYPT);
#ifdef LEFT_JUSTIFIED
	  memcpy(buf, buf, num);
#else
	  memcpy(buf, buf+(8-num), num);
#endif
	}
    }
  return(num);
}

