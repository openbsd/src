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
 * cookie.c:
 * cookie generation
 */

#ifndef lint
static char rcsid[] = "$Id: cookie.c,v 1.1 1998/11/14 23:37:22 deraadt Exp $";
#endif

#define _COOKIE_C_

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <md5.h>
#include "state.h"
#include "cookie.h"

void
reset_secret(void)
{
     secret_generate(rsecret, SECRET_SIZE);
}

int
secret_generate(u_int8_t *secret, u_int16_t size)
{
  int i = 0;
  long tmp = 0;

  while(size > 0) {
    size--;
    if (i++ % 4 == 0)
      tmp = arc4random();
    
    secret[size] = tmp & 0xFF;
    tmp = tmp >> 8;
  }
  return 1;
}
      
int
cookie_generate(struct stateob *st, u_int8_t *cookie, u_int16_t size,
		u_int8_t *data, u_int16_t dsize)
{
  MD5_CTX ctx;
  u_int8_t digest[16];
  u_int8_t tmpsecret[SECRET_SIZE], *secret;

  if (st->initiator) {
    secret = tmpsecret;
    secret_generate(tmpsecret, SECRET_SIZE); /* New secret each CookieReq */
  } else
    secret = rsecret;

  /* Generate a cookie which depends on both parties and on local 
   * information, which is fast computed.
   */
  MD5Init(&ctx);
  MD5Update(&ctx, st->address, strlen(st->address));
  MD5Update(&ctx, (u_int8_t *)&st->port, sizeof(st->port));
  MD5Update(&ctx, (u_int8_t *)&st->counter, sizeof(st->counter));
  MD5Update(&ctx, secret, SECRET_SIZE);
  MD5Update(&ctx, st->icookie, COOKIE_SIZE);

  /* For the responder cookie we also hash the schemes */
  if (data != NULL && dsize)
       MD5Update(&ctx, data, dsize);

  MD5Final(digest, &ctx);

  bcopy(digest, cookie, size);
  return 1;
}

