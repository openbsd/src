/*	$OpenBSD: path.c,v 1.1.1.1 1996/08/14 06:19:11 downsj Exp $	*/

#include "sh.h"
#include "ksh_stat.h"

/*
 *	Contains a routine to search a : seperated list of
 *	paths (a la CDPATH) and make appropiate file names.
 *	Also contains a routine to simplify .'s and ..'s out of
 *	a path name.
 *
 *	Larry Bouzane (larry@cs.mun.ca)
 */

/*
 * $Log: path.c,v $
 * Revision 1.1.1.1  1996/08/14 06:19:11  downsj
 * Import pdksh 5.2.7.
 *
 * Revision 1.2  1994/05/19  18:32:40  michael
 * Merge complete, stdio replaced, various fixes. (pre autoconf)
 *
 * Revision 1.1  1994/04/06  13:14:03  michael
 * Initial revision
 *
 * Revision 4.2  1990/12/06  18:05:24  larry
 * Updated test code to reflect parameter change.
 * Fixed problem with /a/./.dir being simplified to /a and not /a/.dir due
 * to *(cur+2) == *f test instead of the correct cur+2 == f
 *
 * Revision 4.1  90/10/29  14:42:19  larry
 * base MUN version
 * 
 * Revision 3.1.0.4  89/02/16  20:28:36  larry
 * Forgot to set *pathlist to NULL when last changed make_path().
 * 
 * Revision 3.1.0.3  89/02/13  20:29:55  larry
 * Fixed up cd so that it knew when a node from CDPATH was used and would
 * print a message only when really necessary.
 * 
 * Revision 3.1.0.2  89/02/13  17:51:22  larry
 * Merged with Eric Gisin's version.
 * 
 * Revision 3.1.0.1  89/02/13  17:50:58  larry
 * *** empty log message ***
 * 
 * Revision 3.1  89/02/13  17:49:28  larry
 * *** empty log message ***
 * 
 */

#ifdef S_ISLNK
static char	*do_phys_path ARGS((XString *xsp, char *xp, const char *path));
#endif /* S_ISLNK */

/*
 *	Makes a filename into result using the following algorithm.
 *	- make result NULL
 *	- if file starts with '/', append file to result & set cdpathp to NULL
 *	- if file starts with ./ or ../ append cwd and file to result
 *	  and set cdpathp to NULL
 *	- if the first element of cdpathp doesnt start with a '/' xx or '.' xx
 *	  then cwd is appended to result.
 *	- the first element of cdpathp is appended to result
 *	- file is appended to result
 *	- cdpathp is set to the start of the next element in cdpathp (or NULL
 *	  if there are no more elements.
 *	The return value indicates whether a non-null element from cdpathp
 *	was appened to result.
 */
int
make_path(cwd, file, cdpathp, xsp, phys_pathp)
	const char *cwd;
	const char *file;
	char	**cdpathp;	/* & of : seperated list */
	XString	*xsp;
	int	*phys_pathp;
{
	int	rval = 0;
	int	use_cdpath = 1;
	char	*plist;
	int	len;
	int	plen = 0;
	char	*xp = Xstring(*xsp, xp);

	if (!file)
		file = null;

	if (!ISRELPATH(file)) {
		*phys_pathp = 0;
		use_cdpath = 0;
	} else {
		if (file[0] == '.') {
			char c = file[1];

			if (c == '.')
				c = file[2];
			if (ISDIRSEP(c) || c == '\0')
				use_cdpath = 0;
		}

		plist = *cdpathp;
		if (!plist)
			use_cdpath = 0;
		else if (use_cdpath) {
			char *pend;

			for (pend = plist; *pend && *pend != PATHSEP; pend++)
				;
			plen = pend - plist;
			*cdpathp = *pend ? ++pend : (char *) 0;
		}

		if ((use_cdpath == 0 || !plen || ISRELPATH(plist))
		    && (cwd && *cwd))
		{
			len = strlen(cwd);
			XcheckN(*xsp, xp, len);
			memcpy(xp, cwd, len);
			xp += len;
			if (!ISDIRSEP(cwd[len - 1]))
				Xput(*xsp, xp, DIRSEP);
		}
		*phys_pathp = Xlength(*xsp, xp);
		if (use_cdpath && plen) {
			XcheckN(*xsp, xp, plen);
			memcpy(xp, plist, plen);
			xp += plen;
			if (!ISDIRSEP(plist[plen - 1]))
				Xput(*xsp, xp, DIRSEP);
			rval = 1;
		}
	}

	len = strlen(file) + 1;
	XcheckN(*xsp, xp, len);
	memcpy(xp, file, len);

	if (!use_cdpath)
		*cdpathp = (char *) 0;

	return rval;
}

/*
 * Simplify pathnames containing "." and ".." entries.
 * ie, simplify_path("/a/b/c/./../d/..") returns "/a/b"
 */
