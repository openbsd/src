/*	$OpenBSD: bgplg.c,v 1.6 2007/09/13 23:32:39 cloder Exp $	*/

/*
 * Copyright (c) 2005, 2006 Reyk Floeter <reyk@vantronix.net>
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

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>

#include "bgplg.h"

#define INC_STYLE	"/conf/bgplg.css"
#define INC_HEAD	"/conf/bgplg.head"
#define INC_FOOT	"/conf/bgplg.foot"

#define BGPDSOCK	"/logs/bgpd.rsock"
#define BGPCTL		"/bin/bgpctl", "-s", BGPDSOCK
#define PING		"/bin/ping"
#define TRACEROUTE	"/bin/traceroute"
#define CONTENT_TYPE	"text/html"

static struct cmd cmds[] = CMDS;

char		 *lg_getenv(const char *, int *);
void		  lg_urldecode(char *);
char		**lg_arg2argv(char *, int *);
char		**lg_argextra(char **, int, struct cmd *);
char		 *lg_getarg(const char *, char *, int);
int		  lg_incl(const char *);

void
lg_urldecode(char *str)
{
	size_t i, c, len;
	char code[3];
	long result;

	if (str && *str) {
		len = strlen(str);
		i = c = 0;
		while (i < len) {
			if (str[i] == '%' && i <= (len - 2)) {
				if (isxdigit(str[i + 1]) &&
				    isxdigit(str[i + 2])) {
					code[0] = str[i + 1];
					code[1] = str[i + 2];
					code[2] = 0;
					result = strtol(code, NULL, 16);
					/* Replace NUL chars with a space */
					if (result == 0)
						result = ' ';
					str[c++] = result;
					i += 3;
				} else {
					str[c++] = '%';
					i++;
				}
			} else if (str[i] == '+') {
				str[i] = ' ';
			} else {
				if (c != i)
					str[c] = str[i];
				c++;
				i++;
			}
		}
		str[c] = 0x0;
	}
}

char *
lg_getenv(const char *name, int *lenp)
{
	size_t len;
	u_int i;
	char *ptr;

	if ((ptr = getenv(name)) == NULL)
		return (NULL);

	lg_urldecode(ptr);

	if (!(len = strlen(ptr)))
		return (NULL);

	if (lenp != NULL)
		*lenp = len;

#define allowed_in_string(_x)                                           \
	((isalnum(_x) || isprint(_x)) &&				\
	(_x != '%' && _x != '\\' && _x != ';' && _x != '|'))

	for (i = 0; i < len; i++) {
		if (!allowed_in_string(ptr[i])) {
			printf("invalid character in input\n");
			return (NULL);
		}
		if (ptr[i] == '&')
			ptr[i] = '\0';
	}

	return (ptr);
}

char *
lg_getarg(const char *name, char *arg, int len)
{
	char *ptr = arg;
	size_t namelen, ptrlen;
	int i;

	namelen = strlen(name);

	for (i = 0; i < len; i++) {
		if (arg[i] == '\0')
			continue;
		ptr = arg + i;
		ptrlen = strlen(ptr);
		if (namelen >= ptrlen)
			continue;
		if (strncmp(name, ptr, namelen) == 0)
			return (ptr + namelen);
	}

	return (NULL);
}

char **
lg_arg2argv(char *arg, int *argc)
{
	char **argv, *ptr = arg;
	size_t len;
	u_int i, c = 1;

	len = strlen(arg);

	/* Count elements */
	for (i = 0; i < (len - 1); i++) {
		if (isspace(arg[i])) {
			/* filter out additional options */
			if (arg[i + 1] == '-') {
				printf("invalid input\n");
				return (NULL);
			}
			arg[i] = '\0';
			c++;
		}
	}

	/* Generate array */
	if ((argv = calloc(c + 1, sizeof(char *))) == NULL) {
		printf("fatal error: %s\n", strerror(errno));
		return (NULL);
	}

	argv[c] = NULL;
	*argc = c;

	/* Fill array */
	for (i = c = 0; i < (len - 1); i++) {
		if (arg[i] == '\0' || i == 0) {
			if (i != 0)
				ptr = &arg[i + 1];
			argv[c++] = ptr;
		}
	}

	return (argv);
}

char **
lg_argextra(char **argv, int argc, struct cmd *cmdp)
{
	char **new_argv;
	int i, c = 0;

	/* Count elements */
	for (i = 0; cmdp->earg[i] != NULL; i++)
		c++;

	/* Generate array */
	if ((new_argv = calloc(c + argc + 1, sizeof(char *))) == NULL) {
		printf("fatal error: %s\n", strerror(errno));
		return (NULL);
	}

	/* Fill array */
	for (i = c = 0; cmdp->earg[i] != NULL; i++)
		new_argv[c++] = cmdp->earg[i];

	/* Append old array */
	for (i = 0; i < argc; i++)
		new_argv[c++] = argv[i];

	new_argv[c] = NULL;

	if (argv != NULL)
		free(argv);

	return (new_argv);
}

