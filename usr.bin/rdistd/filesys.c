/*	$OpenBSD: filesys.c,v 1.16 2015/01/10 07:56:16 guenther Exp $	*/

/*
 * Copyright (c) 1983 Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/param.h>
#include <sys/mount.h>

#include "defs.h"

/*
 * This file contains functions dealing with getting info
 * about mounted filesystems.
 */


jmp_buf env;

/*
 * Given a pathname, find the fullest component that exists.
 * If statbuf is not NULL, set it to point at our stat buffer.
 */
char *
find_file(char *pathname, struct stat *statbuf, int *isvalid)
{
	static char last_pathname[MAXPATHLEN];
	static char file[MAXPATHLEN + 3];
	static struct stat filestat;
	char *p;

	/*
	 * Mark the statbuf as invalid to start with.
	 */
	*isvalid = 0;

	/*
	 * If this is the same pathname as the last time, and
	 * the file buffer is valid and we're doing the same stat()
	 * or lstat(), then set statbuf to the last filestat and 
	 * return the last file we found.
	 */
	if (strcmp(pathname, last_pathname) == 0 && file[0]) {
		if (statbuf)
			statbuf = &filestat;
		if (strcmp(pathname, file) == 0)
			*isvalid = 1;
		return(file);
	}

	if (strlen(pathname) > sizeof(file) + 3) {
		error("%s: Name to large for buffer.", pathname);
	        return(NULL);
	}

	/*
	 * Save for next time
	 */
	(void) strlcpy(last_pathname, pathname, sizeof(last_pathname));

	if (*pathname == '/')
	        (void) strlcpy(file, pathname, sizeof(file));
	else {
		/*
		 * Ensure we have a directory (".") in our path
		 * so we have something to stat in case the file
		 * does not exist.
		 */
	        (void) strlcpy(file, "./", sizeof(file));
		(void) strlcat(file, pathname, sizeof(file));
	}

	while (lstat(file, &filestat) != 0) {
		/*
		 * Trim the last part of the pathname to try next level up
		 */
		if (errno == ENOENT) {
			/*
			 * Trim file name to get directory name.
			 * Normally we want to change /dir1/dir2/file
			 * into "/dir1/dir2/."
			 */
			if ((p = (char *) strrchr(file, '/')) != NULL) {
				if (strcmp(p, "/.") == 0) {
					*p = CNULL;
				} else {
					*++p = '.';
					*++p = CNULL;
				}
			} else {
				/*
				 * Couldn't find anything, so give up.
				 */
				debugmsg(DM_MISC, "Cannot find dir of `%s'",
					 pathname);
				return(NULL);
			}
			continue;
		} else {
			error("%s: lstat failed: %s", pathname, SYSERR);
			return(NULL);
		}
	}

	if (statbuf)
		bcopy((char *) &filestat, (char *) statbuf, sizeof(filestat));

	/*
	 * Trim the "/." that we added.
	 */
	p = &file[strlen(file) - 1];
	if (*p == '.')
		*p-- = CNULL;
	for ( ; p && *p && *p == '/' && p != file; --p)
		*p = CNULL;

	/*
	 * If this file is a symlink we really want the parent directory
	 * name in case the symlink points to another filesystem.
	 */
	if (S_ISLNK(filestat.st_mode))
		if ((p = (char *) strrchr(file, '/')) && *p+1) {
			/* Is this / (root)? */
			if (p == file)
				file[1] = CNULL;
			else
				*p = CNULL;
		}

	if (strcmp(pathname, file) == 0)
		*isvalid = 1;

	return(*file ? file : NULL);
}

#if defined(NFS_CHECK) || defined(RO_CHECK)

/*
 * Find the device that "filest" is on in the "mntinfo" linked list.
 */
mntent_t *
findmnt(struct stat *filest, struct mntinfo *mntinfo)
{
	struct mntinfo *mi;

	for (mi = mntinfo; mi; mi = mi->mi_nxt) {
		if (mi->mi_mnt->me_flags & MEFLAG_IGNORE)
			continue;
		if (filest->st_dev == mi->mi_statb->st_dev)
			return(mi->mi_mnt);
	}

	return(NULL);
}

/*
 * Is "mnt" a duplicate of any of the mntinfo->mi_mnt elements?
 */
int
isdupmnt(mntent_t *mnt, struct mntinfo *mntinfo)
{
	struct mntinfo *m;

	for (m = mntinfo; m; m = m->mi_nxt)
		if (strcmp(m->mi_mnt->me_path, mnt->me_path) == 0)
			return(1);

	return(0);
}

/*
 * Alarm clock
 */
void
wakeup(int dummy)
{
	debugmsg(DM_CALL, "wakeup() in filesys.c called");
	longjmp(env, 1);
}

/*
 * Make a linked list of mntinfo structures.
 * Use "mi" as the base of the list if it's non NULL.
 */
