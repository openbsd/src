/*	$OpenBSD: quip_client.c,v 1.6 2002/11/07 09:33:21 kjc Exp $	*/
/*	$KAME: quip_client.c,v 1.4 2001/08/16 07:43:15 itojun Exp $	*/
/*
 * Copyright (C) 1999-2000
 *	Sony Computer Science Laboratories, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY SONY CSL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL SONY CSL OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <err.h>

#include "quip_client.h"
#include "altqstat.h"

/*
 * quip (queue information protocol) is a http-like protocol
 * in order to retrieve information from the server.
 * a unix domain TCP socket "/var/run/altq_quip" is used for
 * client-server style communication.
 *
 * there are 2 quip message types: request and response.
 * request format: (only single-line request message is used at this moment)
 *	request-line
 *
 *      request-line = <method> <operation>[?<query>] <quip-version>
 *	<method> = GET (only GET is defined at this moment)
 *	<operation> = list | handle-to-name | qdisc | filter
 *	query format is operation dependent but most query takes
 *	<interface> or <class> or <filter>.
 *	<interface> = <if_name>
 *	<class>     = <if_name>:<class_path>/<class_name>
 *	<filter>    = <if_name>:<class_path>/<class_name>:<filter_name>
 *	"list" operation accepts "*" as a wildcard.
 *
 * response format:
 *	status-line
 * 	response-headers (0 or more)
 *	<blank line>
 *	body
 *
 *	status-line = <quip-version> <status-code> <reason phrase>
 *	response-header = Content-Length:<value>
 *
 *	"Content-Length" specifies the length of the message body.
 *
 * example:
 *	to retrieve a list of classes (handle and name) on interface "fxp0":
 *	a request message looks like,
 *		GET list?fxp0:* QUIP/1.0<cr>
 *	a response message looks like,
 *		QUIP/1.0 200 OK<cr>
 *		Content-Length:86<cr>
 *		<cr>
 *		0000000000	fxp0:/root<cr>
 *		0xc0d1be00	fxp0:/root/parent<cr>
 *		0xc0d1ba00	fxp0:/root/parent/child<cr>
 *
 *	other examples:
 *	list all interfaces, classes, and filters:
 *		GET list QUIP/1.0<cr>
 *	list all interfaces:
 *		GET list?* QUIP/1.0<cr>
 *	list all classes:
 *		GET list?*:* QUIP/1.0<cr>
 *	list all filters:
 *		GET list?*:*:* QUIP/1.0<cr>
 *	convert class handle to class name:
 *		GET handle-to-name?fxp0:0xc0d1be00 QUIP/1.0<cr>
 *	convert filter handle to filter name:
 *		GET handle-to-name?fxp0::0x1000000a QUIP/1.0<cr>
 */

#define	MAXLINESIZE	1024

enum nametype { INTERFACE, CLASS, FILTER, CONDITIONER };

static FILE *server = NULL;
int quip_echo = 0;

static char *extract_ifname(const char *);

int
quip_openserver(void)
{
	struct sockaddr_un addr;
	int fd;

	if ((fd = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0)
		err(1, "can't open socket");

	bzero(&addr, sizeof(addr));
	addr.sun_family = AF_LOCAL;
	strlcpy(addr.sun_path, QUIP_PATH,sizeof(addr.sun_path));

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "can't talk to altqd!\n"
			"probably, altqd is not running\n");
		return (-1);
	}

	if ((server = fdopen(fd, "r+")) == NULL) {
		warn("fdopen: can't open stream to the quip server");
		return (-1);
	}
	return (0);
}

int
quip_closeserver(void)
{
	if (server != NULL)
		return fclose(server);
	return (0);
}

void
quip_sendrequest(FILE *fp, const char *request)
{
	char buf[QUIPMSG_MAXSIZE], *cp;
	int n;

	if ((cp = strstr(request, "QUIP")) == NULL) {
		cp = strchr(request, '\n');
		n = cp - request;
		if (cp == NULL || n > REQ_MAXSIZE - 10)
			return;
		strncpy(buf, request, n);
		snprintf(buf + n, REQ_MAXSIZE - n, " QUIP/1.0");
		strlcat(buf, cp, REQ_MAXSIZE);
	}
	else
		strlcpy(buf, request, REQ_MAXSIZE);

	if (fputs(buf, fp) != 0)
		err(1, "fputs");
	if (fflush(fp) != 0)
		err(1, "fflush");
	if (quip_echo) {
		fputs("<< ", stdout);
		fputs(buf, stdout);
	}
}

