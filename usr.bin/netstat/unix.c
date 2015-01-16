/*	$OpenBSD: unix.c,v 1.24 2015/01/16 06:40:10 deraadt Exp $	*/
/*	$NetBSD: unix.c,v 1.13 1995/10/03 21:42:48 thorpej Exp $	*/

/*-
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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

/*
 * Display protocol blocks in the unix domain.
 */
#include <sys/param.h>	/* MSIZE */
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>
#include <sys/un.h>
#include <sys/unpcb.h>
#define _KERNEL
#include <sys/ucred.h>
#include <sys/file.h>
#undef _KERNEL

#include <netinet/in.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <kvm.h>
#include "netstat.h"

static	void unixdomainpr(const struct kinfo_file *, u_long);

void
unixpr(kvm_t *kvmd, u_long pcbaddr)
{
	struct kinfo_file *kf;
	int i, fcnt;

	kf = kvm_getfiles(kvmd, KERN_FILE_BYFILE, 0, sizeof(*kf), &fcnt);
	if (kf == NULL) {
		printf("Out of memory (file table).\n");
		return;
	}
	for (i = 0; i < fcnt; i++) {
		if (kf[i].f_count != 0 && kf[i].f_type == DTYPE_SOCKET &&
		    kf[i].so_family == AF_LOCAL && (kf[i].so_pcb != 0 ||
		    kf[i].unp_path[0] != '\0'))
			unixdomainpr(&kf[i], pcbaddr);
	}
}

static	const char *socktype[] =
    { "#0", "stream", "dgram", "raw", "rdm", "seqpacket" };

static void
unixdomainpr(const struct kinfo_file *kf, u_long pcbaddr)
{
	static int first = 1;

	if (Pflag) {
		if (pcbaddr == kf->f_data)
			socket_dump(pcbaddr);
		return;
	}

	if (first) {
		printf("Active UNIX domain sockets\n");
		printf("%-*.*s %-6.6s %-6.6s %-6.6s %*.*s %*.*s %*.*s %*.*s Addr\n",
		    PLEN, PLEN, "Address", "Type", "Recv-Q", "Send-Q",
		    PLEN, PLEN, "Inode", PLEN, PLEN, "Conn",
		    PLEN, PLEN, "Refs", PLEN, PLEN, "Nextref");
		first = 0;
	}

#define	FAKE_PTR(p)	(PLEN - ((p) ? 0 : 2)), p, ((p) ? "" : "x0")
	printf("%#*llx%s %-6.6s %6llu %6llu %#*llx%s %#*llx%s %#*llx%s %#*llx%s",
	    FAKE_PTR(kf->f_data), socktype[kf->so_type],
	    kf->so_rcv_cc, kf->so_snd_cc,
	    FAKE_PTR(kf->v_un),
	    FAKE_PTR(kf->unp_conn),
	    FAKE_PTR(kf->unp_refs),
	    FAKE_PTR(kf->unp_nextref));
	if (kf->unp_path[0] != '\0')
		printf(" %.*s", KI_UNPPATHLEN, kf->unp_path);
	putchar('\n');
}

/*
 * Dump the contents of a UNIX PCB
 */
void
unpcb_dump(u_long off)
{
	struct unpcb unp;

	if (off == 0)
		return;
	kread(off, &unp, sizeof(unp));

#define	p(fmt, v, sep) printf(#v " " fmt sep, unp.v);
#define	pll(fmt, v, sep) printf(#v " " fmt sep, (long long) unp.v);
#define	pull(fmt, v, sep) printf(#v " " fmt sep, (unsigned long long) unp.v);
#define	pp(fmt, v, sep) printf(#v " " fmt sep, hideroot ? 0 : unp.v);
	printf("unpcb %#lx\n ", hideroot ? 0 : off);
	pp("%p", unp_socket, "\n ");
	pp("%p", unp_vnode, ", ");
	pull("%llu", unp_ino, "\n ");
	pp("%p", unp_conn, ", ");
	pp("%p", unp_refs, ", ");
	pp("%p", unp_nextref, "\n ");
	pp("%p", unp_addr, "\n ");
	p("%#.8x", unp_flags, "\n ");
	p("%u", unp_connid.uid, ", ");
	p("%u", unp_connid.gid, ", ");
	p("%d", unp_connid.pid, "\n ");
	p("%d", unp_cc, ", ");
	p("%d", unp_mbcnt, "\n ");
	pll("%lld", unp_ctime.tv_sec, ", ");
	p("%ld", unp_ctime.tv_nsec, "\n");
#undef p
#undef pp
}
