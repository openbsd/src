/*	$OpenBSD: mkuboot.c,v 1.2 2013/07/28 18:07:16 miod Exp $	*/

/*
 * Copyright (c) 2008 Mark Kettenis
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
#include <sys/stat.h>
#include <err.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>
#include <sys/exec_elf.h>

#define IH_OS_OPENBSD		1 /* OpenBSD */
#define IH_OS_LINUX		5 /* Linux */

#define IH_ARCH_ALPHA           1       /* Alpha        */
#define IH_ARCH_ARM             2       /* ARM          */
#define IH_ARCH_I386            3       /* Intel x86    */
#define IH_ARCH_IA64            4       /* IA64         */
#define IH_ARCH_MIPS            5       /* MIPS         */
#define IH_ARCH_MIPS64          6       /* MIPS  64 Bit */
#define IH_ARCH_PPC             7       /* PowerPC      */
#define IH_ARCH_SH              9       /* SuperH       */
#define IH_ARCH_SPARC           10      /* Sparc        */
#define IH_ARCH_SPARC64         11      /* Sparc 64 Bit */
#define IH_ARCH_M68K            12      /* M68K         */

#define IH_TYPE_STANDALONE	1 /* Standalone */
#define IH_TYPE_KERNEL		2 /* OS Kernel Image */
#define IH_TYPE_SCRIPT		6 /* Script file */

#define IH_COMP_NONE		0 /* No compression */

#define IH_MAGIC		0x27051956	/* Image Magic Number */
#define IH_NMLEN		32 		/* Image Name Length */

struct image_header {
	uint32_t	ih_magic;
	uint32_t	ih_hcrc;
	uint32_t	ih_time;
	uint32_t	ih_size;
	uint32_t	ih_load;
	uint32_t	ih_ep;
	uint32_t	ih_dcrc;
	uint8_t		ih_os;
	uint8_t		ih_arch;
	uint8_t		ih_type;
	uint8_t		ih_comp;
	uint8_t		ih_name[IH_NMLEN];
};

extern char *__progname;

u_long	copy_data(int, const char *, int, const char *, u_long,
	    struct image_header *, Elf_Word);
u_long	copy_elf(int, const char *, int, const char *, u_long,
	    struct image_header *);
u_long	copy_raw(int, const char *, int, const char *, u_long,
	    struct image_header *);
u_long	fill_zeroes(int, const char *, u_long, struct image_header *, Elf_Word);
int	is_elf(int, const char *);
void	usage(void);

struct arch_map {
	int id;
	const char *arch;
};

static const struct arch_map archmap[] = {
    { IH_ARCH_ALPHA,	"alpha" },
    { IH_ARCH_IA64,	"amd64" },
    { IH_ARCH_ARM,	"arm" },
    { IH_ARCH_I386,	"i386" },
    { IH_ARCH_M68K,	"m68k" },
    { IH_ARCH_MIPS,	"mips" },
    { IH_ARCH_MIPS64,	"mips64" },
    { IH_ARCH_PPC,	"powerpc" },
    { IH_ARCH_SPARC,	"sparc" },
    { IH_ARCH_SPARC64,	"sparc64" },
    { IH_ARCH_SH,	"superh" },
    { 0, NULL }
};

struct type_map {
	int id;
	const char *type;
};
static const struct type_map typemap[] = {
    { IH_TYPE_STANDALONE,	"standalone" },
    { IH_TYPE_KERNEL,		"kernel" },
    { IH_TYPE_SCRIPT,		"script" },
    { 0, NULL }
};

struct os_map {
	int id;
	const char *arch;
};

static const struct os_map osmap[] = {
    { IH_OS_OPENBSD,	"OpenBSD" },
    { IH_OS_LINUX,	"Linux" },
    { 0, NULL }
};


