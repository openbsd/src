/*	$OpenBSD: filter.c,v 1.24 2002/12/09 07:24:56 itojun Exp $	*/
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/tree.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <regex.h>
#include <fnmatch.h>
#include <err.h>

#include "intercept.h"
#include "systrace.h"
#include "filter.h"

extern int allow;
extern int noalias;
extern int connected;
extern char cwd[];
extern char home[];
extern char username[];

static void logic_free(struct logic *);
static int filter_match(struct intercept_pid *, struct intercept_tlq *,
    struct logic *);
static void filter_review(struct filterq *);
static void filter_templates(const char *);
static int filter_template(int, struct policy *, int);
static int filter_quickpredicate(struct filter *);
static void filter_policyrecord(struct policy *, struct filter *, const char *,
    const char *, char *);
static void filter_replace(char *, size_t, char *, char *);

static int
filter_match(struct intercept_pid *icpid, struct intercept_tlq *tls,
    struct logic *logic)
{
	struct intercept_translate *tl;
	int off = 0, res;

	switch (logic->op) {
	case LOGIC_NOT:
		return (!filter_match(icpid, tls, logic->left));
	case LOGIC_OR:
		if (filter_match(icpid, tls, logic->left))
			return (1);
		return (filter_match(icpid, tls, logic->right));
	case LOGIC_AND:
		if (!filter_match(icpid, tls, logic->left))
			return (0);
		return (filter_match(icpid, tls, logic->right));
	default:
		break;
	}

	/* Now we just have a logic single */
	if (logic->type == NULL)
		goto match;

	if (tls == NULL)
		errx(1, "filter_match has no translators");

	TAILQ_FOREACH(tl, tls, next) {
		if (!tl->trans_valid)
			continue;

		if (strcasecmp(tl->name, logic->type))
			continue;

		if (logic->typeoff == -1 || logic->typeoff == off)
			break;

		off++;
	}

	if (tl == NULL)
		return (0);

 match:
	/* We need to do dynamic expansion on the data */
	if (logic->filterdata && (logic->flags & LOGIC_NEEDEXPAND)) {
		char *old = logic->filterdata;
		size_t oldlen = logic->filterlen;

		logic->filterdata = filter_dynamicexpand(icpid, old);
		logic->filterlen = strlen(logic->filterdata) + 1;

		res = logic->filter_match(tl, logic);

		logic->filterdata = old;
		logic->filterlen = oldlen;
	} else
		res = logic->filter_match(tl, logic);

	return (res);
}

/* Evaluate filter predicate */

int
filter_predicate(struct intercept_pid *icpid, struct predicate *pdc)
{
	int negative;
	int res = 0;

	if (!pdc->p_flags)
		return (1);

	negative = pdc->p_flags & PREDIC_NEGATIVE;
	if (pdc->p_flags & PREDIC_UID)
		res = icpid->uid == pdc->p_uid;
	else if (pdc->p_flags & PREDIC_GID)
		res = icpid->gid == pdc->p_gid;

	return (negative ? !res : res);
}

short
filter_evaluate(struct intercept_tlq *tls, struct filterq *fls,
    struct intercept_pid *icpid)
{
	struct filter *filter, *last = NULL;
	short action, laction = 0;

	TAILQ_FOREACH(filter, fls, next) {
		action = filter->match_action;

		if (filter_predicate(icpid, &filter->match_predicate) &&
		    filter_match(icpid, tls, filter->logicroot)) {
			/* Profile feedback optimization */
			filter->match_count++;
			if (last != NULL && last->match_action == action &&
			    filter->match_count > last->match_count) {
				TAILQ_REMOVE(fls, last, next);
				TAILQ_INSERT_AFTER(fls, filter, last, next);
			}

			if (action == ICPOLICY_NEVER)
				action = filter->match_error;
			icpid->uflags = filter->match_flags;

			/* Policy requests privilege elevation */
			if (filter->elevate.e_flags)
				icpid->elevate = &filter->elevate;
			return (action);
		}

		/* Keep track of last processed filtered in a group */
		last = filter;
		laction = action;
	}

