/*	$OpenBSD: fileio.c,v 1.20 2001/09/21 15:08:16 wilfried Exp $	*/

/*
 *	POSIX fileio.c
 */
#include	"def.h"

static FILE	*ffp;

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

/*
 * Open a file for reading.
 */
int
ffropen(fn, bp)
	char	*fn;
	BUFFER	*bp;
{
	struct stat	statbuf;

	if ((ffp = fopen(fn, "r")) == NULL)
		return (FIOFNF);
	if (bp && fstat(fileno(ffp), &statbuf) == 0) {
		/* set highorder bit to make sure this isn't all zero */
		bp->b_fi.fi_mode = statbuf.st_mode | 0x8000;
		bp->b_fi.fi_uid = statbuf.st_uid;
		bp->b_fi.fi_gid = statbuf.st_gid;
	}
	return (FIOSUC);
}

/*
 * Open a file for writing.
 * Return TRUE if all is well, and
 * FALSE on error (cannot create).
 */
int
ffwopen(fn, bp)
	char   *fn;
	BUFFER *bp;
{

	if ((ffp = fopen(fn, "w")) == NULL) {
		ewprintf("Cannot open file for writing");
		return (FIOERR);
	}

	/*
	 * If we have file information, use it.  We don't bother to check for
	 * errors, because there's no a lot we can do about it.  Certainly
	 * trying to change ownership will fail if we aren' root.  That's
	 * probably OK.  If we don't have info, no need to get it, since any
	 * future writes will do the same thing.
	 */
	if (bp && bp->b_fi.fi_mode) {
		chmod(fn, bp->b_fi.fi_mode & 07777);
		chown(fn, bp->b_fi.fi_uid, bp->b_fi.fi_gid);
	}
	return (FIOSUC);
}

/*
 * Close a file.
 * XXX - Should look at the status.
 */
/* ARGSUSED */
int
ffclose(bp)
	BUFFER *bp;
{

	(void) fclose(ffp);
	return (FIOSUC);
}

/*
 * Write a buffer to the already
 * opened file. bp points to the
 * buffer. Return the status.
 * Check only at the newline and
 * end of buffer.
 */
int
ffputbuf(bp)
	BUFFER *bp;
{
	char   *cp;
	char   *cpend;
	LINE   *lp;
	LINE   *lpend;

	lpend = bp->b_linep;
	lp = lforw(lpend);
	do {
		cp = &ltext(lp)[0];		/* begining of line	 */
		cpend = &cp[llength(lp)];	/* end of line		 */
		while (cp != cpend) {
			putc(*cp, ffp);
			cp++;			/* putc may evaluate arguments
						   more than once */
		}
		lp = lforw(lp);
		if (lp == lpend)
			break;			/* no implied \n on last line */
		putc('\n', ffp);
	} while (!ferror(ffp));
	if (ferror(ffp)) {
		ewprintf("Write I/O error");
		return FIOERR;
	}
	return (FIOSUC);
}

/*
 * Read a line from a file, and store the bytes
 * in the supplied buffer. Stop on end of file or end of
 * line.  When FIOEOF is returned, there is a valid line
 * of data without the normally implied \n.
 */
int
ffgetline(buf, nbuf, nbytes)
	char	*buf;
	int	nbuf;
	int	*nbytes;
{
	int	c, i;

	i = 0;
	while ((c = getc(ffp)) != EOF && c != '\n') {
		buf[i++] = c;
		if (i >= nbuf)
			return FIOLONG;
	}
	if (c == EOF && ferror(ffp) != FALSE) {
		ewprintf("File read error");
		return FIOERR;
	}
	*nbytes = i;
	return c == EOF ? FIOEOF : FIOSUC;
}

#ifndef NO_BACKUP
/*
 * Make a backup copy of "fname".  On Unix the backup has the same
 * name as the original file, with a "~" on the end; this seems to
 * be newest of the new-speak. The error handling is all in "file.c".
 * We do a copy instead of a rename since otherwise another process
 * with an open fd will get the backup, not the new file.  This is
 * a problem when using mg with things like crontab and vipw.
 */
