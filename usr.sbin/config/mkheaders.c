/*	$OpenBSD: mkheaders.c,v 1.7 1997/08/07 10:22:26 downsj Exp $	*/
/*	$NetBSD: mkheaders.c,v 1.15 1997/07/18 11:27:37 jtk Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratories.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)mkheaders.c	8.1 (Berkeley) 6/6/93
 */

#include <sys/param.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

static int emitcnt __P((struct nvlist *));
static int emitlocs __P((void));
static int emitopt __P((struct nvlist *));
static int err __P((const char *, char *, FILE *));
static int locators_print __P((const char *, void *, void *));
static char *cntname __P((const char *));


/*
 * Make headers containing counts, as needed.
 */
int
mkheaders()
{
	register struct files *fi;
	register struct nvlist *nv;

	for (fi = allfiles; fi != NULL; fi = fi->fi_next) {
		if (fi->fi_flags & FI_HIDDEN)
			continue;
		if (fi->fi_flags & (FI_NEEDSCOUNT | FI_NEEDSFLAG) &&
		    emitcnt(fi->fi_optf))
			return (1);
	}

	for (nv = defoptions; nv != NULL; nv = nv->nv_next)
		if (emitopt(nv))
			return (1);

	if (emitlocs())
		return (1);

	return (0);
}

static int
emitcnt(head)
	register struct nvlist *head;
{
	register struct nvlist *nv;
	register FILE *fp;
	int cnt;
	char nam[100];
	char buf[BUFSIZ];
	char fname[BUFSIZ];

	(void)sprintf(fname, "%s.h", head->nv_name);
	if ((fp = fopen(fname, "r")) == NULL)
		goto writeit;
	nv = head;
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		if (nv == NULL)
			goto writeit;
		if (sscanf(buf, "#define %s %d", nam, &cnt) != 2 ||
		    strcmp(nam, cntname(nv->nv_name)) != 0 ||
		    cnt != nv->nv_int)
			goto writeit;
		nv = nv->nv_next;
	}
	if (ferror(fp))
		return (err("read", fname, fp));
	(void)fclose(fp);
	if (nv == NULL)
		return (0);
writeit:
	if ((fp = fopen(fname, "w")) == NULL) {
		(void)fprintf(stderr, "config: cannot write %s: %s\n",
		    fname, strerror(errno));
		return (1);
	}
	for (nv = head; nv != NULL; nv = nv->nv_next)
		if (fprintf(fp, "#define\t%s\t%d\n",
		    cntname(nv->nv_name), nv->nv_int) < 0)
			return (err("writ", fname, fp));
	if (fclose(fp))
		return (err("writ", fname, NULL));
	return (0);
}

static int
emitopt(nv)
	struct nvlist *nv;
{
	struct nvlist *option;
	char new_contents[BUFSIZ], buf[BUFSIZ];
	char fname[BUFSIZ], *p;
	int nlines;
	FILE *fp;

	/*
	 * Generate the new contents of the file.
	 */
	p = new_contents;
	if ((option = ht_lookup(opttab, nv->nv_str)) == NULL)
		p += sprintf(p, "/* option `%s' not defined */\n",
		    nv->nv_str);
	else {
		p += sprintf(p, "#define\t%s", option->nv_name);
		if (option->nv_str != NULL)
			p += sprintf(p, "\t%s", option->nv_str);
		p += sprintf(p, "\n");
	}

	/*
	 * Compare the new file to the old.
	 */
	sprintf(fname, "opt_%s.h", nv->nv_name);
	if ((fp = fopen(fname, "r")) == NULL)
		goto writeit;
	nlines = 0;
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		if (++nlines != 1 ||
		    strcmp(buf, new_contents) != 0)
			goto writeit;
	}
	if (ferror(fp))
		return (err("read", fname, fp));
	(void)fclose(fp);
	if (nlines == 1)
		return (0);
writeit:
	/*
	 * They're different, or the file doesn't exist.
	 */
	if ((fp = fopen(fname, "w")) == NULL) {
		(void)fprintf(stderr, "config: cannot write %s: %s\n",
		    fname, strerror(errno));
		return (1);
	}
	if (fprintf(fp, "%s", new_contents) < 0)
		return (err("writ", fname, fp));
	if (fclose(fp))
		return (err("writ", fname, fp));
	return (0);
}

