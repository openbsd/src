/*	$OpenBSD: zdump.c,v 1.10 2015/04/23 05:26:33 deraadt Exp $ */
/*
** This file is in the public domain, so clarified as of
** 2009-05-17 by Arthur David Olson.
*/

/*
** This code has been made independent of the rest of the time
** conversion package to increase confidence in the verification it provides.
** You can use this code to help in verifying other implementations.
*/

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define ZDUMP_LO_YEAR	(-500)
#define ZDUMP_HI_YEAR	2500

#define MAX_STRING_LENGTH	1024

#define TRUE		1
#define FALSE		0

#define SECSPERMIN	60
#define MINSPERHOUR	60
#define SECSPERHOUR	(SECSPERMIN * MINSPERHOUR)
#define HOURSPERDAY	24
#define EPOCH_YEAR	1970
#define TM_YEAR_BASE	1900
#define DAYSPERNYEAR	365

#define SECSPERDAY	((long) SECSPERHOUR * HOURSPERDAY)
#define SECSPERNYEAR	(SECSPERDAY * DAYSPERNYEAR)
#define SECSPERLYEAR	(SECSPERNYEAR + SECSPERDAY)

#define isleap(y) (((y) % 4) == 0 && (((y) % 100) != 0 || ((y) % 400) == 0))

#ifndef isleap_sum
/*
** See tzfile.h for details on isleap_sum.
*/
#define isleap_sum(a, b)	isleap((a) % 400 + (b) % 400)
#endif /* !defined isleap_sum */

extern char	**environ;
extern char	*tzname[2];
extern char 	*__progname;

time_t		absolute_min_time;
time_t		absolute_max_time;
size_t		longest;
int		warned;

static char 		*abbr(struct tm *tmp);
static void		abbrok(const char *abbrp, const char *zone);
static long		delta(struct tm *newp, struct tm *oldp);
static void		dumptime(const struct tm *tmp);
static time_t		hunt(char *name, time_t lot, time_t	hit);
static void		setabsolutes(void);
static void		show(char *zone, time_t t, int v);
static const char 	*tformat(void);
static time_t		yeartot(long y);
static void		usage(void);

