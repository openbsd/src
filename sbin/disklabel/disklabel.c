/*	$OpenBSD: disklabel.c,v 1.198 2015/01/20 18:22:20 deraadt Exp $	*/

/*
 * Copyright (c) 1987, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Symmetric Computer Systems.
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

#include <sys/param.h>	/* DEV_BSIZE */
#include <sys/ioctl.h>
#include <sys/dkio.h>
#include <sys/stat.h>
#include <sys/wait.h>
#define DKTYPENAMES
#include <sys/disklabel.h>

#include <ufs/ffs/fs.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <util.h>
#include <fstab.h>
#include "pathnames.h"
#include "extern.h"

/*
 * Disklabel: read and write disklabels.
 * The label is usually placed on one of the first sectors of the disk.
 * Many machines also place a bootstrap in the same area,
 * in which case the label is embedded in the bootstrap.
 * The bootstrap source must leave space at the proper offset
 * for the label on such machines.
 */

#ifndef BBSIZE
#define	BBSIZE	8192			/* size of boot area, with label */
#endif

#ifndef NUMBOOT
#define NUMBOOT 0
#endif

char	*dkname, *specname, *fstabfile;
char	tmpfil[] = _PATH_TMPFILE;
char	*mountpoints[MAXPARTITIONS];
struct	disklabel lab;
char	bootarea[BBSIZE];

#if NUMBOOT > 0
char	namebuf[BBSIZE], *np = namebuf;
int	installboot;	/* non-zero if we should install a boot program */
char	*bootbuf;	/* pointer to buffer with remainder of boot prog */
int	bootsize;	/* size of remaining boot program */
char	*xxboot;	/* primary boot */
char	boot0[MAXPATHLEN];
void	setbootflag(struct disklabel *);
#endif

enum {
	UNSPEC, EDIT, EDITOR, READ, RESTORE, WRITE, WRITEBOOT
} op = UNSPEC;

int	aflag;
int	cflag;
int	dflag;
int	tflag;
int	uidflag;
int	verbose;
int	donothing;
char	print_unit;

void	makedisktab(FILE *, struct disklabel *);
void	makelabel(char *, char *, struct disklabel *);
int	writelabel(int, char *, struct disklabel *);
void	l_perror(char *);
int	edit(struct disklabel *, int);
int	editit(const char *);
char	*skip(char *);
char	*word(char *);
int	getasciilabel(FILE *, struct disklabel *);
int	cmplabel(struct disklabel *, struct disklabel *);
void	usage(void);
u_int64_t getnum(char *, u_int64_t, u_int64_t, const char **);

int
main(int argc, char *argv[])
{
	int ch, f, error = 0;
	struct disklabel *lp;
	FILE *t;

	while ((ch = getopt(argc, argv, "ABEf:F:hRb:cdenp:tvw")) != -1)
		switch (ch) {
		case 'A':
			++aflag;
			break;
#if NUMBOOT > 0
		case 'B':
			++installboot;
			break;
		case 'b':
			xxboot = optarg;
			break;
#endif
		case 'R':
			if (op != UNSPEC)
				usage();
			op = RESTORE;
			break;
		case 'c':
			++cflag;
			break;
		case 'd':
			++dflag;
			break;
		case 'e':
			if (op != UNSPEC)
				usage();
			op = EDIT;
			break;
		case 'E':
			if (op != UNSPEC)
				usage();
			op = EDITOR;
			break;
		case 'f':
			fstabfile = optarg;
			uidflag = 0;
			break;
		case 'F':
			fstabfile = optarg;
			++uidflag;
			break;
		case 'h':
			print_unit = '*';
			break;
		case 't':
			++tflag;
			break;
		case 'w':
			if (op != UNSPEC)
				usage();
			op = WRITE;
			break;
		case 'p':
			if (strchr("bckmgtBCKMGT", optarg[0]) == NULL ||
			    optarg[1] != '\0') {
				fprintf(stderr, "Valid units are bckmgt\n");
				exit(1);
			}
			print_unit = tolower((unsigned char)optarg[0]);
			break;
		case 'n':
			donothing++;
			break;
		case 'v':
			verbose++;
			break;
		case '?':
		default:
			usage();
	}
	argc -= optind;
	argv += optind;

#if NUMBOOT > 0
	if (installboot) {
		if (op == UNSPEC)
			op = WRITEBOOT;
	} else {
		if (op == UNSPEC)
			op = READ;
	}
#else
	if (op == UNSPEC)
		op = READ;
#endif

	if (argc < 1 || (fstabfile && !(op == EDITOR || op == RESTORE ||
		    aflag)))
		usage();

	dkname = argv[0];
	f = opendev(dkname, (op == READ ? O_RDONLY : O_RDWR), OPENDEV_PART,
	    &specname);
	if (f < 0)
		err(4, "%s", specname);

	switch (op) {
	case EDIT:
		if (argc != 1)
			usage();
		readlabel(f);
		error = edit(&lab, f);
		break;
	case EDITOR:
		if (argc != 1)
			usage();
		readlabel(f);
		error = editor(f);
		break;
	case READ:
		if (argc != 1)
			usage();
		readlabel(f);
		if (tflag)
			makedisktab(stdout, &lab);
		else
			display(stdout, &lab, print_unit, 1);
		error = checklabel(&lab);
		break;
	case RESTORE:
		if (argc < 2 || argc > 3)
			usage();
		readlabel(f);
#if NUMBOOT > 0
		if (installboot && argc == 3)
			makelabel(argv[2], NULL, &lab);
#endif
		lp = makebootarea(bootarea, &lab);
		*lp = lab;
		if (!(t = fopen(argv[1], "r")))
			err(4, "%s", argv[1]);
		error = getasciilabel(t, lp);
		bzero(lp->d_uid, sizeof(lp->d_uid));
		if (error == 0) {
			error = writelabel(f, bootarea, lp);
			if (error == 0) {
				if (ioctl(f, DIOCGDINFO, &lab) < 0)
					err(4, "ioctl DIOCGDINFO");
				mpsave(&lab);
			}
		}
		fclose(t);
		break;
	case WRITE:
		if (dflag || aflag) {
			readlabel(f);
		} else if (argc < 2 || argc > 3)
			usage();
		else
			makelabel(argv[1], argc == 3 ? argv[2] : NULL, &lab);
		lp = makebootarea(bootarea, &lab);
		*lp = lab;
		error = checklabel(&lab);
		if (error == 0)
			error = writelabel(f, bootarea, lp);
		break;
#if NUMBOOT > 0
	case WRITEBOOT:
	{
		struct disklabel tlab;

		readlabel(f);
		tlab = lab;
		if (argc == 2)
			makelabel(argv[1], NULL, &lab);
		lp = makebootarea(bootarea, &lab);
		*lp = tlab;
		error = checklabel(&lab);
		if (error == 0)
			error = writelabel(f, bootarea, lp);
		break;
	}
#endif
	default:
		break;
	}
	exit(error);
}

