/*
 * Copyright (c) 1999 - 2002 Kungliga Tekniska Högskolan
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

RCSID("$arla: nnpfs.c,v 1.20 2002/10/28 11:16:39 haba Exp $");

/*
 * Begining of breakout of nnpfs releated junk
 */

static u_int *seqnums;

static List *sleepers;

/* number of times each type of message has been sent/recv */

static struct {
    unsigned long sent;
    unsigned long recv;
} nnpfs_stats[NNPFS_MSG_COUNT];

static const char *rcvfuncs_name[] = 
{
  "version",
  "wakeup",
  "getroot",
  "installroot",
  "getnode",
  "installnode",
  "getattr",
  "installattr",
  "getdata",
  "installdata",
  "inactivenode",
  "invalidnode",
  "open",
  "put_data",
  "put_attr",
  "create",
  "mkdir",
  "link",
  "symlink",
  "remove",
  "rmdir",
  "rename",
  "pioctl",
  "wakeup_data",
  "updatefid",
  "advlock",
  "gc nodes"
};

/*
 * A interface for the userland to talk the kernel and recv
 * back a integer. For larger messages implement a simularfunction
 * that uses the `wakeup_data' message.
 */

int
nnpfs_message_rpc (int fd, struct nnpfs_message_header *h, u_int size)
{
    int ret;

    ret = nnpfs_message_send (fd, h, size);
    if (ret)
	return ret;
    return nnpfs_message_sleep (h->sequence_num);
}

/*
 * Try to probe the version on `fd', returning the version.
 */

static int
nnpfs_send_message_version (int fd)
{
     struct nnpfs_message_version msg;
     int ret;

     msg.header.opcode = NNPFS_MSG_VERSION;
     arla_warnx (ADEBMSG, "sending version");
     ret = nnpfs_message_rpc (fd, (struct nnpfs_message_header *)&msg, 
			    sizeof(msg));
     return ret;
}

/*
 * Probe for version on `fd'.  Fail if != version
 */

void
nnpfs_probe_version (int fd, int version)
{
    int ret = nnpfs_send_message_version (fd);

    if (ret != version)
	arla_errx (1, ADEBERROR,
		   "Version mismatch. Nnpfs is version %d and arlad is version %d. Please {up,down}grade.",
		   ret, version);
}

/*
 * Send `num' `fids' to nnpfs on `fd' as proposed gc-able fids
 * If `num' is 0 nnpfs should gc everything gc:able.
 */

/* XXX VenusFid is wrong here */

void
nnpfs_send_message_gc_nodes (int fd, int num, VenusFid *fids)
{
    struct nnpfs_message_gc_nodes msg;
    int i;
    
    arla_warnx (ADEBMSG, 
		"nnpfs_send_message_gc_nodes sending gc: num = %d", num);
    
    if (num > NNPFS_GC_NODES_MAX_HANDLE)
	num = NNPFS_GC_NODES_MAX_HANDLE;
    
    msg.header.opcode = NNPFS_MSG_GC_NODES;
    msg.len = num;
    
    for (i = 0; i < num; i++)
	memcpy (&msg.handle[i], &fids[i], sizeof(*fids));
     
    nnpfs_message_send (fd, (struct nnpfs_message_header *)&msg, 
		      sizeof(msg));
}

/*
 * Init the nnpfs message passing things.
 */

void
nnpfs_message_init (void)
{
     unsigned i;

     seqnums = (u_int *)malloc (sizeof (*seqnums) * getdtablesize ());
     if (seqnums == NULL)
	 arla_err (1, ADEBERROR, errno, "nnpfs_message_init: malloc");
     for (i = 0; i < getdtablesize (); ++i)
	  seqnums[i] = 0;
     sleepers = listnew ();
     if (sleepers == NULL)
	 arla_err (1, ADEBERROR, errno, "nnpfs_message_init: listnew");

     assert (sizeof(rcvfuncs_name) / sizeof(*rcvfuncs_name) == NNPFS_MSG_COUNT);
}

/*
 * Go to entry in jump-table depending on entry.
 */

int
nnpfs_message_receive (int fd, struct nnpfs_message_header *h, u_int size)
{
     unsigned opcode = h->opcode;

     if (opcode >= NNPFS_MSG_COUNT || rcvfuncs[opcode] == NULL ) {
	  arla_warnx (ADEBMSG, "Bad message opcode = %u", opcode);
	  return -1;
     }

     ++nnpfs_stats[opcode].recv;

     arla_warnx (ADEBMSG, "Rec message: opcode = %u (%s), size = %u",
		 opcode, rcvfuncs_name[opcode], h->size);

     return (*rcvfuncs[opcode])(fd, h, size);
}

