/*	$OpenBSD: pflogd.c,v 1.4 2001/08/23 04:07:33 deraadt Exp $	*/

/*
 * Copyright (c) 2001 Theo de Raadt
 * Copyright (c) 2001 Can Erkin Acar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pcap-int.h>
#include <pcap.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>

#define DEF_SNAPLEN 96		/* default plus allow for larger header of pflog */
#define PCAP_TO_MS 500		/* pcap read timeout (ms) */
#define PCAP_NUM_PKTS 1000	/* max number of packets to process at each loop */
#define PCAP_OPT_FIL 0		/* filter optimization */
#define FLUSH_DELAY 60		/* flush delay */

#define PFLOGD_PID_FILE		"/var/run/pflogd.pid"
#define PFLOGD_LOG_FILE		"/var/log/pflog"
#define PFLOGD_DEFAULT_IF	"pflog0"

pcap_t *hpcap;
pcap_dumper_t *dpcap;

int Debug = 0;
int snaplen = DEF_SNAPLEN;
int gotsig_close, gotsig_alrm, gotsig_hup;

char *filename = PFLOGD_LOG_FILE;
char *interface = PFLOGD_DEFAULT_IF;
char *pidfile = PFLOGD_PID_FILE;
char *filter = 0;

char errbuf[PCAP_ERRBUF_SIZE];

int log_debug = 0;
int delay = FLUSH_DELAY;

char *copy_argv(char * const *argv);
void logmsg(int priority, const char *message, ...);

char *
copy_argv(char * const *argv)
{
	int len = 0, n;
	char *buf;

	if (argv == NULL)
		return NULL;

	for (n = 0; argv[n]; n++)
		len += strlen(argv[n])+1;
	if (len <= 0)
		return NULL;

	buf = malloc(len);
	if (buf == NULL)
		return NULL;

	strlcpy(buf, argv[0], len);
	for (n = 1; argv[n]; n++) {
		strlcat(buf, " ", len);
		strlcat(buf, argv[n], len);
	}
	return buf;
}

void
logmsg(int pri, const char *message, ...)
{
	va_list ap;
	va_start(ap, message);

	if (log_debug)
		vfprintf(stderr,message,ap);
	else
		vsyslog(pri,message,ap);
	va_end(ap);
}

void
usage(void)
{
	fprintf(stderr, "usage: pflogd [-D] [-d delay] [-f filename] [-i interface] ");
	fprintf(stderr, "[-s snaplen] [expression]\n");
	exit(1);
}

void
sig_close(int signal)
{
	gotsig_close = 1;
}

void
sig_hup(int signal)
{
	gotsig_hup = 1;
}

void
sig_alrm(int signal)
{
	gotsig_alrm = 1;
}

int
reset_dump(void)
{
        struct pcap_file_header hdr;
	struct stat st;
	FILE *fp;

	if (hpcap == NULL)
		return 1;
	if (dpcap)
		pcap_dump_close(dpcap);

	/*
	 * Basically reimpliment pcap_dump_open() because it truncates
	 * files and duplicates headers and such.
	 */
	fp = fopen(filename, "a");
	if (fp == NULL) {
		snprintf(hpcap->errbuf, PCAP_ERRBUF_SIZE, "%s: %s",
		    filename, pcap_strerror(errno));
		logmsg(LOG_ERR, "Error: %s\n", pcap_geterr(hpcap));
		return 1;
	}
	if (fstat(fileno(fp), &st) == -1) {
		snprintf(hpcap->errbuf, PCAP_ERRBUF_SIZE, "%s: %s",
		    filename, pcap_strerror(errno));
		logmsg(LOG_ERR, "Error: %s\n", pcap_geterr(hpcap));
		return 1;
	}

	dpcap = (pcap_dumper_t *)fp;
	if (st.st_size != 0)
		return (0);

#define TCPDUMP_MAGIC 0xa1b2c3d4

	/* 
	 * XXX Must read the file, compare the header against our new
	 * options (in particular, snaplen) and adjust our options so
	 * that we generate a correct file.
	 */
	(void) fseek(fp, 0L, SEEK_SET);
	if (fread((char *)&hdr, sizeof(hdr), 1, fp) == 1) {
		if (hdr.magic == TCPDUMP_MAGIC &&
		    hdr.version_major == PCAP_VERSION_MAJOR &&
		    hdr.version_minor == PCAP_VERSION_MINOR &&
		    hdr.snaplen != snaplen) {
			logmsg(LOG_WARNING,
			    "Existing file specifies a snaplen of %d, using it",
			    hdr.snaplen);
			snaplen = hdr.snaplen;
		}
	}
	(void) fseek(fp, 0L, SEEK_END);

	hdr.magic = TCPDUMP_MAGIC;
	hdr.version_major = PCAP_VERSION_MAJOR;
	hdr.version_minor = PCAP_VERSION_MINOR;

	hdr.thiszone = hpcap->tzoff;
	hdr.snaplen = hpcap->snapshot;
	hdr.sigfigs = 0;
	hdr.linktype = hpcap->linktype;

	if (fwrite((char *)&hdr, sizeof(hdr), 1, fp) != 1) {
		dpcap = NULL;
		fclose(fp);
		return (-1);
	}
	return (0);
}

