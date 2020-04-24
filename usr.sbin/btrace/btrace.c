/*	$OpenBSD: btrace.c,v 1.18 2020/04/24 14:56:43 mpi Exp $ */

/*
 * Copyright (c) 2019 - 2020 Martin Pieuchot <mpi@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/ioctl.h>
#include <sys/exec_elf.h>
#include <sys/syscall.h>
#include <sys/queue.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <dev/dt/dtvar.h>

#include "btrace.h"
#include "bt_parser.h"

/*
 * Maximum number of operands an arithmetic operation can have.  This
 * is necessary to stop infinite recursion when evaluating expressions.
 */
#define __MAXOPERANDS	5

#define __PATH_DEVDT "/dev/dt"

__dead void		 usage(void);
char			*read_btfile(const char *);

/*
 * Retrieve & parse probe information.
 */
void			 dtpi_cache(int);
void			 dtpi_print_list(void);
char			*dtpi_func(struct dtioc_probe_info *);
int			 dtpi_is_unit(const char *);
struct dtioc_probe_info	*dtpi_get_by_value(const char *, const char *,
			     const char *);

/*
 * Main loop and rule evaluation.
 */
void			 rules_do(int);
void			 rules_setup(int);
void			 rules_apply(struct dt_evt *);
void			 rules_teardown(int);
void			 rule_eval(struct bt_rule *, struct dt_evt *);
void			 rule_printmaps(struct bt_rule *);

/*
 * Language builtins & functions.
 */
uint64_t		 builtin_nsecs(struct dt_evt *);
const char		*builtin_kstack(struct dt_evt *);
const char		*builtin_arg(struct dt_evt *, enum bt_argtype);
void			 stmt_clear(struct bt_stmt *);
void			 stmt_delete(struct bt_stmt *, struct dt_evt *);
void			 stmt_insert(struct bt_stmt *, struct dt_evt *);
void			 stmt_print(struct bt_stmt *, struct dt_evt *);
void			 stmt_store(struct bt_stmt *, struct dt_evt *);
void			 stmt_time(struct bt_stmt *, struct dt_evt *);
void			 stmt_zero(struct bt_stmt *);
struct bt_arg		*ba_read(struct bt_arg *);
const char		*ba2hash(struct bt_arg *, struct dt_evt *);
int			 ba2dtflags(struct bt_arg *);

/*
 * Debug routines.
 */
__dead void		 xabort(const char *, ...);
void			 debug(const char *, ...);
void			 debugx(const char *, ...);
const char		*debug_rule_name(struct bt_rule *);
void			 debug_dump_filter(struct bt_rule *);
void			 debug_dump_rule(struct bt_rule *);

struct dtioc_probe_info	*dt_dtpis;	/* array of available probes */
size_t			 dt_ndtpi;	/* # of elements in the array */

int			 verbose = 0;
volatile sig_atomic_t	 quit_pending;

static void
signal_handler(int sig)
{
	quit_pending = sig;
}


int
main(int argc, char *argv[])
{
	int fd = -1, ch, error = 0;
	const char *filename = NULL, *btscript = NULL;
	int showprobes = 0;

	setlocale(LC_ALL, "");

#if notyet
	if (pledge("stdio rpath", NULL) == -1)
		err(1, "pledge");
#endif

	while ((ch = getopt(argc, argv, "e:lv")) != -1) {
		switch (ch) {
		case 'e':
			btscript = optarg;
			break;
		case 'l':
			showprobes = 1;
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc > 0) {
		if (btscript != NULL)
			usage();

		filename = argv[0];
		btscript = read_btfile(filename);
		argc--;
		argv++;
	}

	if (argc != 0 || (btscript == NULL && !showprobes))
		usage();

	if (btscript != NULL) {
		error = btparse(btscript, strlen(btscript), filename, 1);
		if (error)
			return error;
	}

	if (showprobes || g_nprobes > 0) {
		fd = open(__PATH_DEVDT, O_RDONLY);
		if (fd == -1)
			err(1, "could not open %s", __PATH_DEVDT);
	}

	if (showprobes) {
		dtpi_cache(fd);
		dtpi_print_list();
	}

	if (!TAILQ_EMPTY(&g_rules))
		rules_do(fd);

	if (fd != -1)
		close(fd);

	return error;
}

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s [-lv] [-e program|file]\n",
	    getprogname());
	exit(1);
}

