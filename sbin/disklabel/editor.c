/*	$OpenBSD: editor.c,v 1.352 2018/11/25 17:01:20 krw Exp $	*/

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

#include <sys/param.h>	/* MAXBSIZE DEV_BSIZE */
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/dkio.h>
#include <sys/sysctl.h>
#define	DKTYPENAMES
#include <sys/disklabel.h>

#include <ufs/ffs/fs.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#include "extern.h"
#include "pathnames.h"

#define MAXIMUM(a, b)	(((a) > (b)) ? (a) : (b))

/* flags for getuint64() */
#define	DO_CONVERSIONS	0x00000001
#define	DO_ROUNDING	0x00000002

/* flags for alignpartition() */
#define	ROUND_OFFSET_UP		0x00000001
#define	ROUND_OFFSET_DOWN	0x00000002
#define	ROUND_SIZE_UP		0x00000004
#define	ROUND_SIZE_DOWN		0x00000008
#define	ROUND_SIZE_OVERLAP	0x00000010

/* Special return values for getnumber and getuint64() */
#define	CMD_ABORTED	(ULLONG_MAX - 1)
#define	CMD_BADVALUE	(ULLONG_MAX)

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
	u_int64_t	minsz;	/* starts as blocks, xlated to sectors. */
	u_int64_t	maxsz;	/* starts as blocks, xlated to sectors. */
	int		rate;	/* % of extra space to use */
	char	       *mp;
};

/* entries for swap and var are changed by editor_allocspace() */
struct space_allocation alloc_big[] = {
	{  MEG(150),         GIG(1),   5, "/"		},
	{   MEG(80),       MEG(256),  10, "swap"	},
	{  MEG(120),         GIG(4),   8, "/tmp"	},
	{   MEG(80),         GIG(4),  13, "/var"	},
	{  MEG(900),         GIG(2),   5, "/usr"	},
	{  MEG(384),         GIG(1),   3, "/usr/X11R6"	},
	{    GIG(1),        GIG(20),  15, "/usr/local"	},
	{ MEG(1300),         GIG(2),   2, "/usr/src"	},
	{    GIG(5),         GIG(6),   4, "/usr/obj"	},
	{    GIG(1),       GIG(300),  35, "/home"	}
	/* Anything beyond this leave for the user to decide */
};

struct space_allocation alloc_medium[] = {
	{  MEG(800),         GIG(2),   5, "/"		},
	{   MEG(80),       MEG(256),  10, "swap"	},
	{  MEG(900),         GIG(3),  78, "/usr"	},
	{  MEG(256),         GIG(2),   7, "/home"	}
};

struct space_allocation alloc_small[] = {
	{  MEG(700),         GIG(4),  95, "/"		},
	{    MEG(1),       MEG(256),   5, "swap"	}
};

struct space_allocation alloc_stupid[] = {
	{    MEG(1),      MEG(2048), 100, "/"		}
};

#ifndef nitems
#define nitems(_a)	(sizeof((_a)) / sizeof((_a)[0]))
#endif

struct alloc_table {
	struct space_allocation *table;
	int sz;
};

struct alloc_table alloc_table_default[] = {
	{ alloc_big,	nitems(alloc_big) },
	{ alloc_medium,	nitems(alloc_medium) },
	{ alloc_small,	nitems(alloc_small) },
	{ alloc_stupid,	nitems(alloc_stupid) }
};
struct alloc_table *alloc_table = alloc_table_default;
int alloc_table_nitems = 4;

void	edit_parms(struct disklabel *);
void	editor_resize(struct disklabel *, char *);
void	editor_add(struct disklabel *, char *);
void	editor_change(struct disklabel *, char *);
u_int64_t editor_countfree(struct disklabel *);
void	editor_delete(struct disklabel *, char *);
void	editor_help(void);
void	editor_modify(struct disklabel *, char *);
void	editor_name(struct disklabel *, char *);
char	*getstring(const char *, const char *, const char *);
u_int64_t getuint64(struct disklabel *, char *, char *, u_int64_t,
    u_int64_t, int *);
u_int64_t getnumber(char *, char *, u_int32_t, u_int32_t);
int	has_overlap(struct disklabel *);
int	partition_cmp(const void *, const void *);
struct partition **sort_partitions(struct disklabel *);
void	getdisktype(struct disklabel *, char *, char *);
void	find_bounds(struct disklabel *);
void	set_bounds(struct disklabel *);
void	set_duid(struct disklabel *);
struct diskchunk *free_chunks(struct disklabel *);
int	micmp(const void *, const void *);
int	mpequal(char **, char **);
int	get_bsize(struct disklabel *, int);
int	get_fsize(struct disklabel *, int);
int	get_cpg(struct disklabel *, int);
int	get_fstype(struct disklabel *, int);
int	get_mp(struct disklabel *, int);
int	get_offset(struct disklabel *, int);
int	get_size(struct disklabel *, int);
void	get_geometry(int, struct disklabel **);
void	set_geometry(struct disklabel *, struct disklabel *, struct disklabel *,
    char *);
void	zero_partitions(struct disklabel *);
u_int64_t max_partition_size(struct disklabel *, int);
void	display_edit(struct disklabel *, char);
void	psize(u_int64_t sz, char unit, struct disklabel *lp);
char	*get_token(char **, size_t *);
int	apply_unit(double, u_char, u_int64_t *);
int	parse_sizespec(const char *, double *, char **);
int	parse_sizerange(char *, u_int64_t *, u_int64_t *);
int	parse_pct(char *, int *);
int	alignpartition(struct disklabel *, int, u_int64_t, u_int64_t, int);

static u_int64_t starting_sector;
static u_int64_t ending_sector;
static int expert;
static int overlap;

/*
 * Simple partition editor.
 */
