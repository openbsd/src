/*	$OpenBSD: ldconfig.c,v 1.14 2003/11/21 08:50:38 djm Exp $	*/

/*
 * Copyright (c) 1993,1995 Paul Kranenburg
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
 *	This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <ar.h>
#include <ranlib.h>
#include <a.out.h>
#include <stab.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ld.h"

#undef major
#undef minor

#if DEBUG
/* test */
#undef _PATH_LD_HINTS
#define _PATH_LD_HINTS		"./ld.so.hints"
#endif

extern char			*__progname;

static int			verbose;
static int			nostd;
static int			justread;
static int			merge;
static int			rescan;
static int			unconfig;

struct shlib_list {
	/* Internal list of shared libraries found */
	char			*name;
	char			*path;
	int			dewey[MAXDEWEY];
	int			ndewey;
#define major dewey[0]
#define minor dewey[1]
	struct shlib_list	*next;
};

static struct shlib_list	*shlib_head = NULL, **shlib_tail = &shlib_head;
static char			*dir_list;

static void	enter(char *, char *, char *, int *, int);
static int	dodir(char *, const char *, int);
static int	buildhints(const char *);
static int	readhints(const char *);
static void	listhints(const char *);

int
main(int argc, char *argv[])
{
	int i, c;
	int rval = 0;
	char path[PATH_MAX], *strip = NULL;

	strlcpy(path, _PATH_LD_HINTS, sizeof(path));
	while ((c = getopt(argc, argv, "RUmrsvf:S:")) != -1) {
		switch (c) {
		case 'f':
			strlcpy(path, optarg, sizeof(path));
			break;
		case 'R':
			rescan = 1;
			break;
		case 'S':
			strip = optarg;
			break;
		case 'U':
			rescan = unconfig = 1;
			break;
		case 'm':
			merge = 1;
			break;
		case 'r':
			justread = 1;
			break;
		case 's':
			nostd = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			fprintf(stderr, "usage: %s [-RUmrsv] [-f hints_file] "
			    "[-S strip_path] [dir ...]\n", __progname);
			exit(1);
			break;
		}
	}

	if (unconfig && merge)
		errx(1, "cannot use -U with -m");

	dir_list = xmalloc(1);
	*dir_list = '\0';

	if (justread || merge || rescan) {
		if ((rval = readhints(path)) != 0)
			return rval;
		if (justread) {
			listhints(path);
			return 0;
		}
		add_search_path(dir_list);
		dir_list = xrealloc(dir_list, 1);
		*dir_list = '\0';
	} else
		if (!nostd)
			std_search_path();

	if (unconfig) {
		if (optind < argc)
			for (i = optind; i < argc; i++)
				remove_search_dir(argv[i]);
		else {
			i = 0;
			while (i < n_search_dirs) {
				if (access(search_dirs[i], R_OK) < 0)
					remove_search_dir(search_dirs[i]);
				else
					i++;
			}
		}
	} else
		for (i = optind; i < argc; i++)
			add_search_dir(argv[i]);

	for (i = 0; i < n_search_dirs; i++) {
		char *cp = concat(dir_list, *dir_list?":":"", search_dirs[i]);

		free(dir_list);
		dir_list = cp;
		rval |= dodir(search_dirs[i], strip, 0);
	}

	rval |= buildhints(path);

	return rval;
}

int
dodir(char *dir, const char *strip, int silent)
{
	DIR		*dd;
	struct dirent	*dp;
	char		name[MAXPATHLEN];
	int		dewey[MAXDEWEY], ndewey, doff = 0;

	if (strip != NULL && strncmp(dir, strip, strlen(strip)) == 0)
		doff = strlen(strip);

	if ((dd = opendir(dir)) == NULL) {
		if (!silent || errno != ENOENT)
			warn("%s", dir);
		return -1;
	}

	while ((dp = readdir(dd)) != NULL) {
		int n;
		char *cp;

		/* Check for `lib' prefix */
		if (dp->d_name[0] != 'l' ||
		    dp->d_name[1] != 'i' ||
		    dp->d_name[2] != 'b')
			continue;

		/* Copy the entry minus prefix */
		(void)strlcpy(name, dp->d_name + 3, sizeof name);
		n = strlen(name);
		if (n < 4)
			continue;

		/* Find ".so." in name */
		for (cp = name + n - 4; cp > name; --cp) {
			if (cp[0] == '.' &&
			    cp[1] == 's' &&
			    cp[2] == 'o' &&
			    cp[3] == '.')
				break;
		}
		if (cp <= name)
			continue;

		*cp = '\0';
		if (!isdigit(*(cp+4)))
			continue;

		bzero((caddr_t)dewey, sizeof(dewey));
		ndewey = getdewey(dewey, cp + 4);
		enter(dir + doff, dp->d_name, name, dewey, ndewey);
	}
	return 0;
}

