/*
 * Copyright (c) 1995 - 2003 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include <nnpfs/nnpfs_locl.h>
#include <nnpfs/nnpfs_message.h>
#include <nnpfs/nnpfs_msg_locl.h>
#include <nnpfs/nnpfs_fs.h>
#include <nnpfs/nnpfs_dev.h>
#include <nnpfs/nnpfs_deb.h>

RCSID("$arla: nnpfs_dev-common.c,v 1.61 2003/07/15 16:25:42 lha Exp $");

struct nnpfs_channel nnpfs_channel[NNNPFS];

void
nnpfs_initq(struct nnpfs_link *q)
{
    q->next = q;
    q->prev = q;
}

/* Is this queue empty? */
int
nnpfs_emptyq(const struct nnpfs_link *q)
{
    return q->next == q;
}

/* Is this link on any queue? Link *must* be inited! */
int
nnpfs_onq(const struct nnpfs_link *link)
{
    return link->next != NULL || link->prev != NULL;
}

/* Append q with p */
void
nnpfs_appendq(struct nnpfs_link *q, struct nnpfs_link *p)
{
    p->next = q;
    p->prev = q->prev;
    p->prev->next = p;
    q->prev = p;
}

/* remove `p' from its queue */
void
nnpfs_outq(struct nnpfs_link *p)
{
    p->next->prev = p->prev;
    p->prev->next = p->next;
    p->next = p->prev = NULL;
}

/*
 * Only allow one open.
 */
int
nnpfs_devopen_common(dev_t dev)
{
    struct nnpfs_channel *chan;

    if (minor(dev) < 0 || minor(dev) >= NNNPFS)
	return ENXIO;

    chan = &nnpfs_channel[minor(dev)];

    /* Only allow one reader/writer */
    if (chan->status & CHANNEL_OPENED) {
	NNPFSDEB(XDEBDEV, ("nnpfs_devopen: already open\n"));
	return EBUSY;
    } else {
	chan->status |= CHANNEL_OPENED;
    }

    chan->message_buffer = nnpfs_alloc(MAX_XMSG_SIZE, M_NNPFS_MSG);

    /* initialize the queues if they have not been initialized before */
    nnpfs_initq(&chan->sleepq);
    nnpfs_initq(&chan->messageq);

    return 0;
}

#if defined(HAVE_TWO_ARGUMENT_VFS_BUSY)
#define nnpfs_vfs_busy(mp, flags, lock, proc) vfs_busy((mp), (flags))
#define nnpfs_vfs_unbusy(mp, proc) vfs_unbusy((mp))
#elif defined(HAVE_THREE_ARGUMENT_VFS_BUSY)
#define nnpfs_vfs_busy(mp, flags, lock, proc) vfs_busy((mp), (flags), (lock))
#define nnpfs_vfs_unbusy(mp, proc) vfs_unbusy((mp))
#elif defined(HAVE_FOUR_ARGUMENT_VFS_BUSY)
#define nnpfs_vfs_busy(mp, flags, lock, proc) vfs_busy((mp), (flags), (lock), (proc))
#define nnpfs_vfs_unbusy(mp, proc) vfs_unbusy((mp), (proc))
#elif defined(__osf__)
#define nnpfs_vfs_busy(mp, flags, lock, proc) (0)
#define nnpfs_vfs_unbusy(mp, proc) (0)
#else
#define nnpfs_vfs_busy(mp, flags, lock, proc) vfs_busy((mp))
#define nnpfs_vfs_unbusy(mp, proc) vfs_unbusy((mp))
#endif

/*
 * Wakeup all sleepers and cleanup.
 */
