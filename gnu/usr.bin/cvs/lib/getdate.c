#ifndef lint
static char yysccsid[] = "@(#)yaccpar	1.9 (Berkeley) 02/21/93 (BSDI)";
#endif
#include <stdlib.h>
#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9
#define YYEMPTY (-1)
#define YYLEX yylex()
#define yyclearin (yychar=YYEMPTY)
#define yyerrok (yyerrflag=0)
#define YYRECOVERING (yyerrflag!=0)
#define YYPREFIX "yy"
#line 2 "/u/scjones/cvs-nightly/lib/getdate.y"
/*
**  Originally written by Steven M. Bellovin <smb@research.att.com> while
**  at the University of North Carolina at Chapel Hill.  Later tweaked by
**  a couple of people on Usenet.  Completely overhauled by Rich $alz
**  <rsalz@bbn.com> and Jim Berets <jberets@bbn.com> in August, 1990;
**
**  This grammar has 10 shift/reduce conflicts.
**
**  This code is in the public domain and has no copyright.
*/
/* SUPPRESS 287 on yaccpar_sccsid *//* Unused static variable */
/* SUPPRESS 288 on yyerrlab *//* Label unused */

#ifdef HAVE_CONFIG_H
#if defined (emacs) || defined (CONFIG_BROKETS)
#include <config.h>
#else
#include "config.h"
#endif
#endif

/* Since the code of getdate.y is not included in the Emacs executable
   itself, there is no need to #define static in this file.  Even if
   the code were included in the Emacs executable, it probably
   wouldn't do any harm to #undef it here; this will only cause
   problems if we try to write to a static variable, which I don't
   think this code needs to do.  */
#ifdef emacs
#undef static
#endif

#include <stdio.h>
#include <ctype.h>

/* The code at the top of get_date which figures out the offset of the
   current time zone checks various CPP symbols to see if special
   tricks are need, but defaults to using the gettimeofday system call.
   Include <sys/time.h> if that will be used.  */

#if	defined(vms)

#include <types.h>
#include <time.h>

#else

#include <sys/types.h>

#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif

#ifdef timezone
#undef timezone /* needed for sgi */
#endif

#if defined(HAVE_SYS_TIMEB_H)
#include <sys/timeb.h>
#else
/*
** We use the obsolete `struct timeb' as part of our interface!
** Since the system doesn't have it, we define it here;
** our callers must do likewise.
*/
struct timeb {
    time_t		time;		/* Seconds since the epoch	*/
    unsigned short	millitm;	/* Field not used		*/
    short		timezone;	/* Minutes west of GMT		*/
    short		dstflag;	/* Field not used		*/
};
#endif /* defined(HAVE_SYS_TIMEB_H) */

#endif	/* defined(vms) */

#if defined (STDC_HEADERS) || defined (USG)
#include <string.h>
#endif

/* Some old versions of bison generate parsers that use bcopy.
   That loses on systems that don't provide the function, so we have
   to redefine it here.  */
#if !defined (HAVE_BCOPY) && defined (HAVE_MEMCPY) && !defined (bcopy)
#define bcopy(from, to, len) memcpy ((to), (from), (len))
#endif

#if defined (STDC_HEADERS)
#include <stdlib.h>
#endif

/* NOTES on rebuilding getdate.c (particularly for inclusion in CVS
   releases):

   We don't want to mess with all the portability hassles of alloca.
   In particular, most (all?) versions of bison will use alloca in
   their parser.  If bison works on your system (e.g. it should work
   with gcc), then go ahead and use it, but the more general solution
   is to use byacc instead of bison, which should generate a portable
   parser.  I played with adding "#define alloca dont_use_alloca", to
   give an error if the parser generator uses alloca (and thus detect
   unportable getdate.c's), but that seems to cause as many problems
   as it solves.  */

extern struct tm	*gmtime();
extern struct tm	*localtime();

#define yyparse getdate_yyparse
#define yylex getdate_yylex
#define yyerror getdate_yyerror

static int yylex ();
static int yyerror ();

#define EPOCH		1970
#define HOUR(x)		((time_t)(x) * 60)
#define SECSPERDAY	(24L * 60L * 60L)


/*
**  An entry in the lexical lookup table.
*/
typedef struct _TABLE {
    char	*name;
    int		type;
    time_t	value;
} TABLE;


/*
**  Daylight-savings mode:  on, off, or not yet known.
*/
typedef enum _DSTMODE {
    DSTon, DSToff, DSTmaybe
} DSTMODE;

/*
**  Meridian:  am, pm, or 24-hour style.
*/
typedef enum _MERIDIAN {
    MERam, MERpm, MER24
} MERIDIAN;


/*
**  Global variables.  We could get rid of most of these by using a good
**  union as the yacc stack.  (This routine was originally written before
**  yacc had the %union construct.)  Maybe someday; right now we only use
**  the %union very rarely.
*/
static char	*yyInput;
static DSTMODE	yyDSTmode;
static time_t	yyDayOrdinal;
static time_t	yyDayNumber;
static int	yyHaveDate;
static int	yyHaveDay;
static int	yyHaveRel;
static int	yyHaveTime;
static int	yyHaveZone;
static time_t	yyTimezone;
static time_t	yyDay;
static time_t	yyHour;
static time_t	yyMinutes;
static time_t	yyMonth;
static time_t	yySeconds;
static time_t	yyYear;
static MERIDIAN	yyMeridian;
static time_t	yyRelMonth;
static time_t	yyRelSeconds;

