/* $OpenBSD: b_sock.c,v 1.64 2017/04/30 04:18:58 beck Exp $ */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/err.h>

int
BIO_get_host_ip(const char *str, unsigned char *ip)
{
	int i;
	int err = 1;
	struct hostent *he;

	if (inet_pton(AF_INET, str, ip) == 1)
		return (1);

	/* do a gethostbyname */
	CRYPTO_w_lock(CRYPTO_LOCK_GETHOSTBYNAME);
	he = BIO_gethostbyname(str);
	if (he == NULL) {
		BIOerror(BIO_R_BAD_HOSTNAME_LOOKUP);
		goto err;
	}

	if (he->h_addrtype != AF_INET) {
		BIOerror(BIO_R_GETHOSTBYNAME_ADDR_IS_NOT_AF_INET);
		goto err;
	}
	for (i = 0; i < 4; i++)
		ip[i] = he->h_addr_list[0][i];
	err = 0;

err:
	CRYPTO_w_unlock(CRYPTO_LOCK_GETHOSTBYNAME);
	if (err) {
		ERR_asprintf_error_data("host=%s", str);
		return 0;
	} else
		return 1;
}

int
BIO_get_port(const char *str, unsigned short *port_ptr)
{
	struct addrinfo *res = NULL;
	struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
		.ai_flags = AI_PASSIVE,
	};
	int error;

	if (str == NULL) {
		BIOerror(BIO_R_NO_PORT_SPECIFIED);
		return (0);
	}

	if ((error = getaddrinfo(NULL, str, &hints, &res)) != 0) {
		ERR_asprintf_error_data("getaddrinfo: service='%s' : %s'", str,
		    gai_strerror(error));
		return (0);
	}
	*port_ptr = ntohs(((struct sockaddr_in *)(res->ai_addr))->sin_port);
	freeaddrinfo(res);
	return (1);
}

int
BIO_sock_error(int sock)
{
	socklen_t len;
	int err;

	len = sizeof(err);
	if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len) != 0)
		return (1);
	return (err);
}

struct hostent *
BIO_gethostbyname(const char *name)
{
	return gethostbyname(name);
}

int
BIO_socket_ioctl(int fd, long type, void *arg)
{
	int ret;

	ret = ioctl(fd, type, arg);
	if (ret < 0)
		SYSerror(errno);
	return (ret);
}

int
BIO_get_accept_socket(char *host, int bind_mode)
{
	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
		.ai_flags = AI_PASSIVE,
	};
	struct addrinfo *res = NULL;
	char *h, *p, *str = NULL;
	int error, ret = 0, s = -1;

	if (host == NULL || (str = strdup(host)) == NULL)
		return (-1);
	p = NULL;
	h = str;
	if ((p = strrchr(str, ':')) == NULL) {
		BIOerror(BIO_R_NO_PORT_SPECIFIED);
		goto err;
	}
	*p++ = '\0';
	if (*p == '\0') {
		BIOerror(BIO_R_NO_PORT_SPECIFIED);
		goto err;
	}
	if (*h == '\0' || strcmp(h, "*") == 0)
		h = NULL;

	if ((error = getaddrinfo(h, p, &hints, &res)) != 0) {
		ERR_asprintf_error_data("getaddrinfo: '%s:%s': %s'", h, p,
		    gai_strerror(error));
		goto err;
	}
	if (h == NULL) {
		struct sockaddr_in *sin = (struct sockaddr_in *)res->ai_addr;
		sin->sin_addr.s_addr = INADDR_ANY;
	}

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == -1) {
		SYSerror(errno);
		ERR_asprintf_error_data("host='%s'", host);
		BIOerror(BIO_R_UNABLE_TO_CREATE_SOCKET);
		goto err;
	}
	if (bind_mode == BIO_BIND_REUSEADDR) {
		int i = 1;

		ret = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));
		bind_mode = BIO_BIND_NORMAL;
	}
	if (bind(s, res->ai_addr, res->ai_addrlen) == -1) {
		SYSerror(errno);
		ERR_asprintf_error_data("host='%s'", host);
		BIOerror(BIO_R_UNABLE_TO_BIND_SOCKET);
		goto err;
	}
	if (listen(s, SOMAXCONN) == -1) {
		SYSerror(errno);
		ERR_asprintf_error_data("host='%s'", host);
		BIOerror(BIO_R_UNABLE_TO_LISTEN_SOCKET);
		goto err;
	}
	ret = 1;

err:
	free(str);
	freeaddrinfo(res);
	if ((ret == 0) && (s != -1)) {
		close(s);
		s = -1;
	}
	return (s);
}

int
BIO_accept(int sock, char **addr)
{
	int ret = -1;
	unsigned long l;
	unsigned short port;
	char *p, *tmp;

	struct {
		socklen_t len;
		union {
			struct sockaddr sa;
			struct sockaddr_in sa_in;
			struct sockaddr_in6 sa_in6;
		} from;
	} sa;

	sa.len = sizeof(sa.from);
	memset(&sa.from, 0, sizeof(sa.from));
	ret = accept(sock, &sa.from.sa, &sa.len);
	if (ret == -1) {
		if (BIO_sock_should_retry(ret))
			return -2;
		SYSerror(errno);
		BIOerror(BIO_R_ACCEPT_ERROR);
		goto end;
	}

	if (addr == NULL)
		goto end;

	do {
		char   h[NI_MAXHOST], s[NI_MAXSERV];
		size_t nl;

		if (getnameinfo(&sa.from.sa, sa.len, h, sizeof(h),
		    s, sizeof(s), NI_NUMERICHOST|NI_NUMERICSERV))
			break;
		nl = strlen(h) + strlen(s) + 2;
		p = *addr;
		if (p)
			*p = '\0';
		if (!(tmp = realloc(p, nl))) {
			close(ret);
			ret = -1;
			free(p);
			*addr = NULL;
			BIOerror(ERR_R_MALLOC_FAILURE);
			goto end;
		}
		p = tmp;
		*addr = p;
		snprintf(*addr, nl, "%s:%s", h, s);
		goto end;
	} while (0);
	if (sa.from.sa.sa_family != AF_INET)
		goto end;
	l = ntohl(sa.from.sa_in.sin_addr.s_addr);
	port = ntohs(sa.from.sa_in.sin_port);
	if (*addr == NULL) {
		if ((p = malloc(24)) == NULL) {
			close(ret);
			ret = -1;
			BIOerror(ERR_R_MALLOC_FAILURE);
			goto end;
		}
		*addr = p;
	}
	snprintf(*addr, 24, "%d.%d.%d.%d:%d",
	    (unsigned char)(l >> 24L) & 0xff, (unsigned char)(l >> 16L) & 0xff,
	    (unsigned char)(l >> 8L) & 0xff, (unsigned char)(l) & 0xff, port);

end:
	return (ret);
}

int
BIO_set_tcp_ndelay(int s, int on)
{
	return (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) == 0);
}
