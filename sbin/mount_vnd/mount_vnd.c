/*	$OpenBSD: mount_vnd.c,v 1.16 2014/10/29 21:30:10 tedu Exp $	*/
/*
 * Copyright (c) 1993 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: vnconfig.c 1.1 93/12/15$
 *
 *	@(#)vnconfig.c	8.1 (Berkeley) 12/15/93
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/disklabel.h>

#include <dev/vndioctl.h>

#include <blf.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <readpassphrase.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#define DEFAULT_VND	"vnd0"

#define VND_CONFIG	1
#define VND_UNCONFIG	2
#define VND_GET		3

int verbose = 0;
int run_mount_vnd = 0;

__dead void	 usage(void);
int		 config(char *, char *, int, struct disklabel *, char *,
		     size_t);
int		 getinfo(const char *);
char		*get_pkcs_key(char *, char *);

int
main(int argc, char **argv)
{
	int	 ch, rv, action, opt_c, opt_k, opt_K, opt_l, opt_u;
	char	*key, *mntopts, *rounds, *saltopt;
	size_t	 keylen = 0;
	extern char *__progname;
	struct disklabel *dp = NULL;

	if (strcasecmp(__progname, "mount_vnd") == 0)
		run_mount_vnd = 1;

	opt_c = opt_k = opt_K = opt_l = opt_u = 0;
	key = mntopts = rounds = saltopt = NULL;
	action = VND_CONFIG;

	while ((ch = getopt(argc, argv, "ckK:lo:S:t:uv")) != -1) {
		switch (ch) {
		case 'c':
			opt_c = 1;
			break;
		case 'k':
			opt_k = 1;
			break;
		case 'K':
			opt_K = 1;
			rounds = optarg;
			break;
		case 'l':
			opt_l = 1;
			break;
		case 'o':
			mntopts = optarg;
			break;
		case 'S':
			saltopt = optarg;
			break;
		case 't':
			dp = getdiskbyname(optarg);
			if (dp == NULL)
				errx(1, "unknown disk type: %s", optarg);
			break;
		case 'u':
			opt_u = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (opt_c + opt_l + opt_u > 1)
		errx(1, "-c, -l and -u are mutually exclusive options");

	if (opt_l)
		action = VND_GET;
	else if (opt_u)
		action = VND_UNCONFIG;
	else
		action = VND_CONFIG;	/* default behavior */

	if (saltopt && (!opt_K))
		errx(1, "-S only makes sense when used with -K");

	if (action == VND_CONFIG && argc == 2) {
		int ind_raw, ind_reg;

		if (opt_k || opt_K) {
			fprintf(stderr,
			    "WARNING: Consider using softraid crypto.\n");
		}
		if (opt_k) {
			if (opt_K)
				errx(1, "-k and -K are mutually exclusive");
			key = getpass("Encryption key: ");
			if (key == NULL || (keylen = strlen(key)) == 0)
				errx(1, "Need an encryption key");
		} else if (opt_K) {
			key = get_pkcs_key(rounds, saltopt);
			keylen = BLF_MAXUTILIZED;
		}

		/* fix order of arguments. */
		if (run_mount_vnd) {
			ind_raw = 1;
			ind_reg = 0;
		} else {
			ind_raw = 0;
			ind_reg = 1;
		}
		rv = config(argv[ind_raw], argv[ind_reg], action, dp, key,
		    keylen);
	} else if (action == VND_UNCONFIG && argc == 1)
		rv = config(argv[0], NULL, action, NULL, NULL, 0);
	else if (action == VND_GET)
		rv = getinfo(argc ? argv[0] : NULL);
	else
		usage();

	exit(rv);
}

