/* $OpenBSD: xfs_dev-bsd.c,v 1.10 2004/01/18 21:46:54 beck Exp $

 * Copyright (c) 2004 Bob Beck
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

#include <xfs/xfs_locl.h>
#include <xfs/xfs_message.h>
#include <xfs/xfs_msg_locl.h>
#include <xfs/xfs_fs.h>
#include <xfs/xfs_dev.h>
#include <xfs/xfs_deb.h>

int
xfs_devopen(dev_t dev, int flag, int devtype, d_thread_t *proc)
{
	NNPFSDEB(XDEBDEV, ("xfsopen dev = %d.%d, flag = %d, devtype = %d\n", 
	    major(dev), minor(dev), flag, devtype));
	return xfs_devopen_common(dev);
}

int
xfs_devclose(dev_t dev, int flag, int devtype, d_thread_t *p)
{
	NNPFSDEB(XDEBDEV, ("xfs_devclose dev = %d(%d), flag = 0x%x\n",
	   major(dev), minor(dev), flag));

	return xfs_devclose_common(dev, p);
}

int
xfs_devioctl(dev_t dev,  u_long cmd,  caddr_t data, int flags,  d_thread_t *p)
{
	NNPFSDEB(XDEBDEV, ("xfs_devioctl dev = %d.%d, cmd = %lu, "
	    "data = %lx, flags = %x\n", major(dev), minor(dev), 
	    (unsigned long)cmd, (unsigned long)data, flags));
	return ENOTTY;
}

int
xfs_devpoll(dev_t dev, int events, d_thread_t * p)
{
	struct xfs_channel *chan = &xfs_channel[minor(dev)];
	
	NNPFSDEB(XDEBDEV, ("xfs_devpoll dev = %d(%d), events = 0x%x\n",
	    major(dev), minor(dev), events));

	if ((events & (POLLIN | POLLRDNORM)) == 0)
		return 0;			/* only supports read */
	
	if (!xfs_emptyq(&chan->messageq))
		return (events & (POLLIN | POLLRDNORM));
	
	selrecord (p, &chan->selinfo);

	return 0;
}

void
xfs_select_wakeup(struct xfs_channel *chan)
{
	selwakeup (&chan->selinfo);
}

/*
 * Install and uninstall device.
 */

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

int
xfs_install_device(void)
{
	int i;

	for (i = 0; i < NNNPFS; i++) {
		NNPFSDEB(XDEBDEV, ("before initq(messageq and sleepq)\n"));
		xfs_initq(&xfs_channel[i].messageq);
		xfs_initq(&xfs_channel[i].sleepq);
		xfs_channel[i].status = 0;
	}
	return 0;
}

int
xfs_uninstall_device(void)
{
	int i;
	struct xfs_channel *chan;
	int ret = 0;
	
	for (i = 0; i < NNNPFS; i++) {
		chan = &xfs_channel[i];
		if (chan->status & CHANNEL_OPENED)
			xfs_devclose(makedev(0, i), 0, 0, NULL);
	}
	
	NNPFSDEB(XDEBLKM, ("xfs_uninstall_device error %d\n", ret));
	return ret;
}

int
xfs_stat_device(void)
{
	return xfs_uprintf_device();
}

int
xfs_is_xfs_dev(dev_t dev)
{
    return major(dev) <= nchrdev &&
	cdevsw[major(dev)].d_open == xfs_devopen &&
	minor(dev) >= 0 && minor(dev) < NNNPFS;
}
