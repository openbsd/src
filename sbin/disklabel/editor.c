/*	$OpenBSD: editor.c,v 1.50 1999/03/16 04:47:16 millert Exp $	*/

/*
 * Copyright (c) 1997-1999 Todd C. Miller <Todd.Miller@courtesan.com>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: editor.c,v 1.50 1999/03/16 04:47:16 millert Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/param.h>
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

#include "pathnames.h"

/* flags for getuint() */
#define	DO_CONVERSIONS	0x00000001
#define	DO_ROUNDING	0x00000002

#ifndef NUMBOOT
#define NUMBOOT 0
#endif

/* structure to describe a portion of a disk */
struct diskchunk {
	u_int32_t start;
	u_int32_t stop;
};

void	edit_parms __P((struct disklabel *, u_int32_t *));
int	editor __P((struct disklabel *, int, char *, char *));
void	editor_add __P((struct disklabel *, char **, u_int32_t *, char *));
void	editor_change __P((struct disklabel *, u_int32_t *, char *));
void	editor_countfree __P((struct disklabel *, u_int32_t *));
void	editor_delete __P((struct disklabel *, char **, u_int32_t *, char *));
void	editor_display __P((struct disklabel *, char **, u_int32_t *, char));
void	editor_help __P((char *));
void	editor_modify __P((struct disklabel *, char **, u_int32_t *, char *));
void	editor_name __P((struct disklabel *, char **, char *));
char	*getstring __P((struct disklabel *, char *, char *, char *));
u_int32_t getuint __P((struct disklabel *, int, char *, char *, u_int32_t, u_int32_t, u_int32_t, int));
int	has_overlap __P((struct disklabel *, u_int32_t *, int));
void	make_contiguous __P((struct disklabel *));
u_int32_t next_offset __P((struct disklabel *, struct partition *));
int	partition_cmp __P((const void *, const void *));
struct partition **sort_partitions __P((struct disklabel *, u_int16_t *));
void	getdisktype __P((struct disklabel *, char *, char *));
void	find_bounds __P((struct disklabel *));
void	set_bounds __P((struct disklabel *, u_int32_t *));
struct diskchunk *free_chunks __P((struct disklabel *));
char **	mpcopy __P((char **, char **));
int	mpequal __P((char **, char **));
int	mpsave __P((struct disklabel *, char **, char *, char *));

static u_int32_t starting_sector;
static u_int32_t ending_sector;

/* from disklabel.c */
int	checklabel __P((struct disklabel *));
void	display __P((FILE *, struct disklabel *));
void	display_partition __P((FILE *, struct disklabel *, char **, int, char, int));
int	width_partition __P((struct disklabel *, int));

struct disklabel *readlabel __P((int));
struct disklabel *makebootarea __P((char *, struct disklabel *, int));
int	writelabel __P((int, char *, struct disklabel *));
extern	char *bootarea, *specname;
extern	int donothing;
#ifdef DOSLABEL
struct dos_partition *dosdp;	/* DOS partition, if found */
#endif

/*
 * Simple partition editor.  Primarily intended for new labels.
 */
int
editor(lp, f, dev, fstabfile)
	struct disklabel *lp;
	int f;
	char *dev;
	char *fstabfile;
{
	struct disklabel lastlabel, tmplabel, label = *lp;
	struct partition *pp;
	u_int32_t freesectors;
	FILE *fp;
	char buf[BUFSIZ], *cmd, *arg;
	char **mountpoints = NULL, **omountpoints, **tmpmountpoints;

	/* Alloc and init mount point info */
	if (fstabfile) {
		if (!(mountpoints = calloc(MAXPARTITIONS, sizeof(char *))) ||
		    !(omountpoints = calloc(MAXPARTITIONS, sizeof(char *))) ||
		    !(tmpmountpoints = calloc(MAXPARTITIONS, sizeof(char *))))
			errx(4, "out of memory");
	}

	/* Don't allow disk type of "unknown" */
	getdisktype(&label, "You need to specify a type for this disk.", dev);

	/* How big is the OpenBSD portion of the disk?  */
	find_bounds(&label);

	/* Set freesectors based on bounds and initial label */
	editor_countfree(&label, &freesectors);

	/* Make sure there is no partition overlap. */
	if (has_overlap(&label, &freesectors, 1))
		errx(1, "can't run when there is partition overlap.");

	/* If we don't have a 'c' partition, create one. */
	pp = &label.d_partitions[2];
	if (label.d_npartitions < 3 || pp->p_size == 0) {
		puts("No 'c' partition found, adding one that spans the disk.");
		if (label.d_npartitions < 3)
			label.d_npartitions = 3;
		pp->p_offset = 0;
		pp->p_size = label.d_secperunit;
		pp->p_fstype = FS_UNUSED;
		pp->p_fsize = pp->p_frag = pp->p_cpg = 0;
	}

#ifdef CYLCHECK
	puts("This platform requires that partition offsets/sizes be on cylinder boundaries.\nPartition offsets/sizes will be rounded to the nearest cylinder automatically.");
#endif

	/* Set d_bbsize and d_sbsize as necessary */
	if (strcmp(label.d_packname, "fictitious") == 0) {
		if (label.d_bbsize == 0)
			label.d_bbsize = BBSIZE;
		if (label.d_sbsize == 0)
			label.d_sbsize = SBSIZE;
	}

	/* Interleave must be >= 1 */
	if (label.d_interleave == 0)
		label.d_interleave = 1;

	puts("\nInitial label editor (enter '?' for help at any prompt)");
	lastlabel = label;
	for (;;) {
		fputs("> ", stdout);
		fflush(stdout);
		rewind(stdin);
		if (fgets(buf, sizeof(buf), stdin) == NULL) {
			putchar('\n');
			buf[0] = 'q';
			buf[1] = '\0';
		}
		if ((cmd = strtok(buf, " \t\r\n")) == NULL)
			continue;
		arg = strtok(NULL, " \t\r\n");

		switch (*cmd) {

		case '?':
		case 'h':
			editor_help(arg ? arg : "");
			break;

		case 'a':
			tmplabel = lastlabel;
			lastlabel = label;
			if (mountpoints != NULL) {
				mpcopy(tmpmountpoints, omountpoints);
				mpcopy(omountpoints, mountpoints);
			}
			editor_add(&label, mountpoints, &freesectors, arg);
			if (memcmp(&label, &lastlabel, sizeof(label)) == 0)
				lastlabel = tmplabel;
			if (mountpoints != NULL && mpequal(omountpoints, tmpmountpoints))
				mpcopy(omountpoints, tmpmountpoints);
			break;

		case 'b':
			tmplabel = lastlabel;
			lastlabel = label;
			set_bounds(&label, &freesectors);
			if (memcmp(&label, &lastlabel, sizeof(label)) == 0)
				lastlabel = tmplabel;
			break;

		case 'c':
			tmplabel = lastlabel;
			lastlabel = label;
			editor_change(&label, &freesectors, arg);
			if (memcmp(&label, &lastlabel, sizeof(label)) == 0)
				lastlabel = tmplabel;
			break;

		case 'd':
			tmplabel = lastlabel;
			lastlabel = label;
			if (mountpoints != NULL) {
				mpcopy(tmpmountpoints, omountpoints);
				mpcopy(omountpoints, mountpoints);
			}
			editor_delete(&label, mountpoints, &freesectors, arg);
			if (memcmp(&label, &lastlabel, sizeof(label)) == 0)
				lastlabel = tmplabel;
			if (mountpoints != NULL && mpequal(omountpoints, tmpmountpoints))
				mpcopy(omountpoints, tmpmountpoints);
			break;

		case 'm':
			tmplabel = lastlabel;
			lastlabel = label;
			if (mountpoints != NULL) {
				mpcopy(tmpmountpoints, omountpoints);
				mpcopy(omountpoints, mountpoints);
			}
			editor_modify(&label, mountpoints, &freesectors, arg);
			if (memcmp(&label, &lastlabel, sizeof(label)) == 0)
				lastlabel = tmplabel;
			if (mountpoints != NULL && mpequal(omountpoints, tmpmountpoints))
				mpcopy(omountpoints, tmpmountpoints);
			break;

		case 'n':
			if (mountpoints == NULL) {
				fputs("This option is not valid when run "
				    "without the -F flag.\n", stderr);
				break;
			}
			mpcopy(tmpmountpoints, omountpoints);
			mpcopy(omountpoints, mountpoints);
			editor_name(&label, mountpoints, arg);
			if (mpequal(omountpoints, tmpmountpoints))
				mpcopy(omountpoints, tmpmountpoints);
			break;

		case 'p':
			editor_display(&label, mountpoints, &freesectors,
			    arg ? *arg : 0);
			break;

		case 'M': {
			sig_t opipe = signal(SIGPIPE, SIG_IGN);
			char *pager;
			extern char manpage[];

			if ((pager = getenv("PAGER")) == NULL)
				pager = _PATH_LESS;
			if ((fp = popen(pager, "w")) != NULL) {
				(void) fwrite(manpage, strlen(manpage), 1, fp);
				pclose(fp);
			} else
				warn("unable to execute %s", pager);

			(void)signal(SIGPIPE, opipe);
			break;
		}

		case 'q':
			if (donothing) {
				puts("In no change mode, not writing label.");
				return(1);
			}
			/* Save mountpoint info if there is any. */
			if (mountpoints != NULL)
				mpsave(&label, mountpoints, dev, fstabfile);
			if (memcmp(lp, &label, sizeof(label)) == 0) {
				puts("No label changes.");
				return(1);
			}
			do {
				arg = getstring(&label, "Save changes?",
				    "Save changes you have made to the label?",
				    "n");
			} while (arg && tolower(*arg) != 'y' && tolower(*arg) != 'n');
			if (arg && tolower(*arg) == 'y') {
				*lp = label;
				if (writelabel(f, bootarea, lp) == 0)
					return(0);
			}
			return(1);
			/* NOTREACHED */
			break;

		case 'r':
		    /* Recalculate free space */
		    editor_countfree(&label, &freesectors);
		    puts("Recalculated free space.");
		    break;

		case 's':
			if (arg == NULL) {
				arg = getstring(lp, "Filename",
				    "Name of the file to save label into.",
				    NULL);
				if (arg == NULL && *arg == '\0')
					break;
			}
			if ((fp = fopen(arg, "w")) == NULL) {
				warn("cannot open %s", arg);
			} else {
				display(fp, &label);
				(void)fclose(fp);
			}
			break;

		case 'u':
			if (memcmp(&label, &lastlabel, sizeof(label)) == 0 &&
			    mountpoints != NULL &&
			    mpequal(mountpoints, omountpoints)) {
				puts("Nothing to undo!");
			} else {
				tmplabel = label;
				label = lastlabel;
				lastlabel = tmplabel;
				/* Recalculate free space */
				editor_countfree(&label, &freesectors);
				/* Restore mountpoints */
				if (mountpoints != NULL)
					mpcopy(mountpoints, omountpoints);
				puts("Last change undone.");
			}
			break;

		case 'w':
			if (donothing)  {
				puts("In no change mode, not writing label.");
				break;
			}
			/* Save mountpoint info if there is any. */
			if (mountpoints != NULL)
				mpsave(&label, mountpoints, dev, fstabfile);
			/* Save label if it has changed. */
			if (memcmp(lp, &label, sizeof(label)) == 0)
				puts("No label changes.");
			else if (writelabel(f, bootarea, &label) != 0)
				warnx("unable to write label");
			else
				*lp = label;
			break;

		case 'x':
			return(1);
			break;

		case '\n':
			break;

		case 'e':
			tmplabel = lastlabel;
			lastlabel = label;
			edit_parms(&label, &freesectors);
			if (memcmp(&label, &lastlabel, sizeof(label)) == 0)
				lastlabel = tmplabel;
			break;

		default:
			printf("Unknown option: %c ('?' for help)\n", *cmd);
			break;
		}
	}
}