int
editor(int f)
{
	struct disklabel origlabel, lastlabel, tmplabel, newlab = lab;
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
	getdisktype(&newlab, "You need to specify a type for this disk.",
	    specname);

	/* Get the on-disk geometries if possible */
	get_geometry(f, &disk_geop);

	/* How big is the OpenBSD portion of the disk?  */
	find_bounds(&newlab);

	/* Make sure there is no partition overlap. */
	if (has_overlap(&newlab))
		errx(1, "can't run when there is partition overlap.");

	/* If we don't have a 'c' partition, create one. */
	pp = &newlab.d_partitions[RAW_PART];
	if (newlab.d_npartitions < 3 || DL_GETPSIZE(pp) == 0) {
		puts("No 'c' partition found, adding one that spans the disk.");
		if (newlab.d_npartitions < 3)
			newlab.d_npartitions = 3;
		DL_SETPOFFSET(pp, 0);
		DL_SETPSIZE(pp, DL_GETDSIZE(&newlab));
		pp->p_fstype = FS_UNUSED;
		pp->p_fragblock = pp->p_cpg = 0;
	}

#ifdef SUN_CYLCHECK
	if ((newlab.d_flags & D_VENDOR) && !quiet) {
		puts("This platform requires that partition offsets/sizes "
		    "be on cylinder boundaries.\n"
		    "Partition offsets/sizes will be rounded to the "
		    "nearest cylinder automatically.");
	}
#endif

	/* Set d_bbsize and d_sbsize as necessary */
	if (newlab.d_bbsize == 0)
		newlab.d_bbsize = BBSIZE;
	if (newlab.d_sbsize == 0)
		newlab.d_sbsize = SBSIZE;

	/* Save the (U|u)ndo labels and mountpoints. */
	mpcopy(origmountpoints, mountpoints);
	origlabel = newlab;
	lastlabel = newlab;

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
			lastlabel = newlab;
			mpcopy(tmpmountpoints, omountpoints);
			mpcopy(omountpoints, mountpoints);
		}

		switch (*cmd) {
		case '?':
		case 'h':
			editor_help();
			break;

		case 'A':
			if (ioctl(f, DIOCGPDINFO, &newlab) == 0) {
				int oquiet = quiet, oexpert = expert;
				aflag = 1;
				quiet = expert = 0;
				editor_allocspace(&newlab);
				quiet = oquiet;
				expert = oexpert;
			} else
				newlab = lastlabel;
			break;
		case 'a':
			editor_add(&newlab, arg);
			break;

		case 'b':
			set_bounds(&newlab);
			break;

		case 'c':
			editor_change(&newlab, arg);
			break;

		case 'D':
			if (ioctl(f, DIOCGPDINFO, &newlab) == 0) {
				dflag = 1;
				for (i=0; i<MAXPARTITIONS; i++) {
					free(mountpoints[i]);
					mountpoints[i] = NULL;
				}
			} else
				warn("unable to get default partition table");
			break;

		case 'd':
			editor_delete(&newlab, arg);
			break;

		case 'e':
			edit_parms(&newlab);
			break;

		case 'g':
			set_geometry(&newlab, disk_geop, &lab, arg);
			break;

		case 'i':
			set_duid(&newlab);
			break;

		case 'm':
			editor_modify(&newlab, arg);
			break;

		case 'n':
			if (!fstabfile) {
				fputs("This option is not valid when run "
				    "without the -f flag.\n", stderr);
				break;
			}
			editor_name(&newlab, arg);
			break;

		case 'p':
			display_edit(&newlab, arg ? *arg : 0);
			break;

		case 'l':
			display(stdout, &newlab, arg ? *arg : 0, 0);
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

			/*
			 * If we haven't changed the original label, and it
			 * wasn't a default label or an auto-allocated label,
			 * there is no need to do anything before exiting. Note
			 * that 'w' will reset dflag and aflag to allow 'q' to
			 * exit without further questions.
			 */
			if (!dflag && !aflag &&
			    memcmp(&lab, &newlab, sizeof(newlab)) == 0) {
				puts("No label changes.");
				/* Save mountpoint info. */
				mpsave(&newlab);
				goto done;
			}
			do {
				arg = getstring("Write new label?",
				    "Write the modified label to disk?",
				    "y");
			} while (arg && tolower((unsigned char)*arg) != 'y' &&
			    tolower((unsigned char)*arg) != 'n');
			if (arg && tolower((unsigned char)*arg) == 'y') {
				if (writelabel(f, &newlab) == 0) {
					newlab = lab; /* lab now has UID info */
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
				editor_resize(&newlab, arg);
			else
				fputs("Resize only implemented for auto "
				    "allocated labels\n", stderr);
			break;

		case 'r': {
			struct diskchunk *chunks;
			int i;
			/* Display free space. */
			chunks = free_chunks(&newlab);
			for (i = 0; chunks[i].start != 0 || chunks[i].stop != 0;
			    i++)
				fprintf(stderr, "Free sectors: %16llu - %16llu "
				    "(%16llu)\n",
				    chunks[i].start, chunks[i].stop - 1,
				    chunks[i].stop - chunks[i].start);
			fprintf(stderr, "Total free sectors: %llu.\n",
			    editor_countfree(&newlab));
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
				display(fp, &newlab, 0, 1);
				(void)fclose(fp);
			}
			break;

		case 'U':
			/*
			 * If we allow 'U' repeatedly, information would be
			 * lost. This way multiple 'U's followed by 'u' will
			 * undo the 'U's.
			 */
			if (memcmp(&newlab, &origlabel, sizeof(newlab)) ||
			    !mpequal(mountpoints, origmountpoints)) {
				tmplabel = newlab;
				newlab = origlabel;
				lastlabel = tmplabel;
				mpcopy(tmpmountpoints, mountpoints);
				mpcopy(mountpoints, origmountpoints);
				mpcopy(omountpoints, tmpmountpoints);
			}
			puts("Original label and mount points restored.");
			break;

		case 'u':
			tmplabel = newlab;
			newlab = lastlabel;
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
			if (writelabel(f, &newlab) != 0)
				warnx("unable to write label");
			else {
				dflag = aflag = 0;
				newlab = lab; /* lab now has UID info */
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
			zero_partitions(&newlab);
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
		if (memcmp(&newlab, &lastlabel, sizeof(newlab)) == 0 &&
		    (mpequal(mountpoints, omountpoints))) {
			lastlabel = tmplabel;
			mpcopy(omountpoints, tmpmountpoints);
		}
	}
done:
	mpfree(omountpoints);
	mpfree(origmountpoints);
	mpfree(tmpmountpoints);
	free(disk_geop);
	return (error);
}

/*
 * Allocate all disk space according to standard recommendations for a
 * root disk.
 */
int
editor_allocspace(struct disklabel *lp_org)
{
	struct disklabel *lp, label;
	struct space_allocation *alloc;
	struct space_allocation *ap;
	struct partition *pp;
	struct diskchunk *chunks;
	u_int64_t chunkstart, chunkstop, chunksize;
	u_int64_t cylsecs, secs, xtrasecs;
	char **partmp;
	int i, j, lastalloc, index, partno, freeparts;
	extern int64_t physmem;

	/* How big is the OpenBSD portion of the disk?  */
	find_bounds(lp_org);

	overlap = 0;
	freeparts = 0;
	for (i = 0;  i < MAXPARTITIONS; i++) {
		u_int64_t psz, pstart, pend;

		pp = &lp_org->d_partitions[i];
		psz = DL_GETPSIZE(pp);
		if (psz == 0)
			freeparts++;
		pstart = DL_GETPOFFSET(pp);
		pend = pstart + psz;
		if (i != RAW_PART && psz != 0 &&
		    ((pstart >= starting_sector && pstart <= ending_sector) ||
		    (pend > starting_sector && pend < ending_sector))) {
			overlap = 1;
			break;
		}
	}

	cylsecs = lp_org->d_secpercyl;
	alloc = NULL;
	index = -1;
again:
	free(alloc);
	alloc = NULL;
	index++;
	if (index >= alloc_table_nitems)
		return 1;
	lp = &label;
	for (i=0; i<MAXPARTITIONS; i++) {
		free(mountpoints[i]);
		mountpoints[i] = NULL;
	}
	memcpy(lp, lp_org, sizeof(struct disklabel));
	lp->d_npartitions = MAXPARTITIONS;
	lastalloc = alloc_table[index].sz;
	if (lastalloc > freeparts)
		goto again;
	alloc = reallocarray(NULL, lastalloc, sizeof(struct space_allocation));
	if (alloc == NULL)
		errx(4, "out of memory");
	memcpy(alloc, alloc_table[index].table,
	    lastalloc * sizeof(struct space_allocation));

	/* bump max swap based on phys mem, little physmem gets 2x swap */
	if (index == 0 && alloc_table == alloc_table_default) {
		if (physmem / DEV_BSIZE < MEG(256))
			alloc[1].minsz = alloc[1].maxsz = 2 * (physmem /
			    DEV_BSIZE);
		else
			alloc[1].maxsz += (physmem / DEV_BSIZE);
		/* bump max /var to make room for 2 crash dumps */
		alloc[3].maxsz += 2 * (physmem / DEV_BSIZE);
	}

	xtrasecs = editor_countfree(lp);

	for (i = 0; i < lastalloc; i++) {
		alloc[i].minsz = DL_BLKTOSEC(lp, alloc[i].minsz);
		alloc[i].maxsz = DL_BLKTOSEC(lp, alloc[i].maxsz);
		if (xtrasecs >= alloc[i].minsz)
			xtrasecs -= alloc[i].minsz;
		else {
			/* It did not work out, try next strategy */
			goto again;
		}
	}

	for (i = 0; i < lastalloc; i++) {
		/* Find next available partition. */
		for (j = 0;  j < MAXPARTITIONS; j++)
			if (DL_GETPSIZE(&lp->d_partitions[j]) == 0)
				break;
		if (j == MAXPARTITIONS) {
			/* It did not work out, try next strategy */
			goto again;
		}
		partno = j;
		pp = &lp->d_partitions[j];
		partmp = &mountpoints[j];
		ap = &alloc[i];

		/* Find largest chunk of free space. */
		chunks = free_chunks(lp);
		chunksize = 0;
		for (j = 0; chunks[j].start != 0 || chunks[j].stop != 0; j++) {
			if ((chunks[j].stop - chunks[j].start) > chunksize) {
				chunkstart = chunks[j].start;
				chunkstop = chunks[j].stop;
#ifdef SUN_CYLCHECK
				if (lp->d_flags & D_VENDOR) {
					/* Align to cylinder boundaries. */
					chunkstart = ((chunkstart + cylsecs - 1)
					    / cylsecs) * cylsecs;
					chunkstop = (chunkstop / cylsecs) *
					    cylsecs;
				}
#endif
				chunksize = chunkstop - chunkstart;
			}
		}

		/* Figure out the size of the partition. */
		if (i == lastalloc - 1) {
			if (chunksize > ap->maxsz)
				secs = ap->maxsz;
			else
				secs = chunksize;
		} else {
			secs = ap->minsz;
			if (xtrasecs > 0)
				secs += (xtrasecs / 100) * ap->rate;
			if (secs > ap->maxsz)
				secs = ap->maxsz;
		}
#ifdef SUN_CYLCHECK
		if (lp->d_flags & D_VENDOR) {
			secs = ((secs + cylsecs - 1) / cylsecs) * cylsecs;
			while (secs > chunksize)
				secs -= cylsecs;
		}
#endif

		/* See if partition can fit into chunk. */
		if (secs > chunksize)
			secs = chunksize;
		if (secs < ap->minsz) {
			/* It did not work out, try next strategy */
			goto again;
		}

		/* Everything seems ok so configure the partition. */
		DL_SETPSIZE(pp, secs);
		DL_SETPOFFSET(pp, chunkstart);
		if (ap->mp[0] != '/')
			pp->p_fstype = FS_SWAP;
		else {
			pp->p_fstype = FS_BSDFFS;
			pp->p_fragblock = 0;
			if (get_fsize(lp, partno) == 1 ||
			    get_bsize(lp, partno) == 1 ||
			    get_cpg(lp, partno) == 1) {
				free(alloc);
				return 1;
			}
			free(*partmp);
			if ((*partmp = strdup(ap->mp)) == NULL)
				errx(4, "out of memory");
		}
	}

	free(alloc);
	memcpy(lp_org, lp, sizeof(struct disklabel));
	return 0;
}

/*
 * Resize a partition, moving all subsequent partitions
 */
void
editor_resize(struct disklabel *lp, char *p)
{
	struct disklabel label;
	struct partition *pp, *prev;
	u_int64_t ui, sz, off;
	int partno, i, flags, shrunk;

	label = *lp;

	/* Change which partition? */
	if (p == NULL)
		p = getstring("partition to resize",
		    "The letter of the partition to name, a - p.", NULL);
	if (p == NULL)
		return;
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
	flags = DO_CONVERSIONS;
	ui = getuint64(lp, "[+|-]new size (with unit)",
	    "new size or amount to grow (+) or shrink (-) partition including "
	    "unit", sz, sz + editor_countfree(lp), &flags);

	if (ui == CMD_ABORTED)
		return;
	else if (ui == CMD_BADVALUE)
		return;
	else if (ui == 0) {
		fputs("The size must be > 0 sectors\n", stderr);
		return;
	}

#ifdef SUN_CYLCHECK
	if (lp->d_secpercyl & D_VENDOR) {
		u_int64_t cylsecs;
		cylsecs = lp->d_secpercyl;
		ui = ((ui + cylsecs - 1) / cylsecs) * cylsecs;
	}
#endif
	if (DL_GETPOFFSET(pp) + ui > ending_sector) {
		fputs("Amount too big\n", stderr);
		return;
	}

	DL_SETPSIZE(pp, ui);
	pp->p_fragblock = 0;
	if (get_fsize(&label, partno) == 1 ||
	    get_bsize(&label, partno) == 1 ||
	    get_cpg(&label, partno) == 1)
		return;

	/*
	 * Pack partitions above the resized partition, leaving unused
	 * partitions alone.
	 */
	shrunk = -1;
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
				pp->p_fragblock = 0;
				if (get_fsize(&label, partno) == 1 ||
				    get_bsize(&label, partno) == 1 ||
				    get_cpg(&label, partno) == 1)
					return;
				shrunk = i;
			}
		} else {
			fputs("Amount too big\n", stderr);
			return;
		}
		prev = pp;
	}

	if (shrunk != -1)
		fprintf(stderr, "Partition %c shrunk to %llu sectors to make "
		    "room\n", 'a' + shrunk,
		    DL_GETPSIZE(&label.d_partitions[shrunk]));
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
	int i, partno;
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
	if (p == NULL)
		return;
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

	if (get_offset(lp, partno) == 0 &&
	    get_size(lp, partno) == 0 &&
	    get_fstype(lp, partno) == 0 &&
	    get_mp(lp, partno) == 0 &&
	    get_fsize(lp, partno) == 0  &&
	    get_bsize(lp, partno) == 0 &&
	    get_cpg(lp, partno) == 0)
		return;

	/* Bailed out at some point, so effectively delete the partition. */
	memset(pp, 0, sizeof(*pp));
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
	if (p == NULL)
		p = getstring("partition to name",
		    "The letter of the partition to name, a - p.", NULL);
	if (p == NULL)
		return;
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

	get_mp(lp, partno);
}

