/*	$OpenBSD: editor.c,v 1.235 2010/04/23 15:25:21 jsing Exp $	*/

/*
 * Copyright (c) 1997-2000 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#define	DKTYPENAMES
#include <sys/disklabel.h>

#include <ufs/ffs/fs.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "extern.h"
#include "pathnames.h"

/* flags for getuint() */
#define	DO_CONVERSIONS	0x00000001
#define	DO_ROUNDING	0x00000002

#ifndef NUMBOOT
#define NUMBOOT 0
#endif

/* structure to describe a portion of a disk */
struct diskchunk {
	u_int64_t start;
	u_int64_t stop;
};

/* used when sorting mountpoints in mpsave() */
struct mountinfo {
	char *mountpoint;
	int partno;
};

/* used when allocating all space according to recommendations */

struct space_allocation {
	daddr64_t	minsz;	/* starts as blocks, xlated to sectors. */
	daddr64_t	maxsz;	/* starts as blocks, xlated to sectors. */
	int		rate;	/* % of extra space to use */
	char 	       *mp;
};

/* entries for swap and var are changed by editor_allocspace() */
const struct space_allocation alloc_big[] = {
	{   MEG(80),         GIG(1),   5, "/"		},
	{   MEG(80),       MEG(256),   5, "swap"	},
	{  MEG(120),         GIG(4),   8, "/tmp"	},
	{   MEG(80),         GIG(4),  13, "/var"	},
	{  MEG(900),         GIG(2),   5, "/usr"	},
	{  MEG(512),         GIG(1),   3, "/usr/X11R6"	},
	{    GIG(2),         GIG(6),   5, "/usr/local"	},
	{    GIG(1),         GIG(2),   3, "/usr/src"	},
	{    GIG(1),         GIG(2),   3, "/usr/obj"	},
	{    GIG(1),       GIG(300),  50, "/home"	}
	/* Anything beyond this leave for the user to decide */
};

const struct space_allocation alloc_medium[] = {
	{  MEG(800),         GIG(2),   5, "/"		},
	{   MEG(80),       MEG(256),  10, "swap"	},
	{  MEG(900),         GIG(3),  78, "/usr"	},
	{  MEG(256),         GIG(2),   7, "/home"	}
};

const struct space_allocation alloc_small[] = {
	{  MEG(700),         GIG(4),  95, "/"		},
	{    MEG(1),       MEG(256),   5, "swap"	}
};

const struct space_allocation alloc_stupid[] = {
	{    MEG(1),      MEG(2048), 100, "/"		}
};

#ifndef nitems
#define nitems(_a)	(sizeof((_a)) / sizeof((_a)[0]))
#endif

const struct {
	const struct space_allocation *table;
	int sz;
} alloc_table[] = {
	{ alloc_big,	nitems(alloc_big) },
	{ alloc_medium,	nitems(alloc_medium) },
	{ alloc_small,	nitems(alloc_small) },
	{ alloc_stupid,	nitems(alloc_stupid) }
};

void	edit_parms(struct disklabel *);
void	editor_resize(struct disklabel *, char *);
void	editor_add(struct disklabel *, char *);
void	editor_change(struct disklabel *, char *);
u_int64_t editor_countfree(struct disklabel *);
void	editor_delete(struct disklabel *, char *);
void	editor_help(void);
void	editor_modify(struct disklabel *, char *);
void	editor_name(struct disklabel *, char *);
char	*getstring(char *, char *, char *);
u_int64_t getuint(struct disklabel *, char *, char *, u_int64_t, u_int64_t, u_int64_t, int);
int	has_overlap(struct disklabel *);
int	partition_cmp(const void *, const void *);
struct partition **sort_partitions(struct disklabel *);
void	getdisktype(struct disklabel *, char *, char *);
void	find_bounds(struct disklabel *);
void	set_bounds(struct disklabel *);
struct diskchunk *free_chunks(struct disklabel *);
void	mpcopy(char **, char **);
int	micmp(const void *, const void *);
int	mpequal(char **, char **);
int	get_bsize(struct disklabel *, int);
int	get_fsize(struct disklabel *, int);
int	get_fstype(struct disklabel *, int);
int	get_mp(struct disklabel *, int);
int	get_offset(struct disklabel *, int);
int	get_size(struct disklabel *, int);
void	get_geometry(int, struct disklabel **);
void	set_geometry(struct disklabel *, struct disklabel *, struct disklabel *,
	    char *);
void	zero_partitions(struct disklabel *);
u_int64_t max_partition_size(struct disklabel *, int);
void	display_edit(struct disklabel *, char, u_int64_t);

static u_int64_t starting_sector;
static u_int64_t ending_sector;
static int expert;
static int overlap;

/*
 * Simple partition editor.
 */
