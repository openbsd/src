/* $OpenBSD: util.c,v 1.49 2004/12/14 10:17:28 mcbride Exp $	 */
/* $EOM: util.c,v 1.23 2000/11/23 12:22:08 niklas Exp $	 */

/*
 * Copyright (c) 1998, 1999, 2001 Niklas Hallqvist.  All rights reserved.
 * Copyright (c) 2000, 2001, 2004 Håkan Olsson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <limits.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/route.h>
#include <net/if.h>

#include "sysdep.h"

#include "log.h"
#include "message.h"
#include "monitor.h"
#include "sysdep.h"
#include "transport.h"
#include "util.h"

/*
 * Set if -N is given, allowing name lookups to be done, possibly stalling
 * the daemon for quite a while.
 */
int	allow_name_lookups = 0;

/*
 * This is set to true in case of regression-test mode, when it will
 * cause predictable random numbers be generated.
 */
int	regrand = 0;

/*
 * If in regression-test mode, this is the seed used.
 */
u_long	seed;

/*
 * XXX These might be turned into inlines or macros, maybe even
 * machine-dependent ones, for performance reasons.
 */
u_int16_t
decode_16(u_int8_t *cp)
{
	return cp[0] << 8 | cp[1];
}

u_int32_t
decode_32(u_int8_t *cp)
{
	return cp[0] << 24 | cp[1] << 16 | cp[2] << 8 | cp[3];
}

u_int64_t
decode_64(u_int8_t *cp)
{
	return (u_int64_t) cp[0] << 56 | (u_int64_t) cp[1] << 48 |
	    (u_int64_t) cp[2] << 40 | (u_int64_t) cp[3] << 32 |
	    cp[4] << 24 | cp[5] << 16 | cp[6] << 8 | cp[7];
}

#if 0
/*
 * XXX I severly doubt that we will need this.  IPv6 does not have the legacy
 * of representation in host byte order, AFAIK.
 */

void
decode_128(u_int8_t *cp, u_int8_t *cpp)
{
#if BYTE_ORDER == LITTLE_ENDIAN
	int	i;

	for (i = 0; i < 16; i++)
		cpp[i] = cp[15 - i];
#elif BYTE_ORDER == BIG_ENDIAN
	bcopy(cp, cpp, 16);
#else
#error "Byte order unknown!"
#endif
}
#endif

void
encode_16(u_int8_t *cp, u_int16_t x)
{
	*cp++ = x >> 8;
	*cp = x & 0xff;
}

void
encode_32(u_int8_t *cp, u_int32_t x)
{
	*cp++ = x >> 24;
	*cp++ = (x >> 16) & 0xff;
	*cp++ = (x >> 8) & 0xff;
	*cp = x & 0xff;
}

void
encode_64(u_int8_t *cp, u_int64_t x)
{
	*cp++ = x >> 56;
	*cp++ = (x >> 48) & 0xff;
	*cp++ = (x >> 40) & 0xff;
	*cp++ = (x >> 32) & 0xff;
	*cp++ = (x >> 24) & 0xff;
	*cp++ = (x >> 16) & 0xff;
	*cp++ = (x >> 8) & 0xff;
	*cp = x & 0xff;
}

#if 0
/*
 * XXX I severly doubt that we will need this.  IPv6 does not have the legacy
 * of representation in host byte order, AFAIK.
 */

void
encode_128(u_int8_t *cp, u_int8_t *cpp)
{
	decode_128(cpp, cp);
}
#endif

/* Check a buffer for all zeroes.  */
int
zero_test(const u_int8_t *p, size_t sz)
{
	while (sz-- > 0)
		if (*p++ != 0)
			return 0;
	return 1;
}

/* Check a buffer for all ones.  */
int
ones_test(const u_int8_t *p, size_t sz)
{
	while (sz-- > 0)
		if (*p++ != 0xff)
			return 0;
	return 1;
}

/*
 * Generate a random data, len bytes long.
 */
u_int8_t *
getrandom(u_int8_t *buf, size_t len)
{
	u_int32_t	tmp = 0;
	size_t		i;

	for (i = 0; i < len; i++) {
		if (i % sizeof tmp == 0)
			tmp = sysdep_random();

		buf[i] = tmp & 0xff;
		tmp >>= 8;
	}

	return buf;
}

