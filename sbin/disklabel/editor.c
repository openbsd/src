/*	$OpenBSD: editor.c,v 1.7 1997/10/02 16:38:20 millert Exp $	*/

/*
 * Copyright (c) 1997 Todd C. Miller <Todd.Miller@courtesan.com>
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
 *	This product includes software developed by Todd C. Miller.
 * 4. The name of the author may not be used to endorse or promote products
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
static char rcsid[] = "$OpenBSD: editor.c,v 1.7 1997/10/02 16:38:20 millert Exp $";
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* flags for getuint() */
#define	DO_CONVERSIONS	0x00000001
#define	DO_ROUNDING	0x00000002

void	edit_parms __P((struct disklabel *, u_int32_t *));
int	editor __P((struct disklabel *, int));
void	editor_add __P((struct disklabel *, u_int32_t *, char *));
void	editor_modify __P((struct disklabel *, u_int32_t *, char *));
void	editor_delete __P((struct disklabel *, u_int32_t *, char *));
void	editor_display __P((struct disklabel *, u_int32_t *, char));
void	editor_change __P((struct disklabel *, u_int32_t *, char *));
char	*getstring __P((struct disklabel *, char *, char *, char *));
u_int32_t getuint __P((struct disklabel *, int, char *, char *, u_int32_t, u_int32_t, int));
int	has_overlap __P((struct disklabel *, u_int32_t *, int));
void	make_contiguous __P((struct disklabel *));
u_int32_t next_offset __P((struct disklabel *));
int	partition_cmp __P((const void *, const void *));

/* from disklabel.c */
int	checklabel __P((struct disklabel *));
void	display __P((FILE *, struct disklabel *));
void	display_partition __P((FILE *, struct disklabel *, int, char));
struct disklabel *readlabel __P((int));
struct disklabel *makebootarea __P((char *, struct disklabel *, int));
int	writelabel __P((int, char *, struct disklabel *));
extern	char *bootarea, *specname;
extern	int donothing;

/*
 * Simple partition editor.  Primarily intended for new labels.
 */