/*
 * Construct a prototype disklabel from /etc/disktab.  As a side
 * effect, set the names of the primary and secondary boot files
 * if specified.
 */
void
makelabel(char *type, char *name, struct disklabel *lp)
{
	struct disklabel *dp;

	dp = getdiskbyname(type);
	if (dp == NULL)
		errx(1, "unknown disk type: %s", type);
	*lp = *dp;
#if NUMBOOT > 0
	/*
	 * Set bootstrap name(s).
	 * 1. If set from command line, use those,
	 * 2. otherwise, check if disktab specifies them (b0 or b1),
	 * 3. otherwise, makebootarea() will choose ones based on the name
	 *    of the disk special file. E.g. /dev/ra0 -> raboot, bootra
	 */
	if (!xxboot && lp->d_boot0) {
		if (*lp->d_boot0 != '/')
			(void)snprintf(boot0, sizeof boot0, "%s%s",
			    _PATH_BOOTDIR, lp->d_boot0);
		else
			(void)strlcpy(boot0, lp->d_boot0, sizeof boot0);
		xxboot = boot0;
	}
#endif
	/* d_packname is union d_boot[01], so zero */
	memset(lp->d_packname, 0, sizeof(lp->d_packname));
	if (name)
		(void)strncpy(lp->d_packname, name, sizeof(lp->d_packname));
}


int
writelabel(int f, char *boot, struct disklabel *lp)
{
#if NUMBOOT > 0
	setbootflag(lp);
#endif
	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = 0;
	lp->d_checksum = dkcksum(lp);
#if NUMBOOT > 0
	if (installboot) {
		/*
		 * First set the kernel disk label,
		 * then write a label to the raw disk.
		 * If the SDINFO ioctl fails because it is unimplemented,
		 * keep going; otherwise, the kernel consistency checks
		 * may prevent us from changing the current (in-core)
		 * label.
		 */
		if (!donothing) {
			if (ioctl(f, DIOCSDINFO, lp) < 0 &&
			    errno != ENODEV && errno != ENOTTY) {
				l_perror("ioctl DIOCSDINFO");
				return (1);
			}
		}
		if (!donothing) {
			if (lseek(f, 0, SEEK_SET) < 0) {
				perror("lseek");
				return (1);
			}
			if (write(f, boot, lp->d_bbsize) != lp->d_bbsize) {
				perror("write");
				return (1);
			}
		}
		/*
		 * Output the remainder of the disklabel
		 */
		if (!donothing && bootbuf && write(f, bootbuf, bootsize) != bootsize) {
			perror("write");
			return(1);
		}
	} else
#endif /* NUMBOOT > 0 */
	if (!donothing) {
		if (ioctl(f, DIOCWDINFO, lp) < 0) {
			l_perror("ioctl DIOCWDINFO");
			return (1);
		}
	}
#ifdef __vax__
	if (lp->d_type == DTYPE_SMD && lp->d_flags & D_BADSECT) {
		off_t alt;
		int i;

		alt = lp->d_ncylinders * lp->d_secpercyl - lp->d_nsectors;
		for (i = 1; i < 11 && i < lp->d_nsectors; i += 2) {
			(void)lseek(f, (alt + i) * lp->d_secsize, SEEK_SET);
			if (!donothing)
				if (write(f, boot, lp->d_secsize) != lp->d_secsize)
					warn("alternate label %d write", i/2);
		}
	}
#endif
	/* Finally, write out any mount point information. */
	if (!donothing) {
		/* First refresh our copy of the current label to get UID. */
		if (ioctl(f, DIOCGDINFO, &lab) < 0)
			err(4, "ioctl DIOCGDINFO");
		mpsave(lp);
	}

	return (0);
}

void
l_perror(char *s)
{

	switch (errno) {
	case ESRCH:
		warnx("%s: No disk label on disk", s);
		break;
	case EINVAL:
		warnx("%s: Label magic number or checksum is wrong!\n"
		    "(disklabel or kernel is out of date?)", s);
		break;
	case EBUSY:
		warnx("%s: Open partition would move or shrink", s);
		break;
	case EXDEV:
		warnx("%s: Labeled partition or 'a' partition must start "
		    "at beginning of disk", s);
		break;
	default:
		warn("%s", s);
		break;
	}
}

/*
 * Fetch requested disklabel into 'lab' using ioctl.
 */
