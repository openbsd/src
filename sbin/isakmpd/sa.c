/*	$OpenBSD: sa.c,v 1.11 1999/03/31 20:31:05 niklas Exp $	*/
/*	$EOM: sa.c,v 1.70 1999/03/31 20:19:56 niklas Exp $	*/

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

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

#include "sysdep.h"

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

/* Initial number of bits from the cookies used as hash.  */
#define INITIAL_BUCKET_BITS 6

/*
 * Don't try to use more bits than this as a hash.
 * We only XOR 16 bits so going above that means changing the code below
 * too.
 */
#define MAX_BUCKET_BITS 16

static void sa_dump (char *, struct sa *);

static LIST_HEAD (sa_list, sa) *sa_tab;

/* Works both as a maximum index and a mask.  */
static int bucket_mask;

void
sa_init ()
{
  int i;

  bucket_mask = (1 << INITIAL_BUCKET_BITS) - 1;
  sa_tab = malloc ((bucket_mask + 1) * sizeof (struct sa_list));
  if (!sa_tab)
    log_fatal ("init_sa: out of memory");
  for (i = 0; i <= bucket_mask; i++)
    {
      LIST_INIT (&sa_tab[i]);
    }
 
}

/* XXX Ww don't yet resize.  */
void
sa_resize ()
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

/* Lookup an SA with the help from a user-supplied checking function.  */
struct sa *
sa_find (int (*check) (struct sa *, void *), void *arg)
{
  int i;
  struct sa *sa;

  for (i = 0; i < bucket_mask; i++)
    for (sa = LIST_FIRST (&sa_tab[i]); sa; sa = LIST_NEXT (sa, link))
      if (check (sa, arg))
	return sa;
  return 0;
}

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

static int
sa_check_name_phase (struct sa *sa, void *v_arg)
{
  struct name_phase_arg *arg = v_arg;

  return sa->name && strcasecmp (sa->name, arg->name) == 0 &&
    sa->phase == arg->phase;
}

/* Lookup an SA by name, case-independent, and phase.  */
struct sa *
sa_lookup_by_name (char *name, int phase)
{
  struct name_phase_arg arg = { name, phase };

  return sa_find (sa_check_name_phase, &arg);
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
 * NULL, meaning we are looking for phase 1 SAs.
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

/* Create a SA.  */
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
      log_error ("sa_create: calloc (1, %d) failed", sa->doi->sa_size);
      return -1;
    }
  sa->transport = t;
  sa->phase = exchange->phase;
  memcpy (sa->cookies, exchange->cookies, ISAKMP_HDR_COOKIES_LEN);
  memcpy (sa->message_id, exchange->message_id, ISAKMP_HDR_MESSAGE_ID_LEN);
  sa->doi = exchange->doi;

  /* Allocate the DOI-specific structure and initialize it to zeroes.  */
  sa->data = calloc (1, sa->doi->sa_size);
  if (!sa->data)
    {
      log_error ("sa_create: calloc (1, %d) failed", sa->doi->sa_size);
      free (sa);
      return -1;
    }

  TAILQ_INIT (&sa->protos);

  sa_enter (sa);
  TAILQ_INSERT_TAIL (&exchange->sa_list, sa, next);
  sa_reference (sa);

  log_debug (LOG_MISC, 90,
	     "sa_create: sa %p phase %d added to exchange %p (%s)", sa,
	     sa->phase, exchange,
	     exchange->name ? exchange->name : "<unnamed>");
  return 0;
}

static void
sa_dump (char *header, struct sa *sa)
{
  struct proto *proto;
  char spi_header[80];
  int i;

  log_debug (LOG_MISC, 10, "%s: %p %s phase %d doi %d flags 0x%x",
	     header, sa, sa->name ? sa->name : "<unnamed>", sa->phase,
	     sa->doi->id, sa->flags);
  log_debug (LOG_MISC, 10,
	     "%s: icookie %08x%08x rcookie %08x%08x", header,
	     decode_32 (sa->cookies), decode_32 (sa->cookies + 4),
	     decode_32 (sa->cookies + 8), decode_32 (sa->cookies + 12));
  log_debug (LOG_MISC, 10, "%s: msgid %08x", header,
	     decode_32 (sa->message_id));
  for (proto = TAILQ_FIRST (&sa->protos); proto;
       proto = TAILQ_NEXT (proto, link))
    {
      log_debug (LOG_MISC, 10,
		 "%s: suite %d proto %d", header, proto->no, proto->proto);
      log_debug (LOG_MISC, 10,
		 "%s: spi_sz[0] %d spi[0] %p spi_sz[1] %d spi[1] %p", header,
		 proto->spi_sz[0], proto->spi[0], proto->spi_sz[1],
		 proto->spi[1]);
      for (i = 0; i < 2; i++)
	if (proto->spi[i])
	  {
	    snprintf (spi_header, 80, "%s: spi[%d]", header, i);
	    log_debug_buf (LOG_MISC, 10, spi_header, proto->spi[i],
			   proto->spi_sz[i]);
	  }
    }
}

void
sa_report (void)
{
  int i;
  struct sa *sa;

  for (i = 0; i < bucket_mask; i++)
    for (sa = LIST_FIRST (&sa_tab[i]); sa; sa = LIST_NEXT (sa, link))
      sa_dump ("sa_report", sa);
}

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
  free (proto);
}

