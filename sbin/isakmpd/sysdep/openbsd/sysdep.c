/*	$OpenBSD: sysdep.c,v 1.2 1999/03/02 15:27:36 niklas Exp $	*/
/*	$EOM: sysdep.c,v 1.1 1999/02/25 14:18:42 niklas Exp $	*/

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
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/encap.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

#include "sysdep.h"

#ifdef NEED_SYSDEP_APP
#include "app.h"
#include "conf.h"
#include "ipsec.h"
#endif NEED_SYSDEP_APP
#include "log.h"

#ifdef USE_PF_KEY_V2
#include "pf_key_v2.h"
#define KEY_API(x) pf_key_v2_##x
#else
#include "pf_encap.h"
#define KEY_API(x) pf_encap_##x
#endif

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
  char *conf, *doi_str, *local_id, *remote_id, *peer, *address;
  struct in_addr laddr, lmask, raddr, rmask, gwaddr;
  int lid, rid;

  conns = conf_get_list ("Phase 2", "Connections");
  for (conn = TAILQ_FIRST (&conns->fields); conn;
       conn = TAILQ_NEXT (conn, link))
    {
      /* Figure out the DOI.  We only handle IPsec so far.  */
      conf = conf_get_str (conn->field, "Configuration");
      if (!conf)
	{
	  log_print ("sysdep_conf_init_hook: "
		     "No \"Configuration\" specified for %s",
		     conn->field);
	  continue;
	}
      doi_str = conf_get_str (conf, "DOI");
      if (!doi_str)
	{
	  log_print ("sysdep_conf_init_hook: No DOI specified for %s", conf);
	  continue;
	}
      if (strcasecmp (doi_str, "IPSEC") != 0)
	{
	  log_print ("sysdep_conf_init_hook: DOI \"%s\" unsupported", doi_str);
	  continue;
	}

      local_id = conf_get_str (conn->field, "Local-ID");
      remote_id = conf_get_str (conn->field, "Remote-ID");

      /*
       * At the moment I only do on-demand keying for modes with client IDs.
       */
      if (!local_id || !remote_id)
	{
	  log_print ("sysdep_conf_init_hook: "
		     "Both Local-ID and Remote-ID required for %s",
		     conn->field);
	  continue;
	}

      if (ipsec_get_id (local_id, &lid, &laddr, &lmask))
	continue;
      if (ipsec_get_id (remote_id, &rid, &raddr, &rmask))
	continue;

      peer = conf_get_str (conn->field, "ISAKMP-peer");
      if (!peer)
	{
	  log_print ("sysdep_conf_init_hook: "
		     "section %s has no \"ISAKMP-peer\" tag", conn->field);
	  continue;
	}
      address = conf_get_str (peer, "Address");
      if (!address)
	{
	  log_print ("sysdep_conf_init_hook: "
		     "section %s has no \"Address\" tag", peer);
	  continue;
	}
      if (!inet_aton (address, &gwaddr))
	{
	  log_print ("sysdep_conf_init_hook: invalid adress %s in section %s",
		     address, peer);
	  continue;
	}

      /* XXX The special SPI below needs to be symbolic.  */
      if (KEY_API(route) (laddr.s_addr, lmask.s_addr, raddr.s_addr,
			  rmask.s_addr, 1, gwaddr.s_addr, conn->field))
	/* XXX What else?  */
	continue;
    }
}

/*
 * Generate a SPI for protocol PROTO and the destination signified by
 * ID & ID_SZ.  Stash the SPI size in SZ.
 */
u_int8_t *
sysdep_ipsec_get_spi (size_t *sz, u_int8_t proto, void *id, size_t id_sz)
{
  if (app_none)
    {
      *sz = IPSEC_SPI_SIZE;
      /* XXX should be random instead I think.  */
      return strdup ("\x12\x34\x56\x78");
    }
  return KEY_API (get_spi) (sz, proto, id, id_sz);
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
sysdep_ipsec_delete_spi (struct sa *sa, struct proto *proto, int initiator)
{
  if (app_none)
    return 0;
  return KEY_API (delete_spi) (sa, proto, initiator);
}

int
sysdep_ipsec_enable_sa (struct sa *sa, int initiator)
{
  if (app_none)
    return 0;
  return KEY_API (enable_sa) (sa, initiator);
}

int
sysdep_ipsec_group_spis (struct sa *sa, struct proto *proto1,
		      struct proto *proto2, int role)
{
  if (app_none)
    return 0;
  return KEY_API (group_spis) (sa, proto1, proto2, role);
}

int
sysdep_ipsec_set_spi (struct sa *sa, struct proto *proto, int role,
		      int initiator)
{
  if (app_none)
    return 0;
  return KEY_API (set_spi) (sa, proto, role, initiator);
}
#endif