void
readlabel(int f)
{
	char *partname, *partduid;
	struct fstab *fsent;
	int i;

	if (cflag && ioctl(f, DIOCRLDINFO) < 0)
		err(4, "ioctl DIOCRLDINFO");

	if ((op == RESTORE) || dflag || aflag) {
		if (ioctl(f, DIOCGPDINFO, &lab) < 0)
			err(4, "ioctl DIOCGPDINFO");
	} else {
		if (ioctl(f, DIOCGDINFO, &lab) < 0)
			err(4, "ioctl DIOCGDINFO");
	}

	asprintf(&partname, "/dev/%s%c", dkname, 'a');
	asprintf(&partduid,
	    "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx.a",
            lab.d_uid[0], lab.d_uid[1], lab.d_uid[2], lab.d_uid[3],
            lab.d_uid[4], lab.d_uid[5], lab.d_uid[6], lab.d_uid[7]);
	setfsent();
	for (i = 0; i < MAXPARTITIONS; i++) {
		partname[strlen(dkname) + 5] = 'a' + i;
		partduid[strlen(partduid) - 1] = 'a' + i;
		fsent = getfsspec(partname);
		if (fsent == NULL)
			fsent = getfsspec(partduid);
		if (fsent)
			mountpoints[i] = strdup(fsent->fs_file);
	}
	endfsent();
	free(partduid);
	free(partname);

	if (aflag)
		editor_allocspace(&lab);
}

/*
 * Construct a bootarea (d_bbsize bytes) in the specified buffer ``boot''
 * Returns a pointer to the disklabel portion of the bootarea.
 */
struct disklabel *
makebootarea(char *boot, struct disklabel *dp)
{
	struct disklabel *lp;
	char *p;
#if NUMBOOT > 0
	char *dkbasename;
	int b;
	struct stat sb;
#endif

	/* XXX */
	if (dp->d_secsize == 0) {
		dp->d_secsize = DEV_BSIZE;
		dp->d_bbsize = BBSIZE;
	}
	lp = (struct disklabel *)
	    (boot + (LABELSECTOR * dp->d_secsize) + LABELOFFSET);
	memset(lp, 0, sizeof *lp);
#if NUMBOOT > 0
	/*
	 * If we are not installing a boot program but we are installing a
	 * label on disk then we must read the current bootarea so we don't
	 * clobber the existing boot.
	 */
	if (!installboot)
		return (lp);
	/*
	 * We are installing a boot program.  Determine the name(s) and
	 * read them into the appropriate places in the boot area.
	 */
	if (!xxboot) {
		dkbasename = np;
		if ((p = strrchr(dkname, '/')) == NULL)
			p = dkname;
		else
			p++;
		while (*p && !isdigit((unsigned char)*p))
			*np++ = *p++;
		*np++ = '\0';

		if (!xxboot) {
			(void)snprintf(np, namebuf + sizeof namebuf - np,
			    "%s%sboot", _PATH_BOOTDIR, dkbasename);
			if (access(np, F_OK) < 0 && dkbasename[0] == 'r')
				dkbasename++;
			xxboot = np;
			(void)snprintf(xxboot,
			    namebuf + sizeof namebuf - np,
			    "%s%sboot", _PATH_BOOTDIR, dkbasename);
			np += strlen(xxboot) + 1;
		}
	}
	if (verbose)
		warnx("bootstrap: xxboot = %s", xxboot);

	/*
	 * For NUMBOOT > 0 architectures (hppa/hppa64/landisk/vax)
	 * up to d_bbsize bytes of ``xxboot'' go in bootarea, the rest
	 * is remembered and written later following the bootarea.
	 */
	b = open(xxboot, O_RDONLY);
	if (b < 0)
		err(4, "%s", xxboot);
	if (read(b, boot, (int)dp->d_bbsize) < 0)
		err(4, "%s", xxboot);
	(void)fstat(b, &sb);
	bootsize = (int)sb.st_size - dp->d_bbsize;
	if (bootsize > 0) {
		/* XXX assume d_secsize is a power of two */
		bootsize = (bootsize + dp->d_secsize-1) & ~(dp->d_secsize-1);
		bootbuf = (char *)malloc((size_t)bootsize);
		if (bootbuf == NULL)
			err(4, "%s", xxboot);
		if (read(b, bootbuf, bootsize) < 0) {
			free(bootbuf);
			err(4, "%s", xxboot);
		}
	}
	(void)close(b);
#endif
	/*
	 * Make sure no part of the bootstrap is written in the area
	 * reserved for the label.
	 */
	for (p = (char *)lp; p < (char *)lp + sizeof(struct disklabel); p++)
		if (*p)
			errx(2, "Bootstrap doesn't leave room for disk label");
	return (lp);
}

void
makedisktab(FILE *f, struct disklabel *lp)
{
	int i;
	struct partition *pp;

	if (lp->d_packname[0])
		(void)fprintf(f, "%.*s|", (int)sizeof(lp->d_packname),
		    lp->d_packname);
	if (lp->d_typename[0])
		(void)fprintf(f, "%.*s|", (int)sizeof(lp->d_typename),
		    lp->d_typename);
	(void)fputs("Automatically generated label:\\\n\t:dt=", f);
	if (lp->d_type < DKMAXTYPES)
		(void)fprintf(f, "%s:", dktypenames[lp->d_type]);
	else
		(void)fprintf(f, "unknown%d:", lp->d_type);

	(void)fprintf(f, "se#%u:", lp->d_secsize);
	(void)fprintf(f, "ns#%u:", lp->d_nsectors);
	(void)fprintf(f, "nt#%u:", lp->d_ntracks);
	(void)fprintf(f, "nc#%u:", lp->d_ncylinders);
	(void)fprintf(f, "sc#%u:", lp->d_secpercyl);
	(void)fprintf(f, "su#%llu:", DL_GETDSIZE(lp));

	/*
	 * XXX We do not print have disktab information yet for
	 * XXX DL_GETBSTART DL_GETBEND
	 */
	for (i = 0; i < NDDATA; i++)
		if (lp->d_drivedata[i])
			(void)fprintf(f, "d%d#%u", i, lp->d_drivedata[i]);
	pp = lp->d_partitions;
	for (i = 0; i < lp->d_npartitions; i++, pp++) {
		if (DL_GETPSIZE(pp)) {
			char c = 'a' + i;

			(void)fprintf(f, "\\\n\t:");
			(void)fprintf(f, "p%c#%llu:", c, DL_GETPSIZE(pp));
			(void)fprintf(f, "o%c#%llu:", c, DL_GETPOFFSET(pp));
			if (pp->p_fstype != FS_UNUSED) {
				if (pp->p_fstype < FSMAXTYPES)
					(void)fprintf(f, "t%c=%s:", c,
					    fstypenames[pp->p_fstype]);
				else
					(void)fprintf(f, "t%c=unknown%d:",
					    c, pp->p_fstype);
			}
			switch (pp->p_fstype) {

			case FS_UNUSED:
				break;

			case FS_BSDFFS:
				(void)fprintf(f, "b%c#%u:", c,
				    DISKLABELV1_FFS_BSIZE(pp->p_fragblock));
				(void)fprintf(f, "f%c#%u:", c,
				    DISKLABELV1_FFS_FSIZE(pp->p_fragblock));
				break;

			default:
				break;
			}
		}
	}
	(void)fputc('\n', f);
	(void)fflush(f);
}