char *
read_btfile(const char *filename)
{
	static char fcontent[BUFSIZ];
	long offset;
	FILE *fp;

	fp = fopen(filename, "r");
	if (fp == NULL)
		err(1, "can't open '%s'", filename);

	if (fread(fcontent, sizeof(fcontent) - 1, 1, fp) == 0 && errno != 0)
		err(1, "can't read '%s'", filename);

	fseek(fp, 0, SEEK_END);
	offset = ftell(fp);
	if ((size_t)offset >= sizeof(fcontent))
		errx(1, "couldn't read all of '%s'", filename);

	fclose(fp);
	return fcontent;
}

void
dtpi_cache(int fd)
{
	struct dtioc_probe dtpr;

	if (dt_dtpis != NULL)
		return;

	memset(&dtpr, 0, sizeof(dtpr));
	if (ioctl(fd, DTIOCGPLIST, &dtpr))
		err(1, "DTIOCGPLIST");

	dt_ndtpi = (dtpr.dtpr_size / sizeof(*dt_dtpis));
	dt_dtpis = reallocarray(NULL, dt_ndtpi, sizeof(*dt_dtpis));
	if (dt_dtpis == NULL)
		err(1, "malloc");

	dtpr.dtpr_probes = dt_dtpis;
	if (ioctl(fd, DTIOCGPLIST, &dtpr))
		err(1, "DTIOCGPLIST");
}

void
dtpi_print_list(void)
{
	struct dtioc_probe_info *dtpi;
	size_t i;

	dtpi = dt_dtpis;
	for (i = 0; i < dt_ndtpi; i++, dtpi++) {
		printf("%s:%s:%s\n", dtpi->dtpi_prov, dtpi_func(dtpi),
		    dtpi->dtpi_name);
	}
}

char *
dtpi_func(struct dtioc_probe_info *dtpi)
{
	char *sysnb, func[DTNAMESIZE];
	const char *errstr;
	int idx;

	if (strncmp(dtpi->dtpi_prov, "syscall", DTNAMESIZE))
		return dtpi->dtpi_func;

	/* Translate syscall names */
	strlcpy(func, dtpi->dtpi_func, sizeof(func));
	sysnb = func;
	if (strsep(&sysnb, "%") == NULL)
		return dtpi->dtpi_func;

	idx = strtonum(sysnb, 1, SYS_MAXSYSCALL, &errstr);
	if (errstr != NULL)
		return dtpi->dtpi_func;

	return syscallnames[idx];
}

int
dtpi_is_unit(const char *unit)
{
	return !strncmp("hz", unit, sizeof("hz"));
}

struct dtioc_probe_info *
dtpi_get_by_value(const char *prov, const char *func, const char *name)
{
	struct dtioc_probe_info *dtpi;
	size_t i;

	dtpi = dt_dtpis;
	for (i = 0; i < dt_ndtpi; i++, dtpi++) {
		if (prov != NULL &&
		    strncmp(prov, dtpi->dtpi_prov, DTNAMESIZE))
			continue;

		if (func != NULL) {
			if (dtpi_is_unit(func))
				return dtpi;

			if (strncmp(func, dtpi_func(dtpi), DTNAMESIZE))
				continue;
		}

		if (strncmp(name, dtpi->dtpi_name, DTNAMESIZE))
			continue;

		debug("matched probe %s:%s:%s\n", dtpi->dtpi_prov,
		    dtpi_func(dtpi), dtpi->dtpi_name);
		return dtpi;
	}

	return NULL;
}

