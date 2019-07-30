/*	$OpenBSD: boot.c,v 1.24 2014/02/22 20:27:21 miod Exp $ */

/*
 * Copyright (c) 2004 Opsycon AB, www.opsycon.se.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/stat.h>
#define _KERNEL
#include <sys/fcntl.h>
#undef _KERNEL

#include <lib/libkern/libkern.h>
#include <stand.h>

#include <mips64/arcbios.h>
#include <mips64/cpu.h>

#include <sys/exec_elf.h>
#undef ELFSIZE
#include "loadfile.h"

void	 dobootopts(int, char **);
void	 loadrandom(const char *, const char *, void *, size_t);
char	*strstr(char *, const char *);	/* strstr.c */

enum {
	AUTO_NONE,
	AUTO_YES,
	AUTO_NO,
	AUTO_MINI,
	AUTO_DEBUG
} bootauto = AUTO_NONE;
char *OSLoadPartition = NULL;
char *OSLoadFilename = NULL;

int	IP;

char   rnddata[BOOTRANDOM_MAX];

#include "version"

/*
 * OpenBSD/sgi Boot Loader.
 */
void
boot_main(int argc, char *argv[])
{
	u_long marks[MARK_MAX];
	u_int64_t *esym;
	char line[1024];
	u_long entry;
	int fd;
	extern int arcbios_init(void);

	IP = arcbios_init();
	printf("\nOpenBSD/sgi-IP%d ARCBios boot version %s\n", IP, version);
	/* we want to print IP20 but load IP22 */
	if (IP == 20)
		IP = 22;

	for (entry = 0; entry < argc; entry++)
		printf("arg %d: %s\n", entry, argv[entry]);

	dobootopts(argc, argv);
	if (OSLoadPartition == NULL) {
		/*
		 * Things are probably horribly wrong, or user has no idea
		 * what's he's doing.  Be nice lads and try to provide
		 * working defaults, which ought to work on all systems.
		 */
		OSLoadPartition = "disk(0)part(0)";
	}
	strlcpy(line, OSLoadPartition, sizeof(line));
	if (OSLoadFilename != NULL)
		strlcat(line, OSLoadFilename, sizeof(line));

	printf("Boot: %s\n", line);

	/*
	 * Try and load randomness if booting from a disk.
	 */
	
	if (bootauto != AUTO_MINI &&
	    strstr(OSLoadPartition, "bootp(") == NULL &&
	    strstr(OSLoadPartition, "cdrom(") == NULL) {
		loadrandom(OSLoadPartition, BOOTRANDOM, rnddata,
		    sizeof(rnddata));
	}

	/*
	 * Load the kernel and symbol table.
	 */

	marks[MARK_START] = 0;
	if ((fd = loadfile(line, marks, LOAD_KERNEL | COUNT_KERNEL)) != -1) {
		(void)close(fd);

		entry = marks[MARK_ENTRY];
#ifdef __LP64__
		esym = (u_int64_t *)marks[MARK_END];
#else
#undef  CKSEG0_BASE
#define CKSEG0_BASE	0xffffffff80000000ULL
		esym = (u_int64_t *)(uint32_t)PHYS_TO_CKSEG0(marks[MARK_END]);
#endif

		if (entry != 0)
			((void (*)())entry)(argc, argv, esym);
	}

	/* We failed to load the kernel. */
	panic("Boot FAILED!");
	/* NOTREACHED */
}

__dead void
_rtt()
{
	Bios_EnterInteractiveMode();
	for (;;) ;
}

/*
 * Decode boot options.
 */