/*
 * Add a new partition.
 */
void
editor_add(lp, mp, freep, p)
	struct disklabel *lp;
	char **mp;
	u_int32_t *freep;
	char *p;
{
	struct partition *pp;
	struct diskchunk *chunks;
	char buf[BUFSIZ];
	int i, partno;
	u_int32_t ui, old_offset, old_size;

	/* XXX - prompt user to steal space from another partition instead */
	if (*freep == 0) {
		fputs("No space left, you need to shrink a partition\n",
		    stderr);
		return;
	}

	/* XXX - make more like other editor_* */
	if (p != NULL) {
		partno = p[0] - 'a';
		if (partno < 0 || partno == 2 || partno >= MAXPARTITIONS) {
			fprintf(stderr,
			    "Partition must be between 'a' and '%c' "
			    "(excluding 'c').\n", 'a' + MAXPARTITIONS - 1);
			return;
		} else if (lp->d_partitions[partno].p_fstype != FS_UNUSED &&
		    lp->d_partitions[partno].p_size != 0) {
			fprintf(stderr,
			    "Partition '%c' exists.  Delete it first.\n",
			    p[0]);
			return;
		}
	} else {
		/* Find first unused partition that is not 'c' */
		for (partno = 0; partno < MAXPARTITIONS; partno++, p++) {
			if (lp->d_partitions[partno].p_size == 0 && partno != 2)
				break;
		}
		if (partno < MAXPARTITIONS) {
			buf[0] = partno + 'a';
			buf[1] = '\0';
			p = &buf[0];
		} else
			p = NULL;
		for (;;) {
			p = getstring(lp, "partition",
			    "The letter of the new partition, a - p.", p);
			if (p == NULL)
				return;
			partno = p[0] - 'a';
			if (lp->d_partitions[partno].p_fstype != FS_UNUSED &&
			    lp->d_partitions[partno].p_size != 0) {
				fprintf(stderr,
				    "Partition '%c' already exists.\n", p[0]);
			} else if (partno >= 0 && partno < MAXPARTITIONS)
				break;
			fprintf(stderr,
			    "Partition must be between 'a' and '%c'.\n",
			    'a' + MAXPARTITIONS - 1);
		}
	}

	/* Increase d_npartitions if necesary */
	if (partno >= lp->d_npartitions)
		lp->d_npartitions = partno + 1;

	/* Set defaults */
	pp = &lp->d_partitions[partno];
	if (partno >= lp->d_npartitions)
		lp->d_npartitions = partno + 1;
	memset(pp, 0, sizeof(*pp));
	pp->p_fstype = partno == 1 ? FS_SWAP : FS_BSDFFS;
	pp->p_fsize = 1024;
	pp->p_frag = 8;
	pp->p_cpg = 16;
	pp->p_size = *freep;
	pp->p_offset = next_offset(lp, pp);	/* must be computed last */
#if NUMBOOT == 1
	/* Don't clobber boot blocks */
	if (pp->p_offset == 0) {
		pp->p_offset = lp->d_secpercyl;
		pp->p_size -= lp->d_secpercyl;
	}
#endif
	old_offset = pp->p_offset;
	old_size = pp->p_size;

getoff1:
	/* Get offset */
	for (;;) {
		ui = getuint(lp, partno, "offset",
		   "Starting sector for this partition.", pp->p_offset,
		   pp->p_offset, 0, DO_CONVERSIONS |
		   (pp->p_fstype == FS_BSDFFS ? DO_ROUNDING : 0));
		if (ui == UINT_MAX - 1) {
			fputs("Command aborted\n", stderr);
			pp->p_size = 0;		/* effective delete */
			return;
		} else if (ui == UINT_MAX)
			fputs("Invalid entry\n", stderr);
		else if (ui < starting_sector)
			fprintf(stderr, "The OpenBSD portion of the disk starts"
			    " at sector %u, you tried to add a partition at %u."
			    "  You can use the 'b' command to change the size "
			    "of the OpenBSD portion.\n" , starting_sector, ui);
		else if (ui >= ending_sector)
			fprintf(stderr, "The OpenBSD portion of the disk ends "
			    "at sector %u, you tried to add a partition at %u."
			    "  You can use the 'b' command to change the size "
			    "of the OpenBSD portion.\n", ending_sector, ui);
		else
			break;
	}
	pp->p_offset = ui;

	/* Recompute recommended size based on new offset */
	ui = pp->p_fstype;
	pp->p_fstype = FS_UNUSED;
	chunks = free_chunks(lp);
	for (i = 0; chunks[i].start != 0 || chunks[i].stop != 0; i++) {
		if (pp->p_offset >= chunks[i].start &&
		    pp->p_offset < chunks[i].stop) {
			pp->p_size = chunks[i].stop - pp->p_offset;
			break;
		}
	}
	pp->p_fstype = ui;
	
	/* Get size */
	for (;;) {
		ui = getuint(lp, partno, "size", "Size of the partition.",
		    pp->p_size, *freep, pp->p_offset, DO_CONVERSIONS |
		    ((pp->p_fstype == FS_BSDFFS || pp->p_fstype == FS_SWAP) ?
		    DO_ROUNDING : 0));
		if (ui == UINT_MAX - 1) {
			fputs("Command aborted\n", stderr);
			pp->p_size = 0;		/* effective delete */
			return;
		} else if (ui == UINT_MAX)
			fputs("Invalid entry\n", stderr);
		else if (ui > *freep)
			/* XXX - prompt user to steal space from another partition */
			fprintf(stderr,"Sorry, there are only %u sectors left\n",
			    *freep);
		else if (pp->p_offset + ui > ending_sector)
			fprintf(stderr, "The OpenBSD portion of the disk ends "
			    "at sector %u, you tried to add a partition ending "
			    "at sector %u.  You can use the 'b' command to "
			    "change the size of the OpenBSD portion.\n",
			    ending_sector, pp->p_offset + ui);
		else
			break;
	}
	pp->p_size = ui;
	if (pp->p_size == 0)
		return;

	/* Check for overlap */
	if (has_overlap(lp, freep, 0)) {
		printf("\nPlease re-enter an offset and size for partition "
		    "%c.\n", 'a' + partno);
		pp->p_offset = old_offset;
		pp->p_size = old_size;
		goto getoff1;		/* Yeah, I know... */
	}

	/* Get fstype */
	if (pp->p_fstype < FSMAXTYPES) {
		p = getstring(lp, "FS type",
		    "Filesystem type (usually 4.2BSD or swap)",
		    fstypenames[pp->p_fstype]);
		if (p == NULL) {
			fputs("Command aborted\n", stderr);
			pp->p_size = 0;		/* effective delete */
			return;
		}
		for (i = 0; i < FSMAXTYPES; i++) {
			if (!strcasecmp(p, fstypenames[i])) {
				pp->p_fstype = i;
				break;
			}
		}
		if (i >= FSMAXTYPES) {
			printf("Unrecognized filesystem type '%s', treating as 'unknown'\n", p);
			pp->p_fstype = FS_OTHER;
		}
	} else {
		for (;;) {
			ui = getuint(lp, partno, "FS type (decimal)",
			    "Filesystem type as a decimal number; usually 7 (4.2BSD) or 1 (swap).",
			    pp->p_fstype, pp->p_fstype, 0, 0);
			if (ui == UINT_MAX - 1) {
				fputs("Command aborted\n", stderr);
				pp->p_size = 0;		/* effective delete */
				return;
			} if (ui == UINT_MAX)
				fputs("Invalid entry\n", stderr);
			else
				break;
		}
		pp->p_fstype = ui;
	}

	if (pp->p_fstype == FS_BSDFFS || pp->p_fstype == FS_UNUSED) {
		/* get fsize */
		for (;;) {
			ui = getuint(lp, partno, "fragment size",
			    "Size of fs block fragments.  Usually 1024 or 512.",
			    pp->p_fsize, pp->p_fsize, 0, 0);
			if (ui == UINT_MAX - 1) {
				fputs("Command aborted\n", stderr);
				pp->p_size = 0;		/* effective delete */
				return;
			} else if (ui == UINT_MAX)
				fputs("Invalid entry\n", stderr);
			else
				break;
		}
		pp->p_fsize = ui;
		if (pp->p_fsize == 0)
			puts("Zero fragment size implies zero block size");

		/* get bsize */
		/* XXX - do before frag size? */
		for (; pp->p_fsize > 0;) {
			ui = getuint(lp, partno, "block size",
			    "Size of filesystem blocks.  Usually 8192 or 4096.",
			    pp->p_fsize * pp->p_frag, pp->p_fsize * pp->p_frag,
			    0, 0);

			/* sanity checks */
			if (ui == UINT_MAX - 1) {
				fputs("Command aborted\n", stderr);
				pp->p_size = 0;		/* effective delete */
				return;
			} else if (ui == UINT_MAX)
				fputs("Invalid entry\n", stderr);
			else if (ui < getpagesize())
				fprintf(stderr,
				    "Error: block size must be at least as big "
				    "as page size (%d).\n", getpagesize());
			else if (ui % pp->p_fsize != 0)
				fputs("Error: block size must be a multiple of the fragment size.\n", stderr);
			else if (ui / pp->p_fsize < 1)
				fputs("Error: block size must be at least as big as fragment size.\n", stderr);
			else
				break;
		}
		pp->p_frag = ui / pp->p_fsize;

		if (pp->p_fstype == FS_BSDFFS) {
			/* get cpg */
			for (;;) {
				ui = getuint(lp, partno, "cpg",
				    "Number of filesystem cylinders per group.  Usually 16 or 8.",
				    pp->p_cpg, pp->p_cpg, 0, 0);
				if (ui == UINT_MAX - 1) {
					fputs("Command aborted\n", stderr);
					pp->p_size = 0;	/* effective delete */
					return;
				} else if (ui == UINT_MAX)
					fputs("Invalid entry\n", stderr);
				else
					break;
			}
			pp->p_cpg = ui;
		}
		if (mp != NULL && pp->p_fstype != FS_UNUSED &&
		    pp->p_fstype != FS_SWAP && pp->p_fstype != FS_BOOT &&
		    pp->p_fstype != FS_OTHER) {
			/* get mount point */
			p = getstring(lp, "mount point",
			    "Where to mount this filesystem (ie: / /var /usr)",
			    "none");
			if (p == NULL) {
				fputs("Command aborted\n", stderr);
				pp->p_size = 0;		/* effective delete */
				return;
			}
			if (strcasecmp(p, "none") != 0) {
				/* XXX - check that it starts with '/' */
				if (mp[partno] != NULL)
					free(mp[partno]);
				if ((mp[partno] = strdup(p)) == NULL)
					errx(4, "out of memory");
			}
		}
	}
	/* Update free sector count and make sure things stay contiguous. */
	*freep -= pp->p_size;
	if (pp->p_size + pp->p_offset > ending_sector ||
	    has_overlap(lp, freep, -1))
		make_contiguous(lp);
}

