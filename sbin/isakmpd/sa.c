/*	$OpenBSD: sa.c,v 1.65 2002/11/21 12:09:20 ho Exp $	*/
/*	$EOM: sa.c,v 1.112 2000/12/12 00:22:52 niklas Exp $	*/

/*
 * Copyright (c) 1998, 1999, 2000, 2001 Niklas Hallqvist.  All rights reserved.
 * Copyright (c) 1999, 2001 Angelos D. Keromytis.  All rights reserved.
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

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

#if defined (USE_KEYNOTE) || defined (USE_POLICY)
#include <regex.h>
#include <keynote.h>
#endif /* USE_KEYNOTE || USE_POLICY */

#include "sysdep.h"

#include "connection.h"
#include "cookie.h"
#include "doi.h"
#include "exchange.h"
#include "isakmp.h"
#include "log.h"
#include "message.h"
#include "sa.h"
#include "timer.h"
#include "transport.h"
#include "util.h"
#include "cert.h"
#include "policy.h"
#include "key.h"
#include "ipsec.h"
#include "ipsec_num.h"

/* Initial number of bits from the cookies used as hash.  */
#define INITIAL_BUCKET_BITS 6

/*
 * Don't try to use more bits than this as a hash.
 * We only XOR 16 bits so going above that means changing the code below
 * too.
 */
#define MAX_BUCKET_BITS 16

#if 0
static void sa_resize (void);
#endif
static void sa_soft_expire (void *);
static void sa_hard_expire (void *);

static LIST_HEAD (sa_list, sa) *sa_tab;

/* Works both as a maximum index and a mask.  */
static int bucket_mask;

void
sa_init (void)
{
  int i;

  bucket_mask = (1 << INITIAL_BUCKET_BITS) - 1;
  sa_tab = malloc ((bucket_mask + 1) * sizeof (struct sa_list));
  if (!sa_tab)
    log_fatal ("sa_init: malloc (%lu) failed",
	       (bucket_mask + 1) * (unsigned long)sizeof (struct sa_list));
  for (i = 0; i <= bucket_mask; i++)
    {
      LIST_INIT (&sa_tab[i]);
    }
}

#if 0
/* XXX We don't yet resize.  */
static void
sa_resize (void)
{
  int new_mask = (bucket_mask + 1) * 2 - 1;
  int i;
  struct sa_list *new_tab;

  new_tab = realloc (sa_tab, (new_mask + 1) * sizeof (struct sa_list));
  if (!new_tab)
    return;
  sa_tab = new_tab;
  for (i = bucket_mask + 1; i <= new_mask; i++)
    {
      LIST_INIT (&sa_tab[i]);
    }
  bucket_mask = new_mask;

  /* XXX Rehash existing entries.  */
}
#endif

/* Lookup an SA with the help from a user-supplied checking function.  */
struct sa *
sa_find (int (*check) (struct sa *, void *), void *arg)
{
  int i;
  struct sa *sa;

  for (i = 0; i <= bucket_mask; i++)
    for (sa = LIST_FIRST (&sa_tab[i]); sa; sa = LIST_NEXT (sa, link))
      if (check (sa, arg))
      {
	LOG_DBG ((LOG_SA, 90, "sa_find: return SA %p", sa));
	return sa;
      }
  LOG_DBG ((LOG_SA, 90, "sa_find: no SA matched query"));
  return 0;
}

/* Check if SA is an ISAKMP SA with an initiar cookie equal to ICOOKIE.  */
static int
sa_check_icookie (struct sa *sa, void *icookie)
{
  return sa->phase == 1
    && memcmp (sa->cookies, icookie, ISAKMP_HDR_ICOOKIE_LEN) == 0;
}

/* Lookup an ISAKMP SA out of just the initiator cookie.  */
struct sa *
sa_lookup_from_icookie (u_int8_t *cookie)
{
  return sa_find (sa_check_icookie, cookie);
}

struct name_phase_arg {
  char *name;
  u_int8_t phase;
};

