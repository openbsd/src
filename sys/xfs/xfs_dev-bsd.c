/*	$OpenBSD: xfs_dev-bsd.c,v 1.2 2000/03/03 00:54:58 todd Exp $	*/

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

RCSID("$OpenBSD: xfs_dev-bsd.c,v 1.2 2000/03/03 00:54:58 todd Exp $");

int
xfs_devopen(dev_t dev, int flag, int devtype, struct proc *proc)
{
    XFSDEB(XDEBDEV, ("xfsopen dev = %d.%d, flag = %d, devtype = %d\n", 
		     major(dev), minor(dev), flag, devtype));
    return xfs_devopen_common(dev);
}

/* XXX ugly function so we can keep xfs_devopen static */
int
xfs_func_is_devopen(void *func)
{
    return func == (void*)&xfs_devopen;
}

int
xfs_devclose(dev_t dev, int flag, int devtype, struct proc *p)
{
    XFSDEB(XDEBDEV, ("xfs_devclose dev = %d, flag = %d\n", dev, flag));
    return xfs_devclose_common(dev, p);
}

/*
 * Not used.
 */

int
xfs_devioctl(dev_t dev, 
#if defined(__NetBSD__) || defined(__OpenBSD__)
	     u_long cmd,
#elif defined(__FreeBSD__)
	     int cmd,
#endif
              caddr_t data, int flags, struct proc *p)
{
    XFSDEB(XDEBDEV, ("xfs_devioctl dev = %d.%d, cmd = %lu, "
		     "data = %p, flags = %x\n", 
		     major(dev), minor(dev), (unsigned long)cmd, data, flags));
    return ENOTTY;
}

static int
xfs_realselect(dev_t dev, struct proc *p)
{
    struct xfs_channel *chan = &xfs_channel[minor(dev)];

    if (!xfs_emptyq(&chan->messageq))
	return 1;		       /* Something to read */

    selrecord (p, &chan->selinfo);
    return 0;
}


#ifdef HAVE_VOP_POLL
static int
xfs_devpoll(dev_t dev, int events, struct proc * p)
{
    XFSDEB(XDEBDEV, ("xfs_devpoll dev = %d, events = %d\n", dev, events));

    if (!(events & POLLRDNORM))
	return 0;

    return xfs_realselect(dev, p);
}

#endif

#ifdef HAVE_VOP_SELECT
int
xfs_devselect(dev_t dev, int which, struct proc * p)
{
    XFSDEB(XDEBDEV, ("xfs_devselect dev = %d, which = %d\n", dev, which));

    if (which != FREAD)
	return 0;

    return xfs_realselect(dev, p);
}

#endif

void
xfs_select_wakeup(struct xfs_channel *chan)
{
    selwakeup (&chan->selinfo);
}

/*
 * Install and uninstall device.
 */

#if defined(__NetBSD__)
struct cdevsw xfs_dev = {
    xfs_devopen,
    xfs_devclose,
    xfs_devread,
    xfs_devwrite,
    xfs_devioctl,
    (dev_type_stop((*))) enodev,
    0,
    xfs_devpoll,
    (dev_type_mmap((*))) enodev,
    0
};

#elif defined(__OpenBSD__)
struct cdevsw xfs_dev = {
    xfs_devopen,
    xfs_devclose,
    xfs_devread,
    xfs_devwrite,
    xfs_devioctl,
    (dev_type_stop((*))) enodev,
    0,
    xfs_devselect,
    (dev_type_mmap((*))) enodev,
    0
};

#elif defined(__FreeBSD__)
struct cdevsw xfs_dev = {
    xfs_devopen,
    xfs_devclose,
    xfs_devread,
    xfs_devwrite,
    xfs_devioctl,
    nostop,
    noreset,
    nodevtotty,
#if defined(HAVE_VOP_SELECT)
    xfs_devselect,
#elif defined(HAVE_VOP_POLL)
    xfs_devpoll,
#else
#error select or poll: that is the question
#endif
    nommap,
    nostrategy,
    NULL,
    0
};

#endif /* FreeBSD */

int
xfs_install_device(void)
{
    int i;

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
    int i;
    struct xfs_channel *chan;

    for (i = 0; i < NXFS; i++) {
	chan = &xfs_channel[i];
	if (chan->status & CHANNEL_OPENED)
	    xfs_devclose(makedev(0, i), 0, 0, NULL);
    }

    return 0;
}

int
xfs_stat_device(void)
{
    return xfs_uprintf_device();
}
