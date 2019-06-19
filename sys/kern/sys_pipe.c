/*	$OpenBSD: sys_pipe.c,v 1.87 2018/11/13 13:02:20 visa Exp $	*/

/*
 * Copyright (c) 1996 John S. Dyson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Absolutely no warranty of function or purpose is made by the author
 *    John S. Dyson.
 * 4. Modifications may be freely made to this file if the above conditions
 *    are met.
 */

/*
 * This file contains a high-performance replacement for the socket-based
 * pipes scheme originally used in FreeBSD/4.4Lite.  It does not support
 * all features of sockets, but does do everything that pipes normally
 * do.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/pool.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/signalvar.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/event.h>
#include <sys/lock.h>
#include <sys/poll.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <uvm/uvm_extern.h>

#include <sys/pipe.h>

/*
 * interfaces to the outside world
 */
int	pipe_read(struct file *, struct uio *, int);
int	pipe_write(struct file *, struct uio *, int);
int	pipe_close(struct file *, struct proc *);
int	pipe_poll(struct file *, int events, struct proc *);
int	pipe_kqfilter(struct file *fp, struct knote *kn);
int	pipe_ioctl(struct file *, u_long, caddr_t, struct proc *);
int	pipe_stat(struct file *fp, struct stat *ub, struct proc *p);

static struct fileops pipeops = {
	.fo_read	= pipe_read,
	.fo_write	= pipe_write,
	.fo_ioctl	= pipe_ioctl,
	.fo_poll	= pipe_poll,
	.fo_kqfilter	= pipe_kqfilter,
	.fo_stat	= pipe_stat,
	.fo_close	= pipe_close
};

void	filt_pipedetach(struct knote *kn);
int	filt_piperead(struct knote *kn, long hint);
int	filt_pipewrite(struct knote *kn, long hint);

struct filterops pipe_rfiltops =
	{ 1, NULL, filt_pipedetach, filt_piperead };
struct filterops pipe_wfiltops =
	{ 1, NULL, filt_pipedetach, filt_pipewrite };

/*
 * Default pipe buffer size(s), this can be kind-of large now because pipe
 * space is pageable.  The pipe code will try to maintain locality of
 * reference for performance reasons, so small amounts of outstanding I/O
 * will not wipe the cache.
 */
#define MINPIPESIZE (PIPE_SIZE/3)

/*
 * Limit the number of "big" pipes
 */
#define LIMITBIGPIPES	32
unsigned int nbigpipe;
static unsigned int amountpipekva;

struct pool pipe_pool;

int	dopipe(struct proc *, int *, int);
void	pipeclose(struct pipe *);
void	pipe_free_kmem(struct pipe *);
int	pipe_create(struct pipe *);
int	pipelock(struct pipe *);
void	pipeunlock(struct pipe *);
void	pipeselwakeup(struct pipe *);
int	pipespace(struct pipe *, u_int);

/*
 * The pipe system call for the DTYPE_PIPE type of pipes
 */

int
sys_pipe(struct proc *p, void *v, register_t *retval)
{
	struct sys_pipe_args /* {
		syscallarg(int *) fdp;
	} */ *uap = v;

	return (dopipe(p, SCARG(uap, fdp), 0));
}

int
sys_pipe2(struct proc *p, void *v, register_t *retval)
{
	struct sys_pipe2_args /* {
		syscallarg(int *) fdp;
		syscallarg(int) flags;
	} */ *uap = v;

	if (SCARG(uap, flags) & ~(O_CLOEXEC | FNONBLOCK))
		return (EINVAL);

	return (dopipe(p, SCARG(uap, fdp), SCARG(uap, flags)));
}

