/*	$OpenBSD: udp.c,v 1.5 1998/12/22 01:46:04 niklas Exp $	*/
/*	$EOM: udp.c,v 1.24 1998/12/22 01:40:23 niklas Exp $	*/

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
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <err.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "conf.h"
#include "if.h"
#include "isakmp.h"
#include "log.h"
#include "message.h"
#include "sysdep.h"
#include "transport.h"
#include "udp.h"
#include "util.h"

#define BACKLOG 16
#define UDP_SIZE 65536

/* XXX IPv4 specific.  */
struct udp_transport {
  struct transport transport;
  struct sockaddr_in src;
  struct sockaddr_in dst;
  int s;
};

static struct transport *udp_clone (struct udp_transport *,
				    struct sockaddr_in *);
static struct transport *udp_create (char *);
static int udp_fd_set (struct transport *, fd_set *);
static int udp_fd_isset (struct transport *, fd_set *);
static void udp_handle_message (struct transport *);
static struct transport *udp_make (struct sockaddr_in *);
static int udp_send_message (struct message *);
static void udp_get_dst (struct transport *, struct sockaddr **, int *);
static void udp_get_src (struct transport *, struct sockaddr **, int *);

struct transport_vtbl udp_transport_vtbl = {
  { 0 }, "udp",
  udp_create,
  udp_fd_set,
  udp_fd_isset,
  udp_handle_message,
  udp_send_message,
  udp_get_dst,
  udp_get_src
};

in_port_t udp_default_port = 0;
in_port_t udp_bind_port = 0;
static int udp_proto;
static struct transport *default_transport;

/* Create a UDP transport structure bound to LADDR just for listening.  */
static struct transport *
udp_make (struct sockaddr_in *laddr)
{
  struct udp_transport *t = 0;
  int s;
  int on;

  t = malloc (sizeof *t);
  if (!t)
    {
      log_print ("udp_make: malloc (%d) failed", sizeof *t);
      return 0;
    }

  s = socket (AF_INET, SOCK_DGRAM, udp_proto);
  if (s == -1)
    {
      log_error ("udp_make: socket (%d, %d, %d)", AF_INET, SOCK_DGRAM,
		 udp_proto);
      goto err;
    }

  /* Make sure we don't get our traffic encrypted.  */
  sysdep_cleartext (s);

  on = 1;
  /* XXX Is this redundant considering SO_REUSEPORT below?  */
  if (setsockopt (s, SOL_SOCKET, SO_REUSEADDR, (void *)&on, sizeof on) == -1)
    {
      log_error ("udp_make: setsockopt (%d, %d, %d, %p, %d)", s, SOL_SOCKET,
		 SO_REUSEADDR, &on, sizeof on);
      goto err;
    }

  t->transport.vtbl = &udp_transport_vtbl;
  memcpy (&t->src, laddr, sizeof t->src);
  if (bind (s, (struct sockaddr *)&t->src, sizeof t->src))
    {
      log_error ("udp_make: bind (%d, %p, %d)", s, &t->src, sizeof t->src);
      goto err;
    }

  memset (&t->dst, 0, sizeof t->dst);
  t->s = s;
  t->transport.flags |= TRANSPORT_LISTEN;

  transport_add ((struct transport *)t);
  return (struct transport *)t;

err:
  if (s != -1)
    close (s);
  if (t)
    free (t);
  return 0;
}

/* Clone a listen transport U, record a destination RADDR for outbound use.  */
static struct transport *
udp_clone (struct udp_transport *u, struct sockaddr_in *raddr)
{
  struct transport *t;
  struct udp_transport *u2;

  t = malloc (sizeof *u);
  if (!t)
    return 0;
  u2 = (struct udp_transport *)t;

  memcpy (t, u, sizeof *u);
  memcpy (&u2->dst, raddr, sizeof u2->dst);
  t->flags &= ~TRANSPORT_LISTEN;

  transport_add (t);

  return t;
}

/*
 * Initialize an object of the UDP transport class.  Fill in the local
 * IP address and port information and create a server socket bound to
 * that specific port.  Add the polymorphic transport structure to the
 * system-wide pools of known ISAKMP transports.
 */
static struct transport *
udp_bind (in_addr_t addr, in_port_t port)
{
  struct sockaddr_in src;

  memset (&src, 0, sizeof src);
  src.sin_len = sizeof src;
  src.sin_family = AF_INET;
  src.sin_addr.s_addr = addr;
  src.sin_port = port;
  return udp_make (&src);
}

/*
 * When looking at a specific network interface address, if it's an INET one,
 * create an UDP server socket bound to it.
 */
static void
udp_bind_if (struct ifreq *ifrp, void *arg)
{
  in_port_t port = *(in_port_t *)arg;

  /*
   * Well UDP is an internet protocol after all so drop other ifreqs.
   * XXX IPv6 support is missing.
   */
  if (ifrp->ifr_addr.sa_family != AF_INET
      || ifrp->ifr_addr.sa_len != sizeof (struct sockaddr_in))
    return;

  /* XXX Deal with errors better.  */
  udp_bind (((struct sockaddr_in *)&ifrp->ifr_addr)->sin_addr.s_addr, port);
}

/*
 * NAME is a section name found in the config database.  Setup and return
 * a transport useable to talk to the peer specified by that name
 */