int
fbackupfile(fn)
	char  *fn;
{
	struct stat	sb;
	int		from, to, serrno;
	size_t		nread;
	size_t		len;
	char		buf[BUFSIZ];
	char		*nname;

	len = strlen(fn);
	if ((nname = malloc(len + 1 + 1)) == NULL) {
		ewprintf("Can't get %d bytes", len + 1 + 1);
		return (ABORT);
	}
	(void) strcpy(nname, fn);
	(void) strcpy(nname + len, "~");

	if (stat(fn, &sb) == -1) {
		ewprintf("Can't stat %s", fn);
		return (FALSE);
	}

	if ((from = open(fn, O_RDONLY)) == -1)
		return (FALSE);
	to = open(nname, O_WRONLY|O_CREAT|O_TRUNC, (sb.st_mode & 0777));
	if (to == -1) {
		serrno = errno;
		close(from);
		errno = serrno;
		return (FALSE);
	}
	while ((nread = read(from, buf, sizeof(buf))) > 0) {
		if (write(to, buf, nread) != nread) {
		    nread = -1;
		    break;
		}
	}
	serrno = errno;
	close(from);
	close(to);
	if (nread == -1)
		unlink(nname);
	free(nname);
	errno = serrno;
	return (nread == -1 ? FALSE : TRUE);
}
#endif

/*
 * The string "fn" is a file name.
 * Perform any required appending of directory name or case adjustments.
 * If NO_DIR is not defined, the same file should be refered to even if the
 * working directory changes.
 */
#ifdef SYMBLINK
#include <sys/types.h>
#include <sys/stat.h>
#ifndef MAXLINK
#define MAXLINK 8		/* maximum symbolic links to follow */
#endif
#endif
#include <pwd.h>
#ifndef NO_DIR
extern char	*wdir;
#endif

char *
adjustname(fn)
	char	*fn;
{
	char		*cp;
	static char	fnb[NFILEN];
	struct passwd	*pwent;
#ifdef	SYMBLINK
	struct stat	statbuf;
	int		i, j;
	char		linkbuf[NFILEN];
#endif

	switch (*fn) {
	case '/':
		cp = fnb;
		*cp++ = *fn++;
		break;
	case '~':
		fn++;
		cp = getenv("HOME");
		if (cp != NULL && *cp != '\0' && (*fn == '/' || *fn == '\0')) {
			cp = fnb + strlcpy(fnb, cp, sizeof(fnb));
			if (*fn)
				fn++;
			break;
		} else {
			cp = fnb;
			while (*fn && *fn != '/')
				*cp++ = *fn++;
			*cp = '\0';
			if ((pwent = getpwnam(fnb)) != NULL) {
				cp = fnb + strlcpy(fnb, pwent->pw_dir, sizeof(fnb));
				break;
			} else {
				fn -= strlen(fnb) + 1;
				/* can't find ~user, continue to default case */
			}
		}
	default:
#ifndef	NODIR
		cp = fnb + strlcpy(fnb, wdir, sizeof(fnb));
		break;
#else
		return fn;	/* punt */
#endif
	}
	if (cp != fnb && cp[-1] != '/')
		*cp++ = '/';
	while (*fn) {
		switch (*fn) {
		case '.':
			switch (fn[1]) {
			case '\0':
				*--cp = '\0';
				return fnb;
			case '/':
				fn += 2;
				continue;
			case '.':
				if (fn[2] != '/' && fn[2] != '\0')
					break;
#ifdef SYMBLINK
				cp[-1] = '\0';
				for (j = MAXLINK; j-- &&
				     lstat(fnb, &statbuf) != -1 &&
				     (statbuf.st_mode & S_IFMT) == S_IFLNK &&
				     (i = readlink(fnb, linkbuf, sizeof linkbuf))
				     != -1;) {
					if (linkbuf[0] != '/') {
						--cp;
						while (cp > fnb && *--cp != '/') {
						}
						++cp;
						(void) strncpy(cp, linkbuf, i);
						cp += i;
					} else {
						(void) strncpy(fnb, linkbuf, i);
						cp = fnb + i;
					}
					if (cp[-1] != '/')
						*cp++ = '\0';
					else
						cp[-1] = '\0';
				}
				cp[-1] = '/';
#endif
				--cp;
				while (cp > fnb && *--cp != '/') {
				}
				++cp;
				if (fn[2] == '\0') {
					*--cp = '\0';
					return fnb;
				}
				fn += 3;
				continue;
			default:
				break;
			}
			break;
		case '/':
			fn++;
			continue;
		default:
			break;
		}
		while (*fn && (*cp++ = *fn++) != '/') {
		}
	}
	if (cp[-1] == '/')
		--cp;
	*cp = '\0';
	return fnb;
}