int
dopipe(struct proc *p, int *ufds, int flags)
{
	struct filedesc *fdp = p->p_fd;
	struct file *rf, *wf;
	struct pipe *rpipe, *wpipe = NULL;
	int fds[2], cloexec, error;

	cloexec = (flags & O_CLOEXEC) ? UF_EXCLOSE : 0;

	rpipe = pool_get(&pipe_pool, PR_WAITOK);
	error = pipe_create(rpipe);
	if (error != 0)
		goto free1;
	wpipe = pool_get(&pipe_pool, PR_WAITOK);
	error = pipe_create(wpipe);
	if (error != 0)
		goto free1;

	fdplock(fdp);

	error = falloc(p, &rf, &fds[0]);
	if (error != 0)
		goto free2;
	rf->f_flag = FREAD | FWRITE | (flags & FNONBLOCK);
	rf->f_type = DTYPE_PIPE;
	rf->f_data = rpipe;
	rf->f_ops = &pipeops;

	error = falloc(p, &wf, &fds[1]);
	if (error != 0)
		goto free3;
	wf->f_flag = FREAD | FWRITE | (flags & FNONBLOCK);
	wf->f_type = DTYPE_PIPE;
	wf->f_data = wpipe;
	wf->f_ops = &pipeops;

	rpipe->pipe_peer = wpipe;
	wpipe->pipe_peer = rpipe;

	fdinsert(fdp, fds[0], cloexec, rf);
	fdinsert(fdp, fds[1], cloexec, wf);

	error = copyout(fds, ufds, sizeof(fds));
	if (error != 0) {
		fdrelease(p, fds[0]);
		fdrelease(p, fds[1]);
	}
#ifdef KTRACE
	else if (KTRPOINT(p, KTR_STRUCT))
		ktrfds(p, fds, 2);
#endif
	fdpunlock(fdp);

	FRELE(rf, p);
	FRELE(wf, p);
	return (error);

free3:
	fdremove(fdp, fds[0]);
	closef(rf, p);
	rpipe = NULL;
free2:
	fdpunlock(fdp);
free1:
	pipeclose(wpipe);
	pipeclose(rpipe);
	return (error);
}

/*
 * Allocate kva for pipe circular buffer, the space is pageable.
 * This routine will 'realloc' the size of a pipe safely, if it fails
 * it will retain the old buffer.
 * If it fails it will return ENOMEM.
 */
int
pipespace(struct pipe *cpipe, u_int size)
{
	caddr_t buffer;

	KERNEL_LOCK();
	buffer = km_alloc(size, &kv_any, &kp_pageable, &kd_waitok);
	KERNEL_UNLOCK();
	if (buffer == NULL) {
		return (ENOMEM);
	}

	/* free old resources if we are resizing */
	pipe_free_kmem(cpipe);
	cpipe->pipe_buffer.buffer = buffer;
	cpipe->pipe_buffer.size = size;
	cpipe->pipe_buffer.in = 0;
	cpipe->pipe_buffer.out = 0;
	cpipe->pipe_buffer.cnt = 0;

	atomic_add_int(&amountpipekva, cpipe->pipe_buffer.size);

	return (0);
}

/*
 * initialize and allocate VM and memory for pipe
 */
int
pipe_create(struct pipe *cpipe)
{
	int error;

	/* so pipe_free_kmem() doesn't follow junk pointer */
	cpipe->pipe_buffer.buffer = NULL;
	/*
	 * protect so pipeclose() doesn't follow a junk pointer
	 * if pipespace() fails.
	 */
	memset(&cpipe->pipe_sel, 0, sizeof(cpipe->pipe_sel));
	cpipe->pipe_state = 0;
	cpipe->pipe_peer = NULL;
	cpipe->pipe_busy = 0;
	sigio_init(&cpipe->pipe_sigio);

	error = pipespace(cpipe, PIPE_SIZE);
	if (error != 0)
		return (error);

	getnanotime(&cpipe->pipe_ctime);
	cpipe->pipe_atime = cpipe->pipe_ctime;
	cpipe->pipe_mtime = cpipe->pipe_ctime;

	return (0);
}


/*
 * lock a pipe for I/O, blocking other access
 */