#line 179 "/u/scjones/cvs-nightly/lib/getdate.y"
typedef union {
    time_t		Number;
    enum _MERIDIAN	Meridian;
} YYSTYPE;
#line 196 "y.tab.c"
#define tAGO 257
#define tDAY 258
#define tDAYZONE 259
#define tID 260
#define tMERIDIAN 261
#define tMINUTE_UNIT 262
#define tMONTH 263
#define tMONTH_UNIT 264
#define tSEC_UNIT 265
#define tSNUMBER 266
#define tUNUMBER 267
#define tZONE 268
#define tDST 269
#define YYERRCODE 256
short yylhs[] = {                                        -1,
    0,    0,    2,    2,    2,    2,    2,    2,    3,    3,
    3,    3,    3,    4,    4,    4,    6,    6,    6,    5,
    5,    5,    5,    5,    5,    5,    5,    7,    7,    9,
    9,    9,    9,    9,    9,    9,    9,    9,    8,    1,
    1,
};
short yylen[] = {                                         2,
    0,    2,    1,    1,    1,    1,    1,    1,    2,    4,
    4,    6,    6,    1,    1,    2,    1,    2,    2,    3,
    5,    3,    3,    2,    4,    2,    3,    2,    1,    2,
    2,    1,    2,    2,    1,    2,    2,    1,    1,    0,
    1,
};
short yydefred[] = {                                      1,
    0,    0,   15,   32,    0,   38,   35,    0,    0,    0,
    2,    3,    4,    5,    6,    7,    8,    0,   18,    0,
   31,   36,   33,   19,    9,   30,    0,   37,   34,    0,
    0,    0,   16,   28,    0,   23,   27,   22,    0,    0,
   25,   41,   11,    0,   10,    0,    0,   21,   13,   12,
};
short yydgoto[] = {                                       1,
   45,   11,   12,   13,   14,   15,   16,   17,   18,
};
short yysindex[] = {                                      0,
 -249,  -38,    0,    0, -260,    0,    0, -240,  -47, -248,
    0,    0,    0,    0,    0,    0,    0, -237,    0,  -18,
    0,    0,    0,    0,    0,    0, -262,    0,    0, -239,
 -238, -236,    0,    0, -235,    0,    0,    0,  -56,  -19,
    0,    0,    0, -234,    0, -232, -258,    0,    0,    0,
};
short yyrindex[] = {                                      0,
    0,    1,    0,    0,    0,    0,    0,    0,   69,   12,
    0,    0,    0,    0,    0,    0,    0,   23,    0,   34,
    0,    0,    0,    0,    0,    0,   67,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,   56,   45,
    0,    0,    0,    0,    0,    0,   56,    0,    0,    0,
};
short yygindex[] = {                                      0,
  -17,    0,    0,    0,    0,    0,    0,    0,    0,
};
#define YYTABLESIZE 337
short yytable[] = {                                      32,
   17,   44,   42,   36,   37,   19,   20,   49,    2,    3,
   31,   14,    4,    5,    6,    7,    8,    9,   10,   34,
   33,   21,   29,   22,   23,   35,   38,   46,   39,   50,
   40,   41,   47,   24,   48,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   20,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,   40,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,   26,    0,   39,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   42,    0,    0,    0,    0,   43,
   24,    0,    0,   25,   26,   27,   28,   29,   30,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,   17,   17,
    0,    0,   17,   17,   17,   17,   17,   17,   17,   14,
   14,    0,    0,   14,   14,   14,   14,   14,   14,   14,
   29,   29,    0,    0,   29,   29,   29,   29,   29,   29,
   29,   24,   24,    0,    0,   24,   24,   24,   24,   24,
   24,   24,   20,   20,    0,    0,   20,   20,   20,   20,
   20,   20,   20,   40,   40,    0,    0,   40,   40,   40,
   40,    0,   40,   40,   26,   26,    0,   39,   26,   26,
   26,   26,    0,    0,   26,   39,   39,
};
short yycheck[] = {                                      47,
    0,   58,  261,  266,  267,   44,  267,  266,  258,  259,
   58,    0,  262,  263,  264,  265,  266,  267,  268,  257,
  269,  262,    0,  264,  265,   44,  266,   47,  267,   47,
  267,  267,  267,    0,  267,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,    0,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,    0,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,    0,   -1,    0,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,  261,   -1,   -1,   -1,   -1,  266,
  258,   -1,   -1,  261,  262,  263,  264,  265,  266,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  258,  259,
   -1,   -1,  262,  263,  264,  265,  266,  267,  268,  258,
  259,   -1,   -1,  262,  263,  264,  265,  266,  267,  268,
  258,  259,   -1,   -1,  262,  263,  264,  265,  266,  267,
  268,  258,  259,   -1,   -1,  262,  263,  264,  265,  266,
  267,  268,  258,  259,   -1,   -1,  262,  263,  264,  265,
  266,  267,  268,  258,  259,   -1,   -1,  262,  263,  264,
  265,   -1,  267,  268,  258,  259,   -1,  259,  262,  263,
  264,  265,   -1,   -1,  268,  267,  268,
};
#define YYFINAL 1
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 269
#if YYDEBUG
char *yyname[] = {
"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,"','",0,0,"'/'",0,0,0,0,0,0,0,0,0,0,"':'",0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"tAGO","tDAY",
"tDAYZONE","tID","tMERIDIAN","tMINUTE_UNIT","tMONTH","tMONTH_UNIT","tSEC_UNIT",
"tSNUMBER","tUNUMBER","tZONE","tDST",
};
char *yyrule[] = {
"$accept : spec",
"spec :",
"spec : spec item",
"item : time",
"item : zone",
"item : date",
"item : day",
"item : rel",
"item : number",
"time : tUNUMBER tMERIDIAN",
"time : tUNUMBER ':' tUNUMBER o_merid",
"time : tUNUMBER ':' tUNUMBER tSNUMBER",
"time : tUNUMBER ':' tUNUMBER ':' tUNUMBER o_merid",
"time : tUNUMBER ':' tUNUMBER ':' tUNUMBER tSNUMBER",
"zone : tZONE",
"zone : tDAYZONE",
"zone : tZONE tDST",
"day : tDAY",
"day : tDAY ','",
"day : tUNUMBER tDAY",
"date : tUNUMBER '/' tUNUMBER",
"date : tUNUMBER '/' tUNUMBER '/' tUNUMBER",
"date : tUNUMBER tSNUMBER tSNUMBER",
"date : tUNUMBER tMONTH tSNUMBER",
"date : tMONTH tUNUMBER",
"date : tMONTH tUNUMBER ',' tUNUMBER",
"date : tUNUMBER tMONTH",
"date : tUNUMBER tMONTH tUNUMBER",
"rel : relunit tAGO",
"rel : relunit",
"relunit : tUNUMBER tMINUTE_UNIT",
"relunit : tSNUMBER tMINUTE_UNIT",
"relunit : tMINUTE_UNIT",
"relunit : tSNUMBER tSEC_UNIT",
"relunit : tUNUMBER tSEC_UNIT",
"relunit : tSEC_UNIT",
"relunit : tSNUMBER tMONTH_UNIT",
"relunit : tUNUMBER tMONTH_UNIT",
"relunit : tMONTH_UNIT",
"number : tUNUMBER",
"o_merid :",
"o_merid : tMERIDIAN",
};
#endif
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 10000
#define YYMAXDEPTH 10000
#endif
#endif
#define YYINITSTACKSIZE 200
int yydebug;
int yynerrs;
struct yystack {
    short *ssp;
    YYSTYPE *vsp;
    short *ss;
    YYSTYPE *vs;
    int stacksize;
    short *sslim;
};
int yychar; /* some people use this, so we copy it in & out */
int yyerrflag; /* must be global for yyerrok & YYRECOVERING */
YYSTYPE yylval;
#line 398 "/u/scjones/cvs-nightly/lib/getdate.y"

