/*	$OpenBSD: kernel.c,v 1.1.1.1 1998/09/14 21:52:57 art Exp $	*/
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
RCSID("$KTH: kernel.c,v 1.12 1998/07/13 19:19:02 assar Exp $");

/*
 * The fd we use to talk with the kernel on.
 */

int kernel_fd;

/* count of the number of messages in a read */

static unsigned recv_count[20];

/* for more than above... */

static unsigned recv_count_overflow;

static int
process_message (int fd)
{
     static char data[MAX_XMSG_SIZE];
     int res;
     struct xfs_message_header *header;
     char *p;
     int cnt;

     res = read (fd, data, sizeof (data));

     if (res < 0) {
	  arla_warn (ADEBWARN, errno, "read");
	  /* XXX process the errno? Are we supposed to exit on every error?*/
	  return -1;
     }
     cnt = 0;
     for (p = data; res > 0; p += header->size, res -= header->size) {
	 header = (struct xfs_message_header *)p;
	 xfs_message_receive (fd, header, header->size);
	 ++cnt;
     }
     if (cnt < sizeof(recv_count)/sizeof(recv_count[0]))
	 ++recv_count[cnt];
     else
	 ++recv_count_overflow;
     
     return 0;
}

void
kernel_interface (char *device)
{
     int fd;

     fd = open (device, O_RDWR);
     if (fd < 0)
	 arla_err (1, ADEBERROR, errno, "open %s", device);
     kernel_fd = fd;

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
	      
	      if (process_message (fd))
		  arla_errx (1, ADEBKERNEL, "error processing message");
	  }
     }
}
