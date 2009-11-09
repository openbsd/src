/*	$OpenBSD: subr_log.c,v 1.16 2009/11/09 17:53:39 nicm Exp $	*/
/*	$NetBSD: subr_log.c,v 1.11 1996/03/30 22:24:44 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1993
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
 *
 *	@(#)subr_log.c	8.1 (Berkeley) 6/10/93
 */

/*
 * Error log buffer for kernel printf's.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/ioctl.h>
#include <sys/msgbuf.h>
#include <sys/file.h>
#include <sys/signalvar.h>
#include <sys/syslog.h>
#include <sys/conf.h>
#include <sys/poll.h>

#define LOG_RDPRI	(PZERO + 1)

#define LOG_ASYNC	0x04
#define LOG_RDWAIT	0x08

struct logsoftc {
	int	sc_state;		/* see above for possibilities */
	struct	selinfo sc_selp;	/* process waiting on select call */
	int	sc_pgid;		/* process/group for async I/O */
	uid_t	sc_siguid;		/* uid for process that set sc_pgid */
	uid_t	sc_sigeuid;		/* euid for process that set sc_pgid */
} logsoftc;

int	log_open;			/* also used in log() */
int	msgbufmapped;			/* is the message buffer mapped */
int	msgbufenabled;			/* is logging to the buffer enabled */
struct	msgbuf *msgbufp;		/* the mapped buffer, itself. */

void filt_logrdetach(struct knote *kn);
int filt_logread(struct knote *kn, long hint);
   
struct filterops logread_filtops =
	{ 1, NULL, filt_logrdetach, filt_logread};

void
initmsgbuf(caddr_t buf, size_t bufsize)
{
	struct msgbuf *mbp;
	long new_bufs;

	/* Sanity-check the given size. */
	if (bufsize < sizeof(struct msgbuf))
		return;

	mbp = msgbufp = (struct msgbuf *)buf;

	new_bufs = bufsize - offsetof(struct msgbuf, msg_bufc);
	if ((mbp->msg_magic != MSG_MAGIC) || (mbp->msg_bufs != new_bufs) ||
	    (mbp->msg_bufr < 0) || (mbp->msg_bufr >= mbp->msg_bufs) ||
	    (mbp->msg_bufx < 0) || (mbp->msg_bufx >= mbp->msg_bufs)) {
		/*
		 * If the buffer magic number is wrong, has changed
		 * size (which shouldn't happen often), or is
		 * internally inconsistent, initialize it.
		 */

		bzero(buf, bufsize);
		mbp->msg_magic = MSG_MAGIC;
		mbp->msg_bufs = new_bufs;
	}
	
	/* Always start new buffer data on a new line. */
	if (mbp->msg_bufx > 0 && mbp->msg_bufc[mbp->msg_bufx - 1] != '\n')
		msgbuf_putchar('\n');

	/* mark it as ready for use. */
	msgbufmapped = msgbufenabled = 1;
}

void
msgbuf_putchar(const char c) 
{
	struct msgbuf *mbp = msgbufp;

	if (mbp->msg_magic != MSG_MAGIC)
		/* Nothing we can do */
		return;

	mbp->msg_bufc[mbp->msg_bufx++] = c;
	mbp->msg_bufl = min(mbp->msg_bufl+1, mbp->msg_bufs);
	if (mbp->msg_bufx < 0 || mbp->msg_bufx >= mbp->msg_bufs)
		mbp->msg_bufx = 0;
	/* If the buffer is full, keep the most recent data. */
	if (mbp->msg_bufr == mbp->msg_bufx) {
		if (++mbp->msg_bufr >= mbp->msg_bufs)
			mbp->msg_bufr = 0;
	}
}

/*ARGSUSED*/
int
logopen(dev_t dev, int flags, int mode, struct proc *p)
{
	if (log_open)
		return (EBUSY);
	log_open = 1;
	return (0);
}

/*ARGSUSED*/
int
logclose(dev_t dev, int flag, int mode, struct proc *p)
{

	log_open = 0;
	logsoftc.sc_state = 0;
	return (0);
}

