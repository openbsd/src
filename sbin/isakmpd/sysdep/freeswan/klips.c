/*	$OpenBSD: klips.c,v 1.3 2003/09/26 15:59:34 aaron Exp $	*/

/*
 * Copyright (c) 1999 Niklas Hallqvist.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include <asm/types.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <linux/sockios.h>
#include <net/route.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include <freeswan.h>
#include <net/ipsec/radij.h>
#include <net/ipsec/ipsec_encap.h>
#include <net/ipsec/ipsec_netlink.h>
#include <net/ipsec/ipsec_xform.h>
#include <net/ipsec/ipsec_ipe4.h>
#include <net/ipsec/ipsec_ah.h>
#include <net/ipsec/ipsec_esp.h>

#include "sysdep.h"

#include "conf.h"
#include "exchange.h"
#include "hash.h"
#include "ipsec.h"
#include "ipsec_doi.h"
#include "ipsec_num.h"
#include "isakmp.h"
#include "log.h"
#include "klips.h"
#include "sa.h"
#include "timer.h"
#include "transport.h"

#define KLIPS_DEVICE "/dev/ipsec"

#define PROC_ROUTE_FILE	"/proc/net/route"
#define PROC_ROUTE_FMT	"%15s %127s %127s %X %d %d %d %127s %d %d %d\n"

/* XXX Maybe these are available through some system-supplied define?  */
#define AH_NEW_XENCAP_LEN (3 * sizeof(u_short) + 2 * sizeof(u_char))
#define ESP_NEW_XENCAP_LEN sizeof (struct espblkrply_edata)
#define EMT_GRPSPIS_COMPLEN (sizeof (((struct encap_msghdr *)0)->em_rel[0]))

/* How often should we check that connections we require to be up, are up?  */
#define KLIPS_CHECK_FREQ 60

static int klips_socket;

/* Open the KLIPS device.  */
int
klips_open ()
{
  int fd;

  fd = open (KLIPS_DEVICE, O_RDWR);
  if (fd == -1)
    {
      log_error ("klips_open: open (\"%s\", O_RDWR) failed", KLIPS_DEVICE);
      return -1;
    }
  klips_socket = fd;
  return fd;
}

/* Write a KLIPS request down to the kernel.  */
static int
klips_write (struct encap_msghdr *em)
{
  ssize_t n;

  em->em_magic = EM_MAGIC;
  em->em_version = 0;

  LOG_DBG_BUF ((LOG_SYSDEP, 30, "klips_write: em", (u_int8_t *)em,
		em->em_msglen));
  n = write (klips_socket, em, em->em_msglen);
  if (n == -1)
    {
      log_error ("write (%d, ...) failed", klips_socket);
      return -1;
    }
  if ((size_t)n != em->em_msglen)
    {
      log_error ("write (%d, ...) returned prematurely", klips_socket);
      return -1;
    }
  return 0;
}

/*
 * Generate a SPI for protocol PROTO and the source/destination pair given by
 * SRC, SRCLEN, DST & DSTLEN.  Stash the SPI size in SZ.
 */
u_int8_t *
klips_get_spi (size_t *sz, u_int8_t proto, struct sockaddr *src,
	       struct sockaddr *dst, u_int32_t seq)
{
  u_int8_t *spi;
  u_int32_t spinum;

  *sz = IPSEC_SPI_SIZE;
  spi = malloc (*sz);
  if (!spi)
    return 0;
  do
    spinum = sysdep_random ();
  while (spinum < IPSEC_SPI_LOW);
  spinum = htonl (spinum);
  memcpy (spi, &spinum, *sz);

  LOG_DBG_BUF ((LOG_SYSDEP, 50, "klips_get_spi: spi", spi, *sz));

  return spi;
}

