/*
 * Copyright (c) 1995 - 2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "fs_local.h"

RCSID("$arla: fs_statistics.c,v 1.7 2002/05/16 22:52:59 lha Exp $");

static void
printtimeslot(uint32_t slot)
{
    int v;

    v = 1 << slot;

    if (v < 1000) {
	printf("%3d us ", v);
	return;
    }
    if (v < 1000000) {
	printf("%3d ms ", v / 1000);
	return;
    }
    printf("%3d s  ", v / 1000000);
}

static void
printsizeslot(uint32_t slot)
{
    int v;

    v = 1 << slot;

    if (v < 1024) {
	printf("%3d   ", v);
	return;
    }
    if (v < 1000000) {
	printf("%3d K ", v / 1000);
	return;
    }
    printf("%3d M ", v / 1000000);
}

static void
print_stat_header(char *server, char *part,
		  int type, int time, int size, int *once)
{
    if (*once)
	return;
    *once = 1;
    printf("%30s %10s\n", server, part);

    if (time)
	printf("%6s ", "tmrng");

    if (size)
	printf("%5s ", "szrng");
    
    printf("%6s ", "#req");

    printf("%8s ", "tot KB");
    printf("%8s ", "tot us");
    printf("%8s ", "avg us");
    printf("%8s\n", "KB/s");
}

static void
print_statistics(int64_t count, int64_t items_total, int64_t total_time,
		 int timeslot, int sizeslot, int time, int size)
{
    if (time)
	printtimeslot(timeslot);
    if (size)
	printsizeslot(sizeslot);

    printf("%6lld %8lld %8lld %8lld %8lld\n",
	   count, items_total,
	   total_time,
	   total_time/count,
	   items_total*1000LL/total_time);
}

struct stat_type {
    char *name;
    int value;
} stat_types[] = {
    { "fetchdata",	STATISTICS_REQTYPE_FETCHDATA },
    { "storedata",	STATISTICS_REQTYPE_STOREDATA },
    { "fetchstatus",	STATISTICS_REQTYPE_FETCHSTATUS },
    { "storestatus",	STATISTICS_REQTYPE_STORESTATUS },
    { "bulkstatus",	STATISTICS_REQTYPE_BULKSTATUS },
    { NULL, 0 }
};

static int
getstatistics_help (struct agetargs *args, int argc, char **argv)
{
    struct stat_type *t;

    aarg_printusage(args, "getstatistics", NULL, AARG_AFSSTYLE);
    fprintf(stderr, "type can be either of: \n");
    for (t = stat_types; t->name != NULL; t++)
	fprintf(stderr, "  %s\n", t->name);
    return 0;
}

int
getstatistics_cmd (int argc, char **argv)
{
    uint32_t host[100];
    uint32_t part[100];
    int n = 100;
    int i;
    int j;
    int k;
    char server_name[100];
    char partition_name[100];
    uint32_t count[32];
    int64_t items_total[32];
    int64_t total_time[32];
    int64_t tot_total_time;
    int64_t tot_count;
    int64_t tot_items_total;
    int error;
    int type;
    char *reqtype = NULL;
    char *filter_host = NULL;
    int time = 0, size = 0, help = 0;
    int optind = 0;
    int printed_header = 0;
    struct stat_type *t, *t_best;

    struct agetargs statargs[] = {
	{"type", 0, aarg_string,  
	 NULL, "request type", "type", aarg_mandatory},
	{"host", 0, aarg_string,  
	 NULL, "host", "hostname", aarg_optional},
	{"time", 0, aarg_flag,
	 NULL, "time statistics", NULL},
	{"size", 0, aarg_flag,
	 NULL, "size statistics", NULL},
	{"help", 0, aarg_flag,
	 NULL, "get help", NULL},
        {NULL,      0, aarg_end, NULL}},
	*arg;

    arg = statargs;
    arg->value = &reqtype; arg++;
    arg->value = &filter_host; arg++;
    arg->value = &time; arg++;
    arg->value = &size; arg++;
    arg->value = &help; arg++;


    if (agetarg (statargs, argc, argv, &optind, AARG_AFSSTYLE))
	return getstatistics_help(statargs, argc, argv);

    if (help)
	return getstatistics_help(statargs, argc, argv);

    i = strlen(reqtype);
    t_best = NULL;
    for (t = stat_types; t->name != NULL; t++) {
	if (strncmp(reqtype, t->name, i) == 0) {
	    if (i == strlen(t->name))
		break;
	    if (t_best != NULL) {
		warnx("ambitious type argument");
		return 0;
	    }
	    t_best = t;
	}
    }
    if (t->name == NULL && t_best != NULL)
	t = t_best;
    if (t->name == NULL) {
	warnx("no type argument");
	return getstatistics_help(statargs, argc, argv);
    }
    
    type = t->value;

    error = fs_statistics_list(host, part, &n);
    if (error) {
	fserr(PROGNAME, error, NULL);
	return 0;
    }
    
    for (i = 0; i < n; i++) {
	printed_header = 0;
	arlalib_host_to_name(host[i], server_name, 100);
	if (filter_host && strcmp(filter_host, server_name) != 0)
	    continue;
	partition_num2name(part[i], partition_name, 100);

	tot_total_time = 0;
	tot_count = 0;
	tot_items_total = 0;
	for (j = 0; j < 32; j++) {
	    error = fs_statistics_entry(host[i], part[i], type,
					j, count, items_total, total_time);
	    if (error) {
		fserr(PROGNAME, error, NULL);
		return 0;
	    }
	    if (size) {
		tot_total_time = 0;
		tot_count = 0;
		tot_items_total = 0;
	    }
	    for (k = 0; k < 32; k++) {
		if (count[k]) {
		    if (time && size) {
			print_stat_header(server_name, partition_name,
					  type, time, size, &printed_header);
			print_statistics(count[k], items_total[k],
					 total_time[k], k, j,
					 time, size);
		    }
		    tot_total_time += total_time[k];
		    tot_count += count[k];
		    tot_items_total += items_total[k];
		}
	    }
	    if (!time && size && tot_count) {
		print_stat_header(server_name, partition_name,
				  type, time, size, &printed_header);
		print_statistics(tot_count, tot_items_total,
				 tot_total_time, 0, j,
				 time, size);
	    }
	}
	if (!time && !size && tot_count) {
	    print_stat_header(server_name, partition_name,
			      type, time, size, &printed_header);
	    print_statistics(tot_count, tot_items_total,
			     tot_total_time, 0, 0,
			     time, size);
	}
    }
    return 0;
}
