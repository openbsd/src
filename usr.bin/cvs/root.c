/*	$OpenBSD: root.c,v 1.14 2004/12/28 21:58:42 jfb Exp $	*/
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <paths.h>

#include "cvs.h"
#include "log.h"
#include "proto.h"


extern char *cvs_rootstr;


/* keep these ordered with the defines */
const char *cvs_methods[] = {
	"",
	"local",
	"ssh",
	"pserver",
	"kserver",
	"gserver",
	"ext",
	"fork",
};

#define CVS_NBMETHODS  (sizeof(cvs_methods)/sizeof(cvs_methods[0]))

/*
 * CVSROOT cache
 *
 * Whenever cvsroot_parse() gets called for a specific string, it first
 * checks in the cache to see if there is already a parsed version of the
 * same string and returns a pointer to it in case one is found (it also
 * increases the reference count).  Otherwise, it does the parsing and adds
 * the result to the cache for future hits.
 */

static struct cvsroot **cvs_rcache = NULL;
static u_int cvs_rcsz = 0;



/*
 * cvsroot_parse()
 *
 * Parse a CVS root string (as found in CVS/Root files or the CVSROOT
 * environment variable) and store the fields in a dynamically
 * allocated cvs_root structure.  The format of the string is as follows:
 *	:method:[[user[:pass]@host]:path
 * Returns a pointer to the allocated information on success, or NULL
 * on failure.
 */
struct cvsroot*
cvsroot_parse(const char *str)
{
	u_int i;
	char *cp, *sp, *pp;
	void *tmp;
	struct cvsroot *root;

	for (i = 0; i < cvs_rcsz; i++) {
		if (strcmp(str, cvs_rcache[i]->cr_str) == 0) {
			cvs_rcache[i]->cr_ref++;
			return (cvs_rcache[i]);
		}
	}

	root = (struct cvsroot *)malloc(sizeof(*root));
	if (root == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate CVS root data");
		return (NULL);
	}
	memset(root, 0, sizeof(*root));
	root->cr_ref = 2;
	root->cr_method = CVS_METHOD_NONE;
	CVS_RSTVR(root);

	/* enable the most basic commands at least */
	CVS_SETVR(root, CVS_REQ_VALIDREQ);
	CVS_SETVR(root, CVS_REQ_VALIDRESP);

	root->cr_str = strdup(str);
	if (root->cr_str == NULL) {
		free(root);
		return (NULL);
	}
	root->cr_buf = strdup(str);
	if (root->cr_buf == NULL) {
		cvs_log(LP_ERRNO, "failed to copy CVS root");
		cvsroot_free(root);
		return (NULL);
	}

	sp = root->cr_buf;
	cp = root->cr_buf;
	if (*sp == ':') {
		sp++;
		cp = strchr(sp, ':');
		if (cp == NULL) {
			cvs_log(LP_ERR, "failed to parse CVSROOT: "
			    "unterminated method");
			cvsroot_free(root);
			return (NULL);
		}
		*(cp++) = '\0';

		for (i = 0; i < CVS_NBMETHODS; i++) {
			if (strcmp(sp, cvs_methods[i]) == 0) {
				root->cr_method = i;
				break;
			}
		}
		if (i == CVS_NBMETHODS) {
			cvs_log(LP_ERR, "unknown method `%s'", sp);
			cvsroot_free(root);
			return (NULL);
		}
	}

	/* find the start of the actual path */
	sp = strchr(cp, '/');
	if (sp == NULL) {
		cvs_log(LP_ERR, "no path specification in CVSROOT");
		cvsroot_free(root);
		return (NULL);
	}

	root->cr_dir = sp;
	if (sp == cp) {
		if (root->cr_method == CVS_METHOD_NONE)
			root->cr_method = CVS_METHOD_LOCAL;
		/* stop here, it's just a path */
		return (root);
	}

	if (*(sp - 1) != ':') {
		cvs_log(LP_ERR, "missing host/path delimiter in CVS root");
		cvsroot_free(root);
		return (NULL);
	}
	*(sp - 1) = '\0';

	/*
	 * looks like we have more than just a directory path, so
	 * attempt to split it into user and host parts
	 */
	sp = strchr(cp, '@');
	if (sp != NULL) {
		*(sp++) = '\0';

		/* password ? */
		pp = strchr(cp, ':');
		if (pp != NULL) {
			*(pp++) = '\0';
			root->cr_pass = pp;
		}

		root->cr_user = cp;
	} else
		sp = cp;

	pp = strchr(sp, ':');
	if (pp != NULL) {
		*(pp++) = '\0';
		root->cr_port = (u_int)strtol(pp, &cp, 10);
		if (*cp != '\0' || root->cr_port > 65535) {
			cvs_log(LP_ERR,
			    "invalid port specification in CVSROOT");
			cvsroot_free(root);
			return (NULL);
		}

	}

	root->cr_host = sp;

	if (root->cr_method == CVS_METHOD_NONE) {
		/* no method found from start of CVSROOT, guess */
		if (root->cr_host != NULL)
			root->cr_method = CVS_METHOD_SERVER;
		else
			root->cr_method = CVS_METHOD_LOCAL;
	}

	/* add to the cache */
	tmp = realloc(cvs_rcache, (cvs_rcsz + 1) * sizeof(struct cvsroot *));
	if (tmp == NULL) {
		/* just forget about the cache and return anyways */
		root->cr_ref--;
	} else {
		cvs_rcache = (struct cvsroot **)tmp;
		cvs_rcache[cvs_rcsz++] = root;
	}

	return (root);
}


/*
 * cvsroot_free()
 *
 * Free a CVSROOT structure previously allocated and returned by
 * cvsroot_parse().
 */
void
cvsroot_free(struct cvsroot *root)
{
	root->cr_ref--;
	if (root->cr_ref == 0) {
		if (root->cr_str != NULL)
			free(root->cr_str);
		if (root->cr_buf != NULL)
			free(root->cr_buf);
		if (root->cr_version != NULL)
			free(root->cr_version);
		free(root);
	}
}


/*
 * cvsroot_get()
 *
 * Get the CVSROOT information for a specific directory <dir>.  The
 * value is taken from one of 3 possible sources (in order of precedence):
 *
 * 1) the `-d' command-line option
 * 2) the CVS/Root file found in checked-out trees
 * 3) the CVSROOT environment variable
 */
struct cvsroot*
cvsroot_get(const char *dir)
{
	size_t len;
	char rootpath[MAXPATHLEN], *rootstr, line[128];
	FILE *fp;

	if (cvs_rootstr != NULL)
		return cvsroot_parse(cvs_rootstr);

	snprintf(rootpath, sizeof(rootpath), "%s/" CVS_PATH_ROOTSPEC, dir);
	fp = fopen(rootpath, "r");
	if (fp == NULL) {
		if (errno == ENOENT) {
			/* try env as a last resort */
			if ((rootstr = getenv("CVSROOT")) != NULL)
				return cvsroot_parse(rootstr);
			else
				return (NULL);
		} else {
			cvs_log(LP_ERRNO, "failed to open CVS/Root");
			return (NULL);
		}
	}

	if (fgets(line, sizeof(line), fp) == NULL) {
		cvs_log(LP_ERR, "failed to read CVSROOT line from CVS/Root");
		(void)fclose(fp);
		return (NULL);
	}
	(void)fclose(fp);

	len = strlen(line);
	if (len == 0)
		cvs_log(LP_WARN, "empty CVS/Root file");
	else if (line[len - 1] == '\n')
		line[--len] = '\0';

	return cvsroot_parse(line);
}