static struct transport *
udp_create (char *name)
{
  struct sockaddr_in dst;
  char *addr_str, *port_str;
  in_addr_t addr;
  in_port_t port;
  char *port_str_end;
  long port_long;
  struct servent *service;

  port_str = conf_get_str (name, "Port");
  if (port_str)
    {
      port_long = strtol (port_str, &port_str_end, 0);
      if (port_str == port_str_end)
	{
	  service = getservbyname (port_str, "udp");
	  if (!service)
	    {
	      log_print ("udp_create: service \"%s\" unknown", port_str);
	      return 0;
	    }
	  port = service->s_port;
	}
      else if (port_long < 1 || port_long > 65535)
	{
	  log_print ("udp_create: port %l out of range", port_long);
	  return 0;
	}
      else
	port = port_long;
    }
  else
    port = UDP_DEFAULT_PORT;

  addr_str = conf_get_str (name, "Address");
  addr = inet_addr (addr_str);
  if (addr == INADDR_NONE)
    {
      log_print ("udp_create: inet_addr (\"%s\") failed", addr_str);
      return 0;
    }

  memset (&dst, 0, sizeof dst);
  dst.sin_len = sizeof dst;
  dst.sin_family = AF_INET;
  dst.sin_addr.s_addr = addr;
  dst.sin_port = htons (port);

  return udp_clone ((struct udp_transport *)default_transport, &dst);
}

/*
 * Find out the magic numbers for the UDP protocol as well as the UDP port
 * to use.  Setup an UDP server for each address of this machine, and one
 * for the generic case when we are the initiator.
 */
void
udp_init ()
{
  struct protoent *p;
  struct servent *s;
  in_port_t port;

  /* Initialize the protocol and port numbers.  */
  p = getprotobyname ("udp");
  udp_proto = p ? p->p_proto : IPPROTO_UDP;
  if (udp_default_port)
    port = htons (udp_default_port);
  else
    {
      s = getservbyname("isakmp", "udp");
      port = s ? s->s_port : htons (UDP_DEFAULT_PORT);
    }

  /* Bind the ISAKMP UDP port on all network interfaces we have. */
  /* XXX need to check errors */
  if_map (udp_bind_if, &port);

  /*
   * Bind to INADDR_ANY in case of new addresses popping up.
   * XXX We should use packets coming in on this socket as a signal
   * to reprobe for new interfaces.
   */
  default_transport = udp_bind (INADDR_ANY, port);
  if (!default_transport)
    log_error ("udp_init: could not allocate default ISAKMP UDP port");

  transport_method_add (&udp_transport_vtbl);
}

/*
 * Set transport T's socket in FDS, return a value useable by select(2)
 * as the number of file descriptors to check.
 */
static int
udp_fd_set (struct transport *t, fd_set *fds)
{
  struct udp_transport *u = (struct udp_transport *)t;

  FD_SET (u->s, fds);
  return u->s + 1;
}

/* Check if transport T's socket is set in FDS.  */
static int
udp_fd_isset (struct transport *t, fd_set *fds)
{
  struct udp_transport *u = (struct udp_transport *)t;

  return FD_ISSET (u->s, fds);
}

/*
 * A message has arrived on transport T's socket.  If T is single-ended,
 * clone it into a double-ended transport which we will use from now on.
 * Package the message as we want it and continue processing in the message
 * module.
 * XXX We will be leaking transports unless we kill them after last
 * probable use, i.e. when ISAKMP SA's gets torn down.
 */
static void
udp_handle_message (struct transport *t)
{
  struct udp_transport *u = (struct udp_transport *)t;
  u_int8_t buf[UDP_SIZE];
  struct sockaddr_in from;
  int len = sizeof from;
  ssize_t n;
  struct message *msg;

  n = recvfrom (u->s, buf, UDP_SIZE, 0, (struct sockaddr *)&from, &len);
  if (n == -1)
    {
      log_error ("recvfrom (%d, %p, %d, %d, %p, %p)", u->s, buf, UDP_SIZE, 0,
		 &from, &len);
      return;
    }

  /*
   * Make a specialized UDP transport structure out of the incoming
   * transport and the address information we got from recvfrom(2).
   */
  t = udp_clone (u, &from);
  if (!t)
    /* XXX Should we do more here?  */
    return;

  msg = message_alloc (t, buf, n);
  if (!msg)
    /* XXX Log.  */
    return;
  message_recv (msg);
}

/* Physically send the message MSG over its associated transport.  */
static int
udp_send_message (struct message *msg)
{
  struct udp_transport *u = (struct udp_transport *)msg->transport;
  ssize_t n;
  struct msghdr m;

  /*
   * Sending on connected sockets requires that no destination address is
   * given, or else EISCONN will occur.
   */
  m.msg_name = (caddr_t)&u->dst;
  m.msg_namelen = sizeof u->dst;
  m.msg_iov = msg->iov;
  m.msg_iovlen = msg->iovlen;
  m.msg_control = 0;
  m.msg_controllen = 0;
  m.msg_flags = 0;
  n = sendmsg (u->s, &m, 0);
  if (n == -1)
    {
      log_error ("sendmsg (%d, %p, %d)", u->s, &m, 0);
      return -1;
    }
  return 0;
}

/*
 * Get transport T's peer address and stuff it into the sockaddr pointed
 * to by DST.  Put its length into DST_LEN.
 */
static void
udp_get_dst (struct transport *t, struct sockaddr **dst, int *dst_len)
{
  *dst = (struct sockaddr *)&((struct udp_transport *)t)->dst;
  *dst_len = sizeof ((struct udp_transport *)t)->dst;
}

/*
 * Get transport T's local address and stuff it into the sockaddr pointed
 * to by SRC.  Put its length into SRC_LEN.
 */
static void
udp_get_src (struct transport *t, struct sockaddr **src, int *src_len)
{
  *src = (struct sockaddr *)&((struct udp_transport *)t)->src;
  *src_len = sizeof ((struct udp_transport *)t)->src;
}
