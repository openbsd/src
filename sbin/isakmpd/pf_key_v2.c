/*	$OpenBSD: pf_key_v2.c,v 1.17 2000/01/13 06:42:26 angelos Exp $	*/
/*	$EOM: pf_key_v2.c,v 1.19 1999/07/16 00:29:11 niklas Exp $	*/

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
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <net/pfkeyv2.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sysdep.h"

#include "conf.h"
#include "exchange.h"
#include "ipsec.h"
#include "ipsec_num.h"
#include "log.h"
#include "pf_key_v2.h"
#include "sa.h"
#include "timer.h"
#include "transport.h"

/*
 * PF_KEY v2 always work with 64-bit entities and aligns on 64-bit boundaries.
 */
#define PF_KEY_V2_CHUNK 8
#define PF_KEY_V2_ROUND(x)						\
  (((x) + PF_KEY_V2_CHUNK - 1) & ~(PF_KEY_V2_CHUNK - 1))

/* How many microseconds we will wait for a reply from the PF_KEY socket.  */
#define PF_KEY_REPLY_TIMEOUT 1000

struct pf_key_v2_node {
  TAILQ_ENTRY (pf_key_v2_node) link;
  void *seg;
  size_t sz;
  int cnt;
  u_int16_t type;
  u_int8_t flags;
};

TAILQ_HEAD (pf_key_v2_msg, pf_key_v2_node);

#define PF_KEY_V2_NODE_MALLOCED 1
#define PF_KEY_V2_NODE_MARK 2

static struct pf_key_v2_msg *pf_key_v2_call (struct pf_key_v2_msg *);
static struct pf_key_v2_node *pf_key_v2_find_ext (struct pf_key_v2_msg *,
						  u_int16_t);
static void pf_key_v2_notify (struct pf_key_v2_msg *);
static struct pf_key_v2_msg *pf_key_v2_read (u_int32_t);
static u_int32_t pf_key_v2_seq (void);
static u_int32_t pf_key_v2_write (struct pf_key_v2_msg *);

/* The socket to use for PF_KEY interactions.  */
static int pf_key_v2_socket;

static struct pf_key_v2_msg *
pf_key_v2_msg_new (struct sadb_msg *msg, int flags)
{
  struct pf_key_v2_node *node = 0;
  struct pf_key_v2_msg *ret;

  node = malloc (sizeof *node);
  if (!node)
    goto cleanup;
  ret = malloc (sizeof *ret);
  if (!ret)
    goto cleanup;
  TAILQ_INIT (ret);
  node->seg = msg;
  node->sz = sizeof *msg;
  node->type = 0;
  node->cnt = 1;
  node->flags = flags;
  TAILQ_INSERT_HEAD (ret, node, link);
  return ret;

 cleanup:
  if (node)
    free (node);
  return 0;
}

/* Add a SZ sized segment SEG to the PF_KEY message MSG.  */
static int
pf_key_v2_msg_add (struct pf_key_v2_msg *msg, struct sadb_ext *ext, int flags)
{
  struct pf_key_v2_node *node;

  node = malloc (sizeof *node);
  if (!node)
    return -1;
  node->seg = ext;
  node->sz = ext->sadb_ext_len * PF_KEY_V2_CHUNK;
  node->type = ext->sadb_ext_type;
  node->flags = flags;
  TAILQ_FIRST (msg)->cnt++;
  TAILQ_INSERT_TAIL (msg, node, link);
  return 0;
}

/* Deallocate the PF_KEY message MSG.  */
static void
pf_key_v2_msg_free (struct pf_key_v2_msg *msg)
{
  struct pf_key_v2_node *np, *next;

  for (np = TAILQ_FIRST (msg); np; np = next)
    {
      next = TAILQ_NEXT (np, link);
      if (np->flags & PF_KEY_V2_NODE_MALLOCED)
	free (np->seg);
      free (np);
    }
  free (msg);
}

/* Just return a new sequence number.  */
static u_int32_t
pf_key_v2_seq ()
{
  static u_int32_t seq = 0;

  return ++seq;
}

/*
 * Read a PF_KEY packet with SEQ as the sequence number, looping if necessary.
 * If SEQ is zero just read the first message we see, otherwise we queue
 * messages up untile both the PID and the sequence number match.
 */
static struct pf_key_v2_msg *
pf_key_v2_read (u_int32_t seq)
{
  ssize_t n;
  u_int8_t *buf = 0;
  struct pf_key_v2_msg *ret = 0;
  struct sadb_msg *msg;
  struct sadb_msg hdr;
  struct sadb_ext *ext;
  struct timeval tv;
  fd_set *fds;

  while (1)
    {
      /*
       * If this is a read of a reply we should actually expect the reply to
       * get lost as PF_KEY is an unreliable service per the specs.
       * Currently we do this by setting a short timeout, and if it is not
       * readable in that time, we fail the read.
       */
      if (seq)
	{
	  fds = calloc (howmany (pf_key_v2_socket + 1, NFDBITS),
			sizeof (fd_mask));
	  if (!fds)
	    {
	      log_error ("pf_key_v2_read: calloc (%d, %d) failed",
			 howmany (pf_key_v2_socket + 1, NFDBITS),
			 sizeof (fd_mask));
	      goto cleanup;
	    }
	  FD_SET (pf_key_v2_socket, fds);
	  tv.tv_sec = 0;
	  tv.tv_usec = PF_KEY_REPLY_TIMEOUT;
	  n = select (pf_key_v2_socket + 1, fds, 0, 0, &tv);
	  free (fds);
	  if (n == -1)
	    {
	      log_error ("pf_key_v2_read: select (%d, fds, 0, 0, &tv) failed",
			 pf_key_v2_socket + 1);
	      goto cleanup;
	    }
	  if (!n)
	    {
	      log_print ("pf_key_v2_read: no reply from PF_KEY");
	      goto cleanup;
	    }
	}
      n = recv (pf_key_v2_socket, &hdr, sizeof hdr, MSG_PEEK);
      if (n == -1)
	{
	  log_error ("pf_key_v2_read: recv (%d, ...) failed",
		     pf_key_v2_socket);
	  goto cleanup;
	}
      if (n != sizeof hdr)
	{
	  log_error ("pf_key_v2_read: recv (%d, ...) returned short packet "
		     "(%d bytes)",
		     pf_key_v2_socket, n);
	  goto cleanup;
	}

      n = hdr.sadb_msg_len * PF_KEY_V2_CHUNK;
      buf = malloc (n);
      if (!buf)
	{
	  log_error ("pf_key_v2_read: malloc (%d) failed", n);
	  goto cleanup;
	}

      n = read (pf_key_v2_socket, buf, n);
      if (n == -1)
	{
	  log_error ("pf_key_v2_read: read (%d, ...) failed",
		     pf_key_v2_socket);
	  goto cleanup;
	}

      if ((size_t)n != hdr.sadb_msg_len * PF_KEY_V2_CHUNK)
	{
	  log_print ("pf_key_v2_read: read (%d, ...) returned short packet "
		     "(%d bytes)",
		     pf_key_v2_socket, n);
	  goto cleanup;
	}

      log_debug_buf (LOG_SYSDEP, 80, "pf_key_v2_read: msg", buf, n);

      /* We drop all messages that is not what we expect.  */
      msg = (struct sadb_msg *)buf;
      if (msg->sadb_msg_version != PF_KEY_V2
	  || (msg->sadb_msg_pid != 0 && msg->sadb_msg_pid != getpid ()))
	{
	  if (seq)
	    {
	      free (buf);
	      buf = 0;
	      continue;
	    }
	  else
	    {
	      log_debug (LOG_SYSDEP, 90,
			 "pf_key_v2_read:"
			 "bad version (%d) or PID (%d, mine is %d), ignored",
			 msg->sadb_msg_version, msg->sadb_msg_pid, getpid ());
	      goto cleanup;
	    }
	}

      /* Parse the message.  */
      ret = pf_key_v2_msg_new (msg, PF_KEY_V2_NODE_MALLOCED);
      if (!ret)
	goto cleanup;
      buf = 0;
      for (ext = (struct sadb_ext *)(msg + 1);
	   (u_int8_t *)ext - (u_int8_t *)msg
	     < msg->sadb_msg_len * PF_KEY_V2_CHUNK;
	   ext = (struct sadb_ext *)((u_int8_t *)ext
				     + ext->sadb_ext_len * PF_KEY_V2_CHUNK))
	pf_key_v2_msg_add (ret, ext, 0);

      /* If the message is not the one we are waiting for, queue it up.  */
      if (seq && (msg->sadb_msg_pid != getpid () || msg->sadb_msg_seq != seq))
	{
	  gettimeofday (&tv, 0);
	  timer_add_event ("pf_key_v2_notify",
			   (void (*) (void *))pf_key_v2_notify, ret, &tv);
	  ret = 0;
	  continue;
	}

      return ret;
    }

 cleanup:
  if (buf)
    free (buf);
  if (ret)
    pf_key_v2_msg_free (ret);
  return 0;
}