int
lg_incl(const char *file)
{
	char buf[BUFSIZ];
	int fd, len;

	if ((fd = open(file, O_RDONLY)) == -1)
		return (errno);

	do {
		len = read(fd, buf, sizeof(buf));
		fwrite(buf, len, 1, stdout);
	} while(len == BUFSIZ);

	return (0);
}

int
main(void)
{
	char *query, *self, *cmd = NULL, *req;
	char **argv = NULL;
	char myname[MAXHOSTNAMELEN];
	int ret = 1, argc = 0, query_length = 0;
	struct stat st;
	u_int i;
	struct cmd *cmdp = NULL;

	if (gethostname(myname, sizeof(myname)) != 0)
		return (1);

	printf("Content-Type: %s\n"
	    "Cache-Control: no-cache\n\n"
	    "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n"
	    "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" "
	    "\"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">\n"
	    "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n"
	    "<head>\n"
	    "<title>%s: %s</title>\n",
	    CONTENT_TYPE, NAME, myname);
	if (stat(INC_STYLE, &st) == 0) {
		printf("<style type='text/css'><!--\n");
		lg_incl(INC_STYLE);
		printf("--></style>\n");
	}
	if (stat(INC_HEAD, &st) != 0 || lg_incl(INC_HEAD) != 0) {
		printf("</head>\n"
		    "<body>\n");
	}

	printf("<h1>%s: %s</h1>\n", NAME, myname);
	printf("<h2>%s</h2>\n", BRIEF);

	/* print a form with possible options */
	if ((self = lg_getenv("SCRIPT_NAME", NULL)) == NULL) {
		printf("fatal error: invalid request\n");
		goto err;
	}
	if ((query = lg_getenv("QUERY_STRING", &query_length)) != NULL)
		cmd = lg_getarg("cmd=", query, query_length);
	printf(
	    "<form action='%s'>\n"
	    "<div class=\"command\">\n"
	    "<select name='cmd'>\n",
	    self);
	for (i = 0; cmds[i].name != NULL; i++) {
		if (!lg_checkperm(&cmds[i]))
			continue;

		if (cmd != NULL && strcmp(cmd, cmds[i].name) == 0)
			printf("<option value='%s' selected='selected'>%s"
			    "</option>\n",
			    cmds[i].name, cmds[i].name);
		else
			printf("<option value='%s'>%s</option>\n",
			    cmds[i].name, cmds[i].name);
	}
	printf("</select>\n"
	    "<input type='text' name='req'/>\n"
	    "<input type='submit' value='submit'/>\n"
	    "</div>\n"
	    "</form>\n"
	    "<pre>\n");
	fflush(stdout);

#ifdef DEBUG
	if (close(2) == -1 || dup2(1, 2) == -1)
#else
	if (close(2) == -1)
#endif
	{
		printf("fatal error: %s\n", strerror(errno));
		goto err;
	}

	if (query == NULL)
		goto err;
	if (cmd == NULL) {
		printf("unspecified command\n");
		goto err;
	}
	if ((req = lg_getarg("req=", query, query_length)) != NULL) {
		/* Could be NULL */
		argv = lg_arg2argv(req, &argc);
	}

	for (i = 0; cmds[i].name != NULL; i++) {
		if (strcmp(cmd, cmds[i].name) == 0) {
			cmdp = &cmds[i];
			break;
		}
	}

	if (cmdp == NULL) {
		printf("invalid command: %s\n", cmd);
		goto err;
	}
	if (argc > cmdp->maxargs) {
		printf("superfluous argument(s): %s %s\n",
		    cmd, cmdp->args ? cmdp->args : "");
		goto err;
	}
	if (argc < cmdp->minargs) {
		printf("missing argument(s): %s %s\n", cmd, cmdp->args);
		goto err;
	}

	if (cmdp->func != NULL) {
		ret = cmdp->func(cmds, argv);
	} else {
		if ((argv = lg_argextra(argv, argc, cmdp)) == NULL)
			goto err;
		ret = lg_exec(cmdp->earg[0], argv);
	}
	if (ret != 0)
		printf("\nfailed%s\n", ret == 127 ? ": file not found" : ".");
	else
		printf("\nsuccess.\n");

 err:
	fflush(stdout);

	if (argv != NULL)
		free(argv);

	printf("</pre>\n");

	if (stat(INC_FOOT, &st) != 0 || lg_incl(INC_FOOT) != 0)
		printf("<hr/>\n");

	printf("<div class='footer'>\n"
	    "<small>%s - %s<br/>Copyright (c) %s</small>\n"
	    "</div>\n"
	    "</body>\n"
	    "</html>\n", NAME, BRIEF, COPYRIGHT);

	return (ret);
}
