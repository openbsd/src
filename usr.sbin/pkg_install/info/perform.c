/*	$OpenBSD: perform.c,v 1.4 1998/09/07 22:30:15 marc Exp $	*/

#ifndef lint
static const char *rcsid = "$OpenBSD: perform.c,v 1.4 1998/09/07 22:30:15 marc Exp $";
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
 * 23 Aug 1993
 *
 * This is the main body of the info module.
 *
 */

#include "lib.h"
#include "info.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <signal.h>
#include <dirent.h>
#include <fnmatch.h>
#include <ctype.h>

static char    *Home;

static int
pkg_do(char *pkg)
{
	Boolean         installed = FALSE, isTMP = FALSE;
	char            log_dir[FILENAME_MAX];
	char            fname[FILENAME_MAX];
	Package         plist;
	FILE           *fp;
	struct stat     sb;
	char           *cp = NULL;
	int             code = 0;

	if (isURL(pkg)) {
		if ((cp = fileGetURL(NULL, pkg)) != NULL) {
			strcpy(fname, cp);
			isTMP = TRUE;
		}
	} else if (fexists(pkg) && isfile(pkg)) {
		int             len;

		if (*pkg != '/') {
			if (!getcwd(fname, FILENAME_MAX))
				upchuck("getcwd");
			len = strlen(fname);
			snprintf(&fname[len], FILENAME_MAX - len, "/%s", pkg);
		} else
			strcpy(fname, pkg);
		cp = fname;
	} else {
		if ((cp = fileFindByPath(NULL, pkg)) != NULL)
			strncpy(fname, cp, FILENAME_MAX);
	}
	if (cp) {
		if (isURL(pkg)) {
			/* file is already unpacked by fileGetURL() */
			strcpy(PlayPen, cp);
		} else {
			/*
			 * Apply a crude heuristic to see how much space the package will
			 * take up once it's unpacked.  I've noticed that most packages
			 * compress an average of 75%, but we're only unpacking the + files so
			 * be very optimistic.
			 */
			if (stat(fname, &sb) == FAIL) {
				warnx("can't stat package file '%s'", fname);
				code = 1;
				goto bail;
			}
			Home = make_playpen(PlayPen, sb.st_size / 2);
			if (unpack(fname, "+*")) {
				warnx("error during unpacking, no info for '%s' available", pkg);
				code = 1;
				goto bail;
			}
		}
	}
	/*
	 * It's not an ininstalled package, try and find it among the
	 * installed
	 */
	else {
		char           *tmp;

		sprintf(log_dir, "%s/%s", (tmp = getenv(PKG_DBDIR)) ? tmp : DEF_LOG_DIR,
			pkg);
		if (!fexists(log_dir)) {
			warnx("can't find package `%s' installed or in a file!", pkg);
			return 1;
		}
		if (chdir(log_dir) == FAIL) {
			warnx("can't change directory to '%s'!", log_dir);
			return 1;
		}
		installed = TRUE;
	}

	/* Suck in the contents list */
	plist.head = plist.tail = NULL;
	fp = fopen(CONTENTS_FNAME, "r");
	if (!fp) {
		warnx("unable to open %s file", CONTENTS_FNAME);
		code = 1;
		goto bail;
	}
	/* If we have a prefix, add it now */
	read_plist(&plist, fp);
	fclose(fp);

	/*
         * Index is special info type that has to override all others to make
         * any sense.
         */
	if (Flags & SHOW_INDEX) {
		char            tmp[FILENAME_MAX];

		snprintf(tmp, FILENAME_MAX, "%-19s ", pkg);
		show_index(tmp, COMMENT_FNAME);
	} else {
		/* Start showing the package contents */
		if (!Quiet)
			printf("%sInformation for %s:\n\n", InfoPrefix, pkg);
		if (Flags & SHOW_COMMENT)
			show_file("Comment:\n", COMMENT_FNAME);
		if ((Flags & SHOW_REQBY) && !isemptyfile(REQUIRED_BY_FNAME))
			show_file("Required by:\n", REQUIRED_BY_FNAME);
		if (Flags & SHOW_DESC)
			show_file("Description:\n", DESC_FNAME);
		if ((Flags & SHOW_DISPLAY) && fexists(DISPLAY_FNAME))
			show_file("Install notice:\n", DISPLAY_FNAME);
		if (Flags & SHOW_PLIST)
			show_plist("Packing list:\n", &plist, (plist_t) - 1);
		if ((Flags & SHOW_INSTALL) && fexists(INSTALL_FNAME))
			show_file("Install script:\n", INSTALL_FNAME);
		if ((Flags & SHOW_DEINSTALL) && fexists(DEINSTALL_FNAME))
			show_file("De-Install script:\n", DEINSTALL_FNAME);
		if ((Flags & SHOW_MTREE) && fexists(MTREE_FNAME))
			show_file("mtree file:\n", MTREE_FNAME);
		if (Flags & SHOW_PREFIX)
			show_plist("Prefix(s):\n", &plist, PLIST_CWD);
		if (Flags & SHOW_FILES)
			show_files("Files:\n", &plist);
		if (!Quiet)
			puts(InfoPrefix);
	}
	free_plist(&plist);
bail:
	leave_playpen(Home);
	if (isTMP)
		unlink(fname);
	return code;
}