/*
 * Set the mountpoint of an existing partition ('name').
 */
void
editor_name(lp, mp, p)
	struct disklabel *lp;
	char **mp;
	char *p;
{
	struct partition *pp;
	int partno;

	/* Change which partition? */
	if (p == NULL) {
		p = getstring(lp, "partition to name",
		    "The letter of the partition to name, a - p.", NULL);
	}
	if (p == NULL) {
		fputs("Command aborted\n", stderr);
		return;
	}
	partno = p[0] - 'a';
	pp = &lp->d_partitions[partno];
	if (partno < 0 || partno >= lp->d_npartitions) {
		fprintf(stderr, "Partition must be between 'a' and '%c'.\n",
		    'a' + lp->d_npartitions - 1);
		return;
	} else if (partno >= lp->d_npartitions ||
	    (pp->p_fstype == FS_UNUSED && pp->p_size == 0)) {
		fprintf(stderr, "Partition '%c' is not in use.\n", 'a' + partno);
		return;
	}
	
	/* Not all fstypes can be named */
	if (pp->p_fstype == FS_UNUSED || pp->p_fstype == FS_SWAP ||
	    pp->p_fstype == FS_BOOT || pp->p_fstype == FS_OTHER) {
		fprintf(stderr, "You cannot name a filesystem of type %s.\n",
		    fstypesnames[lp->d_partitions[partno].p_fstype]);
		return;
	}

	p = getstring(lp, "mount point",
	    "Where to mount this filesystem (ie: / /var /usr)",
	    mp[partno] ? mp[partno] : "none");
	if (p == NULL) {
		fputs("Command aborted\n", stderr);
		pp->p_size = 0;		/* effective delete */
		return;
	}
	if (mp[partno] != NULL) {
		free(mp[partno]);
		mp[partno] = NULL;
	}
	if (strcasecmp(p, "none") != 0) {
		/* XXX - check that it starts with '/' */
		if ((mp[partno] = strdup(p)) == NULL)
			errx(4, "out of memory");
	}
}

/*
 * Change an existing partition.
 */
