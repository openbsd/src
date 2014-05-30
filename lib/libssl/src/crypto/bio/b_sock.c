/* crypto/bio/b_sock.c */
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

#include <netinet/in.h>

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <openssl/bio.h>
#include <openssl/dso.h>

#include "cryptlib.h"

#define SOCKET_PROTOCOL IPPROTO_TCP

#ifdef SO_MAXCONN
#define MAX_LISTEN  SO_MAXCONN
#elif defined(SOMAXCONN)
#define MAX_LISTEN  SOMAXCONN
#else
#define MAX_LISTEN  32
#endif

static int get_ip(const char *str, unsigned char *ip);

int
BIO_get_host_ip(const char *str, unsigned char *ip)
{
	int i;
	int err = 1;
	int locked = 0;
	struct hostent *he;

	i = get_ip(str, ip);
	if (i < 0) {
		BIOerr(BIO_F_BIO_GET_HOST_IP, BIO_R_INVALID_IP_ADDRESS);
		goto err;
	}

	/* At this point, we have something that is most probably correct
	   in some way, so let's init the socket. */
	if (BIO_sock_init() != 1)
		return 0; /* don't generate another error code here */

	/* If the string actually contained an IP address, we need not do
	   anything more */
	if (i > 0)
		return (1);

	/* do a gethostbyname */
	CRYPTO_w_lock(CRYPTO_LOCK_GETHOSTBYNAME);
	locked = 1;
	he = BIO_gethostbyname(str);
	if (he == NULL) {
		BIOerr(BIO_F_BIO_GET_HOST_IP, BIO_R_BAD_HOSTNAME_LOOKUP);
		goto err;
	}

	/* cast to short because of win16 winsock definition */
	if ((short)he->h_addrtype != AF_INET) {
		BIOerr(BIO_F_BIO_GET_HOST_IP, BIO_R_GETHOSTBYNAME_ADDR_IS_NOT_AF_INET);
		goto err;
	}
	for (i = 0; i < 4; i++)
		ip[i] = he->h_addr_list[0][i];
	err = 0;

err:
	if (locked)
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
	int i;
	struct servent *s;

	if (str == NULL) {
		BIOerr(BIO_F_BIO_GET_PORT, BIO_R_NO_PORT_DEFINED);
		return (0);
	}
	i = atoi(str);
	if (i != 0)
		*port_ptr = (unsigned short)i;
	else {
		CRYPTO_w_lock(CRYPTO_LOCK_GETSERVBYNAME);
		s = getservbyname(str, "tcp");
		if (s != NULL)
			*port_ptr = ntohs((unsigned short)s->s_port);
		CRYPTO_w_unlock(CRYPTO_LOCK_GETSERVBYNAME);
		if (s == NULL) {
			if (strcmp(str, "http") == 0)
				*port_ptr = 80;
			else if (strcmp(str, "telnet") == 0)
				*port_ptr = 23;
			else if (strcmp(str, "socks") == 0)
				*port_ptr = 1080;
			else if (strcmp(str, "https") == 0)
				*port_ptr = 443;
			else if (strcmp(str, "ssl") == 0)
				*port_ptr = 443;
			else if (strcmp(str, "ftp") == 0)
				*port_ptr = 21;
			else if (strcmp(str, "gopher") == 0)
				*port_ptr = 70;
			else {
				SYSerr(SYS_F_GETSERVBYNAME, errno);
				ERR_asprintf_error_data("service='%s'", str);
				return (0);
			}
		}
	}
	return (1);
}

int
BIO_sock_error(int sock)
{
	int j, i;
	int size;

	size = sizeof(int);
	/* Note: under Windows the third parameter is of type (char *)
	 * whereas under other systems it is (void *) if you don't have
	 * a cast it will choke the compiler: if you do have a cast then
	 * you can either go for (char *) or (void *).
	 */
	i = getsockopt(sock, SOL_SOCKET, SO_ERROR, (void *)&j, (void *)&size);
	if (i < 0)
		return (1);
	else
		return (j);
}

struct hostent *
BIO_gethostbyname(const char *name)
{
	return gethostbyname(name);
}


int
BIO_sock_init(void)
{
	return (1);
}

void
BIO_sock_cleanup(void)
{
}

int
BIO_socket_ioctl(int fd, long type, void *arg)
{
	int i;

#  define ARG arg

	i = ioctl(fd, type, ARG);
	if (i < 0)
		SYSerr(SYS_F_IOCTLSOCKET, errno);
	return (i);
}

/* The reason I have implemented this instead of using sscanf is because
 * Visual C 1.52c gives an unresolved external when linking a DLL :-( */