/* Release all resources this SA is using.  */
void
sa_free (struct sa *sa)
{
  if (sa->death)
    timer_remove_event (sa->death);
  if (sa->soft_death)
    timer_remove_event (sa->soft_death);
  sa_free_aux (sa);
}

/* Release all resources this SA is using except the death timers.  */
void
sa_free_aux (struct sa *sa)
{
  if (sa->last_sent_in_setup)
    message_free (sa->last_sent_in_setup);
  LIST_REMOVE (sa, link);
  sa_release (sa);
}

/* Raise the reference count of SA.  */
void
sa_reference (struct sa *sa)
{
  sa->refcnt++;
}

/* Release a reference to SA.  */
void
sa_release (struct sa *sa)
{
  struct proto *proto;

  if (--sa->refcnt)
    return;

  while ((proto = TAILQ_FIRST (&sa->protos)) != 0)
    proto_free (proto);
  if (sa->data)
    {
      if (sa->doi && sa->doi->free_sa_data)
	sa->doi->free_sa_data (sa->data);
      free (sa->data);
    }
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

  LIST_REMOVE (sa, link);
  GET_ISAKMP_HDR_RCOOKIE (msg->iov[0].iov_base,
			  sa->cookies + ISAKMP_HDR_ICOOKIE_LEN);
  /*
   *  We don't install a transport in the initiator case as we don't know
   * what local address will be chosen.  Do it now instead.
   */
  sa->transport = msg->transport;
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
    proto = calloc (1, sizeof *proto);
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
	goto cleanup;
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

  /* Let the DOI get at proto for initializing its own data. */
  if (sa->doi->proto_init)
    sa->doi->proto_init (proto, 0);

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

/* Lookup an ISAKMP SA given its peer address.  */
struct sa *
sa_isakmp_lookup_by_peer (struct sockaddr *addr, size_t addr_len)
{
  int i;
  struct sa *sa;
  struct sockaddr *taddr;
  int taddr_len;

  for (i = 0; i < bucket_mask; i++)
    for (sa = LIST_FIRST (&sa_tab[i]); sa; sa = LIST_NEXT (sa, link))
      /*
       * XXX We check the transport because it can be NULL until we fix
       * the initiator case to set the transport always.
       */
      if (sa->phase == 1 && (sa->flags & SA_FLAG_READY) && sa->transport)
	{
	  sa->transport->vtbl->get_dst (sa->transport, &taddr, &taddr_len);
	  if (taddr_len == addr_len && memcmp (taddr, addr, addr_len) == 0)
	    return sa;
	}
  return 0;
}

/* Delete an SA.  Tell the peer if NOTIFY is set.  */
void
sa_delete (struct sa *sa, int notify)
{
  /* XXX we do not send DELETE payloads just yet.  */

  sa_free (sa);
}

/*
 * This function will get called when we are closing in on the death time of SA
 */
void
sa_soft_expire (struct sa *sa)
{
  sa->soft_death = 0;

  if ((sa->flags & (SA_FLAG_STAYALIVE | SA_FLAG_REPLACED))
      == SA_FLAG_STAYALIVE)
    {
      /* If we are already renegotiating, don't start over.  */
      if (!exchange_lookup_by_name (sa->name, 1))
	{
	  sa_reference (sa);
	  exchange_establish (sa->name, (void (*) (void *))sa_mark_replaced,
			      sa);
	}
    }
  else
    {
      /*
       * Start to watch the use of this SA, so a renegotiation can
       * happen as soon as it is shown to be alive.
       */
      sa->flags |= SA_FLAG_FADING;
    }
}

/* SA has passed its best before date.  */
void
sa_hard_expire (struct sa *sa)
{
  sa->death = 0;

  if ((sa->flags & (SA_FLAG_STAYALIVE | SA_FLAG_REPLACED))
      == SA_FLAG_STAYALIVE)
    {
      /* If we are already renegotiating, don't start over.  */
      if (!exchange_lookup_by_name (sa->name, 1))
	{
	  sa_reference (sa);
	  exchange_establish (sa->name, (void (*) (void *))sa_mark_replaced,
			      sa);
	}
    }

  sa_delete (sa, 1);
}

/*
 * Get a SA attribute's flag value out of textual description.
 * XXX Kind of overkill for just one attribute, maybe simplify?
 */
int
sa_flag (char *attr)
{
  static struct sa_flag_map {
    char *name;
    int flag;
  } sa_flag_map[] = {
    { "stayalive", SA_FLAG_STAYALIVE }
  };
  int i;

  for (i = 0; i < sizeof sa_flag_map / sizeof sa_flag_map[0]; i++)
    if (strcasecmp (attr, sa_flag_map[i].name) == 0)
      return sa_flag_map[i].flag;
  log_print (LOG_MISC, 10, "sa_flag: attribute \"%s\" unknown", attr);
  return 0;
}

/*
 * Mark SA as replaced.  As SA has potentially disappeared before we get
 * called, check if it still exists before marking.
 */
void
sa_mark_replaced (struct sa *sa)
{
  log_debug (LOG_MISC, 90, "SA %p marked as replaced", sa);
  sa->flags |= SA_FLAG_REPLACED;
  sa_release (sa);
}
