/*	$OpenBSD: sdio.c,v 1.4 2007/05/27 04:11:28 jmc Exp $	*/

/*
 * Copyright (c) 2006 Uwe Stuehler <uwe@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/limits.h>
#include <sys/param.h>

#include <dev/biovar.h>

#include <dev/sdmmc/sdmmcvar.h>
#include <dev/sdmmc/sdmmc_ioreg.h>

#define DEV_BIO "/dev/bio"
#define DEVNAME "sdmmc0"

struct sdio_hdl {
	int	 bio;
	char	*devname;
	void	*cookie;
};

#define SDIO_ADDR_MAX 131072

static int	sdio_open(const char *, struct sdio_hdl **);
static void	sdio_close(struct sdio_hdl *);
static int	sdio_debug(struct sdio_hdl *, int);
static int	sdio_exec(struct sdio_hdl *, struct sdmmc_command *);
static int	sdio_command(struct sdio_hdl *, int, int, char *[]);

static int	strtoint(const char *, const char **);
static int	strtorsp(const char *, const char **);
static void	usage(void) __dead;

static int
sdio_open(const char *name, struct sdio_hdl **hdl)
{
	struct bio_locate bl;

	*hdl = malloc(sizeof **hdl);
	if (*hdl == NULL)
		err(1, NULL);

	(*hdl)->bio = open(DEV_BIO, O_RDWR);
	if ((*hdl)->bio == -1) {
		warn("unable to open %s", DEV_BIO);
		return -1;
	}

	bzero(&bl, sizeof bl);
	bl.bl_name = DEVNAME;

	if (ioctl((*hdl)->bio, BIOCLOCATE, &bl) == -1) {
		warn("unable to locate %s", bl.bl_name);
		return -1;
	}

	(*hdl)->cookie = bl.bl_cookie;
	return 0;
}

static void
sdio_close(struct sdio_hdl *hdl)
{
	(void)close(hdl->bio);
	free(hdl);
}

static int
sdio_debug(struct sdio_hdl *hdl, int debug)
{
	struct bio_sdmmc_debug bctl;

	bctl.cookie = hdl->cookie;
	bctl.debug = debug;

	return ioctl(hdl->bio, SDIOCSETDEBUG, &bctl);
}

static int
sdio_exec(struct sdio_hdl *hdl, struct sdmmc_command *cmd)
{
	struct bio_sdmmc_command bcmd;

	bcopy(cmd, &bcmd.cmd, sizeof *cmd);
	bcmd.cookie = hdl->cookie;

	if (ioctl(hdl->bio, SDIOCEXECMMC, &bcmd) == -1)
		err(1, "ioctl");

	bcopy(&bcmd.cmd, cmd, sizeof *cmd);
	if (cmd->c_error) {
		errno = cmd->c_error;
		return -1;
	}
	return 0;
}

static int
sdio_command(struct sdio_hdl *hdl, int datalen, int argc, char *argv[])
{
	struct sdmmc_command cmd;
	const char *errstr;
	u_char *buf = NULL;
	int error;
	int i;

	if (argc < 3)
		errx(1, "sdio_command: wrong # args\n");

	bzero(&cmd, sizeof cmd);

	cmd.c_opcode = strtoint(argv[0], &errstr);
	if (!errstr && cmd.c_opcode > 63)
		errstr = "out of range";
	if (errstr)
		errx(1, "command index is %s: %s", argv[0]);

	cmd.c_arg = strtoint(argv[1], &errstr);
	if (errstr)
		errx(1, "command argument is %s: %s", argv[1]);

	cmd.c_flags = strtorsp(argv[2], &errstr);
	if (errstr)
		errx(1, "response type is %s: %s", argv[2]);

	argc -= 3;
	argv += 3;

	if (datalen > 0) {
		if (argc == 0)
			cmd.c_flags |= SCF_CMD_READ;

		if (argc > 0 && argc < datalen)
			errx(2, "expected %d more argument(s)",
			    datalen - argc);

		buf = (u_char *)malloc(datalen);
		if (buf == NULL)
			err(1, NULL);

		for (i = 0; argc > 0 && i < datalen; i++) {
			int ival = (u_int8_t)strtoint(argv[i], &errstr);
			if (!errstr && (ival < 0 || ival > UCHAR_MAX))
				errstr = "out of range";
			if (errstr)
				errx(1, "data byte at offset %d is %s: %s",
				    i, errstr, argv[i]);
			buf[i] = (u_int8_t)ival;
		}

		cmd.c_datalen = datalen;
		cmd.c_data = (void *)buf;
		/* XXX cmd.c_blklen = ??? */
		cmd.c_blklen = MIN(cmd.c_datalen, 512);
	}

	error = sdio_exec(hdl, &cmd);
	if (error)
		err(1, "sdio_exec");

	printf("0x%08x 0x%08x 0x%08x 0x%08x\n", (u_int)cmd.c_resp[0],
	    (u_int)cmd.c_resp[1], (u_int)cmd.c_resp[2],
	    (u_int)cmd.c_resp[3]);

	if (datalen > 0) {
		for (i = 0; argc == 0 && i < datalen; i++)
			printf("0x%02x\n", buf[i]);
		free(buf);
	}

	return 0;
}