int
main(int argc, char **argv)
{
	struct bpf_program bprog;
	struct pcap_stat pstat;
	int ch, np;
	FILE *fp;

	while ((ch = getopt(argc, argv, "Dd:i:s:f:")) != -1) {
		switch (ch) {
		case 'D':
			Debug = 1;
			break;
		case 'd':
			delay = atoi(optarg);
			if (delay < 5 || delay > 60*60)
				usage();
			break;
		case 'i':
			interface = optarg;
			break;
		case 'f':
			filename = optarg;
			break;
		case 's':
			snaplen = atoi(optarg);
			if (snaplen <= 0)
				snaplen = DEF_SNAPLEN;
			break;
		default:
			usage();
		}

	}

	log_debug = Debug;
	argc -= optind;
	argv += optind;

	if (!Debug) {
		openlog("pflogd", LOG_PID | LOG_CONS, LOG_DAEMON);
		if (daemon(0, 0)) {
			logmsg(LOG_WARNING, "Failed to become deamon: %s",
				strerror(errno));
		}
		fp = fopen(pidfile, "w");
		if (fp != NULL) {
			fprintf(fp, "%d\n", getpid());
			fclose(fp);
		}
	}

	signal(SIGTERM, sig_close);
	signal(SIGINT, sig_close);
	signal(SIGQUIT, sig_close);
	signal(SIGALRM, sig_alrm);
	signal(SIGHUP, sig_hup);
	alarm(delay);

	if (argc) {
		filter = copy_argv(argv);
		if (filter == 0)
			logmsg(LOG_NOTICE, "Failed to form filter expression");
	}

	hpcap = pcap_open_live(interface, snaplen, 1, PCAP_TO_MS, errbuf);
	if (hpcap == NULL) {
		logmsg(LOG_ERR, "Failed to initialize: %s\n",errbuf);
		exit(1);
	}

	if (filter) {
		if (pcap_compile(hpcap, &bprog, filter, PCAP_OPT_FIL, 0) < 0)
			logmsg(LOG_WARNING, "%s", pcap_geterr(hpcap));
		else if (pcap_setfilter(hpcap, &bprog) < 0)
			logmsg(LOG_WARNING, "%s", pcap_geterr(hpcap));
	}

	if (pcap_datalink(hpcap) != DLT_PFLOG) {
		logmsg(LOG_ERR, "Invalid datalink type\n");
		pcap_close(hpcap);
		exit(1);
	}

	if (reset_dump()) {
		logmsg(LOG_ERR, "Failed to open log file %s\n", filename);
		pcap_close(hpcap);
		exit(1);
	}

	logmsg(LOG_NOTICE, "Listening on %s, logging to %s, snaplen %d\n",
	    interface, filename, snaplen);

	while (1) {
		np = pcap_dispatch(hpcap, PCAP_NUM_PKTS, pcap_dump, (u_char *)dpcap);
		if (np < 0)
			logmsg(LOG_NOTICE, "%s\n",pcap_geterr(hpcap));

		if (gotsig_close)
			break;
		if (gotsig_hup) {
			if (reset_dump()) {
				logmsg(LOG_ERR, "Failed to open log file!\n");
				break;
			}
			logmsg(LOG_NOTICE, "Reopened logfile\n");
			gotsig_hup = 0;
		}

		if (gotsig_alrm) {
			if (dpcap)
				fflush((FILE *)dpcap);		/* XXX */
			gotsig_alrm = 0;
			alarm(delay);
		}
	}

	logmsg(LOG_NOTICE, "Exiting due to signal\n");
	if (dpcap)
		pcap_dump_close(dpcap);

	if (pcap_stats(hpcap, &pstat) < 0)
		logmsg(LOG_WARNING, "Reading stats: %s\n", pcap_geterr(hpcap));
	else
		logmsg(LOG_NOTICE, "%d packets received, %d dropped\n",
		    pstat.ps_recv, pstat.ps_drop);

	pcap_close(hpcap);
	if (!Debug)
		closelog();
	return 0;
}