#ifndef NO_STARTUP
/*
 * Find a startup file for the user and return its name. As a service
 * to other pieces of code that may want to find a startup file (like
 * the terminal driver in particular), accepts a suffix to be appended
 * to the startup file name.
 */
char *
startupfile(suffix)
	char	*suffix;
{
	static char	file[NFILEN];
	char		*home;

	if ((home = getenv("HOME")) == NULL || *home == '\0')
		goto nohome;

	if (suffix == NULL) {
		if (snprintf(file, sizeof(file), "%s/.mg", home)
			    >= sizeof(file))
			return NULL;
	} else {
		if (snprintf(file, sizeof(file), "%s/.mg-%s", home, suffix)
			    >= sizeof(file))
			return NULL;
	}

	if (access(file, R_OK) == 0)
		return file;
nohome:
#ifdef STARTUPFILE
	if (suffix == NULL)
		if (snprintf(file, sizeof(file), "%s", STARTUPFILE)
			    >= sizeof(file))
			return NULL;
	} else {
		if (snprintf(file, sizeof(file), "%s%s", STARTUPFILE, suffix)
			    >= sizeof(file))
			return NULL;
	}

	if (access(file, R_OK) == 0)
		return file;
#endif
	return NULL;
}
#endif

#ifndef NO_DIRED
#include <sys/wait.h>
#include "kbd.h"

int
copy(frname, toname)
	char	*frname;
	char	*toname;
{
	pid_t	pid;
	int	status;

	switch ((pid = vfork())) {
	case -1:
		return -1;
	case 0:
		execl("/bin/cp", "cp", frname, toname, (char *)NULL);
		_exit(1);	/* shouldn't happen */
	default:
		waitpid(pid, &status, 0);
		return (WIFEXITED(status) && WEXITSTATUS(status) == 0);
	}
}

BUFFER *
dired_(dirname)
	char	*dirname;
{
	BUFFER	*bp;
	FILE	*dirpipe;
	char	line[256];
	int	len;

	if ((dirname = adjustname(dirname)) == NULL) {
		ewprintf("Bad directory name");
		return NULL;
	}
	/* this should not be done, instead adjustname() should get a flag */
	len = strlen(dirname);
	if (dirname[len - 1] != '/') {
		dirname[len++] = '/';
		dirname[len] = '\0';
	}
	if ((bp = findbuffer(dirname)) == NULL) {
		ewprintf("Could not create buffer");
		return NULL;
	}
	if (bclear(bp) != TRUE)
		return FALSE;
	if (snprintf(line, sizeof(line), "ls -al %s", dirname) >= sizeof(line)){
		ewprintf("Path too long");
		return NULL;
	}
	if ((dirpipe = popen(line, "r")) == NULL) {
		ewprintf("Problem opening pipe to ls");
		return NULL;
	}
	line[0] = line[1] = ' ';
	while (fgets(&line[2], sizeof(line) - 2, dirpipe) != NULL) {
		line[strlen(line) - 1] = '\0';	/* remove ^J	 */
		(void) addline(bp, line);
	}
	if (pclose(dirpipe) == -1) {
		ewprintf("Problem closing pipe to ls");
		return NULL;
	}
	bp->b_dotp = lforw(bp->b_linep);	/* go to first line */
	(void) strncpy(bp->b_fname, dirname, NFILEN);
	if ((bp->b_modes[0] = name_mode("dired")) == NULL) {
		bp->b_modes[0] = name_mode("fundamental");
		ewprintf("Could not find mode dired");
		return NULL;
	}
	bp->b_nmodes = 0;
	return bp;
}

