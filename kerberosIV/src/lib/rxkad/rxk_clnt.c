/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
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

RCSID("$KTH: rxk_clnt.c,v 1.36 1999/12/02 16:58:54 joda Exp $");

/* This code also links into the kernel so we need to use osi_Alloc()
 * to avoid calling malloc(). Similar trick with memcpy() */

#undef osi_Alloc
#undef osi_Free
char *osi_Alloc(int32 size);
void osi_Free(void *p, int32 size);

#undef memcpy
#define memcpy(to, from, len) bcopy1((from), (to), (len))

static
void
bcopy1(const void *from_, void *to_, size_t n)
{
  char *to = to_;
  const char *from = from_;
  for (; n > 0; n--)
    {
      *to = *from;
      to++;
      from++;
    }
}

/* Security object specific client data */
typedef struct rxkad_clnt_class {
  struct rx_securityClass klass;
  rxkad_level level;
  key_stuff k;
  int32 kvno;
  int32 ticket_len;
  char *ticket;
} rxkad_clnt_class;

/* Per connection specific client data */
typedef struct clnt_con_data {
  end_stuff e;
} clnt_con_data;

static
int
client_NewConnection(struct rx_securityClass *obj_, struct rx_connection *con)
{
  rxkad_clnt_class *obj = (rxkad_clnt_class *) obj_;
  clnt_con_data *cdat;

  assert(con->securityData == 0);
  obj->klass.refCount++;
  cdat = (clnt_con_data *) osi_Alloc(sizeof(clnt_con_data));
  cdat->e.bytesReceived = cdat->e.packetsReceived = 0;
  cdat->e.bytesSent = cdat->e.packetsSent = 0;

  con->securityData = (char *) cdat;
  rx_nextCid += RX_MAXCALLS;
  con->epoch = rx_epoch;
  con->cid = rx_nextCid;
  /* We don't use trailers but the transarc implementation breaks when
   * we don't set the trailer size, packets get to large */
  switch (obj->level) {
  case rxkad_clear:
    /* nichts */
    break;
  case rxkad_auth:
    rx_SetSecurityHeaderSize(con, 4);
    rx_SetSecurityMaxTrailerSize(con, 4);
    break;
  case rxkad_crypt:
    rx_SetSecurityHeaderSize(con, 8);
    rx_SetSecurityMaxTrailerSize(con, 8);
    break;
  default:
    assert(0);
  }
  rxkad_calc_header_iv(con, obj->k.keysched,
		       (const des_cblock *)&obj->k.key, cdat->e.header_iv);
  return 0;
}

static
int
client_Close(struct rx_securityClass *obj_)
{
  rxkad_clnt_class *obj = (rxkad_clnt_class *) obj_;
  obj->klass.refCount--;
  if (obj->klass.refCount <= 0)
    {
      osi_Free(obj->ticket, obj->ticket_len);
      osi_Free(obj, sizeof(rxkad_clnt_class));
    }
  return 0;
}

static
int
client_DestroyConnection(struct rx_securityClass *obj,
			 struct rx_connection *con)
{
  clnt_con_data *cdat = (clnt_con_data *)con->securityData;
  
  if (cdat)
    osi_Free(cdat, sizeof(clnt_con_data));
  return client_Close(obj);
}

/*
 * Receive a challange and respond.
 */
static
int
client_GetResponse(const struct rx_securityClass *obj_,
		   const struct rx_connection *con,
		   struct rx_packet *pkt)
{
  rxkad_clnt_class *obj = (rxkad_clnt_class *) obj_;
  rxkad_challenge c;
  rxkad_response r;

  /* Get challenge */
  if (rx_SlowReadPacket(pkt, 0, sizeof(c), &c) != sizeof(c))
    return RXKADPACKETSHORT;

  if (ntohl(c.version) < RXKAD_VERSION)
    return RXKADINCONSISTENCY;	/* Don't know how to make vers 1 response. */
  /* Always make a vers 2 response. */

  if (ntohl(c.min_level) > obj->level)
    return RXKADLEVELFAIL;

  /* Make response */
  r.version = htonl(RXKAD_VERSION);
  r.unused = 0;
  r.encrypted.epoch = htonl(con->epoch);
  r.encrypted.cid = htonl(con->cid & RX_CIDMASK);
  r.encrypted.cksum = 0;
  r.encrypted.security_index = htonl(con->securityIndex);
  {
    int i;
    /* Get and fixup call number vector */
    rxi_GetCallNumberVector(con, r.encrypted.call_numbers);
    for (i = 0; i < RX_MAXCALLS; i++)
      {
	if (r.encrypted.call_numbers[i] < 0)
	  return RXKADINCONSISTENCY;
	r.encrypted.call_numbers[i] = htonl(r.encrypted.call_numbers[i]);
      }
  }
  r.encrypted.inc_nonce = htonl(ntohl(c.nonce) + 1);
  r.encrypted.level = htonl((int32)obj->level);
  r.kvno = htonl(obj->kvno);
  r.ticket_len = htonl(obj->ticket_len);
  /* Make checksum before we seal r.encrypted */
  r.encrypted.cksum = rxkad_cksum_response(&r);
  /* Seal r.encrypted */
  fc_cbc_enc2(&r.encrypted, &r.encrypted, sizeof(r.encrypted),
	      obj->k.keysched, (u_int32*)obj->k.key, ENCRYPT);

