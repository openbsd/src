/*	$OpenBSD: sdio.c,v 1.1 2006/11/28 20:26:31 uwe Exp $	*/

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
#include <unistd.h>

#include <sys/ioctl.h>
#include <dev/biovar.h>

#include <sys/limits.h>
#include <dev/sdmmc/sdmmcreg.h>
#include <dev/sdmmc/sdmmc_ioreg.h>
#if 0
#include <dev/sdmmc/sdmmcvar.h>
#else
/* XXX */
#include <sys/queue.h>
struct sdmmc_softc;
typedef u_int32_t sdmmc_response[4];
struct sdmmc_task {
	void (*func)(void *arg);
	void *arg;
	int onqueue;
	struct sdmmc_softc *sc;
	TAILQ_ENTRY(sdmmc_task) next;
};
struct sdmmc_command {
	struct sdmmc_task c_task;	/* task queue entry */
	u_int16_t	 c_opcode;	/* SD or MMC command index */
	u_int32_t	 c_arg;		/* SD/MMC command argument */
	sdmmc_response	 c_resp;	/* response buffer */
	void		*c_data;	/* buffer to send or read into */
	int		 c_datalen;	/* length of data buffer */
	int		 c_blklen;	/* block length */
	int		 c_flags;	/* see below */
#define SCF_ITSDONE	 0x0001		/* command is complete */
#define SCF_CMD_AC	 0x0000
#define SCF_CMD_ADTC	 0x0010
#define SCF_CMD_BC	 0x0020
#define SCF_CMD_BCR	 0x0030
#define SCF_CMD_READ	 0x0040		/* read command (data expected) */
#define SCF_RSP_BSY	 0x0100
#define SCF_RSP_136	 0x0200
#define SCF_RSP_CRC	 0x0400
#define SCF_RSP_IDX	 0x0800
#define SCF_RSP_PRESENT	 0x1000
/* response types */
#define SCF_RSP_R0	 0 /* none */
#define SCF_RSP_R1	 (SCF_RSP_PRESENT|SCF_RSP_CRC|SCF_RSP_IDX)
#define SCF_RSP_R2	 (SCF_RSP_PRESENT|SCF_RSP_CRC|SCF_RSP_136)
#define SCF_RSP_R3	 (SCF_RSP_PRESENT)
#define SCF_RSP_R4	 (SCF_RSP_PRESENT)
#define SCF_RSP_R5	 (SCF_RSP_PRESENT|SCF_RSP_CRC|SCF_RSP_IDX)
#define SCF_RSP_R5B	 (SCF_RSP_PRESENT|SCF_RSP_CRC|SCF_RSP_IDX|SCF_RSP_BSY)
#define SCF_RSP_R6	 (SCF_RSP_PRESENT|SCF_RSP_CRC)
	int		 c_error;	/* errno value on completion */
};
struct bio_sdmmc_command {
	void *cookie;
	struct sdmmc_command cmd;
};

struct bio_sdmmc_debug {
	void *cookie;
	int debug;
};
#define SDIOCEXECMMC	_IOWR('S',0, struct bio_sdmmc_command)
#define SDIOCEXECAPP	_IOWR('S',1, struct bio_sdmmc_command)
#define SDIOCSETDEBUG	_IOWR('S',2, struct bio_sdmmc_debug)
#endif

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
static int	sdio_cmd52(struct sdio_hdl *, int, int, u_int8_t *, int);
static int	sdio_cmd53(struct sdio_hdl *, int, int, u_char *, size_t,
			   int);

int		sdio_read(struct sdio_hdl *, int, int, int *, int);
int		sdio_write(struct sdio_hdl *, int, int, int, int);
static int	strtoint(const char *, const char **);
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
sdio_cmd52(struct sdio_hdl *hdl, int fnum, int reg, u_int8_t *data, int arg)
{
	struct sdmmc_command cmd;
	int function;
	int error;

	arg &= SD_ARG_CMD52_READ | SD_ARG_CMD52_WRITE;

	arg |= (fnum & SD_ARG_CMD52_FUNC_MASK) <<
	    SD_ARG_CMD52_FUNC_SHIFT;
	arg |= (reg & SD_ARG_CMD52_REG_MASK) <<
	    SD_ARG_CMD52_REG_SHIFT;
	arg |= (*data & SD_ARG_CMD52_DATA_MASK) <<
	    SD_ARG_CMD52_DATA_SHIFT;

	bzero(&cmd, sizeof cmd);
	cmd.c_opcode = SD_IO_RW_DIRECT;
	cmd.c_arg = arg;
	cmd.c_flags = SCF_CMD_BC/* XXX */ | SCF_RSP_R5;

	if (sdio_exec(hdl, &cmd) != 0)
		return -1;

	*data = SD_R5_DATA(cmd.c_resp);
	return 0;
}

