/*	$OpenBSD: policy.c,v 1.18 2002/09/16 04:34:46 itojun Exp $	*/
/*
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
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
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/tree.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <grp.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <err.h>

#include "intercept.h"
#include "systrace.h"

static int psccompare(struct policy_syscall *, struct policy_syscall *);
static int policycompare(struct policy *, struct policy *);
static int polnrcompare(struct policy *, struct policy *);
static char *systrace_policyfilename(char *, const char *);
static int systrace_predicatematch(char *);
static int systrace_writepolicy(struct policy *);

static int
psccompare(struct policy_syscall *a, struct policy_syscall *b)
{
	int diff;
	diff = strcmp(a->emulation, b->emulation);
	if (diff)
		return (diff);
	return (strcmp(a->name, b->name));
}

SPLAY_PROTOTYPE(syscalltree, policy_syscall, node, psccompare)
SPLAY_GENERATE(syscalltree, policy_syscall, node, psccompare)

static SPLAY_HEAD(policytree, policy) policyroot;
static SPLAY_HEAD(polnrtree, policy) polnrroot;

int
policycompare(struct policy *a, struct policy *b)
{
	return (strcmp(a->name, b->name));
}

int
polnrcompare(struct policy *a, struct policy *b)
{
	int diff = a->policynr - b->policynr;

	if (diff == 0)
		return (0);
	if (diff > 0 )
		return (1);
	return (-1);
}

SPLAY_PROTOTYPE(policytree, policy, node, policycompare)
SPLAY_GENERATE(policytree, policy, node, policycompare)

SPLAY_PROTOTYPE(polnrtree, policy, nrnode, polnrcompare)
SPLAY_GENERATE(polnrtree, policy, nrnode, polnrcompare)

extern int userpolicy;

static char policydir[MAXPATHLEN];
static char *groupnames[NGROUPS_MAX];
static int ngroups;

void
systrace_setupdir(char *path)
{
	char *home;
	struct stat sb;

	if (path == NULL) {
		home = getenv("HOME");

		if (home == NULL)
			errx(1, "No HOME environment set");

		if (strlcpy(policydir, home, sizeof(policydir)) >= sizeof(policydir))
			errx(1, "HOME too long");

		if (strlcat(policydir, "/.systrace", sizeof(policydir)) >= sizeof(policydir))
			errx(1, "HOME too long");
	} else if (strlcpy(policydir, path, sizeof(policydir)) >= sizeof(policydir))
		errx(1, "policy directory too long");
		

	if (stat(policydir, &sb) != -1) {
		if (!(sb.st_mode & S_IFDIR))
			errx(1, "Not a directory: \"%s\"", policydir);
	} else if (mkdir(policydir, 0700) == -1)
		err(1, "mdkdir(%s)", policydir);
}

int
systrace_initpolicy(char *file, char *path)
{
	gid_t groups[NGROUPS_MAX];
	char gidbuf[10];
	int i;

	SPLAY_INIT(&policyroot);
	SPLAY_INIT(&polnrroot);

	/* Find out group names for current user */
	if ((ngroups = getgroups(NGROUPS_MAX, groups)) == -1)
		err(1, "getgroups");

	for (i = 0; i < ngroups; i++) {
		struct group *gr;

		if ((gr = getgrgid(groups[i])) != NULL) {
			if ((groupnames[i] = strdup(gr->gr_name)) == NULL)
				err(1, "strdup(%s)", gr->gr_name);
		} else {
			snprintf(gidbuf, sizeof(gidbuf), "%u",
			    groups[i]);
			if ((groupnames[i] = strdup(gidbuf)) == NULL)
				err(1, "strdup(%s)", gidbuf);
		}
	}

	if (userpolicy)
		systrace_setupdir(path);

	if (file != NULL)
		return (systrace_readpolicy(file));

	return (0);
}

struct policy *
systrace_findpolicy(const char *name)
{
	struct policy tmp;

	tmp.name = name;

	return (SPLAY_FIND(policytree, &policyroot, &tmp));
}

struct policy *
systrace_findpolnr(int nr)
{
	struct policy tmp;

	tmp.policynr = nr;

	return (SPLAY_FIND(polnrtree, &polnrroot, &tmp));
}

int
systrace_newpolicynr(int fd, struct policy *tmp)
{
	if (tmp->policynr != -1)
		return (-1);

	if ((tmp->policynr = intercept_newpolicy(fd)) == -1) {
		free(tmp);
		return (-1);
	}

	SPLAY_INSERT(polnrtree, &polnrroot, tmp);

	return (tmp->policynr);
}

