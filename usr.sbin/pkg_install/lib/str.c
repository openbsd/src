/*	$OpenBSD: str.c,v 1.4 1998/10/13 23:09:54 marc Exp $	*/

#ifndef lint
static const char *rcsid = "$OpenBSD: str.c,v 1.4 1998/10/13 23:09:54 marc Exp $";
#endif

/*
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * Jordan K. Hubbard
 * 18 July 1993
 *
 * Miscellaneous string utilities.
 *
 */

#include <err.h>
#include <fnmatch.h>
#include "lib.h"

/* Return the filename portion of a path */
char *
basename_of(char *str)
{
	char *slash;

	return ((slash = strrchr(str, '/')) == (char *) NULL) ? str : slash + 1;
}

/* Return the dirname portion of a path */
char *
dirname_of(const char *path)
{
	size_t	cc;
	char	*s;
	char	*t;

	if ((s = strrchr(path, '/')) == (char *) NULL) {
		return ".";
	}
	if (s == path) {
		/* "/foo" -> return "/" */
		return "/";
	}
	cc = (size_t)(s - path) + 1;
	if ((t = (char *) malloc(cc)) == (char *) NULL) {
		errx(1, "out of memory in dirname_of");
	}
	(void) memcpy(t, path, cc);
	t[cc] = 0;
	return t;
}

char *
strconcat(char *s1, char *s2)
{
    static char tmp[FILENAME_MAX];

    tmp[0] = '\0';
    strncpy(tmp, s1 ? s1 : s2, FILENAME_MAX);
    if (s1 && s2)
	strncat(tmp, s2, FILENAME_MAX - strlen(tmp));
    return tmp;
}

/* Get a string parameter as a file spec or as a "contents follow -" spec */
char *
get_dash_string(char **str)
{
    char *s = *str;

    if (*s == '-')
	*str = copy_string(s + 1);
    else
	*str = fileGetContents(s);
    return *str;
}

/* Rather Obvious */
char *
copy_string(char *str)
{
    char *ret;

    if (!str)
	ret = NULL;
    else {
	ret = (char *)malloc(strlen(str) + 1);
	strcpy(ret, str);
    }
    return ret;
}

/* Return TRUE if 'str' ends in suffix 'suff' */
Boolean
suffix(char *str, char *suff)
{
    char *idx;
    Boolean ret = FALSE;

    idx = strrchr(str, '.');
    if (idx && !strcmp(idx + 1, suff))
	ret = TRUE;
    return ret;
}

/* Assuming str has a suffix, brutally murder it! */
void
nuke_suffix(char *str)
{
    char *idx;

    idx = strrchr(str, '.');
    if (idx)
	*idx = '\0';  /* Yow!  Don't try this on a const! */
}

/* Lowercase a whole string */
void
str_lowercase(char *str)
{
    while (*str) {
	*str = tolower(*str);
	++str;
    }
}


enum deweycmp_ops {
	GT,
	GE,
	LT,
	LE
};

/* compare two dewey decimal numbers */
static int
deweycmp(char *a, enum deweycmp_ops op, char *b)
{
	int             ad;
	int             bd;
	int             cmp;

	for (;;) {
		if (*a == 0 && *b == 0) {
			cmp = 0;
			break;
		}
		ad = bd = 0;
		for (; *a && *a != '.'; a++) {
			ad = (ad * 10) + (*a - '0');
		}
		for (; *b && *b != '.'; b++) {
			bd = (bd * 10) + (*b - '0');
		}
		if ((cmp = ad - bd) != 0) {
			break;
		}
		if (*a == '.') {
			a++;
		}
		if (*b == '.') {
			b++;
		}
	}
	return (op == GE) ? cmp >= 0 : (op == GT) ? cmp > 0 : (op == LE) ? cmp <= 0 : cmp < 0;
}

/* perform alternate match on "pkg" against "pattern", */
/* calling pmatch (recursively) to resolve any other patterns */
/* return 1 on match, 0 otherwise */
static int
alternate_match(const char *pattern, const char *pkg)
{
	char           *sep;
	char            buf[FILENAME_MAX];
	char           *last;
	char           *alt;
	char           *cp;
	int             cnt;
	int             found;

	if ((sep = strchr(pattern, '{')) == (char *) NULL) {
		errx(1, "alternate_match(): '{' expected in \"%s\"\n", pattern);
	}
	(void) strncpy(buf, pattern, (size_t)(sep - pattern));
	alt = &buf[sep - pattern];
	last = (char *) NULL;
	for (cnt = 0, cp = sep; *cp && last == (char *) NULL ; cp++) {
		if (*cp == '{') {
			cnt++;
		} else if (*cp == '}' && --cnt == 0 && last == (char *) NULL) {
			last = cp + 1;
		}
	}
	if (cnt != 0) {
		warnx("Malformed alternate `%s'", pattern);
		return 1;
	}
	for (found = 0, cp = sep + 1; *sep != '}'; cp = sep + 1) {
		for (cnt = 0, sep = cp; cnt > 0 || (cnt == 0 && *sep != '}' && *sep != ','); sep++) {
			if (*sep == '{') {
				cnt++;
			} else if (*sep == '}') {
				cnt--;
			}
		}
		(void) snprintf(alt, sizeof(buf) - (alt - buf), "%.*s%s", (int) (sep - cp), cp, last);
		if (pmatch(buf, pkg) == 1) {
			found = 1;
		}
	}
	return found;
}