void
rules_do(int fd)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = signal_handler;
	if (sigaction(SIGINT, &sa, NULL))
		err(1, "sigaction");

	rules_setup(fd);

	while (!quit_pending && g_nprobes > 0) {
		static struct dt_evt devtbuf[64];
		ssize_t rlen;
		size_t i;

		rlen = read(fd, devtbuf, sizeof(devtbuf) - 1);
		if (rlen == -1) {
			if (errno == EINTR && quit_pending)
				break;
			err(1, "read");
		}

		if ((rlen % sizeof(struct dt_evt)) != 0)
			err(1, "incorrect read");


		for (i = 0; i < nitems(devtbuf); i++) {
			struct dt_evt *dtev = &devtbuf[i];

			if (dtev->dtev_tid == 0)
				break;

			rules_apply(dtev);
		}
	}

	rules_teardown(fd);

	if (verbose && fd != -1) {
		struct dtioc_stat dtst;

		memset(&dtst, 0, sizeof(dtst));
		if (ioctl(fd, DTIOCGSTATS, &dtst))
			warn("DTIOCGSTATS");

		printf("%llu events read\n", dtst.dtst_readevt);
		printf("%llu events dropped\n", dtst.dtst_dropevt);
	}
}

static inline enum dt_operand
dop2dt(enum bt_operand op)
{
	switch (op) {
	case B_OP_EQ:	return DT_OP_EQ;
	case B_OP_NE:	return DT_OP_NE;
	case B_OP_NONE:	return DT_OP_NONE;
	default:	break;
	}
	xabort("unknown operand %d", op);
}


static inline enum dt_filtervar
dvar2dt(enum bt_filtervar var)
{
	switch (var) {
	case B_FV_PID:	return DT_FV_PID;
	case B_FV_TID:	return DT_FV_TID;
	case B_FV_NONE:	return DT_FV_NONE;
	default:	break;
	}
	xabort("unknown filter %d", var);
}


void
rules_setup(int fd)
{
	struct dtioc_probe_info *dtpi;
	struct dtioc_req *dtrq;
	struct bt_rule *r, *rbegin = NULL;
	struct bt_probe *bp;
	struct bt_stmt *bs;
	int dokstack = 0, on = 1;

	TAILQ_FOREACH(r, &g_rules, br_next) {
		debug_dump_rule(r);

		if (r->br_type != B_RT_PROBE) {
			if (r->br_type == B_RT_BEGIN)
				rbegin = r;
			continue;
		}

		bp = r->br_probe;
		dtpi_cache(fd);
		dtpi = dtpi_get_by_value(bp->bp_prov, bp->bp_func, bp->bp_name);
		if (dtpi == NULL) {
			errx(1, "probe '%s:%s:%s' not found", bp->bp_prov,
			    bp->bp_func, bp->bp_name);
		}

		dtrq = calloc(1, sizeof(*dtrq));
		if (dtrq == NULL)
			err(1, "dtrq: 1alloc");

		r->br_pbn = dtpi->dtpi_pbn;
		dtrq->dtrq_pbn = dtpi->dtpi_pbn;
		if (r->br_filter) {
			struct bt_filter *df = r->br_filter;

			dtrq->dtrq_filter.dtf_operand = dop2dt(df->bf_op);
			dtrq->dtrq_filter.dtf_variable = dvar2dt(df->bf_var);
			dtrq->dtrq_filter.dtf_value = df->bf_val;
		}
		dtrq->dtrq_rate = r->br_probe->bp_rate;

		SLIST_FOREACH(bs, &r->br_action, bs_next) {
			struct bt_arg *ba;

			SLIST_FOREACH(ba, &bs->bs_args, ba_next)
				dtrq->dtrq_evtflags |= ba2dtflags(ba);
		}

		if (dtrq->dtrq_evtflags & DTEVT_KSTACK)
			dokstack = 1;
		r->br_cookie = dtrq;
	}

	if (dokstack)
		kelf_open();

	if (rbegin)
		rule_eval(rbegin, NULL);

	/* Enable all probes */
	TAILQ_FOREACH(r, &g_rules, br_next) {
		if (r->br_type != B_RT_PROBE)
			continue;

		dtrq = r->br_cookie;
		if (ioctl(fd, DTIOCPRBENABLE, dtrq))
			err(1, "DTIOCPRBENABLE");
	}

	if (g_nprobes > 0) {
		if (ioctl(fd, DTIOCRECORD, &on))
			err(1, "DTIOCRECORD");
	}
}

