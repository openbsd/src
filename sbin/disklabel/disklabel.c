/*	$OpenBSD: disklabel.c,v 1.94 2005/01/07 21:58:14 otto Exp $	*/

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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1987, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static const char rcsid[] = "$OpenBSD: disklabel.c,v 1.94 2005/01/07 21:58:14 otto Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/ioctl.h>
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

char	*dkname, *specname;
char	tmpfil[] = _PATH_TMPFILE;
char	namebuf[BBSIZE], *np = namebuf;
struct	disklabel lab;
char	bootarea[BBSIZE];

#if NUMBOOT > 0
int	installboot;	/* non-zero if we should install a boot program */
char	*bootbuf;	/* pointer to buffer with remainder of boot prog */
int	bootsize;	/* size of remaining boot program */
char	*xxboot;	/* primary boot */
char	*bootxx;	/* secondary boot */
char	boot0[MAXPATHLEN];
#if NUMBOOT > 1
char	boot1[MAXPATHLEN];
#endif
#endif

enum {
	UNSPEC, EDIT, EDITOR, READ, RESTORE, SETWRITEABLE, WRITE, WRITEBOOT
} op = UNSPEC;

int	cflag;
int	dflag;
int	rflag;
int	tflag;
int	nwflag;
int	verbose;
int	donothing;

#ifdef DOSLABEL
struct dos_partition *dosdp;	/* DOS partition, if found */
struct dos_partition *readmbr(int);
#endif

void	makedisktab(FILE *, struct disklabel *);
void	makelabel(char *, char *, struct disklabel *);
int	writelabel(int, char *, struct disklabel *);
void	l_perror(char *);
int	edit(struct disklabel *, int);
int	editit(void);
char	*skip(char *);
char	*word(char *);
int	getasciilabel(FILE *, struct disklabel *);
int	cmplabel(struct disklabel *, struct disklabel *);
void	setbootflag(struct disklabel *);
void	usage(void);
u_int32_t getnum(char *, u_int32_t, u_int32_t, const char **);

