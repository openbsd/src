/*
** This file is in the public domain, so clarified as of
** Feb 14, 2003 by Arthur David Olson (arthur_david_olson@nih.gov).
*/

#if defined(LIBC_SCCS) && !defined(lint) && !defined(NOID)
static char elsieid[] = "@(#)zdump.c	7.31";
static char rcsid[] = "$OpenBSD: zdump.c,v 1.14 2004/10/18 22:33:43 millert Exp $";
static char	elsieid[] = "@(#)zdump.c	7.40";

/*
** This code has been made independent of the rest of the time
** conversion package to increase confidence in the verification it provides.
** You can use this code to help in verifying other implementations.
*/

#include "stdio.h"	/* for stdout, stderr, perror */
#include "string.h"	/* for strlcpy */
#include "sys/types.h"	/* for time_t */
#include "time.h"	/* for struct tm */
#include "stdlib.h"	/* for exit, malloc, atoi */

#ifndef MAX_STRING_LENGTH
#define MAX_STRING_LENGTH	1024
#endif /* !defined MAX_STRING_LENGTH */

#ifndef TRUE
#define TRUE		1
#endif /* !defined TRUE */

#ifndef FALSE
#define FALSE		0
#endif /* !defined FALSE */

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS	0
#endif /* !defined EXIT_SUCCESS */

#ifndef EXIT_FAILURE
#define EXIT_FAILURE	1
#endif /* !defined EXIT_FAILURE */

#ifndef SECSPERMIN
#define SECSPERMIN	60
#endif /* !defined SECSPERMIN */

#ifndef MINSPERHOUR
#define MINSPERHOUR	60
#endif /* !defined MINSPERHOUR */

#ifndef SECSPERHOUR
#define SECSPERHOUR	(SECSPERMIN * MINSPERHOUR)
#endif /* !defined SECSPERHOUR */

#ifndef HOURSPERDAY
#define HOURSPERDAY	24
#endif /* !defined HOURSPERDAY */

#ifndef EPOCH_YEAR
#define EPOCH_YEAR	1970
#endif /* !defined EPOCH_YEAR */

#ifndef TM_YEAR_BASE
#define TM_YEAR_BASE	1900
#endif /* !defined TM_YEAR_BASE */

#ifndef DAYSPERNYEAR
#define DAYSPERNYEAR	365
#endif /* !defined DAYSPERNYEAR */

#ifndef isleap
#define isleap(y) ((((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0)
#endif /* !defined isleap */

#if HAVE_GETTEXT
#include "locale.h"	/* for setlocale */
#include "libintl.h"
#endif /* HAVE_GETTEXT */

#ifndef GNUC_or_lint
#ifdef lint
#define GNUC_or_lint
#else /* !defined lint */
#ifdef __GNUC__
#define GNUC_or_lint
#endif /* defined __GNUC__ */
#endif /* !defined lint */
#endif /* !defined GNUC_or_lint */

#ifndef INITIALIZE
#ifdef GNUC_or_lint
#define INITIALIZE(x)	((x) = 0)
#else /* !defined GNUC_or_lint */
#define INITIALIZE(x)
#endif /* !defined GNUC_or_lint */
#endif /* !defined INITIALIZE */

/*
** For the benefit of GNU folk...
** `_(MSGID)' uses the current locale's message library string for MSGID.
** The default is to use gettext if available, and use MSGID otherwise.
*/

#ifndef _
#if HAVE_GETTEXT
#define _(msgid) gettext(msgid)
#else /* !HAVE_GETTEXT */
#define _(msgid) msgid
#endif /* !HAVE_GETTEXT */
#endif /* !defined _ */

#ifndef TZ_DOMAIN
#define TZ_DOMAIN "tz"
#endif /* !defined TZ_DOMAIN */

#ifndef P
#ifdef __STDC__
#define P(x)	x
#else /* !defined __STDC__ */
#define P(x)	()
#endif /* !defined __STDC__ */
#endif /* !defined P */

extern char **	environ;
extern int	getopt P((int argc, char * const argv[],
			  const char * options));
extern char *	optarg;
extern int	optind;
extern char *	tzname[2];

static char *	abbr P((struct tm * tmp));
static long	delta P((struct tm * newp, struct tm * oldp));
static time_t	hunt P((char * name, time_t lot, time_t	hit));
static size_t	longest;
static char *	progname;
static void	show P((char * zone, time_t t, int v));
static void	dumptime P((const struct tm * tmp));

