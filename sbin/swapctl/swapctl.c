/*	$OpenBSD: swapctl.c,v 1.2 2000/02/26 04:06:23 hugh Exp $	*/
/*	$NetBSD: swapctl.c,v 1.9 1998/07/26 20:23:15 mycroft Exp $	*/

/*
 * Copyright (c) 1996, 1997 Matthew R. Green
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * swapctl command:
 *	-A		add all devices listed as `sw' in /etc/fstab
 *	-t [blk|noblk]	if -A, add either all block device or all non-block
 *			devices
 *	-a <dev>	add this device
 *	-d <dev>	remove this swap device (not supported yet)
 *	-l		list swap devices
 *	-s		short listing of swap devices
 *	-k		use kilobytes
 *	-p <pri>	use this priority
 *	-c		change priority
 *
 * or, if invoked as "swapon" (compatibility mode):
 *
 *	-a		all devices listed as `sw' in /etc/fstab
 *	-t		same as -t above (feature not present in old
 *			swapon(8) command)
 *	<dev>		add this device
 */

#include <sys/param.h>
#include <sys/stat.h>

#include <sys/swap.h>

#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fstab.h>

#include "swapctl.h"

int	command;

/*
 * Commands for swapctl(8).  These are mutually exclusive.
 */
#define	CMD_A		0x01	/* process /etc/fstab */
#define	CMD_a		0x02	/* add a swap file/device */
#define	CMD_c		0x04	/* change priority of a swap file/device */
#define	CMD_d		0x08	/* delete a swap file/device */
#define	CMD_l		0x10	/* list swap files/devices */
#define	CMD_s		0x20	/* summary of swap files/devices */

#define	SET_COMMAND(cmd) \
do { \
	if (command) \
		usage(); \
	command = (cmd); \
} while (0)

/*
 * Commands that require a "path" argument at the end of the command
 * line, and the ones which require that none exist.
 */
#define	REQUIRE_PATH	(CMD_a | CMD_c | CMD_d)
#define	REQUIRE_NOPATH	(CMD_A | CMD_l | CMD_s)

/*
 * Option flags, and the commands with which they are valid.
 */
int	kflag;		/* display in 1K blocks */
#define	KFLAG_CMDS	(CMD_l | CMD_s)

int	pflag;		/* priority was specified */
#define	PFLAG_CMDS	(CMD_A | CMD_a | CMD_c)

char	*tflag;		/* swap device type (blk or noblk) */
#define	TFLAG_CMDS	(CMD_A)

int	pri;		/* uses 0 as default pri */

static	void change_priority __P((char *));
static	void add_swap __P((char *));
static	void del_swap __P((char *));
	int  main __P((int, char *[]));
static	void do_fstab __P((void));
static	void usage __P((void));
static	void swapon_command __P((int, char **));
#if 0
static	void swapoff_command __P((int, char **));
#endif

extern	char *__progname;	/* from crt0.o */

int
main(argc, argv)
	int	argc;
	char	*argv[];
{
	int	c;

	if (strcmp(__progname, "swapon") == 0) {
		swapon_command(argc, argv);
		/* NOTREACHED */
	}

#if 0
	if (strcmp(__progname, "swapoff") == 0) {
		swapoff_command(argc, argv);
		/* NOTREACHED */
	}
#endif

	while ((c = getopt(argc, argv, "Aacdlkp:st:")) != -1) {
		switch (c) {
		case 'A':
			SET_COMMAND(CMD_A);
			break;

		case 'a':
			SET_COMMAND(CMD_a);
			break;

		case 'c':
			SET_COMMAND(CMD_c);
			break;

		case 'd':
			SET_COMMAND(CMD_d);
			break;

		case 'l':
			SET_COMMAND(CMD_l);
			break;

		case 'k':
			kflag = 1;
			break;

		case 'p':
			pflag = 1;
			/* XXX strtol() */
			pri = atoi(optarg);
			break;

		case 's':
			SET_COMMAND(CMD_s);
			break;

		case 't':
			if (tflag != NULL)
				usage();
			tflag = optarg;
			break;

		default:
			usage();
			/* NOTREACHED */
		}
	}

	/* Did the user specify a command? */
	if (command == 0)
		usage();

	argv += optind;
	argc -= optind;

	switch (argc) {
	case 0:
		if (command & REQUIRE_PATH)
			usage();
		break;

	case 1:
		if (command & REQUIRE_NOPATH)
			usage();
		break;

	default:
		usage();
	}

	/* To change priority, you have to specify one. */
	if ((command == CMD_c) && pflag == 0)
		usage();

	/* Sanity-check -t */
	if (tflag != NULL) {
		if (command != CMD_A)
			usage();
		if (strcmp(tflag, "blk") != 0 &&
		    strcmp(tflag, "noblk") != 0)
			usage();
	}

	/* Dispatch the command. */
	switch (command) {
	case CMD_l:
		list_swap(pri, kflag, pflag, 0, 1);
		break;

	case CMD_s:
		list_swap(pri, kflag, pflag, 0, 0);
		break;

	case CMD_c:
		change_priority(argv[0]);
		break;

	case CMD_a:
		add_swap(argv[0]);
		break;

	case CMD_d:
		del_swap(argv[0]);
		break;

	case CMD_A:
		do_fstab();
		break;
	}

	exit(0);
}