void
editor_modify(lp, mp, freep, p)
	struct disklabel *lp;
	char **mp;
	u_int32_t *freep;
	char *p;
{
	struct partition origpart, *pp;
	u_int32_t ui;
	int partno;

	/* Change which partition? */
	if (p == NULL) {
		p = getstring(lp, "partition to modify",
		    "The letter of the partition to modify, a - p.", NULL);
	}
	if (p == NULL) {
		fputs("Command aborted\n", stderr);
		return;
	}
	partno = p[0] - 'a';
	pp = &lp->d_partitions[partno];
	origpart = lp->d_partitions[partno];
	if (partno < 0 || partno >= lp->d_npartitions) {
		fprintf(stderr, "Partition must be between 'a' and '%c'.\n",
		    'a' + lp->d_npartitions - 1);
		return;
	} else if (partno >= lp->d_npartitions ||
	    (pp->p_fstype == FS_UNUSED && pp->p_size == 0)) {
		fprintf(stderr, "Partition '%c' is not in use.\n", 'a' + partno);
		return;
	}
	
	/* Get filesystem type */
	if (pp->p_fstype < FSMAXTYPES) {
		p = getstring(lp, "FS type",
		    "Filesystem type (usually 4.2BSD or swap)",
		    fstypenames[pp->p_fstype]);
		if (p == NULL) {
			fputs("Command aborted\n", stderr);
			pp->p_size = 0;		/* effective delete */
			return;
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
			ui = getuint(lp, partno, "FS type (decimal)",
			    "Filesystem type as a decimal number; usually 7 (4.2BSD) or 1 (swap).",
			    pp->p_fstype, pp->p_fstype, 0, 0);
			if (ui == UINT_MAX - 1) {
				fputs("Command aborted\n", stderr);
				pp->p_size = 0;		/* effective delete */
				return;
			} else if (ui == UINT_MAX)
				fputs("Invalid entry\n", stderr);
			else
				break;
		}
		pp->p_fstype = ui;
	}

	/* Did they disable/enable the partition? */
	if ((pp->p_fstype == FS_UNUSED || pp->p_fstype == FS_BOOT) &&
	    origpart.p_fstype != FS_UNUSED && origpart.p_fstype != FS_BOOT)
		*freep += origpart.p_size;
	else if (pp->p_fstype != FS_UNUSED && pp->p_fstype != FS_BOOT &&
	    (origpart.p_fstype == FS_UNUSED || origpart.p_fstype == FS_BOOT)) {
		if (pp->p_size > *freep) {
			fprintf(stderr,
			    "Warning, need %u sectors but there are only %u "
			    "free.  Setting size to %u.\n", pp->p_size, *freep,
			    *freep);
			pp->p_fstype = *freep;
			*freep = 0;
		} else
			*freep -= pp->p_size;		/* have enough space */
	}

getoff2:
	/* Get offset */
	for (;;) {
		ui = getuint(lp, partno, "offset",
		    "Starting sector for this partition.", pp->p_offset,
		    pp->p_offset, 0, DO_CONVERSIONS |
		    (pp->p_fstype == FS_BSDFFS ? DO_ROUNDING : 0));
		if (ui == UINT_MAX - 1) {
			fputs("Command aborted\n", stderr);
			*pp = origpart;		/* undo changes */
			return;
		} else if (ui == UINT_MAX)
			fputs("Invalid entry\n", stderr);
		else if (partno != 2 && ui + ui < starting_sector) {
			fprintf(stderr, "The OpenBSD portion of the disk starts"
			    " at sector %u, you tried to start at %u."
			    "  You can use the 'b' command to change the size "
			    "of the OpenBSD portion.\n", starting_sector, ui);
		} else
			break;
	}
	pp->p_offset = ui;

	/* Get size */
	/* XXX - this loop sucks */
	for (;;) {
		ui = getuint(lp, partno, "size", "Size of the partition.",
		    pp->p_size, *freep, pp->p_offset, DO_CONVERSIONS |
		    ((pp->p_fstype == FS_BSDFFS || pp->p_fstype == FS_SWAP)
		    ? DO_ROUNDING : 0));

		if (ui == pp->p_size)
			break;			/* no change */

		if (ui == UINT_MAX - 1) {
			fputs("Command aborted\n", stderr);
			*pp = origpart;		/* undo changes */
			return;
		} else if (ui == UINT_MAX) {
			fputs("Invalid entry\n", stderr);
			continue;
		} else if (partno == 2 && ui + pp->p_offset > lp->d_secperunit) {
			fputs("'c' partition may not be larger than the disk\n",
			    stderr);
			continue;
		}

		if (pp->p_fstype == FS_UNUSED || pp->p_fstype == FS_BOOT) {
			pp->p_size = ui;	/* don't care what's free */
			break;
		} else {
			if (ui > pp->p_size + *freep)
				/* XXX - prompt user to steal space from another partition */
				fprintf(stderr,
				    "Size may not be larger than %u sectors\n",
				    pp->p_size + *freep);
			else {
				*freep += pp->p_size - ui;
				pp->p_size = ui;
				break;
			}
		}
	}
	/* XXX - if (ui % lp->d_secpercyl == 0) make ui + offset on cyl bound */
	pp->p_size = ui;
	if (pp->p_size == 0)
		return;

	/* Check for overlap and restore if not resolved */
	if (has_overlap(lp, freep, 0)) {
		puts("\nPlease re-enter an offset and size");
		pp->p_offset = origpart.p_offset;
		pp->p_size = origpart.p_size;
		goto getoff2;		/* Yeah, I know... */
	}

	if (pp->p_fstype == FS_BSDFFS || pp->p_fstype == FS_UNUSED) {
		/* get fsize */
		for (;;) {
			ui = getuint(lp, partno, "fragment size",
			    "Size of fs block fragments.  Usually 1024 or 512.",
			    pp->p_fsize ? pp->p_fsize : 1024, 1024, 0, 0);
			if (ui == UINT_MAX - 1) {
				fputs("Command aborted\n", stderr);
				*pp = origpart;		/* undo changes */
				return;
			} else if (ui == UINT_MAX)
				fputs("Invalid entry\n", stderr);
			else
				break;
		}
		pp->p_fsize = ui;
		if (pp->p_fsize == 0)
			puts("Zero fragment size implies zero block size");

		/* get bsize */
		for (; pp->p_fsize > 0;) {
			ui = getuint(lp, partno, "block size",
			    "Size of filesystem blocks.  Usually 8192 or 4096.",
			    pp->p_frag ? pp->p_fsize * pp->p_frag : 8192,
			    8192, 0, 0);

			/* sanity check */
			if (ui == UINT_MAX - 1) {
				fputs("Command aborted\n", stderr);
				*pp = origpart;		/* undo changes */
				return;
			} else if (ui == UINT_MAX)
				fputs("Invalid entry\n", stderr);
			else if (ui % pp->p_fsize != 0)
				puts("Error: block size must be a multiple of the fragment size.");
			else if (ui / pp->p_fsize < 1)
				puts("Error: block size must be at least as big as fragment size.");
			else {
				pp->p_frag = ui / pp->p_fsize;
				break;
			}
		}

		if (pp->p_fstype == FS_BSDFFS) {
			/* get cpg */
			for (;;) {
				ui = getuint(lp, partno, "cpg",
				    "Number of filesystem cylinders per group."
				    "  Usually 16 or 8.",
				    pp->p_cpg ? pp->p_cpg : 16, 16, 0, 0);
				if (ui == UINT_MAX - 1) {
					fputs("Command aborted\n", stderr);
					*pp = origpart;	/* undo changes */
					return;
				} else if (ui == UINT_MAX)
					fputs("Invalid entry\n", stderr);
				else
					break;
			}
			pp->p_cpg = ui;
		}
	}

	if (mp != NULL && pp->p_fstype != FS_UNUSED &&
	    pp->p_fstype != FS_SWAP && pp->p_fstype != FS_BOOT &&
	    pp->p_fstype != FS_OTHER) {
		/* get mount point */
		p = getstring(lp, "mount point",
		    "Where to mount this filesystem (ie: / /var /usr)",
		    mp[partno] ? mp[partno] : "none");
		if (p == NULL) {
			fputs("Command aborted\n", stderr);
			pp->p_size = 0;		/* effective delete */
			return;
		}
		if (mp[partno] != NULL) {
			free(mp[partno]);
			mp[partno] = NULL;
		}
		if (strcasecmp(p, "none") != 0) {
			/* XXX - check that it starts with '/' */
			if ((mp[partno] = strdup(p)) == NULL)
				errx(4, "out of memory");
		}
	}

	/* Make sure things stay contiguous. */
	if (pp->p_size + pp->p_offset > ending_sector ||
	    has_overlap(lp, freep, -1))
		make_contiguous(lp);
}

/*
 * Delete an existing partition.
 */