/* Group 2 SPIs in a chain.  XXX Not fully implemented yet.  */
int
klips_group_spis (struct sa *sa, struct proto *proto1, struct proto *proto2,
		  int incoming)
{
  struct encap_msghdr *emsg = 0;
  struct sockaddr *dst;

  emsg = calloc (1, EMT_GRPSPIS_FLEN + 2 * EMT_GRPSPIS_COMPLEN);
  if (!emsg)
    return -1;

  emsg->em_msglen = EMT_GRPSPIS_FLEN + 2 * EMT_GRPSPIS_COMPLEN;
  emsg->em_type = EMT_GRPSPIS;

  /*
   * XXX The code below is wrong if we are in tunnel mode.
   * The fix is to reorder stuff so the IP-in-IP SA will always come
   * upfront, and if there are two such, one is dropped.
   */
  memcpy (&emsg->em_rel[0].emr_spi, proto1->spi[incoming],
	  sizeof emsg->em_rel[0].emr_spi);
  memcpy (&emsg->em_rel[1].emr_spi, proto2->spi[incoming],
	  sizeof emsg->em_rel[1].emr_spi);
  if (incoming)
    sa->transport->vtbl->get_src (sa->transport, &dst);
  else
    sa->transport->vtbl->get_dst (sa->transport, &dst);
  emsg->em_rel[0].emr_dst
    = emsg->em_rel[1].emr_dst = ((struct sockaddr_in *)dst)->sin_addr;
  /* XXX What if IPCOMP etc. comes along?  */
  emsg->em_rel[0].emr_proto
    = proto1->proto == IPSEC_PROTO_IPSEC_ESP ? IPPROTO_ESP : IPPROTO_AH;
  emsg->em_rel[1].emr_proto
    = proto2->proto == IPSEC_PROTO_IPSEC_ESP ? IPPROTO_ESP : IPPROTO_AH;

  if (klips_write (emsg))
    goto cleanup;
  free (emsg);

  LOG_DBG ((LOG_SYSDEP, 50, "klips_group_spis: done"));

  return 0;

 cleanup:
  if (emsg)
    free (emsg);
  return -1;
}

/* Store/update a SPI with full information into the kernel.  */
int
klips_set_spi (struct sa *sa, struct proto *proto, int incoming,
	       struct sa *isakmp_sa)
{
  struct encap_msghdr *emsg = 0;
  struct ipsec_proto *iproto = proto->data;
  struct sockaddr *dst, *src;
  int keylen, hashlen;
  size_t len;
  struct ipe4_xdata *ip4x;

  /* Actually works for all.  */
  struct espblkrply_edata *edx;

  /* Actually works for all.  */
  struct ahhmacmd5_edata *amx;

  switch (proto->proto)
    {
    case IPSEC_PROTO_IPSEC_ESP:
      keylen = ipsec_esp_enckeylength (proto);
      hashlen = ipsec_esp_authkeylength (proto);
      len = EMT_SETSPI_FLEN + ESP_NEW_XENCAP_LEN;
      emsg = calloc (1, len);
      if (!emsg)
	return -1;

      emsg->em_proto = IPPROTO_ESP;

      edx = (struct espblkrply_edata *)emsg->em_dat;

      /* Funny expression due to I just want one switch.  */
      switch (proto->id | (iproto->auth << 8))
	{
	case IPSEC_ESP_3DES:
	  emsg->em_alg = XF_ESP3DES;
	  break;

	case IPSEC_ESP_3DES | (IPSEC_AUTH_HMAC_MD5 << 8):
	  emsg->em_alg = XF_ESP3DESMD596;
	  break;

	case IPSEC_ESP_3DES | (IPSEC_AUTH_HMAC_SHA << 8):
	  emsg->em_alg = XF_ESP3DESSHA196;
	  break;

	default:
	  LOG_DBG ((LOG_SYSDEP, 10,
		    "klips_set_spi: Unsupported enc/auth alg negotiated"));
	  return -1;
	}

      /* XXX What if we have a protocol requiring IV?  */
      edx->eme_ivlen = EMT_ESPDES_IV_SZ;
      edx->eme_klen = keylen;
      edx->ame_klen = hashlen;
#if 0
      /* I have reason to believe Shared-SADB won't work at all in KLIPS.  */
      edx->eme_ooowin
	= conf_get_str ("General", "Shared-SADB") ? 0 : iproto->replay_window;
#else
      edx->eme_ooowin = iproto->replay_window;
#endif
      /*
       * XXX Pluto sets the unused by KLIPS flag EME_INITIATOR in
       * edx->eme_flags, if the party is the initiator.  Should we too?
       */
      edx->eme_flags = 0;
      memcpy (edx->eme_key, iproto->keymat[incoming], keylen);
      if (iproto->auth)
	memcpy (edx->ame_key, iproto->keymat[incoming] + keylen, hashlen);
      break;

    case IPSEC_PROTO_IPSEC_AH:
      hashlen = ipsec_ah_keylength (proto);
      len = EMT_SETSPI_FLEN + AH_NEW_XENCAP_LEN + hashlen;
      emsg = calloc (1, len);
      if (!emsg)
	return -1;

      emsg->em_proto = IPPROTO_AH;

      amx = (struct ahhmacmd5_edata *)emsg->em_dat;

      switch (proto->id)
	{
	case IPSEC_AH_MD5:
	  emsg->em_alg = XF_AHHMACMD5;
	  break;

	case IPSEC_AH_SHA:
	  emsg->em_alg = XF_AHHMACSHA1;
	  break;

	default:
	  /* XXX Log?  */
	  goto cleanup;
	}

      /* XXX Should we be able to send in different lengths here?  */
      amx->ame_alen = amx->ame_klen = hashlen;
#if 0
      /* I have reason to believe Shared-SADB won't work at all in KLIPS.  */
      amx->ame_ooowin
	= conf_get_str ("General", "Shared-SADB") ? 0 : iproto->replay_window;
#else
      amx->ame_ooowin = iproto->replay_window;
#endif
      amx->ame_replayp = amx->ame_ooowin > 0;
      memcpy (amx->ame_key, iproto->keymat[incoming], hashlen);
      break;

    default:
      /* XXX Log?  */
      goto cleanup;
    }

