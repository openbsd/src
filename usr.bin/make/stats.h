#ifndef STAT_H
#define STAT_H
/* $OpenPackages$ */
/* $OpenBSD: stats.h,v 1.4 2004/05/05 09:10:47 espie Exp $ */

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

/* statistical information gathering */

#if defined(STATS_VAR_LOOKUP) || \
	defined(STATS_GN_CREATION) || \
	defined(STATS_BUF) || \
	defined(STATS_HASH) || \
	defined(STATS_GROW) || \
	defined(STATS_SUFF)
#define HAS_STATS
#endif

#ifdef HAS_STATS
extern void Init_Stats(void);

extern unsigned long *statarray;
#define STAT_INVOCATIONS	 statarray[0]
#define STAT_VAR_SEARCHES	 statarray[1]
#define STAT_VAR_COUNT		 statarray[2]
#define STAT_VAR_MAXCOUNT	 statarray[3]
#define STAT_GN_COUNT		 statarray[4]
#define STAT_TOTAL_BUFS 	 statarray[5]
#define STAT_DEFAULT_BUFS	 statarray[6]
#define STAT_WEIRD_BUFS 	 statarray[7]
#define STAT_BUFS_EXPANSION	 statarray[8]
#define STAT_WEIRD_INEFFICIENT	 statarray[9]
#define STAT_VAR_HASH_CREATION	statarray[10]
#define STAT_VAR_FROM_ENV	statarray[11]
#define STAT_VAR_CREATION	statarray[12]
#define STAT_VAR_FIND		statarray[13]
#define STAT_HASH_CREATION	statarray[14]
#define STAT_HASH_ENTRIES	statarray[15]
#define STAT_HASH_EXPAND	statarray[16]
#define STAT_HASH_LOOKUP	statarray[17]
#define STAT_HASH_LENGTH	statarray[18]
#define STAT_HASH_SIZE		statarray[19]
#define STAT_HASH_POSITIVE	statarray[20]
#define STAT_USER_SECONDS	statarray[21]
#define STAT_USER_MS		statarray[22]
#define STAT_SYS_SECONDS	statarray[23]
#define STAT_SYS_MS		statarray[24]
#define STAT_VAR_HASH_MAXSIZE	statarray[25]
#define STAT_VAR_GHASH_MAXSIZE	statarray[26]
#define STAT_VAR_POWER		statarray[27]
#define STAT_GROWARRAY		statarray[28]
#define STAT_SUFF_LOOKUP_NAME	statarray[29]
#define STAT_TRANSFORM_LOOKUP_NAME	statarray[30]

#define STAT_NUMBER		32

#else
#define Init_Stats()
#endif

#endif
