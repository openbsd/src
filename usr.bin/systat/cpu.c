/*	$OpenBSD: cpu.c,v 1.9 2018/11/17 23:10:08 cheloha Exp $	*/

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
	{ "Spin", 10, 20, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0 },
	{ "Interrupt", 10, 20, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0 },
	{ "Idle", 10, 20, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0 },
};

#define FLD_CPU_CPU	FIELD_ADDR(fields_cpu, 0)
#define FLD_CPU_USR	FIELD_ADDR(fields_cpu, 1)
#define FLD_CPU_NIC	FIELD_ADDR(fields_cpu, 2)
#define FLD_CPU_SYS	FIELD_ADDR(fields_cpu, 3)
#define FLD_CPU_SPIN	FIELD_ADDR(fields_cpu, 4)
#define FLD_CPU_INT	FIELD_ADDR(fields_cpu, 5)
#define FLD_CPU_IDLE	FIELD_ADDR(fields_cpu, 6)

/* Define views */
field_def *view_cpu_0[] = {
	FLD_CPU_CPU, FLD_CPU_USR, FLD_CPU_NIC, FLD_CPU_SYS, FLD_CPU_SPIN,
	FLD_CPU_INT, FLD_CPU_IDLE, NULL
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
struct cpustats *cpu_diff, *cpu_old, *cpu_tm;

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
	int	 cpustats_mib[] = { CTL_KERN, KERN_CPUSTATS, 0 }, i;
	int64_t *tmpstate;
	size_t	 size;

	size = sizeof(*cpu_tm);
	for (i = 0; i < cpu_count; i++) {
		cpustats_mib[2] = i;
		tmpstate = cpu_states + (CPUSTATES * i);
		if (sysctl(cpustats_mib, 3, &cpu_tm[i], &size, NULL, 0) < 0)
			error("sysctl KERN_CPUSTATS");
		percentages(CPUSTATES, tmpstate, cpu_tm[i].cs_time,
		    cpu_old[i].cs_time, cpu_diff[i].cs_time);
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
	if ((cpu_tm = calloc(cpu_count, sizeof(*cpu_tm))) == NULL ||
	    (cpu_old = calloc(cpu_count, sizeof(*cpu_old))) == NULL ||
	    (cpu_diff = calloc(cpu_count, sizeof(*cpu_diff))) == NULL)
		return (-1);

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

#define ADD_LINE_CPU(v, _cs) do {					\
	if (cur >= dispstart && cur < end) { 				\
		print_fld_size(FLD_CPU_CPU, (v));			\
		if (cpu_tm[v].cs_flags & CPUSTATS_ONLINE) {		\
			print_fld_percentage(FLD_CPU_USR, _cs[CP_USER]);\
			print_fld_percentage(FLD_CPU_NIC, _cs[CP_NICE]);\
			print_fld_percentage(FLD_CPU_SYS, _cs[CP_SYS]);	\
			print_fld_percentage(FLD_CPU_SPIN, _cs[CP_SPIN]);\
			print_fld_percentage(FLD_CPU_INT, _cs[CP_INTR]);\
			print_fld_percentage(FLD_CPU_IDLE, _cs[CP_IDLE]);\
		} else {						\
			print_fld_str(FLD_CPU_USR, "-");		\
			print_fld_str(FLD_CPU_NIC, "-");		\
			print_fld_str(FLD_CPU_SYS, "-");		\
			print_fld_str(FLD_CPU_SPIN, "-");		\
			print_fld_str(FLD_CPU_INT, "-");		\
			print_fld_str(FLD_CPU_IDLE, "-");		\
		}							\
		end_line();						\
	}								\
	if (++cur >= end)						\
		return;							\
} while (0)

void
print_cpu(void)
{
	int		cur = 0, c, i;
	int		end = dispstart + maxprint;
	int64_t		*states;
	double		value[CPUSTATES];

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
