/*
%%% copyright-nrl-95
This software is Copyright 1995-1998 by Randall Atkinson, Ronald Lee,
Daniel McDonald, Bao Phan, and Chris Winters. All Rights Reserved. All
rights under this copyright have been assigned to the US Naval Research
Laboratory (NRL). The NRL Copyright Notice and License Agreement Version
1.1 (January 17, 1995) applies to this software.
You should have received a copy of the license with this software. If you
didn't get a copy, you may request one from <license@ipv6.nrl.navy.mil>.

*/
/*
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * (Originally from)	@(#)in_cksum.c	7.3 (Berkeley) 6/28/90
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/mbuf.h>
#include <sys/systm.h>

#include <netinet/in.h>
#include <netinet6/in6.h>
#include <netinet6/ipv6.h>

#define ADDCARRY(x)  (x > 65535 ? x -= 65535 : x)
#define REDUCE \
{l_util.l = sum; sum = l_util.s[0] + l_util.s[1]; ADDCARRY(sum);}

/*----------------------------------------------------------------------
 * in6_cksum() takes an mbuf chain (with a IPv6 header at the beginning), and
 * computes the checksum.   This also assumes that the IPv6 header is pulled 
 * up into the first mbuf in the chain, and that said header has valid
 * source and destination fields.  I do the pseudo-header first, then
 * I do the rest.  Unlike v4, I treat the pseudo-header separately, so
 * transports don't have to do funky gymnastics with zeroing out headers.
 ----------------------------------------------------------------------*/

int
in6_cksum(m, proto, len, start)
     struct mbuf *m;	/* Chain, complete with IPv6 header. */
     int proto;		/* Protocol number of HLP that needs sum. */
     uint len;		/* Length of stuff to checksum.   Note that len
			   and proto are what get computed in the pseudo-
			   header.  Len is a uint because of potential
			   jumbograms. */
     uint start;	/* How far (in bytes) into chain's data to
			   start remainder of computation.  (e.g. Where
			   the TCP segment begins. */
{
  u_short *w;
  int sum = 0;
  int mlen = 0;
  int byte_swapped = 0;
  struct ipv6 *header;
  short done = 0;
  union 
    {
      uint8_t	c[2];
      uint16_t	s;
    } s_util;
  union
    {
      uint16_t	s[2];
      uint32_t	l;
    } l_util;

  /*
   * Get pseudo-header summed up.  Assume ipv6 is first.
   * I do pseudo-header here because it'll save bletch in both TCP and
   * UDP.
   */
  
  header = mtod(m,struct ipv6 *);

  w = (u_short *)&(header->ipv6_src);
  /* Source address */
  sum+=w[0]; sum+=w[1]; sum+=w[2]; sum+=w[3];
  sum+=w[4]; sum+=w[5]; sum+=w[6]; sum+=w[7];

  /* Destination address */
  sum+=w[8]; sum+=w[9]; sum+=w[10]; sum+=w[11];
  sum+=w[12]; sum+=w[13]; sum+=w[14]; sum+=w[15];

  /* Next header value for transport layer. */
  sum += htons(proto);

  /* Length of transport header and transport data. */
  l_util.l = htonl(len);
  sum+=l_util.s[0];
  sum+=l_util.s[1];

  /* Find starting point for rest of data.. */
  
  while (!done)
    if (m->m_len >start)
      {
	done = 1;
	mlen = m->m_len - start;
      }
    else
      {
	start -= m->m_len;
	m = m->m_next;
      }

  for (;m && len; m = m->m_next) {
    if (m->m_len == 0)
      continue;

    w = (u_short *)(m->m_data + (done ? start : 0));

    if (mlen == -1) 
      {
	/*
	 * The first byte of this mbuf is the continuation
	 * of a word spanning between this mbuf and the
	 * last mbuf.
	 *
	 * s_util.c[0] is already saved when scanning previous 
	 * mbuf.
	 */
	s_util.c[1] = *(char *)w;
	sum += s_util.s;
	w = (u_short *)((char *)w + 1);
	mlen = m->m_len - 1;
	len--;
      }
    else 
      if (!done)
	mlen = m->m_len;
      else done=0;
    
    if (len < mlen)
      mlen = len;
    len -= mlen;

    /*
     * Force to even boundary.
     */
#ifdef	__alpha__
    if ((1 & (long) w) && (mlen > 0)) {
#else
    if ((1 & (int) w) && (mlen > 0)) {
#endif
      REDUCE;
      sum <<= 8;
      s_util.c[0] = *(u_char *)w;
      w = (u_short *)((char *)w + 1);
      mlen--;
      byte_swapped = 1;
    }
    /*
     * Unroll the loop to make overhead from
     * branches small.
     */
    while ((mlen -= 32) >= 0) {
      sum += w[0]; sum += w[1]; sum += w[2]; sum += w[3];
      sum += w[4]; sum += w[5]; sum += w[6]; sum += w[7];
      sum += w[8]; sum += w[9]; sum += w[10]; sum += w[11];
      sum += w[12]; sum += w[13]; sum += w[14]; sum += w[15];
      w += 16;
    }
    mlen += 32;
    while ((mlen -= 8) >= 0) {
      sum += w[0]; sum += w[1]; sum += w[2]; sum += w[3];
      w += 4;
    }
    mlen += 8;
    if (mlen == 0 && byte_swapped == 0)
      continue;
    REDUCE;
    while ((mlen -= 2) >= 0) {
      sum += *w++;
    }
    if (byte_swapped) {
      REDUCE;
      sum <<= 8;
      byte_swapped = 0;
      if (mlen == -1) {
	s_util.c[1] = *(char *)w;
	sum += s_util.s;
	mlen = 0;
      } else
	mlen = -1;
    } else if (mlen == -1)
      s_util.c[0] = *(char *)w;
  }
  if (len)
    printf("in6_cksum: out of data\n");
  if (mlen == -1) {
    /*
     * The last mbuf has odd # of bytes. Follow the
     * standard (the odd byte may be shifted left by 8 bits
     * or not as determined by endian-ness of the machine)
     */
    s_util.c[1] = 0;
    sum += s_util.s;
  }
  REDUCE;
  return(~sum & 0xffff);
}
