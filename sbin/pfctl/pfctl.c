/*	$OpenBSD: pfctl.c,v 1.25 2001/07/01 16:58:51 kjell Exp $ */

/*
 * Copyright (c) 2001, Daniel Hartmeier
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
 *
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <net/pfvar.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <err.h>

#include "pfctl_parser.h"

#define PF_OPT_DISABLE		0x0001
#define PF_OPT_ENABLE		0x0002
#define PF_OPT_VERBOSE		0x0004
#define PF_OPT_NOACTION		0x0008

void	 usage(void);
char	*load_file(char *, size_t *);
int	 pfctl_enable(int);
int	 pfctl_disable(int);
int	 pfctl_clear_stats(int);
int	 pfctl_clear_rules(int);
int	 pfctl_clear_nat(int);
int	 pfctl_clear_states(int);
int	 pfctl_show_rules(int);
int	 pfctl_show_nat(int);
int	 pfctl_show_states(int, u_int8_t);
int	 pfctl_show_status(int);
int	 pfctl_rules(int, char *, int);
int	 pfctl_nat(int, char *, int);
int	 pfctl_log(int, char *);

int	 opts = 0;
char	*clearopt;
char	*logopt;
char	*natopt;
char	*rulesopt;
char	*showopt;

void
usage()
{
	fprintf(stderr, "usage: pfctl [-denvh] [-F set] [-l interface] ");
	fprintf(stderr, "[-N file] [-R file] [-s set]\n");
	exit(1);
}

char *
load_file(char *name, size_t *len)
{
	FILE *file;
	char *buf = 0, *buf2 = 0;
	u_int32_t i;

	if (!strcmp(name, "-"))
		file = stdin;
	else  
		file = fopen(name, "r");

	*len = 0;
	if (file == NULL) {
		fprintf(stderr, "ERROR: couldn't open file %s (%s)\n",
		    name, strerror(errno));
		return (0);
	}

	i = 512;	/* Start with this. Grow it as req'd */
	*len = 0;
	if ((buf = malloc(i)) == NULL) {
		fprintf(stderr, "ERROR: could not allocate space "
			"for rules file\n");
		return (0);
	}
	while (!feof(file)) {
		*len += fread((buf + *len), 1, (i - *len), file);
		if (*len == i) {
			/* Out of space - realloc time */
			i *= 2;
			if ((buf2 = realloc(buf, i)) == NULL) {
				if (buf)
					free(buf);
				buf = NULL;
				fprintf(stderr, "ERROR: realloc of "
					"stdin buffer failed\n");
				return (0);
			}
			buf = buf2;
		}
	}
	if (*len == i) {
		/* 
		 * file is exactly the size of our buffer.
		 * grow ours one so we can null terminate it
		 */
		if ((buf2 = realloc(buf, i+1)) == NULL) {
			if (buf)
				free(buf);
			buf = NULL;
			fprintf(stderr, "ERROR: realloc of "
				"stdin buffer failed\n");
			return (0);
		}
		buf = buf2;
	}
	if (file != stdin)
		fclose(file);
	buf[*len]='\0';
	if (strlen(buf) != *len) {
		fprintf(stderr, "WARNING: nulls embedded in rules file\n");
		*len = strlen(buf);
	}
	return (buf);
}

int
pfctl_enable(int dev)
{
	if (ioctl(dev, DIOCSTART)) {
		if (errno == EEXIST)
			errx(1, "pf already enabled");
		else
			err(1, "DIOCSTART");
	}
	printf("pf enabled\n");
	return (0);
}

int
pfctl_disable(int dev)
{
	if (ioctl(dev, DIOCSTOP)) {
		if (errno == ENOENT)
			errx(1, "pf not enabled");
		else
			err(1, "DIOCSTOP");
	}
	printf("pf disabled\n");
	return (0);
}

int
pfctl_clear_stats(int dev)
{
	if (ioctl(dev, DIOCCLRSTATUS))
		err(1, "DIOCCLRSTATUS");
	printf("pf: statistics cleared\n");
	return (0);
}

int
pfctl_clear_rules(int dev)
{
	struct pfioc_rule pr;

	if (ioctl(dev, DIOCBEGINRULES, &pr.ticket))
		err(1, "DIOCBEGINRULES");
	else if (ioctl(dev, DIOCCOMMITRULES, &pr.ticket))
		err(1, "DIOCCOMMITRULES");
	printf("rules cleared\n");
	return (0);
}