/*ARGSUSED*/
int
logread(dev_t dev, struct uio *uio, int flag)
{
	struct msgbuf *mbp = msgbufp;
	long l;
	int s;
	int error = 0;

	s = splhigh();
	while (mbp->msg_bufr == mbp->msg_bufx) {
		if (flag & IO_NDELAY) {
			splx(s);
			return (EWOULDBLOCK);
		}
		logsoftc.sc_state |= LOG_RDWAIT;
		error = tsleep(mbp, LOG_RDPRI | PCATCH,
			       "klog", 0);
		if (error) {
			splx(s);
			return (error);
		}
	}
	splx(s);
	logsoftc.sc_state &= ~LOG_RDWAIT;

	while (uio->uio_resid > 0) {
		l = mbp->msg_bufx - mbp->msg_bufr;
		if (l < 0)
			l = mbp->msg_bufs - mbp->msg_bufr;
		l = min(l, uio->uio_resid);
		if (l == 0)
			break;
		error = uiomove(&mbp->msg_bufc[mbp->msg_bufr], (int)l, uio);
		if (error)
			break;
		mbp->msg_bufr += l;
		if (mbp->msg_bufr < 0 || mbp->msg_bufr >= mbp->msg_bufs)
			mbp->msg_bufr = 0;
	}
	return (error);
}

/*ARGSUSED*/
int
logpoll(dev_t dev, int events, struct proc *p)
{
	int revents = 0;
	int s = splhigh();

	if (events & (POLLIN | POLLRDNORM)) {
		if (msgbufp->msg_bufr != msgbufp->msg_bufx)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(p, &logsoftc.sc_selp);
	}
	splx(s);
	return (revents);
}

int
logkqfilter(dev_t dev, struct knote *kn)
{
	struct klist *klist;
	int s;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		klist = &logsoftc.sc_selp.si_note;
		kn->kn_fop = &logread_filtops;
		break;
	default:
		return (1);
	}

	kn->kn_hook = (void *)msgbufp;

	s = splhigh();
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);
	splx(s);

	return (0);
}

void
filt_logrdetach(struct knote *kn)
{
	int s = splhigh();

	SLIST_REMOVE(&logsoftc.sc_selp.si_note, kn, knote, kn_selnext);
	splx(s);
}

int
filt_logread(struct knote *kn, long hint)
{
	struct  msgbuf *p = (struct  msgbuf *)kn->kn_hook;

	kn->kn_data = (int)(p->msg_bufx - p->msg_bufr);

	return (p->msg_bufx != p->msg_bufr);
}

void
logwakeup(void)
{
	if (!log_open)
		return;
	selwakeup(&logsoftc.sc_selp);
	if (logsoftc.sc_state & LOG_ASYNC)
		csignal(logsoftc.sc_pgid, SIGIO,
		    logsoftc.sc_siguid, logsoftc.sc_sigeuid);
	if (logsoftc.sc_state & LOG_RDWAIT) {
		wakeup(msgbufp);
		logsoftc.sc_state &= ~LOG_RDWAIT;
	}
}

/*ARGSUSED*/
int
logioctl(dev_t dev, u_long com, caddr_t data, int flag, struct proc *p)
{
	long l;
	int s;

	switch (com) {

	/* return number of characters immediately available */
	case FIONREAD:
		s = splhigh();
		l = msgbufp->msg_bufx - msgbufp->msg_bufr;
		splx(s);
		if (l < 0)
			l += msgbufp->msg_bufs;
		*(int *)data = l;
		break;

	case FIONBIO:
		break;

	case FIOASYNC:
		if (*(int *)data)
			logsoftc.sc_state |= LOG_ASYNC;
		else
			logsoftc.sc_state &= ~LOG_ASYNC;
		break;

	case TIOCSPGRP:
		logsoftc.sc_pgid = *(int *)data;
		logsoftc.sc_siguid = p->p_cred->p_ruid;
		logsoftc.sc_sigeuid = p->p_ucred->cr_uid;
		break;

	case TIOCGPGRP:
		*(int *)data = logsoftc.sc_pgid;
		break;

	default:
		return (ENOTTY);
	}
	return (0);
}
