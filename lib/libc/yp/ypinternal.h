/*	$OpenBSD: ypinternal.h,v 1.5 2002/07/20 01:35:35 deraadt Exp $	 */

/*
 * Copyright (c) 1992, 1993, 1996 Theo de Raadt <deraadt@theos.com>
 * All rights reserved.
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
 *	This product includes software developed by Theo de Raadt.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * XXX  We have to define these here due to clashes between
 * yp_prot.h and yp.h.
 */
struct dom_binding {
	struct dom_binding *dom_pnext;
	char dom_domain[YPMAXDOMAIN + 1];
	struct sockaddr_in dom_server_addr;
	u_short dom_server_port;
	int dom_socket;
	CLIENT *dom_client;
	u_short dom_local_port;
	long dom_vers;
};

#define BINDINGDIR	"/var/yp/binding"
#define YPBINDLOCK	"/var/run/ypbind.lock"

int (*ypresp_allfn)(u_long, char *, int, char *, int, void *);
void *ypresp_data;

extern struct dom_binding *_ypbindlist;
extern char _yp_domain[MAXHOSTNAMELEN];
extern int _yplib_timeout;

void _yp_unbind(struct dom_binding *);
int _yp_check(char **);

#ifdef YPMATCHCACHE

static bool_t ypmatch_add(const char *, const char *, u_int, char *, u_int);
static bool_t ypmatch_find(const char *, const char *, u_int, char **, u_int *);

static struct ypmatch_ent {
	struct ypmatch_ent	*next;
	char			*map, *key;
	char			*val;
	int			 keylen, vallen;
	time_t			 expire_t;
} *ypmc;
extern int _yplib_cache;

#endif