void
dobootopts(int argc, char **argv)
{
	static char filenamebuf[1 + 32];
	char *SystemPartition = NULL;
	char *cp, *sep;
	int i;
	char *writein = NULL;

	for (i = 1; i < argc; i++) {
		cp = argv[i];
		if (cp == NULL)
			continue;
		if (strncmp(cp, "OSLoadOptions=", 14) == 0) {
			if (strcmp(&cp[14], "auto") == 0)
				bootauto = AUTO_YES;
			else if (strcmp(&cp[14], "single") == 0)
				bootauto = AUTO_NO;
			else if (strcmp(&cp[14], "mini") == 0)
				bootauto = AUTO_MINI;
			else if (strcmp(&cp[14], "debug") == 0)
				bootauto = AUTO_DEBUG;
		} else if (strncmp(cp, "OSLoadPartition=", 16) == 0)
			OSLoadPartition = &cp[16];
		else if (strncmp(cp, "OSLoadFilename=", 15) == 0)
			OSLoadFilename = &cp[15];
		else if (strncmp(cp, "SystemPartition=", 16) == 0)
			SystemPartition = &cp[16];
		else {
			/*
			 * Either a boot-related environment variable, or
			 * a boot write-in (boot path or options to the
			 * program being loaded).
			 */
			if (*cp == '-')
				continue;	/* options */
			if (strchr(cp, '=') != NULL)
				continue;	/* variable (or bad choice */
						/* of filename) */
			if (writein == NULL)
				writein = cp;
		}
	}

	switch (bootauto) {
	case AUTO_NONE:
		/* If "OSLoadOptions=" is missing, use boot path if given. */
		if (writein != NULL) {
			/* check for a possible path component */
			sep = strchr(writein, '(');
			if (sep != NULL && strchr(sep, ')') != NULL) {
				/* looks like this is a full path */
				OSLoadPartition = "";
			} else {
				/* relative path, keep OSLoadPartition */
			}
			OSLoadFilename = writein;
		}
		break;
	case AUTO_MINI:
	    {
		static char loadpart[64];
		char *p;

		strlcpy(loadpart, argv[0], sizeof loadpart);
		if ((p = strstr(loadpart, "partition(8)")) != NULL) {
			p += strlen("partition(");
		} else if (strncmp(loadpart, "dksc(", 5) == 0) {
			p = strstr(loadpart, ",8)");
			if (p != NULL)
				p++;
		} else
			p = NULL;

		if (p != NULL) {
			p[0] = '0';
			p[2] = '\0';
			snprintf(filenamebuf, sizeof filenamebuf,
			    "/bsd.rd.IP%d", IP);
			OSLoadPartition = loadpart;
			OSLoadFilename = filenamebuf;
		}
	    }
		break;
	default:
		break;
	}
}

/*
 * Prevent loading a wrong kernel.
 */
int
check_phdr(void *v)
{
	Elf64_Phdr *phdr = (Elf64_Phdr *)v;
	uint64_t addr;

	switch (IP) {
	case 22:
		addr = 0xffffffff88000000ULL >> 24;
		break;
	case 26:
		addr = 0xa800000008000000ULL >> 24;
		break;
	case 27:
		addr = 0xa800000000000000ULL >> 24;
		break;
	case 28:
	case 30:
		addr = 0xa800000020000000ULL >> 24;
		break;
	case 32:
		addr = 0xffffffff80000000ULL >> 24;
		break;
	default:
		/*
		 * If the system could not be identified, accept any
		 * address and hope the user knows what's he's doing.
		 */
		return 0;
	}

	if ((phdr->p_vaddr >> 24) != addr) {
		/* I'm sorry Dave, I can't let you do that. */
		printf("This kernel does not seem to be compiled for this"
		    " machine type.\nYou need to boot an IP%d kernel.\n", IP);
		return 1;
	}

	return 0;
}

/*
 * Load the saved randomness file.
 */
void
loadrandom(const char *partition, const char *name, void *buf, size_t buflen)
{
	char path[MAXPATHLEN];
	struct stat sb;
	int fd;

	strlcpy(path, partition, sizeof path);
	strlcat(path, name, sizeof path);

	fd = open(path, O_RDONLY);
	if (fd == -1) {
		if (errno != EPERM)
			printf("cannot open %s: %s\n", path, strerror(errno));
		return;
	}
	if (fstat(fd, &sb) == -1 || sb.st_uid != 0 || !S_ISREG(sb.st_mode) ||
	    (sb.st_mode & (S_IWOTH|S_IROTH)))
		goto fail;
	(void) read(fd, buf, buflen);
fail:
	close(fd);
}