void
rules_apply(struct dt_evt *dtev)
{
	struct bt_rule *r;

	TAILQ_FOREACH(r, &g_rules, br_next) {
		if (r->br_type != B_RT_PROBE || r->br_pbn != dtev->dtev_pbn)
			continue;

		rule_eval(r, dtev);
	}
}

void
rules_teardown(int fd)
{
	struct dtioc_req *dtrq;
	struct bt_rule *r, *rend = NULL;
	int dokstack = 0, off = 0;

	if (g_nprobes > 0) {
		if (ioctl(fd, DTIOCRECORD, &off))
			err(1, "DTIOCRECORD");
	}

	TAILQ_FOREACH(r, &g_rules, br_next) {
		if (r->br_type != B_RT_PROBE) {
			if (r->br_type == B_RT_END)
				rend = r;
			continue;
		}

		dtrq = r->br_cookie;
		if (dtrq->dtrq_evtflags & DTEVT_KSTACK)
			dokstack = 1;
	}

	if (dokstack)
		kelf_close();

	if (rend)
		rule_eval(rend, NULL);
	else {
		TAILQ_FOREACH(r, &g_rules, br_next)
			rule_printmaps(r);
	}
}

void
rule_eval(struct bt_rule *r, struct dt_evt *dtev)
{
	struct bt_stmt *bs;

	debug("eval rule '%s'\n", debug_rule_name(r));

	SLIST_FOREACH(bs, &r->br_action, bs_next) {
		switch (bs->bs_act) {
		case B_AC_STORE:
			stmt_store(bs, dtev);
			break;
		case B_AC_INSERT:
			stmt_insert(bs, dtev);
			break;
		case B_AC_CLEAR:
			stmt_clear(bs);
			break;
		case B_AC_DELETE:
			stmt_delete(bs, dtev);
			break;
		case B_AC_EXIT:
			exit(0);
			break;
		case B_AC_PRINT:
			stmt_print(bs, dtev);
			break;
		case B_AC_PRINTF:
			stmt_printf(bs, dtev);
			break;
		case B_AC_TIME:
			stmt_time(bs, dtev);
			break;
		case B_AC_ZERO:
			stmt_zero(bs);
			break;
		default:
			xabort("no handler for action type %d", bs->bs_act);
		}
	}
}

void
rule_printmaps(struct bt_rule *r)
{
	struct bt_stmt *bs;

	SLIST_FOREACH(bs, &r->br_action, bs_next) {
		struct bt_arg *ba;

		SLIST_FOREACH(ba, &bs->bs_args, ba_next) {
			struct bt_var *bv = ba->ba_value;

			if (ba->ba_type != B_AT_MAP)
				continue;

			if (bv->bv_value != NULL) {
				struct map *map = (struct map *)bv->bv_value;

				map_print(map, SIZE_T_MAX, bv_name(bv));
				map_clear(map);
				bv->bv_value = NULL;
			}
		}
	}
}

time_t
builtin_gettime(struct dt_evt *dtev)
{
	struct timespec ts;

	if (dtev == NULL) {
		clock_gettime(CLOCK_REALTIME, &ts);
		return ts.tv_sec;
	}

	return dtev->dtev_tsp.tv_sec;
}

static inline uint64_t
TIMESPEC_TO_NSEC(struct timespec *ts)
{
	return (ts->tv_sec * 1000000000L + ts->tv_nsec);
}

uint64_t
builtin_nsecs(struct dt_evt *dtev)
{
	struct timespec ts;

	if (dtev == NULL) {
		clock_gettime(CLOCK_REALTIME, &ts);
		return TIMESPEC_TO_NSEC(&ts);
	}

	return TIMESPEC_TO_NSEC(&dtev->dtev_tsp);
}