/* Write the message in PMSG to the PF_KEY socket.  */
u_int32_t
pf_key_v2_write (struct pf_key_v2_msg *pmsg)
{
  struct iovec *iov = 0;
  ssize_t n;
  size_t len;
  int i, cnt = TAILQ_FIRST (pmsg)->cnt;
  char header[80];
  struct sadb_msg *msg = TAILQ_FIRST (pmsg)->seg;
  struct pf_key_v2_node *np = TAILQ_FIRST (pmsg);

  iov = (struct iovec *)malloc (cnt * sizeof *iov);
  if (!iov)
    {
      log_error ("pf_key_v2_write: malloc (%d) failed", cnt * sizeof *iov);
      return 0;
    }

  msg->sadb_msg_version = PF_KEY_V2;
  msg->sadb_msg_errno = 0;
  msg->sadb_msg_reserved = 0;
  msg->sadb_msg_pid = getpid ();
  if (!msg->sadb_msg_seq)
    msg->sadb_msg_seq = pf_key_v2_seq ();

  /* Compute the iovec segments as well as the message length.  */
  len = 0;
  for (i = 0; i < cnt; i++)
    {
      iov[i].iov_base = np->seg;
      len += iov[i].iov_len = np->sz;

      /*
       * XXX One can envision setting specific extension fields, like
       * *_reserved ones here.  For now we require them to be set by the
       * caller.
       */

      np = TAILQ_NEXT (np, link);
    }
  msg->sadb_msg_len = len / PF_KEY_V2_CHUNK;

  for (i = 0; i < cnt; i++)
    {
      sprintf (header, "pf_key_v2_write: iov[%d]", i);
      log_debug_buf (LOG_SYSDEP, 80, header, (u_int8_t *)iov[i].iov_base,
		     iov[i].iov_len);
    }

  n = writev (pf_key_v2_socket, iov, cnt);
  if (n == -1)
    {
      log_error ("pf_key_v2_write: writev (%d, 0x%p, %d) failed",
		 pf_key_v2_socket, iov, cnt);
      goto cleanup;
    }
  if ((size_t)n != len)
    {
      log_error ("pf_key_v2_write: writev (%d, ...) returned prematurely (%d)",
		 pf_key_v2_socket, n);
      goto cleanup;
    }
  free (iov);
  return msg->sadb_msg_seq;

 cleanup:
  if (iov)
    free (iov);
  return 0;
}

/*
 * Do a PF_KEY "call", i.e. write a message MSG, read the reply and return
 * it to the caller.
 */
static struct pf_key_v2_msg *
pf_key_v2_call (struct pf_key_v2_msg *msg)
{
  u_int32_t seq;

  seq = pf_key_v2_write (msg);
  if (!seq)
    return 0;
  return pf_key_v2_read (seq);
}

/* Find the TYPE extension in MSG.  Return zero if none found.  */
static struct pf_key_v2_node *
pf_key_v2_find_ext (struct pf_key_v2_msg *msg, u_int16_t type)
{
  struct pf_key_v2_node *ext;

  for (ext = TAILQ_NEXT (TAILQ_FIRST (msg), link); ext;
       ext = TAILQ_NEXT (ext, link))
    if (ext->type == type)
      return ext;
  return 0;
}

/*
 * Open the PF_KEYv2 sockets and return the descriptor used for notifies.
 * Return -1 for failure and -2 if no notifies will show up.
 */
int
pf_key_v2_open ()
{
  int fd = -1, err;
  struct sadb_msg msg;
  struct pf_key_v2_msg *regmsg = 0, *ret = 0;

  /* Open the socket we use to speak to IPSec.  */
  pf_key_v2_socket = -1;
  fd = socket (PF_KEY, SOCK_RAW, PF_KEY_V2);
  if (fd == -1)
    {
      log_error ("pf_key_v2_open: "
		 "socket (PF_KEY, SOCK_RAW, PF_KEY_V2) failed");
      goto cleanup;
    }
  pf_key_v2_socket = fd;

  /* Register it to get ESP and AH acquires from the kernel.  */
  msg.sadb_msg_seq = 0;
  msg.sadb_msg_type = SADB_REGISTER;
  msg.sadb_msg_satype = SADB_SATYPE_ESP;
  regmsg = pf_key_v2_msg_new (&msg, 0);
  if (!regmsg)
    goto cleanup;
  ret = pf_key_v2_call (regmsg);
  pf_key_v2_msg_free (regmsg);
  if (!ret)
    goto cleanup;
  err = ((struct sadb_msg *)TAILQ_FIRST (ret)->seg)->sadb_msg_errno;
  if (err)
    {
      log_print ("pf_key_v2_open: REGISTER: %s", strerror (err));
      goto cleanup;
    }

  /* XXX Register the accepted transforms.  */

  pf_key_v2_msg_free (ret);
  ret = 0;

  msg.sadb_msg_seq = 0;
  msg.sadb_msg_type = SADB_REGISTER;
  msg.sadb_msg_satype = SADB_SATYPE_AH;
  regmsg = pf_key_v2_msg_new (&msg, 0);
  if (!regmsg)
    goto cleanup;
  ret = pf_key_v2_call (regmsg);
  pf_key_v2_msg_free (regmsg);
  if (!ret)
    goto cleanup;
  err = ((struct sadb_msg *)TAILQ_FIRST (ret)->seg)->sadb_msg_errno;
  if (err)
    {
      log_print ("pf_key_v2_open: REGISTER: %s", strerror (err));
      goto cleanup;
    }

  /* XXX Register the accepted transforms.  */

  pf_key_v2_msg_free (ret);
  return fd;

 cleanup:
  if (pf_key_v2_socket != -1)
    {
      close (pf_key_v2_socket);
      pf_key_v2_socket = -1;
    }
  if (ret)
    pf_key_v2_msg_free (ret);
  return -1;
}

/*
 * Generate a SPI for protocol PROTO and the source/destination pair given by
 * SRC, SRCLEN, DST & DSTLEN.  Stash the SPI size in SZ.
 */
