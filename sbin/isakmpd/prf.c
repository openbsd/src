/*	$OpenBSD: prf.c,v 1.3 1998/11/17 11:10:19 niklas Exp $	*/
/*	$EOM: prf.c,v 1.3 1998/07/23 09:00:47 niklas Exp $	*/

/*
 * Copyright (c) 1998 Niels Provos.  All rights reserved.
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

#include <sys/param.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"
#include "log.h"
#include "prf.h"

void prf_hash_init (struct prf_hash_ctx *);
void prf_hash_update (struct prf_hash_ctx *, unsigned char *, unsigned int);
void prf_hash_final (unsigned char *, struct prf_hash_ctx *);

/* PRF behaves likes a hash */

void
prf_hash_init (struct prf_hash_ctx *ctx)
{
  memcpy (ctx->hash->ctx, ctx->ctx, ctx->hash->ctxsize);
  memcpy (ctx->hash->ctx2, ctx->ctx2, ctx->hash->ctxsize);
}

void
prf_hash_update (struct prf_hash_ctx *ctx, unsigned char *data,
		 unsigned int len)
{
  ctx->hash->Update (ctx->hash->ctx, data, len);
}

void
prf_hash_final (unsigned char *digest, struct prf_hash_ctx *ctx)
{
  ctx->hash->HMACFinal (digest, ctx->hash);
}

/*
 * Obtain a Pseudo-Random Function for us. At the moment this is
 * the HMAC version of a hash. See RFC-2104 for reference.
 */

struct prf *
prf_alloc (enum prfs type, int subtype, char *shared, int sharedsize)
{
  struct hash *hash;
  struct prf *prf;

  switch (type)
    {
    case PRF_HMAC:
      hash = hash_get (subtype);
      if (hash == NULL)
	return NULL;
      break;
    default:
      log_print ("Unkown PRF type %d in prf_alloc()", type);
      return NULL;
    }

  if ((prf = malloc (sizeof (struct prf))) == NULL)
    {
      log_print ("Out of memory for struct prf in prf_alloc()");
      return NULL;
    }

  if (type == PRF_HMAC)
    {
      struct prf_hash_ctx *prfctx;

      /* Obtain needed memory */
      prfctx = malloc (sizeof (struct prf_hash_ctx));
      if (prfctx == NULL)
	{
	  log_print ("Out of memory for struct prf_hash_ctx in prf_alloc()");
	  goto cleanprf;
	}
      prf->prfctx = prfctx;

      prfctx->ctx = malloc (hash->ctxsize);
      if (prfctx->ctx == NULL)
	{
	  log_print ("Out of memory for ctx in prf_alloc()");
	  goto cleanprfctx;
	}

      prfctx->ctx2 = malloc (hash->ctxsize);
      if (prfctx->ctx2 == NULL)
	{
	  log_print ("Out of memory for ctx2 in prf_alloc()");
	  free (prfctx->ctx);
	  goto cleanprfctx;
	}
      prf->type = PRF_HMAC;
      prf->blocksize = hash->hashsize;
      prfctx->hash = hash;

      /* Use the correct function pointers */
      prf->Init = (void (*) (void *)) prf_hash_init;
      prf->Update
	= (void (*) (void *, unsigned char *, unsigned int)) prf_hash_update;
      prf->Final = (void (*) (unsigned char *, void *)) prf_hash_final;

      /* Init HMAC contexts */
      hash->HMACInit (hash, shared, sharedsize);

      /* Save contexts */
      memcpy (prfctx->ctx, hash->ctx, hash->ctxsize);
      memcpy (prfctx->ctx2, hash->ctx2, hash->ctxsize);
    }

  return prf;

 cleanprfctx:
  free (prf->prfctx);
 cleanprf:
  free (prf);
  return NULL;
}


void
prf_free (struct prf *prf)
{
  if (prf->type == PRF_HMAC)
    {
      struct prf_hash_ctx *prfctx = prf->prfctx;
      free (prfctx->ctx2);
      free (prfctx->ctx);
    }
  free (prf->prfctx);
  free (prf);
}