static int
get_ip(const char *str, unsigned char ip[4])
{
	unsigned int tmp[4];
	int num = 0, c, ok = 0;

	tmp[0] = tmp[1] = tmp[2] = tmp[3] = 0;

	for (;;) {
		c= *(str++);
		if ((c >= '0') && (c <= '9')) {
			ok = 1;
			tmp[num] = tmp[num]*10 + c-'0';
			if (tmp[num] > 255)
				return (0);
		} else if (c == '.') {
			if (!ok)
				return (-1);
			if (num == 3)
				return (0);
			num++;
			ok = 0;
		} else if (c == '\0' && (num == 3) && ok)
			break;
		else
			return (0);
	}
	ip[0] = tmp[0];
	ip[1] = tmp[1];
	ip[2] = tmp[2];
	ip[3] = tmp[3];
	return (1);
}

int
BIO_get_accept_socket(char *host, int bind_mode)
{
	int ret = 0;
	union {
		struct sockaddr sa;
		struct sockaddr_in sa_in;
		struct sockaddr_in6 sa_in6;
	} server, client;
	int s = -1, cs, addrlen;
	unsigned char ip[4];
	unsigned short port;
	char *str = NULL, *e;
	char *h, *p;
	unsigned long l;
	int err_num;

	if (BIO_sock_init() != 1)
		return (-1);

	if ((str = BUF_strdup(host)) == NULL)
		return (-1);

	h = p = NULL;
	h = str;
	for (e = str; *e; e++) {
		if (*e == ':') {
			p = e;
		} else if (*e == '/') {
			*e = '\0';
			break;
		}
	}
	if (p)
		*p++='\0';	/* points at last ':', '::port' is special [see below] */
	else
		p = h, h = NULL;

#ifdef EAI_FAMILY
	do {
		static union {
			void *p;
			int (*f)(const char *, const char *,
			    const struct addrinfo *,
			    struct addrinfo **);
		} p_getaddrinfo = {NULL};
		static union {
			void *p;
			void (*f)(struct addrinfo *);
		} p_freeaddrinfo = {NULL};
		struct addrinfo *res, hint;

		if (p_getaddrinfo.p == NULL) {
			if ((p_getaddrinfo.p = DSO_global_lookup("getaddrinfo"))==NULL ||
			    (p_freeaddrinfo.p = DSO_global_lookup("freeaddrinfo"))==NULL)
				p_getaddrinfo.p = (void*) - 1;
		}
		if (p_getaddrinfo.p == (void *) - 1)
			break;

		/* '::port' enforces IPv6 wildcard listener. Some OSes,
		 * e.g. Solaris, default to IPv6 without any hint. Also
		 * note that commonly IPv6 wildchard socket can service
		 * IPv4 connections just as well...  */
		memset(&hint, 0, sizeof(hint));
		hint.ai_flags = AI_PASSIVE;
		if (h) {
			if (strchr(h, ':')) {
				if (h[1] == '\0')
					h = NULL;
				hint.ai_family = AF_INET6;
			} else if (h[0] == '*' && h[1] == '\0') {
				hint.ai_family = AF_INET;
				h = NULL;
			}
		}

		if ((*p_getaddrinfo.f)(h, p, &hint, &res))
			break;

		addrlen = res->ai_addrlen <= sizeof(server) ?
		    res->ai_addrlen : sizeof(server);
		memcpy(&server, res->ai_addr, addrlen);

		(*p_freeaddrinfo.f)(res);
		goto again;
	} while (0);
#endif

	if (!BIO_get_port(p, &port))
		goto err;

	memset((char *)&server, 0, sizeof(server));
	server.sa_in.sin_family = AF_INET;
	server.sa_in.sin_port = htons(port);
	addrlen = sizeof(server.sa_in);

	if (h == NULL || strcmp(h, "*") == 0)
		server.sa_in.sin_addr.s_addr = INADDR_ANY;
	else {
		if (!BIO_get_host_ip(h, &(ip[0])))
			goto err;
		l = (unsigned long)((unsigned long)ip[0]<<24L)|
		    ((unsigned long)ip[1]<<16L)|
		    ((unsigned long)ip[2]<< 8L)|
		    ((unsigned long)ip[3]);
		server.sa_in.sin_addr.s_addr = htonl(l);
	}

again:
	s = socket(server.sa.sa_family, SOCK_STREAM, SOCKET_PROTOCOL);
	if (s == -1) {
		SYSerr(SYS_F_SOCKET, errno);
		ERR_asprintf_error_data("port='%s'", host);
		BIOerr(BIO_F_BIO_GET_ACCEPT_SOCKET, BIO_R_UNABLE_TO_CREATE_SOCKET);
		goto err;
	}

#ifdef SO_REUSEADDR
	if (bind_mode == BIO_BIND_REUSEADDR) {
		int i = 1;

		ret = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&i, sizeof(i));
		bind_mode = BIO_BIND_NORMAL;
	}
