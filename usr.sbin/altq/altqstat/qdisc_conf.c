/*	$KAME: qdisc_conf.c,v 1.3 2000/10/18 09:15:16 kjc Exp $	*/
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

#include <sys/param.h>
#include <sys/socket.h>
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/ioctl.h>
#endif
#include <sys/fcntl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <altq/altq.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <err.h>

#include "altqstat.h"

#define ALTQ_DEVICE	"/dev/altq/altq"

struct qdisc_conf qdisc_table[] = {
	{"cbq",		ALTQT_CBQ,	cbq_stat_loop},
	{"hfsc",	ALTQT_HFSC,	hfsc_stat_loop},
	{"cdnr",	ALTQT_CDNR,	cdnr_stat_loop},
	{"wfq",		ALTQT_WFQ,	wfq_stat_loop},
	{"fifoq",	ALTQT_FIFOQ,	fifoq_stat_loop},
	{"red",		ALTQT_RED,	red_stat_loop},
	{"rio",		ALTQT_RIO,	rio_stat_loop},
	{"blue",	ALTQT_BLUE,	blue_stat_loop},
	{"priq",	ALTQT_PRIQ,	priq_stat_loop},
	{NULL, 		0,		NULL}
};

stat_loop_t *
qdisc2stat_loop(const char *qdisc_name)
{
	struct qdisc_conf *stat;

	for (stat = qdisc_table; stat->qdisc_name != NULL; stat++)
		if (strcmp(stat->qdisc_name, qdisc_name) == 0)
			return (stat->stat_loop);
	return (NULL);
}

int
ifname2qdisc(const char *ifname, char *qname)
{
	struct altqreq qtypereq;
	int fd, qtype = 0;

	if (ifname[0] == '_') {
		/* input interface */
		if (qname != NULL)
			strcpy(qname, "cdnr");
		return (ALTQT_CDNR);
	}

	strcpy(qtypereq.ifname, ifname);
	if ((fd = open(ALTQ_DEVICE, O_RDONLY)) < 0) {
		warn("can't open %s", ALTQ_DEVICE);
		return (0);
	}
	if (ioctl(fd, ALTQGTYPE, &qtypereq) < 0) {
		warn("ALTQGQTYPE");
		return (0);
	}
	close(fd);

	if (qname != NULL) {
		struct qdisc_conf *stat;

		qtype = qtypereq.arg;
		for (stat = qdisc_table; stat->qdisc_name != NULL; stat++)
			if (stat->altqtype == qtype)
				strcpy(qname, stat->qdisc_name);
	}
		
	return (qtype);
}

