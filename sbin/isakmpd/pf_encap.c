/*	$Id: pf_encap.c,v 1.1.1.1 1998/11/15 00:03:49 niklas Exp $	*/

/*
 * Copyright (c) 1998 Niklas Hallqvist.  All rights reserved.
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
#include <sys/mbuf.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <net/encap.h>
#include <netinet/ip_ah.h>
#include <netinet/ip_esp.h>
#include <netinet/ip_ip4.h>
#include <netinet/ip_ipsp.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app.h"
#include "conf.h"
#include "hash.h"
#include "ipsec.h"
#include "ipsec_num.h"
#include "isakmp.h"
#include "log.h"
#include "pf_encap.h"
#include "sa.h"
#include "sysdep.h"
#include "timer.h"
#include "transport.h"

void pf_encap_request_sa (struct encap_msghdr *);

int
pf_encap_open ()
{
  int fd;

  fd = socket (PF_ENCAP, SOCK_RAW, PF_UNSPEC);
  if (fd == -1)
    {
      log_error ("pf_encap_open: "
		 "socket (PF_ENCAP, SOCK_RAW, PF_UNSPEC) failed");
      return -1;
    }
  return fd;
}

static void
pf_encap_expire (struct encap_msghdr *emsg)
{
  /* XXX not implemented yet.  */
}

static void
pf_encap_notify (struct encap_msghdr *emsg)
{
  log_debug_buf (LOG_PF_ENCAP, 90, "pf_encap_notify: emsg", (u_int8_t *)emsg,
		 sizeof *emsg);

  switch (emsg->em_not_type)
    {
    case NOTIFY_SOFT_EXPIRE:
    case NOTIFY_HARD_EXPIRE:
      log_debug (LOG_PF_ENCAP, 20,
		 "pf_encap_handler: NOTIFY_%s_EXPIRE dst %s spi %x sproto %d",
		 emsg->em_not_type == NOTIFY_SOFT_EXPIRE ? "SOFT" : "HARD",
		 inet_ntoa (emsg->em_not_dst), emsg->em_not_spi,
		 emsg->em_not_sproto);
      pf_encap_expire (emsg);
      break;

    case NOTIFY_REQUEST_SA:
      log_debug (LOG_PF_ENCAP, 10,
		 "pf_encap_handler: SA requested for %s type %d",
		 inet_ntoa (emsg->em_not_dst), emsg->em_not_satype);
      pf_encap_request_sa (emsg);
      break;

    default:
      log_print ("pf_encap_handler: unknown notify message type (%d)",
		 emsg->em_not_type);
      break;
    }
  free (emsg);
}

void
pf_encap_handler (int fd)
{
  u_int8_t *buf;
  struct encap_msghdr *emsg;
  ssize_t len;

  /*
   * PF_ENCAP version 1 has a max length equal to the notify length on
   * upcoming packets.
   */
  buf = malloc (EMT_NOTIFY_FLEN);
  if (!buf)
    return;
  emsg = (struct encap_msghdr *)buf;

  len = read (fd, buf, EMT_NOTIFY_FLEN);
  if (len == -1)
    {
      log_error ("pf_encap_handler: read (%d, ...) failed", fd);
      free (emsg);
      return;
    }

  if (emsg->em_version != PFENCAP_VERSION_1)
    {
      log_print ("pf_encap_handler: "
		 "unexpected message version (%d) from PF_ENCAP socket",
		 emsg->em_version);
      free (emsg);
      return;
    }

  if (emsg->em_type != EMT_NOTIFY)
    {
      log_print ("pf_encap_handler: "
		 "unexpected message type (%d) from PF_ENCAP socket",
		 emsg->em_type);
      free (emsg);
      return;
    }

  pf_encap_notify (emsg);
}