const char *
builtin_stack(struct dt_evt *dtev, int kernel)
{
	struct stacktrace *st = &dtev->dtev_kstack;
	static char buf[4096];
	size_t i;
	int n = 0;

	if (!kernel || st->st_count == 0)
		return "";

	for (i = 0; i < st->st_count; i++) {
		n += kelf_snprintsym(buf + n, sizeof(buf) - 1 - n,
		    st->st_pc[i]);
	}

	return buf;
}

const char *
builtin_arg(struct dt_evt *dtev, enum bt_argtype dat)
{
	static char buf[sizeof("18446744073709551615")]; /* UINT64_MAX */

	snprintf(buf, sizeof(buf) - 1, "%lu", dtev->dtev_sysargs[dat - B_AT_BI_ARG0]);
	return buf;
}


/*
 * Empty a map:		{ clear(@map); }
 */
void
stmt_clear(struct bt_stmt *bs)
{
	struct bt_arg *ba = SLIST_FIRST(&bs->bs_args);
	struct bt_var *bv = ba->ba_value;

	assert(bs->bs_var == NULL);
	assert(ba->ba_type == B_AT_VAR);

	map_clear((struct map *)bv->bv_value);
	bv->bv_value = NULL;

	debug("map=%p '%s' clear\n", bv->bv_value, bv_name(bv));
}

/*
 * Map delete:	 	{ delete(@map[key]); }
 *
 * In this case 'map' is represented by `bv' and 'key' by `bkey'.
 */
void
stmt_delete(struct bt_stmt *bs, struct dt_evt *dtev)
{
	struct bt_arg *bkey, *bmap = SLIST_FIRST(&bs->bs_args);
	struct bt_var *bv = bmap->ba_value;
	const char *hash;

	assert(bmap->ba_type == B_AT_MAP);
	assert(bs->bs_var == NULL);

	bkey = bmap->ba_key;
	hash = ba2hash(bkey, dtev);
	debug("map=%p '%s' delete key=%p '%s'\n", bv->bv_value, bv_name(bv),
	    bkey, hash);

	map_delete((struct map *)bv->bv_value, hash);
}

/*
 * Map insert:	 	{ @map[key] = 42; }
 *
 * In this case 'map' is represented by `bv', 'key' by `bkey' and
 * '42' by `bval'.
 */
void
stmt_insert(struct bt_stmt *bs, struct dt_evt *dtev)
{
	struct bt_arg *bkey, *bmap = SLIST_FIRST(&bs->bs_args);
	struct bt_arg *bval = (struct bt_arg *)bs->bs_var;
	struct bt_var *bv = bmap->ba_value;
	const char *hash;

	assert(bmap->ba_type == B_AT_MAP);
	assert(SLIST_NEXT(bval, ba_next) == NULL);

	bkey = bmap->ba_key;
	hash = ba2hash(bkey, dtev);
	debug("map=%p '%s' insert key=%p '%s' bval=%p\n", bv->bv_value,
	    bv_name(bv), bkey, hash, bval);

	bv->bv_value = (struct bt_arg *)map_insert((struct map *)bv->bv_value,
	    hash, bval);
}

/*
 * Print map entries:	{ print(@map[, 8]); }
 *
 * In this case the global variable 'map' is pointed at by `ba'
 * and '8' is represented by `btop'.
 */
void
stmt_print(struct bt_stmt *bs, struct dt_evt *dtev)
{
	struct bt_arg *btop, *ba = SLIST_FIRST(&bs->bs_args);
	struct bt_var *bv = ba->ba_value;
	size_t top = SIZE_T_MAX;

	assert(bs->bs_var == NULL);
	assert(ba->ba_type == B_AT_VAR);

	/* Parse optional `top' argument. */
	btop = SLIST_NEXT(ba, ba_next);
	if (btop != NULL) {
		assert(SLIST_NEXT(btop, ba_next) == NULL);
		top = ba2long(btop, dtev);
	}
	debug("map=%p '%s' print (top=%d)\n", bv->bv_value, bv_name(bv), top);

	map_print((struct map *)bv->bv_value, top, bv_name(bv));
}

