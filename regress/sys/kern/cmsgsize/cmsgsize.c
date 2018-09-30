/*	$Id: cmsgsize.c,v 1.1 2018/09/30 08:26:40 vgross Exp $ */
/*
 * Copyright (c) 2017 Alexander Markert <alexander.markert@siemens.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define CFG_SOURCE_ADDRESS		"192.168.88.135"
#define CFG_DESTINATION_ADDRESS		"192.168.57.44"
#define CFG_PORT			5000
#define CFG_SO_MAX_SEND_BUFFER		1024

char payload[CFG_SO_MAX_SEND_BUFFER] = {0};

int test_cmsgsize(int, struct in_addr *, unsigned int, unsigned int, int);

int main(int argc, char *argv[])
{
	int so, bytes;
	struct in_addr src_ia;

	if (argc < 2)
		errx(-1, "not enough parameters: %s <source_address>", argv[0]);

	if (inet_pton(AF_INET, argv[1], &src_ia) != 1)
		err(-1, "unable to parse source address");

	/* !blocking, cmsg + payload > sndbufsize => EMSGSIZE */
	so = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (so < 0)
		err(-1, "opening socket failed");
	if ((bytes = test_cmsgsize(so, &src_ia, CFG_SO_MAX_SEND_BUFFER, CFG_SO_MAX_SEND_BUFFER, 0)) < 0) {
		if (errno != EMSGSIZE)
			err(-1, "incorrect errno");
	} else {
		err(-1, "%d bytes sent\n", bytes);
	}
	close(so);

	/* blocking, cmsg + payload > sndbufsize => EMSGSIZE */
	so = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (so < 0)
		err(-1, "opening socket failed");
	if ((bytes = test_cmsgsize(so, &src_ia, CFG_SO_MAX_SEND_BUFFER, CFG_SO_MAX_SEND_BUFFER, 1)) < 0) {
		if (errno != EMSGSIZE)
			err(-1, "incorrect errno");
	} else {
		err(-1, "%d bytes sent\n", bytes);
	}
	close(so);

	/* !blocking, cmsg + payload < sndbufsize => OK */
	so = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (so < 0)
		err(-1, "opening socket failed");
	if ((bytes = test_cmsgsize(so, &src_ia, CFG_SO_MAX_SEND_BUFFER, CFG_SO_MAX_SEND_BUFFER/2, 0)) < 0) {
		err(-1, "got errno");
	}
	close(so);

	/* blocking, cmsg + payload < sndbufsize => OK */
	so = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (so < 0)
		err(-1, "opening socket failed");
	if ((bytes = test_cmsgsize(so, &src_ia, CFG_SO_MAX_SEND_BUFFER, CFG_SO_MAX_SEND_BUFFER/2, 1)) < 0) {
		err(-1, "got errno");
	}
	close(so);

	return 0;
}

int test_cmsgsize(int so, struct in_addr *ia, unsigned int sndbuf_size, unsigned int payload_size, int blocking)
{
	char cmsgbuf[CMSG_SPACE(sizeof(struct in_addr))];;
	struct sockaddr_in to;
	unsigned int size = CFG_SO_MAX_SEND_BUFFER;
	struct in_addr *source_address;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	struct iovec iov;

	if (setsockopt(so, SOL_SOCKET, SO_SNDBUF, &sndbuf_size, sizeof(sndbuf_size)) < 0)
		err(-1, "adjusting socket send buffer failed");

	if (!blocking) {
		unsigned long on = 1;

		if (ioctl(so, FIONBIO, &on) < 0)
			err(-1, "enabling non-blocking IO failed");
	}

	/* setup remote address */
	memset(&to, 0, sizeof(to));
	to.sin_family = AF_INET;
	to.sin_addr.s_addr = inet_addr(CFG_DESTINATION_ADDRESS);
	to.sin_port = htons(CFG_PORT);

	/* setup buffer to be sent */
	msg.msg_name = &to;
	msg.msg_namelen = sizeof(to);
	iov.iov_base = payload;
	iov.iov_len = payload_size;
	msg.msg_iovlen = 1;
	msg.msg_iov = &iov;

	/* setup configuration for source address */
	memset(cmsgbuf, 0, sizeof(cmsgbuf));
	msg.msg_control = cmsgbuf;
	msg.msg_controllen = sizeof(cmsgbuf);
	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = IPPROTO_IP;
	cmsg->cmsg_type = IP_SENDSRCADDR;
	cmsg->cmsg_len = CMSG_LEN(sizeof(struct in_addr));
	source_address = (struct in_addr *)(CMSG_DATA(cmsg));
	memcpy(source_address, ia, sizeof(struct in_addr));
/*	source_address->s_addr = inet_addr(CFG_SOURCE_ADDRESS); */

	return sendmsg(so, &msg, 0);
}
