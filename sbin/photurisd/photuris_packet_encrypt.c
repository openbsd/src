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
 * photuris_packet_encrypt:
 * encrypts packets with the privacy choice.
 */

#ifndef lint 
static char rcsid[] = "$Id: photuris_packet_encrypt.c,v 1.1 1998/11/14 23:37:26 deraadt Exp $";
#endif 

#define _ENCRYPT_C_
 
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <des.h>
#include "config.h" 
#include "packets.h" 
#include "state.h" 
#include "attributes.h"
#include "encrypt.h"
#include "secrets.h"
#include "errlog.h"
#ifdef DEBUG
#include "config.h"
#endif

void
packet_mask(u_int8_t *packet, u_int16_t len, u_int8_t *key)
{
     int i;
     for (i=0; i<len; i++)
	  packet[i] ^= key[i];
}

int
packet_create_padding(struct stateob *st, u_int16_t size, u_int8_t *padd, 
		      u_int16_t *rsize)
{
     u_int8_t padlength, i;

     switch(ntohs(*((u_int16_t *)st->scheme))) { 
     case DH_G_2_MD5:    
     case DH_G_3_MD5:    
     case DH_G_5_MD5:    
	  padlength = (arc4random() & 0xf0) - (size%16);
	  if (padlength < 8)
	       padlength += 8;
	  break;
     default:
	  padlength = (arc4random() & 0xf0) - (size%16);
	  if (padlength < 8)
	       padlength += 8;
	  break;
     }
 
     if(*rsize < padlength) 
          return -1; 
 
     /* Pad the rest of the payload */ 
     for(i=1;i<=padlength;i++) 
          padd[i-1] = i; 

     *rsize = padlength;

     return 0;
}