/*
 * Change an existing partition.
 */
void
editor_modify(struct disklabel *lp, char *p)
{
	struct partition opp, *pp;
	int partno;

	/* Change which partition? */
	if (p == NULL)
		p = getstring("partition to modify",
		    "The letter of the partition to modify, a - p.", NULL);
	if (p == NULL)
		return;
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

	opp = *pp;

	if (get_offset(lp, partno) == 0 &&
	    get_size(lp, partno) == 0   &&
	    get_fstype(lp, partno) == 0 &&
	    get_mp(lp, partno) == 0 &&
	    get_fsize(lp, partno) == 0  &&
	    get_bsize(lp, partno) == 0 &&
	    get_cpg(lp, partno) == 0)
		return;

	/* Bailed out at some point, so undo any changes. */
	*pp = opp;
}

/*
 * Delete an existing partition.
 */
void
editor_delete(struct disklabel *lp, char *p)
{
	struct partition *pp;
	int partno;

	if (p == NULL)
		p = getstring("partition to delete",
		    "The letter of the partition to delete, a - p, or '*'.",
		    NULL);
	if (p == NULL)
		return;
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

	if (p == NULL)
		p = getstring("partition to change size",
		    "The letter of the partition to change size, a - p.", NULL);
	if (p == NULL)
		return;
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
getstring(const char *prompt, const char *helpstring, const char *oval)
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
				fputs("Command aborted\n", stderr);
				return (NULL);
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

	return (&buf[0]);
}