/* Check if SA has the name and phase given by V_ARG.  */
static int
sa_check_name_phase (struct sa *sa, void *v_arg)
{
  struct name_phase_arg *arg = v_arg;

  return sa->name && strcasecmp (sa->name, arg->name) == 0 &&
    sa->phase == arg->phase && !(sa->flags & SA_FLAG_REPLACED);
}

/* Lookup an SA by name, case-independent, and phase.  */
struct sa *
sa_lookup_by_name (char *name, int phase)
{
  struct name_phase_arg arg;

  arg.name = name;
  arg.phase = phase;

  return sa_find (sa_check_name_phase, &arg);
}

struct addr_arg
{
  struct sockaddr *addr;
  socklen_t len;
  int phase;
  int flags;
};

/*
 * Check if SA is ready and has a peer with an address equal the one given
 * by V_ADDR.  Furthermore if we are searching for a specific phase, check
 * that too.
 */
static int
sa_check_peer (struct sa *sa, void *v_addr)
{
  struct addr_arg *addr = v_addr;
  struct sockaddr *dst;

  if (!sa->transport || (sa->flags & SA_FLAG_READY) == 0
      || (addr->phase && addr->phase != sa->phase))
    return 0;

  sa->transport->vtbl->get_dst (sa->transport, &dst);
  return sysdep_sa_len (dst) == addr->len
    && memcmp (dst, addr->addr, sysdep_sa_len (dst)) == 0;
}

struct dst_isakmpspi_arg {
  struct sockaddr *dst;
  u_int8_t *spi;			/* must be ISAKMP_SPI_SIZE octets */
};

/*
 * Check if SA matches what we are asking for through V_ARG.  It has to
 * be a finished phaes 1 (ISAKMP) SA.
 */
static int
isakmp_sa_check (struct sa *sa, void *v_arg)
{
  struct dst_isakmpspi_arg *arg = v_arg;
  struct sockaddr *dst, *src;

  if (sa->phase != 1 || !(sa->flags & SA_FLAG_READY))
    return 0;

  /* verify address is either src or dst for this sa */
  sa->transport->vtbl->get_dst (sa->transport, &dst);
  sa->transport->vtbl->get_src (sa->transport, &src);
  if (memcmp (src, arg->dst, sysdep_sa_len (src)) &&
      memcmp (dst, arg->dst, sysdep_sa_len (dst)))
    return 0;

  /* match icookie+rcookie against spi */
  if (memcmp (sa->cookies, arg->spi, ISAKMP_HDR_COOKIES_LEN) == 0)
    return 1;

  return 0;
}

/*
 * Find an ISAKMP SA with a "name" of DST & SPI.
 */
struct sa *
sa_lookup_isakmp_sa (struct sockaddr *dst, u_int8_t *spi)
{
  struct dst_isakmpspi_arg arg;

  arg.dst = dst;
  arg.spi = spi;

  return sa_find (isakmp_sa_check, &arg);
}

/* Lookup a ready SA by the peer's address.  */
struct sa *
sa_lookup_by_peer (struct sockaddr *dst, socklen_t dstlen)
{
  struct addr_arg arg;

  arg.addr = dst;
  arg.len = dstlen;
  arg.phase = 0;

  return sa_find (sa_check_peer, &arg);
}

/* Lookup a ready ISAKMP SA given its peer address.  */
struct sa *
sa_isakmp_lookup_by_peer (struct sockaddr *dst, socklen_t dstlen)
{
  struct addr_arg arg;

  arg.addr = dst;
  arg.len = dstlen;
  arg.phase = 1;

  return sa_find (sa_check_peer, &arg);
}

int
sa_enter (struct sa *sa)
{
  u_int16_t bucket = 0;
  int i;
  u_int8_t *cp;

  /* XXX We might resize if we are crossing a certain threshold */

  for (i = 0; i < ISAKMP_HDR_COOKIES_LEN; i += 2)
    {
      cp = sa->cookies + i;
      /* Doing it this way avoids alignment problems.  */
      bucket ^= cp[0] | cp[1] << 8;
    }
  for (i = 0; i < ISAKMP_HDR_MESSAGE_ID_LEN; i += 2)
    {
      cp = sa->message_id + i;
      /* Doing it this way avoids alignment problems.  */
      bucket ^= cp[0] | cp[1] << 8;
    }
  bucket &= bucket_mask;
  LIST_INSERT_HEAD (&sa_tab[bucket], sa, link);
  sa_reference (sa);
  LOG_DBG ((LOG_SA, 70, "sa_enter: SA %p added to SA list", sa));
  return 1;
}

