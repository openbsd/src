/*	$OpenBSD: bindresvport.c,v 1.3 1996/07/29 06:11:57 downsj Exp $	*/
/*	$NetBSD: bindresvport.c,v 1.5 1995/06/03 22:37:19 mycroft Exp $	*/

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
/*static char *sccsid = "from: @(#)bindresvport.c 1.8 88/02/08 SMI";*/
/*static char *sccsid = "from: @(#)bindresvport.c	2.2 88/07/29 4.0 RPCSRC";*/
static char *rcsid = "$OpenBSD: bindresvport.c,v 1.3 1996/07/29 06:11:57 downsj Exp $";
#endif

/*
 * Copyright (c) 1987 by Sun Microsystems, Inc.
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
	int on, error;
	struct sockaddr_in myaddr;

	if (sin == (struct sockaddr_in *)0) {
		sin = &myaddr;
		memset(sin, 0, sizeof (*sin));
		sin->sin_len = sizeof(struct sockaddr_in);
		sin->sin_family = AF_INET;
	} else if (sin->sin_family != AF_INET) {
		errno = EPFNOSUPPORT;
		return (-1);
	}

	if (sin->sin_port == 0) {
		on = IP_PORTRANGE_LOW;
		error = setsockopt(sd, IPPROTO_IP, IP_PORTRANGE,
		           	   (char *)&on, sizeof(on));
		if (error < 0)
			return(error);
	}

	error = bind(sd, (struct sockaddr *)sin, sizeof(struct sockaddr_in));
	return(error);
}
