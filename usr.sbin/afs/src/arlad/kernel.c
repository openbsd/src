/*
 * Copyright (c) 1995 - 2002 Kungliga Tekniska Högskolan
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

#include "arla_local.h"
RCSID("$arla: kernel.c,v 1.35 2003/01/10 17:33:44 tol Exp $");

/*
 * The fd we use to talk with the kernel on.
 */

int kernel_fd = -1;

/* count of the number of messages in a read */

static unsigned recv_count[20];

/* for more than above... */

static unsigned recv_count_overflow;

/*
 * Number of workers used and high
 */

static unsigned long workers_high, workers_used;


unsigned long
kernel_highworkers(void)
{
    return workers_high;
}

unsigned long
kernel_usedworkers(void)
{
    return workers_used;
}

/*
 *
 */

static int
process_message (int msg_length, char *msg)
{
    struct nnpfs_message_header *header;
    char *p;
    int cnt;

    cnt = 0;
    for (p = msg;
	 msg_length > 0;
	 p += header->size, msg_length -= header->size) {
	header = (struct nnpfs_message_header *)p;
	nnpfs_message_receive (kernel_fd, header, header->size);
	++cnt;
    }
    if (cnt < sizeof(recv_count)/sizeof(recv_count[0]))
	++recv_count[cnt];
    else
	++recv_count_overflow;
     
    return 0;
}

/* no threads available to handle messages */

static int overload = FALSE;

/*
 * The work threads.
 */

struct worker {
    char name[10];
    char data[MAX_XMSG_SIZE];
    PROCESS pid;
    int  msg_length;
    int  busyp;
    int  number;
} *workers;

static void
sub_thread (void *v_myself)
{
    struct worker *self = (struct worker *)v_myself;

    for (;;) {
	arla_warnx (ADEBKERNEL, "worker %d waiting", self->number);
	LWP_WaitProcess (self);
	self->busyp = 1;
	++workers_used;
	arla_warnx (ADEBKERNEL, "worker %d: processing", self->number);
	process_message (self->msg_length, self->data);
	arla_warnx (ADEBKERNEL, "worker %d: done", self->number);
	--workers_used;
	self->busyp = 0;
	overload = FALSE;
	LWP_NoYieldSignal(&overload);
    }
}

PROCESS version_pid;

static void
version_thread (void *foo)
{
    nnpfs_probe_version (kernel_fd, NNPFS_VERSION);
}

/*
 * The tcp communication unit
 */

static int
tcp_open (const char *filename)
{
    int s, ret, port;
    struct sockaddr_in addr;

    if (strlen (filename) == 0)
	arla_errx (1, ADEBERROR, "tcp_open doesn't contain tcp-port");

    port = atoi (filename);
    if (port == 0)
	arla_errx (1, ADEBERROR, "tcp_open couldn't parse %s as a port#",
		   filename);

    s = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) {
	arla_warn (ADEBWARN, errno, "tcp_open: socket failed");
	return s;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(0x7f000001); /* 127.0.0.1 */
    addr.sin_port = htons(port);
    ret = connect (s, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
	arla_warn (ADEBWARN, errno, "tcp_open: connect failed");
	return s;
    }
    return s;
}

static int
tcp_opendef (const char *filename)
{
    if (strlen (filename) != 0)
	arla_warnx (ADEBWARN, "tcp_opendef ignoring extra data");

    return tcp_open ("5000"); /* XXX */
}

static ssize_t
tcp_read (int fd, void *data, size_t len)
{
    int32_t slen;
    char in_len[4];
    if (recv (fd, in_len, sizeof(in_len), 0) != sizeof(in_len)) {
	arla_warn (ADEBWARN, errno, "tcp_read: failed to read length");
	return -1;
    }
    memcpy(&slen, in_len, sizeof(slen));
    slen = ntohl(slen);
    if (len < slen) {
	arla_warnx (ADEBWARN, 
		    "tcp_read: recv a too large message %d",
		    slen);	
	return -1;
    }
    return recv (fd, data, slen, 0) == slen ? slen : -1;
}

static ssize_t
tcp_write (int fd, const void *data, size_t len)
{
    int ret;
    int32_t slen = htonl(len);
    char out_len[4];

    memcpy (out_len, &slen, sizeof(len));
    if (send (fd, out_len, sizeof(len), 0) != sizeof(out_len)) {
	arla_warn (ADEBWARN, errno, "tcp_write: failed to write length");
	return -1;
    }
    ret = send (fd, data, len, 0);
    if (ret != len) {
	arla_warn (ADEBWARN, errno, "tcp_write: failed to write msg (%d)", 
		   ret);
	return -1;
    }

    return ret;
}

/*
 * The cdev communication unit
 */

static int
dev_open (const char *filename)
{
    char fn[MAXPATHLEN];
    snprintf (fn, MAXPATHLEN, "/%s", filename);
    return open (fn, O_RDWR);
}