int
main(argc, argv)
int	argc;
char *	argv[];
{
	register int		i;
	register int		c;
	register int		vflag;
	register char *		cutoff;
	register int		cutyear;
	register long		cuttime;
	char **			fakeenv;
	time_t			now;
	time_t			t;
	time_t			newt;
	time_t			hibit;
	struct tm		tm;
	struct tm		newtm;

	INITIALIZE(cuttime);
#if HAVE_GETTEXT
	(void) setlocale(LC_MESSAGES, "");
#ifdef TZ_DOMAINDIR
	(void) bindtextdomain(TZ_DOMAIN, TZ_DOMAINDIR);
#endif /* defined TEXTDOMAINDIR */
	(void) textdomain(TZ_DOMAIN);
#endif /* HAVE_GETTEXT */
	progname = argv[0];
	vflag = 0;
	cutoff = NULL;
	while ((c = getopt(argc, argv, "c:v")) == 'c' || c == 'v')
		if (c == 'v')
			vflag = 1;
		else	cutoff = optarg;
	if ((c != EOF && c != -1) ||
		(optind == argc - 1 && strcmp(argv[optind], "=") == 0)) {
			(void) fprintf(stderr,
_("%s: usage is %s [-v] [-c cutoffyear] zonename ...\n"),
				argv[0], argv[0]);
			(void) exit(EXIT_FAILURE);
	}
	if (cutoff != NULL) {
		int	y;

		cutyear = atoi(cutoff);
		cuttime = 0;
		for (y = EPOCH_YEAR; y < cutyear; ++y)
			cuttime += DAYSPERNYEAR + isleap(y);
		cuttime *= SECSPERHOUR * HOURSPERDAY;
	}
	(void) time(&now);
	longest = 0;
	for (i = optind; i < argc; ++i)
		if (strlen(argv[i]) > longest)
			longest = strlen(argv[i]);
	for (hibit = 1; (hibit << 1) != 0; hibit <<= 1)
		continue;
	{
		register int	from;
		register int	to;

		for (i = 0;  environ[i] != NULL;  ++i)
			continue;
		fakeenv = (char **) malloc((size_t) ((i + 2) *
			sizeof *fakeenv));
		if (fakeenv == NULL ||
			(fakeenv[0] = (char *) malloc(longest + 4)) == NULL) {
					(void) perror(progname);
					(void) exit(EXIT_FAILURE);
		}
		to = 0;
		strlcpy(fakeenv[to++], "TZ=", longest + 4);
		for (from = 0; environ[from] != NULL; ++from)
			if (strncmp(environ[from], "TZ=", 3) != 0)
				fakeenv[to++] = environ[from];
		fakeenv[to] = NULL;
		environ = fakeenv;
	}
	for (i = optind; i < argc; ++i) {
		static char	buf[MAX_STRING_LENGTH];

		strlcpy(&fakeenv[0][3], argv[i], longest + 1);
		if (!vflag) {
			show(argv[i], now, FALSE);
			continue;
		}
		/*
		** Get lowest value of t.
		*/
		t = hibit;
		if (t > 0)		/* time_t is unsigned */
			t = 0;
		show(argv[i], t, TRUE);
		t += SECSPERHOUR * HOURSPERDAY;
		show(argv[i], t, TRUE);
		tm = *localtime(&t);
		strlcpy(buf, abbr(&tm), (sizeof buf));
		for ( ; ; ) {
			if (cutoff != NULL && t >= cuttime)
				break;
			newt = t + SECSPERHOUR * 12;
			if (cutoff != NULL && newt >= cuttime)
				break;
			if (newt <= t)
				break;
			newtm = *localtime(&newt);
			if (delta(&newtm, &tm) != (newt - t) ||
				newtm.tm_isdst != tm.tm_isdst ||
				strcmp(abbr(&newtm), buf) != 0) {
					newt = hunt(argv[i], t, newt);
					newtm = *localtime(&newt);
					strlcpy(buf, abbr(&newtm),
						(sizeof buf));
			}
			t = newt;
			tm = newtm;
		}
		/*
		** Get highest value of t.
		*/
		t = ~((time_t) 0);
		if (t < 0)		/* time_t is signed */
			t &= ~hibit;
		t -= SECSPERHOUR * HOURSPERDAY;
		show(argv[i], t, TRUE);
		t += SECSPERHOUR * HOURSPERDAY;
		show(argv[i], t, TRUE);
	}
	if (fflush(stdout) || ferror(stdout)) {
		(void) fprintf(stderr, "%s: ", argv[0]);
		(void) perror(_("Error writing standard output"));
		(void) exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);

	/* gcc -Wall pacifier */
	for ( ; ; )
		continue;
}

