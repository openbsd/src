/*	$OpenBSD: msg.c,v 1.1.1.1 2004/07/13 22:02:40 jfb Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include <sys/param.h>
#include <sys/uio.h>

#include <pwd.h>
#include <grp.h>
#include <poll.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "log.h"
#include "cvsd.h"





/*
 * cvsd_sendmsg()
 *
 * Send a message of type <type> along with the first <len> bytes of data
 * from <data> (which can be up to CVSD_MSG_MAXLEN bytes) on the descriptor
 * <fd>.
 * Returns 0 on success, or -1 on failure.
 */

int
cvsd_sendmsg(int fd, u_int type, const void *data, size_t len)
{
	struct iovec iov[2];
	struct cvsd_msg msg;

	if (len > CVSD_MSG_MAXLEN) {
		cvs_log(LP_ERR, "message too large");
		return (-1);
	}

	msg.cm_type = type;
	msg.cm_len = len;

	iov[0].iov_base = &msg;
	iov[0].iov_len = sizeof(msg);
	iov[1].iov_base = (void *)data;
	iov[1].iov_len = len;

	if (writev(fd, iov, 2) == -1) {
		cvs_log(LP_ERRNO, "failed to send message");
		return (-1);
	}

	return (0);
}


/*
 * cvsd_recvmsg()
 *
 * Read a message from the file descriptor <fd> and store the message data
 * in the <dst> buffer.  The <len> parameter should contain the maximum
 * length of data that can be stored in <dst>, and will contain the actual
 * size of data stored on return.  The message type is stored in <type>.
 * Returns 0 on success, or -1 on failure.
 */

int
cvsd_recvmsg(int fd, u_int *type, void *dst, size_t *len)
{
	ssize_t ret;
	struct cvsd_msg msg;

	if (read(fd, &msg, sizeof(msg)) == -1) {
		cvs_log(LP_ERRNO, "failed to read message header");
		return (-1);
	}

	if (*len < msg.cm_len) {
		cvs_log(LP_ERR, "buffer size too small for message data");
		return (-1);
	}

	ret = read(fd, dst, msg.cm_len);
	if (ret == -1) {
		cvs_log(LP_ERRNO, "failed to read message");
		return (-1);
	}
	else if (ret == 0) {
	}

	*type = msg.cm_type;

	return (0);
}
