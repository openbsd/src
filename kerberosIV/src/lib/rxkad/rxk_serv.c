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

#if defined(KRB5)
#include <krb5.h>
#endif

RCSID("$KTH: rxk_serv.c,v 1.24 1999/12/02 16:58:55 joda Exp $");

static inline
unsigned int
FT_Time()
{
  struct timeval tv;
  FT_GetTimeOfDay(&tv, 0);
  return tv.tv_sec;
}

/* Security object specific server data */
typedef struct rxkad_serv_class {
  struct rx_securityClass klass;
  rxkad_level min_level;
  void *appl_data;
  int (*get_key)(void *appl_data, int kvno, des_cblock *key);
  int (*user_ok)(char *name, char *inst, char *realm, int kvno);
} rxkad_serv_class;

static
int
server_NewConnection(struct rx_securityClass *obj, struct rx_connection *con)
{
  assert(con->securityData == 0);
  obj->refCount++;
  con->securityData = (char *) osi_Alloc(sizeof(serv_con_data));
  memset(con->securityData, 0x0, sizeof(serv_con_data));
  return 0;
}

static
int
server_Close(struct rx_securityClass *obj)
{
  obj->refCount--;
  if (obj->refCount <= 0)
    osi_Free(obj, sizeof(rxkad_serv_class));
  return 0;
}

static
int
server_DestroyConnection(struct rx_securityClass *obj,
			 struct rx_connection *con)
{
  serv_con_data *cdat = (serv_con_data *)con->securityData;

  if (cdat)
    {
      if (cdat->user)
	osi_Free(cdat->user, sizeof(krb_principal));
      osi_Free(cdat, sizeof(serv_con_data));
    }
  return server_Close(obj);
}

/*
 * Check whether a connection authenticated properly.
 * Zero is good (authentication succeeded).
 */
static
int
server_CheckAuthentication(struct rx_securityClass *obj,
			   struct rx_connection *con)
{
  serv_con_data *cdat = (serv_con_data *) con->securityData;

  if (cdat)
    return !cdat->authenticated;
  else
    return RXKADNOAUTH;
}

/*
 * Select a nonce for later use.
 */
static
int
server_CreateChallenge(struct rx_securityClass *obj_,
		       struct rx_connection *con)
{
  rxkad_serv_class *obj = (rxkad_serv_class *) obj_;
  serv_con_data *cdat = (serv_con_data *) con->securityData;
  union {
    u_int32 rnd[2];
    des_cblock k;
  } u;

  /* Any good random numbers will do, no real need to use
   * cryptographic techniques here */
  des_random_key(u.k);
  cdat->nonce = u.rnd[0] ^ u.rnd[1];
  cdat->authenticated = 0;
  cdat->cur_level = obj->min_level;
  return 0;
}

/*
 * Wrap the nonce in a challenge packet.
 */
static
int
server_GetChallenge(const struct rx_securityClass *obj,
		    const struct rx_connection *con,
		    struct rx_packet *pkt)
{
  serv_con_data *cdat = (serv_con_data *) con->securityData;
  rxkad_challenge c;

  /* Make challenge */
  c.version = htonl(RXKAD_VERSION);
  c.nonce = htonl(cdat->nonce);
  c.min_level = htonl((int32)cdat->cur_level);
  c.unused = 0; /* Use this to hint client we understand krb5 tickets??? */

  /* Stuff into packet */
  if (rx_SlowWritePacket(pkt, 0, sizeof(c), &c) != sizeof(c))
    return RXKADPACKETSHORT;
  rx_SetDataSize(pkt, sizeof(c));
  return 0;
}