struct mntinfo *
makemntinfo(struct mntinfo *mi)
{
	static struct mntinfo *mntinfo;
	struct mntinfo *newmi, *m;
	struct stat mntstat;
	mntent_t *mnt;
	int timeo = 310;

	if (!setmountent()) {
		message(MT_NERROR, "setmntent failed: %s", SYSERR);
		return(NULL);
	}

	(void) signal(SIGALRM, wakeup);
	(void) alarm(timeo);
	if (setjmp(env)) {
		message(MT_NERROR, "Timeout getting mount info");
		return(NULL);
	}

	mntinfo = mi;
	while ((mnt = getmountent()) != NULL) {
		debugmsg(DM_MISC, "mountent = '%s' (%s)", 
			 mnt->me_path, mnt->me_type);

		/*
		 * Make sure we don't already have it for some reason
		 */
		if (isdupmnt(mnt, mntinfo))
			continue;

		/*
		 * Get stat info
		 */
		if (stat(mnt->me_path, &mntstat) != 0) {
			message(MT_WARNING, "%s: Cannot stat filesystem: %s", 
				mnt->me_path, SYSERR);
			continue;
		}

		/*
		 * Create new entry
		 */
		newmi = (struct mntinfo *) xcalloc(1, sizeof(struct mntinfo));
		newmi->mi_mnt = newmountent(mnt);
		newmi->mi_statb = 
		    (struct stat *) xcalloc(1, sizeof(struct stat));
		bcopy((char *) &mntstat, (char *) newmi->mi_statb, 
		      sizeof(struct stat));

		/*
		 * Add entry to list
		 */
		if (mntinfo) {
			for (m = mntinfo; m->mi_nxt; m = m->mi_nxt)
				continue;
			m->mi_nxt = newmi;
		} else
			mntinfo = newmi;
	}

	alarm(0);
	endmountent();

	return(mntinfo);
}

/*
 * Given a name like /usr/src/etc/foo.c returns the mntent
 * structure for the file system it lives in.
 *
 * If "statbuf" is not NULL it is used as the stat buffer too avoid
 * stat()'ing the file again back in server.c.
 */
mntent_t *
getmntpt(char *pathname, struct stat *statbuf, int *isvalid)
{
	static struct mntinfo *mntinfo = NULL;
	static struct stat filestat;
	struct stat *pstat;
	struct mntinfo *tmpmi;
	mntent_t *mnt;

	/*
	 * Use the supplied stat buffer if not NULL or our own.
	 */
	if (statbuf) 
		pstat = statbuf;
	else
		pstat = &filestat;

	if (!find_file(pathname, pstat, isvalid))
	        return(NULL);

	/*
	 * Make mntinfo if it doesn't exist.
	 */
	if (!mntinfo)
		mntinfo = makemntinfo(NULL);

	/*
	 * Find the mnt that pathname is on.
	 */
	if ((mnt = findmnt(pstat, mntinfo)) != NULL)
		return(mnt);

	/*
	 * We failed to find correct mnt, so maybe it's a newly
	 * mounted filesystem.  We rebuild mntinfo and try again.
	 */
	if ((tmpmi = makemntinfo(mntinfo)) != NULL) {
		mntinfo = tmpmi;
		if ((mnt = findmnt(pstat, mntinfo)) != NULL)
			return(mnt);
	}

	error("%s: Could not find mount point", pathname);
	return(NULL);
}

#endif /* NFS_CHECK || RO_CHECK */

#if	defined(NFS_CHECK)
/*
 * Is "path" NFS mounted?  Return 1 if it is, 0 if not, or -1 on error.
 */
int
is_nfs_mounted(char *path, struct stat *statbuf, int *isvalid)
{
	mntent_t *mnt;

	if ((mnt = (mntent_t *) getmntpt(path, statbuf, isvalid)) == NULL)
		return(-1);

	/*
	 * We treat "cachefs" just like NFS
	 */
	if ((strcmp(mnt->me_type, METYPE_NFS) == 0) ||
	    (strcmp(mnt->me_type, "cachefs") == 0))
		return(1);

	return(0);
}
#endif	/* NFS_CHECK */

#if	defined(RO_CHECK)
/*
 * Is "path" on a read-only mounted filesystem?  
 * Return 1 if it is, 0 if not, or -1 on error.
 */
int
is_ro_mounted(char *path, struct stat *statbuf, int *isvalid)
{
	mntent_t *mnt;

	if ((mnt = (mntent_t *) getmntpt(path, statbuf, isvalid)) == NULL)
		return(-1);

	if (mnt->me_flags & MEFLAG_READONLY)
		return(1);

	return(0);
}
#endif	/* RO_CHECK */

/*
 * Is "path" a symlink?
 * Return 1 if it is, 0 if not, or -1 on error.
 */
int
is_symlinked(char *path, struct stat *statbuf, int *isvalid)
{
	static struct stat stb;

	if (!(*isvalid)) {
		if (lstat(path, &stb) != 0)
			return(-1);
		statbuf = &stb;
	}
	
	if (S_ISLNK(statbuf->st_mode))
		return(1);

	return(0);
}

/*
 * Get filesystem information for "file".  Set freespace
 * to the amount of free (available) space and number of free
 * files (inodes) on the filesystem "file" resides on.
 * Returns 0 on success or -1 on failure.
 * Filesystem values < 0 indicate unsupported or unavailable
 * information.
 */
int
getfilesysinfo(char *file, int64_t *freespace, int64_t *freefiles)
{
	struct statfs statfsbuf;
	char *mntpt;
	int64_t val;
	int t, r;

	/*
	 * Get the mount point of the file.
	 */
	mntpt = find_file(file, NULL, &t);
	if (!mntpt) {
		debugmsg(DM_MISC, "unknown mount point for `%s'", file);
		return(-1);
	}

	r = statfs(mntpt, &statfsbuf);
	if (r < 0) {
		error("%s: Cannot statfs filesystem: %s.", mntpt, SYSERR);
		return(-1);
	}

	/*
	 * If values are < 0, then assume the value is unsupported
	 * or unavailable for that filesystem type.
	 */
	val = -1;
	if (statfsbuf.f_bavail >= 0)
		val = (statfsbuf.f_bavail * (statfsbuf.f_bsize / 512)) / 2;
	*freespace = val;

	val = -1;
	if (statfsbuf.f_favail >= 0)
		val = statfsbuf.f_favail;
	*freefiles = val;

	return(0);
}
