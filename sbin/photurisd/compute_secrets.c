/*	$OpenBSD: compute_secrets.c,v 1.7 2002/12/06 02:17:42 deraadt Exp $	*/

/*
 * Copyright 1997-2000 Niels Provos <provos@citi.umich.edu>
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
 *      This product includes software developed by Niels Provos.
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
 * compute_secrets.c:
 * shared secret with diffie-hellman key exchange
 * cryptographic hashes for session keys
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: compute_secrets.c,v 1.7 2002/12/06 02:17:42 deraadt Exp $";
#endif

#define _SECRETS_C_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ssl/bn.h>
#include <md5.h>
#include "state.h"
#include <sha1.h>
#include "config.h"
#include "identity.h"
#include "attributes.h"
#include "modulus.h"
#include "secrets.h"
#include "buffer.h"
#include "spi.h"
#include "exchange.h"
#include "scheme.h"
#include "log.h"

int privacykey(struct stateob *st, struct idxform *hash, u_int8_t *key,
	       u_int8_t *packet, u_int16_t bytes, u_int16_t *order, int owner);

int
compute_shared_secret(struct stateob *st,
		      u_int8_t **shared, size_t *sharedsize)
{
     struct moduli_cache *mod;
     int header, res;
     BIGNUM *tmp, *tex;
     BN_CTX *ctx;

     if ((mod = mod_find_modgen(st->modulus, st->generator)) == NULL) {
	  log_print("Can't find exchange information in cache in compute_shared_secret()");
	  return (-1);
     }

     /* Compute Diffie-Hellmann a^(xy) (mod n) */
     tex = BN_new();
     BN_varpre2bn(st->texchange, st->texchangesize, tex);

     tmp = BN_new();
     ctx = BN_CTX_new();
     BN_mod_exp(tmp, tex, mod->private_value, mod->modulus, ctx);
     BN_CTX_free(ctx);

     BN_clear_free(tex);

     *sharedsize = BUFFER_SIZE;
     res = BN_bn2varpre(tmp, buffer, sharedsize);
     BN_clear_free(tmp);

     if (res == -1)
          return -1;

     /* The shared secret is not used with the size part */
     if (buffer[0] == 255)
	  header = 4;
     else
	  header = 2;

     *sharedsize -= header;

     if ((*shared = calloc(*sharedsize,sizeof(u_int8_t))) == NULL) {
          log_print("Not enough memory for shared secret in compute_shared_secret()");
          return (-1);
     }
     bcopy(buffer + header, *shared, *sharedsize);

     return (0);
}

/*
 * Generate session keys for all attributes in given SPI.
 */

int
make_session_keys(struct stateob *st, struct spiob *spi)
{
     u_int8_t *p, *attributes, **secret;
     u_int16_t attribsize, *secretsize;
     u_int16_t i, count = 0;
     int bits;

     attributes = spi->attributes;
     attribsize = spi->attribsize;
     secret = &(spi->sessionkey);
     secretsize = &(spi->sessionkeysize);

     if (*secret != NULL)
	  return 0;           /* Already calculated */

     p = attributes;
     for (i = 0; i<attribsize; i += p[i+1] + 2) {
	  if (p[i] != AT_AH_ATTRIB && p[i] != AT_ESP_ATTRIB) {
	       bits = get_session_key_length(p+i);
	       if (bits == -1) {
		    log_print("Invalid attribute choice for SPI in make_session_keys()");
		    return -1;
	       }
	       count += bits & 7 ? (bits >> 3) + 1 : bits >> 3;
	  }
     }
     if ((*secret = calloc(count, sizeof(u_int8_t))) == NULL) {
	  log_error("calloc() in make_session_keys()");
	  return -1;
     }
     *secretsize = count;

     count = 0;
     p = *secret;
     for (i = 0; i<attribsize; i += attributes[i+1] + 2) {
	  if (attributes[i] != AT_AH_ATTRIB &&
	      attributes[i] != AT_ESP_ATTRIB) {
	       bits = compute_session_key(st, p, attributes+i,
					  spi->flags & SPI_OWNER,
					  &count);
	       if (bits == -1)
		    return -1;
	       if (bits > 0) {
#ifdef DEBUG
		    {
			 int d = BUFFER_SIZE;
			 printf("%s session key for AT %d: ",
				spi->flags & SPI_OWNER ?
				"Owner" : "User", (int)attributes[i]);
			 bin2hex(buffer, &d, p,
				 bits & 7 ? (bits >> 3) + 1 : bits >> 3);
			 printf("0x%s\n", buffer);
		    }
#endif /* DEBUG */
		
		    p += bits & 7 ? (bits >> 3) + 1 : bits >> 3;
	       }
	  }
     }

     return 0;
}

