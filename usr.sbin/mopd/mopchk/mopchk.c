/*	$OpenBSD: mopchk.c,v 1.4 1998/03/04 20:21:54 deraadt Exp $

/*
 * Copyright (c) 1995-96 Mats O Jansson.  All rights reserved.
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
 *	This product includes software developed by Mats O Jansson.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#ifndef LINT
static char rcsid[] = "$OpenBSD: mopchk.c,v 1.4 1998/03/04 20:21:54 deraadt Exp $";
#endif

/*
 * mopchk - MOP Check Utility
 *
 * Usage:	mopchk [-a] [-v] [filename...]
 */

#include "os.h"
#include "common/common.h"
#include "common/mopdef.h"
#include "common/device.h"
#include "common/pf.h"
#include "common/file.h"

/*
 * The list of all interfaces that are being listened to.  rarp_loop()
 * "selects" on the descriptors in this list.
 */
struct if_info *iflist;

#ifdef NO__P
void   Usage         (/* void */);
void   mopProcess    (/* struct if_info *, u_char * */);
#else
void   Usage         __P((void));
void   mopProcess    __P((struct if_info *, u_char *));
#endif

int     AllFlag = 0;		/* listen on "all" interfaces  */
int	VersionFlag = 0;	/* Show version */
int	promisc = 0;		/* promisc mode not needed */
char	*Program;
extern char version[];

int
main(argc, argv)
	int     argc;
	char  **argv;
{
	int     op, i, fd;
	char   *filename;
	struct if_info *ii;
	int	err, aout;

	extern int optind, opterr;

	if ((Program = strrchr(argv[0], '/')))
		Program++;
	else
		Program = argv[0];
	if (*Program == '-')
		Program++;

	/* All error reporting is done through syslogs. */
	openlog(Program, LOG_PID | LOG_CONS, LOG_DAEMON);

	opterr = 0;
	while ((op = getopt(argc, argv, "av")) != -1) {
		switch (op) {
		case 'a':
			AllFlag++;
			break;
		case 'v':
			VersionFlag++;
			break;
		default:
			Usage();
			/* NOTREACHED */
		}
	}
	
	if (VersionFlag)
		printf("%s: Version %s\n",Program,version);

	if (AllFlag) {
		if (VersionFlag)
			printf("\n");
		iflist = NULL;
		deviceInitAll();
		if (iflist == NULL) {
			printf("No interface\n");
		} else {
			printf("Interface Address\n");
			for (ii = iflist; ii; ii = ii->next) {
				printf("%-9s %x:%x:%x:%x:%x:%x\n",
				       ii->if_name,
				       ii->eaddr[0],ii->eaddr[1],ii->eaddr[2],
				       ii->eaddr[3],ii->eaddr[4],ii->eaddr[5]);
			}
		}
	}
	
	if (VersionFlag || AllFlag)
		i = 1;
	else
		i = 0;

	while (argc > optind) {
		if (i)	printf("\n");
		i++;
		filename = argv[optind++];
		printf("Checking: %s\n",filename);
		fd = open(filename, O_RDONLY, 0);
		if (fd == -1) {
			printf("Unknown file.\n");
		} else {
			err = CheckAOutFile(fd);
			if (err == 0) {
				if (GetAOutFileInfo(fd, 0, 0, 0, 0,
						    0, 0, 0, 0, &aout) < 0) {
					printf("Some failure in GetAOutFileInfo\n");
					aout = -1;
				}
			} else {
				aout = -1;
			}
			if (aout == -1)
				err = CheckMopFile(fd);
			if (aout == -1 && err == 0) {
				if (GetMopFileInfo(fd, 0, 0) < 0) {
					printf("Some failure in GetMopFileInfo\n");
				}
			};
		}
	}
	return 0;
}

void
Usage()
{
	(void) fprintf(stderr, "usage: %d [-a] [-v] [filename...]\n",Program);
	exit(1);
}

/*
 * Process incomming packages, NOT. 
 */
void
mopProcess(ii, pkt)
	struct if_info *ii;
	u_char *pkt;
{
}

