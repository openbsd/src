/*	$OpenBSD: exclude.c,v 1.1.1.1 1997/09/15 06:01:52 downsj Exp $	*/
/*
 * 9-Dec-93 R.-D. Marzusch, marzusch@odiehh.hanse.de:
 * added 'exclude' option (-x) to specify pathnames NOT to be included in 
 * CD image.
 */

static char rcsid[] ="$From: exclude.c,v 1.2 1997/02/23 16:12:34 eric Rel $";

#include <stdio.h>
#ifndef VMS
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#else
#include <stdlib.h>
#endif
#endif
#include <string.h>

/* this allows for 1000 entries to be excluded ... */
#define MAXEXCL 1000
static char * excl[MAXEXCL];

void exclude(fn)
char * fn;
{
  register int i;

  for (i=0; excl[i] && i<MAXEXCL; i++);
  if (i == MAXEXCL) {
    fprintf(stderr,"Can't exclude '%s' - too many entries in table\n",fn);
    return;
  }

 
  excl[i] = (char *) malloc(strlen(fn)+1);
  if (! excl[i]) {
    fprintf(stderr,"Can't allocate memory for excluded filename\n");
    return;
  }

  strcpy(excl[i],fn);
}

int is_excluded(fn)
char * fn;
{
  /* very dumb search method ... */
  register int i;

  for (i=0; excl[i] && i<MAXEXCL; i++) {
    if (strcmp(excl[i],fn) == 0) {
      return 1; /* found -> excluded filenmae */
    }
  }
  return 0; /* not found -> not excluded */
}