/*
 * Lookup the SA given by the header fields MSG.  PHASE2 is false when
 * looking for phase 1 SAa and true otherwise.
 */
struct sa *
sa_lookup_by_header (u_int8_t *msg, int phase2)
{
  return sa_lookup (msg + ISAKMP_HDR_COOKIES_OFF,
		    phase2 ? msg + ISAKMP_HDR_MESSAGE_ID_OFF : 0);
}

/*
 * Lookup the SA given by the COOKIES and possibly the MESSAGE_ID unless
 * a null pointer, meaning we are looking for phase 1 SAs.
 */
struct sa *
sa_lookup (u_int8_t *cookies, u_int8_t *message_id)
{
  u_int16_t bucket = 0;
  int i;
  struct sa *sa;
  u_int8_t *cp;

  /*
   * We use the cookies to get bits to use as an index into sa_tab, as at
   * least one (our cookie) is a good hash, xoring all the bits, 16 at a
   * time, and then masking, should do.  Doing it this way means we can
   * validate cookies very fast thus delimiting the effects of "Denial of
   * service"-attacks using packet flooding.
   */
  for (i = 0; i < ISAKMP_HDR_COOKIES_LEN; i += 2)
    {
      cp = cookies + i;
      /* Doing it this way avoids alignment problems.  */
      bucket ^= cp[0] | cp[1] << 8;
    }
  if (message_id)
    for (i = 0; i < ISAKMP_HDR_MESSAGE_ID_LEN; i += 2)
      {
	cp = message_id + i;
	/* Doing it this way avoids alignment problems.  */
	bucket ^= cp[0] | cp[1] << 8;
      }
  bucket &= bucket_mask;
  for (sa = LIST_FIRST (&sa_tab[bucket]);
       sa && (memcmp (cookies, sa->cookies, ISAKMP_HDR_COOKIES_LEN) != 0
	      || (message_id && memcmp (message_id, sa->message_id,
					ISAKMP_HDR_MESSAGE_ID_LEN)
		  != 0)
	      || (!message_id && !zero_test (sa->message_id,
					     ISAKMP_HDR_MESSAGE_ID_LEN)));
       sa = LIST_NEXT (sa, link))
    ;

  return sa;
}

/* Create an SA.  */
int
sa_create (struct exchange *exchange, struct transport *t)
{
  struct sa *sa;

  /*
   * We want the SA zeroed for sa_free to be able to find out what fields
   * have been filled-in.
   */
  sa = calloc (1, sizeof *sa);
  if (!sa)
    {
      log_error ("sa_create: calloc (1, %lu) failed",
		 (unsigned long)sizeof *sa);
      return -1;
    }
  sa->transport = t;
  if (t)
    transport_reference (t);
  sa->phase = exchange->phase;
  memcpy (sa->cookies, exchange->cookies, ISAKMP_HDR_COOKIES_LEN);
  memcpy (sa->message_id, exchange->message_id, ISAKMP_HDR_MESSAGE_ID_LEN);
  sa->doi = exchange->doi;
  sa->policy_id = -1;

  if (sa->doi->sa_size)
    {
      /* Allocate the DOI-specific structure and initialize it to zeroes.  */
      sa->data = calloc (1, sa->doi->sa_size);
      if (!sa->data)
	{
	  log_error ("sa_create: calloc (1, %lu) failed",
		     (unsigned long)sa->doi->sa_size);
	  free (sa);
	  return -1;
	}
    }

  TAILQ_INIT (&sa->protos);

  sa_enter (sa);
  TAILQ_INSERT_TAIL (&exchange->sa_list, sa, next);
  sa_reference (sa);

  LOG_DBG ((LOG_SA, 60,
	    "sa_create: sa %p phase %d added to exchange %p (%s)", sa,
	    sa->phase, exchange,
	    exchange->name ? exchange->name : "<unnamed>"));
  return 0;
}

