/*	$OpenBSD: sysdep.c,v 1.4 1999/04/05 20:57:35 niklas Exp $	*/
/*	$EOM: sysdep.c,v 1.6 1999/04/05 18:27:42 niklas Exp $	*/

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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

#include "sysdep.h"

#ifdef NEED_SYSDEP_APP
#include "app.h"
#include "conf.h"
#include "ipsec.h"

#ifdef USE_PF_KEY_V2
#include "pf_key_v2.h"
#define KEY_API(x) pf_key_v2_##x
#else
#include <net/encap.h>
#include "pf_encap.h"
#define KEY_API(x) pf_encap_##x
#endif

#endif NEED_SYSDEP_APP
#include "log.h"

extern char *__progname;

/*
 * This is set to true in case of regression-test mode, when it will
 * cause predictable random numbers be generated.
 */
int regrand = 0;

/*
 * An as strong as possible random number generator, reverting to a
 * deterministic pseudo-random one if regrand is set.
 */
u_int32_t
sysdep_random ()
{
  if (!regrand)
    return arc4random ();
  else
    return random();
}

/* Return the basename of the command used to invoke us.  */
char *
sysdep_progname ()
{
  return __progname;
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

/*
 * This is where we try to set up routes that make the IP-stack request
 * SAs from us on demand. 
 */
void
sysdep_conf_init_hook ()
{
  struct conf_list *conns;
  struct conf_list_node *conn;

  conns = conf_get_list ("Phase 2", "Connections");
  if (conns)
    {
      for (conn = TAILQ_FIRST (&conns->fields); conn;
	   conn = TAILQ_NEXT (conn, link))
	{
	  if (KEY_API(connection) (conn->field))
	    /* XXX What else?  */
	    continue;
	}
      conf_free_list (conns);
    }
}

/*
 * Generate a SPI for protocol PROTO and the source/destination pair given by
 * SRC, SRCLEN, DST & DSTLEN.  Stash the SPI size in SZ.
 */
u_int8_t *
sysdep_ipsec_get_spi (size_t *sz, u_int8_t proto, struct sockaddr *src,
		      int srclen, struct sockaddr *dst, int dstlen)
{
  if (app_none)
    {
      *sz = IPSEC_SPI_SIZE;
      /* XXX should be random instead I think.  */
      return strdup ("\x12\x34\x56\x78");
    }
  return KEY_API (get_spi) (sz, proto, src, srclen, dst, dstlen);
}

/* Force communication on socket FD to go in the clear.  */
int
sysdep_cleartext (int fd)
{
  int level;

  if (app_none)
    return 0;

  /*
   * Need to bypass system security policy, so I can send and
   * receive key management datagrams in the clear.
   */
  level = IPSEC_LEVEL_BYPASS;
  if (setsockopt (fd, IPPROTO_IP, IP_AUTH_LEVEL, (char *)&level, sizeof level)
      == -1)
    {
      log_error ("sysdep_cleartext: "
		 "setsockopt (%d, IPPROTO_IP, IP_AUTH_LEVEL, ...) failed", fd);
      return -1;
    }
  if (setsockopt (fd, IPPROTO_IP, IP_ESP_TRANS_LEVEL, (char *)&level,
		  sizeof level) == -1)
    {
      log_error ("sysdep_cleartext: "
		 "setsockopt (%d, IPPROTO_IP, IP_ESP_TRANS_LEVEL, ...) "
		 "failed", fd);
      return -1;
    }
  if (setsockopt (fd, IPPROTO_IP, IP_ESP_NETWORK_LEVEL, (char *)&level,
		  sizeof level) == -1)
    {
      log_error("sysdep_cleartext: "
		"setsockopt (%d, IPPROTO_IP, IP_ESP_NETWORK_LEVEL, ...) "
		 "failed", fd);
      return -1;
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
sysdep_ipsec_enable_sa (struct sa *sa)
{
  if (app_none)
    return 0;
  return KEY_API (enable_sa) (sa);
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
sysdep_ipsec_set_spi (struct sa *sa, struct proto *proto, int incoming)
{
  if (app_none)
    return 0;
  return KEY_API (set_spi) (sa, proto, incoming);
}
#endif
