/*
 * Copyright (c) 1995, 1996, 1997, 2003 Kungliga Tekniska Högskolan
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

/* @(#)$arla: rxkad_locl.h,v 1.14 2003/06/12 05:32:16 lha Exp $ */

#ifndef __RXKAD_LOCL_H
#define __RXKAD_LOCL_H

/* $arla: rxkad_locl.h,v 1.14 2003/06/12 05:32:16 lha Exp $ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <sys/types.h>
#include <netinet/in.h>

#ifdef NDEBUG
#ifndef assert
#define assert(e) ((void)0)
#endif
#else
#ifndef assert
#define assert(e) ((e) ? (void)0 : (void)osi_Panic("assert(%s) failed: file %s, line %d\n", #e, __FILE__, __LINE__, #e))
#endif
#endif

#ifdef HAVE_OPENSSL
#include <openssl/des.h>
#else
#include <des.h>
#endif
#ifdef HAVE_KRB4
#include <krb.h>
#endif /* HAVE_KRB4 */

#undef RCSID
#include <rx/rx.h>
#undef RCSID
#define RCSID(msg) \
static /**/const char *const rcsid[] = { (char *)rcsid, "\100(#)" msg }

extern int rx_epoch, rx_nextCid;

#include "rxkad.h"

#define rxkad_disipline 3

#define rxkad_unallocated 1
#define rxkad_authenticated 2
#define rxkad_expired 4
#define rxkad_checksummed 8

#define ROUNDS 16

int fc_keysched(const void *key_, int32_t sched[ROUNDS]);

/* In_ and out_ MUST be uint32_t aligned */
int fc_ecb_encrypt(const void *in_, void *out_,
		   const int32_t sched[ROUNDS], int encrypt);

/* In_ and out_ MUST be uint32_t aligned */
int fc_cbc_encrypt(const void *in_, void *out_, int32_t length,
		   const int32_t sched[ROUNDS], uint32_t iv[2],
		   int encrypt);

int rxkad_EncryptPacket(const void *rx_connection_not_used,
			const int32_t sched[ROUNDS], const uint32_t iv[2],
			int len, struct rx_packet *packet);

int rxkad_DecryptPacket(const void *rx_connection_not_used,
			const int32_t sched[ROUNDS], const uint32_t iv[2],
			int len, struct rx_packet *packet);

#ifdef __GNUC__
static inline
void
fc_cbc_enc2(const void *in, void *out, int32_t length, const int32_t sched[ROUNDS],
	    const uint32_t iv_[2], int encrypt)
{
  uint32_t iv[2];
  iv[0] = iv_[0];
  iv[1] = iv_[1];
  fc_cbc_encrypt(in, out, length, sched, iv, encrypt);
}
#else
#define fc_cbc_enc2(in, out, length, sched, iv_, encrypt) \
{ uint32_t _iv_[2]; uint32_t *_tmp_ = (iv_); \
  memcpy(_iv_, _tmp_, 8);  \
  fc_cbc_encrypt((in), (out), (length), (sched), (_iv_), (encrypt)); }
#endif

#define RXKAD_VERSION 2

/* Version 2 challenge format */
typedef struct rxkad_challenge {
  int32_t version;
  int32_t nonce;
  int32_t min_level;
  int32_t unused;
} rxkad_challenge;

/* To protect the client from being used as an oracle the response
 * contains connection specific information. */
typedef struct rxkad_response {
  int32_t version;
  int32_t unused;
  struct {
    int32_t epoch;
    int32_t cid;
    uint32_t cksum;		/* Cksum of this response */
    int32_t security_index;
    int32_t call_numbers[RX_MAXCALLS];
    int32_t inc_nonce;
    int32_t level;
  } encrypted;
  int32_t kvno;
  int32_t ticket_len;
  /* u_char the_ticket[ticket_len]; */
} rxkad_response;

typedef struct key_stuff {
  int32_t keysched[ROUNDS];
  des_cblock key;
} key_stuff;

typedef struct end_stuff {
  uint32_t header_iv[2];
  uint32_t bytesReceived, packetsReceived, bytesSent, packetsSent;
} end_stuff;

uint32_t
rxkad_cksum_response(rxkad_response *r);

void
rxkad_calc_header_iv(const struct rx_connection *conn,
		     const int32_t sched[ROUNDS],
		     const des_cblock *in_iv,
		     uint32_t out_iv[2]);

int
rxkad_prepare_packet(struct rx_packet *pkt, struct rx_connection *con,
		     int level, key_stuff *k, end_stuff *e);

int
rxkad_check_packet(struct rx_packet *pkt, struct rx_connection *con,
		   int level, key_stuff *k, end_stuff *e);

#ifdef HAVE_KRB4

/* Per connection specific server data */
typedef struct serv_con_data {
  end_stuff e;
  key_stuff k;
  uint32_t expires;
  int32_t nonce;
  krb_principal *user;
  rxkad_level cur_level;	/* Starts at min_level and can only increase */
  char authenticated;
} serv_con_data;

#endif

#endif /* __RXKAD_LOCL_H */
