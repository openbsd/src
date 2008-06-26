/*	$OpenBSD: urio.h,v 1.4 2008/06/26 05:42:19 ray Exp $	*/
/*	$NetBSD: urio.h,v 1.2 2000/04/27 15:26:49 augustss Exp $	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

struct urio_command
{
	unsigned short	length;
	int		request;
	int		requesttype;
	int		value;
	int		index;
	void		*buffer;
	int		timeout;
};

#define URIO_SEND_COMMAND	_IOWR('U', 200, struct urio_command)
#define URIO_RECV_COMMAND	_IOWR('U', 201, struct urio_command)

#define URIO_DIR_OUT		0x0
#define URIO_DIR_IN		0x1

#ifndef __KERNEL__
#define RIO_DIR_OUT URIO_DIR_OUT
#define RIO_DIR_IN URIO_DIR_IN
#define RIO_SEND_COMMAND URIO_SEND_COMMAND
#define RIO_RECV_COMMAND URIO_RECV_COMMAND
#define RioCommand urio_command
#endif
