/*	$OpenBSD: sock.c,v 1.9 2005/01/27 20:45:18 jfb Exp $	*/
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


volatile sig_atomic_t  cvs_sock_doloop;


char     *cvsd_sock_path = CVSD_SOCK_PATH;

/* daemon API */
#ifdef CVSD
int cvsd_sock = -1;
static struct sockaddr_un cvsd_sun;
#endif

/* for client API */
#ifdef CVS
static int cvs_sock = -1;
static struct sockaddr_un cvs_sun;
#endif


#ifdef CVSD
/*
 * cvsd_sock_open()
 *
 * Open the daemon's local socket.  If the server socket is already opened,
 * we close it before reopening it.
 * Returns 0 on success, -1 on failure.
 */
int
cvsd_sock_open(void)
{
	if (cvsd_sock >= 0)
		cvsd_sock_close();

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

	(void)listen(cvsd_sock, 10);

	if (chown(cvsd_sock_path, getuid(), cvsd_gid) == -1) {
		cvs_log(LP_ERRNO, "failed to change owner of `%s'",
		    cvsd_sock_path);
		(void)close(cvsd_sock);
		(void)unlink(cvsd_sock_path);
		return (-1);
	}

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
	if (seteuid(0) == -1)
		cvs_log(LP_ERRNO, "failed to regain privileges");
	else if (unlink(cvsd_sock_path) == -1)
		cvs_log(LP_ERRNO, "failed to unlink local socket `%s'",
		    cvsd_sock_path);
}


/*
 * cvsd_sock_accept()
 *
 * Handler for connections made on the server's local domain socket.
 * It accepts connections and looks for a child process that is currently
 * idle to which it can dispatch the connection's descriptor.  If there are
 * no available child processes, a new one will be created unless the number
 * of children has attained the maximum.
 */
int
cvsd_sock_accept(int fd)
{
	int cfd;
	socklen_t slen;
	struct sockaddr_un sun;

	slen = sizeof(sun);
	cfd = accept(fd, (struct sockaddr *)&sun, &slen);
	if (cfd == -1) {
		cvs_log(LP_ERRNO, "failed to accept client connection");
		return (-1);
	}

	return (cfd);
}
#endif

#ifdef CVS
/*
 * cvs_sock_connect()
 *
 * Open a connection to the CVS server's local socket.
 */
int
cvs_sock_connect(const char *path)
{
	cvs_sun.sun_family = AF_LOCAL;
	strlcpy(cvs_sun.sun_path, path, sizeof(cvs_sun.sun_path));

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
#endif