/*
 * Return length of requried session key in bits.
 * DES would be 64 bits.
 */

int
get_session_key_length(u_int8_t *attribute)
{
     attrib_t *ob;

     if ((ob = getattrib(*attribute)) == NULL) {
	  log_print("Unknown attribute %d in get_session_key_length()",
		    *attribute);
	  return -1;
     }

     return ob->klen << 3;
}

/*
 * Compute session keys for the attributes in the security association.
 * owner determines the direction of the spi session key.
 * order is the amount of bits we already used for other session keys.
 */

int
compute_session_key(struct stateob *st, u_int8_t *key,
		    u_int8_t *attribute, int owner,
		    u_int16_t *order)
{
     struct idxform *hash;
     u_int16_t size, i, n;
     u_int8_t digest[HASH_MAX];
     int bits;

     switch(ntohs(*((u_int16_t *)st->scheme))) {
     case DH_G_2_MD5:
     case DH_G_3_MD5:
     case DH_G_2_DES_MD5:
     case DH_G_5_MD5:
     case DH_G_3_DES_MD5:
     case DH_G_5_DES_MD5:
     case DH_G_VAR_MD5:
     case DH_G_VAR_DES_MD5:
	  hash = get_hash(HASH_MD5);
	  break;
     case DH_G_2_3DES_SHA1:
     case DH_G_3_3DES_SHA1:
     case DH_G_5_3DES_SHA1:
     case DH_G_VAR_3DES_SHA1:
	  hash = get_hash(HASH_SHA1);
	  break;
     default:
	  log_print("Unknown scheme %d in compute_session_key()",
		    ntohs(*((u_int16_t *)st->scheme)));
	  return -1;
     }	
	

     if ((bits = get_session_key_length(attribute)) == -1)
	  return -1;
     if (bits == 0)
	  return 0;

     size = bits >> 3;
     if(bits & 0x7)
	  size++;

     /* As many shared secrets we used already */
     n = *order;

     hash->Init(hash->ctx);
     hash->Update(hash->ctx, st->icookie, COOKIE_SIZE);
     hash->Update(hash->ctx, st->rcookie, COOKIE_SIZE);
     if(owner) { /* Session key for Owner SPI */
	  hash->Update(hash->ctx,st->oSPIsecret,st->oSPIsecretsize);
	  hash->Update(hash->ctx,st->uSPIsecret,st->uSPIsecretsize);
     } else {    /* Session key for User SPI */
	  hash->Update(hash->ctx,st->uSPIsecret,st->uSPIsecretsize);
	  hash->Update(hash->ctx,st->oSPIsecret,st->oSPIsecretsize);
     }

     /* Message Verification field */
     hash->Update(hash->ctx, st->verification, st->versize);

     for (i=0; i<n; i++)
	  hash->Update(hash->ctx, st->shared, st->sharedsize);

     do {
	  bcopy(hash->ctx, hash->ctx2, hash->ctxsize);
	  hash->Update(hash->ctx2,st->shared, st->sharedsize);
	  bcopy(hash->ctx2, hash->ctx, hash->ctxsize);

	  hash->Final(digest, hash->ctx2);
	  /* One iteration more */
	  n++;

	  bcopy(digest, key, size>hash->hashsize ? hash->hashsize : size);
	  key += size>hash->hashsize ? hash->hashsize : size;

	  /* Unsigned integer arithmetic */
	  size -= size>hash->hashsize ? hash->hashsize : size;
     } while(size > 0);

     *order = n;

     return bits;
}

/*
 * Initializes the hash contexts for privacy key computation.
 */