void
editor_delete(lp, mp, freep, p)
	struct disklabel *lp;
	char **mp;
	u_int32_t *freep;
	char *p;
{
	int c;

	if (p == NULL) {
		p = getstring(lp, "partition to delete",
		    "The letter of the partition to delete, a - p, or '*'.",
		    NULL);
	}
	if (p == NULL) {
		fputs("Command aborted\n", stderr);
		return;
	}
	if (p[0] == '*') {
		for (c = 0; c < lp->d_npartitions; c++) {
			if (c == 2)
				continue;

			/* Update free sector count. */
			if (lp->d_partitions[c].p_fstype != FS_UNUSED &&
			    lp->d_partitions[c].p_fstype != FS_BOOT &&
			    lp->d_partitions[c].p_size != 0)
				*freep += lp->d_partitions[c].p_size;

			(void)memset(&lp->d_partitions[c], 0,
			    sizeof(lp->d_partitions[c]));
		}
		return;
	}
	c = p[0] - 'a';
	if (c < 0 || c >= lp->d_npartitions)
		fprintf(stderr, "Partition must be between 'a' and '%c'.\n",
		    'a' + lp->d_npartitions - 1);
	else if (c >= lp->d_npartitions || (lp->d_partitions[c].p_fstype ==
	    FS_UNUSED && lp->d_partitions[c].p_size == 0))
		fprintf(stderr, "Partition '%c' is not in use.\n", 'a' + c);
	else if (c == 2)
		fputs(
"You may not delete the 'c' partition.  The 'c' partition must exist and\n"
"should span the entire disk.  By default it is of type 'unused' and so\n"
"does not take up any space.\n", stderr);
	else {
		/* Update free sector count. */
		if (lp->d_partitions[c].p_offset < ending_sector &&
		    lp->d_partitions[c].p_offset >= starting_sector &&
		    lp->d_partitions[c].p_fstype != FS_UNUSED &&
		    lp->d_partitions[c].p_fstype != FS_BOOT &&
		    lp->d_partitions[c].p_size != 0)
			*freep += lp->d_partitions[c].p_size;

		/* Really delete it (as opposed to just setting to "unused") */
		(void)memset(&lp->d_partitions[c], 0,
		    sizeof(lp->d_partitions[c]));
	}
	if (mp != NULL && mp[c] != NULL) {
		free(mp[c]);
		mp[c] = NULL;
	}
}

/*
 * Simplified display() for use with the builtin editor.
 */
void
editor_display(lp, mp, freep, unit)
	struct disklabel *lp;
	char **mp;
	u_int32_t *freep;
	char unit;
{
	int i;
	int width;

	printf("device: %s\n", specname);
	printf("type: %s\n", dktypenames[lp->d_type]);
	printf("disk: %.*s\n", (int)sizeof(lp->d_typename), lp->d_typename);
	printf("label: %.*s\n", (int)sizeof(lp->d_packname), lp->d_packname);
	printf("bytes/sector: %ld\n", (long)lp->d_secsize);
	printf("sectors/track: %ld\n", (long)lp->d_nsectors);
	printf("tracks/cylinder: %ld\n", (long)lp->d_ntracks);
	printf("sectors/cylinder: %ld\n", (long)lp->d_secpercyl);
	printf("cylinders: %ld\n", (long)lp->d_ncylinders);
	printf("total sectors: %ld\n", (long)lp->d_secperunit);
	printf("free sectors: %u\n", *freep);
	printf("rpm: %ld\n", (long)lp->d_rpm);
	printf("\n%d partitions:\n", lp->d_npartitions);
	width = width_partition(lp, unit);
	printf("#    %*.*s %*.*s    fstype   [fsize bsize   cpg]\n",
		width, width, "size", width, width, "offset");
	for (i = 0; i < lp->d_npartitions; i++)
		display_partition(stdout, lp, mp, i, unit, width);
}

/*
 * Find the next reasonable starting offset and returns it.
 * Assumes there is a least one free sector left (returns 0 if not).
 */
u_int32_t
next_offset(lp, pp)
	struct disklabel *lp;
	struct partition *pp;
{
	struct partition **spp;
	struct diskchunk *chunks;
	u_int16_t npartitions;
	u_int32_t new_offset, new_size;
	int i, good_offset;

	/* Get a sorted list of the partitions */
	if ((spp = sort_partitions(lp, &npartitions)) == NULL)
		return(0);
	
	new_offset = starting_sector;
	for (i = 0; i < npartitions; i++ ) {
		/* Skip the partition for which we are finding an offset */
		if (pp == spp[i])
			continue;

		/*
		 * Is new_offset inside this partition?  If so,
		 * make it the next sector after the partition ends.
		 */
		if (spp[i]->p_offset + spp[i]->p_size < ending_sector &&
		    ((new_offset >= spp[i]->p_offset &&
		    new_offset < spp[i]->p_offset + spp[i]->p_size) ||
		    (new_offset + pp->p_size >= spp[i]->p_offset && new_offset
		    + pp->p_size <= spp[i]->p_offset + spp[i]->p_size)))
			new_offset = spp[i]->p_offset + spp[i]->p_size;
	}

	/* Did we find a suitable offset? */
	for (good_offset = 1, i = 0; i < npartitions; i++ ) {
		if (new_offset + pp->p_size >= spp[i]->p_offset &&
		    new_offset + pp->p_size <= spp[i]->p_offset + spp[i]->p_size) {
			/* Nope */
			good_offset = 0;
			break;
		}
	}

	/* Specified size is too big, find something that fits */
	if (!good_offset) {
		chunks = free_chunks(lp);
		new_size = 0;
		for (i = 0; chunks[i].start != 0 || chunks[i].stop != 0; i++) {
			if (chunks[i].stop - chunks[i].start > new_size) {
			    new_size = chunks[i].stop - chunks[i].start;
			    new_offset = chunks[i].start;
			}
		}
		/* XXX - should do something intelligent if new_size == 0 */
		pp->p_size = new_size;
	}

	(void)free(spp);
	return(new_offset);
}

/*
 * Change the size of an existing partition.
 */
void
editor_change(lp, freep, p)
	struct disklabel *lp;
	u_int32_t *freep;
	char *p;
{
	int partno;
	u_int32_t newsize;
	struct partition *pp;

	if (p == NULL) {
		p = getstring(lp, "partition to change size",
		    "The letter of the partition to change size, a - p.", NULL);
	}
	if (p == NULL) {
		fputs("Command aborted\n", stderr);
		return;
	}
	partno = p[0] - 'a';
	if (partno < 0 || partno >= lp->d_npartitions) {
		fprintf(stderr, "Partition must be between 'a' and '%c'.\n",
		    'a' + lp->d_npartitions - 1);
		return;
	} else if (partno >= lp->d_npartitions ||
	    lp->d_partitions[partno].p_size == 0) {
		fprintf(stderr, "Partition '%c' is not in use.\n", 'a' + partno);
		return;
	}
	pp = &lp->d_partitions[partno];

	printf("Partition %c is currently %u sectors in size (%u free).\n",
	    partno + 'a', pp->p_size, *freep);
	/* XXX - make maxsize lp->d_secperunit if FS_UNUSED/FS_BOOT? */
	newsize = getuint(lp, partno, "new size", "Size of the partition.  "
	    "You may also say +/- amount for a relative change.",
	    pp->p_size, pp->p_size + *freep, pp->p_offset, DO_CONVERSIONS |
	    (pp->p_fstype == FS_BSDFFS ? DO_ROUNDING : 0));
	if (newsize == UINT_MAX - 1) {
		fputs("Command aborted\n", stderr);
		return;
	} else if (newsize == UINT_MAX) {
		fputs("Invalid entry\n", stderr);
		return;
	} else if (newsize == pp->p_size)
		return;

	if (pp->p_fstype != FS_UNUSED && pp->p_fstype != FS_BOOT) {
		if (newsize > pp->p_size) {
			if (newsize - pp->p_size > *freep) {
				fprintf(stderr,
				    "Only %u sectors free, you asked for %u\n",
				    *freep, newsize - pp->p_size);
				return;
			}
			*freep -= newsize - pp->p_size;
		} else if (newsize < pp->p_size) {
			*freep += pp->p_size - newsize;
		}
	} else {
		if (partno == 2 && newsize +
		    pp->p_offset > lp->d_secperunit) {
			fputs("'c' partition may not be larger than the disk\n",
			    stderr);
			return;
		}
	}
	pp->p_size = newsize;
	if (newsize + pp->p_offset > ending_sector ||
	    has_overlap(lp, freep, -1))
		make_contiguous(lp);
}

void
make_contiguous(lp)
	struct disklabel *lp;
{
	struct partition **spp;
	u_int16_t npartitions;
	int i;

	/* Get a sorted list of the partitions */
	if ((spp = sort_partitions(lp, &npartitions)) == NULL)
		return;

	/*
	 * Make everything contiguous but don't muck with start of the first one
	 * or partitions not in the BSD part of the label.
	 */
	for (i = 1; i < npartitions; i++) {
		if (spp[i]->p_offset >= starting_sector ||
		    spp[i]->p_offset < ending_sector)
			spp[i]->p_offset =
			    spp[i - 1]->p_offset + spp[i - 1]->p_size;
	}

	(void)free(spp);
}

/*
 * Sort the partitions based on starting offset.
 * This assumes there can be no overlap.
 */