int
editor(lp, f)
	struct disklabel *lp;
	int f;
{
	struct disklabel lastlabel, tmplabel, label = *lp;
	u_int32_t freeblocks;
	FILE *fp;
	char buf[BUFSIZ], *cmd, *arg;
	int i;

	/* Set freeblocks based on initial label */
	freeblocks = label.d_secperunit;
	for (i = 0; i < label.d_npartitions; i++)
		if (label.d_partitions[i].p_fstype != FS_UNUSED &&
		    label.d_partitions[i].p_fstype != FS_BOOT &&
		    label.d_partitions[i].p_size > 0)
			freeblocks -= label.d_partitions[i].p_size;

	puts("Initial label editor (enter '?' for help at any prompt)\n");
	if (has_overlap(&label, &freeblocks, 1))
		errx(1, "can't run when there is partition overlap.");

#ifdef CYLCHECK
	puts("This platform requires that partition offsets/sizes be on cylinder boundaries.\nPartition offsets/sizes will be rounded to the nearest cylinder automatically.");
#endif

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
			puts("Available commands:\n");
			puts("\tp [unit]  - print label.");
			puts("\te         - edit drive parameters.");
			puts("\ta [part]  - add new partition.");
			puts("\tc [part]  - change partition size.");
			puts("\td [part]  - delete partition.");
			puts("\tm [part]  - modify existing partition.");
			puts("\tu         - undo last change.");
			puts("\ts [path]  - save label to file.");
			puts("\tw         - write label to disk.");
			puts("\tq         - quit and save changes.");
			puts("\tx         - exit without saving changes.");
			puts("\t?         - this message.\n");
			puts("Numeric parameters may use suffixes to indicate units:\n\t'b' for bytes, 'c' for cylinders, 'k' for kilobytes,\n\t'm' for megabytes or no suffix for blocks (usually 512 bytes).\n\tNon-block units will be rounded to the nearest cylinder.\nThe (optional) argument to p[rint] may be one of the aforementioned units.\n");
			break;

		case 'a':
			tmplabel = lastlabel;
			lastlabel = label;
			editor_add(&label, &freeblocks, arg);
			if (memcmp(&label, &lastlabel, sizeof(label)) == 0)
				lastlabel = tmplabel;
			break;

		case 'c':
			tmplabel = lastlabel;
			lastlabel = label;
			editor_change(&label, &freeblocks, arg);
			if (memcmp(&label, &lastlabel, sizeof(label)) == 0)
				lastlabel = tmplabel;
			break;

		case 'd':
			tmplabel = lastlabel;
			lastlabel = label;
			editor_delete(&label, &freeblocks, arg);
			if (memcmp(&label, &lastlabel, sizeof(label)) == 0)
				lastlabel = tmplabel;
			break;

		case 'm':
			tmplabel = lastlabel;
			lastlabel = label;
			editor_modify(&label, &freeblocks, arg);
			if (memcmp(&label, &lastlabel, sizeof(label)) == 0)
				lastlabel = tmplabel;
			break;

		case 'p':
			editor_display(&label, &freeblocks, arg ? *arg : 0);
			break;

		case 'q':
			if (donothing) {
				puts("In no change mode, not writing label.");
				return(1);
			}
			if (memcmp(lp, &label, sizeof(label)) == 0) {
				puts("No changes.");
				return(1);
			}
			do {
				arg = getstring(lp, "Save changes?",
				    "Save changes you have made to the label?",
				    "n");
			} while (tolower(*arg) != 'y' && tolower(*arg) != 'n');
			if (tolower(*arg) == 'y') {
				*lp = label;
				if (writelabel(f, bootarea, lp) == 0)
					return(0);
			}
			return(1);
			/* NOTREACHED */
			break;

		case 's':
			if (arg == NULL) {
				arg = getstring(lp, "Filename",
				    "Name of the file to save label into.",
				    NULL);
				if (*arg == '\0')
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
			if (memcmp(&label, &lastlabel, sizeof(label)) == 0) {
				puts("Nothing to undo!");
			} else {
				tmplabel = label;
				label = lastlabel;
				lastlabel = tmplabel;
				puts("Last change undone.");
			}
			break;

		case 'w':
			if (donothing)
				puts("In no change mode, not writing label.");
			else if (memcmp(lp, &label, sizeof(label)) == 0)
				puts("No changes.");
			else if (writelabel(f, bootarea, &label) != 0)
				warnx("unable to write label");
			break;

		case 'x':
			return(1);
			break;

		case '\n':
			break;

		case 'e':
			tmplabel = lastlabel;
			lastlabel = label;
			edit_parms(&label, &freeblocks);
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
editor_add(lp, freep, p)
	struct disklabel *lp;
	u_int32_t *freep;
	char *p;
{
	struct partition origpart, *pp;
	char buf[BUFSIZ];
	int i, partno;
	u_int32_t ui;

	/* XXX - prompt user to steal space from another partition instead */
	if (*freep == 0) {
		fputs("No space left, you need to shrink a partition\n",
		    stderr);
		return;
	}

	/* XXX - make more like other editor_* */
	if (p != NULL) {
		partno = p[0] - 'a';
		if (partno < 0 || partno >= MAXPARTITIONS) {
			fprintf(stderr,
			    "Partition must be between 'a' and '%c'.\n",
			    'a' + MAXPARTITIONS - 1);
			return;
		}
	} else {
		/* Find first unused partition that is not 'b' or 'c' */
		for (partno = 0; partno < MAXPARTITIONS; partno++, p++) {
			if (lp->d_partitions[partno].p_size == 0 &&
			    partno != 1 && partno != 2)
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
			partno = p[0] - 'a';
			if (partno >= 0 && partno < MAXPARTITIONS)
				break;
			fprintf(stderr,
			    "Partition must be between 'a' and '%c'.\n",
			    'a' + MAXPARTITIONS - 1);
		}
	}
	/* Increase d_npartitions and set defaults if necesary */
	pp = &lp->d_partitions[partno];
	if (partno >= lp->d_npartitions || pp->p_fstype == FS_UNUSED) {
		if (partno >= lp->d_npartitions)
			lp->d_npartitions = partno + 1;
		memset(pp, 0, sizeof(*pp));
		pp->p_offset = next_offset(lp);
		pp->p_size = *freep;
		if (partno == 1)
			pp->p_fstype = FS_SWAP;
		else
			pp->p_fstype = FS_BSDFFS;
		pp->p_fsize = 1024;
		pp->p_frag = 8;
		pp->p_cpg = 16;
	}
	origpart = *pp;

	/* Get fstype */
	if (pp->p_fstype < FSMAXTYPES) {
		p = getstring(lp, "FS type",
		    "Filesystem type (usually 4.2BSD or swap)",
		    fstypenames[pp->p_fstype]);
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
			    pp->p_fstype, pp->p_fstype, 0);
			if (ui == UINT_MAX)
				fputs("Invalid entry\n", stderr);
			else
				break;
		}
		pp->p_fstype = ui;
	}

getoff1:
	/* Get offset */
	for (;;) {
		ui = getuint(lp, partno, "offset",
		   "Starting sector for this partition.", pp->p_offset,
		   pp->p_offset, DO_CONVERSIONS |
		   (pp->p_fstype == FS_BSDFFS ? DO_ROUNDING : 0));
		if (ui == UINT_MAX)
			fputs("Invalid entry\n", stderr);
		else
			break;
	}
	pp->p_offset = ui;

	/* Get size */
	for (;;) {
		ui = getuint(lp, partno, "size", "Size of the partition.",
		    pp->p_size, *freep, DO_CONVERSIONS |
		    (pp->p_fstype == FS_BSDFFS ? DO_ROUNDING : 0));
		if (ui + pp->p_offset > lp->d_partitions[2].p_size +
		    lp->d_partitions[2].p_offset)
			fputs("Partition must not extend past 'c'\n", stderr);
		else if (ui > *freep)
			/* XXX - prompt user to steal space from another partition */
			fprintf(stderr,"Sorry, there are only %u blocks left\n",
			    *freep);
		else
			break;
	}
	pp->p_size = ui;
	if (pp->p_size == 0)
		return;

	/* Check for overlap and restore if not resolved */
	if (has_overlap(lp, freep, 0)) {
		puts("\nPlease re-enter an offset and size");
		pp->p_offset = origpart.p_offset;
		pp->p_size = origpart.p_size;
		goto getoff1;		/* Yeah, I know... */
	}

	if (pp->p_fstype == FS_BSDFFS || pp->p_fstype == FS_UNUSED) {
		/* get fsize */
		for (;;) {
			ui = getuint(lp, partno, "fragment size",
			    "Size of fs block fragments.  Usually 1024 or 512.",
			    pp->p_fsize, pp->p_fsize, 0);
			if (ui == UINT_MAX)
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
			    0);

			/* sanity checks */
			if (ui == UINT_MAX)
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
				    pp->p_cpg, pp->p_cpg, 0);
				if (ui == UINT_MAX)
					fputs("Invalid entry\n", stderr);
				else
					break;
			}
			pp->p_cpg = ui;
		}
	}
	/* Update free block count and make sure things stay contiguous. */
	*freep -= pp->p_size;
	make_contiguous(lp);
}