void
pf_encap_request_sa (struct encap_msghdr *emsg)
{
  struct transport *transport;
  struct sa *isakmp_sa;
  char addr[20];
  in_port_t port;
  struct sockaddr *taddr;
  int taddr_len;

  /*
   * XXX I'd really want some more flexible way to map the IPv4 address in
   * this message to a general transport endpoint.  For now we hardcode
   * the ISAKMP peer to be at the same IP and talking UDP.
   */
  port = conf_get_num (inet_ntoa (emsg->em_not_dst), "port");
  if (!port)
    port = UDP_DEFAULT_PORT;
  snprintf (addr, 20, "%s:%d", inet_ntoa (emsg->em_not_dst), port);
  transport = transport_create ("udp", addr);
  if (!transport)
    {
      log_print ("pf_encap_request_sa: "
		 "transport \"udp %s\" could not be created",
		 transport, addr);
      return;
    }

  /* Check if we already have an ISAKMP SA setup.  */
  transport->vtbl->get_dst (transport, &taddr, &taddr_len);
  isakmp_sa = sa_isakmp_lookup_by_peer (taddr, taddr_len);
  if (!isakmp_sa)
    /* XXX transport_free (transport)  */ ;
}

/* Write a PF_ENCAP request down to the kernel.  */
static int
pf_encap_write (struct encap_msghdr *em)
{
  ssize_t n;

  em->em_version = PFENCAP_VERSION_1;

  log_debug_buf (LOG_PF_ENCAP, 30, "pf_encap_write: em", (u_int8_t *)em,
		 em->em_msglen);
  n = write (app_socket, em, em->em_msglen);
  if (n == -1)
    {
      log_error ("write (%d, ...) failed", app_socket);
      return -1;
    }
  if ((size_t)n != em->em_msglen)
    {
      log_error ("write (%d, ...) returned prematurely", app_socket);
      return -1;
    }
  return 0;
}

/*
 * Read a PF_ENCAP non-notify packet (e.g. an answer to a request of ours)
 * If we see a notify queue it up as a timeout timing out NOW for the main
 * loop to see.
 */
static struct encap_msghdr *
pf_encap_read ()
{
  u_int8_t *buf;
  ssize_t n;
  struct encap_msghdr *emsg;
  struct timeval now;

  /*
   * PF_ENCAP version 1 has a max length equal to the notify length on
   * upcoming packets.
   */
  buf = malloc (EMT_NOTIFY_FLEN);
  if (!buf)
    goto cleanup;
  emsg = (struct encap_msghdr *)buf;

  while (1)
    {
      /* XXX Should we have a static pf_encap_socket instead?  */
      n = read (app_socket, buf, EMT_NOTIFY_FLEN);
      if (n == -1)
	{
	  log_error ("read (%d, ...) failed", app_socket);
	  goto cleanup;
	}

      if ((size_t)n < EMT_GENLEN || (size_t)n != emsg->em_msglen)
	{
	  log_print ("read (%d, ...) returned short packet (%d bytes)",
		     app_socket, n);
	  goto cleanup;
	}

      /* We drop all messages that is not what we expect.  */
      if (emsg->em_version != PFENCAP_VERSION_1)
	continue;

      /*
       * Enqueue notifications so they will be dealt with as soon as we get
       * back to the main server loop.
       */
      if (emsg->em_type == EMT_NOTIFY)
	{
	  gettimeofday (&now, 0);
	  timer_add_event ("pf_encap_notify",
			   (void (*) (void *))pf_encap_notify, emsg, &now);

	  /* We need a new buffer since we gave our former one away.  */
	  buf = malloc (EMT_NOTIFY_FLEN);
	  if (!buf)
	    goto cleanup;
	  emsg = (struct encap_msghdr *)buf;
	  continue;
	}

      return emsg;
    }

 cleanup:
  if (buf)
    free (buf);
  return 0;
}

u_int8_t *
pf_encap_get_spi (size_t *sz, u_int8_t proto, void *id, size_t id_sz)
{
  struct encap_msghdr *emsg = 0;
  u_int8_t *spi = 0;
  struct sockaddr_in *ipv4_id = id;

  emsg = calloc (1, EMT_RESERVESPI_FLEN);
  if (!emsg)
    return 0;

  emsg->em_msglen = EMT_RESERVESPI_FLEN;
  emsg->em_type = EMT_RESERVESPI;
  emsg->em_gen_spi = 0;
  memcpy (&emsg->em_gen_dst, &ipv4_id->sin_addr, sizeof ipv4_id->sin_addr);
  emsg->em_gen_sproto =
    proto == IPSEC_PROTO_IPSEC_ESP ? IPPROTO_ESP : IPPROTO_AH;

  if (pf_encap_write (emsg))
    goto cleanup;
  free (emsg);
  emsg = pf_encap_read ();
  if (!emsg)
    goto cleanup;

  *sz = sizeof emsg->em_gen_spi;
  spi = malloc (*sz);
  if (!spi)
    goto cleanup;
  memcpy (spi, &emsg->em_gen_spi, *sz);
  free (emsg);

  log_debug_buf (LOG_MISC, 50, "pf_encap_get_spi: spi", spi, *sz);

  return spi;

 cleanup:
  if (emsg)
    free (emsg);
  if (spi)
    free (spi);
  return 0;
}