/* perform dewey match on "pkg" against "pattern" */
/* return 1 on match, 0 otherwise */
static int
dewey_match(const char *pattern, const char *pkg)
{
	char           *cp;
	char           *sep;
	char           *ver;
	int             found;
	enum deweycmp_ops op;
	int             n;
	char            name[FILENAME_MAX];

	found = 0;
	if ((sep = strpbrk(pattern, "<>")) == NULL)
		errx(1, "dewey_match(): '<' or '>' expexted in \"%s\"\n", pattern);

	/* next three lines are static in loops, too (-> cache!) */
	snprintf(name, sizeof(name), "%.*s", (int) (sep - pattern), pattern);
	op = (*sep == '>') ? (*(sep + 1) == '=') ? GE : GT : (*(sep + 1) == '=') ? LE : LT;
	ver = (op == GE || op == LE) ? sep + 2 : sep + 1;
	n = (int)(sep - pattern);
	if ((cp = strrchr(pkg, '-')) != (char *) NULL) {
		if (strncmp(pkg, name, (size_t)(cp - pkg)) == 0 && n == cp - pkg) {
			if (deweycmp(cp + 1, op, ver)) {
				found = 1;
			}
		}
	}
	return found;
}

/* perform glob match on "pkg" against "pattern" */
/* return 1 on match, 0 otherwise */
static int
glob_match(const char *pattern, const char *pkg)
{
	return fnmatch(pattern, pkg, FNM_PERIOD) == 0;
}

/* perform simple match on "pkg" against "pattern" */
/* return 1 on match, 0 otherwise */
static int
simple_match(const char *pattern, const char *pkg)
{
	return !strcmp(pattern, pkg);
}

/* match pkg against pattern, return 1 if matching, 0 else */
/*
 * Optimize: this is called many times in readdir()-loops, where the
 * pattern doesn't change, so the {,} alternates may be unrolles/cached.
 */
int 
pmatch(const char *pattern, const char *pkg)
{
	if (strchr(pattern, '{')) {
		/* emulate csh-type alternates */
		return alternate_match(pattern, pkg);
	}
	if (strpbrk(pattern, "<>")) {
		/* perform relational dewey match on version number */
		return dewey_match(pattern, pkg);
	}
	if (strpbrk(pattern, "*?[]")) {
		/* glob match */
		return glob_match(pattern, pkg);
	}
	/* no alternate, dewey or glob match -> simple compare */
	return simple_match(pattern, pkg);
}


/* search dir for pattern, writing the found match in buf */
/* let's hope there's only one ... - HF */
/* returns -1 on error, 1 if found, 0 otherwise. */
int
findmatchingname(const char *dir, const char *pattern, matchfn f, char *data)
{
    struct dirent  *dp;
    DIR            *dirp;
    int             found;
    
    found = 0;
    if ((dirp = opendir(dir)) == NULL) {
	/* warnx("can't opendir dir '%s'", dir); */
	return -1;
    }
    while ((dp = readdir(dirp)) != NULL) {
	if (strcmp(dp->d_name, ".") == 0 ||
	    strcmp(dp->d_name, "..") == 0) {
	    continue;
	}
	if (pmatch(pattern, dp->d_name)) {
	    if(f)
		f(dp->d_name, data);
	    found=1;
	}
    }
    closedir(dirp);

    return found;    
}

/* does the pkgname contain any of the special chars ("{[]?*<>")? */
/* if so, return 1, else 0 */
int
ispkgpattern(const char *pkg)
{
    return strpbrk(pkg, "<>[]?*{") != NULL;
}

/* auxiliary function called by findbestmatchingname() */
static int
findbestmatchingname_fn(const char *pkg, char *data)
{
    /* if pkg > data */
    char *s1, *s2;

    s1=strrchr(pkg, '-')+1;
    s2=strrchr(data, '-')+1;

    if(data[0] == '\0' || deweycmp(s1, GT, s2))
	strcpy(data, pkg);

    return 0;
}

/* find best matching filename, i.e. the pkg with the highest
 * matching(!) version */
/* returns pointer to pkg name (which can be free(3)ed),
 * or NULL if no match is available. */
char *
findbestmatchingname(const char *dir, const char *pattern)
{
	char buf[FILENAME_MAX];

	buf[0]='\0';
	if (findmatchingname(dir, pattern, findbestmatchingname_fn, buf) > 0
	    && buf[0] != '\0') {
		return strdup(buf);
	}
	return NULL;
}

/* bounds-checking strncpy */
char *
strnncpy(char *to, size_t tosize, char *from, size_t cc)
{
	size_t	len;

	if ((len = cc) >= tosize - 1) {
		len = tosize - 1;
	}
	(void) strncpy(to, from, len);
	to[len] = 0;
	return to;
}