int
editor(struct disklabel *lp, int f)
{
	struct disklabel origlabel, lastlabel, tmplabel, label = *lp;
	struct disklabel *disk_geop = NULL;
	struct partition *pp;
	FILE *fp;
	char buf[BUFSIZ], *cmd, *arg;
	char **omountpoints = NULL;
	char **origmountpoints = NULL, **tmpmountpoints = NULL;
	int i, error = 0;

	/* Alloc and init mount point info */
	if (!(omountpoints = calloc(MAXPARTITIONS, sizeof(char *))) ||
	    !(origmountpoints = calloc(MAXPARTITIONS, sizeof(char *))) ||
	    !(tmpmountpoints = calloc(MAXPARTITIONS, sizeof(char *))))
		errx(4, "out of memory");

	/* Don't allow disk type of "unknown" */
	getdisktype(&label, "You need to specify a type for this disk.", specname);

	/* Get the on-disk geometries if possible */
	get_geometry(f, &disk_geop);

	/* How big is the OpenBSD portion of the disk?  */
	find_bounds(&label);

	/* Make sure there is no partition overlap. */
	if (has_overlap(&label))
		errx(1, "can't run when there is partition overlap.");

	/* If we don't have a 'c' partition, create one. */
	pp = &label.d_partitions[RAW_PART];
	if (label.d_npartitions < 3 || DL_GETPSIZE(pp) == 0) {
		puts("No 'c' partition found, adding one that spans the disk.");
		if (label.d_npartitions < 3)
			label.d_npartitions = 3;
		DL_SETPOFFSET(pp, 0);
		DL_SETPSIZE(pp, DL_GETDSIZE(&label));
		pp->p_fstype = FS_UNUSED;
		pp->p_fragblock = pp->p_cpg = 0;
	}

#ifdef SUN_CYLCHECK
	if (label.d_flags & D_VENDOR) {
		puts("This platform requires that partition offsets/sizes "
		    "be on cylinder boundaries.\n"
		    "Partition offsets/sizes will be rounded to the "
		    "nearest cylinder automatically.");
	}
#endif

	/* Set d_bbsize and d_sbsize as necessary */
	if (label.d_bbsize == 0)
		label.d_bbsize = BBSIZE;
	if (label.d_sbsize == 0)
		label.d_sbsize = SBSIZE;

	/* Save the (U|u)ndo labels and mountpoints. */
	mpcopy(origmountpoints, mountpoints);
	origlabel = label;
	lastlabel = label;

	puts("Label editor (enter '?' for help at any prompt)");
	for (;;) {
		fputs("> ", stdout);
		if (fgets(buf, sizeof(buf), stdin) == NULL) {
			putchar('\n');
			buf[0] = 'q';
			buf[1] = '\0';
		}
		if ((cmd = strtok(buf, " \t\r\n")) == NULL)
			continue;
		arg = strtok(NULL, " \t\r\n");

		if ((*cmd != 'u') && (*cmd != 'U')) {
			/*
			 * Save undo info in case the command tries to make
			 * changes but decides not to.
			 */
			tmplabel = lastlabel;
			lastlabel = label;
			mpcopy(tmpmountpoints, omountpoints);
			mpcopy(omountpoints, mountpoints);
		}

		switch (*cmd) {
		case '?':
		case 'h':
			editor_help();
			break;

		case 'A':
			if (ioctl(f, DIOCGPDINFO, &label) == 0) {
				aflag = 1;
				editor_allocspace(&label);
			} else
				label = lastlabel;
			break;
		case 'a':
			editor_add(&label, arg);
			break;

		case 'b':
			set_bounds(&label);
			break;

		case 'c':
			editor_change(&label, arg);
			break;

		case 'D':
			if (ioctl(f, DIOCGPDINFO, &label) == 0) {
				dflag = 1;
				for (i=0; i<MAXPARTITIONS; i++) {
					free(mountpoints[i]);
					mountpoints[i] = NULL;
				}
			} else
				warn("unable to get default partition table");
			break;

		case 'd':
			editor_delete(&label, arg);
			break;

		case 'e':
			edit_parms(&label);
			break;

		case 'g':
			set_geometry(&label, disk_geop, lp, arg);
			break;

		case 'm':
			editor_modify(&label, arg);
			break;

		case 'n':
			if (!fstabfile) {
				fputs("This option is not valid when run "
				    "without the -f flag.\n", stderr);
				break;
			}
			editor_name(&label, arg);
			break;

		case 'p':
			display_edit(&label, arg ? *arg : 0, editor_countfree(&label));
			break;

		case 'l':
			display(stdout, &label, arg ? *arg : 0, 0);
			break;

		case 'M': {
			sig_t opipe = signal(SIGPIPE, SIG_IGN);
			char *pager, *comm = NULL;
			extern const u_char manpage[];
			extern const int manpage_sz;

			if ((pager = getenv("PAGER")) == NULL || *pager == '\0')
				pager = _PATH_LESS;

			if (asprintf(&comm, "gunzip -qc|%s", pager) != -1 &&
			    (fp = popen(comm, "w")) != NULL) {
				(void) fwrite(manpage, manpage_sz, 1, fp);
				pclose(fp);
			} else
				warn("unable to execute %s", pager);

			free(comm);
			(void)signal(SIGPIPE, opipe);
			break;
		}

		case 'q':
			if (donothing) {
				puts("In no change mode, not writing label.");
				goto done;
			}
			/* Save mountpoint info if there is any. */
			mpsave(&label);

                        /*
			 * If we haven't changed the original label, and it
			 * wasn't a default label or an auto-allocated label,
			 * there is no need to do anything before exiting. Note
			 * that 'w' will reset dflag and aflag to allow 'q' to
			 * exit without further questions.
 			 */
			if (!dflag && !aflag &&
			    memcmp(lp, &label, sizeof(label)) == 0) {
				puts("No label changes.");
				goto done;
			}
			do {
				arg = getstring("Write new label?",
				    "Write the modified label to disk?",
				    "y");
			} while (arg && tolower(*arg) != 'y' && tolower(*arg) != 'n');
			if (arg && tolower(*arg) == 'y') {
				if (writelabel(f, bootarea, &label) == 0) {
					*lp = label;
					goto done;
				}
				warnx("unable to write label");
			}
			error = 1;
			goto done;
			/* NOTREACHED */
			break;

		case 'R':
			if (aflag && !overlap)
				editor_resize(&label, arg);
			else
				fputs("Resize only implemented for auto "
				    "allocated labels\n", stderr);
			break;

		case 'r': {
			struct diskchunk *chunks;
			int i;
			/* Display free space. */
			chunks = free_chunks(&label);
			for (i = 0; chunks[i].start != 0 || chunks[i].stop != 0;
			    i++)
				fprintf(stderr, "Free sectors: %16llu - %16llu "
				    "(%16llu)\n",
			    	    chunks[i].start, chunks[i].stop - 1,
			   	    chunks[i].stop - chunks[i].start);
			fprintf(stderr, "Total free sectors: %llu.\n",
			    editor_countfree(&label));
		    	break;
		}

		case 's':
			if (arg == NULL) {
				arg = getstring("Filename",
				    "Name of the file to save label into.",
				    NULL);
				if (arg == NULL || *arg == '\0')
					break;
			}
			if ((fp = fopen(arg, "w")) == NULL) {
				warn("cannot open %s", arg);
			} else {
				display(fp, &label, 0, 1);
				(void)fclose(fp);
			}
			break;

		case 'U':
			/*
			 * If we allow 'U' repeatedly, information would be lost. This way
			 * multiple 'U's followed by 'u' would undo the 'U's.
			 */
			if (memcmp(&label, &origlabel, sizeof(label)) ||
			    !mpequal(mountpoints, origmountpoints)) {
				tmplabel = label;
				label = origlabel;
				lastlabel = tmplabel;
				mpcopy(tmpmountpoints, mountpoints);
				mpcopy(mountpoints, origmountpoints);
				mpcopy(omountpoints, tmpmountpoints);
			}
			puts("Original label and mount points restored.");
			break;

		case 'u':
			tmplabel = label;
			label = lastlabel;
			lastlabel = tmplabel;
			mpcopy(tmpmountpoints, mountpoints);
			mpcopy(mountpoints, omountpoints);
			mpcopy(omountpoints, tmpmountpoints);
			puts("Last change undone.");
			break;

		case 'w':
			if (donothing)  {
				puts("In no change mode, not writing label.");
				break;
			}

			/* Write label to disk. */
			if (writelabel(f, bootarea, &label) != 0)
				warnx("unable to write label");
			else {
				dflag = aflag = 0;
				*lp = label;
			}
			break;

		case 'X':
			expert = !expert;
			printf("%s expert mode\n", expert ? "Entering" :
			    "Exiting");
			break;

		case 'x':
			goto done;
			break;

		case 'z':
			zero_partitions(&label);
			break;

		case '\n':
			break;

		default:
			printf("Unknown option: %c ('?' for help)\n", *cmd);
			break;
		}

		/*
		 * If no changes were made to label or mountpoints, then
		 * restore undo info.
		 */
		if (memcmp(&label, &lastlabel, sizeof(label)) == 0 &&
		    (mpequal(mountpoints, omountpoints))) {
			lastlabel = tmplabel;
			mpcopy(omountpoints, tmpmountpoints);
		}
	}
done:
	free(omountpoints);
	free(origmountpoints);
	free(tmpmountpoints);
	if (disk_geop)
		free(disk_geop);
	return(error);
}

int64_t
getphysmem(void)
{
	int64_t physmem;
	size_t sz = sizeof(physmem);
	int mib[] = { CTL_HW, HW_PHYSMEM64 };
	if (sysctl(mib, 2, &physmem, &sz, NULL, (size_t)0) == -1)
		errx(4, "can't get mem size");
	return physmem;
}

/*
 * Allocate all disk space according to standard recommendations for a
 * root disk.
 */
void
editor_allocspace(struct disklabel *lp_org)
{
	struct disklabel *lp, label;
	struct space_allocation *alloc;
	struct space_allocation *ap;
	struct partition *pp;
	struct diskchunk *chunks;
	daddr64_t secs, chunkstart, chunksize, cylsecs, totsecs, xtrasecs;
	char **partmp;
	int i, j, lastalloc, index = 0, fragsize;
	int64_t physmem;

	/* How big is the OpenBSD portion of the disk?  */
	find_bounds(lp_org);

	overlap = 0;
	for (i = 0;  i < MAXPARTITIONS; i++) {
		daddr64_t psz, pstart, pend;

		pp = &lp_org->d_partitions[i];
		psz = DL_GETPSIZE(pp);
		pstart = DL_GETPOFFSET(pp);
		pend = pstart + psz;
		if (i != RAW_PART && psz != 0 &&
		    ((pstart >= starting_sector && pstart <= ending_sector) ||
		    (pend > starting_sector && pend < ending_sector))) {
			overlap = 1;
			break;
		}
	}

	physmem = getphysmem() / lp_org->d_secsize;

	cylsecs = lp_org->d_secpercyl;
again:
	lp = &label;
	for (i=0; i<MAXPARTITIONS; i++) {
		free(mountpoints[i]);
		mountpoints[i] = NULL;
	}
	memcpy(lp, lp_org, sizeof(struct disklabel));
	lp->d_npartitions = MAXPARTITIONS;
	lastalloc = alloc_table[index].sz;
	alloc = malloc(lastalloc * sizeof(struct space_allocation));
	if (alloc == NULL)
		errx(4, "out of memory");
	memcpy(alloc, alloc_table[index].table,
	    lastalloc * sizeof(struct space_allocation));

	/* bump max swap based on phys mem, little physmem gets 2x swap */
	if (index == 0) {
		if (physmem < 256LL * 1024 * 1024 / lp->d_secsize)
			alloc[1].maxsz = 2 * physmem;
		else
			alloc[1].maxsz += physmem;
		/* bump max /var to make room for 2 crash dumps */
		alloc[3].maxsz += 2 * physmem;
	}

	xtrasecs = totsecs = editor_countfree(lp);

	for (i = 0; i < lastalloc; i++) {
		alloc[i].minsz = DL_BLKTOSEC(lp, alloc[i].minsz);
		alloc[i].maxsz = DL_BLKTOSEC(lp, alloc[i].maxsz);
		if (xtrasecs > alloc[i].minsz)
			xtrasecs -= alloc[i].minsz;
		else
			xtrasecs = 0;
	}

	for (i = 0; i < lastalloc; i++) {
		/* Find next available partition. */
		for (j = 0;  j < MAXPARTITIONS; j++)
			if (DL_GETPSIZE(&lp->d_partitions[j]) == 0)
				break;
		if (j == MAXPARTITIONS)
			return;
		pp = &lp->d_partitions[j];
		partmp = &mountpoints[j];
		ap = &alloc[i];

		/* Figure out the size of the partition. */
		if (i == lastalloc - 1) {
			if (totsecs > ap->maxsz)
				secs = ap->maxsz;
			else
				secs = totsecs;
#ifdef SUN_CYLCHECK
			goto cylinderalign;
#endif
		} else {
			secs = ap->minsz;
			if (xtrasecs > 0)
				secs += (xtrasecs / 100) * ap->rate;
			if (secs > ap->maxsz)
				secs = ap->maxsz;
#ifdef SUN_CYLCHECK
cylinderalign:
			secs = ((secs + cylsecs - 1) / cylsecs) * cylsecs;
#endif
			totsecs -= secs;
#ifdef SUN_CYLCHECK
			while (totsecs < 0) {
				secs -= cylsecs;
				totsecs += cylsecs;
			}
#endif
		}

		/* Find largest chunk of free space. */
		chunks = free_chunks(lp);
		chunkstart = 0;
		chunksize = 0;
		for (j = 0; chunks[j].start != 0 || chunks[j].stop != 0; j++)
			if ((chunks[j].stop - chunks[j].start) > chunksize) {
				chunkstart = chunks[j].start;
				chunksize = chunks[j].stop - chunks[j].start;
			}
#ifdef SUN_CYLCHECK
		if (lp->d_flags & D_VENDOR) {
			/* Align chunk to cylinder boundaries. */
			chunksize -= chunksize % cylsecs;
			chunkstart = ((chunkstart + cylsecs - 1) / cylsecs) *
			    cylsecs;
		}
#endif
		/* See if partition can fit into chunk. */
		if (secs > chunksize) {
			totsecs += secs - chunksize;
			secs = chunksize;
		}
		if (secs < ap->minsz) {
			/* It did not work out, try next strategy */
			free(alloc);
			if (++index < nitems(alloc_table))
				goto again;
			else
				return;
		}

		/* Everything seems ok so configure the partition. */
		DL_SETPSIZE(pp, secs);
		DL_SETPOFFSET(pp, chunkstart);
		fragsize = 2048;
		if (secs * lp->d_secsize > 128ULL * 1024 * 1024 * 1024)
			fragsize *= 2;
		if (secs * lp->d_secsize > 512ULL * 1024 * 1024 * 1024)
			fragsize *= 2;
#if defined (__sparc__) && !defined(__sparc64__)
		/* can't boot from > 8k boot blocks */
		pp->p_fragblock =
		    DISKLABELV1_FFS_FRAGBLOCK(i == 0 ? 1024 : fragsize, 8);
#else
		pp->p_fragblock = DISKLABELV1_FFS_FRAGBLOCK(fragsize, 8);
#endif
		pp->p_cpg = 1;
		if (ap->mp[0] != '/')
			pp->p_fstype = FS_SWAP;
		else {
			pp->p_fstype = FS_BSDFFS;
			free(*partmp);
			if ((*partmp = strdup(ap->mp)) == NULL)
				errx(4, "out of memory");
		}
	}

	free(alloc);
	memcpy(lp_org, lp, sizeof(struct disklabel));
}

