/*
 * (C)opyright 1993,1994,1995 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#include <stdio.h>
#include <string.h>
#if !defined(__SVR4) && !defined(__svr4__)
#include <strings.h>
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>
#include <nlist.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <net/if.h>
#include <netinet/ip_fil.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include "ipf.h"
#include "kmem.h"
#ifdef	__NetBSD__
#include <paths.h>
#endif

#ifndef	lint
static	char	sccsid[] = "@(#)fils.c	1.15 11/11/95 (C) 1993 Darren Reed";
#endif
#ifdef	_PATH_UNIX
#define	VMUNIX	_PATH_UNIX
#else
#define	VMUNIX	"/vmunix"
#endif

extern	char	*optarg;
#define	F_ST	0
#define	F_IN	1
#define	F_OUT	2
#define	F_FL	3

int	opts = 0;

static	void	showstats();
static	void	showlist();

int main(argc,argv)
int argc;
char *argv[];
{
	struct	friostat	fio;
	char	c, *name = NULL, *device = IPL_NAME;
	int	fd;

	if (openkmem() == -1)
		exit(-1);

	(void)setuid(getuid());
	(void)setgid(getgid());

	while ((c = getopt(argc, argv, "hIiovd:")) != -1)
	{
		switch (c)
		{
		case 'd' :
			device = optarg;
			break;
		case 'h' :
			opts |= OPT_HITS;
			break;
		case 'i' :
			opts |= OPT_INQUE|OPT_SHOWLIST;
			break;
		case 'I' :
			opts |= OPT_INACTIVE;
			break;
		case 'o' :
			opts |= OPT_OUTQUE|OPT_SHOWLIST;
			break;
		case 'v' :
			opts |= OPT_VERBOSE;
			break;
		}
	}

	if ((fd = open(device, O_RDONLY)) < 0) {
		perror("open");
		exit(-1);
	}
	bzero((char *)&fio, sizeof(fio));
	if (ioctl(fd, SIOCGETFS, &fio) == -1) {
		perror("ioctl(SIOCGETFS)");
		exit(-1);
	}

	if (opts & OPT_VERBOSE)
		printf("opts %#x name %s\n", opts, name ? name : "<>");
	if (opts & OPT_SHOWLIST){
		showlist(&fio);
		if((opts & OPT_OUTQUE) && (opts & OPT_INQUE)){
			opts &= ~OPT_OUTQUE;
			showlist(&fio);
		}
	}
	else
		showstats(fd, &fio);
	return 0;
}


/*
 * read the kernel stats for packets blocked and passed
 */
static	void	showstats(fd, fp)
int	fd;
struct	friostat	*fp;
{
	int	frf = 0;

	if (ioctl(fd, SIOCGETFF, &frf) == -1)
		perror("ioctl(SIOCGETFF)");

#if SOLARIS
	(void)printf("dropped packets:\tin %ld\tout %ld\n",
			fp->f_st[0].fr_drop, fp->f_st[1].fr_drop);
	(void)printf("non-ip packets:\t\tin %ld\tout %ld\n",
			fp->f_st[0].fr_notip, fp->f_st[1].fr_notip);
	(void)printf("   bad packets:\t\tin %ld\tout %ld\n",
			fp->f_st[0].fr_bad, fp->f_st[1].fr_bad);
#endif
	(void)printf(" input packets:\t\tblocked %ld passed %ld nomatch %ld\n",
			fp->f_st[0].fr_block, fp->f_st[0].fr_pass,
			fp->f_st[0].fr_nom);
	(void)printf("output packets:\t\tblocked %ld passed %ld nomatch %ld\n",
			fp->f_st[1].fr_block, fp->f_st[1].fr_pass,
			fp->f_st[1].fr_nom);
	(void)printf(" input packets logged:\tblocked %ld passed %ld\n",
			fp->f_st[0].fr_bpkl, fp->f_st[0].fr_ppkl);
	(void)printf("output packets logged:\tblocked %ld passed %ld\n",
			fp->f_st[1].fr_bpkl, fp->f_st[1].fr_ppkl);
	(void)printf(" packets logged:\tinput %ld-%ld output %ld-%ld\n",
			fp->f_st[0].fr_pkl, fp->f_st[0].fr_skip,
			fp->f_st[1].fr_pkl, fp->f_st[1].fr_skip);
	(void)printf("ICMP replies:\t%ld\tTCP RSTs sent:\t%ld\n",
			fp->f_st[0].fr_ret, fp->f_st[1].fr_ret);

	(void)printf("Packet log flags set: (%#x)\n", frf);
	if (frf & FF_LOGPASS)
		printf("\tpackets passed through filter\n");
	if (frf & FF_LOGBLOCK)
		printf("\tpackets blocked by filter\n");
	if (!frf)
		printf("\tnone\n");
}

/*
 * print out filter rule list
 */
static	void	showlist(fiop)
struct	friostat	*fiop;
{
	struct	frentry	fb;
	struct	frentry	*fp = NULL;
	int	i, set;

	if (opts & OPT_OUTQUE)
		i = F_OUT;
	else if (opts & OPT_INQUE)
		i = F_IN;
	else
		return;
	set = fiop->f_active;
	if (opts & OPT_INACTIVE)
		set = 1 - set;
	fp = (i == F_IN) ? (struct frentry *)fiop->f_fin[set] :
			   (struct frentry *)fiop->f_fout[set];
	if (opts & OPT_VERBOSE)
		(void)fprintf(stderr, "showlist:opts %#x i %d\n", opts, i);

	if (opts & OPT_VERBOSE)
		printf("fp %#x set %d\n", (u_int)fp, set);
	if (!fp) {
		(void)fprintf(stderr, "empty list for filter%s\n",
			(i == F_IN) ? "in" : "out");
		return;
	}
	while (fp) {
		if (kmemcpy((char *)&fb, (u_long)fp, sizeof(fb)) == -1) {
			perror("kmemcpy");
			return;
		}
		fp = &fb;
		if (opts & OPT_OUTQUE)
			fp->fr_flags |= FR_OUTQUE;
		if (opts & (OPT_HITS|OPT_VERBOSE))
			printf("%d ", fp->fr_hits);
		printfr(fp);
		if (opts & OPT_VERBOSE)
			binprint(fp);
		fp = fp->fr_next;
	}
}