	return (ICPOLICY_ASK);
}

static void
logic_free(struct logic *logic)
{
	if (logic->left)
		logic_free(logic->left);
	if (logic->right)
		logic_free(logic->right);
	if (logic->type)
		free(logic->type);
	if (logic->filterdata)
		free(logic->filterdata);
	free(logic);
}

void
filter_free(struct filter *filter)
{
	if (filter->logicroot)
		logic_free(filter->logicroot);
	if (filter->rule)
		free(filter->rule);
	free(filter);
}

static void
filter_review(struct filterq *fls)
{
	struct filter *filter;
	int i = 0;

	printf("Filter review:\n");

	TAILQ_FOREACH(filter, fls, next) {
		i++;
		printf("%d. %s\n", i, filter->rule);
	}
}

static void
filter_templates(const char *emulation)
{
	extern struct tmplqueue templates;
	struct template *template;
	int i = 0;

	printf("Available Templates:\n");

	TAILQ_FOREACH(template, &templates, next) {
		if (strcmp(template->emulation, emulation))
			continue;

		i++;
		printf("%d. %s - %s\n", i,
		    template->name, template->description);
	}
}

/* Inserts a policy from a template */

static int
filter_template(int fd, struct policy *policy, int count)
{
	extern struct tmplqueue templates;
	struct template *template;
	int i = 0;

	TAILQ_FOREACH(template, &templates, next) {
		if (strcmp(template->emulation, policy->emulation))
			continue;

		i++;
		if (i == count)
			break;
	}

	if (i != count)
		return (-1);

	template = systrace_readtemplate(template->filename, policy, template);
	if (template == NULL)
		return (-1);

	if (filter_prepolicy(fd, policy) == -1)
		return (-1);

	/* We inserted new statements into the policy */
	policy->flags |= POLICY_CHANGED;

	return (0);
}

static void
filter_policyrecord(struct policy *policy, struct filter *filter,
    const char *emulation, const char *name, char *rule)
{
	/* Record the filter in the policy */
	if (filter == NULL) {
		filter = calloc(1, sizeof(struct filter));
		if (filter == NULL)
			err(1, "%s:%d: calloc", __func__, __LINE__);
		if ((filter->rule = strdup(rule)) == NULL)
			err(1, "%s:%d: strdup", __func__, __LINE__);
	}

	strlcpy(filter->name, name, sizeof(filter->name));
	strlcpy(filter->emulation, emulation, sizeof(filter->emulation));

	TAILQ_INSERT_TAIL(&policy->filters, filter, policy_next);
	policy->nfilters++;

	policy->flags |= POLICY_CHANGED;
}

int
filter_parse(char *line, struct filter **pfilter)
{
	char *rule;

	if (parse_filter(line, pfilter) == -1)
		return (-1);

	if ((rule = strdup(line)) == NULL)
		err(1, "%s:%d: strdup", __func__, __LINE__);

	(*pfilter)->rule = rule;

	return (0);
}

/* Translate a simple action like "permit" or "deny[einval]" to numbers */

int
filter_parse_simple(char *rule, short *paction, short *pfuture)
{
	char buf[_POSIX2_LINE_MAX];
	int isfuture = 1;
	char *line, *p;

	if (strlcpy(buf, rule, sizeof(buf)) >= sizeof(buf))
		return (-1);

	line = buf;

	if (!strcmp("permit", line)) {
		*paction = *pfuture = ICPOLICY_PERMIT;
		return (0);
	} else if (!strcmp("permit-now", line)) {
		*paction = ICPOLICY_PERMIT;
		return (0);
	} else if (strncmp("deny", line, 4))
		return (-1);

	line +=4 ;
	if (!strncmp("-now", line, 4)) {
		line += 4;
		isfuture = 0;
	}

	*paction = ICPOLICY_NEVER;

	switch (line[0]) {
	case '\0':
		break;
	case '[':
		line++;
		p = strsep(&line, "]");
		if (line == NULL || *line != '\0')
			return (-1);

		*paction = systrace_error_translate(p);
		if (*paction == -1)
			return (-1);
		break;
	default:
		return (-1);
	}

	if (isfuture)
		*pfuture = *paction;

	return (0);
}

