/*	$OpenBSD: xfs_dev-common.c,v 1.2 2000/03/03 00:54:58 todd Exp $	*/

/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 *
 * 4. Neither the name of the Institute nor the names of its contributors
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


#include <xfs/xfs_locl.h>
#include <xfs/xfs_message.h>
#include <xfs/xfs_msg_locl.h>
#include <xfs/xfs_fs.h>
#include <xfs/xfs_dev.h>
#include <xfs/xfs_deb.h>

RCSID("$OpenBSD: xfs_dev-common.c,v 1.2 2000/03/03 00:54:58 todd Exp $");

struct xfs_channel xfs_channel[NXFS];

void
xfs_initq(struct xfs_link *q)
{
    q->next = q;
    q->prev = q;
}

/* Is this queue empty? */
int
xfs_emptyq(struct xfs_link *q)
{
    return q->next == q;
}

/* Is this link on any queue? Link *must* be inited! */
int
xfs_onq(struct xfs_link *link)
{
    return link->next != NULL || link->prev != NULL;
}

/* Append q with p */
void
xfs_appendq(struct xfs_link *q, struct xfs_link *p)
{
    p->next = q;
    p->prev = q->prev;
    p->prev->next = p;
    q->prev = p;
}

void
xfs_outq(struct xfs_link *p)
{
    p->next->prev = p->prev;
    p->prev->next = p->next;
    p->next = p->prev = 0;
}

/*
 * Only allow one open.
 */
int
xfs_devopen_common(dev_t dev)
{
    struct xfs_channel *chan;

    if (minor(dev) < 0 || minor(dev) >= NXFS)
	return ENXIO;

    chan = &xfs_channel[minor(dev)];

    /* Only allow one reader/writer */
    if (chan->status & CHANNEL_OPENED) {
	XFSDEB(XDEBDEV, ("xfs_devopen: already open\n"));
	return EBUSY;
    } else
	chan->status |= CHANNEL_OPENED;

    chan->message_buffer = xfs_alloc(MAX_XMSG_SIZE);

    /* initalize the queues if they have not been initialized before */
    xfs_initq(&chan->sleepq);
    xfs_initq(&chan->messageq);

    return 0;
}

#if defined(HAVE_THREE_ARGUMENT_VFS_BUSY)
#define xfs_vfs_busy(mp, flags, lock, proc) vfs_busy((mp), (flags), (lock))
#define xfs_vfs_unbusy(mp, proc) vfs_unbusy((mp))
#elif defined(HAVE_FOUR_ARGUMENT_VFS_BUSY)
#define xfs_vfs_busy(mp, flags, lock, proc) vfs_busy((mp), (flags), (lock), (proc))
#define xfs_vfs_unbusy(mp, proc) vfs_unbusy((mp), (proc))
#elif defined(__osf__)
#define xfs_vfs_busy(mp, flags, lock, proc) (0)
#define xfs_vfs_unbusy(mp, proc) (0)
#else
#define xfs_vfs_busy(mp, flags, lock, proc) vfs_busy((mp))
#define xfs_vfs_unbusy(mp, proc) vfs_unbusy((mp))
#endif

/*
 * Wakeup all sleepers and cleanup.
 */
int
xfs_devclose_common(dev_t dev, struct proc *proc)
{
    struct xfs_channel *chan = &xfs_channel[minor(dev)];
    struct xfs_link *first;
    
    /* Sanity check, paranoia? */
    if (!(chan->status & CHANNEL_OPENED))
	panic("xfs_devclose never opened?");

    chan->status &= ~CHANNEL_OPENED;

    /* No one is going to read those messages so empty queue! */
    while (!xfs_emptyq(&chan->messageq)) {
	XFSDEB(XDEBDEV, ("before outq(messageq)\n"));

	first = chan->messageq.next;
	xfs_outq(first);
	if (first->error_or_size != 0)
	    xfs_free(first, first->error_or_size);

	XFSDEB(XDEBDEV, ("after outq(messageq)\n"));
    }

    /* Wakeup those waiting for replies that will never arrive. */
    while (!xfs_emptyq(&chan->sleepq)) {
	XFSDEB(XDEBDEV, ("before outq(sleepq)\n"));
	first = chan->sleepq.next;
	xfs_outq(first);
	first->error_or_size = ENODEV;
	wakeup((caddr_t) first);
	XFSDEB(XDEBDEV, ("after outq(sleepq)\n"));
    }

    if (chan->status & CHANNEL_WAITING)
	wakeup((caddr_t) chan);

    if (chan->message_buffer) {
	xfs_free(chan->message_buffer, MAX_XMSG_SIZE);
	chan->message_buffer = NULL;
    }

    /*
     * Free all xfs nodes.
     */

    if (xfs[minor(dev)].mp != NULL) {
	if (xfs_vfs_busy(xfs[minor(dev)].mp, 0, NULL, proc)) {
	    XFSDEB(XDEBNODE, ("xfs_dev_close: vfs_busy() --> BUSY\n"));
	    return EBUSY;
	}
	free_all_xfs_nodes(&xfs[minor(dev)], FORCECLOSE, 0);

	xfs_vfs_unbusy(xfs[minor(dev)].mp, proc);
    }
    
    return 0;
}

