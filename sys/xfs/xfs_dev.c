/*	$OpenBSD: xfs_dev.c,v 1.2 1998/08/31 05:13:14 art Exp $	*/
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


#include <sys/types.h>
#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/select.h>
#include <sys/uio.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/fcntl.h>

#include <xfs/xfs_common.h>
#include <xfs/xfs_message.h>
#include <xfs/xfs_msg_locl.h>
#include <xfs/xfs_dev.h>
#include <xfs/xfs_fs.h>
#include <xfs/xfs_deb.h>
#include <xfs/xfs_extern.h>

RCSID("$KTH: xfs_dev.c,v 1.14 1998/07/19 00:19:53 art Exp $");

/*
 * Queues of xfs_links hold outbound messages and processes sleeping
 * for replies. The last field is used to return error to sleepers and
 * to keep record of memory to be deallocated when messages have been
 * delivered or dropped.
 */
struct xfs_link {
	struct xfs_link		*prev;
	struct xfs_link		*next;
	struct xfs_message_header *message;
	u_int			error_or_size;	/* error on sleepq and size on
						 * messageq */
};

struct xfs_channel {
	struct xfs_link		messageq;	/* Messages not yet read */
	struct xfs_link		sleepq;		/* Waiting for reply message */
	u_int			nsequence;
	struct proc		*selecting_proc;
	struct xfs_message_header *message_buffer;
	int			status;
#define CHANNEL_OPENED	0x1
};

static struct xfs_channel xfs_channel[NXFS];

static void
xfs_initq(struct xfs_link *q)
{
	q->next = q;
	q->prev = q;
}

/* Is this queue empty? */
#define xfs_emptyq(q) ((q)->next == (q))

/* Is this link on any queue? Link *must* be inited! */
#define xfs_onq(link) ((link)->next != 0 || (link)->prev != 0)

/* Append q with p */
static void
xfs_appendq(struct xfs_link *q, struct xfs_link *p)
{
	p->next = q;
	p->prev = q->prev;
	p->prev->next = p;
	q->prev = p;
}