/*
 * Returns
 * 0 .. CMD_ABORTED - 1	==> valid value
 * CMD_BADVALUE		==> invalid value
 * CMD_ABORTED		==> ^D on input
 */
u_int64_t
getnumber(char *prompt, char *helpstring, u_int32_t oval, u_int32_t maxval)
{
	char buf[BUFSIZ], *p;
	int rslt;
	long long rval;
	const char *errstr;

	rslt = snprintf(buf, sizeof(buf), "%u", oval);
	if (rslt == -1 || (unsigned int)rslt >= sizeof(buf))
		return (CMD_BADVALUE);

	p = getstring(prompt, helpstring, buf);
	if (p == NULL)
		return (CMD_ABORTED);
	if (strlen(p) == 0)
		return (oval);

	rval = strtonum(p, 0, maxval, &errstr);
	if (errstr != NULL) {
		printf("%s must be between 0 and %u\n", prompt, maxval);
		return (CMD_BADVALUE);
	}

	return (rval);
}

/*
 * Returns
 * 0 .. CMD_ABORTED - 1	==> valid value
 * CMD_BADVALUE		==> invalid value
 * CMD_ABORTED		==> ^D on input
 */
u_int64_t
getuint64(struct disklabel *lp, char *prompt, char *helpstring,
    u_int64_t oval, u_int64_t maxval, int *flags)
{
	char buf[21], *p, operator = '\0';
	char *unit = NULL;
	u_int64_t rval = oval;
	double d;
	int rslt;

	rslt = snprintf(buf, sizeof(buf), "%llu", oval);
	if (rslt == -1 || (unsigned int)rslt >= sizeof(buf))
		goto invalid;

	p = getstring(prompt, helpstring, buf);
	if (p == NULL)
		return (CMD_ABORTED);
	else if (p[0] == '\0')
		rval = oval;
	else if (p[0] == '*' && p[1] == '\0')
		rval = maxval;
	else {
		if (*p == '+' || *p == '-')
			operator = *p++;
		if (parse_sizespec(p, &d, &unit) == -1)
			goto invalid;
		if (unit == NULL)
			rval = d;
		else if (flags != NULL && (*flags & DO_CONVERSIONS) == 0)
			goto invalid;
		else {
			switch (tolower((unsigned char)*unit)) {
			case 'b':
				rval = d / lp->d_secsize;
				break;
			case 'c':
				rval = d * lp->d_secpercyl;
				break;
			case '%':
				rval = DL_GETDSIZE(lp) * (d / 100.0);
				break;
			case '&':
				rval = maxval * (d / 100.0);
				break;
			default:
				if (apply_unit(d, *unit, &rval) == -1)
					goto invalid;
				rval = DL_BLKTOSEC(lp, rval);
				break;
			}
		}

		/* Range check then apply [+-] operator */
		if (operator == '+') {
			if (CMD_ABORTED - oval > rval)
				rval += oval;
			else {
				goto invalid;
			}
		} else if (operator == '-') {
			if (oval >= rval)
				rval = oval - rval;
			else {
				goto invalid;
			}
		}
	}

	if (flags != NULL) {
		if (unit != NULL)
			*flags |= DO_ROUNDING;
#ifdef SUN_CYLCHECK
		if (lp->d_flags & D_VENDOR)
			*flags |= DO_ROUNDING;
#endif
	}
	return (rval);

invalid:
	fputs("Invalid entry\n", stderr);
	return (CMD_BADVALUE);
}

/*
 * Check for partition overlap in lp and prompt the user to resolve the overlap
 * if any is found.  Returns 1 if unable to resolve, else 0.
 */
