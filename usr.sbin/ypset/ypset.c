/*	$OpenBSD: ypset.c,v 1.3 1996/05/22 12:13:01 deraadt Exp $ */
/*	$NetBSD: ypset.c,v 1.8 1996/05/13 02:46:33 thorpej Exp $	*/

/*
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
 * 4. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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

#ifndef LINT
static char rcsid[] = "$OpenBSD: ypset.c,v 1.3 1996/05/22 12:13:01 deraadt Exp $";
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netdb.h>
#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp.h>
#include <rpcsvc/ypclnt.h>
#include <arpa/inet.h>

extern bool_t xdr_domainname();

usage()
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "\typset [-h host ] [-d domain] server\n");
	exit(1);
}

bind_tohost(sin, dom, server)
struct sockaddr_in *sin;
char *dom, *server;
{
	struct ypbind_setdom ypsd;
	struct timeval tv;
	struct hostent *hp;
	CLIENT *client;
	int sock, port;
	int r;
	struct in_addr iaddr;
	
	if( (port=htons(getrpcport(server, YPPROG, YPPROC_NULL, IPPROTO_UDP))) == 0) {
		fprintf(stderr, "%s not running ypserv.\n", server);
		exit(1);
	}

	bzero(&ypsd, sizeof ypsd);

	if (inet_aton(server, &iaddr) == 0) {
		hp = gethostbyname(server);
		if (hp == NULL) {
			fprintf(stderr, "ypset: can't find address for %s\n", server);
			exit(1);
		}
		bcopy(hp->h_addr, &iaddr, sizeof(iaddr));
	}
	ypsd.ypsetdom_domain = dom;
	bcopy(&iaddr, &ypsd.ypsetdom_binding.ypbind_binding_addr,
	    sizeof(ypsd.ypsetdom_binding.ypbind_binding_addr));
	bcopy(&port, &ypsd.ypsetdom_binding.ypbind_binding_port,
	    sizeof(ypsd.ypsetdom_binding.ypbind_binding_port));
	ypsd.ypsetdom_vers = YPVERS;
	
	tv.tv_sec = 15;
	tv.tv_usec = 0;
	sock = RPC_ANYSOCK;
	client = clntudp_create(sin, YPBINDPROG, YPBINDVERS, tv, &sock);
	if (client==NULL) {
		fprintf(stderr, "can't yp_bind: Reason: %s\n",
			yperr_string(YPERR_YPBIND));
		return YPERR_YPBIND;
	}
	client->cl_auth = authunix_create_default();

	r = clnt_call(client, YPBINDPROC_SETDOM,
		xdr_ypbind_setdom, &ypsd, xdr_void, NULL, tv);
	if(r) {
		fprintf(stderr, "Sorry, cannot ypset for domain %s on host.\n", dom);
		clnt_destroy(client);
		return YPERR_YPBIND;
	}
	clnt_destroy(client);
	return 0;
}

int
main(argc, argv)
char **argv;
{
	struct sockaddr_in sin;
	struct hostent *hent;
	extern char *optarg;
	extern int optind;
	char *domainname;
	int c;

	yp_get_default_domain(&domainname);

	bzero(&sin, sizeof sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(0x7f000001);

	while( (c=getopt(argc, argv, "h:d:")) != -1)
		switch(c) {
		case 'd':
			domainname = optarg;
			break;
		case 'h':
			if (inet_aton(optarg, &sin.sin_addr) == 0) {
				hent = gethostbyname(optarg);
				if (hent == NULL) {
					fprintf(stderr, "ypset: host %s unknown\n",
					    optarg);
					exit(1);
				}
				bcopy(&hent->h_addr, &sin.sin_addr,
				    sizeof(sin.sin_addr));
			}
			break;
		default:
			usage();
		}

	if(optind + 1 != argc )
		usage();

	if (bind_tohost(&sin, domainname, argv[optind]))
		exit(1);
	exit(0);
}
