/* $OpenBSD: keynote-main.c,v 1.6 1999/10/01 01:08:30 angelos Exp $ */
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
#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif /* WIN32 */

#include "header.h"

void
mainusage()
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "\tsign ...\n");
    fprintf(stderr, "\tsigver ...\n");
    fprintf(stderr, "\tverify ...\n");
    fprintf(stderr, "\tkeygen ...\n");
    fprintf(stderr, "Issue one of the commands by itself to get more help, "
		    "e.g., keynote sign\n");
}

int
main(int argc, char *argv[])
{
    if (argc < 2)
    {
	mainusage();
	exit(-1);
    }

    if (!strcmp(argv[1], "sign"))
      keynote_sign(argc - 1, argv + 1);
    else
      if (!strcmp(argv[1], "verify"))
	keynote_verify(argc - 1, argv + 1);
      else
	if (!strcmp(argv[1], "sigver"))
	  keynote_sigver(argc - 1, argv + 1);
	else
	  if (!strcmp(argv[1], "keygen"))
	    keynote_keygen(argc - 1, argv + 1);

    mainusage();
    exit(-1);
}
