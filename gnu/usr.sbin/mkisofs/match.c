/*	$OpenBSD: match.c,v 1.1.1.1 1997/09/15 06:01:53 downsj Exp $	*/
/*
 * 27-Mar-96: Jan-Piet Mens <jpm@mens.de>
 * added 'match' option (-m) to specify regular expressions NOT to be included
 * in the CD image.
 */

static char rcsid[] ="$From: match.c,v 1.2 1997/02/23 16:10:42 eric Rel $";

#include <stdio.h>
#ifndef VMS
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#else
#include <stdlib.h>
#endif
#endif
#include <string.h>
#include "match.h"

#define MAXMATCH 1000
static char *mat[MAXMATCH];

void add_match(fn)
char * fn;
{
  register int i;

  for (i=0; mat[i] && i<MAXMATCH; i++);
  if (i == MAXMATCH) {
    fprintf(stderr,"Can't exclude RE '%s' - too many entries in table\n",fn);
    return;
  }

 
  mat[i] = (char *) malloc(strlen(fn)+1);
  if (! mat[i]) {
    fprintf(stderr,"Can't allocate memory for excluded filename\n");
    return;
  }

  strcpy(mat[i],fn);
}

int matches(fn)
char * fn;
{
  /* very dumb search method ... */
  register int i;

  for (i=0; mat[i] && i<MAXMATCH; i++) {
    if (fnmatch(mat[i], fn, FNM_FILE_NAME) != FNM_NOMATCH) {
      return 1; /* found -> excluded filenmae */
    }
  }
  return 0; /* not found -> not excluded */
}