u_int8_t *
pf_key_v2_get_spi (size_t *sz, u_int8_t proto, struct sockaddr *src,
		   int srclen, struct sockaddr *dst, int dstlen)
{
  struct sadb_msg msg;
  struct sadb_sa *sa;
  struct sadb_address *addr = 0;
  struct sadb_spirange spirange;
  struct pf_key_v2_msg *getspi = 0, *ret = 0;
  u_int8_t *spi = 0;
  int len, err;

  msg.sadb_msg_type = SADB_GETSPI;
  switch (proto)
    {
    case IPSEC_PROTO_IPSEC_ESP:
      msg.sadb_msg_satype = SADB_SATYPE_ESP;
      break;
    case IPSEC_PROTO_IPSEC_AH:
      msg.sadb_msg_satype = SADB_SATYPE_AH;
      break;
    default:
      log_print ("pf_key_v2_get_spi: invalid proto %d", proto);
      goto cleanup;
    }
  /*
   * XXX When we have acquires working, the sequence number have to be set
   * from the acquire message.
   */
  msg.sadb_msg_seq = 0;
  getspi = pf_key_v2_msg_new (&msg, 0);
  if (!getspi)
    goto cleanup;

  /* Setup the ADDRESS extensions.  */
  len = sizeof (struct sadb_address) + PF_KEY_V2_ROUND (srclen); 
  addr = malloc (len);
  if (!addr)
    goto cleanup;
  addr->sadb_address_exttype = SADB_EXT_ADDRESS_SRC;
  addr->sadb_address_len = len / PF_KEY_V2_CHUNK;
#if 0
  addr->sadb_address_proto = 0;
  addr->sadb_address_prefixlen = 0;
#endif
  addr->sadb_address_reserved = 0;
  memcpy (addr + 1, src, srclen);
  /* XXX IPv4-specific.  */
  ((struct sockaddr_in *)(addr + 1))->sin_port = 0;
  if (pf_key_v2_msg_add (getspi, (struct sadb_ext *)addr,
			 PF_KEY_V2_NODE_MALLOCED) == -1)
    goto cleanup;
  addr = 0;

  len = sizeof (struct sadb_address) + PF_KEY_V2_ROUND (dstlen); 
  addr = malloc (len);
  if (!addr)
    goto cleanup;
  addr->sadb_address_exttype = SADB_EXT_ADDRESS_DST;
  addr->sadb_address_len = len / PF_KEY_V2_CHUNK;
#if 0
  addr->sadb_address_proto = 0;
  addr->sadb_address_prefixlen = 0;
#endif
  addr->sadb_address_reserved = 0;
  memcpy (addr + 1, dst, dstlen);
  /* XXX IPv4-specific.  */
  ((struct sockaddr_in *)(addr + 1))->sin_port = 0;
  if (pf_key_v2_msg_add (getspi, (struct sadb_ext *)addr,
			 PF_KEY_V2_NODE_MALLOCED) == -1)
    goto cleanup;
  addr = 0;

  /* Setup the SPIRANGE extension.  */
  spirange.sadb_spirange_exttype = SADB_EXT_SPIRANGE;
  spirange.sadb_spirange_len = sizeof spirange / PF_KEY_V2_CHUNK;
  spirange.sadb_spirange_min = IPSEC_SPI_LOW;
  spirange.sadb_spirange_max = 0xffffffff;
  spirange.sadb_spirange_reserved = 0;
  if (pf_key_v2_msg_add (getspi, (struct sadb_ext *)&spirange, 0) == -1)
    goto cleanup;

  ret = pf_key_v2_call (getspi);
  pf_key_v2_msg_free (getspi);
  getspi = 0;
  if (!ret)
    goto cleanup;
  err = ((struct sadb_msg *)TAILQ_FIRST (ret)->seg)->sadb_msg_errno;
  if (err)
    {
      log_print ("pf_key_v2_get_spi: GETSPI: %s", strerror (err));
      goto cleanup;
    }

  sa = (struct sadb_sa *)pf_key_v2_find_ext (ret, SADB_EXT_SA)->seg;
  if (!sa)
    {
      log_print ("pf_key_v2_get_spi: no SA extension found");
      goto cleanup;
    }

  *sz = sizeof sa->sadb_sa_spi;
  spi = malloc (*sz);
  if (!spi)
    goto cleanup;
  memcpy (spi, &sa->sadb_sa_spi, *sz);
  pf_key_v2_msg_free (ret);

  log_debug_buf (LOG_SYSDEP, 50, "pf_key_v2_get_spi: spi", spi, *sz);

  return spi;

 cleanup:
  if (spi)
    free (spi);
  if (addr)
    free (addr);
  if (getspi)
    pf_key_v2_msg_free (getspi);
  if (ret)
    pf_key_v2_msg_free (ret);
  return 0;
}

/*
 * Store/update a PF_KEY_V2 security association with full information from the
 * IKE SA and PROTO into the kernel.  INCOMING is set if we are setting the
 * parameters for the incoming SA, and cleared otherwise.
 */
