/*      $OpenBSD: elf2olf.c,v 1.6 2001/11/19 19:02:13 mpech Exp $	*/
/*
 * Copyright (c) 1996 Erik Theisen.  All rights reserved.
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
#ifndef lint
static char copyright[] = 
"@(#) Copyright (c) 1996 Erik Theisen.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char rcsid[] = "@(#) $Id: elf2olf.c,v 1.6 2001/11/19 19:02:13 mpech Exp $";
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/param.h>

#include <olf_abi.h>

int   retval  = 0;
int   olf2elf;
char *progname;
int   verbose;
int   opsys = OS_ID;

char *os_namev[] = ONAMEV;

/* Handle endianess */
#define word(x,y)((y == ELFDATA2LSB) ? ntohl(htonl(x)) : ntohl(x))
#define half(x,y)((y == ELFDATA2LSB) ? ntohs(htons(x)) : ntohs(x))

void usage(void);
void pwarn(char *, char *, int);

int
main(int argc, char*argv[])
{
    extern char *optarg;
    extern int optind;
    int ch, i, okay;
    char *opstring;

    int fd;
    struct stat st;
    Elf32_Ehdr ehdr;
    Elf32_Shdr shdr;
    int e;
    
    if ((progname = strrchr(*argv, '/')))
	++progname;
    else
	progname = *argv;

    if (strstr(progname, "olf2elf"))
	olf2elf = 1;
    
    /* 
     * Process cmdline
     */
    opstring = olf2elf ? "v" : "vo:";
    while((ch = getopt(argc, argv, opstring)) != -1)
	switch(ch) {
	case 'v':
	    verbose = 1;
	    break;
	case 'o':
	    for (i = 1; i <= OOS_NUM; i++) {
	        if (os_namev[i] == NULL) {
		    fprintf(stderr, 
			    "%s: illegal -o argument -- %s\n", 
			    progname, optarg);
		    usage();
		}
		else if (strcmp(optarg, os_namev[i]) == 0) {
		    opsys = i;
		    break;
		}
	    }
	    break;
	default:
	    usage();
	}
    argc -= optind;
    argv += optind;

    if (argc == 0)
	usage();

    /*
     * Process file(s)
     */
    do {
	okay = 0;

	if ((fd = open(*argv, O_RDWR | O_EXLOCK, 0)) > 0 &&
	    lseek(fd, (off_t)0, SEEK_SET) == 0 &&
	    fstat(fd, &st) == 0) {

	    /* Make sure this is a 32bit ELF or OLF version 1 file */
	    if (read(fd, &ehdr, sizeof(Elf32_Ehdr)) == sizeof(Elf32_Ehdr)&&
		(IS_ELF(ehdr) || IS_OLF(ehdr)) &&
		ehdr.e_ident[EI_CLASS] == ELFCLASS32 &&
		ehdr.e_ident[EI_VERSION] == 1) {

		/* Is this elf2olf? */
		if(!olf2elf) {

		    /* Tag, your it... */
		    ehdr.e_ident[OI_MAG0]    = OLFMAG0;
		    ehdr.e_ident[OI_MAG1]    = OLFMAG1;
		    ehdr.e_ident[OI_MAG2]    = OLFMAG2;
		    ehdr.e_ident[OI_MAG3]    = OLFMAG3;
		    ehdr.e_ident[OI_OS]      = opsys;
		    ehdr.e_ident[OI_DYNAMIC] = ODYNAMIC_N;
		    ehdr.e_ident[OI_STRIP]   = OSTRIP;

		    /* We'll need this endian */
		    e = ehdr.e_ident[EI_DATA];

		    /* Now we need to figure out wether or */
		    /* not we're really stripped. */
		    if (lseek(fd, (off_t)word(ehdr.e_shoff, e),
			      SEEK_SET) == word(ehdr.e_shoff, e)) {

			/* 
			 * search through section header table
			 * looking for a section header of type
			 * SHT_SYMTAB and SHT_DYNAMIC. If there is 
			 * one present we're NOT stripped and/or 
			 * dynamic.
			 */
			for (i = 0; i < half(ehdr.e_shnum, e); i++) {
			    if (read(fd, &shdr, sizeof(Elf32_Shdr)) == sizeof(Elf32_Shdr)){
				if (word(shdr.sh_type, e) == SHT_SYMTAB)
				    ehdr.e_ident[OI_STRIP] = OSTRIP_N;
				else if (word(shdr.sh_type, e) == SHT_DYNAMIC)
				    ehdr.e_ident[OI_DYNAMIC] = ODYNAMIC;
			    } else
				pwarn(progname, *argv, errno);
			} /* while less than number of section headers */

			/* We're ready to modify */
			okay = 1;

		    } else /* Bogus section header table seek */
			pwarn(progname, *argv, errno);

		} else { /* olf2elf */
		    ehdr.e_ident[EI_MAG0]    = ELFMAG0;
		    ehdr.e_ident[EI_MAG1]    = ELFMAG1;
		    ehdr.e_ident[EI_MAG2]    = ELFMAG2;
		    ehdr.e_ident[EI_MAG3]    = ELFMAG3;
		    ehdr.e_ident[OI_OS]      = 0;
		    ehdr.e_ident[OI_DYNAMIC] = 0;
		    ehdr.e_ident[OI_STRIP]   = 0;

		    okay = 1;
		} /* olf2elf */
	    } else /* Bogus non-ELF file encountered */
		pwarn(progname, *argv, ENOEXEC);

	    /*
	     * Do It.
	     */
	    if (okay) {
		if (lseek(fd, (off_t)0, SEEK_SET) == 0) {
		    if (write(fd, &ehdr, sizeof(Elf32_Ehdr)) == sizeof(Elf32_Ehdr)) {
			if (verbose) {
			    if (!olf2elf) {
				printf("ELF %s => OLF %d-bit %s %s linked %s OLF.\n",
				       *argv,
				       (ehdr.e_ident[OI_CLASS] == OLFCLASS32)?\
				           32 : 64,
				       os_namev[ehdr.e_ident[OI_OS]],
				       ehdr.e_ident[OI_DYNAMIC] ? \
				           "dynamically" : "statically", 
				       !ehdr.e_ident[OI_STRIP] ?
				           "stripped" : "unstripped");
			    } else
				printf("OLF %s => ELF.\n", *argv);
			}
		    } else /* bad write */
			pwarn(progname, *argv, errno);	
		} else /* bad seek */
		    pwarn(progname, *argv, errno);
	    } /* okay? */
	    fsync(fd);
	    close(fd);

	} else /* couldn't handle file */
	    pwarn(progname, *argv, errno);
    } while (*(++argv) != NULL);
    

    return (retval);
}

void
pwarn(name, fname, errval)
    char *name;
    char *fname;
    int errval;
{
    fprintf(stderr, "%s: %s: %s.\n", name, fname, strerror(errval));
    retval = 1;
}

void
usage()
{
    int i;
    int col = 8;

    if (olf2elf) {
	fprintf(stderr,	"usage: %s [-v] file ...\n", progname);
    } else {
	fprintf(stderr,	"usage: %s [-v] [-o opsys] elffile ...\n", progname);
	fprintf(stderr, "where opsys is:\n\t");
	for (i = 1; os_namev[i] != NULL; i++) {
	    col = col + strlen(os_namev[i]) + 2;
	    if (col > 78) {
		fprintf(stderr, "\n\t");
		col = 8;
	    }
	    fprintf(stderr, "%s%s", os_namev[i],
		os_namev[i+1] ? ", " : "\n");
	}
    }
    exit(1);
}


