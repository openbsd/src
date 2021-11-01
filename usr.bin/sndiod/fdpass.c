/*	$OpenBSD: fdpass.c,v 1.11 2021/11/01 14:43:25 ratchov Exp $	*/
/*
 * Copyright (c) 2015 Alexandre Ratchov <alex@caoua.org>
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
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sndio.h>
#include <string.h>
#include <unistd.h>
#include "dev.h"
#include "fdpass.h"
#include "file.h"
#include "listen.h"
#include "midi.h"
#include "sock.h"
#include "utils.h"

struct fdpass_msg {
#define FDPASS_OPEN_SND		0	/* open an audio device */
#define FDPASS_OPEN_MIDI	1	/* open a midi port */
#define FDPASS_OPEN_CTL		2	/* open an audio control device */
#define FDPASS_RETURN		3	/* return after above commands */
	unsigned int cmd;		/* one of above */
	unsigned int num;		/* audio device or midi port number */
	unsigned int mode;		/* SIO_PLAY, SIO_REC, MIO_IN, ... */
};

int fdpass_pollfd(void *, struct pollfd *);
int fdpass_revents(void *, struct pollfd *);
void fdpass_in_worker(void *);
void fdpass_in_helper(void *);
void fdpass_out(void *);
void fdpass_hup(void *);

struct fileops worker_fileops = {
	"worker",
	fdpass_pollfd,
	fdpass_revents,
	fdpass_in_worker,
	fdpass_out,
	fdpass_hup
};

struct fileops helper_fileops = {
	"helper",
	fdpass_pollfd,
	fdpass_revents,
	fdpass_in_helper,
	fdpass_out,
	fdpass_hup
};

struct fdpass {
	struct file *file;
	int fd;
} *fdpass_peer = NULL;

static void
fdpass_log(struct fdpass *f)
{
	log_puts(f->file->name);
}

static int
fdpass_send(struct fdpass *f, int cmd, int num, int mode, int fd)
{
	struct fdpass_msg data;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	union {
		struct cmsghdr hdr;
		unsigned char buf[CMSG_SPACE(sizeof(int))];
	} cmsgbuf;
	struct iovec iov;
	ssize_t n;

	data.cmd = cmd;
	data.num = num;
	data.mode = mode;
	iov.iov_base = &data;
	iov.iov_len = sizeof(struct fdpass_msg);
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	if (fd >= 0) {
		msg.msg_control = &cmsgbuf.buf;
		msg.msg_controllen = sizeof(cmsgbuf.buf);
		cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		*(int *)CMSG_DATA(cmsg) = fd;
	}
	n = sendmsg(f->fd, &msg, 0);
	if (n == -1) {
		if (log_level >= 1) {
			fdpass_log(f);
			log_puts(": sendmsg failed\n");
		}
		fdpass_close(f);
		return 0;
	}
	if (n != sizeof(struct fdpass_msg)) {
		if (log_level >= 1) {
			fdpass_log(f);
			log_puts(": short write\n");
		}
		fdpass_close(f);
		return 0;
	}
#ifdef DEBUG
	if (log_level >= 3) {
		fdpass_log(f);
		log_puts(": send: cmd = ");
		log_puti(cmd);
		log_puts(", num = ");
		log_puti(num);
		log_puts(", mode = ");
		log_puti(mode);
		log_puts(", fd = ");
		log_puti(fd);
		log_puts("\n");
	}
#endif
	if (fd >= 0)
		close(fd);
	return 1;
}

static int
fdpass_recv(struct fdpass *f, int *cmd, int *num, int *mode, int *fd)
{
	struct fdpass_msg data;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	union {
		struct cmsghdr hdr;
		unsigned char buf[CMSG_SPACE(sizeof(int))];
	} cmsgbuf;
	struct iovec iov;
	ssize_t n;

	iov.iov_base = &data;
	iov.iov_len = sizeof(struct fdpass_msg);
	memset(&msg, 0, sizeof(msg));
	msg.msg_control = &cmsgbuf.buf;
	msg.msg_controllen = sizeof(cmsgbuf.buf);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	n = recvmsg(f->fd, &msg, MSG_WAITALL);
	if (n == -1 && errno == EMSGSIZE) {
		if (log_level >= 1) {
			fdpass_log(f);
			log_puts(": out of fds\n");
		}
		/*
		 * ancillary data (ie the fd) is discarded,
		 * retrieve the message
		 */
		n = recvmsg(f->fd, &msg, MSG_WAITALL);
	}
	if (n == -1) {
		if (log_level >= 1) {
			fdpass_log(f);
			log_puts(": recvmsg failed\n");
		}
		fdpass_close(f);
		return 0;
	}
	if (n == 0) {
		if (log_level >= 3) {
			fdpass_log(f);
			log_puts(": recvmsg eof\n");
		}
		fdpass_close(f);
		return 0;
	}
	if (msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) {
		if (log_level >= 1) {
			fdpass_log(f);
			log_puts(": truncated\n");
		}
		fdpass_close(f);
		return 0;
	}
	cmsg = CMSG_FIRSTHDR(&msg);
	for (;;) {
		if (cmsg == NULL) {
			*fd = -1;
			break;
		}
		if (cmsg->cmsg_len == CMSG_LEN(sizeof(int)) &&
		    cmsg->cmsg_level == SOL_SOCKET &&
		    cmsg->cmsg_type == SCM_RIGHTS) {
			*fd = *(int *)CMSG_DATA(cmsg);
			break;
		}
		cmsg = CMSG_NXTHDR(&msg, cmsg);
	}
	*cmd = data.cmd;
	*num = data.num;
	*mode = data.mode;
#ifdef DEBUG
	if (log_level >= 3) {
		fdpass_log(f);
		log_puts(": recv: cmd = ");
		log_puti(*cmd);
		log_puts(", num = ");
		log_puti(*num);
		log_puts(", mode = ");
		log_puti(*mode);
		log_puts(", fd = ");
		log_puti(*fd);
		log_puts("\n");
	}
#endif
	return 1;
}