int
pf_key_v2_set_spi (struct sa *sa, struct proto *proto, int incoming)
{
  struct sadb_msg msg;
  struct sadb_sa ssa;
  struct sadb_lifetime *life = 0;
  struct sadb_address *addr = 0;
  struct sadb_key *key = 0;
  struct sockaddr *src, *dst;
  int dstlen, srclen, keylen, hashlen, err;
  struct pf_key_v2_msg *update = 0, *ret = 0;
  struct ipsec_proto *iproto = proto->data;
  size_t len;

  msg.sadb_msg_type = incoming ? SADB_UPDATE : SADB_ADD;
  switch (proto->proto)
    {
    case IPSEC_PROTO_IPSEC_ESP:
      msg.sadb_msg_satype = SADB_SATYPE_ESP;
      keylen = ipsec_esp_enckeylength (proto);
      hashlen = ipsec_esp_authkeylength (proto);

      switch (proto->id)
	{
	case IPSEC_ESP_DES:
	case IPSEC_ESP_DES_IV32:
	case IPSEC_ESP_DES_IV64:
	  ssa.sadb_sa_encrypt = SADB_EALG_DESCBC;
	  break;

	case IPSEC_ESP_3DES:
	  ssa.sadb_sa_encrypt = SADB_EALG_3DESCBC;
	  break;

#ifdef SADB_X_EALG_CAST
	case IPSEC_ESP_CAST:
	  ssa.sadb_sa_encrypt = SADB_X_EALG_CAST;
	  break;
#endif

#ifdef SADB_X_EALG_BLF
	case IPSEC_ESP_BLOWFISH:
	  ssa.sadb_sa_encrypt = SADB_X_EALG_BLF;
	  break;
#endif

	default:
	  /* XXX Log?  */
	  return -1;
	}

      switch (iproto->auth)
	{
	case IPSEC_AUTH_HMAC_MD5:
	  ssa.sadb_sa_auth = SADB_AALG_MD5HMAC96;
	  break;

	case IPSEC_AUTH_HMAC_SHA:
	  ssa.sadb_sa_auth = SADB_AALG_SHA1HMAC96;
	  break;

	case IPSEC_AUTH_DES_MAC:
	case IPSEC_AUTH_KPDK:
	  /* XXX Log?  */
	  return -1;

	default:
	  ssa.sadb_sa_auth = SADB_AALG_NONE;
	}
      break;

    case IPSEC_PROTO_IPSEC_AH:
      msg.sadb_msg_satype = SADB_SATYPE_AH;
      hashlen = ipsec_ah_keylength (proto);
      keylen = 0;

      ssa.sadb_sa_encrypt = SADB_EALG_NONE;
      switch (proto->id)
	{
	case IPSEC_AH_MD5:
	  ssa.sadb_sa_auth = SADB_AALG_MD5HMAC96;
	  break;

	case IPSEC_AH_SHA:
	  ssa.sadb_sa_auth = SADB_AALG_SHA1HMAC96;
	  break;

	default:
	  /* XXX Log?  */
	  goto cleanup;
	}
      break;

    default:
      log_print ("pf_key_v2_set_spi: invalid proto %d", proto->proto);
      goto cleanup;
    }
  msg.sadb_msg_seq = 0;
  update = pf_key_v2_msg_new (&msg, 0);
  if (!update)
    goto cleanup;

  /* Setup the rest of the SA extension.  */
  ssa.sadb_sa_exttype = SADB_EXT_SA;
  ssa.sadb_sa_len = sizeof ssa / PF_KEY_V2_CHUNK;
  memcpy (&ssa.sadb_sa_spi, proto->spi[incoming], sizeof ssa.sadb_sa_spi);
  ssa.sadb_sa_replay
    = conf_get_str ("General", "Shared-SADB") ? 0 : iproto->replay_window;
  ssa.sadb_sa_state = SADB_SASTATE_MATURE;
#ifdef SADB_X_SAFLAGS_TUNNEL
  ssa.sadb_sa_flags
    = iproto->encap_mode == IPSEC_ENCAP_TUNNEL ? SADB_X_SAFLAGS_TUNNEL : 0;
#else
  ssa.sadb_sa_flags = 0;
#endif
  if (pf_key_v2_msg_add (update, (struct sadb_ext *)&ssa, 0) == -1)
    goto cleanup;

  if (sa->seconds || sa->kilobytes)
    {
      /* setup the hard limits.  */
      life = malloc (sizeof *life);
      if (!life)
	goto cleanup;
      life->sadb_lifetime_len = sizeof *life / PF_KEY_V2_CHUNK;
      life->sadb_lifetime_exttype = SADB_EXT_LIFETIME_HARD;
      life->sadb_lifetime_allocations = 0;
      life->sadb_lifetime_bytes = sa->kilobytes * 1024;
      /*
       * XXX I am not sure which one is best in security respect.  Maybe the
       * RFCs actually mandate what a lifetime reaaly is.
       */
#if 0
      life->sadb_lifetime_addtime = 0;
      life->sadb_lifetime_usetime = sa->seconds;
#else
      life->sadb_lifetime_addtime = sa->seconds;
      life->sadb_lifetime_usetime = 0;
#endif
      if (pf_key_v2_msg_add (update, (struct sadb_ext *)life,
			     PF_KEY_V2_NODE_MALLOCED) == -1)
	goto cleanup;
      life = 0;

      /*
       * Setup the soft limits, we use 90 % of the hard ones.
       * XXX A configurable ratio would be better.
       */
      life = malloc (sizeof *life);
      if (!life)
	goto cleanup;
      life->sadb_lifetime_len = sizeof *life / PF_KEY_V2_CHUNK;
      life->sadb_lifetime_exttype = SADB_EXT_LIFETIME_SOFT;
      life->sadb_lifetime_allocations = 0;
      life->sadb_lifetime_bytes = sa->kilobytes * 1024 * 9 / 10;
      /*
       * XXX I am not sure which one is best in security respect.  Maybe the
       * RFCs actually mandate what a lifetime really is.
       */
#if 0
      life->sadb_lifetime_addtime = 0;
      life->sadb_lifetime_usetime = sa->seconds * 9 / 10;
#else
      life->sadb_lifetime_addtime = sa->seconds * 9 / 10;
      life->sadb_lifetime_usetime = 0;
#endif
      if (pf_key_v2_msg_add (update, (struct sadb_ext *)life,
			     PF_KEY_V2_NODE_MALLOCED) == -1)
	goto cleanup;
      life = 0;
    }

  /*
   * Setup the ADDRESS extensions.
   *
   * XXX Addresses has to be thought through.  Assumes IPv4.
   */
  if (incoming)
    sa->transport->vtbl->get_dst (sa->transport, &src, &srclen);
  else
    sa->transport->vtbl->get_src (sa->transport, &src, &srclen);
  len = sizeof *addr + PF_KEY_V2_ROUND (srclen);
  addr = malloc (len);
  if (!addr)
    goto cleanup;
  addr->sadb_address_exttype = SADB_EXT_ADDRESS_SRC;
  addr->sadb_address_len = len / PF_KEY_V2_CHUNK;
#if 0
  addr->sadb_address_proto = 0;
  addr->sadb_address_prefixlen = 0;
#endif
  addr->sadb_address_reserved = 0;
  memcpy (addr + 1, src, srclen);
  ((struct sockaddr_in *)(addr + 1))->sin_port = 0;
  if (pf_key_v2_msg_add (update, (struct sadb_ext *)addr,
			 PF_KEY_V2_NODE_MALLOCED) == -1)
    goto cleanup;
  addr = 0;

  if (incoming)
    sa->transport->vtbl->get_src (sa->transport, &dst, &dstlen);
  else
    sa->transport->vtbl->get_dst (sa->transport, &dst, &dstlen);
  len = sizeof *addr + PF_KEY_V2_ROUND (dstlen);
  addr = malloc (len);
  if (!addr)
    goto cleanup;
  addr->sadb_address_exttype = SADB_EXT_ADDRESS_DST;
  addr->sadb_address_len = len / PF_KEY_V2_CHUNK;
#if 0
  addr->sadb_address_proto = 0;
  addr->sadb_address_prefixlen = 0;
#endif
  addr->sadb_address_reserved = 0;
  memcpy (addr + 1, dst, dstlen);
  ((struct sockaddr_in *)(addr + 1))->sin_port = 0;
  if (pf_key_v2_msg_add (update, (struct sadb_ext *)addr,
			 PF_KEY_V2_NODE_MALLOCED) == -1)
    goto cleanup;
  addr = 0;

#if 0
  /* XXX I am not sure about what to do here just yet. */
  if (iproto->encap_mode == IPSEC_ENCAP_TUNNEL)
    {
      len = sizeof *addr + PF_KEY_V2_ROUND (dstlen);
      addr = malloc (len);
      if (!addr)
	goto cleanup;
      addr->sadb_address_exttype = SADB_EXT_ADDRESS_PROXY;
      addr->sadb_address_len = len / PF_KEY_V2_CHUNK;
#if 0
      addr->sadb_address_proto = 0;
      addr->sadb_address_prefixlen = 0;
#endif
      addr->sadb_address_reserved = 0;
      memcpy (addr + 1, dst, dstlen);
      ((struct sockaddr_in *)(addr + 1))->sin_port = 0;
      if (pf_key_v2_msg_add (update, (struct sadb_ext *)addr,
			     PF_KEY_V2_NODE_MALLOCED) == -1)
	goto cleanup;
      addr = 0;
#if 0
      msg->em_odst = msg->em_dst;
      msg->em_osrc = msg->em_src;
#endif
    }
#endif

  /* Setup the KEY extensions.  */
  len = sizeof *key + PF_KEY_V2_ROUND (hashlen);
  key = malloc (len);
  if (!key)
    goto cleanup;
  key->sadb_key_exttype = SADB_EXT_KEY_AUTH;
  key->sadb_key_len = len / PF_KEY_V2_CHUNK;
  key->sadb_key_bits = hashlen * 8;
  key->sadb_key_reserved = 0;
  memcpy (key + 1,
	  iproto->keymat[incoming]
	  + (proto->proto == IPSEC_PROTO_IPSEC_ESP ? keylen : 0),
	  hashlen);
  if (pf_key_v2_msg_add (update, (struct sadb_ext *)key,
			 PF_KEY_V2_NODE_MALLOCED) == -1)
    goto cleanup;
  key = 0;

  if (keylen)
    {
      len = sizeof *key + PF_KEY_V2_ROUND (keylen);
      key = malloc (len);
      if (!key)
	goto cleanup;
      key->sadb_key_exttype = SADB_EXT_KEY_ENCRYPT;
      key->sadb_key_len = len / PF_KEY_V2_CHUNK;
      key->sadb_key_bits = keylen * 8;
      key->sadb_key_reserved = 0;
      memcpy (key + 1, iproto->keymat[incoming], keylen);
      if (pf_key_v2_msg_add (update, (struct sadb_ext *)key,
			     PF_KEY_V2_NODE_MALLOCED) == -1)
	goto cleanup;
      key = 0;
    }

  /* XXX Here can identity and sensitivity extensions be setup.  */

  /* XXX IPv4 specific.  */
  log_debug (LOG_SYSDEP, 10, "pf_key_v2_set_spi: satype %d dst %s SPI 0x%x",
	     msg.sadb_msg_satype,
	     inet_ntoa (((struct sockaddr_in *)dst)->sin_addr),
	     ntohl (ssa.sadb_sa_spi));

  /*
   * Although PF_KEY knows about expirations, it is unreliable per the specs
   * thus we need to do them inside isakmpd as well.
   */
  if (sa->seconds)
    if (sa_setup_expirations (sa))
      goto cleanup;

  ret = pf_key_v2_call (update);
  pf_key_v2_msg_free (update);
  update = 0;
  if (!ret)
    goto cleanup;
  err = ((struct sadb_msg *)TAILQ_FIRST (ret)->seg)->sadb_msg_errno;
  pf_key_v2_msg_free (ret);
  ret = 0;

  /*
   * If we are doing an addition into an SADB shared with our peer, errors
   * here are to be expected as the peer will already have created the SA,
   * and can thus be ignored.
   */
  if (err && !(msg.sadb_msg_type == SADB_ADD
	       && conf_get_str ("General", "Shared-SADB")))
    {
      log_print ("pf_key_v2_set_spi: %s: %s",
		 msg.sadb_msg_type == SADB_ADD ? "ADD" : "UPDATE",
		 strerror (err));
      goto cleanup;
    }

  log_debug (LOG_SYSDEP, 50, "pf_key_v2_set_spi: done");

  return 0;

 cleanup:
  if (addr)
    free (addr);
  if (life)
    free (life);
  if (key)
    free (key);
  if (update)
    pf_key_v2_msg_free (update);
  if (ret)
    pf_key_v2_msg_free (ret);
  return -1;
}
 
