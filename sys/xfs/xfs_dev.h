/* $OpenBSD: xfs_dev.h,v 1.4 2000/03/03 00:54:58 todd Exp $ */
/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
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


#ifndef _xfs_dev_h
#define _xfs_dev_h

/*
 * Queues of xfs_links hold outbound messages and processes sleeping
 * for replies. The last field is used to return error to sleepers and
 * to keep record of memory to be deallocated when messages have been
 * delivered or dropped.
 */

struct xfs_link {
    struct xfs_link *prev, *next;
    struct xfs_message_header *message;
    u_int error_or_size;	       /* error on sleepq and size on
				        * messageq */
};

struct xfs_channel {
    struct xfs_link messageq;	       /* Messages not yet read */
    struct xfs_link sleepq;	       /* Waiting for reply message */
    u_int nsequence;
#ifdef __osf__
    sel_queue_t sel_q;
#else
    struct selinfo selinfo;
#endif
    struct xfs_message_header *message_buffer;
    int status;
#define CHANNEL_OPENED	0x1
#define CHANNEL_WAITING 0x2
};

extern struct xfs_channel xfs_channel[NXFS];

/*
 * These are variant dependent
 */

int xfs_func_is_devopen(void*);
void xfs_select_wakeup(struct xfs_channel *);

int xfs_install_device(void);
int xfs_uninstall_device(void);

int xfs_install_filesys(void);
int xfs_may_uninstall_filesys(void);
int xfs_uninstall_filesys(void);

int xfs_stat_filesys(void);
int xfs_stat_device(void);

/*
 * And these should be generic
 */

void
xfs_initq(struct xfs_link *q);

int
xfs_emptyq(struct xfs_link *q);

int
xfs_onq(struct xfs_link *link);

void
xfs_appendq(struct xfs_link *q, struct xfs_link *p);

void
xfs_outq(struct xfs_link *p);

int
xfs_devopen_common(dev_t dev);

#ifndef __osf__ /* XXX - we should do the same for osf */
int xfs_devopen(dev_t dev, int flag, int devtype, struct proc *proc);
int xfs_devclose(dev_t dev, int flag, int devtype, struct proc *proc);
int xfs_devioctl(dev_t dev,
#if  defined(__NetBSD__) || defined(__OpenBSD__)
		 u_long cmd,
#else /* if defined(__FreeBSD__) */
		 int cmd,
#endif
		 caddr_t data, int flags, struct proc *p);
int xfs_devselect(dev_t dev, int which, struct proc *p);
#endif /* ! __osf__ */

int
xfs_devclose_common(dev_t dev, struct proc *p);

int
xfs_devread(dev_t dev, struct uio * uiop, int ioflag);

int
xfs_devwrite(dev_t dev, struct uio *uiop, int ioflag);

int
xfs_message_send(int fd, struct xfs_message_header * message, u_int size);

int
xfs_message_rpc(int fd, struct xfs_message_header * message, u_int size);

int
xfs_message_receive(int fd,
		    struct xfs_message_header *message,
		    u_int size,
		    struct proc *p);

int
xfs_message_wakeup(int fd,
		   struct xfs_message_wakeup *message,
		   u_int size,
		   struct proc *p);

int
xfs_message_wakeup_data(int fd,
			struct xfs_message_wakeup_data * message,
			u_int size,
			struct proc *p);

int
xfs_uprintf_device(void);

#endif /* _xfs_dev_h */
