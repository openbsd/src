/*	$NetBSD: fdisk.c,v 1.11 1995/10/04 23:11:19 ghudson Exp $	*/

/*
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#ifndef lint
static char rcsid[] = "$NetBSD: fdisk.c,v 1.11 1995/10/04 23:11:19 ghudson Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LBUF 100
static char lbuf[LBUF];

/*
 * 14-Dec-89  Robert Baron (rvb) at Carnegie-Mellon University
 *	Copyright (c) 1989	Robert. V. Baron
 *	Created.
 */

char *disk = "/dev/rwd0d";

struct disklabel disklabel;		/* disk parameters */

int cylinders, sectors, heads, cylindersectors, disksectors;

struct mboot {
	unsigned char padding[2]; /* force the longs to be long alligned */
	unsigned char bootinst[DOSPARTOFF];
	struct	dos_partition parts[4];
	unsigned short int	signature;
};
struct mboot mboot;

#define ACTIVE 0x80
#define BOOT_MAGIC 0xAA55

int dos_cylinders;
int dos_heads;
int dos_sectors;
int dos_cylindersectors;

#define DOSSECT(s,c)	(((s) & 0x3f) | (((c) >> 2) & 0xc0))
#define DOSCYL(c)	((c) & 0xff)
int partition = -1;

int a_flag;		/* set active partition */
int i_flag;		/* replace partition data */
int u_flag;		/* update partition data */