/*
 * Send a message to the kernel module.
 */

int
nnpfs_message_send (int fd, struct nnpfs_message_header *h, u_int size)
{
     unsigned opcode = h->opcode;
     int ret;

     h->size = size;
     h->sequence_num = seqnums[fd]++;

     if (opcode >= NNPFS_MSG_COUNT) {
	  arla_warnx (ADEBMSG, "Bad message opcode = %u", opcode);
	  return -1;
     }

     ++nnpfs_stats[opcode].sent;

     arla_warnx (ADEBMSG, "Send message: opcode = %u (%s), size = %u",
		 opcode, rcvfuncs_name[opcode], h->size);

     ret = kern_write (fd, h, size);
     if (ret != size) {
	 arla_warn (ADEBMSG, errno, "nnpfs_message_send: write");
	 return errno;
     } else
	 return 0;
}

/*
 * This code can only wake up message of type `nnpfs_message_wakeup'
 */

int
nnpfs_message_wakeup (int fd, struct nnpfs_message_wakeup *h, u_int size)
{
     Listitem *i, *next;
     struct nnpfs_message_wakeup *w;

     assert (sizeof(*w) >= size);

     for (i = listhead (sleepers); i; i = next) {
	  next = listnext (sleepers, i);
	  w = (struct nnpfs_message_wakeup *)listdata(i);
	  if (w->header.sequence_num == h->sleepers_sequence_num) {
	       listdel (sleepers, i);
	       memcpy (w, h, size);
	       LWP_SignalProcess ((char *)w);
	       break;
	  }
     }
     if (i == NULL)
	 arla_warnx (ADEBWARN, "nnpfs_message_wakeup: no message to wakeup!");
     return 0;
}

/*
 * The middle and last part of the nnpfs_message_rpc.
 */

int
nnpfs_message_sleep (u_int seqnum)
{
    struct nnpfs_message_wakeup h;

    h.header.sequence_num = seqnum;
    
    listaddtail (sleepers, &h);
    LWP_WaitProcess ((char *)&h);
    return h.error;
}

/*
 * Wake up a sleeping kernel-thread that sleeps on `seqnum'
 * and pass on `error' as an error the thread.
 */

int
nnpfs_send_message_wakeup (int fd, u_int seqnum, int error)
{
     struct nnpfs_message_wakeup msg;
     
     msg.header.opcode = NNPFS_MSG_WAKEUP;
     msg.sleepers_sequence_num = seqnum;
     msg.error = error;
     arla_warnx (ADEBMSG, "sending wakeup: seq = %u, error = %d",
		 seqnum, error);
     return nnpfs_message_send (fd, (struct nnpfs_message_header *)&msg, 
			      sizeof(msg));
}


/*
 * Wake-up a kernel-thread with `seqnum', and pass on `error'
 * ad return value. Add also a data blob for gerneric use.
 */

int
nnpfs_send_message_wakeup_data (int fd, u_int seqnum, int error,
			      void *data, int size)
{
     struct nnpfs_message_wakeup_data msg;
     
     msg.header.opcode = NNPFS_MSG_WAKEUP_DATA;
     msg.sleepers_sequence_num = seqnum;
     msg.error = error;
     arla_warnx (ADEBMSG,
		 "sending wakeup: seq = %u, error = %d", seqnum, error);

     if (sizeof(msg) >= size && size != 0) {
	 memcpy(msg.msg, data, size);
     }

     msg.len = size;

     return nnpfs_message_send (fd, (struct nnpfs_message_header *)&msg, 
			      sizeof(msg));
}

/*
 *
 */

struct write_buf {
    unsigned char buf[MAX_XMSG_SIZE];
    size_t len;
};

/*
 * Return 1 it buf is full, 0 if it's not.
 */

static int
add_new_msg (int fd, 
	     struct nnpfs_message_header *h, size_t size,
	     struct write_buf *buf)
{
    /* align on 8 byte boundery */

    if (size > sizeof (buf->buf) - buf->len)
	return 1;

    h->sequence_num 	= seqnums[fd]++;
    h->size		= (size + 8) & ~ 7;

    assert (h->opcode >= 0 && h->opcode < NNPFS_MSG_COUNT);
    ++nnpfs_stats[h->opcode].sent;

    arla_warnx (ADEBMSG, "Multi-send: opcode = %u (%s), size = %u",
		h->opcode, rcvfuncs_name[h->opcode], h->size);
    