struct policy *
systrace_newpolicy(const char *emulation, const char *name)
{
	struct policy *tmp;

	if ((tmp = systrace_findpolicy(name)) != NULL)
		return (tmp);

	tmp = calloc(1, sizeof(struct policy));
	if (tmp == NULL)
		return (NULL);

	tmp->policynr = -1;

	/* New policies requires intialization */
	if ((tmp->name = strdup(name)) == NULL)
		err(1, "%s:%d: strdup", __func__, __LINE__);
	strlcpy(tmp->emulation, emulation, sizeof(tmp->emulation));

	SPLAY_INSERT(policytree, &policyroot, tmp);
	SPLAY_INIT(&tmp->pflqs);
	TAILQ_INIT(&tmp->filters);
	TAILQ_INIT(&tmp->prefilters);

	return (tmp);
}

struct filterq *
systrace_policyflq(struct policy *policy, const char *emulation,
    const char *name)
{
	struct policy_syscall tmp2, *tmp;

	strlcpy(tmp2.emulation, emulation, sizeof(tmp2.emulation));
	strlcpy(tmp2.name, name, sizeof(tmp2.name));

	tmp = SPLAY_FIND(syscalltree, &policy->pflqs, &tmp2);
	if (tmp != NULL)
		return (&tmp->flq);

	if ((tmp = calloc(1, sizeof(struct policy_syscall))) == NULL)
		err(1, "%s:%d: out of memory", __func__, __LINE__);

	strlcpy(tmp->emulation, emulation, sizeof(tmp->emulation));
	strlcpy(tmp->name, name, sizeof(tmp->name));
	TAILQ_INIT(&tmp->flq);

	SPLAY_INSERT(syscalltree, &policy->pflqs, tmp);

	return (&tmp->flq);
}

int
systrace_modifypolicy(int fd, int policynr, const char *name, short action)
{
	struct policy *policy;
	int res;

	if ((policy = systrace_findpolnr(policynr)) == NULL)
		return (-1);

	res = intercept_modifypolicy(fd, policynr, policy->emulation,
	    name, action);

	return (res);
}

char *
systrace_policyfilename(char *dirname, const char *name)
{
	static char file[2*MAXPATHLEN];
	const char *p;
	int i, plen;

	if (strlen(name) + strlen(dirname) + 1 >= sizeof(file))
		return (NULL);

	strlcpy(file, dirname, sizeof(file));
	i = strlen(file);
	file[i++] = '/';
	plen = i;

	p = name;
	while (*p) {
		if (!isalnum(*p)) {
			if (i != plen)
				file[i++] = '_';
		} else
			file[i++] = *p;
		p++;
	}

	file[i] = '\0';

	return (file);
}

int
systrace_addpolicy(const char *name)
{
	char *file = NULL;

	if (userpolicy) {
		file = systrace_policyfilename(policydir, name);
		/* Check if the user policy file exists */
		if (file != NULL && access(file, R_OK) == -1)
			file = NULL;
	}

	/* Read global policy */
	if (file == NULL) {
		file = systrace_policyfilename(POLICY_PATH, name);
		if (file == NULL)
			return (-1);
	}

	return (systrace_readpolicy(file));
}

int
systrace_predicatematch(char *p)
{
	extern char *username;
	int i, res, neg;

	res = 0;
	neg = 0;

	if (!strncasecmp(p, "user", 4)) {
		/* Match against user name */
		p += 4;
		p += strspn(p, " \t");
		if (!strncmp(p, "=", 1)) {
			p += 1;
			neg = 0;
		} else if (!strncmp(p, "!=", 2)) {
			p += 2;
			neg = 1;
		} else
			return (-1);
		p += strspn(p, " \t");

		res = (!strcmp(p, username));
	} else if (!strncasecmp(p, "group", 5)) {
		/* Match against group list */
		p += 5;
		p += strspn(p, " \t");
		if (!strncmp(p, "=", 1)) {
			p += 1;
			neg = 0;
		} else if (!strncmp(p, "!=", 2)) {
			p += 2;
			neg = 1;
		} else
			return (-1);
		p += strspn(p, " \t");

		for (i = 0; i < ngroups; i++) {
			if (!strcmp(p, groupnames[i])) {
				res = 1;
				break;
			}
		}
	} else
		return (-1);

	if (neg)
		res = !res;

	return (res);
}