void
filter_modifypolicy(int fd, int policynr, const char *emulation,
    const char *name, short future)
{
	struct systrace_revalias *reverse = NULL;

	if (!noalias)
		reverse = systrace_find_reverse(emulation, name);
	if (reverse == NULL) {
		if (systrace_modifypolicy(fd, policynr, name, future) == -1)
			errx(1, "%s:%d: modify policy for %s-%s",
			    __func__, __LINE__, emulation, name);
	} else {
		struct systrace_alias *alias; 

		/* For every system call associated with this alias
		 * set the permanent in-kernel policy.
		 */
		TAILQ_FOREACH(alias, &reverse->revl, next) {
			if(systrace_modifypolicy(fd, policynr,
			       alias->name, future) == -1)
				errx(1, "%s:%d: modify policy for %s-%s",
				    __func__, __LINE__,
				    emulation, alias->name);
		}
	}
}

/* In non-root case, evaluate predicates early */ 

static int
filter_quickpredicate(struct filter *filter)
{
	struct predicate *pdc;
	struct intercept_pid icpid;

	pdc = &filter->match_predicate;
	if (!pdc->p_flags)
		return (1);

	intercept_setpid(&icpid, getuid(), getgid());

	if (!filter_predicate(&icpid, pdc))
		return (0);

	memset(pdc, 0, sizeof(filter->match_predicate));

	return (1);
}

int
filter_prepolicy(int fd, struct policy *policy)
{
	int res;
	struct filter *filter, *parsed;
	struct filterq *fls;
	short action, future;
	extern int iamroot;

	/* Commit all matching pre-filters */
	for (filter = TAILQ_FIRST(&policy->prefilters);
	    filter; filter = TAILQ_FIRST(&policy->prefilters)) {
		future = ICPOLICY_ASK;

		TAILQ_REMOVE(&policy->prefilters, filter, policy_next);

		res = 0;
		parsed = NULL;
		/* Special rules that are not real filters */
		if (filter_parse_simple(filter->rule, &action, &future) == -1)
			res = filter_parse(filter->rule, &parsed);
		if (res == -1)
			errx(1, "%s:%d: can not parse \"%s\"",
			    __func__, __LINE__, filter->rule);

		if (future == ICPOLICY_ASK) {
			if (iamroot || filter_quickpredicate(parsed)) {
				fls = systrace_policyflq(policy,
				    policy->emulation, filter->name);
				TAILQ_INSERT_TAIL(fls, parsed, next);
			}
		} else {
			filter_modifypolicy(fd, policy->policynr,
			    policy->emulation, filter->name, future);
		}
		filter_policyrecord(policy, parsed, policy->emulation,
		    filter->name, filter->rule);

		filter_free(filter);
	}

	/* Existing policy applied undo changed flag */
	policy->flags &= ~POLICY_CHANGED;

	return (0);
}