#endif
	if (bind(s, &server.sa, addrlen) == -1) {
#ifdef SO_REUSEADDR
		err_num = errno;
		if ((bind_mode == BIO_BIND_REUSEADDR_IF_UNUSED) &&
		    (err_num == EADDRINUSE)) {
			client = server;
			if (h == NULL || strcmp(h, "*") == 0) {
				if (client.sa.sa_family == AF_INET6) {
					memset(&client.sa_in6.sin6_addr, 0, sizeof(client.sa_in6.sin6_addr));
					client.sa_in6.sin6_addr.s6_addr[15] = 1;
				} else if (client.sa.sa_family == AF_INET) {
					client.sa_in.sin_addr.s_addr = htonl(0x7F000001);
				} else
					goto err;
			}
			cs = socket(client.sa.sa_family, SOCK_STREAM, SOCKET_PROTOCOL);
			if (cs != -1) {
				int ii;
				ii = connect(cs, &client.sa, addrlen);
				close(cs);
				if (ii == -1) {
					bind_mode = BIO_BIND_REUSEADDR;
					close(s);
					goto again;
				}
				/* else error */
			}
			/* else error */
		}
#endif
		SYSerr(SYS_F_BIND, err_num);
		ERR_asprintf_error_data("port='%s'", host);
		BIOerr(BIO_F_BIO_GET_ACCEPT_SOCKET, BIO_R_UNABLE_TO_BIND_SOCKET);
		goto err;
	}
	if (listen(s, MAX_LISTEN) == -1) {
		SYSerr(SYS_F_BIND, errno);
		ERR_asprintf_error_data("port='%s'", host);
		BIOerr(BIO_F_BIO_GET_ACCEPT_SOCKET, BIO_R_UNABLE_TO_LISTEN_SOCKET);
		goto err;
	}
	ret = 1;
err:
	free(str);
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
		SYSerr(SYS_F_ACCEPT, errno);
		BIOerr(BIO_F_BIO_ACCEPT, BIO_R_ACCEPT_ERROR);
		goto end;
	}

	if (addr == NULL)
		goto end;

#ifdef EAI_FAMILY
	do {
		char   h[NI_MAXHOST], s[NI_MAXSERV];
		size_t nl;
		static union {
			void *p;
			int (*f)(const struct sockaddr *,
			socklen_t, char *, size_t,
			    char *, size_t, int);
		} p_getnameinfo = {NULL};
		/* 2nd argument to getnameinfo is specified to
		 * be socklen_t. Unfortunately there is a number
		 * of environments where socklen_t is not defined.
		 * As it's passed by value, it's safe to pass it
		 * as size_t... <appro> */

		if (p_getnameinfo.p == NULL) {
			if ((p_getnameinfo.p = DSO_global_lookup("getnameinfo")) == NULL)
				p_getnameinfo.p = (void*) - 1;
		}
		if (p_getnameinfo.p == (void *) - 1)
			break;

		if ((*p_getnameinfo.f)(&sa.from.sa, sa.len, h, sizeof(h),
		    s, sizeof(s), NI_NUMERICHOST|NI_NUMERICSERV))
			break;
		nl = strlen(h) + strlen(s) + 2;
		p = *addr;
		if (p) {
			*p = '\0';
			if (!(tmp = realloc(p, nl))) {
				close(ret);
				ret = -1;
				free(p);
				*addr = NULL;
				BIOerr(BIO_F_BIO_ACCEPT, ERR_R_MALLOC_FAILURE);
				goto end;
			}
			p = tmp;
		} else {
			p = malloc(nl);
		}
		if (p == NULL) {
			close(ret);
			ret = -1;
			BIOerr(BIO_F_BIO_ACCEPT, ERR_R_MALLOC_FAILURE);
			goto end;
		}
		*addr = p;
		snprintf(*addr, nl, "%s:%s", h, s);
		goto end;
	} while (0);
#endif
	if (sa.from.sa.sa_family != AF_INET)
		goto end;
	l = ntohl(sa.from.sa_in.sin_addr.s_addr);
	port = ntohs(sa.from.sa_in.sin_port);
	if (*addr == NULL) {
		if ((p = malloc(24)) == NULL) {
			close(ret);
			ret = -1;
			BIOerr(BIO_F_BIO_ACCEPT, ERR_R_MALLOC_FAILURE);
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
	int ret = 0;
#if defined(TCP_NODELAY) && (defined(IPPROTO_TCP) || defined(SOL_TCP))
	int opt;

#ifdef SOL_TCP
	opt = SOL_TCP;
#else
#ifdef IPPROTO_TCP
	opt = IPPROTO_TCP;
#endif
#endif

	ret = setsockopt(s, opt, TCP_NODELAY, (char *)&on, sizeof(on));
#endif
	return (ret == 0);
}

int
BIO_socket_nbio(int s, int mode)
{
	int ret = -1;
	int l;

	l = mode;
	ret = BIO_socket_ioctl(s, FIONBIO, &l);

	return (ret == 0);
}
