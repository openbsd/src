/*	$OpenBSD: login_fbtab.c,v 1.12 2004/09/18 19:22:42 deraadt Exp $	*/

/************************************************************************
* Copyright 1995 by Wietse Venema.  All rights reserved.  Some individual
* files may be covered by other copyrights.
*
* This material was originally written and compiled by Wietse Venema at
* Eindhoven University of Technology, The Netherlands, in 1990, 1991,
* 1992, 1993, 1994 and 1995.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that this entire copyright notice
* is duplicated in all such copies.
*
* This software is provided "as is" and without any expressed or implied
* warranties, including, without limitation, the implied warranties of
* merchantibility and fitness for any particular purpose.
************************************************************************/
/*
    SYNOPSIS
	void login_fbtab(tty, uid, gid)
	char *tty;
	uid_t uid;
	gid_t gid;

    DESCRIPTION
	This module implements device security as described in the
	SunOS 4.1.x fbtab(5) and SunOS 5.x logindevperm(4) manual
	pages. The program first looks for /etc/fbtab. If that file
	cannot be opened it attempts to process /etc/logindevperm.
	We expect entries with the folowing format:

	    Comments start with a # and extend to the end of the line.

	    Blank lines or lines with only a comment are ignored.

	    All other lines consist of three fields delimited by
	    whitespace: a login device (/dev/console), an octal
	    permission number (0600), and a ":"-delimited list of
	    devices (/dev/kbd:/dev/mouse). All device names are
	    absolute paths. A path that ends in "*" refers to all
	    directory entries except "." and "..".

	    If the tty argument (relative path) matches a login device
	    name (absolute path), the permissions of the devices in the
	    ":"-delimited list are set as specified in the second
	    field, and their ownership is changed to that of the uid
	    and gid arguments.

    DIAGNOSTICS
	Problems are reported via the syslog daemon with severity
	LOG_ERR.

    AUTHOR
	Wietse Venema (wietse@wzv.win.tue.nl)
	Eindhoven University of Technology
	The Netherlands
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <dirent.h>
#include <limits.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <util.h>

#define _PATH_FBTAB	"/etc/fbtab"

static void login_protect(const char *, int, uid_t, gid_t);

#define	WSPACE		" \t\n"

/*
 * login_fbtab - apply protections specified in /etc/fbtab or logindevperm
 */
void
login_fbtab(const char *tty, uid_t uid, gid_t gid)
{
	FILE	*fp;
	char	*buf, *toklast, *tbuf, *devnam, *cp;
	int	prot;
	size_t	len;

	if ((fp = fopen(_PATH_FBTAB, "r")) == NULL)
		return;

	tbuf = NULL;
	while ((buf = fgetln(fp, &len)) != NULL) {
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';
		else {
			if ((tbuf = (char *)malloc(len + 1)) == NULL)
				break;
			memcpy(tbuf, buf, len);
			tbuf[len] = '\0';
			buf = tbuf;
		}
		if ((cp = strchr(buf, '#')))
			*cp = '\0';	/* strip comment */
		if (buf[0] == '\0' ||
		    (cp = devnam = strtok_r(buf, WSPACE, &toklast)) == NULL)
			continue;	/* empty or comment */
		if (strncmp(devnam, _PATH_DEV, sizeof(_PATH_DEV) - 1) != 0 ||
		    (cp = strtok_r(NULL, WSPACE, &toklast)) == NULL ||
		    *cp != '0' ||
		    sscanf(cp, "%o", &prot) == 0 ||
		    prot == 0 ||
		    (prot & 0777) != prot ||
		    (cp = strtok_r(NULL, WSPACE, &toklast)) == NULL) {
			syslog(LOG_ERR, "%s: bad entry: %s", _PATH_FBTAB,
			    cp ? cp : "(null)");
			continue;
		}
		if (strcmp(devnam + sizeof(_PATH_DEV) - 1, tty) == 0) {
			for (cp = strtok_r(cp, ":", &toklast); cp != NULL;
			    cp = strtok_r(NULL, ":", &toklast))
				login_protect(cp, prot, uid, gid);
		}
	}
	if (tbuf != NULL)
		free(tbuf);
	fclose(fp);
}

/*
 * login_protect - protect one device entry
 */
static void
login_protect(const char *path, int mask, uid_t uid, gid_t gid)
{
	char	buf[PATH_MAX];
	size_t	pathlen = strlen(path);
	DIR	*dir;
	struct	dirent *ent;

	if (pathlen >= sizeof(buf)) {
		errno = ENAMETOOLONG;
		syslog(LOG_ERR, "%s: %s: %m", _PATH_FBTAB, path);
		return;
	}

	if (strcmp("/*", path + pathlen - 2) != 0) {
		if (chmod(path, mask) && errno != ENOENT)
			syslog(LOG_ERR, "%s: chmod(%s): %m", _PATH_FBTAB, path);
		if (chown(path, uid, gid) && errno != ENOENT)
			syslog(LOG_ERR, "%s: chown(%s): %m", _PATH_FBTAB, path);
	} else {
		/*
		 * This is a wildcard directory (/path/to/whatever/*).
		 * Make a copy of path without the trailing '*' (but leave
		 * the trailing '/' so we can append directory entries.)
		 */
		memcpy(buf, path, pathlen - 1);
		buf[pathlen - 1] = '\0';
		if ((dir = opendir(buf)) == NULL) {
			syslog(LOG_ERR, "%s: opendir(%s): %m", _PATH_FBTAB,
			    path);
			return;
		}

		while ((ent = readdir(dir)) != NULL) {
			if (strcmp(ent->d_name, ".")  != 0 &&
			    strcmp(ent->d_name, "..") != 0) {
				buf[pathlen - 1] = '\0';
				if (strlcat(buf, ent->d_name, sizeof(buf))
				    >= sizeof(buf)) {
					errno = ENAMETOOLONG;
					syslog(LOG_ERR, "%s: %s: %m",
					    _PATH_FBTAB, path);
				} else
					login_protect(buf, mask, uid, gid);
			}
		}
		closedir(dir);
	}
}