int
has_overlap(struct disklabel *lp)
{
	struct partition **spp;
	int i, p1, p2;
	char *line = NULL;
	size_t linesize = 0;
	ssize_t linelen;

	for (;;) {
		spp = sort_partitions(lp);
		for (i = 0; spp[i+1] != NULL; i++) {
			if (DL_GETPOFFSET(spp[i]) + DL_GETPSIZE(spp[i]) >
			    DL_GETPOFFSET(spp[i+1]))
				break;
		}
		if (spp[i+1] == NULL) {
			free(line);
			return (0);
		}

		p1 = 'a' + (spp[i] - lp->d_partitions);
		p2 = 'a' + (spp[i+1] - lp->d_partitions);
		printf("\nError, partitions %c and %c overlap:\n", p1, p2);
		printf("#    %16.16s %16.16s  fstype [fsize bsize    cpg]\n",
		    "size", "offset");
		display_partition(stdout, lp, p1 - 'a', 0);
		display_partition(stdout, lp, p2 - 'a', 0);

		for (;;) {
			printf("Disable which one? (%c %c) ", p1, p2);
			linelen = getline(&line, &linesize, stdin);
			if (linelen == -1)
				goto done;
			if (linelen == 2 && (line[0] == p1 || line[0] == p2))
				break;
		}
		lp->d_partitions[line[0] - 'a'].p_fstype = FS_UNUSED;
	}

done:
	putchar('\n');
	free(line);
	return (1);
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
		if (p == NULL)
			return;
		if (strcasecmp(p, "IDE") == 0)
			ui = DTYPE_ESDI;
		else
			for (ui = 1; ui < DKMAXTYPES && strcasecmp(p,
			    dktypenames[ui]); ui++)
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
		*lp = oldlabel;		/* undo damage */
		return;
	}
	strncpy(lp->d_packname, p, sizeof(lp->d_packname));	/* checked */

	/* sectors/track */
	for (;;) {
		ui = getnumber("sectors/track",
		    "The Number of sectors per track.", lp->d_nsectors,
		    UINT32_MAX);
		if (ui == CMD_ABORTED) {
			*lp = oldlabel;		/* undo damage */
			return;
		} else if (ui == CMD_BADVALUE)
			;	/* Try again. */
		else
			break;
	}
	lp->d_nsectors = ui;

	/* tracks/cylinder */
	for (;;) {
		ui = getnumber("tracks/cylinder",
		    "The number of tracks per cylinder.", lp->d_ntracks,
		    UINT32_MAX);
		if (ui == CMD_ABORTED) {
			*lp = oldlabel;		/* undo damage */
			return;
		} else if (ui == CMD_BADVALUE)
			;	/* Try again. */
		else
			break;
	}
	lp->d_ntracks = ui;

	/* sectors/cylinder */
	for (;;) {
		ui = getnumber("sectors/cylinder",
		    "The number of sectors per cylinder (Usually sectors/track "
		    "* tracks/cylinder).", lp->d_secpercyl, UINT32_MAX);
		if (ui == CMD_ABORTED) {
			*lp = oldlabel;		/* undo damage */
			return;
		} else if (ui == CMD_BADVALUE)
			;	/* Try again. */
		else
			break;
	}
	lp->d_secpercyl = ui;

	/* number of cylinders */
	for (;;) {
		ui = getnumber("number of cylinders",
		    "The total number of cylinders on the disk.",
		    lp->d_ncylinders, UINT32_MAX);
		if (ui == CMD_ABORTED) {
			*lp = oldlabel;		/* undo damage */
			return;
		} else if (ui == CMD_BADVALUE)
			;	/* Try again. */
		else
			break;
	}
	lp->d_ncylinders = ui;

	/* total sectors */
	for (;;) {
		u_int64_t nsec = MAXIMUM(DL_GETDSIZE(lp),
		    (u_int64_t)lp->d_ncylinders * lp->d_secpercyl);
		ui = getuint64(lp, "total sectors",
		    "The total number of sectors on the disk.",
		    nsec, nsec, NULL);
		if (ui == CMD_ABORTED) {
			*lp = oldlabel;		/* undo damage */
			return;
		} else if (ui == CMD_BADVALUE)
			;	/* Try again. */
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
	if (ending_sector > ui) {
		ending_sector = ui;
		DL_SETBEND(lp, ending_sector);
	}
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
		    DL_GETPSIZE(&lp->d_partitions[i]) != 0)
			spp[npartitions++] = &lp->d_partitions[i];
	}

	/*
	 * Sort the partitions based on starting offset.
	 * This is safe because we guarantee no overlap.
	 */
	if (npartitions > 1)
		if (mergesort((void *)spp, npartitions, sizeof(spp[0]),
		    partition_cmp))
			err(4, "failed to sort partition table");

	return (spp);
}

/*
 * Get a valid disk type if necessary.
 */
void
getdisktype(struct disklabel *lp, char *banner, char *dev)
{
	int i;
	char *s;
	const char *def = "SCSI";
	const struct dtypes {
		const char *dev;
		const char *type;
	} dtypes[] = {
		{ "sd",   "SCSI" },
		{ "wd",   "IDE" },
		{ "fd",   "FLOPPY" },
		{ "xd",   "SMD" },
		{ "xy",   "SMD" },
		{ "hd",   "HP-IB" },
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
			    "ESDI, ST506, or floppy.", def);
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
	for (;;) {
		ui = getuint64(lp, "Starting sector",
		    "The start of the OpenBSD portion of the disk.",
		    starting_sector, DL_GETDSIZE(lp), NULL);
		if (ui == CMD_ABORTED)
			return;
		else if (ui == CMD_BADVALUE)
			;	/* Try again. */
		else if (ui >= DL_GETDSIZE(lp))
			fprintf(stderr, "starting sector must be < %llu\n",
			    DL_GETDSIZE(lp));
		else
			break;
	}
	start_temp = ui;

	/* Size */
	for (;;) {
		ui = getuint64(lp, "Size ('*' for entire disk)",
		    "The size of the OpenBSD portion of the disk ('*' for the "
		    "entire disk).", ending_sector - starting_sector,
		    DL_GETDSIZE(lp) - start_temp, NULL);
		if (ui == CMD_ABORTED)
			return;
		else if (ui == CMD_BADVALUE)
			;	/* Try again. */
		else if (ui > DL_GETDSIZE(lp) - start_temp)
			fprintf(stderr, "size must be <= %llu\n",
			    DL_GETDSIZE(lp) - start_temp);
		else
			break;
	}
	ending_sector = start_temp + ui;
	DL_SETBEND(lp, ending_sector);
	starting_sector = start_temp;
	DL_SETBSTART(lp, starting_sector);
}

/*
 * Allow user to interactively change disklabel UID.
 */
void
set_duid(struct disklabel *lp)
{
	char *s;
	int i;

	printf("The disklabel UID is currently: "
	    "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n",
	    lp->d_uid[0], lp->d_uid[1], lp->d_uid[2], lp->d_uid[3],
	    lp->d_uid[4], lp->d_uid[5], lp->d_uid[6], lp->d_uid[7]);

	do {
		s = getstring("duid", "The disklabel UID, given as a 16 "
		    "character hexadecimal string.", NULL);
		if (s == NULL || strlen(s) == 0) {
			fputs("Command aborted\n", stderr);
			return;
		}
		i = duid_parse(lp, s);
		if (i != 0)
			fputs("Invalid UID entered.\n", stderr);
	} while (i != 0);
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
		return (chunks);
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
	return (chunks);
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
" ? | h    - show help                 n [part] - set mount point\n"
" A        - auto partition all space  p [unit] - print partitions\n"
" a [part] - add partition             q        - quit & save changes\n"
" b        - set OpenBSD boundaries    R [part] - resize auto allocated partition\n"
" c [part] - change partition size     r        - display free space\n"
" D        - reset label to default    s [path] - save label to file\n"
" d [part] - delete partition          U        - undo all changes\n"
" e        - edit drive parameters     u        - undo last change\n"
" g [d|u]  - [d]isk or [u]ser geometry w        - write label to disk\n"
" i        - modify disklabel UID      X        - toggle expert mode\n"
" l [unit] - print disk label header   x        - exit & lose changes\n"
" M        - disklabel(8) man page     z        - delete all partitions\n"
" m [part] - modify partition\n"
"\n"
"Suffixes can be used to indicate units other than sectors:\n"
" 'b' (bytes), 'k' (kilobytes), 'm' (megabytes), 'g' (gigabytes) 't' (terabytes)\n"
" 'c' (cylinders), '%' (% of total disk), '&' (% of free space).\n"
"Values in non-sector units are truncated to the nearest cylinder boundary.");

}

