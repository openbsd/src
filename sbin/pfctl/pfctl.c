/*	$OpenBSD: pfctl.c,v 1.19 2001/06/27 10:31:49 kjell Exp $ */

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
int	 pfctl_rules(int, char *, int, int);
int	 pfctl_nat(int, char *, int, int);
int	 pfctl_log(int, char *);

int	 dflag = 0;
int	 eflag = 0;
int	 vflag = 0;
int	 Nflag = 0;
int	 zflag = 0;
char	*clearopt;
char	*logopt;
char	*natopt;
char	*rulesopt;
char	*showopt;

void
usage()
{
	extern char *__progname;
 
	fprintf(stderr, "usage: %s [-de] [-c set] [-l interface]", __progname);
	fprintf(stderr, " [-N] [-n file] [-r file] [-s set] [-v] [-z]\n");
	exit(1);
}

char *
load_file(char *name, size_t *len)
{
	FILE *file = fopen(name, "r");
	char *buf = 0;

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
		errx(1, "DIOCSTART");
		return (1);
	}
	printf("pf enabled\n");
	return (0);
}

int
pfctl_disable(int dev)
{
	if (ioctl(dev, DIOCSTOP)) {
		errx(1, "DIOCSTOP");
		return (1);
	}
	printf("pf disabled\n");
	return (0);
}

int
pfctl_clear_stats(int dev)
{
	if (ioctl(dev, DIOCCLRSTATUS)) {
		errx(1, "DIOCCLRSTATUS");
		return (1);
	}
	printf("pf: statistics cleared\n");
	return (0);
}

int
pfctl_clear_rules(int dev)
{
	struct pfioc_rule pr;

	if (ioctl(dev, DIOCBEGINRULES, &pr.ticket)) {
		errx(1, "DIOCBEGINRULES");
		return (1);
	} else if (ioctl(dev, DIOCCOMMITRULES, &pr.ticket)) {
		errx(1, "DIOCCOMMITRULES");
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
		errx(1, "DIOCBEGINNATS");
		return (1);
	} else if (ioctl(dev, DIOCCOMMITNATS, &pn.ticket)) {
		errx(1, "DIOCCOMMITNATS");
		return (1);
	} else if (ioctl(dev, DIOCBEGINRDRS, &pr.ticket)) {
		errx(1, "DIOCBEGINRDRS");
		return (1);
	} else if (ioctl(dev, DIOCCOMMITRDRS, &pr.ticket)) {
		errx(1, "DIOCCOMMITRDRS");
		return (1);
	}
	printf("nat cleared\n");
	return (0);
}

int
pfctl_clear_states(int dev)
{
	if (ioctl(dev, DIOCCLRSTATES)) {
		errx(1, "DIOCCLRSTATES");
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
		errx(1, "DIOCGETRULES");
		return (1);
	}
	mnr = pr.nr;
	for (nr = 0; nr < mnr; ++nr) {
		pr.nr = nr;
		if (ioctl(dev, DIOCGETRULE, &pr)) {
			errx(1, "DIOCGETRULE");
			return (1);
		}
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
		errx(1, "DIOCGETNATS");
		return (1);
	}
	mnr = pn.nr;
	for (nr = 0; nr < mnr; ++nr) {
		pn.nr = nr;
		if (ioctl(dev, DIOCGETNAT, &pn)) {
			errx(1, "DIOCGETNAT");
			return (1);
		}
		print_nat(&pn.nat);
	}
	if (ioctl(dev, DIOCGETRDRS, &pr)) {
		errx(1, "DIOCGETRDRS");
		return (1);
	}
	mnr = pr.nr;
	for (nr = 0; nr < mnr; ++nr) {
		pr.nr = nr;
		if (ioctl(dev, DIOCGETRDR, &pr)) {
			errx(1, "DIOCGETRDR");
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
		errx(1, "DIOCGETSTATUS");
		return (1);
	}
	print_status(&status);
	return (0);
}

int
pfctl_rules(int dev, char *filename, int nflag, int vflag)
{
	struct pfioc_rule pr;
	char *buf, *s;
	size_t len;
	unsigned n, nr;

	buf = load_file(filename, &len);
	if (buf == NULL)
		return (1);
	if (!nflag) {
		if (ioctl(dev, DIOCBEGINRULES, &pr.ticket)) {
			errx(1, "DIOCBEGINRULES");
			free(buf);
			return (1);
		}
	}
	n = 0;
	nr = 0;
	s = buf;
	do {
		char *line = next_line(&s);
		nr++;
		if (*line && (*line != '#'))
			if (parse_rule(nr, line, &pr.rule)) {
				if (!nflag) {
					if (ioctl(dev, DIOCADDRULE, &pr)) {
						errx(1, "DIOCADDRULE");
						free(buf);
						return (1);
					}
				}
				if (vflag)
					print_rule(&pr.rule);
				n++;
			}
	} while (s < (buf + len));
	free(buf);
	if (!nflag) {
		if (ioctl(dev, DIOCCOMMITRULES, &pr.ticket)) {
			errx(1, "DIOCCOMMITRULES");
			return (1);
		}
		printf("%u rules loaded\n", n);
	}
	return (0);
}

int
pfctl_nat(int dev, char *filename, int nflag, int vflag)
{
	struct pfioc_nat pn;
	struct pfioc_rdr pr;
	char *buf, *s;
	size_t len;
	unsigned n, nr;

	if (!nflag) 
		if (ioctl(dev, DIOCBEGINNATS, &pn.ticket)) {
			errx(1, "DIOCBEGINNATS");
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
				if (!nflag)
					if (ioctl(dev, DIOCADDNAT, &pn)) {
						errx(1, "DIOCADDNAT");
						free(buf);
						return (1);
					}
				if (vflag)
					print_nat(&pn.nat);
				n++;
			}
	} while (s < (buf + len));
	free(buf);
	if (!nflag) {
		if (ioctl(dev, DIOCCOMMITNATS, &pn.ticket)) {
			errx(1, "DIOCCOMMITNATS");
			return (1);
		}
		printf("%u nat entries loaded\n", n);

		if (ioctl(dev, DIOCBEGINRDRS, &pr.ticket)) {
			errx(1, "DIOCBEGINRDRS");
			return 1;
		}
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
				if (!nflag)
					if (ioctl(dev, DIOCADDRDR, &pr)) {
						errx(1, "DIOCADDRDR");
						free(buf);
						return (1);
					}
				if (vflag)
					print_rdr(&pr.rdr);
				n++;
			}
	} while (s < (buf + len));
	free(buf);
	if (!nflag) {
		if (ioctl(dev, DIOCCOMMITRDRS, &pr.ticket)) {
			errx(1, "DIOCCOMMITRDRS");
			return (1);
		}
		printf("%u rdr entries loaded\n", n);
	}
	return (0);
}

