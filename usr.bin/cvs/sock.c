/*	$OpenBSD: sock.c,v 1.1.1.1 2004/07/13 22:02:40 jfb Exp $	*/
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <poll.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "log.h"
#include "sock.h"
#include "cvsd.h"
#include "event.h"


volatile sig_atomic_t  cvs_sock_doloop;


char     *cvsd_sock_path = CVSD_SOCK_PATH;



/* daemon API */
static int cvsd_sock = -1;
static struct sockaddr_un cvsd_sun;

/* for client API */
static int cvs_sock = -1;
static struct sockaddr_un cvs_sun;


/*
 * cvsd_sock_open()
 *
 * Open the daemon's local socket.
 */

int
cvsd_sock_open(void)
{
	cvsd_sun.sun_family = AF_LOCAL;
	strlcpy(cvsd_sun.sun_path, cvsd_sock_path, sizeof(cvsd_sun.sun_path));

	cvsd_sock = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (cvsd_sock == -1) {
		cvs_log(LP_ERRNO, "failed to open socket"); 
		return (-1);
	}

	if (bind(cvsd_sock, (struct sockaddr *)&cvsd_sun,
	    SUN_LEN(&cvsd_sun)) == -1) {
		cvs_log(LP_ERRNO, "failed to bind local socket to `%s'",
		    cvsd_sock_path);
		(void)close(cvsd_sock);
		return (-1);
	}

	listen(cvsd_sock, 10);

	if (chmod(cvsd_sock_path, CVSD_SOCK_PERMS) == -1) {
		cvs_log(LP_ERRNO, "failed to change mode of `%s'",
		    cvsd_sock_path);
		(void)close(cvsd_sock);
		(void)unlink(cvsd_sock_path);
		return (-1);
	}

	cvs_log(LP_DEBUG, "opened local socket `%s'", cvsd_sock_path);

	return (0);
}


/*
 * cvsd_sock_close()
 *
 * Close the local socket.
 */

void
cvsd_sock_close(void)
{
	cvs_log(LP_DEBUG, "closing local socket `%s'", CVSD_SOCK_PATH);
	if (close(cvsd_sock) == -1) {
		cvs_log(LP_ERRNO, "failed to close local socket");
	}
	if (unlink(cvsd_sock_path) == -1)
		cvs_log(LP_ERRNO, "failed to unlink local socket `%s'",
		    CVSD_SOCK_PATH);
}


/*
 * cvsd_sock_loop()
 *
 */

void
cvsd_sock_loop(void)
{
	int nfds, sock;
	socklen_t slen;
	struct sockaddr_un sun;
	struct pollfd pfd[1];

	cvs_sock_doloop = 1;

	while (cvs_sock_doloop) {
		pfd[0].fd = cvsd_sock;
		pfd[0].events = POLLIN;

		nfds = poll(pfd, 1, INFTIM);
		if (nfds == -1) {
			if (errno == EINTR)
				continue;
			cvs_log(LP_ERR, "failed to poll local socket");
		}

		if ((nfds == 0) || !(pfd[0].revents & POLLIN))
			continue;

		sock = accept(pfd[0].fd, (struct sockaddr *)&sun, &slen);
		if (sock == -1) {
			cvs_log(LP_ERRNO, "failed to accept connection");
		}
		cvs_log(LP_DEBUG, "accepted connection");

		cvsd_sock_hdl(sock);
	}


}


/*
 * cvsd_sock_hdl()
 *
 * Handle the events for a single connection.
 */

int
cvsd_sock_hdl(int fd)
{
	uid_t uid;
	gid_t gid;
	struct cvs_event ev;

	/* don't trust what the other end put in */
	if (getpeereid(fd, &uid, &gid) == -1) {
		cvs_log(LP_ERR, "failed to get peer credentials");
		(void)close(fd);
		return (-1);
	}

	if (read(fd, &ev, sizeof(ev)) == -1) {
		cvs_log(LP_ERR, "failed to read cvs event");
		return (-1);
	}

	return (0);
}


/*
 * cvs_sock_connect()
 *
 * Open a connection to the CVS server's local socket.
 */

int
cvs_sock_connect(const char *cvsroot)
{
	cvs_sun.sun_family = AF_LOCAL;
	snprintf(cvs_sun.sun_path, sizeof(cvs_sun.sun_path), "%s/%s",
	    cvsroot, CVSD_SOCK_PATH);

	cvs_log(LP_INFO, "connecting to CVS server socket `%s'",
	    cvs_sun.sun_path);

	cvs_sock = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (cvs_sock == -1) {
		cvs_log(LP_ERRNO, "failed to open local socket");
		return (-1);
	}

	if (connect(cvs_sock, (struct sockaddr *)&cvs_sun,
	    SUN_LEN(&cvs_sun)) == -1) {
		cvs_log(LP_ERRNO, "failed to connect to server socket `%s'",
		    cvs_sun.sun_path);
		(void)close(cvs_sock);
		return (-1);
	}

	return (0);
}


/*
 * cvs_sock_disconnect()
 *
 * Disconnect from the open socket to the CVS server.
 */

void
cvs_sock_disconnect(void)
{
	if (close(cvs_sock) == -1)
		cvs_log(LP_ERRNO, "failed to close local socket");
}
