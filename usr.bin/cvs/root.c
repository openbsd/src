/*	$OpenBSD: root.c,v 1.44 2007/10/05 19:28:23 gilles Exp $	*/
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
 * cvsroot_parse()
 *
 * Parse a CVS root string (as found in CVS/Root files or the CVSROOT
 * environment variable) and store the fields in a dynamically
 * allocated cvs_root structure.  The format of the string is as follows:
 *	[:method:][[user[:pass]@]host[:port]:]path
 * Returns a pointer to the allocated information on success, or NULL
 * on failure.
 */
static struct cvsroot *
cvsroot_parse(const char *str)
{
	u_int i;
	char *cp, *sp, *pp;
	const char *errstr;
	static struct cvsroot *root = NULL;

	if (root != NULL)
		return (root);

	root = xcalloc(1, sizeof(*root));
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

	return (root);
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
	char rootpath[MAXPATHLEN], *rootstr, line[128];
	FILE *fp;

	if (cvs_rootstr != NULL)
		return cvsroot_parse(cvs_rootstr);

	if (cvs_server_active == 1)
		return cvsroot_parse(dir);

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

	line[strcspn(line, "\n")] = '\0';
	if (line[0] == '\0')
		cvs_log(LP_ERR, "empty %s file", CVS_PATH_ROOTSPEC);

	return cvsroot_parse(line);
}