/*
 * Enable/disable a flow.
 * XXX Assumes OpenBSD {ADD,DEL}FLOW extensions.
 * Should probably be moved to sysdep.c
 */
static int
pf_key_v2_flow (in_addr_t laddr, in_addr_t lmask, in_addr_t raddr,
		in_addr_t rmask, u_int8_t *spi, u_int8_t proto,
		in_addr_t dst, int delete, int ingress)
{
#if defined (SADB_X_ADDFLOW) && defined (SADB_X_DELFLOW)
  struct sadb_msg msg;
  struct sadb_sa ssa;
  struct sadb_address *addr = 0;
  struct pf_key_v2_msg *flow = 0, *ret = 0;
  size_t len;
  int err;

  msg.sadb_msg_type = delete ? SADB_X_DELFLOW : SADB_X_ADDFLOW;
  switch (proto)
    {
    case IPSEC_PROTO_IPSEC_ESP:
      msg.sadb_msg_satype = SADB_SATYPE_ESP;
      break;
    case IPSEC_PROTO_IPSEC_AH:
      msg.sadb_msg_satype = SADB_SATYPE_AH;
      break;
    default:
      log_print ("pf_key_v2_flow: invalid proto %d", proto);
      goto cleanup;
    }
  msg.sadb_msg_seq = 0;
  flow = pf_key_v2_msg_new (&msg, 0);
  if (!flow)
    goto cleanup;

  /* Setup the SA extension.  */
  ssa.sadb_sa_exttype = SADB_EXT_SA;
  ssa.sadb_sa_len = sizeof ssa / PF_KEY_V2_CHUNK;
  memcpy (&ssa.sadb_sa_spi, spi, sizeof ssa.sadb_sa_spi);
  ssa.sadb_sa_replay = 0;
  ssa.sadb_sa_state = 0;
  ssa.sadb_sa_auth = 0;
  ssa.sadb_sa_encrypt = 0;
  ssa.sadb_sa_flags = 0;
  if (!delete)
    ssa.sadb_sa_flags |= SADB_X_SAFLAGS_REPLACEFLOW;
  if (ingress)
    ssa.sadb_sa_flags |= SADB_X_SAFLAGS_INGRESS_FLOW;
  if (pf_key_v2_msg_add (flow, (struct sadb_ext *)&ssa, 0) == -1)
    goto cleanup;

  /*
   * Setup the ADDRESS extensions.
   *
   * XXX Addresses has to be thought through.  Assumes IPv4.
   */
  len = sizeof *addr + PF_KEY_V2_ROUND (sizeof (struct sockaddr_in));
  if (!delete)
    {
      addr = malloc (len);
      if (!addr)
	goto cleanup;
      addr->sadb_address_exttype = SADB_EXT_ADDRESS_DST;
      addr->sadb_address_len = len / PF_KEY_V2_CHUNK;
#if 0
      addr->sadb_address_proto = 0;
      addr->sadb_address_prefixlen = 0;
#endif
      addr->sadb_address_reserved = 0;
      memset (addr + 1, '\0', sizeof (struct sockaddr_in));
      ((struct sockaddr_in *)(addr + 1))->sin_len
	= sizeof (struct sockaddr_in);
      ((struct sockaddr_in *)(addr + 1))->sin_family = AF_INET;
      ((struct sockaddr_in *)(addr + 1))->sin_addr.s_addr = dst;
      ((struct sockaddr_in *)(addr + 1))->sin_port = 0;
      if (pf_key_v2_msg_add (flow, (struct sadb_ext *)addr,
			     PF_KEY_V2_NODE_MALLOCED) == -1)
	goto cleanup;
      addr = 0;
    }

  addr = malloc (len);
  if (!addr)
    goto cleanup;
  addr->sadb_address_exttype = SADB_X_EXT_SRC_FLOW;
  addr->sadb_address_len = len / PF_KEY_V2_CHUNK;
#if 0
  addr->sadb_address_proto = 0;
  addr->sadb_address_prefixlen = 0;
#endif
  addr->sadb_address_reserved = 0;
  memset (addr + 1, '\0', sizeof (struct sockaddr_in));
  ((struct sockaddr_in *)(addr + 1))->sin_len = sizeof (struct sockaddr_in);
  ((struct sockaddr_in *)(addr + 1))->sin_family = AF_INET;
  ((struct sockaddr_in *)(addr + 1))->sin_addr.s_addr = laddr;
  ((struct sockaddr_in *)(addr + 1))->sin_port = 0;
  if (pf_key_v2_msg_add (flow, (struct sadb_ext *)addr,
			 PF_KEY_V2_NODE_MALLOCED) == -1)
    goto cleanup;
  addr = 0;

  addr = malloc (len);
  if (!addr)
    goto cleanup;
  addr->sadb_address_exttype = SADB_X_EXT_SRC_MASK;
  addr->sadb_address_len = len / PF_KEY_V2_CHUNK;
#if 0
  addr->sadb_address_proto = 0;
  addr->sadb_address_prefixlen = 0;
#endif
  addr->sadb_address_reserved = 0;
  memset (addr + 1, '\0', sizeof (struct sockaddr_in));
  ((struct sockaddr_in *)(addr + 1))->sin_len = sizeof (struct sockaddr_in);
  ((struct sockaddr_in *)(addr + 1))->sin_family = AF_INET;
  ((struct sockaddr_in *)(addr + 1))->sin_addr.s_addr = lmask;
  ((struct sockaddr_in *)(addr + 1))->sin_port = 0;
  if (pf_key_v2_msg_add (flow, (struct sadb_ext *)addr,
			 PF_KEY_V2_NODE_MALLOCED) == -1)
    goto cleanup;
  addr = 0;

  addr = malloc (len);
  if (!addr)
    goto cleanup;
  addr->sadb_address_exttype = SADB_X_EXT_DST_FLOW;
  addr->sadb_address_len = len / PF_KEY_V2_CHUNK;
#if 0
  addr->sadb_address_proto = 0;
  addr->sadb_address_prefixlen = 0;
#endif
  addr->sadb_address_reserved = 0;
  memset (addr + 1, '\0', sizeof (struct sockaddr_in));
  ((struct sockaddr_in *)(addr + 1))->sin_len = sizeof (struct sockaddr_in);
  ((struct sockaddr_in *)(addr + 1))->sin_family = AF_INET;
  ((struct sockaddr_in *)(addr + 1))->sin_addr.s_addr = raddr;
  ((struct sockaddr_in *)(addr + 1))->sin_port = 0;
  if (pf_key_v2_msg_add (flow, (struct sadb_ext *)addr,
			 PF_KEY_V2_NODE_MALLOCED) == -1)
    goto cleanup;
  addr = 0;

  addr = malloc (len);
  if (!addr)
    goto cleanup;
  addr->sadb_address_exttype = SADB_X_EXT_DST_MASK;
  addr->sadb_address_len = len / PF_KEY_V2_CHUNK;
#if 0
  addr->sadb_address_proto = 0;
  addr->sadb_address_prefixlen = 0;
#endif
  addr->sadb_address_reserved = 0;
  memset (addr + 1, '\0', sizeof (struct sockaddr_in));
  ((struct sockaddr_in *)(addr + 1))->sin_len = sizeof (struct sockaddr_in);
  ((struct sockaddr_in *)(addr + 1))->sin_family = AF_INET;
  ((struct sockaddr_in *)(addr + 1))->sin_addr.s_addr = rmask;
  ((struct sockaddr_in *)(addr + 1))->sin_port = 0;
  if (pf_key_v2_msg_add (flow, (struct sadb_ext *)addr,
			 PF_KEY_V2_NODE_MALLOCED) == -1)
    goto cleanup;
  addr = 0;

  log_debug (LOG_SYSDEP, 50, "pf_key_v2_flow: src %x %x dst %x %x",
	     htonl(laddr), htonl(lmask), htonl(raddr), htonl(rmask));

  ret = pf_key_v2_call (flow);
  pf_key_v2_msg_free (flow);
  flow = 0;
  if (!ret)
    goto cleanup;
  err = ((struct sadb_msg *)TAILQ_FIRST (ret)->seg)->sadb_msg_errno;
  if (err)
    {
      log_print ("pf_key_v2_flow: %sFLOW: %s", delete ? "DEL" : "ADD",
		 strerror (err));
      goto cleanup;
    }
  pf_key_v2_msg_free (ret);

  log_debug (LOG_MISC, 50, "pf_key_v2_flow: done");

  return 0;

 cleanup:
  if (addr)
    free (addr);
  if (flow)
    pf_key_v2_msg_free (flow);
  if (ret)
    pf_key_v2_msg_free (ret);
  return -1;

#else
  log_error ("pf_key_v2_flow: not supported in pure PF_KEYv2");
  return -1;
#endif
}

