/* $OpenBSD: keynote-sigver.c,v 1.5 1999/10/06 20:27:46 angelos Exp $ */
/*
 * The author of this code is Angelos D. Keromytis (angelos@dsl.cis.upenn.edu)
 *
 * This code was written by Angelos D. Keromytis in Philadelphia, PA, USA,
 * in April-May 1998
 *
 * Copyright (C) 1998, 1999 by Angelos D. Keromytis.
 *	
 * Permission to use, copy, and modify this software without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software. 
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, THE AUTHORS MAKES NO
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#if STDC_HEADERS
#include <string.h>
#endif /* STDC_HEADERS */

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif /* HAVE_FCNTL_H */

#if HAVE_IO_H
#include <io.h>
#elif HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_IO_H */

#include "keynote.h"
#include "header.h"

void
sigverusage(void)
{
    fprintf(stderr, "Arguments:\n");
    fprintf(stderr, "\t<AssertionFile>\n");
}

void
keynote_sigver(int argc, char *argv[])
{
    struct stat sb;
    int fd, i;
    char *buf;

    if (argc != 2)
    {
	sigverusage();
	exit(0);
    }

    /* Open and read assertion file */
    fd = open(argv[1], O_RDONLY, 0);
    if (fd < 0)
    {
	perror(argv[1]);
	exit(-1);
    }

    if (fstat(fd, &sb) < 0)
    {
	perror("fstat()");
	exit(-1);
    }

    if (sb.st_size == 0) /* Paranoid */
    {
	fprintf(stderr, "Illegal assertion-file size 0\n");
	exit(-1);
    }

    buf = (char *) calloc(sb.st_size + 1, sizeof(char));
    if (buf == (char *) NULL)
    {
	perror("calloc()");
	exit(-1);
    }

    if (read(fd, buf, sb.st_size) < 0)
    {
	perror("read()");
	exit(-1);
    }

    close(fd);

    i = kn_verify_assertion(buf, sb.st_size);
    if (i == -1)
    {
	switch (keynote_errno)
	{
	    case ERROR_MEMORY:
		fprintf(stderr,
			"Out of memory while parsing the assertion.\n");
		break;

	    case ERROR_SYNTAX:
		fprintf(stderr,
			"Syntax error while parsing the assertion.\n");
		break;

	    default:
		fprintf(stderr,
			"Unknown error while parsing the assertion.\n");
	}

	exit(-1);
    }

    free(buf);

    if (i == SIGRESULT_TRUE)
      fprintf(stdout, "Signature verified.\n");
    else
    {
	if (keynote_errno != 0)
	  fprintf(stdout, "Signature could not be verified "
		  "(keynote_errno = %d).\n", keynote_errno);
	else
	  fprintf(stdout, "Signature did not verify!\n");
    }

    exit(0);
}