int
init_privacy_key(struct stateob *st, int owner)
{
     void **ctx;
     struct idxform *hash;
     u_int8_t *first, *second;
     u_int16_t firstsize, secondsize;

     if (owner) {
	  ctx = &st->oSPIprivacyctx;
	  first = st->exchangevalue;
	  firstsize = st->exchangesize;
	  second = st->texchange;
	  secondsize = st->texchangesize;
     } else {
	  ctx = &st->uSPIprivacyctx;
	  first = st->texchange;
	  firstsize = st->texchangesize;
	  second = st->exchangevalue;
	  secondsize = st->exchangesize;
     }

     switch(ntohs(*((u_int16_t *)st->scheme))) {
     case DH_G_2_MD5:
     case DH_G_3_MD5:
     case DH_G_5_MD5:
     case DH_G_2_DES_MD5:
     case DH_G_3_DES_MD5:
     case DH_G_5_DES_MD5:
	  hash = get_hash(HASH_MD5);
	  break;
     case DH_G_2_3DES_SHA1:
     case DH_G_3_3DES_SHA1:
     case DH_G_5_3DES_SHA1:
	  hash = get_hash(HASH_SHA1);
	  break;
     default:
          log_print("Unknown exchange scheme in init_privacy_key()");
          return -1;
     }

     if (hash == NULL)
	  return -1;

     if (*ctx != NULL)
	  free(*ctx);

     if ((*ctx = calloc(hash->ctxsize, sizeof(char))) == NULL) {
	  log_error("calloc() in init_privacy_key()");
	  return -1;
     }
     hash->Init(*ctx);
     hash->Update(*ctx, first, firstsize);
     hash->Update(*ctx, second, secondsize);
     return 1;
}

/*
 * order gives the number of iterations already done for keys
 */

int
compute_privacy_key(struct stateob *st, u_int8_t *key, u_int8_t *packet,
		    u_int16_t bits, u_int16_t *order, int owner)
{
     u_int16_t size;
     struct idxform *hash;

     size = bits >> 3;
     if(bits & 0x7)
          size++;

     switch(ntohs(*((u_int16_t *)st->scheme))) {
     case DH_G_2_MD5:
     case DH_G_3_MD5:
     case DH_G_5_MD5:
     case DH_G_2_DES_MD5:
     case DH_G_3_DES_MD5:
     case DH_G_5_DES_MD5:
	  hash = get_hash(HASH_MD5);
	  break;
     case DH_G_2_3DES_SHA1:
     case DH_G_3_3DES_SHA1:
     case DH_G_5_3DES_SHA1:
	  hash = get_hash(HASH_SHA1);
	  break;
     default:
          log_print("Unknown exchange scheme in compute_privacy_key()");
          return -1;
     }

     if (hash == NULL)
	  return -1;

     return privacykey(st, hash, key, packet, size, order, owner);
}


int
privacykey(struct stateob *st, struct idxform *hash,
	   u_int8_t *key, u_int8_t *packet,
	   u_int16_t bytes, u_int16_t *order, int owner)
{
     u_int16_t i, n;
     u_int8_t digest[HASH_MAX];

     /* SPIprivacyctx contains the hashed exchangevalues */
     bcopy(owner ? st->oSPIprivacyctx : st->uSPIprivacyctx,
	   hash->ctx2, hash->ctxsize);
	
     hash->Update(hash->ctx2, packet, 2*COOKIE_SIZE + 4 + SPI_SIZE);

     /* As many shared secrets we used already */
     n = *order;
     for(i=0; i<n; i++)
	  hash->Update(hash->ctx2, st->shared, st->sharedsize);

     do {
	  bcopy(hash->ctx2, hash->ctx, hash->ctxsize);
	  hash->Update(hash->ctx, st->shared, st->sharedsize);
	  bcopy(hash->ctx, hash->ctx2, hash->ctxsize);
	
	  hash->Final(digest, hash->ctx);
          bcopy(digest, key, bytes>hash->hashsize ? hash->hashsize : bytes);
	  key += bytes>hash->hashsize ? hash->hashsize : bytes;

	  /* Unsigned integer arithmetic */
          bytes -= bytes>hash->hashsize ? hash->hashsize : bytes;
	
	  /* Increment the times we called Final */
	  i++;
     } while(bytes > 0);

     *order = i;
     return 0;
}