/*
 * Resize a partition, moving all subsequent partitions
 */
void
editor_resize(struct disklabel *lp, char *p)
{
	struct disklabel label;
	struct partition *pp, *prev;
	daddr64_t secs, sz, off;
#ifdef SUN_CYLCHECK
	daddr64_t cylsecs;
#endif
	int partno, i;

	label = *lp;

	/* Change which partition? */
	if (p == NULL) {
		p = getstring("partition to resize",
		    "The letter of the partition to name, a - p.", NULL);
	}
	if (p == NULL) {
		fputs("Command aborted\n", stderr);
		return;
	}
	partno = p[0] - 'a';
        if (partno < 0 || partno == RAW_PART || partno >= lp->d_npartitions) {
		fprintf(stderr, "Partition must be between 'a' and '%c' "
		    "(excluding 'c').\n", 'a' + lp->d_npartitions - 1);
		return;
	}

	pp = &label.d_partitions[partno];
	sz = DL_GETPSIZE(pp);
	if (sz == 0) {
		fputs("No such partition\n", stderr);
		return;
	}
	if (pp->p_fstype != FS_BSDFFS && pp->p_fstype != FS_SWAP) {
		fputs("Cannot resize spoofed partition\n", stderr);
		return;
	}
	secs = getuint(lp, "resize", "amount to grow (+) or shrink (-)",
	    0, editor_countfree(lp), 0, DO_CONVERSIONS);

	if (secs == 0 || secs == -1) {
		fputs("Command aborted\n", stderr);
		return;
	}

#ifdef SUN_CYLCHECK
	cylsecs = lp->d_secpercyl;
	if (secs > 0)
		secs = ((secs + cylsecs - 1) / cylsecs) * cylsecs;
	else
		secs = ((secs - cylsecs + 1) / cylsecs) * cylsecs;
#endif
	if (DL_GETPOFFSET(pp) + sz + secs > ending_sector) {
		fputs("Amount too big\n", stderr);
		return;
	}
	if (sz + secs < 0) {
		fputs("Amount too small\n", stderr);
		return;
	}

	DL_SETPSIZE(pp, sz + secs);

	/*
	 * Pack partitions above the resized partition, leaving unused
	 * partions alone.
	 */
	prev = pp;
	for (i = partno + 1; i < MAXPARTITIONS; i++) {
		if (i == RAW_PART)
			continue;
		pp = &label.d_partitions[i];
		if (pp->p_fstype != FS_BSDFFS && pp->p_fstype != FS_SWAP)
			continue;
		sz = DL_GETPSIZE(pp);
		if (sz == 0)
			continue;

		off = DL_GETPOFFSET(prev) + DL_GETPSIZE(prev);

		if (off < ending_sector) {
			DL_SETPOFFSET(pp, off);
			if (off + DL_GETPSIZE(pp) > ending_sector) {
				DL_SETPSIZE(pp, ending_sector - off);
				fprintf(stderr,
				    "Partition %c shrunk to make room\n",
				    i + 'a');
			}
		} else {
			fputs("No room left for all partitions\n", stderr);
			return;
		}
		prev = pp;
	}
	*lp = label;
}

/*
 * Add a new partition.
 */
void
editor_add(struct disklabel *lp, char *p)
{
	struct partition *pp;
	struct diskchunk *chunks;
	char buf[2];
	int i, partno, fragsize;
	u_int64_t freesectors, new_offset, new_size;

	freesectors = editor_countfree(lp);

	/* XXX - prompt user to steal space from another partition instead */
#ifdef SUN_CYLCHECK
	if ((lp->d_flags & D_VENDOR) && freesectors < lp->d_secpercyl) {
		fputs("No space left, you need to shrink a partition "
		    "(need at least one full cylinder)\n",
		    stderr);
		return;
	}
#endif
	if (freesectors == 0) {
		fputs("No space left, you need to shrink a partition\n",
		    stderr);
		return;
	}

	if (p == NULL) {
		/*
		 * Use the first unused partition that is not 'c' as the
		 * default partition in the prompt string.
		 */
		pp = &lp->d_partitions[0];
		buf[0] = buf[1] = '\0';
		for (partno = 0; partno < MAXPARTITIONS; partno++, pp++) {
			if (DL_GETPSIZE(pp) == 0 && partno != RAW_PART) {
				buf[0] = partno + 'a';
				p = &buf[0];
				break;
			}
		}
		p = getstring("partition",
		    "The letter of the new partition, a - p.", p);
	}
	if (p == NULL) {
		fputs("Command aborted\n", stderr);
		return;
	}
	partno = p[0] - 'a';
	if (partno < 0 || partno == RAW_PART || partno >= MAXPARTITIONS) {
		fprintf(stderr, "Partition must be between 'a' and '%c' "
		    "(excluding 'c').\n", 'a' + MAXPARTITIONS - 1);
		return;
	}	
	pp = &lp->d_partitions[partno];

	if (pp->p_fstype != FS_UNUSED && DL_GETPSIZE(pp) != 0) {
		fprintf(stderr, "Partition '%c' exists.  Delete it first.\n",
		    p[0]);
		return;
	}

	/*
	 * Increase d_npartitions if necessary. Ensure all new partitions are
	 * zero'ed to avoid inadvertent overlaps.
	 */
	for(; lp->d_npartitions <= partno; lp->d_npartitions++)
		memset(&lp->d_partitions[lp->d_npartitions], 0, sizeof(*pp));

	/* Make sure selected partition is zero'd too. */
	memset(pp, 0, sizeof(*pp)); 
	chunks = free_chunks(lp);

	/*
	 * Since we know there's free space, there must be at least one
	 * chunk. So find the largest chunk and assume we want to add the
	 * partition in that free space.
	 */
	new_size = new_offset = 0;
	for (i = 0; chunks[i].start != 0 || chunks[i].stop != 0; i++) {
		if (chunks[i].stop - chunks[i].start > new_size) {
		    new_size = chunks[i].stop - chunks[i].start;
		    new_offset = chunks[i].start;
		}
	}
	DL_SETPSIZE(pp, new_size);
	DL_SETPOFFSET(pp, new_offset);
	pp->p_fstype = partno == 1 ? FS_SWAP : FS_BSDFFS;
	pp->p_cpg = 1;

	if (get_offset(lp, partno) == 0 &&
	    get_size(lp, partno) == 0) {
		fragsize = 2048;
		new_size = DL_GETPSIZE(pp) * lp->d_secsize;
		if (new_size > 128ULL * 1024 * 1024 * 1024)
			fragsize *= 2;
		if (new_size > 512ULL * 1024 * 1024 * 1024)
			fragsize *= 2;
#if defined (__sparc__) && !defined(__sparc64__)
		/* can't boot from > 8k boot blocks */
		pp->p_fragblock =
		    DISKLABELV1_FFS_FRAGBLOCK(partno == 0 ? 1024 : fragsize, 8);
#else
		pp->p_fragblock = DISKLABELV1_FFS_FRAGBLOCK(fragsize, 8);
#endif
		if (get_fstype(lp, partno) == 0 &&
		    get_mp(lp, partno) == 0 &&
		    get_fsize(lp, partno) == 0  &&
		    get_bsize(lp, partno) == 0)
			return;
	}
	/* Bailed out at some point, so effectively delete the partition. */
	DL_SETPSIZE(pp, 0);
}