int
partition_cmp(e1, e2)
	const void *e1, *e2;
{
	struct partition *p1 = *(struct partition **)e1;
	struct partition *p2 = *(struct partition **)e2;

	return((int)(p1->p_offset - p2->p_offset));
}

char *
getstring(lp, prompt, helpstring, oval)
	struct disklabel *lp;
	char *prompt;
	char *helpstring;
	char *oval;
{
	static char buf[BUFSIZ];
	int n;

	buf[0] = '\0';
	do {
		printf("%s: [%s] ", prompt, oval ? oval : "");
		fflush(stdout);
		rewind(stdin);
		if (fgets(buf, sizeof(buf), stdin) == NULL) {
			buf[0] = '\0';
			if (feof(stdin)) {
				putchar('\n');
				return(NULL);
			}
		}
		n = strlen(buf);
		if (n > 0 && buf[n-1] == '\n')
			buf[--n] = '\0';
		if (buf[0] == '?')
			puts(helpstring);
		else if (oval != NULL && buf[0] == '\0') {
			(void)strncpy(buf, oval, sizeof(buf) - 1);
			buf[sizeof(buf) - 1] = '\0';
		}
	} while (buf[0] == '?');

	return(&buf[0]);
}

/*
 * Returns UINT_MAX on error
 * XXX - there are way too many parameters here.  There should be macros...
 */