/* Month and day table. */
static TABLE const MonthDayTable[] = {
    { "january",	tMONTH,  1 },
    { "february",	tMONTH,  2 },
    { "march",		tMONTH,  3 },
    { "april",		tMONTH,  4 },
    { "may",		tMONTH,  5 },
    { "june",		tMONTH,  6 },
    { "july",		tMONTH,  7 },
    { "august",		tMONTH,  8 },
    { "september",	tMONTH,  9 },
    { "sept",		tMONTH,  9 },
    { "october",	tMONTH, 10 },
    { "november",	tMONTH, 11 },
    { "december",	tMONTH, 12 },
    { "sunday",		tDAY, 0 },
    { "monday",		tDAY, 1 },
    { "tuesday",	tDAY, 2 },
    { "tues",		tDAY, 2 },
    { "wednesday",	tDAY, 3 },
    { "wednes",		tDAY, 3 },
    { "thursday",	tDAY, 4 },
    { "thur",		tDAY, 4 },
    { "thurs",		tDAY, 4 },
    { "friday",		tDAY, 5 },
    { "saturday",	tDAY, 6 },
    { NULL }
};

/* Time units table. */
static TABLE const UnitsTable[] = {
    { "year",		tMONTH_UNIT,	12 },
    { "month",		tMONTH_UNIT,	1 },
    { "fortnight",	tMINUTE_UNIT,	14 * 24 * 60 },
    { "week",		tMINUTE_UNIT,	7 * 24 * 60 },
    { "day",		tMINUTE_UNIT,	1 * 24 * 60 },
    { "hour",		tMINUTE_UNIT,	60 },
    { "minute",		tMINUTE_UNIT,	1 },
    { "min",		tMINUTE_UNIT,	1 },
    { "second",		tSEC_UNIT,	1 },
    { "sec",		tSEC_UNIT,	1 },
    { NULL }
};

/* Assorted relative-time words. */
static TABLE const OtherTable[] = {
    { "tomorrow",	tMINUTE_UNIT,	1 * 24 * 60 },
    { "yesterday",	tMINUTE_UNIT,	-1 * 24 * 60 },
    { "today",		tMINUTE_UNIT,	0 },
    { "now",		tMINUTE_UNIT,	0 },
    { "last",		tUNUMBER,	-1 },
    { "this",		tMINUTE_UNIT,	0 },
    { "next",		tUNUMBER,	2 },
    { "first",		tUNUMBER,	1 },
/*  { "second",		tUNUMBER,	2 }, */
    { "third",		tUNUMBER,	3 },
    { "fourth",		tUNUMBER,	4 },
    { "fifth",		tUNUMBER,	5 },
    { "sixth",		tUNUMBER,	6 },
    { "seventh",	tUNUMBER,	7 },
    { "eighth",		tUNUMBER,	8 },
    { "ninth",		tUNUMBER,	9 },
    { "tenth",		tUNUMBER,	10 },
    { "eleventh",	tUNUMBER,	11 },
    { "twelfth",	tUNUMBER,	12 },
    { "ago",		tAGO,	1 },
    { NULL }
};