int
d_makename(lp, fn)
	LINE  *lp;
	char  *fn;
{
	char  *cp;

	if (llength(lp) <= 56)
		return ABORT;
	(void) strcpy(fn, curbp->b_fname);
	cp = fn + strlen(fn);
	bcopy(&lp->l_text[56], cp, llength(lp) - 56);
	cp[llength(lp) - 56] = '\0';
	return lgetc(lp, 2) == 'd';
}
#endif				/* NO_DIRED */

struct filelist {
	LIST	fl_l;
	char	fl_name[NFILEN + 2];
};

/*
 * return list of file names that match the name in buf.
 */

LIST *
make_file_list(buf)
	char	*buf;
{
	char		*dir, *file, *cp;
	int		len, preflen;
	DIR		*dirp;
	struct dirent	*dent;
	LIST		*last;
	struct filelist *current;
	char		prefixx[NFILEN + 1];

	/*
	 * We need three different strings: dir - the name of the directory
	 * containing what the user typed. Must be a real unix file name,
	 * e.g. no ~user, etc..  Must not end in /. prefix - the portion of
	 * what the user typed that is before the names we are going to find
	 * in the directory.  Must have a trailing / if the user typed it.
	 * names from the directory. we open dir, and return prefix
	 * concatenated with names.
	 */

	/* first we get a directory name we can look up */
	/*
	 * Names ending in . are potentially odd, because adjustname will
	 * treat foo/.. as a reference to another directory, whereas we are
	 * interested in names starting with ..
	 */
	len = strlen(buf);
	if (buf[len - 1] == '.') {
		buf[len - 1] = 'x';
		dir = adjustname(buf);
		buf[len - 1] = '.';
	} else
		dir = adjustname(buf);
	/*
	 * If the user typed a trailing / or the empty string
	 * he wants us to use his file spec as a directory name.
	 */
	if (buf[0] && buf[strlen(buf) - 1] != '/') {
		file = strrchr(dir, '/');
		if (file) {
			*file = 0;
			if (*dir == 0)
				dir = "/";
		} else {
			return (NULL);
		}
	}
	/* Now we get the prefix of the name the user typed. */
	strcpy(prefixx, buf);
	cp = strrchr(prefixx, '/');
	if (cp == NULL)
		prefixx[0] = 0;
	else
		cp[1] = 0;

	preflen = strlen(prefixx);
	/* cp is the tail of buf that really needs to be compared */
	cp = buf + preflen;
	len = strlen(cp);

	/*
	 * Now make sure that file names will fit in the buffers allocated.
	 * SV files are fairly short.  For BSD, something more general would
	 * be required.
	 */
	if ((preflen + MAXNAMLEN) > NFILEN)
		return (NULL);

	/* loop over the specified directory, making up the list of files */

	/*
	 * Note that it is worth our time to filter out names that don't
	 * match, even though our caller is going to do so again, and to
	 * avoid doing the stat if completion is being done, because stat'ing
	 * every file in the directory is relatively expensive.
	 */

	dirp = opendir(dir);
	if (dirp == NULL)
		return (NULL);
	last = NULL;

	while ((dent = readdir(dirp)) != NULL) {
		int isdir;

		if (dent->d_namlen < len || memcmp(cp, dent->d_name, len) != 0)
			continue;

		isdir = 0;
		if (dent->d_type == DT_DIR) {
			isdir = 1;
		} else if (dent->d_type == DT_LNK ||
			    dent->d_type == DT_UNKNOWN) {
			struct stat	statbuf;
			char		statname[NFILEN + 2];

			statbuf.st_mode = 0;
			if (snprintf(statname, sizeof(statname), "%s/%s",
			    dir, dent->d_name) > sizeof(statname) - 1) {
				continue;
			}
			if (stat(statname, &statbuf) < 0)
				continue;
			if (statbuf.st_mode & S_IFDIR)
				isdir = 1;
		}

		current = malloc(sizeof(struct filelist));
		if (current == NULL)
			break;

		if (snprintf(current->fl_name, sizeof(current->fl_name),
		    "%s%s%s", prefixx, dent->d_name, isdir ? "/" : "")
		    >= sizeof(current->fl_name)) {
			free(current);
			continue;
		}
		current->fl_l.l_next = last;
		current->fl_l.l_name = current->fl_name;
		last = (LIST *) current;
	}
	closedir(dirp);

	return (last);
}
