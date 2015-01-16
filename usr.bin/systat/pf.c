/*	$OpenBSD: pf.c,v 1.7 2015/01/16 00:03:37 deraadt Exp $ */
/*
 * Copyright (c) 2001, 2007 Can Erkin Acar <canacar@openbsd.org>
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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <net/pfvar.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <err.h>
#include <unistd.h>
#include <syslog.h>
#include "pfctl_parser.h"
#include "systat.h"

void print_pf(void);
int read_pf(void);
int select_pf(void);
void print_fld_double(field_def *, double);

const char	*pf_reasons[PFRES_MAX+1] = PFRES_NAMES;
const char	*pf_lcounters[LCNT_MAX+1] = LCNT_NAMES;
const char	*pf_fcounters[FCNT_MAX+1] = FCNT_NAMES;
const char	*pf_scounters[FCNT_MAX+1] = FCNT_NAMES;

static struct pf_status status;
extern int pf_dev;
int num_pf = 0;

field_def fields_pf[] = {
	{"TYPE", 13, 16, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"NAME", 12, 24, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0},
	{"VALUE", 8, 10, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"RATE", 8, 10, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 60},
	{"NOTES", 10, 20, 1, FLD_ALIGN_LEFT, -1, 0, 0, 60},
};

#define FLD_PF_TYPE	FIELD_ADDR(fields_pf,0)
#define FLD_PF_NAME	FIELD_ADDR(fields_pf,1)
#define FLD_PF_VALUE	FIELD_ADDR(fields_pf,2)
#define FLD_PF_RATE	FIELD_ADDR(fields_pf,3)
#define FLD_PF_DESC	FIELD_ADDR(fields_pf,4)

/* Define views */
field_def *view_pf_0[] = {
	FLD_PF_TYPE, FLD_PF_NAME, FLD_PF_VALUE, FLD_PF_RATE, FLD_PF_DESC, NULL
};


/* Define view managers */
struct view_manager pf_mgr = {
	"PF", select_pf, read_pf, NULL, print_header,
	print_pf, keyboard_callback, NULL, NULL
};

field_view views_pf[] = {
	{view_pf_0, "pf", 'P', &pf_mgr},
	{NULL, NULL, 0, NULL}
};



int
select_pf(void)
{
	return (0);
}

int
read_pf(void)
{
	if (pf_dev < 0) {
		num_disp = 0;
		return 0;
	}

	if (ioctl(pf_dev, DIOCGETSTATUS, &status)) {
		error("DIOCGETSTATUS: %s", strerror(errno));
		return (-1);
	}

	num_disp = 4;

	if (status.ifname[0] != 0)
		num_disp += 13;

	num_disp += FCNT_MAX + 2;
	num_disp += SCNT_MAX + 2;
	num_disp += PFRES_MAX + 1;
	num_disp += LCNT_MAX + 1;

	return (0);
}

int
initpf(void)
{
	field_view *v;

	for (v = views_pf; v->name != NULL; v++)
		add_view(v);

	return(1);
}

void
print_fld_double(field_def *fld, double val)
{
	int len;

	if (fld == NULL)
		return;

	len = fld->width;
	if (len < 1)
		return;

	tb_start();
	if (tbprintf("%.2f", val) > len)
		print_fld_str(fld, "*");
	else
		print_fld_tb(fld);
	tb_end();
}

#define ADD_LINE_A(t, n, v) \
	do {							\
		if (cur >= dispstart && cur < end) { 		\
			print_fld_str(FLD_PF_TYPE, (t));	\
			print_fld_str(FLD_PF_NAME, (n));	\
			print_fld_age(FLD_PF_VALUE, (v));	\
			end_line();				\
		}						\
		if (++cur >= end)				\
			return;					\
	} while (0)

#define ADD_EMPTY_LINE \
	do {							\
		if (cur >= dispstart && cur < end) 		\
			end_line();				\
		if (++cur >= end)				\
			return;					\
	} while (0)

#define ADD_LINE_S(t, n, v) \
	do {							\
		if (cur >= dispstart && cur < end) { 		\
			print_fld_str(FLD_PF_TYPE, (t));	\
			print_fld_str(FLD_PF_NAME, (n));	\
			print_fld_str(FLD_PF_VALUE, (v));	\
			end_line();				\
		}						\
		if (++cur >= end)				\
			return;					\
	} while (0)

#define ADD_LINE_V(t, n, v) \
	do {							\
		if (cur >= dispstart && cur < end) { 		\
			print_fld_str(FLD_PF_TYPE, (t));	\
			print_fld_str(FLD_PF_NAME, (n));	\
			print_fld_size(FLD_PF_VALUE, (v));	\
			end_line();				\
		}						\
		if (++cur >= end)				\
			return;					\
	} while (0)

#define ADD_LINE_VD(t, n, v, d) \
	do {							\
		if (cur >= dispstart && cur < end) { 		\
			print_fld_str(FLD_PF_TYPE, (t));	\
			print_fld_str(FLD_PF_NAME, (n));	\
			print_fld_size(FLD_PF_VALUE, (v));	\
			print_fld_str(FLD_PF_DESC, (d));	\
			end_line();				\
		}						\
		if (++cur >= end)				\
			return;					\
	} while (0)