short
filter_ask(int fd, struct intercept_tlq *tls, struct filterq *fls,
    int policynr, const char *emulation, const char *name,
    char *output, short *pfuture, struct intercept_pid *icpid)
{
	char line[2*MAXPATHLEN], *p;
	char compose[2*MAXPATHLEN];
	struct filter *filter;
	struct policy *policy;
	short action;
	int first = 1, isalias;

	*pfuture = ICPOLICY_ASK;
	icpid->uflags = 0;

	isalias = systrace_find_reverse(emulation, name) != NULL;

	if ((policy = systrace_findpolnr(policynr)) == NULL)
		errx(1, "%s:%d: no policy %d", __func__, __LINE__, policynr);

	if (!allow)
		printf("%s\n", output);
	else {
		/* Automatically allow */
		if (tls != NULL) {
			struct intercept_translate *tl;
			char *l, *lst = NULL;
			int set = 0;

			/* Explicitly match every component */
			line[0] = '\0';
			TAILQ_FOREACH(tl, tls, next) {
				if (!tl->trans_valid)
					continue;
				l = intercept_translate_print(tl);
				if (l == NULL)
					continue;

				snprintf(compose, sizeof(compose),
				    "%s%s eq \"%s\"",
				    tl->name,
				    lst && !strcmp(tl->name, lst) ? "[1]" : "",
				    l);

				lst = tl->name;

				if (set)
					strlcat(line, " and ",
					    sizeof(line));
				else
					set = 1;
				strlcat(line, compose, sizeof(line));
			}
			if (!set)
				strlcpy(line, "true", sizeof(line));
			strlcat(line, " then permit", sizeof(line));
		} else
			strlcpy(line, "permit", sizeof(line));
	}

	while (1) {
		filter = NULL;

		if (!allow) {
			/* Ask for a policy */
			if (!connected)
				printf("Answer: ");
			else {
				/* Do not prompt the first time */
				if (!first) {
					printf("WRONG\n");
				}
			}

			fgets(line, sizeof(line), stdin);
			p = line;
			strsep(&p, "\n");
		} else if (!first) {
			/* Error with filter */
			errx(1, "Filter generation error: %s", line);
		}
		first = 0;

		/* Simple keywords */
		if (!strcasecmp(line, "detach")) {
			if (policy->nfilters) {
				policy->flags |= POLICY_UNSUPERVISED;
				action = ICPOLICY_NEVER;
			} else {
				policy->flags |= POLICY_DETACHED;
				policy->flags |= POLICY_CHANGED;
				action = ICPOLICY_PERMIT;
			}
			goto out;
		} else if (!strcasecmp(line, "kill")) {
			action = ICPOLICY_KILL;
			goto out;
		} else if (!strcasecmp(line, "review") && fls != NULL) {
			filter_review(fls);
			continue;
		} else if (!strcasecmp(line, "templates")) {
			filter_templates(emulation);
			continue;
		} else if (!strncasecmp(line, "template ", 9)) {
			int count = atoi(line + 9);

			if (count == 0 ||
			    filter_template(fd, policy, count) == -1) {
				printf("Syntax error.\n");
				continue;
			}

			if (fls != NULL)
				action = filter_evaluate(tls, fls, icpid);
			else
				action = ICPOLICY_PERMIT;
			if (action == ICPOLICY_ASK) {
				printf("Filter unmatched.\n");
				continue;
			}

			goto out;
		}

		if (filter_parse_simple(line, &action, pfuture) != -1) {
			if (*pfuture == ICPOLICY_ASK)
				goto out;
			/* We have a policy decision */
			if (!isalias)
				break;

			/* No in-kernel policy for aliases */
			strlcpy(compose, line, sizeof(compose));
			
			/* Change into userland rule */
			snprintf(line, sizeof(line), "true then %s", compose);
		}

		if (fls == NULL) {
			printf("Syntax error.\n");
			continue;
		}

		if (filter_parse(line, &filter) == -1)
			continue;

		TAILQ_INSERT_TAIL(fls, filter, next);
		action = filter_evaluate(tls, fls, icpid);
		if (action == ICPOLICY_ASK) {
			TAILQ_REMOVE(fls, filter, next);
			printf("Filter unmatched. Freeing it\n");
			filter_free(filter);
			continue;
		}

		break;
	}

	filter_policyrecord(policy, filter, emulation, name, line);

 out:
	if (connected)
		printf("OKAY\n");
	return (action);

}

static void
filter_replace(char *buf, size_t buflen, char *match, char *repl)
{
	while (strrpl(buf, buflen, match, repl) != NULL)
		;
}

