/* $OpenBSD: kstat.c,v 1.11 2022/07/10 19:51:37 kn Exp $ */

/*
 * Copyright (c) 2020 David Gwynne <dlg@openbsd.org>
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <inttypes.h>
#include <fnmatch.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <vis.h>

#include <sys/tree.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/queue.h>

#include <sys/kstat.h>

#ifndef roundup
#define roundup(x, y)		((((x)+((y)-1))/(y))*(y))
#endif

#ifndef nitems
#define nitems(_a)		(sizeof((_a)) / sizeof((_a)[0]))
#endif

#ifndef ISSET
#define ISSET(_i, _m)		((_i) & (_m))
#endif

#ifndef SET
#define SET(_i, _m)		((_i) |= (_m))
#endif

#define str_is_empty(_str)	(*(_str) == '\0')

#define DEV_KSTAT "/dev/kstat"

struct kstat_filter {
	TAILQ_ENTRY(kstat_filter)	 kf_entry;
	const char			*kf_provider;
	const char			*kf_name;
	unsigned int			 kf_flags;
#define KSTAT_FILTER_F_INST			(1 << 0)
#define KSTAT_FILTER_F_UNIT			(1 << 1)
	unsigned int			 kf_instance;
	unsigned int			 kf_unit;
};

TAILQ_HEAD(kstat_filters, kstat_filter);

struct kstat_entry {
	struct kstat_req	kstat;
	RBT_ENTRY(kstat_entry)	entry;
	int			serrno;
};

RBT_HEAD(kstat_tree, kstat_entry);

static inline int
kstat_cmp(const struct kstat_entry *ea, const struct kstat_entry *eb)
{
	const struct kstat_req *a = &ea->kstat;
	const struct kstat_req *b = &eb->kstat;
	int rv;

	rv = strncmp(a->ks_provider, b->ks_provider, sizeof(a->ks_provider));
	if (rv != 0)
		return (rv);
	if (a->ks_instance > b->ks_instance)
		return (1);
	if (a->ks_instance < b->ks_instance)
		return (-1);

	rv = strncmp(a->ks_name, b->ks_name, sizeof(a->ks_name));
	if (rv != 0)
		return (rv);
	if (a->ks_unit > b->ks_unit)
		return (1);
	if (a->ks_unit < b->ks_unit)
		return (-1);

	return (0);
}

RBT_PROTOTYPE(kstat_tree, kstat_entry, entry, kstat_cmp);
RBT_GENERATE(kstat_tree, kstat_entry, entry, kstat_cmp);

static void handle_alrm(int);
static struct kstat_filter *
		kstat_filter_parse(char *);
static int	kstat_filter_entry(struct kstat_filters *,
		    const struct kstat_req *);

static void	kstat_list(struct kstat_tree *, int, unsigned int,
		    struct kstat_filters *);
static void	kstat_print(struct kstat_tree *);
static void	kstat_read(struct kstat_tree *, int);

__dead static void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-w wait] "
	    "[name | provider:instance:name:unit] ...\n", __progname);

	exit(1);
}

int
main(int argc, char *argv[])
{
	struct kstat_filters kfs = TAILQ_HEAD_INITIALIZER(kfs);
	struct kstat_tree kt = RBT_INITIALIZER();
	unsigned int version;
	int fd;
	const char *errstr;
	int ch;
	struct itimerval itv;
	sigset_t empty, mask;
	int i;
	unsigned int wait = 0;

	while ((ch = getopt(argc, argv, "w:")) != -1) {
		switch (ch) {
		case 'w':
			wait = strtonum(optarg, 1, UINT_MAX, &errstr);
			if (errstr != NULL)
				errx(1, "wait is %s: %s", errstr, optarg);
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	for (i = 0; i < argc; i++) {
		struct kstat_filter *kf = kstat_filter_parse(argv[i]);
		TAILQ_INSERT_TAIL(&kfs, kf, kf_entry);
	}

	fd = open(DEV_KSTAT, O_RDONLY);
	if (fd == -1)
		err(1, "%s", DEV_KSTAT);

	if (ioctl(fd, KSTATIOC_VERSION, &version) == -1)
		err(1, "kstat version");

	kstat_list(&kt, fd, version, &kfs);
	kstat_print(&kt);

	if (wait == 0)
		return (0);

	if (signal(SIGALRM, handle_alrm) == SIG_ERR)
		err(1, "signal");
	sigemptyset(&empty);
	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
		err(1, "sigprocmask");

	itv.it_value.tv_sec = wait;
	itv.it_value.tv_usec = 0;
	itv.it_interval = itv.it_value;
	if (setitimer(ITIMER_REAL, &itv, NULL) == -1)
		err(1, "setitimer");

	for (;;) {
		sigsuspend(&empty);
		kstat_read(&kt, fd);
		kstat_print(&kt);
	}

	return (0);
}

static struct kstat_filter *
kstat_filter_parse(char *arg)
{
	struct kstat_filter *kf;
	const char *errstr;
	char *argv[4];
	size_t argc;

	for (argc = 0; argc < nitems(argv); argc++) {
		char *s = strsep(&arg, ":");
		if (s == NULL)
			break;

		argv[argc] = s;
	}
	if (arg != NULL)
		usage();

	kf = malloc(sizeof(*kf));
	if (kf == NULL)
		err(1, NULL);

	memset(kf, 0, sizeof(*kf));

	switch (argc) {
	case 1:
		if (str_is_empty(argv[0]))
			errx(1, "empty name");

		kf->kf_name = argv[0];
		break;
	case 4:
		if (!str_is_empty(argv[0]))
			kf->kf_provider = argv[0];
		if (!str_is_empty(argv[1])) {
			kf->kf_instance =
			    strtonum(argv[1], 0, 0xffffffffU, &errstr);
			if (errstr != NULL) {
				errx(1, "%s:%s:%s:%s: instance %s: %s",
				    argv[0], argv[1], argv[2], argv[3],
				    argv[1], errstr);
			}
			SET(kf->kf_flags, KSTAT_FILTER_F_INST);
		}
		if (!str_is_empty(argv[2]))
			kf->kf_name = argv[2];
		if (!str_is_empty(argv[3])) {
			kf->kf_unit =
			    strtonum(argv[3], 0, 0xffffffffU, &errstr);
			if (errstr != NULL) {
				errx(1, "%s:%s:%s:%s: unit %s: %s",
				    argv[0], argv[1], argv[2], argv[3],
				    argv[3], errstr);
			}
			SET(kf->kf_flags, KSTAT_FILTER_F_UNIT);
		}
		break;
	default:
		usage();
	}

	return (kf);
}

static int
kstat_filter_entry(struct kstat_filters *kfs, const struct kstat_req *ksreq)
{
	struct kstat_filter *kf;

	if (TAILQ_EMPTY(kfs))
		return (1);

	TAILQ_FOREACH(kf, kfs, kf_entry) {
		if (kf->kf_provider != NULL) {
			if (fnmatch(kf->kf_provider, ksreq->ks_provider,
			    FNM_NOESCAPE | FNM_LEADING_DIR) == FNM_NOMATCH)
				continue;
		}
		if (ISSET(kf->kf_flags, KSTAT_FILTER_F_INST)) {
			if (kf->kf_instance != ksreq->ks_instance)
				continue;
		}
		if (kf->kf_name != NULL) {
			if (fnmatch(kf->kf_name, ksreq->ks_name,
			    FNM_NOESCAPE | FNM_LEADING_DIR) == FNM_NOMATCH)
				continue;
		}
		if (ISSET(kf->kf_flags, KSTAT_FILTER_F_UNIT)) {
			if (kf->kf_unit != ksreq->ks_unit)
				continue;
		}

		return (1);
	}

	return (0);
}

static int
printable(int ch)
{
	if (ch == '\0')
		return ('_');
	if (!isprint(ch))
		return ('~');
	return (ch);
}

static void
hexdump(const void *d, size_t datalen)
{
	const uint8_t *data = d;
	size_t i, j = 0;

	for (i = 0; i < datalen; i += j) {
		printf("%4zu: ", i);

		for (j = 0; j < 16 && i+j < datalen; j++)
			printf("%02x ", data[i + j]);
		while (j++ < 16)
			printf("   ");
		printf("|");

		for (j = 0; j < 16 && i+j < datalen; j++)
			putchar(printable(data[i + j]));
		printf("|\n");
	}
}

static void
strdump(const void *s, size_t len)
{
	const char *str = s;
	char dst[8];
	size_t i;

	for (i = 0; i < len; i++) {
		char ch = str[i];
		if (ch == '\0')
			break;

		vis(dst, ch, VIS_TAB | VIS_NL, 0);
		printf("%s", dst);
	}
}

static void
strdumpnl(const void *s, size_t len)
{
	strdump(s, len);
	printf("\n");
}

static void
kstat_kv(const void *d, ssize_t len)
{
	const uint8_t *buf;
	const struct kstat_kv *kv;
	ssize_t blen;
	void (*trailer)(const void *, size_t);
	double f;

	if (len < (ssize_t)sizeof(*kv)) {
		warn("short kv (len %zu < size %zu)", len, sizeof(*kv));
		return;
	}

	buf = d;
	do {
		kv = (const struct kstat_kv *)buf;

		buf += sizeof(*kv);
		len -= sizeof(*kv);

		blen = 0;
		trailer = hexdump;

		printf("%16.16s: ", kv->kv_key);

		switch (kv->kv_type) {
		case KSTAT_KV_T_NULL:
			printf("null");
			break;
		case KSTAT_KV_T_BOOL:
			printf("%s", kstat_kv_bool(kv) ? "true" : "false");
			break;
		case KSTAT_KV_T_COUNTER64:
		case KSTAT_KV_T_UINT64:
			printf("%" PRIu64, kstat_kv_u64(kv));
			break;
		case KSTAT_KV_T_INT64:
			printf("%" PRId64, kstat_kv_s64(kv));
			break;
		case KSTAT_KV_T_COUNTER32:
		case KSTAT_KV_T_UINT32:
			printf("%" PRIu32, kstat_kv_u32(kv));
			break;
		case KSTAT_KV_T_INT32:
			printf("%" PRId32, kstat_kv_s32(kv));
			break;
		case KSTAT_KV_T_COUNTER16:
		case KSTAT_KV_T_UINT16:
			printf("%" PRIu16, kstat_kv_u16(kv));
			break;
		case KSTAT_KV_T_INT16:
			printf("%" PRId16, kstat_kv_s16(kv));
			break;
		case KSTAT_KV_T_STR:
			blen = kstat_kv_len(kv);
			trailer = strdumpnl;
			break;
		case KSTAT_KV_T_BYTES:
			blen = kstat_kv_len(kv);
			trailer = hexdump;

			printf("\n");
			break;

		case KSTAT_KV_T_ISTR:
			strdump(kstat_kv_istr(kv), sizeof(kstat_kv_istr(kv)));
			break;

		case KSTAT_KV_T_TEMP:
			f = kstat_kv_temp(kv);
			printf("%.2f degC", (f - 273150000.0) / 1000000.0);
			break;

		default:
			printf("unknown type %u, stopping\n", kv->kv_type);
			return;
		}

		switch (kv->kv_unit) {
		case KSTAT_KV_U_NONE:
			break;
		case KSTAT_KV_U_PACKETS:
			printf(" packets");
			break;
		case KSTAT_KV_U_BYTES:
			printf(" bytes");
			break;
		case KSTAT_KV_U_CYCLES:
			printf(" cycles");
			break;

		default:
			printf(" unit-type-%u", kv->kv_unit);
			break;
		}

		if (blen > 0) {
			if (blen > len) {
				blen = len;
			}

			(*trailer)(buf, blen);
		} else
			printf("\n");

		blen = roundup(blen, KSTAT_KV_ALIGN);
		buf += blen;
		len -= blen;
	} while (len >= (ssize_t)sizeof(*kv));
}

static void
kstat_list(struct kstat_tree *kt, int fd, unsigned int version,
    struct kstat_filters *kfs)
{
	struct kstat_entry *kse;
	struct kstat_req *ksreq;
	size_t len;
	uint64_t id = 0;

	for (;;) {
		kse = malloc(sizeof(*kse));
		if (kse == NULL)
			err(1, NULL);

		memset(kse, 0, sizeof(*kse));
		ksreq = &kse->kstat;
		ksreq->ks_version = version;
		ksreq->ks_id = ++id;

		ksreq->ks_datalen = len = 64; /* magic */
		ksreq->ks_data = malloc(len);
		if (ksreq->ks_data == NULL)
			err(1, "data alloc");

		if (ioctl(fd, KSTATIOC_NFIND_ID, ksreq) == -1) {
			if (errno == ENOENT) {
				free(ksreq->ks_data);
				free(kse);
				break;
			}

			kse->serrno = errno;
		} else
			id = ksreq->ks_id;

		if (!kstat_filter_entry(kfs, ksreq)) {
			free(ksreq->ks_data);
			free(kse);
			continue;
		}

		if (RBT_INSERT(kstat_tree, kt, kse) != NULL)
			errx(1, "duplicate kstat entry");

		if (kse->serrno != 0)
			continue;

		while (ksreq->ks_datalen > len) {
			len = ksreq->ks_datalen;
			ksreq->ks_data = realloc(ksreq->ks_data, len);
			if (ksreq->ks_data == NULL)
				err(1, "data resize (%zu)", len);

			if (ioctl(fd, KSTATIOC_FIND_ID, ksreq) == -1)
				err(1, "find id %llu", ksreq->ks_id);
		}
	}
}