/*
 * Set the mountpoint of an existing partition ('name').
 */
void
editor_name(struct disklabel *lp, char *p)
{
	struct partition *pp;
	int partno;

	/* Change which partition? */
	if (p == NULL) {
		p = getstring("partition to name",
		    "The letter of the partition to name, a - p.", NULL);
	}
	if (p == NULL) {
		fputs("Command aborted\n", stderr);
		return;
	}
	partno = p[0] - 'a';
	if (partno < 0 || partno == RAW_PART || partno >= lp->d_npartitions) {
		fprintf(stderr, "Partition must be between 'a' and '%c' "
		    "(excluding 'c').\n", 'a' + lp->d_npartitions - 1);
		return;
	}
	pp = &lp->d_partitions[partno];

	if (pp->p_fstype == FS_UNUSED && DL_GETPSIZE(pp) == 0) {
		fprintf(stderr, "Partition '%c' is not in use.\n", p[0]);
		return;
	}

	/* Not all fstypes can be named */
	if (pp->p_fstype == FS_UNUSED || pp->p_fstype == FS_SWAP ||
	    pp->p_fstype == FS_BOOT || pp->p_fstype == FS_OTHER ||
	    pp->p_fstype == FS_RAID) {
		fprintf(stderr, "You cannot name a filesystem of type %s.\n",
		    fstypenames[lp->d_partitions[partno].p_fstype]);
		return;
	}

	get_mp(lp, partno);
}

/*
 * Change an existing partition.
 */
void
editor_modify(struct disklabel *lp, char *p)
{
	struct partition origpart, *pp;
	int partno;

	/* Change which partition? */
	if (p == NULL) {
		p = getstring("partition to modify",
		    "The letter of the partition to modify, a - p.", NULL);
	}
	if (p == NULL) {
		fputs("Command aborted\n", stderr);
		return;
	}
	partno = p[0] - 'a';
	if (partno < 0 || partno == RAW_PART || partno >= lp->d_npartitions) {
		fprintf(stderr, "Partition must be between 'a' and '%c' "
		    "(excluding 'c').\n", 'a' + lp->d_npartitions - 1);
		return;
	}
	pp = &lp->d_partitions[partno];

	if (pp->p_fstype == FS_UNUSED && DL_GETPSIZE(pp) == 0) {
		fprintf(stderr, "Partition '%c' is not in use.\n", p[0]);
		return;
	}

	origpart = *pp;

	if (get_offset(lp, partno) == 0 &&
	    get_size(lp, partno) == 0   &&
	    get_fstype(lp, partno) == 0 &&
	    get_mp(lp, partno) == 0 &&
	    get_fsize(lp, partno) == 0  &&
	    get_bsize(lp, partno) == 0)
		return;

	/* Bailed out at some point, so undo any changes. */
	*pp = origpart;
}

/*
 * Delete an existing partition.
 */
void
editor_delete(struct disklabel *lp, char *p)
{
	struct partition *pp;
	int partno;

	if (p == NULL) {
		p = getstring("partition to delete",
		    "The letter of the partition to delete, a - p, or '*'.",
		    NULL);
	}
	if (p == NULL) {
		fputs("Command aborted\n", stderr);
		return;
	}
	if (p[0] == '*') {
		zero_partitions(lp);
		return;
	}
	partno = p[0] - 'a';
	if (partno < 0 || partno == RAW_PART || partno >= lp->d_npartitions) {
		fprintf(stderr, "Partition must be between 'a' and '%c' "
		    "(excluding 'c').\n", 'a' + lp->d_npartitions - 1);
		return;
	}
	pp = &lp->d_partitions[partno];

	if (pp->p_fstype == FS_UNUSED && DL_GETPSIZE(pp) == 0) {
		fprintf(stderr, "Partition '%c' is not in use.\n", p[0]);
		return;
	}

	/* Really delete it (as opposed to just setting to "unused") */
	memset(pp, 0, sizeof(*pp));
	free(mountpoints[partno]);
	mountpoints[partno] = NULL;
}

/*
 * Change the size of an existing partition.
 */
void
editor_change(struct disklabel *lp, char *p)
{
	struct partition *pp;
	int partno;

	if (p == NULL) {
		p = getstring("partition to change size",
		    "The letter of the partition to change size, a - p.", NULL);
	}
	if (p == NULL) {
		fputs("Command aborted\n", stderr);
		return;
	}
	partno = p[0] - 'a';
	if (partno < 0 || partno == RAW_PART || partno >= lp->d_npartitions) {
		fprintf(stderr, "Partition must be between 'a' and '%c' "
		    "(excluding 'c').\n", 'a' + lp->d_npartitions - 1);
		return;
	}
	pp = &lp->d_partitions[partno];

	if (DL_GETPSIZE(pp) == 0) {
		fprintf(stderr, "Partition '%c' is not in use.\n", p[0]);
		return;
	}

	printf("Partition %c is currently %llu sectors in size, and can have "
	    "a maximum\nsize of %llu sectors.\n",
	    p[0], DL_GETPSIZE(pp), max_partition_size(lp, partno));

	/* Get new size */
	get_size(lp, partno);
}

/*
 * Sort the partitions based on starting offset.
 * This assumes there can be no overlap.
 */
int
partition_cmp(const void *e1, const void *e2)
{
	struct partition *p1 = *(struct partition **)e1;
	struct partition *p2 = *(struct partition **)e2;
	u_int64_t o1 = DL_GETPOFFSET(p1);
	u_int64_t o2 = DL_GETPOFFSET(p2);

	if (o1 < o2)
		return -1;
	else if (o1 > o2)
		return 1;
	else
		return 0;
}

char *
getstring(char *prompt, char *helpstring, char *oval)
{
	static char buf[BUFSIZ];
	int n;

	buf[0] = '\0';
	do {
		printf("%s: [%s] ", prompt, oval ? oval : "");
		if (fgets(buf, sizeof(buf), stdin) == NULL) {
			buf[0] = '\0';
			if (feof(stdin)) {
				clearerr(stdin);
				putchar('\n');
				return(NULL);
			}
		}
		n = strlen(buf);
		if (n > 0 && buf[n-1] == '\n')
			buf[--n] = '\0';
		if (buf[0] == '?')
			puts(helpstring);
		else if (oval != NULL && buf[0] == '\0')
			strlcpy(buf, oval, sizeof(buf));
	} while (buf[0] == '?');

	return(&buf[0]);
}

/*
 * Returns ULLONG_MAX on error
 * Usually only called by helper functions.
 */
