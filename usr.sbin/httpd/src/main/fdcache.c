/*	$OpenBSD: fdcache.c,v 1.5 2002/08/02 11:52:01 henning Exp $ */

/*
 * Copyright (c) 2002 Henning Brauer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

struct fdcache {
    char *fname;
    int  fd;
    struct fdcache *next;
};

struct fdcache	*fdc;

int
fdcache_open(char *fn, int flags, mode_t mode)
{
    struct fdcache *fdcp = NULL, *tmp = NULL;

    for (fdcp = fdc; fdcp && strncmp(fn, fdcp->fname, 1024); fdcp = fdcp->next);
	/* nothing */

    if (fdcp == NULL) {
	/* need to open */
	tmp = calloc(1, sizeof(struct fdcache));
	if (tmp == NULL) {
	    fprintf(stderr, "calloc failed\n");
	    exit(1);
	}
	tmp->fname = malloc(strlen(fn) + 1);
	if (tmp->fname == NULL) {
	    fprintf(stderr, "malloc failed\n");
	    exit(1);
	}
	strlcpy(tmp->fname, fn, strlen(fn) + 1);
	if ((tmp->fd = open(fn, flags, mode)) < 0) {
	    fprintf(stderr, "Cannot open %s: %s\n",
	      tmp->fname, strerror(errno));
	    exit(1);
	}
	tmp->next = fdc;
	fdc = tmp;
	return(fdc->fd);
    } else
	return(fdcp->fd);	/* fd cached */
}

void
fdcache_closeall(void)
{
    struct fdcache *fdcp = NULL, *tmp = NULL;

    for (fdcp = fdc; fdcp; ) {
	tmp = fdcp;
	fdcp = tmp->next;
	if (tmp->fd > 0)
	    close(tmp->fd);
	free(tmp->fname);
	free(tmp);
    }
}

