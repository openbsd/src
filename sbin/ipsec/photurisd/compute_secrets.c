/*
 * Copyright 1997 Niels Provos <provos@physnet.uni-hamburg.de>
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
static char rcsid[] = "$Id: compute_secrets.c,v 1.3 1997/07/24 23:47:08 provos Exp $"; 
#endif 

#define _SECRETS_C_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <gmp.h>
#include <md5.h>
#include "state.h"
#include <sha1.h>
#include "config.h"
#include "attributes.h"
#include "modulus.h"
#include "secrets.h"
#include "buffer.h"
#include "spi.h"
#include "exchange.h"
#include "scheme.h"
#include "errlog.h"

int MD5privacykey(struct stateob *st, u_int8_t *key, u_int8_t *packet,
		  u_int16_t bytes, u_int16_t order, int owner);
int SHA1privacykey(struct stateob *st, u_int8_t *key, u_int8_t *packet,
		   u_int16_t bytes, u_int16_t order, int owner);


int
compute_shared_secret(struct stateob *st, 
		      u_int8_t **shared, u_int16_t *sharedsize)
{
     struct moduli_cache *mod;

     mpz_t tmp, bits, tex;

     mpz_init(tmp);
     mpz_init(bits);

     if((mod=mod_find_modgen(st->modulus, st->generator)) == NULL) {
	  log_error(0, "Can't find exchange information in cache in compute_shared_secret()");
	  return -1;
     }

     /* Compute Diffie-Hellmann a^(xy) (mod n) */

     mpz_init_set_varpre(tex, st->texchange);
     mpz_powm(tmp, tex, mod->private_value, mod->modulus);

     mpz_clear(tex);

     varpre_get_number_bits(bits, scheme_get_mod(st->scheme));

     *sharedsize = BUFFER_SIZE;
     if(mpz_to_varpre(buffer, sharedsize, tmp, bits) == -1)   
          return -1;
     mpz_clear(bits);
     mpz_clear(tmp);

     if((*shared = calloc(*sharedsize,sizeof(u_int8_t))) == NULL) {
          log_error(0, "Not enough memory for shared secret in compute_shared_secret()");
          return -1;
     }
     bcopy(buffer, *shared, *sharedsize);
     return 0;
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
		    log_error(0, "Invalid attribute choice for SPI in make_session_keys()");
		    return -1;
	       }
	       count += bits & 7 ? (bits >> 3) + 1 : bits >> 3;
	  }
     }
     if ((*secret = calloc(count, sizeof(u_int8_t))) == NULL) {
	  log_error(1, "calloc() in make_session_keys()");
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
#ifdef DEBUG
	       {    int d = BUFFER_SIZE;
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
     
     return 0;
}


int
get_session_key_length(u_int8_t *attribute)
{
     switch(*attribute) {
     case AT_MD5_KDP:
	  return MD5_KEYLEN;
     case AT_DES_CBC:
	  return DES_KEYLEN;
     default:
	  log_error(0, "Unknown attribute %d in get_session_key_length()", 
		    *attribute);
	  return -1;
     }
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
     u_int16_t size, i,n;
     u_int8_t digest[16];
     int bits;
     MD5_CTX ctx;

     if ((bits = get_session_key_length(attribute)) == -1)
	  return -1;

     size = bits >> 3;
     if(bits & 0x7)
	  size++;

     /* XXX - we only do md5 at the moment */
     *order = (*order^(*order&0x7f)) + (*order & 0x7f ? 128 : 0);

     /* As many shared secrets we used already */
     n = *order >> 7;

     do {
	  MD5Init(&ctx);
	  MD5Update(&ctx, st->icookie, COOKIE_SIZE);
	  MD5Update(&ctx, st->rcookie, COOKIE_SIZE);
	  if(owner) { /* Session key for Owner SPI */
	       MD5Update(&ctx,st->oSPIsecret,st->oSPIsecretsize);
	       MD5Update(&ctx,st->uSPIsecret,st->uSPIsecretsize);
	       MD5Update(&ctx,st->oSPIidentver, st->oSPIidentversize);
	  } else {    /* Session key for User SPI */
               MD5Update(&ctx,st->uSPIsecret,st->uSPIsecretsize); 
               MD5Update(&ctx,st->oSPIsecret,st->oSPIsecretsize); 
               MD5Update(&ctx,st->uSPIidentver, st->uSPIidentversize); 
	  }
	  for(i=0; i<n; i++)
	       MD5Update(&ctx,st->shared, st->sharedsize);
	  n++;
	  MD5Final(digest, &ctx);
	  bcopy(digest, key, size>16 ? 16 : size);
	  key += size>16 ? 16 : size;

	  /* Unsigned integer arithmetic */
	  size -= size>16 ? 16 : size;
     } while(size > 0);  
     
     *order += (bits^(bits&0x7f)) + (bits & 0x7f ? 128 : 0);

     return bits;
}

/*
 * order gives the number of bits already used for keys
 */

int
compute_privacy_key(struct stateob *st, u_int8_t *key, u_int8_t *packet,
		    u_int16_t bits, u_int16_t order, int owner)
{
     u_int16_t size;

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
	  return MD5privacykey(st, key, packet, size, order, owner);
     case DH_G_2_3DES_SHA1:  
     case DH_G_3_3DES_SHA1:  
     case DH_G_5_3DES_SHA1:  
	  return SHA1privacykey(st, key, packet, size, order, owner);
     default:  
          log_error(0, "Unknown exchange scheme in compute_privacy_key()");
          return -1;  
     }  
}