/*
 * Change an existing partition.
 */
void
editor_modify(lp, freep, p)
	struct disklabel *lp;
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
			    pp->p_fstype, pp->p_fstype, 0);
			if (ui == UINT_MAX)
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
			    "Warning, need %u blocks but there are only %u "
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
		    pp->p_offset, DO_CONVERSIONS |
		    (pp->p_fstype == FS_BSDFFS ? DO_ROUNDING : 0));
		if (ui == UINT_MAX)
			fputs("Invalid entry\n", stderr);
		else
			break;
	}
	pp->p_offset = ui;

	/* Get size */
	/* XXX - this loop sucks */
	for (;;) {
		ui = getuint(lp, partno, "size", "Size of the partition.",
		    pp->p_size, *freep, 1);

		if (ui == pp->p_size)
			break;			/* no change */

		if (ui == UINT_MAX) {
			fputs("Invalid entry\n", stderr);
			continue;
		} else if (partno != 2 && ui + pp->p_offset >
		    lp->d_partitions[2].p_size + lp->d_partitions[2].p_offset) {
			fputs("Partition must not extend past 'c'\n", stderr);
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
				    "Size may not be larger than %u blocks\n",
				    pp->p_size + *freep);
			else {
				*freep += pp->p_size - ui;
				pp->p_size = ui;
				break;
			}
		}
	}
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
			    pp->p_fsize, 1024, 0);
			if (ui == UINT_MAX)
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
			    pp->p_fsize * pp->p_frag, pp->p_fsize * pp->p_frag,
			    0);

			/* sanity check */
			if (ui == UINT_MAX)
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
				    "  Usually 16 or 8.", pp->p_cpg, pp->p_cpg,
				    0);
				if (ui == UINT_MAX)
					fputs("Invalid entry\n", stderr);
				else
					break;
			}
			pp->p_cpg = ui;
		}
	}

	/* Make sure things stay contiguous. */
	make_contiguous(lp);
}

