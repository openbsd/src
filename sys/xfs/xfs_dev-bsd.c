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

#include <xfs/xfs_locl.h>
#include <xfs/xfs_message.h>
#include <xfs/xfs_msg_locl.h>
#include <xfs/xfs_fs.h>
#include <xfs/xfs_dev.h>
#include <xfs/xfs_deb.h>

RCSID("$arla: xfs_dev-bsd.c,v 1.35 2003/04/01 14:04:11 lha Exp $");

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
#ifdef NNPFS_DEBUG
    char devname[64];
#endif

    NNPFSDEB(XDEBDEV, ("xfs_devclose dev = %s, flag = 0x%x\n",
		     xfs_devtoname_r(dev, devname, sizeof(devname)),
		     flag));
    return xfs_devclose_common(dev, p);
}

/*
 * Not used.
 */

int
xfs_devioctl(dev_t dev, 
	     u_long cmd,
	     caddr_t data,
	     int flags,
	     d_thread_t *p)
{
    NNPFSDEB(XDEBDEV, ("xfs_devioctl dev = %d.%d, cmd = %lu, "
		     "data = %lx, flags = %x\n", 
		     major(dev), minor(dev), (unsigned long)cmd,
		     (unsigned long)data, flags));
    return ENOTTY;
}

static int
xfs_realselect(dev_t dev, d_thread_t *p, void *wql)
{
    struct xfs_channel *chan = &xfs_channel[minor(dev)];

    if (!xfs_emptyq(&chan->messageq))
	return 1;		       /* Something to read */

#ifdef HAVE_THREE_ARGUMENT_SELRECORD
    selrecord (p, &chan->selinfo, wql);
#else
    selrecord (p, &chan->selinfo);
#endif
    return 0;
}


#ifdef HAVE_VOP_POLL
static int
xfs_devpoll(dev_t dev, int events, d_thread_t * p)
{
#ifdef NNPFS_DEBUG
    char devname[64];
#endif

    NNPFSDEB(XDEBDEV, ("xfs_devpoll dev = %s, events = 0x%x\n",
		     xfs_devtoname_r (dev, devname, sizeof(devname)),
		     events));

    if (!(events & POLLRDNORM))
	return 0;

    return xfs_realselect(dev, p, NULL);
}

#endif

#ifdef HAVE_VOP_SELECT
#ifdef HAVE_THREE_ARGUMENT_SELRECORD
int
xfs_devselect(dev_t dev, int which, void *wql, struct proc * p)
{
    NNPFSDEB(XDEBDEV, ("xfs_devselect dev = %d, which = %d\n", dev, which));

    if (which != FREAD)
	return 0;

    return xfs_realselect(dev, p, wql);
}
#else
int
xfs_devselect(dev_t dev, int which, d_thread_t * p)
{
    NNPFSDEB(XDEBDEV, ("xfs_devselect dev = %d, which = %d\n", dev, which));

    if (which != FREAD)
	return 0;

    return xfs_realselect(dev, p, NULL);
}
#endif
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
    d_name: "xfs",
    d_open: xfs_devopen,
    d_close: xfs_devclose,
    d_read: xfs_devread,
    d_write: xfs_devwrite,
    d_ioctl: xfs_devioctl,
    d_mmap: nommap,
    d_strategy: nostrategy,
    d_flags: 0,
#ifdef HAVE_STRUCT_CDEVSW_D_STOP
    d_stop: nostop,
#endif
#ifdef HAVE_STRUCT_CDEVSW_D_RESET
    d_reset: noreset,
#endif
#ifdef HAVE_STRUCT_CDEVSW_D_BOGORESET
    d_bogoreset: noreset,
#endif
#ifdef HAVE_STRUCT_CDEVSW_D_DEVTOTTY
    d_devtotty: nodevtotty,
#endif
#if defined(HAVE_VOP_SELECT)
    d_select: xfs_devselect,
#elif defined(HAVE_VOP_POLL)
    d_poll: xfs_devpoll,
#else
#error select or poll: that is the question
#endif
#ifdef HAVE_STRUCT_CDEVSW_D_BOGOPARMS
    d_bogoparms: noparms,
#endif
#ifdef HAVE_STRUCT_CDEVSW_D_SPARE
    d_spare: NULL,
#endif
    d_maj: 128,			/* XXX */
    d_dump: nodump,
#ifdef HAVE_STRUCT_CDEVSW_D_PSIZE
    d_psize: nopsize,
#endif
#ifdef HAVE_STRUCT_CDEVSW_D_MAXIO
    d_maxio: 0,
#endif
#ifdef HAVE_STRUCT_CDEVSW_D_BMAJ
#ifdef NOUDEV
    d_bmaj: NOUDEV
#else
    d_bmaj: NODEV
#endif
#endif /* HAVE_STRUCT_CDEVSW_D_BMAJ */
};

#elif defined(__APPLE__)
static struct cdevsw xfs_dev = {
      xfs_devopen,
          xfs_devclose,
          xfs_devread,
          xfs_devwrite,
          xfs_devioctl,
          eno_stop,
          eno_reset,
          0,
          xfs_devselect,
          eno_mmap,
          eno_strat,
          eno_getc,
          eno_putc,
          0
};
#endif /* __APPLE__ */

#if defined(__APPLE__)
extern int xfs_dev_major;
#include <miscfs/devfs/devfs.h>

static void *devfs_handles[NNNPFS];

#endif

int
xfs_install_device(void)
{
    int i;

#if defined(__APPLE__)
    xfs_dev_major = cdevsw_add(-1, &xfs_dev);
    if (xfs_dev_major == -1) {
	NNPFSDEB(XDEBDEV, ("failed installing cdev\n"));
	return ENFILE;
    }

    for (i = 0; i < NNNPFS; ++i)
	devfs_handles[i] = devfs_make_node(makedev(xfs_dev_major, i),
					   DEVFS_CHAR,
					   UID_ROOT, GID_WHEEL, 0600,
					   "xfs%d", i);

    NNPFSDEB(XDEBDEV, ("done installing cdev !\n"));
    NNPFSDEB(XDEBDEV, ("Char device number %d\n", xfs_dev_major));
#endif

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

#if defined(__APPLE__)
    for (i = 0; i < NNNPFS; ++i)
	devfs_remove (devfs_handles[i]);

    ret = cdevsw_remove(xfs_dev_major, &xfs_dev);
    if (ret == -1) {
	NNPFSDEB(XDEBLKM, ("xfs_uninstall_device error %d\n", ret));
    } else if (ret == xfs_dev_major) {
	ret = 0;
    } else {
	NNPFSDEB(XDEBLKM, ("xfs_uninstall_device unexpected error error %d\n",
			 ret));
    }
#endif
    NNPFSDEB(XDEBLKM, ("xfs_uninstall_device error %d\n", ret));
    return ret;
}

int
xfs_stat_device(void)
{
    return xfs_uprintf_device();
}

#if !defined(_LKM) && !defined(KLD_MODULE) && !defined(__APPLE__)
int
xfs_is_xfs_dev(dev_t dev)
{
    return major(dev) <= nchrdev &&
	cdevsw[major(dev)].d_open == xfs_devopen &&
	minor(dev) >= 0 && minor(dev) < NNNPFS;
}
#endif
