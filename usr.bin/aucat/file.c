/*	$OpenBSD: file.c,v 1.1 2008/05/23 07:15:46 ratchov Exp $	*/
/*
 * Copyright (c) 2008 Alexandre Ratchov <alex@caoua.org>
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
/*
 * non-blocking file i/o module: each file can be read or written (or
 * both). To achieve non-blocking io, we simply use the poll() syscall
 * in an event loop. If a read() or write() syscall return EAGAIN
 * (operation will block), then the file is marked as "for polling", else
 * the file is not polled again.
 *
 */
#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "conf.h"
#include "file.h"
#include "aproc.h"
#include "abuf.h"

#define MAXFDS 100

struct filelist file_list;

struct file *
file_new(int fd, char *name)
{
	unsigned i;
	struct file *f;

	i = 0;
	LIST_FOREACH(f, &file_list, entry)
		i++;		
	if (i >= MAXFDS)
		err(1, "%s: too many polled files", name);

	f = malloc(sizeof(struct file));
	if (f == NULL)
		err(1, "%s", name);

	f->fd = fd;
	f->events = 0;
	f->rbytes = -1;
	f->wbytes = -1;
	f->name = name;
	f->state = 0;
	f->rproc = NULL;
	f->wproc = NULL;
	LIST_INSERT_HEAD(&file_list, f, entry);
	DPRINTF("file_new: %s\n", f->name);
	return f;
}

void
file_del(struct file *f)
{
	DPRINTF("file_del: %s|%x\n", f->name, f->state);
}

int
file_poll(void)
{
#ifdef DEBUG
	int ndead;
#endif
	nfds_t nfds;
	struct pollfd pfds[MAXFDS];
	struct pollfd *pfd;
	struct file *f, *fnext;

	nfds = 0;
#ifdef DEBUG
	ndead = 0;
#endif
	LIST_FOREACH(f, &file_list, entry) {
		if (!f->events) {
#ifdef DEBUG
			if (f->state & (FILE_EOF | FILE_HUP))
				ndead++;
#endif
			f->pfd = NULL;
			continue;
		}
		pfd = &pfds[nfds++];
		f->pfd = pfd;
		pfd->fd = f->fd;
		pfd->events = f->events;
	}

#ifdef DEBUG
	if (debug_level >= 4) {
		fprintf(stderr, "file_poll:");
		LIST_FOREACH(f, &file_list, entry) {
			fprintf(stderr, " %s(%x)", f->name, f->events);
		}
		fprintf(stderr, "\n");
	}
	if (nfds == 0 && ndead == 0 && !LIST_EMPTY(&file_list)) {
		fprintf(stderr, "file_poll: deadlock\n");
		abort();
	}
#endif
	if (LIST_EMPTY(&file_list)) {
		DPRINTF("file_poll: nothing to do...\n");
		return 0;
	}
	if (nfds) {
		while (poll(pfds, nfds, -1) < 0) {
			if (errno != EINTR)
				err(1, "file_poll: poll failed");
		}
	}
	LIST_FOREACH(f, &file_list, entry) {
		pfd = f->pfd;
		if (pfd == NULL)
			continue;
		if ((f->events & POLLIN) && (pfd->revents & POLLIN)) {
			f->events &= ~POLLIN;
			f->state |= FILE_ROK;
			DPRINTFN(3, "file_poll: %s rok\n", f->name);
			while (f->state & FILE_ROK) {
				if (!f->rproc->ops->in(f->rproc, NULL))
					break;
			}
		}
		if ((f->events & POLLOUT) && (pfd->revents & POLLOUT)) {
			f->events &= ~POLLOUT;
			f->state |= FILE_WOK;
			DPRINTFN(3, "file_poll: %s wok\n", f->name);
			while (f->state & FILE_WOK) {
				if (!f->wproc->ops->out(f->wproc, NULL))
					break;
			}
		}
	}
	LIST_FOREACH(f, &file_list, entry) {
		if (f->state & FILE_EOF) {
			DPRINTFN(2, "file_poll: %s: eof\n", f->name);
			f->rproc->ops->eof(f->rproc, NULL);
			f->state &= ~FILE_EOF;
		}
		if (f->state & FILE_HUP) {
			DPRINTFN(2, "file_poll: %s hup\n", f->name);
			f->wproc->ops->hup(f->wproc, NULL);
			f->state &= ~FILE_HUP;
		}
	}
	for (f = LIST_FIRST(&file_list); f != NULL; f = fnext) {
		fnext = LIST_NEXT(f, entry);
		if (f->rproc == NULL && f->wproc == NULL) {
			LIST_REMOVE(f, entry);
			DPRINTF("file_poll: %s: deleted\n", f->name);
			free(f);
		}
	}
	if (LIST_EMPTY(&file_list)) {
		DPRINTFN(2, "file_poll: terminated\n");
		return 0;
	}
	return 1;
}