double
scale(u_int64_t sz, char unit, struct disklabel *lp)
{
	double fsz;

	fsz = (double)sz * lp->d_secsize;

	switch (unit) {
	case 'B':
		return fsz;
	case 'C':
		return fsz / lp->d_secsize / lp->d_secpercyl;
	case 'K':
		return fsz / 1024;
	case 'M':
		return fsz / (1024 * 1024);
	case 'G':
		return fsz / (1024 * 1024 * 1024);
	case 'T':
		return fsz / (1024ULL * 1024 * 1024 * 1024);
	default:
		return -1.0;
	}
}

/*
 * Display a particular partition.
 */
void
display_partition(FILE *f, struct disklabel *lp, int i, char unit)
{
	volatile struct partition *pp = &lp->d_partitions[i];
	double p_size;

	p_size = scale(DL_GETPSIZE(pp), unit, lp);
	if (DL_GETPSIZE(pp)) {
		u_int32_t frag = DISKLABELV1_FFS_FRAG(pp->p_fragblock);
		u_int32_t fsize = DISKLABELV1_FFS_FSIZE(pp->p_fragblock);

		if (p_size < 0)
			fprintf(f, "  %c: %16llu %16llu ", 'a' + i,
			    DL_GETPSIZE(pp), DL_GETPOFFSET(pp));
		else
			fprintf(f, "  %c: %15.*f%c %16llu ", 'a' + i,
			    unit == 'B' ? 0 : 1, p_size, unit,
			    DL_GETPOFFSET(pp));
		if (pp->p_fstype < FSMAXTYPES)
			fprintf(f, "%7.7s", fstypenames[pp->p_fstype]);
		else
			fprintf(f, "%7d", pp->p_fstype);

		switch (pp->p_fstype) {
		case FS_BSDFFS:
			fprintf(f, "  %5u %5u %4hu ",
			    fsize, fsize * frag,
			    pp->p_cpg);
			break;
		default:
			fprintf(f, "%19.19s", "");
			break;
		}

		if (mountpoints[i] != NULL)
			fprintf(f, "# %s", mountpoints[i]);
		putc('\n', f);
	}
}

char
canonical_unit(struct disklabel *lp, char unit)
{
	struct partition *pp;
	u_int64_t small;
	int i;

	if (unit == '*') {
		small = DL_GETDSIZE(lp);
		pp = &lp->d_partitions[0];
		for (i = 0; i < lp->d_npartitions; i++, pp++)
			if (DL_GETPSIZE(pp) > 0 && DL_GETPSIZE(pp) < small)
				small = DL_GETPSIZE(pp);
		if (small < DL_BLKTOSEC(lp, MEG(1)))
			unit = 'K';
		else if (small < DL_BLKTOSEC(lp, MEG(1024)))
			unit = 'M';
		else if (small < DL_BLKTOSEC(lp, GIG(1024)))
			unit = 'G';
		else
			unit = 'T';
	}
	unit = toupper((unsigned char)unit);

	return (unit);
}

void
display(FILE *f, struct disklabel *lp, char unit, int all)
{
	int i, j;
	double d;

	unit = canonical_unit(lp, unit);

	fprintf(f, "# %s:\n", specname);

	if (lp->d_type < DKMAXTYPES)
		fprintf(f, "type: %s\n", dktypenames[lp->d_type]);
	else
		fprintf(f, "type: %d\n", lp->d_type);
	fprintf(f, "disk: %.*s\n", (int)sizeof(lp->d_typename),
	    lp->d_typename);
	fprintf(f, "label: %.*s\n", (int)sizeof(lp->d_packname),
	    lp->d_packname);
	fprintf(f, "duid: %02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n",
            lp->d_uid[0], lp->d_uid[1], lp->d_uid[2], lp->d_uid[3],
            lp->d_uid[4], lp->d_uid[5], lp->d_uid[6], lp->d_uid[7]);
	fprintf(f, "flags:");
	if (lp->d_flags & D_BADSECT)
		fprintf(f, " badsect");
	if (lp->d_flags & D_VENDOR)
		fprintf(f, " vendor");
	putc('\n', f);

	fprintf(f, "bytes/sector: %u\n", lp->d_secsize);
	fprintf(f, "sectors/track: %u\n", lp->d_nsectors);
	fprintf(f, "tracks/cylinder: %u\n", lp->d_ntracks);
	fprintf(f, "sectors/cylinder: %u\n", lp->d_secpercyl);
	fprintf(f, "cylinders: %u\n", lp->d_ncylinders);
	fprintf(f, "total sectors: %llu", DL_GETDSIZE(lp));
	d = scale(DL_GETDSIZE(lp), unit, lp);
	if (d > 0)
		fprintf(f, " # total bytes: %.*f%c", unit == 'B' ? 0 : 1,
		    d, unit);
	fprintf(f, "\n");

	fprintf(f, "boundstart: %llu\n", DL_GETBSTART(lp));
	fprintf(f, "boundend: %llu\n", DL_GETBEND(lp));
	fprintf(f, "drivedata: ");
	for (i = NDDATA - 1; i >= 0; i--)
		if (lp->d_drivedata[i])
			break;
	if (i < 0)
		i = 0;
	for (j = 0; j <= i; j++)
		fprintf(f, "%d ", lp->d_drivedata[j]);
	fprintf(f, "\n");
	if (all) {
		fprintf(f, "\n%hu partitions:\n", lp->d_npartitions);
		fprintf(f, "#    %16.16s %16.16s  fstype [fsize bsize  cpg]\n",
		    "size", "offset");
		for (i = 0; i < lp->d_npartitions; i++)
			display_partition(f, lp, i, unit);
	}
	fflush(f);
}