/*
 * swapon_command: emulate the old swapon(8) program.
 */
void
swapon_command(argc, argv)
	int argc;
	char **argv;
{
	int ch, fiztab = 0;

	while ((ch = getopt(argc, argv, "at:")) != -1) {
		switch (ch) {
		case 'a':
			fiztab = 1;
			break;
		case 't':
			if (tflag != NULL)
				usage();
			tflag = optarg;
			break;
		default:
			goto swapon_usage;
		}
	}
	argc -= optind;
	argv += optind;

	if (fiztab) {
		if (argc)
			goto swapon_usage;
		/* Sanity-check -t */
		if (tflag != NULL) {
			if (strcmp(tflag, "blk") != 0 &&
			    strcmp(tflag, "noblk") != 0)
				usage();
		}
		do_fstab();
		exit(0);
	} else if (argc == 0 || tflag != NULL)
		goto swapon_usage;

	while (argc) {
		add_swap(argv[0]);
		argc--;
		argv++;
	}
	exit(0);
	/* NOTREACHED */

 swapon_usage:
	fprintf(stderr, "usage: %s -a [-t blk|noblk]\n", __progname);
	fprintf(stderr, "       %s <path> ...\n", __progname);
	exit(1);
}

/*
 * change_priority:  change the priority of a swap device.
 */
void
change_priority(path)
	char	*path;
{

	if (swapctl(SWAP_CTL, path, pri) < 0)
		warn("%s", path);
}

/*
 * add_swap:  add the pathname to the list of swap devices.
 */
void
add_swap(path)
	char *path;
{

	if (swapctl(SWAP_ON, path, pri) < 0)
		err(1, "%s", path);
}

/*
 * del_swap:  remove the pathname to the list of swap devices.
 */
void
del_swap(path)
	char *path;
{

	if (swapctl(SWAP_OFF, path, pri) < 0)
		err(1, "%s", path);
}

void
do_fstab()
{
	struct	fstab *fp;
	char	*s;
	long	priority;
	struct	stat st;
	int	isblk;
	int	gotone = 0;

#define PRIORITYEQ	"priority="
#define NFSMNTPT	"nfsmntpt="
#define PATH_MOUNT	"/sbin/mount_nfs"
	while ((fp = getfsent()) != NULL) {
		const char *spec;

		if (strcmp(fp->fs_type, "sw") != 0)
			continue;

		spec = fp->fs_spec;
		isblk = 0;

		if ((s = strstr(fp->fs_mntops, PRIORITYEQ)) != NULL) {
			s += sizeof(PRIORITYEQ) - 1;
			priority = atol(s);
		} else
			priority = pri;

		if ((s = strstr(fp->fs_mntops, NFSMNTPT)) != NULL) {
			char *t, cmd[2*PATH_MAX+sizeof(PATH_MOUNT)+2];

			/*
			 * Skip this song and dance if we're only
			 * doing block devices.
			 */
			if (tflag != NULL &&
			    strcmp(tflag, "blk") == 0)
				continue;

			t = strpbrk(s, ",");
			if (t != 0)
				*t = '\0';
			spec = strdup(s + strlen(NFSMNTPT));
			if (t != 0)
				*t = ',';

			if (spec == NULL)
				errx(1, "Out of memory");

			if (strlen(spec) == 0) {
				warnx("empty mountpoint");
				free((char *)spec);
				continue;
			}
			snprintf(cmd, sizeof(cmd), "%s %s %s",
				PATH_MOUNT, fp->fs_spec, spec);
			if (system(cmd) != 0) {
				warnx("%s: mount failed", fp->fs_spec);
				continue;
			}
		} else {
			/*
			 * Determine blk-ness.
			 */
			if (stat(spec, &st) < 0) {
				warn(spec);
				continue;
			}
			if (S_ISBLK(st.st_mode))
				isblk = 1;
		}

		/*
		 * Skip this type if we're told to.
		 */
		if (tflag != NULL) {
			if (strcmp(tflag, "blk") == 0 && isblk == 0)
				continue;
			if (strcmp(tflag, "noblk") == 0 && isblk == 1)
				continue;
		}

		if (swapctl(SWAP_ON, spec, (int)priority) < 0)
			warn("%s", spec);
		else {
			gotone = 1;
			printf("%s: adding %s as swap device at priority %d\n",
			    __progname, fp->fs_spec, (int)priority);
		}

		if (spec != fp->fs_spec)
			free((char *)spec);
	}
	if (gotone == 0)
		exit(1);
}

void
usage()
{

	fprintf(stderr, "usage: %s -A [-p priority] [-t blk|noblk]\n",
	    __progname);
	fprintf(stderr, "       %s -a [-p priority] path\n", __progname);
	fprintf(stderr, "       %s -c -p priority path\n", __progname);
	fprintf(stderr, "       %s -d path\n", __progname);
	fprintf(stderr, "       %s -l | -s [-k]\n", __progname);
	exit(1);
}