static time_t
hunt(name, lot, hit)
char *	name;
time_t	lot;
time_t	hit;
{
	time_t		t;
	struct tm	lotm;
	struct tm	tm;
	static char	loab[MAX_STRING_LENGTH];

	lotm = *localtime(&lot);
	strlcpy(loab, abbr(&lotm), (sizeof loab));
	while ((hit - lot) >= 2) {
		t = lot / 2 + hit / 2;
		if (t <= lot)
			++t;
		else if (t >= hit)
			--t;
		tm = *localtime(&t);
		if (delta(&tm, &lotm) == (t - lot) &&
			tm.tm_isdst == lotm.tm_isdst &&
			strcmp(abbr(&tm), loab) == 0) {
				lot = t;
				lotm = tm;
		} else	hit = t;
	}
	show(name, lot, TRUE);
	show(name, hit, TRUE);
	return hit;
}

/*
** Thanks to Paul Eggert (eggert@twinsun.com) for logic used in delta.
*/

static long
delta(newp, oldp)
struct tm *	newp;
struct tm *	oldp;
{
	long	result;
	int	tmy;

	if (newp->tm_year < oldp->tm_year)
		return -delta(oldp, newp);
	result = 0;
	for (tmy = oldp->tm_year; tmy < newp->tm_year; ++tmy)
		result += DAYSPERNYEAR + isleap(tmy + (long) TM_YEAR_BASE);
	result += newp->tm_yday - oldp->tm_yday;
	result *= HOURSPERDAY;
	result += newp->tm_hour - oldp->tm_hour;
	result *= MINSPERHOUR;
	result += newp->tm_min - oldp->tm_min;
	result *= SECSPERMIN;
	result += newp->tm_sec - oldp->tm_sec;
	return result;
}

static void
show(zone, t, v)
char *	zone;
time_t	t;
int	v;
{
	struct tm *	tmp;

	(void) printf("%-*s  ", (int) longest, zone);
	if (v) {
		dumptime(gmtime(&t));
		(void) printf(" UTC = ");
	}
	tmp = localtime(&t);
	dumptime(tmp);
	if (*abbr(tmp) != '\0')
		(void) printf(" %s", abbr(tmp));
	if (v) {
		(void) printf(" isdst=%d", tmp->tm_isdst);
#ifdef TM_GMTOFF
		(void) printf(" gmtoff=%ld", tmp->TM_GMTOFF);
#endif /* defined TM_GMTOFF */
	}
	(void) printf("\n");
}

static char *
abbr(tmp)
struct tm *	tmp;
{
	register char *	result;
	static char	nada;

	if (tmp->tm_isdst != 0 && tmp->tm_isdst != 1)
		return &nada;
	result = tzname[tmp->tm_isdst];
	return (result == NULL) ? &nada : result;
}

static void
dumptime(timeptr)
register const struct tm *	timeptr;
{
	static const char	wday_name[][3] = {
		"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
	};
	static const char	mon_name[][3] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	register const char *	wn;
	register const char *	mn;

	/*
	** The packaged versions of localtime and gmtime never put out-of-range
	** values in tm_wday or tm_mon, but since this code might be compiled
	** with other (perhaps experimental) versions, paranoia is in order.
	*/
	if (timeptr->tm_wday < 0 || timeptr->tm_wday >=
		(int) (sizeof wday_name / sizeof wday_name[0]))
			wn = "???";
	else		wn = wday_name[timeptr->tm_wday];
	if (timeptr->tm_mon < 0 || timeptr->tm_mon >=
		(int) (sizeof mon_name / sizeof mon_name[0]))
			mn = "???";
	else		mn = mon_name[timeptr->tm_mon];
	(void) printf("%.3s %.3s%3d %.2d:%.2d:%.2d %ld",
		wn, mn,
		timeptr->tm_mday, timeptr->tm_hour,
		timeptr->tm_min, timeptr->tm_sec,
		timeptr->tm_year + (long) TM_YEAR_BASE);
}