/*
 * Dump the internal state of SA to the report channel, with HEADER
 * prepended to each line.
 */
void
sa_dump (int cls, int level, char *header, struct sa *sa)
{
  struct proto *proto;
  char spi_header[80];
  int i;

  LOG_DBG ((cls, level, "%s: %p %s phase %d doi %d flags 0x%x", header, sa,
	    sa->name ? sa->name : "<unnamed>", sa->phase, sa->doi->id,
	    sa->flags));
  LOG_DBG ((cls, level, "%s: icookie %08x%08x rcookie %08x%08x", header,
	    decode_32 (sa->cookies), decode_32 (sa->cookies + 4),
	    decode_32 (sa->cookies + 8), decode_32 (sa->cookies + 12)));
  LOG_DBG ((cls, level, "%s: msgid %08x refcnt %d", header,
	    decode_32 (sa->message_id), sa->refcnt));
  LOG_DBG ((cls, level, "%s: life secs %llu kb %llu", header, sa->seconds,
	    sa->kilobytes));
  for (proto = TAILQ_FIRST (&sa->protos); proto;
       proto = TAILQ_NEXT (proto, link))
    {
      LOG_DBG ((cls, level, "%s: suite %d proto %d", header, proto->no,
		proto->proto));
      LOG_DBG ((cls, level,
		"%s: spi_sz[0] %d spi[0] %p spi_sz[1] %d spi[1] %p", header,
		proto->spi_sz[0], proto->spi[0], proto->spi_sz[1],
		proto->spi[1]));
      LOG_DBG ((cls, level, "%s: %s, %s", header,
		!sa->doi ? "<nodoi>"
		: sa->doi->decode_ids ("initiator id: %s, responder id: %s",
				     sa->id_i, sa->id_i_len,
				     sa->id_r, sa->id_r_len, 0),
		!sa->transport ? "<no transport>" :
		sa->transport->vtbl->decode_ids (sa->transport)));
      for (i = 0; i < 2; i++)
	if (proto->spi[i])
	  {
	    snprintf (spi_header, 80, "%s: spi[%d]", header, i);
	    LOG_DBG_BUF ((cls, level, spi_header, proto->spi[i],
			  proto->spi_sz[i]));
	  }
    }
}

/*
 * Display the SA's two SPI values.
 */
static void
report_spi (FILE *fd, const u_int8_t *buf, size_t sz, int spi)
{
#define SBUFSZ (2 * 32 + 9)
  char s[SBUFSZ];
  int i, j;

  for (i = j = 0; i < sz;)
    {
      snprintf (s + j, SBUFSZ - j, "%02x", buf[i++]);
      j += 2;
      if (i % 4 == 0)
	{
	  if (i % 32 == 0)
	    {
	      s[j] = '\0';
	      fprintf (fd, "%s", s);
	      j = 0;
	    }
	  else
	    s[j++] = ' ';
	}
    }

  if (j)
    {
      s[j] = '\0';
      fprintf (fd, "SPI %d: %s\n", spi, s);
    }
}


/*
 * Display the transform names to file SA_FILE.
 * Structure is taken from pf_key_v2.c, pf_key_v2_set_spi.
 * Transform names are taken from /usr/src/sys/crypto/xform.c.
 */
