/*	$OpenBSD: if.c,v 1.2 1998/11/15 00:43:54 niklas Exp $	*/

/*
 * Copyright (c) 1998 Niklas Hallqvist.  All rights reserved.
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

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <stdlib.h>
#include <unistd.h>

#include "if.h"

/* XXX Unsafe if either x or y has side-effects.  */
#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Most boxes has less than 16 interfaces, so this might be a good guess.  */
#define INITIAL_IFREQ_COUNT 16

/*
 * Get all network interface configurations.
 * Return 0 if successful, -1 otherwise.
 */
int
siocgifconf (struct ifconf *ifcp)
{
  int s;
  int len;
  caddr_t buf, new_buf;

  /* Get a socket to ask for the network interface configurations.  */
  s = socket (AF_INET, SOCK_DGRAM, 0);
  if (s == -1)
    return -1;

  len = sizeof (struct ifreq) * INITIAL_IFREQ_COUNT;
  buf = 0;
  while (1)
    {
      /*
       * Allocate a larger buffer each time around the loop and get the
       * network interfaces configurations into it.
       */
      ifcp->ifc_len = len;
      new_buf = realloc (buf, len);
      if (!new_buf)
	goto err;
      ifcp->ifc_buf = buf = new_buf;
      if (ioctl (s, SIOCGIFCONF, ifcp) == -1)
	goto err;

      /*
       * If there is place for another ifreq we can be sure that the buffer
       * was big enough, otherwise double the size and try again.
       */
      if (len - ifcp->ifc_len >= sizeof (struct ifreq))
	break;
      len *= 2;
    }
  close (s);
  return 0;
  
err:
  if (buf)
    free (buf);
  close (s);
  return -1;
}

int
if_map (void (*func) (struct ifreq *, void *), void *arg)
{
  struct ifconf ifc;
  struct ifreq *ifrp;
  caddr_t limit, p;
  size_t len;

  if (siocgifconf (&ifc))
    return -1;

  limit = ifc.ifc_buf + ifc.ifc_len;
  for (p = ifc.ifc_buf; p < limit; p += len)
    {
      ifrp = (struct ifreq *)p;
      (*func) (ifrp, arg);
      len = sizeof ifrp->ifr_name
	+ MAX (ifrp->ifr_addr.sa_len, sizeof ifrp->ifr_addr);
    }
  return 0;
}