/* The timezone table. */
/* Some of these are commented out because a time_t can't store a float. */
static TABLE const TimezoneTable[] = {
    { "gmt",	tZONE,     HOUR( 0) },	/* Greenwich Mean */
    { "ut",	tZONE,     HOUR( 0) },	/* Universal (Coordinated) */
    { "utc",	tZONE,     HOUR( 0) },
    { "wet",	tZONE,     HOUR( 0) },	/* Western European */
    { "bst",	tDAYZONE,  HOUR( 0) },	/* British Summer */
    { "wat",	tZONE,     HOUR( 1) },	/* West Africa */
    { "at",	tZONE,     HOUR( 2) },	/* Azores */
#if	0
    /* For completeness.  BST is also British Summer, and GST is
     * also Guam Standard. */
    { "bst",	tZONE,     HOUR( 3) },	/* Brazil Standard */
    { "gst",	tZONE,     HOUR( 3) },	/* Greenland Standard */
#endif
#if 0
    { "nft",	tZONE,     HOUR(3.5) },	/* Newfoundland */
    { "nst",	tZONE,     HOUR(3.5) },	/* Newfoundland Standard */
    { "ndt",	tDAYZONE,  HOUR(3.5) },	/* Newfoundland Daylight */
#endif
    { "ast",	tZONE,     HOUR( 4) },	/* Atlantic Standard */
    { "adt",	tDAYZONE,  HOUR( 4) },	/* Atlantic Daylight */
    { "est",	tZONE,     HOUR( 5) },	/* Eastern Standard */
    { "edt",	tDAYZONE,  HOUR( 5) },	/* Eastern Daylight */
    { "cst",	tZONE,     HOUR( 6) },	/* Central Standard */
    { "cdt",	tDAYZONE,  HOUR( 6) },	/* Central Daylight */
    { "mst",	tZONE,     HOUR( 7) },	/* Mountain Standard */
    { "mdt",	tDAYZONE,  HOUR( 7) },	/* Mountain Daylight */
    { "pst",	tZONE,     HOUR( 8) },	/* Pacific Standard */
    { "pdt",	tDAYZONE,  HOUR( 8) },	/* Pacific Daylight */
    { "yst",	tZONE,     HOUR( 9) },	/* Yukon Standard */
    { "ydt",	tDAYZONE,  HOUR( 9) },	/* Yukon Daylight */
    { "hst",	tZONE,     HOUR(10) },	/* Hawaii Standard */
    { "hdt",	tDAYZONE,  HOUR(10) },	/* Hawaii Daylight */
    { "cat",	tZONE,     HOUR(10) },	/* Central Alaska */
    { "ahst",	tZONE,     HOUR(10) },	/* Alaska-Hawaii Standard */
    { "nt",	tZONE,     HOUR(11) },	/* Nome */
    { "idlw",	tZONE,     HOUR(12) },	/* International Date Line West */
    { "cet",	tZONE,     -HOUR(1) },	/* Central European */
    { "met",	tZONE,     -HOUR(1) },	/* Middle European */
    { "mewt",	tZONE,     -HOUR(1) },	/* Middle European Winter */
    { "mest",	tDAYZONE,  -HOUR(1) },	/* Middle European Summer */
    { "swt",	tZONE,     -HOUR(1) },	/* Swedish Winter */
    { "sst",	tDAYZONE,  -HOUR(1) },	/* Swedish Summer */
    { "fwt",	tZONE,     -HOUR(1) },	/* French Winter */
    { "fst",	tDAYZONE,  -HOUR(1) },	/* French Summer */
    { "eet",	tZONE,     -HOUR(2) },	/* Eastern Europe, USSR Zone 1 */
    { "bt",	tZONE,     -HOUR(3) },	/* Baghdad, USSR Zone 2 */
#if 0
    { "it",	tZONE,     -HOUR(3.5) },/* Iran */
#endif
    { "zp4",	tZONE,     -HOUR(4) },	/* USSR Zone 3 */
    { "zp5",	tZONE,     -HOUR(5) },	/* USSR Zone 4 */
#if 0
    { "ist",	tZONE,     -HOUR(5.5) },/* Indian Standard */
#endif
    { "zp6",	tZONE,     -HOUR(6) },	/* USSR Zone 5 */
#if	0
    /* For completeness.  NST is also Newfoundland Stanard, and SST is
     * also Swedish Summer. */
    { "nst",	tZONE,     -HOUR(6.5) },/* North Sumatra */
    { "sst",	tZONE,     -HOUR(7) },	/* South Sumatra, USSR Zone 6 */
#endif	/* 0 */
    { "wast",	tZONE,     -HOUR(7) },	/* West Australian Standard */
    { "wadt",	tDAYZONE,  -HOUR(7) },	/* West Australian Daylight */
#if 0
    { "jt",	tZONE,     -HOUR(7.5) },/* Java (3pm in Cronusland!) */
#endif
    { "cct",	tZONE,     -HOUR(8) },	/* China Coast, USSR Zone 7 */
    { "jst",	tZONE,     -HOUR(9) },	/* Japan Standard, USSR Zone 8 */
#if 0
    { "cast",	tZONE,     -HOUR(9.5) },/* Central Australian Standard */
    { "cadt",	tDAYZONE,  -HOUR(9.5) },/* Central Australian Daylight */
#endif
    { "east",	tZONE,     -HOUR(10) },	/* Eastern Australian Standard */
    { "eadt",	tDAYZONE,  -HOUR(10) },	/* Eastern Australian Daylight */
    { "gst",	tZONE,     -HOUR(10) },	/* Guam Standard, USSR Zone 9 */
    { "nzt",	tZONE,     -HOUR(12) },	/* New Zealand */
    { "nzst",	tZONE,     -HOUR(12) },	/* New Zealand Standard */
    { "nzdt",	tDAYZONE,  -HOUR(12) },	/* New Zealand Daylight */
    { "idle",	tZONE,     -HOUR(12) },	/* International Date Line East */
    {  NULL  }
};

/* Military timezone table. */
static TABLE const MilitaryTable[] = {
    { "a",	tZONE,	HOUR(  1) },
    { "b",	tZONE,	HOUR(  2) },
    { "c",	tZONE,	HOUR(  3) },
    { "d",	tZONE,	HOUR(  4) },
    { "e",	tZONE,	HOUR(  5) },
    { "f",	tZONE,	HOUR(  6) },
    { "g",	tZONE,	HOUR(  7) },
    { "h",	tZONE,	HOUR(  8) },
    { "i",	tZONE,	HOUR(  9) },
    { "k",	tZONE,	HOUR( 10) },
    { "l",	tZONE,	HOUR( 11) },
    { "m",	tZONE,	HOUR( 12) },
    { "n",	tZONE,	HOUR(- 1) },
    { "o",	tZONE,	HOUR(- 2) },
    { "p",	tZONE,	HOUR(- 3) },
    { "q",	tZONE,	HOUR(- 4) },
    { "r",	tZONE,	HOUR(- 5) },
    { "s",	tZONE,	HOUR(- 6) },
    { "t",	tZONE,	HOUR(- 7) },
    { "u",	tZONE,	HOUR(- 8) },
    { "v",	tZONE,	HOUR(- 9) },
    { "w",	tZONE,	HOUR(-10) },
    { "x",	tZONE,	HOUR(-11) },
    { "y",	tZONE,	HOUR(-12) },
    { "z",	tZONE,	HOUR(  0) },
    { NULL }
};




/* ARGSUSED */
static int
yyerror(s)
    char	*s;
{
  return 0;
}


static time_t
ToSeconds(Hours, Minutes, Seconds, Meridian)
    time_t	Hours;
    time_t	Minutes;
    time_t	Seconds;
    MERIDIAN	Meridian;
{
    if (Minutes < 0 || Minutes > 59 || Seconds < 0 || Seconds > 59)
	return -1;
    switch (Meridian) {
    case MER24:
	if (Hours < 0 || Hours > 23)
	    return -1;
	return (Hours * 60L + Minutes) * 60L + Seconds;
    case MERam:
	if (Hours < 1 || Hours > 12)
	    return -1;
	if (Hours == 12)
	    Hours = 0;
	return (Hours * 60L + Minutes) * 60L + Seconds;
    case MERpm:
	if (Hours < 1 || Hours > 12)
	    return -1;
	if (Hours == 12)
	    Hours = 0;
	return ((Hours + 12) * 60L + Minutes) * 60L + Seconds;
    default:
	abort ();
    }
    /* NOTREACHED */
}


