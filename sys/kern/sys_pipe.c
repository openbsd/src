/*	$OpenBSD: sys_pipe.c,v 1.40 2001/11/06 19:53:20 miod Exp $	*/

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
#include <sys/file.h>
#include <sys/protosw.h>
#include <sys/stat.h>
#include <sys/filedesc.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/signalvar.h>
#include <sys/errno.h>
#include <sys/queue.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/event.h>
#include <sys/lock.h>

#include <uvm/uvm_extern.h>

#include <sys/pipe.h>

/*
 * interfaces to the outside world
 */
int	pipe_read __P((struct file *, off_t *, struct uio *, struct ucred *));
int	pipe_write __P((struct file *, off_t *, struct uio *, struct ucred *));
int	pipe_close __P((struct file *, struct proc *));
int	pipe_select __P((struct file *, int which, struct proc *));
int	pipe_kqfilter __P((struct file *fp, struct knote *kn));
int	pipe_ioctl __P((struct file *, u_long, caddr_t, struct proc *));
int	pipe_stat __P((struct file *fp, struct stat *ub, struct proc *p));

static struct fileops pipeops = {
	pipe_read, pipe_write, pipe_ioctl, pipe_select, pipe_kqfilter,
	pipe_stat, pipe_close 
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
int nbigpipe;
static int amountpipekva;

struct pool pipe_pool;

void	pipeclose __P((struct pipe *));
void	pipeinit __P((struct pipe *));
static __inline int pipelock __P((struct pipe *));
static __inline void pipeunlock __P((struct pipe *));
static __inline void pipeselwakeup __P((struct pipe *));
void	pipespace __P((struct pipe *));

/*
 * The pipe system call for the DTYPE_PIPE type of pipes
 */

/* ARGSUSED */
int
sys_opipe(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct filedesc *fdp = p->p_fd;
	struct file *rf, *wf;
	struct pipe *rpipe, *wpipe;
	int fd, error;

	rpipe = pool_get(&pipe_pool, PR_WAITOK);
	pipeinit(rpipe);
	wpipe = pool_get(&pipe_pool, PR_WAITOK);
	pipeinit(wpipe);

	error = falloc(p, &rf, &fd);
	if (error)
		goto free2;
	rf->f_flag = FREAD | FWRITE;
	rf->f_type = DTYPE_PIPE;
	rf->f_ops = &pipeops;
	rf->f_data = (caddr_t)rpipe;
	retval[0] = fd;

	error = falloc(p, &wf, &fd);
	if (error)
		goto free3;
	wf->f_flag = FREAD | FWRITE;
	wf->f_type = DTYPE_PIPE;
	wf->f_ops = &pipeops;
	wf->f_data = (caddr_t)wpipe;
	retval[1] = fd;

	rpipe->pipe_peer = wpipe;
	wpipe->pipe_peer = rpipe;

	FILE_SET_MATURE(rf);
	FILE_SET_MATURE(wf);
	return (0);
free3:
	ffree(rf);
	fdremove(fdp, retval[0]);
free2:
	(void)pipeclose(wpipe);
	(void)pipeclose(rpipe);
	return (error);
}

/*
 * Allocate kva for pipe circular buffer, the space is pageable
 */
void
pipespace(cpipe)
	struct pipe *cpipe;
{
	cpipe->pipe_buffer.buffer = (caddr_t) uvm_km_valloc(kernel_map,
						cpipe->pipe_buffer.size);
	if (cpipe->pipe_buffer.buffer == NULL)
		panic("pipespace: out of kvm");

	amountpipekva += cpipe->pipe_buffer.size;
}

/*
 * initialize and allocate VM and memory for pipe
 */
void
pipeinit(cpipe)
	struct pipe *cpipe;
{

	cpipe->pipe_buffer.in = 0;
	cpipe->pipe_buffer.out = 0;
	cpipe->pipe_buffer.cnt = 0;
	cpipe->pipe_buffer.size = PIPE_SIZE;

