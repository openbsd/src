/*
 * (C)opyright 1993,1994,1995 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#if !defined(__SVR4) && !defined(__GNUC__)
#include <strings.h>
#endif
#if !defined(__SVR4) && defined(__GNUC__)
extern	char	*index();
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <netinet/ip_fil.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include "ipf.h"

#ifndef	lint
static	char	sccsid[] = "@(#)ipf.c	1.18 11/11/95 (C) 1993-1995 Darren Reed";
#endif

extern	char	*optarg;

int	opts = 0;

static	int	fd = -1;
static	void	procfile(), flushfilter(), set_state();
static	void	packetlogon(), swapactive();

int main(argc,argv)
int argc;
char *argv[];
{
	char	c;

	if ((fd = open(IPL_NAME, O_RDONLY)) == -1)
		perror("open device");

	while ((c = getopt(argc, argv, "AsInovdryf:F:l:EDZ")) != -1)
		switch (c)
		{
		case 'E' :
			set_state(1);
			break;
		case 'D' :
			set_state(0);
			break;
		case 'A' :
			opts &= ~OPT_INACTIVE;
			break;
		case 'd' :
			opts |= OPT_DEBUG;
			break;
		case 'f' :
			procfile(optarg);
			break;
		case 'F' :
			flushfilter(optarg);
			break;
		case 'I' :
			opts |= OPT_INACTIVE;
			break;
		case 'l' :
			packetlogon(optarg);
			break;
		case 'n' :
			opts |= OPT_DONOTHING;
			break;
		case 'o' :
			opts |= OPT_OUTQUE;
			break;
		case 'r' :
			opts |= OPT_REMOVE;
			break;
		case 's' :
			swapactive();
			break;
		case 'v' :
			opts |= OPT_VERBOSE;
			break;
#if defined(sun) && (defined(__SVR4) || defined(__svr4__))
		case 'y' :
			frsync();
			break;
#endif
		case 'Z' :
			zerostats();
			break;
		}

	if (fd != -1)
		(void) close(fd);
	return 0;
}

static	void	set_state(enable)
u_int	enable;
{
	if (ioctl(fd, SIOCFRENB, &enable) == -1)
		perror("SIOCFRENB");
	return;
}

static	void	procfile(file)
char	*file;
{
	FILE	*fp;
	char	line[513], *s;
	struct	frentry	*fr;
	u_int	add = SIOCADAFR, del = SIOCRMAFR;

	if (opts & OPT_INACTIVE) {
		add = SIOCADIFR;
		del = SIOCRMIFR;
	}
	if (opts & OPT_DEBUG)
		printf("add %x del %x\n", add, del);

	if (!strcmp(file, "-"))
		fp = stdin;
	else if (!(fp = fopen(file, "r")))
		return;

	while (fgets(line, sizeof(line)-1, fp)) {
		/*
		 * treat both CR and LF as EOL
		 */
		if ((s = index(line, '\n')))
			*s = '\0';
		if ((s = index(line, '\r')))
			*s = '\0';
		/*
		 * # is comment marker, everything after is a ignored
		 */
		if ((s = index(line, '#')))
			*s = '\0';

		if (!*line)
			continue;

		if (opts & OPT_VERBOSE)
			(void)fprintf(stderr, "[%s]\n",line);

		fr = parse(line);
		(void)fflush(stdout);

		if (fr) {
			if (opts & OPT_INACTIVE)
				add = fr->fr_hits ? SIOCINIFR : SIOCADIFR;
			else
				add = fr->fr_hits ? SIOCINAFR : SIOCADAFR;
			if (fr->fr_hits)
				fr->fr_hits--;
			if (fr && (opts & OPT_VERBOSE))
				printfr(fr);
			if (fr && (opts & OPT_OUTQUE))
				fr->fr_flags |= FR_OUTQUE;

			if (opts & OPT_DEBUG)
				binprint(fr);
				
			if ((opts & OPT_REMOVE) && !(opts & OPT_DONOTHING)) {
				if (ioctl(fd, del, fr) == -1)
					perror("ioctl(SIOCDELFR)");
			} else if (!(opts & OPT_DONOTHING)) {
				if (ioctl(fd, add, fr) == -1)
					perror("ioctl(SIOCADDFR)");
			}
		}
	}
	(void)fclose(fp);
}


static void packetlogon(opt)
char	*opt;
{
	int	err, flag;

	if (opts & OPT_VERBOSE) {
		if ((err = ioctl(fd, SIOCGETFF, &flag)))
			perror("ioctl(SIOCGETFF)");

		printf("log flag is currently %#x\n", flag);
	}

	flag = 0;

	if (index(opt, 'p')) {
		flag |= FF_LOGPASS;
		if (opts & OPT_VERBOSE)
			printf("set log flag: pass\n");
	}
	if (index(opt, 'b') || index(opt, 'd')) {
		flag |= FF_LOGBLOCK;
		if (opts & OPT_VERBOSE)
			printf("set log flag: block\n");
	}

	if (!(opts & OPT_DONOTHING) &&
	    (err = ioctl(fd, SIOCSETFF, &flag)))
		perror("ioctl(SIOCSETFF)");

	if (opts & OPT_VERBOSE) {
		if ((err = ioctl(fd, SIOCGETFF, &flag)))
			perror("ioctl(SIOCGETFF)");

		printf("log flag is now %#x\n", flag);
	}
}


static	void	flushfilter(arg)
char	*arg;
{
	int	fl = 0, rem;

	if (!arg || !*arg)
		return;
	if (*arg == 'i' || *arg == 'I')
		fl = FR_INQUE;
	else if (*arg == 'o' || *arg == 'O')
		fl = FR_OUTQUE;
	else if (*arg == 'a' || *arg == 'A')
		fl = FR_OUTQUE|FR_INQUE;
	fl |= (opts & FR_INACTIVE);
	rem = fl;

	if (!(opts & OPT_DONOTHING) && ioctl(fd, SIOCIPFFL, &fl) == -1)
		perror("ioctl(SIOCIPFFL)");
	if (opts & OPT_VERBOSE){
		printf("remove flags %s%s (%d)\n", (rem & FR_INQUE) ? "I" : "",
			(rem & FR_OUTQUE) ? "O" : "", rem);
		printf("removed %d filter rules\n", fl);
	}
	return;
}


static void swapactive()
{
	int in = 2;

	if (ioctl(fd, SIOCSWAPA, &in) == -1)
		perror("ioctl(SIOCSWAPA)");
	else
		printf("Set %d now inactive\n", in);
}


#if defined(sun) && (defined(__SVR4) || defined(__svr4__))
frsync()
{
	if (ioctl(fd, SIOCFRSYN, 0) == -1)
		perror("SIOCFRSYN");
	else
		printf("filter sync'd\n");
}
#endif


zerostats()
{
	struct	friostat	fio;

	if (ioctl(fd, SIOCFRZST, &fio) == -1) {
		perror("ioctl(SIOCFRZST)");
		exit(-1);
	}

}