static __inline int
hex2nibble(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

/*
 * Convert hexadecimal string in S to raw binary buffer at BUF sized SZ
 * bytes.  Return 0 if everything is OK, -1 otherwise.
 */
int
hex2raw(char *s, u_int8_t *buf, size_t sz)
{
	u_int8_t *bp;
	char	*p;
	int	tmp;

	if (strlen(s) > sz * 2)
		return -1;
	for (p = s + strlen(s) - 1, bp = &buf[sz - 1]; bp >= buf; bp--) {
		*bp = 0;
		if (p >= s) {
			tmp = hex2nibble(*p--);
			if (tmp == -1)
				return -1;
			*bp = tmp;
		}
		if (p >= s) {
			tmp = hex2nibble(*p--);
			if (tmp == -1)
				return -1;
			*bp |= tmp << 4;
		}
	}
	return 0;
}

int
text2sockaddr(char *address, char *port, struct sockaddr **sa, sa_family_t af,
    int netmask)
{
	struct addrinfo *ai, hints;
	struct sockaddr_storage tmp_sas;
	struct ifaddrs *ifap, *ifa = NULL, *llifa = NULL;
	char *np = address;
#ifdef USE_DEFAULT_ROUTE
	char ifname[IFNAMSIZ];
	u_char buf[BUFSIZ];
	struct rt_msghdr *rtm;
	struct sockaddr *sa2;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	int fd = 0, seq, len, b;
	pid_t pid;
#endif /* USE_DEFAULT_ROUTE */

	memset(&hints, 0, sizeof hints);
	if (!allow_name_lookups)
		hints.ai_flags = AI_NUMERICHOST;
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;

	if (getaddrinfo(address, port, &hints, &ai)) {
#ifdef USE_DEFAULT_ROUTE
		/*
		 * If the 'default' keyword is used, do a route lookup for
		 * the default route, and use the interface associated with
		 * it to select a source address.
		 */
		if (!strcmp(address, "default")) {
			fd = socket(PF_ROUTE, SOCK_RAW, af);
			
			memset(buf, 0, sizeof(buf));

			rtm = (struct rt_msghdr *)buf;
			rtm->rtm_version = RTM_VERSION;
			rtm->rtm_type = RTM_GET;
			rtm->rtm_flags = RTF_UP;
			rtm->rtm_addrs = RTA_DST;
			rtm->rtm_seq = seq = arc4random(); 

			/* default destination */
			sa2 = (struct sockaddr *)(rtm + 1);
			switch (af) {
			case AF_INET: {
				sin = (struct sockaddr_in *)sa2;
				sin->sin_len = sizeof(*sin);
				sin->sin_family = af;
				break;
			}
			case AF_INET6: {
				sin6 = (struct sockaddr_in6 *)sa2;
				sin6->sin6_len = sizeof(*sin6);
				sin6->sin6_family = af;
				break;
			}
			default:
				close(fd);
				return (-1);
			}
			rtm->rtm_addrs |= RTA_NETMASK|RTA_IFP|RTA_IFA;
			rtm->rtm_msglen = sizeof(*rtm) + sizeof(*sa2);

			if ((b = write(fd, buf, rtm->rtm_msglen)) < 0) {
				close(fd);
				return (-1);
			}

			pid = getpid();

			while ((len = read(fd, buf, sizeof(buf))) > 0) {
				if (len < sizeof(*rtm)) {
					close(fd);
					return (-1);
				}

				if (rtm->rtm_type == RTM_GET &&
				    rtm->rtm_pid == pid &&
				    rtm->rtm_seq == seq) {
					if (rtm->rtm_errno) {
						close(fd);
						return (-1);
					}
					break;
				}
			}
			close(fd);

			if ((rtm->rtm_addrs & (RTA_DST|RTA_GATEWAY)) ==
			    (RTA_DST|RTA_GATEWAY)) {
				np = if_indextoname(rtm->rtm_index, ifname);
				if (np == NULL)
					return (-1);
			}	
		}
#endif /* USE_DEFAULT_ROUTE */
	
		if (getifaddrs(&ifap) != 0)
			return (-1);

		switch (af) {
		default:
		case AF_INET:
			for (ifa = ifap; ifa; ifa = ifa->ifa_next)
				if (!strcmp(ifa->ifa_name, np) &&
				    ifa->ifa_addr != NULL && 
				    ifa->ifa_addr->sa_family == AF_INET)
					break;
			break;
		case AF_INET6:
			for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
				if (!strcmp(ifa->ifa_name, np) &&
				    ifa->ifa_addr != NULL && 
				    ifa->ifa_addr->sa_family == AF_INET6) {
					if (IN6_IS_ADDR_LINKLOCAL(
					    &((struct sockaddr_in6 *)
					    ifa->ifa_addr)->sin6_addr) && 
					    llifa == NULL)
						llifa = ifa;
					else
						break;
				}
			}
			if (ifa == NULL) {
				ifa = llifa;
			}
			break;
		}
  
		if (ifa) {
			if (netmask)
				memcpy(&tmp_sas, ifa->ifa_netmask,
				    sysdep_sa_len(ifa->ifa_netmask));
				    
			else
				memcpy(&tmp_sas, ifa->ifa_addr,
				    sysdep_sa_len(ifa->ifa_addr));
			freeifaddrs(ifap);
		} else {
			freeifaddrs(ifap);
			return -1;
		}
	} else {
		memcpy(&tmp_sas, ai->ai_addr, sysdep_sa_len(ai->ai_addr));
		freeaddrinfo(ai);
	}

	*sa = malloc(sysdep_sa_len((struct sockaddr *)&tmp_sas));
	if (!*sa)
		return -1;

	memcpy(*sa, &tmp_sas, sysdep_sa_len((struct sockaddr *)&tmp_sas));
	return 0;
}