int
packet_encrypt(struct stateob *st, u_int8_t *payload, u_int16_t payloadlen)
{
     des_cblock keys[4], *input;
     des_key_schedule key1,key2,key3;
     u_int8_t *pkey;
     u_int16_t order = 0;
     int i;
     
     input = (des_cblock *)payload;

     /* No encryption needed */
     switch(ntohs(*((u_int16_t *)st->scheme))) {
     case DH_G_2_MD5:   
     case DH_G_3_MD5:   
     case DH_G_5_MD5:   
#ifdef DEBUG
	  printf("[Packet encryption: None]\n");
#endif
	  pkey = calloc(payloadlen,sizeof(u_int8_t));
	  if(pkey == NULL) {
	       log_error(1, "Not enough memory for privacy secret");
	       return -1;
	  }
	  if(compute_privacy_key(st, pkey, 
				 payload - 2*COOKIE_SIZE - 4 - SPI_SIZE,
				 payloadlen*8, &order, 1) == -1)
	       return -1;
#ifdef DEBUG 
	  {  
	       int i; 
	       char buffer[3000]; 
	       i = 3000; 
	       bin2hex(buffer, &i, pkey, payloadlen); 
	       printf("Encrypt key: %s\n", buffer ); 
	  } 
#endif 
	  packet_mask(payload, payloadlen, pkey);
	  return 0;
     case DH_G_2_DES_MD5:   
     case DH_G_3_DES_MD5:   
     case DH_G_5_DES_MD5:   
#ifdef DEBUG
	  printf("[Packet encryption: DES]\n");
#endif
	  pkey = calloc(payloadlen + 8, sizeof(u_int8_t));
	  if(pkey == NULL) {
	       log_error(1, "Not enough memory for privacy secret");
	       return -1;
	  }
	  /* XOR Mask */
	  if(compute_privacy_key(st, pkey, 
				 payload - 2*COOKIE_SIZE - 4 - SPI_SIZE,
				 payloadlen*8, &order, 1) == -1)
	       return -1;
	  /* DES Key */
	  if(compute_privacy_key(st, pkey+payloadlen, 
				 payload - 2*COOKIE_SIZE - 4 - SPI_SIZE,
				 64, &order, 1) == -1)
	       return -1;
#ifdef DEBUG 
	  {  
	       int i; 
	       char buffer[3000]; 
	       i = 3000; 
	       bin2hex(buffer, &i, pkey, payloadlen+8); 
	       printf("Encrypt key: %s\n", buffer ); 
	  } 
#endif 
	  bcopy(pkey+payloadlen, &keys[0], 8);
	  des_set_odd_parity(&keys[0]);

	  /* Zero IV, we will mask the packet instead */
	  bzero(&keys[1], 8);

	  des_set_key(&keys[0], key1);

	  packet_mask(payload, payloadlen, pkey);

	  des_cbc_encrypt(input,input,payloadlen, key1,&keys[1], DES_ENCRYPT);
	  break;
     case DH_G_2_3DES_SHA1:   
     case DH_G_3_3DES_SHA1:   
     case DH_G_5_3DES_SHA1:   
#ifdef DEBUG
	  printf("[Packet encryption: 3DES]\n");
#endif
	  pkey = calloc(payloadlen+24, sizeof(u_int8_t));
	  if(pkey == NULL) {
	       log_error(1, "Not enough memory for owner privacy secret");
	       return -1;
	  }
	  /* XOR Mask */
	  if(compute_privacy_key(st, pkey, 
				 payload - 2*COOKIE_SIZE - 4 - SPI_SIZE,
				 payloadlen*8, &order, 1) == -1)
	       return -1;
	  /* 3 DES Keys */
	  for (i=0; i<3; i++) {
	       if(compute_privacy_key(st, pkey+payloadlen + (i<<3), 
				      payload - 2*COOKIE_SIZE - 4 - SPI_SIZE,
				      64, &order, 1) == -1)
		    return -1;
	  }
#ifdef DEBUG
	  { 
	       int i;
	       char buffer[3000];
	       i = 3000;
	       bin2hex(buffer, &i, pkey, payloadlen+24);
	       printf("Encrypt key: %s\n", buffer );
	  }
#endif
	  bcopy(pkey+payloadlen   , &keys[0], 8);
	  des_set_odd_parity(&keys[0]);
	  bcopy(pkey+payloadlen+8 , &keys[1], 8);
	  des_set_odd_parity(&keys[1]);
	  bcopy(pkey+payloadlen+16, &keys[2], 8);
	  des_set_odd_parity(&keys[2]);

	  /* Zero IV, we will make the packet instead */
	  bzero(&keys[3], 8);

	  des_set_key(&keys[0], key1);
	  des_set_key(&keys[1], key2);
	  des_set_key(&keys[2], key3);

	  packet_mask(payload, payloadlen, pkey);

	  des_ede3_cbc_encrypt(input, input, payloadlen,
			   key1, key2, key3, &keys[3], DES_ENCRYPT);
	  break;
     default:   
          log_error(0, "Unknown exchange scheme: %d\n",    
                    *((u_int16_t *)st->scheme));   
          return -1;   
     }

     free(pkey);

     return 0;
}   