/*
 * Variable store: 	{ var = 3; }
 *
 * In this case '3' is represented by `ba', the argument of a STORE
 * action.
 *
 * If the argument depends of the value of an event (builtin) or is
 * the result of an operation, its evaluation is stored in a new `ba'.
 */
void
stmt_store(struct bt_stmt *bs, struct dt_evt *dtev)
{
	struct bt_arg *ba = SLIST_FIRST(&bs->bs_args);
	struct bt_var *bv = bs->bs_var;

	assert(SLIST_NEXT(ba, ba_next) == NULL);

	switch (ba->ba_type) {
	case B_AT_LONG:
		bv->bv_value = ba;
		break;
	case B_AT_BI_NSECS:
		bv->bv_value = ba_new(builtin_nsecs(dtev), B_AT_LONG);
		break;
	case B_AT_OP_ADD ... B_AT_OP_DIVIDE:
		bv->bv_value = ba_new(ba2long(ba, dtev), B_AT_LONG);
		break;
	default:
		xabort("store not implemented for type %d", ba->ba_type);
	}

	debug("bv=%p var '%s' store (%p) \n", bv, bv_name(bv), bv->bv_value);
}

/*
 * Print time: 		{ time("%H:%M:%S"); }
 */
void
stmt_time(struct bt_stmt *bs, struct dt_evt *dtev)
{
	struct bt_arg *ba = SLIST_FIRST(&bs->bs_args);
	time_t time;
	struct tm *tm;
	char buf[64];

	assert(bs->bs_var == NULL);
	assert(ba->ba_type == B_AT_STR);
	assert(strlen(ba2str(ba, dtev)) < (sizeof(buf) - 1));

	time = builtin_gettime(dtev);
	tm = localtime(&time);
	strftime(buf, sizeof(buf), ba2str(ba, dtev), tm);
	printf("%s", buf);
}

/*
 * Set entries to 0:	{ zero(@map); }
 */
void
stmt_zero(struct bt_stmt *bs)
{
	struct bt_arg *ba = SLIST_FIRST(&bs->bs_args);
	struct bt_var *bv = ba->ba_value;

	assert(bs->bs_var == NULL);
	assert(ba->ba_type == B_AT_VAR);

	map_zero((struct map *)bv->bv_value);

	debug("map=%p '%s' zero\n", bv->bv_value, bv_name(bv));
}

struct bt_arg *
ba_read(struct bt_arg *ba)
{
	struct bt_var *bv = ba->ba_value;

	assert(ba->ba_type == B_AT_VAR);

	debug("bv=%p read '%s' (%p)\n", bv, bv_name(bv), bv->bv_value);

	return bv->bv_value;
}

const char *
ba2hash(struct bt_arg *ba, struct dt_evt *dtev)
{
	static char buf[256];
	char *hash;
	int l, len;

	l = snprintf(buf, sizeof(buf), "%s", ba2str(ba, dtev));
	if (l < 0 || (size_t)l > sizeof(buf)) {
		warn("string too long %d > %lu", l, sizeof(buf));
		return buf;
	}

	len = 0;
	while ((ba = SLIST_NEXT(ba, ba_next)) != NULL) {
		len += l;
		hash = buf + len;

		l = snprintf(hash, sizeof(buf) - len, ", %s", ba2str(ba, dtev));
		if (l < 0 || (size_t)l > (sizeof(buf) - len)) {
			warn("hash too long %d > %lu", l + len, sizeof(buf));
			break;
		}
	}

	return buf;
}

/*
 * Helper to evaluate the operation encoded in `ba' and return its
 * result.
 */