int
edit(struct disklabel *lp, int f)
{
	int first, ch, fd, error = 0;
	struct disklabel label;
	FILE *fp;
	u_int64_t total_sectors, starting_sector, ending_sector;

	if ((fd = mkstemp(tmpfil)) == -1 || (fp = fdopen(fd, "w")) == NULL) {
		warn("%s", tmpfil);
		if (fd != -1)
			close(fd);
		return (1);
	}
	display(fp, lp, 0, 1);
	fprintf(fp, "\n# Notes:\n");
	fprintf(fp,
"# Up to 16 partitions are valid, named from 'a' to 'p'.  Partition 'a' is\n"
"# your root filesystem, 'b' is your swap, and 'c' should cover your whole\n"
"# disk. Any other partition is free for any use.  'size' and 'offset' are\n"
"# in 512-byte blocks. fstype should be '4.2BSD', 'swap', or 'none' or some\n"
"# other values.  fsize/bsize/cpg should typically be '2048 16384 16' for a\n"
"# 4.2BSD filesystem (or '512 4096 16' except on alpha, sun4, ...)\n");
	fclose(fp);
	for (;;) {
		if (editit(tmpfil) == -1)
			break;
		fp = fopen(tmpfil, "r");
		if (fp == NULL) {
			warn("%s", tmpfil);
			break;
		}
		/* Get values set by OS and not the label. */
		if (ioctl(f, DIOCGPDINFO, &label) < 0)
			err(4, "ioctl DIOCGPDINFO");
		ending_sector = DL_GETBEND(&label);
		starting_sector = DL_GETBSTART(&label);
		total_sectors = DL_GETDSIZE(&label);
		error = getasciilabel(fp, &label);
		DL_SETBEND(&label, ending_sector);
		DL_SETBSTART(&label, starting_sector);
		DL_SETDSIZE(&label, total_sectors);

		if (error == 0) {
			if (cmplabel(lp, &label) == 0) {
				puts("No changes.");
				fclose(fp);
				(void) unlink(tmpfil);
				return (0);
			}
			*lp = label;
			if (writelabel(f, bootarea, lp) == 0) {
				fclose(fp);
				(void) unlink(tmpfil);
				return (0);
			}
		}
		fclose(fp);
		printf("re-edit the label? [y]: ");
		fflush(stdout);
		first = ch = getchar();
		while (ch != '\n' && ch != EOF)
			ch = getchar();
		if (first == 'n' || first == 'N')
			break;
	}
	(void)unlink(tmpfil);
	return (1);
}

/*
 * Execute an editor on the specified pathname, which is interpreted
 * from the shell.  This means flags may be included.
 *
 * Returns -1 on error, or the exit value on success.
 */
int
editit(const char *pathname)
{
	char *argp[] = {"sh", "-c", NULL, NULL}, *ed, *p;
	sig_t sighup, sigint, sigquit, sigchld;
	pid_t pid;
	int saved_errno, st, ret = -1;

	ed = getenv("VISUAL");
	if (ed == NULL || ed[0] == '\0')
		ed = getenv("EDITOR");
	if (ed == NULL || ed[0] == '\0')
		ed = _PATH_VI;
	if (asprintf(&p, "%s %s", ed, pathname) == -1)
		return (-1);
	argp[2] = p;

	sighup = signal(SIGHUP, SIG_IGN);
	sigint = signal(SIGINT, SIG_IGN);
	sigquit = signal(SIGQUIT, SIG_IGN);
	sigchld = signal(SIGCHLD, SIG_DFL);
	if ((pid = fork()) == -1)
		goto fail;
	if (pid == 0) {
		execv(_PATH_BSHELL, argp);
		_exit(127);
	}
	while (waitpid(pid, &st, 0) == -1)
		if (errno != EINTR)
			goto fail;
	if (!WIFEXITED(st))
		errno = EINTR;
	else
		ret = WEXITSTATUS(st);

 fail:
	saved_errno = errno;
	(void)signal(SIGHUP, sighup);
	(void)signal(SIGINT, sigint);
	(void)signal(SIGQUIT, sigquit);
	(void)signal(SIGCHLD, sigchld);
	free(p);
	errno = saved_errno;
	return (ret);
}

char *
skip(char *cp)
{

	cp += strspn(cp, " \t");
	if (*cp == '\0')
		return (NULL);
	return (cp);
}

char *
word(char *cp)
{

	cp += strcspn(cp, " \t");
	if (*cp == '\0')
		return (NULL);
	*cp++ = '\0';
	cp += strspn(cp, " \t");
	if (*cp == '\0')
		return (NULL);
	return (cp);
}

/* Base the max value on the sizeof of the value we are reading */
#define GETNUM(field, nptr, min, errstr)				\
	    getnum((nptr), (min),					\
		sizeof(field) == 8 ? LLONG_MAX :			\
		(sizeof(field) == 4 ? UINT_MAX :			\
		(sizeof(field) == 2 ? USHRT_MAX : UCHAR_MAX)),  (errstr))

u_int64_t
getnum(char *nptr, u_int64_t min, u_int64_t max, const char **errstr)
{
	char *p, c;
	u_int64_t ret;

	for (p = nptr; *p != '\0' && !isspace((unsigned char)*p); p++)
		;
	c = *p;
	*p = '\0';
	ret = strtonum(nptr, min, max, errstr);
	*p = c;
	return (ret);
}