int
packet_decrypt(struct stateob *st, u_int8_t *payload, u_int16_t *payloadlen)
{
     u_int8_t padlength, i;
     des_cblock keys[4], *input;
     des_key_schedule key1,key2,key3;
     u_int8_t *pkey;
     u_int16_t order = 0;

     input = (des_cblock *)payload;

     /* No encryption needed */
     switch(ntohs(*((u_int16_t *)st->scheme))) {
     case DH_G_2_MD5:   
     case DH_G_3_MD5:   
     case DH_G_5_MD5:   
#ifdef DEBUG
	  printf("[Packet decryption: None]\n");
#endif
	  pkey = calloc(*payloadlen, sizeof(u_int8_t));
	  if(pkey == NULL) {
	       log_error(1, "Not enough memory for privacy secret");
	       return -1;
	  }
	  if(compute_privacy_key(st, pkey, 
				 payload - 2*COOKIE_SIZE - 4 - SPI_SIZE,
				 *payloadlen*8, &order, 0) == -1)
	       return -1;
#ifdef DEBUG 
	  {  
	       int i = 3000; 
	       char buffer[3000]; 
	       bin2hex(buffer, &i, pkey, *payloadlen); 
	       printf("Decrypt key: %s\n", buffer ); 
	  } 
#endif 
	  packet_mask(payload, *payloadlen, pkey);
	  return 0;
     case DH_G_2_DES_MD5:   
     case DH_G_3_DES_MD5:   
     case DH_G_5_DES_MD5:   
#ifdef DEBUG
	  printf("[Packet decryption: DES]\n");
#endif
	  pkey = calloc(*payloadlen+8, sizeof(u_int8_t));
	  if(pkey == NULL) {
	       log_error(1, "Not enough memory for privacy secret");
	       return -1;
	  }
	  /* XOR Mask */
	  if(compute_privacy_key(st, pkey, 
				 payload - 2*COOKIE_SIZE - 4 - SPI_SIZE,
				 *payloadlen*8, &order, 0) == -1)
	       return -1;
	  /* DES Key */
	  if(compute_privacy_key(st, pkey + *payloadlen, 
				 payload - 2*COOKIE_SIZE - 4 - SPI_SIZE,
				 64, &order, 0) == -1)
	       return -1;
#ifdef DEBUG 
	  {  
	       int i = 3000; 
	       char buffer[3000]; 
	       bin2hex(buffer, &i, pkey, *payloadlen + 8); 
	       printf("Decrypt key: %s\n", buffer ); 
	  } 
#endif 
	  bcopy(pkey+*payloadlen, &keys[0], 8);
	  des_set_odd_parity(&keys[0]);

	  /* Zero IV, we will mask the packet instead */
	  bzero(&keys[1], 8);

	  des_set_key(&keys[0], key1);

	  des_cbc_encrypt(input,input,*payloadlen, key1,&keys[1], DES_DECRYPT);

	  packet_mask(payload, *payloadlen, pkey);
	  break;
     case DH_G_2_3DES_SHA1:   
     case DH_G_3_3DES_SHA1:   
     case DH_G_5_3DES_SHA1:   
#ifdef DEBUG
	  printf("[Packet decryption: 3DES]\n");
#endif
	  pkey  = calloc(*payloadlen + 24, sizeof(u_int8_t));
	  if(pkey == NULL) {
	       log_error(1, "Not enough memory for privacy secret");
	       return -1;
	  }
	  /* XOR Mask */
	  if(compute_privacy_key(st, pkey, 
				 payload - 2*COOKIE_SIZE - 4 - SPI_SIZE,
				 *payloadlen*8, &order, 0) == -1)
	       return -1;
	  /* 3 DES keys + 1 DES IV */
	  for (i=0; i<3; i++) {
	       if(compute_privacy_key(st, pkey + *payloadlen + (i<<3), 
				      payload - 2*COOKIE_SIZE - 4 - SPI_SIZE,
				      64, &order, 0) == -1)
		    return -1;
	  }
#ifdef DEBUG
	  { 
	       int i = 3000;
	       char buffer[3000];
	       bin2hex(buffer, &i, pkey, *payloadlen+24);
	       printf("Decrypt key: %s\n", buffer );
	  }
#endif
	  bcopy(pkey+*payloadlen   , &keys[0], 8);
	  des_set_odd_parity(&keys[0]);
	  bcopy(pkey+*payloadlen+8 , &keys[1], 8);
	  des_set_odd_parity(&keys[1]);
	  bcopy(pkey+*payloadlen+16, &keys[2], 8);
	  des_set_odd_parity(&keys[2]);

	  /* Zero IV, we will mask the packet instead */
	  bzero(&keys[3], 8);

	  des_set_key(&keys[0], key1);
	  des_set_key(&keys[1], key2);
	  des_set_key(&keys[2], key3);

	  des_ede3_cbc_encrypt(input, input, *payloadlen,
			   key1, key2, key3, &keys[3], DES_DECRYPT);

	  packet_mask(payload, *payloadlen, pkey);
	  break;
     default:   
          log_error(0,"Unknown exchange scheme: %d\n",    
                    *((u_int16_t *)st->scheme));   
          return -1;   
     }

     padlength = *(payload+(*payloadlen)-1);

     /* Check the padding */

     if(padlength > 255 || padlength < 8)
	  return -1;

     *payloadlen = *payloadlen - padlength;

     for(i=1;i<=padlength;i++)
	  if(payload[*payloadlen+i-1] != i)
	       return -1;

     return 0;
}   
     