/*
 * recv_response receives a response message from the server
 * and returns status_code.
 */
int
quip_recvresponse(FILE *fp, char *header, char *body, int *blen)
{
	char buf[MAXLINESIZE], version[MAXLINESIZE];
	int code, resid, len, buflen;
	int end_of_header = 0;

	if (blen != NULL)
		*blen = 0;
	code = 0;
	resid = 0;
	buflen = RES_MAXSIZE;
	while (fgets(buf, sizeof(buf), fp) != 0) {
		if (quip_echo) {
			fputs(">  ", stdout);
			fputs(buf, stdout);
		}

		if (!end_of_header) {
			/* process message header */
			if (header != NULL) {
				len = strlcpy(header, buf, buflen);
				if (len >= buflen) {
					/* header too long */
					fpurge(fp);
					return (-1);
				}
				header += len;
				buflen -= len;
			}

			if (code == 0) {
				/* status line expected */
				if (buf[0] == '\n') {
					/* ignore blank lines */
				}
				else if (sscanf(buf, "%s %d",
						version, &code) != 2) {
					/* can't get result code */
					fpurge(fp);
					return (-1);
				}
			}
			else {
				/* entity header expected */
				char *field, *cp;

				if (buf[0] == '\n') {
					/* end of header */
					end_of_header = 1;
					buflen = BODY_MAXSIZE;
					if (resid == 0)
						/* no message body */
						return (code);
				}

				cp = buf;
				field = strsep(&cp, ":");
				if (strcmp(field, "Content-Length") == 0) {
					if (sscanf(cp, "%d", &resid) != 1) {
						fpurge(fp);
						return (-1);
					}
					if (blen != NULL)
						*blen = resid;
				}
			}
		}
		else {
			/* process message body */
			if (body != NULL) {
				len = strlcpy(body, buf, buflen);
				if (len >= buflen) {
					/* body too long */
					fpurge(fp);
					return (-1);
				}
				body += len;
				buflen -= len;
			}
			else
				len = strlen(buf);
			resid -= len;
			if (resid <= 0)
				return (code);
		}
	}
	return (-1);
}

void
quip_rawmode(void)
{
	char line[MAXLINESIZE];
	int result_code;

	printf(">>>Entering the raw interactive mode to the server:\n\n");
	if (server == NULL) {
		printf("No server available!\n");
		return;
	}

	while (1) {
		printf("%% "); fflush(stdout);
		/* read a line from stdin */
		if (fgets(line, sizeof(line), stdin) == NULL)
			break;

		if (line[0] == '\n') {
			/* if a blank line, echo locally */
			fputs(line, stdout);
			continue;
		}
		if (line[0] == 'q') {
			printf("Exit\n");
			break;
		}

		/* send the input line to the server */
		quip_sendrequest(server, line);

		/* get a response message from the server */
		result_code = quip_recvresponse(server, NULL, NULL, NULL);
	}
}

char *
quip_selectinterface(char *ifname)
{
	char buf[BODY_MAXSIZE], *cp;
	int result_code, len;
	u_int if_index;
	static char interface[64];

	if (server == NULL)
		return (ifname);

	/* get an inferface list from the server */
	quip_sendrequest(server, "GET list?*\n");

	result_code = quip_recvresponse(server, NULL, buf, &len);
	if (result_code != 200)
		errx(1, "can't get interface list");

	cp = buf;
	while (1) {
		if (sscanf(cp, "%x %s", &if_index, interface) != 2)
			break;
		if (ifname == NULL) {
			/* if name isn't specified, return the 1st entry */
			return (interface);
		}
		if (strcmp(ifname, interface) == 0)
			/* found the matching entry */
			return (interface);
		if ((cp = strchr(cp+1, '\n')) == NULL)
			break;
	}
	errx(1, "can't get interface");
	return (NULL);
}

char *
quip_selectqdisc(char *ifname, char *qdisc_name)
{
	char buf[BODY_MAXSIZE], req[REQ_MAXSIZE];
	int result_code, len;
	static char qdisc[64];

	if (server == NULL) {
		if (ifname == NULL || qdisc_name == NULL)
			errx(1, "when disabling server communication,\n"
			    "specify both interface (-i) and qdisc (-q)!");
		return (qdisc_name);
	}

	/* get qdisc info from the server */
	snprintf(req, sizeof(req), "GET qdisc?%s\n", ifname);
	quip_sendrequest(server, req);

	result_code = quip_recvresponse(server, NULL, buf, &len);
	if (result_code != 200)
		errx(1, "can't get qdisc info");

	if (sscanf(buf, "%s", qdisc) != 1)
		errx(1, "can't get qdisc name");

	if (qdisc_name != NULL && strcmp(qdisc, qdisc_name) != 0)
		errx(1, "qdisc %s on %s doesn't match specified qdisc %s",
		    qdisc, ifname, qdisc_name);

	return (qdisc);
}