static void
report_proto (FILE *fd, struct proto *proto)
{
  int keylen, hashlen;
  struct ipsec_proto *iproto = proto->data;

  switch (proto->proto)
    {
    case IPSEC_PROTO_IPSEC_ESP:
      keylen = ipsec_esp_enckeylength (proto);
      hashlen = ipsec_esp_authkeylength (proto);
      fprintf (fd, "Transform: IPsec ESP\n");
      fprintf (fd, "Encryption key length: %d\n", keylen);
      fprintf (fd, "Authentication key length: %d\n", hashlen);

      fprintf (fd, "Encryption algorithm: ");
      switch (proto->id)
	{
	case IPSEC_ESP_DES:
	case IPSEC_ESP_DES_IV32:
	case IPSEC_ESP_DES_IV64:
	  fprintf (fd, "DES\n");
	  break;

	case IPSEC_ESP_3DES:
	  fprintf (fd, "3DES\n");
	  break;

	case IPSEC_ESP_AES:
	  fprintf (fd, "Rijndael-128/AES\n");
	  break;

	case IPSEC_ESP_CAST:
	  fprintf (fd, "Cast-128\n");
	  break;

	case IPSEC_ESP_BLOWFISH:
	  fprintf (fd, "Blowfish\n");
	  break;

	default:
	  fprintf (fd, "unknown (%d)\n", proto->id);
	}

      fprintf (fd, "Authentication algorithm: ");
      switch (iproto->auth)
	{
	case IPSEC_AUTH_HMAC_MD5:
	  fprintf (fd, "HMAC-MD5\n");
	  break;

	case IPSEC_AUTH_HMAC_SHA:
	  fprintf (fd, "HMAC-SHA1\n");
	  break;

        case IPSEC_AUTH_HMAC_RIPEMD:
	  fprintf (fd, "HMAC-RIPEMD-160\n");
	  break;

	case IPSEC_AUTH_DES_MAC:
	case IPSEC_AUTH_KPDK:
	  /* XXX We should be supporting KPDK */
	  fprintf (fd, "unknown (%d)", iproto->auth);
	  break;

	default:
	  fprintf (fd, "none\n");
	}
      break;

    case IPSEC_PROTO_IPSEC_AH:
      hashlen = ipsec_ah_keylength (proto);
      fprintf (fd, "Transform: IPsec AH\n");
      fprintf (fd, "Encryption not used.\n");
      fprintf (fd, "Authentication key length: %d\n", hashlen);

      fprintf (fd, "Authentication algorithm: ");
      switch (proto->id)
	{
	case IPSEC_AH_MD5:
	  fprintf (fd, "HMAC-MD5\n");
	  break;

	case IPSEC_AH_SHA:
	  fprintf (fd, "HMAC-SHA1\n");
	  break;

	case IPSEC_AH_RIPEMD:
	  fprintf (fd, "HMAC-RIPEMD-160\n");
	  break;

	default:
	  fprintf (fd, "unknown (%d)", proto->id);
	}
      break;

    default:
      fprintf (fd, "report_proto: invalid proto %d\n", proto->proto);
    }
}

/* Report all the SAs to the report channel.  */
void
sa_report (void)
{
  int i;
  struct sa *sa;

  for (i = 0; i <= bucket_mask; i++)
    for (sa = LIST_FIRST (&sa_tab[i]); sa; sa = LIST_NEXT (sa, link))
      sa_dump (LOG_REPORT, 0, "sa_report", sa);
}


/*
 * Print an SA's connection details to file SA_FILE.
 */
static void
sa_dump_all (FILE *fd, struct sa *sa)
{
  struct proto *proto;
  int i;

  /* SA name and phase. */
  fprintf (fd, "SA name: %s", sa->name ? sa->name : "<unnamed>");
  fprintf (fd, " (Phase %d)\n", sa->phase);

  /* Source and destination IPs. */
  fprintf (fd, sa->transport == NULL ? "<no transport>" :
	   sa->transport->vtbl->decode_ids (sa->transport));
  fprintf (fd, "\n");

  /* Transform information. */
  for (proto = TAILQ_FIRST (&sa->protos); proto;
       proto = TAILQ_NEXT (proto, link))
    {
      /* SPI values. */
      for (i = 0; i < 2; i++)
	if (proto->spi[i])
	  report_spi (fd, proto->spi[i], proto->spi_sz[i], i);
	else
	  fprintf (fd, "SPI %d not defined.", i);

      /* Proto values. */
      report_proto (fd, proto);

      /* SA separator. */
      fprintf (fd, "\n");
    }
}

/* Report info of all SAs to file SA_FILE.  */
void
sa_report_all (void)
{
  int i;
  FILE *fd;
  struct sa *sa;

  /* Open SA_FILE. */
  fd = fopen (SA_FILE, "w");

  /* Start sa_config_report. */
  for (i = 0; i <= bucket_mask; i++)
    for (sa = LIST_FIRST (&sa_tab[i]); sa; sa = LIST_NEXT (sa, link))
      if (sa->phase == 1)
	fprintf (fd, "SA name: none (phase 1)\n\n");
      else
	sa_dump_all (fd, sa);

  /* End sa_config_report. */
  fclose (fd);
}