char *
filter_expand(char *data)
{
	static char expand[2*MAXPATHLEN];

	strlcpy(expand, data, sizeof(expand));

	filter_replace(expand, sizeof(expand), "$HOME", home);
	filter_replace(expand, sizeof(expand), "$USER", username);
	filter_replace(expand, sizeof(expand), "$CWD", cwd);

	return (expand);
}

char *
filter_dynamicexpand(struct intercept_pid *icpid, char *data)
{
	static char expand[2*MAXPATHLEN];

	strlcpy(expand, data, sizeof(expand));

	filter_replace(expand, sizeof(expand), "$HOME", icpid->home);
	filter_replace(expand, sizeof(expand), "$USER", icpid->username);
	filter_replace(expand, sizeof(expand), "$CWD", icpid->cwd);

	return (expand);
}

/* Checks if the string needs expansion */

int
filter_needexpand(char *data)
{
	if (strstr(data, "$HOME") != NULL)
		return (1);
	if (strstr(data, "$USER") != NULL)
		return (1);
	if (strstr(data, "$CWD") != NULL)
		return (1);

	return (0);
}

int
filter_fnmatch(struct intercept_translate *tl, struct logic *logic)
{
	int res;
	char *line;

	if ((line = intercept_translate_print(tl)) == NULL)
		return (0);
	res = fnmatch(logic->filterdata, line, FNM_PATHNAME | FNM_LEADING_DIR);

	return (res == 0);
}

int
filter_substrmatch(struct intercept_translate *tl, struct logic *logic)
{
	char *line;

	if ((line = intercept_translate_print(tl)) == NULL)
		return (0);

	return (strstr(line, logic->filterdata) != NULL);
}

int
filter_negsubstrmatch(struct intercept_translate *tl, struct logic *logic)
{
	char *line;

	if ((line = intercept_translate_print(tl)) == NULL)
		return (0);

	return (strstr(line, logic->filterdata) == NULL);
}

int
filter_stringmatch(struct intercept_translate *tl, struct logic *logic)
{
	char *line;

	if ((line = intercept_translate_print(tl)) == NULL)
		return (0);

	return (!strcasecmp(line, logic->filterdata));
}

int
filter_negstringmatch(struct intercept_translate *tl, struct logic *logic)
{
	char *line;

	if ((line = intercept_translate_print(tl)) == NULL)
		return (1);

	return (strcasecmp(line, logic->filterdata) != 0);
}

int
filter_inpath(struct intercept_translate *tl, struct logic *logic)
{
	char *line, c;
	int len;

	if ((line = intercept_translate_print(tl)) == NULL)
		return (0);

	len = strlen(line);
	if (len == 0 || len > strlen(logic->filterdata))
		return (0);

	/* Root is always in path */
	if (len == 1)
		return (line[0] == '/');

	/* Complete filename needs to fit */
	if (strncmp(line, logic->filterdata, len))
		return (0);

	/* Termination has to be \0 or / */
	c = ((char *)logic->filterdata)[len];
	if (c != '/' && c != '\0')
		return (0);

	return (1);
}

int
filter_regex(struct intercept_translate *tl, struct logic *logic)
{
	regex_t tmpre, *re;
	char *line;
	int res;

	if ((line = intercept_translate_print(tl)) == NULL)
		return (0);

	re = logic->filterarg;
	if (re == NULL) {
		/* If regex does not compute, we just do not match */
		if (regcomp(&tmpre, logic->filterdata,
			REG_EXTENDED | REG_NOSUB) != 0)
			return (0);
		re = &tmpre;
	}

	res = regexec(re, line, 0, NULL, 0);

	/* Clean up temporary memory associated with regex */
	if (re == &tmpre)
		regfree(re);

	return (res == 0);
}

int
filter_true(struct intercept_translate *tl, struct logic *logic)
{
	return (1);
}