int
main(int argc, char *argv[])
{
	struct image_header ih;
	struct stat sb;
	const struct arch_map *mapptr;
	const struct os_map *osmapptr;
	const struct type_map *typemapptr;
	const char *iname, *oname;
	const char *arch = MACHINE_ARCH;
	const char *os = "OpenBSD";
	const char *type = "kernel";
	const char *imgname = "boot";
	int ifd, ofd;
	uint32_t fsize;
	u_long crc;
	int c, ep, load;

	ep = load = 0;
	while ((c = getopt(argc, argv, "a:e:l:n:o:t:")) != -1) {
		switch (c) {
		case 'a':
			arch = optarg;
			break;
		case 'e':
			sscanf(optarg, "0x%x", &ep);
			break;
		case 'l':
			sscanf(optarg, "0x%x", &load);
			break;
		case 'n':
			imgname = optarg;
			break;
		case 'o':
			os = optarg;
			break;
		case 't':
			type = optarg;
			break;
		default:
			usage();
		}
	}

	for (mapptr = archmap; mapptr->arch; mapptr++)
		if (strcasecmp(arch, mapptr->arch) == 0)
			break;

	if (mapptr->arch == NULL) {
		printf("unknown arch '%s'\n", arch);
		usage();
	}

	for (osmapptr = osmap; osmapptr->arch; osmapptr++)
		if (strcasecmp(os, osmapptr->arch) == 0)
			break;

	if (osmapptr->arch == NULL) {
		printf("unknown OS '%s'\n", os);
		usage();
	}

	for (typemapptr = typemap; typemapptr->type; typemapptr++)
		if (strcasecmp(type, typemapptr->type) == 0)
			break;

	if (typemapptr->type == NULL) {
		printf("unknown type '%s'\n", os);
		usage();
	}

	if (argc - optind != 2)
		usage();

	iname = argv[optind++];
	oname = argv[optind++];

	/* Initialize U-Boot header. */
	bzero(&ih, sizeof ih);
	ih.ih_magic = htobe32(IH_MAGIC);
	ih.ih_time = htobe32(time(NULL));
	ih.ih_load = htobe32(load);
	ih.ih_ep = htobe32(ep);
	ih.ih_os = osmapptr->id;
	ih.ih_arch = mapptr->id;
	ih.ih_type = typemapptr->id;
	ih.ih_comp = IH_COMP_NONE;
	strlcpy(ih.ih_name, imgname, sizeof ih.ih_name);

	ifd = open(iname, O_RDONLY);
	if (ifd < 0)
		err(1, "%s", iname);

	ofd = open(oname, O_RDWR | O_TRUNC | O_CREAT, 0644);
	if (ofd < 0)
		err(1, "%s", oname);

	if (stat(iname, &sb) == -1) {
		err(1, "%s", oname);
	}

	/* Write initial header. */
	if (write(ofd, &ih, sizeof ih) != sizeof ih)
		err(1, "%s", oname);

	/* Write data, calculating the data CRC as we go. */
	crc = crc32(0L, Z_NULL, 0);

	if (ih.ih_type == IH_TYPE_SCRIPT) {
		/* scripts have two extra words of size/pad */
		fsize = htobe32(sb.st_size);
		crc = crc32(crc, (void *)&fsize, sizeof(fsize));
		if (write(ofd, &fsize, sizeof fsize) != sizeof fsize)
			err(1, "%s", oname);
		fsize = 0;
		crc = crc32(crc, (void *)&fsize, sizeof(fsize));
		if (write(ofd, &fsize, sizeof fsize) != sizeof fsize)
			err(1, "%s", oname);
	}

	if (is_elf(ifd, iname))
		crc = copy_elf(ifd, iname, ofd, oname, crc, &ih);
	else
		crc = copy_raw(ifd, iname, ofd, oname, crc, &ih);
	ih.ih_dcrc = htobe32(crc);

	if (ih.ih_type == IH_TYPE_SCRIPT) {
		ih.ih_size += 8; /* two extra pad words */
	}

	ih.ih_size = htobe32(ih.ih_size);

	/* Calculate header CRC. */
	crc = crc32(0, (void *)&ih, sizeof ih);
	ih.ih_hcrc = htobe32(crc);

	/* Write finalized header. */
	if (lseek(ofd, 0, SEEK_SET) != 0)
		err(1, "%s", oname);
	if (write(ofd, &ih, sizeof ih) != sizeof ih)
		err(1, "%s", oname);

	return(0);
}

int
is_elf(int ifd, const char *iname)
{
	ssize_t nbytes;
	Elf_Ehdr ehdr;

	nbytes = read(ifd, &ehdr, sizeof ehdr);
	if (nbytes == -1)
		err(1, "%s", iname);
	if (nbytes != sizeof ehdr)
		return 0;

	if (lseek(ifd, 0, SEEK_SET) != 0)
		err(1, "%s", iname);

	return IS_ELF(ehdr);
}

u_long
copy_elf(int ifd, const char *iname, int ofd, const char *oname, u_long crc,
    struct image_header *ih)
{
	ssize_t nbytes;
	Elf_Ehdr ehdr;
	Elf_Phdr phdr;
	Elf_Addr vaddr;
	int i;

	nbytes = read(ifd, &ehdr, sizeof ehdr);
	if (nbytes == -1)
		err(1, "%s", iname);
	if (nbytes != sizeof ehdr)
		return 0;