static void
abbrok(const char * const abbrp, const char * const zone)
{
	const char 	*cp;
	char 		*wp;

	if (warned)
		return;
	cp = abbrp;
	wp = NULL;
	while (isascii((unsigned char) *cp) && isalpha((unsigned char) *cp))
		++cp;
	if (cp - abbrp == 0)
		wp = "lacks alphabetic at start";
	else if (cp - abbrp < 3)
		wp = "has fewer than 3 alphabetics";
	else if (cp - abbrp > 6)
		wp = "has more than 6 alphabetics";
	if (wp == NULL && (*cp == '+' || *cp == '-')) {
		++cp;
		if (isascii((unsigned char) *cp) &&
		    isdigit((unsigned char) *cp))
			if (*cp++ == '1' && *cp >= '0' && *cp <= '4')
				++cp;
		if (*cp != '\0')
			wp = "differs from POSIX standard";
	}
	if (wp == NULL)
		return;
	fflush(stdout);
	fprintf(stderr, "%s: warning: zone \"%s\" abbreviation \"%s\" %s\n",
		__progname, zone, abbrp, wp);
	warned = TRUE;
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-v] [-c [loyear,]hiyear] zonename ...\n",
	    __progname);
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	int		i, c, vflag = 0;
	char		*cutarg = NULL;
	long		cutloyear = ZDUMP_LO_YEAR;
	long		cuthiyear = ZDUMP_HI_YEAR;
	time_t		cutlotime = 0, cuthitime = 0;
	time_t		now, t, newt;
	struct tm	tm, newtm, *tmp, *newtmp;
	char		**fakeenv;

	while ((c = getopt(argc, argv, "c:v")) == 'c' || c == 'v') {
		switch (c) {
		case 'v':
			vflag = 1;
			break;
		case 'c':
			cutarg = optarg;
			break;
		default:
			usage();
			break;
		}
	}
	if (c != -1 ||
	    (optind == argc - 1 && strcmp(argv[optind], "=") == 0)) {
		usage();
	}
	if (vflag) {
		if (cutarg != NULL) {
			long	lo, hi;
			char	dummy;

			if (sscanf(cutarg, "%ld%c", &hi, &dummy) == 1) {
				cuthiyear = hi;
			} else if (sscanf(cutarg, "%ld,%ld%c",
			    &lo, &hi, &dummy) == 2) {
				cutloyear = lo;
				cuthiyear = hi;
			} else {
				fprintf(stderr, "%s: wild -c argument %s\n",
				    __progname, cutarg);
				exit(EXIT_FAILURE);
			}
		}
		setabsolutes();
		cutlotime = yeartot(cutloyear);
		cuthitime = yeartot(cuthiyear);
	}
	time(&now);
	longest = 0;
	for (i = optind; i < argc; ++i)
		if (strlen(argv[i]) > longest)
			longest = strlen(argv[i]);

	{
		int	from, to;

		for (i = 0; environ[i] != NULL; ++i)
			continue;
		fakeenv = reallocarray(NULL, i + 2, sizeof *fakeenv);
		if (fakeenv == NULL ||
		    (fakeenv[0] = malloc(longest + 4)) == NULL) {
			perror(__progname);
			exit(EXIT_FAILURE);
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
		char	buf[MAX_STRING_LENGTH];

		strlcpy(&fakeenv[0][3], argv[i], longest + 1);
		if (!vflag) {
			show(argv[i], now, FALSE);
			continue;
		}
		warned = FALSE;
		t = absolute_min_time;
		show(argv[i], t, TRUE);
		t += SECSPERHOUR * HOURSPERDAY;
		show(argv[i], t, TRUE);
		if (t < cutlotime)
			t = cutlotime;
		tmp = localtime(&t);
		if (tmp != NULL) {
			tm = *tmp;
			strlcpy(buf, abbr(&tm), sizeof buf);
		}
		for ( ; ; ) {
			if (t >= cuthitime || t >= cuthitime - SECSPERHOUR * 12)
				break;
			newt = t + SECSPERHOUR * 12;
			newtmp = localtime(&newt);
			if (newtmp != NULL)
				newtm = *newtmp;
			if ((tmp == NULL || newtmp == NULL) ? (tmp != newtmp) :
			    (delta(&newtm, &tm) != (newt - t) ||
			    newtm.tm_isdst != tm.tm_isdst ||
			    strcmp(abbr(&newtm), buf) != 0)) {
				newt = hunt(argv[i], t, newt);
				newtmp = localtime(&newt);
				if (newtmp != NULL) {
					newtm = *newtmp;
					strlcpy(buf, abbr(&newtm), sizeof buf);
				}
			}
			t = newt;
			tm = newtm;
			tmp = newtmp;
		}
		t = absolute_max_time;
		t -= SECSPERHOUR * HOURSPERDAY;
		show(argv[i], t, TRUE);
		t += SECSPERHOUR * HOURSPERDAY;
		show(argv[i], t, TRUE);
	}
	if (fflush(stdout) || ferror(stdout)) {
		fprintf(stderr, "%s: ", __progname);
		perror("Error writing to standard output");
		exit(EXIT_FAILURE);
	}
	return 0;
}

static void
setabsolutes(void)
{
	time_t t = 0, t1 = 1;

	while (t < t1) {
		t = t1;
		t1 = 2 * t1 + 1;
	}

	absolute_max_time = t;
	t = -t;
	absolute_min_time = t - 1;
	if (t < absolute_min_time)
		absolute_min_time = t;
}

static time_t
yeartot(const long y)
{
	long	myy = EPOCH_YEAR, seconds;
	time_t	t = 0;

	while (myy != y) {
		if (myy < y) {
			seconds = isleap(myy) ? SECSPERLYEAR : SECSPERNYEAR;
			++myy;
			if (t > absolute_max_time - seconds) {
				t = absolute_max_time;
				break;
			}
			t += seconds;
		} else {
			--myy;
			seconds = isleap(myy) ? SECSPERLYEAR : SECSPERNYEAR;
			if (t < absolute_min_time + seconds) {
				t = absolute_min_time;
				break;
			}
			t -= seconds;
		}
	}
	return t;
}