char *
get_pkcs_key(char *arg, char *saltopt)
{
	char		 passphrase[128];
	char		 saltbuf[128], saltfilebuf[PATH_MAX];
	char		*key = NULL;
	char		*saltfile;
	const char	*errstr;
	int		 rounds;

	rounds = strtonum(arg, 1000, INT_MAX, &errstr);
	if (errstr)
		err(1, "rounds: %s", errstr);
	bzero(passphrase, sizeof(passphrase));
	if (readpassphrase("Encryption key: ", passphrase, sizeof(passphrase),
	    RPP_REQUIRE_TTY) == NULL)
		errx(1, "Unable to read passphrase");
	if (saltopt)
		saltfile = saltopt;
	else {
		printf("Salt file: ");
		fflush(stdout);
		saltfile = fgets(saltfilebuf, sizeof(saltfilebuf), stdin);
		if (saltfile)
			saltfile[strcspn(saltfile, "\n")] = '\0';
	}
	if (!saltfile || saltfile[0] == '\0') {
		warnx("Skipping salt file, insecure");
		memset(saltbuf, 0, sizeof(saltbuf));
	} else {
		int fd;

		fd = open(saltfile, O_RDONLY);
		if (fd == -1) {
			int *s;

			fprintf(stderr, "Salt file not found, attempting to "
			    "create one\n");
			fd = open(saltfile, O_RDWR|O_CREAT|O_EXCL, 0600);
			if (fd == -1)
				err(1, "Unable to create salt file: '%s'",
				    saltfile);
			for (s = (int *)saltbuf;
			    s < (int *)(saltbuf + sizeof(saltbuf)); s++)
				*s = arc4random();
			if (write(fd, saltbuf, sizeof(saltbuf))
			    != sizeof(saltbuf))
				err(1, "Unable to write salt file: '%s'",
				    saltfile);
			fprintf(stderr, "Salt file created as '%s'\n",
			    saltfile);
		} else {
			if (read(fd, saltbuf, sizeof(saltbuf))
			    != sizeof(saltbuf))
				err(1, "Unable to read salt file: '%s'",
				    saltfile);
		}
		close(fd);
	}
	if ((key = calloc(1, BLF_MAXUTILIZED)) == NULL)
		err(1, NULL);
	if (pkcs5_pbkdf2(passphrase, sizeof(passphrase), saltbuf,
	    sizeof (saltbuf), key, BLF_MAXUTILIZED, rounds))
		errx(1, "pkcs5_pbkdf2 failed");
	memset(passphrase, 0, sizeof(passphrase));

	return (key);
}

int
getinfo(const char *vname)
{
	int vd, print_all = 0;
	struct vnd_user vnu;

	if (vname == NULL) {
		vname = DEFAULT_VND;
		print_all = 1;
	}

	vd = opendev((char *)vname, O_RDONLY, OPENDEV_PART, NULL);
	if (vd < 0)
		err(1, "open: %s", vname);

	vnu.vnu_unit = -1;

query:
	if (ioctl(vd, VNDIOCGET, &vnu) == -1) {
		if (print_all && errno == ENXIO && vnu.vnu_unit > 0) {
			close(vd);
			return (0);
		} else {
			err(1, "ioctl: %s", vname);
		}
	}

	fprintf(stdout, "vnd%d: ", vnu.vnu_unit);

	if (!vnu.vnu_ino)
		fprintf(stdout, "not in use\n");
	else
		fprintf(stdout, "covering %s on %s, inode %llu\n",
		    vnu.vnu_file, devname(vnu.vnu_dev, S_IFBLK),
		    (unsigned long long)vnu.vnu_ino);

	if (print_all) {
		vnu.vnu_unit++;
		goto query;
	}

	close(vd);

	return (0);
}

int
config(char *dev, char *file, int action, struct disklabel *dp, char *key,
    size_t keylen)
{
	struct vnd_ioctl vndio;
	char *rdev;
	int fd, rv = -1;

	if ((fd = opendev(dev, O_RDONLY, OPENDEV_PART, &rdev)) < 0) {
		err(4, "%s", rdev);
		goto out;
	}

	vndio.vnd_file = file;
	vndio.vnd_secsize = (dp && dp->d_secsize) ? dp->d_secsize : DEV_BSIZE;
	vndio.vnd_nsectors = (dp && dp->d_nsectors) ? dp->d_nsectors : 100;
	vndio.vnd_ntracks = (dp && dp->d_ntracks) ? dp->d_ntracks : 1;
	vndio.vnd_key = (u_char *)key;
	vndio.vnd_keylen = keylen;

	/*
	 * Clear (un-configure) the device
	 */
	if (action == VND_UNCONFIG) {
		rv = ioctl(fd, VNDIOCCLR, &vndio);
		if (rv)
			warn("VNDIOCCLR");
		else if (verbose)
			printf("%s: cleared\n", dev);
	}
	/*
	 * Configure the device
	 */
	if (action == VND_CONFIG) {
		rv = ioctl(fd, VNDIOCSET, &vndio);
		if (rv)
			warn("VNDIOCSET");
		else if (verbose)
			printf("%s: %llu bytes on %s\n", dev, vndio.vnd_size,
			    file);
	}

	close(fd);
	fflush(stdout);
 out:
	if (key)
		memset(key, 0, keylen);
	return (rv < 0);
}

__dead void
usage(void)
{
	extern char *__progname;

	if (run_mount_vnd)
		(void)fprintf(stderr,
		    "usage: mount_vnd [-k] [-K rounds] [-o options] "
		    "[-S saltfile] [-t disktype]\n"
		    "\t\t image vnd_dev\n");
	else
		(void)fprintf(stderr,
		    "usage: %s [-ckluv] [-K rounds] [-S saltfile] "
		    "[-t disktype] vnd_dev image\n", __progname);

	exit(1);
}
