/*	$OpenBSD: altqstat.h,v 1.1.1.1 2001/06/27 18:23:19 kjc Exp $	*/
/*	$KAME: altqstat.h,v 1.2 2000/10/18 09:15:16 kjc Exp $	*/
/*
 * Copyright (C) 1999-2000
 *	Sony Computer Science Laboratories, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY SONY CSL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL SONY CSL OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

typedef void (stat_loop_t)(int fd, const char *ifname,
			   int count, int interval);

struct qdisc_conf {
	const char	*qdisc_name;		/* e.g., cbq */
	int		altqtype;		/* e.g., ALTQT_CBQ */
	stat_loop_t	*stat_loop;
};

/*
 * cast u_int64_t to ull for printf, since type of u_int64_t
 * is architecture dependent
 */
typedef	unsigned long long	ull;

stat_loop_t cbq_stat_loop;
stat_loop_t hfsc_stat_loop;
stat_loop_t cdnr_stat_loop;
stat_loop_t wfq_stat_loop;
stat_loop_t fifoq_stat_loop;
stat_loop_t red_stat_loop;
stat_loop_t rio_stat_loop;
stat_loop_t blue_stat_loop;
stat_loop_t priq_stat_loop;

struct redstats;

void chandle2name(const char *ifname, u_long handle, char *name);
stat_loop_t *qdisc2stat_loop(const char *qdisc_name);
int ifname2qdisc(const char *ifname, char *qname);
double calc_interval(struct timeval *cur_time, struct timeval *last_time);
double calc_rate(u_int64_t new_bytes, u_int64_t last_bytes, double interval);
double calc_pps(u_int64_t new_pkts, u_int64_t last_pkts, double interval);
char *rate2str(double rate);
int print_redstats(struct redstats *rp);
int print_riostats(struct redstats *rp);