int
pipelock(struct pipe *cpipe)
{
	int error;
	while (cpipe->pipe_state & PIPE_LOCK) {
		cpipe->pipe_state |= PIPE_LWANT;
		if ((error = tsleep(cpipe, PRIBIO|PCATCH, "pipelk", 0)))
			return error;
	}
	cpipe->pipe_state |= PIPE_LOCK;
	return 0;
}

/*
 * unlock a pipe I/O lock
 */
void
pipeunlock(struct pipe *cpipe)
{
	cpipe->pipe_state &= ~PIPE_LOCK;
	if (cpipe->pipe_state & PIPE_LWANT) {
		cpipe->pipe_state &= ~PIPE_LWANT;
		wakeup(cpipe);
	}
}

void
pipeselwakeup(struct pipe *cpipe)
{
	if (cpipe->pipe_state & PIPE_SEL) {
		cpipe->pipe_state &= ~PIPE_SEL;
		selwakeup(&cpipe->pipe_sel);
	} else
		KNOTE(&cpipe->pipe_sel.si_note, 0);
	if (cpipe->pipe_state & PIPE_ASYNC)
		pgsigio(&cpipe->pipe_sigio, SIGIO, 0);
}

int
pipe_read(struct file *fp, struct uio *uio, int fflags)
{
	struct pipe *rpipe = fp->f_data;
	int error;
	size_t size, nread = 0;

	error = pipelock(rpipe);
	if (error)
		return (error);

	++rpipe->pipe_busy;

	while (uio->uio_resid) {
		/*
		 * normal pipe buffer receive
		 */
		if (rpipe->pipe_buffer.cnt > 0) {
			size = rpipe->pipe_buffer.size - rpipe->pipe_buffer.out;
			if (size > rpipe->pipe_buffer.cnt)
				size = rpipe->pipe_buffer.cnt;
			if (size > uio->uio_resid)
				size = uio->uio_resid;
			error = uiomove(&rpipe->pipe_buffer.buffer[rpipe->pipe_buffer.out],
					size, uio);
			if (error) {
				break;
			}
			rpipe->pipe_buffer.out += size;
			if (rpipe->pipe_buffer.out >= rpipe->pipe_buffer.size)
				rpipe->pipe_buffer.out = 0;

			rpipe->pipe_buffer.cnt -= size;
			/*
			 * If there is no more to read in the pipe, reset
			 * its pointers to the beginning.  This improves
			 * cache hit stats.
			 */
			if (rpipe->pipe_buffer.cnt == 0) {
				rpipe->pipe_buffer.in = 0;
				rpipe->pipe_buffer.out = 0;
			}
			nread += size;
		} else {
			/*
			 * detect EOF condition
			 * read returns 0 on EOF, no need to set error
			 */
			if (rpipe->pipe_state & PIPE_EOF)
				break;

			/*
			 * If the "write-side" has been blocked, wake it up now.
			 */
			if (rpipe->pipe_state & PIPE_WANTW) {
				rpipe->pipe_state &= ~PIPE_WANTW;
				wakeup(rpipe);
			}

			/*
			 * Break if some data was read.
			 */
			if (nread > 0)
				break;

			/*
			 * Unlock the pipe buffer for our remaining processing.
			 * We will either break out with an error or we will
			 * sleep and relock to loop.
			 */
			pipeunlock(rpipe);

			/*
			 * Handle non-blocking mode operation or
			 * wait for more data.
			 */
			if (fp->f_flag & FNONBLOCK) {
				error = EAGAIN;
			} else {
				rpipe->pipe_state |= PIPE_WANTR;
				if ((error = tsleep(rpipe, PRIBIO|PCATCH, "piperd", 0)) == 0)
					error = pipelock(rpipe);
			}
			if (error)
				goto unlocked_error;
		}
	}
	pipeunlock(rpipe);

	if (error == 0)
		getnanotime(&rpipe->pipe_atime);
unlocked_error:
	--rpipe->pipe_busy;

	/*
	 * PIPE_WANT processing only makes sense if pipe_busy is 0.
	 */
	if ((rpipe->pipe_busy == 0) && (rpipe->pipe_state & PIPE_WANT)) {
		rpipe->pipe_state &= ~(PIPE_WANT|PIPE_WANTW);
		wakeup(rpipe);
	} else if (rpipe->pipe_buffer.cnt < MINPIPESIZE) {
		/*
		 * Handle write blocking hysteresis.
		 */
		if (rpipe->pipe_state & PIPE_WANTW) {
			rpipe->pipe_state &= ~PIPE_WANTW;
			wakeup(rpipe);
		}
	}

	if ((rpipe->pipe_buffer.size - rpipe->pipe_buffer.cnt) >= PIPE_BUF)
		pipeselwakeup(rpipe);

	return (error);
}