int
pfctl_clear_nat(int dev)
{
	struct pfioc_nat pn;
	struct pfioc_rdr pr;

	if (ioctl(dev, DIOCBEGINNATS, &pn.ticket))
		err(1, "DIOCBEGINNATS");
	else if (ioctl(dev, DIOCCOMMITNATS, &pn.ticket))
		err(1, "DIOCCOMMITNATS");
	else if (ioctl(dev, DIOCBEGINRDRS, &pr.ticket))
		err(1, "DIOCBEGINRDRS");
	else if (ioctl(dev, DIOCCOMMITRDRS, &pr.ticket))
		err(1, "DIOCCOMMITRDRS");
	printf("nat cleared\n");
	return (0);
}

int
pfctl_clear_states(int dev)
{
	if (ioctl(dev, DIOCCLRSTATES))
		err(1, "DIOCCLRSTATES");
	printf("states cleared\n");
	return (0);
}

int
pfctl_show_rules(int dev)
{
	struct pfioc_rule pr;
	u_int32_t nr, mnr;

	if (ioctl(dev, DIOCGETRULES, &pr))
		err(1, "DIOCGETRULES");
	mnr = pr.nr;
	for (nr = 0; nr < mnr; ++nr) {
		pr.nr = nr;
		if (ioctl(dev, DIOCGETRULE, &pr))
			err(1, "DIOCGETRULE");
		print_rule(&pr.rule);
	}
	return (0);
}

int
pfctl_show_nat(int dev)
{
	struct pfioc_nat pn;
	struct pfioc_rdr pr;
	u_int32_t mnr, nr;

	if (ioctl(dev, DIOCGETNATS, &pn))
		err(1, "DIOCGETNATS");
	mnr = pn.nr;
	for (nr = 0; nr < mnr; ++nr) {
		pn.nr = nr;
		if (ioctl(dev, DIOCGETNAT, &pn))
			err(1, "DIOCGETNAT");
		print_nat(&pn.nat);
	}
	if (ioctl(dev, DIOCGETRDRS, &pr))
		err(1, "DIOCGETRDRS");
	mnr = pr.nr;
	for (nr = 0; nr < mnr; ++nr) {
		pr.nr = nr;
		if (ioctl(dev, DIOCGETRDR, &pr))
			err(1, "DIOCGETRDR");
		print_rdr(&pr.rdr);
	}
	return (0);
}

int
pfctl_show_states(int dev, u_int8_t proto)
{
	struct pfioc_state ps;

	ps.nr = 0;
	while (!ioctl(dev, DIOCGETSTATE, &ps)) {
		if (!proto || (ps.state.proto == proto))
			print_state(&ps.state);
		ps.nr++;
	}
	return (0);
}

int
pfctl_show_status(int dev)
{
	struct pf_status status;

	if (ioctl(dev, DIOCGETSTATUS, &status))
		err(1, "DIOCGETSTATUS");
	print_status(&status);
	return (0);
}

int
pfctl_rules(int dev, char *filename, int opts)
{
	struct pfioc_rule pr;
	char *buf, *s;
	size_t len;
	unsigned n, nr;

	buf = load_file(filename, &len);
	if (buf == NULL)
		return (1);
	if ((opts & PF_OPT_NOACTION) == 0) {
		if (ioctl(dev, DIOCBEGINRULES, &pr.ticket))
			err(1, "DIOCBEGINRULES");
	}
	n = 0;
	nr = 0;
	s = buf;
	do {
		char *line = next_line(&s);
		nr++;
		if (*line && (*line != '#'))
			if (parse_rule(nr, line, &pr.rule)) {
				if ((opts & PF_OPT_NOACTION) == 0) {
					if (ioctl(dev, DIOCADDRULE, &pr))
						err(1, "DIOCADDRULE");
				}
				if (opts & PF_OPT_VERBOSE)
					print_rule(&pr.rule);
				n++;
			}
	} while (s < (buf + len));
	free(buf);
	if ((opts & PF_OPT_NOACTION) == 0) {
		if (ioctl(dev, DIOCCOMMITRULES, &pr.ticket))
			err(1, "DIOCCOMMITRULES");
		printf("%u rules loaded\n", n);
	}
	return (0);
}

