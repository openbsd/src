/*	$OpenBSD: getnetgrent.c,v 1.8 1998/08/14 21:39:28 deraadt Exp $	*/

/*
 * Copyright (c) 1994 Christos Zoulas
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
 *	This product includes software developed by Christos Zoulas.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$OpenBSD: getnetgrent.c,v 1.8 1998/08/14 21:39:28 deraadt Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <stdio.h>
#define _NETGROUP_PRIVATE
#include <netgroup.h>
#include <string.h>
#include <fcntl.h>
#include <err.h>
#include <ctype.h>
#include <stdlib.h>
#include <db.h>

#define _NG_STAR(s)	(((s) == NULL || *(s) == '\0') ? _ngstar : s)
#define _NG_EMPTY(s)	((s) == NULL ? "" : s)
#define _NG_ISSPACE(p)	(isspace((unsigned char) (p)) || (p) == '\n')

static const char _ngstar[] = "*";
static const char _ngoomem[] = "netgroup: %m";
static struct netgroup *_nghead = (struct netgroup *)NULL;
static struct netgroup *_nglist = (struct netgroup *)NULL;
static DB *_ng_db;

/*
 * Simple string list
 */
struct stringlist {
	char		**sl_str;
	size_t		  sl_max;
	size_t		  sl_cur;
};

static int		getstring __P((char **, int, char **));
static struct netgroup	*getnetgroup __P((char **));
static int		 lookup __P((const char *, char *, char **, int));
static void		 addgroup __P((char *, struct stringlist *, char *));
static int		 in_check __P((const char *, const char *,
				       const char *, struct netgroup *));
static int		 in_find __P((char *, struct stringlist *,
				      char *, const char *,
				      const char *, const char *));
static char		*in_lookup1 __P((const char *, const char *,
					 const char *, int));
static int		 in_lookup __P((const char *, const char *,
					const char *, const char *, int));

/*
 * _ng_sl_init(): Initialize a string list
 */
struct stringlist *
_ng_sl_init()
{
	struct stringlist *sl = malloc(sizeof(struct stringlist));
	if (sl == NULL)
		_err(1, _ngoomem);

	sl->sl_cur = 0;
	sl->sl_max = 20;
	sl->sl_str = malloc(sl->sl_max * sizeof(char *));
	if (sl->sl_str == NULL)
		_err(1, _ngoomem);
	return sl;
}


/*
 * _ng_sl_add(): Add an item to the string list
 */
void
_ng_sl_add(sl, name)
	struct stringlist	*sl;
	char			*name;
{
	if (sl->sl_cur == sl->sl_max - 1) {
		char **slstr;

		sl->sl_max += 20;
		slstr = realloc(sl->sl_str, sl->sl_max * sizeof(char *));
		if (slstr == NULL) {
			if (sl->sl_str)
				free(sl->sl_str);
			sl->sl_str = NULL;
			_err(1, _ngoomem);
		}
		sl->sl_str = slstr;
	}
	sl->sl_str[sl->sl_cur++] = name;
}


/*
 * _ng_sl_free(): Free a stringlist
 */
void
_ng_sl_free(sl, all)
	struct stringlist	*sl;
	int			 all;
{
	size_t	i;

	if (all)
		for (i = 0; i < sl->sl_cur; i++)
			free(sl->sl_str[i]);
	free(sl->sl_str);
	free(sl);
}


/*
 * sl_find(): Find a name in the string list
 */
char *
_ng_sl_find(sl, name)
	struct stringlist	*sl;
	char			*name;
{
	size_t	i;

	for (i = 0; i < sl->sl_cur; i++)
		if (strcmp(sl->sl_str[i], name) == 0)
			return sl->sl_str[i];

	return NULL;
}


/*
 * getstring(): Get a string delimited by the character, skipping leading and
 * trailing blanks and advancing the pointer
 */
static int
getstring(pp, del, str)
	char	**pp;
	int	  del;
	char	**str;
{
	char *sp, *ep, *dp;

	/* skip leading blanks */
	for (sp = *pp; *sp && _NG_ISSPACE(*sp); sp++)
		continue;

	/* accumulate till delimiter or space */
	for (ep = sp; *ep && *ep != del && !_NG_ISSPACE(*ep); ep++)
		continue;

	/* hunt for the delimiter */
	for (dp = ep; *dp && *dp != del && _NG_ISSPACE(*dp); dp++)
		continue;

	if (*dp != del) {
		*str = NULL;
		return 0;
	}

	*pp = ++dp;

	del = (ep - sp) + 1;
	if (del > 1) {
		dp = malloc(del);
		if (dp == NULL)
			_err(1, _ngoomem);
		memcpy(dp, sp, del);
		dp[del - 1] = '\0';
	} else
		dp = NULL;

	*str = dp;
	return 1;
}