	/* Buffer kva gets dynamically allocated */
	cpipe->pipe_buffer.buffer = NULL;
	/* cpipe->pipe_buffer.object = invalid */

	cpipe->pipe_state = 0;
	cpipe->pipe_peer = NULL;
	cpipe->pipe_busy = 0;
	microtime(&cpipe->pipe_ctime);
	cpipe->pipe_atime = cpipe->pipe_ctime;
	cpipe->pipe_mtime = cpipe->pipe_ctime;
	bzero(&cpipe->pipe_sel, sizeof cpipe->pipe_sel);
	cpipe->pipe_pgid = NO_PID;
}


/*
 * lock a pipe for I/O, blocking other access
 */
static __inline int
pipelock(cpipe)
	struct pipe *cpipe;
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
static __inline void
pipeunlock(cpipe)
	struct pipe *cpipe;
{
	cpipe->pipe_state &= ~PIPE_LOCK;
	if (cpipe->pipe_state & PIPE_LWANT) {
		cpipe->pipe_state &= ~PIPE_LWANT;
		wakeup(cpipe);
	}
}

static __inline void
pipeselwakeup(cpipe)
	struct pipe *cpipe;
{
	if (cpipe->pipe_state & PIPE_SEL) {
		cpipe->pipe_state &= ~PIPE_SEL;
		selwakeup(&cpipe->pipe_sel);
	}
	if ((cpipe->pipe_state & PIPE_ASYNC) && cpipe->pipe_pgid != NO_PID)
		gsignal(cpipe->pipe_pgid, SIGIO);
	KNOTE(&cpipe->pipe_sel.si_note, 0);
}

/* ARGSUSED */
int
pipe_read(fp, poff, uio, cred)
	struct file *fp;
	off_t *poff;
	struct uio *uio;
	struct ucred *cred;
{
	struct pipe *rpipe = (struct pipe *) fp->f_data;
	int error;
	int nread = 0;
	int size;

	error = pipelock(rpipe);
	if (error)
		goto unlocked_error;

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
			 */
			if (rpipe->pipe_state & PIPE_EOF) {
				/* XXX error = ? */
				break;
			}

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
			if (fp->f_flag & FNONBLOCK)
				error = EAGAIN;
			else {
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
		microtime(&rpipe->pipe_atime);
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

	return error;
}

int
pipe_write(fp, poff, uio, cred)
	struct file *fp;
	off_t *poff;
	struct uio *uio;
	struct ucred *cred;
{
	int error = 0;
	int orig_resid;

	struct pipe *wpipe, *rpipe;

	rpipe = (struct pipe *) fp->f_data;
	wpipe = rpipe->pipe_peer;

	/*
	 * detect loss of pipe read side, issue SIGPIPE if lost.
	 */
	if ((wpipe == NULL) || (wpipe->pipe_state & PIPE_EOF)) {
		return EPIPE;
	}

	/*
	 * If it is advantageous to resize the pipe buffer, do
	 * so.
	 */
	if ((uio->uio_resid > PIPE_SIZE) &&
	    (nbigpipe < LIMITBIGPIPES) &&
	    (wpipe->pipe_buffer.size <= PIPE_SIZE) &&
	    (wpipe->pipe_buffer.cnt == 0)) {

		if (wpipe->pipe_buffer.buffer) {
			amountpipekva -= wpipe->pipe_buffer.size;
			uvm_km_free(kernel_map,
				(vaddr_t)wpipe->pipe_buffer.buffer,
				wpipe->pipe_buffer.size);
		}

		wpipe->pipe_buffer.in = 0;
		wpipe->pipe_buffer.out = 0;
		wpipe->pipe_buffer.cnt = 0;
		wpipe->pipe_buffer.size = BIG_PIPE_SIZE;
		wpipe->pipe_buffer.buffer = NULL;
		++nbigpipe;
	}
		

