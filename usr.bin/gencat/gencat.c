/*	$OpenBSD: gencat.c,v 1.14 2011/01/01 11:25:54 lum Exp $	*/
/*	$NetBSD: gencat.c,v 1.9 1998/10/09 17:00:56 itohy Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by J.T. Conklin.
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

#include <sys/cdefs.h>

/***********************************************************
Copyright 1990, by Alfalfa Software Incorporated, Cambridge, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that Alfalfa's name not be used in
advertising or publicity pertaining to distribution of the software
without specific, written prior permission.

ALPHALPHA DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
ALPHALPHA BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

If you make any modifications, bugfixes or other changes to this software
we'd appreciate it if you could send a copy to us so we can keep things
up-to-date.  Many thanks.
				Kee Hinckley
				Alfalfa Software, Inc.
				267 Allston St., #3
				Cambridge, MA 02139  USA
				nazgul@alfalfa.com

******************************************************************/

#define _NLS_PRIVATE

/* ensure 8-bit cleanliness */
#define ISSPACE(c) \
    (isascii((unsigned char)c) && isspace((unsigned char)c))

#include <sys/queue.h>
#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <nl_types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <nl_types.h>
#include <err.h>

struct _msgT {
	long    msgId;
	char   *str;
        LIST_ENTRY(_msgT) entries;
};

struct _setT {
	long    setId;
        LIST_HEAD(msghead, _msgT) msghead;
        LIST_ENTRY(_setT) entries;
};

LIST_HEAD(sethead, _setT) sethead;
static struct _setT *curSet;

static char *curline = NULL;
static long lineno = 0;

extern	char	*__progname;		/* from crt0.o */

static	char   *cskip(char *);
static	void	error(char *, char *);
static	void	nomem(void);
static	char   *getline(int);
static	char   *getmsg(int, char *, char);
static	void	warning(char *, char *);
static	char   *wskip(char *);
static	char   *xstrdup(const char *);
static	void   *xmalloc(size_t);
static	void   *xrealloc(void *, size_t);

void	MCParse(int fd);
void	MCWriteCat(int fd);
void	MCDelMsg(int msgId);
void	MCAddMsg(int msgId, const char *msg);
void	MCAddSet(int setId);
void	MCDelSet(int setId);
int	main(int, char **);
void	usage(void);