/* Year is either
   * A negative number, which means to use its absolute value (why?)
   * A number from 0 to 99, which means a year from 1900 to 1999, or
   * The actual year (>=100).  */
static time_t
Convert(Month, Day, Year, Hours, Minutes, Seconds, Meridian, DSTmode)
    time_t	Month;
    time_t	Day;
    time_t	Year;
    time_t	Hours;
    time_t	Minutes;
    time_t	Seconds;
    MERIDIAN	Meridian;
    DSTMODE	DSTmode;
{
    static int DaysInMonth[12] = {
	31, 0, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    time_t	tod;
    time_t	Julian;
    int		i;

    if (Year < 0)
	Year = -Year;
    if (Year < 69)
	Year += 2000;
    else if (Year < 100)
	Year += 1900;
    DaysInMonth[1] = Year % 4 == 0 && (Year % 100 != 0 || Year % 400 == 0)
		    ? 29 : 28;
    /* Checking for 2038 bogusly assumes that time_t is 32 bits.  But
       I'm too lazy to try to check for time_t overflow in another way.  */
    if (Year < EPOCH || Year > 2038
     || Month < 1 || Month > 12
     /* Lint fluff:  "conversion from long may lose accuracy" */
     || Day < 1 || Day > DaysInMonth[(int)--Month])
	return -1;

    for (Julian = Day - 1, i = 0; i < Month; i++)
	Julian += DaysInMonth[i];
    for (i = EPOCH; i < Year; i++)
	Julian += 365 + (i % 4 == 0);
    Julian *= SECSPERDAY;
    Julian += yyTimezone * 60L;
    if ((tod = ToSeconds(Hours, Minutes, Seconds, Meridian)) < 0)
	return -1;
    Julian += tod;
    if (DSTmode == DSTon
     || (DSTmode == DSTmaybe && localtime(&Julian)->tm_isdst))
	Julian -= 60 * 60;
    return Julian;
}


static time_t
DSTcorrect(Start, Future)
    time_t	Start;
    time_t	Future;
{
    time_t	StartDay;
    time_t	FutureDay;

    StartDay = (localtime(&Start)->tm_hour + 1) % 24;
    FutureDay = (localtime(&Future)->tm_hour + 1) % 24;
    return (Future - Start) + (StartDay - FutureDay) * 60L * 60L;
}


static time_t
RelativeDate(Start, DayOrdinal, DayNumber)
    time_t	Start;
    time_t	DayOrdinal;
    time_t	DayNumber;
{
    struct tm	*tm;
    time_t	now;

    now = Start;
    tm = localtime(&now);
    now += SECSPERDAY * ((DayNumber - tm->tm_wday + 7) % 7);
    now += 7 * SECSPERDAY * (DayOrdinal <= 0 ? DayOrdinal : DayOrdinal - 1);
    return DSTcorrect(Start, now);
}


static time_t
RelativeMonth(Start, RelMonth)
    time_t	Start;
    time_t	RelMonth;
{
    struct tm	*tm;
    time_t	Month;
    time_t	Year;

    if (RelMonth == 0)
	return 0;
    tm = localtime(&Start);
    Month = 12 * (tm->tm_year + 1900) + tm->tm_mon + RelMonth;
    Year = Month / 12;
    Month = Month % 12 + 1;
    return DSTcorrect(Start,
	    Convert(Month, (time_t)tm->tm_mday, Year,
		(time_t)tm->tm_hour, (time_t)tm->tm_min, (time_t)tm->tm_sec,
		MER24, DSTmaybe));
}


static int
LookupWord(buff)
    char		*buff;
{
    register char	*p;
    register char	*q;
    register const TABLE	*tp;
    int			i;
    int			abbrev;

    /* Make it lowercase. */
    for (p = buff; *p; p++)
	if (isupper(*p))
	    *p = tolower(*p);

    if (strcmp(buff, "am") == 0 || strcmp(buff, "a.m.") == 0) {
	yylval.Meridian = MERam;
	return tMERIDIAN;
    }
    if (strcmp(buff, "pm") == 0 || strcmp(buff, "p.m.") == 0) {
	yylval.Meridian = MERpm;
	return tMERIDIAN;
    }

    /* See if we have an abbreviation for a month. */
    if (strlen(buff) == 3)
	abbrev = 1;
    else if (strlen(buff) == 4 && buff[3] == '.') {
	abbrev = 1;
	buff[3] = '\0';
    }
    else
	abbrev = 0;

    for (tp = MonthDayTable; tp->name; tp++) {
	if (abbrev) {
	    if (strncmp(buff, tp->name, 3) == 0) {
		yylval.Number = tp->value;
		return tp->type;
	    }
	}
	else if (strcmp(buff, tp->name) == 0) {
	    yylval.Number = tp->value;
	    return tp->type;
	}
    }

    for (tp = TimezoneTable; tp->name; tp++)
	if (strcmp(buff, tp->name) == 0) {
	    yylval.Number = tp->value;
	    return tp->type;
	}

    if (strcmp(buff, "dst") == 0) 
	return tDST;

    for (tp = UnitsTable; tp->name; tp++)
	if (strcmp(buff, tp->name) == 0) {
	    yylval.Number = tp->value;
	    return tp->type;
	}

    /* Strip off any plural and try the units table again. */
    i = strlen(buff) - 1;
    if (buff[i] == 's') {
	buff[i] = '\0';
	for (tp = UnitsTable; tp->name; tp++)
	    if (strcmp(buff, tp->name) == 0) {
		yylval.Number = tp->value;
		return tp->type;
	    }
	buff[i] = 's';		/* Put back for "this" in OtherTable. */
    }

    for (tp = OtherTable; tp->name; tp++)
	if (strcmp(buff, tp->name) == 0) {
	    yylval.Number = tp->value;
	    return tp->type;
	}

    /* Military timezones. */
    if (buff[1] == '\0' && isalpha(*buff)) {
	for (tp = MilitaryTable; tp->name; tp++)
	    if (strcmp(buff, tp->name) == 0) {
		yylval.Number = tp->value;
		return tp->type;
	    }
    }

    /* Drop out any periods and try the timezone table again. */
    for (i = 0, p = q = buff; *q; q++)
	if (*q != '.')
	    *p++ = *q;
	else
	    i++;
    *p = '\0';
    if (i)
	for (tp = TimezoneTable; tp->name; tp++)
	    if (strcmp(buff, tp->name) == 0) {
		yylval.Number = tp->value;
		return tp->type;
	    }

    return tID;
}


static int
yylex()
{
    register char	c;
    register char	*p;
    char		buff[20];
    int			Count;
    int			sign;

    for ( ; ; ) {
	while (isspace(*yyInput))
	    yyInput++;

	if (isdigit(c = *yyInput) || c == '-' || c == '+') {
	    if (c == '-' || c == '+') {
		sign = c == '-' ? -1 : 1;
		if (!isdigit(*++yyInput))
		    /* skip the '-' sign */
		    continue;
	    }
	    else
		sign = 0;
	    for (yylval.Number = 0; isdigit(c = *yyInput++); )
		yylval.Number = 10 * yylval.Number + c - '0';
	    yyInput--;
	    if (sign < 0)
		yylval.Number = -yylval.Number;
	    return sign ? tSNUMBER : tUNUMBER;
	}
	if (isalpha(c)) {
	    for (p = buff; isalpha(c = *yyInput++) || c == '.'; )
		if (p < &buff[sizeof buff - 1])
		    *p++ = c;
	    *p = '\0';
	    yyInput--;
	    return LookupWord(buff);
	}
	if (c != '(')
	    return *yyInput++;
	Count = 0;
	do {
	    c = *yyInput++;
	    if (c == '\0')
		return c;
	    if (c == '(')
		Count++;
	    else if (c == ')')
		Count--;
	} while (Count > 0);
    }
}

#define TM_YEAR_ORIGIN 1900

/* Yield A - B, measured in seconds.  */
static long
difftm (a, b)
     struct tm *a, *b;
{
  int ay = a->tm_year + (TM_YEAR_ORIGIN - 1);
  int by = b->tm_year + (TM_YEAR_ORIGIN - 1);
  int days = (
	      /* difference in day of year */
	      a->tm_yday - b->tm_yday
	      /* + intervening leap days */
	      +  ((ay >> 2) - (by >> 2))
	      -  (ay/100 - by/100)
	      +  ((ay/100 >> 2) - (by/100 >> 2))
	      /* + difference in years * 365 */
	      +  (long)(ay-by) * 365
	      );
  return (60*(60*(24*days + (a->tm_hour - b->tm_hour))
	      + (a->tm_min - b->tm_min))
	  + (a->tm_sec - b->tm_sec));
}

time_t
get_date(p, now)
    char		*p;
    struct timeb	*now;
{
    struct tm		*tm, gmt;
    struct timeb	ftz;
    time_t		Start;
    time_t		tod;
    time_t nowtime;

    yyInput = p;
    if (now == NULL) {
	struct tm *gmt_ptr;

        now = &ftz;
	(void)time (&nowtime);

	gmt_ptr = gmtime (&nowtime);
	if (gmt_ptr != NULL)
	{
	    /* Make a copy, in case localtime modifies *tm (I think
	       that comment now applies to *gmt_ptr, but I am too
	       lazy to dig into how gmtime and locatime allocate the
	       structures they return pointers to).  */
	    gmt = *gmt_ptr;
	}

	if (! (tm = localtime (&nowtime)))
	    return -1;

	if (gmt_ptr != NULL)
	    ftz.timezone = difftm (&gmt, tm) / 60;
	else
	    /* We are on a system like VMS, where the system clock is
	       in local time and the system has no concept of timezones.
	       Hopefully we can fake this out (for the case in which the
	       user specifies no timezone) by just saying the timezone
	       is zero.  */
	    ftz.timezone = 0;

	if(tm->tm_isdst)
	    ftz.timezone += 60;
    }
    else
    {
	nowtime = now->time;
    }

    tm = localtime(&nowtime);
    yyYear = tm->tm_year + 1900;
    yyMonth = tm->tm_mon + 1;
    yyDay = tm->tm_mday;
    yyTimezone = now->timezone;
    yyDSTmode = DSTmaybe;
    yyHour = 0;
    yyMinutes = 0;
    yySeconds = 0;
    yyMeridian = MER24;
    yyRelSeconds = 0;
    yyRelMonth = 0;
    yyHaveDate = 0;
    yyHaveDay = 0;
    yyHaveRel = 0;
    yyHaveTime = 0;
    yyHaveZone = 0;

    if (yyparse()
     || yyHaveTime > 1 || yyHaveZone > 1 || yyHaveDate > 1 || yyHaveDay > 1)
	return -1;

    if (yyHaveDate || yyHaveTime || yyHaveDay) {
	Start = Convert(yyMonth, yyDay, yyYear, yyHour, yyMinutes, yySeconds,
		    yyMeridian, yyDSTmode);
	if (Start < 0)
	    return -1;
    }
    else {
	Start = nowtime;
	if (!yyHaveRel)
	    Start -= ((tm->tm_hour * 60L + tm->tm_min) * 60L) + tm->tm_sec;
    }

    Start += yyRelSeconds;
    Start += RelativeMonth(Start, yyRelMonth);

    if (yyHaveDay && !yyHaveDate) {
	tod = RelativeDate(Start, yyDayOrdinal, yyDayNumber);
	Start += tod;
    }

    /* Have to do *something* with a legitimate -1 so it's distinguishable
     * from the error return value.  (Alternately could set errno on error.) */
    return Start == -1 ? 0 : Start;
}


#if	defined(TEST)

/* ARGSUSED */
int
main(ac, av)
    int		ac;
    char	*av[];
{
    char	buff[128];
    time_t	d;

    (void)printf("Enter date, or blank line to exit.\n\t> ");
    (void)fflush(stdout);
    while (gets(buff) && buff[0]) {
	d = get_date(buff, (struct timeb *)NULL);
	if (d == -1)
	    (void)printf("Bad format - couldn't convert.\n");
	else
	    (void)printf("%s", ctime(&d));
	(void)printf("\t> ");
	(void)fflush(stdout);
    }
    exit(0);
    /* NOTREACHED */
}
#endif	/* defined(TEST) */
#line 1056 "y.tab.c"
/* allocate initial stack */
#if defined(__STDC__) || defined(__cplusplus)
static int yyinitstack(struct yystack *sp)
#else
static int yyinitstack(sp)
    struct yystack *sp;
#endif
{
    int newsize;
    short *newss;
    YYSTYPE *newvs;

    newsize = YYINITSTACKSIZE;
    newss = (short *)malloc(newsize * sizeof *newss);
    newvs = (YYSTYPE *)malloc(newsize * sizeof *newvs);
    sp->ss = sp->ssp = newss;
    sp->vs = sp->vsp = newvs;
    if (newss == NULL || newvs == NULL) return -1;
    sp->stacksize = newsize;
    sp->sslim = newss + newsize - 1;
    return 0;
}

/* double stack size, up to YYMAXDEPTH */
#if defined(__STDC__) || defined(__cplusplus)
static int yygrowstack(struct yystack *sp)
#else
static int yygrowstack(sp)
    struct yystack *sp;
#endif
{
    int newsize, i;
    short *newss;
    YYSTYPE *newvs;

    if ((newsize = sp->stacksize) >= YYMAXDEPTH) return -1;
    if ((newsize *= 2) > YYMAXDEPTH) newsize = YYMAXDEPTH;
    i = sp->ssp - sp->ss;
    if ((newss = (short *)realloc(sp->ss, newsize * sizeof *newss)) == NULL)
        return -1;
    sp->ss = newss;
    sp->ssp = newss + i;
    if ((newvs = (YYSTYPE *)realloc(sp->vs, newsize * sizeof *newvs)) == NULL)
        return -1;
    sp->vs = newvs;
    sp->vsp = newvs + i;
    sp->stacksize = newsize;
    sp->sslim = newss + newsize - 1;
    return 0;
}

#define YYFREESTACK(sp) { free((sp)->ss); free((sp)->vs); }

#define YYABORT goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR goto yyerrlab
int
yyparse()
{
    register int yym, yyn, yystate, yych;
    register YYSTYPE *yyvsp;
    YYSTYPE yyval;
    struct yystack yystk;
#if YYDEBUG
    register char *yys;
    extern char *getenv();

    if (yys = getenv("YYDEBUG"))
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = yych = YYEMPTY;

    if (yyinitstack(&yystk)) goto yyoverflow;
    *yystk.ssp = yystate = 0;

yyloop:
    if (yyn = yydefred[yystate]) goto yyreduce;
    if (yych < 0)
    {
        if ((yych = YYLEX) < 0) yych = 0;
        yychar = yych;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yych <= YYMAXTOKEN) yys = yyname[yych];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, reading %d (%s)\n",
                    YYPREFIX, yystate, yych, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yych) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yych)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: state %d, shifting to state %d\n",
                    YYPREFIX, yystate, yytable[yyn]);