static void
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
xfs_devopen(dev_t dev, int flags, int devtype, struct proc *p)
{
	struct xfs_channel *chan;

	XFSDEB(XDEBDEV, ("xfs_devopen dev = %d.%d, flags = %d\n", major(dev),
			 minor(dev), flags));

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

/*
 * Wakeup all sleepers and cleanup.
 */
int
xfs_devclose(dev_t dev, int flags, int devtype, struct proc *p)
{
	struct xfs_channel	*chan = &xfs_channel[minor(dev)];
	struct xfs_link		*first;

	XFSDEB(XDEBDEV, ("xfs_devclose dev = %d, flags = %d\n", dev, flags));

	/* Sanity check, paranoia? */
	if (!(chan->status & CHANNEL_OPENED))
		panic("xfs_devclose never opened?");

	chan->status &= ~CHANNEL_OPENED;

	/* No one is going to read those messages so empty queue! */
	while (!xfs_emptyq(&chan->messageq)) {
		XFSDEB(XDEBDEV, ("before outq(messageq)\n"));
		first = chan->messageq.next;
		xfs_outq(first);
		if (first->error_or_size != 0) {
			xfs_free(first, first->error_or_size);
			first = NULL;
		}

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

	if (chan->message_buffer) {
		xfs_free(chan->message_buffer, MAX_XMSG_SIZE);
		chan->message_buffer = NULL;
	}

	/*
	 * Free all xfs nodes.
	 * The force flag is set because we do not have any choice.
	 *
	 * Only try to unmount a mounted xfs...
	 */

	if (xfs[minor(dev)].mp != NULL) {
		if (vfs_busy(xfs[minor(dev)].mp, 0, 0, p)) {
			XFSDEB(XDEBNODE, ("xfs_dev_close: vfs_busy() BUSY\n"));
			return EBUSY;
		}
		free_all_xfs_nodes(&xfs[minor(dev)], FORCECLOSE);

		vfs_unbusy(xfs[minor(dev)].mp, p);
	}

	return 0;
}

/*
 * Move messages from kernel to user space.
 */
int
xfs_devread(dev_t dev, struct uio *uiop, int ioflag)
{
	struct xfs_channel	*chan = &xfs_channel[minor(dev)];
	struct xfs_link		*first;
	int			error = 0;

	XFSDEB(XDEBDEV, ("xfs_devread dev = %d\n", dev));

	XFSDEB(XDEBDEV, ("xfs_devread: m = %p, m->prev = %p, m->next = %p\n",
			 &chan->messageq, chan->messageq.prev,
			 chan->messageq.next));

	while (!xfs_emptyq (&chan->messageq)) {
		/* Remove message */
		first = chan->messageq.next;
		XFSDEB(XDEBDEV, ("xfs_devread: first = %p, "
				 "first->prev = %p, first->next = %p\n",
				 first, first->prev, first->next));
		XFSDEB(XDEBDEV, ("xfs_devread: message->size = %u\n",
				 first->message->size));

		error = uiomove((caddr_t) first->message,
				first->message->size, uiop);
		if (error)
			break;

		xfs_outq(first);

		if (first->error_or_size != 0) {
			xfs_free(first, first->error_or_size);
			first = NULL;
		}
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
	struct xfs_channel	*chan = &xfs_channel[minor(dev)];
	char			*p;
	int			error;
	u_int			cnt;
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
		msg_buf = (struct xfs_message_header *)p;
		error = xfs_message_receive (minor(dev),
					     msg_buf,
					     msg_buf->size,
					     uiop->uio_procp);
	}

	XFSDEB(XDEBDEV, ("xfs_devwrite error = %d\n", error));
	return error;
}

/*
 * Not used.
 */

int
#if defined(__NetBSD__) || defined(__OpenBSD__)
xfs_devioctl(dev_t dev, u_long cmd, caddr_t data, int flags, struct proc *p)
#elif defined(__FreeBSD__)
xfs_devioctl(dev_t dev, int cmd, caddr_t data, int flags, struct proc *p)
#endif
{
	XFSDEB(XDEBDEV, ("xfs_devioctl dev = %d, flags = %d\n", dev, flags));
	return EINVAL;
}

/*
 * Are there any messages on this filesystem?
 */

static int
xfs_realselect(dev_t dev, struct proc * p)
{
	struct xfs_channel *chan = &xfs_channel[minor(dev)];

	if (!xfs_emptyq(&chan->messageq))
		return 1;		       /* Something to read */

	/*
	 * No need to handle a "collission" since we only allow one
	 * concurrent open.
	 */
	chan->selecting_proc = p;

	return 0;
}


#ifdef USE_POLL
static int
xfs_devpoll(dev_t dev, int events, struct proc *p)
{
	XFSDEB(XDEBDEV, ("xfs_devpoll dev = %d, events = %d\n", dev, events));

	if (!(events & POLLRDNORM))
		return 0;

	return xfs_realselect(dev, p);
}

#endif

#ifdef USE_SELECT
int
xfs_devselect(dev_t dev, int which, struct proc *p)
{
	XFSDEB(XDEBDEV, ("xfs_devselect dev = %d, which = %d\n", dev, which));

	if (which != FREAD)
		return 0;

	return xfs_realselect(dev, p);
}

#endif

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
	if (chan->selecting_proc != 0
	    && chan->selecting_proc->p_wchan == (caddr_t) & selwait) {
		struct selinfo selinfo;

#if defined(__NetBSD__) || defined(__FreeBSD__)
		selinfo.si_pid = chan->selecting_proc->p_pid;
#else
		selinfo.si_selpid = chan->selecting_proc->p_pid;
#endif
		selinfo.si_flags = 0;

		selwakeup(&selinfo);
		chan->selecting_proc = 0;
	}

    return 0;
}

/*
 * Send a message to user space and wait for reply.
 */
int
xfs_message_rpc(int fd, struct xfs_message_header *message, u_int size)
{
	int			ret;
	struct xfs_channel	*chan = &xfs_channel[fd];
	struct xfs_link		*this_message;
	struct xfs_link		*this_process;
	struct xfs_message_header *msg;

	this_message = xfs_alloc(sizeof(struct xfs_link));
	this_process = xfs_alloc(sizeof(struct xfs_link));

	XFSDEB(XDEBMSG, ("xfs_message_rpc opcode = %d\n", message->opcode));

	if (!(chan->status & CHANNEL_OPENED))	/* No receiver? */
		return ENODEV;

#ifdef DIAGNOSTIC
	if (size < sizeof(struct xfs_message_wakeup)) {
		printf("XFS Error: Message to small for wakeup, opcode = %d\n",
		       message->opcode);
		return ENOMEM;
	}
#endif
	msg = xfs_alloc(size);
	bcopy(message, msg, size);

	msg->size = size;
	msg->sequence_num = chan->nsequence++;
	this_message->error_or_size = 0;
	this_message->message = msg;       /* message; */
	this_process->message = msg;       /* message; */
	xfs_appendq(&chan->messageq, this_message);
	xfs_appendq(&chan->sleepq, this_process);
	if (chan->selecting_proc != 0
	    && chan->selecting_proc->p_wchan == (caddr_t) & selwait) {
		struct selinfo selinfo;

#if defined(__NetBSD__) || defined(__FreeBSD__)
		selinfo.si_pid = chan->selecting_proc->p_pid;
#else
		selinfo.si_selpid = chan->selecting_proc->p_pid;
#endif
		selinfo.si_flags = 0;

		selwakeup(&selinfo);
		chan->selecting_proc = 0;
	}
	this_process->error_or_size = 0;

	if (tsleep((caddr_t) this_process, (PZERO + 1) | PCATCH, "xfs", 0)) {
		XFSDEB(XDEBMSG, ("caught signal\n"));
		this_process->error_or_size = EINTR;
	}
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
	XFSDEB(XDEBMSG, ("xfs_message_rpc this_process->error = %d\n",
			 ((struct xfs_message_wakeup *) (this_process->message))->error));

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
	XFSDEB(XDEBMSG, ("xfs_message_receive opcode = %d\n",
			 message->opcode));

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
	default:
		printf("XFS Warning xfs_dev: Unknown message opcode == %d\n",
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
	struct xfs_channel	*chan = &xfs_channel[fd];
	struct xfs_link		*sleepq = &chan->sleepq;
	struct xfs_link		*t = chan->sleepq.next;	/* Really first in q */

	XFSDEB(XDEBMSG, ("xfs_message_wakeup error: %d\n", message->error));

	for (; t != sleepq; t = t->next)
		if (t->message->sequence_num ==
		    message->sleepers_sequence_num) {
			if (t->message->size < size) {
				printf("XFS Error: Could not wakeup requestor with opcode = %d properly, to small receive buffer.\n", t->message->opcode);
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
			struct xfs_message_wakeup_data *message,
			u_int size,
			struct proc *p)
{
	struct xfs_channel	*chan = &xfs_channel[fd];
	struct xfs_link		*sleepq = &chan->sleepq;
	struct xfs_link		*t = chan->sleepq.next;	/* Really first in q */

	XFSDEB(XDEBMSG, ("xfs_message_wakeup_data error: %d\n",
			 message->error));

	for (; t != sleepq; t = t->next)
		if (t->message->sequence_num ==
		    message->sleepers_sequence_num) {
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

#ifdef ACTUALLY_LKM_NOT_KERNEL
/*
 *
 */
static int
xfs_uprintf_device(void)
{
#if 0
	int i;

	for (i = 0; i < NXFS; i++) {
		uprintf("xfs_channel[%d] = {\n", i);
		uprintf("messageq.next = 0x%x ", (u_int) xfs_channel[i].messageq.next);
		uprintf("messageq.prev = 0x%x ", (u_int) xfs_channel[i].messageq.prev);
		uprintf("sleepq.next = 0x%x ", (u_int) xfs_channel[i].sleepq.next);
		uprintf("sleepq.prev = 0x%x ", (u_int) xfs_channel[i].sleepq.prev);
		uprintf("nsequence = %d selecting_proc = 0x%x status = %d\n",
			xfs_channel[i].nsequence,
			(u_int) xfs_channel[i].selecting_proc,
			xfs_channel[i].status);
		uprintf("}\n");
	}
#endif
	return 0;
}

/*
 * Install and uninstall device.
 */

#if defined(__NetBSD__) || defined(__OpenBSD__)
static struct cdevsw xfs_dev = {
	xfs_devopen,
	xfs_devclose,
	xfs_devread,
	xfs_devwrite,
	xfs_devioctl,
	(dev_type_stop((*))) enodev,
	0,
#ifdef __OpenBSD__
	xfs_devselect,
#else
	xfs_devpoll,
#endif
	(dev_type_mmap((*))) enodev,
	0
};
#elif defined(__FreeBSD__)
static struct cdevsw xfs_dev = {
	xfs_devopen,
	xfs_devclose,
	xfs_devread,
	xfs_devwrite,
	xfs_devioctl,
	nostop,
	noreset,
	nodevtotty,
	xfs_devselect,
	nommap,
	nostrategy,
	NULL,
	0
};
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__)

static int dev_offset;
static struct cdevsw dev_olddev;

int
xfs_install_device(void)
{
	int i;

	/*
	 * Search the table looking for a slot...
	 */
	for (i = 0; i < nchrdev; i++)
		if (cdevsw[i].d_open == (dev_type_open((*))) lkmenodev)
			break;		       /* found it! */
	/* out of allocable slots? */
	if (i == nchrdev) {
		XFSDEB(XDEBDEV, ("xfs_install_device: no available slots\n"));
		return (ENFILE);
	}

	/* save old */
	dev_olddev = cdevsw[i];

	/* replace with new */
	cdevsw[i] = xfs_dev;

	printf("done installing cdev !\n");

	/* done! */
	dev_offset = i;

	printf("Char device number %d\n", i);

	for (i = 0; i < NXFS; i++) {
		XFSDEB(XDEBDEV, ("before initq(messageq and sleepq)\n"));
		xfs_initq(&xfs_channel[i].messageq);
		xfs_initq(&xfs_channel[i].sleepq);
		xfs_channel[i].status = 0;
	}

	return 0;
}

int
xfs_uninstall_device(void)
{
	int		i;
	dev_t		dev;
	struct xfs_channel *chan;

	for (i = 0; i < NXFS; i++) {
		dev = makedev(dev_offset, i);
		chan = &xfs_channel[minor(dev)];
		if (chan->status & CHANNEL_OPENED)
			xfs_devclose(dev, 0, 0, NULL);
	}

	/* replace current slot contents with old contents */
	cdevsw[dev_offset] = dev_olddev;

	return 0;
}

int
xfs_stat_device(void)
{
	return xfs_uprintf_device();
}

#elif defined(__FreeBSD__)

int
xfs_install_device(void)
{
	int	i;
	int	err;
	dev_t	dev = NODEV;

	err = cdevsw_add(&dev,
			 &xfs_dev,
			 NULL);
	if (err)
		return err;
	printf("char device number %d\n", major(dev));

	for (i = 0; i < NXFS; i++) {
		XFSDEB(XDEBDEV, ("before initq(messageq and sleepq)\n"));
		xfs_initq(&xfs_channel[i].messageq);
		xfs_initq(&xfs_channel[i].sleepq);
	}

	return 0;
}

int
xfs_uninstall_device(void)
{
	int		i;
	struct xfs_channel *chan;
	dev_t		dev = makedev(xfs_dev.d_maj, 0);

	for (i = 0; i < NXFS; i++) {
		chan = &xfs_channel[minor(dev)];
		if (chan->status & CHANNEL_OPENED)
			xfs_devclose(dev, 0, 0, NULL);
	}

	/* replace current slot contents with old contents */
	cdevsw_add(&dev, NULL, NULL);

	return 0;
}

int
xfs_stat_device(void)
{
	return xfs_uprintf_device();
}

#endif
#endif /* ACTUALLY_LKM_NOT_KERNEL */