static int
dev_fileopen (const char *filename)
{
    return dev_open (filename);
}

static ssize_t
dev_read (int fd, void *msg, size_t len)
{
    return read (fd, msg, len);
}

static ssize_t
dev_write (int fd, const void *msg, size_t len)
{
    return write (fd, msg, len);
}

/*
 * The null communication unit
 */

static int
null_open (const char *filename)
{
    return 0;
}

static ssize_t
null_read (int fd, void *msg, size_t len)
{
    return 0;
}

static ssize_t
null_write (int fd, const void *msg, size_t len)
{
    return len;
}

/*
 * Way to communticate with the kernel
 */ 

struct kern_interface {
    const char *prefix;
    int (*open) (const char *filename);
    ssize_t (*read) (int fd, void *msg, size_t len);
    ssize_t (*write) (int fd, const void *msg, size_t len);
} kern_comm[] = {
    { "/",	dev_open, dev_read, dev_write},
    { "file:/",	dev_fileopen, dev_read, dev_write},
    { "tcpport:", tcp_open, tcp_read, tcp_write},
    { "tcp",	tcp_opendef, tcp_read, tcp_write},
    { "null",	null_open, null_read, null_write},
    { NULL }
} ;

struct kern_interface *kern_cur = NULL;

static int
kern_open (const char *filename)
{
    struct kern_interface *ki = &kern_comm[0];
    int len;

    while (ki->prefix) {
	len = strlen (ki->prefix);
	if (strncasecmp (ki->prefix, filename, len) == 0) {
	    break;
	}    
	ki++;
    }
    if (ki->prefix == NULL)
	return -1;
    kern_cur = ki;
    return (ki->open) (filename+len);
}

ssize_t
kern_read (int fd, void *data, size_t len)
{
    assert (kern_cur != NULL);
    return (kern_cur->read) (fd, data, len);
}

ssize_t
kern_write (int fd, const void *data, size_t len)
{
    assert (kern_cur != NULL);
    return (kern_cur->write) (fd, data, len);
}

/*
 *
 */

void
kernel_opendevice (const char *dev)
{
    int fd;

    fd = kern_open (dev);
    if (fd < 0)
	arla_err (1, ADEBERROR, errno, "kern_open %s", dev);
    kernel_fd = fd;
    if (kernel_fd >= FD_SETSIZE)
	arla_errx (1, ADEBERROR, "kernel fd too large");
}


/*
 *
 */

#define WORKER_STACKSIZE (16*1024)

void
kernel_interface (struct kernel_args *args)
{
    int i;

    assert (kernel_fd >= 0);

    workers = malloc (sizeof(*workers) * args->num_workers);
    if (workers == NULL)
	arla_err (1, ADEBERROR, errno, "malloc %lu failed",
		  (unsigned long)sizeof(*workers) * args->num_workers);

    workers_high = args->num_workers;
    workers_used = 0;
 
    for (i = 0; i < args->num_workers; ++i) {
	workers[i].busyp  = 0;
	workers[i].number = i;
	snprintf(workers[i].name, sizeof(workers[i].name), "worker %d", i);

	if (LWP_CreateProcess (sub_thread, WORKER_STACKSIZE, 1,
			       (char *)&workers[i],
			       workers[i].name, &workers[i].pid))
	    arla_errx (1, ADEBERROR, "CreateProcess of worker failed");
    }

    if (LWP_CreateProcess (version_thread, WORKER_STACKSIZE, 1,
			   NULL,
			   "version", &version_pid))
	arla_errx (1, ADEBERROR, "CreateProcess of version thread failed");

    arla_warnx(ADEBKERNEL, "Arla: selecting on fd: %d", kernel_fd);

    for (;;) {
	fd_set readset;
	int ret;
	  
	FD_ZERO(&readset);
	FD_SET(kernel_fd, &readset);

	ret = IOMGR_Select (kernel_fd + 1, &readset, NULL, NULL, NULL); 

	if (ret < 0)
	    arla_warn (ADEBKERNEL, errno, "select");
	else if (ret == 0)
	    arla_warnx (ADEBKERNEL,
			"Arla: select returned with 0. strange.");
	else if (FD_ISSET(kernel_fd, &readset)) {
	    for (i = 0; i < args->num_workers; ++i) {
		if (workers[i].busyp == 0) {
		    ret = kern_read (kernel_fd, workers[i].data,
				     sizeof(workers[i].data));
		    if (ret <= 0) {
			arla_warn (ADEBWARN, errno, "read");
		    } else {
			workers[i].msg_length = ret;
			LWP_SignalProcess (&workers[i]);
		    }
		    break;
		}
	    }
	    if (i == args->num_workers) {
		arla_warnx (ADEBWARN, "kernel: all %u workers busy",
			    args->num_workers);
		overload = TRUE;
		LWP_WaitProcess(&overload);
	    }
	}
    }
}