int
duid_parse(struct disklabel *lp, char *s)
{
	u_char duid[8];
	char c;
	int i;

	if (strlen(s) != 16)
		return -1;

	memset(duid, 0, sizeof(duid));
	for (i = 0; i < 16; i++) {
		c = s[i];
		if (c >= '0' && c <= '9')
			c -= '0';
		else if (c >= 'a' && c <= 'f')
			c -= ('a' - 10);
		else if (c >= 'A' && c <= 'F')
			c -= ('A' - 10);
		else
			return -1;
		duid[i / 2] <<= 4;
		duid[i / 2] |= c & 0xf;
	}

	memcpy(lp->d_uid, &duid, sizeof(lp->d_uid));
	return 0;
}

/*
 * Read an ascii label in from FILE f,
 * in the same format as that put out by display(),
 * and fill in lp.
 */
int
getasciilabel(FILE *f, struct disklabel *lp)
{
	char **cpp, *cp;
	const char *errstr;
	struct partition *pp;
	char *mp, *tp, *s, line[BUFSIZ];
	char **omountpoints = NULL;
	int lineno = 0, errors = 0;
	u_int32_t v, fsize;
	u_int64_t lv;
	unsigned int part;

	lp->d_version = 1;
	lp->d_bbsize = BBSIZE;				/* XXX */
	lp->d_sbsize = SBSIZE;				/* XXX */

	if (!(omountpoints = calloc(MAXPARTITIONS, sizeof(char *))))
		errx(4, "out of memory");

	mpcopy(omountpoints, mountpoints);
	for (part = 0; part < MAXPARTITIONS; part++) {
		free(mountpoints[part]);
		mountpoints[part] = NULL;
	}
	
	while (fgets(line, sizeof(line), f)) {
		lineno++;
		mp = NULL;
		if ((cp = strpbrk(line, "\r\n")))
			*cp = '\0';
		if ((cp = strpbrk(line, "#"))) {
			*cp = '\0';
			mp = skip(cp+1);
			if (mp && *mp != '/')
				mp = NULL;
		}
		cp = skip(line);
		if (cp == NULL)
			continue;
		tp = strchr(cp, ':');
		if (tp == NULL) {
			warnx("line %d: syntax error", lineno);
			errors++;
			continue;
		}
		*tp++ = '\0', tp = skip(tp);
		if (!strcmp(cp, "type")) {
			if (tp == NULL)
				tp = "unknown";
			else if (strcasecmp(tp, "IDE") == 0)
				tp = "ESDI";
			cpp = dktypenames;
			for (; cpp < &dktypenames[DKMAXTYPES]; cpp++)
				if ((s = *cpp) && !strcasecmp(s, tp)) {
					lp->d_type = cpp - dktypenames;
					goto next;
				}
			v = GETNUM(lp->d_type, tp, 0, &errstr);
			if (errstr || v >= DKMAXTYPES)
				warnx("line %d: warning, unknown disk type: %s",
				    lineno, tp);
			lp->d_type = v;
			continue;
		}
		if (!strcmp(cp, "flags")) {
			for (v = 0; (cp = tp) && *cp != '\0';) {
				tp = word(cp);
				if (!strcmp(cp, "badsect"))
					v |= D_BADSECT;
				else if (!strcmp(cp, "vendor"))
					v |= D_VENDOR;
				else {
					warnx("line %d: bad flag: %s",
					    lineno, cp);
					errors++;
				}
			}
			lp->d_flags = v;
			continue;
		}
		if (!strcmp(cp, "drivedata")) {
			int i;

			for (i = 0; (cp = tp) && *cp != '\0' && i < NDDATA;) {
				v = GETNUM(lp->d_drivedata[i], cp, 0, &errstr);
				if (errstr)
					warnx("line %d: bad drivedata %s",
					   lineno, cp);
				lp->d_drivedata[i++] = v;
				tp = word(cp);
			}
			continue;
		}
		if (sscanf(cp, "%d partitions", &v) == 1) {
			if (v == 0 || v > MAXPARTITIONS) {
				warnx("line %d: bad # of partitions", lineno);
				lp->d_npartitions = MAXPARTITIONS;
				errors++;
			} else
				lp->d_npartitions = v;
			continue;
		}
		if (tp == NULL)
			tp = "";
		if (!strcmp(cp, "disk")) {
			strncpy(lp->d_typename, tp, sizeof (lp->d_typename));
			continue;
		}
		if (!strcmp(cp, "label")) {
			strncpy(lp->d_packname, tp, sizeof (lp->d_packname));
			continue;
		}
		if (!strcmp(cp, "duid")) {
			if (duid_parse(lp, tp) != 0) {
				warnx("line %d: bad %s: %s", lineno, cp, tp);
				errors++;
			}
			continue;
		}
		if (!strcmp(cp, "bytes/sector")) {
			v = GETNUM(lp->d_secsize, tp, 1, &errstr);
			if (errstr || (v % 512) != 0) {
				warnx("line %d: bad %s: %s", lineno, cp, tp);
				errors++;
			} else
				lp->d_secsize = v;
			continue;
		}
		if (!strcmp(cp, "sectors/track")) {
			v = GETNUM(lp->d_nsectors, tp, 1, &errstr);
			if (errstr) {
				warnx("line %d: bad %s: %s", lineno, cp, tp);
				errors++;
			} else
				lp->d_nsectors = v;
			continue;
		}
		if (!strcmp(cp, "sectors/cylinder")) {
			v = GETNUM(lp->d_secpercyl, tp, 1, &errstr);
			if (errstr) {
				warnx("line %d: bad %s: %s", lineno, cp, tp);
				errors++;
			} else
				lp->d_secpercyl = v;
			continue;
		}
		if (!strcmp(cp, "tracks/cylinder")) {
			v = GETNUM(lp->d_ntracks, tp, 1, &errstr);
			if (errstr) {
				warnx("line %d: bad %s: %s", lineno, cp, tp);
				errors++;
			} else
				lp->d_ntracks = v;
			continue;
		}
		if (!strcmp(cp, "cylinders")) {
			v = GETNUM(lp->d_ncylinders, tp, 1, &errstr);
			if (errstr) {
				warnx("line %d: bad %s: %s", lineno, cp, tp);
				errors++;
			} else
				lp->d_ncylinders = v;
			continue;
		}

		/* Ignore fields that are no longer in the disklabel. */
		if (!strcmp(cp, "rpm") ||
		    !strcmp(cp, "interleave") ||
		    !strcmp(cp, "trackskew") ||
		    !strcmp(cp, "cylinderskew") ||
		    !strcmp(cp, "headswitch") ||
		    !strcmp(cp, "track-to-track seek"))
			continue;

		/* Ignore fields that are forcibly set when label is read. */
		if (!strcmp(cp, "total sectors") ||
		    !strcmp(cp, "boundstart") ||
		    !strcmp(cp, "boundend"))
			continue;

		if ('a' <= *cp && *cp <= 'z' && cp[1] == '\0') {
			unsigned int part = *cp - 'a';

			if (part >= lp->d_npartitions) {
				if (part >= MAXPARTITIONS) {
					warnx("line %d: bad partition name: %s",
					    lineno, cp);
					errors++;
					continue;
				} else {
					lp->d_npartitions = part + 1;
				}
			}
			pp = &lp->d_partitions[part];
#define NXTNUM(n, field, errstr) { \
	if (tp == NULL) {					\
		warnx("line %d: too few fields", lineno);	\
		errors++;					\
		break;						\
	} else							\
		cp = tp, tp = word(cp), (n) = GETNUM(field, cp, 0, errstr); \
}
			NXTNUM(lv, lv, &errstr);
			if (errstr) {
				warnx("line %d: bad partition size: %s",
				    lineno, cp);
				errors++;
			} else {
				DL_SETPSIZE(pp, lv);
			}
			NXTNUM(lv, lv, &errstr);
			if (errstr) {
				warnx("line %d: bad partition offset: %s",
				    lineno, cp);
				errors++;
			} else {
				DL_SETPOFFSET(pp, lv);
			}
			if (tp == NULL) {
				pp->p_fstype = FS_UNUSED;
				goto gottype;
			}
			cp = tp, tp = word(cp);
			cpp = fstypenames;
			for (; cpp < &fstypenames[FSMAXTYPES]; cpp++)
				if ((s = *cpp) && !strcasecmp(s, cp)) {
					pp->p_fstype = cpp - fstypenames;
					goto gottype;
				}
			if (isdigit((unsigned char)*cp))
				v = GETNUM(pp->p_fstype, cp, 0, &errstr);
			else
				v = FSMAXTYPES;
			if (errstr || v >= FSMAXTYPES) {
				warnx("line %d: warning, unknown filesystem type: %s",
				    lineno, cp);
				v = FS_UNUSED;
			}
			pp->p_fstype = v;
	gottype:
			switch (pp->p_fstype) {

			case FS_UNUSED:				/* XXX */
				if (tp == NULL)	/* ok to skip fsize/bsize */
					break;
				NXTNUM(fsize, fsize, &errstr);
				if (fsize == 0)
					break;
				NXTNUM(v, v, &errstr);
				pp->p_fragblock =
				    DISKLABELV1_FFS_FRAGBLOCK(fsize, v / fsize);
				break;

			case FS_BSDFFS:
				NXTNUM(fsize, fsize, &errstr);
				if (fsize == 0)
					break;
				NXTNUM(v, v, &errstr);
				pp->p_fragblock =
				    DISKLABELV1_FFS_FRAGBLOCK(fsize, v / fsize);
				NXTNUM(pp->p_cpg, pp->p_cpg, &errstr);
				break;

			default:
				break;
			}
			if (mp)
				mountpoints[part] = strdup(mp);
			continue;
		}
		warnx("line %d: unknown field: %s", lineno, cp);
		errors++;
	next:
		;
	}
	errors += checklabel(lp);

	if (errors > 0)
		mpcopy(mountpoints, omountpoints);
	mpfree(omountpoints);
	
	return (errors > 0);
}