int
nnpfs_devclose_common(dev_t dev, d_thread_t *proc)
{
    struct nnpfs_channel *chan = &nnpfs_channel[minor(dev)];
    struct nnpfs_link *first;
    
    /* Sanity check, paranoia? */
    if (!(chan->status & CHANNEL_OPENED))
	panic("nnpfs_devclose never opened?");

    chan->status &= ~CHANNEL_OPENED;

    /* No one is going to read those messages so empty queue! */
    while (!nnpfs_emptyq(&chan->messageq)) {
	NNPFSDEB(XDEBDEV, ("before outq(messageq)\n"));

	first = chan->messageq.next;
	nnpfs_outq(first);
	if (first->error_or_size != 0)
	    nnpfs_free(first, first->error_or_size, M_NNPFS_LINK);

	NNPFSDEB(XDEBDEV, ("after outq(messageq)\n"));
    }

    /* Wakeup those waiting for replies that will never arrive. */
    while (!nnpfs_emptyq(&chan->sleepq)) {
	NNPFSDEB(XDEBDEV, ("before outq(sleepq)\n"));
	first = chan->sleepq.next;
	nnpfs_outq(first);
	first->error_or_size = ENODEV;
	wakeup((caddr_t) first);
	NNPFSDEB(XDEBDEV, ("after outq(sleepq)\n"));
    }

    if (chan->status & CHANNEL_WAITING)
	wakeup((caddr_t) chan);

    if (chan->message_buffer) {
	nnpfs_free(chan->message_buffer, MAX_XMSG_SIZE, M_NNPFS_MSG);
	chan->message_buffer = NULL;
    }

    /*
     * Free all nnpfs nodes.
     */

    if (nnpfs[minor(dev)].mp != NULL) {
	if (nnpfs_vfs_busy(nnpfs[minor(dev)].mp, VB_READ|VB_WAIT, NULL, proc)) {
	    NNPFSDEB(XDEBNODE, ("nnpfs_dev_close: vfs_busy() --> BUSY\n"));
	    return EBUSY;
	}
	free_all_nnpfs_nodes(&nnpfs[minor(dev)], FORCECLOSE, 0);

	nnpfs_vfs_unbusy(nnpfs[minor(dev)].mp, proc);
    }
    
    return 0;
}

#ifdef NNPFS_DEBUG
/*
 * debugging glue for CURSIG
 */

static long
nnpfs_cursig (d_thread_t *p)
{
#if defined(__osf__)
    thread_t th 	= current_thread();
    struct np_uthread	*npu = thread_to_np_uthread(th);
    return CURSIG(p,npu);
#elif defined(HAVE_FREEBSD_THREAD)
#ifndef CURSIG
    return 0; /* XXX we would like to use sig_ffs, but that isn't
	       * exported */
#else
    return CURSIG(p->td_proc);
#endif
#else
#if defined(__NetBSD__) && __NetBSD_Version__ >= 106130000
    return 0; /* XXX CURSIG operates on a struct lwp */
#else
    return CURSIG(p);
#endif
#endif
}
#endif

/*
 * Move messages from kernel to user space.
 */

int
nnpfs_devread(dev_t dev, struct uio * uiop, int ioflag)
{
    struct nnpfs_channel *chan = &nnpfs_channel[minor(dev)];
    struct nnpfs_link *first;
    int error = 0;
#ifdef NNPFS_DEBUG
    char devname[64];
#endif

    NNPFSDEB(XDEBDEV, ("nnpfs_devread dev = %s\n",
		     nnpfs_devtoname_r(dev, devname, sizeof(devname))));

    NNPFSDEB(XDEBDEV, ("nnpfs_devread: m = %lx, m->prev = %lx, m->next = %lx\n",
		     (unsigned long)&chan->messageq,
		     (unsigned long)chan->messageq.prev,
		     (unsigned long)chan->messageq.next));

#ifdef HAVE_FREEBSD_THREAD
    chan->proc = nnpfs_uio_to_thread(uiop);
#else
    chan->proc = nnpfs_uio_to_proc(uiop);
#endif

 again:

    if (!nnpfs_emptyq (&chan->messageq)) {
	while (!nnpfs_emptyq (&chan->messageq)) {
	    /* Remove message */
	    first = chan->messageq.next;
	    NNPFSDEB(XDEBDEV, ("nnpfs_devread: first = %lx, "
			     "first->prev = %lx, first->next = %lx\n",
			     (unsigned long)first,
			     (unsigned long)first->prev,
			     (unsigned long)first->next));
	    
	    NNPFSDEB(XDEBDEV, ("nnpfs_devread: message->size = %u\n",
			     first->message->size));
	    
	    if (first->message->size > uiop->uio_resid)
		break;

	    error = uiomove((caddr_t) first->message, first->message->size, 
			    uiop);
	    if (error)
		break;
	    
	    nnpfs_outq(first);
	    
	    if (first->error_or_size != 0)
		nnpfs_free(first, first->error_or_size, M_NNPFS_LINK);
	}
    } else {
	chan->status |= CHANNEL_WAITING;
	if (tsleep((caddr_t) chan, (PZERO + 1) | PCATCH, "nnpfsread", 0)) {
#ifdef HAVE_FREEBSD_THREAD
	    NNPFSDEB(XDEBMSG,
		   ("caught signal nnpfs_devread: %ld\n",
		    nnpfs_cursig(nnpfs_uio_to_thread(uiop))));
#else
	    NNPFSDEB(XDEBMSG,
		   ("caught signal nnpfs_devread: %ld\n",
		    nnpfs_cursig(nnpfs_uio_to_proc(uiop))));
#endif
	    error = EINTR;
	} else if ((chan->status & CHANNEL_WAITING) == 0) {
	    goto again;
	} else
	    error = EIO;
    }
    
    NNPFSDEB(XDEBDEV, ("nnpfs_devread done error = %d\n", error));

    return error;
}

