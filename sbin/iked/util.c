/*	$OpenBSD: util.c,v 1.11 2011/01/17 17:16:43 mikeb Exp $	*/
/*	$vantronix: util.c,v 1.39 2010/06/02 12:22:58 reyk Exp $	*/

/*
 * Copyright (c) 2010 Reyk Floeter <reyk@vantronix.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <event.h>

#include "iked.h"
#include "ikev2.h"

void
socket_set_blockmode(int fd, enum blockmodes bm)
{
	int	flags;

	if ((flags = fcntl(fd, F_GETFL, 0)) == -1)
		fatal("fcntl F_GETFL");

	if (bm == BM_NONBLOCK)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;

	if ((flags = fcntl(fd, F_SETFL, flags)) == -1)
		fatal("fcntl F_SETFL");
}

int
socket_af(struct sockaddr *sa, in_port_t port)
{
	errno = 0;
	switch (sa->sa_family) {
	case AF_INET:
		((struct sockaddr_in *)sa)->sin_port = port;
		((struct sockaddr_in *)sa)->sin_len =
		    sizeof(struct sockaddr_in);
		break;
	case AF_INET6:
		((struct sockaddr_in6 *)sa)->sin6_port = port;
		((struct sockaddr_in6 *)sa)->sin6_len =
		    sizeof(struct sockaddr_in6);
		break;
	default:
		errno = EPFNOSUPPORT;
		return (-1);
	}

	return (0);
}

in_port_t
socket_getport(struct sockaddr_storage *ss)
{
	switch (ss->ss_family) {
	case AF_INET:
		return (ntohs(((struct sockaddr_in *)ss)->sin_port));
	case AF_INET6:
		return (ntohs(((struct sockaddr_in6 *)ss)->sin6_port));
	default:
		return (0);
	}

	/* NOTREACHED */
	return (0);
}

int
socket_bypass(int s, struct sockaddr *sa)
{
	int	 v, *a;
	int	 a4[] = {
		    IPPROTO_IP,
		    IP_AUTH_LEVEL,
		    IP_ESP_TRANS_LEVEL,
		    IP_ESP_NETWORK_LEVEL,
#ifdef IPV6_IPCOMP_LEVEL
		    IP_IPCOMP_LEVEL
#endif
	};
	int	 a6[] = {
		    IPPROTO_IPV6,
		    IPV6_AUTH_LEVEL,
		    IPV6_ESP_TRANS_LEVEL,
		    IPV6_ESP_NETWORK_LEVEL,
#ifdef IPV6_IPCOMP_LEVEL
		    IPV6_IPCOMP_LEVEL
#endif
	};

	switch (sa->sa_family) {
	case AF_INET:
		a = a4;
		break;
	case AF_INET6:
		a = a6;
		break;
	default:
		log_warn("%s: invalid address family", __func__);
		return (-1);
	}

	v = IPSEC_LEVEL_BYPASS;
	if (setsockopt(s, a[0], a[1], &v, sizeof(v)) == -1) {
		log_warn("%s: AUTH_LEVEL", __func__);
		return (-1);
	}
	if (setsockopt(s, a[0], a[2], &v, sizeof(v)) == -1) {
		log_warn("%s: ESP_TRANS_LEVEL", __func__);
		return (-1);
	}
	if (setsockopt(s, a[0], a[3], &v, sizeof(v)) == -1) {
		log_warn("%s: ESP_NETWORK_LEVEL", __func__);
		return (-1);
	}
#ifdef IP_IPCOMP_LEVEL
	if (setsockopt(s, a[0], a[4], &v, sizeof(v)) == -1) {
		log_warn("%s: IPCOMP_LEVEL", __func__);
		return (-1);
	}
#endif

	return (0);
}

int
udp_bind(struct sockaddr *sa, in_port_t port)
{
	int	 s, val;

	if (socket_af(sa, port) == -1) {
		log_warn("%s: failed to set UDP port", __func__);
		return (-1);
	}

	if ((s = socket(sa->sa_family, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		log_warn("%s: failed to get UDP socket", __func__);
		return (-1);
	}

	/* Skip IPsec processing (don't encrypt) for IKE messages */
	if (socket_bypass(s, sa) == -1) {
		log_warn("%s: failed to bypass IPsec on IKE socket",
		    __func__);
		goto bad;
	}

	val = 1;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(int)) == -1) {
		log_warn("%s: failed to set reuseport", __func__);
		goto bad;
	}
	val = 1;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int)) == -1) {
		log_warn("%s: failed to set reuseaddr", __func__);
		goto bad;
	}

	if (sa->sa_family == AF_INET) {
		val = 1;
		if (setsockopt(s, IPPROTO_IP, IP_RECVDSTADDR,
		    &val, sizeof(int)) == -1) {
			log_warn("%s: failed to set IPv4 packet info",
			    __func__);
			goto bad;
		}
	} else {
		val = 1;
		if (setsockopt(s, IPPROTO_IPV6, IPV6_RECVPKTINFO,
		    &val, sizeof(int)) == -1) {
			log_warn("%s: failed to set IPv6 packet info",
			    __func__);
			goto bad;
		}
	}

	if (bind(s, sa, sa->sa_len) == -1) {
		log_warn("%s: failed to bind UDP socket", __func__);
		goto bad;
	}

	return (s);
 bad:
	close(s);
	return (-1);
}