int
pipe_write(struct file *fp, struct uio *uio, int fflags)
{
	int error = 0;
	size_t orig_resid;
	struct pipe *wpipe, *rpipe;

	rpipe = fp->f_data;
	wpipe = rpipe->pipe_peer;

	/*
	 * detect loss of pipe read side, issue SIGPIPE if lost.
	 */
	if ((wpipe == NULL) || (wpipe->pipe_state & PIPE_EOF)) {
		return (EPIPE);
	}
	++wpipe->pipe_busy;

	/*
	 * If it is advantageous to resize the pipe buffer, do
	 * so.
	 */
	if ((uio->uio_resid > PIPE_SIZE) &&
	    (wpipe->pipe_buffer.size <= PIPE_SIZE) &&
	    (wpipe->pipe_buffer.cnt == 0)) {
	    	unsigned int npipe;

		npipe = atomic_inc_int_nv(&nbigpipe);
		if ((npipe <= LIMITBIGPIPES) &&
		    (error = pipelock(wpipe)) == 0) {
			if (pipespace(wpipe, BIG_PIPE_SIZE) != 0)
				atomic_dec_int(&nbigpipe);
			pipeunlock(wpipe);
		} else
			atomic_dec_int(&nbigpipe);
	}

	/*
	 * If an early error occurred unbusy and return, waking up any pending
	 * readers.
	 */
	if (error) {
		--wpipe->pipe_busy;
		if ((wpipe->pipe_busy == 0) &&
		    (wpipe->pipe_state & PIPE_WANT)) {
			wpipe->pipe_state &= ~(PIPE_WANT | PIPE_WANTR);
			wakeup(wpipe);
		}
		return (error);
	}

	orig_resid = uio->uio_resid;

	while (uio->uio_resid) {
		size_t space;

retrywrite:
		if (wpipe->pipe_state & PIPE_EOF) {
			error = EPIPE;
			break;
		}

		space = wpipe->pipe_buffer.size - wpipe->pipe_buffer.cnt;

		/* Writes of size <= PIPE_BUF must be atomic. */
		if ((space < uio->uio_resid) && (orig_resid <= PIPE_BUF))
			space = 0;

		if (space > 0) {
			if ((error = pipelock(wpipe)) == 0) {
				size_t size;	/* Transfer size */
				size_t segsize;	/* first segment to transfer */

				/*
				 * If a process blocked in uiomove, our
				 * value for space might be bad.
				 *
				 * XXX will we be ok if the reader has gone
				 * away here?
				 */
				if (space > wpipe->pipe_buffer.size -
				    wpipe->pipe_buffer.cnt) {
					pipeunlock(wpipe);
					goto retrywrite;
				}

				/*
				 * Transfer size is minimum of uio transfer
				 * and free space in pipe buffer.
				 */
				if (space > uio->uio_resid)
					size = uio->uio_resid;
				else
					size = space;
				/*
				 * First segment to transfer is minimum of
				 * transfer size and contiguous space in
				 * pipe buffer.  If first segment to transfer
				 * is less than the transfer size, we've got
				 * a wraparound in the buffer.
				 */
				segsize = wpipe->pipe_buffer.size -
					wpipe->pipe_buffer.in;
				if (segsize > size)
					segsize = size;

				/* Transfer first segment */

				error = uiomove(&wpipe->pipe_buffer.buffer[wpipe->pipe_buffer.in],
						segsize, uio);

				if (error == 0 && segsize < size) {
					/*
					 * Transfer remaining part now, to
					 * support atomic writes.  Wraparound
					 * happened.
					 */
#ifdef DIAGNOSTIC
					if (wpipe->pipe_buffer.in + segsize !=
					    wpipe->pipe_buffer.size)
						panic("Expected pipe buffer wraparound disappeared");
#endif

					error = uiomove(&wpipe->pipe_buffer.buffer[0],
							size - segsize, uio);
				}
				if (error == 0) {
					wpipe->pipe_buffer.in += size;
					if (wpipe->pipe_buffer.in >=
					    wpipe->pipe_buffer.size) {
#ifdef DIAGNOSTIC
						if (wpipe->pipe_buffer.in != size - segsize + wpipe->pipe_buffer.size)
							panic("Expected wraparound bad");
#endif
						wpipe->pipe_buffer.in = size - segsize;
					}

					wpipe->pipe_buffer.cnt += size;
#ifdef DIAGNOSTIC
					if (wpipe->pipe_buffer.cnt > wpipe->pipe_buffer.size)
						panic("Pipe buffer overflow");
#endif
				}
				pipeunlock(wpipe);
			}
			if (error)
				break;
		} else {
			/*
			 * If the "read-side" has been blocked, wake it up now.
			 */
			if (wpipe->pipe_state & PIPE_WANTR) {
				wpipe->pipe_state &= ~PIPE_WANTR;
				wakeup(wpipe);
			}

			/*
			 * don't block on non-blocking I/O
			 */
			if (fp->f_flag & FNONBLOCK) {
				error = EAGAIN;
				break;
			}

			/*
			 * We have no more space and have something to offer,
			 * wake up select/poll.
			 */
			pipeselwakeup(wpipe);

			wpipe->pipe_state |= PIPE_WANTW;
			error = tsleep(wpipe, (PRIBIO + 1)|PCATCH,
			    "pipewr", 0);
			if (error)
				break;
			/*
			 * If read side wants to go away, we just issue a
			 * signal to ourselves.
			 */
			if (wpipe->pipe_state & PIPE_EOF) {
				error = EPIPE;
				break;
			}	
		}
	}

	--wpipe->pipe_busy;

	if ((wpipe->pipe_busy == 0) && (wpipe->pipe_state & PIPE_WANT)) {
		wpipe->pipe_state &= ~(PIPE_WANT | PIPE_WANTR);
		wakeup(wpipe);
	} else if (wpipe->pipe_buffer.cnt > 0) {
		/*
		 * If we have put any characters in the buffer, we wake up
		 * the reader.
		 */
		if (wpipe->pipe_state & PIPE_WANTR) {
			wpipe->pipe_state &= ~PIPE_WANTR;
			wakeup(wpipe);
		}
	}

	/*
	 * Don't return EPIPE if I/O was successful
	 */
	if ((wpipe->pipe_buffer.cnt == 0) &&
	    (uio->uio_resid == 0) &&
	    (error == EPIPE)) {
		error = 0;
	}

	if (error == 0)
		getnanotime(&wpipe->pipe_mtime);
	/*
	 * We have something to offer, wake up select/poll.
	 */
	if (wpipe->pipe_buffer.cnt)
		pipeselwakeup(wpipe);

	return (error);
}