/*
 * Move messages from user space to kernel space,
 * wakeup sleepers, insert new data in VFS.
 */
int
nnpfs_devwrite(dev_t dev, struct uio *uiop, int ioflag)
{
    struct nnpfs_channel *chan = &nnpfs_channel[minor(dev)];
    char *p;
    int error;
    u_int cnt;
    struct nnpfs_message_header *msg_buf;
#ifdef NNPFS_DEBUG
    char devname[64];
#endif

    NNPFSDEB(XDEBDEV, ("nnpfs_devwrite dev = %s\n",
		     nnpfs_devtoname_r (dev, devname, sizeof(devname))));

#ifdef HAVE_FREEBSD_THREAD
    chan->proc = nnpfs_uio_to_thread(uiop);
#else
    chan->proc = nnpfs_uio_to_proc(uiop);
#endif
    cnt = uiop->uio_resid;
    error = uiomove((caddr_t) chan->message_buffer, MAX_XMSG_SIZE, uiop);
    if (error != 0)
	return error;

    cnt -= uiop->uio_resid;

    /*
     * This thread handles the received message.
     */
    for (p = (char *)chan->message_buffer;
	 cnt > 0;
	 p += msg_buf->size, cnt -= msg_buf->size) {
#ifdef HAVE_FREEBSD_THREAD
	d_thread_t *pp = nnpfs_uio_to_thread(uiop);
#else
	d_thread_t *pp = nnpfs_uio_to_proc(uiop);
#endif

	msg_buf = (struct nnpfs_message_header *)p;
	error = nnpfs_message_receive (minor(dev),
				     msg_buf,
				     msg_buf->size,
				     pp);
    }
    NNPFSDEB(XDEBDEV, ("nnpfs_devwrite error = %d\n", error));
    return error;
}

/*
 * Send a message to user space.
 */
int
nnpfs_message_send(int fd, struct nnpfs_message_header * message, u_int size)
{
    struct nnpfs_channel *chan = &nnpfs_channel[fd];
    struct {
	struct nnpfs_link this_message;
	struct nnpfs_message_header msg;
    } *t;

    NNPFSDEB(XDEBMSG, ("nnpfs_message_send opcode = %d\n", message->opcode));

    if (!(chan->status & CHANNEL_OPENED))	/* No receiver? */
	return ENODEV;

    /* Prepare message and copy it later */
    message->size = size;
    message->sequence_num = chan->nsequence++;

    t = nnpfs_alloc(sizeof(t->this_message) + size, M_NNPFS);
    t->this_message.error_or_size = sizeof(t->this_message) + size;
    bcopy(message, &t->msg, size);

    t->this_message.message = &t->msg;
    nnpfs_appendq(&chan->messageq, &t->this_message);
    if (chan->status & CHANNEL_WAITING) {
	chan->status &= ~CHANNEL_WAITING;
	wakeup((caddr_t) chan);
    }
    nnpfs_select_wakeup(chan);

    return 0;
}

#if defined(SWEXIT)
#define NNPFS_P_EXIT SWEXIT
#elif defined(P_WEXIT)
#define NNPFS_P_EXIT P_WEXIT
#else
#error what is your exit named ?
#endif