  emsg->em_msglen = len;
  emsg->em_type = EMT_SETSPI;
  memcpy (&emsg->em_spi, proto->spi[incoming], sizeof emsg->em_spi);
  emsg->em_flags = incoming ? EMT_INBOUND : 0;

  /*
   * XXX Addresses has to be thought through.  Assumes IPv4.
   */
  sa->transport->vtbl->get_dst (sa->transport, &dst);
  sa->transport->vtbl->get_src (sa->transport, &src);
  emsg->em_dst
    = ((struct sockaddr_in *)(incoming ? src : dst))->sin_addr;

  /*
   * Klips does not know about expirations, thus we need to do them inside
   * isakmpd.
   */
  if (sa->seconds)
    if (sa_setup_expirations (sa))
      goto cleanup;

  LOG_DBG ((LOG_SYSDEP, 10, "klips_set_spi: proto %d dst %s SPI 0x%x",
	    emsg->em_proto, inet_ntoa (emsg->em_dst), htonl (emsg->em_spi)));
  if (klips_write (emsg))
    goto cleanup;
  free (emsg);

  /* If we are tunneling we have to setup an IP in IP tunnel too.  */
  if (iproto->encap_mode == IPSEC_ENCAP_TUNNEL)
    {
      len = EMT_SETSPI_FLEN + EMT_IPE4_ULEN;
      emsg = calloc (1, len);
      if (!emsg)
	goto cleanup;

      emsg->em_proto = IPPROTO_IPIP;
      emsg->em_msglen = len;
      emsg->em_type = EMT_SETSPI;
      /*
       * XXX Code in Pluto suggests this is not possible, but that we have
       * to have a unique SPI for the IP4 SA.
       */
      memcpy (&emsg->em_spi, proto->spi[incoming], sizeof emsg->em_spi);
      emsg->em_flags = 0;
      emsg->em_alg = XF_IP4;

      ip4x = (struct ipe4_xdata *)emsg->em_dat;
      ip4x->i4_dst = emsg->em_dst
	= ((struct sockaddr_in *)(incoming ? src : dst))->sin_addr;
      ip4x->i4_src
	= ((struct sockaddr_in *)(incoming ? dst : src))->sin_addr;

      LOG_DBG ((LOG_SYSDEP, 10, "klips_set_spi: proto %d dst %s SPI 0x%x",
		emsg->em_proto, inet_ntoa (emsg->em_dst),
		htonl (emsg->em_spi)));
      if (klips_write (emsg))
	goto cleanup;
      free (emsg);

      /*
       * Grouping the IP-in-IP SA with the IPsec one means we must be careful
       * in klips_group_spis so that we'll remove duplicate IP-in-IP SAs
       * and get everything grouped in the right order.
       *
       * XXX Could we not share code with klips_group_spis here?
       */
      emsg = calloc (1, EMT_GRPSPIS_FLEN + 2 * EMT_GRPSPIS_COMPLEN);
      if (!emsg)
	goto cleanup;

      emsg->em_msglen = EMT_GRPSPIS_FLEN + 2 * EMT_GRPSPIS_COMPLEN;
      emsg->em_type = EMT_GRPSPIS;

      memcpy (&emsg->em_rel[0].emr_spi, proto->spi[incoming],
	      sizeof emsg->em_rel[0].emr_spi);
      memcpy (&emsg->em_rel[1].emr_spi, proto->spi[incoming],
	      sizeof emsg->em_rel[1].emr_spi);
      emsg->em_rel[0].emr_dst = emsg->em_rel[1].emr_dst
	= ((struct sockaddr_in *)(incoming ? src : dst))->sin_addr;

      emsg->em_rel[0].emr_proto = IPPROTO_IPIP;
      /* XXX What if IPCOMP etc. comes along?  */
      emsg->em_rel[1].emr_proto
	= proto->proto == IPSEC_PROTO_IPSEC_ESP ? IPPROTO_ESP : IPPROTO_AH;

      if (klips_write (emsg))
	goto cleanup;
      free (emsg);
    }