/*
 * we implement a very minimal set of ioctls for compatibility with sockets.
 */
int
pipe_ioctl(struct file *fp, u_long cmd, caddr_t data, struct proc *p)
{
	struct pipe *mpipe = fp->f_data;

	switch (cmd) {

	case FIONBIO:
		return (0);

	case FIOASYNC:
		if (*(int *)data) {
			mpipe->pipe_state |= PIPE_ASYNC;
		} else {
			mpipe->pipe_state &= ~PIPE_ASYNC;
		}
		return (0);

	case FIONREAD:
		*(int *)data = mpipe->pipe_buffer.cnt;
		return (0);

	case TIOCSPGRP:
		/* FALLTHROUGH */
	case SIOCSPGRP:
		return (sigio_setown(&mpipe->pipe_sigio, *(int *)data));

	case SIOCGPGRP:
		*(int *)data = sigio_getown(&mpipe->pipe_sigio);
		return (0);

	case TIOCGPGRP:
		*(int *)data = -sigio_getown(&mpipe->pipe_sigio);
		return (0);

	}
	return (ENOTTY);
}

int
pipe_poll(struct file *fp, int events, struct proc *p)
{
	struct pipe *rpipe = fp->f_data;
	struct pipe *wpipe;
	int revents = 0;

	wpipe = rpipe->pipe_peer;
	if (events & (POLLIN | POLLRDNORM)) {
		if ((rpipe->pipe_buffer.cnt > 0) ||
		    (rpipe->pipe_state & PIPE_EOF))
			revents |= events & (POLLIN | POLLRDNORM);
	}

	/* NOTE: POLLHUP and POLLOUT/POLLWRNORM are mutually exclusive */
	if ((rpipe->pipe_state & PIPE_EOF) ||
	    (wpipe == NULL) ||
	    (wpipe->pipe_state & PIPE_EOF))
		revents |= POLLHUP;
	else if (events & (POLLOUT | POLLWRNORM)) {
		if ((wpipe->pipe_buffer.size - wpipe->pipe_buffer.cnt) >= PIPE_BUF)
			revents |= events & (POLLOUT | POLLWRNORM);
	}

	if (revents == 0) {
		if (events & (POLLIN | POLLRDNORM)) {
			selrecord(p, &rpipe->pipe_sel);
			rpipe->pipe_state |= PIPE_SEL;
		}
		if (events & (POLLOUT | POLLWRNORM)) {
			selrecord(p, &wpipe->pipe_sel);
			wpipe->pipe_state |= PIPE_SEL;
		}
	}
	return (revents);
}

