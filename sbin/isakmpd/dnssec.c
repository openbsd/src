/*	$OpenBSD: dnssec.c,v 1.2 2001/01/27 12:03:32 niklas Exp $	*/

/*
 * Copyright (c) 2001 Håkan Olsson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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

#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <dns/keyvalues.h>
#include <lwres/lwres.h>
#include <lwres/netdb.h>

#include <openssl/rsa.h>

#include "sysdep.h"

#include "exchange.h"
#include "log.h"
#include "message.h"
#include "transport.h"

#include "ipsec_num.h"
#include "dnssec.h"

/* adapted from <dns/rdatastruct.h> / RFC 2535  */
struct dns_rdata_key {
  u_int16_t      flags;
  u_int8_t       protocol;
  u_int8_t       algorithm;
  u_int16_t      datalen;
  unsigned char *data;
};

/* XXX IPv4 specific */
void *
dns_get_key (int type, struct message *msg, int *keylen)
{
  struct rrsetinfo *rr;
  struct hostent *hostent;
  struct sockaddr_in *dst;
  int ret, i;
  struct dns_rdata_key key_rr;
  u_int8_t algorithm;

  switch (type)
    {
    case IKE_AUTH_RSA_SIG:
      algorithm = DNS_KEYALG_RSA;
      break;

    case IKE_AUTH_RSA_ENC:
    case IKE_AUTH_RSA_ENC_REV:
      /* XXX Not yes. */
      /* algorithm = DNS_KEYALG_RSA; */
      return NULL;

    case IKE_AUTH_DSS:
      /* XXX Not yet. */
      /* algorithm = DNS_KEYALG_DSS; */
      return NULL;

    case IKE_AUTH_PRE_SHARED: 
    default:
      return NULL;
    }

  /* Get peer IP address */
  msg->transport->vtbl->get_dst (msg->transport, (struct sockaddr **)&dst, &i);
  /* Get peer name and aliases */
  hostent = lwres_gethostbyaddr ((char *)&dst->sin_addr, 
				 sizeof (struct in_addr), PF_INET);

  if (!hostent)
    {
      LOG_DBG ((LOG_MISC, 30, 
		"dns_get_key: lwres_gethostbyaddr (%s) failed: %s", 
		inet_ntoa (((struct sockaddr_in *)dst)->sin_addr),
		lwres_hstrerror (lwres_h_errno)));
      return NULL;
    }

  /* Try host official name */
  LOG_DBG ((LOG_MISC, 50, "dns_get_key: trying KEY RR for %s", 
	    hostent->h_name));
  ret = lwres_getrrsetbyname (hostent->h_name, C_IN, T_KEY, 0, &rr);
  if (ret)
    {
      /* Try host aliases */
      i = 0;
      while (hostent->h_aliases[i] && ret)
	{
	  LOG_DBG ((LOG_MISC, 50, "dns_get_key: trying KEY RR for alias %s", 
		    hostent->h_aliases[i]));
	  ret = lwres_getrrsetbyname (hostent->h_aliases[i], C_IN, T_KEY, 0,
				      &rr);
	  i ++;
	}
    }

  if (ret)
    {
      LOG_DBG ((LOG_MISC, 30, "dns_get_key: no DNS responses (error %d)", 
		ret));
      return NULL;
    }
  
  LOG_DBG ((LOG_MISC, 80, 
	    "dns_get_key: rrset class %d type %d ttl %d nrdatas %d nrsigs %d",
	    rr->rri_rdclass, rr->rri_rdtype, rr->rri_ttl, rr->rri_nrdatas, 
	    rr->rri_nsigs));
  
  /* We don't accept unvalidated data. */
  if (!(rr->rri_flags & RRSET_VALIDATED))
    {
      LOG_DBG ((LOG_MISC, 10, "dns_get_key: got unvalidated response"));
      lwres_freerrset (rr);
      return NULL;
    }
  
  /* Sanity. */
  if (rr->rri_nrdatas == 0 || rr->rri_rdtype != T_KEY)
    {
      LOG_DBG ((LOG_MISC, 30, "dns_get_key: no KEY RRs recieved"));
      lwres_freerrset (rr);
      return NULL;
    } 

  memset (&key_rr, 0, sizeof (key_rr));

  /* 
   * Find a key with the wanted algorithm, if any.
   * XXX If there are several keys present, we currently only find the first.
   */
  for (i = 0; i < rr->rri_nrdatas && key_rr.datalen == 0; i ++)
    {
      key_rr.flags     = ntohs ((u_int16_t) *rr->rri_rdatas[i].rdi_data);
      key_rr.protocol  = *(rr->rri_rdatas[i].rdi_data + 2);
      key_rr.algorithm = *(rr->rri_rdatas[i].rdi_data + 3);

      if (key_rr.protocol != DNS_KEYPROTO_IPSEC)
	{
	  LOG_DBG ((LOG_MISC, 50, "dns_get_key: ignored non-IPSEC key"));
	  continue;
	}

      if (key_rr.algorithm != algorithm)
	{
	  LOG_DBG ((LOG_MISC, 50, "dns_get_key: ignored key with other alg"));
	  continue;
	}

      key_rr.datalen   = rr->rri_rdatas[i].rdi_length - 4;
      if (key_rr.datalen <= 0)
	{
	  LOG_DBG ((LOG_MISC, 50, "dns_get_key: ignored bad key"));
	  key_rr.datalen = 0;
	  continue;
	}

      /* This key seems to fit our requirements... */
      key_rr.data = (char *)malloc (key_rr.datalen);
      if (!key_rr.data)
	{
	  log_error ("dns_get_key: malloc (%d) failed", key_rr.datalen);
	  lwres_freerrset (rr);
	  return NULL;
	}
      memcpy (key_rr.data, rr->rri_rdatas[i].rdi_data + 4, key_rr.datalen);
      *keylen = key_rr.datalen;
    }

  lwres_freerrset (rr);

  if (key_rr.datalen)
    return key_rr.data;
  else
    return NULL;
}

int
dns_RSA_dns_to_x509 (u_int8_t *key, int keylen, RSA **rsa_key)
{
  RSA *rsa;
  int key_offset;
  u_int8_t e_len;

  if (!key || keylen <= 0)
    {
      log_print ("dns_RSA_dns_to_x509: invalid public key");
      return -1;
    }

  rsa = RSA_new ();
  if (!rsa)
    {
      log_error ("dns_RSA_dns_to_x509: failed to allocate new RSA struct");
      return -1;
    }

  e_len = *key;
  key_offset = 1;

  if (e_len == 0)
    {
      if (keylen < 3)
	{
	  log_print ("dns_RSA_dns_to_x509: invalid public key");
	  RSA_free (rsa);
	  return -1;
	}
      e_len  = *(key + key_offset++) << 8;
      e_len += *(key + key_offset++);
    }

  if (e_len > (keylen - key_offset))
    {
      log_print ("dns_RSA_dns_to_x509: invalid public key");
      RSA_free (rsa);
      return -1;
    }

  rsa->e = BN_bin2bn (key + key_offset, e_len, NULL);
  key_offset += e_len;

  /* XXX if (keylen <= key_offset) -> "invalid public key" ? */

  rsa->n = BN_bin2bn (key + key_offset, keylen - key_offset, NULL);

  *rsa_key = rsa;

  LOG_DBG ((LOG_MISC, 30, "dns_RSA_dns_to_x509: got %d bits RSA key",
	    BN_num_bits (rsa->n)));

  return 0;
}

#if notyet
int
dns_RSA_x509_to_dns (RSA *rsa_key, u_int8_t *key, int *keylen)
{
  return 0;
}
#endif
