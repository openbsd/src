/*	$NetBSD: diskio.c,v 1.2 1996/01/16 15:15:16 leo Exp $	*/

/*
 * Copyright (c) 1995 Waldi Ravens.
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
 *        This product includes software developed by Waldi Ravens.
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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <xhdi.h>
#include "libtos.h"
#include "aptck.h"
#include "ahdilbl.h"
#include <osbind.h>

struct pun_info {
	u_int16_t	puns;
	u_int8_t	pun[16];
	u_int32_t	part_start[16];
	u_int32_t	P_cookie;
	u_int32_t	*P_cookptr;
	u_int16_t	P_version;
	u_int16_t	P_max_sector;
	u_int32_t	reserved[16];
};

static char *	strbd    PROTO((char *, ...));
static int	setmami  PROTO((disk_t *, char *));
static int	setnames PROTO((disk_t *));
static int	setsizes PROTO((disk_t *));
static int	ahdi_compatible PROTO((void));

disk_t *
disk_open(name)
	char	*name;
{
	disk_t	*dd;
	
	dd = xmalloc(sizeof *dd);
	memset(dd, 0, sizeof *dd);

	if (setmami(dd, name) || setnames(dd) || setsizes(dd)) {
		disk_close(dd);
		return(NULL);
	}
	return(dd);
}

void
disk_close(dd)
	disk_t	*dd;
{
	if (dd) {
		free(dd->product);
		free(dd->sname);
		free(dd->fname);
		free(dd->roots);
		free(dd->parts);
		free(dd);
	}
}

void *
disk_read(dd, start, count)
	disk_t	*dd;
	u_int	start,
		count;
{
	char	*buffer;
	int	bdev;
	long	e;

	buffer = xmalloc(count * dd->bsize);

	e = XHReadWrite(dd->major, dd->minor, 0, start, count, buffer);
	if (!e)
		return(buffer);
	if (e == -32 || (e == -1 && XHGetVersion() == -1)) {
		if (!ahdi_compatible())
			fatal(-1, "AHDI 3.0 compatible harddisk driver required");
		bdev = BIOSDEV(dd->major, dd->minor);
		if (bdev && !bios_read(buffer, start, count, bdev))
			return(buffer);
	}

	free(buffer);
	return(NULL);
}

int
disk_write(dd, start, count, buffer)
	disk_t	*dd;
	u_int	start,
		count;
	void	*buffer;
{
	int	bdev;
	long	e;

	e = XHReadWrite(dd->major, dd->minor, 1, start, count, buffer);
	if (e == -32 || (e == -1 && XHGetVersion() == -1)) {
		if (!ahdi_compatible())
			fatal(-1, "AHDI 3.0 compatible harddisk driver required");
		bdev = BIOSDEV(dd->major, dd->minor);
		if (bdev)
			e = bios_write(buffer, start, count, bdev);
	}

	return((int)e);
}

static int
ahdi_compatible()
{
	static int	ahdi_compat;

	if (!ahdi_compat) {
		long		oldsp = Super(0L);
		struct pun_info	*punp = *((struct pun_info **)0x0516);
		Super(oldsp);
		if (punp && punp->P_cookie == 0x41484449
				&& punp->P_cookptr == &punp->P_cookie
				&& punp->P_version >= 0x0300)
			ahdi_compat = 1;
	}
	return(ahdi_compat);
}

static int
setmami(dd, name)
	disk_t	*dd;
	char	*name;
{
	char	*p = name;
	u_int	target, lun;
	bus_t	bus;

	if (*p == 'i') {
		bus = IDE;
		if (*++p < '0' || *p > '1') {
			if (*p)
				error(-1, "%s: invalid IDE target `%c'", name, *p);
			else
				error(-1, "%s: missing IDE target", name);
			return(-1);
		}
		target = *p++ - '0';
		lun = 0;
	} else {
		char	*b;

		if (*p == 'a') {
			bus = ACSI;
			b = "ACSI";
		} else if (*p == 's') {
			bus = SCSI;
			b = "SCSI";
		} else {
			error(-1, "%s: invalid DISK argument", name);
			return(-1);
		}
		if (*++p < '0' || *p > '7') {
			if (*p)
				error(-1, "%s: invalid %s target `%c'", name, b, *p);
			else
				error(-1, "%s: missing %s target", name, b);
			return(-1);
		}
		target = *p++ - '0';

		if (*p < '0' || *p > '7') {
			if (*p) {
				error(-1, "%s: invalid %s lun `%c'", name, b, *p);
				return(-1);
			}
			lun = 0;
		} else
			lun = *p++ - '0';
	}
	if (*p) {
		error(-1, "%s: invalid DISK argument", name);
		return(-1);
	}
	dd->major = MAJOR(bus, target, lun);
	dd->minor = MINOR(bus, target, lun);
	return(0);
}

static int
setnames(dd)
	disk_t	*dd;
{
	char	sn[16], us[16], ls[16], *bs;
	int	b, u, l;

	b = BUS(dd->major, dd->minor);
	u = TARGET(dd->major, dd->minor);
	l = LUN(dd->major, dd->minor);

	switch (b) {
	case IDE:	bs = "IDE";
			break;
	case ACSI:	bs = "ACSI";
			break;
	case SCSI:	bs = "SCSI";
			break;
	default:	error(-1, "invalid bus no. %d", b);
			return(-1);
	}

	if (u < 0 || u > 7 || (b == IDE && u > 1)) {
		error(-1, "invalid %s target `%d'", bs, u);
		return(-1);
	}
	sprintf(us, " target %d", u);

	if (l < 0 || l > 7 || (b == IDE && l > 0)) {
		error(-1, "invalid %s lun `%d'", bs, l);
		return(-1);
	}
	if (b == IDE) {
		sprintf(sn, "i%d", u);
		ls[0] = '\0';
	} else {
		sprintf(sn, "%c%d%d", tolower(*bs), u, l);
		sprintf(ls, " lun %d", l);
	}

	dd->fname = strbd(bs, us, ls, NULL);
	dd->sname = strbd(sn, NULL);
	return(0);
}

static int
setsizes(dd)
	disk_t	*dd;
{
	if (XHGetVersion() != -1) {
		char	*p, prod[1024];

		if (XHInqTarget2(dd->major, dd->minor, &dd->bsize, NULL, prod, sizeof(prod))) {
			if (XHInqTarget(dd->major, dd->minor, &dd->bsize, NULL, prod)) {
				error(-1, "%s: device not configured", dd->sname);
				return(-1);
			}
		}
		p = strrchr(prod, '\0');
		while (isspace(*--p))
			*p = '\0';
		dd->product = strbd(prod, NULL);
		if (!XHGetCapacity(dd->major, dd->minor, &dd->msize, &dd->bsize))
			return(0);
	} else {
		dd->product = strbd("unknown", NULL);
		dd->bsize = AHDI_BSIZE;		/* XXX */
	}

	/* Trial&error search for last sector on medium */
	{
		u_int	u, l, m;
		void	*p, (*oldvec)();

		/* turn off etv_critic handler */
		oldvec = Setexc(257, bios_critic);

		u = (u_int)-2; l = 0;
		while (u != l) {
			m = l + ((u - l + 1) / 2);
			p = disk_read(dd, m, 1);
			free(p);
			if (p == NULL)
				u = m - 1;
			else
				l = m;
		}

		/* turn on etv_critic handler */
		(void)Setexc(257, oldvec);

		if (l) {
			dd->msize = l + 1;
			return(0);
		}
		error(-1, "%s: device not configured", dd->sname);
		return(-1);
	}
}

char *
strbd(string1)
	char	*string1;
{
	char		*p, *result;
	size_t		length = 1;
	va_list		ap;

	va_start(ap, string1);
	for (p = string1; p; p = va_arg(ap, char *))
		length += strlen(p);
	va_end(ap);

	*(result = xmalloc(length)) = '\0';

	va_start(ap, string1);
	for (p = string1; p; p = va_arg(ap, char *))
		strcat(result, p);
	va_end(ap);

	return(result);
}