#if defined(HAVE_STRUCT_PROC_P_SIGMASK) || defined(HAVE_STRUCT_PROC_P_SIGCTX) || defined(HAVE_STRUCT_PROC_P_SIGWAITMASK) || defined(__osf__) || defined(HAVE_FREEBSD_THREAD)
static void
nnpfs_block_sigset (sigset_t *sigset)
{

#if defined(__sigaddset)
#define nnpfs_sig_block(ss,signo) __sigaddset((ss), (signo))
#elif defined(SIGADDSET)
#define nnpfs_sig_block(ss,signo) SIGADDSET(*(ss), (signo))
#else
#define nnpfs_sig_block(ss,signo) *(ss) |= sigmask(signo)
#endif

    nnpfs_sig_block(sigset, SIGIO);
    nnpfs_sig_block(sigset, SIGALRM);
    nnpfs_sig_block(sigset, SIGVTALRM);
    nnpfs_sig_block(sigset, SIGCHLD);
#ifdef SIGINFO
    nnpfs_sig_block(sigset, SIGINFO);
#endif
#undef nnpfs_sig_block
}
#endif

/*
 * Send a message to user space and wait for reply.
 */

int
nnpfs_message_rpc(int fd, struct nnpfs_message_header * message, u_int size,
		d_thread_t *proc)
{
    int ret;
    struct nnpfs_channel *chan = &nnpfs_channel[fd];
    struct nnpfs_link *this_message;
    struct nnpfs_link *this_process;
    struct nnpfs_message_header *msg;
#if defined(HAVE_STRUCT_PROC_P_SIGMASK) || defined(HAVE_STRUCT_PROC_P_SIGCTX) || defined(__osf__) || defined(HAVE_FREEBSD_THREAD)
    sigset_t oldsigmask;
#endif
    int catch;

    NNPFSDEB(XDEBMSG, ("nnpfs_message_rpc opcode = %d\n", message->opcode));

    if (proc == NULL) {
#ifdef HAVE_FREEBSD_THREAD
	proc = nnpfs_curthread();
#else
	proc = nnpfs_curproc();
#endif
    }
    if (!(chan->status & CHANNEL_OPENED))	/* No receiver? */
	return ENODEV;

#ifdef HAVE_FREEBSD_THREAD
    if (chan->proc != NULL && chan->proc->td_proc != NULL &&
      proc->td_proc->p_pid == chan->proc->td_proc->p_pid) {
	printf("nnpfs_message_rpc: deadlock avoided "
	       "pid = %u == %u\n", proc->td_proc->p_pid, chan->proc->td_proc->p_pid);
#else
    if (chan->proc != NULL && proc->p_pid == chan->proc->p_pid) {
	printf("nnpfs_message_rpc: deadlock avoided "
	       "pid = %u == %u\n", proc->p_pid, chan->proc->p_pid);
#endif
#if 0
	psignal (proc, SIGABRT);
#endif
	return EDEADLK;
    }

    if (size < sizeof(struct nnpfs_message_wakeup)) {
	printf("NNPFS PANIC Error: Message to small to receive wakeup, opcode = %d\n", message->opcode);
	return ENOMEM;
    }
    this_message = nnpfs_alloc(sizeof(struct nnpfs_link), M_NNPFS_LINK);
    this_process = nnpfs_alloc(sizeof(struct nnpfs_link), M_NNPFS_LINK);
    msg = nnpfs_alloc(size, M_NNPFS_MSG);
    bcopy(message, msg, size);

    msg->size = size;
    msg->sequence_num = chan->nsequence++;
    this_message->error_or_size = 0;
    this_message->message = msg;
    this_process->message = msg;
    nnpfs_appendq(&chan->messageq, this_message);
    nnpfs_appendq(&chan->sleepq, this_process);
    nnpfs_select_wakeup(chan);
    this_process->error_or_size = 0;

    if (chan->status & CHANNEL_WAITING) {
	chan->status &= ~CHANNEL_WAITING;
	wakeup((caddr_t) chan);
    }

    /*
     * Remove signals from the sigmask so no IO will wake us up from
     * tsleep(). We don't want to wake up from since program (emacs,
     * bash & co can't handle them.
     */

#ifdef HAVE_FREEBSD_THREAD
    /* FreeBSD 5.1 */
    oldsigmask = proc->td_sigmask;
    nnpfs_block_sigset (&proc->td_sigmask);
#elif HAVE_STRUCT_PROC_P_SIGMASK
    /* NetBSD 1.5, Darwin 1.3, FreeBSD 4.3, 5.0, OpenBSD 2.8 */
    oldsigmask = proc->p_sigmask;
    nnpfs_block_sigset (&proc->p_sigmask);
#elif defined(HAVE_STRUCT_PROC_P_SIGCTX)
    /* NetBSD 1.6 */
    oldsigmask = proc->p_sigctx.ps_sigmask;
    nnpfs_block_sigset (&proc->p_sigctx.ps_sigmask);
#elif defined(HAVE_STRUCT_PROC_P_SIGWAITMASK)
    /* OSF 4.0 */
    oldsigmask = proc->p_sigwaitmask;
    nnpfs_block_sigset (&proc->p_sigwaitmask);
#elif defined(__osf__)
    /* OSF 5.0 */
    oldsigmask = u.u_sigmask;
    nnpfs_block_sigset (&u.u_sigmask);
#endif

    /*
     * if we are exiting we should not try to catch signals, since
     * there might not be enough context left in the process to handle
     * signal delivery, and besides, most BSD-variants ignore all
     * signals while closing anyway.
     */

    catch = 0;
#ifdef HAVE_FREEBSD_THREAD
    if (!(proc->td_proc->p_flag & NNPFS_P_EXIT))
#else
    if (!(proc->p_flag & NNPFS_P_EXIT))
#endif
	catch |= PCATCH;

    /*
     * We have to check if we have a receiver here too because the
     * daemon could have terminated before we sleep. This seems to
     * happen sometimes when rebooting.  */

    if (!(chan->status & CHANNEL_OPENED)) {
	NNPFSDEB(XDEBMSG, ("nnpfs_message_rpc: channel went away\n"));
	this_process->error_or_size = EINTR;
    } else if ((ret = tsleep((caddr_t) this_process,
			     (PZERO + 1) | catch, "nnpfs", 0)) != 0) {
	NNPFSDEB(XDEBMSG, ("caught signal (%d): %ld\n",
			 ret, nnpfs_cursig(proc)));
	this_process->error_or_size = EINTR;
    }

#ifdef HAVE_FREEBSD_THREAD
    proc->td_sigmask = oldsigmask;
#elif HAVE_STRUCT_PROC_P_SIGMASK
    proc->p_sigmask = oldsigmask;
#elif defined(HAVE_STRUCT_PROC_P_SIGCTX)
    proc->p_sigctx.ps_sigmask = oldsigmask;
#elif defined(HAVE_STRUCT_PROC_P_SIGWAITMASK)
    proc->p_sigwaitmask = oldsigmask;
#elif defined(__osf__)
    u.u_sigmask = oldsigmask;
#endif

    /*
     * Caught signal, got reply message or device was closed.
     * Need to clean up both messageq and sleepq.
     */
    if (nnpfs_onq(this_message)) {
	nnpfs_outq(this_message);
    }
    if (nnpfs_onq(this_process)) {
	nnpfs_outq(this_process);
    }
    ret = this_process->error_or_size;

    NNPFSDEB(XDEBMSG, ("nnpfs_message_rpc this_process->error_or_size = %d\n",
		     this_process->error_or_size));
    NNPFSDEB(XDEBMSG, ("nnpfs_message_rpc opcode ((nnpfs_message_wakeup*)(this_process->message))->error = %d\n", ((struct nnpfs_message_wakeup *) (this_process->message))->error));

    bcopy(msg, message, size);

    nnpfs_free(this_message, sizeof(*this_message), M_NNPFS_LINK);
    nnpfs_free(this_process, sizeof(*this_process), M_NNPFS_LINK);
    nnpfs_free(msg, size, M_NNPFS_MSG);

    return ret;
}

/*
 * For each message type there is a message handler
 * that implements its action, nnpfs_message_receive
 * invokes the correct function.
 */
int
nnpfs_message_receive(int fd,
		    struct nnpfs_message_header *message,
		    u_int size,
		    d_thread_t *p)
{
    NNPFSDEB(XDEBMSG, ("nnpfs_message_receive opcode = %d\n", message->opcode));

    /* Dispatch and coerce message type */
    switch (message->opcode) {
    case NNPFS_MSG_WAKEUP:
	return nnpfs_message_wakeup(fd,
				  (struct nnpfs_message_wakeup *) message,
				  message->size,
				  p);
    case NNPFS_MSG_WAKEUP_DATA:
	return nnpfs_message_wakeup_data(fd,
				 (struct nnpfs_message_wakeup_data *) message,
				       message->size,
				       p);
    case NNPFS_MSG_INSTALLROOT:
	return nnpfs_message_installroot(fd,
				 (struct nnpfs_message_installroot *) message,
				       message->size,
				       p);
    case NNPFS_MSG_INSTALLNODE:
	return nnpfs_message_installnode(fd,
				 (struct nnpfs_message_installnode *) message,
				       message->size,
				       p);
    case NNPFS_MSG_INSTALLATTR:
	return nnpfs_message_installattr(fd,
				 (struct nnpfs_message_installattr *) message,
				       message->size,
				       p);
    case NNPFS_MSG_INSTALLDATA:
	return nnpfs_message_installdata(fd,
				 (struct nnpfs_message_installdata *) message,
				       message->size,
				       p);
    case NNPFS_MSG_INVALIDNODE:
	return nnpfs_message_invalidnode(fd,
				 (struct nnpfs_message_invalidnode *) message,
				       message->size,
				       p);
    case NNPFS_MSG_UPDATEFID:
	return nnpfs_message_updatefid(fd,
				     (struct nnpfs_message_updatefid *)message,
				     message->size,
				     p);
    case NNPFS_MSG_GC_NODES:
	return nnpfs_message_gc_nodes(fd,
				    (struct nnpfs_message_gc_nodes *)message,
				    message->size,
				    p);
    case NNPFS_MSG_VERSION:
	return nnpfs_message_version(fd,
				   (struct nnpfs_message_version *)message,
				   message->size,
				   p);
    default:
	printf("NNPFS PANIC Warning nnpfs_dev: Unknown message opcode == %d\n",
	       message->opcode);
	return EINVAL;
    }
}

int
nnpfs_message_wakeup(int fd,
		   struct nnpfs_message_wakeup *message,
		   u_int size,
		   d_thread_t *p)
{
    struct nnpfs_channel *chan = &nnpfs_channel[fd];
    struct nnpfs_link *sleepq = &chan->sleepq;
    struct nnpfs_link *t = chan->sleepq.next;	/* Really first in q */

    NNPFSDEB(XDEBMSG, ("nnpfs_message_wakeup error: %d\n", message->error));

    for (; t != sleepq; t = t->next)
	if (t->message->sequence_num == message->sleepers_sequence_num) {
	    if (t->message->size < size) {
		printf("NNPFS PANIC Error: Could not wakeup requestor with opcode = %d properly, to small receive buffer.\n", t->message->opcode);
		t->error_or_size = ENOMEM;
	    } else
		bcopy(message, t->message, size);

	    wakeup((caddr_t) t);
	    break;
	}

    return 0;
}

int
nnpfs_message_wakeup_data(int fd,
			struct nnpfs_message_wakeup_data * message,
			u_int size,
			d_thread_t *p)
{
    struct nnpfs_channel *chan = &nnpfs_channel[fd];
    struct nnpfs_link *sleepq = &chan->sleepq;
    struct nnpfs_link *t = chan->sleepq.next;	/* Really first in q */

    NNPFSDEB(XDEBMSG, ("nnpfs_message_wakeup_data error: %d\n", message->error));

    for (; t != sleepq; t = t->next)
	if (t->message->sequence_num == message->sleepers_sequence_num) {
	    if (t->message->size < size) {
		printf("NNPFS PANIC Error: Could not wakeup requestor with opcode = %d properly, to small receive buffer.\n", t->message->opcode);
		t->error_or_size = ENOMEM;
	    } else
		bcopy(message, t->message, size);
	    wakeup((caddr_t) t);
	    break;
	}
    return 0;
}

/*
 *
 */
int
nnpfs_uprintf_device(void)
{
#if 0
    int i;

    for (i = 0; i < NNNPFS; i++) {
	uprintf("nnpfs_channel[%d] = {\n", i);
	uprintf("messageq.next = %lx ", nnpfs_channel[i].messageq.next);
	uprintf("messageq.prev = %lx ", nnpfs_channel[i].messageq.prev);
	uprintf("sleepq.next = %lx ", nnpfs_channel[i].sleepq.next);
	uprintf("sleepq.prev = %lx ", nnpfs_channel[i].sleepq.prev);
	uprintf("nsequence = %d status = %d\n",
		nnpfs_channel[i].nsequence,
		nnpfs_channel[i].status);
	uprintf("}\n");
    }
#endif
    return 0;
}