/*
 * Delete an existing partition.
 */
void
editor_delete(lp, freep, p)
	struct disklabel *lp;
	u_int32_t *freep;
	char *p;
{
	int c;

	if (p == NULL) {
		p = getstring(lp, "partition to delete",
		    "The letter of the partition to delete, a - p.", NULL);
	}
	c = p[0] - 'a';
	if (c < 0 || c >= lp->d_npartitions)
		fprintf(stderr, "Partition must be between 'a' and '%c'.\n",
		    'a' + lp->d_npartitions - 1);
	else if (c >= lp->d_npartitions || (lp->d_partitions[c].p_fstype ==
	    FS_UNUSED && lp->d_partitions[c].p_size == 0))
		fprintf(stderr, "Partition '%c' is not in use.\n", 'a' + c);
	else if (c == 2)
		fputs("You may not delete the 'c' partition.\n", stderr);
	else {
		/* Update free block count. */
		if (lp->d_partitions[c].p_fstype != FS_UNUSED &&
		    lp->d_partitions[c].p_fstype != FS_BOOT &&
		    lp->d_partitions[c].p_size != 0)
			*freep += lp->d_partitions[c].p_size;

		/* Really delete it (as opposed to just setting to "unused") */
		(void)memset(&lp->d_partitions[c], 0,
		    sizeof(lp->d_partitions[c]));

		/* Fill in the gap we just made */
		make_contiguous(lp);
	}
}

/*
 * Simplified display() for use with the builtin editor.
 */
void
editor_display(lp, freep, unit)
	struct disklabel *lp;
	u_int32_t *freep;
	char unit;
{
	int i;

	printf("device: %s\n", specname);
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
	printf("#        size   offset    fstype   [fsize bsize   cpg]\n");
	for (i = 0; i < lp->d_npartitions; i++)
		display_partition(stdout, lp, i, unit);
}

/*
 * Find the next reasonable starting offset and returns it.
 * Assumes there is a least one free block left (returns 0 if not).
 */
u_int32_t
next_offset(lp)
	struct disklabel *lp;
{
	struct partition **spp;
	u_int16_t npartitions;
	u_int32_t new_offset = 0;
	int i;

