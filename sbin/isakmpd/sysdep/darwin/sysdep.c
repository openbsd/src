/*	$OpenBSD: sysdep.c,v 1.3 2004/08/10 15:59:10 ho Exp $	*/

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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

#include "sysdep.h"

#include "util.h"

#ifdef NEED_SYSDEP_APP
#include "app.h"
#include "conf.h"
#include "ipsec.h"

#ifdef USE_PF_KEY_V2
#include "pf_key_v2.h"
#define KEY_API(x) pf_key_v2_##x
#endif

#endif /* NEED_SYSDEP_APP */
#include "log.h"

extern char *__progname;

/*
 * An as strong as possible random number generator, reverting to a
 * deterministic pseudo-random one if regrand is set.
 */
u_int32_t
sysdep_random ()
{
  return random();
}

/* Return the basename of the command used to invoke us.  */
char *
sysdep_progname ()
{
  return __progname;
}

/* Return the length of the sockaddr struct.  */
u_int8_t
sysdep_sa_len (struct sockaddr *sa)
{
  return sa->sa_len;
}

/* As regress/ use this file I protect the sysdep_app_* stuff like this.  */
#ifdef NEED_SYSDEP_APP
/*
 * Prepare the application we negotiate SAs for (i.e. the IPsec stack)
 * for communication.  We return a file descriptor useable to select(2) on.
 */
int
sysdep_app_open ()
{
  return KEY_API(open) ();
}

/*
 * When select(2) has noticed our application needs attendance, this is what
 * gets called.  FD is the file descriptor causing the alarm.
 */
void
sysdep_app_handler (int fd)
{
  KEY_API (handler) (fd);
}

/* Check that the connection named NAME is active, or else make it active.  */
void
sysdep_connection_check (char *name)
{
  KEY_API (connection_check) (name);
}

/*
 * Generate a SPI for protocol PROTO and the source/destination pair given by
 * SRC, SRCLEN, DST & DSTLEN.  Stash the SPI size in SZ.
 */
u_int8_t *
sysdep_ipsec_get_spi (size_t *sz, u_int8_t proto, struct sockaddr *src,
		      struct sockaddr *dst, u_int32_t seq)
{
  if (app_none)
    {
      *sz = IPSEC_SPI_SIZE;
      /* XXX should be random instead I think.  */
      return strdup ("\x12\x34\x56\x78");
    }
  return KEY_API (get_spi) (sz, proto, src, dst, seq);
}

struct sa_kinfo *
sysdep_ipsec_get_kernel_sa(u_int8_t *spi, size_t spi_sz, u_int8_t proto,
    struct sockaddr *dst)
{
	if (app_none)
		return 0;
	/* XXX return KEY_API(get_kernel_sa)(spi, spi_sz, proto, dst); */
	return 0;
}

/* Force communication on socket FD to go in the clear.  */
int
sysdep_cleartext (int fd, int af)
{
  char *buf;
  char *policy[] = { "in bypass", "out bypass", NULL };
  char **p;
  int ipp;

  if (app_none)
    return 0;

  switch (af)
    {
    case AF_INET:
      ipp = IPPROTO_IP;
      break;
    case AF_INET6:
      ipp = IPPROTO_IPV6;
      break;
    default:
      log_print ("sysdep_cleartext: unsupported protocol family %d", af);
      return -1;
    }

  /*
   * Need to bypass system security policy, so I can send and
   * receive key management datagrams in the clear.
   */

  for (p = policy; p && *p; p++)
    {
      buf = ipsec_set_policy (*p, strlen(*p));
      if (buf == NULL)
	{
	  log_error ("sysdep_cleartext: %s: %s", *p, ipsec_strerror());
	  return -1;
	}

      if (setsockopt(fd, ipp, IP_IPSEC_POLICY, buf,
		     ipsec_get_policylen(buf)) < 0)
	{
	  log_error ("sysdep_cleartext: "
		     "setsockopt (%d, IPPROTO_IP, IP_IPSEC_POLICY, ...) failed",
		     fd);
	  return -1;
	}
      free(buf);
    }

  return 0;
}

int
sysdep_ipsec_delete_spi (struct sa *sa, struct proto *proto, int incoming)
{
  if (app_none)
    return 0;
  return KEY_API (delete_spi) (sa, proto, incoming);
}

int
sysdep_ipsec_enable_sa (struct sa *sa, struct sa *isakmp_sa)
{
  if (app_none)
    return 0;
  return KEY_API (enable_sa) (sa, isakmp_sa);
}

int
sysdep_ipsec_group_spis (struct sa *sa, struct proto *proto1,
			 struct proto *proto2, int incoming)
{
  if (app_none)
    return 0;
  return KEY_API (group_spis) (sa, proto1, proto2, incoming);
}

int
sysdep_ipsec_set_spi (struct sa *sa, struct proto *proto, int incoming,
		      struct sa *isakmp_sa)
{
  if (app_none)
    return 0;
  return KEY_API (set_spi) (sa, proto, incoming, isakmp_sa);
}
#endif