/*
 * getnetgroup(): Parse a netgroup, and advance the pointer
 */
static struct netgroup *
getnetgroup(pp)
	char	**pp;
{
	struct netgroup *ng = malloc(sizeof(struct netgroup));

	if (ng == NULL)
		_err(1, _ngoomem);

	(*pp)++;	/* skip '(' */
	if (!getstring(pp, ',', &ng->ng_host))
		goto badhost;

	if (!getstring(pp, ',', &ng->ng_user))
		goto baduser;

	if (!getstring(pp, ')', &ng->ng_domain))
		goto baddomain;

#ifdef DEBUG_NG
	{
		char buf[1024];
		(void) fprintf(stderr, "netgroup %s\n",
		    _ng_print(buf, sizeof(buf), ng));
	}
#endif
	return ng;

baddomain:
	if (ng->ng_user)
		free(ng->ng_user);
baduser:
	if (ng->ng_host)
		free(ng->ng_host);
badhost:
	free(ng);
	return NULL;
}


/*
 * lookup(): Find the given key in the database or yp, and return its value
 * in *line; returns 1 if key was found, 0 otherwise
 */
static int
lookup(ypdom, name, line, bywhat)
	const char	 *ypdom;
	char		 *name;
	char		**line;
	int		  bywhat;
{
#ifdef YP
	int		i;
	char	       *map = NULL;
#endif

	if (_ng_db) {
		DBT	 key, data;
		size_t	 len = strlen(name) + 2;
		char	*ks = malloc(len);

		ks[0] = bywhat;
		memcpy(&ks[1], name, len - 1);

		key.data = (u_char *) ks;
		key.size = len;

		switch ((_ng_db->get) (_ng_db, &key, &data, 0)) {
		case 0:
			free(ks);
			*line = strdup(data.data);
			if (*line == NULL)
				_err(1, _ngoomem);
			return 1;

		case 1:
			break;

		case -1:
			_warn("netgroup: db get");
			break;
		}
		free(ks);
	}
#ifdef YP
	if (ypdom) {
		switch (bywhat) {
		case _NG_KEYBYNAME:
			map = "netgroup";
			break;

		case _NG_KEYBYUSER:
			map = "netgroup.byuser";
			break;

		case _NG_KEYBYHOST:
			map = "netgroup.byhost";
			break;
		}


		if (yp_match(ypdom, map, name, strlen(name), line, &i) == 0)
			return 1;
	}
#endif

	return 0;
}


/*
 * _ng_parse(): Parse a line and return: _NG_ERROR: Syntax Error _NG_NONE:
 * line was empty or a comment _NG_GROUP: line had a netgroup definition,
 * returned in ng _NG_NAME:  line had a netgroup name, returned in name
 * 
 * Public since used by netgroup_mkdb
 */
int
_ng_parse(p, name, ng)
	char		**p;
	char		**name;
	struct netgroup	**ng;
{
	while (**p) {
		if (**p == '#')
			/* comment */
			return _NG_NONE;

		while (**p && _NG_ISSPACE(**p))
			/* skipblank */
			(*p)++;

		if (**p == '(') {
			if ((*ng = getnetgroup(p)) == NULL) {
				_warnx("netgroup: Syntax error `%s'", *p);
				return _NG_ERROR;
			}
			return _NG_GROUP;
		} else {
			char	       *np;
			int		i;

			for (np = *p; **p && !_NG_ISSPACE(**p); (*p)++)
				continue;
			if (np != *p) {
				i = (*p - np) + 1;
				*name = malloc(i);
				if (*name == NULL)
					_err(1, _ngoomem);
				memcpy(*name, np, i);
				(*name)[i - 1] = '\0';
				return _NG_NAME;
			}
		}
	}
	return _NG_NONE;
}


/*
 * addgroup(): Recursively add all the members of the netgroup to this group
 */