static inline long
baexpr2long(struct bt_arg *ba, struct dt_evt *dtev)
{
	static long recursions;
	struct bt_arg *a, *b;
	long first, second, result;

	if (++recursions >= __MAXOPERANDS)
		errx(1, "too many operands (>%d) in expression", __MAXOPERANDS);

	a = ba->ba_value;
	b = SLIST_NEXT(a, ba_next);

	assert(SLIST_NEXT(b, ba_next) == NULL);

	first = ba2long(a, dtev);
	second = ba2long(b, dtev);

	switch (ba->ba_type) {
	case B_AT_OP_ADD:
		result = first + second;
		break;
	case B_AT_OP_MINUS:
		result = first - second;
		break;
	case B_AT_OP_MULT:
		result = first * second;
		break;
	case B_AT_OP_DIVIDE:
		result = first / second;
		break;
	default:
		xabort("unsuported operation %d", ba->ba_type);
	}

	debug("ba=%p (%ld op %ld) = %ld\n", ba, first, second, result);

	--recursions;

	return result;
}

/*
 * Return the representation of `ba' as long.
 */
long
ba2long(struct bt_arg *ba, struct dt_evt *dtev)
{
	long val;

	switch (ba->ba_type) {
	case B_AT_LONG:
		val = (long)ba->ba_value;
		break;
	case B_AT_VAR:
		ba = ba_read(ba);
		val = (long)ba->ba_value;
		break;
	case B_AT_BI_NSECS:
		val = builtin_nsecs(dtev);
		break;
	case B_AT_OP_ADD ... B_AT_OP_DIVIDE:
		val = baexpr2long(ba, dtev);
		break;
	default:
		xabort("no long conversion for type %d", ba->ba_type);
	}

	return  val;
}

/*
 * Return the representation of `ba' as string.
 */
const char *
ba2str(struct bt_arg *ba, struct dt_evt *dtev)
{
	static char buf[sizeof("18446744073709551615")]; /* UINT64_MAX */
	struct bt_var *bv;
	const char *str;

	switch (ba->ba_type) {
	case B_AT_STR:
		str = (const char *)ba->ba_value;
		break;
	case B_AT_LONG:
		snprintf(buf, sizeof(buf) - 1, "%ld",(long)ba->ba_value);
		str = buf;
		break;
	case B_AT_BI_KSTACK:
		str = builtin_stack(dtev, 1);
		break;
	case B_AT_BI_USTACK:
		str = builtin_stack(dtev, 0);
		break;
	case B_AT_BI_COMM:
		str = dtev->dtev_comm;
		break;
	case B_AT_BI_CPU:
		snprintf(buf, sizeof(buf) - 1, "%u", dtev->dtev_cpu);
		str = buf;
		break;
	case B_AT_BI_PID:
		snprintf(buf, sizeof(buf) - 1, "%d", dtev->dtev_pid);
		str = buf;
		break;
	case B_AT_BI_TID:
		snprintf(buf, sizeof(buf) - 1, "%d", dtev->dtev_tid);
		str = buf;
		break;
	case B_AT_BI_NSECS:
		snprintf(buf, sizeof(buf) - 1, "%llu", builtin_nsecs(dtev));
		str = buf;
		break;
	case B_AT_BI_ARG0 ... B_AT_BI_ARG9:
		str = builtin_arg(dtev, ba->ba_type);
		break;
	case B_AT_BI_RETVAL:
		snprintf(buf, sizeof(buf) - 1, "%ld", (long)dtev->dtev_sysretval);
		str = buf;
		break;
	case B_AT_MAP:
		bv = ba->ba_value;
		str = ba2str(map_get((struct map *)bv->bv_value,
		    ba2str(ba->ba_key, dtev)), dtev);
		break;
	case B_AT_VAR:
		str = ba2str(ba_read(ba), dtev);
		break;
	case B_AT_OP_ADD ... B_AT_OP_DIVIDE:
		snprintf(buf, sizeof(buf) - 1, "%ld", ba2long(ba, dtev));
		str = buf;
		break;
	case B_AT_MF_COUNT:
	case B_AT_MF_MAX:
	case B_AT_MF_MIN:
	case B_AT_MF_SUM:
		assert(0);
		break;
	default:
		xabort("no string conversion for type %d", ba->ba_type);
	}

	return str;
}