u_int64_t
getuint(struct disklabel *lp, char *prompt, char *helpstring,
    u_int64_t oval, u_int64_t maxval, u_int64_t offset, int flags)
{
	char buf[BUFSIZ], *endptr, *p, operator = '\0';
	u_int64_t rval = oval;
	size_t n;
	int mult = 1;
	double d, percent = 1.0;

	/* We only care about the remainder */
	offset = offset % lp->d_secpercyl;

	buf[0] = '\0';
	do {
		printf("%s: [%llu] ", prompt, oval);
		if (fgets(buf, sizeof(buf), stdin) == NULL) {
			buf[0] = '\0';
			if (feof(stdin)) {
				clearerr(stdin);
				putchar('\n');
				return(ULLONG_MAX - 1);
			}
		}
		n = strlen(buf);
		if (n > 0 && buf[n-1] == '\n')
			buf[--n] = '\0';
		if (buf[0] == '?')
			puts(helpstring);
	} while (buf[0] == '?');

	if (buf[0] == '*' && buf[1] == '\0') {
		rval = maxval;
	} else {
		/* deal with units */
		if (buf[0] != '\0' && n > 0) {
			if ((flags & DO_CONVERSIONS)) {
				switch (tolower(buf[n-1])) {

				case 'c':
					mult = lp->d_secpercyl;
					buf[--n] = '\0';
					break;
				case 'b':
					mult = -lp->d_secsize;
					buf[--n] = '\0';
					break;
				case 'k':
					if (lp->d_secsize > 1024)
						mult = -lp->d_secsize / 1024;
					else
						mult = 1024 / lp->d_secsize;
					buf[--n] = '\0';
					break;
				case 'm':
					mult = 1048576 / lp->d_secsize;
					buf[--n] = '\0';
					break;
				case 'g':
					mult = 1073741824 / lp->d_secsize;
					buf[--n] = '\0';
					break;
				case '%':
					buf[--n] = '\0';
					percent = strtod(buf, NULL) / 100.0;
					snprintf(buf, sizeof(buf), "%lld",
					    DL_GETDSIZE(lp));
					break;
				case '&':
					buf[--n] = '\0';
					percent = strtod(buf, NULL) / 100.0;
					snprintf(buf, sizeof(buf), "%lld",
					    maxval);
					break;
				}
			}

			/* Did they give us an operator? */
			p = &buf[0];
			if (*p == '+' || *p == '-')
				operator = *p++;

			endptr = p;
			errno = 0;
			d = strtod(p, &endptr);
			if (errno == ERANGE)
				rval = ULLONG_MAX;	/* too big/small */
			else if (*endptr != '\0') {
				errno = EINVAL;		/* non-numbers in str */
				rval = ULLONG_MAX;
			} else {
				/* XXX - should check for overflow */
				if (mult > 0)
					rval = d * mult * percent;
				else
					/* Negative mult means divide (fancy) */
					rval = d / (-mult) * percent;

				/* Apply the operator */
				if (operator == '+')
					rval += oval;
				else if (operator == '-')
					rval = oval - rval;
			}
		}
	}
	if ((flags & DO_ROUNDING) && rval != ULLONG_MAX) {
		/* Round to nearest cylinder unless given in sectors */
		if (
#ifdef SUN_CYLCHECK
		    ((lp->d_flags & D_VENDOR) || mult != 1) &&
#else
		    mult != 1 &&
#endif
		    (rval + offset) % lp->d_secpercyl != 0) {
			u_int64_t cyls;

			/* Round to higher cylinder but no more than maxval */
			cyls = (rval / lp->d_secpercyl) + 1;
			if ((cyls * lp->d_secpercyl) - offset > maxval)
				cyls--;
			rval = (cyls * lp->d_secpercyl) - offset;
			printf("Rounding to cylinder: %llu\n", rval);
		}
	}

	return(rval);
}

/*
 * Check for partition overlap in lp and prompt the user to resolve the overlap
 * if any is found.  Returns 1 if unable to resolve, else 0.
 */
int
has_overlap(struct disklabel *lp)
{
	struct partition **spp;
	int c, i, j;
	char buf[BUFSIZ];

	/* Get a sorted list of the in-use partitions. */
	spp = sort_partitions(lp);

	/* If there are less than two partitions in use, there is no overlap. */
	if (spp[1] == NULL)
		return(0);

	/* Now that we have things sorted by starting sector check overlap */
	for (i = 0; spp[i] != NULL; i++) {
		for (j = i + 1; spp[j] != NULL; j++) {
			/* `if last_sec_in_part + 1 > first_sec_in_next_part' */
			if (DL_GETPOFFSET(spp[i]) + DL_GETPSIZE(spp[i]) > DL_GETPOFFSET(spp[j])) {
				/* Overlap!  Convert to real part numbers. */
				i = ((char *)spp[i] - (char *)lp->d_partitions)
				    / sizeof(**spp);
				j = ((char *)spp[j] - (char *)lp->d_partitions)
				    / sizeof(**spp);
				printf("\nError, partitions %c and %c overlap:\n",
				    'a' + i, 'a' + j);
				printf("#    %16.16s %16.16s  fstype "
				    "[fsize bsize  cpg]\n", "size", "offset");
				display_partition(stdout, lp, i, 0);
				display_partition(stdout, lp, j, 0);

				/* Get partition to disable or ^D */
				do {
					printf("Disable which one? (^D to abort) [%c %c] ",
					    'a' + i, 'a' + j);
					buf[0] = '\0';
					if (!fgets(buf, sizeof(buf), stdin)) {
						putchar('\n');
						return(1);	/* ^D */
					}
					c = buf[0] - 'a';
				} while (buf[1] != '\n' && buf[1] != '\0' &&
				    c != i && c != j);

				/* Mark the selected one as unused */
				lp->d_partitions[c].p_fstype = FS_UNUSED;
				return (has_overlap(lp));
			}
		}
	}

	return(0);
}

void
edit_parms(struct disklabel *lp)
{
	char *p;
	u_int64_t freesectors, ui;
	struct disklabel oldlabel = *lp;

	printf("Changing device parameters for %s:\n", specname);

	/* disk type */
	for (;;) {
		p = getstring("disk type",
		    "What kind of disk is this?  Usually SCSI, ESDI, ST506, or "
		    "floppy (use ESDI for IDE).", dktypenames[lp->d_type]);
		if (p == NULL) {
			fputs("Command aborted\n", stderr);
			return;
		}
		if (strcasecmp(p, "IDE") == 0)
			ui = DTYPE_ESDI;
		else
			for (ui = 1; ui < DKMAXTYPES &&
			    strcasecmp(p, dktypenames[ui]); ui++)
				;
		if (ui < DKMAXTYPES) {
			break;
		} else {
			printf("\"%s\" is not a valid disk type.\n", p);
			fputs("Valid types are: ", stdout);
			for (ui = 1; ui < DKMAXTYPES; ui++) {
				printf("\"%s\"", dktypenames[ui]);
				if (ui < DKMAXTYPES - 1)
					fputs(", ", stdout);
			}
			putchar('\n');
		}
	}
	lp->d_type = ui;

	/* pack/label id */
	p = getstring("label name",
	    "15 char string that describes this label, usually the disk name.",
	    lp->d_packname);
	if (p == NULL) {
		fputs("Command aborted\n", stderr);
		*lp = oldlabel;		/* undo damage */
		return;
	}
	strncpy(lp->d_packname, p, sizeof(lp->d_packname));	/* checked */

	/* sectors/track */
	for (;;) {
		ui = getuint(lp, "sectors/track",
		    "The Number of sectors per track.", lp->d_nsectors,
		    lp->d_nsectors, 0, 0);
		if (ui == ULLONG_MAX - 1) {
			fputs("Command aborted\n", stderr);
			*lp = oldlabel;		/* undo damage */
			return;
		} if (ui == ULLONG_MAX)
			fputs("Invalid entry\n", stderr);
		else
			break;
	}
	lp->d_nsectors = ui;

	/* tracks/cylinder */
	for (;;) {
		ui = getuint(lp, "tracks/cylinder",
		    "The number of tracks per cylinder.", lp->d_ntracks,
		    lp->d_ntracks, 0, 0);
		if (ui == ULLONG_MAX - 1) {
			fputs("Command aborted\n", stderr);
			*lp = oldlabel;		/* undo damage */
			return;
		} else if (ui == ULLONG_MAX)
			fputs("Invalid entry\n", stderr);
		else
			break;
	}
	lp->d_ntracks = ui;

	/* sectors/cylinder */
	for (;;) {
		ui = getuint(lp, "sectors/cylinder",
		    "The number of sectors per cylinder (Usually sectors/track "
		    "* tracks/cylinder).", lp->d_secpercyl, lp->d_secpercyl,
		    0, 0);
		if (ui == ULLONG_MAX - 1) {
			fputs("Command aborted\n", stderr);
			*lp = oldlabel;		/* undo damage */
			return;
		} else if (ui == ULLONG_MAX)
			fputs("Invalid entry\n", stderr);
		else
			break;
	}
	lp->d_secpercyl = ui;

	/* number of cylinders */
	for (;;) {
		ui = getuint(lp, "number of cylinders",
		    "The total number of cylinders on the disk.",
		    lp->d_ncylinders, lp->d_ncylinders, 0, 0);
		if (ui == ULLONG_MAX - 1) {
			fputs("Command aborted\n", stderr);
			*lp = oldlabel;		/* undo damage */
			return;
		} else if (ui == ULLONG_MAX)
			fputs("Invalid entry\n", stderr);
		else
			break;
	}
	lp->d_ncylinders = ui;

	/* total sectors */
	for (;;) {
		u_int64_t nsec = MAX(DL_GETDSIZE(lp),
		    (u_int64_t)lp->d_ncylinders * lp->d_secpercyl);
		ui = getuint(lp, "total sectors",
		    "The total number of sectors on the disk.",
		    nsec, nsec, 0, 0);
		if (ui == ULLONG_MAX - 1) {
			fputs("Command aborted\n", stderr);
			*lp = oldlabel;		/* undo damage */
			return;
		} else if (ui == ULLONG_MAX)
			fputs("Invalid entry\n", stderr);
		else if (ui > DL_GETDSIZE(lp) &&
		    ending_sector == DL_GETDSIZE(lp)) {
			puts("You may want to increase the size of the 'c' "
			    "partition.");
			break;
		} else if (ui < DL_GETDSIZE(lp) &&
		    ending_sector == DL_GETDSIZE(lp)) {
			/* shrink free count */
			freesectors = editor_countfree(lp);
			if (DL_GETDSIZE(lp) - ui > freesectors)
				fprintf(stderr,
				    "Not enough free space to shrink by %llu "
				    "sectors (only %llu sectors left)\n",
				    DL_GETDSIZE(lp) - ui, freesectors);
			else
				break;
		} else
			break;
	}
	/* Adjust ending_sector if necessary. */
	if (ending_sector > ui)
		ending_sector = ui;
	DL_SETDSIZE(lp, ui);
}