static void
addgroup(ypdom, sl, grp)
	char			*ypdom;
	struct stringlist	*sl;
	char			*grp;
{
	char		*line, *p;
	struct netgroup	*ng;
	char		*name;

#ifdef DEBUG_NG
	(void) fprintf(stderr, "addgroup(%s)\n", grp);
#endif
	/* check for cycles */
	if (_ng_sl_find(sl, grp) != NULL) {
		free(grp);
		_warnx("netgroup: Cycle in group `%s'", grp);
		return;
	}
	_ng_sl_add(sl, grp);

	/* Lookup this netgroup */
	if (!lookup(ypdom, grp, &line, _NG_KEYBYNAME))
		return;

	p = line;

	for (;;) {
		switch (_ng_parse(&p, &name, &ng)) {
		case _NG_NONE:
			/* Done with the line */
			free(line);
			return;

		case _NG_GROUP:
			/* new netgroup */
			/* add to the list */
			ng->ng_next = _nglist;
			_nglist = ng;
			break;

		case _NG_NAME:
			/* netgroup name */
			addgroup(ypdom, sl, name);
			break;

		case _NG_ERROR:
			return;
		}
	}
}


/*
 * in_check(): Compare the spec with the netgroup
 */
static int
in_check(host, user, domain, ng)
	const char	*host;
	const char	*user;
	const char	*domain;
	struct netgroup	*ng;
{
	if ((host != NULL) && (ng->ng_host != NULL)
	    && strcmp(ng->ng_host, host) != 0)
		return 0;

	if ((user != NULL) && (ng->ng_user != NULL)
	    && strcmp(ng->ng_user, user) != 0)
		return 0;

	if ((domain != NULL) && (ng->ng_domain != NULL)
	    && strcmp(ng->ng_domain, domain) != 0)
		return 0;

	return 1;
}


/*
 * in_find(): Find a match for the host, user, domain spec
 */
static int
in_find(ypdom, sl, grp, host, user, domain)
	char			*ypdom;
	struct stringlist	*sl;
	char			*grp;
	const char		*host;
	const char		*user;
	const char		*domain;
{
	char		*line, *p;
	int		 i;
	struct netgroup	*ng;
	char		*name;

#ifdef DEBUG_NG
	(void) fprintf(stderr, "in_find(%s)\n", grp);
#endif
	/* check for cycles */
	if (_ng_sl_find(sl, grp) != NULL) {
		free(grp);
		_warnx("netgroup: Cycle in group `%s'", grp);
		return 0;
	}
	_ng_sl_add(sl, grp);

	/* Lookup this netgroup */
	if (!lookup(ypdom, grp, &line, _NG_KEYBYNAME))
		return 0;

	p = line;

	for (;;) {
		switch (_ng_parse(&p, &name, &ng)) {
		case _NG_NONE:
			/* Done with the line */
			free(line);
			return 0;

		case _NG_GROUP:
			/* new netgroup */
			i = in_check(host, user, domain, ng);
			if (ng->ng_host != NULL)
				free(ng->ng_host);
			if (ng->ng_user != NULL)
				free(ng->ng_user);
			if (ng->ng_domain != NULL)
				free(ng->ng_domain);
			free(ng);
			if (i) {
				free(line);
				return 1;
			}
			break;

		case _NG_NAME:
			/* netgroup name */
			if (in_find(ypdom, sl, name, host, user, domain)) {
				free(line);
				return 1;
			}
			break;

		case _NG_ERROR:
			free(line);
			return 0;
		}
	}
}


/*
 * _ng_makekey(): Make a key from the two names given. The key is of the form
 * <name1>.<name2> Names strings are replaced with * if they are empty;
 */
char *
_ng_makekey(s1, s2, len)
	const char	*s1, *s2;
	size_t		 len;
{
	char *buf = malloc(len);
	if (buf == NULL)
		_err(1, _ngoomem);
	(void) snprintf(buf, len, "%s.%s", _NG_STAR(s1), _NG_STAR(s2));
	return buf;
}

void
_ng_print(buf, len, ng)
	char *buf;
	size_t len;
	const struct netgroup *ng;
{
	(void) snprintf(buf, len, "(%s,%s,%s)", _NG_EMPTY(ng->ng_host),
	    _NG_EMPTY(ng->ng_user), _NG_EMPTY(ng->ng_domain));
}


/*
 * in_lookup1(): Fast lookup for a key in the appropriate map
 */
static char *
in_lookup1(ypdom, key, domain, map)
	const char	*ypdom;
	const char	*key;
	const char	*domain;
	int		 map;
{
	char	*line;
	size_t	 len;
	char	*ptr;
	int	 res;

	len = (key ? strlen(key) : 1) + (domain ? strlen(domain) : 1) + 2;
	ptr = _ng_makekey(key, domain, len);
	res = lookup(ypdom, ptr, &line, map);
	free(ptr);
	return res ? line : NULL;
}


/*
 * in_lookup(): Fast lookup for a key in the appropriate map
 */