	if (wpipe->pipe_buffer.buffer == NULL) {
		if ((error = pipelock(wpipe)) == 0) {
			pipespace(wpipe);
			pipeunlock(wpipe);
		} else {
			return error;
		}
	}

	++wpipe->pipe_busy;
	orig_resid = uio->uio_resid;

retrywrite:
	while (uio->uio_resid) {
		int space;

		if (wpipe->pipe_state & PIPE_EOF) {
			error = EPIPE;
			break;
		}

		space = wpipe->pipe_buffer.size - wpipe->pipe_buffer.cnt;

		/* Writes of size <= PIPE_BUF must be atomic. */
		if ((space < uio->uio_resid) && (orig_resid <= PIPE_BUF))
			space = 0;

		if (space > 0 &&
		    (wpipe->pipe_buffer.cnt < wpipe->pipe_buffer.size)) {
			if ((error = pipelock(wpipe)) == 0) {
				int size;	/* Transfer size */
				int segsize;	/* first segment to transfer */

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
			 * wake up selects.
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
	if ((wpipe->pipe_busy == 0) &&
	    (wpipe->pipe_state & PIPE_WANT)) {
		wpipe->pipe_state &= ~(PIPE_WANT|PIPE_WANTR);
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
	    (error == EPIPE))
		error = 0;

	if (error == 0)
		microtime(&wpipe->pipe_mtime);
	/*
	 * We have something to offer,
	 * wake up select.
	 */
	if (wpipe->pipe_buffer.cnt)
		pipeselwakeup(wpipe);

	return error;
}

/*
 * we implement a very minimal set of ioctls for compatibility with sockets.
 */
int
pipe_ioctl(fp, cmd, data, p)
	struct file *fp;
	u_long cmd;
	caddr_t data;
	struct proc *p;
{
	struct pipe *mpipe = (struct pipe *)fp->f_data;

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

	case SIOCSPGRP:
		mpipe->pipe_pgid = *(int *)data;
		return (0);

	case SIOCGPGRP:
		*(int *)data = mpipe->pipe_pgid;
		return (0);

	}
	return (ENOTTY);
}

int
pipe_select(fp, which, p)
	struct file *fp;
	int which;
	struct proc *p;
{
	struct pipe *rpipe = (struct pipe *)fp->f_data;
	struct pipe *wpipe;

	wpipe = rpipe->pipe_peer;
	switch (which) {

	case FREAD:
		if ((rpipe->pipe_buffer.cnt > 0) ||
		    (rpipe->pipe_state & PIPE_EOF)) {
			return (1);
		}
		selrecord(p, &rpipe->pipe_sel);
		rpipe->pipe_state |= PIPE_SEL;
		break;

	case FWRITE:
		if ((wpipe == NULL) ||
		    (wpipe->pipe_state & PIPE_EOF) ||
		    ((wpipe->pipe_buffer.size - wpipe->pipe_buffer.cnt) >= PIPE_BUF)) {
			return (1);
		}
		selrecord(p, &wpipe->pipe_sel);
		wpipe->pipe_state |= PIPE_SEL;
		break;

	case 0:
		if ((rpipe->pipe_state & PIPE_EOF) ||
		    (wpipe == NULL) ||
		    (wpipe->pipe_state & PIPE_EOF)) {
			return (1);
		}
			
		selrecord(p, &rpipe->pipe_sel);
		rpipe->pipe_state |= PIPE_SEL;
		break;
	}
	return (0);
}

int
pipe_stat(fp, ub, p)
	struct file *fp;
	struct stat *ub;
	struct proc *p;
{
	struct pipe *pipe = (struct pipe *)fp->f_data;

	bzero((caddr_t)ub, sizeof (*ub));
	ub->st_mode = S_IFIFO;
	ub->st_blksize = pipe->pipe_buffer.size;
	ub->st_size = pipe->pipe_buffer.cnt;
	ub->st_blocks = (ub->st_size + ub->st_blksize - 1) / ub->st_blksize;
	TIMEVAL_TO_TIMESPEC(&pipe->pipe_atime, &ub->st_atimespec);
	TIMEVAL_TO_TIMESPEC(&pipe->pipe_mtime, &ub->st_mtimespec);
	TIMEVAL_TO_TIMESPEC(&pipe->pipe_ctime, &ub->st_ctimespec);
	ub->st_uid = fp->f_cred->cr_uid;
	ub->st_gid = fp->f_cred->cr_gid;
	/*
	 * Left as 0: st_dev, st_ino, st_nlink, st_rdev, st_flags, st_gen.
	 * XXX (st_dev, st_ino) should be unique.
	 */
	return 0;
}

/* ARGSUSED */
int
pipe_close(fp, p)
	struct file *fp;
	struct proc *p;
{
	struct pipe *cpipe = (struct pipe *)fp->f_data;

	pipeclose(cpipe);
	fp->f_data = NULL;
	return 0;
}

/*
 * shutdown the pipe
 */
void
pipeclose(cpipe)
	struct pipe *cpipe;
{
	struct pipe *ppipe;
	if (cpipe) {
		
		pipeselwakeup(cpipe);

		/*
		 * If the other side is blocked, wake it up saying that
		 * we want to close it down.
		 */
		while (cpipe->pipe_busy) {
			wakeup(cpipe);
			cpipe->pipe_state |= PIPE_WANT|PIPE_EOF;
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
		if (cpipe->pipe_buffer.buffer) {
			if (cpipe->pipe_buffer.size > PIPE_SIZE)
				--nbigpipe;
			amountpipekva -= cpipe->pipe_buffer.size;
			uvm_km_free(kernel_map,
				(vaddr_t)cpipe->pipe_buffer.buffer,
				cpipe->pipe_buffer.size);
		}
		pool_put(&pipe_pool, cpipe);
	}
}

int
pipe_kqfilter(struct file *fp, struct knote *kn)
{
	struct pipe *rpipe = (struct pipe *)kn->kn_fp->f_data;
	struct pipe *wpipe = rpipe->pipe_peer;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &pipe_rfiltops;
		SLIST_INSERT_HEAD(&rpipe->pipe_sel.si_note, kn, kn_selnext);
		break;
	case EVFILT_WRITE:
		if (wpipe == NULL)
			return (1);
		kn->kn_fop = &pipe_wfiltops;
		SLIST_INSERT_HEAD(&wpipe->pipe_sel.si_note, kn, kn_selnext);
		break;
	default:
		return (1);
	}
	
	return (0);
}

void
filt_pipedetach(struct knote *kn)
{
	struct pipe *rpipe = (struct pipe *)kn->kn_fp->f_data;
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

/*ARGSUSED*/
int
filt_piperead(struct knote *kn, long hint)
{
	struct pipe *rpipe = (struct pipe *)kn->kn_fp->f_data;
	struct pipe *wpipe = rpipe->pipe_peer;

	kn->kn_data = rpipe->pipe_buffer.cnt;

	if ((rpipe->pipe_state & PIPE_EOF) ||
	    (wpipe == NULL) || (wpipe->pipe_state & PIPE_EOF)) {
		kn->kn_flags |= EV_EOF; 
		return (1);
	}
	return (kn->kn_data > 0);
}

/*ARGSUSED*/
int
filt_pipewrite(struct knote *kn, long hint)
{
	struct pipe *rpipe = (struct pipe *)kn->kn_fp->f_data;
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
pipe_init()
{
	pool_init(&pipe_pool, sizeof(struct pipe), 0, 0, 0, "pipepl",
		0, pool_page_alloc_nointr, pool_page_free_nointr,
		M_PIPE);
}