static
int
decode_krb5_ticket(rxkad_serv_class *obj,
		   int serv_kvno,
		   char *ticket,
		   int32 ticket_len,
		   /* OUT parms */
		   des_cblock session_key,
		   u_int32 *expires,
		   krb_principal *p)
{
#if !defined(KRB5)
  return RXKADBADTICKET;
#else
  des_cblock serv_key;		/* Service's secret key */
  krb5_keyblock key;		/* Uses serv_key above */
  int code;
  size_t siz;

  Ticket t5;			/* Must free */
  EncTicketPart decr_part;	/* Must free */
  krb5_context context;		/* Must free */
  krb5_data plain;		/* Must free */

  memset(&t5, 0x0, sizeof(t5));
  memset(&decr_part, 0x0, sizeof(decr_part));
  krb5_init_context(&context);
  krb5_data_zero(&plain);

  assert(serv_kvno == RXKAD_TKT_TYPE_KERBEROS_V5);

  code = decode_Ticket(ticket, ticket_len, &t5, &siz);
  if (code != 0)
    goto bad_ticket;

  /* Find the real service key version number */
  serv_kvno = t5.tkt_vno;

  /* Check that the key type really fit into 8 bytes */
  switch (t5.enc_part.etype) {
  case ETYPE_DES_CBC_CRC:
  case ETYPE_DES_CBC_MD4:
  case ETYPE_DES_CBC_MD5:
    key.keytype = KEYTYPE_DES;
    key.keyvalue.length = 8;
    key.keyvalue.data = serv_key;
    break;
  default:
    goto unknown_key;
  }
  
  /* Get the service key. We have to assume that the key type is of
   * size 8 bytes or else we can't store service keys for both krb4
   * and krb5 in the same way in /usr/afs/etc/KeyFile.
   */
  code = (*obj->get_key)(obj->appl_data, serv_kvno, &serv_key);
  if (code)
    goto unknown_key;

  /* Decrypt ticket */
  code = krb5_decrypt(context,
		      t5.enc_part.cipher.data,
		      t5.enc_part.cipher.length,
		      t5.enc_part.etype,
		      &key,
		      &plain);
  if (code != 0)
    goto bad_ticket;
  
  /* Decode ticket */
  code = decode_EncTicketPart(plain.data, plain.length, &decr_part, &siz);
  if (code != 0)
    goto bad_ticket;

  /* Extract realm and principal */  
  memset(p, 0x0, sizeof(p));
  strlcpy(p->realm, decr_part.crealm, REALM_SZ);
  switch (decr_part.cname.name_string.len) {
  case 2:
    strlcpy(p->instance,
		    decr_part.cname.name_string.val[1],
		    ANAME_SZ);
  case 1:
    strlcpy(p->name,
		    decr_part.cname.name_string.val[0],
		    INST_SZ);
    break;
  default:
    goto bad_ticket;
  }
  
  /* Extract session key */
  memcpy(session_key, decr_part.key.keyvalue.data, 8);

  /* Check lifetimes and host addresses, flags etc */
  {
    time_t now = FT_Time(); /* Use fast time package but not approx time */
    time_t start = decr_part.authtime;
    if (decr_part.starttime)
      start = *decr_part.starttime;
    if (start - now > context->max_skew || decr_part.flags.invalid)
      goto no_auth;
    if (now > decr_part.endtime)
      goto tkt_expired;
    *expires = decr_part.endtime;
  }

#if 0
  /* Check host addresses */
#endif

cleanup:
  free_Ticket(&t5);
  free_EncTicketPart(&decr_part);
  krb5_free_context(context);
  krb5_data_free(&plain);
  return code;
  
unknown_key:
  code = RXKADUNKNOWNKEY;
  goto cleanup;
no_auth:
  code = RXKADNOAUTH;
  goto cleanup;
tkt_expired:
  code = RXKADEXPIRED;
  goto cleanup;
bad_ticket:
  code = RXKADBADTICKET;
  goto cleanup;
#endif /* KRB5 */
}

