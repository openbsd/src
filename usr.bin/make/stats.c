/* $OpenPackages$ */
/* $OpenBSD: stats.c,v 1.6 2002/08/12 00:42:56 aaron Exp $ */

/*
 * Copyright (c) 1999 Marc Espie.
 *
 * Code written for the OpenBSD project.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/* statistics gathering */

/* collection across make invocations is done with an mmap shared file,
   to allow for concurrent adjustment to variables.
 */

#include "config.h"
#include "defines.h"
#include "stats.h"

#ifdef HAS_STATS
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/resource.h>
#include "memory.h"

static void print_stats(void);
void Init_Stats(void);
static float average_runs(unsigned long val);
unsigned long *statarray;

static bool mmapped = false;

static float
average_runs(val)
    unsigned long val;
{
    return (float)val / STAT_INVOCATIONS;
}

static void
print_stats()
{
    struct rusage ru;

    if (getrusage(RUSAGE_SELF, &ru) != -1) {
	STAT_USER_SECONDS += ru.ru_utime.tv_sec;
	STAT_USER_MS += ru.ru_utime.tv_usec;
	if (STAT_USER_MS > 1000000) {
	    STAT_USER_MS -= 1000000;
	    STAT_USER_SECONDS++;
	}
	STAT_SYS_SECONDS += ru.ru_stime.tv_sec;
	STAT_SYS_MS += ru.ru_stime.tv_usec;
	if (STAT_SYS_MS > 1000000) {
	    STAT_SYS_MS -= 1000000;
	    STAT_SYS_SECONDS++;
	}
    }
    fprintf(stderr, "Make runs: %lu\n", STAT_INVOCATIONS);
    fprintf(stderr, "Time user: %lu.%06lu, sys %lu.%06lu\n",
	STAT_USER_SECONDS, STAT_USER_MS,
	STAT_SYS_SECONDS, STAT_SYS_MS);
#ifdef STATS_VAR_LOOKUP
	/* to get the average total of MAXSIZE, we need this value */
    STAT_VAR_POWER +=
	STAT_VAR_HASH_MAXSIZE * STAT_VAR_HASH_CREATION;
    fprintf(stderr, "Var finds: %f, lookups: %f, average: %f, max: %lu\n",
	average_runs(STAT_VAR_FIND),
	average_runs(STAT_VAR_SEARCHES),
	(float)STAT_VAR_COUNT/STAT_VAR_SEARCHES,
	STAT_VAR_MAXCOUNT);
    fprintf(stderr, "Average hash: %f, creation: %f, from env %f\n",
	average_runs(STAT_VAR_HASH_CREATION),
	average_runs(STAT_VAR_CREATION),
	average_runs(STAT_VAR_FROM_ENV));
    fprintf(stderr, "Local hash max: %lu, global hash max: %lu, average local: %f\n",
	STAT_VAR_HASH_MAXSIZE,
	STAT_VAR_GHASH_MAXSIZE,
	(float)STAT_VAR_POWER/STAT_VAR_HASH_CREATION);
#endif
#ifdef STATS_GN_CREATION
    fprintf(stderr, "Average GN: %f\n", average_runs(STAT_GN_COUNT));
#endif
#ifdef STATS_BUF
    fprintf(stderr, "Buf tot: %f, def: %f, exp %f, weird %f, bad %f\n",
	average_runs(STAT_TOTAL_BUFS),
	average_runs(STAT_DEFAULT_BUFS),
	average_runs(STAT_BUFS_EXPANSION),
	average_runs(STAT_WEIRD_BUFS),
	average_runs(STAT_WEIRD_INEFFICIENT));
#endif
#ifdef STATS_HASH
    fprintf(stderr, "Hashes new: %f, exp: %f, lookup %f, l: %f, +: %f, ent : %f\n",
	average_runs(STAT_HASH_CREATION),
	average_runs(STAT_HASH_EXPAND),
	average_runs(STAT_HASH_LOOKUP),
	(float)STAT_HASH_LENGTH/STAT_HASH_LOOKUP,
	(float)STAT_HASH_POSITIVE/STAT_HASH_LOOKUP,
	(float)STAT_HASH_ENTRIES/STAT_HASH_SIZE);
#endif
#ifdef STATS_GROW
    fprintf(stderr, "Grow: %f\n", average_runs(STAT_GROWARRAY));
#endif
    if (mmapped)
	munmap(statarray, STAT_NUMBER * sizeof(unsigned long));
}

void
Init_Stats()
{
    char *name;
    int fd;

	/* try to get ahold of a stats collecting file */
    name = getenv("MAKESTATS");
    if (name) {
	while ((fd = open(name, O_RDWR)) == -1) {
		/* if collecting file does not already exist, fill it with
		 * zeros (so all stats starting values should be 0) */
	    unsigned long n;
	    int i;
	    FILE *f;

	    f = fopen(name, "w");

	    n = 0;
	    for (i = 0; i < STAT_NUMBER; i++)
		fwrite(&n, sizeof(unsigned long), 1, f);
	    fclose(f);
	}

    /* either we've got the file -> share it across makes */
	if (fd) {
	    statarray = mmap(0, STAT_NUMBER * sizeof(unsigned long),
		PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	    if (statarray == MAP_FAILED)
		exit(1);
	    mmapped = true;
	}
    } else
    /* or we don't -> simple stats gathering */
	statarray = ecalloc(STAT_NUMBER, sizeof(unsigned long));
    STAT_INVOCATIONS++;
    atexit(print_stats);
}

#endif