/*
 * Convert a sockaddr to text. With zflag non-zero fill out with zeroes,
 * i.e 10.0.0.10 --> "010.000.000.010"
 */
int
sockaddr2text(struct sockaddr *sa, char **address, int zflag)
{
	char	buf[NI_MAXHOST], *token, *bstart, *ep;
	int	addrlen, i, j;
	long	val;

	if (getnameinfo(sa, sysdep_sa_len(sa), buf, sizeof buf, 0, 0,
			allow_name_lookups ? 0 : NI_NUMERICHOST))
		return -1;

	if (zflag == 0) {
		*address = strdup(buf);
		if (!*address)
			return -1;
	} else
		switch (sa->sa_family) {
		case AF_INET:
			addrlen = sizeof "000.000.000.000";
			*address = malloc(addrlen);
			if (!*address)
				return -1;
			buf[addrlen] = '\0';
			bstart = buf;
			**address = '\0';
			while ((token = strsep(&bstart, ".")) != NULL) {
				if (strlen(*address) > 12) {
					free(*address);
					return -1;
				}
				val = strtol(token, &ep, 10);
				if (ep == token || val < (long)0 ||
				    val > (long)UCHAR_MAX) {
					free(*address);
					return -1;
				}
				snprintf(*address + strlen(*address),
				    addrlen - strlen(*address), "%03ld", val);
				if (bstart)
					strlcat(*address, ".", addrlen);
			}
			break;

		case AF_INET6:
			/*
			 * XXX In the algorithm below there are some magic
			 * numbers we probably could give explaining names.
			 */
			addrlen =
			    sizeof "0000:0000:0000:0000:0000:0000:0000:0000";
			*address = malloc(addrlen);
			if (!*address)
				return -1;

			for (i = 0, j = 0; i < 8; i++) {
				snprintf((*address) + j, addrlen - j,
				    "%02x%02x",
				    ((struct sockaddr_in6 *)sa)->sin6_addr.s6_addr[2*i],
				    ((struct sockaddr_in6 *)sa)->sin6_addr.s6_addr[2*i + 1]);
				j += 4;
				(*address)[j] =
				    (j < (addrlen - 1)) ? ':' : '\0';
				j++;
			}
			break;

		default:
			*address = strdup("<error>");
			if (!*address)
				return -1;
		}

	return 0;
}

