/*	$OpenBSD: pf_encap.h,v 1.6 1999/04/05 21:03:01 niklas Exp $	*/
/*	$EOM: pf_encap.h,v 1.11 1999/04/02 00:51:32 niklas Exp $	*/

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

#ifndef _PF_ENCAP_H_
#define _PF_ENCAP_H_

#include <sys/types.h>
#include <sys/queue.h>

struct proto;
struct sa;
struct sockaddr;

struct pf_encap_node {
  /* Link to next node.  */
  TAILQ_ENTRY (pf_encap_node) link;

  /* The message itself.  */
  struct encap_msghdr *emsg;

  /* The callback function and its argument.  */
  void (*callback) (void *);
  void *arg;
};

extern int pf_encap_connection (char *);
extern int pf_encap_delete_spi (struct sa *, struct proto *, int);
extern int pf_encap_enable_sa (struct sa *);
extern int pf_encap_enable_spi (in_addr_t, in_addr_t, in_addr_t, in_addr_t,
				u_int8_t *, u_int8_t, in_addr_t);
extern u_int8_t *pf_encap_get_spi (size_t *, u_int8_t, struct sockaddr *, int,
				   struct sockaddr *, int);
extern int pf_encap_group_spis (struct sa *, struct proto *, struct proto *,
				int);
extern void pf_encap_handler (int);
extern int pf_encap_open (void);
extern int pf_encap_set_spi (struct sa *, struct proto *, int);

#endif /* _PF_ENCAP_H_ */
