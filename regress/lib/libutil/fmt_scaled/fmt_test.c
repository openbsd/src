/* $OpenBSD: fmt_test.c,v 1.9 2009/06/20 14:23:38 ian Exp $ */

/*
 * Combined tests for fmt_scaled and scan_scaled.
 * Ian Darwin, January 2001. Public domain.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>

#include <util.h>

static int fmt_test(void);
static int scan_test(void);

static void print_errno(int e);
static int assert_int(int testnum, int checknum, int expect, int result);
static int assert_errno(int testnum, int checknum, int expect, int result);
static int assert_quad_t(int testnum, int checknum, quad_t expect, quad_t result);
static int assert_str(int testnum, int checknum, char * expect, char * result);

extern char *__progname;
static int verbose = 0;

__dead static void usage(int stat)
{
	fprintf(stderr, "usage: %s [-v]\n", __progname);
	exit(stat);
}

int
main(int argc, char **argv)
{
	extern char *optarg;
	extern int optind;
	int i, ch;
 
	while ((ch = getopt(argc, argv, "hv")) != -1) {
			switch (ch) {
			case 'v':
					verbose = 1;
					break;
			case 'h':
					usage(0);
			case '?':
			default:
					usage(1);
			}
	}
	argc -= optind;
	argv += optind;

	if (verbose)
		printf("Starting fmt_test\n");
	i = fmt_test();
	if (verbose)
		printf("Starting scan_test\n");
	i += scan_test();
	if (i) {
		printf("*** %d errors in libutil/fmt_scaled tests ***\n", i);
	} else {
		if (verbose)
			printf("Tests done; no unexpected errors\n");
	}
	return i;
}

/************** tests for fmt_scaled *******************/

static struct {			/* the test cases */
	quad_t input;
	char *expect;
	int err;
} ddata[] = {
	{ 0, "0B", 0 },
	{ 1, "1B", 0 },
	{ -1, "-1B", 0 },
	{ 100, "100B", 0},
	{ -100, "-100B", 0},
	{ 999, "999B", 0 },
	{ 1000, "1000B", 0 },
	{ 1023, "1023B", 0 },
	{ -1023, "-1023B", 0 },
	{ 1024, "1.0K", 0 },
	{ 1025, "1.0K", 0 },
	{ 1234, "1.2K", 0 },
	{ -1234, "-1.2K", 0 },
	{ 1484, "1.4K", 0 },		/* rounding boundary, down */
	{ 1485, "1.5K", 0 },		/* rounding boundary, up   */
	{ -1484, "-1.4K", 0 },		/* rounding boundary, down */
	{ -1485, "-1.5K", 0 },		/* rounding boundary, up   */
	{ 1536, "1.5K", 0 },
	{ 1786, "1.7K", 0 },
	{ 1800, "1.8K", 0 },
	{ 2000, "2.0K", 0 },
	{ 123456, "121K", 0 },
	{ 578318, "565K", 0 },
	{ 902948, "882K", 0 },
	{ 1048576, "1.0M", 0},
	{ 1048628, "1.0M", 0},
	{ 1049447, "1.0M", 0},
	{ -102400, "-100K", 0},
	{ -103423, "-101K", 0 },
	{ 7299072, "7.0M", 0 },
	{ 409478144L, "391M", 0 },
	{ -409478144L, "-391M", 0 },
	{ 999999999L, "954M", 0 },
	{ 1499999999L, "1.4G", 0 },
	{ 12475423744LL, "11.6G", 0},
	{ 1LL<<61, "2.0E", 0 },
	{ 1LL<<62, "4.0E", 0 },
	{ 1LL<<63, "", ERANGE },
	{ 1099512676352LL, "1.0T", 0}
};
#	define DDATA_LENGTH (sizeof ddata/sizeof *ddata)

static int
fmt_test(void)
{
	unsigned int i, e, errs = 0;
	int ret;
	char buf[FMT_SCALED_STRSIZE];

	for (i = 0; i < DDATA_LENGTH; i++) {
		strlcpy(buf, "UNSET", FMT_SCALED_STRSIZE);
		ret = fmt_scaled(ddata[i].input, buf);
		e = errno;
		if (verbose) {
			printf("%lld --> %s (%d)", ddata[i].input, buf, ret);
			if (ret == -1)
				print_errno(e);
			printf("\n");
		}
		if (ret == -1)
			errs += assert_int(i, 1, ret, ddata[i].err == 0 ? 0 : -1);
		if (ddata[i].err)
			errs += assert_errno(i, 2, ddata[i].err, errno);
		else
			errs += assert_str(i, 3, ddata[i].expect, buf);
	}

	return errs;
}

/************** tests for scan_scaled *******************/


#define	IMPROBABLE	(-42)