struct partition **
sort_partitions(struct disklabel *lp)
{
	static struct partition *spp[MAXPARTITIONS+2];
	int i, npartitions;

	memset(spp, 0, sizeof(spp));

	for (npartitions = 0, i = 0; i < lp->d_npartitions; i++) {
		if (lp->d_partitions[i].p_fstype != FS_UNUSED &&
		    lp->d_partitions[i].p_fstype != FS_BOOT &&
		    DL_GETPSIZE(&lp->d_partitions[i]) != 0)
			spp[npartitions++] = &lp->d_partitions[i];
	}

	/*
	 * Sort the partitions based on starting offset.
	 * This is safe because we guarantee no overlap.
	 */
	if (npartitions > 1)
		if (heapsort((void *)spp, npartitions, sizeof(spp[0]),
		    partition_cmp))
			err(4, "failed to sort partition table");

	return(spp);
}

/*
 * Get a valid disk type if necessary.
 */
void
getdisktype(struct disklabel *lp, char *banner, char *dev)
{
	int i;
	char *s, *def = "SCSI";
	struct dtypes {
		char *dev;
		char *type;
	} dtypes[] = {
		{ "sd",   "SCSI" },
		{ "rz",   "SCSI" },
		{ "wd",   "IDE" },
		{ "fd",   "FLOPPY" },
		{ "xd",   "SMD" },
		{ "xy",   "SMD" },
		{ "hd",   "HP-IB" },
		{ "ccd",  "CCD" },
		{ "vnd",  "VND" },
		{ "svnd", "VND" },
		{ NULL,   NULL }
	};

	if ((s = basename(dev)) != NULL) {
		if (*s == 'r')
			s++;
		i = strcspn(s, "0123456789");
		s[i] = '\0';
		dev = s;
		for (i = 0; dtypes[i].dev != NULL; i++) {
			if (strcmp(dev, dtypes[i].dev) == 0) {
				def = dtypes[i].type;
				break;
			}
		}
	}

	if (lp->d_type > DKMAXTYPES || lp->d_type == 0) {
		puts(banner);
		puts("Possible values are:");
		printf("\"IDE\", ");
		for (i = 1; i < DKMAXTYPES; i++) {
			printf("\"%s\"", dktypenames[i]);
			if (i < DKMAXTYPES - 1)
				fputs(", ", stdout);
		}
		putchar('\n');

		for (;;) {
			s = getstring("Disk type",
			    "What kind of disk is this?  Usually SCSI, IDE, "
			    "ESDI, CCD, ST506, or floppy.", def);
			if (s == NULL)
				continue;
			if (strcasecmp(s, "IDE") == 0) {
				lp->d_type = DTYPE_ESDI;
				return;
			}
			for (i = 1; i < DKMAXTYPES; i++)
				if (strcasecmp(s, dktypenames[i]) == 0) {
					lp->d_type = i;
					return;
				}
			printf("\"%s\" is not a valid disk type.\n", s);
			fputs("Valid types are: ", stdout);
			for (i = 1; i < DKMAXTYPES; i++) {
				printf("\"%s\"", dktypenames[i]);
				if (i < DKMAXTYPES - 1)
					fputs(", ", stdout);
			}
			putchar('\n');
		}
	}
}

/*
 * Get beginning and ending sectors of the OpenBSD portion of the disk
 * from the user.
 */
void
set_bounds(struct disklabel *lp)
{
	u_int64_t ui, start_temp;

	/* Starting sector */
	do {
		ui = getuint(lp, "Starting sector",
		  "The start of the OpenBSD portion of the disk.",
		  starting_sector, DL_GETDSIZE(lp), 0, 0);
		if (ui == ULLONG_MAX - 1) {
			fputs("Command aborted\n", stderr);
			return;
		}
	} while (ui >= DL_GETDSIZE(lp));
	start_temp = ui;

	/* Size */
	do {
		ui = getuint(lp, "Size ('*' for entire disk)",
		  "The size of the OpenBSD portion of the disk ('*' for the "
		  "entire disk).", ending_sector - starting_sector,
		  DL_GETDSIZE(lp) - start_temp, 0, 0);
		if (ui == ULLONG_MAX - 1) {
			fputs("Command aborted\n", stderr);
			return;
		}
	} while (ui > DL_GETDSIZE(lp) - start_temp);
	ending_sector = start_temp + ui;
	starting_sector = start_temp;
}

/*
 * Return a list of the "chunks" of free space available
 */
struct diskchunk *
free_chunks(struct disklabel *lp)
{
	struct partition **spp;
	static struct diskchunk chunks[MAXPARTITIONS + 2];
	u_int64_t start, stop;
	int i, numchunks;

	/* Sort the in-use partitions based on offset */
	spp = sort_partitions(lp);

	/* If there are no partitions, it's all free. */
	if (spp[0] == NULL) {
		chunks[0].start = starting_sector;
		chunks[0].stop = ending_sector;
		chunks[1].start = chunks[1].stop = 0;
		return(chunks);
	}

	/* Find chunks of free space */
	numchunks = 0;
	if (DL_GETPOFFSET(spp[0]) > starting_sector) {
		chunks[0].start = starting_sector;
		chunks[0].stop = DL_GETPOFFSET(spp[0]);
		numchunks++;
	}
	for (i = 0; spp[i] != NULL; i++) {
		start = DL_GETPOFFSET(spp[i]) + DL_GETPSIZE(spp[i]);
		if (start < starting_sector)
			start = starting_sector;
		else if (start > ending_sector)
			start = ending_sector;
		if (spp[i + 1] != NULL)
			stop = DL_GETPOFFSET(spp[i+1]);
		else
			stop = ending_sector;
		if (stop < starting_sector)
			stop = starting_sector;
		else if (stop > ending_sector)
			stop = ending_sector;
		if (start < stop) {
			chunks[numchunks].start = start;
			chunks[numchunks].stop = stop;
			numchunks++;
		}
	}

	/* Terminate and return */
	chunks[numchunks].start = chunks[numchunks].stop = 0;
	return(chunks);
}

void
find_bounds(struct disklabel *lp)
{
	starting_sector = DL_GETBSTART(lp);
	ending_sector = DL_GETBEND(lp);

	if (ending_sector) {
		if (verbose)
			printf("Treating sectors %llu-%llu as the OpenBSD"
			    " portion of the disk.\nYou can use the 'b'"
			    " command to change this.\n\n", starting_sector,
			    ending_sector);
	} else {
#if (NUMBOOT == 1)
		/* Boot blocks take up the first cylinder */
		starting_sector = lp->d_secpercyl;
		if (verbose)
			printf("Reserving the first data cylinder for boot"
			    " blocks.\nYou can use the 'b' command to change"
			    " this.\n\n");
#endif
	}
}

/*
 * Calculate free space.
 */
u_int64_t
editor_countfree(struct disklabel *lp)
{
	struct diskchunk *chunks;
	u_int64_t freesectors = 0;
	int i;

	chunks = free_chunks(lp);

	for (i = 0; chunks[i].start != 0 || chunks[i].stop != 0; i++)
		freesectors += chunks[i].stop - chunks[i].start;

	return (freesectors);	
}