/*
 * Move messages from kernel to user space.
 */
int
xfs_devread(dev_t dev, struct uio * uiop, int ioflag)
{
    struct xfs_channel *chan = &xfs_channel[minor(dev)];
    struct xfs_link *first;
    int error = 0;

    XFSDEB(XDEBDEV, ("xfs_devread dev = %d\n", dev));

    XFSDEB(XDEBDEV, ("xfs_devread: m = %p, m->prev = %p, m->next = %p\n",
		&chan->messageq, chan->messageq.prev, chan->messageq.next));

 again:

    if (!xfs_emptyq (&chan->messageq)) {
	while (!xfs_emptyq (&chan->messageq)) {
	    /* Remove message */
	    first = chan->messageq.next;
	    XFSDEB(XDEBDEV, ("xfs_devread: first = %p, "
			     "first->prev = %p, first->next = %p\n",
			     first, first->prev, first->next));
	    
	    XFSDEB(XDEBDEV, ("xfs_devread: message->size = %u\n",
			     first->message->size));
	    
	    error = uiomove((caddr_t) first->message, first->message->size, 
			    uiop);
	    if (error)
		break;
	    
	    xfs_outq(first);
	    
	    if (first->error_or_size != 0)
		xfs_free(first, first->error_or_size);
	}
    } else {
	chan->status |= CHANNEL_WAITING;
	if (tsleep((caddr_t) chan, (PZERO + 1) | PCATCH, "xfsr", 0)) {
	    XFSDEB(XDEBMSG, ("caught signal xfs_devread\n"));
	    error = EINTR;
	} else if ((chan->status & CHANNEL_WAITING) == 0) {
	    goto again;
	} else
	    error = EIO;
    }
    
    
    XFSDEB(XDEBDEV, ("xfs_devread done error = %d\n", error));

    return error;
}

/*
 * Move messages from user space to kernel space,
 * wakeup sleepers, insert new data in VFS.
 */
int
xfs_devwrite(dev_t dev, struct uio *uiop, int ioflag)
{
    struct xfs_channel *chan = &xfs_channel[minor(dev)];
    char *p;
    int error;
    u_int cnt;
    struct xfs_message_header *msg_buf;

    XFSDEB(XDEBDEV, ("xfs_devwrite dev = %d\n", dev));

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
	struct proc *pp = xfs_uio_to_proc(uiop);

	msg_buf = (struct xfs_message_header *)p;
	error = xfs_message_receive (minor(dev),
				     msg_buf,
				     msg_buf->size,
				     pp);
    }
    XFSDEB(XDEBDEV, ("xfs_devwrite error = %d\n", error));
    return error;
}

/*
 * Send a message to user space.
 */
int
xfs_message_send(int fd, struct xfs_message_header * message, u_int size)
{
    struct xfs_channel *chan = &xfs_channel[fd];
    struct {
	struct xfs_link this_message;
	struct xfs_message_header msg;
    } *t;

    XFSDEB(XDEBMSG, ("xfs_message_send opcode = %d\n", message->opcode));

    if (!(chan->status & CHANNEL_OPENED))	/* No receiver? */
	return ENODEV;

    /* Prepare message and copy it later */
    message->size = size;
    message->sequence_num = chan->nsequence++;

    t = xfs_alloc(sizeof(t->this_message) + size);
    t->this_message.error_or_size = sizeof(t->this_message) + size;
    bcopy(message, &t->msg, size);

    t->this_message.message = &t->msg;
    xfs_appendq(&chan->messageq, &t->this_message);
    if (chan->status & CHANNEL_WAITING) {
	chan->status &= ~CHANNEL_WAITING;
	wakeup((caddr_t) chan);
    }
    xfs_select_wakeup(chan);

    return 0;
}

