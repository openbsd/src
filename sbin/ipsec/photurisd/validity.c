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
static char rcsid[] = "$Id: validity.c,v 1.1.1.1 1997/07/18 22:48:50 provos Exp $";
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
#include "buffer.h"

int MD5valsign(struct stateob *st, u_int8_t *signature,
	       u_int8_t *packet, u_int16_t psize);
int MD5valverify(struct stateob *st, u_int8_t *signature,
		 u_int8_t *packet, u_int16_t psize);
int SHA1valsign(struct stateob *st, u_int8_t *signature,
		u_int8_t *packet, u_int16_t psize);
int SHA1valverify(struct stateob *st, u_int8_t *signature,
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
     int hash_size;

     switch(ntohs(*((u_int16_t *)st->scheme))) { 
     case DH_G_2_MD5: 
     case DH_G_3_MD5: 
     case DH_G_5_MD5: 
     case DH_G_2_DES_MD5: 
     case DH_G_3_DES_MD5: 
     case DH_G_5_DES_MD5: 
	  hash_size = MD5valsign(st, buffer+2, packet, size);
	  break;
     case DH_G_2_3DES_SHA1: 
     case DH_G_3_3DES_SHA1: 
     case DH_G_5_3DES_SHA1: 
          hash_size = SHA1valsign(st, buffer+2, packet, size);
	  break;
     default: 
          log_error(0, "validity.c: Unknown exchange scheme: %d\n",  
                    *((u_int16_t *)st->scheme)); 
          return 0; 
     }

     if(hash_size) { 
          /* Create varpre number from digest */ 
          buffer[0] = (hash_size >> 5) & 0xFF; 
          buffer[1] = (hash_size << 3) & 0xFF; 
     } 

     return size+2;
}

int 
verify_validity_verification(struct stateob *st, u_int8_t *buffer,  
                    u_int8_t *packet, u_int16_t size) 
{ 
     switch(ntohs(*((u_int16_t *)st->scheme))) {  
     case DH_G_2_MD5:  
     case DH_G_3_MD5:  
     case DH_G_5_MD5:  
     case DH_G_2_DES_MD5:  
     case DH_G_3_DES_MD5:  
     case DH_G_5_DES_MD5:  
	  if (varpre2octets(buffer) != 18)
	       return 0;
          return MD5valverify(st, buffer+2, packet, size); 
     case DH_G_2_3DES_SHA1:  
     case DH_G_3_3DES_SHA1:  
     case DH_G_5_3DES_SHA1:  
	  if (varpre2octets(buffer) != 22)
	       return 0;
          return SHA1valverify(st, buffer+2, packet, size); 
     default:  
	  log_error(0, "validity.c: Unknown exchange scheme: %d\n",   
                    *((u_int16_t *)st->scheme));  
          return 0;  
     }  
} 


int
MD5valsign(struct stateob *st, u_int8_t *signature,  
                    u_int8_t *packet, u_int16_t psize) 
{
     MD5_CTX ctx; 
 
     MD5Init(&ctx); 
 
     MD5Update(&ctx, st->shared, st->sharedsize);

     MD5Update(&ctx, st->icookie, COOKIE_SIZE);
     MD5Update(&ctx, st->rcookie, COOKIE_SIZE);

     MD5Update(&ctx, st->oSPIidentver, st->oSPIidentversize);
     MD5Update(&ctx, st->uSPIidentver, st->uSPIidentversize);

     packet += 2*COOKIE_SIZE; psize -= 2*COOKIE_SIZE;
     MD5Update(&ctx, packet, 4 + SPI_SIZE);

     packet += 4 + SPI_SIZE + 18; psize -= 4 + SPI_SIZE + 18;
     MD5Update(&ctx, packet, psize);

     /* Data fill */
     MD5Final(NULL, &ctx); 

     MD5Update(&ctx, st->shared, st->sharedsize);
     MD5Final(signature, &ctx); 

     return MD5_SIZE;
}

/* We assume that the verification field is zeroed */

int
MD5valverify(struct stateob *st, u_int8_t *signature,   
	  u_int8_t *packet, u_int16_t psize)
{
     MD5_CTX ctx; 
     u_int8_t digest[MD5_SIZE];
 

     MD5Init(&ctx); 
 
     MD5Update(&ctx, st->shared, st->sharedsize);

     MD5Update(&ctx, st->icookie, COOKIE_SIZE);
     MD5Update(&ctx, st->rcookie, COOKIE_SIZE);

     
     MD5Update(&ctx, st->uSPIidentver, st->uSPIidentversize);
     MD5Update(&ctx, st->oSPIidentver, st->oSPIidentversize);

     packet += 2*COOKIE_SIZE; psize -= 2*COOKIE_SIZE;
     MD5Update(&ctx, packet, 4 + SPI_SIZE);

     packet += 4 + SPI_SIZE + 18; psize -= 4 + SPI_SIZE + 18;
     MD5Update(&ctx, packet, psize);

     /* Data fill */
     MD5Final(NULL, &ctx); 

     MD5Update(&ctx, st->shared, st->sharedsize);
     MD5Final(digest, &ctx); 

     return !bcmp(digest,signature,MD5_SIZE);
}

int
SHA1valsign(struct stateob *st, u_int8_t *signature,  
                    u_int8_t *packet, u_int16_t psize) 
{
     SHA1_CTX ctx; 
 
     SHA1Init(&ctx); 
 
     SHA1Update(&ctx, st->shared, st->sharedsize);

     SHA1Update(&ctx, st->icookie, COOKIE_SIZE);
     SHA1Update(&ctx, st->rcookie, COOKIE_SIZE);

     SHA1Update(&ctx, st->oSPIidentver, st->oSPIidentversize);
     SHA1Update(&ctx, st->uSPIidentver, st->uSPIidentversize);

     packet += 2*COOKIE_SIZE; psize -= 2*COOKIE_SIZE;
     SHA1Update(&ctx, packet, 4 + SPI_SIZE);

     packet += 4 + SPI_SIZE + 22; psize -= 4 + SPI_SIZE + 22;
     SHA1Update(&ctx, packet, psize);

     /* Data fill */
     SHA1Final(NULL, &ctx); 

     SHA1Update(&ctx, st->shared, st->sharedsize);
     SHA1Final(signature, &ctx); 

     return SHA1_SIZE;
}

/* We assume that the verification field is zeroed */

int
SHA1valverify(struct stateob *st, u_int8_t *signature,   
	  u_int8_t *packet, u_int16_t psize)
{
     SHA1_CTX ctx; 
     u_int8_t digest[SHA1_SIZE];
 

     SHA1Init(&ctx); 
 
     SHA1Update(&ctx, st->shared, st->sharedsize);

     SHA1Update(&ctx, st->icookie, COOKIE_SIZE);
     SHA1Update(&ctx, st->rcookie, COOKIE_SIZE);

     SHA1Update(&ctx, st->uSPIidentver, st->uSPIidentversize);
     SHA1Update(&ctx, st->oSPIidentver, st->oSPIidentversize);

     packet += 2*COOKIE_SIZE; psize -= 2*COOKIE_SIZE;
     SHA1Update(&ctx, packet, 4 + SPI_SIZE);

     packet += 4 + SPI_SIZE + 22; psize -= 4 + SPI_SIZE + 22;
     SHA1Update(&ctx, packet, psize);

     /* Data fill */
     SHA1Final(NULL, &ctx); 

     SHA1Update(&ctx, st->shared, st->sharedsize);
     SHA1Final(digest, &ctx); 

     return !bcmp(digest,signature,SHA1_SIZE);
}