static int
sdio_cmd53(struct sdio_hdl *hdl, int fnum, int reg, u_char *data,
    size_t datalen, int arg)
{
	struct sdmmc_command cmd;
	int function;
	int error;

	arg &= SD_ARG_CMD53_READ | SD_ARG_CMD53_WRITE |
	    SD_ARG_CMD53_INCREMENT;

	arg |= (fnum & SD_ARG_CMD53_FUNC_MASK) <<
	    SD_ARG_CMD53_FUNC_SHIFT;
	arg |= (reg & SD_ARG_CMD53_REG_MASK) <<
	    SD_ARG_CMD53_REG_SHIFT;
	arg |= (datalen & SD_ARG_CMD53_LENGTH_MASK) <<
	    SD_ARG_CMD53_LENGTH_SHIFT;

	bzero(&cmd, sizeof cmd);
	cmd.c_opcode = SD_IO_RW_EXTENDED;
	cmd.c_arg = arg;
	cmd.c_flags = SCF_CMD_ADTC/* XXX */ | SCF_RSP_R5;
	cmd.c_data = data;
	cmd.c_datalen = datalen;
	cmd.c_blklen = datalen;

	if (!(arg & SD_ARG_CMD53_WRITE))
		cmd.c_flags |= SCF_CMD_READ;

	return sdio_exec(hdl, &cmd);
}

int
sdio_read(struct sdio_hdl *hdl, int fnum, int addr, int *ival, int width)
{
	u_int16_t v2 = 0;
	u_int8_t v1 = 0;
	int error;

	switch (width) {
	case 2:
		error = sdio_cmd53(hdl, fnum, addr, (u_char *)&v2, 2,
		    SD_ARG_CMD53_READ | SD_ARG_CMD53_INCREMENT);
		if (!error)
			*ival = (int)v2;
		return error;
	default:
		error = sdio_cmd52(hdl, fnum, addr, &v1, SD_ARG_CMD52_READ);
		if (!error)
			*ival = (int)v1;
		return error;
	}
}

int
sdio_write(struct sdio_hdl *hdl, int fnum, int addr, int ival, int width)
{
	u_int16_t v2 = 0;
	u_int8_t v1 = 0;

	switch (width) {
	case 2:
		v2 = (u_int16_t)ival;
		return sdio_cmd53(hdl, fnum, addr, (u_char *)&v2,
		    sizeof v2, SD_ARG_CMD53_WRITE | SD_ARG_CMD53_INCREMENT);
	default:
		v1 = (u_int8_t)ival;
		return sdio_cmd52(hdl, fnum, addr, &v1, SD_ARG_CMD52_WRITE);
	}
}

int
main(int argc, char *argv[])
{
	struct sdio_hdl *hdl;
	const char *errstr;
	int dflag = 0, debug;
	int rflag = 0;
	int wflag = 0;
	int fnum = 0;
	int width = 1;
	int c;

	while ((c = getopt(argc, argv, "2d:f:rw")) != -1) {
		switch (c) {
		case '2':
			width = 2;
			break;

		case 'd':
			dflag = 1;
			debug = strtoint(optarg, &errstr);
			if (errstr)
				errx(2, "argument is %s: %s",
				    errstr, optarg);
			break;

		case 'f':
			fnum = strtonum(optarg, 0, 7, &errstr);
			if (errstr)
				errx(2, "function number is %s: %s",
				    errstr, optarg);
			break;

		case 'r':
			rflag = 1;
			break;

		case 'w':
			wflag = 1;
			break;

		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (rflag && wflag)
		errx(2, "only one of -r or -w may be specified");

	if ((rflag || wflag) && argc < 1)
		errx(2, "address required for -r or -w");

	if (wflag && argc < 2)
		errx(2, "value required for -w");

	if ((rflag && argc != 1) || (wflag && argc != 2) ||
	    ((rflag|wflag) == 0 && argc > 0) ||
	    (rflag|wflag|dflag) == 0)
		usage();

	if (sdio_open(DEVNAME, &hdl))
		return 1;

	if (dflag && sdio_debug(hdl, debug))
		err(1, "unable to set debug flags");

	if (rflag) {
		int addr;
		int val;

		addr = strtoint(argv[0], &errstr);
		if (!errstr && addr < 0 || addr > SDIO_ADDR_MAX)
			errstr = "out of range";
		if (errstr)
			errx(1, "address is %s: %s", errstr, argv[0]);

		if (sdio_read(hdl, fnum, addr, &val, width))
			err(1, "unable to read");
		printf("%u\n", (unsigned)val);
	}

	if (wflag) {
		int addr;
		int val;

		addr = strtoint(argv[0], &errstr);
		if (!errstr && addr < 0 || addr > SDIO_ADDR_MAX)
			errstr = "out of range";
		if (errstr)
			errx(1, "address is %s: %s", errstr, argv[0]);

		val = strtoint(argv[1], &errstr);
		if (!errstr &&
		    ((width == 1 && (val < 0 || val > UCHAR_MAX)) ||
		     (width == 2 && (val < 0 || val > USHRT_MAX))))
			errstr = "out of range";
		if (errstr)
			errx(1, "value is %s: %s", errstr, argv[1]);

		if (sdio_write(hdl, fnum, addr, val, width))
			err(1, "unable to write");
	}

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

__dead static void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-d flags] [-f fnum] "
	    "[-r|-w addr [value]]\n", __progname);
	exit(2);
}
