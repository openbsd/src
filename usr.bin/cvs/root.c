/*	$OpenBSD: root.c,v 1.26 2005/12/10 20:27:45 joris Exp $	*/
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

#include <err.h>
#include <errno.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

#define CVS_NBMETHODS	(sizeof(cvs_methods)/sizeof(cvs_methods[0]))

/*
 * CVSROOT cache
 *
 * Whenever cvsroot_parse() gets called for a specific string, it first
 * checks in the cache to see if there is already a parsed version of the
 * same string and returns a pointer to it in case one is found (it also
 * increases the reference count).  Otherwise, it does the parsing and adds
 * the result to the cache for future hits.
 */
static TAILQ_HEAD(, cvsroot) cvs_rcache = TAILQ_HEAD_INITIALIZER(cvs_rcache);
static void cvsroot_free(struct cvsroot *);

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
struct cvsroot *
cvsroot_parse(const char *str)
{
	u_int i;
	char *cp, *sp, *pp;
	struct cvsroot *root;

	/*
	 * Look if we have it in cache, if we found it add it to the cache
	 * at the first position again.
	 */
	TAILQ_FOREACH(root, &cvs_rcache, root_cache) {
		if (strcmp(str, root->cr_str) == 0) {
			TAILQ_REMOVE(&cvs_rcache, root, root_cache);
			TAILQ_INSERT_HEAD(&cvs_rcache, root, root_cache);
			root->cr_ref++;
			return (root);
		}
	}

	root = (struct cvsroot *)xmalloc(sizeof(*root));
	memset(root, 0, sizeof(*root));
	root->cr_ref = 1;
	root->cr_method = CVS_METHOD_NONE;
	CVS_RSTVR(root);

	/* enable the most basic commands at least */
	CVS_SETVR(root, CVS_REQ_VALIDREQ);
	CVS_SETVR(root, CVS_REQ_VALIDRESP);

	root->cr_str = xstrdup(str);
	root->cr_buf = xstrdup(str);

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
		TAILQ_INSERT_HEAD(&cvs_rcache, root, root_cache);
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
	TAILQ_INSERT_HEAD(&cvs_rcache, root, root_cache);
	return (root);
}

/*
 * cvsroot_remove()
 *
 * Remove a CVSROOT structure from the cache, and free it.
 */
void
cvsroot_remove(struct cvsroot *root)
{
	root->cr_ref--;
	if (root->cr_ref == 0) {
		TAILQ_REMOVE(&cvs_rcache, root, root_cache);
		cvsroot_free(root);
	}
}

/*
 * cvsroot_free()
 *
 * Free a CVSROOT structure previously allocated and returned by
 * cvsroot_parse().
 */
static void
cvsroot_free(struct cvsroot *root)
{
	if (root->cr_str != NULL)
		xfree(root->cr_str);
	if (root->cr_buf != NULL)
		xfree(root->cr_buf);
	if (root->cr_version != NULL)
		xfree(root->cr_version);
	xfree(root);
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
struct cvsroot *
cvsroot_get(const char *dir)
{
	int l;
	size_t len;
	char rootpath[MAXPATHLEN], *rootstr, line[128];
	FILE *fp;

	if (cvs_rootstr != NULL)
		return cvsroot_parse(cvs_rootstr);

	l = snprintf(rootpath, sizeof(rootpath), "%s/" CVS_PATH_ROOTSPEC, dir);
	if (l == -1 || l >= (int)sizeof(rootpath)) {
		errno = ENAMETOOLONG;
		cvs_log(LP_ERRNO, "%s", rootpath);
		return (NULL);
	}

	fp = fopen(rootpath, "r");
	if (fp == NULL) {
		if (errno == ENOENT) {
			/* try env as a last resort */
			if ((rootstr = getenv("CVSROOT")) != NULL)
				return cvsroot_parse(rootstr);
			else
				return (NULL);
		} else {
			cvs_log(LP_ERRNO, "failed to open %s",
			    CVS_PATH_ROOTSPEC);
			return (NULL);
		}
	}

	if (fgets(line, (int)sizeof(line), fp) == NULL) {
		cvs_log(LP_ERR, "failed to read line from %s",
		    CVS_PATH_ROOTSPEC);
		(void)fclose(fp);
		return (NULL);
	}
	(void)fclose(fp);

	len = strlen(line);
	if (len == 0)
		cvs_log(LP_WARN, "empty %s file", CVS_PATH_ROOTSPEC);
	else if (line[len - 1] == '\n')
		line[--len] = '\0';

	return cvsroot_parse(line);
}
