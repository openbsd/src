/*	$OpenBSD: pfctl.c,v 1.9 2001/06/25 17:17:06 dhartmei Exp $ */

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

#include "pfctl_parser.h"

void	 print_error(char *);
void	 usage();
char	*load_file(char *, size_t *);
int	 pfctl_enable(int);
int	 pfctl_disable(int);
int	 pfctl_clear_rules(int);
int	 pfctl_clear_nat(int);
int	 pfctl_clear_states(int);
int	 pfctl_show_rules(int);
int	 pfctl_show_nat(int);
int	 pfctl_show_states(int, u_int8_t);
int	 pfctl_show_status(int);
int	 pfctl_rules(int, char *);
int	 pfctl_nat(int, char *);
int	 pfctl_log(int, char *);
int	 main(int, char *[]);

void
print_error(char *s)
{
	fprintf(stderr, "ERROR: %s: %s\n", s, strerror(errno));
	return;
}

void
usage()
{
	extern char *__progname;
 
	fprintf(stderr, "usage: %s [-d] [-c set] [-r file]", __progname);
	fprintf(stderr, " [-n file] [-s set] [-l if] [-e]\n");
}

char *
load_file(char *name, size_t *len)
{
	char *buf = 0;
	FILE *file = fopen(name, "r");

	*len = 0;
	if (file == NULL) {
		fprintf(stderr, "ERROR: couldn't open file %s (%s)\n",
		    name, strerror(errno));
		return (0);
	}
	fseek(file, 0, SEEK_END);
	*len = ftell(file);
	fseek(file, 0, SEEK_SET);
	buf = malloc(*len);
    	if (buf == NULL) {
		fclose(file);
		fprintf(stderr, "ERROR: malloc() failed\n");
		return (0);
	}
	if (fread(buf, 1, *len, file) != *len) {
		free(buf);
		fclose(file);
		fprintf(stderr, "ERROR: fread() failed\n");
		return (0);
	}
	fclose(file);
	return (buf);
}

int
pfctl_enable(int dev)
{
	if (ioctl(dev, DIOCSTART)) {
		print_error("DIOCSTART");
		return 1;
	}
	printf("pf enabled\n");
	return 0;
}

int
pfctl_disable(int dev)
{
	if (ioctl(dev, DIOCSTOP)) {
		print_error("DIOCSTOP");
		return 1;
	}
	printf("pf disabled\n");
	return 0;
}

int
pfctl_clear_rules(int dev)
{
	struct pfioc_rule pr;

	if (ioctl(dev, DIOCBEGINRULES, &pr.ticket)) {
		print_error("DIOCBEGINRULES");
		return (1);
	} else if (ioctl(dev, DIOCCOMMITRULES, &pr.ticket)) {
		print_error("DIOCCOMMITRULES");
		return (1);
	}
	printf("rules cleared\n");
	return (0);
}

int
pfctl_clear_nat(int dev)
{
	struct pfioc_nat pn;
	struct pfioc_rdr pr;

	if (ioctl(dev, DIOCBEGINNATS, &pn.ticket)) {
		print_error("DIOCBEGINNATS");
		return (1);
	} else if (ioctl(dev, DIOCCOMMITNATS, &pn.ticket)) {
		print_error("DIOCCOMMITNATS");
		return (1);
	} else if (ioctl(dev, DIOCBEGINRDRS, &pr.ticket)) {
		print_error("DIOCBEGINRDRS");
		return (1);
	} else if (ioctl(dev, DIOCCOMMITRDRS, &pr.ticket)) {
		print_error("DIOCCOMMITRDRS");
		return (1);
	}
	printf("nat cleared\n");
	return (0);
}

int
pfctl_clear_states(int dev)
{
	if (ioctl(dev, DIOCCLRSTATES)) {
		print_error("DIOCCLRSTATES");
		return (1);
	}
	printf("states cleared\n");
	return (0);
}