void
quip_chandle2name(const char *ifname, u_long handle, char *name, size_t size)
{
	char buf[BODY_MAXSIZE], req[REQ_MAXSIZE], *cp;
	int result_code, len;

	name[0] = '\0';
	if (server == NULL)
		return;
		
	/* get class name from the server */
	snprintf(req, sizeof(req), "GET handle-to-name?%s:%#lx\n", ifname, handle);
	quip_sendrequest(server, req);

	result_code = quip_recvresponse(server, NULL, buf, &len);
	if (result_code != 200)
		errx(1, "can't get class name");
	
	if ((cp = strchr(buf, '\n')) != NULL)
		*cp = '\0';
	if ((cp = strrchr(buf, '/')) != NULL)
		strlcpy(name, cp+1, size);
}

void
quip_printqdisc(const char *ifname)
{
	char buf[BODY_MAXSIZE], req[REQ_MAXSIZE], *cp;
	int result_code, len;

	if (server == NULL) {
		printf("No server available!\n");
		return;
	}

	/* get qdisc info from the server */
	snprintf(req, sizeof(req), "GET qdisc?%s\n", ifname);
	quip_sendrequest(server, req);

	result_code = quip_recvresponse(server, NULL, buf, &len);
	if (result_code != 200)
		errx(1, "can't get qdisc info");
	
	/* replace newline by space */
	cp = buf;
	while ((cp = strchr(cp, '\n')) != NULL)
		*cp = ' ';

	printf("  qdisc:%s\n", buf);
}

void
quip_printfilter(const char *ifname, const u_long handle)
{
	char buf[BODY_MAXSIZE], req[REQ_MAXSIZE], *cp;
	int result_code, len;

	if (server == NULL) {
		printf("No server available!\n");
		return;
	}

	/* get qdisc info from the server */
	snprintf(req, sizeof(req), "GET filter?%s::%#lx\n", ifname, handle);
	quip_sendrequest(server, req);

	result_code = quip_recvresponse(server, NULL, buf, &len);
	if (result_code != 200)
		errx(1, "can't get filter info");

	if ((cp = strchr(buf, '\n')) != NULL)
		*cp = '\0';
	printf("%s", buf);
}

static char *
extract_ifname(const char *name)
{
	char *cp;
	int len;
	static char ifname[64];

	if ((cp = strchr(name, ':')) != NULL)
		len = cp - name;
	else
		len = strlen(name);
	len = MIN(len, 63);
	strncpy(ifname, name, len);
	ifname[len] = '\0';
	return (ifname);
}

void 
quip_printconfig(void)
{
	char buf[BODY_MAXSIZE], name[256], *cp, *p, *flname;
	int result_code, len;
	enum nametype type;
	u_long handle;

	if (server == NULL) {
		printf("No server available!\n");
		return;
	}

	/* get a total list from the server */
	quip_sendrequest(server, "GET list\n");

	result_code = quip_recvresponse(server, NULL, buf, &len);
	if (result_code != 200)
		errx(1, "can't get total list");

	printf("------------ current configuration ------------");

	cp = buf;
	while (1) {
		if (sscanf(cp, "%lx %s", &handle, name) != 2)
			break;

		if ((p = strchr(name, ':')) == NULL)
			type = INTERFACE;
		else if (strchr(p+1, ':') == NULL)
			type = CLASS;
		else
			type = FILTER;

		switch (type) {
		case INTERFACE:
			printf("\ninterface: %s  (index:%lu)\n",
			       name, handle);
			quip_printqdisc(name);
			break;
		case CLASS:
			printf("class: %s  (handle:%#lx)\n",
			       name, handle);
			break;
		case FILTER:
			flname = strrchr(name, ':') + 1;
			printf("  filter: name:%s [", flname);
			quip_printfilter(extract_ifname(name), handle);
			printf("]  (handle:%#lx)\n", handle);
			break;
		case CONDITIONER:
			break;
		}

		if ((cp = strchr(cp+1, '\n')) == NULL)
			break;
	}
	printf("-----------------------------------------------\n\n");
}

