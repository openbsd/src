/*	$OpenBSD: common.c,v 1.12 2004/05/06 20:29:04 deraadt Exp $	*/

/*
 * Copyright (c) 2000 Network Security Technologies, Inc. http://www.netsec.net
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/ppp_defs.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <net/bpf.h>
#include <errno.h>
#include <string.h>
#include <err.h>
#include <fcntl.h>
#include <unistd.h>
#include <sysexits.h>
#include <stdlib.h>
#include <md5.h>

#include "pppoe.h"

#define PPP_PROG	"/usr/sbin/ppp"

int
runppp(int bpffd, u_int8_t *sysname)
{
	int socks[2], fdm, fds, closeit;
	pid_t pid;

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, socks) < 0)
		return (-1);

	fdm = socks[0];
	fds = socks[1];

	pid = fork();
	if (pid < 0) {
		close(fds);
		close(fdm);
		return (-1);
	}

	if (pid != 0) {
		/* Parent */
		close(fds);
		return (fdm);
	}

	if (setsid() < 0)
		_exit(99);

	/* Child */
	close(bpffd);
	close(fdm);
	closeit = 1;

	if (fds == STDIN_FILENO)
		closeit = 0;
	else
		dup2(fds, STDIN_FILENO);

	if (fds == STDOUT_FILENO)
		closeit = 0;
	else
		dup2(fds, STDOUT_FILENO);

	if (fds == STDERR_FILENO)
		closeit = 0;
	else
		dup2(fds, STDERR_FILENO);

	if (closeit)
		close(fds);

	execlp(PPP_PROG, "ppp", "-direct", sysname, (char *)NULL);
	perror("execlp");
	_exit(1);
	/*NOTREACHED*/
	return (-1);
}

int
bpf_to_ppp(int pppfd, u_long len, u_int8_t *pkt)
{
	int r;
	u_int8_t hdr[2] = { PPP_ALLSTATIONS, PPP_UI };
	struct iovec iov[2];

	iov[0].iov_base = hdr;
	iov[0].iov_len = sizeof(hdr);
	iov[1].iov_base = pkt;
	iov[1].iov_len = len;

	r = writev(pppfd, iov, 2);
	if (r < 0) {
		if (errno == EINTR || errno == EPIPE || errno == ENOBUFS)
			return (0);
		return (-1);
	}
	return (1);
}

int
ppp_to_bpf(int bfd, int pppfd, struct ether_addr *myea,
    struct ether_addr *rmea, u_int16_t id)
{
	static u_int8_t *pktbuf = NULL;
	struct pppoe_header ph;
	struct iovec iov[5];
	u_int16_t etype;
	u_int8_t trash[2];
	int r;

	if (pktbuf == NULL) {
		pktbuf = (u_int8_t *)malloc(PPPOE_MTU);
		if (pktbuf == NULL)
			return (-1);
	}

	iov[0].iov_base = trash;
	iov[0].iov_len = 2;
	iov[1].iov_base = pktbuf;
	iov[1].iov_len = PPPOE_MTU;
	r = readv(pppfd, iov, 2);
	if (r <= 0)
		return (-1);
	r -= 2;

	ph.vertype = PPPOE_VERTYPE(1, 1);
	ph.code = PPPOE_CODE_SESSION;
	ph.len = htons(r);
	ph.sessionid = htons(id);
	etype = htons(ETHERTYPE_PPPOE);

	iov[0].iov_base = rmea;		iov[0].iov_len = ETHER_ADDR_LEN;
	iov[1].iov_base = myea;		iov[1].iov_len = ETHER_ADDR_LEN;
	iov[2].iov_base = &etype;	iov[2].iov_len = sizeof(etype);
	iov[3].iov_base = &ph;		iov[3].iov_len = sizeof(ph);
	iov[4].iov_base = pktbuf;	iov[4].iov_len = r;

	r = writev(bfd, iov, 5);

	return (r == -1 && errno == ENOBUFS ? 0 : r);
}

int
send_padt(int bpffd, struct ether_addr *src_ea,
    struct ether_addr *dst_ea, u_int16_t id)
{
	struct iovec iov[4];
	struct pppoe_header ph;
	u_int16_t etype = htons(ETHERTYPE_PPPOEDISC);

	iov[0].iov_base = dst_ea;
	iov[0].iov_len = ETHER_ADDR_LEN;
	iov[1].iov_base = src_ea;
	iov[1].iov_len = ETHER_ADDR_LEN;
	iov[2].iov_base = &etype;
	iov[2].iov_len = sizeof(etype);
	iov[3].iov_base = &ph;
	iov[3].iov_len = sizeof(ph);

	ph.vertype = PPPOE_VERTYPE(1, 1);
	ph.code = PPPOE_CODE_PADT;
	ph.len = 0;
	ph.sessionid = htons(id);

	return (writev(bpffd, iov, 4));
}

u_int32_t
cookie_bake(void)
{
	MD5_CTX ctx;
	unsigned char buf[40];
	u_int32_t x, y;

	x = arc4random();
	MD5Init(&ctx);
	MD5Update(&ctx, (unsigned char *)&x, sizeof(x));
	MD5Final((unsigned char *)buf, &ctx);
	bcopy(buf, &y, sizeof(y));
	x = x ^ y;
	bcopy(buf + 4, &y, sizeof(y));
	x = x ^ y;
	bcopy(buf + 8, &y, sizeof(y));
	x = x ^ y;
	bcopy(buf + 12, &y, sizeof(y));
	x = x ^ y;
	return (x);
}
