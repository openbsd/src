/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$OpenBSD: bindresvport.c,v 1.12 2000/01/24 02:24:21 deraadt Exp $";
#endif /* LIBC_SCCS and not lint */

/*
 * Copyright (c) 1987 by Sun Microsystems, Inc.
 *
 * Portions Copyright(C) 1996, Jason Downs.  All rights reserved.
 */

#include <string.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <netinet/in.h>

/*
 * Bind a socket to a privileged IP port
 */
int
bindresvport(sd, sin)
	int sd;
	struct sockaddr_in *sin;
{
	if (sin)
		return bindresvport_af(sd, (struct sockaddr *)sin, sin->sin_family);
	return bindresvport_af(sd, NULL, AF_INET);
}

/*
 * Bind a socket to a privileged IP port
 */
int
bindresvport_af(sd, sa, af)
	int sd;
	struct sockaddr *sa;
	int af;
{
	int old, error;
	struct sockaddr_storage myaddr;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	int proto, portrange, portlow;
	u_int16_t *portp;
	int salen;

	if (sa == NULL) {
		memset(&myaddr, 0, sizeof(myaddr));
		sa = (struct sockaddr *)&myaddr;
	}

	if (af == AF_INET) {
		proto = IPPROTO_IP;
		portrange = IP_PORTRANGE;
		portlow = IP_PORTRANGE_LOW;
		sin = (struct sockaddr_in *)sa;
		salen = sizeof(struct sockaddr_in);
		portp = &sin->sin_port;
	} else if (af == AF_INET6) {
		proto = IPPROTO_IPV6;
		portrange = IPV6_PORTRANGE;
		portlow = IPV6_PORTRANGE_LOW;
		sin6 = (struct sockaddr_in6 *)sa;
		salen = sizeof(struct sockaddr_in6);
		portp = &sin6->sin6_port;
	} else {
		errno = EPFNOSUPPORT;
		return (-1);
	}
	sa->sa_family = af;
	sa->sa_len = salen;

	if (*portp == 0) {
		int oldlen = sizeof(old);

		error = getsockopt(sd, proto, portrange, &old, &oldlen);
		if (error < 0)
			return(error);

		error = setsockopt(sd, proto, portrange, &portlow,
		    sizeof(portlow));
		if (error < 0)
			return(error);
	}

	error = bind(sd, sa, salen);

	if (*portp == 0) {
		int saved_errno = errno;

		if (error) {
			if (setsockopt(sd, proto, portrange, &old,
			    sizeof(old)) < 0)
				errno = saved_errno;
			return (error);
		}

		if (sa != (struct sockaddr *)&myaddr) {
			/* Hmm, what did the kernel assign... */
			if (getsockname(sd, sa, &salen) < 0)
				errno = saved_errno;
			return (error);
		}
	}
	return (error);
}
