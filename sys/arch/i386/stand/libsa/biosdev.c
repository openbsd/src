/*	$OpenBSD: biosdev.c,v 1.2 1997/03/31 03:12:09 weingart Exp $	*/

/*
 * Copyright (c) 1996 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <libsa.h>
#include "biosdev.h"

dev_t	maj, unit, part;

int
biosstrategy(void *devdata, int rw,
	daddr_t blk, size_t size, void *buf, size_t *rsize)
{
	int	error = 0;
	u_int	dinfo;
	register size_t i, nsect;
	void *local_buf, *lb;			/* 1MB real mode stuff */

	/* We need a buffer below 1MB 
	 *
	 * Note: not really needed for the gzip stuff,
	 * as it already allocates buffers below 1MB,
	 * but just to make sure (in case using oread()
	 */
	lb = local_buf = alloc(size);

#ifdef	BIOS_DEBUG
	printf("biosstrategy: %s %d bytes @ %d -> %p\n",
		(rw==F_READ?"reading":"writing"), size, blk, buf);
#endif

	dinfo = biosdinfo(bootdev);
	nsect = (size + DEV_BSIZE-1) / DEV_BSIZE;
	for (i = 0; error == 0 && i < nsect; ) {
		register int	cyl, hd, sect, n;

		btochs(blk, cyl, hd, sect, 
			BIOSNHEADS(dinfo), BIOSNSECTS(dinfo));
		if ((sect + (nsect - i)) >= BIOSNSECTS(dinfo))
			n = BIOSNSECTS(dinfo) - sect;
		else
			n = nsect - i;
#ifdef	BIOS_DEBUG
		printf("biosread: dev=%x, cyl=%d, hd=%d, sc=%d, n=%d, buf=%lx",
			bootdev, cyl, hd, sect, n, (u_long)local_buf);
#endif
		if (rw == F_READ)
			error = biosread (bootdev, cyl, hd, sect, n, local_buf);
		else
			error = bioswrite(bootdev, cyl, hd, sect, n, local_buf);
#ifdef	BIOS_DEBUG
		printf(", ret=%x\n", error);
#endif
		local_buf += n * DEV_BSIZE;
		i += n;
		blk += n;
	}

	/* Copy to final location */
	memcpy(buf, lb, size);
	free(lb, size);

	/* Amount we read */
	*rsize = i * DEV_BSIZE;

	return error;
}

int
biosopen(struct open_file *f, ...)
{
	va_list ap;
	register char	*cp, *fname, **file;
	static const char *devs[] = {"wd", "hd", "fd", "wt", "sd", NULL};

	va_start(ap, f);
	cp = fname = *(file = va_arg(ap, char **));
	va_end(ap);

	/* search for device specification */
	while (*cp != 0 && *cp != '(')
		cp++;
	if (*cp != 0) {
		if (*cp++ == '(') {
			char **devp = (char **)devs;

			for (; *devp != NULL && 
				(fname[0] != (*devp)[0] ||
				 fname[1] != (*devp)[1]); devp++)
				;
			if (*devp == NULL) {
				printf("Unknown device");
				errno = ENXIO;
				return -1;
			}
			maj = devp - (char **)devs;
		}
		/* check syntax */
		if (cp[1] != ',' || cp[3] != ')') {
			printf("Syntax error\n");
			errno = EINVAL;
			return -1;
		}

		/* get unit */
		if ('0' <= *cp && *cp <= '9')
			unit = *cp++ - '0';
		else {
			printf("Bad unit number\n");
			errno = ENXIO;
			return -1;
		}
		cp++;	/* skip ',' */
		/* get partition */
		if ('a' <= *cp && *cp <= 'p')
			part = *cp++ - 'a';
		else {
			printf("Bad partition id\n");
			errno = ENXIO;
			return -1;
		}
		cp++;	/* skip ')' */
		if (*cp != 0)
			*file = cp;

		switch (maj) {
		case 0:
		case 4:
			bootdev = unit | 0x80;
			break;
		case 1:
			bootdev = unit | 0x80;
			unit = 0;
			break;
		case 2:
			bootdev = unit;
			break;
		case 3:
#ifdef DEBUG
			printf("Wangtek is unsupported\n");
#endif
			errno = ENXIO;
			return -1;
		default:
			break;
		}

	}

	return 0;
}

int
biosclose(struct open_file *f)
{

	return 0;
}

int
biosioctl(struct open_file *f, u_long cmd, void *data)
{

	return 0;
}