static void
enter(char *dir, char *file, char *name, int dewey[], int ndewey)
{
	struct shlib_list	*shp;

	for (shp = shlib_head; shp; shp = shp->next) {
		if (strcmp(name, shp->name) != 0 || major != shp->major)
			continue;

		/* Name matches existing entry */
		if (cmpndewey(dewey, ndewey, shp->dewey, shp->ndewey) > 0) {

			/* Update this entry with higher versioned lib */
			if (verbose)
				printf("Updating lib%s.%d.%d to %s/%s\n",
				    shp->name, shp->major, shp->minor,
				    dir, file);

			free(shp->name);
			shp->name = xstrdup(name);
			free(shp->path);
			shp->path = concat(dir, "/", file);
			bcopy(dewey, shp->dewey, sizeof(shp->dewey));
			shp->ndewey = ndewey;
		}
		break;
	}

	if (shp)
		/* Name exists: older version or just updated */
		return;

	/* Allocate new list element */
	if (verbose)
		printf("Adding %s/%s\n", dir, file);

	shp = (struct shlib_list *)xmalloc(sizeof *shp);
	shp->name = xstrdup(name);
	shp->path = concat(dir, "/", file);
	bcopy(dewey, shp->dewey, MAXDEWEY);
	shp->ndewey = ndewey;
	shp->next = NULL;

	*shlib_tail = shp;
	shlib_tail = &shp->next;
}

static int
hinthash(char *cp, int vmajor, int vminor)
{
	int	k = 0;

	while (*cp)
		k = (((k << 1) + (k >> 14)) ^ (*cp++)) & 0x3fff;

	k = (((k << 1) + (k >> 14)) ^ (vmajor*257)) & 0x3fff;
#if 0
	k = (((k << 1) + (k >> 14)) ^ (vminor*167)) & 0x3fff;
#endif

	return k;
}

int
buildhints(const char *path)
{
	int strtab_sz = 0, nhints = 0, fd, i, n, str_index = 0;
	struct hints_bucket *blist;
	struct hints_header hdr;
	struct shlib_list *shp;
	char *strtab, *tmpfile;

	for (shp = shlib_head; shp; shp = shp->next) {
		strtab_sz += 1 + strlen(shp->name);
		strtab_sz += 1 + strlen(shp->path);
		nhints++;
	}

	/* Fill hints file header */
	hdr.hh_magic = HH_MAGIC;
	hdr.hh_version = LD_HINTS_VERSION_2;
	hdr.hh_nbucket = 1 * nhints;
	n = hdr.hh_nbucket * sizeof(struct hints_bucket);
	hdr.hh_hashtab = sizeof(struct hints_header);
	hdr.hh_strtab = hdr.hh_hashtab + n;
	hdr.hh_dirlist = strtab_sz;
	strtab_sz += 1 + strlen(dir_list);
	hdr.hh_strtab_sz = strtab_sz;
	hdr.hh_ehints = hdr.hh_strtab + hdr.hh_strtab_sz;

	if (verbose)
		printf("Totals: entries %d, buckets %ld, string size %d\n",
		    nhints, hdr.hh_nbucket, strtab_sz);

	/* Allocate buckets and string table */
	blist = (struct hints_bucket *)xmalloc(n);
	bzero((char *)blist, n);
	for (i = 0; i < hdr.hh_nbucket; i++)
		/* Empty all buckets */
		blist[i].hi_next = -1;

	strtab = (char *)xmalloc(strtab_sz);

	/* Enter all */
	for (shp = shlib_head; shp; shp = shp->next) {
		struct hints_bucket	*bp;

		bp = blist + (hinthash(shp->name, shp->major, shp->minor) %
		    hdr.hh_nbucket);

		if (bp->hi_pathx) {
			int	i;

			for (i = 0; i < hdr.hh_nbucket; i++) {
				if (blist[i].hi_pathx == 0)
					break;
			}
			if (i == hdr.hh_nbucket) {
				warnx("Bummer!");
				return -1;
			}
			while (bp->hi_next != -1)
				bp = &blist[bp->hi_next];
			bp->hi_next = i;
			bp = blist + i;
		}

		/* Insert strings in string table */
		bp->hi_namex = str_index;
		strlcpy(strtab + str_index, shp->name, strtab_sz - str_index);
		str_index += 1 + strlen(shp->name);

		bp->hi_pathx = str_index;
		strlcpy(strtab + str_index, shp->path, strtab_sz - str_index);
		str_index += 1 + strlen(shp->path);

		/* Copy versions */
		bcopy(shp->dewey, bp->hi_dewey, sizeof(bp->hi_dewey));
		bp->hi_ndewey = shp->ndewey;
	}

	/* Copy search directories */
	strlcpy(strtab + str_index, dir_list, strtab_sz - str_index);
	str_index += 1 + strlen(dir_list);

	/* Sanity check */
	if (str_index != strtab_sz) {
		errx(1, "str_index(%d) != strtab_sz(%d)", str_index, strtab_sz);
	}

	tmpfile = concat(path, ".XXXXXXXXXX", "");
	if ((fd = mkstemp(tmpfile)) == -1) {
		warn("%s", tmpfile);
		return -1;
	}
	fchmod(fd, 0444);

	if (write(fd, &hdr, sizeof(struct hints_header)) !=
	    sizeof(struct hints_header)) {
		warn("%s", path);
		return -1;
	}
	if (write(fd, blist, hdr.hh_nbucket * sizeof(struct hints_bucket)) !=
	    hdr.hh_nbucket * sizeof(struct hints_bucket)) {
		warn("%s", path);
		return -1;
	}
	if (write(fd, strtab, strtab_sz) != strtab_sz) {
		warn("%s", path);
		return -1;
	}
	if (close(fd) != 0) {
		warn("%s", path);
		return -1;
	}

	/* Install it */
	if (unlink(path) != 0 && errno != ENOENT) {
		warn("%s", path);
		return -1;
	}

	if (rename(tmpfile, path) != 0) {
		warn("%s", path);
		return -1;
	}

	return 0;
}