    memcpy (buf->buf + buf->len, h, size);
    memset (buf->buf + buf->len + size, 0, h->size - size);
    buf->len += h->size;
    return 0;
}

/*
 * Blast of a `buf' to `fd'.
 */

static int
send_msg (int fd, struct write_buf *buf)
{
    int ret;

    if (buf->len == 0)
	return 0;

    ret = kern_write (fd, buf->buf, buf->len);
    if (ret != buf->len) {
	arla_warn (ADEBMSG, errno,
		   "send_msg: write");
	buf->len = 0;
	return errno;
    }
    buf->len = 0;
    return 0;
}

/*
 *
 */

int
nnpfs_send_message_vmultiple (int fd,
			    va_list args)
{
    struct nnpfs_message_header *h;
    struct write_buf *buf;
    size_t size;
    int ret;

    buf = malloc (sizeof (*buf));
    if (buf == NULL)
	return ENOMEM;

    h = va_arg (args, struct nnpfs_message_header *);
    size = va_arg (args, size_t);
    buf->len = 0;
    while (h != NULL) {
	if (add_new_msg (fd, h, size, buf)) {
	    ret = send_msg (fd, buf);
	    if (ret) {
		free (buf);
		return ret;
	    }
	    if (add_new_msg (fd, h, size, buf))
		arla_warnx (ADEBERROR, 
			    "nnpfs_send_message_vmultiple: "
			    "add_new_msg failed");
	}
	    
	h = va_arg (args, struct nnpfs_message_header *);
	size = va_arg (args, size_t);
    }
    ret = send_msg (fd, buf);
    free (buf);
    return ret;
}

/*
 * Same as above but different.
 */

int
nnpfs_send_message_multiple (int fd,
			   ...)
{
    va_list args;
    int ret;

    va_start (args, fd);
    ret = nnpfs_send_message_vmultiple (fd, args);
    va_end (args);
    return ret;
}
			   
/*
 * Almost same as above but different.
 */

int
nnpfs_send_message_multiple_list (int fd,
				struct nnpfs_message_header *h,
				size_t size,
				u_int num)
{
    struct write_buf *buf;
    int ret = 0;

    buf = malloc (sizeof (*buf));
    if (buf == NULL)
	return ENOMEM;
    buf->len = 0;
    while (num && ret == 0) {
	if (add_new_msg (fd, h, size, buf)) {
	    ret = send_msg (fd, buf);
	    if (add_new_msg (fd, h, size, buf))
		arla_warnx (ADEBERROR, 
			    "nnpfs_send_message_multiple_list: "
			    "add_new_msg failed");
	}
	h = (struct nnpfs_message_header *) (((unsigned char *)h) + size);
	num--;
    }
    if (ret) {
	free (buf);
	return ret;
    }
    ret = send_msg (fd, buf);
    free (buf);
    return ret;
}

/*
 * Send multiple message to the kernel (for performace/simple resons)
 */

int
nnpfs_send_message_wakeup_vmultiple (int fd,
				   u_int seqnum,
				   int error,
				   va_list args)
{
    struct nnpfs_message_wakeup msg;
    int ret;

    ret = nnpfs_send_message_vmultiple (fd, args);
    if (ret)
	arla_warnx (ADEBERROR, "nnpfs_send_message_wakeup_vmultiple: "
		    "failed sending messages with error %d", ret);

    msg.header.opcode = NNPFS_MSG_WAKEUP;
    msg.header.size  = sizeof(msg);
    msg.header.sequence_num = seqnums[fd]++;
    msg.sleepers_sequence_num = seqnum;
    msg.error = error;

    ++nnpfs_stats[NNPFS_MSG_WAKEUP].sent;

    arla_warnx (ADEBMSG, "multi-sending wakeup: seq = %u, error = %d",
		seqnum, error);

    ret = kern_write (fd, &msg, sizeof(msg));
    if (ret != sizeof(msg)) {
	arla_warn (ADEBMSG, errno,
		   "nnpfs_send_message_wakeup_vmultiple: writev");
	return -1;
    }
    return 0;
}

/*
 * Same as above but different.
 */

int
nnpfs_send_message_wakeup_multiple (int fd,
				  u_int seqnum,
				  int error,
				  ...)
{
    va_list args;
    int ret;

    va_start (args, error);
    ret = nnpfs_send_message_wakeup_vmultiple (fd, seqnum, error, args);
    va_end (args);
    return ret;
}