int
main(int argc, char *argv[])
{
	struct sdio_hdl *hdl;
	const char *errstr;
	int datalen = 0;
	int dflag = 0;
	int cflag = 0;
	int c;

	while ((c = getopt(argc, argv, "cdl:")) != -1) {
		switch (c) {
		case 'c':
			cflag = 1;
			break;

		case 'd':
			dflag = 1;
			break;

		case 'l':
			datalen = strtoint(optarg, &errstr);
			if (errstr)
				errx(2, "data length is %s: %s", optarg);
			break;

		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if ((cflag + dflag) != 1 ||
	    (cflag && argc < 3) ||
	    (dflag && argc != 1))
		usage();

	if (sdio_open(DEVNAME, &hdl))
		return 1;

	if (dflag) {
		int debug = strtoint(argv[0], &errstr);
		if (errstr)
			errx(2, "argument is %s: %s", errstr, argv[0]);
		if (sdio_debug(hdl, debug))
			err(1, "unable to set debug flags");
	}

	if (cflag && sdio_command(hdl, datalen, argc, argv))
		errx(1, "unable to send command");

	sdio_close(hdl);
	return 0;
}

static int
strtoint(const char *str, const char **errstr)
{
	char *ep;
	u_long val;

	errno = 0;
	val = strtoul(str, &ep, 0);
	if (str[0] == '\0' || *ep != '\0') {
		*errstr = "not a number";
		return 0;
	}
	if (errno == ERANGE && val == ULONG_MAX) {
		*errstr = "out of range";
		return 0;
	}
	*errstr = NULL;
	return (int)val;
}

static int
strtorsp(const char *str, const char **errstr)
{
	*errstr = NULL;
	if (!strcasecmp(str, "R1"))
		return SCF_RSP_R1;
	else if (!strcasecmp(str, "R1b"))
		return SCF_RSP_R1B;
	else if (!strcasecmp(str, "R2"))
		return SCF_RSP_R2;
	else if (!strcasecmp(str, "R3"))
		return SCF_RSP_R3;
	else if (!strcasecmp(str, "R4"))
		return SCF_RSP_R4;
	else if (!strcasecmp(str, "R5"))
		return SCF_RSP_R5;
	else if (!strcasecmp(str, "R5b"))
		return SCF_RSP_R5B;
	else if (!strcasecmp(str, "R6"))
		return SCF_RSP_R6;

	*errstr = "not valid";
	return 0;
}

__dead static void
usage(void)
{
	extern char *__progname;

	fprintf(stderr,
		"usage: %s -c index argument response_type [data ...]\n",
		__progname);
	fprintf(stderr, "\t%s -d debug_flags\n", __progname);
	exit(2);
}