/* use fnmatch to do a glob-style match */
/* returns 0 if found, 1 if not (1) */
static int
globmatch(char *pkgspec, char *dbdir, int quiet)
{
	/* Using glob-match */
	struct dirent  *dp;
	int             found;
	DIR            *dirp;

	found = 0;
	if ((dirp = opendir(dbdir)) == (DIR *) NULL) {
		warnx("can't opendir package dir '%s'", dbdir);
		return !0;
	}
	while ((dp = readdir(dirp)) != (struct dirent *) NULL) {
		if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) {
			continue;
		}
		if (fnmatch(pkgspec, dp->d_name, FNM_PERIOD) == 0) {
			if (!quiet)
				printf("%s\n", dp->d_name);
			found = 1;
		}
	}
	closedir(dirp);
	return !found;
}

enum {
	GT,
	GE,
	LT,
	LE
};

/* compare two dewey decimal numbers */
static int
deweycmp(char *a, int op, char *b)
{
	int	ad;
	int	bd;
	int	cmp;

	for (;;) {
		if (*a == 0 && *b == 0) {
			cmp = 0;
			break;
		}
		ad = bd = 0;
		for ( ; *a && *a != '.' ; a++) {
			ad = (ad * 10) + (*a - '0');
		}
		for ( ; *b && *b != '.' ; b++) {
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

/* match on a relation against dewey decimal numbers */
/* returns 0 if found, 1 if not (!) */
static int
deweymatch(char *name, int op, char *ver, char *dbdir, int quiet)
{
	struct dirent  *dp;
	char		*cp;
	DIR            *dirp;
	int             ret;
        int     	n;

	n = strlen(name);
	ret = 1;
	if ((dirp = opendir(dbdir)) == (DIR *) NULL) {
		warnx("can't opendir package dir '%s'", dbdir);
		return 1;
	}
	while ((dp = readdir(dirp)) != (struct dirent *) NULL) {
		if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) {
			continue;
		}
		if ((cp = strrchr(dp->d_name, '-')) != (char *) NULL) {
			if (strncmp(dp->d_name, name, cp - dp->d_name) == 0 && n == cp - dp->d_name) {
				if (deweycmp(cp + 1, op, ver)) {
					if (!quiet)
						printf("%s\n", dp->d_name);
					ret = 0;
				}
			}
		}
	}
	closedir(dirp);
	return ret;
}

/* do a match on a package pattern in dbdir */
/* returns 0 if found, 1 if not (!) */
static int
matchname(char *pkgspec, char *dbdir, int quiet)
{
	struct stat	st;
	char            buf[FILENAME_MAX];
	char		*sep;
	char		*last;
	char		*alt;
	char		*cp;
	int             error;
	int		cnt;
	int		ret;

	if ((sep = strchr(pkgspec, '{')) != (char *) NULL) {
		/* emulate csh-type alternates */
		(void) strncpy(buf, pkgspec, sep - pkgspec);
		alt = &buf[sep - pkgspec];
		for (last = NULL, cnt = 0, cp = sep ; *cp && !last ; cp++) {
			if (*cp == '{') {
				cnt++;
			} else if (*cp == '}' && --cnt == 0 && last == NULL) {
				last = cp + 1;
			}
		}
		if (cnt != 0) {
			warnx("Malformed alternate `%s'", pkgspec);
			return 1;
		}
		for (ret = 1, cp = sep + 1 ; *sep != '}' ; cp = sep + 1) {
			for (cnt = 0, sep = cp ; cnt > 0 || (cnt == 0 && *sep != '}' && *sep != ',') ; sep++) {
				if (*sep == '{') {
					cnt++;
				} else if (*sep == '}') {
					cnt--;
				}
			}
			(void) snprintf(alt, sizeof(buf) - (alt - buf), "%.*s%s", (int)(sep - cp), cp, last);
			if (matchname(buf, dbdir, quiet) == 0) {
				ret = 0;
			}
		}
		return ret;
	}
	if ((sep = strpbrk(pkgspec, "<>")) != (char *) NULL) {
		/* perform relational dewey match on version number */
		(void) snprintf(buf, sizeof(buf), "%.*s", (int)(sep - pkgspec), pkgspec);
		cnt = (*sep == '>') ? (*(sep + 1) == '=') ? GE : GT : (*(sep + 1) == '=') ? LE : LT;
		cp = (cnt == GE || cnt == LE) ? sep + 2 : sep + 1;
		return deweymatch(buf, cnt, cp, dbdir, quiet);
	}
	if (strpbrk(pkgspec, "*?[]") != (char *) NULL) {
		return globmatch(pkgspec, dbdir, quiet);
	}
	/* No shell meta character given - simple check */
	(void) snprintf(buf, sizeof(buf), "%s/%s", dbdir, pkgspec);
	error = (lstat(buf, &st) < 0);
	if (!error && !quiet)
		printf("%s\n", pkgspec);
	return error;
}

void
cleanup(int sig)
{
	leave_playpen(Home);
	exit(1);
}

int
pkg_perform(char **pkgs)
{
	int             i, err_cnt = 0;
	char           *tmp;

	signal(SIGINT, cleanup);

	tmp = getenv(PKG_DBDIR);
	if (!tmp)
		tmp = DEF_LOG_DIR;
	/* Overriding action? */
	if (CheckPkg) {
		return matchname(CheckPkg, tmp, Quiet);
	} else if (AllInstalled) {
		struct dirent  *dp;
		DIR            *dirp;

		if (!(isdir(tmp) || islinktodir(tmp)))
			return 1;
		if ((dirp = opendir(tmp)) != (DIR *) NULL) {
			while ((dp = readdir(dirp)) != (struct dirent *) NULL) {
				if (strcmp(dp->d_name, ".") && strcmp(dp->d_name, "..")) {
					err_cnt += pkg_do(dp->d_name);
				}
			}
			(void) closedir(dirp);
		}
	} else {
		for (i = 0; pkgs[i]; i++) {
			err_cnt += pkg_do(pkgs[i]);
		}
	}
	return err_cnt;
}