int
pipe_stat(struct file *fp, struct stat *ub, struct proc *p)
{
	struct pipe *pipe = fp->f_data;

	memset(ub, 0, sizeof(*ub));
	ub->st_mode = S_IFIFO;
	ub->st_blksize = pipe->pipe_buffer.size;
	ub->st_size = pipe->pipe_buffer.cnt;
	ub->st_blocks = (ub->st_size + ub->st_blksize - 1) / ub->st_blksize;
	ub->st_atim.tv_sec  = pipe->pipe_atime.tv_sec;
	ub->st_atim.tv_nsec = pipe->pipe_atime.tv_nsec;
	ub->st_mtim.tv_sec  = pipe->pipe_mtime.tv_sec;
	ub->st_mtim.tv_nsec = pipe->pipe_mtime.tv_nsec;
	ub->st_ctim.tv_sec  = pipe->pipe_ctime.tv_sec;
	ub->st_ctim.tv_nsec = pipe->pipe_ctime.tv_nsec;
	ub->st_uid = fp->f_cred->cr_uid;
	ub->st_gid = fp->f_cred->cr_gid;
	/*
	 * Left as 0: st_dev, st_ino, st_nlink, st_rdev, st_flags, st_gen.
	 * XXX (st_dev, st_ino) should be unique.
	 */
	return (0);
}

int
pipe_close(struct file *fp, struct proc *p)
{
	struct pipe *cpipe = fp->f_data;

	fp->f_ops = NULL;
	fp->f_data = NULL;
	KERNEL_LOCK();
	pipeclose(cpipe);
	KERNEL_UNLOCK();
	return (0);
}