/* Enable a flow given a SA.  */
int
pf_key_v2_enable_sa (struct sa *sa)
{
  struct ipsec_sa *isa = sa->data;
  struct sockaddr *dst;
  int dstlen, error;
  struct proto *proto = TAILQ_FIRST (&sa->protos);

  sa->transport->vtbl->get_dst (sa->transport, &dst, &dstlen);

  error = pf_key_v2_flow (isa->src_net, isa->src_mask, isa->dst_net,
			  isa->dst_mask, proto->spi[0], proto->proto,
			  ((struct sockaddr_in *)dst)->sin_addr.s_addr, 0, 0);

  if (error)
    return error;

  /* Ingress flow */
  while (TAILQ_NEXT(proto, link))
    proto = TAILQ_NEXT(proto, link);

  sa->transport->vtbl->get_src (sa->transport, &dst, &dstlen);

  return pf_key_v2_flow(isa->dst_net, isa->dst_mask, isa->src_net,
			isa->src_mask, proto->spi[1], proto->proto,
			((struct sockaddr_in *)dst)->sin_addr.s_addr, 0, 1);
}

/* Disable a flow given a SA.  */
static int
pf_key_v2_disable_sa (struct sa *sa)
{
  struct ipsec_sa *isa = sa->data;
  struct sockaddr *dst;
  int dstlen, error;
  struct proto *proto = TAILQ_FIRST (&sa->protos);

  sa->transport->vtbl->get_dst (sa->transport, &dst, &dstlen);

  error = pf_key_v2_flow (isa->src_net, isa->src_mask, isa->dst_net,
			  isa->dst_mask, proto->spi[0], proto->proto,
			  ((struct sockaddr_in *)dst)->sin_addr.s_addr, 1, 0);
  if (error)
    return error;

  /* Ingress flow */
  while (TAILQ_NEXT(proto, link))
    proto = TAILQ_NEXT(proto, link);

  sa->transport->vtbl->get_src (sa->transport, &dst, &dstlen);

  return pf_key_v2_flow(isa->dst_net, isa->dst_mask, isa->src_net,
			isa->src_mask, proto->spi[1], proto->proto,
			((struct sockaddr_in *)dst)->sin_addr.s_addr, 1, 1);
}

/*
 * Delete the IPSec SA represented by the INCOMING direction in protocol PROTO
 * of the IKE security association SA.  Also delete potential flows tied to it.
 */