void
mpcopy(char **to, char **from)
{
	int i;

	for (i = 0; i < MAXPARTITIONS; i++) {
		free(to[i]);
		to[i] = NULL;
		if (from[i] != NULL) {
			to[i] = strdup(from[i]);
			if (to[i] == NULL)
				errx(4, "out of memory");
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
			return (0);
	}
	return (1);
}

void
mpsave(struct disklabel *lp)
{
	int i, j;
	char bdev[PATH_MAX], *p;
	struct mountinfo mi[MAXPARTITIONS];
	FILE *fp;
	u_int8_t fstype;

	if (!fstabfile)
		return;

	memset(&mi, 0, sizeof(mi));

	for (i = 0; i < MAXPARTITIONS; i++) {
		fstype = lp->d_partitions[i].p_fstype;
		if (mountpoints[i] != NULL || fstype == FS_SWAP) {
			mi[i].mountpoint = mountpoints[i];
			mi[i].partno = i;
		}
	}

	/* Convert specname to bdev */
	if (uidflag) {
		snprintf(bdev, sizeof(bdev),
		    "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx.%c",
		    lab.d_uid[0], lab.d_uid[1], lab.d_uid[2], lab.d_uid[3],
		    lab.d_uid[4], lab.d_uid[5], lab.d_uid[6], lab.d_uid[7],
		    specname[strlen(specname)-1]);
	} else if (strncmp(_PATH_DEV, specname, sizeof(_PATH_DEV) - 1) == 0 &&
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

	if ((fp = fopen(fstabfile, "w"))) {
		for (i = 0; i < MAXPARTITIONS; i++) {
			j =  mi[i].partno;
			fstype = lp->d_partitions[j].p_fstype;
			if (fstype == FS_SWAP) {
				fprintf(fp, "%s%c none swap sw\n", bdev, 'a'+j);
			} else if (mi[i].mountpoint) {
				fprintf(fp, "%s%c %s %s rw 1 %d\n", bdev,
				    'a' + j, mi[i].mountpoint,
				    fstypesnames[fstype], j == 0 ? 1 : 2);
			}
		}
		fclose(fp);
	}
}

void
mpfree(char **mp)
{
	int part;

	if (mp == NULL)
		return;

	for (part = 0; part < MAXPARTITIONS; part++)
		free(mp[part]);

	free(mp);
}

int
get_offset(struct disklabel *lp, int partno)
{
	struct partition opp, *pp = &lp->d_partitions[partno];
	u_int64_t ui, offsetalign;
	int flags;

	flags = DO_CONVERSIONS;
	ui = getuint64(lp, "offset",
	    "Starting sector for this partition.",
	    DL_GETPOFFSET(pp),
	    DL_GETPOFFSET(pp), &flags);

	if (ui == CMD_ABORTED || ui == CMD_BADVALUE)
		return (1);
#ifdef SUN_AAT0
	if (partno == 0 && ui != 0) {
		fprintf(stderr, "This architecture requires that "
		    "partition 'a' start at sector 0.\n");
		return (1);
	}
#endif
	opp = *pp;
	DL_SETPOFFSET(pp, ui);
	offsetalign = 1;
	if ((flags & DO_ROUNDING) != 0 && pp->p_fstype == FS_BSDFFS)
		offsetalign = lp->d_secpercyl;

	if (alignpartition(lp, partno, offsetalign, 1, ROUND_OFFSET_UP) == 1) {
		*pp = opp;
		return (1);
	}

	if (expert == 1 && quiet == 0 && ui != DL_GETPOFFSET(pp))
		printf("offset rounded to sector %llu\n", DL_GETPOFFSET(pp));

	return (0);
}

int
get_size(struct disklabel *lp, int partno)
{
	struct partition opp, *pp = &lp->d_partitions[partno];
	u_int64_t maxsize, ui, sizealign;
	int flags;

	maxsize = max_partition_size(lp, partno);
	flags = DO_CONVERSIONS;
	ui = getuint64(lp, "size", "Size of the partition. "
	    "You may also say +/- amount for a relative change.",
	    DL_GETPSIZE(pp), maxsize, &flags);

	if (ui == CMD_ABORTED || ui == CMD_BADVALUE)
		return (1);

	opp = *pp;
	DL_SETPSIZE(pp, ui);
	sizealign = 1;
	if ((flags & DO_ROUNDING) != 0 && (pp->p_fstype == FS_SWAP ||
	    pp->p_fstype == FS_BSDFFS))
		sizealign = lp->d_secpercyl;

	if (alignpartition(lp, partno, 1, sizealign, ROUND_SIZE_UP) == 1) {
		*pp = opp;
		return (1);
	}

	if (expert == 1 && quiet == 0 && ui != DL_GETPSIZE(pp))
		printf("size rounded to %llu sectors\n", DL_GETPSIZE(pp));

	return (0);
}

int
get_cpg(struct disklabel *lp, int partno)
{
	u_int64_t ui;
	struct partition *pp = &lp->d_partitions[partno];

	if (pp->p_fstype != FS_BSDFFS)
		return (0);

	if (pp->p_cpg == 0)
		pp->p_cpg = 1;

	if (expert == 0)
		return (0);

	for (;;) {
		ui = getnumber("cpg", "Size of partition in fs blocks.",
		    pp->p_cpg, USHRT_MAX);
		if (ui == CMD_ABORTED)
			return (1);
		else if (ui == CMD_BADVALUE)
			;	/* Try again. */
		else
			break;
	}
	pp->p_cpg = ui;
	return (0);
}

int
get_fsize(struct disklabel *lp, int partno)
{
	struct partition *pp = &lp->d_partitions[partno];
	u_int64_t ui, bytes;
	u_int32_t frag, fsize;

	if (pp->p_fstype != FS_BSDFFS)
		return (0);

	fsize = DISKLABELV1_FFS_FSIZE(pp->p_fragblock);
	frag = DISKLABELV1_FFS_FRAG(pp->p_fragblock);
	if (fsize == 0) {
		fsize = 2048;
		frag = 8;
		bytes = DL_GETPSIZE(pp) * lp->d_secsize;
		if (bytes > 128ULL * 1024 * 1024 * 1024)
			fsize *= 2;
		if (bytes > 512ULL * 1024 * 1024 * 1024)
			fsize *= 2;
		if (fsize < lp->d_secsize)
			fsize = lp->d_secsize;
		if (fsize > MAXBSIZE / frag)
			fsize = MAXBSIZE / frag;
		pp->p_fragblock = DISKLABELV1_FFS_FRAGBLOCK(fsize, frag);
	}

	if (expert == 0)
		return (0);

	for (;;) {
		ui = getnumber("fragment size",
		    "Size of ffs block fragments. A multiple of the disk "
		    "sector-size.", fsize, UINT32_MAX);
		if (ui == CMD_ABORTED)
			return (1);
		else if (ui == CMD_BADVALUE)
			;	/* Try again. */
		else if (ui < lp->d_secsize || (ui % lp->d_secsize) != 0)
			fprintf(stderr, "Error: fragment size must be a "
			    "multiple of the disk sector size (%d)\n",
			    lp->d_secsize);
		else
			break;
	}
	if (ui == 0)
		puts("Zero fragment size implies zero block size");
	pp->p_fragblock = DISKLABELV1_FFS_FRAGBLOCK(ui, frag);
	return (0);
}

int
get_bsize(struct disklabel *lp, int partno)
{
	u_int64_t ui, frag, fsize;
	struct partition opp, *pp = &lp->d_partitions[partno];
	u_int64_t offsetalign, sizealign;
	char *p;

	if (pp->p_fstype != FS_BSDFFS)
		return (0);

	/* Avoid dividing by zero... */
	if (pp->p_fragblock == 0)
		return (1);

	opp = *pp;
	if (expert == 0)
		goto align;

	fsize = DISKLABELV1_FFS_FSIZE(pp->p_fragblock);
	frag = DISKLABELV1_FFS_FRAG(pp->p_fragblock);

	for (;;) {
		ui = getnumber("block size",
		    "Size of ffs blocks. 1, 2, 4 or 8 times ffs fragment size.",
		    fsize * frag, UINT32_MAX);

		/* sanity checks */
		if (ui == CMD_ABORTED)
			return (1);
		else if (ui == CMD_BADVALUE)
			;	/* Try again. */
		else if (ui < getpagesize())
			fprintf(stderr,
			    "Error: block size must be at least as big "
			    "as page size (%d).\n", getpagesize());
		else if (ui < fsize || (fsize != ui && fsize * 2 != ui &&
		    fsize * 4 != ui && fsize * 8 != ui))
			fprintf(stderr, "Error: block size must be 1, 2, 4 or "
			    "8 times fragment size (%llu).\n",
			    (unsigned long long) fsize);
		else
			break;
	}
	frag = ui / fsize;
	pp->p_fragblock = DISKLABELV1_FFS_FRAGBLOCK(fsize, frag);

#ifdef SUN_CYLCHECK
	return (0);
#endif
	p = getstring("Align partition to block size",
	    "Round the partition offset and size to multiples of bsize?", "y");
	if (*p == 'n' || *p == 'N')
		return (0);

align:
#ifdef SUN_CYLCHECK
	return (0);
#endif
	sizealign = (DISKLABELV1_FFS_FRAG(pp->p_fragblock) *
	    DISKLABELV1_FFS_FSIZE(pp->p_fragblock)) / lp->d_secsize;
	offsetalign = 1;
	if (DL_GETPOFFSET(pp) != starting_sector)
		offsetalign = sizealign;

	if (alignpartition(lp, partno, offsetalign, sizealign, ROUND_OFFSET_UP |
	    ROUND_SIZE_DOWN | ROUND_SIZE_OVERLAP) == 1) {
		*pp = opp;
		return (1);
	}

	if (expert == 1 && quiet == 0 &&
	    DL_GETPOFFSET(&opp) != DL_GETPOFFSET(pp))
		printf("offset rounded to sector %llu\n", DL_GETPOFFSET(pp));
	if (expert == 1 && quiet == 0 && DL_GETPSIZE(&opp) != DL_GETPSIZE(pp))
		printf("size rounded to %llu sectors\n", DL_GETPSIZE(pp));

	return (0);
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
			return (1);
		}
		for (ui = 0; ui < FSMAXTYPES; ui++) {
			if (!strcasecmp(p, fstypenames[ui])) {
				pp->p_fstype = ui;
				break;
			}
		}
		if (ui >= FSMAXTYPES) {
			printf("Unrecognized filesystem type '%s', treating "
			    "as 'unknown'\n", p);
			pp->p_fstype = FS_OTHER;
		}
	} else {
		for (;;) {
			ui = getnumber("FS type (decimal)",
			    "Filesystem type as a decimal number; usually 7 "
			    "(4.2BSD) or 1 (swap).",
			    pp->p_fstype, UINT8_MAX);
			if (ui == CMD_ABORTED)
				return (1);
			else if (ui == CMD_BADVALUE)
				;	/* Try again. */
			else
				break;
		}
		pp->p_fstype = ui;
	}
	return (0);
}