int
MD5privacykey(struct stateob *st, u_int8_t *key, u_int8_t *packet, 
	      u_int16_t bytes, u_int16_t order, int owner) 
{
     MD5_CTX ctx, ctxb; 
     u_int16_t i, n;
     u_int8_t digest[16];
     
     MD5Init(&ctxb); 
	  
     MD5Update(&ctxb, packet, 2*COOKIE_SIZE + 4 + SPI_SIZE); 
     
     if (owner) {
	  MD5Update(&ctxb, st->exchangevalue, st->exchangesize);   
	  MD5Update(&ctxb, st->texchange, st->texchangesize);   
     } else {
	  MD5Update(&ctxb, st->texchange, st->texchangesize);    
	  MD5Update(&ctxb, st->exchangevalue, st->exchangesize);    
     }
     
     /* As many shared secrets we used already */ 
     n = order&0x7f ? (order >> 7) + 1 : order >> 7; 
     for(i=0; i<n; i++) 
	  MD5Update(&ctxb, st->shared, st->sharedsize); 

     do {
	  ctx = ctxb;
	  MD5Update(&ctx, st->shared, st->sharedsize);
	  ctxb = ctx;
	  
	  MD5Final(digest, &ctx);
          bcopy(digest, key, bytes>16 ? 16 : bytes); 
	  key += bytes>16 ? 16 : bytes;
 
	  /* Unsigned integer arithmetic */ 
          bytes -= bytes>16 ? 16 : bytes; 
     } while(bytes>=16);   

     return 0;
}

int
SHA1privacykey(struct stateob *st, u_int8_t *key, u_int8_t *packet,
	      u_int16_t bytes, u_int16_t order, int owner) 
{
     SHA1_CTX ctx, ctxb; 
     u_int16_t i, n; 
     u_int8_t digest[20];

     SHA1Init(&ctxb); 
     
     SHA1Update(&ctxb, packet, 2*COOKIE_SIZE + 4 + SPI_SIZE);  
	  
     if (owner) {
	  SHA1Update(&ctxb, st->exchangevalue, st->exchangesize);   
	  SHA1Update(&ctxb, st->texchange, st->texchangesize);   
     } else {
	  SHA1Update(&ctxb, st->texchange, st->texchangesize);    
	  SHA1Update(&ctxb, st->exchangevalue, st->exchangesize);    
     }


     /* As many shared secrets we used already */ 
     n = order%160 ? order/160+1 : order/160; 
     for (i=0; i<n; i++)
	  SHA1Update(&ctxb, st->shared, st->sharedsize);

     do {

	  ctx = ctxb;
	  SHA1Update(&ctx, st->shared, st->sharedsize); 
	  ctxb = ctx;

	  SHA1Final(digest, &ctx);

          bcopy(digest, key, bytes>20 ? 20 : bytes); 

	  key += bytes>20 ? 20 : bytes;
 
          /* Unsigned integer arithmetic */ 
          bytes -= bytes>20 ? 20 : bytes; 
     } while(bytes>0);   

     return 0;
}