unsigned char bootcode[] = {
0x33, 0xc0, 0xfa, 0x8e, 0xd0, 0xbc, 0x00, 0x7c, 0x8e, 0xc0, 0x8e, 0xd8, 0xfb, 0x8b, 0xf4, 0xbf,
0x00, 0x06, 0xb9, 0x00, 0x02, 0xfc, 0xf3, 0xa4, 0xea, 0x1d, 0x06, 0x00, 0x00, 0xb0, 0x04, 0xbe,
0xbe, 0x07, 0x80, 0x3c, 0x80, 0x74, 0x0c, 0x83, 0xc6, 0x10, 0xfe, 0xc8, 0x75, 0xf4, 0xbe, 0xbd,
0x06, 0xeb, 0x43, 0x8b, 0xfe, 0x8b, 0x14, 0x8b, 0x4c, 0x02, 0x83, 0xc6, 0x10, 0xfe, 0xc8, 0x74,
0x0a, 0x80, 0x3c, 0x80, 0x75, 0xf4, 0xbe, 0xbd, 0x06, 0xeb, 0x2b, 0xbd, 0x05, 0x00, 0xbb, 0x00,
0x7c, 0xb8, 0x01, 0x02, 0xcd, 0x13, 0x73, 0x0c, 0x33, 0xc0, 0xcd, 0x13, 0x4d, 0x75, 0xef, 0xbe,
0x9e, 0x06, 0xeb, 0x12, 0x81, 0x3e, 0xfe, 0x7d, 0x55, 0xaa, 0x75, 0x07, 0x8b, 0xf7, 0xea, 0x00,
0x7c, 0x00, 0x00, 0xbe, 0x85, 0x06, 0x2e, 0xac, 0x0a, 0xc0, 0x74, 0x06, 0xb4, 0x0e, 0xcd, 0x10,
0xeb, 0xf4, 0xfb, 0xeb, 0xfe,
'M', 'i', 's', 's', 'i', 'n', 'g', ' ',
	'o', 'p', 'e', 'r', 'a', 't', 'i', 'n', 'g', ' ', 's', 'y', 's', 't', 'e', 'm', 0,
'E', 'r', 'r', 'o', 'r', ' ', 'l', 'o', 'a', 'd', 'i', 'n', 'g', ' ',
	'o', 'p', 'e', 'r', 'a', 't', 'i', 'n', 'g', ' ', 's', 'y', 's', 't', 'e', 'm', 0,
'I', 'n', 'v', 'a', 'l', 'i', 'd', ' ',
	'p', 'a', 'r', 't', 'i', 't', 'i', 'o', 'n', ' ', 't', 'a', 'b', 'l', 'e', 0,
'A', 'u', 't', 'h', 'o', 'r', ' ', '-', ' ',
	'S', 'i', 'e', 'g', 'm', 'a', 'r', ' ', 'S', 'c', 'h', 'm', 'i', 'd', 't', 0,0,0,

  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

struct part_type {
	int type;
	char *name;
} part_types[] = {
	{0x00, "unused"},
	{0x01, "Primary DOS with 12 bit FAT"},
	{0x02, "XENIX / filesystem"},
	{0x03, "XENIX /usr filesystem"},
	{0x04, "Primary DOS with 16 bit FAT"},
	{0x05, "Extended DOS"},
	{0x06, "Primary 'big' DOS (> 32MB)"},
	{0x07, "OS/2 HPFS, QNX or Advanced UNIX"},
	{0x08, "AIX filesystem"},
	{0x09, "AIX boot partition or Coherent"},
	{0x0A, "OS/2 Boot Manager or OPUS"},
	{0x10, "OPUS"},
	{0x40, "VENIX 286"},
	{0x50, "DM"},
	{0x51, "DM"},
	{0x52, "CP/M or Microport SysV/AT"},
	{0x56, "GB"},
	{0x61, "Speed"},
	{0x63, "ISC UNIX, other System V/386, GNU HURD or Mach"},
	{0x64, "Novell Netware 2.xx"},
	{0x65, "Novell Netware 3.xx"},
	{0x75, "PCIX"},
	{0x80, "Minix 1.1 ... 1.4a"},
	{0x81, "Minix 1.4b ... 1.5.10"},
	{0x82, "Linux swap"},
	{0x83, "Linux filesystem"},
	{0x93, "Amoeba filesystem"},
	{0x94, "Amoeba bad block table"},
	{0xA5, "NetBSD or 386BSD"},
	{0xB7, "BSDI BSD/386 filesystem"},
	{0xB8, "BSDI BSD/386 swap"},
	{0xDB, "Concurrent CPM or C.DOS or CTOS"},
	{0xE1, "Speed"},
	{0xE3, "Speed"},
	{0xE4, "Speed"},
	{0xF1, "Speed"},
	{0xF2, "DOS 3.3+ Secondary"},
	{0xF4, "Speed"},
	{0xFF, "BBT (Bad Blocks Table)"},
};

void	usage __P((void));
void	print_s0 __P((int));
void	print_part __P((int));
void	init_sector0 __P((int));
void	intuit_translated_geometry __P((void));
int	try_heads __P((quad_t, quad_t, quad_t, quad_t, quad_t, quad_t, quad_t,
		       quad_t));
int	try_sectors __P((quad_t, quad_t, quad_t, quad_t, quad_t));
void	change_part __P((int));
void	print_params __P((void));
void	change_active __P((int));
void	get_params_to_use __P((void));
void	dos __P((int, unsigned char *, unsigned char *, unsigned char *));
int	open_disk __P((int));
int	read_disk __P((int, void *));
int	write_disk __P((int, void *));
int	get_params __P((void));
int	read_s0 __P((void));
int	write_s0 __P((void));
int	yesno __P((char *));
void	decimal __P((char *, int *));
int	type_match __P((const void *, const void *));
char	*get_type __P((int));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch;
	int part;

	a_flag = i_flag = u_flag = 0;
	while ((ch = getopt(argc, argv, "0123aiu")) != -1)
		switch (ch) {
		case '0':
			partition = 0;
			break;
		case '1':
			partition = 1;
			break;
		case '2':
			partition = 2;
			break;
		case '3':
			partition = 3;
			break;
		case 'a':
			a_flag = 1;
			break;
		case 'i':
			i_flag = 1;
		case 'u':
			u_flag = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		disk = argv[0];

	if (open_disk(a_flag || i_flag || u_flag) < 0)
		exit(1);

	if (read_s0())
		init_sector0(1);

	intuit_translated_geometry();

	printf("******* Working on device %s *******\n", disk);
	if (u_flag)
		get_params_to_use();
	else
		print_params();

	printf("Warning: BIOS sector numbering starts with sector 1\n");
	printf("Information from DOS bootblock is:\n");
	if (partition == -1) {
		for (part = 0; part < NDOSPART; part++)
			change_part(part);
	} else
		change_part(partition);

	if (u_flag || a_flag)
		change_active(partition);

	if (u_flag || a_flag) {
		printf("\nWe haven't changed the partition table yet.  ");
		printf("This is your last chance.\n");
		print_s0(-1);
		if (yesno("Should we write new partition table?"))
			write_s0();
	}

	exit(0);
}

void
usage()
{

	(void)fprintf(stderr, "usage: fdisk [-aiu] [-0|-1|-2|-3] [device]\n");
	exit(1);
}

void
print_s0(which)
	int which;
{
	int part;

	print_params();
	printf("Information from DOS bootblock is:\n");
	if (which == -1) {
		for (part = 0; part < NDOSPART; part++)
			printf("%d: ", part), print_part(part);
	} else
		print_part(which);
}

static struct dos_partition mtpart = { 0 };

void
print_part(part)
	int part;
{
	struct dos_partition *partp;

	partp = &mboot.parts[part];
	if (!memcmp(partp, &mtpart, sizeof(struct dos_partition))) {
		printf("<UNUSED>\n");
		return;
	}
	printf("sysid %d (%s)\n", partp->dp_typ, get_type(partp->dp_typ));
	printf("    start %d, size %d (%d MB), flag %x\n",
	    partp->dp_start, partp->dp_size,
	    partp->dp_size * 512 / (1024 * 1024), partp->dp_flag);
	printf("\tbeg: cylinder %4d, head %3d, sector %2d\n",
	    DPCYL(partp->dp_scyl, partp->dp_ssect),
	    partp->dp_shd, DPSECT(partp->dp_ssect));
	printf("\tend: cylinder %4d, head %3d, sector %2d\n",
	    DPCYL(partp->dp_ecyl, partp->dp_esect),
	    partp->dp_ehd, DPSECT(partp->dp_esect));
}

void
init_sector0(start)
	int start;
{
	struct dos_partition *partp;

	memcpy(mboot.bootinst, bootcode, sizeof(bootcode));
	mboot.signature = BOOT_MAGIC;

	partp = &mboot.parts[3];
	partp->dp_typ = DOSPTYP_386BSD;
	partp->dp_flag = ACTIVE;
	partp->dp_start = start;
	partp->dp_size = disksectors - start;

	dos(partp->dp_start,
	    &partp->dp_scyl, &partp->dp_shd, &partp->dp_ssect);
	dos(partp->dp_start + partp->dp_size - 1,
	    &partp->dp_ecyl, &partp->dp_ehd, &partp->dp_esect);
}

/* Prerequisite: the disklabel parameters and master boot record must
 *		 have been read (i.e. dos_* and mboot are meaningful).
 * Specification: modifies dos_cylinders, dos_heads, dos_sectors, and
 *		  dos_cylindersectors to be consistent with what the
 *		  partition table is using, if we can find a geometry
 *		  which is consistent with all partition table entries.
 *		  We may get the number of cylinders slightly wrong (in
 *		  the conservative direction).  The idea is to be able
 *		  to create a NetBSD partition on a disk we don't know
 *		  the translated geometry of.
 * This whole routine should be replaced with a kernel interface to get
 * the BIOS geometry (which in turn requires modifications to the i386
 * boot loader to pass in the BIOS geometry for each disk). */
void
intuit_translated_geometry()
{
	int cylinders = -1, heads = -1, sectors = -1, i, j;
	int c1, h1, s1, c2, h2, s2;
	long a1, a2;
	quad_t num, denom;

	/* Try to deduce the number of heads from two different mappings. */
	for (i = 0; i < NDOSPART * 2; i++) {
		if (get_mapping(i, &c1, &h1, &s1, &a1) < 0)
			continue;
		for (j = 0; j < 8; j++) {
			if (get_mapping(j, &c2, &h2, &s2, &a2) < 0)
				continue;
			num = (quad_t)h1*(a2-s2) - h2*(a1-s1);
			denom = (quad_t)c2*(a1-s1) - c1*(a2-s2);
			if (denom != 0 && num % denom == 0) {
				heads = num / denom;
				break;
			}
		}
		if (heads != -1)	
			break;
	}

	if (heads == -1)
		return;

	/* Now figure out the number of sectors from a single mapping. */
	for (i = 0; i < NDOSPART * 2; i++) {
		if (get_mapping(i, &c1, &h1, &s1, &a1) < 0)
			continue;
		num = a1 - s1;
		denom = c1 * heads + h1;
		if (denom != 0 && num % denom == 0) {
			sectors = num / denom;
			break;
		}
	}

	if (sectors == -1)
		return;

	/* Estimate the number of cylinders. */
	cylinders = dos_cylinders * dos_cylindersectors / heads / sectors;

	/* Now verify consistency with each of the partition table entries.
	 * Be willing to shove cylinders up a little bit to make things work,
	 * but translation mismatches are fatal. */
	for (i = 0; i < NDOSPART * 2; i++) {
		if (get_mapping(i, &c1, &h1, &s1, &a1) < 0)
			continue;
		if (sectors * (c1 * heads + h1) + s1 != a1)
			return;
		if (c1 >= cylinders)
			cylinders = c1 + 1;
	}

	/* Everything checks out.  Reset the geometry to use for further
	 * calculations. */
	dos_cylinders = cylinders;
	dos_heads = heads;
	dos_sectors = sectors;
	dos_cylindersectors = heads * sectors;
}

/* For the purposes of intuit_translated_geometry(), treat the partition
 * table as a list of eight mapping between (cylinder, head, sector)
 * triplets and absolute sectors.  Get the relevant geometry triplet and
 * absolute sectors for a given entry, or return -1 if it isn't present.
 * Note: for simplicity, the returned sector is 0-based. */
int
get_mapping(i, cylinder, head, sector, absolute)
	int i, *cylinder, *head, *sector;
	long *absolute;
{
	struct dos_partition *part = &mboot.parts[i / 2];

	if (part->dp_typ == 0)
		return -1;
	if (i % 2 == 0) {
		*cylinder = DPCYL(part->dp_scyl, part->dp_ssect);
		*head = part->dp_shd;
		*sector = DPSECT(part->dp_ssect) - 1;
		*absolute = part->dp_start;
	} else {
		*cylinder = DPCYL(part->dp_ecyl, part->dp_esect);
		*head = part->dp_ehd;
		*sector = DPSECT(part->dp_esect) - 1;
		*absolute = part->dp_start + part->dp_size - 1;
	}
	return 0;
}

void
change_part(part)
	int part;
{
	struct dos_partition *partp;

	partp = &mboot.parts[part];

	printf("The data for partition %d is:\n", part);
	print_part(part);

	if (!u_flag || !yesno("Do you want to change it?"))
		return;

	if (i_flag) {
		memset(partp, 0, sizeof(*partp));
		if (part == 3) {
			init_sector0(1);
			printf("\nThe static data for the DOS partition 3 has been reinitialized to:\n");
			print_part(part);
		}
	}

	do {
		{
			int sysid, start, size;

			sysid = partp->dp_typ,
			start = partp->dp_start,
			size = partp->dp_size;
			decimal("sysid", &sysid);
			decimal("start", &start);
			decimal("size", &size);
			partp->dp_typ = sysid;
			partp->dp_start = start;
			partp->dp_size = size;
		}

		if (yesno("Explicitly specify beg/end address?")) {
			int tsector, tcylinder, thead;

			tcylinder = DPCYL(partp->dp_scyl, partp->dp_ssect);
			thead = partp->dp_shd;
			tsector = DPSECT(partp->dp_ssect);
			decimal("beginning cylinder", &tcylinder);
			decimal("beginning head", &thead);
			decimal("beginning sector", &tsector);
			partp->dp_scyl = DOSCYL(tcylinder);
			partp->dp_shd = thead;
			partp->dp_ssect = DOSSECT(tsector, tcylinder);

			tcylinder = DPCYL(partp->dp_ecyl, partp->dp_esect);
			thead = partp->dp_ehd;
			tsector = DPSECT(partp->dp_esect);
			decimal("ending cylinder", &tcylinder);
			decimal("ending head", &thead);
			decimal("ending sector", &tsector);
			partp->dp_ecyl = DOSCYL(tcylinder);
			partp->dp_ehd = thead;
			partp->dp_esect = DOSSECT(tsector, tcylinder);
		} else {
			dos(partp->dp_start,
			    &partp->dp_scyl, &partp->dp_shd, &partp->dp_ssect);
			dos(partp->dp_start + partp->dp_size - 1,
			    &partp->dp_ecyl, &partp->dp_ehd, &partp->dp_esect);
		}

		print_part(part);
	} while (!yesno("Is this entry okay?"));
}

void
print_params()
{

	printf("parameters extracted from in-core disklabel are:\n");
	printf("cylinders=%d heads=%d sectors/track=%d (%d sectors/cylinder)\n\n",
	    cylinders, heads, sectors, cylindersectors);
	if (dos_sectors > 63 || dos_cylinders > 1023 || dos_heads > 255)
		printf("Figures below won't work with BIOS for partitions not in cylinder 1\n");
	printf("parameters to be used for BIOS calculations are:\n");
	printf("cylinders=%d heads=%d sectors/track=%d (%d sectors/cylinder)\n\n",
	    dos_cylinders, dos_heads, dos_sectors, dos_cylindersectors);
}

void
change_active(which)
	int which;
{
	struct dos_partition *partp;
	int part;
	int active = 3;

	partp = &mboot.parts[0];

	if (a_flag && which != -1)
		active = which;
	else {
		for (part = 0; part < NDOSPART; part++)
			if (partp[part].dp_flag & ACTIVE)
				active = part;
	}
	if (yesno("Do you want to change the active partition?")) {
		do {
			decimal("active partition", &active);
		} while (!yesno("Are you happy with this choice?"));
	}
	for (part = 0; part < NDOSPART; part++)
		partp[part].dp_flag &= ~ACTIVE;
	partp[active].dp_flag |= ACTIVE;
}

void
get_params_to_use()
{

	print_params();
	if (yesno("Do you want to change our idea of what BIOS thinks?")) {
		do {
			decimal("BIOS's idea of #cylinders", &dos_cylinders);
			decimal("BIOS's idea of #heads", &dos_heads);
			decimal("BIOS's idea of #sectors", &dos_sectors);
			dos_cylindersectors = dos_heads * dos_sectors;
			print_params();
		} while (!yesno("Are you happy with this choice?"));
	}
}

/***********************************************\
* Change real numbers into strange dos numbers	*
\***********************************************/
void
dos(sector, cylinderp, headp, sectorp)
	int sector;
	unsigned char *cylinderp, *headp, *sectorp;
{
	int cylinder, head;

	cylinder = sector / dos_cylindersectors;
	sector -= cylinder * dos_cylindersectors;

	head = sector / dos_sectors;
	sector -= head * dos_sectors;

	*cylinderp = DOSCYL(cylinder);
	*headp = head;
	*sectorp = DOSSECT(sector + 1, cylinder);
}

int fd;

int
open_disk(u_flag)
	int u_flag;
{
	struct stat st;

	if ((fd = open(disk, u_flag ? O_RDWR : O_RDONLY)) == -1) {
		warn("%s", disk);
		return (-1);
	}
	if (fstat(fd, &st) == -1) {
		close(fd);
		warn("%s", disk);
		return (-1);
	}
	if (!S_ISCHR(st.st_mode) && !S_ISREG(st.st_mode)) {
		close(fd);
		warnx("%s is not a character device or regular file", disk);
		return (-1);
	}
	if (get_params() == -1) {
		close(fd);
		return (-1);
	}
	return (0);
}

int
read_disk(sector, buf)
	int sector;
	void *buf;
{

	if (lseek(fd, (off_t)(sector * 512), 0) == -1)
		return (-1);
	return (read(fd, buf, 512));
}

int
write_disk(sector, buf)
	int sector;
	void *buf;
{

	if (lseek(fd, (off_t)(sector * 512), 0) == -1)
		return (-1);
	return (write(fd, buf, 512));
}

int
get_params()
{

	if (ioctl(fd, DIOCGDINFO, &disklabel) == -1) {
		warn("DIOCGDINFO");
		return (-1);
	}

	dos_cylinders = cylinders = disklabel.d_ncylinders;
	dos_heads = heads = disklabel.d_ntracks;
	dos_sectors = sectors = disklabel.d_nsectors;
	dos_cylindersectors = cylindersectors = heads * sectors;
	disksectors = cylinders * heads * sectors;

	return (0);
}

int
read_s0()
{

	if (read_disk(0, mboot.bootinst) == -1) {
		warn("can't read fdisk partition table");
		return (-1);
	}
	if (mboot.signature != BOOT_MAGIC) {
		warn("invalid fdisk partition table found");
		/* So should we initialize things? */
		return (-1);
	}
	return (0);
}

int
write_s0()
{
	int flag;

	/*
	 * write enable label sector before write (if necessary),
	 * disable after writing.
	 * needed if the disklabel protected area also protects
	 * sector 0. (e.g. empty disk)
	 */
	flag = 1;
	if (ioctl(fd, DIOCWLABEL, &flag) < 0)
		warn("DIOCWLABEL");
	if (write_disk(0, mboot.bootinst) == -1) {
		warn("can't write fdisk partition table");
		return -1;
	}
	flag = 0;
	if (ioctl(fd, DIOCWLABEL, &flag) < 0)
		warn("DIOCWLABEL");
}

int
yesno(str)
	char *str;
{
	int ch, first;

	printf("%s [n] ", str);

	first = ch = getchar();
	while (ch != '\n' && ch != EOF)
		ch = getchar();
	return (first == 'y' || first == 'Y');
}

void
decimal(str, num)
	char *str;
	int *num;
{
	int acc = 0;
	char *cp;

	for (;; printf("%s is not a valid decimal number.\n", lbuf)) {
		printf("Supply a decimal value for \"%s\" [%d] ", str, *num);

		fgets(lbuf, LBUF, stdin);
		lbuf[strlen(lbuf)-1] = '\0';
		cp = lbuf;

		cp += strspn(cp, " \t");
		if (*cp == '\0')
			return;

		if (!isdigit(*cp))
			continue;
		acc = strtol(lbuf, &cp, 10);

		cp += strspn(cp, " \t");
		if (*cp != '\0')
			continue;

		*num = acc;
		return;
	}

}

int
type_match(key, item)
	const void *key, *item;
{
	const int *typep = key;
	const struct part_type *ptr = item;

	if (*typep < ptr->type)
		return (-1);
	if (*typep > ptr->type)
		return (1);
	return (0);
}

char *
get_type(type)
	int type;
{
	struct part_type *ptr;

	ptr = bsearch(&type, part_types,
	    sizeof(part_types) / sizeof(struct part_type),
	    sizeof(struct part_type), type_match);
	if (ptr == 0)
		return ("unknown");
	else
		return (ptr->name);
}