/*
 * Send a message to user space and wait for reply.
 */
int
xfs_message_rpc(int fd, struct xfs_message_header * message, u_int size)
{
    int ret;
    struct xfs_channel *chan = &xfs_channel[fd];
    struct xfs_link *this_message;
    struct xfs_link *this_process;
    struct xfs_message_header *msg;
#if defined(HAVE_STRUCT_PROC_P_SIGMASK)
    sigset_t oldsigmask;
#endif /* HAVE_STRUCT_PROC_P_SIGMASK */

    XFSDEB(XDEBMSG, ("xfs_message_rpc opcode = %d\n", message->opcode));

    if (!(chan->status & CHANNEL_OPENED))	/* No receiver? */
	return ENODEV;

    if (size < sizeof(struct xfs_message_wakeup)) {
	printf("XFS PANIC Error: Message to small to receive wakeup, opcode = %d\n", message->opcode);
	return ENOMEM;
    }
    this_message = xfs_alloc(sizeof(struct xfs_link));
    this_process = xfs_alloc(sizeof(struct xfs_link));
    msg = xfs_alloc(size);
    bcopy(message, msg, size);

    msg->size = size;
    msg->sequence_num = chan->nsequence++;
    this_message->error_or_size = 0;
    this_message->message = msg;
    this_process->message = msg;
    xfs_appendq(&chan->messageq, this_message);
    xfs_appendq(&chan->sleepq, this_process);
    xfs_select_wakeup(chan);
    this_process->error_or_size = 0;

    if (chan->status & CHANNEL_WAITING) {
	chan->status &= ~CHANNEL_WAITING;
	wakeup((caddr_t) chan);
    }

    /*
     * Remove SIGIO from the sigmask so no IO will
     * wake us up from tsleep()
     */

#ifdef HAVE_STRUCT_PROC_P_SIGMASK
    oldsigmask = xfs_curproc()->p_sigmask;
#ifdef __sigaddset
    __sigaddset(&xfs_curproc()->p_sigmask, SIGIO);
#else
    xfs_curproc()->p_sigmask |= sigmask(SIGIO);
#endif /* __sigaddset */
#elif defined(HAVE_STRUCT_PROC_P_SIGWAITMASK)
    oldsigmask = xfs_curproc()->p_sigwaitmask;
    sigaddset(&xfs_curproc()->p_sigwaitmask, SIGIO);
#endif
    /*
     * We have to check if we have a receiver here too because the
     * daemon could have terminated before we sleep. This seems to
     * happen sometimes when rebooting.
     */
    if (!(chan->status & CHANNEL_OPENED) ||
	tsleep((caddr_t) this_process, (PZERO + 1) | PCATCH, "xfs", 0)) {
	XFSDEB(XDEBMSG, ("caught signal\n"));
	this_process->error_or_size = EINTR;
    }

#ifdef HAVE_STRUCT_PROC_P_SIGMASK
    xfs_curproc()->p_sigmask = oldsigmask;
#elif defined(HAVE_STRUCT_PROC_P_SIGWAITMASK)
    xfs_curproc()->p_sigwaitmask = oldsigmask;
#endif

    /*
     * Caught signal, got reply message or device was closed.
     * Need to clean up both messageq and sleepq.
     */
    if (xfs_onq(this_message)) {
	xfs_outq(this_message);
    }
    if (xfs_onq(this_process)) {
	xfs_outq(this_process);
    }
    ret = this_process->error_or_size;

    XFSDEB(XDEBMSG, ("xfs_message_rpc this_process->error_or_size = %d\n",
		     this_process->error_or_size));
    XFSDEB(XDEBMSG, ("xfs_message_rpc opcode ((xfs_message_wakeup*)(this_process->message))->error = %d\n", ((struct xfs_message_wakeup *) (this_process->message))->error));

    bcopy(msg, message, size);

    xfs_free(this_message, sizeof(*this_message));
    xfs_free(this_process, sizeof(*this_process));
    xfs_free(msg, size);

    return ret;
}

