/*	$OpenBSD: ipsec.h,v 1.5 1999/02/26 03:44:04 niklas Exp $	*/
/*	$EOM: ipsec.h,v 1.32 1999/02/25 09:30:26 niklas Exp $	*/

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

#ifndef _IPSEC_H_
#define _IPSEC_H_

#include <netinet/in.h>

#include "ipsec_doi.h"

struct group;
struct hash;
struct ike_auth;
struct message;
struct proto;
struct sa;

/*
 * IPSEC-specific data to be linked into the exchange struct.
 * XXX Should probably be two different structs, one for phase 1 and one
 * for phase 2 parameters.
 */
struct ipsec_exch {
  struct hash *hash;
  struct ike_auth *ike_auth;
  struct group *group;
  u_int16_t prf_type;

  /*
   * A copy of the initiator SA payload body for later computation of hashes.
   * Phase 1 only.
   */
  size_t sa_i_b_len;
  u_int8_t *sa_i_b;

  /* Diffie-Hellman values.  */
  size_t g_x_len;
  u_int8_t *g_xi;
  u_int8_t *g_xr;
  u_int8_t* g_xy;

  /* SKEYIDs.  XXX Phase 1 only?  */
  size_t skeyid_len;
  u_int8_t *skeyid;
  u_int8_t *skeyid_d;
  u_int8_t *skeyid_a;
  u_int8_t *skeyid_e;

  /* HASH_I & HASH_R.  XXX Do these need to be saved here?  */
  u_int8_t *hash_i;
  u_int8_t *hash_r;

  /* KEYMAT */
  size_t keymat_len;

  /* Phase 2.  */
  u_int8_t *id_ci;
  size_t id_ci_sz;
  u_int8_t *id_cr;
  size_t id_cr_sz;
};

struct ipsec_sa {
  /* Phase 1.  */
  u_int8_t hash;
  size_t skeyid_len;
  u_int8_t *skeyid_d;
  u_int8_t *skeyid_a;
  u_int16_t prf_type;

  /* Phase 2.  */
  u_int16_t group_desc;

  /* Tunnel parameters.  */
  in_addr_t src_net;
  in_addr_t src_mask;
  in_addr_t dst_net;
  in_addr_t dst_mask;
};

struct ipsec_proto {
  /* Phase 2.  */
  u_int16_t encap_mode;
  u_int16_t auth;
  u_int16_t keylen;
  u_int16_t keyrounds;

  /* This is not negotiated, but rather configured.  */
  int32_t replay_window;

  /* KEYMAT */
  u_int8_t *keymat[2];
};

extern int ipsec_ah_keylength (struct proto *);
extern u_int8_t *ipsec_build_id (char *, size_t *);
extern int ipsec_decode_attribute (u_int16_t, u_int8_t *, u_int16_t, void *);
extern void ipsec_decode_transform (struct message *, struct sa *,
				    struct proto *, u_int8_t *);
extern int ipsec_esp_authkeylength (struct proto *);
extern int ipsec_esp_enckeylength (struct proto *);
extern int ipsec_gen_g_x (struct message *);
extern int ipsec_get_id (char *, int *, struct in_addr *, struct in_addr *);
extern void ipsec_init (void);
extern int ipsec_is_attribute_incompatible (u_int16_t, u_int8_t *, u_int16_t,
					    void *);
extern int ipsec_keymat_length (struct proto *);
extern int ipsec_save_g_x (struct message *);
extern struct sa *ipsec_sa_lookup (in_addr_t, u_int32_t, u_int8_t);

#endif /* _IPSEC_H_ */