static void
kstat_print(struct kstat_tree *kt)
{
	struct kstat_entry *kse;
	struct kstat_req *ksreq;

	RBT_FOREACH(kse, kstat_tree, kt) {
		ksreq = &kse->kstat;
		printf("%s:%u:%s:%u\n",
		    ksreq->ks_provider, ksreq->ks_instance,
		    ksreq->ks_name, ksreq->ks_unit);
		if (kse->serrno != 0) {
			printf("\t%s\n", strerror(kse->serrno));
			continue;
		}
		switch (ksreq->ks_type) {
		case KSTAT_T_RAW:
			hexdump(ksreq->ks_data, ksreq->ks_datalen);
			break;
		case KSTAT_T_KV:
			kstat_kv(ksreq->ks_data, ksreq->ks_datalen);
			break;
		default:
			hexdump(ksreq->ks_data, ksreq->ks_datalen);
			break;
		}
	}

	fflush(stdout);
}

static void
kstat_read(struct kstat_tree *kt, int fd)
{
	struct kstat_entry *kse;
	struct kstat_req *ksreq;

	RBT_FOREACH(kse, kstat_tree, kt) {
		ksreq = &kse->kstat;
		if (ioctl(fd, KSTATIOC_FIND_ID, ksreq) == -1)
			err(1, "update id %llu", ksreq->ks_id);
	}
}

static void
handle_alrm(int signo)
{
}
