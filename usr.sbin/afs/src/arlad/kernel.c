/*	$OpenBSD: kernel.c,v 1.2 1999/04/30 01:59:08 art Exp $	*/
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

#include "arla_local.h"
RCSID("$KTH: kernel.c,v 1.17 1998/12/06 20:48:40 lha Exp $");

/*
 * The fd we use to talk with the kernel on.
 */

int kernel_fd;

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
     struct xfs_message_header *header;
     char *p;
     int cnt;

     cnt = 0;
     for (p = msg;
	  msg_length > 0;
	  p += header->size, msg_length -= header->size) {
	 header = (struct xfs_message_header *)p;
	 xfs_message_receive (kernel_fd, header, header->size);
	 ++cnt;
     }
     if (cnt < sizeof(recv_count)/sizeof(recv_count[0]))
	 ++recv_count[cnt];
     else
	 ++recv_count_overflow;
     
     return 0;
}

/*
 * The work threads.
 */

struct worker {
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
    }
}

#define WORKER_STACKSIZE (16*1024)

void
kernel_interface (struct kernel_args *args)
{
     int fd;
     int i;

     fd = open (args->device, O_RDWR);
     if (fd < 0)
	 arla_err (1, ADEBERROR, errno, "open %s", args->device);
     kernel_fd = fd;

     workers = malloc (sizeof(*workers) * args->num_workers);
     if (workers == NULL)
	 arla_err (1, ADEBERROR, errno, "malloc %u failed",
		   sizeof(*workers) * args->num_workers);

     workers_high = args->num_workers;
     workers_used = 0;
 
    for (i = 0; i < args->num_workers; ++i) {
	 workers[i].busyp  = 0;
	 workers[i].number = i;
	 if (LWP_CreateProcess (sub_thread, WORKER_STACKSIZE, 1,
				(char *)&workers[i],
				"worker", &workers[i].pid))
	     arla_errx (1, ADEBERROR, "CreateProcess of worker failed");
     }

     arla_warnx(ADEBKERNEL, "Arla: selecting on fd: %d", fd);

     for (;;) {
	  fd_set readset;
	  int ret;
	  
	  FD_ZERO(&readset);
	  FD_SET(fd, &readset);

	  ret = IOMGR_Select (fd + 1, &readset, NULL, NULL, NULL); 

	  if (ret < 0)
	      arla_warn (ADEBKERNEL, errno, "select");
	  else if (ret == 0)
	      arla_warnx (ADEBKERNEL,
			  "Arla: select returned with 0. strange.");
	  else if (FD_ISSET(fd, &readset)) {
	      for (i = 0; i < args->num_workers; ++i) {
		  if (workers[i].busyp == 0) {
		      ret = read (fd, workers[i].data,
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
	      if (i == args->num_workers)
		  arla_warnx (ADEBWARN, "kernel: all workers busy");
	  }
     }
}
