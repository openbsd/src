/*	$OpenBSD: pf_encap.c,v 1.16 2000/02/25 17:23:41 niklas Exp $	*/
/*	$EOM: pf_encap.c,v 1.70 2000/02/20 19:58:40 niklas Exp $	*/

/*
 * Copyright (c) 1998, 1999 Niklas Hallqvist.  All rights reserved.
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
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <net/route.h>
#include <netinet/in.h>
#include <net/encap.h>
#include <netinet/ip_ah.h>
#include <netinet/ip_esp.h>
#include <netinet/ip_ip4.h>
#include <netinet/ip_ipsp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sysdep.h"

#include "conf.h"
#include "exchange.h"
#include "hash.h"
#include "ipsec.h"
#include "ipsec_num.h"
#include "isakmp.h"
#include "log.h"
#include "pf_encap.h"
#include "sa.h"
#include "timer.h"
#include "transport.h"

#define ROUNDUP(a) \
  ((a) > 0 ? (1 + (((a) - 1) | (sizeof (long) - 1))) : sizeof (long))

static void pf_encap_deregister_on_demand_connection (char *);
static int pf_encap_register_on_demand_connection (in_addr_t, char *);
static void pf_encap_request_sa (struct encap_msghdr *);

struct on_demand_connection {
  /* Connections are linked together.  */
  LIST_ENTRY (on_demand_connection) link;

  /* The security gateway's IP-address.  */
  in_addr_t dst;

  /* The name of a phase 2 connection associated with the security gateway.  */
  char *conn;
};

static LIST_HEAD (on_demand_connection_list_list, on_demand_connection)
  on_demand_connections;

static int pf_encap_socket;

void
pf_encap_init ()
{
  LIST_INIT (&on_demand_connections);
}

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
  pf_encap_socket = fd;
  return fd;
}

static void
pf_encap_expire (struct encap_msghdr *emsg)
{
  struct sa *sa;

  LOG_DBG ((LOG_SYSDEP, 20,
	    "pf_encap_expire: NOTIFY_%s_EXPIRE dst %s spi %x sproto %d",
	    emsg->em_not_type == NOTIFY_SOFT_EXPIRE ? "SOFT" : "HARD",
	    inet_ntoa (emsg->em_not_dst), htonl (emsg->em_not_spi),
	    emsg->em_not_sproto));

  /*
   * Find the IPsec SA.  The IPsec stack has two SAs for every IKE SA,
   * one outgoing and one incoming, we regard expirations for any of
   * them as an expiration of the full IKE SA.  Likewise, in
   * protection suites consisting of more than one protocol, any
   * expired individual IPsec stack SA will be seen as an expiration
   * of the full suite.
   *
   * XXX When anything else than AH and ESP is supported this needs to change.
   */
  sa = ipsec_sa_lookup (emsg->em_not_dst.s_addr, emsg->em_not_spi,
			emsg->em_not_sproto == IPPROTO_ESP
			? IPSEC_PROTO_IPSEC_ESP : IPSEC_PROTO_IPSEC_AH);

  /* If the SA is already gone, don't do anything.  */
  if (!sa)
    return;

  /*
   * If we want this connection to stay "forever", we should renegotiate
   * already at the soft expire, and certainly at the hard expire if we
   * haven't started a negotiation by then.
   */
  if ((sa->flags & (SA_FLAG_STAYALIVE | SA_FLAG_REPLACED))
      == SA_FLAG_STAYALIVE)
    exchange_establish (sa->name, 0, 0);

  if (emsg->em_not_type == NOTIFY_HARD_EXPIRE)
    {
      /*
       * XXX This should not be necessary anymore due to the 
       *     connection abstraction.
       */
#if 0
      /*
       * If the expired SA is something we know how to renegotiate, and it
       * has not already been replaced.  Establish routes that requests SAs
       * from us on use.
       */
      if (sa->name && (sa->flags & SA_FLAG_REPLACED) == 0)
	/*
	 * We reestablish the on-demand route here even if we have started
	 * a new negotiation, considering it might fail.
	 */
	pf_encap_connection_check (sa->name);
#endif

      /* Remove the old SA, it isn't useful anymore.  */
      sa_free (sa);
    }
}

