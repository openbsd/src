/*	$Id: transport.h,v 1.1.1.1 1998/11/15 00:03:49 niklas Exp $	*/

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

/*
 * The transport module tries to separate out details concerning the
 * actual transferral of ISAKMP messages to other parties.
 */

#ifndef _TRANSPORT_H_
#define _TRANSPORT_H_

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/socket.h>

struct message;
struct transport;

/* This describes a tranport "method" like UDP or similar.  */
struct transport_vtbl {
  /* All transport methods are linked together.  */
  LIST_ENTRY (transport_vtbl) link;

  /* A textual name of the transport method.  */
  char *name;

  /* Create a transport instance of this method.  */
  struct transport *(*create) (char *);

  /* Let the given transport set it's bit in the fd_set passed in.  */
  int (*fd_set) (struct transport *, fd_set *);

  /* Is the given transport ready for I/O?  */
  int (*fd_isset) (struct transport *, fd_set *);

  /*
   * Read a message from the transport's incoming pipe and start
   * handling it.
   */
  void (*handle_message) (struct transport *);

  /* Send a message through the outgoing pipe.  */
  int (*send_message) (struct message *);

  /*
   * Fill out a sockaddr structure with the transport's destination end's
   * address info.  XXX Why not size_t * as last arg?
   */
  void (*get_dst) (struct transport *, struct sockaddr **, int *);

  /*
   * Fill out a sockaddr structure with the transport's source end's
   * address info.  XXX Why not size_t * as last arg?
   */
  void (*get_src) (struct transport *, struct sockaddr **, int *);
};

struct transport {
  /* All transports used are linked together.  */
  LIST_ENTRY (transport) link;

  /* What transport method is this an instance of?  */
  struct transport_vtbl *vtbl;

  /* The queue holding messages to send on this transport.  */
  TAILQ_HEAD (msg_head, message) sendq;

  /* Flags describing the transport.  */
  int flags;
};

/* Set if this is a transport we want to listen on.  */
#define TRANSPORT_LISTEN	1

extern void transport_add (struct transport *);
extern struct transport *transport_create (char *, char *);
extern int transport_fd_set (fd_set *);
extern void transport_handle_messages (fd_set *);
extern void transport_init (void);
extern void transport_map (void (*) (struct transport *));
extern void transport_method_add (struct transport_vtbl *);
extern int transport_pending_wfd_set (fd_set *);
extern void transport_send_messages (fd_set *);

#endif /* _TRANSPORT_H_ */
