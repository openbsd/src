/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "rxkad_locl.h"

RCSID("$KTH: rxk_locl.c,v 1.23 1999/12/02 16:58:55 joda Exp $");

/* The header checksum is the last 16 bits of this struct after
 * encryption. Note that only the last 8 bytes change per packet. */
#if 0
struct header_data {
  struct const_header_data c;	/* 16 bytes */
  struct variable_header_data v; /* 8 bytes */
};
#endif

struct const_header_data {
  /* Data that is constant per connection */
  u_int32 epoch;
  u_int32 cid;
  u_int32 zero;
  u_int32 security_index;
};

struct variable_header_data {
  /* Data that changes per packet */
  u_int32 call_number;
  u_int32 channel_and_seq;
};

/* To create a 16 bit packet header checksum we first create an iv
 * dependent on the epoch, the connection ID and the security index.
 */
void
rxkad_calc_header_iv(const struct rx_connection *conn,
		     const int32 *sched,
		     const des_cblock *in_iv,
		     u_int32 *out_iv)
{
  struct const_header_data h;
  u_int32 *t;

  h.epoch = htonl(conn->epoch);
  h.cid = htonl(conn->cid & RX_CIDMASK);
  h.zero = 0;
  h.security_index = htonl(conn->securityIndex);

  t = (u_int32 *)in_iv;		/* memcpy(out_iv, in_iv, 8); */
  out_iv[0] = t[0];
  out_iv[1] = t[1];
  fc_cbc_encrypt(&h, &h, sizeof(h), sched, out_iv, ENCRYPT);
  /* Extract last 8 bytes as iv */
  assert(out_iv[0] == h.zero);
  /* out_iv[0] = h.zero; */
  out_iv[1] = h.security_index;
}

/* Make a 16 bit header checksum dependent on call number, channel
 * number and packet sequence number. In addition, the checksum is
 * indirectly dependent (via the iv) on epoch, connection ID and
 * security index.
 */
static
int
rxkad_cksum_header(const struct rx_packet *packet,
		   const int32 *sched,
		   const unsigned int *iv)
{
  struct variable_header_data h;
  u_int32 t;

  /* Collect selected packet fields */
  h.call_number = htonl(packet->header.callNumber);
  t = ((packet->header.cid & RX_CHANNELMASK) << (32 - RX_CIDSHIFT))
    | ((packet->header.seq & 0x3fffffff));
  h.channel_and_seq = htonl(t);

  /* Encrypt selected fields (this is hand rolled CBC mode) */
  h.call_number     ^= iv[0];
  h.channel_and_seq ^= iv[1];
  fc_ecb_encrypt(&h, &h, sched, ENCRYPT);

  /* Select 16 bits that are now dependent on all selected packet fields */
  t = (ntohl(h.channel_and_seq) >> 16) & 0xffff;
  if (t != 0)
    return t;
  else
    return 1;			/* No checksum is 0 */
}

/* Checksum a rxkad_response, this checksum is buried within the
 * encrypted part of the response but covers the entire response. */
u_int32
rxkad_cksum_response(rxkad_response *r)
{
  u_char *t;
  u_int32 cksum = 1000003;
  
  for (t = (u_char *)r; t < (u_char*)(r + 1); t++)
    cksum = *t + cksum * 0x10204081;
  
  return htonl(cksum);
}

int
rxkad_prepare_packet(struct rx_packet *pkt,
		     struct rx_connection *con,
		     int level,
		     key_stuff *k,
		     end_stuff *e)
{
  u_int len = rx_GetDataSize(pkt);

  /* Checksum header */
  rx_SetPacketCksum(pkt, rxkad_cksum_header(pkt, k->keysched, e->header_iv));

  e->packetsSent++;
  e->bytesSent += len;
  
  if (level != rxkad_clear)
    {
      u_int32 *data = (u_int32 *) rx_DataOf(pkt);
      u_int32 t;
      int32 code = 0;

      assert(pkt->wirevec[1].iov_len >= 4);

      /* First 4 bytes of security header, includes encrypted length */
      t = pkt->header.seq ^ pkt->header.callNumber;
      t <<= 16;
      t |= len;			/* Extracted on receiving side */
      data[0] = htonl(t);

      switch (level) {
      case rxkad_auth:
	len += rx_GetSecurityHeaderSize(con); /* Extended pkt len */
	/* Extend packet length so that we can encrypt the first 8 bytes */
	if (pkt->wirevec[1].iov_len < 8)
	  {
	    int diff = 8 - pkt->wirevec[1].iov_len;
	    pkt->wirevec[1].iov_len += diff;
	    len += diff;
	  }
	rx_SetDataSize(pkt, len); /* Set extended packet length */

	/* Encrypt security header (4 bytes) and the next 4 bytes */
	assert(pkt->wirevec[1].iov_len >= 8);
	fc_ecb_encrypt(data, data, k->keysched, ENCRYPT);
	break;

      case rxkad_crypt:
	len += rx_GetSecurityHeaderSize(con); /* Extended pkt len */
	 /* Round up to 8 byte boundary for encryption to work */
	if (len % 8)
	  {
	    int diff = 8 - (len % 8);
	    rxi_RoundUpPacket(pkt, diff);
	    len += diff;
	  }
	rx_SetDataSize(pkt, len); /* Set extended packet length */

	assert((len % 8) == 0);
	code = rxkad_EncryptPacket(con, k->keysched,(u_int32*)k->key, len,pkt);
	break;

      default:
	assert(0);
      }

      return code;
    }
  return 0;
}

int
rxkad_check_packet(struct rx_packet *pkt,
		   struct rx_connection *con,
		   int level,
		   key_stuff *k,
		   end_stuff *e)
{
  u_int xlen = rx_GetDataSize(pkt); /* Extended packet length */

  if (rx_GetPacketCksum(pkt)
      != rxkad_cksum_header(pkt, k->keysched, e->header_iv))
    return RXKADSEALEDINCON;
  
  e->packetsReceived++;

  if (level == rxkad_clear)
    {
      e->bytesReceived += xlen;	/* Same as real length */
    }
  else
    {
      u_int len;		/* Real packet length */
      u_int32 *data = (u_int32 *) rx_DataOf(pkt);
      u_int32 t;
      int32 code;

      switch (level) {
      case rxkad_auth:
	assert(rx_Contiguous(pkt) >= 8);
	fc_ecb_encrypt(data, data, k->keysched, DECRYPT);
	break;

      case rxkad_crypt:
	code = rxkad_DecryptPacket(con, k->keysched,(u_int32*)k->key, xlen, pkt);
	if (code)
	  return code;
	break;

      default:
	assert(0);
      }

      assert(rx_Contiguous(pkt) >= 4);

      t = ntohl(data[0]);
      len = t & 0xffff;		/* Extract real length */
      t >>= 16;
      if (t != ((pkt->header.seq ^ pkt->header.callNumber) & 0xffff))
	return RXKADSEALEDINCON;

#define TBYTES 15
      /* The packet is extended with 0 - 7 bytes to a chipher block
       * boundary. This is however not true with the Transarc
       * implementation, for unknown reasons it sometimes extendeds
       * the packet with anything up to TBYTES. */
      if (len > xlen)
	return RXKADSEALEDINCON;
      if (xlen > len + TBYTES)
	return RXKADSEALEDINCON;

      e->bytesReceived += len;
      rx_SetDataSize(pkt, len);	/* Set real packet length */
      return 0;
    }
  return 0;
}