#endif
        if (yystk.ssp >= yystk.sslim && yygrowstack(&yystk))
            goto yyoverflow;
        *++yystk.ssp = yystate = yytable[yyn];
        *++yystk.vsp = yylval;
        yychar = yych = YYEMPTY;
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if ((yyn = yyrindex[yystate]) && (yyn += yych) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yych)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag) goto yyinrecovery;
#ifdef lint
    goto yynewerror;
#endif
yynewerror:
    yyerror("syntax error");
#ifdef lint
    goto yyerrlab;
#endif
yyerrlab:
    ++yynerrs;
yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yystk.ssp]) &&
                    (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yystk.ssp, yytable[yyn]);
#endif
                if (yystk.ssp >= yystk.sslim && yygrowstack(&yystk))
                    goto yyoverflow;
                *++yystk.ssp = yystate = yytable[yyn];
                *++yystk.vsp = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yystk.ssp);
#endif
                if (yystk.ssp <= yystk.ss) goto yyabort;
                --yystk.ssp;
                --yystk.vsp;
            }
        }
    }
    else
    {
        if (yych == 0) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yych <= YYMAXTOKEN) yys = yyname[yych];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yych, yys);
        }
#endif
        yychar = yych = YYEMPTY;
        goto yyloop;
    }
yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    yyvsp = yystk.vsp; /* for speed in code under switch() */
    yyval = yyvsp[1-yym];
    switch (yyn)
    {
case 3:
#line 197 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyHaveTime++;
	}
