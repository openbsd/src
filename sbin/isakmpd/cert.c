/*	$OpenBSD: cert.c,v 1.2 1998/11/15 00:43:50 niklas Exp $	*/

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

#include "cert.h"
#include "isakmp_num.h"
#include "x509.h"

struct cert_handler cert_handler[] = {
    {ISAKMP_CERTENC_X509_SIG, 
     x509_certreq_validate, x509_certreq_decode, x509_free_aca,
     x509_cert_obtain, x509_cert_get_key, x509_cert_get_subject}
};

struct cert_handler *
cert_get (u_int16_t id)
{
  int i;

  for (i = 0; i < sizeof cert_handler / sizeof cert_handler[0]; i++)
    if (id == cert_handler[i].id)
      return &cert_handler[i];
  return NULL;
}


/* Decode a CERTREQ and return a parsed structure */

struct certreq_aca *
certreq_decode (u_int16_t type, u_int8_t *data, u_int32_t datalen)
{
  struct cert_handler *handler;
  struct certreq_aca aca, *ret;

  if ((handler = cert_get (type)) == NULL)
    return NULL;

  aca.id = type;
  aca.handler = handler;

  if (datalen > 0)
    {
      aca.data = handler->certreq_decode (data, datalen);
      if (aca.data == NULL)
	return NULL;
    }
  else
    aca.data = NULL;

  if ((ret = malloc (sizeof (aca))) == NULL)
    {
      handler->free_aca (aca.data);
      return NULL;
    }

  memcpy (ret, &aca, sizeof (aca));

  return ret;
}