/* Group 2 SPIs in a chain.  XXX not implemented yet.  */
int
pf_encap_group_spis (struct sa *sa, struct proto *proto1, struct proto *proto2,
		     int role)
{
  struct encap_msghdr *emsg = 0;
  struct sockaddr *dst;
  int dstlen;

  emsg = calloc (1, EMT_GRPSPIS_FLEN);
  if (!emsg)
    return -1;

  emsg->em_msglen = EMT_GRPSPIS_FLEN;
  emsg->em_type = EMT_GRPSPIS;

  memcpy (&emsg->em_rel_spi, proto1->spi[role], sizeof emsg->em_rel_spi);
  memcpy (&emsg->em_rel_spi2, proto2->spi[role],
	  sizeof emsg->em_rel_spi2);
  sa->transport->vtbl->get_dst (sa->transport, &dst, &dstlen);
  emsg->em_rel_dst = emsg->em_rel_dst2 = ((struct sockaddr_in *)dst)->sin_addr;
  /* XXX What if IPCOMP etc. comes along?  */
  emsg->em_rel_sproto
    = proto1->proto == IPSEC_PROTO_IPSEC_ESP ? IPPROTO_ESP : IPPROTO_AH;
  emsg->em_rel_sproto2
    = proto2->proto == IPSEC_PROTO_IPSEC_ESP ? IPPROTO_ESP : IPPROTO_AH;

  if (pf_encap_write (emsg))
    goto cleanup;
  free (emsg);

  log_debug (LOG_MISC, 50, "pf_encap_group_spis: done");

  return 0;

 cleanup:
  if (emsg)
    free (emsg);
  return -1;
}

