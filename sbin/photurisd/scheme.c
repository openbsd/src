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
 * scheme.c:
 * SCHEME handling functions
 */

#ifndef lint
static char rcsid[] = "$Id: scheme.c,v 1.1 1998/11/14 23:37:28 deraadt Exp $";
#endif

#define _SCHEME_C_

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include "config.h"
#include "attributes.h"
#include "buffer.h"
#include "scheme.h"
#include "errlog.h"

u_int8_t *
scheme_get_gen(u_int8_t *scheme)
{
     int header;
     switch(ntohs(*(u_int16_t *)scheme)) { 
     case DH_G_2_MD5: 
     case DH_G_3_MD5:  
     case DH_G_2_DES_MD5:  
     case DH_G_5_MD5:  
     case DH_G_3_DES_MD5:  
     case DH_G_2_3DES_SHA1:  
     case DH_G_5_DES_MD5:  
     case DH_G_3_3DES_SHA1:  
     case DH_G_5_3DES_SHA1:
	  return NULL;
     case DH_G_VAR_MD5: 
     case DH_G_VAR_DES_MD5: 
     case DH_G_VAR_3DES_SHA1: 
	  if (scheme[2] == 255 && scheme[3] == 255)
	       header = 8;
	  else if (scheme[2] == 255)
	       header = 4;
	  else
	       header = 2;
	  return scheme+2+header;
     default: 
          log_error(0, "Unknown scheme in scheme_get_gen()"); 
          return NULL; 
     }
}
 
u_int8_t * 
scheme_get_mod(u_int8_t *scheme) 
{
     int header;
     switch(ntohs(*(u_int16_t *)scheme)) {
     case DH_G_2_MD5:
     case DH_G_3_MD5: 
     case DH_G_2_DES_MD5: 
     case DH_G_5_MD5: 
     case DH_G_3_DES_MD5: 
     case DH_G_2_3DES_SHA1: 
     case DH_G_5_DES_MD5: 
     case DH_G_3_3DES_SHA1: 
     case DH_G_5_3DES_SHA1: 
	  return scheme+2;
	  break;
     case DH_G_VAR_MD5:
     case DH_G_VAR_DES_MD5:
     case DH_G_VAR_3DES_SHA1:
	  if (scheme[2] == 255 && scheme[3] == 255)
	       header = 8;
	  else if (scheme[2] == 255)
	       header = 4;
	  else
	       header = 2;
	  if (varpre2octets(scheme+2) > 2)
	       return scheme+2+header+varpre2octets(scheme+2+header);
	  else
	       return scheme+2;
	  break;
     default:
	  log_error(0, "Unknown scheme in scheme_get_mod()");
	  return NULL;
     }
}

size_t
scheme_get_len(u_int8_t *scheme)
{
     return 2 + varpre2octets(scheme+2); 
}

u_int16_t
scheme_get_ref(u_int8_t *scheme)
{
     switch(ntohs(*(u_int16_t *)scheme)) { 
     case DH_G_2_MD5: 
     case DH_G_2_DES_MD5:  
     case DH_G_2_3DES_SHA1:  
	  return DH_G_2_MD5;
     case DH_G_3_MD5:  
     case DH_G_3_DES_MD5:  
     case DH_G_3_3DES_SHA1: 
	  return DH_G_3_MD5;
     case DH_G_5_MD5:  
     case DH_G_5_DES_MD5:  
     case DH_G_5_3DES_SHA1: 
	  return DH_G_5_MD5;
     case DH_G_VAR_MD5: 
     case DH_G_VAR_DES_MD5: 
     case DH_G_VAR_3DES_SHA1: 
          return DH_G_VAR_MD5;
     default: 
          log_error(0, "Unknown scheme in scheme_get_ref()"); 
          return 0; 
     } 
}

size_t 
varpre2octets(u_int8_t *varpre)
{
     int blocks, header;
     size_t size;
     mpz_t offset, a;

     mpz_init(offset);
     mpz_init(a);

     /* XXX - only support a few octets at the moment */

     if(*varpre == 255 && *(varpre+1) == 255) {
          blocks = 6;
          varpre += 2;
          mpz_set_ui(offset, 16776960);
	  header = 8;
     } else if (*varpre == 255) {
	  blocks = 3;
	  varpre++;
	  mpz_set_ui(offset, 65280);
	  header = 4;
     } else {
	  header = 2;
	  blocks = 2;
     }

     while(blocks--) {
	  mpz_mul_ui(a, a, 256);
	  mpz_add_ui(a, a, *varpre);
	  varpre++;
     }
     mpz_add(offset, offset, a);
     mpz_cdiv_q_ui(offset, offset, 8);
     size = mpz_get_ui(offset) + header;
     mpz_clear(offset);
     mpz_clear(a);

     return size;
}