static int
fdpass_waitret(struct fdpass *f, int *retfd)
{
	int cmd, unused;

	if (!fdpass_recv(fdpass_peer, &cmd, &unused, &unused, retfd))
		return 0;
	if (cmd != FDPASS_RETURN) {
		if (log_level >= 1) {
			fdpass_log(f);
			log_puts(": expected RETURN message\n");
		}
		fdpass_close(f);
		return 0;
	}
	return 1;
}

struct sio_hdl *
fdpass_sio_open(int num, unsigned int mode)
{
	int fd;

	if (fdpass_peer == NULL)
		return NULL;
	if (!fdpass_send(fdpass_peer, FDPASS_OPEN_SND, num, mode, -1))
		return NULL;
	if (!fdpass_waitret(fdpass_peer, &fd))
		return NULL;
	if (fd < 0)
		return NULL;
	return sio_sun_fdopen(fd, mode, 1);
}

struct mio_hdl *
fdpass_mio_open(int num, unsigned int mode)
{
	int fd;

	if (fdpass_peer == NULL)
		return NULL;
	if (!fdpass_send(fdpass_peer, FDPASS_OPEN_MIDI, num, mode, -1))
		return NULL;
	if (!fdpass_waitret(fdpass_peer, &fd))
		return NULL;
	if (fd < 0)
		return NULL;
	return mio_rmidi_fdopen(fd, mode, 1);
}

struct sioctl_hdl *
fdpass_sioctl_open(int num, unsigned int mode)
{
	int fd;

	if (fdpass_peer == NULL)
		return NULL;
	if (!fdpass_send(fdpass_peer, FDPASS_OPEN_CTL, num, mode, -1))
		return NULL;
	if (!fdpass_waitret(fdpass_peer, &fd))
		return NULL;
	if (fd < 0)
		return NULL;
	return sioctl_sun_fdopen(fd, mode, 1);
}

void
fdpass_in_worker(void *arg)
{
	struct fdpass *f = arg;

	if (log_level >= 3) {
		fdpass_log(f);
		log_puts(": exit\n");
	}
	fdpass_close(f);
	return;
}

void
fdpass_in_helper(void *arg)
{
	int cmd, num, mode, fd;
	struct fdpass *f = arg;
	struct dev *d;
	struct port *p;

	if (!fdpass_recv(f, &cmd, &num, &mode, &fd))
		return;
	switch (cmd) {
	case FDPASS_OPEN_SND:
		d = dev_bynum(num);
		if (d == NULL || !(mode & (SIO_PLAY | SIO_REC))) {
			if (log_level >= 1) {
				fdpass_log(f);
				log_puts(": bad audio device or mode\n");
			}
			fdpass_close(f);
			return;
		}
		fd = sio_sun_getfd(d->path, mode, 1);
		break;
	case FDPASS_OPEN_MIDI:
		p = port_bynum(num);
		if (p == NULL || !(mode & (MIO_IN | MIO_OUT))) {
			if (log_level >= 1) {
				fdpass_log(f);
				log_puts(": bad midi port or mode\n");
			}
			fdpass_close(f);
			return;
		}
		fd = mio_rmidi_getfd(p->path, mode, 1);
		break;
	case FDPASS_OPEN_CTL:
		d = dev_bynum(num);
		if (d == NULL || !(mode & (SIOCTL_READ | SIOCTL_WRITE))) {
			if (log_level >= 1) {
				fdpass_log(f);
				log_puts(": bad audio control device\n");
			}
			fdpass_close(f);
			return;
		}
		fd = sioctl_sun_getfd(d->path, mode, 1);
		break;
	default:
		fdpass_close(f);
		return;
	}
	fdpass_send(f, FDPASS_RETURN, 0, 0, fd);
}

void
fdpass_out(void *arg)
{
}

void
fdpass_hup(void *arg)
{
	struct fdpass *f = arg;

	if (log_level >= 3) {
		fdpass_log(f);
		log_puts(": hup\n");
	}
	fdpass_close(f);
}

struct fdpass *
fdpass_new(int sock, struct fileops *ops)
{
	struct fdpass *f;

	f = xmalloc(sizeof(struct fdpass));
	f->file = file_new(ops, f, ops->name, 1);
	if (f->file == NULL) {
		close(sock);
		xfree(f);
		return NULL;
	}
	f->fd = sock;
	fdpass_peer = f;
	return f;
}

void
fdpass_close(struct fdpass *f)
{
	fdpass_peer = NULL;
	file_del(f->file);
	close(f->fd);
	xfree(f);
}

int
fdpass_pollfd(void *arg, struct pollfd *pfd)
{
	struct fdpass *f = arg;

	pfd->fd = f->fd;
	pfd->events = POLLIN;
	return 1;
}

int
fdpass_revents(void *arg, struct pollfd *pfd)
{
	return pfd->revents;
}