int
systrace_readpolicy(char *filename)
{
	FILE *fp;
	struct policy *policy;
	char line[_POSIX2_LINE_MAX], *p;
	int linenumber = 0;
	char *name, *emulation, *rule;
	struct filter *filter, *parsed;
	short action, future;
	int res = -1;

	if ((fp = fopen(filename, "r")) == NULL)
		return (-1);

	policy = NULL;
	while (fgets(line, sizeof(line), fp)) {
		linenumber++;
		if ((p = strchr(line, '\n')) == NULL) {
			fprintf(stderr, "%s:%d: input line too long.\n",
			    filename, linenumber);
			goto out;
		}
		*p = '\0';

		p = strchr(line, '#');
		if (p != NULL) {
			if (p != line && *(p-1) == '-')
				p = strchr(p + 1, '#');
			if (p != NULL)
				*p = '\0';
		}

		p = line + strlen(line) - 1;
		while (p > line) {
			if (!isspace(*p))
				break;
			*p-- = '\0';
		}

		p = line;
		p += strspn(p, " \t");
		if (strlen(p) == 0)
			continue;

		if (!strncasecmp(p, "Policy: ", 8)) {
			p += 8;
			name = strsep(&p, ",");
			if (p == NULL)
				goto error;
			if (strncasecmp(p, " Emulation: ", 12))
				goto error;
			p += 12;
			emulation = p;

			policy = systrace_newpolicy(emulation, name);
			if (policy == NULL)
				goto error;
			continue;
		}

		if (policy == NULL)
			goto error;

		if (!strncasecmp(p, "detached", 8)) {
			policy->flags |= POLICY_DETACHED;
			policy = NULL;
			continue;
		}

		emulation = strsep(&p, "-");
		if (p == NULL || *p == '\0')
			goto error;

		if (strcmp(emulation, policy->emulation))
			goto error;

		name = strsep(&p, ":");
		if (p == NULL || *p != ' ')
			goto error;
		p++;
		rule = p;

		if ((p = strrchr(p, ',')) != NULL &&
		    !strncasecmp(p, ", if", 4)) {
			int match;

			*p = '\0';

			/* Process predicates */
			p += 4;
			p += strspn(p, " \t");

			match = systrace_predicatematch(p);
			if (match == -1)
				goto error;
			/* If the predicate does not match skip rule */
			if (!match)
				continue;
		}

		if (filter_parse_simple(rule, &action, &future) == -1) {
			if (parse_filter(rule, &parsed) == -1)
				goto error;
			filter_free(parsed);
		}

		filter = calloc(1, sizeof(struct filter));
		if (filter == NULL)
			err(1, "%s:%d: calloc", __func__, __LINE__);

		filter->rule = strdup(rule);
		if (filter->rule == NULL)
			err(1, "%s:%d: strdup", __func__, __LINE__);

		strlcpy(filter->name, name, sizeof(filter->name));
		strlcpy(filter->emulation,emulation,sizeof(filter->emulation));

		TAILQ_INSERT_TAIL(&policy->prefilters, filter, policy_next);
	}
	res = 0;

 out:
	fclose(fp);
	return (res);

 error:
	fprintf(stderr, "%s:%d: syntax error.\n",
	    filename, linenumber);
	goto out;
}

int
systrace_writepolicy(struct policy *policy)
{
	FILE *fp;
	int fd;
	char *p;
	char tmpname[2*MAXPATHLEN];
	char finalname[2*MAXPATHLEN];
	struct filter *filter;

	if ((p = systrace_policyfilename(policydir, policy->name)) == NULL)
		return (-1);
	strlcpy(finalname, p, sizeof(finalname));
	if ((p = systrace_policyfilename(policydir, "tmpXXXXXXXX")) == NULL)
		return (-1);
	strlcpy(tmpname, p, sizeof(tmpname));
	if ((fd = mkstemp(tmpname)) == -1 ||
	    (fp = fdopen(fd, "w+")) == NULL) {
		if (fd != -1) {
			unlink(tmpname);
			close(fd);
		}
		return (-1);
	}


	fprintf(fp, "Policy: %s, Emulation: %s\n",
	    policy->name, policy->emulation);
	if (policy->flags & POLICY_DETACHED) {
		fprintf(fp, "detached\n");
	} else {
		TAILQ_FOREACH(filter, &policy->prefilters, policy_next) {
			fprintf(fp, "\t%s-%s: %s\n",
			    filter->emulation, filter->name, filter->rule);
		}
		TAILQ_FOREACH(filter, &policy->filters, policy_next) {
			fprintf(fp, "\t%s-%s: %s\n",
			    filter->emulation, filter->name, filter->rule);
		}
	}
	fprintf(fp, "\n");
	fclose(fp);

	if (rename(tmpname, finalname) == -1) {
		warn("rename(%s, %s)", tmpname, finalname);
		return (-1);
	}

	return (0);
}

int
systrace_dumppolicy(void)
{
	struct policy *policy;

	SPLAY_FOREACH(policy, policytree, &policyroot) {
		if (!(policy->flags & POLICY_CHANGED))
			continue;

		if (systrace_writepolicy(policy) == -1)
			fprintf(stderr, "Failed to write policy for %s\n",
			    policy->name);
		else
			policy->flags &= ~POLICY_CHANGED;
	}

	return (0);
}