  /* Stuff response and kerberos ticket into packet */
  if (rx_SlowWritePacket(pkt, 0, sizeof(r), &r) != sizeof(r))
    return RXKADPACKETSHORT;
  if (rx_SlowWritePacket(pkt, sizeof(r), obj->ticket_len, obj->ticket) != obj->ticket_len)
    return RXKADPACKETSHORT;
  rx_SetDataSize(pkt, sizeof(r) + obj->ticket_len);
  return 0;
}

/*
 * Checksum and/or encrypt packet.
 */
static
int
client_PreparePacket(struct rx_securityClass *obj_,
		     struct rx_call *call,
		     struct rx_packet *pkt)
{
  rxkad_clnt_class *obj = (rxkad_clnt_class *) obj_;
  key_stuff *k = &obj->k;
  struct rx_connection *con = rx_ConnectionOf(call);
  end_stuff *e = &((clnt_con_data *) con->securityData)->e;

  return rxkad_prepare_packet(pkt, con, obj->level, k, e);
}

/*
 * Verify checksums and/or decrypt packet.
 */
static
int
client_CheckPacket(struct rx_securityClass *obj_,
		   struct rx_call *call,
		   struct rx_packet *pkt)
{
  rxkad_clnt_class *obj = (rxkad_clnt_class *) obj_;
  key_stuff *k = &obj->k;
  struct rx_connection *con = rx_ConnectionOf(call);
  end_stuff *e = &((clnt_con_data *) con->securityData)->e;

  return rxkad_check_packet(pkt, con, obj->level, k, e);
}

static
int
client_GetStats(const struct rx_securityClass *obj,
		const struct rx_connection *con,
		struct rx_securityObjectStats *st)
{
  clnt_con_data *cdat = (clnt_con_data *) con->securityData;

  st->type = rxkad_disipline;
  st->level = ((rxkad_clnt_class *)obj)->level;
  st->flags = rxkad_checksummed;
  if (cdat == 0)
    st->flags |= rxkad_unallocated;
  {
    st->bytesReceived = cdat->e.bytesReceived;
    st->packetsReceived = cdat->e.packetsReceived;
    st->bytesSent = cdat->e.bytesSent;
    st->packetsSent = cdat->e.packetsSent;
  }
  return 0;
}

static
struct rx_securityOps client_ops = {
  client_Close,
  client_NewConnection,
  client_PreparePacket,
  0,
  0,
  0,
  0,
  client_GetResponse,
  0,
  client_CheckPacket,
  client_DestroyConnection,
  client_GetStats,
  0,
  0,
  0,
};

int rxkad_EpochWasSet = 0;

int rxkad_min_level = rxkad_clear; /* rxkad_{clear, auth, crypt} */

struct rx_securityClass *
rxkad_NewClientSecurityObject(/*rxkad_level*/ int level,
			      void *sessionkey,
			      int32 kvno,
			      int ticket_len,
			      char *ticket)
{
  rxkad_clnt_class *obj;
  static int inited = 0;

  if (level < rxkad_min_level)
    level = rxkad_min_level;	/* Boost security level */

  if (!inited)
    {
      /* Any good random numbers will do, no real need to use
       * cryptographic techniques here */
      union {
	u_int32 rnd[2];
	des_cblock k;
      } u;
      int32 sched[ROUNDS];

      u.rnd[0] = rx_nextCid;
      u.rnd[1] = rx_epoch;
      fc_keysched(sessionkey, sched);
      fc_ecb_encrypt(&u.k, &u.k, sched, ENCRYPT);

      /* Some paranoia so we won't reveal the key */
      /*des_set_odd_parity(&u.k);*/
      fc_keysched(&u.k, sched);
      fc_ecb_encrypt(&u.k, &u.k, sched, ENCRYPT);

      /* Some paranoia so we won't reveal the key */
      /*des_set_odd_parity(&u.k);*/
      fc_keysched(&u.k, sched);
      fc_ecb_encrypt(&u.k, &u.k, sched, ENCRYPT);

      /* Set new cid and epoch generator */
      rx_nextCid = u.rnd[0] << RX_CIDSHIFT;
      rx_SetEpoch(u.rnd[0] ^ u.rnd[1]);
      rxkad_EpochWasSet = 1;
      inited = 1;
    }

#if 0
  /* If we are passed a to large kerberos 5 ticket hope for the best */
  if (ticket_len > MAXKRB5TICKETLEN)
    ticket_len = MAXKRB5TICKETLEN;
#endif

  obj = (rxkad_clnt_class *) osi_Alloc(sizeof(rxkad_clnt_class));
  obj->klass.refCount = 1;
  obj->klass.ops = &client_ops;

  obj->klass.privateData = (char *) obj;

  obj->level = level;
  fc_keysched(sessionkey, obj->k.keysched);
  memcpy(obj->k.key, sessionkey, sizeof(des_cblock));
  obj->kvno = kvno;

  obj->ticket_len = ticket_len;
  obj->ticket = osi_Alloc(ticket_len);
  memcpy(obj->ticket, ticket, ticket_len);

  return &obj->klass;
}