/* Free the protocol structure pointed to by PROTO.  */
void
proto_free (struct proto *proto)
{
  int i;
  struct sa *sa = proto->sa;

  for (i = 0; i < 2; i++)
    if (proto->spi[i])
      {
	if (sa->doi->delete_spi)
	  sa->doi->delete_spi (sa, proto, i);
	free (proto->spi[i]);
      }
  TAILQ_REMOVE (&sa->protos, proto, link);
  if (proto->data)
    {
      if (sa->doi && sa->doi->free_proto_data)
	sa->doi->free_proto_data (proto->data);
      free (proto->data);
    }

  /* XXX Use class LOG_SA instead?  */
  LOG_DBG ((LOG_MISC, 90, "proto_free: freeing %p", proto));

  free (proto);
}

/* Release all resources this SA is using.  */
void
sa_free (struct sa *sa)
{
  if (sa->death)
    {
      timer_remove_event (sa->death);
      sa->death = 0;
      sa->refcnt--;
    }
  if (sa->soft_death)
    {
      timer_remove_event (sa->soft_death);
      sa->soft_death = 0;
      sa->refcnt--;
    }
  sa_remove (sa);
}

/* Remove the SA from the hash table of live SAs.  */
void
sa_remove (struct sa *sa)
{
  LIST_REMOVE (sa, link);
  LOG_DBG ((LOG_SA, 70, "sa_remove: SA %p removed from SA list", sa));
  sa_release (sa);
}

/* Raise the reference count of SA.  */
void
sa_reference (struct sa *sa)
{
  sa->refcnt++;
  LOG_DBG ((LOG_SA, 80, "sa_reference: SA %p now has %d references",
	    sa, sa->refcnt));
}

/* Release a reference to SA.  */
void
sa_release (struct sa *sa)
{
  struct proto *proto;
  struct cert_handler *handler;

  LOG_DBG ((LOG_SA, 80, "sa_release: SA %p had %d references",
	    sa, sa->refcnt));

  if (--sa->refcnt)
    return;

  LOG_DBG ((LOG_SA, 60, "sa_release: freeing SA %p", sa));

  while ((proto = TAILQ_FIRST (&sa->protos)) != 0)
    proto_free (proto);
  if (sa->data)
    {
      if (sa->doi && sa->doi->free_sa_data)
	sa->doi->free_sa_data (sa->data);
      free (sa->data);
    }
  if (sa->id_i)
    free (sa->id_i);
  if (sa->id_r)
    free (sa->id_r);
  if (sa->recv_cert)
    {
	handler = cert_get (sa->recv_certtype);
	if (handler)
	  handler->cert_free (sa->recv_cert);
    }
  if (sa->sent_cert)
    {
	handler = cert_get (sa->sent_certtype);
	if (handler)
	  handler->cert_free (sa->sent_cert);
    }
  if (sa->recv_key)
    key_free (sa->recv_keytype, ISAKMP_KEYTYPE_PUBLIC, sa->recv_key);
  if (sa->sent_key)
    key_free (sa->sent_keytype, ISAKMP_KEYTYPE_PRIVATE, sa->sent_key);
  if (sa->keynote_key)
    free (sa->keynote_key); /* This is just a string */
#if defined (USE_POLICY) || defined (USE_KEYNOTE)
  if (sa->policy_id != -1)
    kn_close (sa->policy_id);
#endif
  if (sa->name)
    free (sa->name);
  if (sa->keystate)
    free (sa->keystate);
  if (sa->transport)
    transport_release (sa->transport);
  free (sa);
}

/*
 * Rehash the ISAKMP SA this MSG is negotiating with the responder cookie
 * filled in.
 */