void
file_start(void)
{
	sigset_t set;

	sigemptyset(&set);
	(void)sigaddset(&set, SIGPIPE);
	if (sigprocmask(SIG_BLOCK, &set, NULL))
		err(1, "sigprocmask");

	LIST_INIT(&file_list);
}

void
file_stop(void)
{
	struct file *f;

	if (!LIST_EMPTY(&file_list)) {
		fprintf(stderr, "file_stop:");
		LIST_FOREACH(f, &file_list, entry) {
			fprintf(stderr, " %s(%x)", f->name, f->events);
		}
		fprintf(stderr, "\nfile_stop: list not empty\n");
		exit(1);
	}
}

unsigned
file_read(struct file *file, unsigned char *data, unsigned count)
{
	int n;
	
	if (file->rbytes >= 0 && count > file->rbytes) {
		count = file->rbytes; /* file->rbytes fits in count */
		if (count == 0) {
			DPRINTFN(2, "file_read: %s: complete\n", file->name);
			file->state &= ~FILE_ROK;
			file->state |= FILE_EOF;
			return 0;
		}
	}
	while ((n = read(file->fd, data, count)) < 0) {
		if (errno == EINTR)
			continue;
		file->state &= ~FILE_ROK;
		if (errno == EAGAIN) {
			DPRINTFN(3, "file_read: %s: blocking...\n",
			    file->name);
			file->events |= POLLIN;
		} else {
			warn("%s", file->name);
			file->state |= FILE_EOF;
		}
		return 0;
	}
	if (n == 0) {
		DPRINTFN(2, "file_read: %s: eof\n", file->name);
		file->state &= ~FILE_ROK;
		file->state |= FILE_EOF;
		return 0;
	}
	if (file->rbytes >= 0)
		file->rbytes -= n;
	DPRINTFN(4, "file_read: %s: got %d bytes\n", file->name, n);
	return n;
}


unsigned
file_write(struct file *file, unsigned char *data, unsigned count)
{
	int n;
	
	if (file->wbytes >= 0 && count > file->wbytes) {
		count = file->wbytes; /* file->wbytes fits in count */
		if (count == 0) {
			DPRINTFN(2, "file_write: %s: complete\n", file->name);
			file->state &= ~FILE_WOK;
			file->state |= FILE_HUP;
			return 0;
		}
	}
	while ((n = write(file->fd, data, count)) < 0) {
		if (errno == EINTR)
			continue;
		file->state &= ~FILE_WOK;
		if (errno == EAGAIN) {
			DPRINTFN(3, "file_write: %s: blocking...\n",
			    file->name);
			file->events |= POLLOUT;
		} else {
			warn("%s", file->name);
			file->state |= FILE_HUP;
		}
		return 0;
	}
	if (file->wbytes >= 0)
		file->wbytes -= n;
	DPRINTFN(4, "file_write: %s: wrote %d bytes\n", file->name, n);
	return n;
}

void
file_eof(struct file *f)
{
	DPRINTFN(2, "file_eof: %s: scheduled for eof\n", f->name);
	f->events &= ~POLLIN;
	f->state &= ~FILE_ROK;
	f->state |= FILE_EOF;
}

void
file_hup(struct file *f)
{
	DPRINTFN(2, "file_hup: %s: scheduled for hup\n", f->name);
	f->events &= ~POLLOUT;
	f->state &= ~FILE_WOK;
	f->state |= FILE_HUP;
}