void
editor_help(void)
{
	puts("Available commands:");
	puts(
"  ? | h    - show help                  n [part] - set mount point\n"
"  A        - auto partition all space   p [unit] - print partitions\n"
"  a [part] - add partition              q        - quit & save changes\n"
"  b        - set OpenBSD boundaries     R [part] - resize a partition\n"
"  c [part] - change partition size      r        - display free space\n"
"  D        - reset label to default     s [path] - save label to file\n"
"  d [part] - delete partition           U        - undo all changes\n"
"  e        - edit drive parameters      u        - undo last change\n"
"  g [d|u]  - [d]isk or [u]ser geometry  w        - write label to disk\n"
"  l [unit] - print disk label header    X        - toggle expert mode\n"
"  M        - disklabel(8) man page      x        - exit & lose changes\n"
"  m [part] - modify partition           z        - delete all partitions\n"
"\n"
"Suffixes can be used to indicate units other than sectors:\n"
"\t'b' (bytes), 'k' (kilobytes), 'm' (megabytes), 'g' (gigabytes)\n"
"\t'c' (cylinders), '%' (% of total disk), '&' (% of free space).\n"
"Values in non-sector units are truncated to the nearest cylinder boundary.");

}

void
mpcopy(char **to, char **from)
{
	int i;
	char *top;

	for (i = 0; i < MAXPARTITIONS; i++) {
		if (from[i] != NULL) {
			int len = strlen(from[i]) + 1;

			top = realloc(to[i], len);
			if (top == NULL)
				errx(4, "out of memory");
			to[i] = top;
			(void)strlcpy(to[i], from[i], len);
		} else if (to[i] != NULL) {
			free(to[i]);
			to[i] = NULL;
		}
	}
}

int
mpequal(char **mp1, char **mp2)
{
	int i;

	for (i = 0; i < MAXPARTITIONS; i++) {
		if (mp1[i] == NULL && mp2[i] == NULL)
			continue;

		if ((mp1[i] != NULL && mp2[i] == NULL) ||
		    (mp1[i] == NULL && mp2[i] != NULL) ||
		    (strcmp(mp1[i], mp2[i]) != 0))
			return(0);
	}
	return(1);
}

void
mpsave(struct disklabel *lp)
{
	int i, j;
	char bdev[MAXPATHLEN], *p;
	struct mountinfo mi[MAXPARTITIONS];
	FILE *fp;

	if (!fstabfile)
		return;

	memset(&mi, 0, sizeof(mi));

	for (i = 0; i < MAXPARTITIONS; i++) {
		if (mountpoints[i] != NULL) {
			mi[i].mountpoint = mountpoints[i];
			mi[i].partno = i;
		}
	}

	/* Convert specname to bdev */
	if (strncmp(_PATH_DEV, specname, sizeof(_PATH_DEV) - 1) == 0 &&
	    specname[sizeof(_PATH_DEV) - 1] == 'r') {
		snprintf(bdev, sizeof(bdev), "%s%s", _PATH_DEV,
		    &specname[sizeof(_PATH_DEV)]);
	} else {
		if ((p = strrchr(specname, '/')) == NULL || *(++p) != 'r')
			return;
		*p = '\0';
		snprintf(bdev, sizeof(bdev), "%s%s", specname, p + 1);
		*p = 'r';
	}
	bdev[strlen(bdev) - 1] = '\0';

	/* Sort mountpoints so we don't try to mount /usr/local before /usr */
	qsort((void *)mi, MAXPARTITIONS, sizeof(struct mountinfo), micmp);

	if (fp = fopen(fstabfile, "w")) {
		for (i = 0; i < MAXPARTITIONS && mi[i].mountpoint; i++) {
			j =  mi[i].partno;
			fprintf(fp, "%s%c %s %s rw 1 %d\n", bdev, 'a' + j,
			    mi[i].mountpoint,
			    fstypesnames[lp->d_partitions[j].p_fstype],
			    j == 0 ? 1 : 2);
		}
		fclose(fp);
	}
}

int
get_offset(struct disklabel *lp, int partno)
{
	struct diskchunk *chunks;
	struct partition *pp = &lp->d_partitions[partno];
	u_int64_t ui, maxsize;
	int i, fstype;

	ui = getuint(lp, "offset",
	   "Starting sector for this partition.",
	   DL_GETPOFFSET(pp),
	   DL_GETPOFFSET(pp), 0, DO_CONVERSIONS |
	   (pp->p_fstype == FS_BSDFFS ? DO_ROUNDING : 0));

	if (ui == ULLONG_MAX - 1)
		fputs("Command aborted\n", stderr);
	else if (ui == ULLONG_MAX)
		fputs("Invalid entry\n", stderr);
	else if (ui < starting_sector || ui >= ending_sector)
		fprintf(stderr, "The offset must be >= %llu and < %llu, "
		    "the limits of the OpenBSD portion\n"
		    "of the disk. The 'b' command can change these limits.\n",
		    starting_sector, ending_sector);
#ifdef SUN_AAT0
	else if (partno == 0 && ui != 0)
		fprintf(stderr, "This architecture requires that "
		    "partition 'a' start at sector 0.\n");
#endif
	else {
		fstype = pp->p_fstype;
		pp->p_fstype = FS_UNUSED;
		chunks = free_chunks(lp);
		pp->p_fstype = fstype;
		for (i = 0; chunks[i].start != 0 || chunks[i].stop != 0; i++) {
			if (ui < chunks[i].start || ui >= chunks[i].stop)
				continue;
			DL_SETPOFFSET(pp, ui);
			maxsize = chunks[i].stop - DL_GETPOFFSET(pp);
			if (DL_GETPSIZE(pp) > maxsize)
				DL_SETPSIZE(pp, maxsize);
			return (0);
		}
		fputs("The offset must be in a free area.\n", stderr);
	}

	/* Partition offset was not set. */
	return (1);
}

int
get_size(struct disklabel *lp, int partno)
{
	struct partition *pp = &lp->d_partitions[partno];
	u_int64_t maxsize, ui;

	maxsize = max_partition_size(lp, partno);

	ui = getuint(lp, "size", "Size of the partition. "
	    "You may also say +/- amount for a relative change.",
	    DL_GETPSIZE(pp), maxsize, DL_GETPOFFSET(pp),
	    DO_CONVERSIONS | ((pp->p_fstype == FS_BSDFFS ||
	    pp->p_fstype == FS_SWAP) ?  DO_ROUNDING : 0));

	if (ui == ULLONG_MAX - 1)
		fputs("Command aborted\n", stderr);
	else if (ui == ULLONG_MAX)
		fputs("Invalid entry\n", stderr);
	else if (ui == 0)
		fputs("The size must be > 0\n", stderr);
	else if (ui + DL_GETPOFFSET(pp) > ending_sector)
		fprintf(stderr, "The size can't be more than "
		    "%llu sectors, or the partition would\n"
		    "extend beyond the last sector (%llu) of the "
		    "OpenBSD portion of\nthe disk. "
		    "The 'b' command can change this limit.\n",
		    ending_sector - DL_GETPOFFSET(pp), ending_sector);
	else if (ui > maxsize)
		fprintf(stderr,"Sorry, there are only %llu sectors left\n",
		    maxsize);
	else {
		DL_SETPSIZE(pp, ui);
		return (0);
	}

	/* Partition size was not set. */
	return (1);
}

int
get_fsize(struct disklabel *lp, int partno)
{
	u_int64_t ui, fsize, frag;
	struct partition *pp = &lp->d_partitions[partno];
	
	if (!expert || pp->p_fstype != FS_BSDFFS)
		return (0);

	fsize = DISKLABELV1_FFS_FSIZE(pp->p_fragblock);
	frag = DISKLABELV1_FFS_FRAG(pp->p_fragblock);
	if (fsize == 0)
		frag = 8;

	for (;;) {
		ui = getuint(lp, "fragment size",
		    "Size of fs block fragments.  Usually 2048 or 512.",
		    fsize, fsize, 0, 0);
		if (ui == ULLONG_MAX - 1) {
			fputs("Command aborted\n", stderr);
			return(1);
		} else if (ui == ULLONG_MAX)
			fputs("Invalid entry\n", stderr);
		else
			break;
	}
	if (ui == 0)
		puts("Zero fragment size implies zero block size");
	pp->p_fragblock = DISKLABELV1_FFS_FRAGBLOCK(ui, frag);
	return(0);
}

int
get_bsize(struct disklabel *lp, int partno)
{
	u_int64_t ui, bsize, frag, fsize;
	struct partition *pp = &lp->d_partitions[partno];

	if (!expert || pp->p_fstype != FS_BSDFFS)
		return (0);

	/* Avoid dividing by zero... */
	if (pp->p_fragblock == 0)
		return(1);

	bsize = DISKLABELV1_FFS_BSIZE(pp->p_fragblock);
	fsize = DISKLABELV1_FFS_FSIZE(pp->p_fragblock);
	frag = DISKLABELV1_FFS_FRAG(pp->p_fragblock);

	for (;;) {
		ui = getuint(lp, "block size",
		    "Size of filesystem blocks.  Usually 16384 or 4096.",
		    fsize * frag, fsize * frag,
		    0, 0);

		/* sanity checks */
		if (ui == ULLONG_MAX - 1) {
			fputs("Command aborted\n", stderr);
			return(1);
		} else if (ui == ULLONG_MAX)
			fputs("Invalid entry\n", stderr);
		else if (ui < getpagesize())
			fprintf(stderr,
			    "Error: block size must be at least as big "
			    "as page size (%d).\n", getpagesize());
		else if (ui % fsize != 0)
			fputs("Error: block size must be a multiple of the "
			    "fragment size.\n", stderr);
		else if (ui / fsize < 1)
			fputs("Error: block size must be at least as big as "
			    "fragment size.\n", stderr);
		else
			break;
	}
	pp->p_fragblock = DISKLABELV1_FFS_FRAGBLOCK(ui / frag, frag);
	return(0);
}