/*
 * Check disklabel for errors and fill in
 * derived fields according to supplied values.
 */
int
checklabel(struct disklabel *lp)
{
	struct partition *pp;
	int i, errors = 0;
	char part;

	if (lp->d_secsize == 0) {
		warnx("sector size %d", lp->d_secsize);
		return (1);
	}
	if (lp->d_nsectors == 0) {
		warnx("sectors/track %d", lp->d_nsectors);
		return (1);
	}
	if (lp->d_ntracks == 0) {
		warnx("tracks/cylinder %d", lp->d_ntracks);
		return (1);
	}
	if  (lp->d_ncylinders == 0) {
		warnx("cylinders/unit %d", lp->d_ncylinders);
		errors++;
	}
	if (lp->d_secpercyl == 0)
		lp->d_secpercyl = lp->d_nsectors * lp->d_ntracks;
	if (DL_GETDSIZE(lp) == 0)
		DL_SETDSIZE(lp, (u_int64_t)lp->d_secpercyl * lp->d_ncylinders);
	if (lp->d_bbsize == 0) {
		warnx("boot block size %d", lp->d_bbsize);
		errors++;
	} else if (lp->d_bbsize % lp->d_secsize)
		warnx("warning, boot block size %% sector-size != 0");
	if (lp->d_sbsize == 0) {
		warnx("super block size %d", lp->d_sbsize);
		errors++;
	} else if (lp->d_sbsize % lp->d_secsize)
		warnx("warning, super block size %% sector-size != 0");
	if (lp->d_npartitions > MAXPARTITIONS)
		warnx("warning, number of partitions (%d) > MAXPARTITIONS (%d)",
		    lp->d_npartitions, MAXPARTITIONS);
	for (i = 0; i < lp->d_npartitions; i++) {
		part = 'a' + i;
		pp = &lp->d_partitions[i];
		if (DL_GETPSIZE(pp) == 0 && DL_GETPOFFSET(pp) != 0)
			warnx("warning, partition %c: size 0, but offset %llu",
			    part, DL_GETPOFFSET(pp));
#ifdef SUN_CYLCHECK
		if (lp->d_flags & D_VENDOR) {
			if (i != RAW_PART && DL_GETPSIZE(pp) % lp->d_secpercyl)
				warnx("warning, partition %c: size %% "
				    "cylinder-size != 0", part);
			if (i != RAW_PART && DL_GETPOFFSET(pp) % lp->d_secpercyl)
				warnx("warning, partition %c: offset %% "
				    "cylinder-size != 0", part);
		}
#endif
#ifdef SUN_AAT0
		if ((lp->d_flags & D_VENDOR) &&
		    i == 0 && DL_GETPSIZE(pp) != 0 && DL_GETPOFFSET(pp) != 0) {
			warnx("this architecture requires partition 'a' to "
			    "start at sector 0");
			errors++;
		}
#endif
		if (DL_GETPOFFSET(pp) > DL_GETDSIZE(lp)) {
			warnx("partition %c: offset past end of unit", part);
			errors++;
		}
		if (DL_GETPOFFSET(pp) + DL_GETPSIZE(pp) > DL_GETDSIZE(lp)) {
			warnx("partition %c: partition extends past end of unit",
			    part);
			errors++;
		}
#if 0
		if (pp->p_frag == 0 && pp->p_fsize != 0) {
			warnx("partition %c: block size < fragment size", part);
			errors++;
		}
#endif
	}
	for (; i < MAXPARTITIONS; i++) {
		part = 'a' + i;
		pp = &lp->d_partitions[i];
		if (DL_GETPSIZE(pp) || DL_GETPOFFSET(pp))
			warnx("warning, unused partition %c: size %llu "
			    "offset %llu", part, DL_GETPSIZE(pp),
			    DL_GETPOFFSET(pp));
	}
	return (errors > 0);
}