static int
readhints(const char *path)
{
	struct hints_bucket *blist;
	struct hints_header *hdr;
	struct shlib_list *shp;
	caddr_t addr;
	char *strtab;
	long msize;
	int fd, i;

	if ((fd = open(path, O_RDONLY, 0)) == -1) {
		warn("%s", path);
		return -1;
	}

	msize = PAGSIZ;
	addr = mmap(0, msize, PROT_READ, MAP_PRIVATE, fd, 0);

	if (addr == MAP_FAILED) {
		warn("%s", path);
		return -1;
	}

	hdr = (struct hints_header *)addr;
	if (HH_BADMAG(*hdr)) {
		warnx("%s: Bad magic: %lo",
		    path, hdr->hh_magic);
		return -1;
	}

	if (hdr->hh_version != LD_HINTS_VERSION_2) {
		warnx("Unsupported version: %ld", hdr->hh_version);
		return -1;
	}

	if (hdr->hh_ehints > msize) {
		if (mmap(addr+msize, hdr->hh_ehints - msize,
		    PROT_READ, MAP_PRIVATE|MAP_FIXED,
		    fd, msize) != (caddr_t)(addr+msize)) {

			warn("%s", path);
			return -1;
		}
	}
	close(fd);

	blist = (struct hints_bucket *)(addr + hdr->hh_hashtab);
	strtab = (char *)(addr + hdr->hh_strtab);

	dir_list = xstrdup(strtab + hdr->hh_dirlist);

	if (rescan)
		return (0);

	for (i = 0; i < hdr->hh_nbucket; i++) {
		struct hints_bucket	*bp = &blist[i];

		/* Sanity check */
		if (bp->hi_namex >= hdr->hh_strtab_sz) {
			warnx("Bad name index: %#x", bp->hi_namex);
			return -1;
		}
		if (bp->hi_pathx >= hdr->hh_strtab_sz) {
			warnx("Bad path index: %#x", bp->hi_pathx);
			return -1;
		}

		/* Allocate new list element */
		shp = (struct shlib_list *)xmalloc(sizeof *shp);
		shp->name = xstrdup(strtab + bp->hi_namex);
		shp->path = xstrdup(strtab + bp->hi_pathx);
		bcopy(bp->hi_dewey, shp->dewey, sizeof(shp->dewey));
		shp->ndewey = bp->hi_ndewey;
		shp->next = NULL;

		*shlib_tail = shp;
		shlib_tail = &shp->next;
	}
	return 0;
}

static void
listhints(const char *path)
{
	struct shlib_list *shp;
	int i;

	printf("%s:\n", path);
	printf("\tsearch directories: %s\n", dir_list);

	for (i = 0, shp = shlib_head; shp; i++, shp = shp->next)
		printf("\t%d:-l%s.%d.%d => %s\n",
		    i, shp->name, shp->major, shp->minor, shp->path);
}
