/*	$OpenBSD: msg.c,v 1.5 2004/12/06 21:03:12 deraadt Exp $	*/
/*
 * Copyright (c) 2002 Matthieu Herrb
 * Copyright (c) 2001 Niels Provos <provos@citi.umich.edu>
 * Copyright (c) 2004 Jean-Francois Brousseau
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * This code was adapted from the tcpdump source
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "log.h"
#include "cvsd.h"

/*
 * cvsd_sendfd()
 *
 * Pass a file descriptor <fd> to the other endpoint of the socket <sock>.
 */

int
cvsd_sendfd(int sock, int fd)
{
	struct msghdr msg;
	char tmp[CMSG_SPACE(sizeof(int))];
	struct cmsghdr *cmsg;
	struct iovec vec;
	int result = 0;
	ssize_t n;

	memset(&msg, 0, sizeof(msg));

	if (fd >= 0) {
		msg.msg_control = (caddr_t)tmp;
		msg.msg_controllen = CMSG_LEN(sizeof(int));
		cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		*(int *)CMSG_DATA(cmsg) = fd;
	} else
		result = errno;

	vec.iov_base = &result;
	vec.iov_len = sizeof(int);
	msg.msg_iov = &vec;
	msg.msg_iovlen = 1;

	if ((n = sendmsg(sock, &msg, 0)) == -1) {
		cvs_log(LP_ERRNO, "failed to pass file descriptor");
		return (-1);
	}
	if (n != sizeof(int))
		cvs_log(LP_WARN, "unexpected return count from sendmsg()");
	return (0);
}

/*
 * cvsd_recvfd()
 *
 * Receive a file descriptor over the socket <sock>.  Returns the descriptor
 * on success, or -1 on failure.
 */

int
cvsd_recvfd(int sock)
{
	struct msghdr msg;
	char tmp[CMSG_SPACE(sizeof(int))];
	struct cmsghdr *cmsg;
	struct iovec vec;
	ssize_t n;
	int result;
	int fd;

	memset(&msg, 0, sizeof(msg));
	vec.iov_base = &result;
	vec.iov_len = sizeof(int);
	msg.msg_iov = &vec;
	msg.msg_iovlen = 1;
	msg.msg_control = tmp;
	msg.msg_controllen = sizeof(tmp);

	if ((n = recvmsg(sock, &msg, 0)) == -1)
		cvs_log(LP_ERRNO, "failed to receive descriptor");
	if (n != sizeof(int))
		cvs_log(LP_WARN, "recvmsg: expected received 1 got %ld",
		    (long)n);
	if (result == 0) {
		cmsg = CMSG_FIRSTHDR(&msg);
		if (cmsg->cmsg_type != SCM_RIGHTS)
			cvs_log(LP_WARN,
			    "unexpected message type in descriptor reception");
		fd = (*(int *)CMSG_DATA(cmsg));
		return (fd);
	} else {
		errno = result;
		return (-1);
	}
}



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
	int cnt;
	struct iovec iov[2];
	struct cvsd_msg msg;

	if (len > CVSD_MSG_MAXLEN) {
		cvs_log(LP_ERR, "message too large");
		return (-1);
	}

	memset(&msg, 0, sizeof(msg));

	cnt = 1;
	iov[0].iov_base = &msg;
	iov[0].iov_len = sizeof(msg);
	msg.cm_type = type;

	if (type != CVSD_MSG_PASSFD) {
		msg.cm_len = len;
		iov[1].iov_base = (void *)data;
		iov[1].iov_len = len;
		cnt = 2;
	} else
		msg.cm_len = sizeof(int);	/* dummy */

	if (writev(fd, iov, cnt) == -1) {
		cvs_log(LP_ERRNO, "failed to send message");
		return (-1);
	}

	if (type == CVSD_MSG_PASSFD) {
		/* pass the file descriptor for real */
		cvsd_sendfd(fd, *(int *)data);
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
 * Returns 1 if a message was read, 0 if the remote end closed the message
 * socket and no further messages can be read, or -1 on failure.
 */

int
cvsd_recvmsg(int fd, u_int *type, void *dst, size_t *len)
{
	int sfd;
	ssize_t ret;
	struct cvsd_msg msg;

	if ((ret = read(fd, &msg, sizeof(msg))) == -1) {
		cvs_log(LP_ERRNO, "failed to read message header");
		return (-1);
	} else if (ret == 0)
		return (0);

	if (*len < msg.cm_len) {
		cvs_log(LP_ERR, "buffer size too small for message data");
		return (-1);
	}

	if (msg.cm_type == CVSD_MSG_PASSFD) {
		sfd = cvsd_recvfd(fd);
		if (sfd == -1)
			return (-1);

		*(int *)dst = sfd;
		*len = sizeof(sfd);
	} else {
		ret = read(fd, dst, msg.cm_len);
		if (ret == -1) {
			cvs_log(LP_ERRNO, "failed to read message");
			return (-1);
		} else if (ret == 0) {
		}

		*len = (size_t)ret;
	}

	*type = msg.cm_type;

	return (1);
}