/* Store/update a SPI with full information into the kernel.  */
int
pf_encap_set_spi (struct sa *sa, struct proto *proto, int role, int initiator)
{
  struct encap_msghdr *emsg = 0;
  struct ipsec_proto *iproto = proto->data;
  struct sockaddr *dst, *src;
  int dstlen, srclen, keylen, hashlen;
  size_t len;
  struct esp_new_xencap *edx;
  struct ah_new_xencap *amx;

  switch (proto->proto)
    {
    case IPSEC_PROTO_IPSEC_ESP:
      keylen = ipsec_esp_enckeylength (proto);
      hashlen = ipsec_esp_authkeylength (proto);
      len = EMT_SETSPI_FLEN + ESP_NEW_XENCAP_LEN + keylen + hashlen + 8;
      emsg = calloc (1, len);
      if (!emsg)
	return -1;

      /* Whenever should the "old" transforms be used?  Policy thing?  */
      emsg->em_alg = XF_NEW_ESP;
      emsg->em_sproto = IPPROTO_ESP;

      edx = (struct esp_new_xencap *)emsg->em_dat;

      switch (proto->id)
	{
	case IPSEC_ESP_DES:
	case IPSEC_ESP_DES_IV32:
	case IPSEC_ESP_DES_IV64:
	  edx->edx_enc_algorithm = ALG_ENC_DES;
	  break;

	case IPSEC_ESP_3DES:
	  edx->edx_enc_algorithm = ALG_ENC_3DES;
	  break;

	case IPSEC_ESP_CAST:
	  edx->edx_enc_algorithm = ALG_ENC_CAST;
	  break;

	case IPSEC_ESP_BLOWFISH:
	  edx->edx_enc_algorithm = ALG_ENC_BLF;
	  break;

	default:
	  /* XXX Log?  */
	  return -1;
	}

      switch (iproto->auth)
	{
	case IPSEC_AUTH_HMAC_MD5:
	  edx->edx_hash_algorithm = ALG_AUTH_MD5;
	  break;

	case IPSEC_AUTH_HMAC_SHA:
	  edx->edx_hash_algorithm = ALG_AUTH_SHA1;
	  break;

	case IPSEC_AUTH_DES_MAC:
	case IPSEC_AUTH_KPDK:
	  /* XXX Log?  */
	  return -1;

	default:
	  edx->edx_hash_algorithm = 0;
	}

      /* XXX What if we have a protocol requiring IV?  */
      edx->edx_ivlen = 8;
      edx->edx_confkeylen = keylen;
      edx->edx_authkeylen = hashlen;
      edx->edx_wnd = 16;
      edx->edx_flags = iproto->auth ? ESP_NEW_FLAG_AUTH : 0;
      memcpy (edx->edx_data + 8, iproto->keymat[role], keylen);
      if (iproto->auth)
	memcpy (edx->edx_data + keylen + 8, iproto->keymat[role] + keylen,
		hashlen);
      break;

    case IPSEC_PROTO_IPSEC_AH:
      hashlen = ipsec_ah_keylength (proto);
      len = EMT_SETSPI_FLEN + AH_NEW_XENCAP_LEN + hashlen;
      emsg = calloc (1, len);
      if (!emsg)
	return -1;

      /* Whenever should the "old" transforms be used?  Policy thing?  */
      emsg->em_alg = XF_NEW_AH;
      emsg->em_sproto = IPPROTO_AH;

      amx = (struct ah_new_xencap *)emsg->em_dat;

      switch (proto->id)
	{
	case IPSEC_AH_MD5:
	  amx->amx_hash_algorithm = ALG_AUTH_MD5;
	  break;

	case IPSEC_AH_SHA:
	  amx->amx_hash_algorithm = ALG_AUTH_SHA1;
	  break;

	default:
	  /* XXX Log?  */
	  goto cleanup;
	}

      amx->amx_keylen = hashlen;
      amx->amx_wnd = 16;
      memcpy (amx->amx_key, iproto->keymat[role], hashlen);
      break;

    default:
      /* XXX Log?  */
      goto cleanup;
    }

  emsg->em_msglen = len;
  emsg->em_type = EMT_SETSPI;
  memcpy (&emsg->em_spi, proto->spi[role], sizeof emsg->em_spi);
  emsg->em_ttl = IP4_DEFAULT_TTL;
  /* Fill in a well-defined value in this reserved field.  */
  emsg->em_satype = 0;

  /*
   * XXX Addresses has to be thought through.  Assumes IPv4.
   */
  sa->transport->vtbl->get_dst (sa->transport, &dst, &dstlen);
  sa->transport->vtbl->get_src (sa->transport, &src, &srclen);
  emsg->em_dst
    = ((struct sockaddr_in *)((initiator ^ role) ? dst : src))->sin_addr;
  emsg->em_src
    = ((struct sockaddr_in *)((initiator ^ role) ? src : dst))->sin_addr;
  if (iproto->encap_mode == IPSEC_ENCAP_TUNNEL)
    {
      emsg->em_odst = emsg->em_dst;
      emsg->em_osrc = emsg->em_src;
    }

  /* XXX I am not sure which one is best in security respect.  */
#if 0
  emsg->em_first_use_hard = (u_int64_t)sa->seconds;
  /* XXX Perhaps we could calculate something out of the last negotiation.  */
  emsg->em_first_use_soft = (u_int64_t)sa->seconds * 9 / 10;
  emsg->em_expire_hard = 0;
  emsg->em_expire_soft = 0;
#else
  emsg->em_expire_hard = time ((time_t *)0) + (u_int64_t)sa->seconds;
  /* XXX Perhaps we could calculate something out of the last negotiation.  */
  emsg->em_expire_soft = time ((time_t *)0) + (u_int64_t)sa->seconds * 9 / 10;
  emsg->em_first_use_hard = 0;
  emsg->em_first_use_soft = 0;
#endif
  emsg->em_bytes_hard = (u_int64_t)sa->kilobytes * 1024;
  /* XXX A configurable ratio would be better.  */
  emsg->em_bytes_soft = (u_int64_t)sa->kilobytes * 1024 * 9 / 10;
  emsg->em_packets_hard = 0;
  emsg->em_packets_soft = 0;

  log_debug (LOG_PF_ENCAP, 10, "pf_encap_set_spi: proto %d dst %s SPI 0x%x",
	     emsg->em_sproto, inet_ntoa (emsg->em_dst), htonl (emsg->em_spi));
  if (pf_encap_write (emsg))
    goto cleanup;
  free (emsg);

  log_debug (LOG_PF_ENCAP, 50, "pf_encap_set_spi: done");

  return 0;

 cleanup:
  if (emsg)
    free (emsg);
  return -1;
}
 