int
pf_key_v2_delete_spi (struct sa *sa, struct proto *proto, int incoming)
{
  struct sadb_msg msg; 
  struct sadb_sa ssa;
  struct sadb_address *addr = 0;
  struct sockaddr *saddr;
  int saddrlen, len, err;
  struct pf_key_v2_msg *delete = 0, *ret = 0;

  /*
   * If the SA was outbound and it has not yet been replaced, remove the
   * flow associated with it.
   * We ignore any errors from the disabling of the flow, it does not matter.
   */
  if (!incoming && !(sa->flags & SA_FLAG_REPLACED))
    pf_key_v2_disable_sa (sa);

  msg.sadb_msg_type = SADB_DELETE;
  switch (proto->proto)
    {
    case IPSEC_PROTO_IPSEC_ESP:
      msg.sadb_msg_satype = SADB_SATYPE_ESP;
      break;
    case IPSEC_PROTO_IPSEC_AH:
      msg.sadb_msg_satype = SADB_SATYPE_AH;
      break;
    default:
      log_print ("pf_key_v2_delete_spi: invalid proto %d", proto->proto);
      goto cleanup;
    }
  msg.sadb_msg_seq = 0;
  delete = pf_key_v2_msg_new (&msg, 0);
  if (!delete)
    goto cleanup;

  /* Setup the SA extension.  */
  ssa.sadb_sa_exttype = SADB_EXT_SA;
  ssa.sadb_sa_len = sizeof ssa / PF_KEY_V2_CHUNK;
  memcpy (&ssa.sadb_sa_spi, proto->spi[incoming], sizeof ssa.sadb_sa_spi);
  ssa.sadb_sa_replay = 0;
  ssa.sadb_sa_state = 0;
  ssa.sadb_sa_auth = 0;
  ssa.sadb_sa_encrypt = 0;
  ssa.sadb_sa_flags = 0;
  if (pf_key_v2_msg_add (delete, (struct sadb_ext *)&ssa, 0) == -1)
    goto cleanup;

  /*
   * Setup the ADDRESS extensions.
   *
   * XXX Addresses has to be thought through.  Assumes IPv4.
   */
  if (incoming)
    sa->transport->vtbl->get_dst (sa->transport, &saddr, &saddrlen);
  else
    sa->transport->vtbl->get_src (sa->transport, &saddr, &saddrlen);
  len = sizeof *addr + PF_KEY_V2_ROUND (saddrlen);
  addr = malloc (len);
  if (!addr)
    goto cleanup;
  addr->sadb_address_exttype = SADB_EXT_ADDRESS_SRC;
  addr->sadb_address_len = len / PF_KEY_V2_CHUNK;
#if 0
  addr->sadb_address_proto = 0;
  addr->sadb_address_prefixlen = 0;
#endif
  addr->sadb_address_reserved = 0;
  memcpy (addr + 1, saddr, saddrlen);
  ((struct sockaddr_in *)(addr + 1))->sin_port = 0;
  if (pf_key_v2_msg_add (delete, (struct sadb_ext *)addr,
			 PF_KEY_V2_NODE_MALLOCED) == -1)
    goto cleanup;
  addr = 0;

  if (incoming)
    sa->transport->vtbl->get_src (sa->transport, &saddr, &saddrlen);
  else
    sa->transport->vtbl->get_dst (sa->transport, &saddr, &saddrlen);
  len = sizeof *addr + PF_KEY_V2_ROUND (saddrlen);
  addr = malloc (len);
  if (!addr)
    goto cleanup;
  addr->sadb_address_exttype = SADB_EXT_ADDRESS_DST;
  addr->sadb_address_len = len / PF_KEY_V2_CHUNK;
#if 0
  addr->sadb_address_proto = 0;
  addr->sadb_address_prefixlen = 0;
#endif
  addr->sadb_address_reserved = 0;
  memcpy (addr + 1, saddr, saddrlen);
  ((struct sockaddr_in *)(addr + 1))->sin_port = 0;
  if (pf_key_v2_msg_add (delete, (struct sadb_ext *)addr,
			 PF_KEY_V2_NODE_MALLOCED) == -1)
    goto cleanup;
  addr = 0;

  ret = pf_key_v2_call (delete);
  pf_key_v2_msg_free (delete);
  delete = 0;
  if (!ret)
    goto cleanup;
  err = ((struct sadb_msg *)TAILQ_FIRST (ret)->seg)->sadb_msg_errno;
  if (err)
    {
      log_print ("pf_key_v2_delete_spi: DELETE: %s", strerror (err));
      goto cleanup;
    }
  pf_key_v2_msg_free (ret);

  log_debug (LOG_MISC, 50, "pf_key_v2_delete_spi: done");

  return 0;

 cleanup:
  if (addr)
    free (addr);
  if (delete)
    pf_key_v2_msg_free (delete);
  if (ret)
    pf_key_v2_msg_free (ret);
  return -1;
}

static void
pf_key_v2_stayalive (struct exchange *exchange, void *vconn, int fail)
{
  char *conn = vconn;
  struct sa *sa;

  /* XXX What if it is phase 1?  */
  sa = sa_lookup_by_name (conn, 2);
  if (sa)
    sa->flags |= SA_FLAG_STAYALIVE;
}

/* Check if a connection CONN exists, otherwise establish it.  */
void
pf_key_v2_connection_check (char *conn)
{
  if (!sa_lookup_by_name (conn, 2))
    {
      log_debug (LOG_SYSDEP, 70,
		 "pf_key_v2_connection_check: SA for %s missing", conn);
      exchange_establish (conn, pf_key_v2_stayalive, conn);
    }
  else
    log_debug (LOG_SYSDEP, 70, "pf_key_v2_connection_check: SA for %s exists",
	       conn);
}

/* Handle a PF_KEY lifetime expiration message PMSG.  */
static void
pf_key_v2_expire (struct pf_key_v2_msg *pmsg)
{
  struct sadb_msg *msg;
  struct sadb_sa *ssa;
  struct sadb_address *dst;
  struct sockaddr *dstaddr;
  struct sadb_lifetime *life;
  struct sa *sa;
  struct pf_key_v2_node *lifenode;

  msg = (struct sadb_msg *)TAILQ_FIRST (pmsg)->seg;
  ssa = pf_key_v2_find_ext (pmsg, SADB_EXT_SA)->seg;
  dst = pf_key_v2_find_ext (pmsg, SADB_EXT_ADDRESS_DST)->seg;
  dstaddr = (struct sockaddr *)(dst + 1);
  lifenode = pf_key_v2_find_ext (pmsg, SADB_EXT_LIFETIME_HARD);
  if (!lifenode)
    lifenode = pf_key_v2_find_ext (pmsg, SADB_EXT_LIFETIME_SOFT);
  life = lifenode->seg;

  /* XXX IPv4 specific.  */
  log_debug (LOG_SYSDEP, 20,
	     "pf_key_v2_expire: %s dst %s SPI %x sproto %d",
	     life->sadb_lifetime_exttype == SADB_EXT_LIFETIME_SOFT ? "SOFT"
	     : "HARD",
	     inet_ntoa (((struct sockaddr_in *)dstaddr)->sin_addr),
	     ntohl (ssa->sadb_sa_spi), msg->sadb_msg_satype);

  /*
   * Find the IPsec SA.  The IPsec stack has two SAs for every IKE SA,
   * one outgoing and one incoming, we regard expirations for any of
   * them as an expiration of the full IKE SA.  Likewise, in
   * protection suites consisting of more than one protocol, any
   * expired individual IPsec stack SA will be seen as an expiration
   * of the full suite.
   *
   * XXX When anything else than AH and ESP is supported this needs to change.
   * XXX IPv4 specific.
   */
  sa = ipsec_sa_lookup (((struct sockaddr_in *)dstaddr)->sin_addr.s_addr,
			ssa->sadb_sa_spi,
			msg->sadb_msg_satype == SADB_SATYPE_ESP
			? IPSEC_PROTO_IPSEC_ESP : IPSEC_PROTO_IPSEC_AH);

  /* If the SA is already gone, don't do anything.  */
  if (!sa)
    return;

  /*
   * If we want this connection to stay "forever", we should renegotiate
   * already at the soft expire, and certainly at the hard expire if we
   * haven't started a negotiation by then.  However, do not renegotiate
   * if this SA is already obsoleted by another.
   */
  if ((sa->flags & (SA_FLAG_STAYALIVE | SA_FLAG_REPLACED))
      == SA_FLAG_STAYALIVE)
    exchange_establish (sa->name, 0, 0);

  if (life->sadb_lifetime_exttype == SADB_EXT_LIFETIME_HARD)
    {
      /*
       * XXX We need to reestablish the on-demand route here.  This we need
       * even if we have started a new negotiation, considering it might
       * fail.
       */

      /* Remove the old SA, it isn't useful anymore.  */
      sa_free (sa);
    }
}

