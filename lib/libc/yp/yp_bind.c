/*
 * Copyright (c) 1996 Theo de Raadt <deraadt@theos.com>
 * Copyright (c) 1992, 1993 Theo de Raadt <deraadt@theos.com>
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

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$OpenBSD: yp_bind.c,v 1.13 2002/07/20 01:35:34 deraadt Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp.h>
#include <rpcsvc/ypclnt.h>
#include "ypinternal.h"

struct dom_binding *_ypbindlist;
char _yp_domain[MAXHOSTNAMELEN];
int _yplib_timeout = 10;

int
_yp_dobind(const char *dom, struct dom_binding **ypdb)
{
	static pid_t	pid = -1;
	char            path[MAXPATHLEN];
	struct dom_binding *ysd, *ysd2;
	struct ypbind_resp ypbr;
	struct timeval  tv;
	struct sockaddr_in clnt_sin;
	struct ypbind_binding *bn;
	int             clnt_sock, fd;
	pid_t		gpid;
	CLIENT         *client;
	int             new = 0, r;
	int             count = 0;
	u_short		port;

	/*
	 * test if YP is running or not
	 */
	if ((fd = open(YPBINDLOCK, O_RDONLY)) == -1)
		return YPERR_YPBIND;
	if (!(flock(fd, LOCK_EX | LOCK_NB) == -1 && errno == EWOULDBLOCK)) {
		(void)close(fd);
		return YPERR_YPBIND;
	}
	(void)close(fd);

	gpid = getpid();
	if (!(pid == -1 || pid == gpid)) {
		ysd = _ypbindlist;
		while (ysd) {
			if (ysd->dom_client)
				clnt_destroy(ysd->dom_client);
			ysd2 = ysd->dom_pnext;
			free(ysd);
			ysd = ysd2;
		}
		_ypbindlist = NULL;
	}
	pid = gpid;

	if (ypdb != NULL)
		*ypdb = NULL;

	if (dom == NULL || strlen(dom) == 0)
		return YPERR_BADARGS;

	for (ysd = _ypbindlist; ysd; ysd = ysd->dom_pnext)
		if (strcmp(dom, ysd->dom_domain) == 0)
			break;
	if (ysd == NULL) {
		if ((ysd = malloc(sizeof *ysd)) == NULL)
			return YPERR_YPERR;
		(void)memset(ysd, 0, sizeof *ysd);
		ysd->dom_socket = -1;
		ysd->dom_vers = 0;
		new = 1;
	}
again:
	if (ysd->dom_vers == 0) {
		(void) snprintf(path, sizeof(path), "%s/%s.%d",
		    BINDINGDIR, dom, 2);
		if ((fd = open(path, O_RDONLY)) == -1) {
			/*
			 * no binding file, YP is dead, or not yet fully
			 * alive.
			 */
			goto trynet;
		}
		if (flock(fd, LOCK_EX | LOCK_NB) == -1 &&
		    errno == EWOULDBLOCK) {
			struct iovec    iov[2];
			u_short         ypb_port;

			/*
			 * we fetch the ypbind port number, but do
			 * nothing with it.
			 */
			iov[0].iov_base = (caddr_t) &ypb_port;
			iov[0].iov_len = sizeof ypb_port;
			iov[1].iov_base = (caddr_t) &ypbr;
			iov[1].iov_len = sizeof ypbr;

			r = readv(fd, iov, 2);
			if (r != iov[0].iov_len + iov[1].iov_len) {
				(void)close(fd);
				ysd->dom_vers = -1;
				goto again;
			}
			(void)close(fd);
			goto gotdata;
		} else {
			/* no lock on binding file, YP is dead. */
			(void)close(fd);
			if (new)
				free(ysd);
			return YPERR_YPBIND;
		}
	}
