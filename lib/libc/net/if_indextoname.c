/*
 * %%% copyright-cmetz-98-bsd
 * Copyright (c) 1998-1999, Craig Metz, All rights reserved.
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
 *      This product includes software developed by Craig Metz and
 *      by other contributors.
 * 4. Neither the name of the author nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <errno.h>
#include <string.h>

static char __name[IFNAMSIZ];

char *
if_indextoname(unsigned int index, char *name)
{
	int     i, fd = -1, extra, len = 0;
	struct ifconf ifconf;
	char    lastname[IFNAMSIZ], iname[IFNAMSIZ], *retname = NULL, *inbuf;
	struct sockaddr *sa;
	void	*p;

	ifconf.ifc_buf = 0;

	if (!name)
		name = __name;

	if ((fd = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
		goto ret;

	/*
	 * Try ifc_len == 0 hack first, to get the actual length.
	 * If that fails, revert to a loop which grows the ifc_buf
	 * until it is sufficiently large.
	 */
	extra = sizeof(struct ifreq);
	while (1) {
		ifconf.ifc_len = len;
		if (ioctl(fd, SIOCGIFCONF, (void *) &ifconf) == -1 &&
		    ifconf.ifc_buf)
			goto ret;
		if (ifconf.ifc_buf &&
		    ifconf.ifc_len + extra < len)
			break;
		if (ifconf.ifc_buf) {
			if (len == 0)
				len = 4096;
			ifconf.ifc_len = len *= 2;
		} else {
			len = ifconf.ifc_len;
			extra = 0;
		}
		inbuf = realloc(ifconf.ifc_buf, ifconf.ifc_len);
		if (inbuf == NULL)
			goto ret;
		ifconf.ifc_buf = inbuf;
	}

	i = 0;
	p = ifconf.ifc_buf;
	len = ifconf.ifc_len;
	lastname[0] = 0;
	lastname[sizeof(lastname)-1] = 0;
	iname[0] = 0;

	while (len > 0) {
		if (len < (IFNAMSIZ + sizeof(struct sockaddr)))
			goto ret;
		if (strncmp(lastname, p, IFNAMSIZ)) {
			if (i == index)
				memcpy(iname, lastname, sizeof(iname));
			strlcpy(lastname, p, sizeof(lastname));
			i++;
		}
		len -= IFNAMSIZ;
		p += IFNAMSIZ;
		sa = p;

		if (sa->sa_family == AF_LINK) {
			struct sockaddr_dl *sd = p;

			if (sd->sdl_index == index) {
				strlcpy(name, lastname, IFNAMSIZ);
				retname = name;
				goto ret;
			}
		}

		if (len < sa->sa_len)
			goto ret;
		len -= sa->sa_len;
		p += sa->sa_len;
	}

	if (i == index)
		strlcpy(iname, lastname, sizeof(iname));

	if (iname[0]) {
		strlcpy(name, iname, IFNAMSIZ);
		retname = name;
	}
ret:
	if (fd != -1)
		close(fd);
	if (ifconf.ifc_buf)
		free(ifconf.ifc_buf);
	return (retname);
}