	for (i = 0; i < ehdr.e_phnum; i++) {
#ifdef DEBUG
		fprintf(stderr, "phdr %d/%d\n", i, ehdr.e_phnum);
#endif
		if (lseek(ifd, ehdr.e_phoff + i * ehdr.e_phentsize, SEEK_SET) ==
		    (off_t)-1)
			err(1, "%s", iname);
		if (read(ifd, &phdr, sizeof phdr) != sizeof(phdr))
			err(1, "%s", iname);

#ifdef DEBUG
		fprintf(stderr, "vaddr %p offset %p filesz %p memsz %p\n",
		    phdr.p_vaddr, phdr.p_offset, phdr.p_filesz, phdr.p_memsz);
#endif
		if (i == 0)
			vaddr = phdr.p_vaddr;
		else if (vaddr != phdr.p_vaddr) {
#ifdef DEBUG
			fprintf(stderr, "gap %p->%p\n", vaddr, phdr.p_vaddr);
#endif
			/* fill the gap between the previous phdr if any */
			crc = fill_zeroes(ofd, oname, crc, ih,
			    phdr.p_vaddr - vaddr);
			vaddr = phdr.p_vaddr;
		}

		if (phdr.p_filesz != 0) {
#ifdef DEBUG
			fprintf(stderr, "copying %p from infile %p\n",
			    phdr.p_filesz, phdr.p_offset);
#endif
			if (lseek(ifd, phdr.p_offset, SEEK_SET) == (off_t)-1)
				err(1, "%s", iname);
			crc = copy_data(ifd, iname, ofd, oname, crc, ih,
			    phdr.p_filesz);
			if (phdr.p_memsz - phdr.p_filesz != 0) {
#ifdef DEBUG
				fprintf(stderr, "zeroing %p\n",
				    phdr.p_memsz - phdr.p_filesz);
#endif
				crc = fill_zeroes(ofd, oname, crc, ih,
				    phdr.p_memsz - phdr.p_filesz);
			}
		}
		/*
		 * If p_filesz == 0, this is likely .bss, which we do not
		 * need to provide. If it's not the last phdr, the gap
		 * filling code will output the necessary zeroes anyway.
		 */
		vaddr += phdr.p_memsz;
	}

	return crc;
}

u_long
copy_data(int ifd, const char *iname, int ofd, const char *oname, u_long crc,
    struct image_header *ih, Elf_Word size)
{
	ssize_t nbytes, chunk;
	char buf[BUFSIZ];

	while (size != 0) {
		chunk = size > BUFSIZ ? BUFSIZ : size;
		nbytes = read(ifd, buf, chunk);
		if (nbytes != chunk)
			err(1, "%s", iname);
		if (write(ofd, buf, nbytes) != nbytes)
			err(1, "%s", oname);
		crc = crc32(crc, buf, nbytes);
		ih->ih_size += nbytes;
		size -= nbytes;
	}

	return crc;
}

u_long
fill_zeroes(int ofd, const char *oname, u_long crc, struct image_header *ih,
    Elf_Word size)
{
	ssize_t nbytes, chunk;
	char buf[BUFSIZ];

	memset(buf, 0, BUFSIZ);
	while (size != 0) {
		chunk = size > BUFSIZ ? BUFSIZ : size;
		nbytes = write(ofd, buf, chunk);
		if (nbytes != chunk)
			err(1, "%s", oname);
		crc = crc32(crc, buf, nbytes);
		ih->ih_size += nbytes;
		size -= nbytes;
	}

	return crc;
}

u_long
copy_raw(int ifd, const char *iname, int ofd, const char *oname, u_long crc,
    struct image_header *ih)
{
	ssize_t nbytes;
	char buf[BUFSIZ];

	while ((nbytes = read(ifd, buf, sizeof buf)) != 0) {
		if (nbytes == -1)
			err(1, "%s", iname);
		if (write(ofd, buf, nbytes) != nbytes)
			err(1, "%s", oname);
		crc = crc32(crc, buf, nbytes);
		ih->ih_size += nbytes;
	}

	return crc;
}

void
usage(void)
{
	const struct arch_map *mapptr;
	const struct os_map *osmapptr;

	(void)fprintf(stderr,
	    "usage: %s [-a arch] [-e entry] [-l loadaddr] [-n name] [-o os] "
	    "[-t type] infile outfile\n", __progname);
	(void)fprintf(stderr,
	    "arch is one of:");
	for (mapptr = archmap; mapptr->arch; mapptr++)
		(void)fprintf(stderr, " %s", mapptr->arch);
	(void)fprintf(stderr, "\n");
	(void)fprintf(stderr,
	    "os is one of:");
	for (osmapptr = osmap; osmapptr->arch; osmapptr++)
		(void)fprintf(stderr, " %s", osmapptr->arch);
	(void)fprintf(stderr, "\n");
	
	exit(1);
}