/*
 * For each message type there is a message handler
 * that implements its action, xfs_message_receive
 * invokes the correct function.
 */
int
xfs_message_receive(int fd,
		    struct xfs_message_header *message,
		    u_int size,
		    struct proc *p)
{
    XFSDEB(XDEBMSG, ("xfs_message_receive opcode = %d\n", message->opcode));

    /* Dispatch and coerce message type */
    switch (message->opcode) {
    case XFS_MSG_WAKEUP:
	return xfs_message_wakeup(fd,
				  (struct xfs_message_wakeup *) message,
				  message->size,
				  p);
    case XFS_MSG_WAKEUP_DATA:
	return xfs_message_wakeup_data(fd,
				 (struct xfs_message_wakeup_data *) message,
				       message->size,
				       p);
    case XFS_MSG_INSTALLROOT:
	return xfs_message_installroot(fd,
				 (struct xfs_message_installroot *) message,
				       message->size,
				       p);
    case XFS_MSG_INSTALLNODE:
	return xfs_message_installnode(fd,
				 (struct xfs_message_installnode *) message,
				       message->size,
				       p);
    case XFS_MSG_INSTALLATTR:
	return xfs_message_installattr(fd,
				 (struct xfs_message_installattr *) message,
				       message->size,
				       p);
    case XFS_MSG_INSTALLDATA:
	return xfs_message_installdata(fd,
				 (struct xfs_message_installdata *) message,
				       message->size,
				       p);
    case XFS_MSG_INVALIDNODE:
	return xfs_message_invalidnode(fd,
				 (struct xfs_message_invalidnode *) message,
				       message->size,
				       p);
    case XFS_MSG_UPDATEFID:
	return xfs_message_updatefid(fd,
				     (struct xfs_message_updatefid *)message,
				     message->size,
				     p);
    default:
	printf("XFS PANIC Warning xfs_dev: Unknown message opcode == %d\n",
	       message->opcode);
	return EINVAL;
    }
}

int
xfs_message_wakeup(int fd,
		   struct xfs_message_wakeup *message,
		   u_int size,
		   struct proc *p)
{
    struct xfs_channel *chan = &xfs_channel[fd];
    struct xfs_link *sleepq = &chan->sleepq;
    struct xfs_link *t = chan->sleepq.next;	/* Really first in q */

    XFSDEB(XDEBMSG, ("xfs_message_wakeup error: %d\n", message->error));

    for (; t != sleepq; t = t->next)
	if (t->message->sequence_num == message->sleepers_sequence_num) {
	    if (t->message->size < size) {
		printf("XFS PANIC Error: Could not wakeup requestor with opcode = %d properly, to small receive buffer.\n", t->message->opcode);
		t->error_or_size = ENOMEM;
	    } else
		bcopy(message, t->message, size);

	    wakeup((caddr_t) t);
	    break;
	}

    return 0;
}

int
xfs_message_wakeup_data(int fd,
			struct xfs_message_wakeup_data * message,
			u_int size,
			struct proc *p)
{
    struct xfs_channel *chan = &xfs_channel[fd];
    struct xfs_link *sleepq = &chan->sleepq;
    struct xfs_link *t = chan->sleepq.next;	/* Really first in q */

    XFSDEB(XDEBMSG, ("xfs_message_wakeup_data error: %d\n", message->error));

    for (; t != sleepq; t = t->next)
	if (t->message->sequence_num == message->sleepers_sequence_num) {
	    if (t->message->size < size) {
		printf("XFS PANIC Error: Could not wakeup requestor with opcode = %d properly, to small receive buffer.\n", t->message->opcode);
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
xfs_uprintf_device(void)
{
#if 0
    int i;

    for (i = 0; i < NXFS; i++) {
	uprintf("xfs_channel[%d] = {\n", i);
	uprintf("messageq.next = %p ", xfs_channel[i].messageq.next);
	uprintf("messageq.prev = %p ", xfs_channel[i].messageq.prev);
	uprintf("sleepq.next = %p ", xfs_channel[i].sleepq.next);
	uprintf("sleepq.prev = %p ", xfs_channel[i].sleepq.prev);
	uprintf("nsequence = %d status = %d\n",
		xfs_channel[i].nsequence,
		xfs_channel[i].status);
	uprintf("}\n");
    }
#endif
    return 0;
}