void
sa_isakmp_upgrade (struct message *msg)
{
  struct sa *sa = TAILQ_FIRST (&msg->exchange->sa_list);

  sa_remove (sa);
  GET_ISAKMP_HDR_RCOOKIE (msg->iov[0].iov_base,
			  sa->cookies + ISAKMP_HDR_ICOOKIE_LEN);

  /*
   * We don't install a transport in the initiator case as we don't know
   * what local address will be chosen.  Do it now instead.
   */
  sa->transport = msg->transport;
  transport_reference (sa->transport);
  sa_enter (sa);
}

/*
 * Register the chosen transform XF into SA.  As a side effect set PROTOP
 * to point at the corresponding proto structure.  INITIATOR is true if we
 * are the initiator.
 */
int
sa_add_transform (struct sa *sa, struct payload *xf, int initiator,
		  struct proto **protop)
{
  struct proto *proto;
  struct payload *prop = xf->context;

  *protop = 0;
  if (!initiator)
    {
      proto = calloc (1, sizeof *proto);
      if (!proto)
	log_error ("sa_add_transform: calloc (1, %lu) failed",
		   (unsigned long)sizeof *proto);
    }
  else
    /* Find the protection suite that were chosen.  */
    for (proto = TAILQ_FIRST (&sa->protos);
	 proto && proto->no != GET_ISAKMP_PROP_NO (prop->p);
	 proto = TAILQ_NEXT (proto, link))
      ;
  if (!proto)
    return -1;
  *protop = proto;

  /* Allocate DOI-specific part.  */
  if (!initiator)
    {
      proto->data = calloc (1, sa->doi->proto_size);
      if (!proto->data)
	{
	  log_error ("sa_add_transform: calloc (1, %lu) failed",
		     (unsigned long)sa->doi->proto_size);
	  goto cleanup;
	}
    }

  proto->no = GET_ISAKMP_PROP_NO (prop->p);
  proto->proto = GET_ISAKMP_PROP_PROTO (prop->p);
  proto->spi_sz[0] = GET_ISAKMP_PROP_SPI_SZ (prop->p);
  if (proto->spi_sz[0])
    {
      proto->spi[0] = malloc (proto->spi_sz[0]);
      if (!proto->spi[0])
	goto cleanup;
      memcpy (proto->spi[0], prop->p + ISAKMP_PROP_SPI_OFF, proto->spi_sz[0]);
    }
  proto->chosen = xf;
  proto->sa = sa;
  proto->id = GET_ISAKMP_TRANSFORM_ID (xf->p);
  if (!initiator)
    TAILQ_INSERT_TAIL (&sa->protos, proto, link);

  /* Let the DOI get at proto for initializing its own data.  */
  if (sa->doi->proto_init)
    sa->doi->proto_init (proto, 0);

  LOG_DBG ((LOG_SA, 80,
	    "sa_add_transform: "
	    "proto %p no %d proto %d chosen %p sa %p id %d",
	    proto, proto->no, proto->proto, proto->chosen, proto->sa,
	    proto->id));

  return 0;

 cleanup:
  if (!initiator)
    {
      if (proto->data)
	free (proto->data);
      free (proto);
    }
  *protop = 0;
  return -1;
}

/* Delete an SA.  Tell the peer if NOTIFY is set.  */
void
sa_delete (struct sa *sa, int notify)
{
  /* Don't bother notifying of Phase 1 SA deletes.  */
  if (sa->phase != 1 && notify)
    message_send_delete (sa);
  sa_free (sa);
}


/* Teardown all SAs.  */
void
sa_teardown_all (void)
{
  int i;
  struct sa *sa;

  LOG_DBG((LOG_MISC, 70, "sa_teardown_all.a"));
  /* Get Phase 2 SAs. */
  for (i = 0; i <= bucket_mask; i++)
    for (sa = LIST_FIRST (&sa_tab[i]); sa; sa = LIST_NEXT (sa, link))
      if (sa->phase == 2)
	{
	  /* Teardown the phase 2 SAs by name, similar to ui_teardown. */
	  LOG_DBG((LOG_MISC, 70, "sa_teardown_all: tearing down SA %s",
	      sa->name));
	  connection_teardown (sa->name);
	  sa_delete (sa, 1);
	}
}

/*
 * This function will get called when we are closing in on the death time of SA
 */