int
sockaddr_cmp(struct sockaddr *a, struct sockaddr *b, int prefixlen)
{
	struct sockaddr_in	*a4, *b4;
	struct sockaddr_in6	*a6, *b6;
	u_int32_t		 av[4], bv[4], mv[4];

	if (b->sa_family != AF_UNSPEC && (a->sa_family > b->sa_family))
		return (1);
	if (b->sa_family != AF_UNSPEC && (a->sa_family < b->sa_family))
		return (-1);

	if (prefixlen == -1)
		memset(&mv, 0xff, sizeof(mv));

	switch (a->sa_family) {
	case AF_INET:
		a4 = (struct sockaddr_in *)a;
		b4 = (struct sockaddr_in *)b;

		av[0] = a4->sin_addr.s_addr;
		bv[0] = b4->sin_addr.s_addr;
		if (prefixlen != -1)
			mv[0] = prefixlen2mask(prefixlen);

		if ((av[0] & mv[0]) > (bv[0] & mv[0]))
			return (1);
		if ((av[0] & mv[0]) < (bv[0] & mv[0]))
			return (-1);
		break;
	case AF_INET6:
		a6 = (struct sockaddr_in6 *)a;
		b6 = (struct sockaddr_in6 *)b;

		memcpy(&av, &a6->sin6_addr.s6_addr, 16);
		memcpy(&bv, &b6->sin6_addr.s6_addr, 16);
		if (prefixlen != -1)
			prefixlen2mask6(prefixlen, mv);

		if ((av[3] & mv[3]) > (bv[3] & mv[3]))
			return (1);
		if ((av[3] & mv[3]) < (bv[3] & mv[3]))
			return (-1);
		if ((av[2] & mv[2]) > (bv[2] & mv[2]))
			return (1);
		if ((av[2] & mv[2]) < (bv[2] & mv[2]))
			return (-1);
		if ((av[1] & mv[1]) > (bv[1] & mv[1]))
			return (1);
		if ((av[1] & mv[1]) < (bv[1] & mv[1]))
			return (-1);
		if ((av[0] & mv[0]) > (bv[0] & mv[0]))
			return (1);
		if ((av[0] & mv[0]) < (bv[0] & mv[0]))
			return (-1);
		break;
	}

	return (0);
}

ssize_t
recvfromto(int s, void *buf, size_t len, int flags, struct sockaddr *from,
    socklen_t *fromlen, struct sockaddr *to, socklen_t *tolen)
{
	struct iovec		 iov;
	struct msghdr		 msg;
	struct cmsghdr		*cmsg;
	struct in6_pktinfo	*pkt6;
	struct sockaddr_in	*in;
	struct sockaddr_in6	*in6;
	ssize_t			 ret;
	union {
		struct cmsghdr hdr;
		char	buf[CMSG_SPACE(sizeof(struct sockaddr_storage))];
	} cmsgbuf;

	bzero(&msg, sizeof(msg));
	bzero(&cmsgbuf.buf, sizeof(cmsgbuf.buf));

	iov.iov_base = buf;
	iov.iov_len = len;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_name = from;
	msg.msg_namelen = *fromlen;
	msg.msg_control = &cmsgbuf.buf;
	msg.msg_controllen = sizeof(cmsgbuf.buf);

	if ((ret = recvmsg(s, &msg, 0)) == -1)
		return (-1);

	*fromlen = from->sa_len;
	*tolen = 0;

	if (getsockname(s, to, tolen) != 0)
		*tolen = 0;

	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
	    cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		switch (from->sa_family) {
		case AF_INET:
			if (cmsg->cmsg_level == IPPROTO_IP &&
			    cmsg->cmsg_type == IP_RECVDSTADDR) {
				in = (struct sockaddr_in *)to;
				in->sin_family = AF_INET;
				in->sin_len = *tolen = sizeof(*in);
				memcpy(&in->sin_addr, CMSG_DATA(cmsg),
				    sizeof(struct in_addr));
			}
			break;
		case AF_INET6:
			if (cmsg->cmsg_level == IPPROTO_IPV6 &&
			    cmsg->cmsg_type == IPV6_PKTINFO) {
				in6 = (struct sockaddr_in6 *)to;
				in6->sin6_family = AF_INET6;
				in6->sin6_len = *tolen = sizeof(*in6);
				pkt6 = (struct in6_pktinfo *)CMSG_DATA(cmsg);
				memcpy(&in6->sin6_addr, &pkt6->ipi6_addr,
				    sizeof(struct in6_addr));
				if (IN6_IS_ADDR_LINKLOCAL(&in6->sin6_addr))
					in6->sin6_scope_id =
					    pkt6->ipi6_ifindex;
			}
			break;
		}
	}

	return (ret);
}