/*
 * sockaddr_addrlen and sockaddr_addrdata return the relevant sockaddr info
 * depending on address family.  Useful to keep other code shorter(/clearer?).
 */
int
sockaddr_addrlen(struct sockaddr *sa)
{
	switch (sa->sa_family) {
	case AF_INET6:
		return sizeof((struct sockaddr_in6 *)sa)->sin6_addr.s6_addr;
	case AF_INET:
		return sizeof((struct sockaddr_in *)sa)->sin_addr.s_addr;
	default:
		log_print("sockaddr_addrlen: unsupported protocol family %d",
		    sa->sa_family);
		return 0;
	}
}

u_int8_t *
sockaddr_addrdata(struct sockaddr *sa)
{
	switch (sa->sa_family) {
	case AF_INET6:
		return (u_int8_t *)&((struct sockaddr_in6 *)sa)->sin6_addr.s6_addr;
	case AF_INET:
		return (u_int8_t *)&((struct sockaddr_in *)sa)->sin_addr.s_addr;
	default:
		log_print("sockaddr_addrdata: unsupported protocol family %d",
		    sa->sa_family);
		return 0;
	}
}

in_port_t
sockaddr_port(struct sockaddr *sa)
{
	switch (sa->sa_family) {
	case AF_INET6:
		return ((struct sockaddr_in6 *)sa)->sin6_port;
	case AF_INET:
		return ((struct sockaddr_in *)sa)->sin_port;
	default:
		log_print("sockaddr_port: unsupported protocol family %d",
		    sa->sa_family);
		return 0;
	}
}

/* Utility function used to set the port of a sockaddr.  */
void
sockaddr_set_port(struct sockaddr *sa, in_port_t port)
{
	switch (sa->sa_family) {
	case AF_INET:
		((struct sockaddr_in *)sa)->sin_port = htons (port);
		break;

	case AF_INET6:
		((struct sockaddr_in6 *)sa)->sin6_port = htons (port);
		break;
	}
}

/*
 * Convert network address to text. The network address does not need
 * to be properly aligned.
 */
void
util_ntoa(char **buf, int af, u_int8_t *addr)
{
	struct sockaddr_storage from;
	struct sockaddr *sfrom = (struct sockaddr *) & from;
	socklen_t	fromlen = sizeof from;

	memset(&from, 0, fromlen);
	sfrom->sa_family = af;
#ifndef USE_OLD_SOCKADDR
	switch (af) {
	case AF_INET:
		sfrom->sa_len = sizeof(struct sockaddr_in);
		break;
	case AF_INET6:
		sfrom->sa_len = sizeof(struct sockaddr_in6);
		break;
	}
#endif
	memcpy(sockaddr_addrdata(sfrom), addr, sockaddr_addrlen(sfrom));

	if (sockaddr2text(sfrom, buf, 0)) {
		log_print("util_ntoa: could not make printable address out "
		    "of sockaddr %p", sfrom);
		*buf = 0;
	}
}

/*
 * Perform sanity check on files containing secret information.
 * Returns -1 on failure, 0 otherwise.
 * Also, if FILE_SIZE is a not a null pointer, store file size here.
 */

int
check_file_secrecy_fd(int fd, char *name, size_t *file_size)
{
	struct stat st;

	if (fstat(fd, &st) == -1) {
		log_error("check_file_secrecy: stat (\"%s\") failed", name);
		return -1;
	}
	if (st.st_uid != 0 && st.st_uid != getuid()) {
		log_print("check_file_secrecy_fd: "
		    "not loading %s - file owner is not process user", name);
		errno = EPERM;
		return -1;
	}
	if ((st.st_mode & (S_IRWXG | S_IRWXO)) != 0) {
		log_print("check_file_secrecy_fd: not loading %s - too open "
		    "permissions", name);
		errno = EPERM;
		return -1;
	}
	if (file_size)
		*file_size = (size_t)st.st_size;

	return 0;
}

/* Special for compiling with Boehms GC. See Makefile and sysdep.h  */
#if defined (USE_BOEHM_GC)
char *
gc_strdup(const char *x)
{
	char *strcpy(char *,const char *);
	char *y = malloc(strlen(x) + 1);
	return strcpy(y,x);
}
#endif
