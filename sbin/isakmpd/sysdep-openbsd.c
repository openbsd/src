/*	$Id: sysdep-openbsd.c,v 1.1.1.1 1998/11/15 00:03:49 niklas Exp $	*/

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
#include <stdlib.h>
#include <string.h>

#ifdef NEED_SYSDEP_APP
#include "app.h"
#include "ipsec.h"
#include "pf_encap.h"
#endif NEED_SYSDEP_APP
#include "log.h"
#include "sysdep.h"

extern char *__progname;
int regrand = 0;

u_int32_t
sysdep_random ()
{
  if (!regrand)
    return arc4random ();
  else
    return random();
}

char *
sysdep_progname ()
{
  return __progname;
}

/* As regress/ use this file I protect the sysdep_app_* stuff like this.  */
#ifdef NEED_SYSDEP_APP
int
sysdep_app_open ()
{
  return pf_encap_open ();
}

void
sysdep_app_handler (int fd)
{
  pf_encap_handler (fd);
}

u_int8_t *
sysdep_ipsec_get_spi (size_t *sz, u_int8_t proto, void *id, size_t id_sz)
{
  if (app_none)
    {
      *sz = IPSEC_SPI_SIZE;
      /* XXX should be random instead I think.  */
      return strdup ("\x12\x34\x56\x78");
    }
  return pf_encap_get_spi (sz, proto, id, id_sz);
}

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
  return pf_encap_delete_spi (sa, proto, initiator);
}

int
sysdep_ipsec_enable_spi (struct sa *sa, int initiator)
{
  if (app_none)
    return 0;
  return pf_encap_enable_spi (sa, initiator);
}

int
sysdep_ipsec_group_spis (struct sa *sa, struct proto *proto1,
		      struct proto *proto2, int role)
{
  if (app_none)
    return 0;
  return pf_encap_group_spis (sa, proto1, proto2, role);
}

int
sysdep_ipsec_set_spi (struct sa *sa, struct proto *proto, int role,
		      int initiator)
{
  if (app_none)
    return 0;
  return pf_encap_set_spi (sa, proto, role, initiator);
}
#endif