int
get_mp(struct disklabel *lp, int partno)
{
	struct partition *pp = &lp->d_partitions[partno];
	char *p;
	int i;

	if (fstabfile == NULL ||
	    pp->p_fstype == FS_UNUSED ||
	    pp->p_fstype == FS_SWAP ||
	    pp->p_fstype == FS_BOOT ||
	    pp->p_fstype == FS_OTHER ||
	    pp->p_fstype == FS_RAID) {
		/* No fstabfile, no names. Not all fstypes can be named */
		return 0;
	}

	for (;;) {
		p = getstring("mount point",
		    "Where to mount this filesystem (ie: / /var /usr)",
		    mountpoints[partno] ? mountpoints[partno] : "none");
		if (p == NULL)
			return (1);
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

	return (0);
}

int
micmp(const void *a1, const void *a2)
{
	struct mountinfo *mi1 = (struct mountinfo *)a1;
	struct mountinfo *mi2 = (struct mountinfo *)a2;

	/* We want all the NULLs at the end... */
	if (mi1->mountpoint == NULL && mi2->mountpoint == NULL)
		return (0);
	else if (mi1->mountpoint == NULL)
		return (1);
	else if (mi2->mountpoint == NULL)
		return (-1);
	else
		return (strcmp(mi1->mountpoint, mi2->mountpoint));
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
	if (ioctl(f, DIOCGPDINFO, disk_geop) < 0)
		err(4, "ioctl DIOCGPDINFO");
	*dgpp = disk_geop;
}

void
set_geometry(struct disklabel *lp, struct disklabel *dgp,
    struct disklabel *ugp, char *p)
{
	if (p == NULL)
		p = getstring("[d]isk or [u]ser geometry",
		    "Enter 'd' to use the geometry based on what the disk "
		    "itself thinks it is, or 'u' to use the geometry that "
		    "was found in the label.",
		    "d");
	if (p == NULL)
		return;
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
psize(u_int64_t sz, char unit, struct disklabel *lp)
{
	double d = scale(sz, unit, lp);
	if (d < 0)
		printf("%llu", sz);
	else
		printf("%.*f%c", unit == 'B' ? 0 : 1, d, unit);
}

void
display_edit(struct disklabel *lp, char unit)
{
	u_int64_t fr;
	int i;

	fr = editor_countfree(lp);
	unit = canonical_unit(lp, unit);

	printf("OpenBSD area: ");
	psize(starting_sector, 0, lp);
	printf("-");
	psize(ending_sector, 0, lp);
	printf("; size: ");
	psize(ending_sector - starting_sector, unit, lp);
	printf("; free: ");
	psize(fr, unit, lp);

	printf("\n#    %16.16s %16.16s  fstype [fsize bsize   cpg]\n",
	    "size", "offset");
	for (i = 0; i < lp->d_npartitions; i++)
		display_partition(stdout, lp, i, unit);
}

void
parse_autotable(char *filename)
{
	FILE	*cfile;
	size_t	 len;
	char	*buf, *t;
	uint	 idx = 0, pctsum = 0;
	struct space_allocation *sa;

	if ((cfile = fopen(filename, "r")) == NULL)
		err(1, "%s", filename);
	if ((alloc_table = calloc(1, sizeof(struct alloc_table))) == NULL)
		err(1, NULL);
	alloc_table_nitems = 1;

	while ((buf = fgetln(cfile, &len)) != NULL) {
		if ((alloc_table[0].table = reallocarray(alloc_table[0].table,
		    idx + 1, sizeof(*sa))) == NULL)
			err(1, NULL);
		sa = &(alloc_table[0].table[idx]);
		memset(sa, 0, sizeof(*sa));
		idx++;

		if ((sa->mp = get_token(&buf, &len)) == NULL ||
		    (sa->mp[0] != '/' && strcmp(sa->mp, "swap")))
			errx(1, "%s: parse error on line %u", filename, idx);
		if ((t = get_token(&buf, &len)) == NULL ||
		    parse_sizerange(t, &sa->minsz, &sa->maxsz) == -1)
			errx(1, "%s: parse error on line %u", filename, idx);
		if ((t = get_token(&buf, &len)) != NULL &&
		    parse_pct(t, &sa->rate) == -1)
			errx(1, "%s: parse error on line %u", filename, idx);
		if (sa->minsz > sa->maxsz)
			errx(1, "%s: min size > max size on line %u", filename,
			    idx);
		pctsum += sa->rate;
	}
	if (pctsum > 100)
		errx(1, "%s: sum of extra space allocation > 100%%", filename);
	alloc_table[0].sz = idx;
	fclose(cfile);
}

char *
get_token(char **s, size_t *len)
{
	char	*p, *r;
	size_t	 tlen = 0;

	p = *s;
	while (*len > 0 && !isspace((u_char)*s[0])) {
		(*s)++;
		(*len)--;
		tlen++;
	}
	if (tlen == 0)
		return (NULL);

	/* eat whitespace */
	while (*len > 0 && isspace((u_char)*s[0])) {
		(*s)++;
		(*len)--;
	}

	if ((r = strndup(p, tlen)) == NULL)
		err(1, NULL);
	return (r);
}

int
apply_unit(double val, u_char unit, u_int64_t *n)
{
	u_int64_t factor = 1;

	switch (tolower(unit)) {
	case 't':
		factor *= 1024;
		/* FALLTHROUGH */
	case 'g':
		factor *= 1024;
		/* FALLTHROUGH */
	case 'm':
		factor *= 1024;
		/* FALLTHROUGH */
	case 'k':
		factor *= 1024;
		break;
	default:
		return (-1);
	}

	val *= factor / DEV_BSIZE;
	if (val > ULLONG_MAX)
		return (-1);
	*n = val;
	return (0);
}

int
parse_sizespec(const char *buf, double *val, char **unit)
{
	errno = 0;
	*val = strtod(buf, unit);
	if (errno == ERANGE || *val < 0 || *val > ULLONG_MAX)
		return (-1);	/* too big/small */
	if (*val == 0 && *unit == buf)
		return (-1);	/* No conversion performed. */
	if (*unit != NULL && *unit[0] == '\0')
		*unit = NULL;
	return (0);
}

int
parse_sizerange(char *buf, u_int64_t *min, u_int64_t *max)
{
	char	*p, *unit1 = NULL, *unit2 = NULL;
	double	 val1 = 0, val2 = 0;

	if ((p = strchr(buf, '-')) != NULL) {
		p[0] = '\0';
		p++;
	}
	*max = 0;
	if (parse_sizespec(buf, &val1, &unit1) == -1)
		return (-1);
	if (p != NULL && p[0] != '\0') {
		if (p[0] == '*')
			*max = -1;
		else
			if (parse_sizespec(p, &val2, &unit2) == -1)
				return (-1);
	}
	if (unit1 == NULL && (unit1 = unit2) == NULL)
		return (-1);
	if (apply_unit(val1, unit1[0], min) == -1)
		return (-1);
	if (val2 > 0) {
		if (apply_unit(val2, unit2[0], max) == -1)
			return (-1);
	} else
		if (*max == 0)
			*max = *min;
	free(buf);
	return (0);
}

int
parse_pct(char *buf, int *n)
{
	const char	*errstr;

	if (buf[strlen(buf) - 1] == '%')
		buf[strlen(buf) - 1] = '\0';
	*n = strtonum(buf, 0, 100, &errstr);
	if (errstr) {
		warnx("parse percent %s: %s", buf, errstr);
		return (-1);
	}
	free(buf);
	return (0);
}

int
alignpartition(struct disklabel *lp, int partno, u_int64_t startalign,
    u_int64_t stopalign, int flags)
{
	struct partition *pp = &lp->d_partitions[partno];
	struct diskchunk *chunks;
	u_int64_t start, stop, maxstop;
	unsigned int i;
	u_int8_t fstype;

	start = DL_GETPOFFSET(pp);
	if ((flags & ROUND_OFFSET_UP) == ROUND_OFFSET_UP)
		start = ((start + startalign - 1) / startalign) * startalign;
	else if ((flags & ROUND_OFFSET_DOWN) == ROUND_OFFSET_DOWN)
		start = (start / startalign) * startalign;

	/* Find the chunk that contains 'start'. */
	fstype = pp->p_fstype;
	pp->p_fstype = FS_UNUSED;
	chunks = free_chunks(lp);
	pp->p_fstype = fstype;
	for (i = 0; chunks[i].start != 0 || chunks[i].stop != 0; i++) {
		if (start >= chunks[i].start && start < chunks[i].stop)
			break;
	}
	if (chunks[i].stop == 0) {
		fprintf(stderr, "'%c' aligned offset %llu lies outside "
		    "the OpenBSD bounds or inside another partition\n",
		    'a' + partno, start);
		return (1);
	}

	/* Calculate the new 'stop' sector, the sector after the partition. */
	if ((flags & ROUND_SIZE_OVERLAP) == 0)
		maxstop = (chunks[i].stop / stopalign) * stopalign;
	else
		maxstop = (ending_sector / stopalign) * stopalign;

	stop = DL_GETPOFFSET(pp) + DL_GETPSIZE(pp);
	if ((flags & ROUND_SIZE_UP) == ROUND_SIZE_UP)
		stop = ((stop + stopalign - 1) / stopalign) * stopalign;
	else if ((flags & ROUND_SIZE_DOWN) == ROUND_SIZE_DOWN)
		stop = (stop / stopalign) * stopalign;
	if (stop > maxstop)
		stop = maxstop;

	if (stop <= start) {
		fprintf(stderr, "'%c' aligned size <= 0\n", 'a' + partno);
		return (1);
	}

	if (start != DL_GETPOFFSET(pp))
		DL_SETPOFFSET(pp, start);
	if (stop != DL_GETPOFFSET(pp) + DL_GETPSIZE(pp))
		DL_SETPSIZE(pp, stop - start);

	return (0);
}