const char *
print_spi(u_int64_t spi, int size)
{
	static char		 buf[IKED_CYCLE_BUFFERS][32];
	static int		 i = 0;
	char			*ptr;

	ptr = buf[i];

	switch (size) {
	case 4:
		snprintf(ptr, 32, "0x%08x", (u_int32_t)spi);
		break;
	case 8:
		snprintf(ptr, 32, "0x%016llx", spi);
		break;
	default:
		snprintf(ptr, 32, "%llu", spi);
		break;
	}

	if (++i >= IKED_CYCLE_BUFFERS)
		i = 0;

	return (ptr);
}

const char *
print_map(u_int type, struct iked_constmap *map)
{
	u_int			 i;
	static char		 buf[IKED_CYCLE_BUFFERS][32];
	static int		 idx = 0;
	const char		*name = NULL;

	if (idx >= IKED_CYCLE_BUFFERS)
		idx = 0;
	bzero(buf[idx], sizeof(buf[idx]));

	for (i = 0; map[i].cm_name != NULL; i++) {
		if (map[i].cm_type == type)
			name = map[i].cm_name;
	}

	if (name == NULL)
		snprintf(buf[idx], sizeof(buf[idx]), "<UNKNOWN:%u>", type);
	else
		strlcpy(buf[idx], name, sizeof(buf[idx]));

	return (buf[idx++]);
}

void
lc_string(char *str)
{
	for (; *str != '\0'; str++)
		*str = tolower(*str);
}

void
print_hex(u_int8_t *buf, off_t offset, size_t length)
{
	u_int		 i;
	extern int	 verbose;

	if (verbose < 2 || !length)
		return;

	for (i = 0; i < length; i++) {
		if (i && (i % 4) == 0) {
			if ((i % 32) == 0)
				print_debug("\n");
			else
				print_debug(" ");
		}
		print_debug("%02x", buf[offset + i]);
	}
	print_debug("\n");
}

void
print_hexval(u_int8_t *buf, off_t offset, size_t length)
{
	u_int		 i;
	extern int	 verbose;

	if (verbose < 2 || !length)
		return;

	print_debug("0x");
	for (i = 0; i < length; i++)
		print_debug("%02x", buf[offset + i]);
	print_debug("\n");
}

const char *
print_bits(u_short v, char *bits)
{
	static char	 buf[IKED_CYCLE_BUFFERS][BUFSIZ];
	static int	 idx = 0;
	u_int		 i, any = 0, j = 0;
	char		 c;

	if (!bits)
		return ("");

	if (++idx >= IKED_CYCLE_BUFFERS)
		idx = 0;

	bzero(buf[idx], sizeof(buf[idx]));

	bits++;
	while ((i = *bits++)) {
		if (v & (1 << (i-1))) {
			if (any) {
				buf[idx][j++] = ',';
				if (j >= sizeof(buf[idx]))
					return (buf[idx]);
			}
			any = 1;
			for (; (c = *bits) > 32; bits++) {
				buf[idx][j++] = tolower(c);
				if (j >= sizeof(buf[idx]))
					return (buf[idx]);
			}
		} else
			for (; *bits > 32; bits++)
				;
	}

	return (buf[idx]);
}

u_int8_t
mask2prefixlen(struct sockaddr *sa)
{
	struct sockaddr_in	*sa_in = (struct sockaddr_in *)sa;
	in_addr_t		 ina = sa_in->sin_addr.s_addr;

	if (ina == 0)
		return (0);
	else
		return (33 - ffs(ntohl(ina)));
}