trynet:
	if (ysd->dom_vers == -1 || ysd->dom_vers == 0) {
		(void)memset(&clnt_sin, 0, sizeof clnt_sin);
		clnt_sin.sin_len = sizeof(struct sockaddr_in);
		clnt_sin.sin_family = AF_INET;
		clnt_sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

		clnt_sock = RPC_ANYSOCK;
		client = clnttcp_create(&clnt_sin, YPBINDPROG, YPBINDVERS,
		    &clnt_sock, 0, 0);
		if (client == NULL) {
			clnt_pcreateerror("clnttcp_create");
			if (new)
				free(ysd);
			return YPERR_YPBIND;
		}
		if (ntohs(clnt_sin.sin_port) >= IPPORT_RESERVED ||
		    ntohs(clnt_sin.sin_port) == 20) {
			/*
			 * YP was not running, but someone has registered
			 * ypbind with portmap -- this simply means YP is
			 * not running.
			 */
			clnt_destroy(client);
			if (new)
				free(ysd);
			return YPERR_YPBIND;
		}
		tv.tv_sec = _yplib_timeout;
		tv.tv_usec = 0;
		r = clnt_call(client, YPBINDPROC_DOMAIN, xdr_domainname,
		    &dom, xdr_ypbind_resp, &ypbr, tv);
		if (r != RPC_SUCCESS) {
			if (new == 0 || count)
				fprintf(stderr,
		    "YP server for domain %s not responding, still trying\n",
				    dom);
			count++;
			clnt_destroy(client);
			ysd->dom_vers = -1;
			goto again;
		}
		clnt_destroy(client);
gotdata:
		bn = &ypbr.ypbind_resp_u.ypbind_bindinfo;
		memcpy(&port, &bn->ypbind_binding_port, sizeof port);
		if (ntohs(port) >= IPPORT_RESERVED ||
		    ntohs(port) == 20) {
			/*
			 * This is bullshit -- the ypbind wants me to
			 * communicate to an insecure ypserv.  We are
			 * within rights to syslog this as an attack,
			 * but for now we'll simply ignore it; real YP
			 * is obviously not running.
			 */
			if (new)
				free(ysd);
			return YPERR_YPBIND;
		}
		(void)memset(&ysd->dom_server_addr, 0,
		    sizeof ysd->dom_server_addr);
		ysd->dom_server_addr.sin_len = sizeof(struct sockaddr_in);
		ysd->dom_server_addr.sin_family = AF_INET;
		memcpy(&ysd->dom_server_addr.sin_port,
		    &bn->ypbind_binding_port,
		    sizeof(ysd->dom_server_addr.sin_port));
		memcpy(&ysd->dom_server_addr.sin_addr.s_addr,
		    &bn->ypbind_binding_addr,
		    sizeof(ysd->dom_server_addr.sin_addr.s_addr));
		ysd->dom_server_port = ysd->dom_server_addr.sin_port;
		ysd->dom_vers = YPVERS;
		strlcpy(ysd->dom_domain, dom, sizeof ysd->dom_domain);
	}
	tv.tv_sec = _yplib_timeout / 2;
	tv.tv_usec = 0;
	if (ysd->dom_client)
		clnt_destroy(ysd->dom_client);
	ysd->dom_socket = RPC_ANYSOCK;
	ysd->dom_client = clntudp_create(&ysd->dom_server_addr,
	    YPPROG, YPVERS, tv, &ysd->dom_socket);
	if (ysd->dom_client == NULL) {
		clnt_pcreateerror("clntudp_create");
		ysd->dom_vers = -1;
		goto again;
	}
	if (fcntl(ysd->dom_socket, F_SETFD, 1) == -1)
		perror("fcntl: F_SETFD");

	if (new) {
		ysd->dom_pnext = _ypbindlist;
		_ypbindlist = ysd;
	}
	if (ypdb != NULL)
		*ypdb = ysd;
	return 0;
}

void
_yp_unbind(struct dom_binding *ypb)
{
	clnt_destroy(ypb->dom_client);
	ypb->dom_client = NULL;
	ypb->dom_socket = -1;
}

int
yp_bind(const char *dom)
{
	return _yp_dobind(dom, NULL);
}

void
yp_unbind(const char *dom)
{
	struct dom_binding *ypb, *ypbp;

	ypbp = NULL;
	for (ypb = _ypbindlist; ypb; ypb = ypb->dom_pnext) {
		if (strcmp(dom, ypb->dom_domain) == 0) {
			clnt_destroy(ypb->dom_client);
			if (ypbp)
				ypbp->dom_pnext = ypb->dom_pnext;
			else
				_ypbindlist = ypb->dom_pnext;
			free(ypb);
			return;
		}
		ypbp = ypb;
	}
}