static time_t
hunt(char *name, time_t lot, time_t hit)
{
	time_t			t;
	long			diff;
	struct tm		lotm, *lotmp;
	struct tm		tm, *tmp;
	char			loab[MAX_STRING_LENGTH];

	lotmp = localtime(&lot);
	if (lotmp != NULL) {
		lotm = *lotmp;
		strlcpy(loab, abbr(&lotm), sizeof loab);
	}
	for ( ; ; ) {
		diff = (long) (hit - lot);
		if (diff < 2)
			break;
		t = lot;
		t += diff / 2;
		if (t <= lot)
			++t;
		else if (t >= hit)
			--t;
		tmp = localtime(&t);
		if (tmp != NULL)
			tm = *tmp;
		if ((lotmp == NULL || tmp == NULL) ? (lotmp == tmp) :
		    (delta(&tm, &lotm) == (t - lot) &&
		    tm.tm_isdst == lotm.tm_isdst &&
		    strcmp(abbr(&tm), loab) == 0)) {
			lot = t;
			lotm = tm;
			lotmp = tmp;
		} else
			hit = t;
	}
	show(name, lot, TRUE);
	show(name, hit, TRUE);
	return hit;
}

/*
** Thanks to Paul Eggert for logic used in delta.
*/

static long
delta(struct tm *newp, struct tm *oldp)
{
	long	result;
	int	tmy;

	if (newp->tm_year < oldp->tm_year)
		return -delta(oldp, newp);
	result = 0;
	for (tmy = oldp->tm_year; tmy < newp->tm_year; ++tmy)
		result += DAYSPERNYEAR + isleap_sum(tmy, TM_YEAR_BASE);
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
show(char *zone, time_t t, int v)
{
	struct tm 	*tmp;

	printf("%-*s  ", (int) longest, zone);
	if (v) {
		tmp = gmtime(&t);
		if (tmp == NULL) {
			printf(tformat(), t);
		} else {
			dumptime(tmp);
			printf(" UTC");
		}
		printf(" = ");
	}
	tmp = localtime(&t);
	dumptime(tmp);
	if (tmp != NULL) {
		if (*abbr(tmp) != '\0')
			printf(" %s", abbr(tmp));
		if (v) {
			printf(" isdst=%d", tmp->tm_isdst);
#ifdef TM_GMTOFF
			printf(" gmtoff=%ld", tmp->TM_GMTOFF);
#endif /* defined TM_GMTOFF */
		}
	}
	printf("\n");
	if (tmp != NULL && *abbr(tmp) != '\0')
		abbrok(abbr(tmp), zone);
}

static char *
abbr(struct tm *tmp)
{
	char 		*result;
	static char	nada;

	if (tmp->tm_isdst != 0 && tmp->tm_isdst != 1)
		return &nada;
	result = tzname[tmp->tm_isdst];
	return (result == NULL) ? &nada : result;
}

/*
** The code below can fail on certain theoretical systems;
** it works on all known real-world systems as of 2004-12-30.
*/

static const char *
tformat(void)
{
	return "%lld";
}

static void
dumptime(const struct tm *timeptr)
{
	static const char wday_name[][3] = {
		"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
	};
	static const char mon_name[][3] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	const char	*wn, *mn;
	int		lead, trail;

	if (timeptr == NULL) {
		printf("NULL");
		return;
	}
	/*
	** The packaged versions of localtime and gmtime never put out-of-range
	** values in tm_wday or tm_mon, but since this code might be compiled
	** with other (perhaps experimental) versions, paranoia is in order.
	*/
	if (timeptr->tm_wday < 0 || timeptr->tm_wday >=
	    (int) (sizeof wday_name / sizeof wday_name[0]))
		wn = "???";
	else
		wn = wday_name[timeptr->tm_wday];
	if (timeptr->tm_mon < 0 || timeptr->tm_mon >=
	    (int) (sizeof mon_name / sizeof mon_name[0]))
		mn = "???";
	else
		mn = mon_name[timeptr->tm_mon];
	printf("%.3s %.3s%3d %.2d:%.2d:%.2d ",
	    wn, mn,
	    timeptr->tm_mday, timeptr->tm_hour,
	    timeptr->tm_min, timeptr->tm_sec);
#define DIVISOR	10
	trail = timeptr->tm_year % DIVISOR + TM_YEAR_BASE % DIVISOR;
	lead = timeptr->tm_year / DIVISOR + TM_YEAR_BASE / DIVISOR +
		trail / DIVISOR;
	trail %= DIVISOR;
	if (trail < 0 && lead > 0) {
		trail += DIVISOR;
		--lead;
	} else if (lead < 0 && trail > 0) {
		trail -= DIVISOR;
		++lead;
	}
	if (lead == 0)
		printf("%d", trail);
	else
		printf("%d%d", lead, ((trail < 0) ? -trail : trail));
}