/*
 * A callback function for walking the attribute hash table.
 * Emit CPP definitions of manifest constants for the locators on the
 * "name" attribute node (passed as the "value" parameter).
 */
static int
locators_print(name, value, arg)
	const char *name;
	void *value;
	void *arg;
{
	struct attr *a;
	register struct nvlist *nv;
	register int i;
	char *locdup, *namedup;
	register char *cp;
	FILE *fp = arg;

	a = value;
	if (a->a_locs) {
		if (strchr(name, ' ') != NULL || strchr(name, '\t') != NULL)
			/*
			 * name contains a space; we can't generate
			 * usable defines, so ignore it.
			 */
			return 0;
		locdup = strdup(name);
		for (cp = locdup; *cp; cp++)
			if (islower(*cp))
				*cp = toupper(*cp);
		if (fprintf(fp, "extern const char *%scf_locnames[];\n",
			    name) < 0)
			return 1;
		for (i = 0, nv = a->a_locs; nv; nv = nv->nv_next, i++) {
			if (strchr(nv->nv_name, ' ') != NULL ||
			    strchr(nv->nv_name, '\t') != NULL)
				/*
				 * name contains a space; we can't generate
				 * usable defines, so ignore it.
				 */
				continue;
			namedup = strdup(nv->nv_name);
			for (cp = namedup; *cp; cp++)
				if (islower(*cp))
					*cp = toupper(*cp);
			if (fprintf(fp, "#define %sCF_%s %d\n",
				    locdup, namedup, i) < 0)
				return 1;
			if (nv->nv_str &&
			    fprintf(fp,
				    "#define %sCF_%s_DEFAULT %s\n",
				    locdup, namedup, nv->nv_str) < 0)
				return 1;
			free(namedup);
		}
		free(locdup);
	}
	return 0;
}

/*
 * Build the "locators.h" file with manifest constants for all potential
 * locators in the configuration.  Do this by enumerating the attribute
 * hash table and emitting all the locators for each attribute.
 */
static int
emitlocs()
{
	struct nvlist *option;
	char nbuf[BUFSIZ], obuf[BUFSIZ];
	char *tfname, *nfname;
	const char *n;
	int count, rval;
	FILE *tfp = NULL, *nfp = NULL;
	
	tfname = "tmp_locators.h";
	if ((tfp = fopen(tfname, "w")) == NULL) {
		(void)fprintf(stderr, "config: cannot write %s: %s\n",
		    tfname, strerror(errno));
		return (1);
	}

	rval = ht_enumerate(attrtab, locators_print, tfp);
	if (fclose(tfp) == EOF)
		return(err("clos", tfname, NULL));

	if ((tfp = fopen(tfname, "r")) == NULL)
		goto moveit;

	/*
	 * Compare the new file to the old.
	 */
	nfname = "locators.h";
	if ((nfp = fopen(nfname, "r")) == NULL)
		goto moveit;

	while (fgets(obuf, sizeof(obuf), tfp) != NULL) {
		if (fgets(nbuf, sizeof(nbuf), nfp) == NULL)
			goto moveit;

		if (strcmp(obuf, nbuf) != 0)
			goto moveit;
	}
	(void) fclose(nfp);
	(void) fclose(tfp);
	if (remove(tfname) == -1)
		return(err("remov", tfname, NULL));
	return (0);

moveit:
	/*
	 * They're different, or the file doesn't exist.
	 */
	if (nfp)
		(void) fclose(nfp);
	if (tfp)
		(void) fclose(tfp);
	if (rename(tfname, nfname) == -1)
		return(err("renam", tfname, NULL));
	return (0);
}

static int
err(what, fname, fp)
	const char *what;
	char *fname;
	FILE *fp;
{

	(void)fprintf(stderr, "config: error %sing %s: %s\n",
	    what, fname, strerror(errno));
	if (fp)
		(void)fclose(fp);
	return (1);
}

static char *
cntname(src)
	register const char *src;
{
	register char *dst, c;
	static char buf[100];

	dst = buf;
	*dst++ = 'N';
	while ((c = *src++) != 0)
		*dst++ = islower(c) ? toupper(c) : c;
	*dst = 0;
	return (buf);
}
