/*	$NetBSD: unfdpass.c,v 1.3 1998/06/24 23:51:30 thorpej Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

/*
 * Test passing of file descriptors and credentials over Unix domain sockets.
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define	SOCK_NAME	"test-sock"

int	main __P((int, char *[]));
void	child __P((void));
void	catch_sigchld __P((int));

struct fdcmessage {
	struct cmsghdr cm;
	int files[2];
};

struct crcmessage {
	struct cmsghdr cm;
	char creds[SOCKCREDSIZE(NGROUPS)];
};

/* ARGSUSED */
int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct msghdr msg;
	int listensock, sock, fd, i, status;
	char fname[16], buf[64];
	struct cmsghdr *cmp;
	struct {
		struct fdcmessage fdcm;
		struct crcmessage crcm;
	} message;
	int *files = NULL;
	struct sockcred *sc = NULL;
	struct sockaddr_un sun, csun;
	int csunlen;
	fd_set oob;
	pid_t pid;

	/*
	 * Create the test files.
	 */
	for (i = 0; i < 2; i++) {
		(void) sprintf(fname, "file%d", i + 1);
		if ((fd = open(fname, O_WRONLY|O_CREAT|O_TRUNC, 0666)) == -1)
			err(1, "open %s", fname);
		(void) sprintf(buf, "This is file %d.\n", i + 1);
		if (write(fd, buf, strlen(buf)) != strlen(buf))
			err(1, "write %s", fname);
		(void) close(fd);
	}

	/*
	 * Create the listen socket.
	 */
	if ((listensock = socket(PF_LOCAL, SOCK_STREAM, 0)) == -1)
		err(1, "socket");

	(void) unlink(SOCK_NAME);
	(void) memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_LOCAL;
	(void) strcpy(sun.sun_path, SOCK_NAME);
	sun.sun_len = SUN_LEN(&sun);

	i = 1;
	if (setsockopt(listensock, 0, LOCAL_CREDS, &i, sizeof(i)) == -1)
		err(1, "setsockopt");

	if (bind(listensock, (struct sockaddr *)&sun, sizeof(sun)) == -1)
		err(1, "bind");

	if (listen(listensock, 1) == -1)
		err(1, "listen");

	/*
	 * Create the sender.
	 */
	(void) signal(SIGCHLD, catch_sigchld);
	pid = fork();
	switch (pid) {
	case -1:
		err(1, "fork");
		/* NOTREACHED */

	case 0:
		child();
		/* NOTREACHED */
	}

	/*
	 * Wait for the sender to connect.
	 */
	if ((sock = accept(listensock, (struct sockaddr *)&csun,
	    &csunlen)) == -1)
		err(1, "accept");

	/*
	 * Give sender a chance to run.  We will get going again
	 * once the SIGCHLD arrives.
	 */
	(void) sleep(10);

	/*
	 * Grab the descriptors and credentials passed to us.
	 */
	(void) memset(&msg, 0, sizeof(msg));
	msg.msg_control = (caddr_t) &message;
	msg.msg_controllen = sizeof(message);

	if (recvmsg(sock, &msg, 0) < 0)
		err(1, "recvmsg");

	(void) close(sock);

	if (msg.msg_controllen == 0)
		errx(1, "no control messages received");

	if (msg.msg_flags & MSG_CTRUNC)
		errx(1, "lost control message data");

	cmp = CMSG_FIRSTHDR(&msg);
	for (cmp = CMSG_FIRSTHDR(&msg); cmp != NULL;
	    cmp = CMSG_NXTHDR(&msg, cmp)) {
		if (cmp->cmsg_level != SOL_SOCKET)
			errx(1, "bad control message level %d",
			    cmp->cmsg_level);

		switch (cmp->cmsg_type) {
		case SCM_RIGHTS:
			if (cmp->cmsg_len != sizeof(message.fdcm))
				errx(1, "bad fd control message length");

			files = (int *)CMSG_DATA(cmp);
			break;

		case SCM_CREDS:
			if (cmp->cmsg_len < sizeof(struct sockcred))
				errx(1, "bad cred control message length");

			sc = (struct sockcred *)CMSG_DATA(cmp);
			break;

		default:
			errx(1, "unexpected control message");
			/* NOTREACHED */
		}
	}

	/*
	 * Read the files and print their contents.
	 */
	if (files == NULL)
		warnx("didn't get fd control message");
	else {
		for (i = 0; i < 2; i++) {
			(void) memset(buf, 0, sizeof(buf));
			if (read(files[i], buf, sizeof(buf)) <= 0)
				err(1, "read file %d", i + 1);
			printf("%s", buf);
		}
	}

	/*
	 * Double-check credentials.
	 */
	if (sc == NULL)
		warnx("didn't get cred control message");
	else {
		if (sc->sc_uid == getuid() &&
		    sc->sc_euid == geteuid() &&
		    sc->sc_gid == getgid() &&
		    sc->sc_egid == getegid())
			printf("Credentials match.\n");
		else
			printf("Credentials do NOT match.\n");
	}

	/*
	 * All done!
	 */
	exit(0);
}

void
catch_sigchld(sig)
	int sig;
{
	int status;

	(void) wait(&status);
}

void
child()
{
	struct msghdr msg;
	char fname[16], buf[64];
	struct cmsghdr *cmp;
	struct fdcmessage fdcm;
	int i, fd, sock;
	struct sockaddr_un sun;

	/*
	 * Create socket and connect to the receiver.
	 */
	if ((sock = socket(PF_LOCAL, SOCK_STREAM, 0)) == -1)
		errx(1, "child socket");

	(void) memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_LOCAL;
	(void) strcpy(sun.sun_path, SOCK_NAME);
	sun.sun_len = SUN_LEN(&sun);

	if (connect(sock, (struct sockaddr *)&sun, sizeof(sun)) == -1)
		err(1, "child connect");

	/*
	 * Open the files again, and pass them to the child over the socket.
	 */
	for (i = 0; i < 2; i++) {
		(void) sprintf(fname, "file%d", i + 1);
		if ((fd = open(fname, O_RDONLY, 0666)) == -1)
			err(1, "child open %s", fname);
		fdcm.files[i] = fd;
	}

	(void) memset(&msg, 0, sizeof(msg));
	msg.msg_control = (caddr_t) &fdcm;
	msg.msg_controllen = sizeof(fdcm);

	cmp = CMSG_FIRSTHDR(&msg);
	cmp->cmsg_len = sizeof(fdcm);
	cmp->cmsg_level = SOL_SOCKET;
	cmp->cmsg_type = SCM_RIGHTS;

	if (sendmsg(sock, &msg, 0))
		err(1, "child sendmsg");

	/*
	 * All done!
	 */
	exit(0);
}
