/*	$OpenBSD: util.c,v 1.1 2006/04/26 02:55:13 joris Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
 * Copyright (c) 2005, 2006 Joris Vink <joris@openbsd.org>
 * Copyright (c) 2005, 2006 Xavier Santolaria <xsa@openbsd.org>
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

#include "includes.h"

#include "buf.h"
#include "util.h"
#include "xmalloc.h"

/*
 * Split the contents of a file into a list of lines.
 */
struct rcs_lines *
rcs_splitlines(const char *fcont)
{
	char *dcp;
	struct rcs_lines *lines;
	struct rcs_line *lp;

	lines = xmalloc(sizeof(*lines));
	TAILQ_INIT(&(lines->l_lines));
	lines->l_nblines = 0;
	lines->l_data = xstrdup(fcont);

	lp = xmalloc(sizeof(*lp));
	lp->l_line = NULL;
	lp->l_lineno = 0;
	TAILQ_INSERT_TAIL(&(lines->l_lines), lp, l_list);

	for (dcp = lines->l_data; *dcp != '\0';) {
		lp = xmalloc(sizeof(*lp));
		lp->l_line = dcp;
		lp->l_lineno = ++(lines->l_nblines);
		TAILQ_INSERT_TAIL(&(lines->l_lines), lp, l_list);

		dcp = strchr(dcp, '\n');
		if (dcp == NULL)
			break;
		*(dcp++) = '\0';
	}

	return (lines);
}

void
rcs_freelines(struct rcs_lines *lines)
{
	struct rcs_line *lp;

	while ((lp = TAILQ_FIRST(&(lines->l_lines))) != NULL) {
		TAILQ_REMOVE(&(lines->l_lines), lp, l_list);
		xfree(lp);
	}

	xfree(lines->l_data);
	xfree(lines);
}

BUF *
rcs_patchfile(const char *data, const char *patch,
    int (*p)(struct rcs_lines *, struct rcs_lines *))
{
	struct rcs_lines *dlines, *plines;
	struct rcs_line *lp;
	size_t len;
	int lineno;
	BUF *res;

	len = strlen(data);

	if ((dlines = rcs_splitlines(data)) == NULL)
		return (NULL);

	if ((plines = rcs_splitlines(patch)) == NULL)
		return (NULL);

	if (p(dlines, plines) < 0) {
		rcs_freelines(dlines);
		rcs_freelines(plines);
		return (NULL);
	}

	lineno = 0;
	res = rcs_buf_alloc(len, BUF_AUTOEXT);
	TAILQ_FOREACH(lp, &dlines->l_lines, l_list) {
		if (lineno != 0)
			rcs_buf_fappend(res, "%s\n", lp->l_line);
		lineno++;
	}

	rcs_freelines(dlines);
	rcs_freelines(plines);
	return (res);
}

/*
 * rcs_yesno()
 *
 * Read from standart input for `y' or `Y' character.
 * Returns 0 on success, or -1 on failure.
 */
int
rcs_yesno(void)
{
	int c, ret;

	ret = 0;

	fflush(stderr);
	fflush(stdout);

	if ((c = getchar()) != 'y' && c != 'Y')
		ret = -1;
	else
		while (c != EOF && c != '\n')
			c = getchar();

	return (ret);
}

/*
 * rcs_strsplit()
 *
 * Split a string <str> of <sep>-separated values and allocate
 * an argument vector for the values found.
 */
struct rcs_argvector *
rcs_strsplit(char *str, const char *sep)
{
	struct rcs_argvector *av;
	size_t i = 0;
	char **nargv;
	char *cp, *p;

	cp = xstrdup(str);
	av = xmalloc(sizeof(*av));
	av->str = cp;
	av->argv = xcalloc(i + 1, sizeof(*(av->argv)));

	while ((p = strsep(&cp, sep)) != NULL) {
		av->argv[i++] = p;
		nargv = xrealloc(av->argv,
		    i + 1, sizeof(*(av->argv)));
		av->argv = nargv;
	}
	av->argv[i] = NULL;

	return (av);
}

/*
 * rcs_argv_destroy()
 *
 * Free an argument vector previously allocated by rcs_strsplit().
 */
void
rcs_argv_destroy(struct rcs_argvector *av)
{
	xfree(av->str);
	xfree(av->argv);
	xfree(av);
}