u_int32_t
getuint(lp, partno, prompt, helpstring, oval, maxval, offset, flags)
	struct disklabel *lp;
	int partno;
	char *prompt;
	char *helpstring;
	u_int32_t oval;
	u_int32_t maxval;		/* XXX - used inconsistently */
	u_int32_t offset;
	int flags;
{
	char buf[BUFSIZ], *endptr, *p, operator = '\0';
	u_int32_t rval = oval;
	size_t n;
	int mult = 1;
	double d;

	/* We only care about the remainder */
	offset = offset % lp->d_secpercyl;

	buf[0] = '\0';
	do {
		printf("%s: [%u] ", prompt, oval);
		fflush(stdout);
		rewind(stdin);
		if (fgets(buf, sizeof(buf), stdin) == NULL) {
			buf[0] = '\0';
			if (feof(stdin)) {
				putchar('\n');
				return(UINT_MAX - 1);
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
				rval = UINT_MAX;	/* too big/small */
			else if (*endptr != '\0') {
				errno = EINVAL;		/* non-numbers in str */
				rval = UINT_MAX;
			} else {
				/* XXX - should check for overflow */
				if (mult > 0)
					rval = d * mult;
				else
					/* Negative mult means divide (fancy) */
					rval = d / (-mult);

				/* Apply the operator */
				if (operator == '+')
					rval += oval;
				else if (operator == '-')
					rval = oval - rval;
			}
		}
	}
	if ((flags & DO_ROUNDING) && rval < UINT_MAX) {
#ifndef CYLCHECK
		/* Round to nearest cylinder unless given in sectors */
		if (mult != 1)
#endif
		{
			u_int32_t cyls;

			/* If we round up past the end, round down instead */
			cyls = (u_int32_t)((rval / (double)lp->d_secpercyl)
			    + 0.5);
			if (cyls != 0 && lp->d_secpercyl != 0) {
				if ((cyls * lp->d_secpercyl) - offset > maxval)
					cyls--;

				if (rval != (cyls * lp->d_secpercyl) - offset) {
					rval = (cyls * lp->d_secpercyl) - offset;
					printf("Rounding to nearest cylinder: %u\n",
					    rval);
				}
			}
		}
	}

	return(rval);
}

/*
 * Check for partition overlap in lp and prompt the user
 * to resolve the overlap if any is found.  Returns 1
 * if unable to resolve, else 0.
 */
int
has_overlap(lp, freep, resolve)
	struct disklabel *lp;
	u_int32_t *freep;
	int resolve;
{
	struct partition **spp;
	u_int16_t npartitions;
	int c, i, j;
	char buf[BUFSIZ];

	/* Get a sorted list of the partitions */
	spp = sort_partitions(lp, &npartitions);

	if (npartitions < 2) {
		(void)free(spp);
		return(0);			/* nothing to do */
	}

	/* Now that we have things sorted by starting sector check overlap */
	for (i = 0; i < npartitions; i++) {
		for (j = i + 1; j < npartitions; j++) {
			/* `if last_sec_in_part + 1 > first_sec_in_next_part' */
			if (spp[i]->p_offset + spp[i]->p_size > spp[j]->p_offset) {
				/* Don't print, just return */
				if (resolve == -1) {
					(void)free(spp);
					return(1);
				}
					
				/* Overlap!  Convert to real part numbers. */
				i = ((char *)spp[i] - (char *)lp->d_partitions)
				    / sizeof(**spp);
				j = ((char *)spp[j] - (char *)lp->d_partitions)
				    / sizeof(**spp);
				printf("\nError, partitions %c and %c overlap:\n",
				    'a' + i, 'a' + j);
				puts("         size   offset    fstype   [fsize bsize   cpg]");
				display_partition(stdout, lp, NULL, i, 0, 0);
				display_partition(stdout, lp, NULL, j, 0, 0);

				/* Did they ask us to resolve it ourselves? */
				if (resolve != 1) {
					(void)free(spp);
					return(1);
				}

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
				*freep += lp->d_partitions[c].p_size;
				(void)free(spp);
				return(has_overlap(lp, freep, resolve));
			}
		}
	}

	(void)free(spp);
	return(0);
}

void
edit_parms(lp, freep)
	struct disklabel *lp;
	u_int32_t *freep;
{
	char *p;
	u_int32_t ui;
	struct disklabel oldlabel = *lp;

	printf("Changing device parameters for %s:\n", specname);

	/* disk type */
	for (;;) {
		p = getstring(lp, "disk type",
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
	p = getstring(lp, "label name",
	    "15 char string that describes this label, usually the disk name.",
	    lp->d_packname);
	if (p == NULL) {
		fputs("Command aborted\n", stderr);
		*lp = oldlabel;		/* undo damage */
		return;
	}
	strncpy(lp->d_packname, p, sizeof(lp->d_packname) - 1);
	lp->d_packname[sizeof(lp->d_packname) - 1] = '\0';

	/* sectors/track */
	for (;;) {
		ui = getuint(lp, 0, "sectors/track",
		    "The Numer of sectors per track.", lp->d_nsectors,
		    lp->d_nsectors, 0, 0);
		if (ui == UINT_MAX - 1) {
			fputs("Command aborted\n", stderr);
			*lp = oldlabel;		/* undo damage */
			return;
		} if (ui == UINT_MAX)
			fputs("Invalid entry\n", stderr);
		else
			break;
	}
	lp->d_nsectors = ui;

	/* tracks/cylinder */
	for (;;) {
		ui = getuint(lp, 0, "tracks/cylinder",
		    "The number of tracks per cylinder.", lp->d_ntracks,
		    lp->d_ntracks, 0, 0);
		if (ui == UINT_MAX - 1) {
			fputs("Command aborted\n", stderr);
			*lp = oldlabel;		/* undo damage */
			return;
		} else if (ui == UINT_MAX)
			fputs("Invalid entry\n", stderr);
		else
			break;
	}
	lp->d_ntracks = ui;

	/* sectors/cylinder */
	for (;;) {
		ui = getuint(lp, 0, "sectors/cylinder",
		    "The number of sectors per cylinder (Usually sectors/track "
		    "* tracks/cylinder).", lp->d_secpercyl, lp->d_secpercyl,
		    0, 0);
		if (ui == UINT_MAX - 1) {
			fputs("Command aborted\n", stderr);
			*lp = oldlabel;		/* undo damage */
			return;
		} else if (ui == UINT_MAX)
			fputs("Invalid entry\n", stderr);
		else
			break;
	}
	lp->d_secpercyl = ui;

	/* number of cylinders */
	for (;;) {
		ui = getuint(lp, 0, "number of cylinders",
		    "The total number of cylinders on the disk.",
		    lp->d_ncylinders, lp->d_ncylinders, 0, 0);
		if (ui == UINT_MAX - 1) {
			fputs("Command aborted\n", stderr);
			*lp = oldlabel;		/* undo damage */
			return;
		} else if (ui == UINT_MAX)
			fputs("Invalid entry\n", stderr);
		else
			break;
	}
	lp->d_ncylinders = ui;

	/* total sectors */
	for (;;) {
		ui = getuint(lp, 0, "total sectors",
		    "The total number of sectors on the disk.",
		    lp->d_secperunit ? lp->d_secperunit :
		    lp->d_ncylinders * lp->d_ncylinders,
		    lp->d_ncylinders * lp->d_ncylinders, 0, 0);
		if (ui == UINT_MAX - 1) {
			fputs("Command aborted\n", stderr);
			*lp = oldlabel;		/* undo damage */
			return;
		} else if (ui == UINT_MAX)
			fputs("Invalid entry\n", stderr);
		else if (ui > lp->d_secperunit &&
		    ending_sector == lp->d_secperunit) {
			/* grow free count */
			*freep += ui - lp->d_secperunit;
			puts("You may want to increase the size of the 'c' "
			    "partition.");
			break;
		} else if (ui < lp->d_secperunit &&
		    ending_sector == lp->d_secperunit) {
			/* shrink free count */
			if (lp->d_secperunit - ui > *freep)
				fprintf(stderr,
				    "Not enough free space to shrink by %u "
				    "sectors (only %u sectors left)\n",
				    lp->d_secperunit - ui, *freep);
			else {
				*freep -= lp->d_secperunit - ui;
				break;
			}
		} else
			break;
	}
	/* Adjust ending_sector if necesary. */
	if (ending_sector > ui)
		ending_sector = ui;
	lp->d_secperunit = ui;

	/* rpm */
	for (;;) {
		ui = getuint(lp, 0, "rpm",
		  "The rotational speed of the disk in revolutions per minute.",
		  lp->d_rpm, lp->d_rpm, 0, 0);
		if (ui == UINT_MAX - 1) {
			fputs("Command aborted\n", stderr);
			*lp = oldlabel;		/* undo damage */
			return;
		} else if (ui == UINT_MAX)
			fputs("Invalid entry\n", stderr);
		else
			break;
	}
	lp->d_rpm = ui;

	/* interleave */
	for (;;) {
		ui = getuint(lp, 0, "interleave",
		  "The physical sector interleave, set when formatting.  Almost always 1.",
		  lp->d_interleave, lp->d_interleave, 0, 0);
		if (ui == UINT_MAX - 1) {
			fputs("Command aborted\n", stderr);
			*lp = oldlabel;		/* undo damage */
			return;
		} else if (ui == UINT_MAX || ui == 0)
			fputs("Invalid entry\n", stderr);
		else
			break;
	}
	lp->d_interleave = ui;
}

struct partition **
sort_partitions(lp, npart)
	struct disklabel *lp;
	u_int16_t *npart;
{
	u_int16_t npartitions;
	struct partition **spp;
	int i;

	/* How many "real" partitions do we have? */
	for (npartitions = 0, i = 0; i < lp->d_npartitions; i++) {
		if (lp->d_partitions[i].p_fstype != FS_UNUSED &&
		    lp->d_partitions[i].p_fstype != FS_BOOT &&
		    lp->d_partitions[i].p_size != 0)
			npartitions++;
	}
	if (npartitions == 0) {
		*npart = 0;
		return(NULL);
	}

	/* Create an array of pointers to the partition data */
	if ((spp = malloc(sizeof(struct partition *) * npartitions)) == NULL)
		errx(4, "out of memory");
	for (npartitions = 0, i = 0; i < lp->d_npartitions; i++) {
		if (lp->d_partitions[i].p_fstype != FS_UNUSED &&
		    lp->d_partitions[i].p_fstype != FS_BOOT &&
		    lp->d_partitions[i].p_size != 0)
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

	*npart = npartitions;
	return(spp);
}

/*
 * Get a valid disk type if necessary.
 */
void
getdisktype(lp, banner, dev)
	struct disklabel *lp;
	char *banner;
	char *dev;
{
	int i;
	char *s, *def = "SCSI";
	struct dtypes {
		char *dev;
		char *type;
	} dtypes[] = {
		"sd",	"SCSI",
		"rz",	"SCSI",
		"wd",	"IDE",
		"fd",	"FLOPPY",
		"xd",	"SMD",
		"xy",	"SMD",
		"hd",	"HP-IB",
		"ccd",	"CCD",
		"vnd",	"VND",
		"svnd",	"VND",
		NULL,	NULL
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
			s = getstring(lp, "Disk type",
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
 * XXX - should mention MBR values if DOSLABEL
 */
void
set_bounds(lp, freep)
	struct disklabel *lp;
	u_int32_t *freep;
{
	u_int32_t ui, start_temp;

	/* Starting sector */
	do {
		ui = getuint(lp, 0, "Starting sector",
		  "The start of the OpenBSD portion of the disk.",
		  starting_sector, lp->d_secperunit, 0, 0);
		if (ui == UINT_MAX - 1) {
			fputs("Command aborted\n", stderr);
			return;
		}
	} while (ui >= lp->d_secperunit);
	start_temp = ui;

	/* Size */
	do {
		ui = getuint(lp, 0, "Size ('*' for entire disk)",
		  "The size of the OpenBSD portion of the disk ('*' for the "
		  "entire disk).", ending_sector - starting_sector,
		  lp->d_secperunit - start_temp, 0, 0);
		if (ui == UINT_MAX - 1) {
			fputs("Command aborted\n", stderr);
			return;
		}
	} while (ui > lp->d_secperunit - start_temp);
	ending_sector = start_temp + ui;
	starting_sector = start_temp;
	
	/* Recalculate the free sectors */
	editor_countfree(lp, freep);
}

/*
 * Return a list of the "chunks" of free space available
 */
struct diskchunk *
free_chunks(lp)
	struct disklabel *lp;
{
	u_int16_t npartitions;
	struct partition **spp;
	static struct diskchunk chunks[MAXPARTITIONS + 2];
	int i, numchunks;

	/* Sort the partitions based on offset */
	spp = sort_partitions(lp, &npartitions);

	/* If there are no partitions, it's all free. */
	if (spp == NULL) {
		chunks[0].start = 0;
		chunks[0].stop = ending_sector;
		chunks[1].start = chunks[1].stop = 0;
		return(chunks);
	}

	/* Find chunks of free space */
	numchunks = 0;
	if (spp && spp[0]->p_offset > 0) {
		chunks[0].start = 0;
		chunks[0].stop = spp[0]->p_offset;
		numchunks++;
	}
	for (i = 0; i < npartitions; i++) {
		if (i + 1 < npartitions) {
			if (spp[i]->p_offset + spp[i]->p_size < spp[i+1]->p_offset) {
				chunks[numchunks].start =
				    spp[i]->p_offset + spp[i]->p_size;
				chunks[numchunks].stop = spp[i+1]->p_offset;
				numchunks++;
			}
		} else {
			/* Last partition */
			if (spp[i]->p_offset + spp[i]->p_size < ending_sector) {
				
				chunks[numchunks].start =
				    spp[i]->p_offset + spp[i]->p_size;
				chunks[numchunks].stop = ending_sector;
				numchunks++;
			}
		}
	}

	/* Terminate and return */
	chunks[numchunks].start = chunks[numchunks].stop = 0;
	(void)free(spp);
	return(chunks);
}

/*
 * What is the OpenBSD portion of the disk?  Uses the MBR if applicable.
 */
void
find_bounds(lp)
	struct disklabel *lp;
{
	struct  partition *pp = &lp->d_partitions[2];

	/* Defaults */
	/* XXX - reserve a cylinder for hp300? */
	starting_sector = 0;
	ending_sector = lp->d_secperunit;

#ifdef DOSLABEL
	/* If we have an MBR, use values from the OpenBSD/FreeBSD parition. */
	if (dosdp && pp->p_size &&
	    (dosdp->dp_typ == DOSPTYP_OPENBSD ||
	    dosdp->dp_typ == DOSPTYP_FREEBSD ||
	    dosdp->dp_typ == DOSPTYP_NETBSD)) {
		starting_sector = get_le(&dosdp->dp_start);
		ending_sector = starting_sector + get_le(&dosdp->dp_size);
		printf("\nTreating sectors %u-%u as the OpenBSD portion of the "
		    "disk.\nYou can use the 'b' command to change this.\n",
		    starting_sector, ending_sector);
		/*
		 * XXX - check to see if any BSD/SWAP partitions go beyond
		 *	 ending_sector and prompt to extend ending_sector if so.
		 */
	}
#endif
}

/*
 * Calculate free space.
 */
void
editor_countfree(lp, freep)
	struct disklabel *lp;
	u_int32_t *freep;
{
	struct partition *pp;
	int i;

	*freep = ending_sector - starting_sector;
	for (i = 0; i < lp->d_npartitions; i++) {
		    pp = &lp->d_partitions[i];
		    if (pp->p_fstype != FS_UNUSED && pp->p_fstype != FS_BOOT &&
			pp->p_size > 0 && 
			pp->p_offset + pp->p_size <= ending_sector &&
			pp->p_offset >= starting_sector)
			*freep -= pp->p_size;
	}
}

void
editor_help(arg)
	char *arg;
{

	/* XXX - put these strings in a table instead? */
	switch (*arg) {
	case 'p':
		puts(
"The 'p' command prints the current disk label.  By default, it prints the\n"
"size and offset in sectors (a sector is usually 512 bytes).  The 'p' command\n"
"takes an optional units argument.  Possible values are 'b' for bytes, 'c'\n"
"for cylinders, 'k' for kilobytes, 'm' for megabytes, and 'g' for gigabytes.\n");
		break;
	case 'M':
		puts(
"The 'M' command pipes the entire OpenBSD manual page for disklabel though\n"
"the pager specified by the PAGER environment variable or 'less' if PAGER is\n"
"not set.  It is especially useful during install when the normal system\n"
"manual is not available.\n");
		break;
	case 'e':
		puts(
"The 'e' command is used to edit the disk drive parameters.  These include\n"
"the number of sectors/track, tracks/cylinder, sectors/cylinder, number of\n"
"cylinders on the disk , total sectors on the disk, rpm, interleave, disk\n"
"type, and a descriptive label string.  You should not change these unless\n"
"you know what you are doing\n");
		break;
	case 'a':
		puts(
"The 'a' command adds new partitions to the disk.  It takes as an optional\n"
"argument the partition letter to add.  If you do not specify a partition\n"
"letter, you will be prompted for it; the next available letter will be the\n"
"default answer\n");
		break;
	case 'b':
		puts(
"The 'b' command is used to change the boundaries of the OpenBSD portion of\n"
"the disk.  This is only useful on disks with an fdisk partition.  By default,\n"
"on a disk with an fdisk partition, the boundaries are set to be the first\n"
"and last sectors of the OpenBSD fdisk partition.  You should only change\n"
"these if your fdisk partition table is incorrect or you have a disk larger\n"
"than 8gig, since 8gig is the maximum size an fdisk partition can be.  You\n"
"may enter '*' at the 'Size' prompt to indicate the entire size of the disk\n"
"(minus the starting sector).  Use this option with care; if you extend the\n"
"boundaries such that they overlap with another operating system you will\n"
"corrupt the other operating system's data.\n");
		break;
	case 'c':
		puts(
"The 'c' command is used to change the size of an existing partition.  It\n"
"takes as an optional argument the partition letter to change.  If you do not\n"
"specify a partition letter, you will be prompted for one.  You may add a '+'\n"
"or '-' prefix to the new size to increase or decrease the existing value\n"
"instead of entering an absolute value.  You may also use a suffix to indicate\n"
"the units the values is in terms of.  Possible suffixes are 'b' for bytes,\n"
"'c' for cylinders, 'k' for kilobytes, 'm' for megabytes, 'g' for gigabytes or\n"
"no suffix for sectors (usually 512 bytes).  You may also enter '*' to change\n"
"the size to be the total number of free sectors remaining.\n");
		break;
	case 'd':
		puts(
"The 'd' command is used to delete an existing partition.  It takes as an\n"
"optional argument the partition letter to change.  If you do not specify a\n"
"partition letter, you will be prompted for one.  You may not delete the ``c''\n"
"partition as 'c' must always exist and by default is marked as 'unused' (so\n"
"it does not take up any space).\n");
		break;
	case 'm':
		puts(
"The 'm' command is used to modify an existing partition.  It takes as an\n"    "optional argument the partition letter to change.  If you do not specify a\n"
"partition letter, you will be prompted for one.  This option allows the user\n"
"to change the filesystem type, starting offset, partition size, block fragment\n"
"size, block size, and cylinders per group for the specified partition (not all\n"
"parameters are configurable for non-BSD partitions).\n");
		break;
	case 'n':
		puts(
"The 'm' command is used to set the mount point for a partition (ie: name it).\n"
"It takes as an optional argument the partition letter to name.  If you do\n"
"not specify a partition letter, you will be prompted for one.  This option\n"
"is only valid if disklabel was invoked with the -F flag.\n");
		break;
	case 'r':
		puts(
"The 'r' command is used to recalculate the free space available.  This option\n"
"should really not be necessary under normal circumstances but can be useful if\n"
"disklabel gets confused.\n");
		break;
	case 'u':
		puts(
"The 'u' command will undo (or redo) the last change.  Entering 'u' once will\n"
"undo your last change.  Entering it again will restore the change.\n");
		break;
	case 's':
		puts(
"The 's' command is used to save a copy of the label to a file in ascii format\n"
"(suitable for loading via disklabel's [-R] option).  It takes as an optional\n"
"argument the filename to save the label to.  If you do not specify a filename,\n"
"you will be prompted for one.\n");
		break;
	case 'w':
		puts(
"The 'w' command will write the current label to disk.  This option will\n"
"commit any changes to the on-disk label.\n");
		break;
	case 'q':
		puts(
"The 'q' command quits the label editor.  If any changes have been made you\n"
"will be asked whether or not to save the changes to the on-disk label.\n");
		break;
	case 'x':
		puts(
"The 'x' command exits the label editor without saving any changes to the\n"
"on-disk label.\n");
		break;
	default:
		puts("Available commands:");
		puts("\tp [unit]  - print label.");
		puts("\tM         - show entire OpenBSD man page for disklabel.");
		puts("\te         - edit drive parameters.");
		puts("\ta [part]  - add new partition.");
		puts("\tb         - set OpenBSD disk boundaries.");
		puts("\tc [part]  - change partition size.");
		puts("\td [part]  - delete partition.");
		puts("\tm [part]  - modify existing partition.");
		puts("\tn [part]  - set the mount point for a partition.");
		puts("\tr         - recalculate free space.");
		puts("\tu         - undo last change.");
		puts("\ts [path]  - save label to file.");
		puts("\tw         - write label to disk.");
		puts("\tq         - quit and save changes.");
		puts("\tx         - exit without saving changes.");
		puts("\t? [cmnd]  - this message or command specific help.");
		puts(
"Numeric parameters may use suffixes to indicate units:\n\t"
"'b' for bytes, 'c' for cylinders, 'k' for kilobytes, 'm' for megabytes,\n\t"
"'g' for gigabytes or no suffix for sectors (usually 512 bytes).\n\t"
"Non-sector units will be rounded to the nearest cylinder.\n"
"Entering '?' at most prompts will give you (simple) context sensitive help.");
		break;
	}
}

char **
mpcopy(to, from)
	char **to;
	char **from;
{
	int i;

	for (i = 0; i < MAXPARTITIONS; i++) {
		if (from[i] != NULL) {
			to[i] = realloc(to[i], strlen(from[i]) + 1);
			if (to[i] == NULL)
				errx(4, "out of memory");
			(void)strcpy(to[i], from[i]);
		} else if (to[i] != NULL) {
			free(to[i]);
			to[i] = NULL;
		}
	}
	return(to);
}

int
mpequal(mp1, mp2)
	char **mp1;
	char **mp2;
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

int
mpsave(lp, mp, cdev, fstabfile)
	struct disklabel *lp;
	char **mp;
	char *cdev;
	char *fstabfile;
{
	int i, mpset;
	char bdev[MAXPATHLEN], *p;
	FILE *fp;

	for (i = 0, mpset = 0; i < MAXPARTITIONS; i++) {
		if (mp[i] != NULL) {
			mpset = 1;
			break;
		}
	}
	if (!mpset)
		return(1);

	/* Convert cdev to bdev */
	if (strncmp(_PATH_DEV, cdev, sizeof(_PATH_DEV) - 1) == 0 &&
	    cdev[sizeof(_PATH_DEV) - 1] == 'r') {
		snprintf(bdev, sizeof(bdev), "%s%s", _PATH_DEV,
		    &cdev[sizeof(_PATH_DEV)]);
	} else {
		if ((p = strrchr(cdev, '/')) == NULL || *(++p) != 'r')
			return(1);
		*p = '\0';
		snprintf(bdev, sizeof(bdev), "%s%s", cdev, p + 1);
		*p = 'r';
	}
	bdev[strlen(bdev) - 1] = '\0';

	if ((fp = fopen(fstabfile, "w")) == NULL)
		return(1);

	for (i = 0; i < MAXPARTITIONS; i++) {
		if (mp[i] == NULL)
			continue;
		fprintf(fp, "%s%c %s %s rw 1 %d\n", bdev, 'a' + i, mp[i],
		    fstypesnames[lp->d_partitions[i].p_fstype],
		    i == 0 ? 1 : 2);
	}
	fclose(fp);
	printf("Wrote fstab entries to %s\n", fstabfile);
	return(0);
}