static void
sa_soft_expire (void *v_sa)
{
  struct sa *sa = v_sa;

  sa->soft_death = 0;
  sa_release (sa);

  if ((sa->flags & (SA_FLAG_STAYALIVE | SA_FLAG_REPLACED))
      == SA_FLAG_STAYALIVE)
    exchange_establish (sa->name, 0, 0);
  else
    /*
     * Start to watch the use of this SA, so a renegotiation can
     * happen as soon as it is shown to be alive.
     */
    sa->flags |= SA_FLAG_FADING;
}

/* SA has passed its best before date.  */
static void
sa_hard_expire (void *v_sa)
{
  struct sa *sa = v_sa;

  sa->death = 0;
  sa_release (sa);

  if ((sa->flags & (SA_FLAG_STAYALIVE | SA_FLAG_REPLACED))
      == SA_FLAG_STAYALIVE)
    exchange_establish (sa->name, 0, 0);

  sa_delete (sa, 1);
}

/*
 * Get an SA attribute's flag value out of textual description.
 */
int
sa_flag (char *attr)
{
  static struct sa_flag_map {
    char *name;
    int flag;
  } sa_flag_map[] = {
    { "active-only", SA_FLAG_ACTIVE_ONLY },
    /* Below this point are flags that are internal to the implementation.  */
    { "__ondemand", SA_FLAG_ONDEMAND },
    { "ikecfg", SA_FLAG_IKECFG },
  };
  int i;

  for (i = 0; i < sizeof sa_flag_map / sizeof sa_flag_map[0]; i++)
    if (strcasecmp (attr, sa_flag_map[i].name) == 0)
      return sa_flag_map[i].flag;
  log_print ("sa_flag: attribute \"%s\" unknown", attr);
  return 0;
}

/* Mark SA as replaced.  */
void
sa_mark_replaced (struct sa *sa)
{
  LOG_DBG ((LOG_SA, 60, "sa_mark_replaced: SA %p (%s) marked as replaced",
	    sa, sa->name ? sa->name : "unnamed"));
  sa->flags |= SA_FLAG_REPLACED;
}

/*
 * Setup expiration timers for SA.  This is used for ISAKMP SAs, but also
 * possible to use for application SAs if the application does not deal
 * with expirations itself.  An example is the Linux FreeS/WAN KLIPS IPsec
 * stack.
 */
int
sa_setup_expirations (struct sa *sa)
{
  u_int64_t seconds = sa->seconds;
  struct timeval expiration;

  /*
   * Set the soft timeout to a random percentage between 85 & 95 of
   * the negotiated lifetime to break strictly synchronized
   * renegotiations.  This works better when the randomization is on the
   * order of processing plus network-roundtrip times, or larger.
   * I.e. it depends on configuration and negotiated lifetimes.
   * It is not good to do the decrease on the hard timeout, because then
   * we may drop our SA before our peer.
   * XXX Better scheme to come?
   */
  if (!sa->soft_death)
    {
      gettimeofday (&expiration, 0);
      /* XXX This should probably be configuration controlled somehow.  */
      seconds = sa->seconds * (850 + sysdep_random () % 100) / 1000;
      LOG_DBG ((LOG_TIMER, 95,
		"sa_setup_expirations: SA %p soft timeout in %llu seconds",
		sa, seconds));
      expiration.tv_sec += seconds;
      sa->soft_death
	= timer_add_event ("sa_soft_expire", sa_soft_expire, sa, &expiration);
      if (!sa->soft_death)
	{
	  /* If we don't give up we might start leaking...  */
	  sa_delete (sa, 1);
	  return -1;
	}
      sa_reference (sa);
    }

  if (!sa->death)
    {
      gettimeofday (&expiration, 0);
      LOG_DBG ((LOG_TIMER, 95,
		"sa_setup_expirations: SA %p hard timeout in %llu seconds",
		sa, sa->seconds));
      expiration.tv_sec += sa->seconds;
      sa->death
	= timer_add_event ("sa_hard_expire", sa_hard_expire, sa, &expiration);
      if (!sa->death)
	{
	  /* If we don't give up we might start leaking...  */
	  sa_delete (sa, 1);
	  return -1;
	}
      sa_reference (sa);
    }
  return 0;
}
