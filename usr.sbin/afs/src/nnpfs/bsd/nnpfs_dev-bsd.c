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

RCSID("$arla: nnpfs_dev-bsd.c,v 1.35 2003/04/01 14:04:11 lha Exp $");

int
nnpfs_devopen(dev_t dev, int flag, int devtype, d_thread_t *proc)
{
    NNPFSDEB(XDEBDEV, ("nnpfsopen dev = %d.%d, flag = %d, devtype = %d\n", 
		     major(dev), minor(dev), flag, devtype));
    return nnpfs_devopen_common(dev);
}

int
nnpfs_devclose(dev_t dev, int flag, int devtype, d_thread_t *p)
{
#ifdef NNPFS_DEBUG
    char devname[64];
#endif

    NNPFSDEB(XDEBDEV, ("nnpfs_devclose dev = %s, flag = 0x%x\n",
		     nnpfs_devtoname_r(dev, devname, sizeof(devname)),
		     flag));
    return nnpfs_devclose_common(dev, p);
}

/*
 * Not used.
 */

int
nnpfs_devioctl(dev_t dev, 
	     u_long cmd,
	     caddr_t data,
	     int flags,
	     d_thread_t *p)
{
    NNPFSDEB(XDEBDEV, ("nnpfs_devioctl dev = %d.%d, cmd = %lu, "
		     "data = %lx, flags = %x\n", 
		     major(dev), minor(dev), (unsigned long)cmd,
		     (unsigned long)data, flags));
    return ENOTTY;
}

static int
nnpfs_realselect(dev_t dev, d_thread_t *p, void *wql)
{
    struct nnpfs_channel *chan = &nnpfs_channel[minor(dev)];

    if (!nnpfs_emptyq(&chan->messageq))
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
nnpfs_devpoll(dev_t dev, int events, d_thread_t * p)
{
#ifdef NNPFS_DEBUG
    char devname[64];
#endif

    NNPFSDEB(XDEBDEV, ("nnpfs_devpoll dev = %s, events = 0x%x\n",
		     nnpfs_devtoname_r (dev, devname, sizeof(devname)),
		     events));

    if (!(events & POLLRDNORM))
	return 0;

    return nnpfs_realselect(dev, p, NULL);
}

#endif

#ifdef HAVE_VOP_SELECT
#ifdef HAVE_THREE_ARGUMENT_SELRECORD
int
nnpfs_devselect(dev_t dev, int which, void *wql, struct proc * p)
{
    NNPFSDEB(XDEBDEV, ("nnpfs_devselect dev = %d, which = %d\n", dev, which));

    if (which != FREAD)
	return 0;

    return nnpfs_realselect(dev, p, wql);
}
#else
int
nnpfs_devselect(dev_t dev, int which, d_thread_t * p)
{
    NNPFSDEB(XDEBDEV, ("nnpfs_devselect dev = %d, which = %d\n", dev, which));

    if (which != FREAD)
	return 0;

    return nnpfs_realselect(dev, p, NULL);
}
#endif
#endif

void
nnpfs_select_wakeup(struct nnpfs_channel *chan)
{
    selwakeup (&chan->selinfo);
}

/*
 * Install and uninstall device.
 */

#if defined(__NetBSD__)
struct cdevsw nnpfs_dev = {
    nnpfs_devopen,
    nnpfs_devclose,
    nnpfs_devread,
    nnpfs_devwrite,
    nnpfs_devioctl,
    (dev_type_stop((*))) enodev,
    0,
    nnpfs_devpoll,
    (dev_type_mmap((*))) enodev,
    0
};

#elif defined(__OpenBSD__)
struct cdevsw nnpfs_dev = {
    nnpfs_devopen,
    nnpfs_devclose,
    nnpfs_devread,
    nnpfs_devwrite,
    nnpfs_devioctl,
    (dev_type_stop((*))) enodev,
    0,
    nnpfs_devselect,
    (dev_type_mmap((*))) enodev,
    0
};

#elif defined(__FreeBSD__)
struct cdevsw nnpfs_dev = {
    d_name: "nnpfs",
    d_open: nnpfs_devopen,
    d_close: nnpfs_devclose,
    d_read: nnpfs_devread,
    d_write: nnpfs_devwrite,
    d_ioctl: nnpfs_devioctl,
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
    d_select: nnpfs_devselect,
#elif defined(HAVE_VOP_POLL)
    d_poll: nnpfs_devpoll,
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
static struct cdevsw nnpfs_dev = {
      nnpfs_devopen,
          nnpfs_devclose,
          nnpfs_devread,
          nnpfs_devwrite,
          nnpfs_devioctl,
          eno_stop,
          eno_reset,
          0,
          nnpfs_devselect,
          eno_mmap,
          eno_strat,
          eno_getc,
          eno_putc,
          0
};
#endif /* __APPLE__ */

#if defined(__APPLE__)
extern int nnpfs_dev_major;
#include <miscfs/devfs/devfs.h>

static void *devfs_handles[NNNPFS];

#endif

int
nnpfs_install_device(void)
{
    int i;

#if defined(__APPLE__)
    nnpfs_dev_major = cdevsw_add(-1, &nnpfs_dev);
    if (nnpfs_dev_major == -1) {
	NNPFSDEB(XDEBDEV, ("failed installing cdev\n"));
	return ENFILE;
    }

    for (i = 0; i < NNNPFS; ++i)
	devfs_handles[i] = devfs_make_node(makedev(nnpfs_dev_major, i),
					   DEVFS_CHAR,
					   UID_ROOT, GID_WHEEL, 0600,
					   "nnpfs%d", i);

    NNPFSDEB(XDEBDEV, ("done installing cdev !\n"));
    NNPFSDEB(XDEBDEV, ("Char device number %d\n", nnpfs_dev_major));
#endif

    for (i = 0; i < NNNPFS; i++) {
	NNPFSDEB(XDEBDEV, ("before initq(messageq and sleepq)\n"));
	nnpfs_initq(&nnpfs_channel[i].messageq);
	nnpfs_initq(&nnpfs_channel[i].sleepq);
	nnpfs_channel[i].status = 0;
    }
    return 0;
}

int
nnpfs_uninstall_device(void)
{
    int i;
    struct nnpfs_channel *chan;
    int ret = 0;

    for (i = 0; i < NNNPFS; i++) {
	chan = &nnpfs_channel[i];
	if (chan->status & CHANNEL_OPENED)
	    nnpfs_devclose(makedev(0, i), 0, 0, NULL);
    }

#if defined(__APPLE__)
    for (i = 0; i < NNNPFS; ++i)
	devfs_remove (devfs_handles[i]);

    ret = cdevsw_remove(nnpfs_dev_major, &nnpfs_dev);
    if (ret == -1) {
	NNPFSDEB(XDEBLKM, ("nnpfs_uninstall_device error %d\n", ret));
    } else if (ret == nnpfs_dev_major) {
	ret = 0;
    } else {
	NNPFSDEB(XDEBLKM, ("nnpfs_uninstall_device unexpected error error %d\n",
			 ret));
    }
#endif
    NNPFSDEB(XDEBLKM, ("nnpfs_uninstall_device error %d\n", ret));
    return ret;
}

int
nnpfs_stat_device(void)
{
    return nnpfs_uprintf_device();
}

#if !defined(_LKM) && !defined(KLD_MODULE) && !defined(__APPLE__)
int
nnpfs_is_nnpfs_dev(dev_t dev)
{
    return major(dev) <= nchrdev &&
	cdevsw[major(dev)].d_open == nnpfs_devopen &&
	minor(dev) >= 0 && minor(dev) < NNNPFS;
}
#endif
