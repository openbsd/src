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
 * validity.c:
 * validity verification
 */

#ifndef lint
static char rcsid[] = "$Id: validity.c,v 1.1 1998/11/14 23:37:30 deraadt Exp $";
#endif

#define _VALIDITY_C_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <md5.h>
#include <sha1.h>
#include "config.h"
#include "scheme.h"
#include "exchange.h"
#include "errlog.h"
#include "state.h"
#include "attributes.h"
#include "validity.h"
#include "identity.h"
#include "buffer.h"

int valsign(struct stateob *st, struct idxform *hash, u_int8_t *signature,
	    u_int8_t *packet, u_int16_t psize);
int valverify(struct stateob *st, struct idxform *hash, u_int8_t *signature,
	      u_int8_t *packet, u_int16_t psize);

u_int16_t
get_validity_verification_size(struct stateob *st)
{
     switch(ntohs(*((u_int16_t *)st->scheme))) {
     case DH_G_2_MD5:
     case DH_G_3_MD5:
     case DH_G_5_MD5:
     case DH_G_2_DES_MD5:
     case DH_G_3_DES_MD5:
     case DH_G_5_DES_MD5:
	  return (128/8)+2;            /* Two octets for varpre size */
     case DH_G_2_3DES_SHA1:
     case DH_G_3_3DES_SHA1:
     case DH_G_5_3DES_SHA1:
	  return (160/8)+2;
     default:
	  log_error(0, "validitiy.c: Unknown exchange scheme: %d\n", 
		    *((u_int16_t *)st->scheme));
	  return 0;
     }
}

int
create_validity_verification(struct stateob *st, u_int8_t *buffer, 
			     u_int8_t *packet, u_int16_t size)
{
     struct idxform *hash;

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
          log_error(0, "validity.c: Unknown exchange scheme: %d\n",  
                    *((u_int16_t *)st->scheme)); 
          return 0; 
     }

     if(valsign(st, hash, buffer+2, packet, size)) { 
          /* Create varpre number from digest */ 
          buffer[0] = (hash->hashsize >> 5) & 0xFF; 
          buffer[1] = (hash->hashsize << 3) & 0xFF; 
     } 

     state_save_verification(st, buffer, hash->hashsize+2);

     return hash->hashsize+2;
}

int 
verify_validity_verification(struct stateob *st, u_int8_t *buffer,  
                    u_int8_t *packet, u_int16_t size) 
{ 
     struct idxform *hash;

     switch(ntohs(*((u_int16_t *)st->scheme))) {  
     case DH_G_2_MD5:  
     case DH_G_3_MD5:  
     case DH_G_5_MD5:  
     case DH_G_2_DES_MD5:  
     case DH_G_3_DES_MD5:  
     case DH_G_5_DES_MD5:  
	  if (varpre2octets(buffer) != 18)
	       return 0;
	  hash = get_hash(HASH_MD5);
	  break;
     case DH_G_2_3DES_SHA1:  
     case DH_G_3_3DES_SHA1:  
     case DH_G_5_3DES_SHA1:  
	  if (varpre2octets(buffer) != 22)
	       return 0;
	  hash = get_hash(HASH_SHA1);
	  break;
     default:  
	  log_error(0, "validity.c: Unknown exchange scheme: %d\n",   
                    *((u_int16_t *)st->scheme));  
          return 0;  
     }  

     state_save_verification(st, buffer, hash->hashsize+2);

     return valverify(st, hash, buffer+2, packet, size);
} 


int
valsign(struct stateob *st, struct idxform *hash, u_int8_t *signature,  
	u_int8_t *packet, u_int16_t psize) 
{
     u_int8_t key[HASH_MAX];
     u_int16_t keylen = HASH_MAX;

     create_verification_key(st, key, &keylen, 1); /* Owner direction */
 
     hash->Init(hash->ctx); 
 
     hash->Update(hash->ctx, key, keylen);

     hash->Update(hash->ctx, st->icookie, COOKIE_SIZE);
     hash->Update(hash->ctx, st->rcookie, COOKIE_SIZE);

     packet += 2*COOKIE_SIZE; psize -= 2*COOKIE_SIZE;
     hash->Update(hash->ctx, packet, 4 + SPI_SIZE);

     hash->Update(hash->ctx, st->oSPIidentver, st->oSPIidentversize);
     hash->Update(hash->ctx, st->uSPIidentver, st->uSPIidentversize);

     packet += 4 + SPI_SIZE + hash->hashsize + 2; 
     psize -=  4 + SPI_SIZE + hash->hashsize + 2;
     hash->Update(hash->ctx, packet, psize);

     /* Data fill */
     hash->Final(NULL, hash->ctx); 

     hash->Update(hash->ctx, key, keylen);
     hash->Final(signature, hash->ctx); 

     return hash->hashsize;
}

/* We assume that the verification field is zeroed */

int
valverify(struct stateob *st, struct idxform *hash, u_int8_t *signature,   
	  u_int8_t *packet, u_int16_t psize)
{
     u_int8_t digest[HASH_MAX];
     u_int8_t key[HASH_MAX];
     u_int16_t keylen = HASH_MAX;

     create_verification_key(st, key, &keylen, 0); /* User direction */
 
     hash->Init(hash->ctx); 
 
     hash->Update(hash->ctx, key, keylen);

     hash->Update(hash->ctx, st->icookie, COOKIE_SIZE);
     hash->Update(hash->ctx, st->rcookie, COOKIE_SIZE);

     packet += 2*COOKIE_SIZE; psize -= 2*COOKIE_SIZE;
     hash->Update(hash->ctx, packet, 4 + SPI_SIZE);

     hash->Update(hash->ctx, st->uSPIidentver, st->uSPIidentversize);
     hash->Update(hash->ctx, st->oSPIidentver, st->oSPIidentversize);

     packet += 4 + SPI_SIZE + hash->hashsize + 2; 
     psize -=  4 + SPI_SIZE + hash->hashsize + 2;
     hash->Update(hash->ctx, packet, psize);

     /* Data fill */
     hash->Final(NULL, hash->ctx); 

     hash->Update(hash->ctx, key, keylen);
     hash->Final(digest, hash->ctx); 

     return !bcmp(digest,signature,hash->hashsize);
}