int
pfctl_show_rules(int dev)
{
	struct pfioc_rule pr;
	u_int32_t nr, mnr;

	if (ioctl(dev, DIOCGETRULES, &pr)) {
		print_error("DIOCGETRULES");
		return (1);
	}
	mnr = pr.nr;
	for (nr = 0; nr < mnr; ++nr) {
		pr.nr = nr;
		if (ioctl(dev, DIOCGETRULE, &pr)) {
			print_error("DIOCGETRULE");
			return (1);
		}
		printf("@%u ", nr + 1);
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

	if (ioctl(dev, DIOCGETNATS, &pn)) {
		print_error("DIOCGETNATS");
		return (1);
	}
	mnr = pn.nr;
	for (nr = 0; nr < mnr; ++nr) {
		pn.nr = nr;
		if (ioctl(dev, DIOCGETNAT, &pn)) {
			print_error("DIOCGETNAT");
			return (1);
		}
		print_nat(&pn.nat);
	}
	if (ioctl(dev, DIOCGETRDRS, &pr)) {
		print_error("DIOCGETRDRS");
		return (1);
	}
	mnr = pr.nr;
	for (nr = 0; nr < mnr; ++nr) {
		pr.nr = nr;
		if (ioctl(dev, DIOCGETRDR, &pr)) {
			print_error("DIOCGETRDR");
			return (1);
		}
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

	if (ioctl(dev, DIOCGETSTATUS, &status)) {
		print_error("DIOCGETSTATUS");
		return (1);
	}
	print_status(&status);
	return (0);
}

int
pfctl_rules(int dev, char *filename)
{
	struct pfioc_rule pr;
	char *buf, *s;
	size_t len;
	unsigned n, nr;

	buf = load_file(filename, &len);
	if (buf == NULL)
		return (1);
	if (ioctl(dev, DIOCBEGINRULES, &pr.ticket)) {
		print_error("DIOCBEGINRULES");
		free(buf);
		return (1);
	}
	n = 0;
	nr = 0;
	s = buf;
	do {
		char *line = next_line(&s);
		nr++;
		if (*line && (*line != '#'))
			if (parse_rule(nr, line, &pr.rule)) {
				if (ioctl(dev, DIOCADDRULE, &pr)) {
					print_error("DIOCADDRULE");
					free(buf);
					return (1);
				}
				n++;
			}
	} while (s < (buf + len));
	free(buf);
	if (ioctl(dev, DIOCCOMMITRULES, &pr.ticket)) {
		print_error("DIOCCOMMITRULES");
		return (1);
	}
	printf("%u rules loaded\n", n);
	return (0);
}

int
pfctl_nat(int dev, char *filename)
{
	struct pfioc_nat pn;
	struct pfioc_rdr pr;
	char *buf, *s;
	size_t len;
	unsigned n, nr;

	if (ioctl(dev, DIOCBEGINNATS, &pn.ticket)) {
		print_error("DIOCBEGINNATS");
		return (1);
	}
	buf = load_file(filename, &len);
	if (buf == NULL)
		return (1);
	n = 0;
	nr = 0;
	s = buf;
	do {
		char *line = next_line(&s);
		nr++;
		if (*line && (*line == 'n'))
			if (parse_nat(nr, line, &pn.nat)) {
				if (ioctl(dev, DIOCADDNAT, &pn)) {
					print_error("DIOCADDNAT");
					free(buf);
					return (1);
				}
				n++;
			}
	} while (s < (buf + len));
	free(buf);
	if (ioctl(dev, DIOCCOMMITNATS, &pn.ticket)) {
		print_error("DIOCCOMMITNATS");
		return (1);
	}
	printf("%u nat entries loaded\n", n);

	if (ioctl(dev, DIOCBEGINRDRS, &pr.ticket)) {
		print_error("DIOCBEGINRDRS");
		return 1;
	}
	buf = load_file(filename, &len);
	if (buf == NULL)
		return (1);
	n = 0;
	nr = 0;
	s = buf;
	do {
		char *line = next_line(&s);
		nr++;
		if (*line && (*line == 'r'))
			if (parse_rdr(nr, line, &pr.rdr)) {
				if (ioctl(dev, DIOCADDRDR, &pr)) {
					print_error("DIOCADDRDR");
					free(buf);
					return (1);
				}
				n++;
			}
	} while (s < (buf + len));
	free(buf);
	if (ioctl(dev, DIOCCOMMITRDRS, &pr.ticket)) {
		print_error("DIOCCOMMITRDRS");
		return (1);
	}
	printf("%u rdr entries loaded\n", n);
	return (0);
}

int
pfctl_log(int dev, char *ifname)
{
	struct pfioc_if pi;

	strncpy(pi.ifname, ifname, 16);
	if (ioctl(dev, DIOCSETSTATUSIF, &pi)) {
		print_error("DIOCSETSTATUSIF");
		return (1);
	}
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

	if (argc <= 1) {
		usage();
		return (0);
	}
	dev = open("/dev/pf", O_RDWR);
	if (dev < 0) {
		print_error("open(/dev/pf)");
		return (1);
	}
	while (!error && (ch = getopt(argc, argv, "dc:r:n:s:l:e")) != -1) {
		switch (ch) {
		case 'd':
			if (pfctl_disable(dev))
				error = 1;
			break;
		case 'c':
			if (!strcmp(optarg, "rules")) {
				if (pfctl_clear_rules(dev))
					error = 1;
			} else if (!strcmp(optarg, "nat")) {
				if (pfctl_clear_nat(dev))
					error = 1;
			} else if (!strcmp(optarg, "states")) {
				if (pfctl_clear_states(dev))
					error = 1;
			} else
				error = 1;
			break;
		case 'r':
			if (pfctl_rules(dev, optarg))
				error = 1;
			break;
		case 'n':
			if (pfctl_nat(dev, optarg))
				error = 1;
			break;
		case 's':
			if (!strcmp(optarg, "rules")) {
				if (pfctl_show_rules(dev))
					error = 1;
			} else if (!strcmp(optarg, "nat")) {
				if (pfctl_show_nat(dev))
					error = 1;
			} else if (!strcmp(optarg, "states")) {
				if (pfctl_show_states(dev, 0))
					error = 1;
			} else if (!strcmp(optarg, "status")) {
				if (pfctl_show_status(dev))
					error = 1;
			} else
				error = 1;
			break;
		case 'l':
			if (pfctl_log(dev, optarg))
				error = 1;
			break;
		case 'e':
			if (pfctl_enable(dev))
				error = 1;
			break;
		default:
			usage();
			error = 1;
		}
	}
	close(dev);
	return (error);
}