static void
pf_encap_notify (struct encap_msghdr *emsg)
{
  LOG_DBG_BUF ((LOG_SYSDEP, 90, "pf_encap_notify: emsg", (u_int8_t *)emsg,
		emsg->em_msglen));

  switch (emsg->em_not_type)
    {
    case NOTIFY_SOFT_EXPIRE:
    case NOTIFY_HARD_EXPIRE:
      pf_encap_expire (emsg);
      break;

    case NOTIFY_REQUEST_SA:
      pf_encap_request_sa (emsg);
      break;

    default:
      log_print ("pf_encap_notify: unknown notify message type (%d)",
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
  int n;

  /*
   * As synchronous read/writes to the socket can have taken place between
   * the select(2) call of the main loop and this handler, we need to recheck
   * the readability.
   */
  if (ioctl (pf_encap_socket, FIONREAD, &n) == -1)
    {
      log_error ("pf_encap_handler: ioctl (%d, FIONREAD, &n) failed",
		 pf_encap_socket);
      return;
    }
  if (!n)
    return;

  /*
   * PF_ENCAP version 1 has a max length equal to the notify length on
   * upcoming packets.
   */
  buf = malloc (EMT_NOTIFY_FLEN);
  if (!buf)
    {
      log_error ("pf_encap_handler: malloc (%d) failed", EMT_NOTIFY_FLEN);
      return;
    }
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

/* Write a PF_ENCAP request down to the kernel.  */
static int
pf_encap_write (struct encap_msghdr *em)
{
  ssize_t n;

  em->em_version = PFENCAP_VERSION_1;

  LOG_DBG_BUF ((LOG_SYSDEP, 30, "pf_encap_write: em", (u_int8_t *)em,
		em->em_msglen));
  n = write (pf_encap_socket, em, em->em_msglen);
  if (n == -1)
    {
      log_error ("pf_encap_write: write (%d, ...) failed", pf_encap_socket);
      return -1;
    }
  if ((size_t)n != em->em_msglen)
    {
      log_error ("pf_encap_write: write (%d, ...) returned prematurely", pf_encap_socket);
      return -1;
    }
  return 0;
}

/*
 * We are asked to setup an SA that can protect packets like the one described
 * in EMSG.  We are supposed to deallocate EMSG too.
 */
static void
pf_encap_request_sa (struct encap_msghdr *emsg)
{
  struct on_demand_connection *node;

  LOG_DBG ((LOG_SYSDEP, 10,
	    "pf_encap_request_sa: SA requested for %s type %d",
	    inet_ntoa (emsg->em_not_dst), emsg->em_not_satype));

  /*
   * In my mind this is rediculous, PF_ENCAP is just broken.  Well, to
   * describe how it is broken it suffices to say that REQUEST_SA messages
   * does not tell which of all connections using a specific security
   * gateway needs to be brought up.  So we have to bring them all up.
   * I won't bother replying to the PF_ENCAP socket because the kernel
   * does not require it when this request is due to a SPI 1 route.
   */  
  for (node = LIST_FIRST (&on_demand_connections); node;
       node = LIST_NEXT (node, link))
    if (emsg->em_not_dst.s_addr == node->dst
	&& !sa_lookup_by_name (node->conn, 2))
      exchange_establish (node->conn, 0, 0);
}

/*
 * Read a PF_ENCAP non-notify packet (e.g. an answer to a request of ours)
 * If we see a notify queue it up as a timeout timing out now for the main
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
      n = read (pf_encap_socket, buf, EMT_NOTIFY_FLEN);
      if (n == -1)
	{
	  log_error ("read (%d, ...) failed", pf_encap_socket);
	  goto cleanup;
	}

      if ((size_t)n < EMT_GENLEN || (size_t)n != emsg->em_msglen)
	{
	  log_print ("read (%d, ...) returned short packet (%d bytes)",
		     pf_encap_socket, n);
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

/*
 * Generate a SPI for protocol PROTO and the source/destination pair given by
 * SRC, SRCLEN, DST & DSTLEN.  Stash the SPI size in SZ.
 */
u_int8_t *
pf_encap_get_spi (size_t *sz, u_int8_t proto, struct sockaddr *src, int srclen,
		  struct sockaddr *dst, int dstlen)
{
  struct encap_msghdr *emsg = 0;
  u_int8_t *spi = 0;
  struct sockaddr_in *ipv4_dst = (struct sockaddr_in *)dst;

  emsg = calloc (1, EMT_RESERVESPI_FLEN);
  if (!emsg)
    return 0;

  emsg->em_msglen = EMT_RESERVESPI_FLEN;
  emsg->em_type = EMT_RESERVESPI;
  emsg->em_gen_spi = 0;
  memcpy (&emsg->em_gen_dst, &ipv4_dst->sin_addr, sizeof ipv4_dst->sin_addr);
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

  LOG_DBG_BUF ((LOG_SYSDEP, 50, "pf_encap_get_spi: spi", spi, *sz));

  return spi;

 cleanup:
  if (emsg)
    free (emsg);
  if (spi)
    free (spi);
  return 0;
}

/* Group 2 SPIs in a chain.  */
int
pf_encap_group_spis (struct sa *sa, struct proto *proto1, struct proto *proto2,
		     int incoming)
{
  struct encap_msghdr *emsg = 0;
  struct sockaddr *dst;
  int dstlen;

  emsg = calloc (1, EMT_GRPSPIS_FLEN);
  if (!emsg)
    return -1;

  emsg->em_msglen = EMT_GRPSPIS_FLEN;
  emsg->em_type = EMT_GRPSPIS;

  memcpy (&emsg->em_rel_spi, proto1->spi[incoming], sizeof emsg->em_rel_spi);
  memcpy (&emsg->em_rel_spi2, proto2->spi[incoming],
	  sizeof emsg->em_rel_spi2);
  if (incoming)
    sa->transport->vtbl->get_src (sa->transport, &dst, &dstlen);
  else
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

  LOG_DBG ((LOG_SYSDEP, 50, "pf_encap_group_spis: done"));

  return 0;

 cleanup:
  if (emsg)
    free (emsg);
  return -1;
}

/*
 * Store/update a PF_KEY_V2 security association with full information from the
 * IKE SA and PROTO into the kernel.  INCOMING is set if we are setting the
 * parameters for the incoming SA, and cleared otherwise.
 */
int
pf_encap_set_spi (struct sa *sa, struct proto *proto, int incoming)
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
      edx->edx_wnd
	= conf_get_str ("General", "Shared-SADB") ? -1 : iproto->replay_window;
      edx->edx_flags = iproto->auth ? ESP_NEW_FLAG_AUTH : 0;
      memcpy (edx->edx_data + 8, iproto->keymat[incoming], keylen);
      if (iproto->auth)
	memcpy (edx->edx_data + keylen + 8, iproto->keymat[incoming] + keylen,
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
      amx->amx_wnd
	= conf_get_str ("General", "Shared-SADB") ? -1 : iproto->replay_window;
      memcpy (amx->amx_key, iproto->keymat[incoming], hashlen);
      break;

    default:
      /* XXX Log?  */
      goto cleanup;
    }

  emsg->em_msglen = len;
  emsg->em_type = EMT_SETSPI;
  memcpy (&emsg->em_spi, proto->spi[incoming], sizeof emsg->em_spi);
  emsg->em_ttl = IP4_DEFAULT_TTL;
  /* Fill in a well-defined value in this reserved field.  */
  emsg->em_satype = 0;

  sa->transport->vtbl->get_dst (sa->transport, &dst, &dstlen);
  sa->transport->vtbl->get_src (sa->transport, &src, &srclen);
  emsg->em_dst
    = ((struct sockaddr_in *)(incoming ? src : dst))->sin_addr;
  emsg->em_src
    = ((struct sockaddr_in *)(incoming ? dst : src))->sin_addr;
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
  emsg->em_expire_hard
    = sa->seconds ? time ((time_t *)0) + (u_int64_t)sa->seconds : 0;
  /* XXX Perhaps we could calculate something out of the last negotiation.  */
  emsg->em_expire_soft
    = sa->seconds ? time ((time_t *)0) + (u_int64_t)sa->seconds * 9 / 10 : 0;
  emsg->em_first_use_hard = 0;
  emsg->em_first_use_soft = 0;
#endif
  emsg->em_bytes_hard = (u_int64_t)sa->kilobytes * 1024;
  /* XXX A configurable ratio would be better.  */
  emsg->em_bytes_soft = (u_int64_t)sa->kilobytes * 1024 * 9 / 10;
  emsg->em_packets_hard = 0;
  emsg->em_packets_soft = 0;

  LOG_DBG ((LOG_SYSDEP, 10, "pf_encap_set_spi: proto %d dst %s SPI 0x%x",
	    emsg->em_sproto, inet_ntoa (emsg->em_dst),
	    htonl (emsg->em_spi)));
  if (pf_encap_write (emsg))
    goto cleanup;
  free (emsg);

  LOG_DBG ((LOG_SYSDEP, 50, "pf_encap_set_spi: done"));

  return 0;

 cleanup:
  if (emsg)
    free (emsg);
  return -1;
}
 
/*
 * Delete the IPSec SA represented by the INCOMING direction in protocol PROTO
 * of the IKE security association SA.
 */
int
pf_encap_delete_spi (struct sa *sa, struct proto *proto, int incoming)
{
  struct encap_msghdr *emsg = 0;
  struct sockaddr *dst;
  int dstlen;

  emsg = calloc (1, EMT_DELSPI_FLEN);
  if (!emsg)
    return -1;

  emsg->em_msglen = EMT_DELSPI_FLEN;
  emsg->em_type = EMT_DELSPI;

  memcpy (&emsg->em_gen_spi, proto->spi[incoming], sizeof emsg->em_gen_spi);
  if (incoming)
    sa->transport->vtbl->get_src (sa->transport, &dst, &dstlen);
  else
    sa->transport->vtbl->get_dst (sa->transport, &dst, &dstlen);
  emsg->em_gen_dst = ((struct sockaddr_in *)dst)->sin_addr;
  /* XXX What if IPCOMP etc. comes along?  */
  emsg->em_gen_sproto
    = proto->proto == IPSEC_PROTO_IPSEC_ESP ? IPPROTO_ESP : IPPROTO_AH;

  if (pf_encap_write (emsg))
    goto cleanup;
  free (emsg);

  LOG_DBG ((LOG_SYSDEP, 50, "pf_encap_delete_spi: done"));

  return 0;

 cleanup:
  if (emsg)
    free (emsg);
  return -1;
}

/* Enable a flow given an SA.  */
int
pf_encap_enable_sa (struct sa *sa)
{
  struct ipsec_sa *isa = sa->data;
  struct sockaddr *dst;
  int dstlen;
  struct proto *proto = TAILQ_FIRST (&sa->protos);

  sa->transport->vtbl->get_dst (sa->transport, &dst, &dstlen);

  return pf_encap_enable_spi (isa->src_net, isa->src_mask, isa->dst_net,
			      isa->dst_mask, proto->spi[0], proto->proto,
			      ((struct sockaddr_in *)dst)->sin_addr.s_addr);
}

/* Enable a flow.  */
int
pf_encap_enable_spi (in_addr_t laddr, in_addr_t lmask, in_addr_t raddr,
		     in_addr_t rmask, u_int8_t *spi, u_int8_t proto,
		     in_addr_t dst)
{
  struct encap_msghdr *emsg = 0;

  emsg = calloc (1, EMT_ENABLESPI_FLEN);
  if (!emsg)
    /* XXX Log?  */
    return -1;

  emsg->em_msglen = EMT_ENABLESPI_FLEN;
  emsg->em_type = EMT_ENABLESPI;

  memcpy (&emsg->em_ena_spi, spi, sizeof emsg->em_ena_spi);
  emsg->em_ena_dst.s_addr = dst;

  LOG_DBG ((LOG_SYSDEP, 50, "pf_encap_enable_spi: src %x %x dst %x %x",
	    htonl(laddr), htonl(lmask), htonl(raddr), htonl(rmask)));
  emsg->em_ena_isrc.s_addr = laddr;
  emsg->em_ena_ismask.s_addr = lmask;
  emsg->em_ena_idst.s_addr = raddr;
  emsg->em_ena_idmask.s_addr = rmask;
  emsg->em_ena_flags = ENABLE_FLAG_REPLACE;

  /* XXX What if IPCOMP etc. comes along?  */
  emsg->em_ena_sproto
    = proto == IPSEC_PROTO_IPSEC_ESP ? IPPROTO_ESP : IPPROTO_AH;

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
  LOG_DBG ((LOG_SYSDEP, 50, "pf_encap_enable_spi: done"));
  return 0;

 cleanup:
  if (emsg)
    free (emsg);
  return -1;
}

/*
 * Establish an encap route.
 * XXX We should add delete support here a la ipsecadm/xf_flow.c the day
 * we want to clean up after us.
 */
static int
pf_encap_route (in_addr_t laddr, in_addr_t lmask, in_addr_t raddr,
		in_addr_t rmask, in_addr_t dst)
{
  int s = -1;
  int off;
  struct sockaddr_encap *ddst, *msk, *gw;
  struct rt_msghdr *rtmsg = 0;

  rtmsg = calloc (1,
		  sizeof *rtmsg + 2 * ROUNDUP (SENT_IP4_LEN)
		  + ROUNDUP (SENT_IPSP_LEN));
  if (!rtmsg)
    {
      log_error ("pf_encap_route: calloc (1, %d) failed",
		 sizeof *rtmsg + 2 * ROUNDUP (SENT_IP4_LEN)
		 + ROUNDUP (SENT_IPSP_LEN));
      goto fail;
    }

  s = socket (PF_ROUTE, SOCK_RAW, AF_UNSPEC);
  if (s == -1)
    {
      log_error ("pf_encap_route: "
		 "socket(PF_ROUTE, SOCK_RAW, AF_UNSPEC) failed");
      goto fail;
    }

  off = sizeof *rtmsg;
  ddst = (struct sockaddr_encap *)((char *)rtmsg + off);
  off = ROUNDUP (off + SENT_IP4_LEN);
  gw = (struct sockaddr_encap *)((char *)rtmsg + off);
  off = ROUNDUP (off + SENT_IPSP_LEN);
  msk = (struct sockaddr_encap *)((char *)rtmsg + off);
  bzero (rtmsg, off + SENT_IP4_LEN);
	
  rtmsg->rtm_version = RTM_VERSION;
  rtmsg->rtm_type = RTM_ADD;
  rtmsg->rtm_index = 0;
  rtmsg->rtm_pid = getpid ();
  rtmsg->rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK;
  rtmsg->rtm_errno = 0;
  rtmsg->rtm_flags = RTF_UP | RTF_GATEWAY | RTF_STATIC;
  rtmsg->rtm_inits = 0;
	
  ddst->sen_len = SENT_IP4_LEN;
  ddst->sen_family = AF_ENCAP;
  ddst->sen_type = SENT_IP4;
  ddst->sen_ip_src.s_addr = laddr & lmask;
  ddst->sen_ip_dst.s_addr = raddr & rmask;
  ddst->sen_proto = ddst->sen_sport = ddst->sen_dport = 0;

  gw->sen_len = SENT_IPSP_LEN;
  gw->sen_family = AF_ENCAP;
  gw->sen_type = SENT_IPSP;
  gw->sen_ipsp_dst.s_addr = dst;
  gw->sen_ipsp_spi = htonl(1);
  gw->sen_ipsp_sproto = 0;	/* XXX Correct?  */

  msk->sen_len = SENT_IP4_LEN;
  msk->sen_family = AF_ENCAP;
  msk->sen_type = SENT_IP4;
  msk->sen_ip_src.s_addr = lmask;
  msk->sen_ip_dst.s_addr = rmask;

  rtmsg->rtm_msglen = off + msk->sen_len;

  LOG_DBG ((LOG_SYSDEP, 70, "pf_encap_route: rtmsg", rtmsg,
	    rtmsg->rtm_msglen));
  if (write(s, (caddr_t)rtmsg, rtmsg->rtm_msglen) == -1)
    {
      if (errno == EEXIST)
	{
	  rtmsg->rtm_type = RTM_CHANGE;

	  LOG_DBG ((LOG_SYSDEP, 70, "pf_encap_route: rtmsg", rtmsg,
		    rtmsg->rtm_msglen));
	  if (write(s, (caddr_t)rtmsg, rtmsg->rtm_msglen) == -1)
	    {
	      log_error("pf_encap_route: write(%d, %p, %d) failed", s, rtmsg,
			rtmsg->rtm_msglen);
	      goto fail;
	    }
	}
      else
	{
	  log_error("pf_encap_route: write(%d, %p, %d) failed", s, rtmsg,
		    rtmsg->rtm_msglen);
	  goto fail;
	}
    }

  /* XXX Local packet route should be setup here.  */

  /*
   * Setup a reverse map, address -> name, we can use when getting SA
   * requests back from the stack.
   */

  close (s);
  free (rtmsg);

  LOG_DBG ((LOG_SYSDEP, 30, "pf_encap_route: done"));
  return 0;

 fail:
  if (s != -1)
    close (s);
  if (rtmsg)
    free (rtmsg);
  return -1;
}

/* Check that the CONN connection has SPI 1 routes in-place.  */
void
pf_encap_connection_check (char *conn)
{
  char *conf, *doi_str, *local_id, *remote_id, *peer, *address;
  struct in_addr laddr, lmask, raddr, rmask, gwaddr;
  int lid, rid, err;

  if (sa_lookup_by_name (conn, 2) || exchange_lookup_by_name (conn, 2))
    {
      LOG_DBG ((LOG_SYSDEP, 70,
		"pf_encap_connection_check: SA or exchange for %s exists", 
		conn));
      return;
    }

  /* Figure out the DOI.  We only handle IPsec so far.  */
  conf = conf_get_str (conn, "Configuration");
  if (!conf)
    {
      log_print ("pf_encap_connection_check: "
		 "no \"Configuration\" specified for %s",
		 conn);
      return;
    }
  doi_str = conf_get_str (conf, "DOI");
  if (!doi_str)
    {
      log_print ("pf_encap_connection_check: No DOI specified for %s", conf);
      return;
    }
  if (strcasecmp (doi_str, "IPSEC") != 0)
    {
      log_print ("pf_encap_connection_check: DOI \"%s\" unsupported", doi_str);
      return;
    }

  local_id = conf_get_str (conn, "Local-ID");
  remote_id = conf_get_str (conn, "Remote-ID");

  /* At the moment I only do on-demand keying for modes with client IDs.  */
  if (!local_id || !remote_id)
    {
      log_print ("pf_encap_connection_check: "
		 "both Local-ID and Remote-ID required for %s", conn);
      return;
    }

  if (ipsec_get_id (local_id, &lid, &laddr, &lmask))
    return;
  if (ipsec_get_id (remote_id, &rid, &raddr, &rmask))
    return;

  peer = conf_get_str (conn, "ISAKMP-peer");
  if (!peer)
    {
      log_print ("pf_encap_connection_check: "
		 "section %s has no \"ISAKMP-peer\" tag", conn);
      return;
    }
  address = conf_get_str (peer, "Address");
  if (!address)
    {
      log_print ("pf_encap_connection_check: "
		 "section %s has no \"Address\" tag",
		 peer);
      return;
    }
  if (!inet_aton (address, &gwaddr))
    {
      log_print ("pf_encap_connection_check: invalid adress %s in section %s",
		 address, peer);
      return;
    }

  err = pf_encap_register_on_demand_connection (gwaddr.s_addr, conn);
  if (err)
    return;

  if (pf_encap_route (laddr.s_addr, lmask.s_addr, raddr.s_addr, rmask.s_addr,
		      gwaddr.s_addr))
    {
      pf_encap_deregister_on_demand_connection (conn);
      return;
    }
}

/* Lookup an on-demand connection from its name: CONN.  */
static struct on_demand_connection *
pf_encap_lookup_on_demand_connection (char *conn)
{
  struct on_demand_connection *node;

  for (node = LIST_FIRST (&on_demand_connections); node;
       node = LIST_NEXT (node, link))
    if (strcasecmp (conn, node->conn) == 0)
      return node;
  return 0;
}

/*
 * Register an IP-address to Phase 2 connection name mapping.
 */
static int
pf_encap_register_on_demand_connection (in_addr_t dst, char *conn)
{
  struct on_demand_connection *node;

  /* Don't add duplicates.  */
  if (pf_encap_lookup_on_demand_connection (conn))
    return 0;

  node = malloc (sizeof *node);
  if (!node)
    {
      log_error ("pf_encap_register_on_demand_connection: malloc (%d) failed",
		 sizeof *node);
      return -1;
    }

  node->dst = dst;
  node->conn = strdup (conn);
  if (!node->conn)
    {
      log_error ("pf_encap_register_on_demand_connection: "
		 "strdup (\"%s\") failed",
		 conn);
      free (node);
      return -1;
    }

  LIST_INSERT_HEAD (&on_demand_connections, node, link);
  return 0;
}

/*
 * Remove an IP-address to Phase 2 connection name mapping.
 */
static void
pf_encap_deregister_on_demand_connection (char *conn)
{
  struct on_demand_connection *node;

  node = pf_encap_lookup_on_demand_connection (conn);
  if (node)
    {
      LIST_REMOVE (node, link);
      free (node->conn);
      free (node);
    }
}