static void
pf_key_v2_notify (struct pf_key_v2_msg *msg)
{
  switch (((struct sadb_msg *)TAILQ_FIRST (msg)->seg)->sadb_msg_type)
    {
    case SADB_EXPIRE:
      pf_key_v2_expire (msg);
      break;

    case SADB_ACQUIRE:
      log_print ("pf_key_v2_notify: ACQUIRE not yet implemented");
      /* XXX To be implemented.  */
      break;

    default:
      log_print ("pf_key_v2_notify: unexpected message type (%d)",
		 ((struct sadb_msg *)TAILQ_FIRST (msg)->seg)->sadb_msg_type);
    }
  pf_key_v2_msg_free (msg);
}

void
pf_key_v2_handler (int fd)
{
  struct pf_key_v2_msg *msg;
  int n;

  /*
   * As synchronous read/writes to the socket can have taken place between
   * the select(2) call of the main loop and this handler, we need to recheck
   * the readability.
   */
  if (ioctl (pf_key_v2_socket, FIONREAD, &n) == -1)
    {
      log_error ("pf_key_v2_handler: ioctl (%d, FIONREAD, &n) failed",
		 pf_key_v2_socket);
      return;
    }
  if (!n)
    return;

  msg = pf_key_v2_read (0);
  if (msg)
    pf_key_v2_notify (msg);
}

/*
 * Group 2 IPSec SAs given by the PROTO1 and PROTO2 protocols of the SA IKE
 * security association in a chain.
 * XXX Assumes OpenBSD GRPSPIS extension.  Should probably be moved to sysdep.c
 */
int
pf_key_v2_group_spis (struct sa *sa, struct proto *proto1,
		      struct proto *proto2, int incoming)
{
#ifdef SADB_X_GRPSPIS
  struct sadb_msg msg;
  struct sadb_sa sa1, sa2;
  struct sadb_address *addr = 0;
  struct sadb_protocol protocol;
  struct pf_key_v2_msg *grpspis = 0, *ret = 0;
  struct sockaddr *saddr;
  int saddrlen, err;
  size_t len;

  msg.sadb_msg_type = SADB_X_GRPSPIS;
  switch (proto1->proto)
    {
    case IPSEC_PROTO_IPSEC_ESP:
      msg.sadb_msg_satype = SADB_SATYPE_ESP;
      break;
    case IPSEC_PROTO_IPSEC_AH:
      msg.sadb_msg_satype = SADB_SATYPE_AH;
      break;
    default:
      log_print ("pf_key_v2_group_spis: invalid proto %d", proto1->proto);
      goto cleanup;
    }
  msg.sadb_msg_seq = 0;
  grpspis = pf_key_v2_msg_new (&msg, 0);
  if (!grpspis)
    goto cleanup;

  /* Setup the SA extensions.  */
  sa1.sadb_sa_exttype = SADB_EXT_SA;
  sa1.sadb_sa_len = sizeof sa1 / PF_KEY_V2_CHUNK;
  memcpy (&sa1.sadb_sa_spi, proto1->spi[incoming], sizeof sa1.sadb_sa_spi);
  sa1.sadb_sa_replay = 0;
  sa1.sadb_sa_state = 0;
  sa1.sadb_sa_auth = 0;
  sa1.sadb_sa_encrypt = 0;
  sa1.sadb_sa_flags = 0;
  if (pf_key_v2_msg_add (grpspis, (struct sadb_ext *)&sa1, 0) == -1)
    goto cleanup;

  sa2.sadb_sa_exttype = SADB_X_EXT_SA2;
  sa2.sadb_sa_len = sizeof sa2 / PF_KEY_V2_CHUNK;
  memcpy (&sa2.sadb_sa_spi, proto2->spi[incoming], sizeof sa2.sadb_sa_spi);
  sa2.sadb_sa_replay = 0;
  sa2.sadb_sa_state = 0;
  sa2.sadb_sa_auth = 0;
  sa2.sadb_sa_encrypt = 0;
  sa2.sadb_sa_flags = 0;
  if (pf_key_v2_msg_add (grpspis, (struct sadb_ext *)&sa2, 0) == -1)
    goto cleanup;

  /*
   * Setup the ADDRESS extensions.
   *
   * XXX Addresses has to be thought through.  Assumes IPv4.
   */
  if (incoming)
    sa->transport->vtbl->get_src (sa->transport, &saddr, &saddrlen);
  else
    sa->transport->vtbl->get_dst (sa->transport, &saddr, &saddrlen);
  len = sizeof *addr + PF_KEY_V2_ROUND (saddrlen);
  addr = malloc (len);
  if (!addr)
    goto cleanup;
  addr->sadb_address_exttype = SADB_EXT_ADDRESS_DST;
  addr->sadb_address_len = len / PF_KEY_V2_CHUNK;
#if 0
  addr->sadb_address_proto = 0;
  addr->sadb_address_prefixlen = 0;
#endif
  addr->sadb_address_reserved = 0;
  memcpy (addr + 1, saddr, saddrlen);
  ((struct sockaddr_in *)(addr + 1))->sin_port = 0;
  if (pf_key_v2_msg_add (grpspis, (struct sadb_ext *)addr,
			 PF_KEY_V2_NODE_MALLOCED) == -1)
    goto cleanup;
  addr = 0;

  addr = malloc (len);
  if (!addr)
    goto cleanup;
  addr->sadb_address_exttype = SADB_X_EXT_DST2;
  addr->sadb_address_len = len / PF_KEY_V2_CHUNK;
#if 0
  addr->sadb_address_proto = 0;
  addr->sadb_address_prefixlen = 0;
#endif
  addr->sadb_address_reserved = 0;
  memcpy (addr + 1, saddr, saddrlen);
  ((struct sockaddr_in *)(addr + 1))->sin_port = 0;
  if (pf_key_v2_msg_add (grpspis, (struct sadb_ext *)addr,
			 PF_KEY_V2_NODE_MALLOCED) == -1)
    goto cleanup;
  addr = 0;

  /* Setup the PROTOCOL extension.  */
  protocol.sadb_protocol_exttype = SADB_X_EXT_PROTOCOL;
  protocol.sadb_protocol_len = sizeof protocol / PF_KEY_V2_CHUNK;
  switch (proto2->proto)
    {
    case IPSEC_PROTO_IPSEC_ESP:
      protocol.sadb_protocol_proto = SADB_SATYPE_ESP;
      break;
    case IPSEC_PROTO_IPSEC_AH:
      protocol.sadb_protocol_proto = SADB_SATYPE_AH;
      break;
    default:
      log_print ("pf_key_v2_group_spis: invalid proto %d", proto2->proto);
      goto cleanup;
    }
  protocol.sadb_protocol_reserved1 = 0;
  protocol.sadb_protocol_reserved2 = 0;
  if (pf_key_v2_msg_add (grpspis, (struct sadb_ext *)&protocol, 0) == -1)
    goto cleanup;

  ret = pf_key_v2_call (grpspis);
  pf_key_v2_msg_free (grpspis);
  grpspis = 0;
  if (!ret)
    goto cleanup;
  err = ((struct sadb_msg *)TAILQ_FIRST (ret)->seg)->sadb_msg_errno;
  if (err)
    {
      log_print ("pf_key_v2_group_spis: GRPSPIS: %s", strerror (err));
      goto cleanup;
    }
  pf_key_v2_msg_free (ret);

  log_debug (LOG_SYSDEP, 50, "pf_key_v2_group_spis: done");

  return 0;

 cleanup:
  if (addr)
     free (addr);
  if (grpspis)
    pf_key_v2_msg_free (grpspis);
  if (ret)
    pf_key_v2_msg_free (ret);
  return -1;

#else
  log_error ("pf_key_v2_group_spis: not supported in pure PF_KEYv2");
  return -1;
#endif
}