int
pfctl_log(int dev, char *ifname)
{
	struct pfioc_if pi;

	strncpy(pi.ifname, ifname, 16);
	if (ioctl(dev, DIOCSETSTATUSIF, &pi)) {
		errx(1, "DIOCSETSTATUSIF");
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

	if (argc < 2)
		usage();

	while ((ch = getopt(argc, argv, "c:del:Nn:r:s:vz")) != -1) {
		switch (ch) {
		case 'c':
			clearopt = optarg;
			break;
		case 'd':
			dflag++;
			break;
		case 'e':
			eflag++;
			break;
		case 'l':
			logopt = optarg;
			break;
		case 'N':
			Nflag++;
			break;
		case 'n':
			natopt = optarg;
			break;
		case 'r':
			rulesopt = optarg;
			break;
		case 's':
			showopt = optarg;
			break;
		case 'v':
			vflag++;
			break;
		case 'z':
			zflag++;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}

	dev = open("/dev/pf", O_RDWR);
	if (dev == -1) {
		errx(1, "/dev/pf");
		return (1);
	}

	if (dflag)
		if (pfctl_disable(dev))
			error = 1;

	if (zflag)
		if (pfctl_clear_stats(dev))
			error = 1;

	if (clearopt != NULL) {
		if (!strcmp(clearopt, "rules")) {
			if (pfctl_clear_rules(dev))
				error = 1;
		} else if (!strcmp(clearopt, "nat")) {
			if (pfctl_clear_nat(dev))
				error = 1;
		} else if (!strcmp(clearopt, "states")) {
			if (pfctl_clear_states(dev))
				error = 1;
		} else {
			warnx("Unknown keyword '%s'", clearopt);
			error = 1;
		}
	}

	if (rulesopt != NULL)
		if (pfctl_rules(dev, rulesopt, Nflag, vflag))
			error = 1;

	if (natopt != NULL)
		if (pfctl_nat(dev, natopt, Nflag, vflag))
			error = 1;

	if (showopt != NULL) {
		if (!strcmp(showopt, "rules")) {
			if (pfctl_show_rules(dev))
				error = 1;
		} else if (!strcmp(showopt, "nat")) {
			if (pfctl_show_nat(dev))
				error = 1;
		} else if (!strcmp(showopt, "states")) {
			if (pfctl_show_states(dev, 0))
				error = 1;
		} else if (!strcmp(showopt, "status")) {
			if (pfctl_show_status(dev))
				error = 1;
		} else {
			warnx("Unknown keyword '%s'", showopt);
			error = 1;
		}
	}

	if (logopt != NULL)
		if (pfctl_log(dev, logopt))
			error = 1;

	if (eflag)
		if (pfctl_enable(dev))
			error = 1;

	close(dev);

	exit(error);
}