int
pfctl_nat(int dev, char *filename, int opts)
{
	struct pfioc_nat pn;
	struct pfioc_rdr pr;
	char *buf, *s;
	size_t len;
	unsigned n, r, nr;

	if ((opts & PF_OPT_NOACTION) == 0) {
		if (ioctl(dev, DIOCBEGINNATS, &pn.ticket))
			err(1, "DIOCBEGINNATS");
		
		if (ioctl(dev, DIOCBEGINRDRS, &pr.ticket))
			err(1, "DIOCBEGINRDRS");
	}

	buf = load_file(filename, &len);
	if (buf == NULL)
		return (1);
	n = 0;
	r = 0;
	nr = 0;
	s = buf;
	do {
		char *line = next_line(&s);
		nr++;
		if (*line && (*line == 'n'))
			if (parse_nat(nr, line, &pn.nat)) {
				if ((opts & PF_OPT_NOACTION) == 0)
					if (ioctl(dev, DIOCADDNAT, &pn))
						err(1, "DIOCADDNAT");
				if (opts & PF_OPT_VERBOSE)
					print_nat(&pn.nat);
				n++;
			}
		if (*line && (*line == 'r'))
			if (parse_rdr(nr, line, &pr.rdr)) {
				if ((opts & PF_OPT_NOACTION) == 0)
					if (ioctl(dev, DIOCADDRDR, &pr))
						err(1, "DIOCADDRDR");
				if (opts & PF_OPT_VERBOSE)
					print_rdr(&pr.rdr);
				r++;
			}
	} while (s < (buf + len));

	if ((opts & PF_OPT_NOACTION) == 0) {
		if (ioctl(dev, DIOCCOMMITNATS, &pn.ticket))
			err(1, "DIOCCOMMITNATS");
		if (ioctl(dev, DIOCCOMMITRDRS, &pr.ticket))
			err(1, "DIOCCOMMITRDRS");
		printf("%u nat entries loaded\n", n);
		printf("%u rdr entries loaded\n", r);
	}
	free(buf);
	return (0);
}

int
pfctl_log(int dev, char *ifname)
{
	struct pfioc_if pi;

	strncpy(pi.ifname, ifname, 16);
	if (ioctl(dev, DIOCSETSTATUSIF, &pi))
		err(1, "DIOCSETSTATUSIF");
	printf("now logging %s\n", pi.ifname);
	return (0);
}

int
main(int argc, char *argv[])
{
	extern char *optarg;
	extern int optind;
	int error = 0;
	int dev;
	int ch;

	if (argc < 2)
		usage();

	while ((ch = getopt(argc, argv, "deF:hl:nN:R:s:v")) != -1) {
		switch (ch) {
		case 'd':
			opts |= PF_OPT_DISABLE;
			break;
		case 'e':
			opts |= PF_OPT_ENABLE;
			break;
		case 'F':
			clearopt = optarg;
			break;
		case 'l':
			logopt = optarg;
			break;
		case 'n':
			opts |= PF_OPT_NOACTION;
			break;
		case 'N':
			natopt = optarg;
			break;
		case 'R':
			rulesopt = optarg;
			break;
		case 's':
			showopt = optarg;
			break;
		case 'v':
			opts |= PF_OPT_VERBOSE;
			break;
		case 'h':
		default:
			usage();
			/* NOTREACHED */
		}
	}

	dev = open("/dev/pf", O_RDWR);
	if (dev == -1)
		err(1, "open(\"/dev/pf\")");

	if (opts & PF_OPT_DISABLE)
		if (pfctl_disable(dev))
			error = 1;

	if (clearopt != NULL) {
		
		switch (*clearopt) {
		case 'r':
			pfctl_clear_rules(dev);
			break;
		case 'n':
			pfctl_clear_nat(dev);
			break;
		case 's':
			pfctl_clear_states(dev);
			break;
		case 'i':
			pfctl_clear_stats(dev);
			break;
		case 'a':
			pfctl_clear_rules(dev);
			pfctl_clear_nat(dev);
			pfctl_clear_states(dev);
			pfctl_clear_stats(dev);
			break;
		default:
			warnx("Unknown flush modifier '%s'", clearopt);
			error = 1;
		}
	}

	if (rulesopt != NULL)
		if (pfctl_rules(dev, rulesopt, opts))
			error = 1;

	if (natopt != NULL)
		if (pfctl_nat(dev, natopt, opts))
			error = 1;

	if (showopt != NULL) {
		switch (*showopt) {
		case 'r':
			pfctl_show_rules(dev);
			break;
		case 'n':
			pfctl_show_nat(dev);
			break;
		case 's':
			pfctl_show_states(dev, 0);
			break;
		case 'i':
			pfctl_show_status(dev);
			break;
		case 'a':
			pfctl_show_rules(dev);
			pfctl_show_nat(dev);
			pfctl_show_states(dev, 0);
			pfctl_show_status(dev);
			break;
		default:
			warnx("Unknown show modifier '%s'", showopt);
			error = 1;
		}
	}

	if (logopt != NULL)
		if (pfctl_log(dev, logopt))
			error = 1;

	if (opts & PF_OPT_ENABLE)
		if (pfctl_enable(dev))
			error = 1;

	close(dev);

	exit(error);
}