int
get_fstype(struct disklabel *lp, int partno)
{
	char *p;
	u_int64_t ui;
	struct partition *pp = &lp->d_partitions[partno];

	if (pp->p_fstype < FSMAXTYPES) {
		p = getstring("FS type",
		    "Filesystem type (usually 4.2BSD or swap)",
		    fstypenames[pp->p_fstype]);
		if (p == NULL) {
			fputs("Command aborted\n", stderr);
			return(1);
		}
		for (ui = 0; ui < FSMAXTYPES; ui++) {
			if (!strcasecmp(p, fstypenames[ui])) {
				pp->p_fstype = ui;
				break;
			}
		}
		if (ui >= FSMAXTYPES) {
			printf("Unrecognized filesystem type '%s', treating as 'unknown'\n", p);
			pp->p_fstype = FS_OTHER;
		}
	} else {
		for (;;) {
			ui = getuint(lp, "FS type (decimal)",
			    "Filesystem type as a decimal number; usually 7 (4.2BSD) or 1 (swap).",
			    pp->p_fstype, pp->p_fstype, 0, 0);
			if (ui == ULLONG_MAX - 1) {
				fputs("Command aborted\n", stderr);
				return(1);
			} if (ui == ULLONG_MAX)
				fputs("Invalid entry\n", stderr);
			else
				break;
		}
		pp->p_fstype = ui;
	}
	return(0);
}

int
get_mp(struct disklabel *lp, int partno)
{
	struct partition *pp = &lp->d_partitions[partno];
	char *p;
	int i;

	if (fstabfile && pp->p_fstype != FS_UNUSED &&
	    pp->p_fstype != FS_SWAP && pp->p_fstype != FS_BOOT &&
	    pp->p_fstype != FS_OTHER) {
		for (;;) {
			p = getstring("mount point",
			    "Where to mount this filesystem (ie: / /var /usr)",
			    mountpoints[partno] ? mountpoints[partno] : "none");
			if (p == NULL) {
				fputs("Command aborted\n", stderr);
				return(1);
			}
			if (strcasecmp(p, "none") == 0) {
				free(mountpoints[partno]);
				mountpoints[partno] = NULL;
				break;
			}
			for (i = 0; i < MAXPARTITIONS; i++)
				if (mountpoints[i] != NULL && i != partno &&
				    strcmp(p, mountpoints[i]) == 0)
					break;
			if (i < MAXPARTITIONS) {
				fprintf(stderr, "'%c' already being mounted at "
				    "'%s'\n", 'a'+i, p);
				break;
			}
			if (*p == '/') {
				/* XXX - might as well realloc */
				free(mountpoints[partno]);
				if ((mountpoints[partno] = strdup(p)) == NULL)
					errx(4, "out of memory");
				break;
			}
			fputs("Mount points must start with '/'\n", stderr);
		}
	}
	return(0);
}

int
micmp(const void *a1, const void *a2)
{
	struct mountinfo *mi1 = (struct mountinfo *)a1;
	struct mountinfo *mi2 = (struct mountinfo *)a2;

	/* We want all the NULLs at the end... */
	if (mi1->mountpoint == NULL && mi2->mountpoint == NULL)
		return(0);
	else if (mi1->mountpoint == NULL)
		return(1);
	else if (mi2->mountpoint == NULL)
		return(-1);
	else
		return(strcmp(mi1->mountpoint, mi2->mountpoint));
}

void
get_geometry(int f, struct disklabel **dgpp)
{
	struct stat st;
	struct disklabel *disk_geop;

	if (fstat(f, &st) == -1)
		err(4, "Can't stat device");

	/* Get disk geometry */
	if ((disk_geop = calloc(1, sizeof(struct disklabel))) == NULL)
		errx(4, "out of memory");
	if (ioctl(f, DIOCGPDINFO, disk_geop) < 0 &&
	    ioctl(f, DIOCGDINFO, disk_geop) < 0)
		err(4, "ioctl DIOCGDINFO");
	*dgpp = disk_geop;
}

void
set_geometry(struct disklabel *lp, struct disklabel *dgp,
    struct disklabel *ugp, char *p)
{
	if (p == NULL) {
		p = getstring("[d]isk or [u]ser geometry",
		    "Enter 'd' to use the geometry based on what the disk "
		    "itself thinks it is, or 'u' to use the geometry that "
		    "was found in the label.",
		    "d");
	}
	if (p == NULL) {
		fputs("Command aborted\n", stderr);
		return;
	}
	switch (*p) {
	case 'd':
	case 'D':
		if (dgp == NULL)
			fputs("BIOS geometry not defined.\n", stderr);
		else {
			lp->d_secsize = dgp->d_secsize;
			lp->d_nsectors = dgp->d_nsectors;
			lp->d_ntracks = dgp->d_ntracks;
			lp->d_ncylinders = dgp->d_ncylinders;
			lp->d_secpercyl = dgp->d_secpercyl;
			DL_SETDSIZE(lp, DL_GETDSIZE(dgp));
		}
		break;
	case 'u':
	case 'U':
		if (ugp == NULL)
			fputs("BIOS geometry not defined.\n", stderr);
		else {
			lp->d_secsize = ugp->d_secsize;
			lp->d_nsectors = ugp->d_nsectors;
			lp->d_ntracks = ugp->d_ntracks;
			lp->d_ncylinders = ugp->d_ncylinders;
			lp->d_secpercyl = ugp->d_secpercyl;
			DL_SETDSIZE(lp, DL_GETDSIZE(ugp));
			if (dgp != NULL && ugp->d_secsize == dgp->d_secsize &&
			    ugp->d_nsectors == dgp->d_nsectors &&
			    ugp->d_ntracks == dgp->d_ntracks &&
			    ugp->d_ncylinders == dgp->d_ncylinders &&
			    ugp->d_secpercyl == dgp->d_secpercyl &&
			    DL_GETDSIZE(ugp) == DL_GETDSIZE(dgp))
				fputs("Note: user geometry is the same as disk "
				    "geometry.\n", stderr);
		}
		break;
	default:
		fputs("You must enter either 'd' or 'u'.\n", stderr);
		break;
	}
}

void
zero_partitions(struct disklabel *lp)
{
	int i;

	for (i = 0; i < MAXPARTITIONS; i++) {
		memset(&lp->d_partitions[i], 0, sizeof(struct partition));
		free(mountpoints[i]);
		mountpoints[i] = NULL;
	}

	DL_SETPSIZE(&lp->d_partitions[RAW_PART], DL_GETDSIZE(lp));
}

u_int64_t
max_partition_size(struct disklabel *lp, int partno)
{
	struct partition *pp = &lp->d_partitions[partno];
	struct diskchunk *chunks;
	u_int64_t maxsize = 0, offset;
	int fstype, i;

	fstype = pp->p_fstype;
	pp->p_fstype = FS_UNUSED;
	chunks = free_chunks(lp);
	pp->p_fstype = fstype;

	offset = DL_GETPOFFSET(pp);
	for (i = 0; chunks[i].start != 0 || chunks[i].stop != 0; i++) {
		if (offset < chunks[i].start || offset >= chunks[i].stop)
			continue;
		maxsize = chunks[i].stop - offset;
		break;
	}	
	return (maxsize);
}

void
psize(daddr64_t sz, char unit, struct disklabel *lp)
{
	double d = scale(sz, unit, lp);
	if (d < 0)
		printf("%llu", sz);
	else
		printf("%.*f%c", unit == 'B' ? 0 : 1, d, unit);
}

void
display_edit(struct disklabel *lp, char unit, u_int64_t fr)
{
	int i;

	unit = canonical_unit(lp, unit);

	printf("OpenBSD area: ");
	psize(starting_sector, 0, lp);
	printf("-");
	psize(ending_sector, 0, lp);
	printf("; size: ");
	psize(ending_sector - starting_sector, unit, lp);
	printf("; free: ");
	psize(fr, unit, lp);

	printf("\n#    %16.16s %16.16s  fstype [fsize bsize  cpg]\n",
	    "size", "offset");
	for (i = 0; i < lp->d_npartitions; i++)
		display_partition(stdout, lp, i, unit);
}