int
main(int argc, char *argv[])
{
	int ch, f, writeable, error = 0;
	char *fstabfile = NULL;
	struct disklabel *lp;
	char print_unit = 0;
	FILE *t;

	while ((ch = getopt(argc, argv, "BEf:NRWb:cdenp:rs:tvw")) != -1)
		switch (ch) {
#if NUMBOOT > 0
		case 'B':
			++installboot;
			break;
		case 'b':
			xxboot = optarg;
			break;
#if NUMBOOT > 1
		case 's':
			bootxx = optarg;
			break;
#endif
#endif
		case 'N':
			if (op != UNSPEC)
				usage();
			writeable = 0;
			op = SETWRITEABLE;
			break;
		case 'R':
			if (op != UNSPEC)
				usage();
			op = RESTORE;
			break;
		case 'W':
			if (op != UNSPEC)
				usage();
			writeable = 1;
			op = SETWRITEABLE;
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
			break;
		case 'r':
			++rflag;
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
			if (strchr("bckmg", optarg[0]) == NULL ||
			    optarg[1] != '\0')
				usage();
			print_unit = optarg[0];
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
		rflag++;
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

	if (argc < 1 || (rflag && cflag + dflag > 0) ||
	    (fstabfile && op != EDITOR))
		usage();

	dkname = argv[0];
	f = opendev(dkname, (op == READ ? O_RDONLY : O_RDWR), OPENDEV_PART,
	    &specname);
	if (f < 0)
		err(4, "%s", specname);

#ifdef DOSLABEL
	/*
	 * Check for presence of DOS partition table in
	 * master boot record. Return pointer to OpenBSD
	 * partition, if present. If no valid partition table,
	 * return 0. If valid partition table present, but no
	 * partition to use, return a pointer to a non-386bsd
	 * partition.
	 */
	dosdp = readmbr(f);
#endif

	switch (op) {
	case EDIT:
		if (argc != 1)
			usage();
		if ((lp = readlabel(f)) == NULL)
			exit(1);
		error = edit(lp, f);
		break;
	case EDITOR:
		if (argc != 1)
			usage();
		if ((lp = readlabel(f)) == NULL)
			exit(1);
		error = editor(lp, f, specname, fstabfile);
		break;
	case READ:
		if (argc != 1)
			usage();
		if ((lp = readlabel(f)) == NULL)
			exit(1);
		if (tflag)
			makedisktab(stdout, lp);
		else
			display(stdout, lp, NULL, print_unit, 0, 0);
		error = checklabel(lp);
		break;
	case RESTORE:
		if (argc < 2 || argc > 3)
			usage();
#if NUMBOOT > 0
		if (installboot && argc == 3)
			makelabel(argv[2], NULL, &lab);
#endif
		lp = makebootarea(bootarea, &lab, f);
		if (!(t = fopen(argv[1], "r")))
			err(4, "%s", argv[1]);
		if (getasciilabel(t, lp))
			error = writelabel(f, bootarea, lp);
		else
			error = 1;
		break;
	case SETWRITEABLE:
		if (!donothing) {
			if (ioctl(f, DIOCWLABEL, (char *)&writeable) < 0)
				err(4, "ioctl DIOCWLABEL");
		}
		break;
	case WRITE:
		if (argc < 2 || argc > 3)
			usage();
		makelabel(argv[1], argc == 3 ? argv[2] : NULL, &lab);
		lp = makebootarea(bootarea, &lab, f);
		*lp = lab;
		if (checklabel(lp) == 0)
			error = writelabel(f, bootarea, lp);
		break;
#if NUMBOOT > 0
	case WRITEBOOT:
	{
		struct disklabel tlab;

		if ((lp = readlabel(f)) == NULL)
			exit(1);
		tlab = *lp;
		if (argc == 2)
			makelabel(argv[1], NULL, &lab);
		lp = makebootarea(bootarea, &lab, f);
		*lp = tlab;
		if (checklabel(lp) == 0)
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
#if NUMBOOT > 1
	if (!bootxx && lp->d_boot1) {
		if (*lp->d_boot1 != '/')
			(void)snprintf(boot1, sizeof boot1, "%s%s",
			    _PATH_BOOTDIR, lp->d_boot1);
		else
			(void)strlcpy(boot1, lp->d_boot1, sizeof boot1);
		bootxx = boot1;
	}
#endif
#endif
	/* d_packname is union d_boot[01], so zero */
	memset(lp->d_packname, 0, sizeof(lp->d_packname));
	if (name)
		(void)strncpy(lp->d_packname, name, sizeof(lp->d_packname));
}

int
writelabel(int f, char *boot, struct disklabel *lp)
{
	int writeable;
	off_t sectoffset = 0;

	if (nwflag) {
		warnx("DANGER! The disklabel was not found at the correct location!");
		warnx("To repair this situation, use `disklabel %s > file' to",
		    dkname);
		warnx("save it, then use `disklabel -R %s file' to replace it.",
		    dkname);
		warnx("A new disklabel is not being installed now.");
		return(0); /* Actually 1 but we want to exit */
	}
#if NUMBOOT > 0
	setbootflag(lp);
#endif
	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = 0;
	lp->d_checksum = dkcksum(lp);
	if (rflag) {
#ifdef DOSLABEL
		struct partition *pp = &lp->d_partitions[2];

		/*
		 * If OpenBSD DOS partition is missing, or if
		 * the label to be written is not within partition,
		 * prompt first. Need to allow this in case operator
		 * wants to convert the drive for dedicated use.
		 * In this case, partition 'a' had better start at 0,
		 * otherwise we reject the request as meaningless. -wfj
		 */
		if (dosdp && pp->p_size &&
		    (dosdp->dp_typ == DOSPTYP_OPENBSD ||
		    dosdp->dp_typ == DOSPTYP_FREEBSD ||
		    dosdp->dp_typ == DOSPTYP_NETBSD)) {
		        sectoffset = (off_t)get_le(&dosdp->dp_start) *
			    lp->d_secsize;
		} else {
			if (dosdp) {
				int first, ch;

				printf("Erase the previous contents of the disk? [n]: ");
				fflush(stdout);
				first = ch = getchar();
				while (ch != '\n' && ch != EOF)
					ch = getchar();
				if (first != 'y' && first != 'Y')
					exit(0);
			}
			sectoffset = 0;
		}

#if NUMBOOT > 0
		/*
		 * If we are not installing a boot program
		 * we must read the current bootarea so we don't
		 * clobber the existing boot.
		 */
		if (!installboot) {
			struct disklabel tlab;

			if (lseek(f, sectoffset, SEEK_SET) < 0) {
				perror("lseek");
				return (1);
			}
			tlab = *lp;
			if (read(f, boot, tlab.d_bbsize) != tlab.d_bbsize) {
				perror("read");
				return (1);
			}
			*lp =tlab;
		}
#endif
#endif

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
		if (verbose)
			printf("writing label to block %lld (0x%qx)\n",
			    (long long)sectoffset/DEV_BSIZE,
			    (long long)sectoffset/DEV_BSIZE);
		if (!donothing) {
			if (lseek(f, sectoffset, SEEK_SET) < 0) {
				perror("lseek");
				return (1);
			}
			/*
			 * write enable label sector before write (if necessary),
			 * disable after writing.
			 */
			writeable = 1;

			if (ioctl(f, DIOCWLABEL, &writeable) < 0)
				perror("ioctl DIOCWLABEL");
#ifdef __alpha__
			/*
			 * The Alpha requires that the boot block be checksummed.
			 * The first 63 8-byte quantites are summed into the 64th.
			 */
			{
				int i;
				u_int64_t *dp, sum;

				dp = (u_int64_t *)boot;
				sum = 0;
				for (i = 0; i < 63; i++)
					sum += dp[i];
				dp[63] = sum;
			}
#endif
			if (write(f, boot, lp->d_bbsize) != lp->d_bbsize) {
				perror("write");
				return (1);
			}
		}
#if NUMBOOT > 0
		/*
		 * Output the remainder of the disklabel
		 */
		if (!donothing && bootbuf && write(f, bootbuf, bootsize) != bootsize) {
			perror("write");
			return(1);
		}
#endif
		writeable = 0;
		if (!donothing)
			if (ioctl(f, DIOCWLABEL, &writeable) < 0)
				perror("ioctl DIOCWLABEL");
	} else {
		if (!donothing) {
			if (ioctl(f, DIOCWDINFO, lp) < 0) {
				l_perror("ioctl DIOCWDINFO");
				return (1);
			}
		}
	}
#ifdef __vax__
	if (lp->d_type == DTYPE_SMD && lp->d_flags & D_BADSECT) {
		daddr_t alt;
		int i;

		alt = lp->d_ncylinders * lp->d_secpercyl - lp->d_nsectors;
		for (i = 1; i < 11 && i < lp->d_nsectors; i += 2) {
			(void)lseek(f, (off_t)((alt + i) * lp->d_secsize),
			    SEEK_SET);
			if (!donothing)
				if (write(f, boot, lp->d_secsize) < lp->d_secsize)
					warn("alternate label %d write", i/2);
		}
	}
#endif
	return (0);
}

void
l_perror(char *s)
{

	switch (errno) {
	case ESRCH:
		warnx("%s: No disk label on disk;\n"
		    "use \"disklabel -r\" to install initial label", s);
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

#ifdef DOSLABEL
/*
 * Fetch DOS partition table from disk.
 */
struct dos_partition *
readmbr(int f)
{
	static int mbr[DEV_BSIZE / sizeof(int)];
	struct dos_partition *dp;
	u_int16_t signature;
	int part;

	/*
	 * This must be done this way due to alignment restrictions
	 * in for example mips processors.
         */
	dp = (struct dos_partition *)mbr;
	if (lseek(f, (off_t)DOSBBSECTOR * DEV_BSIZE, SEEK_SET) < 0 ||
	    read(f, mbr, sizeof(mbr)) < sizeof(mbr))
		err(4, "can't read master boot record");
	signature = *((u_char *)mbr + DOSMBR_SIGNATURE_OFF) |
	    (*((u_char *)mbr + DOSMBR_SIGNATURE_OFF + 1) << 8);
	bcopy((char *)mbr+DOSPARTOFF, (char *)mbr, sizeof(*dp) * NDOSPART);

	/*
	 * Don't (yet) know disk geometry (BIOS), use
	 * partition table to find OpenBSD partition, and obtain
	 * disklabel from there.
	 */
	/* Check if table is valid. */
	for (part = 0; part < NDOSPART; part++) {
		if ((dp[part].dp_flag & ~0x80) != 0)
			return (0);
	}
	/* Find OpenBSD partition. */
	for (part = 0; part < NDOSPART; part++) {
		if (get_le(&dp[part].dp_size) && dp[part].dp_typ == DOSPTYP_OPENBSD) {
			fprintf(stderr, "# using MBR partition %d: "
			    "type %02X off %u (0x%x) size %u (0x%x)\n", part,
			    dp[part].dp_typ,
			    get_le(&dp[part].dp_start), get_le(&dp[part].dp_start),
			    get_le(&dp[part].dp_size), get_le(&dp[part].dp_size));
			return (&dp[part]);
		}
	}
	for (part = 0; part < NDOSPART; part++) {
		if (get_le(&dp[part].dp_size) && dp[part].dp_typ == DOSPTYP_FREEBSD) {
			fprintf(stderr, "# using MBR partition %d: "
			    "type %02X off %u (0x%x) size %u (0x%x)\n", part,
			    dp[part].dp_typ,
			    get_le(&dp[part].dp_start), get_le(&dp[part].dp_start),
			    get_le(&dp[part].dp_size), get_le(&dp[part].dp_size));
			return (&dp[part]);
		}
	}
	for (part = 0; part < NDOSPART; part++) {
		if (get_le(&dp[part].dp_size) && dp[part].dp_typ == DOSPTYP_NETBSD) {
			fprintf(stderr, "# using MBR partition %d: "
			    "type %02X off %u (0x%x) size %u (0x%x)\n", part,
			    dp[part].dp_typ,
			    get_le(&dp[part].dp_start), get_le(&dp[part].dp_start),
			    get_le(&dp[part].dp_size), get_le(&dp[part].dp_size));
			return (&dp[part]);
		}
	}

	/*
	 * If there is no signature and no OpenBSD partition this is probably
	 * not an MBR.
	 */
	if (signature != DOSMBR_SIGNATURE)
		return (NULL);

	/* If no OpenBSD partition, find first used partition. */
	for (part = 0; part < NDOSPART; part++) {
		if (get_le(&dp[part].dp_size)) {
			warnx("warning, DOS partition table with no valid OpenBSD partition");
			return (&dp[part]);
		}
	}
	/* Table appears to be empty. */
	return (NULL);
}
#endif

/*
 * Fetch disklabel for disk.
 * Use ioctl to get label unless -r flag is given.
 */
struct disklabel *
readlabel(int f)
{
	struct disklabel *lp = NULL;

	if (rflag) {
		char *msg;
		off_t sectoffset = 0;

#ifdef DOSLABEL
		if (dosdp && get_le(&dosdp->dp_size) &&
		    (dosdp->dp_typ == DOSPTYP_OPENBSD ||
		    dosdp->dp_typ == DOSPTYP_FREEBSD ||
		    dosdp->dp_typ == DOSPTYP_NETBSD))
			sectoffset = (off_t)get_le(&dosdp->dp_start) *
			    DEV_BSIZE;
#endif
		if (verbose)
			printf("reading label from block %lld, offset %lld\n",
			    (long long)sectoffset/DEV_BSIZE,
			    sectoffset/DEV_BSIZE +
			    (LABELSECTOR * DEV_BSIZE) + LABELOFFSET);
		if (lseek(f, sectoffset, SEEK_SET) < 0 ||
		    read(f, bootarea, BBSIZE) < BBSIZE)
			err(4, "%s", specname);

		lp = (struct disklabel *)(bootarea +
			(LABELSECTOR * DEV_BSIZE) + LABELOFFSET);
		if (lp->d_magic == DISKMAGIC &&
		    lp->d_magic2 == DISKMAGIC) {
			if (lp->d_npartitions <= MAXPARTITIONS &&
			    dkcksum(lp) == 0)
				return (lp);

			msg = "disk label corrupted";
		}
		else {
			warnx("no disklabel found. scanning.");
		}
		nwflag++;

		msg = "no disk label";
		for (lp = (struct disklabel *)bootarea;
		    lp <= (struct disklabel *)(bootarea + BBSIZE - sizeof(*lp));
		    lp = (struct disklabel *)((char *)lp + sizeof(long))) {
			if (lp->d_magic == DISKMAGIC &&
			    lp->d_magic2 == DISKMAGIC) {
				if (lp->d_npartitions <= MAXPARTITIONS &&
				    dkcksum(lp) == 0) {
					warnx("found at 0x%lx",
					    (long)((char *)lp - bootarea));
					return (lp);
				}
				msg = "disk label corrupted";
			}
		}
		warnx("%s", msg);
		return(NULL);
	} else {
		if (cflag && ioctl(f, DIOCRLDINFO) < 0)
			err(4, "ioctl DIOCRLDINFO");
		if (dflag) {
			lp = &lab;
			if (ioctl(f, DIOCGPDINFO, lp) < 0)
				err(4, "ioctl DIOCGPDINFO");
		} else {
			lp = &lab;
			if (ioctl(f, DIOCGDINFO, lp) < 0)
				err(4, "ioctl DIOCGDINFO");
		}
	}
	return (lp);
}

/*
 * Construct a bootarea (d_bbsize bytes) in the specified buffer ``boot''
 * Returns a pointer to the disklabel portion of the bootarea.
 */
struct disklabel *
makebootarea(char *boot, struct disklabel *dp, int f)
{
	struct disklabel *lp;
	char *p;
#if NUMBOOT > 0
	char *dkbasename;
	int b;
#if NUMBOOT == 1
	struct stat sb;
#endif
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
	if (!installboot) {
#ifndef __i386__
		if (rflag) {
			if (read(f, boot, BBSIZE) < BBSIZE)
				err(4, "%s", specname);
			memset(lp, 0, sizeof *lp);
		}
#endif
		return (lp);
	}
	/*
	 * We are installing a boot program.  Determine the name(s) and
	 * read them into the appropriate places in the boot area.
	 */
	if (!xxboot || !bootxx) {
		dkbasename = np;
		if ((p = strrchr(dkname, '/')) == NULL)
			p = dkname;
		else
			p++;
		while (*p && !isdigit(*p))
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
#if NUMBOOT > 1
		if (!bootxx) {
			(void)snprintf(np, namebuf + sizeof namebuf - np,
			    "%sboot%s", _PATH_BOOTDIR, dkbasename);
			if (access(np, F_OK) < 0 && dkbasename[0] == 'r')
				dkbasename++;
			bootxx = np;
			(void)snprintf(bootxx, namebuf + sizeof namebuf - bootxx,
			    "%sboot%s", _PATH_BOOTDIR, dkbasename);
			np += strlen(bootxx) + 1;
		}
#endif
	}
	if (verbose)
		warnx("bootstraps: xxboot = %s, bootxx = %s", xxboot,
		    bootxx ? bootxx : "NONE");

	/*
	 * Strange rules:
	 * 1. One-piece bootstrap (hp300/hp800)
	 *	up to d_bbsize bytes of ``xxboot'' go in bootarea, the rest
	 *	is remembered and written later following the bootarea.
	 * 2. Two-piece bootstraps (vax/i386?/mips?)
	 *	up to d_secsize bytes of ``xxboot'' go in first d_secsize
	 *	bytes of bootarea, remaining d_bbsize-d_secsize filled
	 *	from ``bootxx''.
	 */
	b = open(xxboot, O_RDONLY);
	if (b < 0)
		err(4, "%s", xxboot);
#if NUMBOOT > 1
	if (read(b, boot, (int)dp->d_secsize) < 0)
		err(4, "%s", xxboot);
	(void)close(b);
	b = open(bootxx, O_RDONLY);
	if (b < 0)
		err(4, "%s", bootxx);
	if (read(b, &boot[dp->d_secsize], (int)(dp->d_bbsize-dp->d_secsize)) < 0)
		err(4, "%s", bootxx);
#else
	if (read(b, boot, (int)dp->d_bbsize) < 0)
		err(4, "%s", xxboot);
	(void)fstat(b, &sb);
	bootsize = (int)sb.st_size - dp->d_bbsize;
	if (bootsize > 0) {
		/* XXX assume d_secsize is a power of two */
		bootsize = (bootsize + dp->d_secsize-1) & ~(dp->d_secsize-1);
		bootbuf = (char *)malloc((size_t)bootsize);
		if (bootbuf == 0)
			err(4, "%s", xxboot);
		if (read(b, bootbuf, bootsize) < 0) {
			free(bootbuf);
			err(4, "%s", xxboot);
		}
	}
#endif
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
	char *did = "\\\n\t:";
	struct partition *pp;

	if (lp->d_packname[0])
		(void)fprintf(f, "%.*s|", (int)sizeof(lp->d_packname),
		    lp->d_packname);
	if (lp->d_typename[0])
		(void)fprintf(f, "%.*s|", (int)sizeof(lp->d_typename),
		    lp->d_typename);
	(void)fputs("Automatically generated label:\\\n\t:dt=", f);
	if ((unsigned) lp->d_type < DKMAXTYPES)
		(void)fprintf(f, "%s:", dktypenames[lp->d_type]);
	else
		(void)fprintf(f, "unknown%d:", lp->d_type);

	(void)fprintf(f, "se#%u:", lp->d_secsize);
	(void)fprintf(f, "ns#%u:", lp->d_nsectors);
	(void)fprintf(f, "nt#%u:", lp->d_ntracks);
	(void)fprintf(f, "nc#%u:", lp->d_ncylinders);
	(void)fprintf(f, "sc#%u:", lp->d_secpercyl);
	(void)fprintf(f, "su#%u:", lp->d_secperunit);

	if (lp->d_rpm != 3600) {
		(void)fprintf(f, "%srm#%hu:", did, lp->d_rpm);
		did = "";
	}
	if (lp->d_interleave != 1) {
		(void)fprintf(f, "%sil#%hu:", did, lp->d_interleave);
		did = "";
	}
	if (lp->d_trackskew != 0) {
		(void)fprintf(f, "%ssk#%hu:", did, lp->d_trackskew);
		did = "";
	}
	if (lp->d_cylskew != 0) {
		(void)fprintf(f, "%scs#%hu:", did, lp->d_cylskew);
		did = "";
	}
	if (lp->d_headswitch != 0) {
		(void)fprintf(f, "%shs#%u:", did, lp->d_headswitch);
		did = "";
	}
	if (lp->d_trkseek != 0) {
		(void)fprintf(f, "%sts#%u:", did, lp->d_trkseek);
		did = "";
	}
	for (i = 0; i < NDDATA; i++)
		if (lp->d_drivedata[i])
			(void)fprintf(f, "d%d#%u", i, lp->d_drivedata[i]);
	pp = lp->d_partitions;
	for (i = 0; i < lp->d_npartitions; i++, pp++) {
		if (pp->p_size) {
			char c = 'a' + i;

			(void)fprintf(f, "\\\n\t:");
			(void)fprintf(f, "p%c#%u:", c, pp->p_size);
			(void)fprintf(f, "o%c#%u:", c, pp->p_offset);
			if (pp->p_fstype != FS_UNUSED) {
				if ((unsigned) pp->p_fstype < FSMAXTYPES)
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
				    pp->p_fsize * pp->p_frag);
				(void)fprintf(f, "f%c#%u:", c, pp->p_fsize);
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
scale(u_int32_t sz, char unit, struct disklabel *lp)
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
	default:
		return -1.0;
	}
}

/*
 * Display a particular partition.
 */
void
display_partition(FILE *f, struct disklabel *lp, char **mp, int i,
    char unit)
{
	volatile struct partition *pp = &lp->d_partitions[i];
	double p_size, p_offset;

	p_size = scale(pp->p_size, unit, lp);
	p_offset = scale(pp->p_offset, unit, lp);
	if (pp->p_size) {
		if (p_size < 0)
			fprintf(f, "  %c: %13u %13u ", 'a' + i,
			    pp->p_size, pp->p_offset);
		else
			fprintf(f, "  %c: %12.*f%c %12.*f%c ", 'a' + i,
			    unit == 'B' ? 0 : 1, p_size, unit,
			    unit == 'B' ? 0 : 1, p_offset, unit);
		if ((unsigned) pp->p_fstype < FSMAXTYPES)
			fprintf(f, "%7.7s", fstypenames[pp->p_fstype]);
		else
			fprintf(f, "%7d", pp->p_fstype);
		switch (pp->p_fstype) {

		case FS_UNUSED:				/* XXX */
			fprintf(f, "  %5u %5u %4.4s ",
			    pp->p_fsize, pp->p_fsize * pp->p_frag, "");
			break;

		case FS_BSDFFS:
			fprintf(f, "  %5u %5u %4hu ",
			    pp->p_fsize, pp->p_fsize * pp->p_frag,
			    pp->p_cpg);
			break;

		default:
			fprintf(f, "%19.19s", "");
			break;
		}
		if (mp != NULL) {
			if (mp[i] != NULL)
				fprintf(f, "# %s", mp[i]);
		} else if (lp->d_secpercyl) {
			fprintf(f, "# Cyl %5u",
			    pp->p_offset / lp->d_secpercyl);
			if (pp->p_offset % lp->d_secpercyl)
				putc('*', f);
			else
				putc(' ', f);
			fprintf(f, "-%6u",
			    (pp->p_offset +
			    pp->p_size + lp->d_secpercyl - 1) /
			    lp->d_secpercyl - 1);
			if ((pp->p_offset + pp->p_size) % lp->d_secpercyl)
				putc('*', f);
			else
				putc(' ', f);
		}
		putc('\n', f);
	}
}

void
display(FILE *f, struct disklabel *lp, char **mp, char unit, int edit,
     u_int32_t fr)
{
	int i, j;
	double d;

	unit = toupper(unit);
	if (edit)
		fprintf(f, "device: %s\n", specname);
	else
		fprintf(f, "# %s:\n", specname);

	if ((unsigned) lp->d_type < DKMAXTYPES)
		fprintf(f, "type: %s\n", dktypenames[lp->d_type]);
	else
		fprintf(f, "type: %d\n", lp->d_type);
	fprintf(f, "disk: %.*s\n", (int)sizeof(lp->d_typename), lp->d_typename);
	fprintf(f, "label: %.*s\n", (int)sizeof(lp->d_packname), lp->d_packname);
	if (!edit) {
		fprintf(f, "flags:");
		if (lp->d_flags & D_REMOVABLE)
			fprintf(f, " removable");
		if (lp->d_flags & D_ECC)
			fprintf(f, " ecc");
		if (lp->d_flags & D_BADSECT)
			fprintf(f, " badsect");
		putc('\n', f);
	}
	fprintf(f, "bytes/sector: %u\n", lp->d_secsize);
	fprintf(f, "sectors/track: %u\n", lp->d_nsectors);
	fprintf(f, "tracks/cylinder: %u\n", lp->d_ntracks);
	fprintf(f, "sectors/cylinder: %u\n", lp->d_secpercyl);
	fprintf(f, "cylinders: %u\n", lp->d_ncylinders);
	d = scale(lp->d_secperunit, unit, lp);
	if (d < 0)
		fprintf(f, "total sectors: %u\n", lp->d_secperunit);
	else
		fprintf(f, "total bytes: %.*f%c\n", unit == 'B' ? 0 : 1,
		    d, unit);

	if (edit) {
		d = scale(fr, unit, lp);
		if (d < 0)
			fprintf(f, "free sectors: %u\n", fr);
		else
			fprintf(f, "free bytes: %.*f%c\n", unit == 'B' ? 0 : 1,
			    d, unit);
	}
	fprintf(f, "rpm: %hu\n", lp->d_rpm);
	if (!edit) {
		fprintf(f, "interleave: %hu\n", lp->d_interleave);
		fprintf(f, "trackskew: %hu\n", lp->d_trackskew);
		fprintf(f, "cylinderskew: %hu\n", lp->d_cylskew);
		fprintf(f, "headswitch: %u\t\t# microseconds\n",
		    lp->d_headswitch);
		fprintf(f, "track-to-track seek: %u\t# microseconds\n",
		    lp->d_trkseek);
		fprintf(f, "drivedata: ");
		for (i = NDDATA - 1; i >= 0; i--)
			if (lp->d_drivedata[i])
				break;
		if (i < 0)
			i = 0;
		for (j = 0; j <= i; j++)
			fprintf(f, "%d ", lp->d_drivedata[j]);
		fprintf(f, "\n");
	}
	fprintf(f, "\n%hu partitions:\n", lp->d_npartitions);
	fprintf(f, "#    %13.13s %13.13s  fstype [fsize bsize  cpg]\n",
	    "size", "offset");
	for (i = 0; i < lp->d_npartitions; i++)
		display_partition(f, lp, mp, i, unit);
	fflush(f);
}

int
edit(struct disklabel *lp, int f)
{
	int first, ch, fd;
	struct disklabel label;
	FILE *fp;

	if ((fd = mkstemp(tmpfil)) == -1 || (fp = fdopen(fd, "w")) == NULL) {
		if (fd != -1)
			close(fd);
		warn("%s", tmpfil);
		return (1);
	}
	display(fp, lp, NULL, 0, 0, 0);
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
		if (!editit())
			break;
		fp = fopen(tmpfil, "r");
		if (fp == NULL) {
			warn("%s", tmpfil);
			break;
		}
		memset(&label, 0, sizeof(label));
		if (getasciilabel(fp, &label)) {
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

int
editit(void)
{
	pid_t pid;
	int stat = 0, len;
	char *argp[] = {"sh", "-c", NULL, NULL};
	char *ed, *p;

	if ((ed = getenv("EDITOR")) == NULL)
		ed = _PATH_VI;
	len = strlen(ed) + 1 + strlen(tmpfil) + 1;
	p = (char *)malloc(len);
	if (!p) {
		warn("failed to start editor");
		return (0);
	}
	snprintf(p, len, "%s %s", ed, tmpfil);
	argp[2] = p;

	/* Turn off signals. */
	(void)signal(SIGHUP, SIG_IGN);
	(void)signal(SIGINT, SIG_IGN);
	(void)signal(SIGQUIT, SIG_IGN);
	while ((pid = fork()) < 0) {
		if (errno != EAGAIN) {
			warn("fork");
			free(p);
			stat = 1;
			goto bail;
		}
		sleep(1);
	}
	if (pid == 0) {
		execv(_PATH_BSHELL, argp);
		_exit(127);
	}
	free(p);
	for (;;) {
		if (waitpid(pid, (int *)&stat, WUNTRACED) == -1) {
			if (errno == EINTR)
				continue;
			if (errno == ECHILD)
				stat = 1;
			break;
		}
		if (WIFSTOPPED(stat))
			raise(WSTOPSIG(stat));
		else if (WIFEXITED(stat))
			break;
	}
bail:
	(void)signal(SIGHUP, SIG_DFL);
	(void)signal(SIGINT, SIG_DFL);
	(void)signal(SIGQUIT, SIG_DFL);
	return (!stat);
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
#define GETNUM(field, nptr, min, errstr) 				\
	    getnum((nptr), (min),					\
		sizeof(field) == 4 ? UINT_MAX : 			\
		(sizeof(field) == 2 ? USHRT_MAX : UCHAR_MAX),  (errstr))

u_int32_t
getnum(char *nptr, u_int32_t min, u_int32_t max, const char **errstr)
{
	char *p, c;
	u_int32_t ret;

	for (p = nptr; *p != '\0' && !isspace(*p); p++)
		;
	c = *p;
	*p = '\0';
	ret = strtonum(nptr, min, max, errstr);
	*p = c;
	return (ret);
}

/*
 * Read an ascii label in from fd f,
 * in the same format as that put out by display(),
 * and fill in lp.
 */
int
getasciilabel(FILE *f, struct disklabel *lp)
{
	char **cpp, *cp;
	const char *errstr;
	struct partition *pp;
	char *tp, *s, line[BUFSIZ];
	int lineno = 0, errors = 0;
	u_int32_t v;

	lp->d_bbsize = BBSIZE;				/* XXX */
	lp->d_sbsize = SBSIZE;				/* XXX */
	while (fgets(line, sizeof(line) - 1, f)) {
		lineno++;
		if ((cp = strpbrk(line, "#\r\n")))
			*cp = '\0';
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
				if (!strcmp(cp, "removable"))
					v |= D_REMOVABLE;
				else if (!strcmp(cp, "ecc"))
					v |= D_ECC;
				else if (!strcmp(cp, "badsect"))
					v |= D_BADSECT;
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
		if (!strcmp(cp, "total sectors")) {
			v = GETNUM(lp->d_secperunit, tp, 1, &errstr);
			if (errstr) {
				warnx("line %d: bad %s: %s", lineno, cp, tp);
				errors++;
			} else
				lp->d_secperunit = v;
			continue;
		}
		if (!strcmp(cp, "rpm")) {
			v = GETNUM(lp->d_rpm, tp, 1, &errstr);
			if (errstr) {
				warnx("line %d: bad %s: %s", lineno, cp, tp);
				errors++;
			} else
				lp->d_rpm = v;
			continue;
		}
		if (!strcmp(cp, "interleave")) {
			v = GETNUM(lp->d_interleave, tp, 1, &errstr);
			if (errstr) {
				warnx("line %d: bad %s: %s", lineno, cp, tp);
				errors++;
			} else
				lp->d_interleave = v;
			continue;
		}
		if (!strcmp(cp, "trackskew")) {
			v = GETNUM(lp->d_trackskew, tp, 0, &errstr);
			if (errstr) {
				warnx("line %d: bad %s: %s", lineno, cp, tp);
				errors++;
			} else
				lp->d_trackskew = v;
			continue;
		}
		if (!strcmp(cp, "cylinderskew")) {
			v = GETNUM(lp->d_cylskew, tp, 0, &errstr);
			if (errstr) {
				warnx("line %d: bad %s: %s", lineno, cp, tp);
				errors++;
			} else
				lp->d_cylskew = v;
			continue;
		}
		if (!strcmp(cp, "headswitch")) {
			v = GETNUM(lp->d_headswitch, tp, 0, &errstr);
			if (errstr) {
				warnx("line %d: bad %s: %s", lineno, cp, tp);
				errors++;
			} else
				lp->d_headswitch = v;
			continue;
		}
		if (!strcmp(cp, "track-to-track seek")) {
			v = GETNUM(lp->d_trkseek, tp, 0, &errstr);
			if (errstr) {
				warnx("line %d: bad %s: %s", lineno, cp, tp);
				errors++;
			} else
				lp->d_trkseek = v;
			continue;
		}
		if ('a' <= *cp && *cp <= 'z' && cp[1] == '\0') {
			unsigned part = *cp - 'a';

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
			NXTNUM(v, pp->p_size, &errstr);
			if (errstr) {
				warnx("line %d: bad partition size: %s",
				    lineno, cp);
				errors++;
			} else
				pp->p_size = v;
			NXTNUM(v, pp->p_offset, &errstr);
			if (errstr) {
				warnx("line %d: bad partition offset: %s",
				    lineno, cp);
				errors++;
			} else
				pp->p_offset = v;
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
			if (isdigit(*cp))
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
				NXTNUM(pp->p_fsize, pp->p_fsize, &errstr);
				if (pp->p_fsize == 0)
					break;
				NXTNUM(v, v, &errstr);
				pp->p_frag = v / pp->p_fsize;
				break;

			case FS_BSDFFS:
				NXTNUM(pp->p_fsize, pp->p_fsize, &errstr);
				if (pp->p_fsize == 0)
					break;
				NXTNUM(v, v, &errstr);
				pp->p_frag = v / pp->p_fsize;
				NXTNUM(pp->p_cpg, pp->p_cpg, &errstr);
				break;

			default:
				break;
			}
			continue;
		}
		warnx("line %d: unknown field: %s", lineno, cp);
		errors++;
	next:
		;
	}
	errors += checklabel(lp);
	return (errors == 0);
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
	if (lp->d_rpm == 0)
		warnx("warning, revolutions/minute %d", lp->d_rpm);
	if (lp->d_secpercyl == 0)
		lp->d_secpercyl = lp->d_nsectors * lp->d_ntracks;
	if (lp->d_secperunit == 0)
		lp->d_secperunit = lp->d_secpercyl * lp->d_ncylinders;
#ifdef i386__notyet
	if (dosdp && dosdp->dp_size &&
	    (dosdp->dp_typ == DOSPTYP_OPENBSD ||
	    dosdp->dp_typ == DOSPTYP_FREEBSD ||
	    dosdp->dp_typ == DOSPTYP_NETBSD)) {
		&& lp->d_secperunit > dosdp->dp_start + dosdp->dp_size) {
		warnx("exceeds DOS partition size");
		errors++;
		lp->d_secperunit = dosdp->dp_start + dosdp->dp_size;
	}
	/* XXX should also check geometry against BIOS's idea */
#endif
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
		if (pp->p_size == 0 && pp->p_offset != 0)
			warnx("warning, partition %c: size 0, but offset %d",
			    part, pp->p_offset);
#ifdef CYLCHECK
		if (pp->p_size % lp->d_secpercyl)
			warnx("warning, partition %c: size %% cylinder-size != 0",
			    part);
		if (pp->p_offset % lp->d_secpercyl)
			warnx("warning, partition %c: offset %% cylinder-size != 0",
			    part);
#endif
#ifdef AAT0
		if (i == 0 && pp->p_size != 0 && pp->p_offset != 0) {
			warnx("this architecture requires partition 'a' to "
			    "start at sector 0");
			errors++;
		}
#endif
		if (pp->p_offset > lp->d_secperunit) {
			warnx("partition %c: offset past end of unit", part);
			errors++;
		}
		if (pp->p_offset + pp->p_size > lp->d_secperunit) {
			warnx("partition %c: partition extends past end of unit",
			    part);
			errors++;
		}
		if (pp->p_frag == 0 && pp->p_fsize != 0) {
			warnx("partition %c: block size < fragment size", part);
			errors++;
		}
	}
	for (; i < MAXPARTITIONS; i++) {
		part = 'a' + i;
		pp = &lp->d_partitions[i];
		if (pp->p_size || pp->p_offset)
			warnx("warning, unused partition %c: size %d offset %d",
			    'a' + i, pp->p_size, pp->p_offset);
	}
	return (errors);
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
	u_long boffset;
	char part;

	if (bootbuf == 0)
		return;
	boffset = bootsize / lp->d_secsize;
	for (i = 0; i < lp->d_npartitions; i++) {
		part = 'a' + i;
		pp = &lp->d_partitions[i];
		if (pp->p_size == 0)
			continue;
		if (boffset <= pp->p_offset) {
			if (pp->p_fstype == FS_BOOT)
				pp->p_fstype = FS_UNUSED;
		} else if (pp->p_fstype != FS_BOOT) {
			if (pp->p_fstype != FS_UNUSED) {
				warnx("boot overlaps used partition %c", part);
				errors++;
			} else {
				pp->p_fstype = FS_BOOT;
				warnx("warning, boot overlaps partition %c, %s",
				    part, "marked as FS_BOOT");
			}
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

	return (memcmp(&lab1, &lab2, sizeof(struct disklabel)));
}

void
usage(void)
{
	char *boot = "";
	char blank[] = "                             ";

#if NUMBOOT == 1
	boot = " [-b boot1]";
#elif NUMBOOT == 2
	boot = " [-b boot1] [-s boot2]";
#endif
	blank[strlen(boot)] = '\0';

	fprintf(stderr, "usage:\n");
	fprintf(stderr,
	    "  disklabel [-c | -d | -r | -t] [-v] [-p unit] disk\t\t(read)\n");
	fprintf(stderr,
	    "  disklabel -w [-c | -d | -r] [-nv] disk disktype [packid]\t(write)\n");
	fprintf(stderr,
	    "  disklabel -e [-c | -d | -r] [-nv] disk\t\t\t(edit)\n");
	fprintf(stderr,
	    "  disklabel -E [-c | -d | -r] [-nv] [-f tempfile] disk\t\t(simple editor)\n");
	fprintf(stderr,
	    "  disklabel -R [-nrv] disk protofile\t\t\t\t(restore)\n");
	fprintf(stderr,
	    "  disklabel -N | -W [-nv] disk\t\t\t\t\t(protect)\n\n");
	fprintf(stderr,
	    "  disklabel -B  [-nv]%s disk [disktype]\t       (boot)\n",
	    boot);
	fprintf(stderr,
	    "  disklabel -Bw [-nv]%s disk disktype [packid]     (write)\n",
	    boot);
	fprintf(stderr,
	    "  disklabel -BR [-nv]%s disk protofile [disktype]  (restore)\n\n",
	    boot);
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
