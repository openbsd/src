/*	$OpenBSD: root.c,v 1.38 2007/05/11 06:32:02 xsa Exp $	*/
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "cvs.h"

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
 *	[:method:][[user[:pass]@]host[:port]]:path
 * Returns a pointer to the allocated information on success, or NULL
 * on failure.
 */
struct cvsroot *
cvsroot_parse(const char *str)
{
	u_int i;
	char *cp, *sp, *pp;
	const char *errstr;
	struct cvsroot *root;

	/*
	 * Look if we have it in cache, if we found it add it to the cache
	 * at the first position again.
	 */
	TAILQ_FOREACH(root, &cvs_rcache, root_cache) {
		if (root->cr_str != NULL && strcmp(str, root->cr_str) == 0) {
			TAILQ_REMOVE(&cvs_rcache, root, root_cache);
			TAILQ_INSERT_HEAD(&cvs_rcache, root, root_cache);
			root->cr_ref++;
			return (root);
		}
	}

	root = xcalloc(1, sizeof(*root));
	root->cr_ref = 1;
	root->cr_method = CVS_METHOD_NONE;
	CVS_RSTVR(root);

	root->cr_str = xstrdup(str);
	root->cr_buf = xstrdup(str);

	sp = root->cr_buf;
	cp = root->cr_buf;
	if (*sp == ':') {
		sp++;
		if ((cp = strchr(sp, ':')) == NULL)
			fatal("failed to parse CVSROOT: unterminated method");

		*(cp++) = '\0';

		for (i = 0; i < CVS_NBMETHODS; i++) {
			if (strcmp(sp, cvs_methods[i]) == 0) {
				root->cr_method = i;
				break;
			}
		}
		if (i == CVS_NBMETHODS)
			fatal("cvsroot_parse: unknown method `%s'", sp);
	}

	/* find the start of the actual path */
	if ((sp = strchr(cp, '/')) == NULL)
		fatal("no path specification in CVSROOT");

	root->cr_dir = sp;
	STRIP_SLASH(root->cr_dir);
	if (sp == cp) {
		if (root->cr_method == CVS_METHOD_NONE)
			root->cr_method = CVS_METHOD_LOCAL;
		/* stop here, it's just a path */
		TAILQ_INSERT_HEAD(&cvs_rcache, root, root_cache);
		return (root);
	}

	if (*(sp - 1) != ':')
		fatal("missing host/path delimiter in CVSROOT");

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
		root->cr_port = strtonum(pp, 1, 65535, &errstr);
		if (errstr != NULL)
			fatal("port specification in CVSROOT is %s", errstr);

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
	size_t len;
	char rootpath[MAXPATHLEN], *rootstr, line[128];
	FILE *fp;

	if (cvs_rootstr != NULL)
		return cvsroot_parse(cvs_rootstr);

	(void)xsnprintf(rootpath, MAXPATHLEN, "%s/%s", dir, CVS_PATH_ROOTSPEC);

	if ((fp = fopen(rootpath, "r")) == NULL) {
		if (errno == ENOENT) {
			/* try env as a last resort */
			if ((rootstr = getenv("CVSROOT")) != NULL)
				return cvsroot_parse(rootstr);
			else
				return (NULL);
		} else {
			fatal("cvsroot_get: fopen: `%s': %s",
			    CVS_PATH_ROOTSPEC, strerror(errno));
		}
	}

	if (fgets(line, (int)sizeof(line), fp) == NULL)
		fatal("cvsroot_get: fgets: `%s'", CVS_PATH_ROOTSPEC);

	(void)fclose(fp);

	len = strlen(line);
	if (len == 0)
		cvs_log(LP_ERR, "empty %s file", CVS_PATH_ROOTSPEC);
	else if (line[len - 1] == '\n')
		line[--len] = '\0';

	return cvsroot_parse(line);
}