void
usage(void)
{
	fprintf(stderr, "usage: %s catfile msgfile ...\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int     ofd, ifd;
	char   *catfile = NULL;
	int     c;

	while ((c = getopt(argc, argv, "")) != -1) {
		switch (c) {
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 2) {
		usage();
		/* NOTREACHED */
	}
	catfile = *argv++;

	for (; *argv; argv++) {
		if ((ifd = open(*argv, O_RDONLY)) < 0)
			err(1, "Unable to read %s", *argv);
		MCParse(ifd);
		close(ifd);
	}

	if ((ofd = open(catfile, O_WRONLY | O_TRUNC | O_CREAT, 0666)) < 0)
		err(1, "Unable to create a new %s", catfile);
	MCWriteCat(ofd);
	exit(0);
}

static void
warning(char *cptr, char *msg)
{
	warnx("%s on line %ld\n%s", msg, lineno, curline);
	if (cptr) {
		char   *tptr;
		for (tptr = curline; tptr < cptr; ++tptr)
			putc(' ', stderr);
		fprintf(stderr, "^\n");
	}
}

static void
error(char *cptr, char *msg)
{
	warning(cptr, msg);
	exit(1);
}

static void
nomem(void)
{
	error(NULL, "out of memory");
}

static void *
xmalloc(size_t len)
{
	void   *p;

	if ((p = malloc(len)) == NULL)
		nomem();
	return (p);
}

static void *
xrealloc(void *ptr, size_t size)
{
	if ((ptr = realloc(ptr, size)) == NULL)
		nomem();
	return (ptr);
}

static char *
xstrdup(const char *str)
{
	char *nstr;

	if ((nstr = strdup(str)) == NULL)
		nomem();
	return (nstr);
}

static char *
getline(int fd)
{
	static long curlen = BUFSIZ;
	static char buf[BUFSIZ], *bptr = buf, *bend = buf;
	char   *cptr, *cend;
	long    buflen;

	if (!curline) {
		curline = xmalloc(curlen);
	}
	++lineno;

	cptr = curline;
	cend = curline + curlen;
	for (;;) {
		for (; bptr < bend && cptr < cend; ++cptr, ++bptr) {
			if (*bptr == '\n') {
				*cptr = '\0';
				++bptr;
				return (curline);
			} else
				*cptr = *bptr;
		}
		if (bptr == bend) {
			buflen = read(fd, buf, BUFSIZ);
			if (buflen <= 0) {
				if (cptr > curline) {
					*cptr = '\0';
					return (curline);
				}
				return (NULL);
			}
			bend = buf + buflen;
			bptr = buf;
		}
		if (cptr == cend) {
			cptr = curline = xrealloc(curline, curlen *= 2);
			cend = curline + curlen;
		}
	}
}

static char *
wskip(char *cptr)
{
	if (!*cptr || !ISSPACE(*cptr)) {
		warning(cptr, "expected a space");
		return (cptr);
	}
	while (*cptr && ISSPACE(*cptr))
		++cptr;
	return (cptr);
}

static char *
cskip(char *cptr)
{
	if (!*cptr || ISSPACE(*cptr)) {
		warning(cptr, "wasn't expecting a space");
		return (cptr);
	}
	while (*cptr && !ISSPACE(*cptr))
		++cptr;
	return (cptr);
}

static char *
getmsg(int fd, char *cptr, char quote)
{
	static char *msg = NULL;
	static long msglen = 0;
	long    clen, i;
	char   *tptr;

	if (quote && *cptr == quote) {
		++cptr;
	} 

	clen = strlen(cptr) + 1;
	if (clen > msglen) {
		if (msglen)
			msg = xrealloc(msg, clen);
		else
			msg = xmalloc(clen);
		msglen = clen;
	}
	tptr = msg;

	while (*cptr) {
		if (quote && *cptr == quote) {
			char   *tmp;
			tmp = cptr + 1;

			if (*tmp && (!ISSPACE(*tmp) || *wskip(tmp))) {
				warning(cptr, "unexpected quote character, ignoring");
				*tptr++ = *cptr++;
			} else {
				*cptr = '\0';
			}
		} else if (*cptr == '\\') {
			++cptr;
			switch (*cptr) {
			case '\0':
				cptr = getline(fd);
				if (!cptr)
					error(NULL, "premature end of file");
				msglen += strlen(cptr);
				i = tptr - msg;
				msg = xrealloc(msg, msglen);
				tptr = msg + i;
				break;
			case 'n':
				*tptr++ = '\n';
				++cptr;
				break;
			case 't':
				*tptr++ = '\t';
				++cptr;
				break;
			case 'v':
				*tptr++ = '\v';
				++cptr;
				break;
			case 'b':
				*tptr++ = '\b';
				++cptr;
				break;
			case 'r':
				*tptr++ = '\r';
				++cptr;
				break;
			case 'f':
				*tptr++ = '\f';
				++cptr;
				break;
			case '\\':
				*tptr++ = '\\';
				++cptr;
				break;
			case '"':
				/* FALLTHROUGH */
			case '\'':
				/*
				 * While it isn't necessary to
				 * escape ' and ", let's accept
				 * them escaped and not complain.
				 * (XPG4 states that '\' should be
				 * ignored when not used in a 
				 * valid escape sequence)
				 */
				*tptr++ = '"';
				++cptr;
				break;
			default:
				if (quote && *cptr == quote) {
					*tptr++ = *cptr++;
				} else if (isdigit((unsigned char) *cptr)) {
					*tptr = 0;
					for (i = 0; i < 3; ++i) {
						if (!isdigit((unsigned char) *cptr))
							break;
						if (*cptr > '7')
							warning(cptr, "octal number greater than 7?!");
						*tptr *= 8;
						*tptr += (*cptr - '0');
						++cptr;
					}
				} else {
					warning(cptr, "unrecognized escape sequence; ignoring esacpe character");
				}
				break;
			}
		} else {
			*tptr++ = *cptr++;
		}
	}
	*tptr = '\0';
	return (msg);
}

void
MCParse(int fd)
{
	char   *cptr, *str;
	int     setid, msgid = 0;
	char    quote = 0;

	/* XXX: init sethead? */

	while ((cptr = getline(fd))) {
		if (*cptr == '$') {
			++cptr;
			if (strncmp(cptr, "set", 3) == 0) {
				cptr += 3;
				cptr = wskip(cptr);
				setid = atoi(cptr);
				MCAddSet(setid);
				msgid = 0;
			} else if (strncmp(cptr, "delset", 6) == 0) {
				cptr += 6;
				cptr = wskip(cptr);
				setid = atoi(cptr);
				MCDelSet(setid);
			} else if (strncmp(cptr, "quote", 5) == 0) {
				cptr += 5;
				if (!*cptr)
					quote = 0;
				else {
					cptr = wskip(cptr);
					if (!*cptr)
						quote = 0;
					else
						quote = *cptr;
				}
			} else if (ISSPACE(*cptr)) {
				;
			} else {
				if (*cptr) {
					cptr = wskip(cptr);
					if (*cptr)
						warning(cptr, "unrecognized line");
				}
			}
		} else {
			/*
			 * First check for (and eat) empty lines....
			 */
			if (!*cptr)
				continue;
			/*
			 * We have a digit? Start of a message. Else,
			 * syntax error.
			 */
			if (isdigit((unsigned char) *cptr)) {
				msgid = atoi(cptr);
				cptr = cskip(cptr);
				cptr = wskip(cptr);
				/* if (*cptr) ++cptr; */
			} else {
				warning(cptr, "neither blank line nor start of a message id");
				continue;
			}
			/*
			 * If we have a message ID, but no message,
			 * then this means "delete this message id
			 * from the catalog".
			 */
			if (!*cptr) {
				MCDelMsg(msgid);
			} else {
				str = getmsg(fd, cptr, quote);
				MCAddMsg(msgid, str);
			}
		}
	}
}

/*
 * Write message catalog.
 *
 * The message catalog is first converted from its internal to its
 * external representation in a chunk of memory allocated for this
 * purpose.  Then the completed catalog is written.  This approach
 * avoids additional housekeeping variables and/or a lot of seeks
 * that would otherwise be required.
 */
void
MCWriteCat(int fd)
{
	int     nsets;		/* number of sets */
	int     nmsgs;		/* number of msgs */
	int     string_size;	/* total size of string pool */
	int     msgcat_size;	/* total size of message catalog */
	void   *msgcat;		/* message catalog data */
	struct _nls_cat_hdr *cat_hdr;
	struct _nls_set_hdr *set_hdr;
	struct _nls_msg_hdr *msg_hdr;
	char   *strings;
	struct _setT *set;
	struct _msgT *msg;
	int     msg_index;
	int     msg_offset;

	/* determine number of sets, number of messages, and size of the
	 * string pool */
	nsets = 0;
	nmsgs = 0;
	string_size = 0;

	LIST_FOREACH(set, &sethead, entries) {
		nsets++;

		LIST_FOREACH(msg, &set->msghead, entries) {
			nmsgs++;
			string_size += strlen(msg->str) + 1;
		}
	}

#ifdef DEBUG
	printf("number of sets: %d\n", nsets);
	printf("number of msgs: %d\n", nmsgs);
	printf("string pool size: %d\n", string_size);
#endif

	/* determine size and then allocate buffer for constructing external
	 * message catalog representation */
	msgcat_size = sizeof(struct _nls_cat_hdr)
	    + (nsets * sizeof(struct _nls_set_hdr))
	    + (nmsgs * sizeof(struct _nls_msg_hdr))
	    + string_size;

	msgcat = xmalloc(msgcat_size);
	memset(msgcat, '\0', msgcat_size);

	/* fill in msg catalog header */
	cat_hdr = (struct _nls_cat_hdr *) msgcat;
	cat_hdr->__magic = htonl(_NLS_MAGIC);
	cat_hdr->__nsets = htonl(nsets);
	cat_hdr->__mem = htonl(msgcat_size - sizeof(struct _nls_cat_hdr));
	cat_hdr->__msg_hdr_offset =
	    htonl(nsets * sizeof(struct _nls_set_hdr));
	cat_hdr->__msg_txt_offset =
	    htonl(nsets * sizeof(struct _nls_set_hdr) +
	    nmsgs * sizeof(struct _nls_msg_hdr));

	/* compute offsets for set & msg header tables and string pool */
	set_hdr = (struct _nls_set_hdr *) ((char *) msgcat +
	    sizeof(struct _nls_cat_hdr));
	msg_hdr = (struct _nls_msg_hdr *) ((char *) msgcat +
	    sizeof(struct _nls_cat_hdr) +
	    nsets * sizeof(struct _nls_set_hdr));
	strings = (char *) msgcat +
	    sizeof(struct _nls_cat_hdr) +
	    nsets * sizeof(struct _nls_set_hdr) +
	    nmsgs * sizeof(struct _nls_msg_hdr);

	msg_index = 0;
	msg_offset = 0;
	LIST_FOREACH(set, &sethead, entries) {

		nmsgs = 0;
		LIST_FOREACH(msg, &set->msghead, entries) {
			int     msg_len = strlen(msg->str) + 1;

			msg_hdr->__msgno = htonl(msg->msgId);
			msg_hdr->__msglen = htonl(msg_len);
			msg_hdr->__offset = htonl(msg_offset);

			memcpy(strings, msg->str, msg_len);
			strings += msg_len;
			msg_offset += msg_len;

			nmsgs++;
			msg_hdr++;
		}

		set_hdr->__setno = htonl(set->setId);
		set_hdr->__nmsgs = htonl(nmsgs);
		set_hdr->__index = htonl(msg_index);
		msg_index += nmsgs;
		set_hdr++;
	}

	/* write out catalog.  XXX: should this be done in small chunks? */
	write(fd, msgcat, msgcat_size);
}

void
MCAddSet(int setId)
{
	struct _setT *p, *q;

	if (setId <= 0) {
		error(NULL, "setId's must be greater than zero");
		/* NOTREACHED */
	}
#if 0
	/* XXX */
	if (setId > NL_SETMAX) {
		error(NULL, "setId %d exceeds limit (%d)");
		/* NOTREACHED */
	}
#endif

	p = LIST_FIRST(&sethead);
	q = NULL;
	for (; p != NULL && p->setId < setId; q = p, p = LIST_NEXT(p, entries));

	if (p && p->setId == setId) {
		;
	} else {
		p = xmalloc(sizeof(struct _setT));
		memset(p, '\0', sizeof(struct _setT));
		LIST_INIT(&p->msghead);

		p->setId = setId;

		if (q == NULL) {
			LIST_INSERT_HEAD(&sethead, p, entries);
		} else {
			LIST_INSERT_AFTER(q, p, entries);
		}
	}

	curSet = p;
}

void
MCAddMsg(int msgId, const char *str)
{
	struct _msgT *p, *q;

	if (!curSet)
		error(NULL, "can't specify a message when no set exists");

	if (msgId <= 0) {
		error(NULL, "msgId's must be greater than zero");
		/* NOTREACHED */
	}
#if 0
	/* XXX */
	if (msgId > NL_SETMAX) {
		error(NULL, "msgId %d exceeds limit (%d)");
		/* NOTREACHED */
	}
#endif

	p = LIST_FIRST(&curSet->msghead);
	q = NULL;
	for (; p != NULL && p->msgId < msgId; q = p, p = LIST_NEXT(p, entries));

	if (p && p->msgId == msgId) {
		free(p->str);
	} else {
		p = xmalloc(sizeof(struct _msgT));
		memset(p, '\0', sizeof(struct _msgT));

		if (q == NULL) {
			LIST_INSERT_HEAD(&curSet->msghead, p, entries);
		} else {
			LIST_INSERT_AFTER(q, p, entries);
		}
	}

	p->msgId = msgId;
	p->str = xstrdup(str);
}

void
MCDelSet(int setId)
{
	struct _setT *set;
	struct _msgT *msg;

	set = LIST_FIRST(&sethead);
	for (; set != NULL && set->setId < setId;
	    set = LIST_NEXT(set, entries));

	if (set && set->setId == setId) {

		msg = LIST_FIRST(&set->msghead);
		while (msg) {
			free(msg->str);
			LIST_REMOVE(msg, entries);
		}

		LIST_REMOVE(set, entries);
		return;
	}
	warning(NULL, "specified set doesn't exist");
}

void
MCDelMsg(int msgId)
{
	struct _msgT *msg;

	if (!curSet)
		error(NULL, "you can't delete a message before defining the set");

	msg = LIST_FIRST(&curSet->msghead);
	for (; msg != NULL && msg->msgId < msgId;
	    msg = LIST_NEXT(msg, entries));

	if (msg && msg->msgId == msgId) {
		free(msg->str);
		LIST_REMOVE(msg, entries);
		return;
	}
	warning(NULL, "specified msg doesn't exist");
}
