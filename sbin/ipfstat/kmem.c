/*	$OpenBSD: kmem.c,v 1.14 2000/02/16 22:34:22 kjell Exp $	*/

/*
 * Copyright (C) 1993-1998 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
/*
 * kmemcpy() - copies n bytes from kernel memory into user buffer.
 * returns 0 on success, -1 on error.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include "kmem.h"

#if !defined(lint)
static const char sccsid[] = "@(#)kmem.c	1.4 1/12/96 (C) 1992 Darren Reed";
static const char rcsid[] = "@(#)$IPFilter: kmem.c,v 2.1 1999/08/04 17:30:09 darrenr Exp $";
#endif

static	int	kmemfd = -1;

int	openkmem(nlistf, memf)
char *nlistf, *memf;
{
	if (memf == NULL)
		memf = KMEM;

	if ((kmemfd = open(memf,O_RDONLY)) == -1)
	    {
		perror("kmeminit:open");
		return -1;
	    }
	return kmemfd;
}

int	kmemcpy(buf, pos, n)
register char	*buf;
long	pos;
register int	n;
{
	register int	r;

	if (!n)
		return 0;
	if (kmemfd == -1)
		if (openkmem(nlistf, memf) == -1)
			return -1;
	if (lseek(kmemfd, pos, 0) == -1)
	    {
		perror("kmemcpy:lseek");
		return -1;
	    }
	while ((r = read(kmemfd, buf, n)) < n)
		if (r <= 0)
		    {
			perror("kmemcpy:read");
			return -1;
		    }
		else
		    {
			buf += r;
			n -= r;
		    }
	return 0;
}

int	kstrncpy(buf, pos, n)
register char	*buf;
long	pos;
register int	n;
{
	register int	r;

	if (!n)
		return 0;
	if (kmemfd == -1)
		if (openkmem(nlistf, memf) == -1)
			return -1;
	if (lseek(kmemfd, pos, 0) == -1)
	    {
		perror("kmemcpy:lseek");
		return -1;
	    }
	while (n > 0) {
		r = read(kmemfd, buf, 1);
		if (r <= 0)
		    {
			perror("kmemcpy:read");
			return -1;
		    }
		else
		    {
			if (*buf == '\0')
				break;
			buf++;
			n--;
		    }
	}
	return 0;
}