static
int
decode_krb4_ticket(rxkad_serv_class *obj,
		   int serv_kvno,
		   char *ticket,
		   int32 ticket_len,
		   /* OUT parms */
		   des_cblock session_key,
		   u_int32 *expires,
		   krb_principal *p)
{
  u_char kflags;
  int klife;
  u_int32 start;
  u_int32 paddress;
  char sname[SNAME_SZ], sinstance[INST_SZ];
  KTEXT_ST tkt;
  des_cblock serv_key;		/* Service's secret key */
  des_key_schedule serv_sched;	/* Service's schedule */

  /* First get service key */
  int code = (*obj->get_key)(obj->appl_data, serv_kvno, &serv_key);
  if (code)
    return RXKADUNKNOWNKEY;

  des_key_sched(&serv_key, serv_sched);
  tkt.length = ticket_len;
  memcpy(tkt.dat, ticket, ticket_len);
  code = decomp_ticket(&tkt, &kflags,
		       p->name, p->instance, p->realm, &paddress,
		       session_key, &klife, &start,
		       sname, sinstance,
		       &serv_key, serv_sched);
  if (code != KSUCCESS)
    return RXKADBADTICKET;

#if 0
  if (paddress != ntohl(con->peer->host))
    return RXKADBADTICKET;
#endif

  {
    time_t end = krb_life_to_time(start, klife);
    time_t now = FT_Time(); /* Use fast time package but not approx time */
    start -= CLOCK_SKEW;
    if (now < start)
      return RXKADNOAUTH;
    else if (now > end)
      return RXKADEXPIRED;
    *expires = end;
  }
  return 0;			/* Success */
}

/*
 * Process a response to a challange.
 */
static
int
server_CheckResponse(struct rx_securityClass *obj_,
		     struct rx_connection *con,
		     struct rx_packet *pkt)
{
  rxkad_serv_class *obj = (rxkad_serv_class *) obj_;
  serv_con_data *cdat = (serv_con_data *) con->securityData;

  int serv_kvno;		/* Service's kvno we used */
  int32 ticket_len;
  char ticket[MAXKRB5TICKETLEN];
  int code;
  rxkad_response r;
  krb_principal p;
  u_int32 cksum;

  if (rx_SlowReadPacket(pkt, 0, sizeof(r), &r) != sizeof(r))
    return RXKADPACKETSHORT;
  
  serv_kvno = ntohl(r.kvno);
  ticket_len = ntohl(r.ticket_len);

  if (ticket_len > MAXKRB5TICKETLEN)
    return RXKADTICKETLEN;

  if (rx_SlowReadPacket(pkt, sizeof(r), ticket_len, ticket) != ticket_len)
    return RXKADPACKETSHORT;

  /* Disassemble kerberos ticket */
  if (serv_kvno == RXKAD_TKT_TYPE_KERBEROS_V5)
    code = decode_krb5_ticket(obj, serv_kvno, ticket, ticket_len,
			      cdat->k.key, &cdat->expires, &p);
  else
    code = decode_krb4_ticket(obj, serv_kvno, ticket, ticket_len,
			      cdat->k.key, &cdat->expires, &p);
  if (code != 0)
    return code;

  fc_keysched(cdat->k.key, cdat->k.keysched);

  /* Unseal r.encrypted */
  fc_cbc_enc2(&r.encrypted, &r.encrypted, sizeof(r.encrypted),
	      cdat->k.keysched, (u_int32*)cdat->k.key, DECRYPT);

  /* Verify response integrity */
  cksum = r.encrypted.cksum;
  r.encrypted.cksum = 0;
  if (r.encrypted.epoch != ntohl(con->epoch)
      || r.encrypted.cid != ntohl(con->cid & RX_CIDMASK)
      || r.encrypted.security_index != ntohl(con->securityIndex)
      || cksum != rxkad_cksum_response(&r))
    return RXKADSEALEDINCON;
  {
    int i;
    for (i = 0; i < RX_MAXCALLS; i++)
      {
	r.encrypted.call_numbers[i] = ntohl(r.encrypted.call_numbers[i]);
	if (r.encrypted.call_numbers[i] < 0)
	  return RXKADSEALEDINCON;
      }
  }

  if (ntohl(r.encrypted.inc_nonce) != cdat->nonce+1)
    return RXKADOUTOFSEQUENCE;

  {
    int level = ntohl(r.encrypted.level);
    if ((level < cdat->cur_level) || (level > rxkad_crypt))
      return RXKADLEVELFAIL;
    cdat->cur_level = level;
    /* We don't use trailers but the transarc implementation breaks if
     * we don't set the trailer size, packets get to large */
    if (level == rxkad_auth)
      {
	rx_SetSecurityHeaderSize(con, 4);
	rx_SetSecurityMaxTrailerSize(con, 4);
      }
    else if (level == rxkad_crypt)
      {
	rx_SetSecurityHeaderSize(con, 8);
	rx_SetSecurityMaxTrailerSize(con, 8);
      }
  }
  