break;
case 4:
#line 200 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyHaveZone++;
	}
break;
case 5:
#line 203 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyHaveDate++;
	}
break;
case 6:
#line 206 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyHaveDay++;
	}
break;
case 7:
#line 209 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyHaveRel++;
	}
break;
case 9:
#line 215 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyHour = yyvsp[-1].Number;
	    yyMinutes = 0;
	    yySeconds = 0;
	    yyMeridian = yyvsp[0].Meridian;
	}
break;
case 10:
#line 221 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyHour = yyvsp[-3].Number;
	    yyMinutes = yyvsp[-1].Number;
	    yySeconds = 0;
	    yyMeridian = yyvsp[0].Meridian;
	}
break;
case 11:
#line 227 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyHour = yyvsp[-3].Number;
	    yyMinutes = yyvsp[-1].Number;
	    yyMeridian = MER24;
	    yyDSTmode = DSToff;
	    yyTimezone = - (yyvsp[0].Number % 100 + (yyvsp[0].Number / 100) * 60);
	}
break;
case 12:
#line 234 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyHour = yyvsp[-5].Number;
	    yyMinutes = yyvsp[-3].Number;
	    yySeconds = yyvsp[-1].Number;
	    yyMeridian = yyvsp[0].Meridian;
	}
break;
case 13:
#line 240 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyHour = yyvsp[-5].Number;
	    yyMinutes = yyvsp[-3].Number;
	    yySeconds = yyvsp[-1].Number;
	    yyMeridian = MER24;
	    yyDSTmode = DSToff;
	    yyTimezone = - (yyvsp[0].Number % 100 + (yyvsp[0].Number / 100) * 60);
	}
