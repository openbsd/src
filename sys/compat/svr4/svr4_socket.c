/*	$OpenBSD: svr4_socket.c,v 1.5 2006/03/05 21:48:56 miod Exp $	*/
/*	$NetBSD: svr4_socket.c,v 1.4 1997/07/21 23:02:37 christos Exp $	*/

/*
 * Copyright (c) 1996 Christos Zoulas.  All rights reserved.
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
 *	This product includes software developed by Christos Zoulas.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * In SVR4 unix domain sockets are referenced sometimes
 * (in putmsg(2) for example) as a [device, inode] pair instead of a pathname.
 * Since there is no iname() routine in the kernel, and we need access to
 * a mapping from inode to pathname, we keep our own table. This is a simple
 * linked list that contains the pathname, the [device, inode] pair, the
 * file corresponding to that socket and the process. When the
 * socket gets closed we remove the item from the list. The list gets loaded
 * every time a stat(2) call finds a socket.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/mbuf.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syscallargs.h>
#include <sys/un.h>
#include <sys/stat.h>

#include <compat/svr4/svr4_types.h>
#include <compat/svr4/svr4_util.h>
#include <compat/svr4/svr4_socket.h>
#include <compat/svr4/svr4_signal.h>
#include <compat/svr4/svr4_sockmod.h>
#include <compat/svr4/svr4_syscallargs.h>

struct svr4_sockcache_entry {
	struct proc *p;		/* Process for the socket		*/
	void *cookie;		/* Internal cookie used for matching	*/
	struct sockaddr_un sock;/* Pathname for the socket		*/
	dev_t dev;		/* Device where the socket lives on	*/
	ino_t ino;		/* Inode where the socket lives on	*/
	TAILQ_ENTRY(svr4_sockcache_entry) entries;
};

static TAILQ_HEAD(svr4_sockcache_head, svr4_sockcache_entry) svr4_head;
static int initialized = 0;

struct sockaddr_un *
svr4_find_socket(p, fp, dev, ino)
	struct proc *p;
	struct file *fp;
	dev_t dev;
	ino_t ino;
{
	struct svr4_sockcache_entry *e;
	void *cookie = ((struct socket *) fp->f_data)->so_internal;

	if (!initialized) {
		DPRINTF(("svr4_find_socket: uninitialized [%p,%d,%d]\n",
		    p, dev, ino));
		TAILQ_INIT(&svr4_head);
		initialized = 1;
		return NULL;
	}


	DPRINTF(("svr4_find_socket: [%p,%d,%d]: ", p, dev, ino));
	TAILQ_FOREACH(e, &svr4_head, entries)
		if (e->p == p && e->dev == dev && e->ino == ino) {
#ifdef DIAGNOSTIC
			if (e->cookie != NULL && e->cookie != cookie)
				panic("svr4 socket cookie mismatch");
#endif
			e->cookie = cookie;
			DPRINTF(("%s\n", e->sock.sun_path));
			return &e->sock;
		}

	DPRINTF(("not found\n"));
	return NULL;
}


void
svr4_delete_socket(p, fp)
	struct proc *p;
	struct file *fp;
{
	struct svr4_sockcache_entry *e;
	void *cookie = ((struct socket *) fp->f_data)->so_internal;

	if (!initialized) {
		TAILQ_INIT(&svr4_head);
		initialized = 1;
		return;
	}

	TAILQ_FOREACH(e, &svr4_head, entries)
		if (e->p == p && e->cookie == cookie) {
			TAILQ_REMOVE(&svr4_head, e, entries);
			DPRINTF(("svr4_delete_socket: %s [%p,%d,%d]\n",
				 e->sock.sun_path, p, e->dev, e->ino));
			free(e, M_TEMP);
			return;
		}
}


int
svr4_add_socket(p, path, st)
	struct proc *p;
	const char *path;
	struct stat *st;
{
	struct svr4_sockcache_entry *e;
	size_t len;
	int error;

	if (!initialized) {
		TAILQ_INIT(&svr4_head);
		initialized = 1;
	}

	e = malloc(sizeof(*e), M_TEMP, M_WAITOK);
	e->cookie = NULL;
	e->dev = st->st_dev;
	e->ino = st->st_ino;
	e->p = p;

	if ((error = copyinstr(path, e->sock.sun_path,
	    sizeof(e->sock.sun_path), &len)) != 0) {
		DPRINTF(("svr4_add_socket: copyinstr failed %d\n", error));
		free(e, M_TEMP);
		return error;
	}

	e->sock.sun_family = AF_UNIX;
	e->sock.sun_len = len;

	TAILQ_INSERT_HEAD(&svr4_head, e, entries);
	DPRINTF(("svr4_add_socket: %s [%p,%d,%d]\n", e->sock.sun_path,
		 p, e->dev, e->ino));
	return 0;
}


int
svr4_sys_socket(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_socket_args *uap = v;

	switch (SCARG(uap, type)) {
	case SVR4_SOCK_DGRAM:
		SCARG(uap, type) = SOCK_DGRAM;
		break;

	case SVR4_SOCK_STREAM:
		SCARG(uap, type) = SOCK_STREAM;
		break;

	case SVR4_SOCK_RAW:
		SCARG(uap, type) = SOCK_RAW;
		break;

	case SVR4_SOCK_RDM:
		SCARG(uap, type) = SOCK_RDM;
		break;

	case SVR4_SOCK_SEQPACKET:
		SCARG(uap, type) = SOCK_SEQPACKET;
		break;
	default:
		return EINVAL;
	}
	return sys_socket(p, uap, retval);
}