  LOG_DBG ((LOG_SYSDEP, 50, "klips_set_spi: done"));

  return 0;

 cleanup:
  /* XXX Cleanup the potential SAs we have setup.  */
  if (emsg)
    free (emsg);
  return -1;
}

/*
 * Delete the IPsec SA represented by the INCOMING direction in protocol PROTO
 * of the IKE security association SA.
 */
int
klips_delete_spi (struct sa *sa, struct proto *proto, int incoming)
{
  struct encap_msghdr *emsg = 0;
  struct sockaddr *dst;
  struct ipsec_proto *iproto = proto->data;

  emsg = calloc (1, EMT_SETSPI_FLEN);
  if (!emsg)
    return -1;

  emsg->em_msglen = EMT_SETSPI_FLEN;
  emsg->em_type = EMT_DELSPI;

  memcpy (&emsg->em_spi, proto->spi[incoming], sizeof emsg->em_spi);
  if (incoming)
    sa->transport->vtbl->get_src (sa->transport, &dst);
  else
    sa->transport->vtbl->get_dst (sa->transport, &dst);
  emsg->em_dst = ((struct sockaddr_in *)dst)->sin_addr;
  /* XXX What if IPCOMP etc. comes along?  */
  emsg->em_proto
    = (iproto->encap_mode == IPSEC_ENCAP_TUNNEL ? IPPROTO_IPIP
       : proto->proto == IPSEC_PROTO_IPSEC_ESP ? IPPROTO_ESP : IPPROTO_AH);

  if (klips_write (emsg))
    goto cleanup;
  free (emsg);

  LOG_DBG ((LOG_SYSDEP, 50, "klips_delete_spi: done"));

  return 0;

 cleanup:
  if (emsg)
    free (emsg);
  return -1;
}

int
klips_hex_decode (char *src, u_char *dst, int dstsize)
{
  char *p, *pe;
  u_char *q, *qe, ch, cl;

  pe = src + strlen (src);
  qe = dst + dstsize;

  for (p = src, q = dst; p < pe && q < qe && isxdigit ((int)*p); p += 2)
    {
      ch = tolower (p[0]);
      cl = tolower (p[1]);

      if ((ch >= '0') && (ch <= '9'))
	ch -= '0';
      else if ((ch >= 'a') && (ch <= 'f'))
	ch -= 'a' - 10;
      else
	return -1;

      if ((cl >= '0') && (cl <= '9'))
	cl -= '0';
      else if ((cl >= 'a') && (cl <= 'f'))
	cl -= 'a' - 10;
      else
	return -1;

      *q++ = (ch << 4) | cl;
    }

  return (int)(q - dst);
}

/* Consult kernel routing table for next-hop lookup. From dugsong@monkey.org */
u_long
klips_route_get (u_long dst)
{
  FILE *f;
  char buf[BUFSIZ];
  char ifbuf[16], netbuf[128], gatebuf[128], maskbuf[128];
  int i, iflags, refcnt, use, metric, mss, win, irtt;
  u_long ret, gate, net, mask;

  if ((f = fopen (PROC_ROUTE_FILE, "r")) == NULL)
    return dst;

  ret = dst;

  while (fgets (buf, sizeof buf, f) != NULL)
    {
      i = sscanf (buf, PROC_ROUTE_FMT, ifbuf, netbuf, gatebuf, &iflags,
		  &refcnt, &use, &metric, maskbuf, &mss, &win, &irtt);
      if (i < 10 || !(iflags & RTF_UP))
	continue;

      klips_hex_decode (netbuf, (u_char *)&net, sizeof net);
      klips_hex_decode (gatebuf, (u_char *)&gate, sizeof gate);
      klips_hex_decode (maskbuf, (u_char *)&mask, sizeof mask);

      net = htonl (net);
      gate = htonl (gate);
      mask = htonl (mask);

      if ((dst & mask) == net)
	{
	  if (gate != INADDR_ANY)
	    ret = gate;
	  break;
	}
    }

  fclose (f);
  return ret;
}

