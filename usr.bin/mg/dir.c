/*	$OpenBSD: dir.c,v 1.28 2015/03/19 21:22:15 bcallah Exp $	*/

/* This file is in the public domain. */

/*
 * Name:	MG 2a
 *		Directory management functions
 * Created:	Ron Flax (ron@vsedev.vse.com)
 *		Modified for MG 2a by Mic Kaczmarczik 03-Aug-1987
 */

#include <sys/queue.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "def.h"

static char	 mgcwd[NFILEN];

/*
 * Initialize anything the directory management routines need.
 */
void
dirinit(void)
{
	mgcwd[0] = '\0';
	if (getcwd(mgcwd, sizeof(mgcwd)) == NULL) {
		ewprintf("Can't get current directory!");
		chdir("/");
	}
	if (!(mgcwd[0] == '/' && mgcwd [1] == '\0'))
		(void)strlcat(mgcwd, "/", sizeof(mgcwd));
}

/*
 * Change current working directory.
 */
/* ARGSUSED */
int
changedir(int f, int n)
{
	char	bufc[NFILEN], *bufp;

	(void)strlcpy(bufc, mgcwd, sizeof(bufc));
	if ((bufp = eread("Change default directory: ", bufc, NFILEN,
	    EFDEF | EFNEW | EFCR | EFFILE)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	/* Append trailing slash */
	if (chdir(bufc) == -1) {
		dobeep();
		ewprintf("Can't change dir to %s", bufc);
		return (FALSE);
	}
	if ((bufp = getcwd(mgcwd, sizeof(mgcwd))) == NULL)
		panic("Can't get current directory!");
	if (mgcwd[strlen(mgcwd) - 1] != '/')
		(void)strlcat(mgcwd, "/", sizeof(mgcwd));
	ewprintf("Current directory is now %s", bufp);
	return (TRUE);
}

/*
 * Show current directory.
 */
/* ARGSUSED */
int
showcwdir(int f, int n)
{
	ewprintf("Current directory: %s", mgcwd);
	return (TRUE);
}

int
getcwdir(char *buf, size_t len)
{
	if (strlcpy(buf, mgcwd, len) >= len)
		return (FALSE);

	return (TRUE);
}

/* Create the directory and it's parents. */
/* ARGSUSED */
int
makedir(int f, int n)
{
	return (ask_makedir());
}

int
ask_makedir(void)
{

	char		 bufc[NFILEN];
	char		*path;

	if (getbufcwd(bufc, sizeof(bufc)) != TRUE)
		return (ABORT);
	if ((path = eread("Make directory: ", bufc, NFILEN,
	    EFDEF | EFNEW | EFCR | EFFILE)) == NULL)
		return (ABORT);
	else if (path[0] == '\0')
		return (FALSE);

	return (do_makedir(path));
}

int
do_makedir(char *path)
{
	struct stat	 sb;
	int		 finished, ishere;
	mode_t		 dir_mode, mode, oumask;
	char		*slash;

	if ((path = adjustname(path, TRUE)) == NULL)
		return (FALSE);

	/* Remove trailing slashes */
	slash = strrchr(path, '\0');
	while (--slash > path && *slash == '/')
		*slash = '\0';

	slash = path;

	oumask = umask(0);
	mode = 0777 & ~oumask;
	dir_mode = mode | S_IWUSR | S_IXUSR;

	for (;;) {
		slash += strspn(slash, "/");
		slash += strcspn(slash, "/");

		finished = (*slash == '\0');
		*slash = '\0';

		ishere = !stat(path, &sb);
		if (finished && ishere) {
			dobeep();
			ewprintf("Cannot create directory %s: file exists",
			     path);
			return(FALSE);
		} else if (!finished && ishere && S_ISDIR(sb.st_mode)) {
			*slash = '/';
			continue;
		}

		if (mkdir(path, finished ? mode : dir_mode) == 0) {
			if (mode > 0777 && chmod(path, mode) < 0) {
				umask(oumask);
				return (ABORT);
			}
		} else {
			if (!ishere || !S_ISDIR(sb.st_mode)) {
				if (!ishere) {
					dobeep();
					ewprintf("Creating directory: "
					    "permission denied, %s", path);
				} else
					eerase();

				umask(oumask);
				return (FALSE);
			}
		}

		if (finished)
			break;

		*slash = '/';
	}

	eerase();
	umask(oumask);
	return (TRUE);
}