  rxi_SetCallNumberVector(con, r.encrypted.call_numbers);

  rxkad_calc_header_iv(con, cdat->k.keysched,
		       (const des_cblock *)&cdat->k.key, cdat->e.header_iv);
  cdat->authenticated = 1;

  if (obj->user_ok)
    {
      code = obj->user_ok(p.name, p.instance, p.realm, serv_kvno);
      if (code)
	return RXKADNOAUTH;
    }
  else
    {
      krb_principal *user = (krb_principal *) osi_Alloc(sizeof(krb_principal));
      *user = p;
      cdat->user = user;
    }
  return 0;
}

/*
 * Checksum and/or encrypt packet
 */
static
int
server_PreparePacket(struct rx_securityClass *obj_,
		     struct rx_call *call,
		     struct rx_packet *pkt)
{
  struct rx_connection *con = rx_ConnectionOf(call);
  serv_con_data *cdat = (serv_con_data *) con->securityData;
  key_stuff *k = &cdat->k;
  end_stuff *e = &cdat->e;

  return rxkad_prepare_packet(pkt, con, cdat->cur_level, k, e);
}

/*
 * Verify checksum and/or decrypt packet.
 */
static
int
server_CheckPacket(struct rx_securityClass *obj_,
		   struct rx_call *call,
		   struct rx_packet *pkt)
{
  struct rx_connection *con = rx_ConnectionOf(call);
  serv_con_data *cdat = (serv_con_data *) con->securityData;
  key_stuff *k = &cdat->k;
  end_stuff *e = &cdat->e;

  if (FT_ApproxTime() > cdat->expires) /* Use fast approx time */
    return RXKADEXPIRED;

  return rxkad_check_packet(pkt, con, cdat->cur_level, k, e);
}

static
int
server_GetStats(const struct rx_securityClass *obj_,
		const struct rx_connection *con,
		struct rx_securityObjectStats *st)
{
  rxkad_serv_class *obj = (rxkad_serv_class *) obj_;
  serv_con_data *cdat = (serv_con_data *) con->securityData;

  st->type = rxkad_disipline;
  st->level = obj->min_level;
  st->flags = rxkad_checksummed;
  if (cdat == 0)
    st->flags |= rxkad_unallocated;
  else
    {
      st->bytesReceived = cdat->e.bytesReceived;
      st->packetsReceived = cdat->e.packetsReceived;
      st->bytesSent = cdat->e.bytesSent;
      st->packetsSent = cdat->e.packetsSent;
      st->expires = cdat->expires;
      st->level = cdat->cur_level;
      if (cdat->authenticated)
	st->flags |= rxkad_authenticated;
    }
  return 0;
}

static struct rx_securityOps server_ops = {
  server_Close,
  server_NewConnection,
  server_PreparePacket,
  0,
  server_CheckAuthentication,
  server_CreateChallenge,
  server_GetChallenge,
  0,
  server_CheckResponse,
  server_CheckPacket,
  server_DestroyConnection,
  server_GetStats,
};

struct rx_securityClass *
rxkad_NewServerSecurityObject(/*rxkad_level*/ int min_level,
			      void *appl_data,
			      int (*get_key)(void *appl_data,
					     int kvno,
					     des_cblock *key),
			      int (*user_ok)(char *name,
					     char *inst,
					     char *realm,
					     int kvno))
{
  rxkad_serv_class *obj;

  if (!get_key)
    return 0;

  obj = (rxkad_serv_class *) osi_Alloc(sizeof(rxkad_serv_class));
  obj->klass.refCount = 1;
  obj->klass.ops = &server_ops;
  obj->klass.privateData = (char *) obj;

  obj->min_level = min_level;
  obj->appl_data = appl_data;
  obj->get_key = get_key;
  obj->user_ok = user_ok;

  return &obj->klass;
}
