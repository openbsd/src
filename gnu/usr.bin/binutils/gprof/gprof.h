/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *      @(#)gprof.h     5.9 (Berkeley) 6/1/90
 */
#ifndef gprof_h
#define gprof_h

#include <ansidecl.h>
#include "sysdep.h"

#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b)	((a) > (b) ? (a) : (b))
#endif

/* AIX defines hz as a macro.  */
#undef hz

#ifdef MACHINE_H
#include MACHINE_H
#else
#if vax
#include "vax.h"
#endif
#if sun
#include "sun.h"
#endif
#if tahoe
#include "tahoe.h"
#endif
#endif

#ifndef FOPEN_RB
#define FOPEN_RB "r"
#endif
#ifndef FOPEN_WB
#define FOPEN_WB "w"
#endif

#ifndef PATH_MAX
#define PATH_MAX	1024
#endif

#define	A_OUTNAME	"a.out"	/* default core filename */
#define	GMONNAME	"gmon.out"	/* default profile filename */
#define	GMONSUM		"gmon.sum"	/* profile summary filename */

/*
 * These may already be defined on some systems.  We could probably
 * just use the BFD versions of these, since BFD has already dealt
 * with this problem.
 */
#undef FALSE
#define	FALSE	0
#undef TRUE
#define	TRUE	1

#define STYLE_FLAT_PROFILE	(1<<0)
#define STYLE_CALL_GRAPH	(1<<1)
#define STYLE_SUMMARY_FILE	(1<<2)
#define STYLE_EXEC_COUNTS	(1<<3)
#define STYLE_ANNOTATED_SOURCE	(1<<4)
#define STYLE_GMON_INFO		(1<<5)

#define	ANYDEBUG	(1<<0)	/*    1 */
#define	DFNDEBUG	(1<<1)	/*    2 */
#define	CYCLEDEBUG	(1<<2)	/*    4 */
#define	ARCDEBUG	(1<<3)	/*    8 */
#define	TALLYDEBUG	(1<<4)	/*   16 */
#define	TIMEDEBUG	(1<<5)	/*   32 */
#define	SAMPLEDEBUG	(1<<6)	/*   64 */
#define	AOUTDEBUG	(1<<7)	/*  128 */
#define	CALLDEBUG	(1<<8)	/*  256 */
#define	LOOKUPDEBUG	(1<<9)	/*  512 */
#define	PROPDEBUG	(1<<10)	/* 1024 */
#define BBDEBUG		(1<<11)	/* 2048 */
#define IDDEBUG		(1<<12)	/* 4096 */
#define SRCDEBUG	(1<<13)	/* 8192 */

#ifdef DEBUG
#define DBG(l,s)	if (debug_level & (l)) {s;}
#else
#define DBG(l,s)
#endif

typedef enum
  {
    FF_AUTO = 0, FF_MAGIC, FF_BSD, FF_PROF
  }
File_Format;

typedef int bool;
typedef unsigned char UNIT[2];	/* unit of profiling */

extern const char *whoami;	/* command-name, for error messages */
extern const char *a_out_name;	/* core filename */
extern long hz;			/* ticks per second */

/*
 * Command-line options:
 */
extern int debug_level;		/* debug level */
extern int output_style;
extern int output_width;	/* controls column width in index */
extern bool bsd_style_output;	/* as opposed to FSF style output */
extern bool discard_underscores;	/* discard leading underscores? */
extern bool ignore_direct_calls;	/* don't count direct calls */
extern bool ignore_static_funcs;	/* suppress static functions */
extern bool ignore_zeros;	/* ignore unused symbols/files */
extern bool line_granularity;	/* function or line granularity? */
extern bool print_descriptions;	/* output profile description */
extern bool print_path;		/* print path or just filename? */
extern File_Format file_format;	/* requested file format */

extern bool first_output;	/* no output so far? */

extern void done PARAMS ((int status));

#endif /* gprof_h */