extern int errno;

struct {					/* the test cases */
	char *input;
	quad_t result;
	int err;
} sdata[] = {
	{ "0",		0, 0 },
	{ "123",	123, 0 },
	{ "1k",		1024, 0 },		/* lower case */
	{ "100.944", 100, 0 },	/* should --> 100 (truncates fraction) */
	{ "10099",	10099LL, 0 },
	{ "1M",		1048576LL, 0 },
	{ "1.1M",	1153433LL, 0 },		/* fractions */
	{ "1.111111111111111111M",	1165084LL, 0 },		/* fractions */
	{ "1.55M",	1625292LL, 0 },	/* fractions */
	{ "1.9M",	1992294LL, 0 },		/* fractions */
	{ "-2K",	-2048LL, 0 },		/* negatives */
	{ "-2.2K",	-2252LL, 0 },	/* neg with fract */
	{ "4.5k", 4608, 0 },
	{ "4.5555555555555555K", 4664, 0 },
	{ "4.5555555555555555555K", 4664, 0 },	/* handle enough digits? */
	{ "4.555555555555555555555555555555K", 4664, 0 }, /* ignores extra digits? */
	{ "1G",		1073741824LL, 0 },
	{ "G", 		0, 0 },			/* should == 0G? */
	{ "1234567890", 1234567890LL, 0 },	/* should work */
	{ "1.5E",	1729382256910270464LL, 0 },		/* big */
	{ "32948093840918378473209480483092", 0, ERANGE },  /* too big */
	{ "329480938409.8378473209480483092", 0, ERANGE },  /* fraction too big */
	{ "1.5Q",	0, ERANGE },		/* invalid multiplier (XXX ERANGE??) */
	{ "1ab",	0, ERANGE },		/* ditto */
	{ "5.0e3",	0, EINVAL },	/* digits after */
	{ "5.0E3",	0, EINVAL },	/* ditto */
	{ "1..0",	0, EINVAL },		/* bad format */
	{ "",		0, 0 },			/* boundary */
	{ "--1", -1, EINVAL },
	{ "++42", -1, EINVAL },
	/* { "9223372036854775808", -9223372036854775808LL, 0 }, */	/* XXX  */
};
#	define SDATA_LENGTH (sizeof sdata/sizeof *sdata)

static void
print_errno(int e)
{
	switch(e) {
		case EINVAL: printf("EINVAL"); break;
		case EDOM:   printf("EDOM"); break;
		case ERANGE: printf("ERANGE"); break;
		default: printf("errno %d", errno);
	}
}

/** Print one result */
static void
print(char *input, quad_t result, int ret)
{
	int e = errno;
	printf("\"%10s\" --> %lld (%d)", input, result, ret);
	if (ret == -1) {
		printf(" -- ");
		print_errno(e);
	}
	printf("\n");
}

static int
scan_test(void)
{
	unsigned int i, errs = 0, e;
	int ret;
	quad_t result;

	for (i = 0; i < SDATA_LENGTH; i++) {
		result = IMPROBABLE;
		/* printf("Calling scan_scaled(%s, ...)\n", sdata[i].input); */
		ret = scan_scaled(sdata[i].input, &result);
		e = errno;	/* protect across printfs &c. */
		if (verbose)
			print(sdata[i].input, result, ret);
		errno = e;
		if (ret == -1)
			errs += assert_int(i, 1, ret, sdata[i].err == 0 ? 0 : -1);
		errno = e;
		if (sdata[i].err)
			errs += assert_errno(i, 2, sdata[i].err, errno);
		else 
			errs += assert_quad_t(i, 3, sdata[i].result, result);
	}
	return errs;
}

/************** common testing stuff *******************/

static int
assert_int(int testnum, int check, int expect, int result)
{
	if (expect == result)
		return 0;
	printf("** FAILURE: test %d check %d, expect %d, result %d **\n",
		testnum, check, expect, result);
	return 1;
}

static int
assert_errno(int testnum, int check, int expect, int result)
{
	if (expect == result)
		return 0;
	printf("** FAILURE: test %d check %d, expect ",
		testnum, check);
	print_errno(expect);
	printf(", got ");
	print_errno(result);
	printf(" **\n");
	return 1;
}

static int
assert_quad_t(int testnum, int check, quad_t expect, quad_t result)
{
	if (expect == result)
		return 0;
	printf("** FAILURE: test %d check %d, expect %lld, result %lld **\n",
		testnum, check, expect, result);
	return 1;
}

static int
assert_str(int testnum, int check, char * expect, char * result)
{
	if (strcmp(expect, result) == 0)
		return 0;
	printf("** FAILURE: test %d check %d, expect %s, result %s **\n",
		testnum, check, expect, result);
	return 1;
}