void
pipe_free_kmem(struct pipe *cpipe)
{
	u_int size = cpipe->pipe_buffer.size;

	if (cpipe->pipe_buffer.buffer != NULL) {
		KERNEL_LOCK();
		km_free(cpipe->pipe_buffer.buffer, size, &kv_any, &kp_pageable);
		KERNEL_UNLOCK();
		atomic_sub_int(&amountpipekva, size);
		cpipe->pipe_buffer.buffer = NULL;
		if (size > PIPE_SIZE)
			atomic_dec_int(&nbigpipe);
	}
}

/*
 * shutdown the pipe
 */
void
pipeclose(struct pipe *cpipe)
{
	struct pipe *ppipe;
	if (cpipe) {
		pipeselwakeup(cpipe);
		sigio_free(&cpipe->pipe_sigio);

		/*
		 * If the other side is blocked, wake it up saying that
		 * we want to close it down.
		 */
		cpipe->pipe_state |= PIPE_EOF;
		while (cpipe->pipe_busy) {
			wakeup(cpipe);
			cpipe->pipe_state |= PIPE_WANT;
			tsleep(cpipe, PRIBIO, "pipecl", 0);
		}

		/*
		 * Disconnect from peer
		 */
		if ((ppipe = cpipe->pipe_peer) != NULL) {
			pipeselwakeup(ppipe);

			ppipe->pipe_state |= PIPE_EOF;
			wakeup(ppipe);
			ppipe->pipe_peer = NULL;
		}

		/*
		 * free resources
		 */
		pipe_free_kmem(cpipe);
		pool_put(&pipe_pool, cpipe);
	}
}

int
pipe_kqfilter(struct file *fp, struct knote *kn)
{
	struct pipe *rpipe = kn->kn_fp->f_data;
	struct pipe *wpipe = rpipe->pipe_peer;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &pipe_rfiltops;
		SLIST_INSERT_HEAD(&rpipe->pipe_sel.si_note, kn, kn_selnext);
		break;
	case EVFILT_WRITE:
		if (wpipe == NULL) {
			/* other end of pipe has been closed */
			return (EPIPE);
		}
		kn->kn_fop = &pipe_wfiltops;
		SLIST_INSERT_HEAD(&wpipe->pipe_sel.si_note, kn, kn_selnext);
		break;
	default:
		return (EINVAL);
	}

	return (0);
}

void
filt_pipedetach(struct knote *kn)
{
	struct pipe *rpipe = kn->kn_fp->f_data;
	struct pipe *wpipe = rpipe->pipe_peer;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		SLIST_REMOVE(&rpipe->pipe_sel.si_note, kn, knote, kn_selnext);
		break;
	case EVFILT_WRITE:
		if (wpipe == NULL)
			return;
		SLIST_REMOVE(&wpipe->pipe_sel.si_note, kn, knote, kn_selnext);
		break;
	}
}

int
filt_piperead(struct knote *kn, long hint)
{
	struct pipe *rpipe = kn->kn_fp->f_data;
	struct pipe *wpipe = rpipe->pipe_peer;

	kn->kn_data = rpipe->pipe_buffer.cnt;

	if ((rpipe->pipe_state & PIPE_EOF) ||
	    (wpipe == NULL) || (wpipe->pipe_state & PIPE_EOF)) {
		kn->kn_flags |= EV_EOF; 
		return (1);
	}
	return (kn->kn_data > 0);
}

int
filt_pipewrite(struct knote *kn, long hint)
{
	struct pipe *rpipe = kn->kn_fp->f_data;
	struct pipe *wpipe = rpipe->pipe_peer;

	if ((wpipe == NULL) || (wpipe->pipe_state & PIPE_EOF)) {
		kn->kn_data = 0;
		kn->kn_flags |= EV_EOF; 
		return (1);
	}
	kn->kn_data = wpipe->pipe_buffer.size - wpipe->pipe_buffer.cnt;

	return (kn->kn_data >= PIPE_BUF);
}

void
pipe_init(void)
{
	pool_init(&pipe_pool, sizeof(struct pipe), 0, IPL_MPFLOOR, PR_WAITOK,
	    "pipepl", NULL);
}