u_int8_t
mask2prefixlen6(struct sockaddr *sa)
{
	struct sockaddr_in6	*sa_in6 = (struct sockaddr_in6 *)sa;
	u_int8_t	 	 l = 0, *ap, *ep;

	/*
	 * sin6_len is the size of the sockaddr so substract the offset of
	 * the possibly truncated sin6_addr struct.
	 */
	ap = (u_int8_t *)&sa_in6->sin6_addr;
	ep = (u_int8_t *)sa_in6 + sa_in6->sin6_len;
	for (; ap < ep; ap++) {
		/* this "beauty" is adopted from sbin/route/show.c ... */
		switch (*ap) {
		case 0xff:
			l += 8;
			break;
		case 0xfe:
			l += 7;
			return (l);
		case 0xfc:
			l += 6;
			return (l);
		case 0xf8:
			l += 5;
			return (l);
		case 0xf0:
			l += 4;
			return (l);
		case 0xe0:
			l += 3;
			return (l);
		case 0xc0:
			l += 2;
			return (l);
		case 0x80:
			l += 1;
			return (l);
		case 0x00:
			return (l);
		default:
			return (0);
		}
	}

	return (l);
}

u_int32_t
prefixlen2mask(u_int8_t prefixlen)
{
	if (prefixlen == 0)
		return (0);

	if (prefixlen > 32)
		prefixlen = 32;

	return (htonl(0xffffffff << (32 - prefixlen)));
}

struct in6_addr *
prefixlen2mask6(u_int8_t prefixlen, u_int32_t *mask)
{
	static struct in6_addr  s6;
	int			i;

	if (prefixlen > 128)
		prefixlen = 128;

	bzero(&s6, sizeof(s6));
	for (i = 0; i < prefixlen / 8; i++)
		s6.s6_addr[i] = 0xff;
	i = prefixlen % 8;
	if (i)
		s6.s6_addr[prefixlen / 8] = 0xff00 >> i;

	memcpy(mask, &s6, sizeof(s6));

	return (&s6);
}

const char *
print_host(struct sockaddr_storage *ss, char *buf, size_t len)
{
	static char	sbuf[IKED_CYCLE_BUFFERS][NI_MAXHOST + 7];
	static int	idx = 0;
	char		pbuf[7];
	in_port_t	port;

	if (buf == NULL) {
		buf = sbuf[idx];
		len = sizeof(sbuf[idx]);
		if (++idx >= IKED_CYCLE_BUFFERS)
			idx = 0;
	}

	if (ss->ss_family == AF_UNSPEC) {
		strlcpy(buf, "any", len);
		return (buf);
	}

	if (getnameinfo((struct sockaddr *)ss, ss->ss_len,
	    buf, len, NULL, 0, NI_NUMERICHOST) != 0) {
		buf[0] = '\0';
		return (NULL);
	}

	if ((port = socket_getport(ss)) != 0) {
		snprintf(pbuf, sizeof(pbuf), ":%d", port);
		(void)strlcat(buf, pbuf, len);
	}

	return (buf);
}

char *
get_string(u_int8_t *ptr, size_t len)
{
	size_t	 i;
	char	*str;

	for (i = 0; i < len; i++)
		if (!isprint((char)ptr[i]))
			break;

	if ((str = calloc(1, i + 1)) == NULL)
		return (NULL);
	memcpy(str, ptr, i);

	return (str);
}

const char *
print_proto(u_int8_t proto)
{
	struct protoent *p;
	static char	 buf[IKED_CYCLE_BUFFERS][BUFSIZ];
	static int	 idx = 0;

	if (idx >= IKED_CYCLE_BUFFERS)
		idx = 0;

	if ((p = getprotobynumber(proto)) != NULL)
		strlcpy(buf[idx], p->p_name, sizeof(buf[idx]));
	else
		snprintf(buf[idx], sizeof(buf), "%u", proto);


	return (buf[idx++]);
}

int
expand_string(char *label, size_t len, const char *srch, const char *repl)
{
	char *tmp;
	char *p, *q;

	if ((tmp = calloc(1, len)) == NULL) {
		log_debug("expand_string: calloc");
		return (-1);
	}
	p = q = label;
	while ((q = strstr(p, srch)) != NULL) {
		*q = '\0';
		if ((strlcat(tmp, p, len) >= len) ||
		    (strlcat(tmp, repl, len) >= len)) {
			log_debug("expand_string: string too long");
			return (-1);
		}
		q += strlen(srch);
		p = q;
	}
	if (strlcat(tmp, p, len) >= len) {
		log_debug("expand_string: string too long");
		return (-1);
	}
	strlcpy(label, tmp, len);	/* always fits */
	free(tmp);

	return (0);
}

u_int8_t *
string2unicode(const char *ascii, size_t *outlen)
{
	u_int8_t	*uc = NULL;
	size_t		 i, len = strlen(ascii);

	if ((uc = calloc(1, (len * 2) + 2)) == NULL)
		return (NULL);

	for (i = 0; i < len; i++) {
		/* XXX what about the byte order? */
		uc[i * 2] = ascii[i];
	}
	*outlen = len * 2;

	return (uc);
}