static int
in_lookup(ypdom, group, key, domain, map)
	const char	*ypdom;
	const char	*group;
	const char	*key;
	const char	*domain;
	int		 map;
{
	size_t	 len;
	char	*ptr, *line;

	if (domain != NULL) {
		/* Domain specified; look in "group.domain" and "*.domain" */
		if ((line = in_lookup1(ypdom, key, domain, map)) == NULL)
			line = in_lookup1(ypdom, NULL, domain, map);
	}
	else 
		line = NULL;

	if (line == NULL) {
		/* 
		 * domain not specified or domain lookup failed; look in
		 * "group.*" and "*.*"
		 */
	    if (((line = in_lookup1(ypdom, key, NULL, map)) == NULL) &&
		((line = in_lookup1(ypdom, NULL, NULL, map)) == NULL))
		return 0;
	}

	len = strlen(group);

	for (ptr = line; (ptr = strstr(ptr, group)) != NULL;)
		/* Make sure we did not find a substring */
		if ((ptr != line && ptr[-1] != ',') ||
		    (ptr[len] != '\0' && strchr("\n\t ,", ptr[len]) == NULL))
			ptr++;
		else {
			free(line);
			return 1;
		}

	free(line);
	return 0;
}


void
endnetgrent()
{
	for (_nglist = _nghead; _nglist != NULL; _nglist = _nghead) {
		_nghead = _nglist->ng_next;
		if (_nglist->ng_host != NULL)
			free(_nglist->ng_host);
		if (_nglist->ng_user != NULL)
			free(_nglist->ng_user);
		if (_nglist->ng_domain != NULL)
			free(_nglist->ng_domain);
		free(_nglist);
	}

	if (_ng_db) {
		(void) (_ng_db->close) (_ng_db);
		_ng_db = NULL;
	}
}


void
setnetgrent(ng)
	const char	*ng;
{
	struct stringlist	*sl = _ng_sl_init();
#ifdef YP
	char			*line;
#endif
	char			*ng_copy, *ypdom = NULL;

	/* Cleanup any previous storage */
	if (_nghead != NULL)
		endnetgrent();

	if (_ng_db == NULL)
		_ng_db = dbopen(_PATH_NETGROUP_DB, O_RDONLY, 0, DB_HASH, NULL);

#ifdef YP
	/*
	 * We use yp if there is a "+" in the netgroup file, or if there is
	 * no netgroup file at all
	 */
	if (_ng_db == NULL || lookup(NULL, "+", &line, _NG_KEYBYNAME) == 0)
		yp_get_default_domain(&ypdom);
	else
		free(line);
#endif
	ng_copy = strdup(ng);
	if (ng_copy == NULL)
		_err(1, _ngoomem);
	addgroup(ypdom, sl, ng_copy);
	_nghead = _nglist;
	_ng_sl_free(sl, 1);
}


int
getnetgrent(host, user, domain)
	const char	**host;
	const char	**user;
	const char	**domain;
{
	if (_nglist == NULL)
		return 0;

	*host   = _nglist->ng_host;
	*user   = _nglist->ng_user;
	*domain = _nglist->ng_domain;

	_nglist = _nglist->ng_next;

	return 1;
}


int
innetgr(grp, host, user, domain)
	const char	*grp, *host, *user, *domain;
{
	char	*ypdom = NULL;
#ifdef YP
	char	*line;
#endif
	int	 found;
	struct stringlist *sl;

	if (_ng_db == NULL)
		_ng_db = dbopen(_PATH_NETGROUP_DB, O_RDONLY, 0, DB_HASH, NULL);

#ifdef YP
	/*
	 * We use yp if there is a "+" in the netgroup file, or if there is
	 * no netgroup file at all
	 */
	if (_ng_db == NULL)
		yp_get_default_domain(&ypdom);
	else if (lookup(NULL, "+", &line, _NG_KEYBYNAME) == 0) {
		yp_get_default_domain(&ypdom);
		free(line);
	}
#endif

	/* Try the fast lookup first */
	if (host != NULL && user == NULL) {
		if (in_lookup(ypdom, grp, host, domain, _NG_KEYBYHOST))
			return 1;
	} else if (host == NULL && user != NULL) {
		if (in_lookup(ypdom, grp, user, domain, _NG_KEYBYUSER))
			return 1;
	}
	/* If a domainname is given, we would have found a match */
	if (domain != NULL)
		return 0;

	/* Too bad need the slow recursive way */
	sl = _ng_sl_init();
	found = in_find(ypdom, sl, strdup(grp), host, user, domain);
	_ng_sl_free(sl, 1);

	return found;
}