#if NUMBOOT > 0
/*
 * If we are installing a boot program that doesn't fit in d_bbsize
 * we need to mark those partitions that the boot overflows into.
 * This allows newfs to prevent creation of a filesystem where it might
 * clobber bootstrap code.
 */
void
setbootflag(struct disklabel *lp)
{
	struct partition *pp;
	int i, errors = 0;
	u_int64_t bend;
	char part;

	if (bootbuf == NULL)
		return;

	bend = (u_int64_t)bootsize / lp->d_secsize;
	for (i = 0; i < lp->d_npartitions; i++) {
		if (i == RAW_PART)
			/* It will *ALWAYS* overlap 'c'. */
			continue;
		pp = &lp->d_partitions[i];
		if (DL_GETPSIZE(pp) == 0)
			/* Partition is unused. */
			continue;
		if (bend <= DL_GETPOFFSET(pp)) {
			/* Boot blocks end before this partition starts. */
			if (pp->p_fstype == FS_BOOT)
				pp->p_fstype = FS_UNUSED;
			continue;
		}

		part = 'a' + i;
		switch (pp->p_fstype) {
		case FS_BOOT:	/* Already marked. */
			break;
		case FS_UNUSED:	/* Mark. */
			pp->p_fstype = FS_BOOT;
			warnx("warning, boot overlaps partition %c, %s",
			    part, "marked as FS_BOOT");
			break;
		default:
			warnx("boot overlaps used partition %c", part);
			errors++;
			break;
		}
	}
	if (errors)
		errx(4, "cannot install boot program");
}
#endif

int
cmplabel(struct disklabel *lp1, struct disklabel *lp2)
{
	struct disklabel lab1 = *lp1;
	struct disklabel lab2 = *lp2;

	/* We don't compare these fields */
	lab1.d_magic = lab2.d_magic;
	lab1.d_magic2 = lab2.d_magic2;
	lab1.d_checksum = lab2.d_checksum;
	lab1.d_bbsize = lab2.d_bbsize;
	lab1.d_sbsize = lab2.d_sbsize;
	lab1.d_bstart = lab2.d_bstart;
	lab1.d_bstarth = lab2.d_bstarth;
	lab1.d_bend = lab2.d_bend;
	lab1.d_bendh = lab2.d_bendh;

	return (memcmp(&lab1, &lab2, sizeof(struct disklabel)));
}

void
usage(void)
{
	fprintf(stderr,
	    "usage: disklabel    [-Acdtv] [-h | -p unit] disk\t(read)\n");
	fprintf(stderr,
	    "       disklabel -w [-Acdnv] disk disktype [packid]\t(write)\n");
	fprintf(stderr,
	    "       disklabel -e [-Acdnv] disk\t\t\t(edit)\n");
	fprintf(stderr,
	    "       disklabel -E [-Acdnv] [-F|-f file] disk\t\t(simple editor)"
	    "\n");
	fprintf(stderr,
	    "       disklabel -R [-nv] [-F|-f file] disk protofile\t\t(restore)\n\n");
#if NUMBOOT > 0
	fprintf(stderr,
	    "       disklabel -B  [-nv] [-b boot1] disk [disktype]\t\t(boot)\n");
 	fprintf(stderr,
	    "       disklabel -Bw [-nv] [-b boot1] disk disktype [packid]\t"
	    "(boot+write)\n");
	fprintf(stderr,
	    "       disklabel -BR [-nv] [-F|-f file ] [-b boot1] disk protofile\t\t"
	    "(boot+restore)\n\n");
#endif
	fprintf(stderr,
	    "`disk' may be of the form: sd0 or /dev/rsd0%c.\n", 'a'+RAW_PART);
	fprintf(stderr,
	    "`disktype' is an entry from %s, see disktab(5) for more info.\n",
	    DISKTAB);
	fprintf(stderr,
	    "`packid' is an identification string for the device.\n");
	fprintf(stderr,
	    "`protofile' is the output from the read cmd form; -R is powerful.\n");
#ifdef SEEALSO
	fprintf(stderr,
	    "For procedures specific to this architecture see: %s\n", SEEALSO);
#endif
	exit(1);
}
