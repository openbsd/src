/* $OpenBSD: generate.c,v 1.1 2000/06/23 16:27:29 espie Exp $ */
/* Written by Marc Espie 1999.  
 * Public domain.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include "make.h"
#include "ohash.h"
#include "error.h"

#define M(x)	x, #x
char *table[] = {
	M(TARGET),
	M(OODATE),
	M(ALLSRC),
	M(IMPSRC),
	M(PREFIX),
	M(ARCHIVE),
	M(MEMBER),
	M(LONGTARGET),
	M(LONGOODATE),
	M(LONGALLSRC),
	M(LONGIMPSRC),
	M(LONGPREFIX),
	M(LONGARCHIVE),
	M(LONGMEMBER)
};


int
main(int argc, char *argv[])
{
	u_int32_t i;
	u_int32_t v;
	u_int32_t h;
	u_int32_t slots;
	const char *e;
	char **occupied;

#ifdef HAS_STATS
	Init_Stats();
#endif
	if (argc < 2)
		exit(1);

	slots = atoi(argv[1]);
	if (!slots)
		exit(1);
	occupied = emalloc(sizeof(char *) * slots);
	for (i = 0; i < slots; i++)
		occupied[i] = NULL;
	
	printf("/* Generated file, do not edit */\n");
	for (i = 0; i < sizeof(table)/sizeof(char *); i++) {
		e = NULL;
		v = hash_interval(table[i], &e);
		h = v % slots;
		if (occupied[h]) {
			fprintf(stderr, "Collision: %s / %s (%d)\n", occupied[h],
				table[i], h);
			exit(1);
		}
		occupied[h] = table[i++];
		printf("#define K_%s %u\n", table[i], v);
	}
	printf("#define MAGICSLOTS %u\n", slots);
	exit(0);
}