/*
 * Return dt(4) flags indicating which data should be recorded by the
 * kernel, if any, for a given `ba'.
 */
int
ba2dtflags(struct bt_arg *ba)
{
	int flags = 0;

	if (ba->ba_type == B_AT_MAP)
		ba = ba->ba_key;

	do {
		switch (ba->ba_type) {
		case B_AT_STR:
		case B_AT_LONG:
		case B_AT_VAR:
			break;
		case B_AT_BI_KSTACK:
			flags |= DTEVT_KSTACK;
			break;
		case B_AT_BI_USTACK:
			flags |= DTEVT_USTACK;
			break;
		case B_AT_BI_COMM:
			flags |= DTEVT_EXECNAME;
			break;
		case B_AT_BI_CPU:
		case B_AT_BI_PID:
		case B_AT_BI_TID:
		case B_AT_BI_NSECS:
			break;
		case B_AT_BI_ARG0 ... B_AT_BI_ARG9:
			flags |= DTEVT_FUNCARGS;
			break;
		case B_AT_BI_RETVAL:
			flags |= DTEVT_RETVAL;
			break;
		case B_AT_MF_COUNT:
		case B_AT_MF_MAX:
		case B_AT_MF_MIN:
		case B_AT_MF_SUM:
		case B_AT_OP_ADD ... B_AT_OP_DIVIDE:
			break;
		default:
			xabort("invalid argument type %d", ba->ba_type);
		}
	} while ((ba = SLIST_NEXT(ba, ba_next)) != NULL);

	return flags;
}

long
bacmp(struct bt_arg *a, struct bt_arg *b)
{
	assert(a->ba_type == b->ba_type);
	assert(a->ba_type == B_AT_LONG);

	return ba2long(a, NULL) - ba2long(b, NULL);
}

__dead void
xabort(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fprintf(stderr, "\n");
	abort();
}

void
debug(const char *fmt, ...)
{
	va_list ap;

	if (verbose < 2)
		return;

	fprintf(stderr, "debug: ");

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

void
debugx(const char *fmt, ...)
{
	va_list ap;

	if (verbose < 2)
		return;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

static inline const char *
debug_getfiltervar(struct bt_filter *df)
{
	switch (df->bf_var) {
	case B_FV_PID:	return "pid";
	case B_FV_TID:	return "tid";
	case B_FV_NONE:	return "";
	default:
		xabort("invalid filtervar %d", df->bf_var);
	}


}

static inline const char *
debug_getfilterop(struct bt_filter *df)
{
	switch (df->bf_op) {
	case B_OP_EQ:	return "==";
	case B_OP_NE:	return "!=";
	case B_OP_NONE:	return "";
	default:
		xabort("invalid operand %d", df->bf_op);
	}
}

void
debug_dump_filter(struct bt_rule *r)
{
	if (r->br_filter) {
		debugx(" / %s %s %u /", debug_getfiltervar(r->br_filter),
		    debug_getfilterop(r->br_filter), r->br_filter->bf_val);
	}
	debugx("\n");
}

const char *
debug_rule_name(struct bt_rule *r)
{
	struct bt_probe *bp = r->br_probe;
	static char buf[64];

	if (r->br_type == B_RT_BEGIN)
		return "BEGIN";

	if (r->br_type == B_RT_END)
		return "END";

	assert(r->br_type == B_RT_PROBE);

	if (r->br_probe->bp_rate) {
		snprintf(buf, sizeof(buf) - 1, "%s:%s:%u", bp->bp_prov,
		    bp->bp_unit, bp->bp_rate);
	} else {
		snprintf(buf, sizeof(buf) - 1, "%s:%s:%s", bp->bp_prov,
		    bp->bp_unit, bp->bp_name);
	}

	return buf;
}

void
debug_dump_rule(struct bt_rule *r)
{
	debug("parsed probe '%s'", debug_rule_name(r));
	debug_dump_filter(r);
}