break;
case 14:
#line 250 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyTimezone = yyvsp[0].Number;
	    yyDSTmode = DSToff;
	}
break;
case 15:
#line 254 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyTimezone = yyvsp[0].Number;
	    yyDSTmode = DSTon;
	}
break;
case 16:
#line 259 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyTimezone = yyvsp[-1].Number;
	    yyDSTmode = DSTon;
	}
break;
case 17:
#line 265 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyDayOrdinal = 1;
	    yyDayNumber = yyvsp[0].Number;
	}
break;
case 18:
#line 269 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyDayOrdinal = 1;
	    yyDayNumber = yyvsp[-1].Number;
	}
break;
case 19:
#line 273 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyDayOrdinal = yyvsp[-1].Number;
	    yyDayNumber = yyvsp[0].Number;
	}
break;
case 20:
#line 279 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyMonth = yyvsp[-2].Number;
	    yyDay = yyvsp[0].Number;
	}
break;
case 21:
#line 283 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    if (yyvsp[-4].Number >= 100) {
		yyYear = yyvsp[-4].Number;
		yyMonth = yyvsp[-2].Number;
		yyDay = yyvsp[0].Number;
	    } else {
		yyMonth = yyvsp[-4].Number;
		yyDay = yyvsp[-2].Number;
		yyYear = yyvsp[0].Number;
	    }
	}
break;
case 22:
#line 294 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    /* ISO 8601 format.  yyyy-mm-dd.  */
	    yyYear = yyvsp[-2].Number;
	    yyMonth = -yyvsp[-1].Number;
	    yyDay = -yyvsp[0].Number;
	}
break;
case 23:
#line 300 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    /* e.g. 17-JUN-1992.  */
	    yyDay = yyvsp[-2].Number;
	    yyMonth = yyvsp[-1].Number;
	    yyYear = -yyvsp[0].Number;
	}
break;
case 24:
#line 306 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyMonth = yyvsp[-1].Number;
	    yyDay = yyvsp[0].Number;
	}
break;
case 25:
#line 310 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyMonth = yyvsp[-3].Number;
	    yyDay = yyvsp[-2].Number;
	    yyYear = yyvsp[0].Number;
	}
break;
case 26:
#line 315 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyMonth = yyvsp[0].Number;
	    yyDay = yyvsp[-1].Number;
	}
break;
case 27:
#line 319 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyMonth = yyvsp[-1].Number;
	    yyDay = yyvsp[-2].Number;
	    yyYear = yyvsp[0].Number;
	}
break;
case 28:
#line 326 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyRelSeconds = -yyRelSeconds;
	    yyRelMonth = -yyRelMonth;
	}
break;
case 30:
#line 333 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyRelSeconds += yyvsp[-1].Number * yyvsp[0].Number * 60L;
	}
break;
case 31:
#line 336 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyRelSeconds += yyvsp[-1].Number * yyvsp[0].Number * 60L;
	}
break;
case 32:
#line 339 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyRelSeconds += yyvsp[0].Number * 60L;
	}
break;
case 33:
#line 342 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyRelSeconds += yyvsp[-1].Number;
	}
break;
case 34:
#line 345 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyRelSeconds += yyvsp[-1].Number;
	}
break;
case 35:
#line 348 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyRelSeconds++;
	}
break;
case 36:
#line 351 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyRelMonth += yyvsp[-1].Number * yyvsp[0].Number;
	}
break;
case 37:
#line 354 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyRelMonth += yyvsp[-1].Number * yyvsp[0].Number;
	}
break;
case 38:
#line 357 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyRelMonth += yyvsp[0].Number;
	}
break;
case 39:
#line 362 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    if (yyHaveTime && yyHaveDate && !yyHaveRel)
		yyYear = yyvsp[0].Number;
	    else {
		if(yyvsp[0].Number>10000) {
		    yyHaveDate++;
		    yyDay= (yyvsp[0].Number)%100;
		    yyMonth= (yyvsp[0].Number/100)%100;
		    yyYear = yyvsp[0].Number/10000;
		}
		else {
		    yyHaveTime++;
		    if (yyvsp[0].Number < 100) {
			yyHour = yyvsp[0].Number;
			yyMinutes = 0;
		    }
		    else {
		    	yyHour = yyvsp[0].Number / 100;
		    	yyMinutes = yyvsp[0].Number % 100;
		    }
		    yySeconds = 0;
		    yyMeridian = MER24;
	        }
	    }
	}
break;
case 40:
#line 389 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyval.Meridian = MER24;
	}
break;
case 41:
#line 392 "/u/scjones/cvs-nightly/lib/getdate.y"
{
	    yyval.Meridian = yyvsp[0].Meridian;
	}
break;
#line 1541 "y.tab.c"
    }
    yystk.ssp -= yym;
    yystate = *yystk.ssp;
    yystk.vsp -= yym;
    yym = yylhs[yyn];
    yych = yychar;
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yystk.ssp = YYFINAL;
        *++yystk.vsp = yyval;
        if (yych < 0)
        {
            if ((yych = YYLEX) < 0) yych = 0;
            yychar = yych;
#if YYDEBUG
            if (yydebug)
            {
                yys = 0;
                if (yych <= YYMAXTOKEN) yys = yyname[yych];
                if (!yys) yys = "illegal-symbol";
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, yych, yys);
            }
#endif
        }
        if (yych == 0) goto yyaccept;
        goto yyloop;
    }
    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: after reduction, shifting from state %d \
to state %d\n", YYPREFIX, *yystk.ssp, yystate);
#endif
    if (yystk.ssp >= yystk.sslim && yygrowstack(&yystk))
        goto yyoverflow;
    *++yystk.ssp = yystate;
    *++yystk.vsp = yyval;
    goto yyloop;
yyoverflow:
    yyerror("yacc stack overflow");
yyabort:
    YYFREESTACK(&yystk);
    return (1);
yyaccept:
    YYFREESTACK(&yystk);
    return (0);
}