	/* How many "real" partitions do we have? */
	for (npartitions = 0, i = 0; i < lp->d_npartitions; i++) {
		if (lp->d_partitions[i].p_fstype != FS_UNUSED &&
		    lp->d_partitions[i].p_fstype != FS_BOOT &&
		    lp->d_partitions[i].p_size != 0)
			npartitions++;
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


	for (i = 0; i < npartitions; i++ ) {
		/*
		 * Is new_offset inside this partition?  If so,
		 * make it the next block after the partition ends.
		 */
		if (new_offset >= spp[i]->p_offset &&
		    new_offset < spp[i]->p_offset + spp[i]->p_size)
			new_offset = spp[i]->p_offset + spp[i]->p_size;
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

	if (p == NULL) {
		p = getstring(lp, "partition to change size",
		    "The letter of the partition to change size, a - p.", NULL);
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

	printf("Partition %c is currently %u sectors in size (%u free).\n",
	    partno + 'a', lp->d_partitions[partno].p_size, *freep);
	/* XXX - make maxsize lp->d_secperunit if FS_UNUSED/FS_BOOT? */
	newsize = getuint(lp, partno, "new size", "Size of the partition.  "
	    "You may also say +/- amount for a relative change.",
	    lp->d_partitions[partno].p_size,
	    lp->d_partitions[partno].p_size + *freep, DO_CONVERSIONS |
	    (lp->d_partitions[partno].p_fstype == FS_BSDFFS ? DO_ROUNDING : 0));
	if (newsize == UINT_MAX) {
		fputs("Invalid entry\n", stderr);
		return;
	} else if (newsize == lp->d_partitions[partno].p_size)
		return;

	if (lp->d_partitions[partno].p_fstype != FS_UNUSED &&
	    lp->d_partitions[partno].p_fstype != FS_BOOT) {
		if (newsize > lp->d_partitions[partno].p_size) {
			if (newsize - lp->d_partitions[partno].p_size > *freep) {
				fprintf(stderr,
				    "Only %u sectors free, you asked for %u\n",
				    *freep,
				    newsize - lp->d_partitions[partno].p_size);
				return;
			}
			*freep -= newsize - lp->d_partitions[partno].p_size;
		} else if (newsize < lp->d_partitions[partno].p_size) {
			*freep += lp->d_partitions[partno].p_size - newsize;
		}
	} else {
		if (partno != 2 && newsize + lp->d_partitions[partno].p_offset >
		    lp->d_partitions[2].p_size) {
			fputs("Partition must not extend past 'c'\n", stderr);
			return;
		} else if (partno == 2 && newsize +
		    lp->d_partitions[partno].p_offset > lp->d_secperunit) {
			fputs("'c' partition may not be larger than the disk\n",
			    stderr);
			return;
		}
	}
	lp->d_partitions[partno].p_size = newsize;
	make_contiguous(lp);
}

void
make_contiguous(lp)
	struct disklabel *lp;
{
	struct partition **spp;
	u_int16_t npartitions;
	int i;

	/* How many "real" partitions do we have? */
	for (npartitions = 0, i = 0; i < lp->d_npartitions; i++) {
		if (lp->d_partitions[i].p_fstype != FS_UNUSED &&
		    lp->d_partitions[i].p_fstype != FS_BOOT &&
		    lp->d_partitions[i].p_size != 0)
			npartitions++;
	}

	if (npartitions < 2)
		return;			/* nothing to do */

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
	if (heapsort((void *)spp, npartitions, sizeof(spp[0]), partition_cmp))
		err(4, "failed to sort partition table");

	/* Now make everything contiguous but don't muck with start of 'a' */
	for (i = 1; i < npartitions; i++)
		spp[i]->p_offset = spp[i - 1]->p_offset + spp[i - 1]->p_size;

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
			putchar('\n');
			buf[0] = '\0';
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
 * XXX - there are way too many parameters here.  Use inline helper functions
 */
u_int32_t
getuint(lp, partno, prompt, helpstring, oval, maxval, flags)
	struct disklabel *lp;
	int partno;
	char *prompt;
	char *helpstring;
	u_int32_t oval;
	u_int32_t maxval;		/* XXX - used inconsistently */
	int flags;
{
	char buf[BUFSIZ], *endptr, *p, operator = '\0';
	u_int32_t rval = oval;
	size_t n;
	int mult = 1;
	u_long ul;

	buf[0] = '\0';
	do {
		printf("%s: [%u] ", prompt, oval);
		fflush(stdout);
		rewind(stdin);
		if (fgets(buf, sizeof(buf), stdin) == NULL) {
			putchar('\n');
			buf[0] = '\0';
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
				switch (buf[n-1]) {

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
				}
			} else
				mult = 1;

			/* Did they give us an operator? */
			p = &buf[0];
			if (*p == '+' || *p == '-')
				operator = *p++;

			endptr = p;
			if (mult > 0)
				ul = strtoul(p, &endptr, 10) * mult;
			else
				/* Negative mult means divide (ooh, fancy) */
				ul = strtoul(p, &endptr, 10) / (-mult);

			/* Sanity check the return val */
			if (ul == ULONG_MAX || ul >= UINT_MAX) {
				errno = ERANGE;		/* too big */
				rval = UINT_MAX;
			} else if (*endptr != '\0') {
				errno = EINVAL;		/* non-numbers in str */
				rval = UINT_MAX;
			} else {
				rval = ul;

				/* Apply the operator (XXX - check for oflow) */
				if (operator == '+')
					rval += oval;
				else if (operator == '-')
					rval = oval - rval;
			}
		}
	}
	if ((flags & DO_ROUNDING) && rval < UINT_MAX) {
		u_int32_t cyls;
		/* XXX - should use maxsize and round down if too big */
#ifdef CYLCHECK
		/* Always round to nearest cylinder, regardless of units */
		cyls = (u_int32_t)((rval / (double)lp->d_secpercyl) + 0.5);
		if (rval != cyls * lp->d_secpercyl) {
			rval = cyls * lp->d_secpercyl;
			printf("Rounding to nearest cylinder: %u\n", rval);
		}
#else
		/* Round to nearest cylinder unless given in blocks */
		if (mult > 1) {
			cyls = (u_int32_t)((rval / (double)lp->d_secpercyl)
			    + 0.5);
			if (rval != cyls * lp->d_secpercyl) {
				rval = cyls * lp->d_secpercyl;
				printf("Rounding to nearest cylinder: %u\n",
				    rval);
			}
		}
#endif
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
	int c, i, j, rval = 0;

	/* How many "real" partitions do we have? */
	for (npartitions = 0, i = 0; i < lp->d_npartitions; i++) {
		if (lp->d_partitions[i].p_fstype != FS_UNUSED &&
		    lp->d_partitions[i].p_fstype != FS_BOOT &&
		    lp->d_partitions[i].p_size != 0)
			npartitions++;
	}

	if (npartitions < 2)
		return(0);			/* nothing to do */

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
	 */
	if (heapsort((void *)spp, npartitions, sizeof(spp[0]), partition_cmp))
		err(4, "failed to sort partition table");

	/* Now that we have things sorted by starting sector check overlap */
	for (i = 0; i < npartitions; i++) {
		for (j = i + 1; j < npartitions; j++) {
			/* `if last_sec_in_part + 1 > first_sec_in_next_part' */
			if (spp[i]->p_offset + spp[i]->p_size > spp[j]->p_offset) {
				/* Overlap!  Convert to real part numbers. */
				i = ((char *)spp[i] - (char *)lp->d_partitions)
				    / sizeof(**spp);
				j = ((char *)spp[j] - (char *)lp->d_partitions)
				    / sizeof(**spp);
				printf("\nError, partitions %c and %c overlap:\n",
				    'a' + i, 'a' + j);
				puts("         size   offset    fstype   [fsize bsize   cpg]");
				display_partition(stdout, lp, i, 0);
				display_partition(stdout, lp, j, 0);

				/* Did they ask us to resolve it ourselves? */
				if (!resolve) {
					(void)free(spp);
					return(1);
				}

				printf("Disable which one? [%c %c] ",
				    'a' + i, 'a' + j);
				/* XXX - make less gross */
				while ((c = getchar()) != EOF && c != '\n') {
					c -= 'a';
					if (c == i || c == j)
						break;
				}
				putchar('\n');
				/* Mark the selected one as unused or... */
				if (c == i || c == j) {
					lp->d_partitions[c].p_fstype = FS_UNUSED;
					*freep += lp->d_partitions[c].p_size;
				} else
					rval = 1;	/* still has overlap */
			    }
		}
	}

	(void)free(spp);
	return(rval);
}

void
edit_parms(lp, freep)
	struct disklabel *lp;
	u_int32_t *freep;
{
	char *p;
	u_int32_t ui;

	printf("Changing device parameters for %s\n\n", specname);

	/* pack/label id */
	p = getstring(lp, "label name",
	    "15 char string that describes this label, usually the disk name.",
	    lp->d_packname);
	strncpy(lp->d_packname, p, sizeof(lp->d_packname) - 1);
	lp->d_packname[sizeof(lp->d_packname) - 1] = '\0';

	/* sectors/track */
	for (;;) {
		ui = getuint(lp, 0, "sectors/track",
		    "The Numer of sectors per track.", lp->d_nsectors,
		    lp->d_nsectors, 0);
		if (ui == UINT_MAX)
			fputs("Invalid entry\n", stderr);
		else
			break;
	}
	lp->d_nsectors = ui;

	/* tracks/cylinder */
	for (;;) {
		ui = getuint(lp, 0, "tracks/cylinder",
		    "The number of tracks per cylinder.", lp->d_ntracks,
		    lp->d_ntracks, 0);
		if (ui == UINT_MAX)
			fputs("Invalid entry\n", stderr);
		else
			break;
	}
	lp->d_ntracks = ui;

	/* sectors/cylinder */
	lp->d_ncylinders = lp->d_ntracks * lp->d_nsectors;

	/* number of cylinders */
	for (;;) {
		ui = getuint(lp, 0, "number of cylinders",
		    "The total number of cylinders on the disk.",
		    lp->d_ncylinders, lp->d_ncylinders, 0);
		if (ui == UINT_MAX)
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
		    lp->d_ncylinders * lp->d_ncylinders, 0);
		if (ui == UINT_MAX)
			fputs("Invalid entry\n", stderr);
		else if (ui > lp->d_secperunit) {
			/* grow free count */
			*freep += ui - lp->d_secperunit;
			puts("You may want to increase the 'c' partition.");
			break;
		} else if (ui < lp->d_secperunit) {
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
	lp->d_secperunit = ui;

	/* rpm */
	for (;;) {
		ui = getuint(lp, 0, "rpm",
		  "The rotational speed of the disk in revoltuions per minute.",
		  lp->d_rpm, lp->d_rpm, 0);
		if (ui == UINT_MAX)
			fputs("Invalid entry\n", stderr);
		else
			break;
	}
	lp->d_rpm = ui;
}