/* Enable a flow given a SA.  */
int
klips_enable_sa (struct sa *sa, struct sa *isakmp_sa)
{
  struct ipsec_sa *isa = sa->data;
  struct sockaddr *dst;
  struct proto *proto = TAILQ_FIRST (&sa->protos);
  struct ipsec_proto *iproto = proto->data;
  struct encap_msghdr emsg;
  int s = -1;
  struct rtentry rt;

  sa->transport->vtbl->get_dst (sa->transport, &dst);

  /* XXX Is this needed?  */
  memset (&emsg, '\0', sizeof emsg);

  emsg.em_msglen = sizeof emsg;
  emsg.em_type = EMT_RPLACEROUTE;

  memcpy (&emsg.em_erspi, proto->spi[0], sizeof emsg.em_erspi);
  emsg.em_erdst = ((struct sockaddr_in *)dst)->sin_addr;

  LOG_DBG ((LOG_SYSDEP, 50, "klips_enable_sa: src %x %x dst %x %x",
	    ntohl (isa->src_net), ntohl (isa->src_mask), ntohl (isa->dst_net),
	    ntohl (isa->dst_mask)));

  /* XXX Magic constant from Pluto (26 = AF_ISDN in BSD).  */
  emsg.em_eaddr.sen_family = emsg.em_emask.sen_family = 26;
  emsg.em_eaddr.sen_type = SENT_IP4;
  /* XXX Magic constant from Pluto.  */
  emsg.em_emask.sen_type = 255;
  emsg.em_eaddr.sen_len = emsg.em_emask.sen_len
    = sizeof (struct sockaddr_encap);

  emsg.em_eaddr.sen_ip_src.s_addr = isa->src_net;
  emsg.em_emask.sen_ip_src.s_addr = isa->src_mask;
  emsg.em_eaddr.sen_ip_dst.s_addr = isa->dst_net;
  emsg.em_emask.sen_ip_dst.s_addr = isa->dst_mask;

  /* XXX What if IPCOMP etc. comes along?  */
  emsg.em_erproto
    = (iproto->encap_mode == IPSEC_ENCAP_TUNNEL ? IPPROTO_IPIP
       : proto->proto == IPSEC_PROTO_IPSEC_ESP ? IPPROTO_ESP : IPPROTO_AH);

  if (klips_write (&emsg))
    {
      emsg.em_type = EMT_SETEROUTE;
      if (klips_write (&emsg))
	goto cleanup;
    }

  s = socket (PF_INET, SOCK_DGRAM, AF_UNSPEC);
  if (s == -1)
    {
      log_error ("klips_enable_sa: "
		 "socket(PF_INET, SOCK_DGRAM, AF_UNSPEC) failed");
      goto cleanup;
    }

  memset (&rt, '\0', sizeof rt);
  rt.rt_dst.sa_family = AF_INET;
  ((struct sockaddr_in *)&rt.rt_dst)->sin_addr.s_addr = isa->dst_net;
  rt.rt_genmask.sa_family = AF_INET;
  ((struct sockaddr_in *)&rt.rt_genmask)->sin_addr.s_addr = isa->dst_mask;
  rt.rt_gateway.sa_family = AF_INET;

  ((struct sockaddr_in *)&rt.rt_gateway)->sin_addr.s_addr
    = klips_route_get (emsg.em_erdst.s_addr);

  rt.rt_flags = RTF_UP | RTF_GATEWAY;
  /* XXX What if we have multiple interfaces?  */
  rt.rt_dev = "ipsec0";

  if (ioctl (s, SIOCDELRT, &rt) == -1 && errno != ESRCH)
    {
      log_error ("klips_enable_sa: ioctl (%d, SIOCDELRT, %p) failed", s, &rt);
      goto cleanup;
    }

  if (ioctl (s, SIOCADDRT, &rt) == -1)
    {
      log_error ("klips_enable_sa: ioctl (%d, SIOCADDRT, %p) failed", s, &rt);
      goto cleanup;
    }

  close (s);
  return 0;

 cleanup:
  if (s != -1)
    close (s);
  return -1;
}

static void
klips_stayalive (struct exchange *exchange, void *vconn, int fail)
{
  char *conn = vconn;
  struct sa *sa;

  /* XXX What if it is phase 1?  */
  sa = sa_lookup_by_name (conn, 2);
  if (sa)
    sa->flags |= SA_FLAG_STAYALIVE;
}

/* Establish the connection in VCONN and set the stayalive flag for it.  */
void
klips_connection_check (char *conn)
{
  if (!sa_lookup_by_name (conn, 2))
    {
      LOG_DBG ((LOG_SYSDEP, 70, "klips_connection_check: SA for %s missing",
		conn));
      exchange_establish (conn, klips_stayalive, conn);
    }
  else
    LOG_DBG ((LOG_SYSDEP, 70, "klips_connection_check: SA for %s exists",
	      conn));
}
