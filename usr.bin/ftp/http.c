/*	$OpenBSD: http.c,v 1.1 1996/09/03 18:00:06 deraadt Exp $	*/

/*
 * Copyright (c) 1996 Theo de Raadt
 * Copyright (c) 1996 Brian Mitchell
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <ctype.h>
#include <err.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * This function lets you retrieve files from the WWW. It will accept
 * any http:// url. It conects and retrieves the file, saving it in
 * the current directory.
 *
 * Limitations:
 * http://host does not work, nor http://host/ - you have to specifically
 * specify the filename you want to transfer.
 */
int
http_fetch(url)
	char *url;
{
	char *hostname, *filename, basename[MAXPATHLEN] = "/";
	char buf[8192], *bufp, *req = NULL;
	struct sockaddr_in addr;
	struct hostent *he;
	FILE *write_to;
	char *s, *p;
	int bytes, c, d;
	int sock = -1, file = -1, ret = 1;

	s = url + strlen("http://");
	p = strchr(s, '/');
	if (p)
		*p = '\0';
	else
		p = s + strlen(s);
	if (p - s > MAXHOSTNAMELEN-1) {
		warn("hostname too long");
		return (1);
	}
	hostname = s;
	filename = p + 1;

	p = strrchr(filename, '/');
	if (p == NULL) {
		if (strlen(s) > MAXPATHLEN-1) {
			warn("filename too long");
			return (1);
		}
		strcpy(basename, filename);
	} else {
		p++;
		if (strlen(p) > MAXPATHLEN-1) {
			warn("filename too long");
			return (1);
		}
		strcpy(basename, p);
	}
	if (strlen(basename) == 0)
		strcpy(basename, "index.html");

	req = (char *)malloc(sizeof("GET ") + strlen(filename) +3);
	if (!req) {
		warn("no memory");
		return (1);
	}
	sprintf(req, "GET /%s\n", filename);

	he = gethostbyname(hostname);
	if (!he) {
		perror("gethostbyname");
		goto die;
	}

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		perror("socket");
		goto die;
	}

	memset(&addr, 0, sizeof addr);
	addr.sin_family = he->h_addrtype;
	addr.sin_port = htons(80);
	memcpy(&addr.sin_addr, he->h_addr, he->h_length);

	if (connect(sock, (struct sockaddr *)&addr, sizeof addr) == -1) {
		perror("connect");
		goto die;
	}
	printf("Connected to %s.\n", hostname);

	printf("Retrieving using: %s", req);
	for (bufp = req, c = strlen(bufp); c > 0; c -= d, bufp += d) {
		if ((d = write(sock, bufp, c)) <= 0)
			break;
	}
	if (d == -1) {
		perror("sending command");
		goto die;
	}

	file = open(basename, O_CREAT|O_WRONLY|O_TRUNC, 0666);
	if (!file) {
		perror("fopen");
		goto die;
	}

	bytes = 0;
	while ((c = read(sock, buf, sizeof (buf))) > 0) {
		bytes += c;
		for (bufp = buf; c > 0; c -= d, bufp += d)
			if ((d = write(file, bufp, c)) <= 0)
				break;
	}
	if (d == -1) {
		perror("failed to receive correctly");
		goto die;
	}
	printf("Success, closing connection.\n"); 
	ret = 0;
die:
	if (sock != -1)
		close(sock);
	if (file != -1)
		close(file);
	if (req)
		free(req);
	return (ret);
}   
