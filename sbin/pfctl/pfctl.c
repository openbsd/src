/*	$OpenBSD: pfctl.c,v 1.7 2001/06/25 09:44:32 deraadt Exp $ */

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

void	 printerror(char *);
void	 usage(char *);
char	*load_file(char *, size_t *);
int		 main(int, char *[]);

void
printerror(char *s)
{
	fprintf(stderr, "ERROR: %s: %s\n", s, strerror(errno));
	return;
}

void
usage(char *argv0)
{
	char *n = rindex(argv0, '/');

	if (n != NULL)
		n++;
	else
		n = argv0;
	fprintf(stderr, "Usage: %s command argument\n", n);
	fprintf(stderr, "\tstart\t\t\tStart packet filter\n");
	fprintf(stderr, "\tstop\t\t\tStop packet filter\n");
	fprintf(stderr, "\tshow\trules\t\tShow filter rules\n");
	fprintf(stderr, "\t\tnat\t\t     NAT/RDR rules\n");
	fprintf(stderr, "\t\tstates [proto]\t     list of active states\n");
	fprintf(stderr, "\t\tstatus\t\t     status\n");
	fprintf(stderr, "\tclear\trules\t\tClear filter rules\n");
	fprintf(stderr, "\t\tnat\t\t      NAT/RDR rules\n");
	fprintf(stderr, "\t\tstates\t\t      states\n");
	fprintf(stderr, "\tparse\trules\t<file>\tCheck syntax of filter rules\n");
	fprintf(stderr, "\t\tnat\t<file>\t                NAT/RDR rules\n");
	fprintf(stderr, "\tload\trules\t<file>\tLoad filter rules\n");
	fprintf(stderr, "\t\tnat\t<file>\t     NAT/RDR rules\n");
	fprintf(stderr, "\tlog\t\t<if>\tSet interface to log\n");
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
main(int argc, char *argv[])
{
	int dev;
	struct pfioc *ub;
	u_int16_t n = 0;

	ub = malloc(sizeof(struct pfioc));
	if (ub == NULL) {
		printf("ERROR: malloc() failed\n");
		return (1);
	}
	ub->size = 131072;
	ub->buffer = malloc(ub->size);
	if (ub->buffer == NULL) {
		printf("ERROR: malloc() failed\n");
		return (1);
	}
	memset(ub->buffer, 0, ub->size);
	ub->entries = 0;
	if (argc < 2) {
		usage(argv[0]);
		return (1);
	}
	dev = open("/dev/pf", O_RDWR);
	if (dev < 0) {
		printerror("open(/dev/pf)");
		return (1);
	}
	if (!strcmp(argv[1], "start")) {
		if (ioctl(dev, DIOCSTART))
			printerror("DIOCSTART");
		else
			printf("packetfilter started\n");
	}
	else if (!strcmp(argv[1], "stop")) {
		if (ioctl(dev, DIOCSTOP))
			printerror("DIOCSTOP");
		else
			printf("packetfilter stopped\n");
	}
	else if (!strcmp(argv[1], "show")) {
		if (argc < 3) {
			close(dev);
			usage(argv[0]);
			return (1);
		}
		if (!strcmp(argv[2], "rules")) {
			struct pf_rule *rule = ub->buffer;

			ub->entries = ub->size / sizeof(struct pf_rule);
			if (ioctl(dev, DIOCGETRULES, ub))
				printerror("DIOCGETRULES");
			for (n = 0; n < ub->entries; ++n) {
				printf("@%u ", n + 1);
				print_rule(rule + n);
			}
		}
		else if (!strcmp(argv[2], "nat")) {
			struct pf_nat *nat = ub->buffer;
			struct pf_rdr *rdr = ub->buffer;

			ub->entries = ub->size / sizeof(struct pf_nat);
			if (ioctl(dev, DIOCGETNAT, ub))
				printerror("DIOCGETNAT");
			for (n = 0; n < ub->entries; ++n)
				print_nat(nat + n);
			ub->entries = ub->size / sizeof(struct pf_rdr);
			if (ioctl(dev, DIOCGETRDR, ub))
				printerror("DIOCGETRDR");
			for (n = 0; n < ub->entries; ++n)
				print_rdr(rdr + n);
		}
		else if (!strcmp(argv[2], "states")) {
			u_int8_t proto = 0;
			struct pf_state *state = ub->buffer;

			if (argc >= 4) {
				if (!strcmp(argv[3], "tcp"))
					proto = IPPROTO_TCP;
				else if (!strcmp(argv[3], "udp"))
					proto = IPPROTO_UDP;
				else if (!strcmp(argv[3], "icmp"))
					proto = IPPROTO_ICMP;
				else {
					close(dev);
					usage(argv[0]);
					return (1);
				}
			}
			ub->entries = ub->size / sizeof(struct pf_state);
			if (ioctl(dev, DIOCGETSTATES, ub))
				printerror("DIOCGETSTATES");
			for (n = ub->entries; n > 0; --n)
				if (!proto || (state[n - 1].proto == proto))
					print_state(state + n - 1);
		}
		else if (!strcmp(argv[2], "status")) {
			struct pf_status *status = ub->buffer;

			ub->entries = 1;
			if (ioctl(dev, DIOCGETSTATUS, ub))
				printerror("DIOCGETSTATUS");
			print_status(status);
		}
		else {
			close(dev);
			usage(argv[0]);
			return (1);
		}
	}
	else if (!strcmp(argv[1], "clear")) {
		if (argc < 3) {
			close(dev);
			usage(argv[0]);
			return (1);
		}
		ub->entries = 0;
		if (!strcmp(argv[2], "rules")) {
			if (ioctl(dev, DIOCSETRULES, ub))
				printerror("DIOCSETRULES");
			else printf("rules cleared\n");
		}
		else if (!strcmp(argv[2], "nat")) {
			if (ioctl(dev, DIOCSETNAT, ub))
				printerror("DIOCSETNAT");
			else if (ioctl(dev, DIOCSETRDR, ub))
				printerror("DIOCSETRDR");
			else printf("nat cleared\n");
		}
		else if (!strcmp(argv[2], "states")) {
			if (ioctl(dev, DIOCCLRSTATES))
				printerror("DIOCCLRSTATES");
			else
				printf("states cleared\n");
		}
		else {
			close(dev);
			usage(argv[0]);
			return (1);
		}
	}
	else if (!strcmp(argv[1], "log")) {
		if (argc < 3) {
			close(dev);
			usage(argv[0]);
			return (1);
		}
		strncpy(ub->buffer, argv[2], 16);
		if (ioctl(dev, DIOCSETSTATUSIF, ub))
			printerror("DIOCSETSTATUSIF");
		else
			printf("now logging %s\n", argv[2]);
	}
	else if (!strcmp(argv[1], "parse") || !strcmp(argv[1], "load")) {
		int load = !strcmp(argv[1], "load");
		char *buf, *s;
		size_t len;
		unsigned nr = 0;
		if ((argc < 4) || (strcmp(argv[2], "nat") &&
		    strcmp(argv[2], "rules"))) {
			close(dev);
			usage(argv[0]);
			return (1);
		}
		buf = load_file(argv[3], &len);
		if (buf == NULL)
			return (1);

		if (!strcmp(argv[2], "rules")) {
			struct pf_rule *rule = ub->buffer;

			n = 0;
			nr = 0;
			s = buf;
			do {
				char *line = next_line(&s);
				nr++;
				if (*line && (*line != '#'))
					if (parse_rule(nr, line, rule + n))
						n++;
			} while (s < (buf + len));
			ub->entries = n;
			if (load) {
				if (ioctl(dev, DIOCSETRULES, ub))
					printerror("DIOCSETRULES");
				else
					printf("%u rules loaded\n",
					    ub->entries);
			} else
				for (n = 0; n < ub->entries; ++n)
					print_rule(rule + n);
		} else {
			struct pf_nat *nat = ub->buffer;
			struct pf_rdr *rdr = ub->buffer;

			n = 0;
			nr = 0;
			s = buf;
			do {
				char *line = next_line(&s);
				nr++;
				if (*line && (*line == 'n'))
					if (parse_nat(nr, line, nat + n))
						n++;
			} while (s < (buf + len));
			ub->entries = n;
			if (load) {
				if (ioctl(dev, DIOCSETNAT, ub))
					printerror("DIOCSETNAT");
				else
					printf("%u nat entries loaded\n",
					    ub->entries);
			} else
				for (n = 0; n < ub->entries; ++n)
					print_nat(nat + n);
			free(buf);
			buf = load_file(argv[3], &len);
			if (buf == NULL)
				return (1);
			n = 0;
			nr = 0;
			s = buf;
			do {
				char *line = next_line(&s);
				nr++;
				if (*line && (*line == 'r'))
					if (parse_rdr(nr, line, rdr + n))
						n++;
			} while (s < (buf + len));
			ub->entries = n;
			if (load) {
				if (ioctl(dev, DIOCSETRDR, ub))
					printerror("DIOCSETRDR");
				else
					printf("%u rdr entries loaded\n",
					    ub->entries);
			} else
				for (n = 0; n < ub->entries; ++n)
					print_rdr(rdr + n);
		}

		free(buf);
	}
	else {
		close(dev);
		usage(argv[0]);
		return (1);
	}
	close(dev);
	free(ub->buffer);
	free(ub);
	return (0);
}

