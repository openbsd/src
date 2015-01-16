/*	$OpenBSD: cpu.c,v 1.4 2015/01/16 00:03:37 deraadt Exp $	*/

/*
 * Copyright (c) 2013 Reyk Floeter <reyk@openbsd.org>
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

/* CPU percentages() function from usr.bin/top/util.c:
 *
 *  Top users/processes display for Unix
 *  Version 3
 *
 * Copyright (c) 1984, 1989, William LeFebvre, Rice University
 * Copyright (c) 1989, 1990, 1992, William LeFebvre, Northwestern University
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS EMPLOYER BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/signal.h>
#include <sys/sched.h>
#include <sys/sysctl.h>

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "systat.h"

void		 print_cpu(void);
int		 read_cpu(void);
int		 select_cpu(void);
static void	 cpu_info(void);
static void	 print_fld_percentage(field_def *, double);
static int	 percentages(int, int64_t *, int64_t *, int64_t *, int64_t *);

field_def fields_cpu[] = {
	{ "CPU", 4, 8, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0 },
	{ "User", 10, 20, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0 },
	{ "Nice", 10, 20, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0 },
	{ "System", 10, 20, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0 },
	{ "Interrupt", 10, 20, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0 },
	{ "Idle", 10, 20, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0 },
};

#define FLD_CPU_CPU	FIELD_ADDR(fields_cpu, 0)
#define FLD_CPU_INT	FIELD_ADDR(fields_cpu, 1)
#define FLD_CPU_SYS	FIELD_ADDR(fields_cpu, 2)
#define FLD_CPU_USR	FIELD_ADDR(fields_cpu, 3)
#define FLD_CPU_NIC	FIELD_ADDR(fields_cpu, 4)
#define FLD_CPU_IDLE	FIELD_ADDR(fields_cpu, 5)

/* Define views */
field_def *view_cpu_0[] = {
	FLD_CPU_CPU,
	FLD_CPU_INT, FLD_CPU_SYS, FLD_CPU_USR, FLD_CPU_NIC, FLD_CPU_IDLE, NULL
};

/* Define view managers */
struct view_manager cpu_mgr = {
	"cpu", select_cpu, read_cpu, NULL, print_header,
	print_cpu, keyboard_callback, NULL, NULL
};

field_view views_cpu[] = {
	{ view_cpu_0, "cpu", 'C', &cpu_mgr },
	{ NULL, NULL, 0, NULL }
};

int	  cpu_count;
int64_t	 *cpu_states;
int64_t	**cpu_tm;
int64_t	**cpu_old;
int64_t	**cpu_diff;

/*
 * percentages(cnt, out, new, old, diffs) - calculate percentage change
 * between array "old" and "new", putting the percentages in "out".
 * "cnt" is size of each array and "diffs" is used for scratch space.
 * The array "old" is updated on each call.
 * The routine assumes modulo arithmetic.  This function is especially
 * useful on BSD machines for calculating cpu state percentages.
 */
static int
percentages(int cnt, int64_t *out, int64_t *new, int64_t *old, int64_t *diffs)
{
	int64_t change, total_change, *dp, half_total;
	int i;

	/* initialization */
	total_change = 0;
	dp = diffs;

	/* calculate changes for each state and the overall change */
	for (i = 0; i < cnt; i++) {
		if ((change = *new - *old) < 0) {
			/* this only happens when the counter wraps */
			change = INT64_MAX - *old + *new;
		}
		total_change += (*dp++ = change);
		*old++ = *new++;
	}

	/* avoid divide by zero potential */
	if (total_change == 0)
		total_change = 1;

	/* calculate percentages based on overall change, rounding up */
	half_total = total_change / 2l;
	for (i = 0; i < cnt; i++)
		*out++ = ((*diffs++ * 1000 + half_total) / total_change);

	/* return the total in case the caller wants to use it */
	return (total_change);
}

static void
cpu_info(void)
{
	int	 cpu_time_mib[] = { CTL_KERN, KERN_CPTIME2, 0 }, i;
	int64_t *tmpstate;
	size_t	 size;

	size = CPUSTATES * sizeof(int64_t);
	for (i = 0; i < cpu_count; i++) {
		cpu_time_mib[2] = i;
		tmpstate = cpu_states + (CPUSTATES * i);
		if (sysctl(cpu_time_mib, 3, cpu_tm[i], &size, NULL, 0) < 0)
			error("sysctl KERN_CPTIME2");
		percentages(CPUSTATES, tmpstate, cpu_tm[i],
		    cpu_old[i], cpu_diff[i]);
	}
}

static void
print_fld_percentage(field_def *fld, double val)
{
	if (fld == NULL)
		return;

	tb_start();
	tbprintf(val >= 1000 ? "%4.0f%%" : "%4.1f%%", val / 10.);
	print_fld_tb(fld);
	tb_end();
}

int
select_cpu(void)
{
	return (0);
}

int
read_cpu(void)
{
	cpu_info();
	num_disp = cpu_count;
	return (0);
}

int
initcpu(void)
{
	field_view	*v;
	size_t		 size = sizeof(cpu_count);
	int		 mib[2], i;

	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	if (sysctl(mib, 2, &cpu_count, &size, NULL, 0) == -1)
		return (-1);
	if ((cpu_states = calloc(cpu_count,
	    CPUSTATES * sizeof(int64_t))) == NULL)
		return (-1);
	if ((cpu_tm = calloc(cpu_count, sizeof(int64_t *))) == NULL ||
	    (cpu_old = calloc(cpu_count, sizeof(int64_t *))) == NULL ||
	    (cpu_diff = calloc(cpu_count, sizeof(int64_t *))) == NULL)
		return (-1);
	for (i = 0; i < cpu_count; i++) {
		if ((cpu_tm[i] = calloc(CPUSTATES, sizeof(int64_t))) == NULL ||
		    (cpu_old[i] = calloc(CPUSTATES, sizeof(int64_t))) == NULL ||
		    (cpu_diff[i] = calloc(CPUSTATES, sizeof(int64_t))) == NULL)
			return (-1);
	}

	for (v = views_cpu; v->name != NULL; v++)
		add_view(v);

	read_cpu();

	return(1);
}

#define ADD_EMPTY_LINE \
	do {								\
		if (cur >= dispstart && cur < end) 			\
			end_line();					\
		if (++cur >= end)					\
			return;						\
	} while (0)

#define ADD_LINE_CPU(v, cs) \
	do {								\
		if (cur >= dispstart && cur < end) { 			\
			print_fld_size(FLD_CPU_CPU, (v));		\
			print_fld_percentage(FLD_CPU_INT, (cs[0]));	\
			print_fld_percentage(FLD_CPU_SYS, (cs[1]));	\
			print_fld_percentage(FLD_CPU_USR, (cs[2]));	\
			print_fld_percentage(FLD_CPU_NIC, (cs[3]));	\
			print_fld_percentage(FLD_CPU_IDLE, (cs[4]));	\
			end_line();					\
		}							\
		if (++cur >= end)					\
			return;						\
	} while (0)

void
print_cpu(void)
{
	time_t		tm;
	int		cur = 0, c, i;
	int		end = dispstart + maxprint;
	int64_t		*states;
	double		value[CPUSTATES];
	tm = time(NULL);

	if (end > num_disp)
		end = num_disp;

	for (c = 0; c < cpu_count; c++) {
		states = cpu_states + (CPUSTATES * c);

		for (i = 0; i < CPUSTATES; i++)
			value[i] = *states++;

		ADD_LINE_CPU(c, value);
	}

	ADD_EMPTY_LINE;
}