/* Delete a specific SPI from the IPSEC kernel subsystem.  */
int
pf_encap_delete_spi (struct sa *sa, struct proto *proto, int initiator)
{
  struct encap_msghdr *emsg = 0;
  struct sockaddr *dst;
  int dstlen;

  emsg = calloc (1, EMT_DELSPI_FLEN);
  if (!emsg)
    return -1;

  emsg->em_msglen = EMT_DELSPI_FLEN;
  emsg->em_type = EMT_DELSPI;

  memcpy (&emsg->em_gen_spi, proto->spi[initiator], sizeof emsg->em_gen_spi);
  sa->transport->vtbl->get_dst (sa->transport, &dst, &dstlen);
  emsg->em_gen_dst = ((struct sockaddr_in *)dst)->sin_addr;
  /* XXX What if IPCOMP etc. comes along?  */
  emsg->em_gen_sproto
    = proto->proto == IPSEC_PROTO_IPSEC_ESP ? IPPROTO_ESP : IPPROTO_AH;

  if (pf_encap_write (emsg))
    goto cleanup;
  free (emsg);

  log_debug (LOG_MISC, 50, "pf_encap_delete_spi: done");

  return 0;

 cleanup:
  if (emsg)
    free (emsg);
  return -1;
}

/* Enable a flow.  */
int
pf_encap_enable_spi (struct sa *sa, int initiator)
{
  struct encap_msghdr *emsg = 0;
  struct sockaddr *dst, *src;
  int dstlen, srclen;
  struct proto *proto = TAILQ_FIRST (&sa->protos);

  emsg = calloc (1, EMT_ENABLESPI_FLEN);
  if (!emsg)
    return -1;

  emsg->em_msglen = EMT_ENABLESPI_FLEN;
  emsg->em_type = EMT_ENABLESPI;

  sa->transport->vtbl->get_dst (sa->transport, &dst, &dstlen);
  sa->transport->vtbl->get_src (sa->transport, &src, &srclen);

  memcpy (&emsg->em_ena_spi, proto->spi[!initiator], sizeof emsg->em_ena_spi);
  emsg->em_ena_dst = ((struct sockaddr_in *)dst)->sin_addr;

  emsg->em_ena_isrc.s_addr = ((struct sockaddr_in *)src)->sin_addr.s_addr;
  emsg->em_ena_ismask.s_addr = 0xffffffff;
  emsg->em_ena_idst.s_addr = emsg->em_ena_dst.s_addr;
  emsg->em_ena_idmask.s_addr = 0xffffffff;
  /* XXX How to deduce if we need ENABLE_FLAG_LOCAL?  */
  emsg->em_ena_flags = ENABLE_FLAG_REPLACE;

  /* XXX What if IPCOMP etc. comes along?  */
  emsg->em_ena_sproto
    = proto->proto == IPSEC_PROTO_IPSEC_ESP ? IPPROTO_ESP : IPPROTO_AH;

  if (pf_encap_write (emsg))
    goto cleanup;

  /*
   * XXX The condition should be true if this machine is part of the source
   * subnet.
   */
  if (1)
    {
      /*
       * This "route" is used for packets from this host where the source
       * address has not yet been decided.
       */
      emsg->em_ena_flags |= ENABLE_FLAG_LOCAL;
      if (pf_encap_write (emsg))
	goto cleanup;
    }
  free (emsg);

  log_debug (LOG_MISC, 50, "pf_encap_enable_spi: done");

  return 0;

 cleanup:
  if (emsg)
    free (emsg);
  return -1;
}