void
simplify_path(path)
	char	*path;
{
	char	*cur;
	char	*t;
	int	isrooted;
	char	*very_start = path;
	char	*start;

	if (!*path)
		return;

	if ((isrooted = ISROOTEDPATH(path)))
		very_start++;
#ifdef OS2
	if (path[0] && path[1] == ':')	/* skip a: */
		very_start += 2;
#endif /* OS2 */

	/* Before			After
	 *  /foo/			/foo
	 *  /foo/../../bar		/bar
	 *  /foo/./blah/..		/foo
	 *  .				.
	 *  ..				..
	 *  ./foo			foo
	 *  foo/../../../bar		../../bar
	 * OS2:
	 *  a:/foo/../..		a:/
	 *  a:.				a:
	 *  a:..			a:..
	 *  a:foo/../../blah		a:../blah
	 */

	for (cur = t = start = very_start; ; ) {
		/* treat multiple '/'s as one '/' */
		while (ISDIRSEP(*t))
			t++;

		if (*t == '\0') {
			if (cur == path)
				/* convert empty path to dot */
				*cur++ = '.';
			*cur = '\0';
			break;
		}

		if (t[0] == '.') {
			if (!t[1] || ISDIRSEP(t[1])) {
				t += 1;
				continue;
			} else if (t[1] == '.' && (!t[2] || ISDIRSEP(t[2]))) {
				if (!isrooted && cur == start) {
					if (cur != very_start)
						*cur++ = DIRSEP;
					*cur++ = '.';
					*cur++ = '.';
					start = cur;
				} else if (cur != start)
					while (--cur > start && !ISDIRSEP(*cur))
						;
				t += 2;
				continue;
			}
		}

		if (cur != very_start)
			*cur++ = DIRSEP;

		/* find/copy next component of pathname */
		while (*t && !ISDIRSEP(*t))
			*cur++ = *t++;
	}
}


void
set_current_wd(path)
	char *path;
{
	int len;
	char *p = path;

	if (!p && !(p = ksh_get_wd((char *) 0, 0)))
		p = null;

	len = strlen(p) + 1;

	if (len > current_wd_size)
		current_wd = aresize(current_wd, current_wd_size = len, APERM);
	memcpy(current_wd, p, len);
	if (p != path && p != null)
		afree(p, ATEMP);
}

#ifdef S_ISLNK
char *
get_phys_path(path)
	const char *path;
{
	XString xs;
	char *xp;

	Xinit(xs, xp, strlen(path) + 1, ATEMP);

	xp = do_phys_path(&xs, xp, path);

	if (!xp)
		return (char *) 0;

	if (Xlength(xs, xp) == 0)
		Xput(xs, xp, DIRSEP);
	Xput(xs, xp, '\0');

	return Xclose(xs, xp);
}

static char *
do_phys_path(xsp, xp, path)
	XString *xsp;
	char *xp;
	const char *path;
{
	const char *p, *q;
	int len, llen;
	int savepos;
	char lbuf[PATH];

	Xcheck(*xsp, xp);
	for (p = path; p; p = q) {
		while (ISDIRSEP(*p))
			p++;
		if (!*p)
			break;
		len = (q = ksh_strchr_dirsep(p)) ? q - p : strlen(p);
		if (len == 1 && p[0] == '.')
			continue;
		if (len == 2 && p[0] == '.' && p[1] == '.') {
			while (xp > Xstring(*xsp, xp)) {
				xp--;
				if (ISDIRSEP(*xp))
					break;
			}
			continue;
		}

		savepos = Xsavepos(*xsp, xp);
		Xput(*xsp, xp, DIRSEP);
		XcheckN(*xsp, xp, len + 1);
		memcpy(xp, p, len);
		xp += len;
		*xp = '\0';

		llen = readlink(Xstring(*xsp, xp), lbuf, sizeof(lbuf) - 1);
		if (llen < 0) {
			/* EINVAL means it wasn't a symlink... */
			if (errno != EINVAL)
				return (char *) 0;
			continue;
		}
		lbuf[llen] = '\0';

		/* If absolute path, start from scratch.. */
		xp = ISABSPATH(lbuf) ? Xstring(*xsp, xp)
				     : Xrestpos(*xsp, xp, savepos);
		if (!(xp = do_phys_path(xsp, xp, lbuf)))
			return (char *) 0;
	}
	return xp;
}
#endif /* S_ISLNK */

#ifdef	TEST

main(argc, argv)
{
	int	rv;
	char	*cp, cdpath[256], pwd[256], file[256], result[256];

	printf("enter CDPATH: "); gets(cdpath);
	printf("enter PWD: "); gets(pwd);
	while (1) {
		if (printf("Enter file: "), gets(file) == 0)
			return 0;
		cp = cdpath;
		do {
			rv = make_path(pwd, file, &cp, result, sizeof(result));
			printf("make_path returns (%d), \"%s\" ", rv, result);
			simplify_path(result);
			printf("(simpifies to \"%s\")\n", result);
		} while (cp);
	}
}
#endif	/* TEST */
