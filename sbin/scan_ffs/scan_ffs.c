/*	$OpenBSD: scan_ffs.c,v 1.4 1998/03/28 01:18:38 deraadt Exp $	*/

/*
 * Copyright (c) 1998 Niklas Hallqvist, Tobias Weingartner
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
 *    This product includes software developed by Tobias Weingartner.
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/fcntl.h>
#include <ufs/ffs/fs.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <err.h>
#include <util.h>

#define SBCOUNT 64		/* XXX - Should be configurable */

/* Flags to control ourselves... */
#define FLAG_VERBOSE		1
#define FLAG_SMART		2
#define FLAG_LABELS		4

int
ufsscan(fd, beg, end, flags)
	int fd;
	daddr_t beg, end;
	int flags;
{
	static char lastmount[MAXMNTLEN];
	static u_int8_t buf[SBSIZE * SBCOUNT];
	struct fs *sb;
	daddr_t blk, lastblk;
	int n;

	lastblk = -1;
	memset(lastmount, 0, MAXMNTLEN);

	for(blk = beg; blk <= ((end<0)?blk:end); blk += (SBCOUNT*SBSIZE/512)){
		memset(buf, 0, SBSIZE * SBCOUNT);
		if (lseek(fd, blk * 512, SEEK_SET) < 0)
		    err(1, "lseek");
		if (read(fd, buf, SBSIZE * SBCOUNT) < 0)
			err(1, "read");

		for(n = 0; n < (SBSIZE * SBCOUNT); n += 512){
			sb = (struct fs*)(&buf[n]);
			if (sb->fs_magic == FS_MAGIC) {
				if (flags & FLAG_VERBOSE)
					printf("block %d id %x,%x size %d\n",
					    blk + (n/512), sb->fs_id[0],
					    sb->fs_id[1], sb->fs_size);

				if (((blk+(n/512)) - lastblk) == (SBSIZE/512)) {
					if (flags & FLAG_LABELS ) {
						printf("X: %d %d 4.2BSD %d %d %d # %s\n",
						    sb->fs_size * sb->fs_fsize / 512,
						    blk+(n/512)-(2*SBSIZE/512),
						    sb->fs_fsize, sb->fs_bsize,
						    sb->fs_cpg, lastmount);
					} else {
						printf("ffs at %d size %d mount %s time %s",
						    blk+(n/512)-(2*SBSIZE/512),
						    sb->fs_size * sb->fs_fsize,
						    lastmount, ctime(&sb->fs_time));
					}

					if (flags & FLAG_SMART) {
						int size = sb->fs_size * sb->fs_fsize;

						if ((n + size) < (SBSIZE * SBCOUNT))
							n += size;
						else {
							blk += (size/512 -
							    (SBCOUNT*SBCOUNT));
							break;
						}
					}
				}

				/* Update last potential FS SBs seen */
				lastblk = blk + (n/512);
				memcpy(lastmount, sb->fs_fsmnt, MAXMNTLEN);
			}
		}
	}
	return(0);
}


void
usage()
{
	fprintf(stderr, "usage: scan_ffs [-lsv] [-b begin] [-e end] <device>\n");
	exit(1);
}


int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, fd, flags = 0;
	daddr_t beg = 0, end = -1;

	while ((ch = getopt(argc, argv, "lsvb:e:")) != -1)
		switch(ch) {
		case 'b':
			beg = atoi(optarg);
			break;
		case 'e':
			end = atoi(optarg);
			break;
		case 'v':
			flags |= FLAG_VERBOSE;
			break;
		case 's':
			flags |= FLAG_SMART;
			break;
		case 'l':
			flags |= FLAG_LABELS;
			break;
		default:
			usage();
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	fd = opendev(argv[0], O_RDONLY, OPENDEV_PART, NULL);
	if (fd < 0)
		err(1, "%s", argv[1]);

	return (ufsscan(fd, beg, end, flags));
}
