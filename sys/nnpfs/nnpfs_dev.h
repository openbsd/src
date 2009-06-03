/*
 * Copyright (c) 1995 - 2001 Kungliga Tekniska Högskolan
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

/* $arla: nnpfs_dev.h,v 1.19 2003/01/19 20:53:52 lha Exp $ */

#ifndef _nnpfs_dev_h
#define _nnpfs_dev_h

/*
 * Queues of nnpfs_links hold outbound messages and processes sleeping
 * for replies. The last field is used to return error to sleepers and
 * to keep record of memory to be deallocated when messages have been
 * delivered or dropped.
 */

struct nnpfs_link {
    struct nnpfs_link *prev, *next;
    struct nnpfs_message_header *message;
    u_int error_or_size;	       /* error on sleepq and size on
				        * messageq */
};

struct nnpfs_channel {
    struct nnpfs_link messageq;	       /* Messages not yet read */
    struct nnpfs_link sleepq;	       /* Waiting for reply message */
    u_int nsequence;
#ifdef __osf__
    sel_queue_t sel_q;
#else
    struct selinfo selinfo;
#endif
    struct nnpfs_message_header *message_buffer;
    int status;
#define CHANNEL_OPENED	0x1
#define CHANNEL_WAITING 0x2
    d_thread_t *proc;
};

extern struct nnpfs_channel nnpfs_channel[NNNPFS];

/*
 * These are variant dependent
 */

void nnpfs_select_wakeup(struct nnpfs_channel *);

int nnpfs_install_device(void);
int nnpfs_uninstall_device(void);

int nnpfs_install_filesys(void);
int nnpfs_may_uninstall_filesys(void);
int nnpfs_uninstall_filesys(void);

int nnpfs_stat_filesys(void);
int nnpfs_stat_device(void);

/*
 * And these should be generic
 */

void
nnpfs_initq(struct nnpfs_link *q);

int
nnpfs_emptyq(const struct nnpfs_link *q);

int
nnpfs_onq(const struct nnpfs_link *link);

void
nnpfs_appendq(struct nnpfs_link *q, struct nnpfs_link *p);

void
nnpfs_outq(struct nnpfs_link *p);

int
nnpfs_devopen_common(dev_t dev);

#ifndef __osf__ /* XXX - we should do the same for osf */
int nnpfs_devopen(dev_t dev, int flag, int devtype, d_thread_t *proc);
int nnpfs_devclose(dev_t dev, int flag, int devtype, d_thread_t *proc);
int nnpfs_devioctl(dev_t dev, u_long cmd, caddr_t data, int flags,
		 d_thread_t *p);
#ifdef HAVE_THREE_ARGUMENT_SELRECORD
int nnpfs_devselect(dev_t dev, int which, void *wql, d_thread_t *p);
#else
int nnpfs_devselect(dev_t dev, int which, d_thread_t *p);
#endif
int nnpfs_devpoll(dev_t dev, int events, d_thread_t *p);
#endif /* ! __osf__ */

int
nnpfs_devclose_common(dev_t dev, d_thread_t *p);

int
nnpfs_devread(dev_t dev, struct uio * uiop, int ioflag);

int
nnpfs_devwrite(dev_t dev, struct uio *uiop, int ioflag);

int
nnpfs_message_send(int fd, struct nnpfs_message_header * message, u_int size);

int
nnpfs_message_rpc(int fd, struct nnpfs_message_header * message, u_int size,
		d_thread_t *p);

int
nnpfs_message_receive(int fd,
		    struct nnpfs_message_header *message,
		    u_int size,
		    d_thread_t *p);

int
nnpfs_message_wakeup(int fd,
		   struct nnpfs_message_wakeup *message,
		   u_int size,
		   d_thread_t *p);

int
nnpfs_message_wakeup_data(int fd,
			struct nnpfs_message_wakeup_data * message,
			u_int size,
			d_thread_t *p);

int
nnpfs_uprintf_device(void);

int
nnpfs_is_nnpfs_dev (dev_t dev);

#endif /* _nnpfs_dev_h */