#define ADD_LINE_VR(t, n, v, r) \
	do {							\
		if (cur >= dispstart && cur < end) { 		\
			print_fld_str(FLD_PF_TYPE, (t));	\
			print_fld_str(FLD_PF_NAME, (n));	\
			print_fld_size(FLD_PF_VALUE, (v));	\
			print_fld_double(FLD_PF_RATE, (r));	\
			end_line();				\
		}						\
		if (++cur >= end)				\
			return;					\
	} while (0)


void
print_pf(void)
{
	char		*debug;
	time_t		tm;
	int		i;
	struct pf_status *s = &status;

	int cur = 0;
	int end = dispstart + maxprint;
	if (end > num_disp)
		end = num_disp;

	tm = time(NULL) - s->since;

	ADD_LINE_S("pf", "Status", s->running ? "Enabled" : "Disabled");
	ADD_LINE_A("pf", "Since", tm);

	switch (s->debug) {
	case LOG_EMERG:
		debug = "emerg";
		break;
	case LOG_ALERT:
		debug = "alert";
		break;
	case LOG_CRIT:
		debug = "crit";
		break;
	case LOG_ERR:
		debug = "err";
		break;
	case LOG_WARNING:
		debug = "warning";
		break;
	case LOG_NOTICE:
		debug = "notice";
		break;
	case LOG_INFO:
		debug = "info";
		break;
	case LOG_DEBUG:
		debug = "debug";
		break;
	}
	ADD_LINE_S("pf", "Debug", debug);

	tb_start();
	tbprintf("0x%08x\n", ntohl(s->hostid));
	tb_end();

	ADD_LINE_S("pf", "Hostid", tmp_buf);

	if (s->ifname[0] != 0) {
		ADD_EMPTY_LINE;
		ADD_LINE_VD(s->ifname, "Bytes In", s->bcounters[0][0], "IPv4");
		ADD_LINE_VD(s->ifname, "Bytes In", s->bcounters[1][0], "IPv6");
		ADD_LINE_VD(s->ifname, "Bytes Out", s->bcounters[0][1], "IPv4");
		ADD_LINE_VD(s->ifname, "Bytes Out", s->bcounters[1][1], "IPv6");
		ADD_LINE_VD(s->ifname, "Packets In", s->pcounters[0][0][PF_PASS], "IPv4, Passed");
		ADD_LINE_VD(s->ifname, "Packets In", s->pcounters[1][0][PF_PASS], "IPv6, Passed");
		ADD_LINE_VD(s->ifname, "Packets In", s->pcounters[0][0][PF_DROP], "IPv4, Blocked");
		ADD_LINE_VD(s->ifname, "Packets In", s->pcounters[1][0][PF_DROP], "IPv6, Blocked");
		ADD_LINE_VD(s->ifname, "Packets Out", s->pcounters[0][1][PF_PASS], "IPv4, Passed");
		ADD_LINE_VD(s->ifname, "Packets Out", s->pcounters[1][1][PF_PASS], "IPv6, Passed");
		ADD_LINE_VD(s->ifname, "Packets Out", s->pcounters[0][1][PF_DROP], "IPv4, Blocked");
		ADD_LINE_VD(s->ifname, "Packets Out", s->pcounters[1][1][PF_DROP], "IPv6, Blocked");
	}


	ADD_EMPTY_LINE;
	ADD_LINE_V("state", "Count", s->states);

	for (i = 0; i < FCNT_MAX; i++) {
		if (tm > 0)
			ADD_LINE_VR("state", pf_fcounters[i], s->fcounters[i],
				    (double)s->fcounters[i] / (double)tm);
		else
			ADD_LINE_V("state", pf_fcounters[i], s->fcounters[i]);
	}


	ADD_EMPTY_LINE;
	ADD_LINE_V("src track", "Count", s->src_nodes);

	for (i = 0; i < SCNT_MAX; i++) {
		if (tm > 0)
			ADD_LINE_VR("src track", pf_scounters[i], s->scounters[i],
				    (double)s->scounters[i] / (double)tm);
		else
			ADD_LINE_V("src track", pf_scounters[i], s->scounters[i]);
	}

	ADD_EMPTY_LINE;
	for (i = 0; i < PFRES_MAX; i++) {
		if (tm > 0)
			ADD_LINE_VR("counter", pf_reasons[i], s->counters[i],
				    (double)s->counters[i] / (double)tm);
		else
			ADD_LINE_V("counter", pf_reasons[i], s->counters[i]);
	}

	ADD_EMPTY_LINE;
	for (i = 0; i < LCNT_MAX; i++) {
		if (tm > 0)
			ADD_LINE_VR("limit counter", pf_lcounters[i], s->lcounters[i],
				    (double)s->lcounters[i] / (double)tm);
		else
			ADD_LINE_V("limit counter", pf_lcounters[i], s->lcounters[i]);
	}
}
